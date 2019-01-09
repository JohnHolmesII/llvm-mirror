//=======- GCNDPPCombine.cpp - optimization for DPP instructions ---==========//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// The pass combines V_MOV_B32_dpp instruction with its VALU uses as a DPP src0
// operand. If any of the use instruction cannot be combined with the mov the
// whole sequence is reverted.
//
// $old = ...
// $dpp_value = V_MOV_B32_dpp $old, $vgpr_to_be_read_from_other_lane,
//                            dpp_controls..., $row_mask, $bank_mask, $bound_ctrl
// $res = VALU $dpp_value [, src1]
//
// to
//
// $res = VALU_DPP $combined_old, $vgpr_to_be_read_from_other_lane, [src1,]
//                 dpp_controls..., $row_mask, $bank_mask, $combined_bound_ctrl
//
// Combining rules :
//
// if $row_mask and $bank_mask are fully enabled (0xF) and
//    $old == $vgpr_to_be_read_from_other_lane
// -> $combined_old = undef
//    $combined_bound_ctrl = $bound_ctrl
//
// if $row_mask and $bank_mask are fully enabled (0xF) and
//    $bound_ctrl==DPP_BOUND_ZERO and $old==any or
//    $bound_ctrl==DPP_BOUND_OFF and $old==0 (immediate)
// -> $combined_old = undef,
//    $combined_bound_ctrl = DPP_BOUND_ZERO
//
// if the VALU op is binary and
//    ($old == 0(immediate) or $bound_ctrl==DPP_BOUND_OFF) and
//    $old==identity value (immediate) for the VALU op
// -> $combined_old = src1,
//    $combined_bound_ctrl = DPP_BOUND_OFF
//
// Othervise cancel.
//===----------------------------------------------------------------------===//

#include "AMDGPU.h"
#include "AMDGPUSubtarget.h"
#include "SIInstrInfo.h"
#include "MCTargetDesc/AMDGPUMCTargetDesc.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/Pass.h"
#include <cassert>

using namespace llvm;

#define DEBUG_TYPE "gcn-dpp-combine"

STATISTIC(NumDPPMovsCombined, "Number of DPP moves combined.");

namespace {

class GCNDPPCombine : public MachineFunctionPass {
  MachineRegisterInfo *MRI;
  const SIInstrInfo *TII;

  using RegSubRegPair = TargetInstrInfo::RegSubRegPair;

  MachineOperand *getOldOpndValue(MachineOperand &OldOpnd) const;

  MachineInstr *createDPPInst(MachineInstr &OrigMI,
                              MachineInstr &MovMI,
                              RegSubRegPair OldOpndVGPR,
                              MachineOperand *OldOpnd,
                              bool BoundCtrlZero) const;

  MachineInstr *createDPPInst(MachineInstr &OrigMI,
                              MachineInstr &MovMI,
                              RegSubRegPair OldOpndVGPR,
                              bool BoundCtrlZero) const;

  bool hasNoImmOrEqual(MachineInstr &MI,
                       unsigned OpndName,
                       int64_t Value,
                       int64_t Mask = -1) const;

  bool combineDPPMov(MachineInstr &MI) const;

public:
  static char ID;

  GCNDPPCombine() : MachineFunctionPass(ID) {
    initializeGCNDPPCombinePass(*PassRegistry::getPassRegistry());
  }

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override { return "GCN DPP Combine"; }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    MachineFunctionPass::getAnalysisUsage(AU);
  }
};

} // end anonymous namespace

INITIALIZE_PASS(GCNDPPCombine, DEBUG_TYPE, "GCN DPP Combine", false, false)

char GCNDPPCombine::ID = 0;

char &llvm::GCNDPPCombineID = GCNDPPCombine::ID;

FunctionPass *llvm::createGCNDPPCombinePass() {
  return new GCNDPPCombine();
}

static int getDPPOp(unsigned Op) {
  auto DPP32 = AMDGPU::getDPPOp32(Op);
  if (DPP32 != -1)
    return DPP32;

  auto E32 = AMDGPU::getVOPe32(Op);
  return E32 != -1 ? AMDGPU::getDPPOp32(E32) : -1;
}

// tracks the register operand definition and returns:
//   1. immediate operand used to initialize the register if found
//   2. nullptr if the register operand is undef
//   3. the operand itself otherwise
MachineOperand *GCNDPPCombine::getOldOpndValue(MachineOperand &OldOpnd) const {
  auto *Def = getVRegSubRegDef(getRegSubRegPair(OldOpnd), *MRI);
  if (!Def)
    return nullptr;

  switch(Def->getOpcode()) {
  default: break;
  case AMDGPU::IMPLICIT_DEF:
    return nullptr;
  case AMDGPU::COPY:
  case AMDGPU::V_MOV_B32_e32: {
    auto &Op1 = Def->getOperand(1);
    if (Op1.isImm())
      return &Op1;
    break;
  }
  }
  return &OldOpnd;
}

MachineInstr *GCNDPPCombine::createDPPInst(MachineInstr &OrigMI,
                                           MachineInstr &MovMI,
                                           RegSubRegPair OldOpndVGPR,
                                           bool BoundCtrlZero) const {
  assert(MovMI.getOpcode() == AMDGPU::V_MOV_B32_dpp);
  assert(TII->getNamedOperand(MovMI, AMDGPU::OpName::vdst)->getReg() ==
         TII->getNamedOperand(OrigMI, AMDGPU::OpName::src0)->getReg());

  auto OrigOp = OrigMI.getOpcode();
  auto DPPOp = getDPPOp(OrigOp);
  if (DPPOp == -1) {
    LLVM_DEBUG(dbgs() << "  failed: no DPP opcode\n");
    return nullptr;
  }

  auto DPPInst = BuildMI(*OrigMI.getParent(), OrigMI,
                         OrigMI.getDebugLoc(), TII->get(DPPOp));
  bool Fail = false;
  do {
    auto *Dst = TII->getNamedOperand(OrigMI, AMDGPU::OpName::vdst);
    assert(Dst);
    DPPInst.add(*Dst);
    int NumOperands = 1;

    const int OldIdx = AMDGPU::getNamedOperandIdx(DPPOp, AMDGPU::OpName::old);
    if (OldIdx != -1) {
      assert(OldIdx == NumOperands);
      assert(isOfRegClass(OldOpndVGPR, AMDGPU::VGPR_32RegClass, *MRI));
      DPPInst.addReg(OldOpndVGPR.Reg, 0, OldOpndVGPR.SubReg);
      ++NumOperands;
    } else {
      // TODO: this discards MAC/FMA instructions for now, let's add it later
      LLVM_DEBUG(dbgs() << "  failed: no old operand in DPP instruction,"
                           " TBD\n");
      Fail = true;
      break;
    }

    if (auto *Mod0 = TII->getNamedOperand(OrigMI,
                                          AMDGPU::OpName::src0_modifiers)) {
      assert(NumOperands == AMDGPU::getNamedOperandIdx(DPPOp,
                                          AMDGPU::OpName::src0_modifiers));
      assert(0LL == (Mod0->getImm() & ~(SISrcMods::ABS | SISrcMods::NEG)));
      DPPInst.addImm(Mod0->getImm());
      ++NumOperands;
    }
    auto *Src0 = TII->getNamedOperand(MovMI, AMDGPU::OpName::src0);
    assert(Src0);
    if (!TII->isOperandLegal(*DPPInst.getInstr(), NumOperands, Src0)) {
      LLVM_DEBUG(dbgs() << "  failed: src0 is illegal\n");
      Fail = true;
      break;
    }
    DPPInst.add(*Src0);
    DPPInst->getOperand(NumOperands).setIsKill(false);
    ++NumOperands;

    if (auto *Mod1 = TII->getNamedOperand(OrigMI,
                                          AMDGPU::OpName::src1_modifiers)) {
      assert(NumOperands == AMDGPU::getNamedOperandIdx(DPPOp,
                                          AMDGPU::OpName::src1_modifiers));
      assert(0LL == (Mod1->getImm() & ~(SISrcMods::ABS | SISrcMods::NEG)));
      DPPInst.addImm(Mod1->getImm());
      ++NumOperands;
    }
    if (auto *Src1 = TII->getNamedOperand(OrigMI, AMDGPU::OpName::src1)) {
      if (!TII->isOperandLegal(*DPPInst.getInstr(), NumOperands, Src1)) {
        LLVM_DEBUG(dbgs() << "  failed: src1 is illegal\n");
        Fail = true;
        break;
      }
      DPPInst.add(*Src1);
      ++NumOperands;
    }

    if (auto *Src2 = TII->getNamedOperand(OrigMI, AMDGPU::OpName::src2)) {
      if (!TII->isOperandLegal(*DPPInst.getInstr(), NumOperands, Src2)) {
        LLVM_DEBUG(dbgs() << "  failed: src2 is illegal\n");
        Fail = true;
        break;
      }
      DPPInst.add(*Src2);
    }

    DPPInst.add(*TII->getNamedOperand(MovMI, AMDGPU::OpName::dpp_ctrl));
    DPPInst.add(*TII->getNamedOperand(MovMI, AMDGPU::OpName::row_mask));
    DPPInst.add(*TII->getNamedOperand(MovMI, AMDGPU::OpName::bank_mask));
    DPPInst.addImm(BoundCtrlZero ? 1 : 0);
  } while (false);

  if (Fail) {
    DPPInst.getInstr()->eraseFromParent();
    return nullptr;
  }
  LLVM_DEBUG(dbgs() << "  combined:  " << *DPPInst.getInstr());
  return DPPInst.getInstr();
}

static bool isIdentityValue(unsigned OrigMIOp, MachineOperand *OldOpnd) {
  assert(OldOpnd->isImm());
  switch (OrigMIOp) {
  default: break;
  case AMDGPU::V_ADD_U32_e32:
  case AMDGPU::V_ADD_I32_e32:
  case AMDGPU::V_OR_B32_e32:
  case AMDGPU::V_SUBREV_U32_e32:
  case AMDGPU::V_SUBREV_I32_e32:
  case AMDGPU::V_MIN_U32_e32:
    if (OldOpnd->getImm() == 0)
      return true;
    break;
  case AMDGPU::V_AND_B32_e32:
  case AMDGPU::V_MAX_U32_e32:
    if (OldOpnd->getImm() ==
        static_cast<int64_t>(std::numeric_limits<uint32_t>::max()))
      return true;
    break;
  case AMDGPU::V_MAX_I32_e32:
    if (OldOpnd->getImm() == std::numeric_limits<int32_t>::max())
      return true;
    break;
  case AMDGPU::V_MIN_I32_e32:
    if (OldOpnd->getImm() == std::numeric_limits<int32_t>::min())
      return true;
    break;
  case AMDGPU::V_MUL_I32_I24_e32:
  case AMDGPU::V_MUL_U32_U24_e32:
    if (OldOpnd->getImm() == 1)
      return true;
    break;
  }
  return false;
}

MachineInstr *GCNDPPCombine::createDPPInst(MachineInstr &OrigMI,
                                           MachineInstr &MovMI,
                                           RegSubRegPair OldOpndVGPR,
                                           MachineOperand *OldOpndValue,
                                           bool BoundCtrlZero) const {
  assert(OldOpndVGPR.Reg);
  if (!BoundCtrlZero && OldOpndValue && OldOpndValue->isImm()) {
    auto *Src1 = TII->getNamedOperand(OrigMI, AMDGPU::OpName::src1);
    if (!Src1 || !Src1->isReg()) {
      LLVM_DEBUG(dbgs() << "  failed: no src1 or it isn't a register\n");
      return nullptr;
    }
    if (!isIdentityValue(OrigMI.getOpcode(), OldOpndValue)) {
      LLVM_DEBUG(dbgs() << "  failed: old immediate ins't an identity\n");
      return nullptr;
    }
    OldOpndVGPR = getRegSubRegPair(*Src1);
    if (!isOfRegClass(OldOpndVGPR, AMDGPU::VGPR_32RegClass, *MRI)) {
      LLVM_DEBUG(dbgs() << "  failed: src1 isn't a VGPR32 register\n");
      return nullptr;
    }
  }
  return createDPPInst(OrigMI, MovMI, OldOpndVGPR, BoundCtrlZero);
}

// returns true if MI doesn't have OpndName immediate operand or the
// operand has Value
bool GCNDPPCombine::hasNoImmOrEqual(MachineInstr &MI, unsigned OpndName,
                                    int64_t Value, int64_t Mask) const {
  auto *Imm = TII->getNamedOperand(MI, OpndName);
  if (!Imm)
    return true;

  assert(Imm->isImm());
  return (Imm->getImm() & Mask) == Value;
}

bool GCNDPPCombine::combineDPPMov(MachineInstr &MovMI) const {
  assert(MovMI.getOpcode() == AMDGPU::V_MOV_B32_dpp);
  LLVM_DEBUG(dbgs() << "\nDPP combine: " << MovMI);

  auto *DstOpnd = TII->getNamedOperand(MovMI, AMDGPU::OpName::vdst);
  assert(DstOpnd && DstOpnd->isReg());
  auto DPPMovReg = DstOpnd->getReg();
  if (!isEXECMaskConstantBetweenDefAndUses(DPPMovReg, *MRI)) {
    LLVM_DEBUG(dbgs() << "  failed: EXEC mask should remain the same"
                         " for all uses\n");
    return false;
  }

  auto *RowMaskOpnd = TII->getNamedOperand(MovMI, AMDGPU::OpName::row_mask);
  assert(RowMaskOpnd && RowMaskOpnd->isImm());
  auto *BankMaskOpnd = TII->getNamedOperand(MovMI, AMDGPU::OpName::bank_mask);
  assert(BankMaskOpnd && BankMaskOpnd->isImm());
  const bool MaskAllLanes = RowMaskOpnd->getImm() == 0xF &&
                            BankMaskOpnd->getImm() == 0xF;

  auto *BCZOpnd = TII->getNamedOperand(MovMI, AMDGPU::OpName::bound_ctrl);
  assert(BCZOpnd && BCZOpnd->isImm());
  bool BoundCtrlZero = BCZOpnd->getImm();

  auto *OldOpnd = TII->getNamedOperand(MovMI, AMDGPU::OpName::old);
  assert(OldOpnd && OldOpnd->isReg());
  auto OldOpndVGPR = getRegSubRegPair(*OldOpnd);
  auto *OldOpndValue = getOldOpndValue(*OldOpnd);
  assert(!OldOpndValue || OldOpndValue->isImm() || OldOpndValue == OldOpnd);

  do {
    if (!OldOpndValue) { // undef case
      if (!BoundCtrlZero || !MaskAllLanes) {
        LLVM_DEBUG(dbgs()
          << "  failed: undef old has bctr off or mask isn't full BB\n");
        return false;
      }
      break;
	}

	if (BoundCtrlZero && MaskAllLanes) { // [1]
      OldOpndVGPR.Reg = AMDGPU::NoRegister;
      OldOpndValue = nullptr;
      break;
    }

    if (OldOpndValue->getParent()->getParent() != MovMI.getParent()) {
      LLVM_DEBUG(dbgs() <<
		"  failed: old reg def and mov should be in the same BB\n");
      return false;
    }

	if (OldOpndValue->isImm()) {
      if (OldOpndValue->getImm() == 0) {
        if (MaskAllLanes) {
          assert(!BoundCtrlZero); // by check [1]
		  // undefine old to make regalloc's life easier but this requires
		  // bound_ctrl:0 set
          OldOpndVGPR.Reg = AMDGPU::NoRegister;
          OldOpndValue = nullptr;
          BoundCtrlZero = true;
        } else {
		  // bound_ctrl value ins't important here but this makes the combiner's
          // logic easier - we check the VALU op for identity value when
		  // BoundCtrlZero == false.
          BoundCtrlZero = false;
        }
      } else if (BoundCtrlZero) {
        assert(!MaskAllLanes); // by check [1]
        LLVM_DEBUG(dbgs() <<
	      "  failed: old!=0 and bctrl:0 and not all lanes isn't combinable\n");
        return false;
      }
      break;
    }

    assert(OldOpndValue->isReg());
	auto *DPPSrcOpnd = TII->getNamedOperand(MovMI, AMDGPU::OpName::src0);
    assert(DPPSrcOpnd && DPPSrcOpnd->isReg());
    if (!MaskAllLanes || getRegSubRegPair(*DPPSrcOpnd) != OldOpndVGPR) {
      LLVM_DEBUG(dbgs() << "  failed: the DPP mov isn't combinable\n");
      return false;
    }
	// case when DPP mov old == DPP scr register
    OldOpndVGPR.Reg = AMDGPU::NoRegister;
    OldOpndValue = nullptr;
  } while (0);

  LLVM_DEBUG(dbgs() << "  old=";
    if (!OldOpndValue)
      dbgs() << "undef";
    else
      dbgs() << *OldOpndValue;
    dbgs() << ", bound_ctrl=" << BoundCtrlZero << '\n');

  std::vector<MachineInstr*> OrigMIs, DPPMIs;
  if (!OldOpndVGPR.Reg) { // OldOpndVGPR = undef
    OldOpndVGPR = RegSubRegPair(
      MRI->createVirtualRegister(&AMDGPU::VGPR_32RegClass));
    auto UndefInst = BuildMI(*MovMI.getParent(), MovMI, MovMI.getDebugLoc(),
                             TII->get(AMDGPU::IMPLICIT_DEF), OldOpndVGPR.Reg);
    DPPMIs.push_back(UndefInst.getInstr());
  }

  OrigMIs.push_back(&MovMI);
  bool Rollback = true;
  for (auto &Use : MRI->use_nodbg_operands(DPPMovReg)) {
    Rollback = true;

    auto &OrigMI = *Use.getParent();
    LLVM_DEBUG(dbgs() << "  try: " << OrigMI);

    auto OrigOp = OrigMI.getOpcode();
    if (TII->isVOP3(OrigOp)) {
      if (!TII->hasVALU32BitEncoding(OrigOp)) {
        LLVM_DEBUG(dbgs() << "  failed: VOP3 hasn't e32 equivalent\n");
        break;
      }
      // check if other than abs|neg modifiers are set (opsel for example)
      const int64_t Mask = ~(SISrcMods::ABS | SISrcMods::NEG);
      if (!hasNoImmOrEqual(OrigMI, AMDGPU::OpName::src0_modifiers, 0, Mask) ||
          !hasNoImmOrEqual(OrigMI, AMDGPU::OpName::src1_modifiers, 0, Mask) ||
          !hasNoImmOrEqual(OrigMI, AMDGPU::OpName::clamp, 0) ||
          !hasNoImmOrEqual(OrigMI, AMDGPU::OpName::omod, 0)) {
        LLVM_DEBUG(dbgs() << "  failed: VOP3 has non-default modifiers\n");
        break;
      }
    } else if (!TII->isVOP1(OrigOp) && !TII->isVOP2(OrigOp)) {
      LLVM_DEBUG(dbgs() << "  failed: not VOP1/2/3\n");
      break;
    }

    LLVM_DEBUG(dbgs() << "  combining: " << OrigMI);
    if (&Use == TII->getNamedOperand(OrigMI, AMDGPU::OpName::src0)) {
      if (auto *DPPInst = createDPPInst(OrigMI, MovMI, OldOpndVGPR,
                                        OldOpndValue, BoundCtrlZero)) {
        DPPMIs.push_back(DPPInst);
        Rollback = false;
      }
    } else if (OrigMI.isCommutable() &&
               &Use == TII->getNamedOperand(OrigMI, AMDGPU::OpName::src1)) {
      auto *BB = OrigMI.getParent();
      auto *NewMI = BB->getParent()->CloneMachineInstr(&OrigMI);
      BB->insert(OrigMI, NewMI);
      if (TII->commuteInstruction(*NewMI)) {
        LLVM_DEBUG(dbgs() << "  commuted:  " << *NewMI);
        if (auto *DPPInst = createDPPInst(*NewMI, MovMI, OldOpndVGPR,
                                          OldOpndValue, BoundCtrlZero)) {
          DPPMIs.push_back(DPPInst);
          Rollback = false;
        }
      } else
        LLVM_DEBUG(dbgs() << "  failed: cannot be commuted\n");
      NewMI->eraseFromParent();
    } else
      LLVM_DEBUG(dbgs() << "  failed: no suitable operands\n");
    if (Rollback)
      break;
    OrigMIs.push_back(&OrigMI);
  }

  for (auto *MI : *(Rollback? &DPPMIs : &OrigMIs))
    MI->eraseFromParent();

  return !Rollback;
}

bool GCNDPPCombine::runOnMachineFunction(MachineFunction &MF) {
  auto &ST = MF.getSubtarget<GCNSubtarget>();
  if (!ST.hasDPP() || skipFunction(MF.getFunction()))
    return false;

  MRI = &MF.getRegInfo();
  TII = ST.getInstrInfo();

  assert(MRI->isSSA() && "Must be run on SSA");

  bool Changed = false;
  for (auto &MBB : MF) {
    for (auto I = MBB.rbegin(), E = MBB.rend(); I != E;) {
      auto &MI = *I++;
      if (MI.getOpcode() == AMDGPU::V_MOV_B32_dpp && combineDPPMov(MI)) {
        Changed = true;
        ++NumDPPMovsCombined;
      }
    }
  }
  return Changed;
}

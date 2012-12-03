//===-- OcamlGCPrinter.cpp - Ocaml frametable emitter ---------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements printing the assembly code for an Ocaml frametable.
//
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/GCs.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/GCMetadataPrinter.h"
#include "llvm/DataLayout.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Module.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Target/Mangler.h"
#include "llvm/Target/TargetLoweringObjectFile.h"
#include "llvm/Target/TargetMachine.h"
#include <cctype>
using namespace llvm;

namespace {

  class OcamlGCMetadataPrinter : public GCMetadataPrinter {
  public:
    void beginAssembly(AsmPrinter &AP);
    void finishAssembly(AsmPrinter &AP);
  };

}

static GCMetadataPrinterRegistry::Add<OcamlGCMetadataPrinter>
Y("ocaml", "ocaml 3.10-compatible collector");

void llvm::linkOcamlGCPrinter() { }

static void EmitCamlGlobal(const Module &M, AsmPrinter &AP, const char *Id) {
  const std::string &MId = M.getModuleIdentifier();

  std::string SymName;
  SymName += "caml";
  size_t Letter = SymName.size();
  SymName.append(MId.begin(), std::find(MId.begin(), MId.end(), '.'));
  SymName += "__";
  SymName += Id;

  // Capitalize the first letter of the module name.
  SymName[Letter] = toupper(SymName[Letter]);

  SmallString<128> TmpStr;
  AP.Mang->getNameWithPrefix(TmpStr, SymName);

  MCSymbol *Sym = AP.OutContext.GetOrCreateSymbol(TmpStr);

  AP.OutStreamer.EmitSymbolAttribute(Sym, MCSA_Global);
  AP.OutStreamer.EmitLabel(Sym);
}

void OcamlGCMetadataPrinter::beginAssembly(AsmPrinter &AP) {
  AP.OutStreamer.SwitchSection(AP.getObjFileLowering().getTextSection());
  EmitCamlGlobal(getModule(), AP, "code_begin");

  AP.OutStreamer.SwitchSection(AP.getObjFileLowering().getDataSection());
  EmitCamlGlobal(getModule(), AP, "data_begin");
}

/// emitAssembly - Print the frametable. The ocaml frametable format is thus:
///
///   extern "C" struct align(sizeof(intptr_t)) {
///     uint16_t NumDescriptors;
///     struct align(sizeof(intptr_t)) {
///       void *ReturnAddress;
///       uint16_t FrameSize;
///       uint16_t NumLiveOffsets;
///       uint16_t LiveOffsets[NumLiveOffsets];
///     } Descriptors[NumDescriptors];
///   } caml${module}__frametable;
///
/// Note that this precludes programs from stack frames larger than 64K
/// (FrameSize and LiveOffsets would overflow). FrameTablePrinter will abort if
/// either condition is detected in a function which uses the GC.
///
void OcamlGCMetadataPrinter::finishAssembly(AsmPrinter &AP) {
  unsigned IntPtrSize = AP.TM.getDataLayout()->getPointerSize();

  AP.OutStreamer.SwitchSection(AP.getObjFileLowering().getTextSection());
  EmitCamlGlobal(getModule(), AP, "code_end");

  AP.OutStreamer.SwitchSection(AP.getObjFileLowering().getDataSection());
  EmitCamlGlobal(getModule(), AP, "data_end");

  // FIXME: Why does ocaml emit this??
  AP.OutStreamer.EmitIntValue(0, IntPtrSize, 0);

  AP.OutStreamer.SwitchSection(AP.getObjFileLowering().getDataSection());
  EmitCamlGlobal(getModule(), AP, "frametable");

  int NumDescriptors = 0;
  for (iterator I = begin(), IE = end(); I != IE; ++I) {
    GCFunctionInfo &FI = **I;
    for (GCFunctionInfo::iterator J = FI.begin(), JE = FI.end(); J != JE; ++J) {
      NumDescriptors++;
    }
  }

  if (NumDescriptors >= 1<<16) {
    // Very rude!
    report_fatal_error(" Too much descriptor for ocaml GC");
  }
  AP.EmitInt16(NumDescriptors);
  AP.EmitAlignment(IntPtrSize == 4 ? 2 : 3);

  for (iterator I = begin(), IE = end(); I != IE; ++I) {
    GCFunctionInfo &FI = **I;

    uint64_t FrameSize = FI.getFrameSize();
    if (FrameSize >= 1<<16) {
      // Very rude!
      report_fatal_error("Function '" + FI.getFunction().getName() +
                         "' is too large for the ocaml GC! "
                         "Frame size " + Twine(FrameSize) + ">= 65536.\n"
                         "(" + Twine(uintptr_t(&FI)) + ")");
    }

    AP.OutStreamer.AddComment("live roots for " +
                              Twine(FI.getFunction().getName()));
    AP.OutStreamer.AddBlankLine();

    for (GCFunctionInfo::iterator J = FI.begin(), JE = FI.end(); J != JE; ++J) {
      size_t LiveCount = FI.live_size(J);
      if (LiveCount >= 1<<16) {
        // Very rude!
        report_fatal_error("Function '" + FI.getFunction().getName() +
                           "' is too large for the ocaml GC! "
                           "Live root count "+Twine(LiveCount)+" >= 65536.");
      }

      AP.OutStreamer.EmitSymbolValue(J->Label, IntPtrSize, 0);
      AP.EmitInt16(FrameSize);
      AP.EmitInt16(LiveCount);

      for (GCFunctionInfo::live_iterator K = FI.live_begin(J),
                                         KE = FI.live_end(J); K != KE; ++K) {
        if (K->StackOffset >= 1<<16) {
          // Very rude!
          report_fatal_error(
                 "GC root stack offset is outside of fixed stack frame and out "
                 "of range for ocaml GC!");
        }
        AP.EmitInt16(K->StackOffset);
      }

      AP.EmitAlignment(IntPtrSize == 4 ? 2 : 3);
    }
  }
}

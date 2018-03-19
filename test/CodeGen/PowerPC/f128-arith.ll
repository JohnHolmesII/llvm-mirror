; RUN: llc -mcpu=pwr9 -mtriple=powerpc64le-unknown-unknown < %s | FileCheck %s

; Function Attrs: norecurse nounwind
define void @qpAdd(fp128* nocapture readonly %a, fp128* nocapture %res) {
entry:
  %0 = load fp128, fp128* %a, align 16
  %add = fadd fp128 %0, %0
  store fp128 %add, fp128* %res, align 16
  ret void
; CHECK-LABEL: qpAdd
; CHECK-NOT bl __addtf3
; CHECK xsaddqp
; CHECK stxv
; CHECK blr
}

; Function Attrs: norecurse nounwind
define void @qpSub(fp128* nocapture readonly %a, fp128* nocapture %res) {
entry:
  %0 = load fp128, fp128* %a, align 16
  %sub = fsub fp128 %0, %0
  store fp128 %sub, fp128* %res, align 16
  ret void
; CHECK-LABEL: qpSub
; CHECK-NOT bl __subtf3
; CHECK xssubqp
; CHECK stxv
; CHECK blr
}

; Function Attrs: norecurse nounwind
define void @qpMul(fp128* nocapture readonly %a, fp128* nocapture %res) {
entry:
  %0 = load fp128, fp128* %a, align 16
  %mul = fmul fp128 %0, %0
  store fp128 %mul, fp128* %res, align 16
  ret void
; CHECK-LABEL: qpMul
; CHECK-NOT bl __multf3
; CHECK xsmulqp
; CHECK stxv
; CHECK blr
}

; Function Attrs: norecurse nounwind
define void @qpDiv(fp128* nocapture readonly %a, fp128* nocapture %res) {
entry:
  %0 = load fp128, fp128* %a, align 16
  %div = fdiv fp128 %0, %0
  store fp128 %div, fp128* %res, align 16
  ret void
; CHECK-LABEL: qpDiv
; CHECK-NOT bl __divtf3
; CHECK xsdivqp
; CHECK stxv
; CHECK blr
}

define void @testLdNSt(i8* nocapture readonly %PtrC, fp128* nocapture %PtrF) {
entry:
  %add.ptr = getelementptr inbounds i8, i8* %PtrC, i64 4
  %0 = bitcast i8* %add.ptr to fp128*
  %1 = load fp128, fp128* %0, align 16
  %2 = bitcast fp128* %PtrF to i8*
  %add.ptr1 = getelementptr inbounds i8, i8* %2, i64 8
  %3 = bitcast i8* %add.ptr1 to fp128*
  store fp128 %1, fp128* %3, align 16
  ret void
; CHECK-LABEL: testLdNSt
; CHECK lxvx
; CHECK stxvx
; CHECK-NEXT blr
}

; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc < %s -mtriple=x86_64-unknown-linux-gnu -mattr=+avx2 -mattr=+fma  | FileCheck %s

; This test checks combinations of FNEG and FMA intrinsics

define <8 x float> @test1(<8 x float> %a, <8 x float> %b, <8 x float> %c)  {
; CHECK-LABEL: test1:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    vfmsub213ps %ymm2, %ymm1, %ymm0
; CHECK-NEXT:    retq
entry:
  %sub.i = fsub <8 x float> <float -0.000000e+00, float -0.000000e+00, float -0.000000e+00, float -0.000000e+00, float -0.000000e+00, float -0.000000e+00, float -0.000000e+00, float -0.000000e+00>, %c
  %0 = tail call <8 x float> @llvm.x86.fma.vfmadd.ps.256(<8 x float> %a, <8 x float> %b, <8 x float> %sub.i) #2
  ret <8 x float> %0
}

declare <8 x float> @llvm.x86.fma.vfmadd.ps.256(<8 x float>, <8 x float>, <8 x float>)

define <4 x float> @test2(<4 x float> %a, <4 x float> %b, <4 x float> %c) {
; CHECK-LABEL: test2:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    vfnmsub213ps %xmm2, %xmm1, %xmm0
; CHECK-NEXT:    retq
entry:
  %0 = tail call <4 x float> @llvm.x86.fma.vfmadd.ps(<4 x float> %a, <4 x float> %b, <4 x float> %c) #2
  %sub.i = fsub <4 x float> <float -0.000000e+00, float -0.000000e+00, float -0.000000e+00, float -0.000000e+00>, %0
  ret <4 x float> %sub.i
}

declare <4 x float> @llvm.x86.fma.vfmadd.ps(<4 x float> %a, <4 x float> %b, <4 x float> %c)

define <4 x float> @test3(<4 x float> %a, <4 x float> %b, <4 x float> %c)  {
; CHECK-LABEL: test3:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    vfnmadd213ss %xmm2, %xmm1, %xmm0
; CHECK-NEXT:    vbroadcastss {{.*}}(%rip), %xmm1
; CHECK-NEXT:    vxorps %xmm1, %xmm0, %xmm0
; CHECK-NEXT:    retq
entry:
  %0 = tail call <4 x float> @llvm.x86.fma.vfnmadd.ss(<4 x float> %a, <4 x float> %b, <4 x float> %c) #2
  %sub.i = fsub <4 x float> <float -0.000000e+00, float -0.000000e+00, float -0.000000e+00, float -0.000000e+00>, %0
  ret <4 x float> %sub.i
}

declare <4 x float> @llvm.x86.fma.vfnmadd.ss(<4 x float> %a, <4 x float> %b, <4 x float> %c)

define <8 x float> @test4(<8 x float> %a, <8 x float> %b, <8 x float> %c) {
; CHECK-LABEL: test4:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    vfnmadd213ps %ymm2, %ymm1, %ymm0
; CHECK-NEXT:    retq
entry:
  %0 = tail call <8 x float> @llvm.x86.fma.vfmsub.ps.256(<8 x float> %a, <8 x float> %b, <8 x float> %c) #2
  %sub.i = fsub <8 x float> <float -0.000000e+00, float -0.000000e+00, float -0.000000e+00, float -0.000000e+00, float -0.000000e+00, float -0.000000e+00, float -0.000000e+00, float -0.000000e+00>, %0
  ret <8 x float> %sub.i
}

define <8 x float> @test5(<8 x float> %a, <8 x float> %b, <8 x float> %c) {
; CHECK-LABEL: test5:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    vbroadcastss {{.*}}(%rip), %ymm3
; CHECK-NEXT:    vxorps %ymm3, %ymm2, %ymm2
; CHECK-NEXT:    vfmsub213ps %ymm2, %ymm1, %ymm0
; CHECK-NEXT:    retq
entry:
  %sub.c = fsub <8 x float> <float -0.000000e+00, float -0.000000e+00, float -0.000000e+00, float -0.000000e+00, float -0.000000e+00, float -0.000000e+00, float -0.000000e+00, float -0.000000e+00>, %c
  %0 = tail call <8 x float> @llvm.x86.fma.vfmsub.ps.256(<8 x float> %a, <8 x float> %b, <8 x float> %sub.c) #2
  ret <8 x float> %0
}

declare <8 x float> @llvm.x86.fma.vfmsub.ps.256(<8 x float>, <8 x float>, <8 x float>)


define <2 x double> @test6(<2 x double> %a, <2 x double> %b, <2 x double> %c) {
; CHECK-LABEL: test6:
; CHECK:       # BB#0: # %entry
; CHECK-NEXT:    vfnmsub213pd %xmm2, %xmm1, %xmm0
; CHECK-NEXT:    retq
entry:
  %0 = tail call <2 x double> @llvm.x86.fma.vfmadd.pd(<2 x double> %a, <2 x double> %b, <2 x double> %c) #2
  %sub.i = fsub <2 x double> <double -0.000000e+00, double -0.000000e+00>, %0
  ret <2 x double> %sub.i
}

declare <2 x double> @llvm.x86.fma.vfmadd.pd(<2 x double> %a, <2 x double> %b, <2 x double> %c)


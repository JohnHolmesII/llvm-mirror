; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc -O3 -mtriple=x86_64-pc-linux < %s | FileCheck %s --check-prefix=COMMON --check-prefix=SSE
; RUN: llc -O3 -mtriple=x86_64-pc-linux -mattr=+fma < %s | FileCheck %s --check-prefix=COMMON --check-prefix=AVX
; RUN: llc -O3 -mtriple=x86_64-pc-linux -mattr=+avx512f < %s | FileCheck %s --check-prefix=COMMON --check-prefix=AVX

; Verify that fma(3.5) isn't simplified when the rounding mode is
; unknown.
define float @f17() #0 {
; SSE-LABEL: f17:
; SSE:       # %bb.0: # %entry
; SSE-NEXT:    movss {{.*#+}} xmm0 = mem[0],zero,zero,zero
; SSE-NEXT:    movaps %xmm0, %xmm1
; SSE-NEXT:    movaps %xmm0, %xmm2
; SSE-NEXT:    jmp fmaf # TAILCALL
;
; AVX-LABEL: f17:
; AVX:       # %bb.0: # %entry
; AVX-NEXT:    vmovss {{.*#+}} xmm0 = mem[0],zero,zero,zero
; AVX-NEXT:    vfmadd213ss {{.*#+}} xmm0 = (xmm0 * xmm0) + xmm0
; AVX-NEXT:    retq
; NOFMA-LABEL: f17:
; NOFMA:       # %bb.0: # %entry
; NOFMA-NEXT:    vmovss {{.*#+}} xmm0 = mem[0],zero,zero,zero
; NOFMA-NEXT:    vmovaps %xmm0, %xmm1
; NOFMA-NEXT:    vmovaps %xmm0, %xmm2
; NOFMA-NEXT:    jmp fmaf # TAILCALL
; FMA-LABEL: f17:
; FMA:       # %bb.0: # %entry
; FMA-NEXT:    vmovss {{.*#+}} xmm0 = mem[0],zero,zero,zero
; FMA-NEXT:    vfmadd213ss {{.*#+}} xmm0 = (xmm0 * xmm0) + xmm0
; FMA-NEXT:    retq
; AVX512-LABEL: f17:
; AVX512:       # %bb.0: # %entry
; AVX512-NEXT:    vmovss {{.*#+}} xmm0 = mem[0],zero,zero,zero
; AVX512-NEXT:    vfmadd213ss {{.*#+}} xmm0 = (xmm0 * xmm0) + xmm0
; AVX512-NEXT:    retq
entry:
  %result = call float @llvm.experimental.constrained.fma.f32(
                                               float 3.5,
                                               float 3.5,
                                               float 3.5,
                                               metadata !"round.dynamic",
                                               metadata !"fpexcept.strict") #0
  ret float %result
}

; Verify that fma(42.1) isn't simplified when the rounding mode is
; unknown.
define double @f18() #0 {
; SSE-LABEL: f18:
; SSE:       # %bb.0: # %entry
; SSE-NEXT:    movsd {{.*#+}} xmm0 = mem[0],zero
; SSE-NEXT:    movaps %xmm0, %xmm1
; SSE-NEXT:    movaps %xmm0, %xmm2
; SSE-NEXT:    jmp fma # TAILCALL
;
; AVX-LABEL: f18:
; AVX:       # %bb.0: # %entry
; AVX-NEXT:    vmovsd {{.*#+}} xmm0 = mem[0],zero
; AVX-NEXT:    vfmadd213sd {{.*#+}} xmm0 = (xmm0 * xmm0) + xmm0
; AVX-NEXT:    retq
; NOFMA-LABEL: f18:
; NOFMA:       # %bb.0: # %entry
; NOFMA-NEXT:    vmovsd {{.*#+}} xmm0 = mem[0],zero
; NOFMA-NEXT:    vmovaps %xmm0, %xmm1
; NOFMA-NEXT:    vmovaps %xmm0, %xmm2
; NOFMA-NEXT:    jmp fma # TAILCALL
; FMA-LABEL: f18:
; FMA:       # %bb.0: # %entry
; FMA-NEXT:    vmovsd {{.*#+}} xmm0 = mem[0],zero
; FMA-NEXT:    vfmadd213sd {{.*#+}} xmm0 = (xmm0 * xmm0) + xmm0
; FMA-NEXT:    retq
; AVX512-LABEL: f18:
; AVX512:       # %bb.0: # %entry
; AVX512-NEXT:    vmovsd {{.*#+}} xmm0 = mem[0],zero
; AVX512-NEXT:    vfmadd213sd {{.*#+}} xmm0 = (xmm0 * xmm0) + xmm0
; AVX512-NEXT:    retq
entry:
  %result = call double @llvm.experimental.constrained.fma.f64(
                                               double 42.1,
                                               double 42.1,
                                               double 42.1,
                                               metadata !"round.dynamic",
                                               metadata !"fpexcept.strict") #0
  ret double %result
}

attributes #0 = { strictfp }

declare float @llvm.experimental.constrained.fma.f32(float, float, float, metadata, metadata)
declare double @llvm.experimental.constrained.fma.f64(double, double, double, metadata, metadata)

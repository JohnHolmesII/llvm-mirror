; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc < %s -verify-machineinstrs -mtriple=aarch64-none-linux-gnu -mattr=+neon | FileCheck %s


define <8 x i8> @mla8xi8(<8 x i8> %A, <8 x i8> %B, <8 x i8> %C) {
; CHECK-LABEL: mla8xi8:
; CHECK:       // %bb.0:
; CHECK-NEXT:    mla v2.8b, v0.8b, v1.8b
; CHECK-NEXT:    mov v0.16b, v2.16b
; CHECK-NEXT:    ret
	%tmp1 = mul <8 x i8> %A, %B;
	%tmp2 = add <8 x i8> %C, %tmp1;
	ret <8 x i8> %tmp2
}

define <16 x i8> @mla16xi8(<16 x i8> %A, <16 x i8> %B, <16 x i8> %C) {
; CHECK-LABEL: mla16xi8:
; CHECK:       // %bb.0:
; CHECK-NEXT:    mla v2.16b, v0.16b, v1.16b
; CHECK-NEXT:    mov v0.16b, v2.16b
; CHECK-NEXT:    ret
	%tmp1 = mul <16 x i8> %A, %B;
	%tmp2 = add <16 x i8> %C, %tmp1;
	ret <16 x i8> %tmp2
}

define <4 x i16> @mla4xi16(<4 x i16> %A, <4 x i16> %B, <4 x i16> %C) {
; CHECK-LABEL: mla4xi16:
; CHECK:       // %bb.0:
; CHECK-NEXT:    mla v2.4h, v0.4h, v1.4h
; CHECK-NEXT:    mov v0.16b, v2.16b
; CHECK-NEXT:    ret
	%tmp1 = mul <4 x i16> %A, %B;
	%tmp2 = add <4 x i16> %C, %tmp1;
	ret <4 x i16> %tmp2
}

define <8 x i16> @mla8xi16(<8 x i16> %A, <8 x i16> %B, <8 x i16> %C) {
; CHECK-LABEL: mla8xi16:
; CHECK:       // %bb.0:
; CHECK-NEXT:    mla v2.8h, v0.8h, v1.8h
; CHECK-NEXT:    mov v0.16b, v2.16b
; CHECK-NEXT:    ret
	%tmp1 = mul <8 x i16> %A, %B;
	%tmp2 = add <8 x i16> %C, %tmp1;
	ret <8 x i16> %tmp2
}

define <2 x i32> @mla2xi32(<2 x i32> %A, <2 x i32> %B, <2 x i32> %C) {
; CHECK-LABEL: mla2xi32:
; CHECK:       // %bb.0:
; CHECK-NEXT:    mla v2.2s, v0.2s, v1.2s
; CHECK-NEXT:    mov v0.16b, v2.16b
; CHECK-NEXT:    ret
	%tmp1 = mul <2 x i32> %A, %B;
	%tmp2 = add <2 x i32> %C, %tmp1;
	ret <2 x i32> %tmp2
}

define <4 x i32> @mla4xi32(<4 x i32> %A, <4 x i32> %B, <4 x i32> %C) {
; CHECK-LABEL: mla4xi32:
; CHECK:       // %bb.0:
; CHECK-NEXT:    mla v2.4s, v0.4s, v1.4s
; CHECK-NEXT:    mov v0.16b, v2.16b
; CHECK-NEXT:    ret
	%tmp1 = mul <4 x i32> %A, %B;
	%tmp2 = add <4 x i32> %C, %tmp1;
	ret <4 x i32> %tmp2
}

define <8 x i8> @mls8xi8(<8 x i8> %A, <8 x i8> %B, <8 x i8> %C) {
; CHECK-LABEL: mls8xi8:
; CHECK:       // %bb.0:
; CHECK-NEXT:    mls v2.8b, v0.8b, v1.8b
; CHECK-NEXT:    mov v0.16b, v2.16b
; CHECK-NEXT:    ret
	%tmp1 = mul <8 x i8> %A, %B;
	%tmp2 = sub <8 x i8> %C, %tmp1;
	ret <8 x i8> %tmp2
}

define <16 x i8> @mls16xi8(<16 x i8> %A, <16 x i8> %B, <16 x i8> %C) {
; CHECK-LABEL: mls16xi8:
; CHECK:       // %bb.0:
; CHECK-NEXT:    mls v2.16b, v0.16b, v1.16b
; CHECK-NEXT:    mov v0.16b, v2.16b
; CHECK-NEXT:    ret
	%tmp1 = mul <16 x i8> %A, %B;
	%tmp2 = sub <16 x i8> %C, %tmp1;
	ret <16 x i8> %tmp2
}

define <4 x i16> @mls4xi16(<4 x i16> %A, <4 x i16> %B, <4 x i16> %C) {
; CHECK-LABEL: mls4xi16:
; CHECK:       // %bb.0:
; CHECK-NEXT:    mls v2.4h, v0.4h, v1.4h
; CHECK-NEXT:    mov v0.16b, v2.16b
; CHECK-NEXT:    ret
	%tmp1 = mul <4 x i16> %A, %B;
	%tmp2 = sub <4 x i16> %C, %tmp1;
	ret <4 x i16> %tmp2
}

define <8 x i16> @mls8xi16(<8 x i16> %A, <8 x i16> %B, <8 x i16> %C) {
; CHECK-LABEL: mls8xi16:
; CHECK:       // %bb.0:
; CHECK-NEXT:    mls v2.8h, v0.8h, v1.8h
; CHECK-NEXT:    mov v0.16b, v2.16b
; CHECK-NEXT:    ret
	%tmp1 = mul <8 x i16> %A, %B;
	%tmp2 = sub <8 x i16> %C, %tmp1;
	ret <8 x i16> %tmp2
}

define <2 x i32> @mls2xi32(<2 x i32> %A, <2 x i32> %B, <2 x i32> %C) {
; CHECK-LABEL: mls2xi32:
; CHECK:       // %bb.0:
; CHECK-NEXT:    mls v2.2s, v0.2s, v1.2s
; CHECK-NEXT:    mov v0.16b, v2.16b
; CHECK-NEXT:    ret
	%tmp1 = mul <2 x i32> %A, %B;
	%tmp2 = sub <2 x i32> %C, %tmp1;
	ret <2 x i32> %tmp2
}

define <4 x i32> @mls4xi32(<4 x i32> %A, <4 x i32> %B, <4 x i32> %C) {
; CHECK-LABEL: mls4xi32:
; CHECK:       // %bb.0:
; CHECK-NEXT:    mls v2.4s, v0.4s, v1.4s
; CHECK-NEXT:    mov v0.16b, v2.16b
; CHECK-NEXT:    ret
	%tmp1 = mul <4 x i32> %A, %B;
	%tmp2 = sub <4 x i32> %C, %tmp1;
	ret <4 x i32> %tmp2
}



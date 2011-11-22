     	.text
	.globl Neon_Convert_yuv422_to_NV21
@ YUV 422 to YUV NV21
Neon_Convert_yuv422_to_NV21:

	PUSH 	{r4-r12,lr}
	
	MUL 	r4, r2, r3					@ImageSize = iWidth x iHeight
	
	ADD 	r8, r1, r4				@ inBufPtrU = inBufPtrY + ImageSize
	
	MOV 	r10, r0
	MOV 	r6, r1
	MOV 	r4, r2
	MOV 	r5, r3

	ADD 	r7, r6, r4
	ADD 	r11, r10, r4, lsl #1


YUVNV21_lineloop:

	SUBS 	r3, r4, #16
	BLT 	YUVNV21_singles_entry

YUVNV21_hexloop:

	VLD4.8 	{d0, d1, d2, d3}, [r10]!
	VLD4.8 	{d4, d5, d6, d7}, [r11]!

	VRHADD.U8 d9, d0, d4	@ Averaging 2 U-values
	VRHADD.U8 d8, d2, d6	@Averaging 2 V-values
	
	VST2.8 	{d1, d3}, [r6]!
	VST2.8 	{d5, d7}, [r7]!

	VMOVL.U8 q10, d9
	VSHL.I16 q10, #8

	VMOVL.U8 q11, d8
	VADD.S16 q11, q11, q10

	VST1.8 	{q11}, [r8]!

	SUBS 	r3, #16
	BGE 	YUVNV21_hexloop

YUVNV21_singles_entry:
	ADDS 	r3, #16
	BEQ 	YUVNV21_linedone

YUVNV21_linedone:
	ADD		r6, r4
	ADD		r7, r4
	ADD		r10, r4, lsl #1
	ADD		r11, r4, lsl #1
	SUBS 	r5, #2
	BGT 	YUVNV21_lineloop

	POP      {r4-r12,pc}
	.text	
	.globl Neon_Convert_yuv422_to_NV12
@ YUV 422 to YUV 420 NV12

Neon_Convert_yuv422_to_NV12:

	PUSH 	{r4-r12,lr}
	
	MUL 	r4, r2, r3					@ImageSize = iWidth x iHeight
	
	ADD 	r8, r1, r4				@ inBufPtrU = inBufPtrY + ImageSize
	
	MOV 	r10, r0
	MOV 	r6, r1
	MOV 	r4, r2
	MOV 	r5, r3

	ADD 	r7, r6, r4
	ADD 	r11, r10, r4, lsl #1


YUVNV12_lineloop:

	SUBS 	r3, r4, #16
	BLT 	YUVNV12_singles_entry

YUVNV12_hexloop:

	VLD4.8 	{d0, d1, d2, d3}, [r10]!
	VLD4.8 	{d4, d5, d6, d7}, [r11]!

	VRHADD.U8 d8, d0, d4	@ Averaging 2 U-values
	VRHADD.U8 d9, d2, d6	@Averaging 2 V-values
	
	VST2.8 	{d1, d3}, [r6]!
	VST2.8 	{d5, d7}, [r7]!

	VMOVL.U8 q10, d9
	VSHL.I16 q10, #8

	VMOVL.U8 q11, d8
	VADD.S16 q10, q10, q11

	VST1.8 	{q11}, [r8]!

	SUBS 	r3, #16
	BGE 	YUVNV12_hexloop

YUVNV12_singles_entry:
	ADDS 	r3, #16
	BEQ 	YUVNV12_linedone

YUVNV12_linedone:
	ADD		r6, r4
	ADD		r7, r4
	ADD		r10, r4, lsl #1
	ADD		r11, r4, lsl #1
	SUBS 	r5, #2
	BGT 	YUVNV12_lineloop

	POP      {r4-r12,pc}

	.text
	.globl Neon_Convert_yuv422_to_YUV420P
@YUV 422 to YUV 420 PLANAR

Neon_Convert_yuv422_to_YUV420P:
	PUSH 	{r4-r12,lr}
	
	MUL 	r4, r2, r3					@ImageSize = iWidth x iHeight
	
	ADD 	r8, r1, r4				@ inBufPtrU = inBufPtrY + ImageSize
	ADD 	r9, r8, r4,LSR #2			@ inBufPtrU = inBufPtrY + ImageSize
	
	MOV 	r10, r0
	MOV 	r6, r1
	MOV 	r4, r2
	MOV 	r5, r3

	ADD 	r7, r6, r4
	ADD 	r11, r10, r4, lsl #1


YUVP_lineloop:

	SUBS 	r3, r4, #16
	BLT 	YUVP_singles_entry

YUVP_hexloop:

	VLD4.8 	{d0, d1, d2, d3}, [r10]!
	VLD4.8 	{d4, d5, d6, d7}, [r11]!

	VRHADD.U8 d8, d0, d4	@ Averaging 2 U-values
	VRHADD.U8 d9, d2, d6	@ Averaging 2 V-values
	
	VST2.8 	{d1, d3}, [r6]!
	VST2.8 	{d5, d7}, [r7]!

	VST1.8 	{d8}, [r8]!
	VST1.8 	{d9}, [r9]!

	SUBS 	r3, #16
	BGE 	YUVP_hexloop

YUVP_singles_entry:
	ADDS 	r3, #16
	BEQ 	YUVP_linedone

YUVP_singles:
	LDR 	r2, [r10], #4

	MOV 	r1, r2
	AND 	r1, r1, #0xFF
	
	LSR		r2, r2, #8
	STRB 	r2, [r6], #1
	
	LSR		r2, r2, #8
	MOV 	r0, r2
	AND 	r0, r0, #0xFF
	
	LSR		r2, r2, #8
	STRB 	r2, [r6], #1
@ ------------
	LDR 	r2, [r11], #4

	MOV 	r12, r2
	AND 	r12, r12, #0xFF
	ADD 	r12, r1
	LSR 	r12, r12, #1
	STRB 	r12, [r8], #1

	LSR		r2, r2, #8
	STRB 	r2, [r7], #1
	
	LSR		r2, r2, #8
	MOV 	r12, r2
	AND 	r12, r12, #0xFF
	ADD 	r12, r0
	LSR 	r12, r12, #1
	STRB 	r12, [r9], #1	

	LSR		r2, r2, #8
	STRB 	r2, [r7], #1
@ ------------
	SUBS 	r3, #2
	BGT		YUVP_singles

YUVP_linedone:
	ADD		r6, r4
	ADD		r7, r4
	ADD		r10, r4, lsl #1
	ADD		r11, r4, lsl #1
	SUBS 	r5, #2
	BGT 	YUVP_lineloop

	POP      {r4-r12,pc}
	



	







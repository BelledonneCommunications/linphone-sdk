/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006-2010  Belledonne Communications SARL (simon.morlat@linphone.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/


#ifdef __ELF__
#   define ELF
#else
#   define ELF @
#endif

.macro  require8 val=1
ELF     .eabi_attribute 24, \val
.endm

.macro  preserve8 val=1
ELF     .eabi_attribute 25, \val
.endm

.macro function name
	.global \name
ELF	.hidden \name
ELF	.type   \name, %function
	.func   \name
\name:
        .endm


.section .rodata
.align 8
ymult:
.word 9535 , 9535, 9535, 9535
rvmult:
.word 13074, 13074, 13074, 13074
gbmult:
.word 6660, 6660, 6660, 6660
gumult:
.word 3203, 3203, 3203, 3203
bumult:
.word 16531, 16531, 16531, 16531


.fpu neon
.text

/*void ms_line_rgb2rgb565_4 (const int16_t *r, const int16_t *g, const int16_t *b, uint16_t *dst, int width)*/
function ms_line_rgb2rgb565_4
		push       {r4}
		ldr			r4	, [sp ,#4]
1:
		vld1.16		{d0}, [r0,:64]!
		vld1.16		{d1}, [r1,:64]!
		vld1.16		{d2}, [r2,:64]!
		vshr.u16	d0, d0, #3
		vshr.u16	d1, d1, #2
		vshr.u16	d2, d2, #3
		vsli.16 		d2, d1, #5	/*inserts g  into d2*/
		vsli.16		d2, d0, #11	/*inserts r into d2 */
		vst1.16	{d2}, [r3,:64]!
		subs		r4,  r4,   #4
		bne 			1b
		pop			{r4}
		bx			lr
.endfunc


/*void ms_line_rgb2rgb565_8 (const int16_t *r, const int16_t *g, const int16_t *b, uint16_t *dst, int width)*/
function ms_line_rgb2rgb565_8
		push       {r4}
		ldr			r4	, [sp ,#4]			/*load width into r4 */
1:
		vld1.16		{d0,d1}, [r0,:64]!
		vshr.u16	q0, q0, #3
		vld1.16		{d2,d3}, [r1,:64]!
		vshr.u16	q1, q1, #2
		vld1.16		{d4,d5}, [r2,:64]!
		vshr.u16	q2, q2, #3
		vsli.16 	q2, q1, #5	/*inserts g  into d2*/
		vsli.16		q2, q0, #11	/*inserts r into d2 */
		vst1.16		{q2}, [r3,:64]!
		subs		r4,  r4,   #8
		bne 			1b
		pop			{r4}
		bx			lr
.endfunc

.macro load_pixels_4_2	d_reg1, d_reg2, src
	add		r12, \src, #2		/* offset to reach next pixels */
	vld1.16		\d_reg1[0], [\src], r4	/* transfer the pixel pointed by r4 into q2 */
	vld1.16		\d_reg1[1], [\src], r5
	vld1.16		\d_reg1[2], [\src], r6
	vld1.16		\d_reg1[3], [\src], r7
	vld1.16		\d_reg2[0], [r12], r4	/* transfer the pixel pointed by r4 into q2 */
	vld1.16		\d_reg2[1], [r12], r5
	vld1.16		\d_reg2[2], [r12], r6
	vld1.16		\d_reg2[3], [r12], r7
.endm


.macro filter_pixels_8 q_srcdst, q_src2
	vsub.s16	q9 , \q_src2, \q_srcdst	/*	q9=x(n+1)-x(n)	*/
	vmul.s16	q10 , q9, q8	/* q10 = coef * q9 */
	vsra.s16	\q_srcdst , q10, #7
	vabs.s16	\q_srcdst , \q_srcdst
.endm

/*void ms_line_scale_8(const uint32_t *grid, const int16_t **src, int16_t **dst int dst_width, int16_t *filter);*/
function ms_line_scale_8
	push {r4-r12,lr}	/* we use lr as a normal register here */
	ldr		lr	, [sp ,#40] /*r4-r12+lr= 10 registers 40=10*4 offset to retrieve filter table*/

	ldm		r1,	{r8,r9}
	ldr		r1,	[r1,#8]

	ldm		r2,	{r10,r11}
	ldr		r2,	[r2,#8]

1:
	
	ldm		r0!, {r4,r5,r6,r7} 	/* load 4 entries of the grid into r4,r5,r6,r7 */

	load_pixels_4_2	d4, d10, r1
	load_pixels_4_2	d6, d12, r8
	load_pixels_4_2	d8, d14, r9

	ldm		r0!, {r4,r5,r6,r7} /* load 4 more entries of the grid into r4,r5,r6,r7 */

	load_pixels_4_2	d5, d11, r1
	load_pixels_4_2	d7, d13, r8
	load_pixels_4_2	d9, d15, r9
				/* x(n)= q2,q3,q4  x(n+1)=q5,q6,q7 */
	vld1.16		{q8} , [lr]!	/* load the filtering coefficients in q8*/
				/* we need to compute (coef*(x(n+1)-x(n)))>>7 + x(n) */

	filter_pixels_8	q2 , q5
	filter_pixels_8	q3 , q6
	filter_pixels_8	q4 , q7

	vst1.16		{q2} , [r2]!	/*write q2 (the 8 selected pixels) into memory pointed by r2*/
	vst1.16		{q3} , [r10]!
	vst1.16		{q4} , [r11]!
	subs		r3,r3,#8	/*we have processed 8 pixels, decrement width*/
	bne		1b
	pop		{r4-r12,pc}
.endfunc



.macro load_pixels_4	d_reg, src
	vld1.16		\d_reg[0], [\src], r4	/* transfer the pixel pointed by r4 into q2 */
	vld1.16		\d_reg[1], [\src], r5
	vld1.16		\d_reg[2], [\src], r6
	vld1.16		\d_reg[3], [\src], r7	
.endm

/*void ms_line_scale_8(const uint32_t *grid, const uint16_t **src, uint16_t **dst int dst_width);*/
function ms_line_scale_simple_8
	push {r4-r11}
	ldr		r8,	[r1,#4]
	ldr		r9,	[r1,#8]
	ldr		r1,	[r1]
	ldr		r10,	[r2,#4]
	ldr		r11,	[r2,#8]
	ldr		r2,	[r2]
1:
	ldrd		r4,r5, [r0],#8	/* load 2 entries of the grid into r4,r5 */
	ldrd		r6,r7, [r0],#8	/* load 2 entries of the grid into r6,r7 */
	
	load_pixels_4   d4, r1
	load_pixels_4	d6, r8
	load_pixels_4	d8, r9

	ldrd		r4,r5, [r0],#8	/* load 2 entries of the grid into r4,r5 */
	ldrd		r6,r7, [r0],#8	/* load 2 entries of the grid into r6,r7 */

	load_pixels_4   d5, r1
	load_pixels_4	d7, r8
	load_pixels_4	d9, r9
	
	vst1.16		{q2} , [r2]!	/*write q2 (the 8 selected pixels) into memory pointed by r2*/
	vst1.16		{q3} , [r10]!
	vst1.16		{q4} , [r11]!
	subs		r3,r3,#8	/*we have processed 8 pixels, decrement width*/
	bne		1b
	pop		{r4-r11}
	bx		lr
.endfunc


.if 0

/* void line_yuv2rgb(uint8_t *y, uint8_t *u, uint8_t *v,  int16_t *r, int16_t *g, int16_t *b, int n) */
function line_yuv2rgb
	push 			{r4-r7}
	ldr				r6, [sp, #12]	/*load n into r6*/
	ldr				r5, [sp, #16]	/*load b into r5*/
	ldr				r4, [sp, #20]	/*load g into r5*/
	vld1.8			d12, [r0]!			/* load 8 y */
	vmovl.u8		q6, d12				/*expand them to 16bits */
	vmovl.u16	q0 , d12				/*expand 4 first of them to 32 bits into q0 */
	vmovl.u16	q1 , d13				/*expand 4 more of them to 32 bits into q1*/
	vld1.8			d12[0], [r1]!		/*load 4 u */ 
	vmovl.u8		q6, d12				/*expand them to 16bits */
	vmovl.u16	q2 , d12				/*expand 4 first of them to 32 bits into q2 */
	vld1.8			d12[0], [r2]!		/*load 4 v */ 
	vmovl.u8		q6, d12				/*expand them to 16bits */
	vmovl.u16	q3 , d12				/*expand 4 first of them to 32 bits into q2 */
				/* at this stage we have y in q0 and q1, u in q2, and v in q3 */
	mov				r7 , # 16
	vdup.32		q4, r7
	vsub.s32		q0 , q0, q4			/*remove bias from y */
	vsub.s32		q1 , q1, q4			/*remove bias from y */
	mov				r7 , # 128
	vdup.32		q4, r7
	vsub.s32		q2 , q2, q4			/*remove bias from u */
	vsub.s32		q3 , q3, q4			/*remove bias from v */
	movrel			r7 , ymult
	vld1.i32		q4 , [r7]
	vmul.s32		q0, q0, q4			/*multiply y with 9535 */
	vmul.s32		q1, q1, q4			/*multiply y with 9535 */
	movrel			r7 , rvmult
	vld1.i32		q4 , [r7]
	/**/
	pop				{r4-r7}
	bx				lr
.endfunc



.endif


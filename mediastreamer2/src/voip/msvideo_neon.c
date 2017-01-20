/*
mediastreamer2 library - modular sound and video processing and streaming
Neon specific video functions
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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/


#include "mediastreamer2/msvideo.h"

#if MS_HAS_ARM

#if MS_HAS_ARM_NEON
#include <arm_neon.h>
#endif

#if defined(__arm__)

#define MATRIX_LOAD_8X8 \
	/*load 8x8 pixel \
	[  0,  1,  2,  3,  4,  5,  6,  7] \
	[  8,  9, 10, 11, 12, 13, 14, 15]\
	[ 16, 17, 18, 19, 20, 21, 22, 23]\
	[ 24, 25, 26, 27, 28, 29, 30, 31]\
	[ 32, 33, 34, 35, 36, 37, 38, 39]\
	[ 40, 41, 42, 43, 44, 45, 46, 47]\
	[ 48, 49, 50, 51, 52, 53, 54, 55]\
	[ 56, 57, 58, 59, 60, 61, 62, 63]*/\
	"vld1.8 {d0},[%0] \n\t"\
	"add     r4, %0, %1 \n\t" /*copy tmp pointer to r4 to avoid src from being changed*/\
	"vld1.8 {d1},[r4] \n\t"\
	"add     r4, r4, %1 \n\t"\
	"vld1.8 {d2},[r4] \n\t"\
	"add     r4, r4, %1 \n\t"\
	"vld1.8 {d3},[r4] \n\t"\
	"add     r4, r4, %1 \n\t"\
	"vld1.8 {d4},[r4] \n\t"\
	"add     r4, r4, %1 \n\t"\
	"vld1.8 {d5},[r4] \n\t"\
	"add     r4, r4, %1 \n\t"\
	"vld1.8 {d6},[r4] \n\t"\
	"add     r4, r4, %1 \n\t"\
	"vld1.8 {d7},[r4] \n\t"
#define MATRIX_TRANSPOSE_8X8 \
	/*build tranposed 2x2 blocks\
	 [  0,  8,  2, 10,  4, 12,  6, 14]\
	 [  1,  9,  3, 11,  5, 13,  7, 15]\
	 [ 16, 24, 18, 26, 20, 28, 22, 30]\
	 [ 17, 25, 19, 27, 21, 29, 23, 31]\
	 [ 32, 40, 34, 42, 36, 44, 38, 46]\
	 [ 33, 41, 35, 43, 37, 45, 39, 47]\
	 [ 48, 56, 50, 58, 52, 60, 54, 62]\
	 [ 49, 57, 51, 59, 53, 61, 55, 63]*/\
	"vzip.8 d0,d1 \n\t"\
	"vzip.8 d2,d3 \n\t"\
	"vzip.8 d4,d5 \n\t"\
	"vzip.8 d6,d7 \n\t"\
	\
	"vzip.32 d0,d1 \n\t"\
	"vzip.32 d2,d3 \n\t"\
	"vzip.32 d4,d5 \n\t"\
	"vzip.32 d6,d7 \n\t"\
	\
	"vzip.16 d0,d1 \n\t"\
	"vzip.16 d2,d3 \n\t"\
	"vzip.16 d4,d5 \n\t"\
	"vzip.16 d6,d7 \n\t"\
	\
	"vzip.32 d0,d1 \n\t"\
	"vzip.32 d2,d3 \n\t"\
	"vzip.32 d4,d5 \n\t"\
	"vzip.32 d6,d7 \n\t"\
	\
	/*assemble 2x2 blocks to form 4x4 blocks\
	 [  0,  8, 16, 24,  2, 10, 18, 26]\
	 [  1,  9, 17, 25,  3, 11, 19, 27]\
	 [  4, 12, 20, 28,  6, 14, 22, 30]\
	 [  5, 13, 21, 29,  7, 15, 23, 31]\
	 [ 32, 40, 48, 56, 34, 42, 50, 58]\
	 [ 33, 41, 49, 57, 35, 43, 51, 59]\
	 [ 36, 44, 52, 60, 38, 46, 54, 62]\
	 [ 37, 45, 53, 61, 39, 47, 55, 63]*/\
	"vzip.16 d0,d2 \n\t"\
	"vzip.16 d1,d3 \n\t"\
	"vzip.16 d4,d6 \n\t"\
	"vzip.16 d5,d7 \n\t"\
	\
	"vzip.32 d0,d2 \n\t"\
	"vzip.32 d1,d3 \n\t"\
	"vzip.32 d4,d6 \n\t"\
	"vzip.32 d5,d7 \n\t"\
	/*assemble 4x4 blocks to form 8x8 blocks\
	 [  0,  8, 16, 24,  4, 12, 20, 28]\
	 [  1,  9, 17, 25,  5, 13, 21, 29]\
	 [  2, 10, 18, 26,  6, 14, 22, 30]\
	 [  3, 11, 19, 27,  7, 15, 23, 31]\
	 [ 32, 40, 48, 56, 36, 44, 52, 60]\
	 [ 33, 41, 49, 57, 37, 45, 53, 61]\
	 [ 34, 42, 50, 58, 38, 46, 54, 62]\
	 [ 35, 43, 51, 59, 39, 47, 55, 63]*/\
	"vzip.32 d0,d4 \n\t"\
	"vzip.32 d1,d5 \n\t"\
	"vzip.32 d2,d6 \n\t"\
	"vzip.32 d3,d7 \n\t"\

#define STORE_8X8 \
	/*store 8x8*/\
	"vst1.8 {d0},[%2] \n\t"\
	"add     r4, %2, %3 \n\t"/*copy tmp pointer to r4 to avoid dest from being changed*/\
	"vst1.8 {d1},[r4] \n\t"\
	"add     r4, r4, %3 \n\t"\
	"vst1.8 {d2},[r4] \n\t"\
	"add     r4, r4, %3 \n\t"\
	"vst1.8 {d3},[r4] \n\t"\
	"add     r4, r4, %3 \n\t"\
	"vst1.8 {d4},[r4] \n\t"\
	"add     r4, r4, %3 \n\t"\
	"vst1.8 {d5},[r4] \n\t"\
	"add     r4, r4, %3 \n\t"\
	"vst1.8 {d6},[r4] \n\t"\
	"add     r4, r4, %3 \n\t"\
	"vst1.8 {d7},[r4] \n\t"

#define VERTICAL_SYMETRIE_8x8 \
	/*vertical symetrie \
	[ 56, 48, 40, 32, 24, 16,  8,  0]\
	[ 57, 49, 41, 33, 25, 17,  9,  1]\
	[ 58, 50, 42, 34, 26, 18, 10,  2]\
	[ 59, 51, 43, 35, 27, 19, 11,  3]\
	[ 60, 52, 44, 36, 28, 20, 12,  4]\
	[ 61, 53, 45, 37, 29, 21, 13,  5]\
	[ 62, 54, 46, 38, 30, 22, 14,  6]\
	[ 63, 55, 47, 39, 31, 23, 15,  7]*/\
	"vrev64.8 q0,q0 \n\t"\
	"vrev64.8 q1,q1 \n\t"\
	"vrev64.8 q2,q2 \n\t"\
	"vrev64.8 q3,q3 \n\t"

#define HORIZONTAL_SYM_AND_STORE_8X8 \
	/*horizontal symetrie + store 8x8*/\
	"vst1.8 {d7},[%2] \n\t"\
	"add     r4, %2, %3 \n\t"/*copy tmp pointer to r4 to avoid dest from being changed*/\
	"vst1.8 {d6},[r4] \n\t"\
	"add     r4, r4, %3 \n\t"\
	"vst1.8 {d5},[r4] \n\t"\
	"add     r4, r4, %3 \n\t"\
	"vst1.8 {d4},[r4] \n\t"\
	"add     r4, r4, %3 \n\t"\
	"vst1.8 {d3},[r4] \n\t"\
	"add     r4, r4, %3 \n\t"\
	"vst1.8 {d2},[r4] \n\t"\
	"add     r4, r4, %3 \n\t"\
	"vst1.8 {d1},[r4] \n\t"\
	"add     r4, r4, %3 \n\t"\
	"vst1.8 {d0},[r4] \n\t"

#define LOAD_16x16_IN_8x8 \
			/*load 16x16 pixels but only keep half   */\
			"vld2.8 {d0,d1},[%0] \n\t"\
			"add     r4, %0, %1 \n\t" /*copy tmp pointer to r4 to avoid src from being changed*/\
			"vld2.8 {d1,d2},[r4] \n\t"\
			"add     r4, r4, %1 \n\t"\
			"vld2.8 {d2,d3},[r4] \n\t"\
			"add     r4, r4, %1 \n\t"\
			"vld2.8 {d3,d4},[r4] \n\t"\
			"add     r4, r4, %1 \n\t"\
			"vld2.8 {d4,d5},[r4] \n\t"\
			"add     r4, r4, %1 \n\t"\
			"vld2.8 {d5,d6},[r4] \n\t"\
			"add     r4, r4, %1 \n\t"\
			"vld2.8 {d6,d7},[r4] \n\t"\
			"add     r4, r4, %1 \n\t"\
			"vld2.8 {d7,d8},[r4] \n\t"


static MS2_INLINE void rotate_block_8x8_clockwise(const unsigned char* src, int src_width, unsigned char* dest,int dest_width) {
#if MS_HAS_ARM_NEON
	__asm  (MATRIX_LOAD_8X8
			MATRIX_TRANSPOSE_8X8
			VERTICAL_SYMETRIE_8x8
			STORE_8X8
		   :/*out*/
		   : "r%"(src),"r"(src_width),"r%"(dest),"r"(dest_width)/*in*/
		   : "r4","d0","d1","d2","d3","d4","d5","d6","d7","memory" /*modified*/
		   );
#endif
}
/*rotate and scale down blocks of 16x16 into 8x8*/
static MS2_INLINE void rotate_and_scale_down_block_16x16_clockwise(const unsigned char* src, int src_width, unsigned char* dest,int dest_width) {
	
#if MS_HAS_ARM_NEON
	__asm  (
			LOAD_16x16_IN_8x8
			MATRIX_TRANSPOSE_8X8
			VERTICAL_SYMETRIE_8x8
			STORE_8X8
			:/*out*/
			: "r%"(src),"r"(src_width*2),"r%"(dest),"r"(dest_width)/*in*/
			: "r4","d0","d1","d2","d3","d4","d5","d6","d7","d8","memory" /*modified*/
			);
#endif
}

/*rotate and scale down blocks of 16x16 into 8x8*/
static MS2_INLINE void rotate_and_scale_down_block_8x8_anticlockwise(const unsigned char* src, int src_width, unsigned char* dest,int dest_width) {
	
#if MS_HAS_ARM_NEON
	__asm  (
			LOAD_16x16_IN_8x8
			MATRIX_TRANSPOSE_8X8
			HORIZONTAL_SYM_AND_STORE_8X8
			:/*out*/
			: "r%"(src),"r"(src_width*2),"r%"(dest),"r"(dest_width)/*in*/
			: "r4","d0","d1","d2","d3","d4","d5","d6","d7","d8","memory" /*modified*/
			);
#endif
}

static MS2_INLINE void rotate_block_8x8_anticlockwise(const unsigned char* src, int src_width, unsigned char* dest,int dest_width) {
#if MS_HAS_ARM_NEON
	__asm  (MATRIX_LOAD_8X8
			MATRIX_TRANSPOSE_8X8
			HORIZONTAL_SYM_AND_STORE_8X8
		   :/*out*/
		   : "r%"(src),"r"(src_width),"r%"(dest),"r"(dest_width)/*in*/
		   : "r4","d0","d1","d2","d3","d4","d5","d6","d7","memory" /*modified*/
		   );
#endif
}

void rotate_down_scale_plane_neon_clockwise(int wDest, int hDest, int full_width, const uint8_t* src, uint8_t* dst, bool_t down_scale) {
#if MS_HAS_ARM_NEON
	char src_block_width=down_scale?16:8;
	char dest_block_width=down_scale?src_block_width/2:src_block_width;
	int hSrc = down_scale?wDest*2:wDest;
	int wSrc = down_scale?hDest*2:hDest;
	int src_incr = full_width*src_block_width;
	int x,y;
	dst += wDest - dest_block_width;
	

	for (y=0; y<hSrc; y+=src_block_width) {
		uint8_t* dst2 = dst;
		for (x=0; x<wSrc; x+=src_block_width) {
			if (down_scale) {
				rotate_and_scale_down_block_16x16_clockwise(src+x,  full_width,dst2,wDest);
			} else {
				rotate_block_8x8_clockwise(src+x,  full_width,dst2,wDest);
			}
			dst2+=(wDest*dest_block_width);
		}
		dst -= dest_block_width;
		src += src_incr;
	}
#else
	ms_error("Neon function '%s' used without hw neon support", __FUNCTION__);
#endif
}

void rotate_down_scale_plane_neon_anticlockwise(int wDest, int hDest, int full_width, const uint8_t* src, uint8_t* dst,bool_t down_scale) {
#if MS_HAS_ARM_NEON
	char src_block_width=down_scale?16:8;
	char dest_block_width=down_scale?src_block_width/2:src_block_width;
	int hSrc = down_scale?wDest*2:wDest;
	int wSrc = down_scale?hDest*2:hDest;
	int src_incr = full_width*src_block_width;
	int x,y;

	dst += wDest * (hDest - dest_block_width);

	for (y=0; y<hSrc; y+=src_block_width) {
		uint8_t* dst2 = dst;
		for (x=0; x<wSrc; x+=src_block_width) {
			if (down_scale) {
				rotate_and_scale_down_block_8x8_anticlockwise(src+x,  full_width,dst2,wDest);
			} else {
				rotate_block_8x8_anticlockwise(src+x,  full_width,dst2,wDest);
			}
			dst2-=wDest*dest_block_width;
		}
		dst += dest_block_width;
		src += src_incr;
	}
#else
	ms_error("Neon function '%s' used without hw neon support", __FUNCTION__);
#endif
}

void rotate_down_scale_cbcr_to_cr_cb(int wDest, int hDest, int full_width, const uint8_t* cbcr_src, uint8_t* cr_dst, uint8_t* cb_dst,bool_t clockWise,bool_t down_scale) {
#if MS_HAS_ARM_NEON
	int hSrc = down_scale?wDest*2:wDest;
	int wSrc = down_scale?hDest*2:hDest;
	int src_stride = 2*full_width;

	int signed_dst_stride;
	int incr;
	int y_step=down_scale?2:1;
	int x,y;

	if (clockWise) {
		/* ms_warning("start writing destination buffer from top right");*/
		cb_dst += wDest - 1;
		cr_dst += wDest - 1;
		incr = 1;
		signed_dst_stride = wDest;
	} else {
		/* ms_warning("start writing destination buffer from top right");*/
		cb_dst += wDest * (hDest - 1);
		cr_dst += wDest * (hDest - 1);
		incr = -1;
		signed_dst_stride = -wDest;
	}

	for (y=0; y<hSrc; y+=y_step) {
		uint8_t* cb_dst2 = cb_dst;
		uint8_t* cr_dst2 = cr_dst;
		for (x=0; x<2*wSrc; x+=16) {
			uint8x8x2_t tmp = vld2_u8 (cbcr_src+x);

			vst1_lane_u8 (cb_dst2, tmp.val[0], 0);
			vst1_lane_u8 (cr_dst2, tmp.val[1], 0);
			cb_dst2+=signed_dst_stride;
			cr_dst2+=signed_dst_stride;
			if (!down_scale) {
				vst1_lane_u8 (cb_dst2, tmp.val[0], 1);
				vst1_lane_u8 (cr_dst2, tmp.val[1], 1);
				cb_dst2+=signed_dst_stride;
				cr_dst2+=signed_dst_stride;
			}
			vst1_lane_u8 (cb_dst2, tmp.val[0], 2);
			vst1_lane_u8 (cr_dst2, tmp.val[1], 2);
			cb_dst2+=signed_dst_stride;
			cr_dst2+=signed_dst_stride;
			if (!down_scale) {
				vst1_lane_u8 (cb_dst2, tmp.val[0], 3);
				vst1_lane_u8 (cr_dst2, tmp.val[1], 3);
				cb_dst2+=signed_dst_stride;
				cr_dst2+=signed_dst_stride;
			}
			vst1_lane_u8 (cb_dst2, tmp.val[0], 4);
			vst1_lane_u8 (cr_dst2, tmp.val[1], 4);
			cb_dst2+=signed_dst_stride;
			cr_dst2+=signed_dst_stride;
			if (!down_scale) {
				vst1_lane_u8 (cb_dst2, tmp.val[0], 5);
				vst1_lane_u8 (cr_dst2, tmp.val[1], 5);
				cb_dst2+=signed_dst_stride;
				cr_dst2+=signed_dst_stride;
			}
			vst1_lane_u8 (cb_dst2, tmp.val[0], 6);
			vst1_lane_u8 (cr_dst2, tmp.val[1], 6);
			cb_dst2+=signed_dst_stride;
			cr_dst2+=signed_dst_stride;
			if (!down_scale) {
				vst1_lane_u8 (cb_dst2, tmp.val[0], 7);
				vst1_lane_u8 (cr_dst2, tmp.val[1], 7);
				cb_dst2+=signed_dst_stride;
				cr_dst2+=signed_dst_stride;
			}
		}
		cb_dst -= incr;
		cr_dst -= incr;
		cbcr_src += src_stride*y_step;
	}
#else
	ms_error("Neon function '%s' used without hw neon support", __FUNCTION__);
#endif
}

#if MS_HAS_ARM_NEON

static void reverse_and_down_scale_32bytes_neon(const unsigned char* src, unsigned char* dest) {
	__asm  (/*load 16x1 pixel
			 [  0,  1,  2,  3,  4,  5,  6,  7, 8, 9, 10, 11, 12, 13, 14, 15]*/
			"vld2.8 {q0,q1},[%0] \n\t"
			/* rev q0
			 [  7,  6,  5,  4,  3,  2,  1, 0, 15, 14, 13, 12, 11, 10, 9, 8]*/
			"vrev64.8 q0,q0 \n\t"
			/* swap d0 d1 */
			"vswp d0,d1 \n\t"
			/* store in dest */
			"vst1.8 {d0,d1},[%1] \n\t"
			:/*out*/
			: "r"(src),"r"(dest)/*in*/
			: "r4","q0","q1","memory" /*modified*/
			);
}

static void reverse_16bytes_neon(const unsigned char* src, unsigned char* dest) {
	__asm  (/*load 16x1 pixel
			[  0,  1,  2,  3,  4,  5,  6,  7, 8, 9, 10, 11, 12, 13, 14, 15]*/
		   "vld1.8 {d0,d1},[%0] \n\t"
			/* rev q0
			[  7,  6,  5,  4,  3,  2,  1, 0, 15, 14, 13, 12, 11, 10, 9, 8]*/
		   "vrev64.8 q0,q0 \n\t"
			/* swap d0 d1 */
			"vswp d0,d1 \n\t"
			/* store in dest */
			"vst1.8 {d0,d1},[%1] \n\t"
			:/*out*/
		   : "r"(src),"r"(dest)/*in*/
		   : "r4","d0","d1","memory" /*modified*/
		   );
}

static void deinterlace_and_reverse_2x8bytes_neon(const unsigned char* src, unsigned char* udest, unsigned char* vdest) {
	__asm  (/*load 16x1 values
			[  U0, V0, U1, V1, U2, V2, U3, V3, U4, V4, U5, V5, U6, V6, U7, V7]
			[  U0, U1, U2, U3, U4, U5, U6, U7, V0, V1, V2, V3, V4, V5, V6, V7]*/
		   "vld2.8 {d0,d1},[%0] \n\t"
			/* rev q0
			[  U7, U6, U5, U4, U3, U2, U1, U0, V7, V6, V5, V4, V3, V2, V1, V0]*/
		   "vrev64.8 q0,q0 \n\t"
			/* store u in udest */
			"vst1.8 {d0},[%1] \n\t"
			/* store v in vdest */
			"vst1.8 {d1},[%2] \n\t"
			:/*out*/
		   : "r"(src),"r"(udest),"r"(vdest)/*in*/
		   : "r4","d0","d1","memory" /*modified*/
		   );
}

static void deinterlace_down_scale_and_reverse_2x16bytes_neon(const unsigned char* src, unsigned char* udest, unsigned char* vdest) {
	__asm  (/*load 32x1 values*/
			
			"vld4.8 {d0,d1,d2,d3},[%0] \n\t" /*only keep half*/ 
			
			/* rev q0
			 [  U7, U6, U5, U4, U3, U2, U1, U0, V7, V6, V5, V4, V3, V2, V1, V0]*/
			"vrev64.8 q0,q0 \n\t"
			/* store u in udest */
			"vst1.8 {d0},[%1] \n\t"
			/* store v in vdest */
			"vst1.8 {d1},[%2] \n\t"
			:/*out*/
			: "r"(src),"r"(udest),"r"(vdest)/*in*/
			: "r4","q0","q1","memory" /*modified*/
			);
}

#endif // MS_HAS_ARM_NEON


void deinterlace_down_scale_and_rotate_180_neon(const uint8_t* ysrc, const uint8_t* cbcrsrc, uint8_t* ydst, uint8_t* udst, uint8_t* vdst, int w, int h, int y_byte_per_row,int cbcr_byte_per_row,bool_t down_scale) {
#if MS_HAS_ARM_NEON
	int y,x;
	int src_h=down_scale?2*h:h;
	int src_w=down_scale?2*w:w;
	int src_uv_h = src_h/2;
	int src_uv_w = src_w;
	char x_src_inc=down_scale?32:16;
	char x_dest_inc=16;
	char y_inc=down_scale?2:1;
	// 180° y rotation

	const uint8_t* src_ptr=ysrc;
	uint8_t* dest_ptr=ydst + h*w; /*start at the end of dest*/
	uint8_t* dest_u_ptr;
	uint8_t* dest_v_ptr;

	for(y=0; y<src_h; y+=y_inc) {
		for(x=0; x<src_w; x+=x_src_inc) {
			dest_ptr-=x_dest_inc;
			if (down_scale) {
				reverse_and_down_scale_32bytes_neon(src_ptr, dest_ptr);
			} else {
				reverse_16bytes_neon(src_ptr, dest_ptr);
			}
			src_ptr+=x_src_inc;

		}
		src_ptr=ysrc+ y*y_byte_per_row;
	}
	// 180° rotation + de-interlace u/v
	src_ptr=cbcrsrc;
	dest_u_ptr=udst + h*w/4; /*start at the end of dest*/
	dest_v_ptr=vdst + h*w/4;
	for(y=0; y<src_uv_h; y+=y_inc) {
		for(x=0; x<src_uv_w; x+=x_src_inc) {
			dest_u_ptr-=x_dest_inc>>1;
			dest_v_ptr-=x_dest_inc>>1;
			if (down_scale) {
				deinterlace_down_scale_and_reverse_2x16bytes_neon(src_ptr, dest_u_ptr, dest_v_ptr);
			} else {
				deinterlace_and_reverse_2x8bytes_neon(src_ptr, dest_u_ptr, dest_v_ptr);
			}
			src_ptr+=x_src_inc;

		}
		src_ptr=cbcrsrc+ y*cbcr_byte_per_row;
	}

#else
	ms_error("Neon function '%s' used without hw neon support", __FUNCTION__);
#endif
}
void deinterlace_and_rotate_180_neon(const uint8_t* ysrc, const uint8_t* cbcrsrc, uint8_t* ydst, uint8_t* udst, uint8_t* vdst, int w, int h, int y_byte_per_row,int cbcr_byte_per_row) {
	return deinterlace_down_scale_and_rotate_180_neon(ysrc, cbcrsrc, ydst, udst, vdst, w, h, y_byte_per_row,cbcr_byte_per_row,FALSE);
}

#endif /* defined(__arm__), the above functions are not used in iOS 64bits, so only the function below is implemented for __arm64__ */

void deinterlace_down_scale_neon(const uint8_t* ysrc, const uint8_t* cbcrsrc, uint8_t* ydst, uint8_t* u_dst, uint8_t* v_dst, int w, int h, int y_byte_per_row,int cbcr_byte_per_row,bool_t down_scale) {
#if MS_HAS_ARM_NEON
    char y_inc  = down_scale?2:1;
    char x_inc  = down_scale?32:16;
    int src_h   = down_scale?2*h:h;
    int src_w   = down_scale?2*w:w;
	int x,y;
	// plain copy
	const uint8_t* ysrc_ptr = ysrc;
	uint8_t* ydest_ptr = ydst;
	const uint8_t* cbcrsrc_ptr = cbcrsrc;
	uint8_t* udest_ptr = u_dst;	
	uint8_t* vdest_ptr = v_dst;
    int crcb_dest_offset=0;

	for(y=0; y<src_h; y+=y_inc) {
		if (down_scale) {
			for(x=0;x<src_w;x+=x_inc) {
				uint8x16x2_t src = vld2q_u8(ysrc_ptr);
				vst1q_u8(ydest_ptr, src.val[0]);
                ysrc_ptr  += 32;
                ydest_ptr += 16;
			}
		} else {
			memcpy(ydest_ptr,ysrc_ptr,w);
			ydest_ptr+=w;
		}
		ysrc_ptr= ysrc +  y* y_byte_per_row;
		
	}
	// de-interlace u/v
	for(y=0; y < (src_h>>1); y+=y_inc) {
		for(x=0;x<src_w;x+=x_inc) {
			if (down_scale) {
				uint8x8x4_t cbr = vld4_u8(cbcrsrc_ptr);
				vst1_u8(udest_ptr, cbr.val[0]);
				vst1_u8(vdest_ptr, cbr.val[1]);
				cbcrsrc_ptr+=32;
				vdest_ptr+=8;
				udest_ptr+=8;
			} else {
				uint8x8x2_t cbr = vld2_u8(cbcrsrc_ptr);
				vst1_u8(udest_ptr, cbr.val[0]);
				vst1_u8(vdest_ptr, cbr.val[1]);
				cbcrsrc_ptr+=16;
				vdest_ptr+=8;
				udest_ptr+=8;
			}
		}
		cbcrsrc_ptr= cbcrsrc +  y * cbcr_byte_per_row;
        crcb_dest_offset+=down_scale?(src_w>>2):(src_w>>1);
        udest_ptr=u_dst + crcb_dest_offset;
        vdest_ptr=v_dst + crcb_dest_offset;
		
	}
#endif
}


#endif


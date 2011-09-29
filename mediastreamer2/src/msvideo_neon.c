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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/


#include "mediastreamer2/msvideo.h"

#ifdef ANDROID
#include <arm_neon.h>
#endif

static inline void rotate_block_8x8_clockwise(unsigned char* src, int src_width, unsigned char* dest,int dest_width) {

	__asm  (/*load 8x8 pixel
			[  0,  1,  2,  3,  4,  5,  6,  7]
			[  8,  9, 10, 11, 12, 13, 14, 15]
			[ 16, 17, 18, 19, 20, 21, 22, 23]
			[ 24, 25, 26, 27, 28, 29, 30, 31]
			[ 32, 33, 34, 35, 36, 37, 38, 39]
			[ 40, 41, 42, 43, 44, 45, 46, 47]
			[ 48, 49, 50, 51, 52, 53, 54, 55]
			[ 56, 57, 58, 59, 60, 61, 62, 63]*/
		   "vld1.8 {d0},[%0] \n\t"
		   "add     r4, %0, %1 \n\t" /*copy tmp pointer to r4 to avoid src from being changed*/
		   "vld1.8 {d1},[r4] \n\t"
		   "add     r4, r4, %1 \n\t"
		   "vld1.8 {d2},[r4] \n\t"
		   "add     r4, r4, %1 \n\t"
		   "vld1.8 {d3},[r4] \n\t"
		   "add     r4, r4, %1 \n\t"
		   "vld1.8 {d4},[r4] \n\t"
		   "add     r4, r4, %1 \n\t"
		   "vld1.8 {d5},[r4] \n\t"
		   "add     r4, r4, %1 \n\t"
		   "vld1.8 {d6},[r4] \n\t"
		   "add     r4, r4, %1 \n\t"
		   "vld1.8 {d7},[r4] \n\t"

		   /*build tranposed 2x2 blocks
			[  0,  8,  2, 10,  4, 12,  6, 14]
			[  1,  9,  3, 11,  5, 13,  7, 15]
			[ 16, 24, 18, 26, 20, 28, 22, 30]
			[ 17, 25, 19, 27, 21, 29, 23, 31]
			[ 32, 40, 34, 42, 36, 44, 38, 46]
			[ 33, 41, 35, 43, 37, 45, 39, 47]
			[ 48, 56, 50, 58, 52, 60, 54, 62]
			[ 49, 57, 51, 59, 53, 61, 55, 63]*/
		   "vzip.8 d0,d1 \n\t"
		   "vzip.8 d2,d3 \n\t"
		   "vzip.8 d4,d5 \n\t"
		   "vzip.8 d6,d7 \n\t"

		   "vzip.32 d0,d1 \n\t"
		   "vzip.32 d2,d3 \n\t"
		   "vzip.32 d4,d5 \n\t"
		   "vzip.32 d6,d7 \n\t"

		   "vzip.16 d0,d1 \n\t"
		   "vzip.16 d2,d3 \n\t"
		   "vzip.16 d4,d5 \n\t"
		   "vzip.16 d6,d7 \n\t"

		   "vzip.32 d0,d1 \n\t"
		   "vzip.32 d2,d3 \n\t"
		   "vzip.32 d4,d5 \n\t"
		   "vzip.32 d6,d7 \n\t"

		   /*assemble 2x2 blocks to form 4x4 blocks
			[  0,  8, 16, 24,  2, 10, 18, 26]
			[  1,  9, 17, 25,  3, 11, 19, 27]
			[  4, 12, 20, 28,  6, 14, 22, 30]
			[  5, 13, 21, 29,  7, 15, 23, 31]
			[ 32, 40, 48, 56, 34, 42, 50, 58]
			[ 33, 41, 49, 57, 35, 43, 51, 59]
			[ 36, 44, 52, 60, 38, 46, 54, 62]
			[ 37, 45, 53, 61, 39, 47, 55, 63]*/
		   "vzip.16 d0,d2 \n\t"
		   "vzip.16 d1,d3 \n\t"
		   "vzip.16 d4,d6 \n\t"
		   "vzip.16 d5,d7 \n\t"

		   "vzip.32 d0,d2 \n\t"
		   "vzip.32 d1,d3 \n\t"
		   "vzip.32 d4,d6 \n\t"
		   "vzip.32 d5,d7 \n\t"
		   /*assemble 4x4 blocks to form 8x8 blocks
			[  0,  8, 16, 24,  4, 12, 20, 28]
			[  1,  9, 17, 25,  5, 13, 21, 29]
			[  2, 10, 18, 26,  6, 14, 22, 30]
			[  3, 11, 19, 27,  7, 15, 23, 31]
			[ 32, 40, 48, 56, 36, 44, 52, 60]
			[ 33, 41, 49, 57, 37, 45, 53, 61]
			[ 34, 42, 50, 58, 38, 46, 54, 62]
			[ 35, 43, 51, 59, 39, 47, 55, 63]*/
		   "vzip.32 d0,d4 \n\t"
		   "vzip.32 d1,d5 \n\t"
		   "vzip.32 d2,d6 \n\t"
		   "vzip.32 d3,d7 \n\t"
		   /*vertical symetrie
			[ 56, 48, 40, 32, 24, 16,  8,  0]
			[ 57, 49, 41, 33, 25, 17,  9,  1]
			[ 58, 50, 42, 34, 26, 18, 10,  2]
			[ 59, 51, 43, 35, 27, 19, 11,  3]
			[ 60, 52, 44, 36, 28, 20, 12,  4]
			[ 61, 53, 45, 37, 29, 21, 13,  5]
			[ 62, 54, 46, 38, 30, 22, 14,  6]
			[ 63, 55, 47, 39, 31, 23, 15,  7]*/
		   "vrev64.8 q0,q0 \n\t"
		   "vrev64.8 q1,q1 \n\t"
		   "vrev64.8 q2,q2 \n\t"
		   "vrev64.8 q3,q3 \n\t"
		   /*store 8x8*/
		   "vst1.8 {d0},[%2] \n\t"
		   "add     r4, %2, %3 \n\t"/*copy tmp pointer to r4 to avoid dest from being changed*/
		   "vst1.8 {d1},[r4] \n\t"
		   "add     r4, r4, %3 \n\t"
		   "vst1.8 {d2},[r4] \n\t"
		   "add     r4, r4, %3 \n\t"
		   "vst1.8 {d3},[r4] \n\t"
		   "add     r4, r4, %3 \n\t"
		   "vst1.8 {d4},[r4] \n\t"
		   "add     r4, r4, %3 \n\t"
		   "vst1.8 {d5},[r4] \n\t"
		   "add     r4, r4, %3 \n\t"
		   "vst1.8 {d6},[r4] \n\t"
			"add     r4, r4, %3 \n\t"
		   "vst1.8 {d7},[r4] \n\t"
		   :/*out*/
		   : "r%"(src),"r"(src_width),"r%"(dest),"r"(dest_width)/*in*/
		   : "r4","d0","d1","d2","d3","d4","d5","d6","d7","memory" /*modified*/
		   );

}

static inline void rotate_block_8x8_anticlockwise(unsigned char* src, int src_width, unsigned char* dest,int dest_width) {

	__asm  (/*load 8x8 pixel
			[  0,  1,  2,  3,  4,  5,  6,  7]
			[  8,  9, 10, 11, 12, 13, 14, 15]
			[ 16, 17, 18, 19, 20, 21, 22, 23]
			[ 24, 25, 26, 27, 28, 29, 30, 31]
			[ 32, 33, 34, 35, 36, 37, 38, 39]
			[ 40, 41, 42, 43, 44, 45, 46, 47]
			[ 48, 49, 50, 51, 52, 53, 54, 55]
			[ 56, 57, 58, 59, 60, 61, 62, 63]*/
		   "vld1.8 {d0},[%0] \n\t"
		   "add     r4, %0, %1 \n\t" /*copy tmp pointer to r4 to avoid src from being changed*/
		   "vld1.8 {d1},[r4] \n\t"
		   "add     r4, r4, %1 \n\t"
		   "vld1.8 {d2},[r4] \n\t"
		   "add     r4, r4, %1 \n\t"
		   "vld1.8 {d3},[r4] \n\t"
		   "add     r4, r4, %1 \n\t"
		   "vld1.8 {d4},[r4] \n\t"
		   "add     r4, r4, %1 \n\t"
		   "vld1.8 {d5},[r4] \n\t"
		   "add     r4, r4, %1 \n\t"
		   "vld1.8 {d6},[r4] \n\t"
		   "add     r4, r4, %1 \n\t"
		   "vld1.8 {d7},[r4] \n\t"

		   /*build tranposed 2x2 blocks
			[  0,  8,  2, 10,  4, 12,  6, 14]
			[  1,  9,  3, 11,  5, 13,  7, 15]
			[ 16, 24, 18, 26, 20, 28, 22, 30]
			[ 17, 25, 19, 27, 21, 29, 23, 31]
			[ 32, 40, 34, 42, 36, 44, 38, 46]
			[ 33, 41, 35, 43, 37, 45, 39, 47]
			[ 48, 56, 50, 58, 52, 60, 54, 62]
			[ 49, 57, 51, 59, 53, 61, 55, 63]*/
		   "vzip.8 d0,d1 \n\t"
		   "vzip.8 d2,d3 \n\t"
		   "vzip.8 d4,d5 \n\t"
		   "vzip.8 d6,d7 \n\t"

		   "vzip.32 d0,d1 \n\t"
		   "vzip.32 d2,d3 \n\t"
		   "vzip.32 d4,d5 \n\t"
		   "vzip.32 d6,d7 \n\t"

		   "vzip.16 d0,d1 \n\t"
		   "vzip.16 d2,d3 \n\t"
		   "vzip.16 d4,d5 \n\t"
		   "vzip.16 d6,d7 \n\t"

		   "vzip.32 d0,d1 \n\t"
		   "vzip.32 d2,d3 \n\t"
		   "vzip.32 d4,d5 \n\t"
		   "vzip.32 d6,d7 \n\t"

		   /*assemble 2x2 blocks to form 4x4 blocks
			[  0,  8, 16, 24,  2, 10, 18, 26]
			[  1,  9, 17, 25,  3, 11, 19, 27]
			[  4, 12, 20, 28,  6, 14, 22, 30]
			[  5, 13, 21, 29,  7, 15, 23, 31]
			[ 32, 40, 48, 56, 34, 42, 50, 58]
			[ 33, 41, 49, 57, 35, 43, 51, 59]
			[ 36, 44, 52, 60, 38, 46, 54, 62]
			[ 37, 45, 53, 61, 39, 47, 55, 63]*/
		   "vzip.16 d0,d2 \n\t"
		   "vzip.16 d1,d3 \n\t"
		   "vzip.16 d4,d6 \n\t"
		   "vzip.16 d5,d7 \n\t"

		   "vzip.32 d0,d2 \n\t"
		   "vzip.32 d1,d3 \n\t"
		   "vzip.32 d4,d6 \n\t"
		   "vzip.32 d5,d7 \n\t"
		   /*assemble 4x4 blocks to form 8x8 blocks
			[  0,  8, 16, 24,  4, 12, 20, 28]
			[  1,  9, 17, 25,  5, 13, 21, 29]
			[  2, 10, 18, 26,  6, 14, 22, 30]
			[  3, 11, 19, 27,  7, 15, 23, 31]
			[ 32, 40, 48, 56, 36, 44, 52, 60]
			[ 33, 41, 49, 57, 37, 45, 53, 61]
			[ 34, 42, 50, 58, 38, 46, 54, 62]
			[ 35, 43, 51, 59, 39, 47, 55, 63]*/
		   "vzip.32 d0,d4 \n\t"
		   "vzip.32 d1,d5 \n\t"
		   "vzip.32 d2,d6 \n\t"
		   "vzip.32 d3,d7 \n\t"
		   /*horizontal symetrie + store 8x8*/
		   "vst1.8 {d7},[%2] \n\t"
		   "add     r4, %2, %3 \n\t"/*copy tmp pointer to r4 to avoid dest from being changed*/
		   "vst1.8 {d6},[r4] \n\t"
		   "add     r4, r4, %3 \n\t"
		   "vst1.8 {d5},[r4] \n\t"
		   "add     r4, r4, %3 \n\t"
		   "vst1.8 {d4},[r4] \n\t"
		   "add     r4, r4, %3 \n\t"
		   "vst1.8 {d3},[r4] \n\t"
		   "add     r4, r4, %3 \n\t"
		   "vst1.8 {d2},[r4] \n\t"
		   "add     r4, r4, %3 \n\t"
		   "vst1.8 {d1},[r4] \n\t"
			"add     r4, r4, %3 \n\t"
		   "vst1.8 {d0},[r4] \n\t"
		   :/*out*/
		   : "r%"(src),"r"(src_width),"r%"(dest),"r"(dest_width)/*in*/
		   : "r4","d0","d1","d2","d3","d4","d5","d6","d7","memory" /*modified*/
		   );

}

void rotate_plane_neon_clockwise(int wDest, int hDest, int full_width, uint8_t* src, uint8_t* dst) {
#define BLOCK_WIDTH 8
	int hSrc = wDest;
	int wSrc = hDest;
	int src_stride = full_width*BLOCK_WIDTH;
	int signed_dst_stride;
	int incr;

	dst += wDest - BLOCK_WIDTH;
	incr = BLOCK_WIDTH;
	signed_dst_stride = wDest;

	int y,x;
	for (y=0; y<hSrc; y+=BLOCK_WIDTH) {
		uint8_t* dst2 = dst;
		for (x=0; x<wSrc; x+=BLOCK_WIDTH) {
			rotate_block_8x8_clockwise(src+x,  full_width,dst2,wDest);
			dst2+=(signed_dst_stride*BLOCK_WIDTH);
		}
		dst -= incr;
		src += src_stride;
	}
}

void rotate_plane_neon_anticlockwise(int wDest, int hDest, int full_width, uint8_t* src, uint8_t* dst) {
#define BLOCK_WIDTH 8
	int hSrc = wDest;
	int wSrc = hDest;
	int src_stride = full_width*BLOCK_WIDTH;

	int signed_dst_stride;
	int incr;

	dst += wDest * (hDest - 1);
	incr = -BLOCK_WIDTH;
	signed_dst_stride = -wDest;

	int y,x;
	for (y=0; y<hSrc; y+=BLOCK_WIDTH) {
		uint8_t* dst2 = dst;
		for (x=0; x<wSrc; x+=BLOCK_WIDTH) {
			rotate_block_8x8_anticlockwise(src+x,  full_width,dst2,wDest);
			dst2+=(signed_dst_stride*BLOCK_WIDTH);
		}
		dst -= incr;
		src += src_stride;
	}
}

void rotate_cbcr_to_cr_cb(int wDest, int hDest, int full_width, uint8_t* cbcr_src, uint8_t* cr_dst, uint8_t* cb_dst,bool_t clockWise) {
	int hSrc = wDest;
	int wSrc = hDest;
	int src_stride = 2*full_width;

	int signed_dst_stride;
	int incr;



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

	int x,y;
	for (y=0; y<hSrc; y++) {
		uint8_t* cb_dst2 = cb_dst;
		uint8_t* cr_dst2 = cr_dst;
		for (x=0; x<2*wSrc; x+=16) {
			uint8x8x2_t tmp = vld2_u8 (cbcr_src+x);

			vst1_lane_u8 (cb_dst2, tmp.val[0], 0);
			vst1_lane_u8 (cr_dst2, tmp.val[1], 0);
			cb_dst2+=signed_dst_stride;
			cr_dst2+=signed_dst_stride;
			vst1_lane_u8 (cb_dst2, tmp.val[0], 1);
			vst1_lane_u8 (cr_dst2, tmp.val[1], 1);
			cb_dst2+=signed_dst_stride;
			cr_dst2+=signed_dst_stride;
			vst1_lane_u8 (cb_dst2, tmp.val[0], 2);
			vst1_lane_u8 (cr_dst2, tmp.val[1], 2);
			cb_dst2+=signed_dst_stride;
			cr_dst2+=signed_dst_stride;
			vst1_lane_u8 (cb_dst2, tmp.val[0], 3);
			vst1_lane_u8 (cr_dst2, tmp.val[1], 3);
			cb_dst2+=signed_dst_stride;
			cr_dst2+=signed_dst_stride;
			vst1_lane_u8 (cb_dst2, tmp.val[0], 4);
			vst1_lane_u8 (cr_dst2, tmp.val[1], 4);
			cb_dst2+=signed_dst_stride;
			cr_dst2+=signed_dst_stride;
			vst1_lane_u8 (cb_dst2, tmp.val[0], 5);
			vst1_lane_u8 (cr_dst2, tmp.val[1], 5);
			cb_dst2+=signed_dst_stride;
			cr_dst2+=signed_dst_stride;
			vst1_lane_u8 (cb_dst2, tmp.val[0], 6);
			vst1_lane_u8 (cr_dst2, tmp.val[1], 6);
			cb_dst2+=signed_dst_stride;
			cr_dst2+=signed_dst_stride;
			vst1_lane_u8 (cb_dst2, tmp.val[0], 7);
			vst1_lane_u8 (cr_dst2, tmp.val[1], 7);
			cb_dst2+=signed_dst_stride;
			cr_dst2+=signed_dst_stride;

		}
		cb_dst -= incr;
		cr_dst -= incr;
		cbcr_src += src_stride;
	}
}

static void reverse_16bytes_neon(unsigned char* src, unsigned char* dest) {

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

static void deinterlace_and_reverse_2x8bytes_neon(unsigned char* src, unsigned char* udest, unsigned char* vdest) {
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


void deinterlace_and_rotate_180_neon(uint8_t* ysrc, uint8_t* cbcrsrc, uint8_t* ydst, uint8_t* udst, uint8_t* vdst, int w, int h, int y_byte_per_row,int cbcr_byte_per_row) {
	int i,j;

	int uv_w = w/2;
	int uv_h = h/2;

	// 180° y rotation
	for(i=0; i<h; i++) {
		for(j=0; j<w/16; j++) {
			int src_index = (h - (i+1))*y_byte_per_row + w - (j + 1)*16;
			int dst_index = i*w + j*16;

			reverse_16bytes_neon((uint8_t*)&ysrc[src_index], &ydst[dst_index]);
		}
	}
	// 180° rotation + de-interlace u/v
	for (i=0; i<uv_h; i++) {
		for(j=0; j<w/16; j++) {
			int src_index = (uv_h - (i+1))*cbcr_byte_per_row + w - (j + 1)*16;
			int dst_index = i*uv_w + j*16/2;

			deinterlace_and_reverse_2x8bytes_neon((uint8_t*)&cbcrsrc[src_index], &udst[dst_index], &vdst[dst_index]);
		}
	}
}


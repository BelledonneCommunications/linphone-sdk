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

#include "mediastreamer2/msvideo.h"

#include <arm_neon.h>

#define ARM 1

void ms_line_rgb2rgb565_4(const int16_t *r, const int16_t *g, const int16_t *b, uint16_t *dst, int width);
void ms_line_rgb2rgb565_8(const int16_t *r, const int16_t *g, const int16_t *b, uint16_t *dst, int width);

typedef struct AndroidScalerCtx{
	MSVideoSize src_size;
	MSVideoSize dst_size;
	int16_t *unscaled_2lines[3];
	int16_t *hscaled_img[3];
	int hscaled_img_stride;
	int unscaled_stride;
	int w_inc;
	int h_inc;
}AndroidScalerCtx;

#define ROUND_UP(i,p)	((i+(p-1)) & ~(p-1))

#define PAD 16

static int32_t premult_y[256];
static int32_t premult_vr[256];
static int32_t premult_vg[256];
static int32_t premult_ug[256];
static int32_t premult_ub[256];
static bool_t premult_initd=FALSE;

static void init_premults(){
	int i;
	for(i=0;i<256;++i){
		premult_y[i]=(i-16)*9535;
		premult_vr[i]=(i-128)*13074;
		premult_vg[i]=-(i-128)*6660;
		premult_ug[i]=-(i-128)*3203;
		premult_ub[i]=(i-128)*16531;
	}
}

static inline void line_yuv2rgb(uint8_t *y, uint8_t *u, uint8_t *v,  int16_t *r, int16_t *g, int16_t *b, int n);

static int32_t yuvmax[4]={255,255,255,255};

#ifndef ARM

static inline void yuv2rgb_4x2(const uint8_t *y1, const uint8_t *y2, const uint8_t *u, const uint8_t *v, int16_t *r1, int16_t *g1, int16_t *b1, int16_t *r2, int16_t *g2, int16_t *b2){
	int32_t py1[4];
	int32_t py2[4];
	int32_t pvr[4];
	int32_t pvug[4];
	int32_t pub[4];
	int i,j;

	for(i=0,j=0;i<2;++i,j+=2){
		int val_v=v[i];
		int val_u=u[i];
		py1[j]=premult_y[y1[j]];
		py1[j+1]=premult_y[y1[j+1]];
		py2[j]=premult_y[y2[j]];
		py2[j+1]=premult_y[y2[j+1]];
		pvr[j]=pvr[j+1]=premult_vr[val_v];
		pvug[j]=pvug[j+1]=premult_vg[val_v]+premult_ug[val_u];
		pub[j]=pub[j+1]=premult_ub[val_u];
	}

	for(i=0;i<4;++i){
		r1[i]=MAX(MIN(((py1[i]+pvr[i])>>13) ,255),0);
		g1[i]=MAX(MIN(((py1[i]+pvug[i])>>13) ,255),0);
		b1[i]=MAX(MIN(((py1[i]+pub[i])>>13)  ,255),0);
		r2[i]=MAX(MIN(((py2[i]+pvr[i])>>13) ,255),0);
		g2[i]=MAX(MIN(((py2[i]+pvug[i])>>13) ,255),0);
		b2[i]=MAX(MIN(((py2[i]+pub[i])>>13) ,255),0);
	}
}

#else

#define LOAD_Y_PREMULTS(i) \
	ry1=vld1q_lane_s32(&premult_y[y1[i]],ry1,i); \
	ry2=vld1q_lane_s32(&premult_y[y2[i]],ry2,i); 

#define LOAD_UV_PREMULTS(i) \
{\
		int tmp=premult_vr[v[i]];\
		rvr=vld1q_lane_s32(&tmp,rvr,2*i); \
		rvr=vld1q_lane_s32(&tmp,rvr,2*i+1); \
		tmp=premult_vg[v[i]]+premult_ug[u[i]]; \
		rvug=vld1q_lane_s32(&tmp,rvug,2*i); \
		rvug=vld1q_lane_s32(&tmp,rvug,2*i+1); \
		tmp=premult_ub[u[i]]; \
		rub=vld1q_lane_s32(&tmp,rub,2*i); \
		rub=vld1q_lane_s32(&tmp,rub,2*i+1); \
}

static inline void yuv2rgb_4x2(const uint8_t *y1, const uint8_t *y2, const uint8_t *u, const uint8_t *v, int16_t *r1, int16_t *g1, int16_t *b1, int16_t *r2, int16_t *g2, int16_t *b2){
	int32x4_t ry1;
	int32x4_t ry2;
	int32x4_t rvug;
	int32x4_t rvr;
	int32x4_t rub;
	int32x4_t rr1,rg1,rb1,rr2,rg2,rb2;
	int32x4_t max;
	int16x4_t res1,res2;
	int i,j;

	LOAD_Y_PREMULTS(0)
	LOAD_Y_PREMULTS(1)
	LOAD_Y_PREMULTS(2)
	LOAD_Y_PREMULTS(3)

	LOAD_UV_PREMULTS(0)
	LOAD_UV_PREMULTS(1)

	rr1=vaddq_s32(ry1,rvr);
	rr2=vaddq_s32(ry2,rvr);
	rg1=vaddq_s32(ry1,rvug);
	rg2=vaddq_s32(ry2,rvug);
	rb1=vaddq_s32(ry1,rub);
	rb2=vaddq_s32(ry2,rub);

	max=vld1q_s32(yuvmax);
	
	rr1=vminq_s32(vabsq_s32(vshrq_n_s32(rr1,13)),max);
	rr2=vminq_s32(vabsq_s32(vshrq_n_s32(rr2,13)),max);
	rg1=vminq_s32(vabsq_s32(vshrq_n_s32(rg1,13)),max);
	rg2=vminq_s32(vabsq_s32(vshrq_n_s32(rg2,13)),max);
	rb1=vminq_s32(vabsq_s32(vshrq_n_s32(rb1,13)),max);
	rb2=vminq_s32(vabsq_s32(vshrq_n_s32(rb2,13)),max);
	
	res1=vmovn_s32(rr1);
	res2=vmovn_s32(rr2);
	vst1_s16(r1,res1);
	vst1_s16(r2,res2);

	res1=vmovn_s32(rg1);
	res2=vmovn_s32(rg2);
	vst1_s16(g1,res1);
	vst1_s16(g2,res2);

	res1=vmovn_s32(rb1);
	res2=vmovn_s32(rb2);
	vst1_s16(b1,res1);
	vst1_s16(b2,res2);
}

#endif

static void line_yuv2rgb_2(const uint8_t *src_lines[],  int src_strides[], int16_t *dst_lines[], int src_w, int dst_stride ){
	int i;
	int uv_offset;
	int16_t *line2[3]={dst_lines[0]+dst_stride,dst_lines[1]+dst_stride,dst_lines[2]+dst_stride};
	
	for(i=0;i<src_w;i+=4){
		uv_offset=i>>1;
		yuv2rgb_4x2(src_lines[0]+i,
		            src_lines[0]+src_strides[0]+i,
		            src_lines[1]+uv_offset,
		            src_lines[2]+uv_offset,
		            dst_lines[0]+i,
		            dst_lines[1]+i,
		            dst_lines[2]+i,
		         	line2[0]+i,
		            line2[1]+i,
		            line2[2]+i);
	}
}

/*horizontal scaling of a single line (with 3 color planes)*/
static void line_horizontal_scale(int16_t *src_lines[], int16_t *dst_lines[], int dst_w, int inc){
	int x=0;
	int i,pos;
	
	for(i=0;i<dst_w;++i){
		pos=x>>16;
		x+=inc;
		dst_lines[0][i]=src_lines[0][pos];
		dst_lines[1][i]=src_lines[1][pos];
		dst_lines[2][i]=src_lines[2][pos];
	}
}

/* horizontal scaling of the entire image */
static void img_yuv2rgb_hscale(AndroidScalerCtx * ctx, uint8_t *src[], int src_strides[]){
	int i;
	const uint8_t *p_src[3];
	int16_t *p_dst[3];
	int16_t *line2[3];


	p_src[0]=src[0];
	p_src[1]=src[1];
	p_src[2]=src[2];
	p_dst[0]=ctx->hscaled_img[0];
	p_dst[1]=ctx->hscaled_img[1];
	p_dst[2]=ctx->hscaled_img[2];

	line2[0]=ctx->unscaled_2lines[0]+ctx->unscaled_stride;
	line2[1]=ctx->unscaled_2lines[1]+ctx->unscaled_stride;
	line2[2]=ctx->unscaled_2lines[2]+ctx->unscaled_stride;
	
	for(i=0;i<ctx->src_size.height;i+=2){
		/* this will convert two lines of yuv into 2 lines of rgb*/
		line_yuv2rgb_2(p_src,src_strides,ctx->unscaled_2lines,ctx->src_size.width,ctx->unscaled_stride);
		p_src[0]+=2*src_strides[0];
		p_src[1]+=src_strides[1];
		p_src[2]+=src_strides[2];
		
		line_horizontal_scale(ctx->unscaled_2lines,p_dst,ctx->dst_size.width,ctx->w_inc);
		p_dst[0]+=ctx->hscaled_img_stride;
		p_dst[1]+=ctx->hscaled_img_stride;
		p_dst[2]+=ctx->hscaled_img_stride;
		line_horizontal_scale(line2,p_dst,ctx->dst_size.width,ctx->w_inc);
		p_dst[0]+=ctx->hscaled_img_stride;
		p_dst[1]+=ctx->hscaled_img_stride;
		p_dst[2]+=ctx->hscaled_img_stride;
	}
}

#ifndef ARM

void ms_line_rgb2rgb565(const int16_t *r, const int16_t *g, const int16_t *b, uint16_t *dst, int width){
	int i;

#if 0
	for(i=0;i<width;i+=4){
		asm volatile(
		             "mov			r0,%[p_r]				\n\t"
		             "mov			r1,%[p_g]				\n\t"
		             "mov			r2,%[p_b]				\n\t"
		             "vld1.16		d0, r0			\n\t"
		             "vld1.16		d1, r1			\n\t"
		             "vld1.16		d2, r2			\n\t"
		             "vshr.u16	d0, d0, #3	\n\t"
		             "vshr.u16	d1, d1, #2	\n\t"
		             "vshr.u16	d2, d2, #3	\n\t"
		             "vsli.16 		d2, d1, #5	\n\t" /*inserts g  into d2*/
		             "vsli.16		d2,d0,  #11	\n\t" /*inserts r into d2 */
		             "vst1.16		%[out], d2	\n\t"
		             : [out]"=m"(&dst[i])
		             : [p_r] "r" (r) , [p_g] "r" (g), [p_b] "r" (b)
		             : "r0", "r1", "r2", "d0", "d1", "d2"
		);
	}
#else
	for(i=0;i<width;++i){
		uint16_t vr=(uint16_t)r[i]>>3;
		uint16_t vg=(uint16_t)g[i]>>2;
		uint16_t vb=(uint16_t)b[i]>>3;
		dst[i]=(vr<<11)|(vg<<5)|vb;
	}
#endif
}

#else
static inline void ms_line_rgb2rgb565(const int16_t *r, const int16_t *g, const int16_t *b, uint16_t *dst, int width){
	ms_line_rgb2rgb565_8(r,g,b,dst,ROUND_UP(width,8));
}
#endif

static void img_yuv2rgb565_scale(AndroidScalerCtx *ctx, uint8_t *src[], int src_strides[], uint8_t *dst[], int dst_strides[]){
	int i,pos,y=0;
	int16_t *p_src[3];
	uint8_t *p_dst=dst[0];
	int offset;
	
	/*scale the entire image horizontally into some temporary buffers*/
	img_yuv2rgb_hscale(ctx,src,src_strides);
	/*write lines as rgb565 format*/
	for(i=0;i<ctx->dst_size.height;++i){
		pos=y>>16;
		offset=pos*ctx->hscaled_img_stride;
		p_src[0]=ctx->hscaled_img[0]+offset;
		p_src[1]=ctx->hscaled_img[1]+offset;
		p_src[2]=ctx->hscaled_img[2]+offset;
		ms_line_rgb2rgb565(p_src[0],p_src[1],p_src[2],(uint16_t*)p_dst,ctx->dst_size.width);
		y+=ctx->h_inc;
		p_dst+=dst_strides[0];
	}
}

static MSScalerContext *android_create_scaler_context(int src_w, int src_h, MSPixFmt src_fmt, int dst_w, int dst_h, MSPixFmt dst_fmt, int flags){
	AndroidScalerCtx *ctx=ms_new0(AndroidScalerCtx,1);
	int i;

	if (!premult_initd){
		init_premults();
		premult_initd=TRUE;
	}
	if (src_fmt!=MS_YUV420P && dst_fmt!=MS_RGB565){
		ms_fatal("FIXME: unsupported rescaling scheme.");
		ms_free(ctx);
		return NULL;
	}
	ctx->src_size.width=src_w;
	ctx->src_size.height=src_h;
	ctx->dst_size.width=dst_w;
	ctx->dst_size.height=dst_h;

	ctx->hscaled_img_stride=ROUND_UP(dst_w,PAD);
	ctx->unscaled_stride=ROUND_UP(src_w,PAD);
	for(i=0;i<3;++i){
		ctx->unscaled_2lines[i]=ms_new(int16_t,ROUND_UP(src_w,PAD)*2);
		ctx->hscaled_img[i]=ms_new(int16_t,ctx->hscaled_img_stride*dst_h);
	}
	ctx->w_inc=(src_w<<16)/dst_w;
	ctx->h_inc=(src_h<<16)/dst_h;
	return (MSScalerContext*)ctx;
}

static void android_scaler_context_free(MSScalerContext *c){
	AndroidScalerCtx *ctx=(AndroidScalerCtx*)c;
	int i;
	for(i=0;i<3;++i){
		ms_free(ctx->unscaled_2lines[i]);
		ms_free(ctx->hscaled_img[i]);
	}
	ms_free(ctx);
}

static int android_scaler_process(MSScalerContext *ctx, uint8_t *src[], int src_strides[], uint8_t *dst[], int dst_strides[]){
	img_yuv2rgb565_scale((AndroidScalerCtx *)ctx,src,src_strides,dst,dst_strides);
	return 0;
}

MSScalerDesc ms_android_scaler={
	android_create_scaler_context,
	android_scaler_process,
	android_scaler_context_free
};

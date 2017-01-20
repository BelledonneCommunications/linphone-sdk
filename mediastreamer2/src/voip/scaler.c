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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "mediastreamer2/msvideo.h"
#if MS_HAS_ARM_NEON
#include <arm_neon.h>
#endif

void ms_line_rgb2rgb565_4(const int16_t *r, const int16_t *g, const int16_t *b, uint16_t *dst, int width);
void ms_line_rgb2rgb565_8(const int16_t *r, const int16_t *g, const int16_t *b, uint16_t *dst, int width);

void ms_line_scale_8(const uint32_t *grid, const int16_t * const src[], int16_t *dst[], int dst_width, const int16_t *filter);
void ms_line_scale_simple_8(const uint32_t *grid, const int16_t * const src[], int16_t *dst[], int dst_width);

typedef struct AndroidScalerCtx{
	MSVideoSize src_size;
	MSVideoSize dst_size;
	int16_t *unscaled_2lines[3];
	int16_t *hscaled_img[3];
	uint32_t *hgrid;
	int16_t *hcoeffs;
	int hscaled_img_stride;
	int unscaled_stride;
	int w_inc;
	int h_inc;
	int dst_w_padded;
}AndroidScalerCtx;


#define ROUND_UP(i,p)	((i+(p-1)) & ~(p-1))

#define PAD 16

static int32_t premult_y[256];
static int32_t premult_vr[256];
static int32_t premult_vg[256];
static int32_t premult_ug[256];
static int32_t premult_ub[256];
static bool_t premult_initd=FALSE;

static void init_premults(void){
	int i;
	for(i=0;i<256;++i){
		premult_y[i]=(i-16)*9535;
		premult_vr[i]=(i-128)*13074;
		premult_vg[i]=-(i-128)*6660;
		premult_ug[i]=-(i-128)*3203;
		premult_ub[i]=(i-128)*16531;
	}
}


#if !MS_HAS_ARM_NEON

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
		int tmp=premult_vr[v[i]]; \
		rvr=vsetq_lane_s32(tmp,rvr,2*i); \
		rvr=vsetq_lane_s32(tmp,rvr,2*i+1); \
		tmp=premult_vg[v[i]]+premult_ug[u[i]]; \
		rvug=vsetq_lane_s32(tmp,rvug,2*i); \
		rvug=vsetq_lane_s32(tmp,rvug,2*i+1); \
		tmp=premult_ub[u[i]]; \
		rub=vsetq_lane_s32(tmp,rub,2*i); \
		rub=vsetq_lane_s32(tmp,rub,2*i+1); \
}
#endif

#if MS_HAS_ARM_NEON
static int32_t yuvmax[4]={255<<13,255<<13,255<<13,255<<13};

static inline void yuv2rgb_4x2(const uint8_t *y1, const uint8_t *y2, const uint8_t *u, const uint8_t *v, int16_t *r1, int16_t *g1, int16_t *b1, int16_t *r2, int16_t *g2, int16_t *b2){
	int32x4_t ry1={0};
	int32x4_t ry2={0};
	int32x4_t rvug={0};
	int32x4_t rvr={0};
	int32x4_t rub={0};
	int32x4_t rr1={0},rg1={0},rb1={0},rr2={0},rg2={0},rb2={0};
	int32x4_t max={0};

	LOAD_Y_PREMULTS(0)
	LOAD_Y_PREMULTS(1)
	LOAD_Y_PREMULTS(2)
	LOAD_Y_PREMULTS(3)

	LOAD_UV_PREMULTS(0)
	LOAD_UV_PREMULTS(1)

	max=vld1q_s32(yuvmax);
	/*the following does not work */
	//max=vdupq_n_s32(255);

	rr1=vaddq_s32(ry1,rvr);
	rr2=vaddq_s32(ry2,rvr);
	rg1=vaddq_s32(ry1,rvug);
	rg2=vaddq_s32(ry2,rvug);
	rb1=vaddq_s32(ry1,rub);
	rb2=vaddq_s32(ry2,rub);



	rr1=vminq_s32(vabsq_s32(rr1),max);
	rr2=vminq_s32(vabsq_s32(rr2),max);
	rg1=vminq_s32(vabsq_s32(rg1),max);
	rg2=vminq_s32(vabsq_s32(rg2),max);
	rb1=vminq_s32(vabsq_s32(rb1),max);
	rb2=vminq_s32(vabsq_s32(rb2),max);

	vst1_s16(r1,vqshrn_n_s32(rr1,13));
	vst1_s16(r2,vqshrn_n_s32(rr2,13));

	vst1_s16(g1,vqshrn_n_s32(rg1,13));
	vst1_s16(g2,vqshrn_n_s32(rg2,13));

	vst1_s16(b1,vqshrn_n_s32(rb1,13));
	vst1_s16(b2,vqshrn_n_s32(rb2,13));
}


#endif

void img_ycrcb420p_to_bgra(uint8_t* src[],unsigned short w,unsigned short h, uint32_t dest[]) {
	unsigned int offset_y=0;
	unsigned int offset_dest=0;
	unsigned int offset_cbcr=0;
	int row;
	int i;
	if (premult_initd == FALSE) {
		init_premults();
	}
	for (row=0;row<h;row+=2) {
		int col_crcb=0;
		int col_y;
		offset_y=row*w;
		offset_cbcr=offset_y>>2;
		for (col_y=0;col_y<w;col_y+=4) {
			int16_t r1[4],  g1[4],  b1[4],  r2[4],  g2[4],  b2[4];
			yuv2rgb_4x2(src[0]+offset_y+col_y
						,src[0]+offset_y+w+col_y
						,(src[1]+offset_cbcr+col_crcb)
						,(src[2]+offset_cbcr+col_crcb)
						,r1
						,g1
						,b1
						,r2
						,g2
						,b2);
			for (i =0;i<4;i++) {
				*(uint8_t*)(dest+offset_dest+i) = b1[i];
				*((uint8_t*)(dest+offset_dest+i)+1) = g1[i];
				*((uint8_t*)(dest+offset_dest+i)+2) = r1[i];
				*((uint8_t*)(dest+offset_dest+i)+3) = 255;
			
				*(uint8_t*)(dest+w+offset_dest+i) = b2[i];
				*((uint8_t*)(dest+w+offset_dest+i)+1) = g2[i];
				*((uint8_t*)(dest+w+offset_dest+i)+2) = r2[i];
				*((uint8_t*)(dest+w+offset_dest+i)+3) = 255;
			}
			col_crcb+=2;
			offset_dest=offset_y+col_y;
			
		}
	}
}

static inline void line_yuv2rgb_2(const uint8_t *src_lines[],  int src_strides[], int16_t *dst_lines[], int src_w, int dst_stride ){
	int i;
	int16_t *line2[3]={dst_lines[0]+dst_stride,dst_lines[1]+dst_stride,dst_lines[2]+dst_stride};

	const uint8_t *y1,*y2,*u,*v;

	y1=src_lines[0];
	y2=src_lines[0]+src_strides[0];
	u= src_lines[1];
	v= src_lines[2];

	for(i=0;i<src_w;i+=4){
		yuv2rgb_4x2(y1,
		            y2,
		            u,
		            v,
		            dst_lines[0]+i,
		            dst_lines[1]+i,
		            dst_lines[2]+i,
				line2[0]+i,
		            line2[1]+i,
		            line2[2]+i);
		y1+=4;
		y2+=4;
		u+=2;
		v+=2;
	}
}

/*horizontal scaling of a single line (with 3 color planes)*/
static inline void line_horizontal_scale(AndroidScalerCtx * ctx, int16_t *src_lines[], int16_t *dst_lines[]){
#if MS_HAS_ARM_NEON_32
	//ms_line_scale_simple_8(ctx->hgrid,src_lines,dst_lines,ctx->dst_w_padded);
	ms_line_scale_8(ctx->hgrid,(const int16_t * const*)src_lines,dst_lines,ctx->dst_w_padded,ctx->hcoeffs);
#else
	int dst_w=ctx->dst_size.width;
	int x=0;
	int i,pos;
	int inc=ctx->w_inc;

	for(i=0;i<dst_w;++i){
		pos=x>>16;
		x+=inc;
		dst_lines[0][i]=src_lines[0][pos];
		dst_lines[1][i]=src_lines[1][pos];
		dst_lines[2][i]=src_lines[2][pos];
	}
#endif
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

		line_horizontal_scale(ctx,ctx->unscaled_2lines,p_dst);
		p_dst[0]+=ctx->hscaled_img_stride;
		p_dst[1]+=ctx->hscaled_img_stride;
		p_dst[2]+=ctx->hscaled_img_stride;
		line_horizontal_scale(ctx,line2,p_dst);
		p_dst[0]+=ctx->hscaled_img_stride;
		p_dst[1]+=ctx->hscaled_img_stride;
		p_dst[2]+=ctx->hscaled_img_stride;
	}
}

#if !MS_HAS_ARM_NEON_32

void ms_line_rgb2rgb565(const int16_t *r, const int16_t *g, const int16_t *b, uint16_t *dst, int width){
	int i;
	for(i=0;i<width;++i){
		uint16_t vr=(uint16_t)r[i]>>3;
		uint16_t vg=(uint16_t)g[i]>>2;
		uint16_t vb=(uint16_t)b[i]>>3;
		dst[i]=(vr<<11)|(vg<<5)|vb;
	}
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
#if MS_HAS_ARM_NEON_32
		ms_line_rgb2rgb565_8(p_src[0],p_src[1],p_src[2],(uint16_t*)p_dst,ctx->dst_w_padded);
#else
		ms_line_rgb2rgb565(p_src[0],p_src[1],p_src[2],(uint16_t*)p_dst,ctx->dst_size.width);
#endif
		y+=ctx->h_inc;
		p_dst+=dst_strides[0];
	}
}

static MSScalerContext *android_create_scaler_context(int src_w, int src_h, MSPixFmt src_fmt, int dst_w, int dst_h, MSPixFmt dst_fmt, int flags){
	AndroidScalerCtx *ctx=ms_new0(AndroidScalerCtx,1);
	int i;
	int tmp,prev;

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
	/*compute the grid (map) for original lines into destination lines*/
	ctx->dst_w_padded=ROUND_UP(dst_w,PAD);
	ctx->hgrid=ms_new0(uint32_t,ctx->dst_w_padded);
	ctx->hcoeffs=ms_new0(int16_t,ctx->dst_w_padded);
	tmp=0;
	prev=0;
	for(i=0;i<dst_w;++i){
		int offset=(tmp>>16)*2;
		ctx->hgrid[i]=offset-prev;
		ctx->hcoeffs[i]=(tmp&0xffff)>>9;
		prev=offset;
		tmp+=ctx->w_inc;
	}

	return (MSScalerContext*)ctx;
}

static void android_scaler_context_free(MSScalerContext *c){
	AndroidScalerCtx *ctx=(AndroidScalerCtx*)c;
	int i;
	for(i=0;i<3;++i){
		ms_free(ctx->unscaled_2lines[i]);
		ms_free(ctx->hscaled_img[i]);
	}
	ms_free(ctx->hgrid);
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

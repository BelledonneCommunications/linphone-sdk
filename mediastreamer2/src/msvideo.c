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
#if !defined(NO_FFMPEG)
#include "ffmpeg-priv.h"
#endif

#ifdef WIN32
#include <malloc.h>
#endif

#ifdef __ARM_NEON__
#include <arm_neon.h>
#endif
static void yuv_buf_init(YuvBuf *buf, int w, int h, uint8_t *ptr){
	int ysize,usize;
	ysize=w*h;
	usize=ysize/4;
	buf->w=w;
	buf->h=h;
	buf->planes[0]=ptr;
	buf->planes[1]=buf->planes[0]+ysize;
	buf->planes[2]=buf->planes[1]+usize;
	buf->planes[3]=0;
	buf->strides[0]=w;
	buf->strides[1]=w/2;
	buf->strides[2]=buf->strides[1];
	buf->strides[3]=0;
}

int ms_yuv_buf_init_from_mblk(YuvBuf *buf, mblk_t *m){
	int size=m->b_wptr-m->b_rptr;
	int w,h;
	if (size==(MS_VIDEO_SIZE_QCIF_W*MS_VIDEO_SIZE_QCIF_H*3)/2){
		w=MS_VIDEO_SIZE_QCIF_W;
		h=MS_VIDEO_SIZE_QCIF_H;
	}else if (size==(MS_VIDEO_SIZE_CIF_W*MS_VIDEO_SIZE_CIF_H*3)/2){
		w=MS_VIDEO_SIZE_CIF_W;
		h=MS_VIDEO_SIZE_CIF_H;
	}else if (size==(MS_VIDEO_SIZE_HQVGA_W*MS_VIDEO_SIZE_HQVGA_H*3)/2){
		w=MS_VIDEO_SIZE_HQVGA_W;
		h=MS_VIDEO_SIZE_HQVGA_H;
	}else if (size==(MS_VIDEO_SIZE_QVGA_W*MS_VIDEO_SIZE_QVGA_H*3)/2){
		w=MS_VIDEO_SIZE_QVGA_W;
		h=MS_VIDEO_SIZE_QVGA_H;
	}else if (size==(MS_VIDEO_SIZE_HVGA_W*MS_VIDEO_SIZE_HVGA_H*3)/2){
		w=MS_VIDEO_SIZE_HVGA_W;
		h=MS_VIDEO_SIZE_HVGA_H;
	}else if (size==(MS_VIDEO_SIZE_VGA_W*MS_VIDEO_SIZE_VGA_H*3)/2){
		w=MS_VIDEO_SIZE_VGA_W;
		h=MS_VIDEO_SIZE_VGA_H;
	}else if (size==(MS_VIDEO_SIZE_4CIF_W*MS_VIDEO_SIZE_4CIF_H*3)/2){
		w=MS_VIDEO_SIZE_4CIF_W;
		h=MS_VIDEO_SIZE_4CIF_H;
	}else if (size==(MS_VIDEO_SIZE_W4CIF_W*MS_VIDEO_SIZE_W4CIF_H*3)/2){
		w=MS_VIDEO_SIZE_W4CIF_W;
		h=MS_VIDEO_SIZE_W4CIF_H;
	}else if (size==(MS_VIDEO_SIZE_SVGA_W*MS_VIDEO_SIZE_SVGA_H*3)/2){
		w=MS_VIDEO_SIZE_SVGA_W;
		h=MS_VIDEO_SIZE_SVGA_H;
	}else if (size==(MS_VIDEO_SIZE_SQCIF_W*MS_VIDEO_SIZE_SQCIF_H*3)/2){
		w=MS_VIDEO_SIZE_SQCIF_W;
		h=MS_VIDEO_SIZE_SQCIF_H;
	}else if (size==(MS_VIDEO_SIZE_QQVGA_W*MS_VIDEO_SIZE_QQVGA_H*3)/2){
		w=MS_VIDEO_SIZE_QQVGA_W;
		h=MS_VIDEO_SIZE_QQVGA_H;
	}else if (size==(MS_VIDEO_SIZE_NS1_W*MS_VIDEO_SIZE_NS1_H*3)/2){
		w=MS_VIDEO_SIZE_NS1_W;
		h=MS_VIDEO_SIZE_NS1_H;
	}else if (size==(MS_VIDEO_SIZE_QSIF_W*MS_VIDEO_SIZE_QSIF_H*3)/2){
		w=MS_VIDEO_SIZE_QSIF_W;
		h=MS_VIDEO_SIZE_QSIF_H;
	}else if (size==(MS_VIDEO_SIZE_SIF_W*MS_VIDEO_SIZE_SIF_H*3)/2){
		w=MS_VIDEO_SIZE_SIF_W;
		h=MS_VIDEO_SIZE_SIF_H;
	}else if (size==(MS_VIDEO_SIZE_4SIF_W*MS_VIDEO_SIZE_4SIF_H*3)/2){
		w=MS_VIDEO_SIZE_4SIF_W;
		h=MS_VIDEO_SIZE_4SIF_H;
	}else if (size==(MS_VIDEO_SIZE_288P_W*MS_VIDEO_SIZE_288P_H*3)/2){
		w=MS_VIDEO_SIZE_288P_W;
		h=MS_VIDEO_SIZE_288P_H;
	}else if (size==(MS_VIDEO_SIZE_432P_W*MS_VIDEO_SIZE_432P_H*3)/2){
		w=MS_VIDEO_SIZE_432P_W;
		h=MS_VIDEO_SIZE_432P_H;
	}else if (size==(MS_VIDEO_SIZE_448P_W*MS_VIDEO_SIZE_448P_H*3)/2){
		w=MS_VIDEO_SIZE_448P_W;
		h=MS_VIDEO_SIZE_448P_H;
	}else if (size==(MS_VIDEO_SIZE_480P_W*MS_VIDEO_SIZE_480P_H*3)/2){
		w=MS_VIDEO_SIZE_480P_W;
		h=MS_VIDEO_SIZE_480P_H;
	}else if (size==(MS_VIDEO_SIZE_576P_W*MS_VIDEO_SIZE_576P_H*3)/2){
		w=MS_VIDEO_SIZE_576P_W;
		h=MS_VIDEO_SIZE_576P_H;
	}else if (size==(MS_VIDEO_SIZE_720P_W*MS_VIDEO_SIZE_720P_H*3)/2){
		w=MS_VIDEO_SIZE_720P_W;
		h=MS_VIDEO_SIZE_720P_H;
	}else if (size==(MS_VIDEO_SIZE_1080P_W*MS_VIDEO_SIZE_1080P_H*3)/2){
		w=MS_VIDEO_SIZE_1080P_W;
		h=MS_VIDEO_SIZE_1080P_H;
	}else if (size==(MS_VIDEO_SIZE_SDTV_W*MS_VIDEO_SIZE_SDTV_H*3)/2){
		w=MS_VIDEO_SIZE_SDTV_W;
		h=MS_VIDEO_SIZE_SDTV_H;
	}else if (size==(MS_VIDEO_SIZE_HDTVP_W*MS_VIDEO_SIZE_HDTVP_H*3)/2){
		w=MS_VIDEO_SIZE_HDTVP_W;
		h=MS_VIDEO_SIZE_HDTVP_H;
	}else if (size==(MS_VIDEO_SIZE_XGA_W*MS_VIDEO_SIZE_XGA_H*3)/2){
		w=MS_VIDEO_SIZE_XGA_W;
		h=MS_VIDEO_SIZE_XGA_H;
	}else if (size==(MS_VIDEO_SIZE_WXGA_W*MS_VIDEO_SIZE_WXGA_H*3)/2){
		w=MS_VIDEO_SIZE_WXGA_W;
		h=MS_VIDEO_SIZE_WXGA_H;
	}else if (size==(MS_VIDEO_SIZE_WQCIF_W*MS_VIDEO_SIZE_WQCIF_H*3)/2){
		w=MS_VIDEO_SIZE_WQCIF_W;
		h=MS_VIDEO_SIZE_WQCIF_H;
	}else if (size==(MS_VIDEO_SIZE_CVD_W*MS_VIDEO_SIZE_CVD_H*3)/2){
		w=MS_VIDEO_SIZE_CVD_W;
		h=MS_VIDEO_SIZE_CVD_H;
	}else if (size==(160*112*3)/2){/*format used by econf*/
		w=160;
		h=112;
	}else if (size==(320*200*3)/2){/*format used by gTalk */
		w=320;
		h=200;
	}else {
		ms_error("Unsupported image size: size=%i (bug somewhere !)",size);
		return -1;
	}
	if (mblk_get_video_orientation(m)==MS_VIDEO_PORTRAIT){
		int tmp=h;
		h=w;
		w=tmp;
	}
	yuv_buf_init(buf,w,h,m->b_rptr);
	return 0;
}

int ms_yuv_buf_init_from_mblk_with_size(YuvBuf *buf, mblk_t *m, int w, int h){
  yuv_buf_init(buf,w,h,m->b_rptr);
  return 0;

}

int ms_picture_init_from_mblk_with_size(MSPicture *buf, mblk_t *m, MSPixFmt fmt, int w, int h){
	switch(fmt){
		case MS_YUV420P:
			return ms_yuv_buf_init_from_mblk_with_size(buf,m,w,h);
		break;
		case MS_YUY2:
		case MS_YUYV:
			memset(buf,0,sizeof(*buf));
			buf->w=w;
			buf->h=h;
			buf->planes[0]=m->b_rptr;
			buf->strides[0]=w*2;
		break;
		case MS_RGB24:
		case MS_RGB24_REV:
			memset(buf,0,sizeof(*buf));
			buf->w=w;
			buf->h=h;
			buf->planes[0]=m->b_rptr;
			buf->strides[0]=w*3;
		break;
		default:
			ms_fatal("FIXME: unsupported format %i",fmt);
			return -1;
	}
	return 0;
}

mblk_t * ms_yuv_buf_alloc(YuvBuf *buf, int w, int h){
	int size=(w*h*3)/2;
	const int padding=16;
	mblk_t *msg=allocb(size+padding,0);
	yuv_buf_init(buf,w,h,msg->b_wptr);
	if (h>w)
		mblk_set_video_orientation(msg,MS_VIDEO_PORTRAIT);
	else
		mblk_set_video_orientation(msg,MS_VIDEO_LANDSCAPE);
	msg->b_wptr+=size;
	return msg;
}

static void plane_copy(const uint8_t *src_plane, int src_stride,
	uint8_t *dst_plane, int dst_stride, MSVideoSize roi){
	int i;
	for(i=0;i<roi.height;++i){
		memcpy(dst_plane,src_plane,roi.width);
		src_plane+=src_stride;
		dst_plane+=dst_stride;
	}
}

void ms_yuv_buf_copy(uint8_t *src_planes[], const int src_strides[],
		uint8_t *dst_planes[], const int dst_strides[3], MSVideoSize roi){
	plane_copy(src_planes[0],src_strides[0],dst_planes[0],dst_strides[0],roi);
	roi.width=roi.width/2;
	roi.height=roi.height/2;
	plane_copy(src_planes[1],src_strides[1],dst_planes[1],dst_strides[1],roi);
	plane_copy(src_planes[2],src_strides[2],dst_planes[2],dst_strides[2],roi);
}

static void plane_horizontal_mirror(uint8_t *p, int linesize, int w, int h){
	int i,j;
	uint8_t tmp;
	for(j=0;j<h;++j){
		for(i=0;i<w/2;++i){
			const int idx_target_pixel = w-1-i;
			tmp=p[i];
			p[i]=p[idx_target_pixel];
			p[idx_target_pixel]=tmp;
		}
		p+=linesize;
	}
}
static void plane_central_mirror(uint8_t *p, int linesize, int w, int h){
	int i,j;
	uint8_t tmp;
	uint8_t *end_of_image = p + (h-1)*linesize+w-1;
	uint8_t *image_center=p+(h/2)*linesize + w/2;
	for(j=0;j<h/2;++j){
		for(i=0;i<w && p<image_center;++i){
			tmp=*p;
			*p=*end_of_image;
			*end_of_image=tmp;
			++p;
			--end_of_image;
		}
		p+=linesize-w;
		end_of_image-=linesize-w;
	}
}
static void plane_vertical_mirror(uint8_t *p, int linesize, int w, int h){
	int j;
	uint8_t *tmp=alloca(w*sizeof(int));
	uint8_t *bottom_line = p + (h-1)*linesize;
	for(j=0;j<h/2;++j){
		memcpy(tmp, p, w);
		memcpy(p, bottom_line, w);
		memcpy(bottom_line, tmp, w);
		p+=linesize;
		bottom_line-=linesize;
	}
}

static void plane_mirror(MSMirrorType type, uint8_t *p, int linesize, int w, int h){
	switch (type){
		case MS_HORIZONTAL_MIRROR:
			 plane_horizontal_mirror(p,linesize,w,h);
			 break;
		case MS_VERTICAL_MIRROR:
			plane_vertical_mirror(p,linesize,w,h);
			break;
		case MS_CENTRAL_MIRROR:
			plane_central_mirror(p,linesize,w,h);
			break;
		case MS_NO_MIRROR:
			break;
	}
}

/*in place horizontal mirroring*/
void ms_yuv_buf_mirror(YuvBuf *buf){
	ms_yuv_buf_mirrors(buf, MS_HORIZONTAL_MIRROR);
}

/*in place mirroring*/
void ms_yuv_buf_mirrors(YuvBuf *buf, MSMirrorType type){
	plane_mirror(type, buf->planes[0],buf->strides[0],buf->w,buf->h);
	plane_mirror(type, buf->planes[1],buf->strides[1],buf->w/2,buf->h/2);
	plane_mirror(type, buf->planes[2],buf->strides[2],buf->w/2,buf->h/2);
}

#ifndef MAKEFOURCC
#define MAKEFOURCC(a,b,c,d) ((d)<<24 | (c)<<16 | (b)<<8 | (a))
#endif

MSPixFmt ms_fourcc_to_pix_fmt(uint32_t fourcc){
	MSPixFmt ret;
	switch (fourcc){
		case MAKEFOURCC('I','4','2','0'):
			ret=MS_YUV420P;
		break;
		case MAKEFOURCC('Y','U','Y','2'):
			ret=MS_YUY2;
		break;
		case MAKEFOURCC('Y','U','Y','V'):
			ret=MS_YUYV;
		break;
		case MAKEFOURCC('U','Y','V','Y'):
			ret=MS_UYVY;
		break;
		case 0: /*BI_RGB on windows*/
			ret=MS_RGB24;
		break;
		default:
			ret=MS_PIX_FMT_UNKNOWN;
	}
	return ret;
}

void rgb24_mirror(uint8_t *buf, int w, int h, int linesize){
	int i,j;
	int r,g,b;
	int end=w*3;
	for(i=0;i<h;++i){
		for(j=0;j<end/2;j+=3){
			r=buf[j];
			g=buf[j+1];
			b=buf[j+2];
			buf[j]=buf[end-j-3];
			buf[j+1]=buf[end-j-2];
			buf[j+2]=buf[end-j-1];
			buf[end-j-3]=r;
			buf[end-j-2]=g;
			buf[end-j-1]=b;
		}
		buf+=linesize;
	}
}

void rgb24_revert(uint8_t *buf, int w, int h, int linesize){
	uint8_t *p,*pe;
	int i,j;
	uint8_t *end=buf+((h-1)*linesize);
	uint8_t exch;
	p=buf;
	pe=end-1;
	for(i=0;i<h/2;++i){
		for(j=0;j<w*3;++j){
			exch=p[i];
			p[i]=pe[-i];
			pe[-i]=exch;
		}
		p+=linesize;
		pe-=linesize;
	}
}

void rgb24_copy_revert(uint8_t *dstbuf, int dstlsz,
				const uint8_t *srcbuf, int srclsz, MSVideoSize roi){
	int i,j;
	const uint8_t *psrc;
	uint8_t *pdst;
	psrc=srcbuf;
	pdst=dstbuf+(dstlsz*(roi.height-1));
	for(i=0;i<roi.height;++i){
		for(j=0;j<roi.width*3;++j){
			pdst[(roi.width*3)-1-j]=psrc[j];
		}
		pdst-=dstlsz;
		psrc+=srclsz;
	}
}

static MSVideoSize _ordered_vsizes[]={
	{MS_VIDEO_SIZE_QCIF_W,MS_VIDEO_SIZE_QCIF_H},
	{MS_VIDEO_SIZE_QVGA_W,MS_VIDEO_SIZE_QVGA_H},
	{MS_VIDEO_SIZE_CIF_W,MS_VIDEO_SIZE_CIF_H},
	{MS_VIDEO_SIZE_VGA_W,MS_VIDEO_SIZE_VGA_H},
	{MS_VIDEO_SIZE_4CIF_W,MS_VIDEO_SIZE_4CIF_H},
	{MS_VIDEO_SIZE_720P_W,MS_VIDEO_SIZE_720P_H},
	{0,0}
};

MSVideoSize ms_video_size_get_just_lower_than(MSVideoSize vs){
	MSVideoSize *p;
	MSVideoSize ret;
	ret.width=0;
	ret.height=0;
	for(p=_ordered_vsizes;p->width!=0;++p){
		if (ms_video_size_greater_than(vs,*p) && !ms_video_size_equal(vs,*p)){
			ret=*p;
		}else return ret;
	}
	return ret;
}

void ms_rgb_to_yuv(const uint8_t rgb[3], uint8_t yuv[3]){
	yuv[0]=(uint8_t)(0.257*rgb[0] + 0.504*rgb[1] + 0.098*rgb[2] + 16);
	yuv[1]=(uint8_t)(-0.148*rgb[0] - 0.291*rgb[1] + 0.439*rgb[2] + 128);
	yuv[2]=(uint8_t)(0.439*rgb[0] - 0.368*rgb[1] - 0.071*rgb[2] + 128);
}

#if !defined(NO_FFMPEG)


int ms_pix_fmt_to_ffmpeg(MSPixFmt fmt){
	switch(fmt){
		case MS_RGBA32:
			return PIX_FMT_RGBA;
		case MS_RGB24:
			return PIX_FMT_RGB24;
		case MS_RGB24_REV:
			return PIX_FMT_BGR24;
		case MS_YUV420P:
			return PIX_FMT_YUV420P;
		case MS_YUYV:
			return PIX_FMT_YUYV422;
		case MS_UYVY:
			return PIX_FMT_UYVY422;
		case MS_YUY2:
			return PIX_FMT_YUYV422;   /* <- same as MS_YUYV */
		case MS_RGB565:
			return PIX_FMT_RGB565;
		default:
			ms_fatal("format not supported.");
			return -1;
	}
	return -1;
}

MSPixFmt ffmpeg_pix_fmt_to_ms(int fmt){
	switch(fmt){
		case PIX_FMT_RGB24:
			return MS_RGB24;
		case PIX_FMT_BGR24:
			return MS_RGB24_REV;
		case PIX_FMT_YUV420P:
			return MS_YUV420P;
		case PIX_FMT_YUYV422:
			return MS_YUYV;     /* same as MS_YUY2 */
		case PIX_FMT_UYVY422:
			return MS_UYVY;
		case PIX_FMT_RGBA:
			return MS_RGBA32;
		case PIX_FMT_RGB565:
			return MS_RGB565;
		default:
			ms_fatal("format not supported.");
			return MS_YUV420P; /* default */
	}
	return MS_YUV420P; /* default */
}

struct _MSFFScalerContext{
	struct SwsContext *ctx;
	int src_h;
};

typedef struct _MSFFScalerContext MSFFScalerContext;

static MSScalerContext *ff_create_swscale_context(int src_w, int src_h, MSPixFmt src_fmt,
                                          int dst_w, int dst_h, MSPixFmt dst_fmt, int flags){
	int ff_flags=0;
	MSFFScalerContext *ctx=ms_new(MSFFScalerContext,1);
	ctx->src_h=src_h;
	if (flags & MS_SCALER_METHOD_BILINEAR)
		ff_flags|=SWS_BILINEAR;
	else if (flags & MS_SCALER_METHOD_NEIGHBOUR)
		ff_flags|=SWS_BILINEAR;
	ctx->ctx=sws_getContext (src_w,src_h,ms_pix_fmt_to_ffmpeg (src_fmt),
	                                       dst_w,dst_h,ms_pix_fmt_to_ffmpeg (dst_fmt),ff_flags,NULL,NULL,NULL);
	if (ctx->ctx==NULL){
		ms_free(ctx);
		ctx=NULL;
	}
	return (MSScalerContext*)ctx;
}

static int ff_sws_scale(MSScalerContext *ctx, uint8_t *src[], int src_strides[], uint8_t *dst[], int dst_strides[]){
	MSFFScalerContext *fctx=(MSFFScalerContext*)ctx;
	int err=sws_scale(fctx->ctx,(const uint8_t * const*)src,src_strides,0,fctx->src_h,dst,dst_strides);
	if (err<0) return -1;
	return 0;
}

static void ff_sws_free(MSScalerContext *ctx){
	MSFFScalerContext *fctx=(MSFFScalerContext*)ctx;
	if (fctx->ctx) sws_freeContext(fctx->ctx);
	ms_free(ctx);
}

static MSScalerDesc ffmpeg_scaler={
	ff_create_swscale_context,
	ff_sws_scale,
	ff_sws_free
};

#endif

#if 0

/*
We use openmax-dl (from ARM) to optimize some scaling routines.
*/

#include "omxIP.h"

typedef struct AndroidScalerCtx{
	MSFFScalerContext base;
	OMXIPColorSpace cs;
	OMXSize src_size;
	OMXSize dst_size;
	bool_t use_omx;
}AndroidScalerCtx;

/* for android we use ffmpeg's scaler except for YUV420P-->RGB565, for which we prefer
 another arm neon optimized routine */

static MSScalerContext *android_create_scaler_context(int src_w, int src_h, MSPixFmt src_fmt,
                                          int dst_w, int dst_h, MSPixFmt dst_fmt, int flags){
	AndroidScalerCtx *ctx=ms_new0(AndroidScalerCtx,1);
	if (src_fmt==MS_YUV420P && dst_fmt==MS_RGB565){
		ctx->use_omx=TRUE;
		ctx->cs=OMX_IP_BGR565;
		ctx->src_size.width=src_w;
		ctx->src_size.height=src_h;
		ctx->dst_size.width=dst_w;
		ctx->dst_size.height=dst_h;
	}else{
		unsigned int ff_flags=0;
		ctx->base.src_h=src_h;
		if (flags & MS_SCALER_METHOD_BILINEAR)
			ff_flags|=SWS_BILINEAR;
		else if (flags & MS_SCALER_METHOD_NEIGHBOUR)
			ff_flags|=SWS_BILINEAR;
		ctx->base.ctx=sws_getContext (src_w,src_h,ms_pix_fmt_to_ffmpeg (src_fmt),
	                                       dst_w,dst_h,ms_pix_fmt_to_ffmpeg (dst_fmt),ff_flags,NULL,NULL,NULL);
		if (ctx->base.ctx==NULL){
			ms_free(ctx);
			ctx=NULL;
		}
	}
	return (MSScalerContext *)ctx;
}

static int android_scaler_process(MSScalerContext *ctx, uint8_t *src[], int src_strides[], uint8_t *dst[], int dst_strides[]){
	AndroidScalerCtx *actx=(AndroidScalerCtx*)ctx;
	if (actx->use_omx){
		int ret;
		OMX_U8 *osrc[3];
		OMX_INT osrc_strides[3];
		OMX_INT xrr_max;
		OMX_INT yrr_max;

		osrc[0]=src[0];
		osrc[1]=src[1];
		osrc[2]=src[2];
		osrc_strides[0]=src_strides[0];
		osrc_strides[1]=src_strides[1];
		osrc_strides[2]=src_strides[2];

		xrr_max = (OMX_INT) ((( (OMX_F32) ((actx->src_size.width&~1)-1) / ((actx->dst_size.width&~1)-1))) * (1<<16) +0.5);
		yrr_max = (OMX_INT) ((( (OMX_F32) ((actx->src_size.height&~1)-1) / ((actx->dst_size.height&~1)-1))) * (1<< 16) +0.5);

		ret=omxIPCS_YCbCr420RszCscRotBGR_U8_P3C3R((const OMX_U8**)osrc,osrc_strides,actx->src_size,dst[0],dst_strides[0],actx->dst_size,actx->cs,
				OMX_IP_BILINEAR, OMX_IP_DISABLE, xrr_max,yrr_max);
		if (ret!=OMX_Sts_NoErr){
			ms_error("omxIPCS_YCbCr420RszCscRotBGR_U8_P3C3R() failed : %i",ret);
			return -1;
		}
		return 0;
	}
	return ff_sws_scale(ctx,src,src_strides,dst,dst_strides);
}

static void android_scaler_free(MSScalerContext *ctx){
	ff_sws_free(ctx);
}

static MSScalerDesc android_scaler={
	android_create_scaler_context,
	android_scaler_process,
	android_scaler_free
};

#endif


#ifdef ANDROID
#include <arm_neon.h>
extern MSScalerDesc ms_android_scaler;

static MSScalerDesc *scaler_impl=&ms_android_scaler;
#elif !defined(NO_FFMPEG)
static MSScalerDesc *scaler_impl=&ffmpeg_scaler;
#else
static MSScalerDesc *scaler_impl=NULL;
#endif

MSScalerContext *ms_scaler_create_context(int src_w, int src_h, MSPixFmt src_fmt,
                                          int dst_w, int dst_h, MSPixFmt dst_fmt, int flags){
	if (scaler_impl)
		return scaler_impl->create_context(src_w,src_h,src_fmt,dst_w,dst_h,dst_fmt, flags);
	ms_fatal("No scaler implementation built-in, please supply one with ms_video_set_scaler_impl ()");
	return NULL;
}

int ms_scaler_process(MSScalerContext *ctx, uint8_t *src[], int src_strides[], uint8_t *dst[], int dst_strides[]){
	return scaler_impl->context_process(ctx,src,src_strides,dst,dst_strides);
}

void ms_scaler_context_free(MSScalerContext *ctx){
	scaler_impl->context_free(ctx);
}

void ms_video_set_scaler_impl(MSScalerDesc *desc){
	scaler_impl=desc;
}
#ifdef __ARM_NEON__
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
#endif

/* Can rotate Y, U or V plane; use step=2 for interleaved UV planes otherwise step=1*/
static void rotate_plane(int wDest, int hDest, int full_width, uint8_t* src, uint8_t* dst, int step, bool_t clockWise) {
	int hSrc = wDest;
	int wSrc = hDest;
	int src_stride = full_width * step;

	int signed_dst_stride;
	int incr;



	if (clockWise) {
		/* ms_warning("start writing destination buffer from top right");*/
		dst += wDest - 1;
		incr = 1;
		signed_dst_stride = wDest;
	} else {
		/* ms_warning("start writing destination buffer from top right");*/
		dst += wDest * (hDest - 1);
		incr = -1;
		signed_dst_stride = -wDest;
	}
	int y,x;
	for (y=0; y<hSrc; y++) {
		uint8_t* dst2 = dst;
		for (x=0; x<step*wSrc; x+=step) {
			/*	Copy a line in source buffer (left to right)
				Clockwise: Store a column in destination buffer (top to bottom)
				Not clockwise: Store a column in destination buffer (bottom to top)
			 */
			*dst2 = src[x];
			dst2 += signed_dst_stride;
		}
		dst -= incr;
		src += src_stride;
	}
}

#ifdef __ARM_NEON__
/*static*/ void rotate_plane_neon_clockwise(int wDest, int hDest, int full_width, uint8_t* src, uint8_t* dst) {
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

/*static*/ void rotate_plane_neon_anticlockwise(int wDest, int hDest, int full_width, uint8_t* src, uint8_t* dst) {
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

/*static*/ void rotate_cbcr_to_cr_cb(int wDest, int hDest, int full_width, uint8_t* cbcr_src, uint8_t* cr_dst, uint8_t* cb_dst,bool_t clockWise) {
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
#endif

/* Destination and source images may have their dimensions inverted.*/
mblk_t *copy_ycbcrbiplanar_to_true_yuv_with_rotation(char* y, char* cbcr, int rotation, int w, int h, int y_byte_per_row,int cbcr_byte_per_row, bool_t uFirstvSecond) {
	MSPicture pict;

	/*if (rotation % 180 != 0) {
		int t = w;
		w = h;
		h = t;
	}*/

	mblk_t *yuv_block = ms_yuv_buf_alloc(&pict, w, h);

	if (!uFirstvSecond) {
		unsigned char* tmp = pict.planes[1];
		pict.planes[1] = pict.planes[2];
		pict.planes[2] = tmp;
	}

	int uv_w = w/2;
	int uv_h = h/2;

	if (rotation % 180 == 0) {
#if defined (__ARM_NEON__)
		int i,j;
		uint8_t* u_dest=pict.planes[1], *v_dest=pict.planes[2];

		if (rotation == 0) {
			// plain copy
			for(i=0; i<h; i++) {
				memcpy(&pict.planes[0][i*w], &y[i*y_byte_per_row], w);
			}
			// de-interlace u/v
			for (i=0; i<uv_h; i++) {
				for(j=0; j<uv_w; j++) {
					*u_dest++ = cbcr[cbcr_byte_per_row*i + 2*j];
					*v_dest++ = cbcr[cbcr_byte_per_row*i + 2*j + 1];
				}
			}
		} else {
			// 180° y rotation
			for(i=0; i<h; i++) {
				for(j=0; j<w/16; j++) {
					int src_index = (h - (i+1))*y_byte_per_row + w - (j + 1)*16;
					int dst_index = i*w + j*16;

					reverse_16bytes_neon((uint8_t*)&y[src_index], &pict.planes[0][dst_index]);
				}
			}
			// 180° rotation + de-interlace u/v
			for (i=0; i<uv_h; i++) {
				for(j=0; j<w/16; j++) {
					int src_index = (uv_h - (i+1))*cbcr_byte_per_row + w - (j + 1)*16;
					int dst_index = i*uv_w + j*16/2;

					deinterlace_and_reverse_2x8bytes_neon((uint8_t*)&cbcr[src_index], &u_dest[dst_index], &v_dest[dst_index]);
				}
			}
		}
#else
	ms_warning("%s : rotation=%d not implemented\n", __FUNCTION__, rotation);
#endif
	} else {
		bool_t clockwise = rotation == 90 ? TRUE : FALSE;

		// Rotate Y
#if defined (__ARM_NEON__)
		if (clockwise) {
			rotate_plane_neon_clockwise(w,h,y_byte_per_row,(uint8_t*)y,pict.planes[0]);
		} else {
			rotate_plane_neon_anticlockwise(w,h,y_byte_per_row,(uint8_t*)y,pict.planes[0]);
		}
#else
		uint8_t* dsty = pict.planes[0];
		uint8_t* srcy = (uint8_t*) y;
		rotate_plane(w,h,y_byte_per_row,srcy,dsty,1, clockwise);
#endif

	#if defined (__ARM_NEON__)
		rotate_cbcr_to_cr_cb(uv_w,uv_h, cbcr_byte_per_row/2, (uint8_t*)cbcr, pict.planes[2], pict.planes[1],clockwise);
	#else
		// Copying U
		uint8_t* srcu = (uint8_t*) cbcr;
		uint8_t* dstu = pict.planes[1];
		rotate_plane(uv_w,uv_h,cbcr_byte_per_row/2,srcu,dstu, 2, clockwise);
		//	memset(dstu, 128, uorvsize);

		// Copying V
		uint8_t* srcv = srcu + 1;
		uint8_t* dstv = pict.planes[2];
		rotate_plane(uv_w,uv_h,cbcr_byte_per_row/2,srcv,dstv, 2, clockwise);
		//	memset(dstv, 128, uorvsize);
	#endif
	}

	return yuv_block;
}

void ms_video_init_framerate_controller(MSFrameRateController* ctrl, float fps) {
	ctrl->start_time = 0;
	ctrl->th_frame_count = -1;
	ctrl->fps = fps;
}

bool_t ms_video_capture_new_frame(MSFrameRateController* ctrl, uint32_t current_time) {
	int cur_frame;
	float elapsed;

	/* init controller */
	if (ctrl->th_frame_count==-1){
		ctrl->start_time = current_time;
		ctrl->th_frame_count = 0;
	}

	elapsed = ((float)(current_time - ctrl->start_time))/1000.0;
	cur_frame = elapsed * ctrl->fps;

	if (cur_frame>=ctrl->th_frame_count){
		ctrl->th_frame_count++;
		return TRUE;
	} else {
		return FALSE;
	}
}

void ms_video_init_average_fps(MSAverageFPS* afps, float expectedFps) {
	afps->last_frame_time = -1;
	afps->last_print_time = -1;
	afps->mean_inter_frame = 0;
	afps->expected_fps = expectedFps;
}

void ms_video_update_average_fps(MSAverageFPS* afps, uint32_t current_time) {
	if (afps->last_frame_time!=-1){
		float frame_interval=(float)(current_time - afps->last_frame_time)/1000.0;
		if (afps->mean_inter_frame==0){
			afps->mean_inter_frame=frame_interval;
		}else{
			afps->mean_inter_frame=(0.8*afps->mean_inter_frame)+(0.2*frame_interval);
		}
	} else {
		afps->last_print_time = current_time;
	}
	afps->last_frame_time=current_time;

	if ((current_time - afps->last_print_time > 10000) && afps->mean_inter_frame!=0){
		ms_message("Captured mean fps=%f, expected=%f",1/afps->mean_inter_frame, afps->expected_fps);
		afps->last_print_time = current_time;
	}
}

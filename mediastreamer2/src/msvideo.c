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
	
	for (int y=0; y<hSrc; y++) {
		uint8_t* dst2 = dst;
		for (int x=0; x<step*wSrc; x+=step) {
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
static void rotate_plane_neon(int wDest, int hDest, int full_width, uint8_t* src, uint8_t* dst, bool_t clockWise) {
	int hSrc = wDest;
	int wSrc = hDest;
	int src_stride = full_width;
	
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
	
	for (int y=0; y<hSrc; y++) {
		uint8_t* dst2 = dst;
		for (int x=0; x<wSrc; x+=8) {
				uint8x8_t tmp = vld1_u8 (src+x);
 				
				vst1_lane_u8 (dst2, tmp, 0);
				dst2+=signed_dst_stride;
				vst1_lane_u8 (dst2, tmp, 1);
				dst2+=signed_dst_stride;
				vst1_lane_u8 (dst2, tmp, 2);
				dst2+=signed_dst_stride;
				vst1_lane_u8 (dst2, tmp, 3);
				dst2+=signed_dst_stride;
				vst1_lane_u8 (dst2, tmp, 4);
				dst2+=signed_dst_stride;
				vst1_lane_u8 (dst2, tmp, 5);
				dst2+=signed_dst_stride;
				vst1_lane_u8 (dst2, tmp, 6);
				dst2+=signed_dst_stride;
				vst1_lane_u8 (dst2, tmp, 7);
				dst2+=signed_dst_stride;
		}
		dst -= incr;
		src += src_stride;
	}
}

static void rotate_cbcr_to_cr_cb(int wDest, int hDest, int full_width, uint8_t* cbcr_src, uint8_t* cr_dst, uint8_t* cb_dst,bool_t clockWise) {
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
	
	for (int y=0; y<hSrc; y++) {
		uint8_t* cb_dst2 = cb_dst;
		uint8_t* cr_dst2 = cr_dst;
		for (int x=0; x<2*wSrc; x+=16) {
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
#endif
/* Destination and source images have their dimensions inverted.*/
mblk_t *copy_ycbcrbiplanar_to_true_yuv_portrait(char* y, char* cbcr, int rotation, int w, int h, int y_byte_per_row,int cbcr_byte_per_row) {
	
	/*	ms_message("copy_frame_to_true_yuv_inverted : Orientation %i; width %i; height %i", orientation, w, h);*/
	MSPicture pict;
	mblk_t *yuv_block = ms_yuv_buf_alloc(&pict, w, h);
	
	bool_t clockwise = rotation == 90 ? TRUE : FALSE;
	
	// Copying Y
#if defined (__ARM_NEON__)
	rotate_plane_neon(w,h,y_byte_per_row,(uint8_t*)y,pict.planes[0], clockwise);
#else
	uint8_t* dsty = pict.planes[0];
	uint8_t* srcy = (uint8_t*) y;
	rotate_plane(w,h,y_byte_per_row,srcy,dsty,1, clockwise);
#endif	
	
	
	int uv_w = w/2;
	int uv_h = h/2;
	//	int uorvsize = uv_w * uv_h;
#if defined (__ARM_NEON__)
	rotate_cbcr_to_cr_cb(uv_w,uv_h, cbcr_byte_per_row/2, (uint8_t*)cbcr, pict.planes[1], pict.planes[2],clockwise); 	
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
	return yuv_block;
}



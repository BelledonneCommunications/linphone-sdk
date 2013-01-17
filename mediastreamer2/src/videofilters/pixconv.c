/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006  Simon MORLAT (simon.morlat@linphone.org)

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

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"



typedef struct PixConvState{
	YuvBuf outbuf;
	mblk_t *yuv_msg;
	MSScalerContext *scaler;
	MSVideoSize size;
	MSPixFmt  in_fmt;
	MSPixFmt out_fmt;
}PixConvState;

static void pixconv_init(MSFilter *f){
	PixConvState *s=(PixConvState *)ms_new(PixConvState,1);
	s->yuv_msg=NULL;
	s->size.width = MS_VIDEO_SIZE_CIF_W;
	s->size.height = MS_VIDEO_SIZE_CIF_H;
	s->in_fmt=MS_YUV420P;
	s->out_fmt=MS_YUV420P;
	s->scaler=NULL;
	f->data=s;
}

static void pixconv_uninit(MSFilter *f){
	PixConvState *s=(PixConvState*)f->data;
	if (s->scaler!=NULL){
		ms_scaler_context_free(s->scaler);
		s->scaler=NULL;
	}
	if (s->yuv_msg!=NULL) freemsg(s->yuv_msg);
	ms_free(s);
}

static mblk_t * pixconv_alloc_mblk(PixConvState *s){
	if (s->yuv_msg!=NULL){
		int ref=s->yuv_msg->b_datap->db_ref;
		if (ref==1){
			return dupmsg(s->yuv_msg);
		}else{
			/*the last msg is still referenced by somebody else*/
			ms_message("Somebody still retaining yuv buffer (ref=%i)",ref);
			freemsg(s->yuv_msg);
			s->yuv_msg=NULL;
		}
	}
	s->yuv_msg=ms_yuv_buf_alloc(&s->outbuf,s->size.width,s->size.height);
	return dupmsg(s->yuv_msg);
}

static void pixconv_process(MSFilter *f){
	mblk_t *im,*om=NULL;
	PixConvState *s=(PixConvState*)f->data;

	while((im=ms_queue_get(f->inputs[0]))!=NULL){
		if (s->in_fmt==s->out_fmt){
			om=im;
		}else{
			MSPicture inbuf;
			if (ms_picture_init_from_mblk_with_size(&inbuf,im,s->in_fmt,s->size.width,s->size.height)==0){
				om=pixconv_alloc_mblk(s);
				if (s->scaler==NULL){
					s->scaler=ms_scaler_create_context(inbuf.w, inbuf.h,
						s->in_fmt,inbuf.w,inbuf.h,
						s->out_fmt,MS_SCALER_METHOD_BILINEAR);
				}
				if (s->in_fmt==MS_RGB24_REV){
					inbuf.planes[0]+=inbuf.strides[0]*(inbuf.h-1);
					inbuf.strides[0]=-inbuf.strides[0];
				}
				if (ms_scaler_process (s->scaler,inbuf.planes,inbuf.strides, s->outbuf.planes, s->outbuf.strides)<0){
					ms_error("MSPixConv: Error in ms_sws_scale().");
				}
			}
			freemsg(im);
		}
		if (om!=NULL) ms_queue_put(f->outputs[0],om);
	}
}

static int pixconv_set_vsize(MSFilter *f, void*arg){
	PixConvState *s=(PixConvState*)f->data;
	s->size=*(MSVideoSize*)arg;
	return 0;
}

static int pixconv_set_pixfmt(MSFilter *f, void *arg){
	MSPixFmt fmt=*(MSPixFmt*)arg;
	PixConvState *s=(PixConvState*)f->data;
	s->in_fmt=fmt;
	return 0;
}

static MSFilterMethod methods[]={
	{	MS_FILTER_SET_VIDEO_SIZE, pixconv_set_vsize	},
	{	MS_FILTER_SET_PIX_FMT,	pixconv_set_pixfmt	},
	{	0	,	NULL }
};

#ifdef _MSC_VER

MSFilterDesc ms_pix_conv_desc={
	MS_PIX_CONV_ID,
	"MSPixConv",
	N_("A pixel format converter"),
	MS_FILTER_OTHER,
	NULL,
	1,
	1,
	pixconv_init,
	NULL,
	pixconv_process,
	NULL,
	pixconv_uninit,
	methods
};

#else

MSFilterDesc ms_pix_conv_desc={
	.id=MS_PIX_CONV_ID,
	.name="MSPixConv",
	.text=N_("A pixel format converter"),
	.category=MS_FILTER_OTHER,
	.ninputs=1,
	.noutputs=1,
	.init=pixconv_init,
	.process=pixconv_process,
	.uninit=pixconv_uninit,
	.methods=methods
};

#endif

MS_FILTER_DESC_EXPORT(ms_pix_conv_desc)


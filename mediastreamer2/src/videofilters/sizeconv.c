/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2 
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msvideo.h"


typedef struct SizeConvState{
	MSVideoSize target_vsize;
	MSVideoSize in_vsize;
	YuvBuf outbuf;
	MSYuvBufAllocator *allocator;
	MSScalerContext *sws_ctx;
	float fps;
	float start_time;
	int frame_count;
	queue_t rq;
	bool_t needRefresh;
} SizeConvState;


/*this MSFilter will do on the fly picture size conversion. It attempts to guess the picture size from the yuv buffer size. YUV420P is assumed on input.
For now it only supports QCIF->CIF, QVGA->CIF and CIF->CIF (does nothing in this case)*/

static void size_conv_init(MSFilter *f){
	SizeConvState *s=ms_new0(SizeConvState,1);
	s->target_vsize.width = MS_VIDEO_SIZE_CIF_W;
	s->target_vsize.height = MS_VIDEO_SIZE_CIF_H;
	s->in_vsize.width=0;
	s->in_vsize.height=0;
	s->sws_ctx=NULL;
	s->allocator = ms_yuv_buf_allocator_new();
	s->start_time=0;
	s->frame_count=-1;
	s->needRefresh=FALSE;
	s->fps=-1; /* default to process ALL frames */
	qinit(&s->rq);
	f->data=s;
}

static void size_conv_uninit(MSFilter *f){
	SizeConvState *s=(SizeConvState*)f->data;
	ms_yuv_buf_allocator_free(s->allocator);
	ms_free(s);
}

static void size_conv_postprocess(MSFilter *f){
	SizeConvState *s=(SizeConvState*)f->data;
	if (s->sws_ctx!=NULL) {
		ms_scaler_context_free(s->sws_ctx);
		s->sws_ctx=NULL;
	}
	flushq(&s->rq,0);
	s->frame_count=-1;
}

static mblk_t * sizeconv_alloc_mblk(SizeConvState *s){
	return ms_yuv_buf_allocator_get(s->allocator, &s->outbuf, s->target_vsize.width,s->target_vsize.height);
}

static MSScalerContext * get_resampler(SizeConvState *s, int w, int h){
	if (s->in_vsize.width!=w ||
		s->in_vsize.height!=h || s->sws_ctx==NULL){
		if (s->sws_ctx!=NULL){
			ms_scaler_context_free(s->sws_ctx);
			s->sws_ctx=NULL;
		}
		s->sws_ctx=ms_scaler_create_context(w,h,MS_YUV420P,
			s->target_vsize.width,s->target_vsize.height,MS_YUV420P,
			MS_SCALER_METHOD_BILINEAR);
		s->in_vsize.width=w;
		s->in_vsize.height=h;
		ms_message("MSSizeConv: create new scaler context with w %d, h %d", w,h);
	}
	return s->sws_ctx;
}

static void size_conv_process(MSFilter *f){
	SizeConvState *s=(SizeConvState*)f->data;
	YuvBuf inbuf;
	mblk_t *im;
	int cur_frame;

	ms_filter_lock(f);

	if (s->frame_count==-1){
		s->start_time=(float)f->ticker->time;
		s->frame_count=0;
	}
	while((im=ms_queue_get(f->inputs[0]))!=NULL ){
		putq(&s->rq, im);
	}

	cur_frame=(int)((f->ticker->time-s->start_time)*s->fps/1000.0);
	if (cur_frame<=s->frame_count && s->fps>=0) {
		/* too much frame */
		while(s->rq.q_mcount>1){
			ms_message("MSSizeConv: extra frame removed.");
			im=getq(&s->rq);
			freemsg(im);
		}
		ms_filter_unlock(f);
		return;
	}

	if (cur_frame>s->frame_count && s->fps>=0) {
		/*keep the most recent frame if several frames have been captured */
		while(s->rq.q_mcount>1){
			ms_message("MSSizeConv: extra frame removed.");
			im=getq(&s->rq);
			freemsg(im);
		}
	}
	while((im=getq(&s->rq))!=NULL ){
		if (ms_yuv_buf_init_from_mblk(&inbuf,im)==0){
			if (inbuf.w==s->target_vsize.width &&
				inbuf.h==s->target_vsize.height){
				ms_queue_put(f->outputs[0],im);
			}else{
				int w = s->target_vsize.width;
				int h = s->target_vsize.height;
				// keep the same orientation
				if(ms_video_size_get_orientation((MSVideoSize){inbuf.w, inbuf.h}) != ms_video_size_get_orientation(s->target_vsize)) {
					s->target_vsize.width = h;
					s->target_vsize.height = w;
				}
				// If input and output do not have the same ratio, this scaler will stretch the image, keep the same aspect ratio to avoid that
				if (inbuf.w * s->target_vsize.height/s->target_vsize.width != inbuf.h) {
					if (inbuf.w > inbuf.h) {
						s->target_vsize.height = inbuf.h * s->target_vsize.width / inbuf.w;
					} else {
						s->target_vsize.width = inbuf.w * s->target_vsize.height / inbuf.h;
					}
				}
				if (s->target_vsize.width != w || s->target_vsize.height !=h) {
					s->needRefresh = TRUE;
					ms_filter_notify_no_arg(f, MS_FILTER_OUTPUT_FMT_CHANGED);
				} else if (!s->needRefresh) {
					MSScalerContext *sws_ctx=get_resampler(s,inbuf.w,inbuf.h);
					mblk_t *om= sizeconv_alloc_mblk(s);
					if (om != NULL) {
						if (ms_scaler_process(sws_ctx,inbuf.planes,inbuf.strides,s->outbuf.planes, s->outbuf.strides)<0){
							ms_error("MSSizeConv: error in ms_scaler_process().");
							freemsg(om);
						}else {
							mblk_set_timestamp_info(om, mblk_get_timestamp_info(im));
							ms_queue_put(f->outputs[0],om);
						}
					}
				} else {
					ms_warning("MSSizeConv: output fmt changed, waiting.");
				}
				freemsg(im);
			}
			s->frame_count++;
		}else{
			ms_warning("size_conv_process(): bad buffer.");
			freemsg(im);
		}
	}

	ms_filter_unlock(f);
}


static int sizeconv_set_vsize(MSFilter *f, void*arg){
	SizeConvState *s=(SizeConvState*)f->data;
	ms_filter_lock(f);
	s->target_vsize=*(MSVideoSize*)arg;
	ms_message("sizeconv_set_vsize(): set target size w %d, h %d", s->target_vsize.width, s->target_vsize.height);
	if (s->sws_ctx!=NULL) {
		ms_scaler_context_free(s->sws_ctx);
		s->sws_ctx=NULL;
	}
	ms_filter_unlock(f);
	return 0;
}

static int sizeconv_set_fps(MSFilter *f, void *arg){
	SizeConvState *s=(SizeConvState*)f->data;
	s->fps=*((float*)arg);
	s->frame_count=-1; /* reset counter used for fps */
	return 0;
}

static int sizeconv_get_vsize(MSFilter *f, void *data){
	SizeConvState *s=(SizeConvState*)f->data;
	MSVideoSize *vsize = (MSVideoSize *)data;
	vsize->width =  s->target_vsize.width;
	vsize->height = s->target_vsize.height;
	return 0;
}

static MSFilterMethod methods[]={
	{	MS_FILTER_SET_FPS	,	sizeconv_set_fps	},
	{	MS_FILTER_SET_VIDEO_SIZE, sizeconv_set_vsize	},
	{   MS_FILTER_GET_VIDEO_SIZE, sizeconv_get_vsize	},
	{	0	,	NULL }
};

#ifdef _MSC_VER

MSFilterDesc ms_size_conv_desc={
	MS_SIZE_CONV_ID,
	"MSSizeConv",
	N_("A video size converter"),
	MS_FILTER_OTHER,
	NULL,
	1,
	1,
	size_conv_init,
	NULL,
	size_conv_process,
	size_conv_postprocess,
	size_conv_uninit,
	methods
};

#else

MSFilterDesc ms_size_conv_desc={
	.id=MS_SIZE_CONV_ID,
	.name="MSSizeConv",
	.text=N_("a small video size converter"),
	.ninputs=1,
	.noutputs=1,
	.init=size_conv_init,
	.process=size_conv_process,
	.postprocess=size_conv_postprocess,
	.uninit=size_conv_uninit,
	.methods=methods
};

#endif

MS_FILTER_DESC_EXPORT(ms_size_conv_desc)


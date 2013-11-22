/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2010  Belledonne Communications SARL <simon.morlat@linphone.org>

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

#include "mediastreamer2/msjpegwriter.h"
#include "mediastreamer2/msvideo.h"
#include "ffmpeg-priv.h"

#ifdef WIN32
#include <malloc.h>
#endif

typedef struct {
	FILE *file;
	AVCodec *codec;
}JpegWriter;

static void jpg_init(MSFilter *f){
	JpegWriter *s=ms_new0(JpegWriter,1);
	s->codec=avcodec_find_encoder(CODEC_ID_MJPEG);
	if (s->codec==NULL){
		ms_error("Could not find CODEC_ID_MJPEG !");
	}
	f->data=s;
}

static void jpg_uninit(MSFilter *f){
	JpegWriter *s=(JpegWriter*)f->data;
	if (s->file!=NULL){
		fclose(s->file);
	}
	ms_free(s);
}

static int take_snapshot(MSFilter *f, void *arg){
	JpegWriter *s=(JpegWriter*)f->data;
	const char *filename=(const char*)arg;
	if (s->file!=NULL){
		fclose(s->file);
		s->file=NULL;
	}
	s->file=fopen(filename,"w");
	if (s->file==NULL){
		ms_error("Could not open %s",filename);
		return -1;
	}
	return 0;
}

static void cleanup(JpegWriter *s, AVCodecContext *avctx){
	if (s->file){
		fclose(s->file);
		s->file=NULL;
	}
	if (avctx){
		avcodec_close(avctx);
		av_free(avctx);
	}
}

static void jpg_process(MSFilter *f){
	JpegWriter *s=(JpegWriter*)f->data;
	if (s->file!=NULL && s->codec!=NULL){
		MSPicture yuvbuf, yuvjpeg;
		mblk_t *m=ms_queue_peek_last(f->inputs[0]);
		if (ms_yuv_buf_init_from_mblk(&yuvbuf,m)==0){
			int error;
			int comp_buf_sz=msgdsize(m);
			uint8_t *comp_buf=(uint8_t*)alloca(comp_buf_sz);
			AVFrame pict;
			mblk_t *jpegm;
			struct SwsContext *sws_ctx;
			
			AVCodecContext *avctx=avcodec_alloc_context();
			
			avctx->width=yuvbuf.w;
			avctx->height=yuvbuf.h;
			avctx->time_base.num = 1;
			avctx->time_base.den =1;
			avctx->pix_fmt=PIX_FMT_YUVJ420P;

			error=avcodec_open(avctx,s->codec);
			if (error!=0) {
				ms_error("avcodec_open() failed: %i",error);
				cleanup(s,NULL);
				av_free(avctx);
				return;
			}
			sws_ctx=sws_getContext(avctx->width,avctx->height,PIX_FMT_YUV420P,
				avctx->width,avctx->height,avctx->pix_fmt,SWS_FAST_BILINEAR,NULL, NULL, NULL);
			if (sws_ctx==NULL) {
				ms_error(" sws_getContext() failed.");
				cleanup(s,avctx);
				goto end;
			}
			jpegm=ms_yuv_buf_alloc (&yuvjpeg,avctx->width, avctx->height);
#if LIBSWSCALE_VERSION_INT >= AV_VERSION_INT(0,9,0)	
			if (sws_scale(sws_ctx,(const uint8_t *const*)yuvbuf.planes,yuvbuf.strides,0,avctx->height,yuvjpeg.planes,yuvjpeg.strides)<0){
#else
			if (sws_scale(sws_ctx,(uint8_t **)yuvbuf.planes,yuvbuf.strides,0,avctx->height,yuvjpeg.planes,yuvjpeg.strides)<0){
#endif
				ms_error("sws_scale() failed.");
				sws_freeContext(sws_ctx);
				cleanup(s,avctx);
				freemsg(jpegm);
				goto end;
			}
			sws_freeContext(sws_ctx);
			
			avcodec_get_frame_defaults(&pict);
			avpicture_fill((AVPicture*)&pict,(uint8_t*)jpegm->b_rptr,avctx->pix_fmt,avctx->width,avctx->height);
			error=avcodec_encode_video(avctx, (uint8_t*)comp_buf,comp_buf_sz, &pict);
			if (error<0){
				ms_error("Could not encode jpeg picture.");
			}else{
				if (fwrite(comp_buf,error,1,s->file)>0){
					ms_message("Snapshot done");
				}else{
					ms_error("Error writing snapshot.");
				}
			}
			cleanup(s,avctx);
			freemsg(jpegm);
		}
		goto end;
	}
	end:
	ms_queue_flush(f->inputs[0]);
}

static MSFilterMethod jpg_methods[]={
	{	MS_JPEG_WRITER_TAKE_SNAPSHOT, take_snapshot },
	{	0,NULL}
};

#ifndef _MSC_VER

MSFilterDesc ms_jpeg_writer_desc={
	.id=MS_JPEG_WRITER_ID,
	.name="MSJpegWriter",
	.text="Take a video snapshot as jpg file",
	.category=MS_FILTER_OTHER,
	.ninputs=1,
	.noutputs=0,
	.init=jpg_init,
	.process=jpg_process,
	.uninit=jpg_uninit,
	.methods=jpg_methods
};

#else

MSFilterDesc ms_jpeg_writer_desc={
	MS_JPEG_WRITER_ID,
	"MSJpegWriter",
	"Take a video snapshot as jpg file",
	MS_FILTER_OTHER,
	NULL,
	1,
	0,
	jpg_init,
	NULL,
	jpg_process,
	NULL,
	jpg_uninit,
	jpg_methods
};


#endif

MS_FILTER_DESC_EXPORT(ms_jpeg_writer_desc)

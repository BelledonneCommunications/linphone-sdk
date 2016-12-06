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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/msjpegwriter.h"
#include "mediastreamer2/msvideo.h"
#include "ffmpeg-priv.h"


#if LIBAVCODEC_VERSION_MAJOR >= 57

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#else
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#endif

typedef struct {
	FILE *file;
	char *filename;
	char *tmpFilename;
	AVCodec *codec;
	AVFrame* pict;
	MSFilter *f;
}JpegWriter;

static void close_file(JpegWriter *obj, bool_t doRenaming);

static bool_t open_file(JpegWriter *obj, const char *filename) {
	obj->filename=ms_strdup(filename);
	obj->tmpFilename=ms_strdup_printf("%s.part",filename);
	obj->file = fopen(obj->tmpFilename, "wb");
	if(!obj->file) {
		ms_error("Could not open %s for write", obj->tmpFilename);
		close_file(obj,FALSE);
	}
	return TRUE;
}

static void close_file(JpegWriter *obj, bool_t doRenaming) {
	if (obj->file) {
		fclose(obj->file);
		obj->file = NULL;
	}
	if(doRenaming) {
		if(rename(obj->tmpFilename, obj->filename) != 0) {
			ms_error("Could not rename %s into %s", obj->tmpFilename, obj->filename);
		} else {
			ms_filter_notify(obj->f, MS_JPEG_WRITER_SNAPSHOT_TAKEN, (void *)obj->filename);
		}
	}
	ms_free(obj->filename);
	obj->filename = NULL;

	ms_free(obj->tmpFilename);
	obj->tmpFilename = NULL;
}

static void jpg_init(MSFilter *f){
	JpegWriter *s=ms_new0(JpegWriter,1);
	s->f = f;
	s->codec=avcodec_find_encoder(CODEC_ID_MJPEG);
	if (s->codec==NULL){
		ms_error("Could not find CODEC_ID_MJPEG !");
	}
	s->pict = av_frame_alloc();
	f->data=s;
}

static void jpg_uninit(MSFilter *f){
	JpegWriter *s=(JpegWriter*)f->data;
	s->f = NULL;
	if (s->file!=NULL){
		close_file(s, FALSE);
	}
	if (s->pict) av_frame_free(&s->pict);
	ms_free(s);
}

static int take_snapshot(MSFilter *f, void *arg){
	JpegWriter *s=(JpegWriter*)f->data;
	const char *filename = (const char *)arg;
	int err = 0;
	ms_filter_lock(f);
	if (s->file!=NULL){
		close_file(s, FALSE);
	}
	if(!open_file(s, filename)) err=-1;
	ms_filter_unlock(f);
	return err;
}

static void cleanup(JpegWriter *s, AVCodecContext *avctx, bool_t success){
	if (s->file){
		close_file(s, success);
	}
	if (avctx){
		avcodec_close(avctx);
		av_free(avctx);
	}
}

static void jpg_process(MSFilter *f){
	JpegWriter *s=(JpegWriter*)f->data;
	ms_filter_lock(f);
	if (s->file!=NULL && s->codec!=NULL){
		MSPicture yuvbuf, yuvjpeg;
		mblk_t *m=ms_queue_peek_last(f->inputs[0]);
		if (ms_yuv_buf_init_from_mblk(&yuvbuf,m)==0){
			int error,got_pict;
			size_t comp_buf_sz=msgdsize(m);
			uint8_t *comp_buf=(uint8_t*)ms_malloc0(comp_buf_sz);
			mblk_t *jpegm;
			struct SwsContext *sws_ctx;
			struct AVPacket packet;
			AVCodecContext *avctx=avcodec_alloc_context3(s->codec);

			memset(&packet, 0, sizeof(packet));

			avctx->width=yuvbuf.w;
			avctx->height=yuvbuf.h;
			avctx->time_base.num = 1;
			avctx->time_base.den =1;
			avctx->pix_fmt=AV_PIX_FMT_YUVJ420P;

			error=avcodec_open2(avctx,s->codec,NULL);
			if (error!=0) {
				ms_error("avcodec_open() failed: %i",error);
				cleanup(s,NULL, FALSE);
				av_free(avctx);
				goto end;
			}
			sws_ctx=sws_getContext(avctx->width,avctx->height,AV_PIX_FMT_YUV420P,
				avctx->width,avctx->height,avctx->pix_fmt,SWS_FAST_BILINEAR,NULL, NULL, NULL);
			if (sws_ctx==NULL) {
				ms_error(" sws_getContext() failed.");
				cleanup(s,avctx, FALSE);
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
				cleanup(s,avctx, FALSE);
				freemsg(jpegm);
				goto end;
			}
			sws_freeContext(sws_ctx);

			av_frame_unref(s->pict);
			avpicture_fill((AVPicture*)s->pict,(uint8_t*)jpegm->b_rptr,avctx->pix_fmt,avctx->width,avctx->height);
			packet.data=comp_buf;
			packet.size=(int)comp_buf_sz;
			error=avcodec_encode_video2(avctx, &packet, s->pict, &got_pict);
			if (error<0){
				ms_error("Could not encode jpeg picture.");
			}else{
				if (fwrite(comp_buf,packet.size,1,s->file)>0){
					ms_message("Snapshot done");
				}else{
					ms_error("Error writing snapshot.");
				}
			}
			ms_free(comp_buf);
			cleanup(s,avctx, TRUE);
			freemsg(jpegm);
		}
		goto end;
	}
	end:
	ms_filter_unlock(f);
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

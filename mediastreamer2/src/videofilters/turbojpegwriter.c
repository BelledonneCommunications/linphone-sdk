/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2016  Belledonne Communications

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
#include "turbojpeg.h"


typedef struct {
	FILE *file;
	char *filename;
	char *tmpFilename;
	tjhandle *turboJpeg;
	MSFilter *f;
}JpegWriter;

static void close_file(JpegWriter *obj, bool_t doRenaming);

static bool_t open_file(JpegWriter *obj, const char *filename) {
	obj->filename = ms_strdup(filename);
	obj->tmpFilename = ms_strdup_printf("%s.part",filename);
	obj->file = fopen(obj->tmpFilename, "wb");
	if (!obj->file) {
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
	if (doRenaming) {
		if (rename(obj->tmpFilename, obj->filename) != 0) {
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

static void jpg_init(MSFilter *f) {
	JpegWriter *s=ms_new0(JpegWriter,1);
	s->f = f;
	s->turboJpeg = tjInitCompress();
	if (s->turboJpeg == NULL) {
		ms_error("TurboJpeg init error:%s", tjGetErrorStr());
	}
	f->data=s;
}

static void jpg_uninit(MSFilter *f) {
	JpegWriter *s = (JpegWriter*)f->data;
	s->f = NULL;
	if (s->file != NULL) {
		close_file(s, FALSE);
	}
	if (s->turboJpeg != NULL) {
		if (tjDestroy(s->turboJpeg) != 0)
			ms_error("TurboJpeg destroy error:%s", tjGetErrorStr());
	}
	ms_free(s);
}

static int take_snapshot(MSFilter *f, void *arg) {
	JpegWriter *s=(JpegWriter*)f->data;
	const char *filename = (const char *)arg;
	int err = 0;
	ms_filter_lock(f);
	if (s->file != NULL) {
		close_file(s, FALSE);
	}
	if (!open_file(s, filename)) err=-1;
	ms_filter_unlock(f);
	return err;
}

static void cleanup(JpegWriter *s, bool_t success) {
	if (s->file){
		close_file(s, success);
	}
}

static void jpg_process(MSFilter *f) {
	bool_t success = FALSE;
	JpegWriter *s=(JpegWriter*)f->data;
	ms_filter_lock(f);
	if (s->file != NULL && s->turboJpeg != NULL) {
		int error;
		MSPicture yuvbuf;
		unsigned char *jpegBuffer = NULL;
		unsigned long jpegSize = 0;

		mblk_t *m=ms_queue_peek_last(f->inputs[0]);

		if (ms_yuv_buf_init_from_mblk(&yuvbuf,m) != 0)
			goto end;

		error = tjCompressFromYUVPlanes(
			s->turboJpeg,
			(const unsigned char **)yuvbuf.planes,
			yuvbuf.w,
			yuvbuf.strides,
			yuvbuf.h,
			TJSAMP_420,
			&jpegBuffer,
			&jpegSize,
			100,
			TJFLAG_ACCURATEDCT
		);

		if (error != 0) {
			ms_error("tjCompressFromYUVPlanes() failed: %s", tjGetErrorStr());
			if (jpegBuffer != NULL) tjFree(jpegBuffer);
			goto end;
		}

		if (fwrite(jpegBuffer, jpegSize, 1, s->file)>0) {
			ms_message("Snapshot done with turbojpeg");
			success = TRUE;
		} else {
			ms_error("Error writing snapshot.");
		}

		tjFree(jpegBuffer);
	}
	end:
	cleanup(s, success);
	ms_filter_unlock(f);
	ms_queue_flush(f->inputs[0]);
}

static MSFilterMethod jpg_methods[] = {
	{	MS_JPEG_WRITER_TAKE_SNAPSHOT, take_snapshot },
	{	0,NULL}
};

#ifndef _MSC_VER

MSFilterDesc ms_jpeg_writer_desc = {
	.id = MS_JPEG_WRITER_ID,
	.name = "MSJpegWriter",
	.text = "Take a video snapshot as jpg file",
	.category = MS_FILTER_OTHER,
	.ninputs = 1,
	.noutputs = 0,
	.init = jpg_init,
	.process = jpg_process,
	.uninit = jpg_uninit,
	.methods = jpg_methods
};

#else

MSFilterDesc ms_jpeg_writer_desc = {
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

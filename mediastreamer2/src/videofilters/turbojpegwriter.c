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

#include "bctoolbox/utils.hh"
#include "bctoolbox/vfs.h"
#include "mediastreamer2/msjpegwriter.h"
#include "mediastreamer2/msvideo.h"
#include "turbojpeg.h"

#include "fd_portab.h" // keep this include at the last of the inclusion sequence!

#ifdef __cplusplus
extern "C"{
#endif

typedef struct {
	bctbx_vfs_file_t *file;
	char *filename;
	char *tmpFilename;
	tjhandle turboJpeg;
	MSFilter *f;
}JpegWriter;

static void close_file(JpegWriter *obj, bool_t doRenaming) {
	MSJpegWriteEventData eventData = {{0}};
	if (obj->file) {
		bctbx_file_close(obj->file);
		obj->file = NULL;
	}
	if (doRenaming) {
		if (rename(obj->tmpFilename, obj->filename) != 0) {
			ms_error("Could not rename %s into %s", obj->tmpFilename, obj->filename);
		} else {
			strncpy(eventData.filePath, obj->filename, sizeof(eventData)-1);
			ms_filter_notify(obj->f, MS_JPEG_WRITER_SNAPSHOT_TAKEN, &eventData);
		}
	}
	ms_free(obj->filename);
	obj->filename = NULL;

	ms_free(obj->tmpFilename);
	obj->tmpFilename = NULL;
}

static bool_t open_file(JpegWriter *obj, const char *filename) {
	obj->filename = ms_strdup(filename);
	obj->tmpFilename = ms_strdup_printf("%s.part",filename);
	obj->file = bctbx_file_open2(bctbx_vfs_get_default(), obj->tmpFilename, O_WRONLY|O_CREAT|O_BINARY);
	if (!obj->file) {
		ms_error("Could not open %s for write", obj->tmpFilename);
		close_file(obj,FALSE);
	}
	return TRUE;
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
//This auto cast has the purpose to support multiple versions of turboJPEG where parameter can be const.
			bctoolbox::Utils::auto_cast<unsigned char **>(yuvbuf.planes),
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
		if(bctbx_file_write2(s->file, jpegBuffer, jpegSize) != BCTBX_VFS_ERROR){
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

#if !defined(_MSC_VER) && !defined(__cplusplus)

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

#ifdef __cplusplus
}
#endif

MS_FILTER_DESC_EXPORT(ms_jpeg_writer_desc)

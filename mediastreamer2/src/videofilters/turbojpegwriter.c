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
#include "mediastreamer2/msasync.h"
#include "mediastreamer2/msjpegwriter.h"
#include "mediastreamer2/msvideo.h"
#include "turbojpeg.h"

#include "fd_portab.h" // keep this include at the last of the inclusion sequence!

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	bctbx_vfs_file_t *file;
	char *filename;
	char *tmpFilename;
	tjhandle turboJpeg;
	MSFilter *f;
	MSWorkerThread *process_thread;
	queue_t entry_q;
	bool_t snapshot_requested;
} JpegWriter;

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
			strncpy(eventData.filePath, obj->filename, sizeof(eventData.filePath) - 1);
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
	obj->tmpFilename = ms_strdup_printf("%s.part", filename);
	obj->file = bctbx_file_open2(bctbx_vfs_get_default(), obj->tmpFilename, O_WRONLY | O_CREAT | O_BINARY);
	if (!obj->file) {
		ms_error("Could not open %s for write", obj->tmpFilename);
		return FALSE;
	}
	return TRUE;
}

static void jpg_init(MSFilter *f) {
	JpegWriter *s = ms_new0(JpegWriter, 1);
	s->f = f;
	s->turboJpeg = tjInitCompress();
	if (s->turboJpeg == NULL) {
		ms_error("TurboJpeg init error:%s", tjGetErrorStr());
	}
	s->process_thread = ms_worker_thread_new("MSJpegWriter");
	qinit(&s->entry_q);
	f->data = s;
}

static void jpg_uninit(MSFilter *f) {
	JpegWriter *s = (JpegWriter *)f->data;
	ms_worker_thread_destroy(s->process_thread, TRUE);
	s->process_thread = NULL;
	flushq(&s->entry_q, 0);
	s->f = NULL;
	if (s->file != NULL) {
		close_file(s, FALSE);
	}
	if (s->turboJpeg != NULL) {
		if (tjDestroy(s->turboJpeg) != 0) ms_error("TurboJpeg destroy error:%s", tjGetErrorStr());
	}
	ms_free(s);
}

static int take_snapshot(MSFilter *f, void *arg) {
	JpegWriter *s = (JpegWriter *)f->data;
	const char *filename = (const char *)arg;
	int err = 0;

	ms_filter_lock(f);
	if (s->file != NULL) {
		close_file(s, FALSE);
		s->snapshot_requested = FALSE;
	}
	if (open_file(s, filename)) {
		s->snapshot_requested = TRUE;
	} else err = -1;
	ms_filter_unlock(f);
	return err;
}

static void cleanup(JpegWriter *s, bool_t success) {
	ms_filter_lock(s->f);
	if (s->file) {
		close_file(s, success);
	}
	ms_filter_unlock(s->f);
}

static bool_t jpg_process_frame_task(void *obj) {
	MSFilter *f = (MSFilter *)obj;
	JpegWriter *s = (JpegWriter *)f->data;
	int error;
	MSPicture yuvbuf;
	unsigned char *jpegBuffer = NULL;
	unsigned long jpegSize = 0;
	bool_t success = FALSE;
	mblk_t *m = NULL;

	ms_filter_lock(f);
	m = getq(&s->entry_q);
	ms_filter_unlock(f);

	ms_message("%s: got image to encode to jpeg.", f->desc->name);
	if (ms_yuv_buf_init_from_mblk(&yuvbuf, m) != 0) goto end;

	if (s->turboJpeg == NULL) goto end;

	error = tjCompressFromYUVPlanes(
	    s->turboJpeg,
	    // This auto cast has the purpose to support multiple versions of turboJPEG where parameter can be const.
	    bctoolbox::Utils::auto_cast<unsigned char **>(yuvbuf.planes), yuvbuf.w, yuvbuf.strides, yuvbuf.h, TJSAMP_420,
	    &jpegBuffer, &jpegSize, 100, TJFLAG_ACCURATEDCT);

	if (error != 0) {
		ms_error("tjCompressFromYUVPlanes() failed: %s", tjGetErrorStr());
		goto end;
	}
	ms_filter_lock(f);
	if (s->file != NULL && bctbx_file_write2(s->file, jpegBuffer, jpegSize) != BCTBX_VFS_ERROR) {
		success = TRUE;
	}
	ms_filter_unlock(f);

	if (success) {
		ms_message("Snapshot done with turbojpeg");
	} else {
		ms_error("Error writing snapshot.");
	}
end:
	if (jpegBuffer) tjFree(jpegBuffer);
	freemsg(m);
	cleanup(s, success);
	return success;
}

static void jpg_process(MSFilter *f) {
	JpegWriter *s = (JpegWriter *)f->data;
	bool_t img_queued = FALSE;

	ms_filter_lock(f);

	if (s->snapshot_requested) {
		mblk_t *img = ms_queue_peek_last(f->inputs[0]);

		if (img != NULL) {
			ms_queue_remove(f->inputs[0], img);
			putq(&s->entry_q, img);
			s->snapshot_requested = FALSE;
			img_queued = TRUE;
		}
	}
	ms_filter_unlock(f);
	/* avoid taking the filter mutex to post the task, since the start of the task
	 * on the worker thread will require to take the mutex. */
	if (img_queued) {
		ms_worker_thread_add_task(s->process_thread, jpg_process_frame_task, (void *)f);
		ms_message("%s: Snapshot request submitted to worker thread.", f->desc->name);
	}

	ms_queue_flush(f->inputs[0]);
}

static MSFilterMethod jpg_methods[] = {{MS_JPEG_WRITER_TAKE_SNAPSHOT, take_snapshot}, {0, NULL}};

#if !defined(_MSC_VER) && !defined(__cplusplus)

MSFilterDesc ms_jpeg_writer_desc = {.id = MS_JPEG_WRITER_ID,
                                    .name = "MSJpegWriter",
                                    .text = "Take a video snapshot as jpg file",
                                    .category = MS_FILTER_OTHER,
                                    .ninputs = 1,
                                    .noutputs = 0,
                                    .init = jpg_init,
                                    .process = jpg_process,
                                    .uninit = jpg_uninit,
                                    .methods = jpg_methods};

#else

MSFilterDesc ms_jpeg_writer_desc = {MS_JPEG_WRITER_ID,
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
                                    jpg_methods};

#endif

#ifdef __cplusplus
}
#endif

MS_FILTER_DESC_EXPORT(ms_jpeg_writer_desc)

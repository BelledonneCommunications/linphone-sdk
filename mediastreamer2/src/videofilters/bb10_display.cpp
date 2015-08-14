/*
 * bb10_display.cpp - Video display filter for Blackberry 10.
 *
 * Copyright (C) 2015  Belledonne Communications, Grenoble, France
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"
#include "layouts.h"

#include <screen/screen.h>

typedef struct BB10Display {
	MSVideoSize vsize;
	MSVideoSize wsize;
	screen_window_t window;
	screen_pixmap_t pixmap;
	screen_buffer_t pixmap_buffer;
	int stride;
	screen_context_t context;
	bool_t window_created;
	const char *window_id;
	const char *window_group;
} BB10Display;

static void bb10display_set_window_id_and_group(BB10Display *d) {
	if (!d->window_created) {
		ms_warning("[bb10_display] window wasn't created yet, skipping...");
		return;
	}
	
	screen_set_window_property_cv(d->window, SCREEN_PROPERTY_ID_STRING, strlen(d->window_id), d->window_id);
	screen_join_window_group(d->window, d->window_group);
}

static void bb10display_createWindow(BB10Display *d) {
	screen_context_t context;
	screen_window_t window;
	screen_pixmap_t pixmap;
	screen_buffer_t buffer;
	
	if (d->window_created) {
		ms_warning("[bb10_display] window is already created, skipping...");
		return;
	}
	
	screen_create_context(&context, SCREEN_APPLICATION_CONTEXT);
	screen_create_window_type(&window, context, SCREEN_CHILD_WINDOW);
	
	int usage = SCREEN_USAGE_NATIVE;
	screen_set_window_property_iv(window, SCREEN_PROPERTY_USAGE, &usage);
	
	int wdims[2] = { d->wsize.width, d->wsize.height };
	screen_set_window_property_iv(window, SCREEN_PROPERTY_BUFFER_SIZE, wdims);
	screen_set_window_property_iv(window, SCREEN_PROPERTY_SOURCE_SIZE, wdims);
	
	int zorder = -5;
	screen_set_window_property_iv(window, SCREEN_PROPERTY_ZORDER, &zorder);
	
	screen_create_window_buffers(window, 1);
	
	screen_create_pixmap(&pixmap, context);
	
	usage = SCREEN_USAGE_WRITE | SCREEN_USAGE_NATIVE;
	screen_set_pixmap_property_iv(pixmap, SCREEN_PROPERTY_USAGE, &usage);
	
	int format = SCREEN_FORMAT_YUV420;
	screen_set_pixmap_property_iv(pixmap, SCREEN_PROPERTY_FORMAT, &format);
	
	int dims[2] = { d->vsize.width, d->vsize.height };
	screen_set_pixmap_property_iv(pixmap, SCREEN_PROPERTY_BUFFER_SIZE, dims);
	
	screen_create_pixmap_buffer(pixmap);
	
	screen_get_pixmap_property_pv(pixmap, SCREEN_PROPERTY_RENDER_BUFFERS, (void**) &buffer);
	int stride;
	screen_get_buffer_property_iv(buffer, SCREEN_PROPERTY_STRIDE, &stride);
	
	d->window = window;
	d->context = context;
	d->pixmap = pixmap;
	d->pixmap_buffer = buffer;
	d->stride = stride;
	d->window_created = TRUE;
	ms_warning("[bb10_display] bb10display_createWindow window created with buffer size %i,%i and stride %i", dims[0], dims[1], stride);
	
	if (d->window_id != NULL && d->window_group != NULL) {
		bb10display_set_window_id_and_group(d);
	}
}

static void bb10display_destroyWindow(BB10Display *d) {
	if (!d->window_created) {
		ms_warning("[bb10_display] screen wasn't created yet, skipping...");
		return;
	}
	
	screen_destroy_pixmap_buffer(d->pixmap);
	d->pixmap_buffer = NULL;
	screen_destroy_pixmap(d->pixmap);
	d->pixmap = NULL;
	
	screen_destroy_window_buffers(d->window);
	screen_destroy_window(d->window);
	d->window = NULL;
	
	screen_destroy_context(d->context);
	d->context = NULL;
	
	d->window_created = FALSE;
	ms_warning("[bb10_display] bb10display_destroyWindow window destroyed");
}

static void bb10display_fillWindowBuffer(BB10Display *d, MSPicture *yuvbuf) {
	uint8_t *ptr = NULL;
	screen_get_buffer_property_pv(d->pixmap_buffer, SCREEN_PROPERTY_POINTER, (void **)&ptr);
	
	if (ptr) {
		uint8_t *dest_planes[3];
		int dest_strides[3];
		MSVideoSize roi = {0};
		
		uint8_t *y = ptr;
		uint8_t *u = y + (d->vsize.height * d->stride);
		uint8_t *v = u + (d->vsize.height * d->stride) / 4;
		
		dest_planes[0] = y;
		dest_planes[1] = u;
		dest_planes[2] = v;
		dest_strides[0] = d->stride;
		dest_strides[1] = d->stride / 2;
		dest_strides[2] = d->stride / 2;
		
		roi.width = yuvbuf->w;
		roi.height = yuvbuf->h;
		
		ms_yuv_buf_copy(yuvbuf->planes, yuvbuf->strides, dest_planes, dest_strides, roi);
		
		screen_buffer_t buffer;
		screen_get_window_property_pv(d->window, SCREEN_PROPERTY_RENDER_BUFFERS, (void**) &buffer);
		
		MSRect rect;
		ms_layout_center_rectangle(d->wsize, d->vsize, &rect);
		int attributes[] = { 
			SCREEN_BLIT_SOURCE_WIDTH, d->vsize.width, SCREEN_BLIT_SOURCE_HEIGHT, d->vsize.height, 
			SCREEN_BLIT_SOURCE_X, rect.x, SCREEN_BLIT_SOURCE_Y, rect.y, 
			SCREEN_BLIT_SOURCE_WIDTH, rect.w, SCREEN_BLIT_SOURCE_HEIGHT, rect.h,
			SCREEN_BLIT_END 
		};
		screen_blit(d->context, buffer, d->pixmap_buffer, attributes);
	
		int rect[4] = { 0, 0, d->wsize.width, d->wsize.height };
		screen_post_window(d->window, buffer, 1, rect, 0);
	}
}

static void bb10display_init(MSFilter *f) {
	BB10Display *d = (BB10Display*) ms_new0(BB10Display, 1);
	MSVideoSize def_size, window_size;
	
	d->window = NULL;
	d->context = NULL;
	d->pixmap = NULL;
	d->pixmap_buffer = NULL;
	d->window_created = FALSE;
	def_size.width = MS_VIDEO_SIZE_CIF_W;
	def_size.height = MS_VIDEO_SIZE_CIF_H;
	d->vsize = def_size;
	window_size.width = MS_VIDEO_SIZE_CIF_W;
	window_size.height = MS_VIDEO_SIZE_CIF_H;
	d->wsize = window_size;
	d->window_id = NULL;
	d->window_group = NULL;
	d->stride = 0;
	
	f->data = d;
	ms_warning("[bb10_display] init done");
}

static void bb10display_uninit(MSFilter *f) {
	BB10Display *d = (BB10Display*) f->data;
	
	bb10display_destroyWindow(d);
	ms_free(d);
	ms_warning("[bb10_display] uninit done");
}

static void bb10display_preprocess(MSFilter *f) {
	BB10Display *d = (BB10Display*) f->data;
}

static void bb10display_process(MSFilter *f) {
	BB10Display *d = (BB10Display*) f->data;
	mblk_t *inm = NULL;
	MSPicture src = {0};

	ms_filter_lock(f);
	if (f->inputs[0] != NULL && (inm = ms_queue_peek_last(f->inputs[0])) != 0) {
		if (ms_yuv_buf_init_from_mblk(&src, inm) == 0) {
			bool_t destroy_and_recreate_window = FALSE;
			
			MSVideoSize newsize;
			newsize.width = src.w;
			newsize.height = src.h;
			if (!ms_video_size_equal(newsize, d->vsize)) {
				ms_warning("[bb10_display] video size changed from %i,%i to %i,%i",  newsize.width, newsize.height, d->vsize.width, d->vsize.height);
				d->vsize = newsize;
				destroy_and_recreate_window = TRUE;
			}
			
			if (d->window_created) {
				int wdims[2] = { 0, 0 };
				screen_get_window_property_iv(d->window, SCREEN_PROPERTY_SIZE, &wdims);
				if (d->wsize.width != wdims[0] || d->wsize.height != wdims[1]) {
					ms_warning("[bb10_display] screen size changed from %i,%i to %i,%i",  d->wsize.width, d->wsize.height, wdims[0], wdims[1]);
					d->wsize.width = wdims[0];
					d->wsize.height = wdims[1];
					destroy_and_recreate_window = TRUE;
				}
			}
			
			if (destroy_and_recreate_window) {
				if (d->window_created) {
					ms_warning("[bb10_display] window created, flush and destroy");
					screen_flush_context(d->context, 0);
					bb10display_destroyWindow(d);
				}
				bb10display_createWindow(d);
			}
			
			if (d->window_created) {
				bb10display_fillWindowBuffer(d, &src);
			}
		}
	}
	ms_filter_unlock(f);
	
	if (f->inputs[0] != NULL)
		ms_queue_flush(f->inputs[0]);
	if (f->inputs[1] != NULL)
		ms_queue_flush(f->inputs[1]);
}

static void bb10display_postprocess(MSFilter *f) {
	BB10Display *d = (BB10Display*) f->data;
}

static int bb10_display_set_window_ids(MSFilter *f, void *arg) {
	BB10Display *d = (BB10Display*) f->data;
	
	ms_filter_lock(f);
	const char *group = *(const char **)arg;
	d->window_id = "LinphoneVideoWindowId";
	d->window_group = group;
	ms_warning("[bb10_display] set window_id: %s and window_group: %s", d->window_id, d->window_group);
	
	if (!d->window_created) {
		bb10display_createWindow(d);
	} else {
		bb10display_set_window_id_and_group(d);
	}
	ms_filter_unlock(f);
	
	return 0;
}

static MSFilterMethod ms_bb10display_methods[] = {
	{ MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID, 	bb10_display_set_window_ids },
	{ 0,										NULL }
};

MSFilterDesc ms_bb10_display_desc = {
	MS_BB10_DISPLAY_ID,
	"MSBB10Display",
	"A display filter for blackberry 10",
	MS_FILTER_OTHER,
	NULL,
	2,
	0,
	bb10display_init,
	bb10display_preprocess,
	bb10display_process,
	bb10display_postprocess,
	bb10display_uninit,
	ms_bb10display_methods
};

MS_FILTER_DESC_EXPORT(ms_bb10_display_desc)
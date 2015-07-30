#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"

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
	ms_message("[DEBUG] bb10display_set_window_id_and_group");
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
	
	int dims[2] = { d->vsize.width, d->vsize.height };
	screen_set_window_property_iv(window, SCREEN_PROPERTY_BUFFER_SIZE, dims);
	screen_set_window_property_iv(window, SCREEN_PROPERTY_SOURCE_SIZE, dims);
	
	int zorder = -5;
	screen_set_window_property_iv(window, SCREEN_PROPERTY_ZORDER, &zorder);
	
	//int wdims[2] = { d->wsize.width, d->wsize.height };
	screen_set_window_property_iv(window, SCREEN_PROPERTY_SIZE, dims);
	
	screen_create_window_buffers(window, 1);
	
	screen_create_pixmap(&pixmap, context);
	
	usage = SCREEN_USAGE_WRITE | SCREEN_USAGE_NATIVE;
	screen_set_pixmap_property_iv(pixmap, SCREEN_PROPERTY_USAGE, &usage);
	
	int format = SCREEN_FORMAT_YUV420;
	screen_set_pixmap_property_iv(pixmap, SCREEN_PROPERTY_FORMAT, &format);
	
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
	ms_debug("[bb10_display] bb10display_createWindow window created");
	
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
	ms_debug("[bb10_display] bb10display_destroyWindow window destroyed");
}

static void bb10display_fillWindowBuffer(BB10Display *d, MSPicture yuvbuf) {
	unsigned char *ptr = NULL;
	screen_get_buffer_property_pv(d->pixmap_buffer, SCREEN_PROPERTY_POINTER, (void **)&ptr);
	
	if (ptr) {
		unsigned char *y = ptr;
		unsigned char *u = y + (d->vsize.height * d->stride);
		unsigned char *v = u + (d->vsize.height * d->stride) / 4;
		
		memcpy(y, yuvbuf.planes[0], yuvbuf.w * yuvbuf.h);
		memcpy(u, yuvbuf.planes[1], yuvbuf.w * yuvbuf.h / 4);
		memcpy(v, yuvbuf.planes[2], yuvbuf.w * yuvbuf.h / 4);
		
		screen_buffer_t buffer;
		screen_get_window_property_pv(d->window, SCREEN_PROPERTY_RENDER_BUFFERS, (void**) &buffer);
		int attributes[] = { SCREEN_BLIT_SOURCE_WIDTH, d->vsize.width, SCREEN_BLIT_SOURCE_HEIGHT, d->vsize.height, SCREEN_BLIT_END };
		screen_blit(d->context, buffer, d->pixmap_buffer, attributes);
	
		int rect[4] = { 0, 0, d->vsize.width, d->vsize.height };
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
	d->wsize = def_size;
	d->window_id = NULL;
	d->window_group = NULL;
	d->stride = 0;
	
	bb10display_createWindow(d);
	
	f->data = d;
}

static void bb10display_uninit(MSFilter *f) {
	BB10Display *d = (BB10Display*) f->data;
	
	bb10display_destroyWindow(d);
	ms_free(d);
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
			MSVideoSize newsize;
			newsize.width = src.w;
			newsize.height = src.h;
			ms_message("[bb10_display] received size is %ix%i, current size is %ix%i", newsize.width, newsize.height, d->vsize.width, d->vsize.height);
			if (!ms_video_size_equal(newsize, d->vsize)) {
				d->vsize = newsize;
				ms_message("[bb10_display] size changed to %i,%i", d->vsize.width, d->vsize.height);
				
				if (d->window_created) {
					screen_flush_context(d->context, 0);
					bb10display_destroyWindow(d);
				}
				bb10display_createWindow(d);
			}
			
			if (d->window_created) {
				bb10display_fillWindowBuffer(d, src);
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

static int bb10display_set_wsize(MSFilter *f, void *arg) {
	BB10Display *d = (BB10Display*) f->data;
	
	ms_filter_lock(f);
	d->wsize = *(MSVideoSize*)arg;
	ms_message("[bb10_display] set wsize: %ix%i", d->wsize.width, d->wsize.height);
	
	if (d->window_created) {
		screen_flush_context(d->context, 0);
		bb10display_destroyWindow(d);
	}
	bb10display_createWindow(d);
	ms_filter_unlock(f);
	
	return 0;
}

static int bb10_display_set_window_ids(MSFilter *f, void *arg) {
	BB10Display *d = (BB10Display*) f->data;
	
	ms_filter_lock(f);
	const char *group = *(const char **)arg;
	d->window_id = "LinphoneVideoWindowId";
	d->window_group = group;
	ms_message("[bb10_display] set window_id: %s and window_group: %s", d->window_id, d->window_group);
	
	if (!d->window_created) {
		bb10display_createWindow(d);
	} else {
		bb10display_set_window_id_and_group(d);
	}
	ms_filter_unlock(f);
	
	return 0;
}

static MSFilterMethod ms_bb10display_methods[] = {
	//{ MS_FILTER_SET_VIDEO_SIZE, 				bb10display_set_wsize },
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
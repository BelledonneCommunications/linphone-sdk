/*
 * bb10_capture.cpp - Video capture filter for Blackberry 10.
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
#include "mediastreamer2/mswebcam.h"
#include "mediastreamer2/msticker.h"

#include <camera/camera_api.h>

static const char *error_to_string(camera_error_t error) {
	switch (error) {
		case CAMERA_EOK:
			return "CAMERA_EOK";
		case CAMERA_EINVAL:
			return "CAMERA_EINVAL";
		case CAMERA_ENODEV:
			return "CAMERA_ENODEV";
		case CAMERA_EMFILE:
			return "CAMERA_EMFILE";
		case CAMERA_EBADF:
			return "CAMERA_EBADF";
		case CAMERA_EACCESS:
			return "CAMERA_EACCESS";
		case CAMERA_EBADR:
			return "CAMERA_EBADR";
		case CAMERA_ENODATA:
			return "CAMERA_ENODATA";
		case CAMERA_ENOENT:
			return "CAMERA_ENOENT";
		case CAMERA_ENOMEM:
			return "CAMERA_ENOMEM";
		case CAMERA_EOPNOTSUPP:
			return "CAMERA_EOPNOTSUPP";
		case CAMERA_ETIMEDOUT:
			return "CAMERA_ETIMEDOUT";
		case CAMERA_EALREADY:
			return "CAMERA_EALREADY";
		case CAMERA_EBUSY:
			return "CAMERA_EBUSY";
		case CAMERA_ENOSPC:
			return "CAMERA_ENOSPC";
		case CAMERA_EUNINIT:
			return "CAMERA_EUNINIT";
		case CAMERA_EREGFAULT:
			return "CAMERA_EREGFAULT";
		case CAMERA_EMICINUSE:
			return "CAMERA_EMICINUSE";
		case CAMERA_EDESKTOPCAMERAINUSE:
			return "CAMERA_EDESKTOPCAMERAINUSE";
		case CAMERA_EPOWERDOWN:
			return "CAMERA_EPOWERDOWN";
		case CAMERA_3ALOCKED:
			return "CAMERA_3ALOCKED";
		case CAMERA_EVIEWFINDERFROZEN:
			return "CAMERA_EVIEWFINDERFROZEN";
		case CAMERA_EOVERFLOW:
			return "CAMERA_EOVERFLOW";
		case CAMERA_ETHERMALSHUTDOWN:
			return "CAMERA_ETHERMALSHUTDOWN";
		default:
			return "UNKNOWN";
	}
	return "UNKNOWN";
}

static const char *vfmode_to_string(camera_vfmode_t mode) {
	switch (mode) {
		case CAMERA_VFMODE_DEFAULT:
			return "CAMERA_VFMODE_DEFAULT";
		case CAMERA_VFMODE_PHOTO:
			return "CAMERA_VFMODE_PHOTO";
		case CAMERA_VFMODE_CONTINUOUS_BURST:
			return "CAMERA_VFMODE_CONTINUOUS_BURST";
		case CAMERA_VFMODE_FIXED_BURST:
			return "CAMERA_VFMODE_FIXED_BURST";
		case CAMERA_VFMODE_EV_BRACKETING:
			return "CAMERA_VFMODE_EV_BRACKETING";
		case CAMERA_VFMODE_VIDEO:
			return "CAMERA_VFMODE_VIDEO";
		case CAMERA_VFMODE_VIDEO_SNAPSHOT:
			return "CAMERA_VFMODE_VIDEO_SNAPSHOT";
		case CAMERA_VFMODE_HIGH_SPEED_VIDEO:
			return "CAMERA_VFMODE_HIGH_SPEED_VIDEO";
		case CAMERA_VFMODE_HDR_VIDEO:
			return "CAMERA_VFMODE_HDR_VIDEO";
		case CAMERA_VFMODE_NUM_MODES:
			return "CAMERA_VFMODE_NUM_MODES";
		default:
			return "UNKNOWN";
	}
	return "UNKNOWN";
}

typedef struct BB10Capture {
	MSVideoSize vsize;
	float framerate;
	MSQueue rq;
	ms_mutex_t mutex;
	
	camera_unit_t camera;
	camera_handle_t cam_handle;
	char *window_group;
	char *window_id;
	bool_t camera_openned;
	bool_t capture_started;
	MSAverageFPS avgfps;
} BB10Capture;

static void bb10capture_video_callback(camera_handle_t cam_handle, camera_buffer_t *buffer, void *arg) {
	BB10Capture *d = (BB10Capture *)arg;
	mblk_t *om = NULL;
	MSYuvBufAllocator *yba = ms_yuv_buf_allocator_new();
	
	if (buffer->frametype == CAMERA_FRAMETYPE_NV12) {
		om = copy_ycbcrbiplanar_to_true_yuv_with_rotation(yba, 
														buffer->framebuf,
														buffer->framebuf + buffer->framedesc.nv12.uv_offset, 
														0,
														buffer->framedesc.nv12.width,
														buffer->framedesc.nv12.height,
														buffer->framedesc.nv12.stride,
														buffer->framedesc.nv12.uv_stride,
														TRUE);
	}
	
	if (om != NULL) {
		ms_mutex_lock(&d->mutex);
		ms_queue_put(&d->rq, om);
		ms_mutex_unlock(&d->mutex);
	}
	ms_yuv_buf_allocator_free(yba);
}

static void bb10capture_open_camera(BB10Capture *d) {
	camera_error_t error;
	
	if (d->camera_openned) {
		ms_warning("[bb10_capture] camera already openned, skipping...");
		return;
	}
	
	ms_message("[bb10_capture] openning %s camera", d->camera == CAMERA_UNIT_FRONT ? "front" : (d->camera == CAMERA_UNIT_REAR ? "rear" : "unknown"));
	error = camera_open(d->camera, CAMERA_MODE_RW, &(d->cam_handle));
	if (error == CAMERA_EOK) {
		d->camera_openned = TRUE;
	} else {
		ms_error("[bb10_capture] openning %i camera failed: %s", d->camera, error_to_string(error));
	}
}

static void bb10capture_start_capture(BB10Capture *d) {
	if (!d->camera_openned) {
		ms_error("[bb10_capture] camera not openned, skipping...");
		return;
	}
	if (d->capture_started) {
		ms_warning("[bb10_capture] capture already started, skipping...");
		return;
	}
	
	camera_set_vf_mode(d->cam_handle, CAMERA_VFMODE_VIDEO);
	camera_set_vf_property(d->cam_handle, CAMERA_IMGPROP_WIDTH, d->vsize.width, CAMERA_IMGPROP_HEIGHT, d->vsize.height);
	camera_set_vf_property(d->cam_handle, CAMERA_IMGPROP_FORMAT, CAMERA_FRAMETYPE_NV12);
	
	if (d->framerate > 0) {
		camera_set_vf_property(d->cam_handle, CAMERA_IMGPROP_VARIABLEFRAMERATE, 1);
		camera_set_vf_property(d->cam_handle, CAMERA_IMGPROP_MINFRAMERATE, (double)d->framerate, CAMERA_IMGPROP_FRAMERATE, (double)d->framerate);
	}
	
	camera_start_viewfinder(d->cam_handle, bb10capture_video_callback, NULL, d);
	d->capture_started = TRUE;
}

static void bb10capture_stop_capture(BB10Capture *d) {
	if (!d->capture_started) {
		ms_warning("[bb10_capture] capture not started, skipping...");
		return;
	}
	
	camera_stop_viewfinder(d->cam_handle);
	d->capture_started = FALSE;
	
}

static void bb10capture_close_camera(BB10Capture *d) {
	if (!d->camera_openned) {
		ms_warning("[bb10_capture] camera not openned, skipping...");
		return;
	}
	
	camera_close(d->cam_handle);
	d->camera_openned = FALSE;
}

static void bb10capture_init(MSFilter *f) {
	BB10Capture *d = (BB10Capture*) ms_new0(BB10Capture, 1);
	MSVideoSize def_size;
	camera_error_t error;
	
	d->camera_openned = FALSE;
	d->capture_started = FALSE;
	def_size.width = MS_VIDEO_SIZE_QVGA_W;
	def_size.height = MS_VIDEO_SIZE_QVGA_H;
	d->framerate = 15.0;
	d->vsize = def_size;
	d->camera = CAMERA_UNIT_NONE;
	ms_queue_init(&d->rq);
	
	f->data = d;
}

static void bb10capture_uninit(MSFilter *f) {
	BB10Capture *d = (BB10Capture*) f->data;
	
	ms_mutex_destroy(&d->mutex);
	ms_free(d);
}

static void bb10capture_preprocess(MSFilter *f) {
	BB10Capture *d = (BB10Capture*) f->data;
	
	bb10capture_open_camera(d);
	ms_average_fps_init(&d->avgfps, "[bb10_capture] fps=%f");
	bb10capture_start_capture(d);
}

static void bb10capture_postprocess(MSFilter *f) {
	BB10Capture *d = (BB10Capture*) f->data;
	
	bb10capture_stop_capture(d);
	bb10capture_close_camera(d);
}

static void bb10capture_process(MSFilter *f) {
	BB10Capture *d = (BB10Capture*) f->data;
	mblk_t *om = NULL;
	mblk_t *tmp = NULL;
	uint32_t timestamp = 0;

	ms_filter_lock(f);
	
	ms_mutex_lock(&d->mutex);
	while ((tmp = ms_queue_get(&d->rq)) != NULL) {
		if (om != NULL) freemsg(om);
		om = tmp;
	}
	ms_mutex_unlock(&d->mutex);
	
	if (om != NULL) {
		timestamp = f->ticker->time * 90;/* rtp uses a 90000 Hz clockrate for video*/
		mblk_set_timestamp_info(om, timestamp);
		ms_queue_put(f->outputs[0], om);
		ms_average_fps_update(&d->avgfps, f->ticker->time);
	}
	
	ms_filter_unlock(f);
}

static int bb10capture_set_vsize(MSFilter *f, void *arg) {
	BB10Capture *d = (BB10Capture*) f->data;
	
	ms_filter_lock(f);
	d->vsize = *(MSVideoSize*)arg;
	
	if (d->capture_started) {
		bb10capture_stop_capture(d);
	}
	bb10capture_start_capture(d);
	ms_filter_unlock(f);
	
	return 0;
}

static int bb10capture_set_fps(MSFilter *f, void *arg){
	BB10Capture *d = (BB10Capture*) f->data;
	
	ms_filter_lock(f);
	d->framerate = *(float*)arg;
	
	if (d->capture_started) {
		bb10capture_stop_capture(d);
	}
	bb10capture_start_capture(d);
	ms_filter_unlock(f);
	return 0;
}

static int bb10capture_get_vsize(MSFilter *f, void *arg) {
	BB10Capture *d = (BB10Capture*) f->data;
	*(MSVideoSize*)arg = d->vsize;
	return 0;
}

static int bb10capture_get_fps(MSFilter *f, void *arg){
	BB10Capture *d = (BB10Capture*) f->data;
	if (f->ticker){
		*(float*)arg=ms_average_fps_get(&d->avgfps);
	}else {
		*(float*)arg = d->framerate;
	}
	return 0;
}

static int bb10capture_get_pixfmt(MSFilter *f, void *arg) {
	BB10Capture *d = (BB10Capture*) f->data;
	MSPixFmt pix_fmt = MS_YUV420P;
	*(MSPixFmt*)arg = pix_fmt;
	return 0;
}

static MSFilterMethod ms_bb10capture_methods[] = {
	{ MS_FILTER_SET_VIDEO_SIZE,					bb10capture_set_vsize },
	{ MS_FILTER_SET_FPS,						bb10capture_set_fps },
	{ MS_FILTER_GET_VIDEO_SIZE,					bb10capture_get_vsize },
	{ MS_FILTER_GET_FPS,						bb10capture_get_fps },
	{ MS_FILTER_GET_PIX_FMT,					bb10capture_get_pixfmt },
	{ 0,										NULL }
};

MSFilterDesc ms_bb10_capture_desc = {
	MS_BB10_CAPTURE_ID,
	"MSBB10Capture",
	"A capture filter for blackberry 10",
	MS_FILTER_OTHER,
	NULL,
	0,
	1,
	bb10capture_init,
	bb10capture_preprocess,
	bb10capture_process,
	bb10capture_postprocess,
	bb10capture_uninit,
	ms_bb10capture_methods
};

MS_FILTER_DESC_EXPORT(ms_bb10_capture_desc)

static MSFilter *bb10camera_create_reader(MSWebCam *obj) {
	MSFilter *f = ms_filter_new_from_desc(&ms_bb10_capture_desc);
	BB10Capture *d = (BB10Capture*) f->data;
	if (strcmp(obj->name, "BB10 Rear Camera") == 0) {
		d->camera = CAMERA_UNIT_REAR;
	} else {
		d->camera = CAMERA_UNIT_FRONT;
	}
	ms_message("[bb10_capture] create reader with id:%i and camera %s (%i)", ms_bb10_capture_desc.id, obj->name, d->camera);
	return f;
}

static void bb10camera_detect(MSWebCamManager *obj);

static void bb10camera_init(MSWebCam *cam) {
	
}

MSWebCamDesc ms_bb10_camera_desc = {
	"BB10Camera",
	&bb10camera_detect,
	&bb10camera_init,
	&bb10camera_create_reader,
	NULL
};

static void bb10camera_detect(MSWebCamManager *obj) {
	camera_error_t error;
	camera_handle_t handle;
	
	error = camera_open(CAMERA_UNIT_FRONT, CAMERA_MODE_RW, &handle);
	if (error == CAMERA_EOK) {
		if (camera_has_feature(handle, CAMERA_FEATURE_VIDEO)) {
			if (camera_can_feature(handle, CAMERA_FEATURE_VIDEO)) {
				MSWebCam *cam = ms_web_cam_new(&ms_bb10_camera_desc);
				cam->name = ms_strdup("BB10 Front Camera");
				ms_message("[bb10_capture] camera added: %s", cam->name);
				ms_web_cam_manager_add_cam(obj, cam);
				camera_close(handle);
			} else {
				ms_warning("[bb10_capture] front camera has video feature but can't do it...");
			}
		} else {
			ms_warning("[bb10_capture] front camera doesn't have video feature");
		}
	} else {
		ms_warning("[bb10_capture] Can't open front camera: %s", error_to_string(error));
	}
	
	error = camera_open(CAMERA_UNIT_REAR, CAMERA_MODE_RW, &handle);
	if (error == CAMERA_EOK) {
		if (camera_has_feature(handle, CAMERA_FEATURE_VIDEO)) {
			if (camera_can_feature(handle, CAMERA_FEATURE_VIDEO)) {
				MSWebCam *cam = ms_web_cam_new(&ms_bb10_camera_desc);
				cam->name = ms_strdup("BB10 Rear Camera");
				ms_message("[bb10_capture] camera added: %s", cam->name);
				ms_web_cam_manager_add_cam(obj, cam);
				camera_close(handle);
			} else {
				ms_warning("[bb10_capture] rear camera has video feature but can't do it...");
			}
		} else {
			ms_warning("[bb10_capture] rear camera doesn't have video feature");
		}
	} else {
		ms_warning("[bb10_capture] Can't open rear camera: %s", error_to_string(error));
	}
}
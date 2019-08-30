/*
 * android-camera2-capture.cpp - Android capture plugin using NDK Camera2 APIs.
 *
 * Copyright (C) 2019 Belledonne Communications, Grenoble, France
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

#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/msjava.h>
#include <mediastreamer2/msticker.h>
#include <mediastreamer2/mssndcard.h>
#include <mediastreamer2/mswebcam.h>
#include <mediastreamer2/android_utils.h>

#include <jni.h>

#include <camera/NdkCameraCaptureSession.h>
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraError.h>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraMetadata.h>
#include <camera/NdkCameraMetadataTags.h>
#include <camera/NdkCaptureRequest.h>

#include <math.h>

struct AndroidCamera2Context {
	AndroidCamera2Context(MSFilter *f) {
		
	};

	~AndroidCamera2Context() {
		
	};
};

static void android_camera2_capture_init(MSFilter *f) { }

static void android_camera2_capture_preprocess(MSFilter *f) { }

static void android_camera2_capture_process(MSFilter *f) { }

static void android_camera2_capture_postprocess(MSFilter *f) { }

static void android_camera2_capture_uninit(MSFilter *f) { }

static int android_camera2_capture_set_native_preview_window(MSFilter *f, void *arg) { return 0; }

static MSFilterMethod android_camera2_capture_methods[] = {
		{	MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID , &android_camera2_capture_set_native_preview_window },
		{	0,0 }
};

MSFilterDesc ms_android_camera2_capture_desc={
		MS_ANDROID_VIDEO_READ_ID,
		"MSAndroidCamera2Capture",
		"A filter that captures Android video using camera2 APIs.",
		MS_FILTER_OTHER,
		NULL,
		0,
		1,
		android_camera2_capture_init,
		android_camera2_capture_preprocess,
		android_camera2_capture_process,
		android_camera2_capture_postprocess,
		android_camera2_capture_uninit,
		android_camera2_capture_methods
};

static void android_camera2_capture_detect(MSWebCamManager *obj);

static void android_camera2_capture_cam_init(MSWebCam *cam) {
	ms_message("[Camera2 Capture] Initializing filter");
}

static MSFilter *android_camera2_capture_create_reader(MSWebCam *obj) {
	ms_message("[Camera2 Capture] Creating reader");
	MSFilter* filter = ms_factory_create_filter_from_desc(ms_web_cam_get_factory(obj), &ms_android_camera2_capture_desc);
	return filter;
}

MSWebCamDesc ms_android_camera2_capture_webcam_desc={
		"AndroidCamera2Capture",
		&android_camera2_capture_detect,
		&android_camera2_capture_cam_init,
		&android_camera2_capture_create_reader,
		NULL
};

void android_camera2_capture_detect(MSWebCamManager *obj) {
	ms_message("[Camera2 Capture] Detecting cameras");

	ACameraIdList *cameraIdList = NULL;
    ACameraMetadata *cameraMetadata = NULL;

	camera_status_t camera_status = ACAMERA_OK;
    ACameraManager *cameraManager = ACameraManager_create();

    camera_status = ACameraManager_getCameraIdList(cameraManager, &cameraIdList);
    if (camera_status != ACAMERA_OK) {
        ms_error("[Camera2 Capture] Failed to get camera(s) list : %d", camera_status);
        return;
    }

	if (cameraIdList->numCameras < 1) {
        ms_warning("[Camera2 Capture] No camera detected, check you have granted CAMERA permission !");
        return;
    }

	const char *camId = NULL;
	for (int i = 0; i < cameraIdList->numCameras; i++) {
		camId = cameraIdList->cameraIds[0];

		camera_status = ACameraManager_getCameraCharacteristics(cameraManager, camId, &cameraMetadata);
		if (camera_status != ACAMERA_OK) {
			ms_error("[Camera2 Capture] Failed to get camera %s characteristics", camId);
		} else {
  			ACameraMetadata_const_entry orientation;
			ACameraMetadata_getConstEntry(cameraMetadata, ACAMERA_SENSOR_ORIENTATION, &orientation));
			int32_t angle = orientation.data.i32[0];

  			ACameraMetadata_const_entry face;
			ACameraMetadata_getConstEntry(cameraMetadata, ACAMERA_LENS_FACING, &face));
			if (face.data.u8[0] == ACAMERA_LENS_FACING_BACK) {
				ms_message("[Camera2 Capture] Camera %s is facing back with angle %8d", camId, angle);
			} else {
				ms_message("[Camera2 Capture] Camera %s is facing front with angle %8d", camId, angle);
			}

			MSWebCam *cam = ms_web_cam_new(&ms_android_camera2_capture_webcam_desc);
			cam->data = NULL;
			cam->name = ms_strdup("Android camera");
			cam->id = ms_strdup(camId);
			ms_web_cam_manager_add_cam(obj, cam);

			ACameraMetadata_free(cameraMetadata);
		}
	}

	ACameraManager_deleteCameraIdList(cameraIdList);
    ACameraManager_delete(cameraManager);
}

#ifdef _MSC_VER
#define MS_PLUGIN_DECLARE(type) extern "C" __declspec(dllexport) type
#else
#define MS_PLUGIN_DECLARE(type) extern "C" type
#endif

MS_PLUGIN_DECLARE(void) libmsandroidcamera2_init(MSFactory* factory) {
	ms_factory_register_filter(factory, &ms_android_camera2_capture_desc);
	ms_message("[Camera2 Capture] libmsandroidcamera2 plugin loaded");
}
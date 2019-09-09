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
#include <mediastreamer2/msvideo.h>
#include <mediastreamer2/msjava.h>
#include <mediastreamer2/msticker.h>
#include <mediastreamer2/mssndcard.h>
#include <mediastreamer2/mswebcam.h>
#include <mediastreamer2/android_utils.h>

#include <camera/NdkCameraCaptureSession.h>
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraError.h>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraMetadata.h>
#include <camera/NdkCameraMetadataTags.h>
#include <camera/NdkCaptureRequest.h>
#include <media/NdkImageReader.h>

#include <jni.h>
#include <math.h>
#include <android/hardware_buffer.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/rect.h>
#include <android/window.h>

struct AndroidCamera2Device {
	AndroidCamera2Device(char *id) : camId(id), orientation(0), back_facing(false) {
		
	};

	~AndroidCamera2Device() {
		if (camId) {
			ms_free(camId);
		}
	};

	char *camId;
	int32_t orientation;
	bool back_facing;
};

struct AndroidCamera2Context {
	AndroidCamera2Context(MSFilter *f) : filter(f), capturing(false), device(nullptr), rotation(0), surfaceTexture(nullptr), surface(nullptr),
			captureFormat(AIMAGE_FORMAT_YUV_420_888), previewFormat(WINDOW_FORMAT_RGBX_8888), 
			frame(nullptr), bufAllocator(ms_yuv_buf_allocator_new()), fps(5), 
			cameraDevice(nullptr), captureSession(nullptr), captureSessionOutputContainer(nullptr), 
			nativeWindow(nullptr), captureWindow(nullptr), captureRequest(nullptr), cameraOutputTarget(nullptr),
			sessionOutput(nullptr), imageReader(nullptr) 
	{
		captureSize.width = 0;
		captureSize.height = 0;
		previewSize.width = 0;
		previewSize.height = 0;
		ms_mutex_init(&mutex, NULL);
	};

	~AndroidCamera2Context() {
		// Don't delete device object in here !
		ms_mutex_destroy(&mutex);
		if (bufAllocator) ms_yuv_buf_allocator_free(bufAllocator);
	};

	MSFilter *filter;
	bool capturing;
	AndroidCamera2Device *device;
	int rotation;
	jobject surfaceTexture;
	jobject surface;

	MSVideoSize captureSize;
	MSVideoSize previewSize;
	int32_t captureFormat;
	int32_t previewFormat;

	ms_mutex_t mutex;
	mblk_t *frame;
	MSYuvBufAllocator *bufAllocator;

	float fps;
	MSFrameRateController fpsControl;
	MSAverageFPS averageFps;
	char fps_context[64];

	ACameraDevice *cameraDevice;
	ACameraCaptureSession *captureSession;
	ACaptureSessionOutputContainer *captureSessionOutputContainer;

	ANativeWindow *nativeWindow;
	ANativeWindow *captureWindow;
	ACaptureRequest *captureRequest;
	ACameraOutputTarget *cameraOutputTarget;
	ACaptureSessionOutput *sessionOutput;
	AImageReader *imageReader;

	ACameraDevice_StateCallbacks deviceStateCallbacks;
	ACameraCaptureSession_stateCallbacks captureSessionStateCallbacks;
};

/* ************************************************************************* */

static void android_camera2_capture_device_on_disconnected(void *context, ACameraDevice *device) {
    ms_message("[Camera2 Capture] Camera %s is diconnected", ACameraDevice_getId(device));
}

static void android_camera2_capture_device_on_error(void *context, ACameraDevice *device, int error) {
    ms_error("[Camera2 Capture] Error %d on camera %s", error, ACameraDevice_getId(device));
}

static void android_camera2_capture_session_on_ready(void *context, ACameraCaptureSession *session) {
    ms_message("[Camera2 Capture] Session is ready %p", session);
}

static void android_camera2_capture_session_on_active(void *context, ACameraCaptureSession *session) {
    ms_message("[Camera2 Capture] Session is activated %p", session);
}

static void android_camera2_capture_session_on_closed(void *context, ACameraCaptureSession *session) {
    ms_message("[Camera2 Capture] Session is closed %p", session);
}

/* ************************************************************************* */

static inline uint32_t android_camera2_capture_yuv_to_rgb(int nY, int nU, int nV) {
	const int kMaxChannelValue = 262143;
	nY -= 16;
	nU -= 128;
	nV -= 128;
	if (nY < 0) nY = 0;

	int nR = (int)(1192 * nY + 1634 * nV);
	int nG = (int)(1192 * nY - 833 * nV - 400 * nU);
	int nB = (int)(1192 * nY + 2066 * nU);

	nR = MIN(kMaxChannelValue, MAX(0, nR));
	nG = MIN(kMaxChannelValue, MAX(0, nG));
	nB = MIN(kMaxChannelValue, MAX(0, nB));

	nR = (nR >> 10) & 0xff;
	nG = (nG >> 10) & 0xff;
	nB = (nB >> 10) & 0xff;

	return 0xff000000 | (nR << 16) | (nG << 8) | nB;
}

static int32_t android_camera2_capture_get_orientation(AndroidCamera2Context *d) {
	int32_t orientation = d->device->orientation;
	if (d->device->back_facing) {
		orientation -= d->rotation;
	} else {
		orientation += d->rotation;
	}
	if (orientation < 0) {
		orientation += 360;
	}
	orientation = orientation % 360;
	return orientation;
}

static mblk_t* android_camera2_capture_image_to_mblkt(AndroidCamera2Context *d, AImage *image) {
	int32_t width, height;
	int32_t yStride, uvStride;
	uint8_t *yPixel, *uPixel, *vPixel;
	int32_t yLen, uLen, vLen;
	int32_t yPixelStride, uvPixelStride;
	int32_t orientation = android_camera2_capture_get_orientation(d);

	AImage_getWidth(image, &width);
	AImage_getHeight(image, &height);
	if (orientation % 180 != 0) {
		int32_t tmp = width;
		width = height;
		height = tmp;
	}

	AImage_getPlaneRowStride(image, 0, &yStride);
	AImage_getPlaneRowStride(image, 1, &uvStride);
	AImage_getPlaneData(image, 0, &yPixel, &yLen);
	AImage_getPlaneData(image, 1, &uPixel, &uLen);
	AImage_getPlaneData(image, 2, &vPixel, &vLen);
	AImage_getPlanePixelStride(image, 0, &yPixelStride);
	AImage_getPlanePixelStride(image, 1, &uvPixelStride);

	//ms_message("[Camera2 Capture] Image %p size %d/%d, y is %p, u is %p, v is %p, ystride %d, uvstride %d, ypixelstride %d, uvpixelstride %d", image, width, height, yPixel, uPixel, vPixel, yStride, uvStride, yPixelStride, uvPixelStride);

	mblk_t* yuv_block = nullptr;
	if (uvPixelStride == 1) {
		yuv_block = copy_yuv_with_rotation(d->bufAllocator, yPixel, uPixel, vPixel, orientation, width, height, yStride, uvStride, uvStride);
	} else {
		yuv_block = copy_ycbcrbiplanar_to_true_yuv_with_rotation_and_down_scale_by_2(d->bufAllocator,
	 		yPixel, vPixel < uPixel ? vPixel : uPixel, orientation, width, height, yStride, uvStride, uPixel < vPixel, false);
	}
	return yuv_block;
}

static void android_camera2_capture_rotate_image(AndroidCamera2Context *d, ANativeWindow_Buffer *buf, AImage *image) {
	int32_t yStride, uvStride;
	uint8_t *yPixel, *uPixel, *vPixel;
	int32_t yLen, uLen, vLen;
	int32_t uvPixelStride;
	int32_t orientation = android_camera2_capture_get_orientation(d);
  	AImageCropRect srcRect;

  	AImage_getCropRect(image, &srcRect);

	int32_t transform = ANATIVEWINDOW_TRANSFORM_IDENTITY;
	if (orientation == 90) {
		transform = ANATIVEWINDOW_TRANSFORM_ROTATE_90;
	} else if (orientation == 180) {
		transform = ANATIVEWINDOW_TRANSFORM_ROTATE_180;
	} else if (orientation == 270) {
		transform = ANATIVEWINDOW_TRANSFORM_ROTATE_270;
	}
	ANativeWindow_setBuffersTransform(d->nativeWindow, transform);

	AImage_getPlaneRowStride(image, 0, &yStride);
	AImage_getPlaneRowStride(image, 1, &uvStride);
	AImage_getPlaneData(image, 0, &yPixel, &yLen);
	AImage_getPlaneData(image, 1, &vPixel, &vLen);
	AImage_getPlaneData(image, 2, &uPixel, &uLen);
	AImage_getPlanePixelStride(image, 1, &uvPixelStride);

	int32_t height = MIN(buf->height, (srcRect.bottom - srcRect.top));
  	int32_t width = MIN(buf->width, (srcRect.right - srcRect.left));

	uint32_t *out = static_cast<uint32_t *>(buf->bits);
	for (int32_t y = 0; y < height; y++) {
		const uint8_t *pY = yPixel + yStride * (y + srcRect.top) + srcRect.left;

		int32_t uv_row_start = uvStride * ((y + srcRect.top) >> 1);
		const uint8_t *pU = uPixel + uv_row_start + (srcRect.left >> 1);
		const uint8_t *pV = vPixel + uv_row_start + (srcRect.left >> 1);

		for (int32_t x = 0; x < width; x++) {
			const int32_t uv_offset = (x >> 1) * uvPixelStride;
			out[x] = android_camera2_capture_yuv_to_rgb(pY[x], pU[uv_offset], pV[uv_offset]);
		}
		out += buf->stride;
	}
}

static void android_camera2_capture_display_image(AndroidCamera2Context *d, AImage *image) {
	if (!d->nativeWindow) {
		ms_error("[Camera2 Capture] Preview surface wasn't set yet");
		return;
	}

	ANativeWindow_Buffer buffer;
	ANativeWindow_acquire(d->nativeWindow);
	int32_t status = ANativeWindow_lock(d->nativeWindow, &buffer, nullptr);
	if (status < 0) {
		ms_error("[Camera2 Capture] Couldn't lock preview window: %d", status);
		ANativeWindow_release(d->nativeWindow);
		return;
	}

	if (buffer.format == d->previewFormat) {
		// No need for conversion, just display it
		android_camera2_capture_rotate_image(d, &buffer, image);
	} else {
		ms_error("[Camera2 Capture] Acquired buffer is in wrong format %d, expected %d", buffer.format, d->previewFormat);
	}
	status = ANativeWindow_unlockAndPost(d->nativeWindow);
	ANativeWindow_release(d->nativeWindow);
	if (status < 0) {
		ms_error("[Camera2 Capture] Couldn't unlock and post buffer: %d", status);
	}
}

static void android_camera2_capture_on_image_available(void *context, AImageReader *reader) {
	AndroidCamera2Context *d = static_cast<AndroidCamera2Context *>(context);

	int32_t format;
  	media_status_t status = AImageReader_getFormat(reader, &format);
	if (format == d->captureFormat) {
		AImage *image = nullptr;
		status = AImageReader_acquireNextImage(reader, &image);
		if (status == AMEDIA_OK) {
			if (d->capturing) {
				ms_filter_lock(d->filter);
				if (d->filter == nullptr && d->filter->ticker == nullptr) {
					ms_error("[Camera2 Capture] Filter destroyed, we shouldn't be here !");
					ms_filter_unlock(d->filter);
					return;
				}
				if (ms_video_capture_new_frame(&d->fpsControl, d->filter->ticker->time)) {
					mblk_t *m = android_camera2_capture_image_to_mblkt(d, image);
					if (m) {
						ms_mutex_lock(&d->mutex);
						if (d->frame) freemsg(d->frame);
						d->frame = m;
						ms_mutex_unlock(&d->mutex);
					}
				}

				android_camera2_capture_display_image(d, image);
				ms_filter_unlock(d->filter);
			}
			
			AImage_delete(image);
		} else {
			ms_error("[Camera2 Capture] Couldn't acquire image");
		}
	} else {
		ms_error("[Camera2 Capture] Aquired image is in wrong format %d, expected %d", format, d->captureFormat);
	}
}

/* ************************************************************************* */

static void android_camera2_capture_create_preview(AndroidCamera2Context *d) {
    ms_message("[Camera2 Capture] Creating preview");
   	JNIEnv *jenv = ms_get_jni_env();

	if (!d->surface) {
		ms_error("[Camera2 Capture] Can't create preview window, no surface");
		return;
	}

	d->nativeWindow = ANativeWindow_fromSurface(jenv, d->surface);
	ANativeWindow_setBuffersGeometry(d->nativeWindow, d->captureSize.width, d->captureSize.height, d->previewFormat);
	ms_message("[Camera2 Capture] Asked buffer size to be %i/%i with format %i", d->captureSize.width, d->captureSize.height, d->previewFormat);

	d->previewSize.width = ANativeWindow_getWidth(d->nativeWindow);
	d->previewSize.height = ANativeWindow_getHeight(d->nativeWindow);
	d->previewFormat = ANativeWindow_getFormat(d->nativeWindow);
	ms_message("[Camera2 Capture] Preview display size is %i/%i, format is %i", d->previewSize.width, d->previewSize.height, d->previewFormat);
}

static void android_camera2_capture_destroy_preview(AndroidCamera2Context *d) {
    ms_message("[Camera2 Capture] Destroying preview");
	if (d->nativeWindow) {
		ANativeWindow_release(d->nativeWindow);
		d->nativeWindow = nullptr;
    	ms_message("[Camera2 Capture] Preview window destroyed");
	}
	if (d->surface) {
		JNIEnv *env = ms_get_jni_env();
		env->DeleteGlobalRef(d->surface);
		d->surface = nullptr;
    	ms_message("[Camera2 Capture] Preview surface destroyed");
	}
}

static void android_camera2_capture_open_camera(AndroidCamera2Context *d) {
	ms_message("[Camera2 Capture] Opening camera");
	d->deviceStateCallbacks.onDisconnected = android_camera2_capture_device_on_disconnected;
    d->deviceStateCallbacks.onError = android_camera2_capture_device_on_error;

	if (!d->device) {
		ms_error("[Camera2 Capture] Can't open camera, no device selected");
		return;
	}

	ACameraDevice *cameraDevice;
    ACameraManager *cameraManager = ACameraManager_create();
	ms_message("[Camera2 Capture] Opening camera %s", d->device->camId);
	camera_status_t camera_status = ACameraManager_openCamera(cameraManager, d->device->camId, &d->deviceStateCallbacks, &cameraDevice);

	if (camera_status != ACAMERA_OK) {
		ms_error("[Camera2 Capture] Failed to open camera %s", d->device->camId);
		return;
    }
	d->cameraDevice = cameraDevice;

	ACameraManager_delete(cameraManager);
}

static void android_camera2_capture_close_camera(AndroidCamera2Context *d) {
	ms_message("[Camera2 Capture] Closing camera");

    if (d->cameraDevice) {
        camera_status_t camera_status = ACameraDevice_close(d->cameraDevice);
        if (camera_status != ACAMERA_OK) {
            ms_error("[Camera2 Capture] Failed to close camera %s", d->device->camId);
        } else {
			ms_message("[Camera2 Capture] Camera closed %s", d->device->camId);
		}
        d->cameraDevice = nullptr;
    }
}

static void android_camera2_capture_start(AndroidCamera2Context *d) {
	ms_message("[Camera2 Capture] Starting capture");
	if (d->captureSize.width == 0 || d->captureSize.height == 0 || d->surface == nullptr) {
		ms_warning("[Camera2 Capture] Filter hasn't been fully configured yet, don't start");
		return;
	}
	if (d->capturing) {
		ms_warning("[Camera2 Capture] Capture was already started, ignoring...");
		return;
	}
	
	if (!d->cameraDevice) {
		android_camera2_capture_open_camera(d);
	}
	if (!d->nativeWindow && d->surface) {
		android_camera2_capture_create_preview(d);
	}

	ACaptureSessionOutputContainer_create(&d->captureSessionOutputContainer);
	d->captureSessionStateCallbacks.onReady = android_camera2_capture_session_on_ready;
	d->captureSessionStateCallbacks.onActive = android_camera2_capture_session_on_active;
	d->captureSessionStateCallbacks.onClosed = android_camera2_capture_session_on_closed;

	media_status_t status = AImageReader_new(d->captureSize.width, d->captureSize.height, d->captureFormat, 1, &d->imageReader);
	if (status != AMEDIA_OK) {
		ms_error("[Camera2 Capture] Failed to create image reader");
		return;
	}

	AImageReader_ImageListener listener {
    	.context = d, .onImageAvailable = android_camera2_capture_on_image_available,
  	};
  	AImageReader_setImageListener(d->imageReader, &listener);
	ms_message("[Camera2 Capture] Image reader created");

  	status = AImageReader_getWindow(d->imageReader, &d->captureWindow);
	if (status != AMEDIA_OK) {
		ms_error("[Camera2 Capture] Capture window couldn't be acquired");
		return;
	}
	ANativeWindow_acquire(d->captureWindow);
	ms_message("[Camera2 Capture] Capture window acquired");

    ACaptureSessionOutput_create(d->captureWindow, &d->sessionOutput);
    ACaptureSessionOutputContainer_add(d->captureSessionOutputContainer, d->sessionOutput);

	ACameraOutputTarget_create(d->captureWindow, &d->cameraOutputTarget);
	camera_status_t camera_status = ACameraDevice_createCaptureRequest(d->cameraDevice, TEMPLATE_RECORD, &d->captureRequest); // TEMPLATE_RECORD > TEMPLATE_PREVIEW > TEMPLATE_STILL_CAPTURE in fps ?
	if (camera_status != ACAMERA_OK) {
		ms_error("[Camera2 Capture] Failed to create capture request");
	}
    ACaptureRequest_addTarget(d->captureRequest, d->cameraOutputTarget);

    ACameraDevice_createCaptureSession(d->cameraDevice, d->captureSessionOutputContainer, &d->captureSessionStateCallbacks, &d->captureSession);
    ACameraCaptureSession_setRepeatingRequest(d->captureSession, NULL, 1, &d->captureRequest, NULL);

	d->capturing = true;
	ms_message("[Camera2 Capture] Capture started");
}

static void android_camera2_capture_stop(AndroidCamera2Context *d) {
	ms_message("[Camera2 Capture] Stopping capture");
	if (!d->capturing) {
		ms_warning("[Camera2 Capture] Capture was already stopped, ignoring...");
		return;
	}
	d->capturing = false;

	if (d->captureSession) {
		ACameraCaptureSession_stopRepeating(d->captureSession);
		ACameraCaptureSession_close(d->captureSession);
		d->captureSession = nullptr;
	}

	if (d->cameraOutputTarget) {
		ACameraOutputTarget_free(d->cameraOutputTarget);
		d->cameraOutputTarget = nullptr;
    }

	if (d->captureRequest) {
		ACaptureRequest_free(d->captureRequest);
		d->captureRequest = nullptr;
    }

	if (d->captureSessionOutputContainer) {
		ACaptureSessionOutputContainer_remove(d->captureSessionOutputContainer, d->sessionOutput);
	}

	if (d->sessionOutput) {
		ACaptureSessionOutput_free(d->sessionOutput);
		d->sessionOutput = nullptr;
    }

	if (d->captureWindow) {
		ANativeWindow_release(d->captureWindow);
		d->captureWindow = nullptr;
	}

	if (d->imageReader) {
		AImageReader_delete(d->imageReader);
		d->imageReader = nullptr;
	}

    if (d->captureSessionOutputContainer) {
		ACaptureSessionOutputContainer_free(d->captureSessionOutputContainer);
		d->captureSessionOutputContainer = nullptr;
    }

	android_camera2_capture_destroy_preview(d);
	android_camera2_capture_close_camera(d);

	ms_message("[Camera2 Capture] Capture stopped");
}

/* ************************************************************************* */

static void android_camera2_capture_init(MSFilter *f) {
	ms_message("[Camera2 Capture] Filter init");
	AndroidCamera2Context* d = new AndroidCamera2Context(f);
	f->data = d;
}

static void android_camera2_capture_preprocess(MSFilter *f) {
	ms_message("[Camera2 Capture] Filter preprocess");
	AndroidCamera2Context *d = (AndroidCamera2Context *)f->data;

	ms_filter_lock(f);

	ms_video_init_framerate_controller(&d->fpsControl, d->fps);
	ms_video_init_average_fps(&d->averageFps, d->fps_context);

	ms_filter_unlock(f);
}

static void android_camera2_capture_process(MSFilter *f) {
	AndroidCamera2Context *d = (AndroidCamera2Context *)f->data;
	ms_filter_lock(f);

	ms_mutex_lock(&d->mutex);
	if (d->frame) {
		ms_video_update_average_fps(&d->averageFps, f->ticker->time);
		mblk_set_timestamp_info(d->frame, f->ticker->time * 90);
		ms_queue_put(f->outputs[0], d->frame);
		d->frame = nullptr;
	}
	ms_mutex_unlock(&d->mutex);

	ms_filter_unlock(f);
}

static void android_camera2_capture_postprocess(MSFilter *f) {
	ms_message("[Camera2 Capture] Filter postprocess");
	AndroidCamera2Context *d = (AndroidCamera2Context *)f->data;
	ms_filter_lock(f);
	if (d->capturing) {
		android_camera2_capture_stop(d);
	}
	ms_filter_unlock(f);
}

static void android_camera2_capture_uninit(MSFilter *f) {
	ms_message("[Camera2 Capture] Filter uninit");
	ms_filter_lock(f);
	AndroidCamera2Context *d = (AndroidCamera2Context *)f->data;
	delete d;
	ms_filter_unlock(f);
}

/* ************************************************************************* */

static int android_camera2_capture_set_fps(MSFilter *f, void *arg) {
	AndroidCamera2Context *d = (AndroidCamera2Context *)f->data;
	d->fps = *((float*)arg);
	snprintf(d->fps_context, sizeof(d->fps_context), "Captured mean fps=%%f, expected=%f", d->fps);
	ms_filter_lock(f);
	ms_video_init_framerate_controller(&d->fpsControl, d->fps);
	ms_video_init_average_fps(&d->averageFps, d->fps_context);
	ms_filter_unlock(f);
	return 0;
}

static int android_camera2_capture_get_fps(MSFilter *f, void *arg) {
	AndroidCamera2Context *d = (AndroidCamera2Context *)f->data;
	*((float*)arg) = ms_average_fps_get(&d->averageFps);
	return 0;
}

static void android_camera2_capture_choose_best_configurations(AndroidCamera2Context *d) {
	ms_message("[Camera2 Capture] Listing camera %s configurations", d->device->camId);

    ACameraMetadata *cameraMetadata = nullptr;
    ACameraManager *cameraManager = ACameraManager_create();
	camera_status_t camera_status = ACameraManager_getCameraCharacteristics(cameraManager, d->device->camId, &cameraMetadata);

	ACameraMetadata_const_entry scaler;
	ACameraMetadata_getConstEntry(cameraMetadata, ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, &scaler);
	
	MSVideoSize backupSize;
	backupSize.width = 0;
	backupSize.height = 0;
	double askedRatio = d->captureSize.width * d->captureSize.height;
	bool found = false;
	
	for (int i = 0; i < scaler.count; i += 4) {
		int32_t input = scaler.data.i32[i + 3];
		int32_t format = scaler.data.i32[i + 0];
		if (input) continue;

		if (format == d->captureFormat) {
			int32_t width = scaler.data.i32[i + 1];
			int32_t height = scaler.data.i32[i + 2];
			double currentSizeRatio = width * height;
			ms_message("[Camera2 Capture] Available size width %d, height %d for requested format %d", width, height, format);

			if (width == d->captureSize.width && height == d->captureSize.height) {
				found = true;
			} else {
				double backupRatio = backupSize.width * backupSize.height;
				if (backupRatio == 0 || fabs(askedRatio - currentSizeRatio) < fabs(askedRatio - backupRatio)) {
					// Current resolution is closer to the one we want than the one in backup, update backup
					backupSize.width = width;
					backupSize.height = height;
				}
			}
		}
	}

	if (found) {
		ms_message("[Camera2 Capture] Found exact match for our required size of %ix%i", d->captureSize.width, d->captureSize.height);
	} else {
		// Asked resolution not found
		d->captureSize.width = backupSize.width;
		d->captureSize.height = backupSize.height;
		ms_warning("[Camera2 Capture] Couldn't find requested resolution, instead using %ix%i", backupSize.width, backupSize.height);
	}
}

static int android_camera2_capture_set_vsize(MSFilter *f, void* arg) {
	AndroidCamera2Context *d = (AndroidCamera2Context *)f->data;
	ms_filter_lock(f);

	MSVideoSize requestedSize = *(MSVideoSize*)arg;
	ms_message("[Camera2 Capture] Current preview size is %i/%i, new size will be %i/%i", 
		d->captureSize.width, d->captureSize.height, requestedSize.width, requestedSize.height);
	
	if (d->captureSize.width == requestedSize.width && d->captureSize.height == requestedSize.height) {
		ms_filter_unlock(f);
		return -1;
	}
	d->captureSize = requestedSize;

	android_camera2_capture_stop(d);
	android_camera2_capture_choose_best_configurations(d);
	android_camera2_capture_start(d);

	ms_filter_notify(f, MS_CAMERA_PREVIEW_SIZE_CHANGED, &d->captureSize);

	ms_filter_unlock(f);
	return 0;
}

static int android_camera2_capture_get_vsize(MSFilter *f, void* arg) {
	AndroidCamera2Context *d = (AndroidCamera2Context *)f->data;
	MSVideoSize size;
	int orientation = android_camera2_capture_get_orientation(d);
	if (orientation % 180 == 0) {
		size.width = d->captureSize.width;
		size.height = d->captureSize.height;
	} else {
		size.width = d->captureSize.height;
		size.height = d->captureSize.width;
	}
	*(MSVideoSize*)arg = size;
	ms_message("[Camera2 Capture] Getting preview size: %i/%i", size.width, size.height);
	return 0;
}

static int android_camera2_capture_set_device_rotation(MSFilter* f, void* arg) {
	AndroidCamera2Context *d = (AndroidCamera2Context *)f->data;
	ms_filter_lock(f);
	d->rotation = *((int*)arg);
	ms_message("[Camera2 Capture] Device rotation is %i", d->rotation);
	ms_filter_unlock(f);
	return 0;
}

static void android_camera2_capture_create_surface_from_surface_texture(AndroidCamera2Context *d) {
	ms_message("[Camera2 Capture] Creating Surface from SurfaceTexture");
	JNIEnv *env = ms_get_jni_env();
	jobject surface;
	jclass klass = env->FindClass("android/view/Surface");
	if (!klass) {
		ms_error("[Camera2 Capture] Could not find android.view.Surface class");
		return;
	}

	jmethodID ctor = env->GetMethodID(klass, "<init>", "(Landroid/graphics/SurfaceTexture;)V");
	surface = env->NewObject(klass, ctor, d->surfaceTexture);
	if (!surface) {
		ms_error("[Camera2 Capture] Could not instanciate android.view.Surface object");
		return;
	}
	d->surface = (jobject)env->NewGlobalRef(surface);
	ms_message("[Camera2 Capture] Surface created: %p", d->surface);
}

static int android_camera2_capture_set_surface_texture(MSFilter *f, void *arg) {
	AndroidCamera2Context *d = (AndroidCamera2Context *)f->data;
	ms_filter_lock(f);

	unsigned long id = *(unsigned long *)arg;
	jobject surfaceTexture = (jobject)id;
	ms_message("[Camera2 Capture] New SurfaceTexture jobject ptr is %p, current one is %p", surfaceTexture, d->surfaceTexture);

	if (id == 0 && d->surface) {
		android_camera2_capture_stop(d);
		d->surfaceTexture = nullptr;
	} else if (surfaceTexture != d->surfaceTexture) {
		android_camera2_capture_stop(d);
		d->surfaceTexture = surfaceTexture;
		android_camera2_capture_create_surface_from_surface_texture(d);
		android_camera2_capture_start(d);
	}
	
	ms_filter_unlock(f);
	return 0; 
}

static int android_camera2_capture_get_pix_fmt(MSFilter *f, void *data){
	*(MSPixFmt*)data = MS_YUV420P;
	return 0;
}

/* ************************************************************************* */

static MSFilterMethod android_camera2_capture_methods[] = {
		{ MS_FILTER_SET_FPS, &android_camera2_capture_set_fps },
		{ MS_FILTER_GET_FPS, &android_camera2_capture_get_fps },
		{ MS_FILTER_SET_VIDEO_SIZE, &android_camera2_capture_set_vsize },
		{ MS_FILTER_GET_VIDEO_SIZE, &android_camera2_capture_get_vsize },
		{ MS_VIDEO_CAPTURE_SET_DEVICE_ORIENTATION, &android_camera2_capture_set_device_rotation },
		{ MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID, &android_camera2_capture_set_surface_texture },
		{ MS_FILTER_GET_PIX_FMT, &android_camera2_capture_get_pix_fmt },
		{ 0, 0 }
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

static void android_camera2_capture_cam_init(MSWebCam *cam) { }

static MSFilter *android_camera2_capture_create_reader(MSWebCam *obj) {
	ms_message("[Camera2 Capture] Creating filter for camera %s", obj->id);

	MSFilter* filter = ms_factory_create_filter_from_desc(ms_web_cam_get_factory(obj), &ms_android_camera2_capture_desc);
	AndroidCamera2Context *d = (AndroidCamera2Context *)filter->data;
	d->device = (AndroidCamera2Device *)obj->data;

	return filter;
}

MSWebCamDesc ms_android_camera2_capture_webcam_desc = {
		"AndroidCamera2Capture",
		&android_camera2_capture_detect,
		&android_camera2_capture_cam_init,
		&android_camera2_capture_create_reader,
		NULL
};

void android_camera2_capture_detect(MSWebCamManager *obj) {
	ms_message("[Camera2 Capture] Detecting cameras");

	ACameraIdList *cameraIdList = nullptr;
    ACameraMetadata *cameraMetadata = nullptr;

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

	bool front_facing_found = false;
	bool back_facing_found = false;

	const char *camId = nullptr;
	for (int i = 0; i < cameraIdList->numCameras; i++) {
		camId = cameraIdList->cameraIds[i];

		camera_status = ACameraManager_getCameraCharacteristics(cameraManager, camId, &cameraMetadata);
		if (camera_status != ACAMERA_OK) {
			ms_error("[Camera2 Capture] Failed to get camera %s characteristics", camId);
		} else {
			AndroidCamera2Device *device = new AndroidCamera2Device(ms_strdup(camId));
			
  			ACameraMetadata_const_entry orientation;
			ACameraMetadata_getConstEntry(cameraMetadata, ACAMERA_SENSOR_ORIENTATION, &orientation);
			int32_t angle = orientation.data.i32[0];
			device->orientation = angle;

  			ACameraMetadata_const_entry face;
			ACameraMetadata_getConstEntry(cameraMetadata, ACAMERA_LENS_FACING, &face);
			bool back_facing = face.data.u8[0] == ACAMERA_LENS_FACING_BACK;
			if (back_facing) {
				ms_message("[Camera2 Capture] Camera %s is facing back with angle %d", camId, angle);
			} else {
				ms_message("[Camera2 Capture] Camera %s is facing front with angle %d", camId, angle);
			}
			device->back_facing = back_facing;

			MSWebCam *cam = ms_web_cam_new(&ms_android_camera2_capture_webcam_desc);
			cam->id = ms_strdup(camId);
			cam->name = ms_strdup(camId);
			cam->data = device;

			if ((back_facing && !back_facing_found) || (!back_facing && !front_facing_found)) {
				ms_web_cam_manager_prepend_cam(obj, cam);
				if (back_facing) {
					back_facing_found = true;
				} else {
					front_facing_found = true;
				}
			} else {
				ms_warning("[Camera2 Capture] A camera with the same direction as already been added, skipping this one");
			}

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

	MSWebCamManager *manager = ms_factory_get_web_cam_manager(factory);
	ms_web_cam_manager_register_desc(manager, &ms_android_camera2_capture_webcam_desc);
}
/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * android-camera2-capture.cpp - Android capture plugin using NDK Camera2 APIs.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/msvideo.h>
#include <mediastreamer2/msjava.h>
#include <mediastreamer2/msticker.h>
#include <mediastreamer2/mssndcard.h>
#include <mediastreamer2/mswebcam.h>
#include <mediastreamer2/android_utils.h>

#include <android/native_window_jni.h>
#include <camera/NdkCaptureRequest.h>
#include <camera/NdkCameraCaptureSession.h>
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraError.h>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraMetadata.h>
#include <camera/NdkCameraMetadataTags.h>
#include <media/NdkImageReader.h>

#include <jni.h>
#include <math.h>

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
	AndroidCamera2Context(MSFilter *f) : filter(f), configured(false), capturing(false), device(nullptr), rotation(0), nativeWindowId(nullptr), surface(nullptr),
			captureFormat(AIMAGE_FORMAT_YUV_420_888),
			frame(nullptr), bufAllocator(ms_yuv_buf_allocator_new()), fps(5), 
			cameraDevice(nullptr), captureSession(nullptr), captureSessionOutputContainer(nullptr), 
			nativeWindow(nullptr), captureWindow(nullptr), capturePreviewRequest(nullptr), 
			cameraCaptureOutputTarget(nullptr), cameraPreviewOutputTarget(nullptr),
			sessionCaptureOutput(nullptr), sessionPreviewOutput(nullptr), imageReader(nullptr) 
	{
		captureSize.width = 0;
		captureSize.height = 0;
		previewSize.width = 0;
		previewSize.height = 0;
		ms_mutex_init(&mutex, NULL);
		ms_mutex_init(&imageReaderMutex, NULL);
		snprintf(fps_context, sizeof(fps_context), "Captured mean fps=%%f");

		cameraManager = ACameraManager_create();
		ms_message("[Camera2 Capture] Context ready");
	};

	~AndroidCamera2Context() {
		ms_message("[Camera2 Capture] Context destroyed");
		// Don't delete device object in here !
		ms_mutex_destroy(&mutex);
		ms_mutex_destroy(&imageReaderMutex);
		if (bufAllocator) ms_yuv_buf_allocator_free(bufAllocator);

		ACameraManager_delete(cameraManager);
	};

	MSFilter *filter;
	bool configured;
	bool capturing;
	AndroidCamera2Device *device;
	int rotation;
	jobject nativeWindowId;
	jobject surface;

	MSVideoSize captureSize;
	MSVideoSize previewSize;
	int32_t captureFormat;

	ms_mutex_t mutex;
	ms_mutex_t imageReaderMutex;
	mblk_t *frame;
	MSYuvBufAllocator *bufAllocator;

	float fps;
	MSFrameRateController fpsControl;
	MSAverageFPS averageFps;
	char fps_context[64];

	ACameraManager *cameraManager;
	ACameraDevice *cameraDevice;
	ACameraCaptureSession *captureSession;
	ACaptureSessionOutputContainer *captureSessionOutputContainer;

	ANativeWindow *nativeWindow;
	ANativeWindow *captureWindow;
	ACaptureRequest *capturePreviewRequest;
	ACameraOutputTarget *cameraCaptureOutputTarget;
	ACameraOutputTarget *cameraPreviewOutputTarget;
	ACaptureSessionOutput *sessionCaptureOutput;
	ACaptureSessionOutput *sessionPreviewOutput;
	AImageReader *imageReader;
	AImageReader_ImageListener *imageReaderListener;

	ACameraDevice_StateCallbacks deviceStateCallbacks;
	ACameraCaptureSession_stateCallbacks captureSessionStateCallbacks;
};

/* ************************************************************************* */

static void android_camera2_capture_stop(AndroidCamera2Context *d);

static void android_camera2_capture_device_on_disconnected(void *context, ACameraDevice *device) {
    ms_message("[Camera2 Capture] Camera %s is diconnected", ACameraDevice_getId(device));

	AndroidCamera2Context *d = (AndroidCamera2Context *)context;
	android_camera2_capture_stop(d);
}

static void android_camera2_capture_device_on_error(void *context, ACameraDevice *device, int error) {
    ms_error("[Camera2 Capture] Error %d on camera %s", error, ACameraDevice_getId(device));

	AndroidCamera2Context *d = (AndroidCamera2Context *)context;
	android_camera2_capture_stop(d);
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

// https://developer.android.com/ndk/reference/group/camera.html
static const char* android_camera2_status_to_string(camera_status_t status) {
	if (status == ACAMERA_OK) {
		return "ACAMERA_OK";
	} else if (status == ACAMERA_ERROR_BASE) {
		return "ACAMERA_ERROR_BASE";
	} else if (status == ACAMERA_ERROR_UNKNOWN) {
		return "ACAMERA_ERROR_UNKNOWN";
	} else if (status == ACAMERA_ERROR_INVALID_PARAMETER) {
		return "ACAMERA_ERROR_INVALID_PARAMETER";
	} else if (status == ACAMERA_ERROR_CAMERA_DISCONNECTED) {
		return "ACAMERA_ERROR_CAMERA_DISCONNECTED";
	} else if (status == ACAMERA_ERROR_NOT_ENOUGH_MEMORY) {
		return "ACAMERA_ERROR_NOT_ENOUGH_MEMORY";
	} else if (status == ACAMERA_ERROR_METADATA_NOT_FOUND) {
		return "ACAMERA_ERROR_METADATA_NOT_FOUND";
	} else if (status == ACAMERA_ERROR_CAMERA_DEVICE) {
		return "ACAMERA_ERROR_CAMERA_DEVICE";
	} else if (status == ACAMERA_ERROR_CAMERA_SERVICE) {
		return "ACAMERA_ERROR_CAMERA_SERVICE";
	} else if (status == ACAMERA_ERROR_SESSION_CLOSED) {
		return "ACAMERA_ERROR_SESSION_CLOSED";
	} else if (status == ACAMERA_ERROR_INVALID_OPERATION) {
		return "ACAMERA_ERROR_INVALID_OPERATION";
	} else if (status == ACAMERA_ERROR_STREAM_CONFIGURE_FAIL) {
		return "ACAMERA_ERROR_STREAM_CONFIGURE_FAIL";
	} else if (status == ACAMERA_ERROR_CAMERA_IN_USE) {
		return "ACAMERA_ERROR_CAMERA_IN_USE";
	} else if (status == ACAMERA_ERROR_MAX_CAMERA_IN_USE) {
		return "ACAMERA_ERROR_MAX_CAMERA_IN_USE";
	} else if (status == ACAMERA_ERROR_CAMERA_DISABLED) {
		return "ACAMERA_ERROR_CAMERA_DISABLED";
	} else if (status == ACAMERA_ERROR_PERMISSION_DENIED) {
		return "ACAMERA_ERROR_PERMISSION_DENIED";
	} else if (status == -10014) { // Can't use ACAMERA_ERROR_UNSUPPORTED_OPERATION, not present in NDK 17c...
		return "ACAMERA_ERROR_UNSUPPORTED_OPERATION";
	}

	return "UNKNOWN";
}

/* ************************************************************************* */

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

static void android_camera2_capture_on_image_available(void *context, AImageReader *reader) {
	AndroidCamera2Context *d = static_cast<AndroidCamera2Context *>(context);
	if (d == nullptr || d->filter == nullptr) {
		ms_error("[Camera2 Capture] Image available callback called after filtre uninit!");
		return;
	}

	ms_filter_lock(d->filter);
	if (!d->filter || !d->filter->ticker || !d->configured) {
		AImage *image = nullptr;
		
		media_status_t status = AMEDIA_ERROR_UNKNOWN;
		ms_mutex_lock(&d->imageReaderMutex);
		if (d->imageReader != nullptr) {
			status = AImageReader_acquireLatestImage(d->imageReader, &image);
		} else {
			ms_error("[Camera2 Capture] Image reader is null in on_image_available callback!");
		}
		ms_mutex_unlock(&d->imageReaderMutex);

		if (status == AMEDIA_OK && image != nullptr) AImage_delete(image);
		ms_filter_unlock(d->filter);
		return;
	}
	ms_filter_unlock(d->filter);

	int32_t format;

	ms_mutex_lock(&d->imageReaderMutex);
	if (d->imageReader == nullptr) {
		ms_error("[Camera2 Capture] Image reader is null in on_image_available callback!");
		if (reader != nullptr) {
			ms_warning("[Camera2 Capture] The reader given by the callback isn't null...");
		} else {
			ms_warning("[Camera2 Capture] The reader given by the callback is also null...");
		}
		ms_mutex_unlock(&d->imageReaderMutex);
		return;
	}

  	media_status_t status = AImageReader_getFormat(d->imageReader, &format);
	if (format == d->captureFormat) {
		AImage *image = nullptr;
		// Using AImageReader_acquireLatestImage will lead to the following log if maxImages given to ImageReader is 1
		// W/NdkImageReader: Unable to acquire a lockedBuffer, very likely client tries to lock more than maxImages buffers
		status = AImageReader_acquireNextImage(d->imageReader, &image);
		if (status == AMEDIA_OK && image != nullptr) {
			MSTicker* ticker = d->filter->ticker;
			uint64_t tickerTime = 0;
			// We need to check if ticker is still available here, as no mutex lock can prevent it to be detached (and attached back later) in case of resizing
			if (ticker != nullptr) tickerTime = ticker->time;
			if (tickerTime != 0 && ms_video_capture_new_frame(&d->fpsControl, tickerTime)) {
				mblk_t *m = android_camera2_capture_image_to_mblkt(d, image);
				if (m) {
					ms_mutex_lock(&d->mutex);
					if (d->frame) freemsg(d->frame);
					d->frame = m;
					ms_mutex_unlock(&d->mutex);
				}
			}
			
			AImage_delete(image);
		} else {
			ms_error("[Camera2 Capture] Couldn't acquire image, image ptr is %p, error is %i", image, status);
		}
	} else {
		ms_error("[Camera2 Capture] Aquired image is in wrong format %d, expected %d", format, d->captureFormat);
	}
	ms_mutex_unlock(&d->imageReaderMutex);
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
	d->deviceStateCallbacks.context = d;
	d->deviceStateCallbacks.onDisconnected = android_camera2_capture_device_on_disconnected;
	d->deviceStateCallbacks.onError = android_camera2_capture_device_on_error;

	if (!d->device) {
		ms_error("[Camera2 Capture] Can't open camera, no device selected");
		return;
	}

	ACameraDevice *cameraDevice;
	camera_status_t camera_status = ACameraManager_openCamera(d->cameraManager, d->device->camId, &d->deviceStateCallbacks, &cameraDevice);
	if (camera_status != ACAMERA_OK) {
		ms_error("[Camera2 Capture] Failed to open camera %s, error is %s", d->device->camId, android_camera2_status_to_string(camera_status));
		return;
    }
	
	ms_message("[Camera2 Capture] Camera %s opened", d->device->camId);
	d->cameraDevice = cameraDevice;
}

static void android_camera2_capture_close_camera(AndroidCamera2Context *d) {
	ms_message("[Camera2 Capture] Closing camera");

    if (d->cameraDevice) {
        camera_status_t camera_status = ACameraDevice_close(d->cameraDevice);
        if (camera_status != ACAMERA_OK) {
            ms_error("[Camera2 Capture] Failed to close camera %s, error is %s", d->device->camId, android_camera2_status_to_string(camera_status));
        } else {
			ms_message("[Camera2 Capture] Camera closed %s", d->device->camId);
		}
        d->cameraDevice = nullptr;
    }
}

static void android_camera2_check_configuration_ok(AndroidCamera2Context *d) {
	if (d->nativeWindowId == 0) {
		ms_error("[Camera2 Capture] TextureView wasn't set (was core.setNativePreviewWindowId() called?)");
		return;
	}
	if (d->surface == nullptr) {
		ms_error("[Camera2 Capture] Failed to get a valid object to display camera preview from native window id [%p]", d->nativeWindowId);
		return;
	}
	if (d->captureSize.width == 0 || d->captureSize.height == 0) {
		ms_error("[Camera2 Capture] Capture size hasn't been configured yet");
		return;
	}
	d->configured = true;
}

static void android_camera2_capture_start(AndroidCamera2Context *d) {
	ms_message("[Camera2 Capture] Starting capture");
	camera_status_t camera_status = ACAMERA_OK;

	if (d->capturing) {
		ms_warning("[Camera2 Capture] Capture was already started, ignoring...");
		return;
	}
	if (!d->configured) {
		ms_warning("[Camera2 Capture] Filter configuration not finished, ignoring...");
		return;
	}
	
	if (!d->nativeWindow && d->surface) {
		android_camera2_capture_create_preview(d);
	}
	if (!d->cameraDevice) {
		android_camera2_capture_open_camera(d);
	}

	if (!d->cameraDevice) {
		ms_error("[Camera2 Capture] Couldn't open camera %s, aborting capture",  d->device->camId);
		return;
	}

	camera_status = ACaptureSessionOutputContainer_create(&d->captureSessionOutputContainer);
	if (camera_status != ACAMERA_OK) {
		ms_error("[Camera2 Capture] Failed to create capture session output container, error is %s", android_camera2_status_to_string(camera_status));
	}
	d->captureSessionStateCallbacks.onReady = android_camera2_capture_session_on_ready;
	d->captureSessionStateCallbacks.onActive = android_camera2_capture_session_on_active;
	d->captureSessionStateCallbacks.onClosed = android_camera2_capture_session_on_closed;

	camera_status = ACameraDevice_createCaptureRequest(d->cameraDevice, TEMPLATE_PREVIEW, &d->capturePreviewRequest);
	if (camera_status != ACAMERA_OK) {
		ms_error("[Camera2 Capture] Failed to create capture preview request, error is %s", android_camera2_status_to_string(camera_status));
	}

	camera_status = ACameraOutputTarget_create(d->nativeWindow, &d->cameraPreviewOutputTarget);
	if (camera_status != ACAMERA_OK) {
		ms_error("[Camera2 Capture] Failed to create output target, error is %s", android_camera2_status_to_string(camera_status));
	}

	camera_status = ACaptureRequest_addTarget(d->capturePreviewRequest, d->cameraPreviewOutputTarget);
	if (camera_status != ACAMERA_OK) {
		ms_error("[Camera2 Capture] Failed to add output target to capture request, error is %s", android_camera2_status_to_string(camera_status));
	}

	camera_status = ACaptureSessionOutput_create(d->nativeWindow, &d->sessionPreviewOutput);
	if (camera_status != ACAMERA_OK) {
		ms_error("[Camera2 Capture] Failed to create capture session output, error is %s", android_camera2_status_to_string(camera_status));
	}

	camera_status = ACaptureSessionOutputContainer_add(d->captureSessionOutputContainer, d->sessionPreviewOutput);
	if (camera_status != ACAMERA_OK) {
		ms_error("[Camera2 Capture] Failed to add capture session output to container, error is %s", android_camera2_status_to_string(camera_status));
	}

	media_status_t status = AImageReader_new(d->captureSize.width, d->captureSize.height, d->captureFormat, 1, &d->imageReader);
	if (status != AMEDIA_OK) {
		ms_error("[Camera2 Capture] Failed to create image reader, error is %i", status);
		d->configured = false;
		return;
	}
	ms_message("[Camera2 Capture] Created image reader for size %ix%i and format %d", d->captureSize.width, d->captureSize.height, d->captureFormat);

	d->imageReaderListener = new AImageReader_ImageListener();
	d->imageReaderListener->context = d;
	d->imageReaderListener->onImageAvailable = android_camera2_capture_on_image_available;
  	status = AImageReader_setImageListener(d->imageReader, d->imageReaderListener);
	if (status != AMEDIA_OK) {
		ms_error("[Camera2 Capture] Failed to set image listener, error is %i", status);
		d->configured = false;
		return;
	}

	status = AImageReader_getWindow(d->imageReader, &d->captureWindow);
	if (status != AMEDIA_OK) {
		ms_error("[Camera2 Capture] Capture window couldn't be acquired, error is %i", status);
		d->configured = false;
		return;
	}
	ANativeWindow_acquire(d->captureWindow);

	camera_status = ACameraOutputTarget_create(d->captureWindow, &d->cameraCaptureOutputTarget);
	if (camera_status != ACAMERA_OK) {
		ms_error("[Camera2 Capture] Couldn't create output target, error is %s", android_camera2_status_to_string(camera_status));
		d->configured = false;
		return;
	}

	camera_status = ACaptureRequest_addTarget(d->capturePreviewRequest, d->cameraCaptureOutputTarget);
	if (camera_status != ACAMERA_OK) {
		ms_error("[Camera2 Capture] Couldn't add output target to capture request, error is %s", android_camera2_status_to_string(camera_status));
		d->configured = false;
		return;
	}

	camera_status = ACaptureSessionOutput_create(d->captureWindow, &d->sessionCaptureOutput);
	if (camera_status != ACAMERA_OK) {
		ms_error("[Camera2 Capture] Couldn't create capture session output, error is %s", android_camera2_status_to_string(camera_status));
		d->configured = false;
		return;
	}

	camera_status = ACaptureSessionOutputContainer_add(d->captureSessionOutputContainer, d->sessionCaptureOutput);
	if (camera_status != ACAMERA_OK) {
		ms_error("[Camera2 Capture] Couldn't add capture session output to container, error is %s", android_camera2_status_to_string(camera_status));
		d->configured = false;
		return;
	}

	camera_status = ACameraDevice_createCaptureSession(d->cameraDevice, d->captureSessionOutputContainer, &d->captureSessionStateCallbacks, &d->captureSession);
	if (camera_status != ACAMERA_OK) {
		ms_error("[Camera2 Capture] Couldn't create capture session, error is %s", android_camera2_status_to_string(camera_status));
		d->configured = false;
		return;
	}

	camera_status = ACameraCaptureSession_setRepeatingRequest(d->captureSession, NULL, 1, &d->capturePreviewRequest, NULL);
	if (camera_status != ACAMERA_OK) {
		ms_error("[Camera2 Capture] Couldn't set capture session repeating request, error is %s", android_camera2_status_to_string(camera_status));
		d->configured = false;
		return;
	}

	d->capturing = true;
	ms_message("[Camera2 Capture] Capture started");
}

static void android_camera2_capture_stop(AndroidCamera2Context *d) {
	ms_message("[Camera2 Capture] Stopping capture");

	ms_filter_lock(d->filter);
	d->configured = false;
	ms_filter_unlock(d->filter);

	if (!d->capturing) {
		ms_warning("[Camera2 Capture] Capture was already stopped, ignoring...");
		ms_filter_unlock(d->filter);
		return;
	}
	d->capturing = false;

	ms_mutex_lock(&d->imageReaderMutex);
	if (d->imageReaderListener) {
		delete d->imageReaderListener;
		d->imageReaderListener = nullptr;
	}

	AImageReader_ImageListener nullListener = {nullptr, nullptr};
  	media_status_t status = AImageReader_setImageListener(d->imageReader, &nullListener);
	if (status != AMEDIA_OK) {
		ms_error("[Camera2 Capture] Failed to set null image listener, error is %i", status);
	}
	ms_mutex_unlock(&d->imageReaderMutex);

	// Seems to provoke an ANR on some devices if the camera close is done after the capture session
	android_camera2_capture_close_camera(d);

	if (d->captureSession) {
		camera_status_t camera_status = ACameraCaptureSession_abortCaptures(d->captureSession);
		if (camera_status != ACAMERA_OK) {
			ms_error("[Camera2 Capture] Couldn't abort captures on session, error is %s", android_camera2_status_to_string(camera_status));
		}

		camera_status = ACameraCaptureSession_stopRepeating(d->captureSession);
		if (camera_status != ACAMERA_OK) {
			ms_error("[Camera2 Capture] Couldn't stop repeating session, error is %s", android_camera2_status_to_string(camera_status));
		}

		ACameraCaptureSession_close(d->captureSession);
		d->captureSession = nullptr;
		ms_message("[Camera2 Capture] Capture session closed");
	}

	if (d->capturePreviewRequest) {
		ACaptureRequest_removeTarget(d->capturePreviewRequest, d->cameraCaptureOutputTarget);
		ACaptureRequest_free(d->capturePreviewRequest);
		d->capturePreviewRequest = nullptr;
		ms_message("[Camera2 Capture] Preview request freed");
    }

	if (d->cameraCaptureOutputTarget) {
		ACameraOutputTarget_free(d->cameraCaptureOutputTarget);
		d->cameraCaptureOutputTarget = nullptr;
		ms_message("[Camera2 Capture] Camera output target freed");
    }

	if (d->cameraPreviewOutputTarget) {
		ACameraOutputTarget_free(d->cameraPreviewOutputTarget);
		d->cameraPreviewOutputTarget = nullptr;
		ms_message("[Camera2 Capture] Preview preview output target freed");
    }

	if (d->captureSessionOutputContainer) {
		ms_message("[Camera2 Capture] Freeing capture session output container");

		if (d->sessionCaptureOutput) {
			ACaptureSessionOutputContainer_remove(d->captureSessionOutputContainer, d->sessionCaptureOutput);
			ACaptureSessionOutput_free(d->sessionCaptureOutput);
			d->sessionCaptureOutput = nullptr;
			ms_message("[Camera2 Capture] Capture session output freed");
		}

		if (d->sessionPreviewOutput) {
			ACaptureSessionOutputContainer_remove(d->captureSessionOutputContainer, d->sessionPreviewOutput);
			ACaptureSessionOutput_free(d->sessionPreviewOutput);
			d->sessionPreviewOutput = nullptr;
			ms_message("[Camera2 Capture] Capture session preview output freed");
		}

		if (d->captureWindow) {
			ANativeWindow_release(d->captureWindow);
			d->captureWindow = nullptr;
			ms_message("[Camera2 Capture] Capture window released");
		}

		ACaptureSessionOutputContainer_free(d->captureSessionOutputContainer);
		d->captureSessionOutputContainer = nullptr;
		ms_message("[Camera2 Capture] Capture session output container freed");
	}
	ms_mutex_lock(&d->imageReaderMutex);
	if (d->imageReader) {
		AImageReader_delete(d->imageReader);
		d->imageReader = nullptr;
		ms_message("[Camera2 Capture] Image reader deleted");
	}
	ms_mutex_unlock(&d->imageReaderMutex);

	android_camera2_capture_destroy_preview(d);

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
	ms_average_fps_init(&d->averageFps, d->fps_context);
	ms_filter_unlock(f);

	ms_mutex_lock(&d->mutex);
	if (d->frame) {
		freemsg(d->frame);
		d->frame = NULL;
	}
	ms_mutex_unlock(&d->mutex);
}

static void android_camera2_capture_process(MSFilter *f) {
	AndroidCamera2Context *d = (AndroidCamera2Context *)f->data;
	ms_filter_lock(f);

	if (!d->capturing && d->configured) {
		android_camera2_capture_start(d);
	}

	ms_mutex_lock(&d->mutex);
	if (d->frame) {
		ms_average_fps_update(&d->averageFps, f->ticker->time);
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
}

static void android_camera2_capture_uninit(MSFilter *f) {
	ms_message("[Camera2 Capture] Filter uninit");
	AndroidCamera2Context *d = (AndroidCamera2Context *)f->data;

	if (d->capturing) {
		android_camera2_capture_stop(d);
	}

	ms_mutex_lock(&d->mutex);
	if (d->frame) {
		freemsg(d->frame);
		d->frame = NULL;
	}
	ms_mutex_unlock(&d->mutex);

	delete d;
}

/* ************************************************************************* */

static int android_camera2_capture_set_fps(MSFilter *f, void *arg) {
	AndroidCamera2Context *d = (AndroidCamera2Context *)f->data;
	d->fps = *((float*)arg);
	
	ms_filter_lock(f);
	snprintf(d->fps_context, sizeof(d->fps_context), "Captured mean fps=%%f, expected=%f", d->fps);
	ms_video_init_framerate_controller(&d->fpsControl, d->fps);
	ms_average_fps_init(&d->averageFps, d->fps_context);
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
	camera_status_t camera_status = ACameraManager_getCameraCharacteristics(d->cameraManager, d->device->camId, &cameraMetadata);
	if (camera_status != ACAMERA_OK) {
		ms_error("[Camera2 Capture] Failed to get camera characteristics, error is %s", android_camera2_status_to_string(camera_status));
		return;
	}

	ACameraMetadata_const_entry supportedFpsRanges;
	ACameraMetadata_getConstEntry(cameraMetadata, ACAMERA_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES, &supportedFpsRanges);
	for (int i = 0; i < supportedFpsRanges.count; i += 2) {
		int32_t min = supportedFpsRanges.data.i32[i];
		int32_t max = supportedFpsRanges.data.i32[i + 1];
		ms_message("[Camera2 Capture] Supported FPS range: [%d-%d]", min, max);
	}
	
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
		} else {
			int32_t width = scaler.data.i32[i + 1];
			int32_t height = scaler.data.i32[i + 2];
			ms_debug("[Camera2 Capture] Available size width %d, height %d for format %d", width, height, format);
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

	ACameraMetadata_free(cameraMetadata);
}

static void android_camera2_capture_create_surface_from_surface_texture(AndroidCamera2Context *d) {
	JNIEnv *env = ms_get_jni_env();
	jobject surface = nullptr;
	jobject surfaceTexture = d->nativeWindowId;

	jclass surfaceTextureClass = env->FindClass("android/graphics/SurfaceTexture");
	if (!surfaceTextureClass) {
		ms_error("[Camera2 Capture] Could not find android.graphics.SurfaceTexture class");
		return;
	}

	jclass surfaceClass = env->FindClass("android/view/Surface");
	if (!surfaceClass) {
		ms_error("[Camera2 Capture] Could not find android.view.Surface class");
		return;
	}

	jclass textureViewClass = env->FindClass("android/view/TextureView");
	if (!textureViewClass) {
		ms_error("[Camera2 Capture] Could not find android.view.TextureView class");
		return;
	}

	if (env->IsInstanceOf(surfaceTexture, surfaceClass)) {
		ms_message("[Camera2 Capture] NativePreviewWindowId %p is a Surface, using it directly", surfaceTexture);
		d->surface = (jobject)env->NewGlobalRef(surfaceTexture);
		return;
	}

	if (env->IsInstanceOf(surfaceTexture, textureViewClass)) {
		ms_message("[Camera2 Capture] NativePreviewWindowId %p is a TextureView", surfaceTexture);

		jmethodID getSurfaceTexture = env->GetMethodID(textureViewClass, "getSurfaceTexture", "()Landroid/graphics/SurfaceTexture;");
		surfaceTexture = env->CallObjectMethod(d->nativeWindowId, getSurfaceTexture);
		if (surfaceTexture == nullptr) {
			ms_error("[Camera2 Capture] TextureView isn't available !");
			return;
		}
		ms_message("[Camera2 Capture] Got SurfaceTexture %p from TextureView %p", surfaceTexture, d->nativeWindowId);
	}

	if (surfaceTexture != nullptr) {
		if (d->captureSize.width != 0 && d->captureSize.height != 0) {
			jmethodID setDefaultBufferSize = env->GetMethodID(surfaceTextureClass, "setDefaultBufferSize", "(II)V");
			env->CallVoidMethod(surfaceTexture, setDefaultBufferSize, d->captureSize.width, d->captureSize.height);
			ms_message("[Camera2 Capture] Set default buffer size for SurfaceTexture %p to %ix%i", surfaceTexture, d->captureSize.width, d->captureSize.height);
		} else {
			ms_warning("[Camera2 Capture] SurfaceTexture buffer size not available yet, aborting for now, will come back later");
			d->surface = nullptr;
			return;
		}
	}

	if (surfaceTexture == nullptr) {
		ms_error("[Camera2 Capture] SurfaceTexture is null, can't create a Surface!");
		return;
	}

	ms_message("[Camera2 Capture] Creating Surface from SurfaceTexture %p", surfaceTexture);
	jmethodID ctor = env->GetMethodID(surfaceClass, "<init>", "(Landroid/graphics/SurfaceTexture;)V");
	surface = env->NewObject(surfaceClass, ctor, surfaceTexture);
	if (!surface) {
		ms_error("[Camera2 Capture] Could not instanciate android.view.Surface object");
		return;
	}
	
	d->surface = (jobject)env->NewGlobalRef(surface);
	ms_message("[Camera2 Capture] Got Surface %p from SurfaceTexture %p",  d->surface, surfaceTexture);
}

static int android_camera2_capture_set_surface_texture(MSFilter *f, void *arg) {
	AndroidCamera2Context *d = (AndroidCamera2Context *)f->data;
	unsigned long id = *(unsigned long *)arg;
	jobject nativeWindowId = (jobject)id;
	JNIEnv *env = ms_get_jni_env();

	ms_filter_lock(f);
	jobject currentWindowId = d->nativeWindowId;
	ms_filter_unlock(f);

	ms_message("[Camera2 Capture] New native window ptr is %p, current one is %p", nativeWindowId, currentWindowId);
	if (id == 0) {
		if (currentWindowId) {
			android_camera2_capture_stop(d);
			env->DeleteGlobalRef(currentWindowId);
			d->nativeWindowId = nullptr;
		}
	} else if (!env->IsSameObject(currentWindowId, nativeWindowId)) {
		if (currentWindowId) {
			android_camera2_capture_stop(d);
			env->DeleteGlobalRef(currentWindowId);
		}
		
		d->nativeWindowId = env->NewGlobalRef(nativeWindowId);
		android_camera2_capture_create_surface_from_surface_texture(d);
		
		ms_filter_lock(f);
		android_camera2_check_configuration_ok(d);
		ms_filter_unlock(f);

		if (d->previewSize.width != 0 && d->previewSize.height != 0) {
			ms_filter_notify(f, MS_CAMERA_PREVIEW_SIZE_CHANGED, &d->previewSize);
		}
	} else {
		ms_message("[Camera2 Capture] New native window is the same as the current one, skipping...");
	}

	return 0; 
}

static void android_camera2_capture_update_preview_size(AndroidCamera2Context *d) {
	int orientation = android_camera2_capture_get_orientation(d);
	if (orientation % 180 == 0) {
		d->previewSize.width = d->captureSize.width;
		d->previewSize.height = d->captureSize.height;
	} else {
		d->previewSize.width = d->captureSize.height;
		d->previewSize.height = d->captureSize.width;
	}
}

static int android_camera2_capture_set_vsize(MSFilter *f, void* arg) {
	AndroidCamera2Context *d = (AndroidCamera2Context *)f->data;
	MSVideoSize requestedSize = *(MSVideoSize*)arg;
	ms_message("[Camera2 Capture] New requested size is %ix%i", requestedSize.width, requestedSize.height);

	if (d->captureSize.width == requestedSize.width && d->captureSize.height == requestedSize.height) {
		ms_warning("[Camera2 Capture] Newly requested size is the same as current one, skipping");
		return -1;
	}
	
	MSVideoSize oldSize;
	oldSize.width = d->captureSize.width;
	oldSize.height = d->captureSize.height;
	d->captureSize = requestedSize;

	android_camera2_capture_stop(d);
	android_camera2_capture_choose_best_configurations(d);
	android_camera2_capture_update_preview_size(d);

	if (d->previewSize.width != 0 && d->previewSize.height != 0) {
		ms_filter_notify(f, MS_CAMERA_PREVIEW_SIZE_CHANGED, &d->previewSize);
	}

	ms_message("[Camera2 Capture] Previous preview size was %i/%i, new size is %i/%i", 
		oldSize.width, oldSize.height, d->previewSize.width, d->previewSize.height);

	if (d->surface == nullptr && d->nativeWindowId != 0) {
		ms_warning("[Camera2 Capture] Video size has changed after video window id has been set, have to recreate Surface object...");
		android_camera2_capture_create_surface_from_surface_texture(d);
	}

	ms_filter_lock(f);
	android_camera2_check_configuration_ok(d);
	ms_filter_unlock(f);

	return 0;
}

static int android_camera2_capture_get_vsize(MSFilter *f, void* arg) {
	AndroidCamera2Context *d = (AndroidCamera2Context *)f->data;

	ms_filter_lock(f);
	android_camera2_capture_update_preview_size(d);
	ms_filter_unlock(f);

	*(MSVideoSize*)arg = d->previewSize;
	ms_message("[Camera2 Capture] Getting preview size: %ix%i", d->previewSize.width, d->previewSize.height);

	return 0;
}

static int android_camera2_capture_set_device_rotation(MSFilter* f, void* arg) {
	AndroidCamera2Context *d = (AndroidCamera2Context *)f->data;

	ms_filter_lock(f);

	int rotation = *((int*)arg);
	if (rotation != d->rotation) {
		d->rotation = rotation;
		android_camera2_capture_update_preview_size(d);

		ms_message("[Camera2 Capture] Device rotation is %i", rotation);
		if (d->previewSize.width != 0 && d->previewSize.height != 0) {
			ms_filter_notify(f, MS_CAMERA_PREVIEW_SIZE_CHANGED, &d->previewSize);
		}
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

extern void android_video_capture_detect_cameras_legacy(MSWebCamManager *obj);

void android_camera2_capture_detect(MSWebCamManager *obj) {
	ms_message("[Camera2 Capture] Detecting cameras");

	ACameraIdList *cameraIdList = nullptr;
	ACameraMetadata *cameraMetadata = nullptr;

	camera_status_t camera_status = ACAMERA_OK;
	ACameraManager *cameraManager = ACameraManager_create();

	camera_status = ACameraManager_getCameraIdList(cameraManager, &cameraIdList);
	if (camera_status != ACAMERA_OK) {
		ms_error("[Camera2 Capture] Failed to get camera(s) list : %d", camera_status);
		android_video_capture_detect_cameras_legacy(obj);
		return;
	}

	if (cameraIdList->numCameras < 1) {
		ms_warning("[Camera2 Capture] No camera detected !");
		android_video_capture_detect_cameras_legacy(obj);
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

			ACameraMetadata_const_entry hardwareLevel;
			ACameraMetadata_getConstEntry(cameraMetadata, ACAMERA_INFO_SUPPORTED_HARDWARE_LEVEL, &hardwareLevel);
			std::string supportedHardwareLevel = "unknown";
			switch (hardwareLevel.data.u8[0]) {
				case ACAMERA_INFO_SUPPORTED_HARDWARE_LEVEL_LIMITED:
					supportedHardwareLevel = "limited";
					break;
				case ACAMERA_INFO_SUPPORTED_HARDWARE_LEVEL_FULL:
					supportedHardwareLevel = "full";
					break;
				case ACAMERA_INFO_SUPPORTED_HARDWARE_LEVEL_LEGACY:
					supportedHardwareLevel = "legacy";
					break;
				case ACAMERA_INFO_SUPPORTED_HARDWARE_LEVEL_3:
					supportedHardwareLevel = "3";
					break;
			}

  			ACameraMetadata_const_entry face;
			ACameraMetadata_getConstEntry(cameraMetadata, ACAMERA_LENS_FACING, &face);
			bool back_facing = face.data.u8[0] == ACAMERA_LENS_FACING_BACK;
			std::string facing = std::string(!back_facing ? "front" : "back");
			ms_message("[Camera2 Capture] Camera %s is facing %s with angle %d, hardware level is %s", camId, facing.c_str(), angle, supportedHardwareLevel.c_str());
			device->back_facing = back_facing;

			MSWebCam *cam = ms_web_cam_new(&ms_android_camera2_capture_webcam_desc);
			std::string idstring = std::string(!back_facing ? "Front" : "Back") + std::string("FacingCamera");
			cam->id = ms_strdup(idstring.c_str());
			cam->name = ms_strdup(idstring.c_str());
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

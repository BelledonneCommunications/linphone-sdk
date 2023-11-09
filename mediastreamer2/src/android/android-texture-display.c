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

#include "layouts.h"
#include "mediastreamer2/msasync.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msjava.h"
#include "mediastreamer2/msvideo.h"
#include "opengles_display.h"

#include <android/hardware_buffer.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/rect.h>
#include <android/window.h>

#include <sys/resource.h>
#include <sys/time.h>

/* All AndroidTextureDisplay filters share a same worker thread to execute OpenGl primitives.
 * This structure is not thread-safe: it works as long as filters are not created or destroyed in parallel.
 */
typedef struct AndroidTextureSharedContext {
	MSWorkerThread *process_thread;
	int use_count;
} AndroidTextureSharedContext;

typedef struct AndroidTextureDisplay {
	jobject surface;
	ANativeWindow *window;
	struct opengles_display *ogl;
	EGLSurface gl_surface;
	EGLDisplay gl_display;
	EGLContext gl_context;
	MSTask *refresh_task;
	jobject nativeWindowId;
	EGLint width, height;
	MSWorkerThread *process_thread;
	MSVideoDisplayMode mode;
} AndroidTextureDisplay;

static AndroidTextureSharedContext shared_context = {NULL, 0};

static MSWorkerThread *android_texture_display_get_worker(void) {
	if (shared_context.use_count == 0) {
		shared_context.process_thread = ms_worker_thread_new("ms2 OGL");
	}
	shared_context.use_count++;
	return shared_context.process_thread;
}

static void android_texture_display_release_worker(MSWorkerThread *worker) {
	if (worker != shared_context.process_thread) {
		ms_error("[TextureView Display]: worker thread mismatch.");
		return;
	}
	shared_context.use_count--;
	if (shared_context.use_count == 0) {
		ms_worker_thread_destroy(shared_context.process_thread, FALSE);
	}
}

static void android_texture_display_destroy_opengl(MSFilter *f) {
	AndroidTextureDisplay *ad = (AndroidTextureDisplay *)f->data;
	ms_filter_lock(f);
	ms_message("[TextureView Display][Filter=%p] Destroying context for windowId %p", f, ad->nativeWindowId);

	if (ad->ogl) {
		if (ad->gl_display && ad->gl_surface && ad->gl_surface && ad->gl_context) {
			if (eglMakeCurrent(ad->gl_display, ad->gl_surface, ad->gl_surface, ad->gl_context) == EGL_FALSE) {
				ms_error("[TextureView Display][Filter=%p] Unable to eglMakeCurrent for windowId %p", f,
				         ad->nativeWindowId);
			}
		}

		ogl_display_uninit(ad->ogl, TRUE);
		ogl_display_free(ad->ogl);
		ad->ogl = NULL;
		ms_message("[TextureView Display][Filter=%p] OGL display destroyed", f);
	}

	EGLBoolean result;
	if (ad->gl_display) {
		if (eglMakeCurrent(ad->gl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_FALSE) {
			ms_error("[TextureView Display][Filter=%p] Unable to eglMakeCurrent in destructor", fflush);
		}

		if (ad->gl_context) {
			result = eglDestroyContext(ad->gl_display, ad->gl_context);
			if (result != EGL_TRUE) {
				ms_error("[TextureView Display][Filter=%p] eglDestroyContext failure: %u", f, result);
			}
			ad->gl_context = NULL;
		}

		if (ad->gl_surface) {
			result = eglDestroySurface(ad->gl_display, ad->gl_surface);
			if (result != EGL_TRUE) {
				ms_error("[TextureView Display][Filter=%p] eglDestroySurface failure: %u", f, result);
			}
			ad->gl_surface = NULL;
		}

		result = eglTerminate(ad->gl_display);
		if (result != EGL_TRUE) {
			ms_error("[TextureView Display][Filter=%p] eglTerminate failure: %u", f, result);
		}

		ad->gl_display = NULL;
		ms_message("[TextureView Display][Filter=%p] EGL display destroyed for windowId %p", f, ad->nativeWindowId);
	}

	if (ad->window) {
		ANativeWindow_release(ad->window);
		ad->window = NULL;
		ms_message("[TextureView Display][Filter=%p] Window released for windowId %p", f, ad->nativeWindowId);
	}

	if (ad->surface) {
		JNIEnv *env = ms_get_jni_env();
		jclass surfaceClass = (*env)->FindClass(env, "android/view/Surface");
		jmethodID release = (*env)->GetMethodID(env, surfaceClass, "release", "()V");
		(*env)->CallVoidMethod(env, ad->surface, release);
		ms_message("[TextureView Display][Filter=%p] Surface released for windowId %p", f, ad->nativeWindowId);

		(*env)->DeleteGlobalRef(env, ad->surface);
		ad->surface = NULL;
		ms_message("[TextureView Display][Filter=%p] Surface destroyed for windowId %p", f, ad->nativeWindowId);
	}

	ms_message("[TextureView Display][Filter=%p] Context destroyed for windowId %p", f, ad->nativeWindowId);
	ms_filter_unlock(f);
}

static void android_texture_display_release_windowId(MSFilter *f) {
	AndroidTextureDisplay *ad = (AndroidTextureDisplay *)f->data;
	ms_filter_lock(f);
	JNIEnv *env = ms_get_jni_env();
	ms_message("[TextureView Display][Filter=%p] Releasing global ref on windowId %p", f, ad->nativeWindowId);
	(*env)->DeleteGlobalRef(env, ad->nativeWindowId);
	ad->nativeWindowId = NULL;

	ms_free(ad);
	ms_filter_unlock(f);
}

static void android_texture_display_create_surface_from_surface_texture(AndroidTextureDisplay *d) {
	JNIEnv *env = ms_get_jni_env();
	jobject surface = NULL;
	jobject windowId = d->nativeWindowId;

	jclass surfaceTextureClass = (*env)->FindClass(env, "android/graphics/SurfaceTexture");
	if (!surfaceTextureClass) {
		ms_error("[TextureView Display] Could not find android.graphics.SurfaceTexture class");
		return;
	}

	jclass surfaceClass = (*env)->FindClass(env, "android/view/Surface");
	if (!surfaceClass) {
		ms_error("[TextureView Display] Could not find android.view.Surface class");
		return;
	}

	jclass textureViewClass = (*env)->FindClass(env, "android/view/TextureView");
	if (!textureViewClass) {
		ms_error("[TextureView Display] Could not find android.view.TextureView class");
		return;
	}

	if ((*env)->IsInstanceOf(env, windowId, surfaceClass)) {
		ms_message("[TextureView Display] NativePreviewWindowId %p is a Surface, using it directly", windowId);
		d->surface = (jobject)(*env)->NewGlobalRef(env, windowId);
		return;
	}

	if ((*env)->IsInstanceOf(env, windowId, textureViewClass)) {
		ms_message(
		    "[TextureView Display] NativePreviewWindowId %p is a TextureView, let's get it's SurfaceTexture first",
		    windowId);
		jmethodID getSurfaceTexture =
		    (*env)->GetMethodID(env, textureViewClass, "getSurfaceTexture", "()Landroid/graphics/SurfaceTexture;");
		windowId = (*env)->CallObjectMethod(env, d->nativeWindowId, getSurfaceTexture);
		if (windowId == NULL) {
			ms_error("[TextureView Display] TextureView %p isn't available !", windowId);
			return;
		}
	}

	if (windowId == NULL) {
		ms_error("[TextureView Display] SurfaceTexture is null, can't create a Surface from windowId %p !", windowId);
		return;
	}

	ms_message("[TextureView Display] Creating Surface from SurfaceTexture %p", windowId);
	jmethodID ctor = (*env)->GetMethodID(env, surfaceClass, "<init>", "(Landroid/graphics/SurfaceTexture;)V");
	surface = (*env)->NewObject(env, surfaceClass, ctor, windowId);
	if (!surface) {
		ms_error("[TextureView Display] Could not instanciate android.view.Surface object");
		return;
	}
	d->surface = (jobject)(*env)->NewGlobalRef(env, surface);
	ms_message("[TextureView Display] Surface created %p for SurfaceTexture %p", d->surface, windowId);
}

static void android_texture_display_init_opengl(MSFilter *f) {
	AndroidTextureDisplay *ad = (AndroidTextureDisplay *)f->data;
	ms_filter_lock(f);
	JNIEnv *jenv = ms_get_jni_env();
	ms_message("[TextureView Display][Filter=%p] Initializing context for windowId %p", f, ad->nativeWindowId);

	if (ad->nativeWindowId) {
		android_texture_display_create_surface_from_surface_texture(ad);
		if (ad->surface == NULL) {
			ms_error("[TextureView Display][Filter=%p] Can't init display, no surface created from texture, releasing "
			         "global ref on windowId %p",
			         f, ad->nativeWindowId);
			(*jenv)->DeleteGlobalRef(jenv, ad->nativeWindowId);
			ad->nativeWindowId = NULL;
			ms_filter_unlock(f);
			return;
		}
	} else {
		ms_error("[TextureView Display][Filter=%p] Can't init display for windowId %p, no surface texture set", f,
		         ad->nativeWindowId);
		ms_filter_unlock(f);
		return;
	}

	EGLint attribs[] = {EGL_RED_SIZE,       5,       EGL_GREEN_SIZE, 6, EGL_BLUE_SIZE, 5, EGL_RENDERABLE_TYPE,
	                    EGL_OPENGL_ES2_BIT, EGL_NONE};

	EGLint w, h, format;
	EGLint numConfigs;
	EGLConfig config;
	EGLSurface surface;
	EGLContext context;

	EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	eglInitialize(display, 0, 0);

	eglChooseConfig(display, attribs, &config, 1, &numConfigs);

	eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);

	ms_message("[TextureView Display][Filter=%p] Chosen format for windowId %p is %i", f, ad->nativeWindowId, format);

	ad->window = ANativeWindow_fromSurface(jenv, ad->surface);
	ANativeWindow_setBuffersGeometry(ad->window, 0, 0, format);

	surface = eglCreateWindowSurface(display, config, ad->window, NULL);

	eglQuerySurface(display, surface, EGL_WIDTH, &w);
	eglQuerySurface(display, surface, EGL_HEIGHT, &h);
	ms_message("[TextureView Display][Filter=%p] Surface size for windowId %p is %ix%i", f, ad->nativeWindowId, w, h);

	if (w == 0 && h == 0) {
		ms_error("[TextureView Display][Filter=%p] Surface size for windowId %p is invalid, do not go further!", f,
		         ad->nativeWindowId);
		ad->gl_display = display;
		ad->gl_surface = surface;
		ms_filter_unlock(f);
		android_texture_display_destroy_opengl(f);
		return;
	}

	EGLint contextAttrs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
	context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttrs);

	if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
		ms_error("[TextureView Display][Filter=%p] Unable to eglMakeCurrent for windowId %p", f, ad->nativeWindowId);
		ms_filter_unlock(f);
		return;
	}

	ad->gl_display = display;
	ad->gl_surface = surface;
	ad->gl_context = context;
	ad->width = w;
	ad->height = h;

	if (ad->ogl) {
		ms_error("[TextureView Display][Filter=%p] ogl_display already created!", f);
	}
	ad->ogl = ogl_display_new();
	ogl_display_init(ad->ogl, NULL, w, h);

	ms_message("[TextureView Display][Filter=%p] Context initialized for windowId %p", f, ad->nativeWindowId);
	ms_filter_unlock(f);
}

static void report_abusive_time(uint64_t orig, const char *cause) {
	uint64_t now = bctbx_get_cur_time_ms();
	uint64_t diff = now - orig;
	if (diff > 10) {
		ms_warning("%s took %i ms !", cause, (int)diff);
	}
}

static void android_texture_display_swap_buffers(MSFilter *f) {
	AndroidTextureDisplay *ad = (AndroidTextureDisplay *)f->data;
	mblk_t *yuv_to_restore = NULL;
	uint64_t start_time;

	if (!ad->ogl || !ad->gl_display || !ad->gl_surface || !ad->gl_context || !ad->nativeWindowId) {
		ms_message("android_texture_display_swap_buffers(): nothing to do.");
		return;
	}

	EGLint w, h;
	start_time = bctbx_get_cur_time_ms();
	eglQuerySurface(ad->gl_display, ad->gl_surface, EGL_WIDTH, &w);
	eglQuerySurface(ad->gl_display, ad->gl_surface, EGL_HEIGHT, &h);

	if (ad->width != w || ad->height != h) {
		ms_warning("[TextureView Display][Filter=%p] Surface size for windowId %p has changed from %ix%i to %ix%i", f,
		           ad->nativeWindowId, ad->width, ad->height, w, h);

		/* Get back the last image displayed, in order to set it immediately after the new view is avaiable */
		yuv_to_restore = ogl_display_get_yuv_to_display(ad->ogl);
		if (yuv_to_restore) yuv_to_restore = dupmsg(yuv_to_restore);
		android_texture_display_destroy_opengl(f);
		android_texture_display_init_opengl(f);
	}

	if (eglMakeCurrent(ad->gl_display, ad->gl_surface, ad->gl_surface, ad->gl_context) == EGL_FALSE) {
		ms_error("[TextureView Display][Filter=%p] Unable to eglMakeCurrent for windowId %p", f, ad->nativeWindowId);
		return;
	}
	if (yuv_to_restore) {
		ogl_display_set_yuv_to_display(ad->ogl, yuv_to_restore);
		freemsg(yuv_to_restore);
	}
	ogl_display_render(ad->ogl, 0, ad->mode);

	EGLBoolean result = eglSwapBuffers(ad->gl_display, ad->gl_surface);
	if (result != EGL_TRUE) {
		ms_error("[TextureView Display][Filter=%p] eglSwapBuffers failure for windowId %p: %u", f, ad->nativeWindowId,
		         result);
	}
	ogl_display_notify_errors(ad->ogl, f);
	report_abusive_time(start_time, "android_texture_display_swap_buffers");
}

static void android_texture_display_init(MSFilter *f) {
	AndroidTextureDisplay *ad = (AndroidTextureDisplay *)ms_new0(AndroidTextureDisplay, 1);
	ad->surface = NULL;
	ad->nativeWindowId = NULL;
	ad->process_thread = android_texture_display_get_worker();
	ad->mode = MSVideoDisplayBlackBars;
	f->data = ad;
}

static void android_texture_display_set_priority(BCTBX_UNUSED(MSFilter *f)) {
	if (setpriority(PRIO_PROCESS, 0, -20) == -1) {
		ms_message("android_texture_display_set_priority(): setpriority() failed: %s, nevermind.", strerror(errno));
	}
}

static void android_texture_display_preprocess(MSFilter *f) {
	AndroidTextureDisplay *ad = (AndroidTextureDisplay *)f->data;
	ms_worker_thread_add_task(ad->process_thread, (MSTaskFunc)android_texture_display_set_priority, f);
	ad->refresh_task =
	    ms_worker_thread_add_repeated_task(ad->process_thread, (MSTaskFunc)android_texture_display_swap_buffers, f, 20);
}

static void android_texture_display_process(MSFilter *f) {
	AndroidTextureDisplay *ad = (AndroidTextureDisplay *)f->data;
	mblk_t *m;

	ms_filter_lock(f);
	if (ad->nativeWindowId != NULL && ad->ogl) {
		if ((m = ms_queue_peek_last(f->inputs[0])) != NULL) {
			ogl_display_set_yuv_to_display(ad->ogl, m);
		}
	}
	ms_filter_unlock(f);
	ms_queue_flush(f->inputs[0]);
	if (f->inputs[1] != NULL) {
		ms_queue_flush(f->inputs[1]);
	}
}

static void android_texture_display_postprocess(MSFilter *f) {
	AndroidTextureDisplay *ad = (AndroidTextureDisplay *)f->data;
	if (ad->refresh_task) ms_task_cancel_and_destroy(ad->refresh_task);
}

static void android_texture_display_uninit(MSFilter *f) {
	AndroidTextureDisplay *ad = (AndroidTextureDisplay *)f->data;
	MSTask *task;
	ms_worker_thread_add_task(ad->process_thread, (MSTaskFunc)android_texture_display_destroy_opengl, (void *)f);
	task = ms_worker_thread_add_waitable_task(ad->process_thread, (MSTaskFunc)android_texture_display_release_windowId,
	                                          (void *)f);
	ms_task_wait_completion(task);
	ms_task_destroy(task);
	android_texture_display_release_worker(ad->process_thread);
}

static int android_texture_display_set_window(MSFilter *f, void *arg) {
	AndroidTextureDisplay *ad = (AndroidTextureDisplay *)f->data;
	unsigned long id = *(unsigned long *)arg;
	jobject windowId = (jobject)id;
	JNIEnv *env = ms_get_jni_env();

	ms_filter_lock(f);

	ms_message("[TextureView Display][Filter=%p] New windowId jobject ptr is %p, current one is %p", f, windowId,
	           ad->nativeWindowId);
	if (id == 0) {
		if (ad->nativeWindowId) {
			ms_message("[TextureView Display][Filter=%p] New windowId is null but current isn't, scheduling it for "
			           "destruction",
			           f);
			(*env)->DeleteGlobalRef(env, ad->nativeWindowId);
			ad->nativeWindowId = NULL;
			ms_worker_thread_add_task(ad->process_thread, (MSTaskFunc)android_texture_display_destroy_opengl,
			                          (void *)f);
		}
	} else if (!(*env)->IsSameObject(env, ad->nativeWindowId, windowId)) {
		if (ad->nativeWindowId) {
			ms_message("[TextureView Display][Filter=%p] Scheduling current window to be destroyed first", f);
			(*env)->DeleteGlobalRef(env, ad->nativeWindowId);
			ad->nativeWindowId = NULL;
			ms_worker_thread_add_task(ad->process_thread, (MSTaskFunc)android_texture_display_destroy_opengl,
			                          (void *)f);
		}

		ad->nativeWindowId = (*env)->NewGlobalRef(env, windowId);
		ms_message("[TextureView Display][Filter=%p] Took global ref on %p, windowId is now %p, scheduling creation", f,
		           windowId, ad->nativeWindowId);
		ms_worker_thread_add_task(ad->process_thread, (MSTaskFunc)android_texture_display_init_opengl, (void *)f);
	} else {
		ms_message(
		    "[TextureView Display][Filter=%p] New windowId jobject %p is the same as the current one, skipping...", f,
		    windowId);
	}

	ms_filter_unlock(f);
	return 0;
}

static int android_texture_display_set_zoom(MSFilter *f, void *arg) {
	AndroidTextureDisplay *ad = (AndroidTextureDisplay *)f->data;
	if (ad->ogl) {
		ogl_display_zoom(ad->ogl, arg);
	}
	return 0;
}

static int android_texture_display_set_mode(MSFilter *f, void *arg) {
	AndroidTextureDisplay *ad = (AndroidTextureDisplay *)f->data;
	ad->mode = *((MSVideoDisplayMode *)arg);
	return 0;
}

static MSFilterMethod methods[] = {{MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID, android_texture_display_set_window},
                                   {MS_VIDEO_DISPLAY_ZOOM, android_texture_display_set_zoom},
                                   {MS_VIDEO_DISPLAY_SET_MODE, android_texture_display_set_mode},
                                   {0, NULL}};

MSFilterDesc ms_android_texture_display_desc = {.id = MS_ANDROID_TEXTURE_DISPLAY_ID,
                                                .name = "MSAndroidTextureDisplay",
                                                .text = "Video display filter for Android.",
                                                .category = MS_FILTER_OTHER,
                                                .ninputs = 2,  /*number of inputs*/
                                                .noutputs = 0, /*number of outputs*/
                                                .init = android_texture_display_init,
                                                .preprocess = android_texture_display_preprocess,
                                                .process = android_texture_display_process,
                                                .postprocess = android_texture_display_postprocess,
                                                .uninit = android_texture_display_uninit,
                                                .methods = methods};

bool_t libmsandroidtexturedisplay_init(MSFactory *factory) {
	ms_factory_register_filter(factory, &ms_android_texture_display_desc);
	return TRUE;
}

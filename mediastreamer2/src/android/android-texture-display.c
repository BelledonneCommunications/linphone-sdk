/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
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

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msjava.h"
#include "mediastreamer2/msasync.h"
#include "layouts.h"
#include "opengles_display.h"

#include <dlfcn.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <android/hardware_buffer.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/rect.h>
#include <android/window.h>

typedef struct AndroidTextureDisplay {
	jobject surface;
	ANativeWindow *window;
	struct opengles_display* ogl;
	EGLSurface gl_surface;
	EGLDisplay gl_display;
	EGLContext gl_context;
	MSWorkerThread *process_thread;
	queue_t entry_q;
	jobject surfaceTexture;
} AndroidTextureDisplay;

static void android_texture_display_destroy_opengl(MSFilter *f) {
	AndroidTextureDisplay *ad = (AndroidTextureDisplay*)f->data;
	ms_filter_lock(f);
	ms_message("[TextureView Display] Destroying context");

	if (ad->ogl) {
		ogl_display_uninit(ad->ogl, TRUE);
		ogl_display_free(ad->ogl);
		ad->ogl = NULL;
		ms_message("[TextureView Display] OGL display destroyed");
	}

	EGLBoolean result;
	if (ad->gl_display) {

		if (eglMakeCurrent(ad->gl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_FALSE) {         
			ms_error("[TextureView Display] Unable to eglMakeCurrent in destructor");
		}

		if (ad->gl_context) {
			result = eglDestroyContext(ad->gl_display, ad->gl_context);
			if (result != EGL_TRUE) {
				ms_error("[TextureView Display] eglDestroyContext failure: %u", result);
			}
			ad->gl_context = NULL;
		}

		if (ad->gl_surface) {
			result = eglDestroySurface(ad->gl_display, ad->gl_surface);
			if (result != EGL_TRUE) {
				ms_error("[TextureView Display] eglDestroySurface failure: %u", result);
			}
			ad->gl_surface = NULL;
		}

		result = eglTerminate(ad->gl_display);
		if (result != EGL_TRUE) {
			ms_error("[TextureView Display] eglTerminate failure: %u", result);
		}
		
		ad->gl_display = NULL;
		ms_message("[TextureView Display] EGL display destroyed");
	}

	if (ad->window) {
		ANativeWindow_release(ad->window);
		ad->window = NULL;
		ms_message("[TextureView Display] Window released");
	}

	if (ad->surface) {
		JNIEnv *env = ms_get_jni_env();
		(*env)->DeleteGlobalRef(env, ad->surface);
		ad->surface = NULL;
		ms_message("[TextureView Display] Surface destroyed");
	}

	ms_message("[TextureView Display] Context destroyed");
	ms_filter_unlock(f);
}

static void android_texture_display_create_surface_from_surface_texture(AndroidTextureDisplay *d) {
	JNIEnv *env = ms_get_jni_env();
	jobject surface;
	jclass surfaceTextureClass = (*env)->FindClass(env, "android/graphics/SurfaceTexture");
	if (!surfaceTextureClass) {
		ms_error("[TextureView Display] Could not find android.graphics.SurfaceTexture class");
		return;
	}

	if (!(*env)->IsInstanceOf(env, d->surfaceTexture, surfaceTextureClass)) {
		ms_message("[TextureView Display] NativePreviewWindowId %p isn't a SurfaceTexture, try to use it directly", d->surfaceTexture);
		d->surface = (jobject)(*env)->NewGlobalRef(env, d->surfaceTexture);
		return;
	}

	jclass surfaceClass = (*env)->FindClass(env, "android/view/Surface");
	if (!surfaceClass) {
		ms_error("[TextureView Display] Could not find android.view.Surface class");
		return;
	}
	ms_message("[TextureView Display] Creating Surface from SurfaceTexture");
	
	jmethodID ctor = (*env)->GetMethodID(env, surfaceClass, "<init>", "(Landroid/graphics/SurfaceTexture;)V");
	surface = (*env)->NewObject(env, surfaceClass, ctor, d->surfaceTexture);
	if (!surface) {
		ms_error("[TextureView Display] Could not instanciate android.view.Surface object");
		return;
	}

	d->surface = (jobject)(*env)->NewGlobalRef(env, surface);
	ms_message("[TextureView Display] Surface created: %p", d->surface);
}

static void android_texture_display_init_opengl(MSFilter *f) {
	AndroidTextureDisplay *ad = (AndroidTextureDisplay*)f->data;
	ms_filter_lock(f);
   	JNIEnv *jenv = ms_get_jni_env();
	ms_message("[TextureView Display] Initializing context");

	if (ad->surfaceTexture) {
		android_texture_display_create_surface_from_surface_texture(ad);
	} else {
		ms_error("[TextureView Display] Can't init display, no surface texture set");
		ms_filter_unlock(f);
		return;
	}

	EGLint attribs [] = {
		EGL_RED_SIZE, 5,
		EGL_GREEN_SIZE, 6,
		EGL_BLUE_SIZE, 5,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_NONE
	};

	EGLint w, h, format;
	EGLint numConfigs;
	EGLConfig config;
	EGLSurface surface;
	EGLContext context;

	EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	eglInitialize(display, 0, 0);

	eglChooseConfig(display, attribs, &config, 1, &numConfigs);

	eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);

	ms_message("[TextureView Display] Chosen format is %i", format);

	ad->window = ANativeWindow_fromSurface(jenv, ad->surface);
	ANativeWindow_setBuffersGeometry(ad->window, 0, 0, format);

	surface = eglCreateWindowSurface(display, config, ad->window, NULL);

	eglQuerySurface(display, surface, EGL_WIDTH, &w);
	eglQuerySurface(display, surface, EGL_HEIGHT, &h);
	ms_message("[TextureView Display] Surface size is %ix%i", w, h);

	EGLint contextAttrs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};
	context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttrs);

	if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {         
		ms_error("[TextureView Display] Unable to eglMakeCurrent");
		ms_filter_unlock(f);
		return;
	}

	ad->gl_display = display;
	ad->gl_surface = surface;
	ad->gl_context = context;

	ad->ogl = ogl_display_new();
	ogl_display_init(ad->ogl, NULL, w, h);

	ms_message("[TextureView Display] Context initialized");
	ms_filter_unlock(f);
}

static void android_texture_display_swap_buffers(MSFilter *f) {
	AndroidTextureDisplay *ad = (AndroidTextureDisplay*)f->data;
	mblk_t *m;
	ms_filter_lock(f);

	if ((m = getq(&ad->entry_q)) == NULL) {
		ms_warning("[TextureView Display] No frame in entry queue");
		ms_filter_unlock(f);
		return;
	}

	ogl_display_set_yuv_to_display(ad->ogl, m);
	ogl_display_render(ad->ogl, 0);

	EGLBoolean result = eglSwapBuffers(ad->gl_display, ad->gl_surface);
	if (result != EGL_TRUE) {
		ms_error("[TextureView Display] eglSwapBuffers failure: %u", result);
	}

	ms_filter_unlock(f);
}

static void android_texture_display_init(MSFilter *f) {
	AndroidTextureDisplay *ad = (AndroidTextureDisplay*)ms_new0(AndroidTextureDisplay, 1);
	ad->surface = NULL;
	ad->surfaceTexture = NULL;
	ad->process_thread = ms_worker_thread_new();
	qinit(&ad->entry_q);
	f->data = ad;
}

static void android_texture_display_process(MSFilter *f) {
	AndroidTextureDisplay *ad = (AndroidTextureDisplay*)f->data;
	mblk_t *m;

	ms_filter_lock(f);
	if (ad->surfaceTexture != NULL) {

		if (!ad->ogl) {
			ms_warning("[TextureView Display] Window set but no OGL context, let's init it");
			ms_worker_thread_add_task(ad->process_thread, (MSTaskFunc)android_texture_display_init_opengl, (void*)f);
		}

		if ((m = ms_queue_peek_last(f->inputs[0])) != NULL) {
			if (ad->ogl) {
				ms_queue_remove(f->inputs[0], m);
				putq(&ad->entry_q, m);
				ms_worker_thread_add_task(ad->process_thread, (MSTaskFunc)android_texture_display_swap_buffers, (void*)f);
			} else {
				ms_error("[TextureView Display] Processing without an OGL context !");
			}
		}
	}
	ms_filter_unlock(f);

	ms_queue_flush(f->inputs[0]);
	if (f->inputs[1] != NULL) {
		ms_queue_flush(f->inputs[1]);
	}
}

static void android_texture_display_postprocess(MSFilter *f) {
	AndroidTextureDisplay *ad = (AndroidTextureDisplay*)f->data;
	ms_worker_thread_add_task(ad->process_thread, (MSTaskFunc)android_texture_display_destroy_opengl, (void*)f);
}

static void android_texture_display_uninit(MSFilter *f) {
	AndroidTextureDisplay *ad = (AndroidTextureDisplay*)f->data;
	ms_worker_thread_destroy(ad->process_thread, TRUE);
	flushq(&ad->entry_q, 0);
	ms_free(ad);
}

static int android_texture_display_set_window(MSFilter *f, void *arg) {
	AndroidTextureDisplay *ad = (AndroidTextureDisplay*)f->data;

	unsigned long id = *(unsigned long *)arg;
	jobject surfaceTexture = (jobject)id;
	ms_message("[TextureView Display] New window jobject ptr is %p, current one is %p", surfaceTexture, ad->surfaceTexture);

	ms_filter_lock(f);

	if (id == 0) {
		ad->surfaceTexture = NULL;
		ms_worker_thread_add_task(ad->process_thread, (MSTaskFunc)android_texture_display_destroy_opengl, (void*)f);
	} else if (surfaceTexture != ad->surfaceTexture) {
		if (ad->surfaceTexture) {
			ms_worker_thread_add_task(ad->process_thread, (MSTaskFunc)android_texture_display_destroy_opengl, (void*)f);
		}
		ad->surfaceTexture = surfaceTexture;
		ms_worker_thread_add_task(ad->process_thread, (MSTaskFunc)android_texture_display_init_opengl, (void*)f);
	}
	
	ms_filter_unlock(f);
	return 0;
}

static int android_texture_display_set_zoom(MSFilter* f, void* arg) {
	AndroidTextureDisplay *ad = (AndroidTextureDisplay*)f->data;
	if (ad->ogl) {
		ogl_display_zoom(ad->ogl, arg);
	}
	return 0;
}

static MSFilterMethod methods[] = {
	{	MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID,	android_texture_display_set_window	},
	{	MS_VIDEO_DISPLAY_ZOOM,					android_texture_display_set_zoom	},
	{	0, 										NULL								}
};

MSFilterDesc ms_android_texture_display_desc = {
	.id=MS_ANDROID_TEXTURE_DISPLAY_ID,
	.name="MSAndroidTextureDisplay",
	.text="Video display filter for Android.",
	.category=MS_FILTER_OTHER,
	.ninputs=2, /*number of inputs*/
	.noutputs=0, /*number of outputs*/
	.init=android_texture_display_init,
	.process=android_texture_display_process,
	.postprocess=android_texture_display_postprocess,
	.uninit=android_texture_display_uninit,
	.methods=methods
};


bool_t libmsandroidtexturedisplay_init(MSFactory *factory) {
	ms_factory_register_filter(factory, &ms_android_texture_display_desc);
	return TRUE;
}

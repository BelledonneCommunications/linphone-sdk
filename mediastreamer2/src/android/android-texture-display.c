/*
mediastreamer2 android video display filter
Copyright (C) 2010-2018 Belledonne Communications SARL (simon.morlat@linphone.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msjava.h"
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
} AndroidTextureDisplay;

static void android_texture_display_destroy_opengl(AndroidTextureDisplay *ad) {
	ms_debug("Destroying context");
	if (ad->ogl) {
		ogl_display_uninit(ad->ogl, FALSE);
		ms_free(ad->ogl);
		ad->ogl = NULL;
	}
	if (ad->gl_surface) {
		if (ad->gl_display) {
			eglDestroySurface(ad->gl_display, ad->gl_surface);
			ad->gl_surface = NULL;
			ad->gl_display = NULL;
		}
	}
	if (ad->window) {
		ANativeWindow_release(ad->window);
		ad->window = NULL;
	}
}

static void android_texture_display_init_opengl(AndroidTextureDisplay *ad) {
   	JNIEnv *jenv = ms_get_jni_env();
	ms_debug("Initializing context");

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

	ms_debug("Chosen format is %i", format);

	ad->window = ANativeWindow_fromSurface(jenv, ad->surface);
	ANativeWindow_setBuffersGeometry(ad->window, 0, 0, format);

	surface = eglCreateWindowSurface(display, config, ad->window, NULL);

	eglQuerySurface(display, surface, EGL_WIDTH, &w);
	eglQuerySurface(display, surface, EGL_HEIGHT, &h);
	ms_debug("Surface size is %ix%i", w, h);

	EGLint contextAttrs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};
	context = eglCreateContext(display, config, NULL, contextAttrs);

	if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {         
		ms_debug("Unable to eglMakeCurrent");
		return;
	}

	ad->gl_display = display;
	ad->gl_surface = surface;

	ad->ogl = ogl_display_new();
	ogl_display_init(ad->ogl, NULL, w, h);
}

static void android_texture_display_init(MSFilter *f) {
	AndroidTextureDisplay *ad = (AndroidTextureDisplay*)ms_new0(AndroidTextureDisplay, 1);
	ad->surface = NULL;
	f->data = ad;
}

static void android_texture_display_uninit(MSFilter *f) {
	AndroidTextureDisplay *ad = (AndroidTextureDisplay*)f->data;
	android_texture_display_destroy_opengl(ad);
	ms_free(ad);
}

static void android_texture_display_process(MSFilter *f) {
	AndroidTextureDisplay *ad = (AndroidTextureDisplay*)f->data;
	MSPicture pic;
	mblk_t *m;

	ms_filter_lock(f);
	if (ad->surface != NULL) {
		if (!ad->ogl) {
			android_texture_display_init_opengl(ad);
		}

		if ((m = ms_queue_peek_last(f->inputs[0])) != NULL) {
			if (ms_yuv_buf_init_from_mblk(&pic, m) == 0) {
				if (ad->ogl) {
					ogl_display_set_yuv_to_display(ad->ogl, m);
					ogl_display_render(ad->ogl, 0);
					eglSwapBuffers(ad->gl_display, ad->gl_surface);
				}
			}
		}
	}
	ms_filter_unlock(f);

	ms_queue_flush(f->inputs[0]);
	if (f->inputs[1] != NULL) {
		ms_queue_flush(f->inputs[1]);
	}
}

static int android_texture_display_set_window(MSFilter *f, void *arg) {
	AndroidTextureDisplay *ad = (AndroidTextureDisplay*)f->data;

	unsigned long id = *(unsigned long *)arg;
	jobject surface = (jobject)id;
	ms_filter_lock(f);
	if (id == 0) {
		ad->surface = NULL;
		android_texture_display_destroy_opengl(ad);
	} else if (surface != ad->surface) {
		if (ad->surface) {
			android_texture_display_destroy_opengl(ad);
		}
		ad->surface = surface;
	}
	ms_filter_unlock(f);
	return 0;
}

static MSFilterMethod methods[] = {
	{	MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID , android_texture_display_set_window	},
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
	.uninit=android_texture_display_uninit,
	.methods=methods
};


bool_t libmsandroidtexturedisplay_init(MSFactory *factory) {
	/*See if we can use AndroidBitmap_* symbols (only since android 2.2 normally)*/
	ms_factory_register_filter(factory, &ms_android_texture_display_desc);
	return TRUE;
}

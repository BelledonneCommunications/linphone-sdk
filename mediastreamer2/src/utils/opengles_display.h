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

#ifndef OPENGLES_DISPLAY_H
#define OPENGLES_DISPLAY_H

#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msfilter.h"

#include "opengl_functions.h"
#ifdef MS2_WINDOWS_UWP
#include <agile.h>
using namespace Windows::UI::Core;
using namespace Windows::ApplicationModel::Core;
using namespace Platform;
#endif

#if defined __cplusplus
extern "C" {
#endif

struct opengles_display;

/**
 * Create opaque structure to handle OpenGL display
 */
MS2_PUBLIC struct opengles_display *ogl_display_new (void);

/**
 * Release opaque struct memory
 */
MS2_PUBLIC void ogl_display_free (struct opengles_display *gldisp);

/**
 * Perform initialization of opaque structure that will be auto-managed.
 * Dimensions are auto-managed and are retrieved from EGL. If they cannot be retrieved for any reason, the input dimensions will be used.
 *
 * @param f OpenGL functions to use. Can be NULL.
 * @param window The native window where a Surface will be created
 * @param width Default width of the display area.
 * @param height Default Height of the display area.
 */
MS2_PUBLIC void ogl_display_auto_init (struct opengles_display *gldisp, const OpenGlFunctions *f, EGLNativeWindowType window, int width, int height);


/**
 * Perform initialization of opaque structure.
 *
 * @param f OpenGL functions to use. Can be NULL.
 * @param width Width of the display area
 * @param height Height of the display area.
 */
MS2_PUBLIC void ogl_display_init (struct opengles_display *gldisp, const OpenGlFunctions *f, int width, int height);

/**
 * Perform resize of opaque structure.
 *
 * Must be called every time the display area dimensions change
 *
 * @param width Width of the display area
 * @param height Height of the display area.
 */
MS2_PUBLIC void ogl_display_set_size (struct opengles_display *gldisp, int width, int height);

/**
 * Describes the EGL context to be created by ogl_display_auto_init
 *
 * Safety:
 *   @param target_context cannot be NULL.
 */
MS2_PUBLIC void ogl_display_set_target_context (struct opengles_display *gldisp, const MSEGLContextDescriptor *const target_context);

/**
 * Uninit opaque structure.
 *
 * @param freeGLresources Are we allowed to release GL resources. GL resources
 * must only be freed within the correct GL context.
 */
MS2_PUBLIC void ogl_display_uninit (struct opengles_display *gldisp, bool_t freeGLresources);

/**
 * Define the next yuv image to display. Note that yuv content will be copied.
 */
MS2_PUBLIC void ogl_display_set_yuv_to_display (struct opengles_display *gldisp, mblk_t *yuv);

/**
 * Get last yuv image that was set to display. The mblk_t is still owned by the opengles_display structure.
 */
MS2_PUBLIC mblk_t *ogl_display_get_yuv_to_display(struct opengles_display *gldisp);

/**
 * Define the next preview image to diaplsy. Note that yuv its content will be copied.
 */
MS2_PUBLIC void ogl_display_set_preview_yuv_to_display (struct opengles_display *gldisp, mblk_t *yuv);


/**
 * Render display. It will update viewport if sizes can be retrieved from EGL Surface. If not, the last size will be used.
 *
 * @param deviceAngleFromPortrait Angle of display. 0=Portrait, 90=Landscape
 * @param mode Method use for the video display.
 */
MS2_PUBLIC void ogl_display_render (struct opengles_display *gldisp, int deviceAngleFromPortrait, MSVideoDisplayMode mode);

/**
 * @params contains the zoom parameters: [0] = zoom_factor, [1][2] = zoom center x/y (between [0,1], relative coords to the gl surface. 0.5/0.5 = center)
 */
MS2_PUBLIC void ogl_display_zoom (struct opengles_display *gldisp, float *params);

/**
 * Request horizontal flip of the image.
 */
MS2_PUBLIC void ogl_display_enable_mirroring_to_display(struct opengles_display *gldisp, bool_t enabled);

/**
 * Request horizontal flip of the preview image.
 */
MS2_PUBLIC void ogl_display_enable_mirroring_to_preview(struct opengles_display *gldisp, bool_t enabled);

/**
 * Create a new Window and store it into the EGLNativeWindowType generic structure
 */
#ifdef MS2_WINDOWS_UWP
MS2_PUBLIC bool_t ogl_create_window(EGLNativeWindowType *window, Platform::Agile<CoreApplicationView>* windowId);
MS2_PUBLIC void ogl_destroy_window(EGLNativeWindowType *window, Platform::Agile<CoreApplicationView>* windowId);
#else
MS2_PUBLIC bool_t ogl_create_window(EGLNativeWindowType *window, void ** window_id);
MS2_PUBLIC void ogl_destroy_window(EGLNativeWindowType *window, void ** window_id);
#endif


#if defined __cplusplus
};
#endif


#ifdef __ANDROID__
#include <jni.h>
JNIEXPORT void JNICALL Java_org_linphone_mediastream_video_display_OpenGLESDisplay_init (JNIEnv * env, jobject obj, jlong ptr, jint width, jint height);
JNIEXPORT void JNICALL Java_org_linphone_mediastream_video_display_OpenGLESDisplay_render (JNIEnv * env, jobject obj, jlong ptr);
#endif

#endif

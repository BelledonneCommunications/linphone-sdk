/*
 opengles_display.m
 Copyright (C) 2011 Belledonne Communications, Grenoble, France

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
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */


#ifndef OPENGLESDISPLAY_H
#define OPENGLESDISPLAY_H

#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msfilter.h"

struct opengles_display;

struct opengles_display* ogl_display_new();

void ogl_display_free(struct opengles_display* gldisp);

void ogl_display_init(struct opengles_display* gldisp, int width, int height);

void ogl_display_uninit(struct opengles_display* gldisp, bool_t freeGLresources);

void ogl_display_set_yuv_to_display(struct opengles_display* gldisp, mblk_t *yuv);

void ogl_display_set_preview_yuv_to_display(struct opengles_display* gldisp, mblk_t *yuv);

void ogl_display_render(struct opengles_display* gldisp);

#ifdef ANDROID
#include <jni.h>
JNIEXPORT void JNICALL Java_org_linphone_mediastream_video_display_OpenGLESDisplay_init(JNIEnv * env, jobject obj, jint ptr, jint width, jint height);
JNIEXPORT void JNICALL Java_org_linphone_mediastream_video_display_OpenGLESDisplay_render(JNIEnv * env, jobject obj, jint ptr);
#endif


#endif

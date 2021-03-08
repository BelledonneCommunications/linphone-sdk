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

#ifndef msogl_h
#define msogl_h

#define MS_OGL_RENDER MS_FILTER_METHOD_NO_ARG(MS_OGL_ID, 0)

/**
 * #MSOglContextInfo is used for the "MSOGL" filter (OpenGL Display) that can be set with #linphone_core_set_video_display_filter.
 * This type is available on Desktop platforms (Linux, MacOS and Windows) but not for UWP where you have to use directly a SwapChainPanel(see #linphone_core_set_native_video_window_id)
 * Use an instance of this structure in #linphone_core_set_native_preview_window_id or #linphone_core_set_native_video_window_id
 * At runtime, you need to ensure to have an access to OpenGL libraries (libGLESv2/libGLEW/opengl32 and libEGL) as it is not provided by the SDK.
 * For example, you can use the nuget package `ANGLE.WindowsStore` for Windows as libraries are not packaged with the OS.
 *
 * Fill `window` with your EGLNativeWindowType in order to auto manage the OpenGL surface.
 ** `CALayer*` for Mac
 ** `HWND` for Windows of `IInspectable*` for Windows Store
 ** `Window` for X11
 *
 * Mediastreamer will use EGL functions(libEGL) to get sizes. If they cannot be retrieved, it will use the input sizes as default.
 *
 * Set `getProcAddress` to customize OpenGL calls : Mediastreamer will use this function to initialize all pointers on OpenGL functions at runtime.
 * If this variable is not set, Mediastreamer will try to load default functions directly from OpenGL libraries at runtime.
 *
**/
struct _MSOglContextInfo {
	void *window;// Set it to use EGL auto managing or else, set sizes below
	unsigned int width;
	unsigned int height;
	void *(*getProcAddress)(const char *name);	// Optional, set to NULL for using default OpenGL functions
};
typedef struct _MSOglContextInfo MSOglContextInfo;

#endif

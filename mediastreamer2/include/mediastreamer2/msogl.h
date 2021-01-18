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
 * Use an instance of this structure in setNativePreviewWindowId or setNativeVideoWindowId
 * On Windows, fill the window with a EGLNativeWindowType in order to auto manage the OpenGL surface
 * If you're not on windows or you cannot use EGL functions, you can fill the size.
 *
 * Use the pointer to function `getProcAddress` to customize openGL calls. If this variable is not set, Linphone will try to load functions directly from OpenGL libraries
**/
struct _MSOglContextInfo {
	void *window;// Set it to use EGL auto managing or else, set sizes below
	unsigned int width;
	unsigned int height;
	void *(*getProcAddress)(const char *name);
};
typedef struct _MSOglContextInfo MSOglContextInfo;

#endif

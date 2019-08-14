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
#if !defined(_X11_HELPER_H_)
#define _X11_HELPER_H_

#ifdef HAVE_X11_XLIB_H

#include <X11/Xlib.h>

typedef struct _x11_helper {
	Display *display;
	Window window;
} MSX11Helper;

/* One time init */
int ms_x11_helper_init(MSX11Helper* x11);

int ms_x11_helper_create_window(MSX11Helper* x11, int width, int height);

int ms_x11_helper_get_window_size(MSX11Helper* x11, int* width, int* height);

int ms_x11_helper_destroy_window(MSX11Helper* x11);

int ms_x11_helper_uninit(MSX11Helper* x11);

#endif

#endif

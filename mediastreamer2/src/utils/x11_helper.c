/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2010  Belledonne Communications SARL
Author: Simon Morlat <simon.morlat@linphone.org>

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

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif
#ifdef HAVE_X11_XLIB_H

#include "mediastreamer2/x11_helper.h"
#include "mediastreamer2/mscommon.h"
#include <ortp/port.h>

int ms_x11_helper_init(MSX11Helper* x11) {
	const char *display;

	display = getenv("DISPLAY");
	if (display == NULL)
		display = ":0";

	x11->display = XOpenDisplay(display);
	if (x11->display == NULL) {
		ms_error("Could not open display %s", display);
		return -1;
	}
	return 0;
}

int ms_x11_helper_create_window(MSX11Helper* x11, int width, int height) {
	XSetWindowAttributes wa;

	memset(&wa, 0, sizeof(wa));
	wa.event_mask = StructureNotifyMask;

	x11->window = XCreateWindow(
		x11->display,
		DefaultRootWindow(x11->display),
		200, 200,
		width, height,
		0, CopyFromParent, CopyFromParent, CopyFromParent,
		CWEventMask | CWBackPixel,
		&wa);

	if (x11->window==0){
		ms_error("Could not create X11 window.");
		return -1;
	}

	XMapWindow(x11->display, x11->window);

	XClearWindow(x11->display, x11->window);

	XCreateGC(x11->display, x11->window, 0, NULL);

	return 0;
}


int ms_x11_helper_get_window_size(MSX11Helper* x11, int* width, int* height) {
	XWindowAttributes wa;
	XGetWindowAttributes(x11->display,x11->window, &wa);

	*width = wa.width;
	*height = wa.height;

	return 0;
}

int ms_x11_helper_destroy_window(MSX11Helper* x11) {
	XDestroyWindow(x11->display, x11->window);

	return 0;
}

int ms_x11_helper_uninit(MSX11Helper* x11) {
	if (x11->display) {
		XCloseDisplay(x11->display);
		x11->display = NULL;
	}

	return 0;
}
#endif

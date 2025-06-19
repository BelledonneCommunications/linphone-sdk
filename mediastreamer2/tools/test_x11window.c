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

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif
#include <stdio.h>
#ifdef HAVE_X11_XLIB_H
#include <X11/Xlib.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include <bctoolbox/defs.h>

int main(BCTBX_UNUSED(int argc), BCTBX_UNUSED(char *argv[])) {
#ifdef HAVE_X11_XLIB_H
	Display *display = XOpenDisplay(getenv("DISPLAY"));
	XSetWindowAttributes wa;
	Window w;
	memset(&wa, 0, sizeof(wa));
	wa.event_mask = StructureNotifyMask;
	if (display == NULL) {
		printf("Could not open display.\n");
		return -1;
	}
	w = XCreateWindow(display, DefaultRootWindow(display), 200, 200, 352, 288, 0, CopyFromParent, CopyFromParent,
	                  CopyFromParent, CWEventMask | CWBackPixel, &wa);
	XMapWindow(display, w);
	XSync(display, 0);
	printf("Created window with id=%lu\n", w);
	while (1) {
		sleep(1);
	}
	return 0;
#else
	printf("This program only works with X11.\n");
	return -1;
#endif
}

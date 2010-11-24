/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2010  Belledonne Communications SARL (simon.morlat@linphone.org)

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
#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#ifdef HAVE_X11_XLIB_H
#include <X11/Xlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#endif


int main(int argc, char *argv[]){
#ifdef HAVE_X11_XLIB_H
	Display *display=XOpenDisplay(getenv("DISPLAY"));
	XSetWindowAttributes wa;
	memset(&wa,0,sizeof(wa));
	wa.event_mask=StructureNotifyMask;
	if (display==NULL){
		printf("Could not open display.\n");
		return -1;
	}
	Window w=XCreateWindow(display,DefaultRootWindow(display),200,200,
	                      352, 288,0,CopyFromParent,CopyFromParent,CopyFromParent,
	                CWEventMask|CWBackPixel,&wa);
	XMapWindow(display,w);
	XSync(display,0);
	printf("Created window with id=%lu\n",w);
	while (1){
		sleep(1);
	}
	return 0;
#else
	printf("This program only works with X11.\n");
	return -1;
#endif
}


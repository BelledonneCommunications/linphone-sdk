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
#ifdef HAVE_X11_XLIB_H

#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/x11_helper.h"
#include <ortp/port.h>

int ms_x11_helper_init(MSX11Helper *x11) {
	const char *display;

	display = getenv("DISPLAY");
	if (display == NULL) display = ":0";

	x11->display = XOpenDisplay(display);
	if (x11->display == NULL) {
		ms_error("Could not open display %s", display);
		return -1;
	}
	return 0;
}

int ms_x11_helper_create_window(MSX11Helper *x11, int width, int height) {
	XSetWindowAttributes wa;

	memset(&wa, 0, sizeof(wa));
	wa.event_mask = StructureNotifyMask;

	x11->window = XCreateWindow(x11->display, DefaultRootWindow(x11->display), 200, 200, width, height, 0,
	                            CopyFromParent, CopyFromParent, CopyFromParent, CWEventMask | CWBackPixel, &wa);

	if (x11->window == 0) {
		ms_error("Could not create X11 window.");
		return -1;
	}

	XMapWindow(x11->display, x11->window);

	XClearWindow(x11->display, x11->window);

	XCreateGC(x11->display, x11->window, 0, NULL);

	return 0;
}

int ms_x11_helper_get_window_size(MSX11Helper *x11, int *width, int *height) {
	XWindowAttributes wa;
	XGetWindowAttributes(x11->display, x11->window, &wa);

	*width = wa.width;
	*height = wa.height;

	return 0;
}

int ms_x11_helper_destroy_window(MSX11Helper *x11) {
	XDestroyWindow(x11->display, x11->window);

	return 0;
}

int ms_x11_helper_uninit(MSX11Helper *x11) {
	if (x11->display) {
		XCloseDisplay(x11->display);
		x11->display = NULL;
	}

	return 0;
}

MSPixFmt ms_x11_image_get_pixel_format(XImage *image) {
	switch (image->bits_per_pixel) {
		case 8:
			return MS_YUV420P; // AV_PIX_FMT_PAL8;
		case 16: {
			if (image->red_mask == 0xf800 && image->green_mask == 0x07e0 && image->blue_mask == 0x001f)
				return MS_RGB565;
			if (image->red_mask == 0x7c00 && image->green_mask == 0x03e0 && image->blue_mask == 0x001f)
				ms_warning("Unsupported pixel format : AV_PIX_FMT_RGB555.");
			break;
		}
		case 24: {
			if (image->red_mask == 0xff0000 && image->green_mask == 0x00ff00 && image->blue_mask == 0x0000ff)
				return MS_RGB24_REV;
			if (image->red_mask == 0x0000ff && image->green_mask == 0x00ff00 && image->blue_mask == 0xff0000)
				return MS_RGB24;
			break;
		}
		case 32: {
			if (image->red_mask == 0xff0000 && image->green_mask == 0x00ff00 && image->blue_mask == 0x0000ff)
				return MS_RGBA32_REV; // Is AV_PIX_FMT_BGRA
			if (image->red_mask == 0x0000ff && image->green_mask == 0x00ff00 && image->blue_mask == 0xff0000)
				return MS_RGBA32;
			if (image->red_mask == 0xff000000 && image->green_mask == 0x00ff0000 && image->blue_mask == 0x0000ff00)
				ms_warning("Unsupported pixel format : AV_PIX_FMT_ABGR.");
			if (image->red_mask == 0x0000ff00 && image->green_mask == 0x00ff0000 && image->blue_mask == 0xff000000)
				ms_warning("Unsupported pixel format : AV_PIX_FMT_ARGB.");
			break;
		}
	}
	ms_error("Unsupported X11 image pixel format!\nbits_per_pixel = %d, red_mask = "
	         "%ld, green_mask = %ld, blue_mask = %ld",
	         image->bits_per_pixel, image->red_mask, image->green_mask, image->blue_mask);
	return 0;
}

// clears a rectangular area of an image (i.e. sets the memory to zero, which will most likely make the image black)
void ms_x11_image_clear_rectangle(XImage *image, int x, int y, int w, unsigned int h) {
	// check the image format
	if (image->bits_per_pixel % 8 != 0) return;
	unsigned int pixel_bytes = image->bits_per_pixel / 8;

	// fill the rectangle with zeros
	for (unsigned int j = 0; j < h; ++j) {
		uint8_t *image_row = (uint8_t *)image->data + image->bytes_per_line * (y + j);
		memset(image_row + pixel_bytes * x, 0, pixel_bytes * w);
	}
}

// Draws the current cursor at the current position on the image. Requires XFixes.
// Note: In the original code from x11grab, the variables for red and blue are swapped
// (which doesn't change the result, but it's confusing).
// Note 2: This function assumes little-endianness.
// Note 3: This function only supports 24-bit and 32-bit images (it does nothing for other bit depths).
void ms_x11_image_draw_cursor(BCTBX_UNUSED(Display *dpy),
                              BCTBX_UNUSED(XImage *image),
                              BCTBX_UNUSED(int recording_area_x),
                              BCTBX_UNUSED(int recording_area_y)) {
#ifdef HAVE_XFIXES
	// check the image format
	unsigned int pixel_bytes, r_offset, g_offset, b_offset;
	if (image->bits_per_pixel == 24 && image->red_mask == 0xff0000 && image->green_mask == 0x00ff00 &&
	    image->blue_mask == 0x0000ff) {
		pixel_bytes = 3;
		r_offset = 2;
		g_offset = 1;
		b_offset = 0;
	} else if (image->bits_per_pixel == 24 && image->red_mask == 0x0000ff && image->green_mask == 0x00ff00 &&
	           image->blue_mask == 0xff0000) {
		pixel_bytes = 3;
		r_offset = 0;
		g_offset = 1;
		b_offset = 2;
	} else if (image->bits_per_pixel == 32 && image->red_mask == 0xff0000 && image->green_mask == 0x00ff00 &&
	           image->blue_mask == 0x0000ff) {
		pixel_bytes = 4;
		r_offset = 2;
		g_offset = 1;
		b_offset = 0;
	} else if (image->bits_per_pixel == 32 && image->red_mask == 0x0000ff && image->green_mask == 0x00ff00 &&
	           image->blue_mask == 0xff0000) {
		pixel_bytes = 4;
		r_offset = 0;
		g_offset = 1;
		b_offset = 2;
	} else if (image->bits_per_pixel == 32 && image->red_mask == 0xff000000 && image->green_mask == 0x00ff0000 &&
	           image->blue_mask == 0x0000ff00) {
		pixel_bytes = 4;
		r_offset = 3;
		g_offset = 2;
		b_offset = 1;
	} else if (image->bits_per_pixel == 32 && image->red_mask == 0x0000ff00 && image->green_mask == 0x00ff0000 &&
	           image->blue_mask == 0xff000000) {
		pixel_bytes = 4;
		r_offset = 1;
		g_offset = 2;
		b_offset = 3;
	} else {
		return;
	}

	// get the cursor
	XFixesCursorImage *xcim = XFixesGetCursorImage(dpy);
	if (xcim == NULL) return;

	// calculate the position of the cursor
	int x = xcim->x - xcim->xhot - recording_area_x;
	int y = xcim->y - xcim->yhot - recording_area_y;

	// calculate the part of the cursor that's visible
	int cursor_left = x < 0 ? -x : 0, cursor_right = xcim->width < image->width - x ? xcim->width : image->width - x;
	int cursor_top = y < 0 ? -y : 0,
	    cursor_bottom = xcim->height < image->height - y ? xcim->height : image->height - y;

	// draw the cursor
	// XFixesCursorImage uses 'long' instead of 'int' to store the cursor images, which is a bit weird since
	// 'long' is 64-bit on 64-bit systems and only 32 bits are actually used. The image uses premultiplied alpha.
	for (int j = cursor_top; j < cursor_bottom; ++j) {
		unsigned long *cursor_row = xcim->pixels + xcim->width * j;
		uint8_t *image_row = (uint8_t *)image->data + image->bytes_per_line * (y + j);
		for (int i = cursor_left; i < cursor_right; ++i) {
			unsigned long cursor_pixel = cursor_row[i];
			uint8_t *image_pixel = image_row + pixel_bytes * (x + i);
			int cursor_a = (uint8_t)(cursor_pixel >> 24);
			int cursor_r = (uint8_t)(cursor_pixel >> 16);
			int cursor_g = (uint8_t)(cursor_pixel >> 8);
			int cursor_b = (uint8_t)(cursor_pixel >> 0);
			if (cursor_a == 255) {
				image_pixel[r_offset] = cursor_r;
				image_pixel[g_offset] = cursor_g;
				image_pixel[b_offset] = cursor_b;
			} else {
				image_pixel[r_offset] = (image_pixel[r_offset] * (255 - cursor_a) + 127) / 255 + cursor_r;
				image_pixel[g_offset] = (image_pixel[g_offset] * (255 - cursor_a) + 127) / 255 + cursor_g;
				image_pixel[b_offset] = (image_pixel[b_offset] * (255 - cursor_a) + 127) / 255 + cursor_b;
			}
		}
	}

	// free the cursor
	XFree(xcim);
#endif
}
#endif

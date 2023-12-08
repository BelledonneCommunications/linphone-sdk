/*
 * Copyright (c) 2010-2024 Belledonne Communications SARL.
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

#ifndef msscreensharing_h
#define msscreensharing_h

#include "mediastreamer2/msfilter.h"

/**
 * @file msscreensharing.h
 * @brief mediastreamer2 msscreensharing.h include file
 *
 * This file provide the API needed for MSScreenSharing
 */

/**
 * Screen sharing types.
 **/
enum _MSScreenSharingType {
	MS_SCREEN_SHARING_EMPTY = 0,       /**< The screen sharing is not set*/
	MS_SCREEN_SHARING_DISPLAY = 1,     /**< The screen sharing is done from a display*/
	MS_SCREEN_SHARING_WINDOW = 1 << 1, /**< The screen sharing is done from a window */
	MS_SCREEN_SHARING_AREA = 1 << 2,   /**< The screen sharing is done from an area */
};

/**
 * Filter's flags controlling special behaviours.
 **/
typedef enum _MSScreenSharingType MSScreenSharingType;

struct _MSScreenSharingDesc {
	MSScreenSharingType type; /**<  */
	void *native_data;        /**< Native data that depend of type.
	                           * 1. #MS_SCREEN_SHARING_DISPLAY
	                           * - Linux : <uintptr_t> The index of the screen ordered by XineramaQueryScreens.
	                           * - Mac : <CGDirectDisplayID> The display identification that can be retrieved from
	                           * SCShareableContent.
	                           * - Windows : <uintptr_t> The index of the screen ordered by IDXGIAdapter->EnumOutputs.
	                           * 2. #MS_SCREEN_SHARING_WINDOW
	                           * - Linux : <Window> The Window object that can be retrieved from XQueryPointer.
	                           * - Mac : <CGWindowID> The window identification that can be retrieved from NSEvent.
	                           * - Windows : <HWND> The window handle that can be retrived from WindowFromPoint.
	                           * 3. #MS_SCREEN_SHARING_AREA
	                           * - not yet supported.*/
};

/**
 * Structure for screen sharing description.
 * @var MSScreenSharingDesc
 */
typedef struct _MSScreenSharingDesc MSScreenSharingDesc;

/**returns a video source descriptor where the video is capture */
#define MS_SCREEN_SHARING_GET_SOURCE_DESCRIPTOR MS_FILTER_METHOD(MSFilterScreenSharingInterface, 0, MSScreenSharingDesc)
/**Sets a video source descriptor that specifiy where the video is to be captured */
#define MS_SCREEN_SHARING_SET_SOURCE_DESCRIPTOR MS_FILTER_METHOD(MSFilterScreenSharingInterface, 1, MSScreenSharingDesc)

#endif

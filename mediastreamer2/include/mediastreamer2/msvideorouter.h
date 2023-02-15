
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

#ifndef msvideorouter_h
#define msvideorouter_h

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideoswitcher.h"

#define ROUTER_MAX_CHANNELS 20
#define ROUTER_MAX_INPUT_CHANNELS ROUTER_MAX_CHANNELS + 2 /* 2 pin reserved for nowebcam and voidsource */
#define ROUTER_MAX_OUTPUT_CHANNELS ROUTER_MAX_CHANNELS *ROUTER_MAX_CHANNELS /* 1 pin reserved for voidsink */

typedef struct _MSVideoRouterPinData {
	int input;
	int output;
	int link_source;
} MSVideoRouterPinData;
#define MS_VIDEO_ROUTER_CONFIGURE_OUTPUT MS_FILTER_METHOD(MS_VIDEO_ROUTER_ID, 0, MSVideoRouterPinData)
#define MS_VIDEO_ROUTER_UNCONFIGURE_OUTPUT MS_FILTER_METHOD(MS_VIDEO_ROUTER_ID, 1, int)
#define MS_VIDEO_ROUTER_SET_FOCUS MS_FILTER_METHOD(MS_VIDEO_ROUTER_ID, 2, int)
#define MS_VIDEO_ROUTER_SET_AS_LOCAL_MEMBER MS_FILTER_METHOD(MS_VIDEO_ROUTER_ID, 3, MSVideoConferenceFilterPinControl)

/* Set the input pin connected to a video placeholder generator */
#define MS_VIDEO_ROUTER_SET_PLACEHOLDER MS_FILTER_METHOD(MS_VIDEO_ROUTER_ID, 4, int)

#define MS_VIDEO_ROUTER_NOTIFY_PLI MS_FILTER_METHOD(MS_VIDEO_ROUTER_ID, 5, int)
#define MS_VIDEO_ROUTER_NOTIFY_FIR MS_FILTER_METHOD(MS_VIDEO_ROUTER_ID, 6, int)

/**Events raised by the router when it needs to receive a key frame in order to complete the route to new input source*/
#define MS_VIDEO_ROUTER_SEND_FIR MS_FILTER_EVENT(MS_VIDEO_ROUTER_ID, 0, int)
#define MS_VIDEO_ROUTER_SEND_PLI MS_FILTER_EVENT(MS_VIDEO_ROUTER_ID, 1, int)

/* Event emitted when an output starts flowing data from a new input */
typedef struct _MSVideoRouterSwitchedEventData {
	int output; /*< The output pin that started flowing packets */
	int input;  /*< The input pin from which packets are taken */
} MSVideoRouterSwitchedEventData;
#define MS_VIDEO_ROUTER_OUTPUT_SWITCHED MS_FILTER_EVENT(MS_VIDEO_ROUTER_ID, 2, MSVideoRouterSwitchedEventData)

#endif

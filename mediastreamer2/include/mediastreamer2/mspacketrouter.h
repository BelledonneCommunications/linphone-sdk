
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

#ifndef mspacketrouter_h
#define mspacketrouter_h

#include "mediastreamer2/msfilter.h"

typedef enum { MS_PACKET_ROUTER_MODE_AUDIO, MS_PACKET_ROUTER_MODE_VIDEO } MSPacketRouterMode;

#define ROUTER_MAX_INPUT_CHANNELS 1024  /* 1 pin reserved for voidsource in video mode */
#define ROUTER_MAX_OUTPUT_CHANNELS 1024 /* 1 pin reserved for voidsink in video mode */

typedef struct _MSPacketRouterPinData {
	int input;
	int output;
	int self;
	int active_speaker_enabled;
	int extension_ids[16];
} MSPacketRouterPinData;

typedef struct _MSPacketRouterPinControl {
	int pin;
	int enabled;
} MSPacketRouterPinControl;

#define MS_PACKET_ROUTER_SET_ROUTING_MODE MS_FILTER_METHOD(MS_PACKET_ROUTER_ID, 0, MSPacketRouterMode)
#define MS_PACKET_ROUTER_SET_FULL_PACKET_MODE_ENABLED MS_FILTER_METHOD(MS_PACKET_ROUTER_ID, 1, bool_t)
#define MS_PACKET_ROUTER_GET_FULL_PACKET_MODE_ENABLED MS_FILTER_METHOD(MS_PACKET_ROUTER_ID, 9, bool_t)

#define MS_PACKET_ROUTER_CONFIGURE_OUTPUT MS_FILTER_METHOD(MS_PACKET_ROUTER_ID, 2, MSPacketRouterPinData)
#define MS_PACKET_ROUTER_UNCONFIGURE_OUTPUT MS_FILTER_METHOD(MS_PACKET_ROUTER_ID, 3, int)
#define MS_PACKET_ROUTER_SET_AS_LOCAL_MEMBER MS_FILTER_METHOD(MS_PACKET_ROUTER_ID, 4, MSPacketRouterPinControl)

#define MS_PACKET_ROUTER_GET_ACTIVE_SPEAKER_PIN MS_FILTER_METHOD(MS_PACKET_ROUTER_ID, 8, int)

#ifdef VIDEO_ENABLED
#define MS_PACKET_ROUTER_SET_FOCUS MS_FILTER_METHOD(MS_PACKET_ROUTER_ID, 5, int)
#define MS_PACKET_ROUTER_NOTIFY_PLI MS_FILTER_METHOD(MS_PACKET_ROUTER_ID, 6, int)
#define MS_PACKET_ROUTER_NOTIFY_FIR MS_FILTER_METHOD(MS_PACKET_ROUTER_ID, 7, int)

// Events raised by the router when it needs to receive a key frame in order to complete the route to new input source
#define MS_PACKET_ROUTER_SEND_FIR MS_FILTER_EVENT(MS_PACKET_ROUTER_ID, 0, int)
#define MS_PACKET_ROUTER_SEND_PLI MS_FILTER_EVENT(MS_PACKET_ROUTER_ID, 1, int)

// Event emitted when an output starts flowing data from a new input
typedef struct _MSPacketRouterSwitchedEventData {
	int output; /*< The output pin that started flowing packets */
	int input;  /*< The input pin from which packets are taken */
} MSPacketRouterSwitchedEventData;
#define MS_PACKET_ROUTER_OUTPUT_SWITCHED MS_FILTER_EVENT(MS_PACKET_ROUTER_ID, 2, MSPacketRouterSwitchedEventData)
#endif

#endif

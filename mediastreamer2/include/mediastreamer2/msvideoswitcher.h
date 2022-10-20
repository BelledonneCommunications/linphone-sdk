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


#ifndef msvideoswitcher_h
#define msvideoswitcher_h

#include "mediastreamer2/msfilter.h"

/**requests the video switcher to broadcast the video from given input pin.*/
#define MS_VIDEO_SWITCHER_SET_FOCUS MS_FILTER_METHOD(MS_VIDEO_SWITCHER_ID,0,int)

typedef struct _MSVideoConferenceFilterPinControl{
	int pin;
	int enabled;
}MSVideoConferenceFilterPinControl;
#define MS_VIDEO_SWITCHER_SET_AS_LOCAL_MEMBER MS_FILTER_METHOD(MS_VIDEO_SWITCHER_ID,1,MSVideoConferenceFilterPinControl)

#define MS_VIDEO_SWITCHER_NOTIFY_PLI MS_FILTER_EVENT(MS_VIDEO_SWITCHER_ID,2,int)
#define MS_VIDEO_SWITCHER_NOTIFY_FIR MS_FILTER_EVENT(MS_VIDEO_SWITCHER_ID,3,int)

/**Events raised by the switcher when it needs to receive a key frame in order to complete the switch to new input source*/
#define MS_VIDEO_SWITCHER_SEND_FIR MS_FILTER_EVENT(MS_VIDEO_SWITCHER_ID,0,int)
#define MS_VIDEO_SWITCHER_SEND_PLI MS_FILTER_EVENT(MS_VIDEO_SWITCHER_ID,1,int)

#endif


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

#ifndef msframemarking_tester_h
#define msframemarking_tester_h

#include "mediastreamer2/msfilter.h"

extern MSFilterDesc ms_framemarking_tester_desc;

typedef void (*MSFrameMarkingTesterCb)(MSFilter *filter, uint8_t marker, void* user_data);

struct _MSFrameMarkingTesterCbData {
	MSFrameMarkingTesterCb cb;
	void *user_data;
};

typedef struct _MSFrameMarkingTesterCbData MSFrameMarkingTesterCbData;

#define MS_FRAMEMARKING_TESTER_SET_CALLBACK MS_FILTER_METHOD(MS_FRAMEMARKING_TESTER_ID, 0, MSFrameMarkingTesterCbData)

#endif
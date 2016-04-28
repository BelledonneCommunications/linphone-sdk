/*
 mediastreamer2 library - modular sound and video processing and streaming
 Copyright (C) 2006  Simon MORLAT (simon.morlat@linphone.org)

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

#ifndef MS_IFRAME_REQUESTS_LIMITER_H
#define MS_IFRAME_REQUESTS_LIMITER_H

#include <mediastreamer2/msticker.h>

typedef struct _MSIFrameRequestsLimiterCtx {
	const MSTicker *ticker;
	bool_t iframe_required;
	uint64_t last_sent_iframe_time;
	int min_iframe_interval;
} MSIFrameRequestsLimiterCtx;

MS2_PUBLIC void ms_iframe_requests_limiter_init(MSIFrameRequestsLimiterCtx *obj, const MSTicker *t, int min_iframe_interval);

MS2_PUBLIC void ms_iframe_requests_limiter_require_iframe(MSIFrameRequestsLimiterCtx *obj);

MS2_PUBLIC bool_t ms_iframe_requests_limiter_iframe_sending_authorized(const MSIFrameRequestsLimiterCtx *obj);

MS2_PUBLIC void ms_iframe_requests_limiter_notify_iframe_sent(MSIFrameRequestsLimiterCtx *obj);

#endif

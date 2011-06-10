/*
mediastreamer2 library - modular sound and video processing and streaming

 * Copyright (C) 2011  Belledonne Communications, Grenoble, France

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

#ifndef ms2_qualityindicator_h
#define ms2_qualityindicator_h

#include "mediastreamer2/mscommon.h"

typedef struct _MSQualityIndicator MSQualityIndicator;

#ifdef __cplusplus
extern "C"{
#endif

MSQualityIndicator *ms_quality_indicator_new(RtpSession *session);

float ms_quality_indicator_get_rating(MSQualityIndicator *qi);

float ms_quality_indicator_get_average_rating(MSQualityIndicator *qi);

void ms_quality_indicator_update_from_feedback(MSQualityIndicator *qi, mblk_t *rtcp);

void ms_quality_indicator_update_local(MSQualityIndicator *qi);

void ms_quality_indicator_destroy(MSQualityIndicator *qi);

#ifdef __cplusplus
}
#endif


#endif


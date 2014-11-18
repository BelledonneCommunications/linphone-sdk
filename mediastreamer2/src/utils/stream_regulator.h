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

#ifndef STREAM_REGULATOR_H
#define STREAM_REGULATOR_H

#include "ortp/str_utils.h"
#include "mediastreamer2/msticker.h"

typedef struct _MSStreamRegulator MSStreamRegulator;

extern MSStreamRegulator *ms_stream_regulator_new(MSTicker *ticker, int64_t clock_rate);
extern void ms_stream_regulator_free(MSStreamRegulator *obj);
extern void ms_stream_regulator_push(MSStreamRegulator *obj, mblk_t *pkt);
extern mblk_t *ms_stream_regulator_get(MSStreamRegulator *obj);
extern void ms_stream_regulator_reset(MSStreamRegulator *obj);

#endif

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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "stream_regulator.h"

struct _MSStreamRegulator {
	MSTicker *ticker;
	int64_t clock_rate;
	int64_t t_origin;
	bool_t origin_set;
	MSQueue queue;
};

MSStreamRegulator *ms_stream_regulator_new(MSTicker *ticker, int64_t clock_rate) {
	MSStreamRegulator *obj = (MSStreamRegulator *)ms_new0(MSStreamRegulator, 1);
	obj->ticker = ticker;
	obj->clock_rate = clock_rate;
	ms_queue_init(&obj->queue);
	return obj;
}

void ms_stream_regulator_free(MSStreamRegulator *obj) {
	ms_queue_flush(&obj->queue);
	ms_free(obj);
}

void ms_stream_regulator_push(MSStreamRegulator *obj, mblk_t *pkt) {
	ms_queue_put(&obj->queue, pkt);
}

mblk_t *ms_stream_regulator_get(MSStreamRegulator *obj) {
	if(ms_queue_empty(&obj->queue)) return NULL;
	if(!obj->origin_set) {
		mblk_t *pkt = ms_queue_get(&obj->queue);
		obj->t_origin = obj->ticker->time - (int64_t)(mblk_get_timestamp_info(pkt)) * 1000LL / obj->clock_rate;
		obj->origin_set = TRUE;
		return pkt;
	} else {
		mblk_t *pkt = ms_queue_peek_first(&obj->queue);
		uint64_t timestamp = (uint64_t)(mblk_get_timestamp_info(pkt)) * 1000LL / obj->clock_rate;
		if(timestamp <= obj->ticker->time - obj->t_origin) {
			return ms_queue_get(&obj->queue);
		} else {
			return NULL;
		}
	}
}

void ms_stream_regulator_reset(MSStreamRegulator *obj) {
	ms_queue_flush(&obj->queue);
	obj->origin_set = FALSE;
}

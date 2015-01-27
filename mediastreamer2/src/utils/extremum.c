/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2014 Belledonne Communications SARL
Author: Simon MORLAT (simon.morlat@linphone.org)

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

#include "mediastreamer2/msutils.h"

void ms_extremum_reset(MSExtremum *obj){
	obj->current_extremum=0;
	obj->extremum_time=(uint64_t)-1;
	obj->last_stable=0;
}

void ms_extremum_init(MSExtremum *obj, int period){
	ms_extremum_reset(obj);
	obj->period=period;
}

static void extremum_set_new(MSExtremum *obj, const char* kind){
	obj->last_stable=obj->current_extremum;
	/*ms_message("New %s value : %f",kind,obj->last_stable);*/
}

static void extremum_check_init(MSExtremum *obj, uint64_t curtime, float value, const char *kind){
	if (obj->extremum_time!=(uint64_t)-1){
		if (curtime-obj->extremum_time>obj->period){
			/*last extremum is too old, drop it*/
			extremum_set_new(obj,kind);
			obj->extremum_time=(uint64_t)-1;
		}
	}
	if (obj->extremum_time==(uint64_t)-1){
		obj->current_extremum=value;
		obj->extremum_time=curtime;
	}
}

void ms_extremum_record_min(MSExtremum *obj, uint64_t curtime, float value){
	extremum_check_init(obj,curtime,value,"min");
	if (value<obj->current_extremum){
		obj->current_extremum=value;
		obj->extremum_time=curtime;
		if (value<obj->last_stable){
			extremum_set_new(obj,"min");
		}
	}
}

void ms_extremum_record_max(MSExtremum *obj, uint64_t curtime, float value){
	extremum_check_init(obj,curtime,value,"max");
	if (value>obj->current_extremum){
		obj->current_extremum=value;
		obj->extremum_time=curtime;
		if (value>obj->last_stable){
			extremum_set_new(obj,"max");
		}
	}
}

float ms_extremum_get_current(MSExtremum *obj){
	return obj->last_stable;
}


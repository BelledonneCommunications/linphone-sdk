/*
 mediastreamer2 library - modular sound and video processing and streaming
 Copyright (C) 2012  Belledonne Communications, Grenoble, France
 
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

#ifndef msprivate_h
#define msprivate_h
#include "ortp/port.h"
typedef struct MSConcealerContext {
	uint64_t sample_time;
	int plc_count;
	unsigned long total_number_for_plc;
	unsigned int max_plc_count;
}MSConcealerContext;

void ms_concealer_context_init(MSConcealerContext* obj,unsigned int max_plc_count);
void ms_concealer_context_set_sampling_time(MSConcealerContext* obj,unsigned long value);
unsigned long ms_concealer_context_get_sampling_time(MSConcealerContext* obj);
unsigned long ms_concealer_context_get_total_number_of_plc(MSConcealerContext* obj);
/* return number of concelad packet since the begening of the concealement period or 0 if not needed*/
unsigned int ms_concealer_context_is_concealement_required(MSConcealerContext* obj,uint64_t current_time);


/*FEC API*/
typedef struct _MSRtpPayloadPickerContext MSRtpPayloadPickerContext;
typedef mblk_t* (*RtpPayloadPicker)(MSRtpPayloadPickerContext* context,unsigned int sequence_number); 
struct _MSRtpPayloadPickerContext {
	void* filter_graph_manager; /*I.E stream*/
	RtpPayloadPicker picker;
};

#endif
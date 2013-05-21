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

#ifndef mscodecutils_h
#define mscodecutils_h

#include "mediastreamer2/mscommon.h"

/**
 * Helper object for audio decoders to determine whether PLC (packet loss concealment is needed).
**/
typedef struct _MSConcealerContext MSConcealerContext;

/**
 * Creates a new concealer object.
 * @param max_plc_count the number of consecutive milliseconds of PLC allowed.
**/
MS2_PUBLIC MSConcealerContext* ms_concealer_context_new(unsigned int max_plc_count);
/**
 * Destroys a concealer object.
**/
MS2_PUBLIC void ms_concealer_context_destroy(MSConcealerContext* context);

/**
 * Returns 1 when PLC is needed, 0 otherwise.
 * @param obj the concealer object
 * @param current_time the current time in milliseconds, as pointed by f->ticker->time .
**/
MS2_PUBLIC unsigned int ms_concealer_context_is_concealement_required(MSConcealerContext* obj,uint64_t current_time);

/**
 * Call this function whenever you decoded a packet, for true or in PLC mode, to inform the concealer
 * of how the audio stream is going.
 * @param obj the concealer object
 * @param current_time the current time in milliseconds, as pointed by f->ticker->time.
 * @param time_increment the number of milliseconds of audio decoded.
 * @param got_packet set to 1 if a real frame was decoded, 0 if it was a PLC frame.
 * @returns if a PLC period terminates, returns the duration of this PLC period in milliseconds, 0 otherwise.
**/
MS2_PUBLIC int ms_concealer_inc_sample_time(MSConcealerContext* obj, uint64_t current_time, int time_increment, int got_packet);


MS2_PUBLIC unsigned long ms_concealer_context_get_total_number_of_plc(MSConcealerContext* obj);


/**
 * Helper object for audio decoders to determine whether PLC (packet loss concealment is needed), based on timestamp information.
**/
typedef struct _MSConcealerTsContext MSConcealerTsContext;

/**
 * Creates a new concealer object.
 * @param max_plc_count maximum duration of PLC allowed, expressed in timestamp units.
**/
MS2_PUBLIC MSConcealerTsContext* ms_concealer_ts_context_new(unsigned int max_plc_ts);
/**
 * Destroys a concealer object.
**/
MS2_PUBLIC void ms_concealer_ts_context_destroy(MSConcealerTsContext* context);

/**
 * Returns 1 when PLC is needed, 0 otherwise.
 * @param obj the concealer object
 * @param current_ts the current time converted in timestamp units, usually (f->ticker->time*clock_rate)/1000 .
**/
MS2_PUBLIC unsigned int ms_concealer_ts_context_is_concealement_required(MSConcealerTsContext* obj,uint64_t current_ts);

/**
 * Call this function whenever you decoded a packet, for true or in PLC mode, to inform the concealer
 * of how the audio stream is going.
 * @param obj the concealer object
 * @param current_ts the current time converted in timestamp units, usually (f->ticker->time*clock_rate)/1000 
 * @param ts_increment the duration of audio decoded expressed in timestamp units
 * @param got_packet set to 1 if a real frame was decoded, 0 if it was a PLC frame.
 * @returns if a PLC period terminates, returns the duration of this PLC period in timestamp units, 0 otherwise.
**/
MS2_PUBLIC int ms_concealer_ts_context_inc_sample_ts(MSConcealerTsContext* obj, uint64_t current_ts, int ts_increment, int got_packet);


MS2_PUBLIC unsigned long ms_concealer_ts_context_get_total_number_of_plc(MSConcealerTsContext* obj);


/*FEC API*/
typedef struct _MSRtpPayloadPickerContext MSRtpPayloadPickerContext;
typedef mblk_t* (*RtpPayloadPicker)(MSRtpPayloadPickerContext* context,unsigned int sequence_number); 
struct _MSRtpPayloadPickerContext {
	void* filter_graph_manager; /*I.E stream*/
	RtpPayloadPicker picker;
};

#endif

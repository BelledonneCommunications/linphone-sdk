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
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef mscodecutils_h
#define mscodecutils_h

#include "mediastreamer2/mscommon.h"

#ifdef __cplusplus
extern "C"{
#endif

/**
 * Helper object for audio decoders to determine whether PLC (packet loss concealment is needed).
**/
typedef struct _MSConcealerContext MSConcealerContext;

/**
 * Creates a new concealer object.
 * @param max_plc_count the number of consecutive milliseconds of PLC allowed.
**/
MS2_PUBLIC MSConcealerContext* ms_concealer_context_new(uint32_t max_plc_count);
/**
 * Destroys a concealer object.
**/
MS2_PUBLIC void ms_concealer_context_destroy(MSConcealerContext* context);

/**
 * Returns 1 when PLC is needed, 0 otherwise.
 * @param obj the concealer object
 * @param current_time the current time in milliseconds, as pointed by f->ticker->time .
**/
MS2_PUBLIC unsigned int ms_concealer_context_is_concealement_required(MSConcealerContext* obj, uint64_t current_time);

/**
 * Call this function whenever you decoded a packet, for true or in PLC mode, to inform the concealer
 * of how the audio stream is going.
 * @param obj the concealer object
 * @param current_time the current time in milliseconds, as pointed by f->ticker->time.
 * @param time_increment the number of milliseconds of audio decoded.
 * @param got_packet set to TRUE if a real frame was decoded, FALSE if it was a PLC frame.
 * @return if a PLC period terminates, returns the duration of this PLC period in milliseconds, 0 otherwise.
**/
MS2_PUBLIC uint32_t ms_concealer_inc_sample_time(MSConcealerContext* obj, uint64_t current_time, uint32_t time_increment, bool_t got_packet);


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
 * @param got_packet set to TRUE if a real frame was decoded, FALSE if it was a PLC frame.
 * @return if a PLC period terminates, returns the duration of this PLC period in timestamp units, 0 otherwise.
**/
MS2_PUBLIC uint32_t ms_concealer_ts_context_inc_sample_ts(MSConcealerTsContext* obj, uint64_t current_ts, uint32_t ts_increment, bool_t got_packet);


MS2_PUBLIC unsigned long ms_concealer_ts_context_get_total_number_of_plc(MSConcealerTsContext* obj);


/*FEC API*/
typedef struct _MSRtpPayloadPickerContext MSRtpPayloadPickerContext;
typedef mblk_t* (*RtpPayloadPicker)(MSRtpPayloadPickerContext* context,unsigned int sequence_number);
struct _MSRtpPayloadPickerContext {
	void* filter_graph_manager; /*I.E stream*/
	RtpPayloadPicker picker;
};

struct _MSOfferAnswerContext;

#ifndef MS_OFFER_ANSWER_CONTEXT_DEFINED
#define MS_OFFER_ANSWER_CONTEXT_DEFINED
typedef struct _MSOfferAnswerContext MSOfferAnswerContext;
#endif

/* SDP offer answer payload matching API*/

/**
 * The MSPayloadMatcherFunc prototype takes:
 * - a list of local payload types
 * - a remote payload type (offered or answered) by remote to be matched agains payload types of the local payload type list.
 * - the full list of remote (offered or answered) payload types, which is sometimes necessary to do the matching in ambiguous situations.
 * - is_reading, a boolean indicating whether we are doing the match processing while reading a SDP response, or (if FALSE) to prepare a response to be sent.
 * The expected return value is a newly allocated PayloadType similar to the local payload type that was matched.
 * Due to specific per codec offer/answer logic, the fmtp of the payload type might be changed compared to the original local payload type.
 * If there is no match, NULL must be returned.
**/
typedef PayloadType * (*MSPayloadMatcherFunc)(MSOfferAnswerContext *context, const MSList *local_payloads, const PayloadType *remote_payload, const MSList *remote_payloads, bool_t is_reading);

/**
 * The MSOfferAnswerContext is only there to provide a context during the SDP offer/answer handshake.
 * It could be used in the future to provide extra information, for the moment the context is almost useless*/
struct _MSOfferAnswerContext{
	MSPayloadMatcherFunc match_payload;
	void (*destroy)(MSOfferAnswerContext *ctx);
	void *context_data;
};


/**
 * Executes an offer/answer processing for a given codec.
 * @param context the context
 * @param local_payloads the local payload type supported
 * @param remote_payload a remote payload type (offered or answered) by remote to be matched agains payload types of the local payload type list.
 * @param remote_payloads the full list of remote (offered or answered) payload types, which is sometimes necessary to do the matching in ambiguous situations.
 * @param is_reading, a boolean indicating whether we are doing the match processing while reading a SDP response, or (if FALSE) to prepare a response to be sent.
 * The expected return value is a newly allocated PayloadType similar to the local payload type that was matched.
 * Due to specific per codec offer/answer logic, the fmtp of the payload type might be changed compared to the original local payload type.
 * If there is no match, NULL must be returned.
**/
MS2_PUBLIC PayloadType * ms_offer_answer_context_match_payload(MSOfferAnswerContext *context, const MSList *local_payloads, const PayloadType *remote_payload, const MSList *remote_payloads, bool_t is_reading); 
MS2_PUBLIC void ms_offer_answer_context_destroy(MSOfferAnswerContext *ctx);

/**
 * A convenience function to instanciate an offer answer context giving only the payload matching function pointer.
**/
MS2_PUBLIC MSOfferAnswerContext *ms_offer_answer_create_simple_context(MSPayloadMatcherFunc func);
/**
 * The struct to declare offer-answer provider, that act as factories per mime type to instanciate MSOfferAnswerContext object able to take in charge
 * the offer answer model for a particular codec
**/
struct _MSOfferAnswerProvider{
	const char *mime_type;
	MSOfferAnswerContext *(*create_context)(void);
};

/**
 * A convenience structure and API to intellengently limit the number of key frame request of an encoder.
 **/
typedef struct _MSIFrameRequestsLimiterCtx {
	uint64_t last_sent_iframe_time;
	int min_iframe_interval;
	bool_t iframe_required;
} MSIFrameRequestsLimiterCtx;

MS2_PUBLIC void ms_iframe_requests_limiter_init(MSIFrameRequestsLimiterCtx *obj, int min_iframe_interval_ms);

MS2_PUBLIC void ms_iframe_requests_limiter_request_iframe(MSIFrameRequestsLimiterCtx *obj);

MS2_PUBLIC bool_t ms_iframe_requests_limiter_iframe_requested(const MSIFrameRequestsLimiterCtx *obj, uint64_t curtime_ms);

MS2_PUBLIC void ms_iframe_requests_limiter_notify_iframe_sent(MSIFrameRequestsLimiterCtx *obj, uint64_t curtime_ms);

/**
 * The goal of this small object is to tell when to send I frames at startup:
 * at 2 and 4 seconds.
 */


typedef struct MSVideoStarter {
	uint64_t next_time;
	int i_frame_count;
	bool_t active;
} MSVideoStarter;

MS2_PUBLIC void ms_video_starter_init(MSVideoStarter *vs);
MS2_PUBLIC void ms_video_starter_first_frame(MSVideoStarter *vs, uint64_t curtime);
MS2_PUBLIC bool_t ms_video_starter_need_i_frame(MSVideoStarter *vs, uint64_t curtime);
MS2_PUBLIC void ms_video_starter_deactivate(MSVideoStarter *vs);

#ifdef __cplusplus
}
#endif

#endif

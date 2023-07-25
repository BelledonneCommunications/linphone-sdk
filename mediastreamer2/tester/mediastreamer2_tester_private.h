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

#ifndef _MEDIASTREAMER2_TESTER_PRIVATE_H
#define _MEDIASTREAMER2_TESTER_PRIVATE_H

#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"

#ifdef _MSC_VER
#define unlink _unlink
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern MSTicker *ms_tester_ticker;
extern MSFilter *ms_tester_fileplay;
extern MSFilter *ms_tester_filerec;
extern MSFilter *ms_tester_dtmfgen;
extern MSFilter *ms_tester_tonedet;
extern MSFilter *ms_tester_voidsource;
extern MSFilter *ms_tester_voidsink;
extern MSFilter *ms_tester_encoder;
extern MSFilter *ms_tester_decoder;
extern MSFilter *ms_tester_rtprecv;
extern MSFilter *ms_tester_rtpsend;
extern MSFilter *ms_tester_resampler;
extern MSFilter *ms_tester_soundwrite;
extern MSFilter *ms_tester_soundread;
extern MSFilter *ms_tester_videocapture;
extern char *ms_tester_codec_mime;
extern char *ms_tester_plugin_location;

enum filter_mask_enum {
	FILTER_MASK_FILEPLAY = (1 << 0),
	FILTER_MASK_FILEREC = (1 << 1),
	FILTER_MASK_DTMFGEN = (1 << 2),
	FILTER_MASK_TONEDET = (1 << 3),
	FILTER_MASK_VOIDSOURCE = (1 << 4),
	FILTER_MASK_VOIDSINK = (1 << 5),
	FILTER_MASK_ENCODER = (1 << 6),
	FILTER_MASK_DECODER = (1 << 7),
	FILTER_MASK_RTPRECV = (1 << 8),
	FILTER_MASK_RTPSEND = (1 << 9),
	FILTER_MASK_RESAMPLER = (1 << 10),
	FILTER_MASK_SOUNDWRITE = (1 << 11),
	FILTER_MASK_SOUNDREAD = (1 << 12),
	FILTER_MASK_VIDEOCAPTURE = (1 << 13)
};

extern unsigned char ms_tester_tone_detected;

/*set payload type assignment here*/
#define PCMU8_PAYLOAD_TYPE 0
#define PCMA8_PAYLOAD_TYPE 8
#define H263_PAYLOAD_TYPE 34
#define VP8_PAYLOAD_TYPE 96
#define H264_PAYLOAD_TYPE 102
#define AV1_PAYLOAD_TYPE 103
#define MP4V_PAYLOAD_TYPE 104
#define UNSUPPORTED_PAYLOAD_TYPE 119
#define DUMMY_PAYLOAD_TYPE 120
#define OPUS_PAYLOAD_TYPE 121
#define SPEEX_PAYLOAD_TYPE 122
#define SPEEX16_PAYLOAD_TYPE 123
#define SILK_PAYLOAD_TYPE 124
#define SILK16_PAYLOAD_TYPE 125
#define BV16_PAYLOAD_TYPE 127

MSFactory *ms_tester_factory_new(void);

void ms_tester_create_ticker(void);
void ms_tester_destroy_ticker(void);
void ms_tester_create_filter(MSFilter **filter, MSFilterId id, MSFactory *f);
void ms_tester_create_filters(unsigned int filter_mask, MSFactory *f);
void ms_tester_destroy_filter(MSFilter **filter);
void ms_tester_destroy_filters(unsigned int filter_mask);
void ms_tester_tone_generation_loop(void);
void ms_tester_tone_detection_loop(void);
void ms_tester_tone_generation_and_detection_loop(void);
RtpProfile *ms_tester_create_rtp_profile(void);

typedef void (*ms_tester_iterate_cb)(MediaStream *ms, void *user_pointer);

bool_t wait_for_list(MSList *mss, int *counter, int value, int timeout_ms);
bool_t wait_for_list_with_parse_events(MSList *mss, int *counter, int value, int timeout_ms, MSList *cbs, MSList *ptrs);
bool_t wait_for_until(MediaStream *ms1, MediaStream *ms2, int *counter, int value, int timeout_ms);
bool_t wait_for_until_with_parse_events(MediaStream *ms1,
                                        MediaStream *ms2,
                                        int *counter,
                                        int value,
                                        int timeout_ms,
                                        ms_tester_iterate_cb cb1,
                                        void *ptr1,
                                        ms_tester_iterate_cb cb2,
                                        void *ptr2);
bool_t wait_for(MediaStream *ms1, MediaStream *ms2, int *counter, int value);

/* define local IP and ports used by all tests requiring it */
#define MARIELLE_IP "127.0.0.1"
#define MARGAUX_IP "127.0.0.1"
#define PAULINE_IP "127.0.0.1"
#define RELAY_IP "127.0.0.1"
#define MULTICAST_IP "224.1.2.3"

#define RECEIVER_IP "127.0.0.1"

/* This base port is randomly choosen at tester launch so several testerc*/
extern int base_port;
void ms_tester_set_random_port(void);
/* Then the following suites uses range (from base port)
 * - audio stream : 0-8
 * - jitter_buffer: 9-10
 * - double encryption 11 - 23
 * - adaptive algorithm 25 - 28
 */

/* generate a NULL terminated 6 characters random token */
void ms_tester_get_random_token(char token[7]);
/* generate a filename with a NULL terminated 6 characters random suffix - to be freed by caller */
char *ms_tester_get_random_filename(const char *basename, const char *suffix);
#ifdef __cplusplus
}
#endif

#endif /* _MEDIASTREAMER2_TESTER_PRIVATE_H */

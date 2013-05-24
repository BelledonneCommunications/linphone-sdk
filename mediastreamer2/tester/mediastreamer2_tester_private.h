/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006-2013 Belledonne Communications, Grenoble

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

#ifndef _MEDIASTREAMER2_TESTER_PRIVATE_H
#define _MEDIASTREAMER2_TESTER_PRIVATE_H


#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"


#if WINAPI_FAMILY_PHONE_APP
#define SOUND_FILE_PATH		"Assets\\Sounds\\"
#else
#define SOUND_FILE_PATH		"./sounds/"
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
extern char *ms_tester_codec_mime;

enum {
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
	FILTER_MASK_SOUNDREAD = (1 << 12)
} filter_mask_enum;


extern unsigned char ms_tester_tone_detected;


void ms_tester_create_ticker(void);
void ms_tester_destroy_ticker(void);
void ms_tester_create_filter(MSFilter **filter, MSFilterId id);
void ms_tester_create_filters(unsigned int filter_mask);
void ms_tester_destroy_filter(MSFilter **filter);
void ms_tester_destroy_filters(unsigned int filter_mask);
void ms_tester_tone_generation_loop(void);
void ms_tester_tone_detection_loop(void);
void ms_tester_tone_generation_and_detection_loop(void);



#endif /* _MEDIASTREAMER2_TESTER_PRIVATE_H */

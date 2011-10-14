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

#ifndef ms2_ratecontrol
#define ms2_ratecontrol

#include "mediastreamer2/msfilter.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Audio Bitrate controller object
**/

typedef struct _MSAudioBitrateController MSAudioBitrateController;

enum MSRateControlActionType{
	MSRateControlActionDoNothing,
	MSRateControlActionDecreaseBitrate,
	MSRateControlActionDecreasePacketRate,
	MSRateControlActionIncreaseQuality
};

typedef struct _MSRateControlAction{
	enum MSRateControlActionType type;
	int value;
}MSRateControlAction;

typedef struct _MSBitrateDriver MSBitrateDriver;
typedef struct _MSBitrateDriverDesc MSBitrateDriverDesc;

struct _MSBitrateDriverDesc{
	void (*execute_action)(MSBitrateDriver *obj, const MSRateControlAction *action);
	void (*uninit)(MSBitrateDriver *obj);
};

/*
 * The MSBitrateDriver has the responsability to execute rate control actions.
 * This is an abstract interface.
**/
struct _MSBitrateDriver{
	MSBitrateDriverDesc *desc;
};

void ms_bitrate_driver_execute_action(MSBitrateDriver *obj, const MSRateControlAction *action);
void ms_bitrate_driver_destroy(MSBitrateDriver *obj);


typedef struct _MSQosAnalyser MSQosAnalyser;
typedef struct _MSQosAnalyserDesc MSQosAnalyserDesc;

struct _MSQosAnalyserDesc{
	bool_t (*process_rtcp)(MSQosAnalyzer *obj, mblk_t *rtcp);
	void (*suggest_action)(MSQosAnalyzer *obj, MSRateControlAction *action);
	bool_t (*has_improved)(MSQosAnalyzer *obj);
	void uninit(MSQosAnalyzer);
};

/**
 * A MSQosAnalyzer is responsible to analyze RTCP feedback and suggest actions on bitrate or packet rate accordingly.
 * This is an abstract interface.
**/
struct _MSQosAnalyser{
	MSQosAnalyserDesc *desc;
};

/**
 * The simple qos analyzer is an implementation of MSQosAnalizer that performs analysis for single stream.
**/
MSQosAnalyzer * ms_simple_qos_analyser_new(RtpSession *session);

#define MS_AUDIO_RATE_CONTROL_OPTIMIZE_QUALITY (1)
#define MS_AUDIO_RATE_CONTROL_OPTIMIZE_LATENCY (1<<1)

MSAudioBitrateController *ms_audio_bitrate_controller_new(RtpSession *session, MSFilter *encoder, unsigned int flags);

void ms_audio_bitrate_controller_process_rtcp(MSAudioBitrateController *obj, mblk_t *rtcp);

void ms_audio_bitrate_controller_destroy(MSAudioBitrateController *obj); 


#ifdef __cplusplus
}
#endif

#endif



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
#include <ortp/ortp.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Audio Bitrate controller object
**/

typedef struct _MSAudioBitrateController MSAudioBitrateController;

enum _MSRateControlActionType{
	MSRateControlActionDoNothing,
	MSRateControlActionDecreaseBitrate,
	MSRateControlActionDecreasePacketRate,
	MSRateControlActionIncreaseQuality,
};

typedef enum _MSRateControlActionType MSRateControlActionType;

const char *ms_rate_control_action_type_name(MSRateControlActionType t);

typedef struct _MSRateControlAction{
	MSRateControlActionType type;
	int value;
}MSRateControlAction;

typedef struct _MSBitrateDriver MSBitrateDriver;
typedef struct _MSBitrateDriverDesc MSBitrateDriverDesc;

struct _MSBitrateDriverDesc{
	int (*execute_action)(MSBitrateDriver *obj, const MSRateControlAction *action);
	void (*uninit)(MSBitrateDriver *obj);
};

/*
 * The MSBitrateDriver has the responsability to execute rate control actions.
 * This is an abstract interface.
**/
struct _MSBitrateDriver{
	MSBitrateDriverDesc *desc;
	int refcnt;
};

int ms_bitrate_driver_execute_action(MSBitrateDriver *obj, const MSRateControlAction *action);
MSBitrateDriver * ms_bitrate_driver_ref(MSBitrateDriver *obj);
void ms_bitrate_driver_unref(MSBitrateDriver *obj);

MSBitrateDriver *ms_audio_bitrate_driver_new(MSFilter *encoder);
MSBitrateDriver *ms_av_bitrate_driver_new(MSFilter *a_encoder, MSFilter *venc);

typedef struct _MSQosAnalyser MSQosAnalyser;
typedef struct _MSQosAnalyserDesc MSQosAnalyserDesc;

struct _MSQosAnalyserDesc{
	bool_t (*process_rtcp)(MSQosAnalyser *obj, mblk_t *rtcp);
	void (*suggest_action)(MSQosAnalyser *obj, MSRateControlAction *action);
	bool_t (*has_improved)(MSQosAnalyser *obj);
	void (*uninit)(MSQosAnalyser *);
};

/**
 * A MSQosAnalyzer is responsible to analyze RTCP feedback and suggest actions on bitrate or packet rate accordingly.
 * This is an abstract interface.
**/
struct _MSQosAnalyser{
	MSQosAnalyserDesc *desc;
	int refcnt;
};

MSQosAnalyser * ms_qos_analyser_ref(MSQosAnalyser *obj);
void ms_qos_analyser_unref(MSQosAnalyser *obj);
void ms_qos_analyser_suggest_action(MSQosAnalyser *obj, MSRateControlAction *action);
bool_t ms_qos_analyser_has_improved(MSQosAnalyser *obj);
bool_t ms_qos_analyser_process_rtcp(MSQosAnalyser *obj, mblk_t *rtcp);

/**
 * The simple qos analyzer is an implementation of MSQosAnalyser that performs analysis for single stream.
**/
MSQosAnalyser * ms_simple_qos_analyser_new(RtpSession *session);

/**
 * The audio/video qos analyser is an implementation of MSQosAnalyser that performs analysis of two audio and video streams.
**/
MSQosAnalyser * ms_av_qos_analyser_new(RtpSession *asession, RtpSession *vsession);

/**
 * The MSBitrateController the overall behavior and state machine of the adaptive rate control system.
 * It requires a MSQosAnalyser to obtain analyse of the quality of service, and a MSBitrateDriver
 * to run the actions on media streams, like decreasing or increasing bitrate.
**/
typedef struct _MSBitrateController MSBitrateController;

/**
 * Instanciates MSBitrateController
 * @param qosanalyser a Qos analyser object
 * @param driver a bitrate driver object.
 * The newly created bitrate controller owns references to the analyser and the driver.
**/
MSBitrateController *ms_bitrate_controller_new(MSQosAnalyser *qosanalyser, MSBitrateDriver *driver);

/**
 * Asks the bitrate controller to process a newly received RTCP packet.
 * @param MSBitrateController the bitrate controller object.
 * @param rtcp an RTCP packet received for the media session(s) being managed by the controller.
 * If the RTCP packet contains useful feedback regarding quality of the media streams received by the far end,
 * then the bitrate controller may take decision and execute actions on the local media streams to adapt the
 * output bitrate.
**/
void ms_bitrate_controller_process_rtcp(MSBitrateController *obj, mblk_t *rtcp);

/**
 * Destroys the bitrate controller
 * 
 * If no other entity holds references to the underlyings MSQosAnalyser and MSBitrateDriver object,
 * then they will be destroyed too.
**/
void ms_bitrate_controller_destroy(MSBitrateController *obj);


/**
 * Convenience function to create a bitrate controller managing a single audio stream.
 * @param session the RtpSession object for the media stream
 * @param encoder the MSFilter object responsible for encoding the audio data.
 * @param flags unused.
 * This function actually calls internally:
 * <br>
 * \code
 * ms_bitrate_controller_new(ms_simple_qos_analyser_new(session),ms_audio_bitrate_driver_new(encoder));
 * \endcode
**/
MSBitrateController *ms_audio_bitrate_controller_new(RtpSession *session, MSFilter *encoder, unsigned int flags);

/**
 * Convenience fonction to create a bitrate controller managing a video and an audio stream.
 * @param vsession the video RtpSession
 * @param venc the video encoder
 * @param asession the audio RtpSession
 * @param aenc the audio encoder
 * This function actually calls internally:
 * <br>
 * \code
 * ms_bitrate_controller_new(ms_av_qos_analyser_new(asession,vsession),ms_av_bitrate_driver_new(aenc,venc));
 * \endcode
**/
MSBitrateController *ms_av_bitrate_controller_new(RtpSession *asession, MSFilter *aenc, RtpSession *vsession, MSFilter *venc);

#ifdef __cplusplus
}
#endif

#endif



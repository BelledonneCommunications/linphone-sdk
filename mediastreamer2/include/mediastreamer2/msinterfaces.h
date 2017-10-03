/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2010  Simon MORLAT (simon.morlat@linphone.org)

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

#ifndef msinterfaces_h
#define msinterfaces_h

#include "mediastreamer2/mscodecutils.h"

typedef struct _MSVideoCodecSLI MSVideoCodecSLI;

struct _MSVideoCodecSLI {
	uint16_t first;
	uint16_t number;
	uint8_t picture_id;
};

typedef struct _MSVideoCodecRPSI MSVideoCodecRPSI;

struct _MSVideoCodecRPSI {
	uint8_t *bit_string;
	uint16_t bit_string_len;
};

typedef struct _MSVideoEncoderPixFmt MSVideoEncoderPixFmt;

struct _MSVideoEncoderPixFmt {
	uint32_t pixfmt;
	bool_t supported;
};

/**
 * Interface definition for video display filters.
**/

typedef struct _MSVideoDisplayDecodingSupport MSVideoDisplayDecodingSupport;

struct _MSVideoDisplayDecodingSupport {
	const char *mime_type;	/**< Input parameter to asking if the display supports decoding of this mime type */
	bool_t supported;	/**< Output telling whether the display supports decoding to the specified mime type */
};

/** whether the video window should be resized to the stream's resolution*/
#define MS_VIDEO_DISPLAY_ENABLE_AUTOFIT \
	MS_FILTER_METHOD(MSFilterVideoDisplayInterface,0,int)

/**position of the local view */
#define MS_VIDEO_DISPLAY_SET_LOCAL_VIEW_MODE \
	MS_FILTER_METHOD(MSFilterVideoDisplayInterface,1,int)

/**whether the video should be reversed as in mirror */
#define MS_VIDEO_DISPLAY_ENABLE_MIRRORING \
	MS_FILTER_METHOD(MSFilterVideoDisplayInterface,2,int)

/**returns a platform dependant window id where the video is drawn */
#define MS_VIDEO_DISPLAY_GET_NATIVE_WINDOW_ID \
	MS_FILTER_METHOD(MSFilterVideoDisplayInterface,3,void*)


/**Sets an external native window id where the video is to be drawn */
#define MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID \
	MS_FILTER_METHOD(MSFilterVideoDisplayInterface,4,void*)


/**scale factor of the local view */
#define MS_VIDEO_DISPLAY_SET_LOCAL_VIEW_SCALEFACTOR \
	MS_FILTER_METHOD(MSFilterVideoDisplayInterface,5,float)

/**Set the background colour for video window */
#define MS_VIDEO_DISPLAY_SET_BACKGROUND_COLOR \
	MS_FILTER_METHOD(MSFilterVideoDisplayInterface,8,int[3])

/**Show video. Useful to free XV port */
#define MS_VIDEO_DISPLAY_SHOW_VIDEO \
	MS_FILTER_METHOD(MSFilterVideoDisplayInterface,9,int)

#define MS_VIDEO_DISPLAY_ZOOM \
	MS_FILTER_METHOD(MSFilterVideoDisplayInterface,10,int[4])

/**Specifiy device orientation from portrait */
#define MS_VIDEO_DISPLAY_SET_DEVICE_ORIENTATION \
   MS_FILTER_METHOD(MSFilterVideoDisplayInterface,11,int)

/**
  * Interface definitions for players
**/

enum _MSPlayerState{
	MSPlayerClosed,
	MSPlayerPaused,
	MSPlayerPlaying
};

typedef enum _MSPlayerState MSPlayerState;

/**open a media file*/
#define MS_PLAYER_OPEN \
	MS_FILTER_METHOD(MSFilterPlayerInterface,0,const char )

#define MS_PLAYER_START \
	MS_FILTER_METHOD_NO_ARG(MSFilterPlayerInterface,1)

#define MS_PLAYER_PAUSE \
	MS_FILTER_METHOD_NO_ARG(MSFilterPlayerInterface,2)

#define MS_PLAYER_CLOSE \
	MS_FILTER_METHOD_NO_ARG(MSFilterPlayerInterface,3)

#define MS_PLAYER_SEEK_MS \
	MS_FILTER_METHOD(MSFilterPlayerInterface,4,int)

#define MS_PLAYER_GET_STATE \
	MS_FILTER_METHOD(MSFilterPlayerInterface,5,MSPlayerState)

/**enable loop mode. Argument is a pause interval in milliseconds to be observed between end of play and resuming at start. A value of -1 disables loop mode*/
#define MS_PLAYER_SET_LOOP \
	MS_FILTER_METHOD(MSFilterPlayerInterface,6,int)

#define MS_PLAYER_GET_DURATION \
	MS_FILTER_METHOD(MSFilterPlayerInterface,7,int)

#define MS_PLAYER_GET_CURRENT_POSITION \
	MS_FILTER_METHOD(MSFilterPlayerInterface,8,int64_t)

#define MS_PLAYER_EOF \
	MS_FILTER_EVENT_NO_ARG(MSFilterPlayerInterface,0)


/**
  * Interface definitions for recorders
**/

enum _MSRecorderState{
	MSRecorderClosed,
	MSRecorderPaused,
	MSRecorderRunning
};

typedef enum _MSRecorderState MSRecorderState;

/**open a media file for recording*/
#define MS_RECORDER_OPEN \
	MS_FILTER_METHOD(MSFilterRecorderInterface,0,const char )

#define MS_RECORDER_START \
	MS_FILTER_METHOD_NO_ARG(MSFilterRecorderInterface,1)

#define MS_RECORDER_PAUSE \
	MS_FILTER_METHOD_NO_ARG(MSFilterRecorderInterface,2)

#define MS_RECORDER_CLOSE \
	MS_FILTER_METHOD_NO_ARG(MSFilterRecorderInterface,3)

#define MS_RECORDER_GET_STATE \
	MS_FILTER_METHOD(MSFilterRecorderInterface,5,MSRecorderState)

#define MS_RECORDER_NEEDS_FIR \
	MS_FILTER_EVENT_NO_ARG(MSFilterRecorderInterface,0)

#define MS_RECORDER_SET_MAX_SIZE \
	MS_FILTER_METHOD(MSFilterRecorderInterface,6,int)

#define MS_RECORDER_MAX_SIZE_REACHED \
	MS_FILTER_EVENT_NO_ARG(MSFilterRecorderInterface,1)


/** Interface definitions for echo cancellers */

/** sets the echo delay in milliseconds*/
#define MS_ECHO_CANCELLER_SET_DELAY \
	MS_FILTER_METHOD(MSFilterEchoCancellerInterface,0,int)

#define MS_ECHO_CANCELLER_SET_FRAMESIZE \
	MS_FILTER_METHOD(MSFilterEchoCancellerInterface,1,int)

/** sets tail length in milliseconds */
#define MS_ECHO_CANCELLER_SET_TAIL_LENGTH \
	MS_FILTER_METHOD(MSFilterEchoCancellerInterface,2,int)

/** put filter in bypass mode */
#define MS_ECHO_CANCELLER_SET_BYPASS_MODE \
	MS_FILTER_METHOD(MSFilterEchoCancellerInterface,3,bool_t)
/** get filter bypass mode */
#define MS_ECHO_CANCELLER_GET_BYPASS_MODE \
	MS_FILTER_METHOD(MSFilterEchoCancellerInterface,4,bool_t)

/** retrieve echo canceller internal state, as a base64 encoded string */
#define MS_ECHO_CANCELLER_GET_STATE_STRING \
	MS_FILTER_METHOD(MSFilterEchoCancellerInterface,5,char *)

/** restore a previous state suppling the echo canceller config as base64 encoded string */
#define MS_ECHO_CANCELLER_SET_STATE_STRING \
	MS_FILTER_METHOD(MSFilterEchoCancellerInterface,6, const char)



/** Event definitions for video decoders */
#define MS_VIDEO_DECODER_DECODING_ERRORS \
	MS_FILTER_EVENT_NO_ARG(MSFilterVideoDecoderInterface,0)
#define MS_VIDEO_DECODER_FIRST_IMAGE_DECODED \
	MS_FILTER_EVENT_NO_ARG(MSFilterVideoDecoderInterface,1)
#define MS_VIDEO_DECODER_SEND_PLI \
	MS_FILTER_EVENT_NO_ARG(MSFilterVideoDecoderInterface, 2)
#define MS_VIDEO_DECODER_SEND_SLI \
	MS_FILTER_EVENT(MSFilterVideoDecoderInterface, 3, MSVideoCodecSLI)
#define MS_VIDEO_DECODER_SEND_RPSI \
	MS_FILTER_EVENT(MSFilterVideoDecoderInterface, 4, MSVideoCodecRPSI)
#define MS_VIDEO_DECODER_SEND_FIR \
	MS_FILTER_EVENT_NO_ARG(MSFilterVideoDecoderInterface, 5)

/** Method definitions for video decoders */
#define MS_VIDEO_DECODER_RESET_FIRST_IMAGE_NOTIFICATION \
	MS_FILTER_METHOD_NO_ARG(MSFilterVideoDecoderInterface, 5)
#define MS_VIDEO_DECODER_ENABLE_AVPF \
	MS_FILTER_METHOD(MSFilterVideoDecoderInterface, 6, bool_t)
#define MS_VIDEO_DECODER_SUPPORT_RENDERING \
	MS_FILTER_METHOD(MSFilterVideoDecoderInterface, 7, MSVideoDisplayDecodingSupport)
#define MS_VIDEO_DECODER_FREEZE_ON_ERROR \
	MS_FILTER_METHOD(MSFilterVideoDecoderInterface, 8, bool_t)
#define MS_VIDEO_DECODER_RECOVERED_FROM_ERRORS \
	MS_FILTER_EVENT_NO_ARG(MSFilterVideoDecoderInterface, 9)
#define MS_VIDEO_DECODER_RESET \
	MS_FILTER_METHOD_NO_ARG(MSFilterVideoDecoderInterface, 10)
#define MS_VIDEO_DECODER_FREEZE_ON_ERROR_ENABLED \
	MS_FILTER_METHOD(MSFilterVideoDecoderInterface, 11, bool_t)



/** Interface definitions for video capture */
#define MS_VIDEO_CAPTURE_SET_DEVICE_ORIENTATION \
	MS_FILTER_METHOD(MSFilterVideoCaptureInterface,0,int)
#define MS_VIDEO_CAPTURE_GET_CAMERA_SENSOR_ROTATION \
	MS_FILTER_METHOD(MSFilterVideoCaptureInterface, 1, int)

/** Interface definitions for audio decoder */

#define MS_AUDIO_DECODER_HAVE_PLC \
	MS_FILTER_METHOD(MSFilterAudioDecoderInterface,0,int)

#define MS_DECODER_HAVE_PLC MS_AUDIO_DECODER_HAVE_PLC /*for backward compatibility*/

#define MS_AUDIO_DECODER_SET_RTP_PAYLOAD_PICKER \
	MS_FILTER_METHOD(MSFilterAudioDecoderInterface,1,MSRtpPayloadPickerContext)

#define MS_DECODER_ENABLE_PLC MS_FILTER_METHOD(MSFilterAudioDecoderInterface,2,int)
/**
 * Interface definition for video encoders.
**/

#define MS_VIDEO_ENCODER_SUPPORTS_PIXFMT \
	MS_FILTER_METHOD(MSFilterVideoEncoderInterface, 0, MSVideoEncoderPixFmt)
/* request a video-fast-update (=I frame for H263,MP4V-ES) to a video encoder*/
#define MS_VIDEO_ENCODER_REQ_VFU \
	MS_FILTER_METHOD_NO_ARG(MSFilterVideoEncoderInterface, 1)
#define MS_VIDEO_ENCODER_GET_CONFIGURATION_LIST \
	MS_FILTER_METHOD(MSFilterVideoEncoderInterface, 2, const MSVideoConfiguration *)
#define MS_VIDEO_ENCODER_SET_CONFIGURATION \
	MS_FILTER_METHOD(MSFilterVideoEncoderInterface, 3, const MSVideoConfiguration )
#define MS_VIDEO_ENCODER_NOTIFY_PLI \
	MS_FILTER_METHOD_NO_ARG(MSFilterVideoEncoderInterface, 4)
#define MS_VIDEO_ENCODER_NOTIFY_FIR \
	MS_FILTER_METHOD(MSFilterVideoEncoderInterface, 5, uint8_t *)
#define MS_VIDEO_ENCODER_NOTIFY_SLI \
	MS_FILTER_METHOD(MSFilterVideoEncoderInterface, 6, MSVideoCodecSLI)
#define MS_VIDEO_ENCODER_NOTIFY_RPSI \
	MS_FILTER_METHOD(MSFilterVideoEncoderInterface, 7, MSVideoCodecRPSI)
#define MS_VIDEO_ENCODER_ENABLE_AVPF \
	MS_FILTER_METHOD(MSFilterVideoEncoderInterface, 8, bool_t)
#define MS_VIDEO_ENCODER_SET_CONFIGURATION_LIST \
	MS_FILTER_METHOD(MSFilterVideoEncoderInterface, 9, const MSVideoConfiguration *)
#define MS_VIDEO_ENCODER_IS_HARDWARE_ACCELERATED \
	MS_FILTER_METHOD(MSFilterVideoEncoderInterface, 10, bool_t)

/** Interface definitions for audio capture */
/* Start numbering from the end for hacks */
#define MS_AUDIO_CAPTURE_SET_VOLUME_GAIN \
	MS_FILTER_METHOD(MSFilterAudioCaptureInterface, 0, float)
#define MS_AUDIO_CAPTURE_GET_VOLUME_GAIN \
	MS_FILTER_METHOD(MSFilterAudioCaptureInterface, 1, float)
#define MS_AUDIO_CAPTURE_FORCE_SPEAKER_STATE \
	MS_FILTER_METHOD(MSFilterAudioCaptureInterface, 255, bool_t)

/** Interface definitions for audio playback */
enum _MSAudioRoute{
	MSAudioRouteEarpiece,
	MSAudioRouteSpeaker
};
typedef enum _MSAudioRoute MSAudioRoute;

#define MS_AUDIO_PLAYBACK_SET_VOLUME_GAIN \
	MS_FILTER_METHOD(MSFilterAudioPlaybackInterface, 0, float)
#define MS_AUDIO_PLAYBACK_GET_VOLUME_GAIN \
	MS_FILTER_METHOD(MSFilterAudioPlaybackInterface, 1, float)
#define MS_AUDIO_PLAYBACK_SET_ROUTE \
	MS_FILTER_METHOD(MSFilterAudioPlaybackInterface, 2, MSAudioRoute)
#define MS_AUDIO_PLAYBACK_MUTE \
	MS_FILTER_METHOD(MSFilterAudioPlaybackInterface, 3, int)

/** Interface definitions for audio encoder */
#define MS_AUDIO_ENCODER_SET_PTIME \
	MS_FILTER_METHOD(MSFilterAudioEncoderInterface,0,int)

#define MS_AUDIO_ENCODER_GET_PTIME \
	MS_FILTER_METHOD(MSFilterAudioEncoderInterface,1,int)

/* Enable encoder's builtin forward error correction, if available*/
#define MS_AUDIO_ENCODER_ENABLE_FEC \
	MS_FILTER_METHOD(MSFilterAudioEncoderInterface,2,int)

/* Set the packet loss percentage reported, so that encoder may compensate if forward-correction is enabled and implemented.*/
#define MS_AUDIO_ENCODER_SET_PACKET_LOSS \
	MS_FILTER_METHOD(MSFilterAudioEncoderInterface,3,int)

#define MS_AUDIO_ENCODER_CAP_AUTO_PTIME (1)

#define MS_AUDIO_ENCODER_GET_CAPABILITIES \
	MS_FILTER_METHOD(MSFilterAudioEncoderInterface,4,int)

/** Interface definitions for void source/sink */
#define MS_VOID_SOURCE_SEND_SILENCE \
	MS_FILTER_METHOD(MSFilterVoidInterface, 0, bool_t)

#endif

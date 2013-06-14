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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef msinterfaces_h
#define msinterfaces_h

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
	MS_FILTER_METHOD(MSFilterVideoDisplayInterface,3,long)


/**Sets an external native window id where the video is to be drawn */
#define MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID \
	MS_FILTER_METHOD(MSFilterVideoDisplayInterface,4,long)


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

#define MS_VIDEO_DISPLAY_SUPPORT_DECODING \
	MS_FILTER_METHOD(MSFilterVideoDisplayInterface, 12, MSVideoDisplayDecodingSupport*)

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
	MS_FILTER_METHOD(MSFilterEchoCancellerInterface,5,char **)

/** restore a previous state suppling the echo canceller config as base64 encoded string */
#define MS_ECHO_CANCELLER_SET_STATE_STRING \
	MS_FILTER_METHOD(MSFilterEchoCancellerInterface,6, const char *)



/** Interface definitions for video decoders */
#define MS_VIDEO_DECODER_DECODING_ERRORS \
		MS_FILTER_EVENT_NO_ARG(MSFilterVideoDecoderInterface,0)
#define MS_VIDEO_DECODER_FIRST_IMAGE_DECODED \
        MS_FILTER_EVENT_NO_ARG(MSFilterVideoDecoderInterface,1)
#define MS_VIDEO_DECODER_RESET_FIRST_IMAGE_NOTIFICATION \
    MS_FILTER_METHOD_NO_ARG(MSFilterVideoDecoderInterface, 0)

/** Interface definitions for video capture */
#define MS_VIDEO_CAPTURE_SET_DEVICE_ORIENTATION \
	MS_FILTER_METHOD(MSFilterVideoCaptureInterface,0,int)
#define MS_VIDEO_CAPTURE_GET_CAMERA_SENSOR_ROTATION \
	MS_FILTER_METHOD(MSFilterVideoCaptureInterface, 1, int)

/** Interface definitions for audio decoder */

#define MS_AUDIO_DECODER_HAVE_PLC \
	MS_FILTER_METHOD(MSFilterAudioDecoderInterface,0,int)
	
#define MS_DECODER_HAVE_PLC MS_AUDIO_DECODER_HAVE_PLC /*for backward compatibility*/

/** Interface definitions for video encoders */
#define MS_VIDEO_ENCODER_HAS_BUILTIN_CONVERTER \
	MS_FILTER_METHOD(MSFilterVideoEncoderInterface, 0, bool_t)
/* request a video-fast-update (=I frame for H263,MP4V-ES) to a video encoder*/
#define MS_VIDEO_ENCODER_REQ_VFU \
	MS_FILTER_METHOD_NO_ARG(MSFilterVideoEncoderInterface, 1)

/** Interface definitions for audio capture */
/* Start numbering from the end for hacks */
#define MS_AUDIO_CAPTURE_FORCE_SPEAKER_STATE \
	MS_FILTER_METHOD(MSFilterAudioCaptureInterface, 255, bool_t)

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
#endif

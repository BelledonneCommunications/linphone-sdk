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
	MS_FILTER_METHOD(MSFilterPlayerInterface,0,const char *)

#define MS_PLAYER_START \
	MS_FILTER_METHOD_NO_ARG(MSFilterPlayerInterface,1)

#define MS_PLAYER_PAUSE \
	MS_FILTER_METHOD_NO_ARG(MSFilterPlayerInterface,2)

#define MS_PLAYER_CLOSE \
	MS_FILTER_METHOD_NO_ARG(MSFilterPlayerInterface,3)

#define MS_PLAYER_SEEK_MS \
	MS_FILTER_METHOD(MSFilterPlayerInterface,4,int)

#define MS_PLAYER_GET_STATE \
	MS_FILTER_METHOD(MSFilterPlayerInterface,5,int)


/** Interface definitions for echo cancellers*/

/** sets the echo delay in milliseconds*/
#define MS_ECHO_CANCELLER_SET_DELAY \
	MS_FILTER_METHOD(MSFilterEchoCancellerInterface,0,int)

#define MS_ECHO_CANCELLER_SET_FRAMESIZE \
	MS_FILTER_METHOD(MSFilterEchoCancellerInterface,1,int)

/** sets tail length in milliseconds */
#define MS_ECHO_CANCELLER_SET_TAIL_LENGTH \
	MS_FILTER_METHOD(MSFilterEchoCancellerInterface,2,int)

#endif

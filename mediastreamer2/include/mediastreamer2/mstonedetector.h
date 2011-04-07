/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2011  Belledonne Communications SARL.
Author: Simon Morlat (simon.morlat@linphone.org)

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

#ifndef mstonedetector_h
#define mstonedetector_h

#include <mediastreamer2/msfilter.h>

/**
 * Structure describing which tone is to be detected.
**/
struct _MSToneDetectorDef{
	char tone_name[8];
	int frequency;		/**<Expected frequency of the tone*/
	int min_duration;	/**<Min duration of the tone in milliseconds */
	float min_amplitude; /**<Minimum amplitude of the tone, 1.0 corresponding to the normalized 0dbm level */
};

typedef struct _MSToneDetectorDef MSToneDetectorDef;

/**
 * Structure carried as argument of the MS_TONE_DETECTOR_EVENT
**/
struct _MSToneDetectorEvent{
	char tone_name[8];
	uint64_t tone_start_time;	/**<Tone start time in millisecond */
};

typedef struct _MSToneDetectorEvent MSToneDetectorEvent;

/** Method to as the tone detector filter to monitor a new tone type.*/
#define MS_TONE_DETECTOR_ADD_SCAN	MS_FILTER_METHOD(MS_TONE_DETECTOR_ID,0,MSToneDetectorDef)

/** Remove previously added scans*/
#define MS_TONE_DETECTOR_CLEAR_SCANS	MS_FILTER_METHOD_NO_ARG(MS_TONE_DETECTOR_ID,1)

/** Event generated when a tone is detected */
#define MS_TONE_DETECTOR_EVENT		MS_FILTER_EVENT(MS_TONE_DETECTOR_ID,0,MSToneDetectorEvent)

#endif

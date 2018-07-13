/*
 mediastreamer2 library - modular sound and video processing and streaming
 Copyright (C) 2010  Belledonne Communications SARL (simon.morlat@linphone.org)

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

#ifndef msmediarecorder_h
#define msmediarecorder_h

#include <mediastreamer2/mssndcard.h>
#include <mediastreamer2/msinterfaces.h>
#include <mediastreamer2/msvideo.h>
#include <mediastreamer2/mswebcam.h>
#include <mediastreamer2/msmediaplayer.h>

/**
 * Media file recorder
 */
typedef struct _MSMediaRecorder MSMediaRecorder;

/**
 * Callbacks definitions */

// typedef void (*MSMediaRecorderYourCallback)(void *user_data);

 /**
 * End of Callbacks definitions */


#ifdef __cplusplus
extern "C"{
#endif

/**
 * Instanciate a media recorder
 * @param factory a MSFactory
 * @param snd_card Recording sound card
 * @param web_cam Recording camera
 * @param video_display_name Video out
 * @param window_id Pointer on the drawing window
 * @param format File format (Wave or MKV)
 * @param video_codec Video codec if MKV file (h264 or vp8)
 * @return A pointer on the created MSMediaRecorder
 */
MS2_PUBLIC MSMediaRecorder *ms_media_recorder_new(MSFactory *factory, MSSndCard *snd_card, MSWebCam *web_cam, const char *video_display_name, void *window_id, MSFileFormat format, const char *video_codec);

/**
 * Free a media Recorder
 * @param obj Pointer on the MSMediaRecorder to free
 */
MS2_PUBLIC void ms_media_recorder_free(MSMediaRecorder *obj);

/**
 * Get the window ID
 * @param obj The recorder
 * @return The window ID
 */
MS2_PUBLIC void * ms_media_recorder_get_window_id(const MSMediaRecorder *obj);

/**
 * Open a media file to write to
 * @param obj The recorder
 * @param filepath Path of the file to write to
 * @return TRUE if the file could be created
 */
MS2_PUBLIC bool_t ms_media_recorder_open(MSMediaRecorder *obj, const char *filepath, int device_orientation);

/**
 * Close a media file
 * That function can be safely called even if no file has been opened
 * @param obj The recorder
 */
MS2_PUBLIC void ms_media_recorder_close(MSMediaRecorder *obj);

/**
 * Start recording
 * @param obj The recorder
 * @return TRUE if recording has been successfuly started
 */
MS2_PUBLIC bool_t ms_media_recorder_start(MSMediaRecorder *obj);

/**
 * Pauses recording.
 * @param obj The recorder
 */
MS2_PUBLIC void ms_media_recorder_pause(MSMediaRecorder *obj);

/**
 * Get the state of the recorder
 * @param obj The recorder
 * @return An MSPLayerSate enum
 */
MS2_PUBLIC MSRecorderState ms_media_recorder_get_state(MSMediaRecorder *obj);

/**
 * Check whether Matroska format is supported by the recorder
 * @return TRUE if supported
 */
MS2_PUBLIC bool_t ms_media_recorder_matroska_supported(void);

/**
 * Return format of the current opened file
 * @param obj Recorder
 * @return Format of the file. UNKNOWN_FORMAT when no file is opened
 */
MS2_PUBLIC MSFileFormat ms_media_recorder_get_file_format(const MSMediaRecorder *obj);

/**
 * Removes the file at provided path if it exists.
 * @param obj Recorder
 * @param filepath Path of the file to remove.
 */
MS2_PUBLIC void ms_media_recorder_remove_file(MSMediaRecorder *obj, const char *filepath);

#ifdef __cplusplus
}
#endif


#endif

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

#ifndef msmediarecorder_h
#define msmediarecorder_h

#include <mediastreamer2/msinterfaces.h>
#include <mediastreamer2/msmediaplayer.h>
#include <mediastreamer2/mssndcard.h>
#include <mediastreamer2/msvideo.h>
#include <mediastreamer2/mswebcam.h>

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
extern "C" {
#endif

/**
 * Instanciate a media recorder
 * @param factory a MSFactory
 * @param snd_card Recording sound card, may be NULL if no audio record is requested.
 * @param web_cam Recording camera, may be NULL if no video record is requested.
 * @param video_display_name Video out
 * @param window_id Pointer on the drawing window
 * @param format File format (Wave or MKV)
 * @param video_codec Video codec if MKV file (h264 or vp8)
 * @return A pointer on the created MSMediaRecorder
 */
MS2_PUBLIC MSMediaRecorder *ms_media_recorder_new(MSFactory *factory,
                                                  MSSndCard *snd_card,
                                                  MSWebCam *web_cam,
                                                  const char *video_display_name,
                                                  void *window_id,
                                                  MSFileFormat format,
                                                  const char *video_codec);

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
MS2_PUBLIC void *ms_media_recorder_get_window_id(const MSMediaRecorder *obj);

/**
 * Create the window ID
 * @param obj The recorder
 * @return The window ID
 */
MS2_PUBLIC void *ms_media_recorder_create_window_id(MSMediaRecorder *obj);

/**
 * Open a media file to write to
 * @param obj The recorder
 * @param filepath Path of the file to write to
 * @return TRUE if the file could be created
 */
MS2_PUBLIC bool_t ms_media_recorder_open(MSMediaRecorder *obj, const char *filepath);

/**
 * Open a media file to write to
 * @param obj The recorder
 * @param filepath Path of the file to write to
 * @param append boolean to enable append mode (in case the file already exists).
 * @return TRUE if the file could be created
 */
MS2_PUBLIC bool_t ms_media_recorder_open_2(MSMediaRecorder *obj, const char *filepath, bool_t append);

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

/**
 * Get linear volume when capturing audio.
 * @param obj Recorder
 * @return Linear volume.
 */
MS2_PUBLIC float ms_media_recorder_get_capture_volume(const MSMediaRecorder *obj);

/**
 * Set the device orientation. Must be set before calling ms_media_recorder_open(),
 * cannot be changed afterwards.
 * @param obj Recorder
 * @param device_orientation the orientation in degrees.
 */
MS2_PUBLIC void ms_media_recorder_set_device_orientation(MSMediaRecorder *obj, int device_orientation);

#ifdef __cplusplus
}
#endif

#endif

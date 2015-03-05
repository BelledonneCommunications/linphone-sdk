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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef MS_MEDIA_PLAYER_H
#define MS_MEDIA_PLAYER_H

#include <mediastreamer2/mssndcard.h>
#include <mediastreamer2/msinterfaces.h>
#include <mediastreamer2/msvideo.h>

/**
 * @brief Media file player
 */
typedef struct _MSMediaPlayer MSMediaPlayer;

/**
 * Callbacks definitions */
typedef void (*MSMediaPlayerEofCallback)(void *user_data);

typedef enum {
	MS_FILE_FORMAT_UNKNOWN,
	MS_FILE_FORMAT_WAVE,
	MS_FILE_FORMAT_MATROSKA
} MSFileFormat;


#ifdef __cplusplus
extern "C"{
#endif

/**
 * @brief Instanciate a media player
 * @param snd_card Playback sound card
 * @param video_display_name Video out
 * @param window_id Pointer on the drawing window
 * @return A pointer on the created MSMediaPlayer
 */
MS2_PUBLIC MSMediaPlayer *ms_media_player_new(MSSndCard *snd_card, const char *video_display_name, unsigned long window_id);

/**
 * @brief Free a media player
 * @param obj Pointer on the MSMediaPlayer to free
 */
MS2_PUBLIC void ms_media_player_free(MSMediaPlayer *obj);

/**
 * @brief Get the window ID
 * @param obj The player
 * @return The window ID
 */
MS2_PUBLIC unsigned long ms_media_player_get_window_id(const MSMediaPlayer *obj);

/**
 * @brief Set the "End of File" callback
 * @param obj The player
 * @param cb Function to call
 * @param user_data Data which will be passed to the function
 */
MS2_PUBLIC void ms_media_player_set_eof_callback(MSMediaPlayer *obj, MSMediaPlayerEofCallback cb, void *user_data);

/**
 * @brief Open a media file
 * @param obj The player
 * @param filepath Path of the file to open
 * @return TRUE if the file could be opened
 */
MS2_PUBLIC bool_t ms_media_player_open(MSMediaPlayer *obj, const char *filepath);

/**
 * @brief Close a media file
 * That function can be safly call even if no file has been opend
 * @param obj The player
 */
MS2_PUBLIC void ms_media_player_close(MSMediaPlayer *obj);

/**
 * @brief Start playback
 * @param obj The player
 * @return TRUE if playback has been successfuly started
 */
MS2_PUBLIC bool_t ms_media_player_start(MSMediaPlayer *obj);

/**
 * @brief Stop a playback
 * When a playback is stoped, the player automatically seek at
 * the begining of the file.
 * @param obj The player
 */
MS2_PUBLIC void ms_media_player_stop(MSMediaPlayer *obj);

/**
 * @brief Turn playback to paused.
 * @param obj The player
 */
MS2_PUBLIC void ms_media_player_pause(MSMediaPlayer *obj);

/**
 * @brief Seek into the opened file
 * Can be safly call when playback is runing
 * @param obj The player
 * @param seek_pos_ms Position where to seek on (in milliseconds)
 * @return
 */
MS2_PUBLIC bool_t ms_media_player_seek(MSMediaPlayer *obj, int seek_pos_ms);

/**
 * @brief Get the state of the player
 * @param obj The player
 * @return An MSPLayerSate enum
 */
MS2_PUBLIC MSPlayerState ms_media_player_get_state(MSMediaPlayer *obj);

/**
 * @brief Get the duration of the opened media
 * @param obj The player
 * @return The duration in milliseconds. -1 if failure
 */
MS2_PUBLIC int ms_media_player_get_duration(MSMediaPlayer *obj);

/**
 * @brief Get the position of the playback
 * @param obj The player
 * @return The position in milliseconds. -1 if failure
 */
MS2_PUBLIC int ms_media_player_get_current_position(MSMediaPlayer *obj);

/**
 * @brief Check whether Matroska format is supported by the player
 * @return TRUE if supported
 */
MS2_PUBLIC bool_t ms_media_player_matroska_supported(void);

/**
 * @brief Return format of the current opened file
 * @param obj Player
 * @return Format of the file. UNKNOWN_FORMAT when no file is opened
 */
MS2_PUBLIC MSFileFormat ms_media_player_get_file_format(const MSMediaPlayer *obj);

#ifdef __cplusplus
}
#endif


#endif

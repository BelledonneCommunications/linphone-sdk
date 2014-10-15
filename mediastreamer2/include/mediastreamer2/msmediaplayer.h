#ifndef FILEPLAYER_H
#define FILEPLAYER_H

#include "mssndcard.h"
#include "msinterfaces.h"
#include "msvideo.h"

/**
 * @brief Media file player
 */
typedef struct _MSMediaPlayer MSMediaPlayer;

/**
 * Callbacks definitions */
typedef void (*MSMediaPlayerEofCallback)(void *user_data);

/**
 * @brief Instanciate a file player
 * @param snd_card Playback sound card
 * @param video_display_name Video out
 * @param window_id Pointer on the drawing window
 * @return A pointer on the created MSFilePlayer
 */
MS2_PUBLIC MSMediaPlayer *ms_media_player_new(MSSndCard *snd_card, const char *video_display_name, void *window_id);

/**
 * @brief Free a file player
 * @param obj Pointer on the MSFilePlayer to free
 */
MS2_PUBLIC void ms_media_player_free(MSMediaPlayer *obj);

/**
 * @brief Get the window ID
 * @param obj The player
 * @return The window ID
 */
MS2_PUBLIC void *ms_media_player_get_window_id(const MSMediaPlayer *obj);

/**
 * @brief Set the "End of File" callback
 * @param obj A MSFilePlayer object pointer
 * @param cb Function to call
 * @param user_data Data which will be passed to the function
 */
MS2_PUBLIC void ms_media_player_set_eof_callback(MSMediaPlayer *obj, MSMediaPlayerEofCallback cb, void *user_data);

/**
 * @brief Open a media file
 * @param obj A pointer on a MSFilePlayer
 * @param filepath Path of the file to open
 * @return TRUE if the file could be opened
 */
MS2_PUBLIC bool_t ms_media_player_open(MSMediaPlayer *obj, const char *filepath);

/**
 * @brief Close a media file
 * That function can be safly call even if no file has been opend
 * @param obj A pointer to a MSFilePlayer
 */
MS2_PUBLIC void ms_media_player_close(MSMediaPlayer *obj);

/**
 * @brief Start playback
 * @param obj A pointer on a MSFilePlayer
 * @return TRUE if playback has been successfuly started
 */
MS2_PUBLIC bool_t ms_media_player_start(MSMediaPlayer *obj);

/**
 * @brief Stop a playback
 * When a playback is stoped, the player automatically seek at
 * the begining of the file.
 * @param obj A pointer on a MSFilePlayer
 */
MS2_PUBLIC void ms_media_player_stop(MSMediaPlayer *obj);

/**
 * @brief Turn playback to paused.
 * @param obj A pointer on a MSFilePlayer
 */
MS2_PUBLIC void ms_media_player_pause(MSMediaPlayer *obj);

/**
 * @brief Seek into the opened file
 * Can be safly call when playback is runing
 * @param obj A pointer on a MSFilePlayer
 * @param seek_pos_ms Position where to seek on (in milliseconds)
 * @return
 */
MS2_PUBLIC bool_t ms_media_player_seek(MSMediaPlayer *obj, int seek_pos_ms);

/**
 * @brief Get the state of the player
 * @param obj A pointer on a MSFilePlayer
 * @return An MSPLayerSate enum
 */
MS2_PUBLIC MSPlayerState ms_media_player_get_state(MSMediaPlayer *obj);

/**
 * @brief Get the duration of the opened media
 * @param obj A pointer on a MSFilePlayer
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

#endif

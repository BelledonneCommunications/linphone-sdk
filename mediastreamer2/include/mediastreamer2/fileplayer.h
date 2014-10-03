#ifndef FILEPLAYER_H
#define FILEPLAYER_H

#include "mssndcard.h"
#include "msinterfaces.h"

/**
 * @brief Media file player
 */
typedef struct _MSFilePlayer MSFilePlayer;

typedef void (*MSFilePlayerEofCallback)(void *user_data);

/**
 * @brief Instanciate a file player
 * @param snd_card Playback sound card
 * @param video_display_name Video out
 * @return A pointer on the created MSFilePlayer
 */
MS2_PUBLIC MSFilePlayer *ms_file_player_new(MSSndCard *snd_card, const char *video_display_name);

/**
 * @brief Free a file player
 * @param obj Pointer on the MSFilePlayer to free
 */
MS2_PUBLIC void ms_file_player_free(MSFilePlayer *obj);

/**
 * @brief Set the "End of File" callback
 * @param obj A MSFilePlayer object pointer
 * @param cb Function to call
 * @param user_data Data which will be passed to the function
 */
MS2_PUBLIC void ms_file_player_set_eof_callback(MSFilePlayer *obj, MSFilePlayerEofCallback cb, void *user_data);

/**
 * @brief Open a media file
 * @param obj A pointer on a MSFilePlayer
 * @param filepath Path of the file to open
 * @return TRUE if the file could be opened
 */
bool_t ms_file_player_open(MSFilePlayer *obj, const char *filepath);

/**
 * @brief Close a media file
 * That function can be safly call even if no file has been opend
 * @param obj A pointer to a MSFilePlayer
 */
MS2_PUBLIC void ms_file_player_close(MSFilePlayer *obj);

/**
 * @brief Start playback
 * @param obj A pointer on a MSFilePlayer
 * @return TRUE if playback has been successfuly started
 */
MS2_PUBLIC bool_t ms_file_player_start(MSFilePlayer *obj);

/**
 * @brief Stop a playback
 * When a playback is stoped, the player automatically seek at
 * the begining of the file.
 * @param obj A pointer on a MSFilePlayer
 */
MS2_PUBLIC void ms_file_player_stop(MSFilePlayer *obj);

/**
 * @brief Turn playback to paused.
 * @param obj A pointer on a MSFilePlayer
 */
MS2_PUBLIC void ms_file_player_pause(MSFilePlayer *obj);

/**
 * @brief Seek into the opened file
 * Can be safly call when playback is runing
 * @param obj A pointer on a MSFilePlayer
 * @param seek_pos_ms Position where to seek on (in milliseconds)
 * @return
 */
MS2_PUBLIC bool_t ms_file_player_seek(MSFilePlayer *obj, int seek_pos_ms);

/**
 * @brief Get the state of the player
 * @param obj A pointer on a MSFilePlayer
 * @return An MSPLayerSate enum
 */
MS2_PUBLIC MSPlayerState ms_file_player_get_state(MSFilePlayer *obj);

/**
 * @brief Check whether Matroska format is supported by the player
 * @return TRUE if supported
 */
MS2_PUBLIC bool_t ms_file_player_matroska_supported(void);

#endif

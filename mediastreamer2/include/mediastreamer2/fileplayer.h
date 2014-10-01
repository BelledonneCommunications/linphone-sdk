#ifndef FILEPLAYER_H
#define FILEPLAYER_H

#include "mscommon.h"
#include "mssndcard.h"

typedef struct _FilePlayer FilePlayer;

typedef void (*FilePlayerEofCallback)(void *user_data);

MS2_PUBLIC FilePlayer *ms_file_player_new(MSSndCard *snd_card, const char *video_display_name);
MS2_PUBLIC void ms_file_player_free(FilePlayer *obj);
MS2_PUBLIC void ms_file_player_set_eof_callback(FilePlayer *obj, FilePlayerEofCallback cb, void *user_data);
MS2_PUBLIC bool_t ms_file_player_open(FilePlayer *obj, const char *filepath);
MS2_PUBLIC void ms_file_player_close(FilePlayer *obj);
MS2_PUBLIC bool_t ms_file_player_start(FilePlayer *obj);
MS2_PUBLIC void ms_file_player_stop(FilePlayer *obj);
MS2_PUBLIC void ms_file_player_pause(FilePlayer *obj);
MS2_PUBLIC void ms_file_player_seek(FilePlayer *obj, int seek_pos_ms);

#endif

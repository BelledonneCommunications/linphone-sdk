/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006  Simon MORLAT (simon.morlat@linphone.org)

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

#ifndef _MKV_PARSER_H
#define _MKV_PARSER_H

#include "mediastreamer2/mscommon.h"

#define MAX_MKV_STRING_LENGTH 256

typedef struct _MKVReader MKVReader;

typedef struct {
	int64_t timecode_scale;
	double duration;
	char muxing_app[MAX_MKV_STRING_LENGTH];
	char writing_app[MAX_MKV_STRING_LENGTH];
} MKVSegmentInfo;

typedef struct {
	uint8_t track_num;
	uint32_t timestamp; // ms
	bool_t keyframe;
	uint8_t *data;
	size_t data_length;
	uint8_t *codec_state_data;
	size_t codec_state_size;
} MKVBlock;

#define MKV_TRACK_TYPE_VIDEO    0x01
#define MKV_TRACK_TYPE_AUDIO    0x02
#define MKV_TRACK_TYPE_COMPLEX  0x03
#define MKV_TRACK_TYPE_LOGO     0x10
#define MKV_TRACK_TYPE_SUBTITLE 0x11
#define MKV_TRACK_TYPE_BUTTONS  0x12
#define MKV_TRACK_TYPE_CONTROL  0x20

typedef struct {
	uint8_t num;
	uint64_t UID;
	uint8_t type;
	bool_t enabled;
	bool_t def;
	bool_t forced;
	bool_t lacing;
	int min_cache;
	int max_block_addition_id;
	char codec_id[MAX_MKV_STRING_LENGTH];
	uint8_t *codec_private;
	int codec_private_length;
	int seek_preroll;
} MKVTrack;

typedef struct {
	MKVTrack base;
	bool_t interlaced;
	int width;
	int height;
	double frame_rate;
} MKVVideoTrack;

typedef struct {
	MKVTrack base;
	int sampling_freq;
	int channels;
} MKVAudioTrack;

typedef struct {
	uint8_t track;
	uint64_t cluster_pos;
	uint64_t relative_pos;
} MKVCueTrackPosition;

typedef struct {
	uint64_t time;
	MSList *track_positions;
} MKVCuePoint;

typedef struct _MKVTrackReader MKVTrackReader;

extern MKVReader *mkv_reader_open(const char *filename);
extern void mkv_reader_close(MKVReader *obj);
extern inline const MKVSegmentInfo *mkv_reader_get_segment_info(const MKVReader *reader);
extern const MKVTrack *mkv_reader_get_default_track(MKVReader *r, int track_type);
extern const MKVTrack *mkv_reader_get_first_track(MKVReader *r, int track_type);
extern MKVTrackReader *mkv_reader_get_track_reader(MKVReader *reader, int track_num);
extern int mkv_reader_seek(MKVReader *reader, int pos_ms);

extern void mkv_track_reader_next_block(MKVTrackReader *reader, MKVBlock **block, bool_t *end_of_track);
extern void mkv_track_reader_reset(MKVTrackReader *reader);
extern void mkv_track_reader_destroy(MKVTrackReader *reader);

extern void mkv_block_free(MKVBlock *block);

#endif

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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
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
	uint8_t channels;
} MKVAudioTrack;

typedef struct _MKVTrackReader MKVTrackReader;

/**
 * @brief Open a MKV file for reading
 * @param filename Name of the file to open
 * @return A pointer on a MKVReader. NULL if opening fails
 */
MKVReader *mkv_reader_open(const char *filename);

/**
 * @brief Close a MKV file.
 * All associated track readers will be automatically destroyed
 * @param obj MKVReader
 */
void mkv_reader_close(MKVReader *obj);

/**
 * @brief Get information about the Matroska segment
 * @param reader MKVReader
 * @return Matroska segment information
 */
const MKVSegmentInfo *mkv_reader_get_segment_info(const MKVReader *reader);

/**
 * @brief Get the default track for a specified track type
 * @param r MKVReader object
 * @param track_type Type of the track
 * @return A pointer on a track descriptor
 */
const MKVTrack *mkv_reader_get_default_track(MKVReader *r, int track_type);

/**
 * @brief Get the first track of the specified type
 * @param r MKVReader object
 * @param track_type Type of the track
 * @return A track descriptor
 */
const MKVTrack *mkv_reader_get_first_track(MKVReader *r, int track_type);

/**
 * @brief Create a track reader from its track number
 * @param reader MKVReader
 * @param track_num Track number
 * @return A pointer on a track reader
 */
MKVTrackReader *mkv_reader_get_track_reader(MKVReader *reader, int track_num);

/**
 * @brief Set the reading head of each assocated track reader at a specific position
 * @param reader MKVReader
 * @param pos_ms Position of the head in miliseconds
 * @return The effective position of the head after the operation
 */
int mkv_reader_seek(MKVReader *reader, int pos_ms);

/**
 * @brief Get the next block
 * @param reader MKVReader
 * @param block Block data
 * @param end_of_track Return TRUE when the end of the track has been reached
 */
void mkv_track_reader_next_block(MKVTrackReader *reader, MKVBlock **block, bool_t *end_of_track);

/**
 * @brief Reset the track reader.
 * The reading head is set at the start of the track
 * @param reader MKVReader
 */
void mkv_track_reader_reset(MKVTrackReader *reader);

/**
 * @brief Destroy a track reader
 * @param reader Track reader to destroy
 */
void mkv_track_reader_destroy(MKVTrackReader *reader);

/**
 * @brief Free block data
 * @param block MKV block to free
 */
void mkv_block_free(MKVBlock *block);

#endif

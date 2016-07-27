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

#define bool_t mkv_bool_t
#include <matroska/matroska.h>
#include <matroska/matroska_sem.h>
#undef bool_t

#include "mkv_reader.h"

extern const nodemeta LangStr_Class[];
extern const nodemeta UrlPart_Class[];
extern const nodemeta BufStream_Class[];
extern const nodemeta MemStream_Class[];
extern const nodemeta Streams_Class[];
extern const nodemeta File_Class[];
extern const nodemeta Stdio_Class[];
extern const nodemeta Matroska_Class[];
extern const nodemeta EBMLElement_Class[];
extern const nodemeta EBMLMaster_Class[];
extern const nodemeta EBMLBinary_Class[];
extern const nodemeta EBMLString_Class[];
extern const nodemeta EBMLInteger_Class[];
extern const nodemeta EBMLCRC_Class[];
extern const nodemeta EBMLDate_Class[];
extern const nodemeta EBMLVoid_Class[];

static void _load_modules(nodemodule *modules);

struct _MKVTrackReader {
	int track_num;
	ebml_parser_context parser;
	const ebml_element *track_elt;
	ebml_element *current_cluster;
	ebml_element *current_frame_elt;
	stream *file;
	MKVReader *root;
	bool_t need_seeking;
};

struct _MKVReader{
	parsercontext p;
	stream *file;
	ebml_element *info_elt;
	bctbx_list_t *tracks_elt;
	ebml_element *cues;
	MKVSegmentInfo info;
	bctbx_list_t *tracks;
	filepos_t first_cluster_pos;
	filepos_t last_cluster_end;
	filepos_t first_level1_pos;
	bctbx_list_t *readers;
};

static int _parse_headers(MKVReader *obj);
static int _parse_segment_info(MKVSegmentInfo *seg_info_out, ebml_element *seg_info_elt);
static int _parse_tracks(bctbx_list_t **tracks_out, ebml_element *tracks_elt);
static void _parse_track(MKVTrack **track_out, ebml_element *track_elt);
static void _parse_video_track_info(MKVVideoTrack *video_info_out, ebml_element *video_info_elt);
static void _parse_audio_track_info(MKVAudioTrack *audio_info_out, ebml_element *audio_info_elt);
static void _mkv_track_free(MKVTrack *obj);
static void _mkv_track_reader_destroy(MKVTrackReader *obj);
static void _mkv_track_reader_seek(MKVTrackReader *obj, filepos_t pos);
static void _mkv_track_reader_edit_seek(MKVTrackReader *obj);

MKVReader *mkv_reader_open(const char *filename) {
	MKVReader *obj = (MKVReader *)ms_new0(MKVReader, 1);
	tchar_t *fname = NULL;
	err_t err;

	ParserContext_Init(&obj->p, NULL, NULL, NULL);
	_load_modules((nodemodule *)&obj->p);
	err = MATROSKA_Init((nodecontext *)&obj->p);
	if(err != ERR_NONE) {
		ms_error("Parser opening failed. Could not initialize Matroska parser. err=%d", (int)err);
		goto fail;
	}
#ifdef UNICODE
	fname = ms_malloc0((strlen(filename) + 1) * sizeof(tchar_t));
#ifdef _WIN32
	MultiByteToWideChar(CP_UTF8, 0, filename, -1, fname, (int)strlen(filename));
#else
	mbstowcs(fname, filename, strlen(filename));
#endif
#else
	fname = (tchar_t *)filename;
#endif
	obj->file = StreamOpen(&obj->p, fname, SFLAG_RDONLY);
	if(obj->file == NULL) {
		ms_error("Parser opening failed. Could not open %s", filename);
		goto fail;
	}
	if(_parse_headers(obj) < 0) {
		ms_error("MKVParser: error while parsing EBML header");
		goto fail;
	}
	return obj;

fail:
#ifdef UNICODE
	if (fname != NULL) ms_free(fname);
#endif
	if(obj->file) StreamClose(obj->file);
	if(obj->info_elt) NodeDelete((node *)obj->info_elt);
	bctbx_list_free_with_data(obj->tracks_elt, (void(*)(void *))NodeDelete);
	MATROSKA_Done((nodecontext *)&obj->p);
	ms_free(obj);
	return NULL;
}

void mkv_reader_close(MKVReader *obj) {
	if(obj) {
		StreamClose(obj->file);
		if(obj->info_elt) NodeDelete((node *)obj->info_elt);
		bctbx_list_free_with_data(obj->tracks_elt, (void(*)(void *))NodeDelete);
		if(obj->tracks) bctbx_list_free_with_data(obj->tracks, (void(*)(void *))_mkv_track_free);
		if(obj->cues) NodeDelete((node *)obj->cues);
		bctbx_list_free_with_data(obj->readers, (void(*)(void *))_mkv_track_reader_destroy);
		MATROSKA_Done((nodecontext *)&obj->p);
		ParserContext_Done(&obj->p);
		ms_free(obj);
	}
}

const MKVSegmentInfo *mkv_reader_get_segment_info(const MKVReader *reader) {
	return &reader->info;
}

const MKVTrack *mkv_reader_get_default_track(MKVReader *r, int track_type) {
	bctbx_list_t *it;
	const MKVTrack *track = NULL;
	for(it=r->tracks; it!=NULL; it=it->next) {
		track = (MKVTrack *)it->data;
		if(track->type == track_type && track->def) break;
	}
	if(it!=NULL) {
		return track;
	} else {
		return NULL;
	}
}

const MKVTrack *mkv_reader_get_first_track(MKVReader *r, int track_type) {
	bctbx_list_t *it;
	const MKVTrack *track = NULL;
	for(it=r->tracks; it!=NULL; it=it->next) {
		track = (MKVTrack *)it->data;
		if(track->type == track_type) break;
	}
	if(it!=NULL) {
		return track;
	} else {
		return NULL;
	}
}

MKVTrackReader *mkv_reader_get_track_reader(MKVReader *reader, int track_num) {
	bctbx_list_t *it, *it2;
	MKVTrack *track;
	MKVTrackReader *track_reader;
	int upper_levels = 0;

	for(it=reader->tracks,it2=reader->tracks_elt; it!=NULL && it2!=NULL; it=it->next,it2=it2->next) {
		track = (MKVTrack *)it->data;
		if(track->num == track_num) break;
	}
	if(it==NULL) return NULL;
	track_reader = ms_new0(MKVTrackReader, 1);
	track_reader->root = reader;
	track_reader->track_num = track_num;
	track_reader->track_elt = (ebml_element *)it2->data;
	track_reader->file = Stream_Duplicate(reader->file, SFLAG_RDONLY);
	track_reader->parser.Context = &MATROSKA_ContextSegment;
	track_reader->parser.EndPosition = reader->last_cluster_end;
	track_reader->parser.UpContext = NULL;
	Stream_Seek(track_reader->file, reader->first_cluster_pos, SEEK_SET);
	track_reader->current_cluster = EBML_FindNextElement(track_reader->file, &track_reader->parser, &upper_levels, FALSE);
	EBML_ElementReadData(track_reader->current_cluster, track_reader->file, &track_reader->parser, FALSE, SCOPE_PARTIAL_DATA, FALSE);
	reader->readers = bctbx_list_append(reader->readers, track_reader);
	return track_reader;
}

int mkv_reader_seek(MKVReader *reader, int pos_ms) {
	ebml_element *cue_point, *next_cue_point;
	bctbx_list_t *it;
	if(reader->cues == NULL) {
		ms_error("MKVReader: unable to seek. No cues table");
		return -1;
	}
	for(cue_point = NULL, next_cue_point = EBML_MasterChildren((ebml_master *)reader->cues);
		next_cue_point!=NULL;
		cue_point = next_cue_point, next_cue_point = EBML_MasterNext(next_cue_point)) {
		MATROSKA_LinkCueSegmentInfo((matroska_cuepoint *)next_cue_point, (ebml_master *)reader->info_elt);
		if(MATROSKA_CueTimecode((matroska_cuepoint *)next_cue_point) > (timecode_t)pos_ms * 1000000LL) {
			break;
		}
	}
	if(cue_point==NULL) cue_point = next_cue_point;
	if(cue_point) {
		ebml_element *track_position;
		filepos_t pos = 0;
		bctbx_list_for_each(reader->readers, (MSIterateFunc)_mkv_track_reader_edit_seek);
		for(track_position=EBML_MasterFindChild((ebml_master *)cue_point, &MATROSKA_ContextCueTrackPositions);
			track_position!=NULL;
			track_position = EBML_MasterFindNextElt((ebml_master *)cue_point, track_position, FALSE, FALSE)) {
			int track_num = (int)EBML_IntegerValue((ebml_integer *)EBML_MasterFindChild((ebml_master *)track_position, &MATROSKA_ContextCueTrack));
			MKVTrackReader *t_reader = NULL;
			for(it = reader->readers; it != NULL; it=it->next) {
				t_reader = (MKVTrackReader *)it->data;
				if(t_reader->track_num == track_num) break;
			}
			if(t_reader) {
				pos = EBML_IntegerValue((ebml_integer *)EBML_MasterFindChild((ebml_master *)track_position, &MATROSKA_ContextCueClusterPosition));
				_mkv_track_reader_seek(t_reader, pos);
				t_reader->need_seeking = FALSE;
			}
		}
		for(it=reader->readers; it!=NULL; it=it->next) {
			MKVTrackReader *t_reader = (MKVTrackReader *)it->data;
			if(t_reader->need_seeking) {
				_mkv_track_reader_seek(t_reader, pos);
				t_reader->need_seeking = FALSE;
			}
		}
	}
	return (int)(MATROSKA_CueTimecode((matroska_cuepoint *)cue_point) / 1000000LL);
}

void mkv_track_reader_next_block(MKVTrackReader *reader, MKVBlock **block, bool_t *end_of_track) {
	matroska_block *block_elt = NULL;
	matroska_frame m_frame;
	int upper_levels =0;

	*block = NULL;
	*end_of_track = FALSE;

	do {
		do {
			do {
				if(reader->current_frame_elt==NULL) {
					reader->current_frame_elt = EBML_MasterChildren(reader->current_cluster);
				} else {
					reader->current_frame_elt = EBML_MasterNext(reader->current_frame_elt);
				}
			} while(reader->current_frame_elt
					&& !EBML_ElementIsType(reader->current_frame_elt, &MATROSKA_ContextSimpleBlock)
					&& !EBML_ElementIsType(reader->current_frame_elt, &MATROSKA_ContextBlockGroup));

			if(reader->current_frame_elt==NULL) {
				Stream_Seek(reader->file, EBML_ElementPositionEnd(reader->current_cluster), SEEK_SET);
				NodeDelete((node *)reader->current_cluster);
				reader->current_cluster = EBML_FindNextElement(reader->file, &reader->parser, &upper_levels, FALSE);
				if(reader->current_cluster) {
					EBML_ElementReadData(reader->current_cluster, reader->file, &reader->parser, FALSE, SCOPE_PARTIAL_DATA, FALSE);
				}
			}
		} while(reader->current_cluster && reader->current_frame_elt == NULL);

		if(reader->current_cluster == NULL) {
			*end_of_track = TRUE;
			return;
		}
		if(EBML_ElementIsType(reader->current_frame_elt, &MATROSKA_ContextBlockGroup)) {
			block_elt = (matroska_block *)EBML_MasterFindChild(reader->current_frame_elt, &MATROSKA_ContextBlock);
		} else {
			block_elt = (matroska_block *)reader->current_frame_elt;
		}
	} while(block_elt->TrackNumber != reader->track_num);

	MATROSKA_LinkBlockReadSegmentInfo(block_elt, (ebml_master *)reader->root->info_elt, TRUE);
	MATROSKA_LinkBlockReadTrack(block_elt, (ebml_master *)reader->track_elt, TRUE);
	MATROSKA_BlockReadData(block_elt, reader->file);
	*block = ms_new0(MKVBlock, 1);
	if(EBML_ElementIsType(reader->current_frame_elt, &MATROSKA_ContextBlockGroup)) {
		ebml_element *codec_state_elt = EBML_MasterFindChild(reader->current_frame_elt, &MATROSKA_ContextCodecState);
		if(codec_state_elt) {
			(*block)->codec_state_size = (size_t)EBML_ElementDataSize(codec_state_elt, FALSE);
			(*block)->codec_state_data = ms_new0(uint8_t, (*block)->codec_state_size);
			memcpy((*block)->codec_state_data, EBML_BinaryGetData((ebml_binary *)codec_state_elt), (*block)->codec_state_size);
		}
	}
	(*block)->keyframe = (bool_t)MATROSKA_BlockKeyframe(block_elt);
	(*block)->track_num = (uint8_t)MATROSKA_BlockTrackNum(block_elt);
	MATROSKA_BlockGetFrame(block_elt, 0, &m_frame, TRUE);
	(*block)->timestamp = (uint32_t)(MATROSKA_BlockTimecode(block_elt) / 1000000LL);
	(*block)->data_length = m_frame.Size;
	(*block)->data = ms_new0(uint8_t, m_frame.Size);
	memcpy((*block)->data, m_frame.Data, m_frame.Size);
	MATROSKA_BlockReleaseData(block_elt, TRUE);
}

void mkv_track_reader_reset(MKVTrackReader *reader) {
	int upper_levels = 0;
	Stream_Seek(reader->file, reader->root->first_cluster_pos, SEEK_SET);
	if(reader->current_cluster) NodeDelete((node *)reader->current_cluster);
	reader->current_cluster = EBML_FindNextElement(reader->file, &reader->parser, &upper_levels, FALSE);
	EBML_ElementReadData(reader->current_cluster, reader->file, &reader->parser, FALSE, SCOPE_PARTIAL_DATA, FALSE);
	reader->current_frame_elt = NULL;
}

void mkv_track_reader_destroy(MKVTrackReader *reader) {
	bctbx_list_remove(reader->root->readers, reader);
	_mkv_track_reader_destroy(reader);
}

void mkv_block_free(MKVBlock *block) {
	if(block->data) ms_free(block->data);
	if(block->codec_state_data) ms_free(block->codec_state_data);
	ms_free(block);
}

static void _load_modules(nodemodule *modules) {
	NodeRegisterClassEx(modules, Streams_Class);
	NodeRegisterClassEx(modules, File_Class);
	NodeRegisterClassEx(modules, Matroska_Class);
	NodeRegisterClassEx(modules, EBMLElement_Class);
	NodeRegisterClassEx(modules, EBMLMaster_Class);
	NodeRegisterClassEx(modules, EBMLBinary_Class);
	NodeRegisterClassEx(modules, EBMLString_Class);
	NodeRegisterClassEx(modules, EBMLInteger_Class);
	NodeRegisterClassEx(modules, EBMLCRC_Class);
	NodeRegisterClassEx(modules, EBMLDate_Class);
	NodeRegisterClassEx(modules, EBMLVoid_Class);
}

static int _parse_headers(MKVReader *obj) {
	ebml_element *level0 = NULL, *level1 = NULL;
	ebml_parser_context pctx, seg_pctx;
	tchar_t doc_type[9];
	int doc_type_version;
	err_t err;
	int upper_level = 0;
	bool_t cluster_found = FALSE;
	bool_t level1_found = FALSE;
	tchar_t *matroska_doc_type =
#ifdef UNICODE
		L"matroska";
#else
		"matroska";
#endif

	pctx.Context = &MATROSKA_ContextStream;
	pctx.EndPosition = INVALID_FILEPOS_T;
	pctx.Profile = 0;
	pctx.UpContext = NULL;

	Stream_Seek(obj->file, 0, SEEK_SET);
	level0 = EBML_FindNextElement(obj->file, &pctx, &upper_level, FALSE);
	if(level0 == NULL) {
		ms_error("MKVParser: file is empty");
		goto fail;
	}
	if(!EBML_ElementIsType(level0, &EBML_ContextHead)) {
		ms_error("MKVParser: first element is not an EBML header");
		goto fail;
	}
	err = EBML_ElementReadData(level0, obj->file, &pctx, FALSE, SCOPE_ALL_DATA, FALSE);
	if(err != ERR_NONE) {
		ms_error("MKVParser: could not parse EBML header. err=%d", (int)err);
		goto fail;
	}
	if(!EBML_MasterCheckMandatory((ebml_master *)level0, FALSE)) {
		ms_error("MKVParser: missing elements in the EBML header");
		goto fail;
	}
	EBML_StringGet((ebml_string *)EBML_MasterGetChild((ebml_master *)level0, &EBML_ContextDocType), doc_type, sizeof(doc_type));
	err = tcscmp(doc_type, matroska_doc_type);
	if(err != 0) {
		ms_error("MKVParser: not a matroska file");
		goto fail;
	}
	doc_type_version = (int)EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild((ebml_master *)level0, &EBML_ContextDocTypeVersion));
	NodeDelete((node *)level0);

	level0 = EBML_FindNextElement(obj->file, &pctx, &upper_level, FALSE);
	if(level0 == NULL) {
		ms_error("MKVParser: no matroska element");
		goto fail;
	}
	if(!EBML_ElementIsType(level0, &MATROSKA_ContextSegment)) {
		ms_error("MKVParser: second element is not a segment");
		goto fail;
	}
	seg_pctx.Context = &MATROSKA_ContextSegment;
	seg_pctx.EndPosition = EBML_ElementPositionEnd(level0);
	seg_pctx.Profile = doc_type_version;
	seg_pctx.UpContext = &pctx;
	upper_level = 0;
	while((level1 = EBML_FindNextElement(obj->file, &seg_pctx, &upper_level, FALSE))
		   && upper_level == 0) {
		if(!level1_found) {
			obj->first_level1_pos = EBML_ElementPosition(level1);
			level1_found = TRUE;
		}
		if(EBML_ElementIsType(level1, &MATROSKA_ContextInfo)) {
			err = EBML_ElementReadData(level1, obj->file, &seg_pctx, FALSE, SCOPE_ALL_DATA, FALSE);
			if(err != ERR_NONE) {
				ms_error("MKVParser: fail to read segment information");
				goto fail;
			}
			if(_parse_segment_info(&obj->info, level1) < 0) {
				ms_error("MKVParser: fail to parse segment information");
				goto fail;
			}
			obj->info_elt = EBML_ElementCopy(level1, NULL);
		} else if(EBML_ElementIsType(level1, &MATROSKA_ContextTracks)) {
			ebml_element *track;
			err = EBML_ElementReadData(level1, obj->file, &seg_pctx, FALSE, SCOPE_ALL_DATA, FALSE);
			if(err != ERR_NONE) {
				ms_error("MKVParser: fail to read tracks information");
				goto fail;
			}
			if(_parse_tracks(&obj->tracks, level1) < 0) {
				ms_error("MKVParser: fail to parse tracks information");
				goto fail;
			}
			for(track=EBML_MasterChildren(level1); track!=NULL; track = EBML_MasterNext(track)) {
				obj->tracks_elt = bctbx_list_append(obj->tracks_elt, EBML_ElementCopy(track, NULL));
			}
		} else if(EBML_ElementIsType(level1, &MATROSKA_ContextCluster)) {
			if(!cluster_found) {
				obj->first_cluster_pos = EBML_ElementPosition(level1);
				cluster_found = TRUE;
			}
			obj->last_cluster_end = EBML_ElementPositionEnd(level1);
			EBML_ElementSkipData(level1, obj->file, &seg_pctx, NULL, FALSE);
		} else if(EBML_ElementIsType(level1, &MATROSKA_ContextCues)) {
			err = EBML_ElementReadData(level1, obj->file, &seg_pctx, FALSE, SCOPE_ALL_DATA, FALSE);
			if(err != ERR_NONE) {
				ms_error("MKVParser: fail to read the table of cues");
			} else if(!EBML_MasterCheckMandatory((ebml_master *)level1, FALSE)) {
				ms_error("MKVParser: fail to parse the table of cues");
			} else {
				obj->cues = EBML_ElementCopy(level1, NULL);
			}
		} else {
			EBML_ElementSkipData(level1, obj->file, &seg_pctx, NULL, FALSE);
		}
		NodeDelete((node *)level1);
	}
	level1 = NULL;
	if(!cluster_found) goto fail;

	NodeDelete((node *)level0);
	return 0;

fail:
	if(level0) NodeDelete((node *)level0);
	if(level1) NodeDelete((node *)level1);
	if(obj->info_elt) NodeDelete((node *)obj->info_elt);
	obj->info_elt = NULL;
	if(obj->tracks_elt) obj->tracks_elt = bctbx_list_free_with_data(obj->tracks_elt, (void(*)(void *))NodeDelete);
	if(obj->cues) NodeDelete((node *)obj->cues);
	obj->cues = NULL;
	return -1;
}

static int _parse_segment_info(MKVSegmentInfo *seg_info_out, ebml_element *seg_info_elt) {
	tchar_t muxing_app[MAX_MKV_STRING_LENGTH];
	tchar_t writing_app[MAX_MKV_STRING_LENGTH];

	if(!EBML_MasterCheckMandatory((ebml_master *)seg_info_elt, FALSE)) {
		ms_error("MKVParser: fail to parse segment info. Missing elements");
		return -1;
	}
	seg_info_out->duration = EBML_FloatValue((ebml_float *)EBML_MasterFindChild((ebml_master *)seg_info_elt, &MATROSKA_ContextDuration));
	seg_info_out->timecode_scale = EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild((ebml_master *)seg_info_elt, &MATROSKA_ContextTimecodeScale));
	memset(muxing_app, 0, sizeof(muxing_app));
	EBML_StringGet((ebml_string *)EBML_MasterFindChild((ebml_master *)seg_info_elt, &MATROSKA_ContextMuxingApp), muxing_app, MAX_MKV_STRING_LENGTH);
#ifdef UNICODE
#ifdef _WIN32
	WideCharToMultiByte(CP_UTF8, 0, muxing_app, -1, seg_info_out->muxing_app, sizeof(seg_info_out->muxing_app), NULL, NULL);
#else
	wcstombs(seg_info_out->muxing_app, muxing_app, sizeof(seg_info_out->muxing_app));
#endif
#else
	strncpy(seg_info_out->muxing_app, muxing_app, sizeof(seg_info_out->muxing_app));
#endif
	memset(writing_app, 0, sizeof(writing_app));
	EBML_StringGet((ebml_string *)EBML_MasterFindChild((ebml_master *)seg_info_elt, &MATROSKA_ContextWritingApp), writing_app, MAX_MKV_STRING_LENGTH);
#ifdef UNICODE
#ifdef _WIN32
	WideCharToMultiByte(CP_UTF8, 0, writing_app, -1, seg_info_out->writing_app, sizeof(seg_info_out->writing_app), NULL, NULL);
#else
	wcstombs(seg_info_out->writing_app, writing_app, sizeof(seg_info_out->writing_app));
#endif
#else
	strncpy(seg_info_out->writing_app, writing_app, sizeof(seg_info_out->writing_app));
#endif
	return 0;
}

static int _parse_tracks(bctbx_list_t **tracks_out, ebml_element *tracks_elt) {
	ebml_element *elt;
	if(!EBML_MasterCheckMandatory((ebml_master *)tracks_elt, FALSE)) {
		ms_error("MKVParser: fail to parse tracks info. Missing elements");
		return -1;
	}
	for(elt=EBML_MasterChildren(tracks_elt); elt!=NULL; elt=EBML_MasterNext(elt)) {
		MKVTrack *track;
		_parse_track(&track, elt);
		*tracks_out = bctbx_list_append(*tracks_out, track);
	}
	return 0;
}

static void _parse_track(MKVTrack **track_out, ebml_element *track_elt) {
	tchar_t codec_id[MAX_MKV_STRING_LENGTH];
	uint8_t track_type;
	ebml_element *codec_private_elt;
	track_type = (uint8_t)EBML_IntegerValue((ebml_integer *)EBML_MasterFindChild(track_elt, &MATROSKA_ContextTrackType));
	if(track_type == TRACK_TYPE_VIDEO) {
		*track_out = (MKVTrack *)ms_new0(MKVVideoTrack, 1);
		_parse_video_track_info((MKVVideoTrack *)*track_out, EBML_MasterFindChild(track_elt, &MATROSKA_ContextVideo));
	} else if(track_type == TRACK_TYPE_AUDIO) {
		*track_out = (MKVTrack *)ms_new0(MKVAudioTrack, 1);
		_parse_audio_track_info((MKVAudioTrack *)*track_out, EBML_MasterFindChild(track_elt, &MATROSKA_ContextAudio));
	} else {
		*track_out = NULL;
		return;
	}
	(*track_out)->num = (uint8_t)EBML_IntegerValue((ebml_integer *)EBML_MasterFindChild(track_elt, &MATROSKA_ContextTrackNumber));
	(*track_out)->UID = EBML_IntegerValue((ebml_integer *)EBML_MasterFindChild(track_elt, &MATROSKA_ContextTrackUID));
	(*track_out)->type = track_type;
	(*track_out)->enabled = EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild((ebml_master *)track_elt, &MATROSKA_ContextFlagEnabled)) == 0 ? FALSE : TRUE;
	(*track_out)->def = EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild((ebml_master *)track_elt, &MATROSKA_ContextFlagDefault)) == 0 ? FALSE : TRUE;
	(*track_out)->forced = EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild((ebml_master *)track_elt, &MATROSKA_ContextFlagForced)) == 0 ? FALSE : TRUE;
	(*track_out)->lacing = EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild((ebml_master *)track_elt, &MATROSKA_ContextFlagLacing)) == 0 ? FALSE : TRUE;
	(*track_out)->min_cache = (int)EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild((ebml_master *)track_elt, &MATROSKA_ContextMinCache));
	(*track_out)->max_block_addition_id = (int)EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild((ebml_master *)track_elt, &MATROSKA_ContextMaxBlockAdditionID));
	memset(codec_id, 0, sizeof(codec_id));
	EBML_StringGet((ebml_string *)EBML_MasterFindChild(track_elt, &MATROSKA_ContextCodecID), codec_id, MAX_MKV_STRING_LENGTH);
#ifdef UNICODE
#ifdef _WIN32
	WideCharToMultiByte(CP_UTF8, 0, codec_id, -1, (*track_out)->codec_id, sizeof((*track_out)->codec_id), NULL, NULL);
#else
	wcstombs((*track_out)->codec_id, codec_id, sizeof((*track_out)->codec_id));
#endif
#else
	strncpy((*track_out)->codec_id, codec_id, sizeof((*track_out)->codec_id));
#endif
	codec_private_elt = EBML_MasterFindChild(track_elt, &MATROSKA_ContextCodecPrivate);
	if(codec_private_elt) {
		size_t data_size = (size_t)EBML_ElementDataSize(codec_private_elt, FALSE);
		(*track_out)->codec_private = ms_new0(uint8_t, data_size);
		memcpy((*track_out)->codec_private, EBML_BinaryGetData((ebml_binary *)codec_private_elt), data_size);
	}
	(*track_out)->seek_preroll = (int)EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild((ebml_master *)track_elt, &MATROSKA_ContextSeekPreRoll));
}

static void _parse_video_track_info(MKVVideoTrack *video_info_out, ebml_element *video_info_elt) {
	ebml_element *elt;
	if(video_info_elt==NULL) return;
	video_info_out->interlaced = EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild((ebml_master *)video_info_elt, &MATROSKA_ContextFlagInterlaced)) == 0 ? FALSE : TRUE;
	video_info_out->width = (int)EBML_IntegerValue((ebml_integer *)EBML_MasterFindChild(video_info_elt, &MATROSKA_ContextPixelWidth));
	video_info_out->height = (int)EBML_IntegerValue((ebml_integer *)EBML_MasterFindChild(video_info_elt, &MATROSKA_ContextPixelHeight));
	elt = EBML_MasterFindChild(video_info_elt, &MATROSKA_ContextFrameRate);
	video_info_out->frame_rate = elt ? EBML_FloatValue((ebml_float *)elt) : 0.0;
}

static void _parse_audio_track_info(MKVAudioTrack *audio_info_out, ebml_element *audio_info_elt) {
	if(audio_info_elt==NULL) return;
	audio_info_out->sampling_freq = (int)EBML_FloatValue((ebml_float *)EBML_MasterGetChild((ebml_master *)audio_info_elt, &MATROSKA_ContextSamplingFrequency));
	audio_info_out->channels = (uint8_t)EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild((ebml_master *)audio_info_elt, &MATROSKA_ContextChannels));
}

static void _mkv_track_free(MKVTrack *obj) {
	if (!obj) return;
	if(obj->codec_private) ms_free(obj->codec_private);
	ms_free(obj);
}

static void _mkv_track_reader_destroy(MKVTrackReader *obj) {
	NodeDelete((node *)obj->current_cluster);
	StreamClose(obj->file);
	ms_free(obj);
}

static void _mkv_track_reader_seek(MKVTrackReader *obj, filepos_t pos) {
	int upper_level = 0;
	obj->current_frame_elt = NULL;
	if(obj->current_cluster) NodeDelete((node *)obj->current_cluster);
	Stream_Seek(obj->file, obj->root->first_level1_pos + pos, SEEK_SET);
	obj->current_cluster = EBML_FindNextElement(obj->file, &obj->parser, &upper_level, FALSE);
	EBML_ElementReadData(obj->current_cluster, obj->file, &obj->parser, FALSE, SCOPE_PARTIAL_DATA, 0);
}

static void _mkv_track_reader_edit_seek(MKVTrackReader *obj) {
	obj->need_seeking = TRUE;
}

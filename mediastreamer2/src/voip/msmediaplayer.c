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

#include <bctoolbox/defs.h>

#include "../audiofilters/waveheader.h"
#include "mediastreamer2/msextdisplay.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msmediaplayer.h"
#include "mediastreamer2/msticker.h"

#ifdef _MSC_VER
#include <mmreg.h>
#include <mmsystem.h>
#endif

#include "fd_portab.h"

#define ms_filter_destroy_and_reset(obj)                                                                               \
	ms_filter_destroy(obj);                                                                                            \
	obj = NULL

#define ms_filter_destroy_and_reset_if_not_null(obj)                                                                   \
	if (obj != NULL) {                                                                                                 \
		ms_filter_destroy(obj);                                                                                        \
		obj = NULL;                                                                                                    \
	}

#define ms_free_if_not_null(obj)                                                                                       \
	if (obj != NULL) ms_free(obj)

typedef uint8_t FourCC[4];

typedef struct _FormatDesc {
	MSFileFormat format;
	FourCC four_cc;
} FormatDesc;

static const FormatDesc _format_desc_list[] = {
    {MS_FILE_FORMAT_WAVE, {'R', 'I', 'F', 'F'}},
    {MS_FILE_FORMAT_MATROSKA, {0x1a, 0x45, 0xdf, 0xa3}},
    {MS_FILE_FORMAT_UNKNOWN, {0x0, 0x0, 0x0, 0x0}},
};

static bool_t four_cc_compare(const FourCC arg1, const FourCC arg2);
static MSFileFormat four_cc_to_file_format(const FourCC four_cc);

enum GraphState { Uninit = 0, ResourcesAcquired, GraphReady };

struct _MSMediaPlayer {
	MSFactory *factory;
	MSFilter *player;
	MSFilter *audio_decoder;
	MSFilter *audio_sink;
	MSFilter *video_decoder;
	MSFilter *video_sink;
	MSFilter *resampler;
	MSPinFormat audio_pin_fmt;
	MSPinFormat video_pin_fmt;
	MSTicker *ticker;
	MSFileFormat format;
	bool_t is_open;
	enum GraphState graph_state;
	int loop_interval;
	char *filename;
	MSMediaPlayerEofCallback eof_cb;
	void *user_data_cb;
	ms_mutex_t cb_access;
	MSSndCard *snd_card;
	char *video_display;
	void *window_id;
};

static bool_t _get_format(const char *filepath, MSFileFormat *format);
static void _create_decoders(MSMediaPlayer *obj);
static void _create_sinks(MSMediaPlayer *obj);
static void _destroy_graph(MSMediaPlayer *obj);
static bool_t _link_all(MSMediaPlayer *obj);
static void _unlink_all(MSMediaPlayer *obj);
static void _eof_filter_notify_cb(void *userdata, struct _MSFilter *f, unsigned int id, void *arg);

static bool_t four_cc_compare(const FourCC arg1, const FourCC arg2) {
	return arg1[0] == arg2[0] && arg1[1] == arg2[1] && arg1[2] == arg2[2] && arg1[3] == arg2[3];
}

static MSFileFormat four_cc_to_file_format(const FourCC four_cc) {
	int i;
	for (i = 0; _format_desc_list[i].format != MS_FILE_FORMAT_UNKNOWN; i++) {
		if (four_cc_compare(four_cc, _format_desc_list[i].four_cc)) {
			return _format_desc_list[i].format;
		}
	}
	return MS_FILE_FORMAT_UNKNOWN;
}

MSMediaPlayer *
ms_media_player_new(MSFactory *factory, MSSndCard *snd_card, const char *video_display_name, void *window_id) {
	MSMediaPlayer *obj = (MSMediaPlayer *)ms_new0(MSMediaPlayer, 1);
	ms_mutex_init(&obj->cb_access, NULL);
	obj->snd_card = ms_snd_card_ref(snd_card);
	if (video_display_name != NULL && strlen(video_display_name) > 0) {
		obj->video_display = ms_strdup(video_display_name);
		obj->window_id = window_id;
#ifdef ANDROID
		if (obj->window_id) {
			JNIEnv *env = ms_get_jni_env();
			obj->window_id = (*env)->NewGlobalRef(env, obj->window_id);
		}
#endif
	}
	obj->factory = factory;
	obj->loop_interval = -1;
	return obj;
}

void ms_media_player_free(MSMediaPlayer *obj) {
	ms_media_player_close(obj);
	ms_snd_card_unref(obj->snd_card);
#ifdef ANDROID
	if (obj->window_id) {
		JNIEnv *env = ms_get_jni_env();
		(*env)->DeleteGlobalRef(env, obj->window_id);
		obj->window_id = NULL;
	}
#endif
	ms_free_if_not_null(obj->video_display);
	ms_free(obj);
}

void *ms_media_player_get_window_id(const MSMediaPlayer *obj) {
	return obj->window_id;
}

void *ms_media_player_create_window_id(MSMediaPlayer *obj) {
	void *id = NULL;
	if (obj->video_sink) {
		ms_filter_call_method(obj->video_sink, MS_VIDEO_DISPLAY_CREATE_NATIVE_WINDOW_ID, &id);
	}
	return id;
}

void ms_media_player_set_window_id(MSMediaPlayer *obj, void *window_id) {
#ifdef ANDROID
	if (obj->window_id) {
		JNIEnv *env = ms_get_jni_env();
		(*env)->DeleteGlobalRef(env, obj->window_id);
		obj->window_id = NULL;
	}
#endif
	obj->window_id = window_id;
#ifdef ANDROID
	if (obj->window_id) {
		JNIEnv *env = ms_get_jni_env();
		obj->window_id = (*env)->NewGlobalRef(env, obj->window_id);
	}
#endif
	if (obj->video_sink) {
		ms_filter_call_method(obj->video_sink, MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID, &obj->window_id);
	}
}

bool_t ms_media_player_open(MSMediaPlayer *obj, const char *filepath) {
	wave_header_t header;
	bctbx_vfs_file_t *fp;
	char *tmp;

	if (obj->is_open) {
		ms_error("Player is already opened, close it first!");
		return FALSE;
	}

	ms_message("Opening %s", filepath);
	if (access(filepath, F_OK) != 0) {
		ms_error("Cannot open %s. File does not exist", filepath);
		return FALSE;
	}
	if (!_get_format(filepath, &obj->format)) {
		ms_error("Fails to detect file format of %s", filepath);
		return FALSE;
	}
	switch (obj->format) {
		case MS_FILE_FORMAT_WAVE:
			fp = bctbx_file_open2(bctbx_vfs_get_default(), filepath, O_RDONLY);
			if (fp == NULL) {
				ms_error("Cannot open %s", filepath);
				return FALSE;
			}
			if (ms_read_wav_header_from_fp(&header, fp) == -1) {
				ms_error("Cannot open %s. Invalid WAV format", filepath);
				return FALSE;
			}
			bctbx_file_close(fp);
			if (wave_header_get_format_type(&header) != WAVE_FORMAT_PCM) {
				ms_error("Cannot open %s. Codec not supported", filepath);
				return FALSE;
			}
			obj->player = ms_factory_create_filter(obj->factory, MS_FILE_PLAYER_ID);
			break;
		case MS_FILE_FORMAT_MATROSKA:
			if ((obj->player = ms_factory_create_filter(obj->factory, MS_MKV_PLAYER_ID)) == NULL) {
				ms_error("Cannot open %s. Matroska file support is disabled", filepath);
				return FALSE;
			}
			break;
		case MS_FILE_FORMAT_UNKNOWN:
			ms_error("Cannot open %s. Unknown format", filepath);
			return FALSE;
	}
	tmp = ms_strdup(filepath);
	if (ms_filter_call_method(obj->player, MS_PLAYER_OPEN, tmp) == -1) {
		ms_error("Cannot open %s", filepath);
		ms_free(tmp);
		ms_filter_destroy(obj->player);
		return FALSE;
	}
	ms_free(tmp);

	obj->is_open = TRUE;
	obj->filename = ms_strdup(filepath);

	return TRUE;
}

bool_t ms_media_player_get_is_video_available(MSMediaPlayer *obj) {
	return obj->video_sink != NULL;
}

void ms_media_player_set_volume_gain(MSMediaPlayer *player, float gain) {
	if (player->audio_sink) {
		ms_filter_call_method(player->audio_sink, MS_AUDIO_PLAYBACK_SET_VOLUME_GAIN, &gain);
	}
}

float ms_media_player_get_volume_gain(const MSMediaPlayer *player) {
	float gain = 0.0f;
	if (player->audio_sink) {
		ms_filter_call_method(player->audio_sink, MS_AUDIO_PLAYBACK_GET_VOLUME_GAIN, &gain);
	}
	return gain;
}

MSFilter *ms_media_player_get_video_sink(const MSMediaPlayer *const obj) {
	return obj->video_sink;
}

void ms_media_player_close(MSMediaPlayer *obj) {
	if (obj->is_open) {

		ms_message("MSMediaPlayer: closing file.");
		ms_filter_call_method_noarg(obj->player, MS_PLAYER_CLOSE);
		obj->is_open = FALSE;
		if (obj->graph_state == GraphReady) {
			ms_media_player_stop(obj);
		}
		ms_free(obj->filename);
		obj->filename = NULL;
	}
}

void ms_media_player_prepare(MSMediaPlayer *obj) {
	if (obj->graph_state < ResourcesAcquired) {
		ms_snd_card_set_stream_type(obj->snd_card, MS_SND_CARD_STREAM_MEDIA);
		_create_decoders(obj);
		_create_sinks(obj);
		obj->graph_state = ResourcesAcquired;
	}
}

bool_t ms_media_player_start(MSMediaPlayer *obj) {
	if (!obj->is_open) {
		ms_error("Cannot start playing. No file has been opened");
		return FALSE;
	}
	if (obj->graph_state < GraphReady) {
		ms_media_player_prepare(obj);
		if (!_link_all(obj)) {
			ms_error("Could not build playing graph");
			_destroy_graph(obj);
			return FALSE;
		}
		ms_filter_add_notify_callback(obj->player, _eof_filter_notify_cb, obj, FALSE);
		ms_filter_call_method(obj->player, MS_PLAYER_SET_LOOP, &obj->loop_interval);
		if (obj->snd_card) {
			ms_snd_card_notify_audio_session_activated(obj->snd_card, TRUE);
		}
		obj->ticker = ms_ticker_new();
		ms_ticker_set_name(obj->ticker, "Player");
		ms_ticker_attach(obj->ticker, obj->player);
		obj->graph_state = GraphReady;
	}

	if (ms_filter_call_method_noarg(obj->player, MS_PLAYER_START) == -1) {
		ms_error("Could not play %s. Playing filter failed to start", obj->filename);
		return FALSE;
	}
	return TRUE;
}

void ms_media_player_stop(MSMediaPlayer *obj) {
	int seek_pos = 0;
	if (obj->is_open && obj->player) {
		ms_message("MSMediaPlayer: stopping playback.");
		ms_filter_call_method_noarg(obj->player, MS_PLAYER_PAUSE);
		ms_filter_call_method(obj->player, MS_PLAYER_SEEK_MS, &seek_pos);
	}
	if (obj->graph_state == GraphReady) {
		ms_ticker_detach(obj->ticker, obj->player);
		ms_ticker_destroy(obj->ticker);

		_unlink_all(obj);
		_destroy_graph(obj);
		obj->graph_state = Uninit;
	}
}

void ms_media_player_pause(MSMediaPlayer *obj) {
	if (obj->is_open) {
		ms_message("MSMediaPlayer: pausing playback.");
		ms_filter_call_method_noarg(obj->player, MS_PLAYER_PAUSE);
	}
}

bool_t ms_media_player_seek(MSMediaPlayer *obj, int seek_pos_ms) {
	if (obj->is_open) {
		ms_message("MSMediaPlayer: seek to %d ms", seek_pos_ms);
		return ms_filter_call_method(obj->player, MS_PLAYER_SEEK_MS, &seek_pos_ms) == 0 ? TRUE : FALSE;
	} else {
		ms_message("MSMediaPlayer: cannot seek: no file opened");
		return FALSE;
	}
}

MSPlayerState ms_media_player_get_state(MSMediaPlayer *obj) {
	if (obj->is_open) {
		MSPlayerState state;
		ms_filter_call_method(obj->player, MS_PLAYER_GET_STATE, &state);
		return state;
	} else {
		return MSPlayerClosed;
	}
}

int ms_media_player_get_duration(MSMediaPlayer *obj) {
	int duration;
	if (!obj->is_open) {
		ms_error("Could not get duration. No file is open");
		return -1;
	}
	if (ms_filter_call_method(obj->player, MS_PLAYER_GET_DURATION, &duration) == -1) {
		ms_error("Could not get duration");
		return -1;
	}
	return duration;
}

int ms_media_player_get_current_position(MSMediaPlayer *obj) {
	int position;
	if (!obj->is_open) {
		ms_error("Could not get position. No file is open");
		return -1;
	}
	if (ms_filter_call_method(obj->player, MS_PLAYER_GET_CURRENT_POSITION, &position) == -1) {
		ms_error("Could not get position");
		return -1;
	}
	return position;
}

void ms_media_player_set_eof_callback(MSMediaPlayer *obj, MSMediaPlayerEofCallback cb, void *user_data) {
	ms_mutex_lock(&obj->cb_access);
	obj->eof_cb = cb;
	obj->user_data_cb = user_data;
	ms_mutex_unlock(&obj->cb_access);
}

void ms_media_player_set_loop(MSMediaPlayer *obj, int loop_interval_ms) {
	obj->loop_interval = loop_interval_ms;
	if (obj->is_open) {
		ms_filter_call_method(obj->player, MS_PLAYER_SET_LOOP, &obj->loop_interval);
	}
}

bool_t ms_media_player_matroska_supported(void) {
#ifdef HAVE_MATROSKA
	return TRUE;
#else
	return FALSE;
#endif
}

MSFileFormat ms_media_player_get_file_format(const MSMediaPlayer *obj) {
	return obj->format;
}

/* Private functions */
static bool_t _get_format(const char *filepath, MSFileFormat *format) {
	FourCC four_cc;
	bctbx_vfs_file_t *fp = bctbx_file_open(bctbx_vfs_get_default(), filepath, "rb");
	if (fp == NULL) {
		ms_error("Could not open %s: %s", filepath, strerror(errno));
		goto err;
	}
	if (bctbx_file_read2(fp, four_cc, sizeof(FourCC)) == BCTBX_VFS_ERROR) {
		ms_error("Could not read the FourCC of %s: %s", filepath, strerror(errno));
		goto err;
	}
	*format = four_cc_to_file_format(four_cc);
	bctbx_file_close(fp);
	return TRUE;

err:
	if (fp) bctbx_file_close(fp);
	*format = MS_FILE_FORMAT_UNKNOWN;
	return FALSE;
}

static void _create_decoders(MSMediaPlayer *obj) {
	int sample_rate = 8000, nchannels = 1;
	switch (obj->format) {
		case MS_FILE_FORMAT_WAVE:
			ms_filter_call_method(obj->player, MS_FILTER_GET_SAMPLE_RATE, &sample_rate);
			ms_filter_call_method(obj->player, MS_FILTER_GET_NCHANNELS, &nchannels);
			obj->audio_pin_fmt.pin = 0;
			obj->audio_pin_fmt.fmt = ms_factory_get_audio_format(obj->factory, "pcm", sample_rate, nchannels, NULL);
			break;
		case MS_FILE_FORMAT_MATROSKA:
			obj->audio_pin_fmt.pin = 1;
			obj->video_pin_fmt.pin = 0;
			ms_filter_call_method(obj->player, MS_FILTER_GET_OUTPUT_FMT, &obj->audio_pin_fmt);
			ms_filter_call_method(obj->player, MS_FILTER_GET_OUTPUT_FMT, &obj->video_pin_fmt);
			if (obj->audio_pin_fmt.fmt) {
				obj->audio_decoder = ms_factory_create_decoder(obj->factory, obj->audio_pin_fmt.fmt->encoding);
				if (obj->audio_decoder == NULL) {
					ms_error("Could not create audio decoder for %s", obj->audio_pin_fmt.fmt->encoding);
					obj->audio_pin_fmt.fmt = NULL;
				} else {
					sample_rate = obj->audio_pin_fmt.fmt->rate;
					nchannels = obj->audio_pin_fmt.fmt->nchannels;
					ms_filter_call_method(obj->audio_decoder, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
					ms_filter_call_method(obj->audio_decoder, MS_FILTER_SET_NCHANNELS, &nchannels);
					if (strcmp(obj->audio_pin_fmt.fmt->encoding, "opus") == 0) {
						// Disable PLC in Opus decoder to prevent white noise when pausing media player
						const char *fmtp = "plc=0";
						ms_filter_call_method(obj->audio_decoder, MS_FILTER_ADD_FMTP, (void *)fmtp);
					}
				}
			}
			if (obj->video_pin_fmt.fmt) {
				obj->video_decoder = ms_factory_create_decoder(obj->factory, obj->video_pin_fmt.fmt->encoding);
				if (obj->video_decoder == NULL) {
					ms_error("Could not create video decoder for %s", obj->video_pin_fmt.fmt->encoding);
					obj->video_pin_fmt.fmt = NULL;
				}
			}
			break;
		default:
			break;
	}
}

static void _create_sinks(MSMediaPlayer *obj) {
	int sink_sample_rate, sample_rate, sink_nchannels, nchannels;
	bool_t need_resampler = FALSE;
	if (obj->audio_pin_fmt.fmt && obj->snd_card) {
		sink_sample_rate = sample_rate = obj->audio_pin_fmt.fmt->rate;
		sink_nchannels = nchannels = obj->audio_pin_fmt.fmt->nchannels;
		if ((obj->audio_sink = ms_snd_card_create_writer(obj->snd_card))) {
			if (ms_filter_call_method(obj->audio_sink, MS_FILTER_SET_SAMPLE_RATE, &sink_sample_rate) == -1) {
				ms_warning("The sound card (%s) does not support %dHz", obj->snd_card->name, sample_rate);
				if (ms_filter_call_method(obj->audio_sink, MS_FILTER_GET_SAMPLE_RATE, &sink_sample_rate) == -1) {
					ms_warning("Unable to get the sample rate expected by the sound card. Falling back to 8000Hz.");
					sink_sample_rate = 8000;
				}
				need_resampler = need_resampler || (sink_sample_rate != sample_rate);
			}
			if (ms_filter_call_method(obj->audio_sink, MS_FILTER_SET_NCHANNELS, &nchannels) == -1) {
				ms_warning("The sound card (%s) does not support %d channels", obj->snd_card->name, nchannels);
				if (ms_filter_call_method(obj->audio_sink, MS_FILTER_GET_NCHANNELS, &sink_nchannels) == -1) {
					ms_warning(
					    "Unable to get the number of channels expected by the sound card. Falling back to 1 channel.");
					sink_nchannels = 1;
				}
				need_resampler = need_resampler || (sink_nchannels != nchannels);
			}
			if (need_resampler) {
				ms_message("Resampling from (%dHz, %dch) to (%dHz, %dch)", sample_rate, nchannels, sink_sample_rate,
				           sink_nchannels);
				obj->resampler = ms_factory_create_filter(obj->factory, MS_RESAMPLE_ID);
				ms_filter_call_method(obj->resampler, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
				ms_filter_call_method(obj->resampler, MS_FILTER_SET_OUTPUT_SAMPLE_RATE, &sink_sample_rate);
				ms_filter_call_method(obj->resampler, MS_FILTER_SET_NCHANNELS, &nchannels);
				ms_filter_call_method(obj->resampler, MS_FILTER_SET_OUTPUT_NCHANNELS, &sink_nchannels);
			}
			ms_filter_call_method(obj->audio_sink, MS_FILTER_SET_NCHANNELS, &nchannels);
		} else {
			ms_error("Could not create audio sink. Soundcard=%s", obj->snd_card->name);
		}
	}
	if (obj->video_pin_fmt.fmt) {
		if (obj->video_display) {
			obj->video_sink = ms_factory_create_filter_from_name(obj->factory, obj->video_display);
			if (obj->video_sink) {
				if (obj->window_id)
					ms_filter_call_method(obj->video_sink, MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID, &obj->window_id);
			} else {
				ms_error("Could not create video sink: %s", obj->video_display);
			}
		}
	}
}

static void _destroy_graph(MSMediaPlayer *obj) {
	ms_filter_destroy_and_reset_if_not_null(obj->player);
	ms_filter_destroy_and_reset_if_not_null(obj->audio_decoder);
	ms_filter_destroy_and_reset_if_not_null(obj->video_decoder);
	ms_filter_destroy_and_reset_if_not_null(obj->audio_sink);
	ms_filter_destroy_and_reset_if_not_null(obj->video_sink);
	ms_filter_destroy_and_reset_if_not_null(obj->resampler);
	obj->audio_pin_fmt.fmt = NULL;
	obj->video_pin_fmt.fmt = NULL;
}

static bool_t _link_all(MSMediaPlayer *obj) {
	MSConnectionHelper helper;
	if (obj->player == NULL) {
		ms_error("Could not link graph. There is no playing filter");
		return FALSE;
	}
	if (obj->audio_sink == NULL && obj->video_sink == NULL) {
		ms_error("Could not link graph. There is neither audio sink nor video sink");
		return FALSE;
	}
	if (obj->audio_pin_fmt.fmt && obj->audio_sink) {
		ms_connection_helper_start(&helper);
		ms_connection_helper_link(&helper, obj->player, -1, obj->audio_pin_fmt.pin);
		if (obj->audio_decoder) ms_connection_helper_link(&helper, obj->audio_decoder, 0, 0);
		if (obj->resampler) ms_connection_helper_link(&helper, obj->resampler, 0, 0);
		ms_connection_helper_link(&helper, obj->audio_sink, 0, -1);
	}
	if (obj->video_pin_fmt.fmt && obj->video_sink) {
		ms_connection_helper_start(&helper);
		ms_connection_helper_link(&helper, obj->player, -1, obj->video_pin_fmt.pin);
		if (obj->video_decoder) ms_connection_helper_link(&helper, obj->video_decoder, 0, 0);
		ms_connection_helper_link(&helper, obj->video_sink, 0, -1);
	}
	return TRUE;
}

static void _unlink_all(MSMediaPlayer *obj) {
	MSConnectionHelper helper;
	if (obj->audio_pin_fmt.fmt && obj->audio_sink) {
		ms_connection_helper_start(&helper);
		ms_connection_helper_unlink(&helper, obj->player, -1, obj->audio_pin_fmt.pin);
		if (obj->audio_decoder) ms_connection_helper_unlink(&helper, obj->audio_decoder, 0, 0);
		if (obj->resampler) ms_connection_helper_unlink(&helper, obj->resampler, 0, 0);
		ms_connection_helper_unlink(&helper, obj->audio_sink, 0, -1);
	}
	if (obj->video_pin_fmt.fmt && obj->video_sink) {
		ms_connection_helper_start(&helper);
		ms_connection_helper_unlink(&helper, obj->player, -1, obj->video_pin_fmt.pin);
		if (obj->video_decoder) ms_connection_helper_unlink(&helper, obj->video_decoder, 0, 0);
		ms_connection_helper_unlink(&helper, obj->video_sink, 0, -1);
	}
}

static void _eof_filter_notify_cb(void *userdata, struct _MSFilter *f, unsigned int id, BCTBX_UNUSED(void *arg)) {
	MSMediaPlayer *obj = (MSMediaPlayer *)userdata;
	ms_mutex_lock(&obj->cb_access);
	if (f == obj->player && id == MS_PLAYER_EOF && obj->eof_cb != NULL) {
		obj->eof_cb(obj->user_data_cb);
	}
	ms_mutex_unlock(&obj->cb_access);
}

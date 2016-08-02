#include "mediastreamer2/msmediaplayer.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msextdisplay.h"
#include "../audiofilters/waveheader.h"

#ifdef _MSC_VER
#include <mmsystem.h>
#include <mmreg.h>
#endif

#define ms_filter_destroy_and_reset(obj) \
	ms_filter_destroy(obj); \
	obj = NULL

#define ms_filter_destroy_and_reset_if_not_null(obj) \
	if(obj != NULL) { \
		ms_filter_destroy(obj); \
		obj = NULL; \
	}

#define ms_free_if_not_null(obj) if(obj != NULL) ms_free(obj)

typedef uint8_t FourCC[4];

typedef struct _FormatDesc {
	MSFileFormat format;
	FourCC four_cc;
} FormatDesc;

static const FormatDesc _format_desc_list[] = {
	{	MS_FILE_FORMAT_WAVE		,	{ 'R', 'I', 'F', 'F' }		},
	{	MS_FILE_FORMAT_MATROSKA	,	{ 0x1a, 0x45, 0xdf, 0xa3 }	},
	{	MS_FILE_FORMAT_UNKNOWN		,	{ 0x0, 0x0, 0x0, 0x0 }		},
};

static bool_t four_cc_compare(const FourCC arg1, const FourCC arg2);
static MSFileFormat four_cc_to_file_format(const FourCC four_cc);

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
	return arg1[0] == arg2[0]
			&& arg1[1] == arg2[1]
			&& arg1[2] == arg2[2]
			&& arg1[3] == arg2[3];
}

static MSFileFormat four_cc_to_file_format(const FourCC four_cc) {
	int i;
	for(i=0; _format_desc_list[i].format != MS_FILE_FORMAT_UNKNOWN; i++) {
		if(four_cc_compare(four_cc, _format_desc_list[i].four_cc)) {
			return _format_desc_list[i].format;
		}
	}
	return MS_FILE_FORMAT_UNKNOWN;
}

MSMediaPlayer *ms_media_player_new(MSFactory* factory, MSSndCard *snd_card, const char *video_display_name, void *window_id) {
	MSMediaPlayer *obj = (MSMediaPlayer *)ms_new0(MSMediaPlayer, 1);
	obj->ticker = ms_ticker_new();
	ms_mutex_init(&obj->cb_access, NULL);
	obj->snd_card = snd_card;
	if(video_display_name != NULL && strlen(video_display_name) > 0) {
		obj->video_display = ms_strdup(video_display_name);
		obj->window_id = window_id;
	}
	obj->factory = factory;
	obj->loop_interval = -1;
	return obj;
}

void ms_media_player_free(MSMediaPlayer *obj) {
	ms_media_player_close(obj);
	ms_ticker_destroy(obj->ticker);
	ms_free_if_not_null(obj->video_display);
	ms_free(obj);
}

void * ms_media_player_get_window_id(const MSMediaPlayer *obj) {
	return obj->window_id;
}

bool_t ms_media_player_open(MSMediaPlayer *obj, const char *filepath) {
	wave_header_t header;
	int fd;
	char *tmp;
	ms_message("Opening %s", filepath);
	if(access(filepath, F_OK) != 0) {
		ms_error("Cannot open %s. File does not exist", filepath);
		return FALSE;
	}
	if(!_get_format(filepath, &obj->format)) {
		ms_error("Fails to detect file format of %s", filepath);
		return FALSE;
	}
	switch(obj->format) {
	case MS_FILE_FORMAT_WAVE:
		fd = open(filepath, O_RDONLY);
		if(fd == -1) {
			ms_error("Cannot open %s", filepath);
			return FALSE;
		}
		if(ms_read_wav_header_from_fd(&header, fd) == -1) {
			ms_error("Cannot open %s. Invalid WAV format", filepath);
			return FALSE;
		}
		close(fd);
		if(wave_header_get_format_type(&header) != WAVE_FORMAT_PCM) {
			ms_error("Cannot open %s. Codec not supported", filepath);
			return FALSE;
		}
		obj->player = ms_factory_create_filter(obj->factory, MS_FILE_PLAYER_ID);
		break;
	case MS_FILE_FORMAT_MATROSKA:
		if((obj->player = ms_factory_create_filter(obj->factory, MS_MKV_PLAYER_ID)) == NULL) {
			ms_error("Cannot open %s. Matroska file support is disabled", filepath);
			return FALSE;
		}
		break;
	case MS_FILE_FORMAT_UNKNOWN:
		ms_error("Cannot open %s. Unknown format", filepath);
		return FALSE;
	}
	tmp = ms_strdup(filepath);
	if(ms_filter_call_method(obj->player, MS_PLAYER_OPEN, tmp) == -1) {
		ms_error("Cannot open %s", filepath);
		ms_free(tmp);
		ms_filter_destroy(obj->player);
		return FALSE;
	}
	ms_free(tmp);
	_create_decoders(obj);
	_create_sinks(obj);
	if(!_link_all(obj)) {
		ms_error("Cannot open %s. Could not build playing graph", filepath);
		_destroy_graph(obj);
		return FALSE;
	}
	ms_filter_add_notify_callback(obj->player, _eof_filter_notify_cb, obj, TRUE);
	ms_filter_call_method(obj->player, MS_PLAYER_SET_LOOP, &obj->loop_interval);
	ms_ticker_attach(obj->ticker, obj->player);
	obj->is_open = TRUE;
	obj->filename = ms_strdup(filepath);
	return TRUE;
}

void ms_media_player_close(MSMediaPlayer *obj) {
	if(obj->is_open) {
		ms_ticker_detach(obj->ticker, obj->player);
		ms_filter_call_method_noarg(obj->player, MS_PLAYER_CLOSE);
		_unlink_all(obj);
		_destroy_graph(obj);
		obj->is_open = FALSE;
		ms_free(obj->filename); obj->filename = NULL;
	}
}

bool_t ms_media_player_start(MSMediaPlayer *obj) {
	if(!obj->is_open) {
		ms_error("Cannot start playing. No file has been opened");
		return FALSE;
	}
	if(ms_filter_call_method_noarg(obj->player, MS_PLAYER_START) == -1) {
		ms_error("Could not play %s. Playing filter failed to start", obj->filename);
		return FALSE;
	}
	return TRUE;
}

void ms_media_player_stop(MSMediaPlayer *obj) {
	int seek_pos = 0;
	if(obj->is_open) {
		ms_filter_call_method_noarg(obj->player, MS_PLAYER_PAUSE);
		ms_filter_call_method(obj->player, MS_PLAYER_SEEK_MS, &seek_pos);
	}
}

void ms_media_player_pause(MSMediaPlayer *obj) {
	if(obj->is_open) {
		ms_filter_call_method_noarg(obj->player, MS_PLAYER_PAUSE);
	}
}

bool_t ms_media_player_seek(MSMediaPlayer *obj, int seek_pos_ms) {
	if(obj->is_open) {
		return ms_filter_call_method(obj->player, MS_PLAYER_SEEK_MS, &seek_pos_ms) == 0 ? TRUE : FALSE;
	} else {
		return FALSE;
	}
}

MSPlayerState ms_media_player_get_state(MSMediaPlayer *obj) {
	if(obj->is_open) {
		MSPlayerState state;
		ms_filter_call_method(obj->player, MS_PLAYER_GET_STATE, &state);
		return state;
	} else {
		return MSPlayerClosed;
	}
}

int ms_media_player_get_duration(MSMediaPlayer *obj) {
	int duration;
	if(!obj->is_open) {
		ms_error("Could not get duration. No file is open");
		return -1;
	}
	if(ms_filter_call_method(obj->player, MS_PLAYER_GET_DURATION, &duration) == -1) {
		ms_error("Could not get duration");
		return -1;
	}
	return duration;
}

int ms_media_player_get_current_position(MSMediaPlayer *obj) {
	int position;
	if(!obj->is_open) {
		ms_error("Could not get position. No file is open");
		return -1;
	}
	if(ms_filter_call_method(obj->player, MS_PLAYER_GET_CURRENT_POSITION, &position) == -1) {
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
	if(obj->is_open) {
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
	size_t data_read;
	FILE *file = fopen(filepath, "rb");
	if(file == NULL) {
		ms_error("Could not open %s: %s", filepath, strerror(errno));
		goto err;
	}
	data_read = fread(four_cc, 4, 1, file);
	if (data_read == 0) {
		const char *error_msg;
		if (ferror(file)) {
			error_msg = strerror(errno);
		} else if (feof(file)) {
			error_msg = "end of file reached";
		} else {
			error_msg = "unknown error";
		}
		ms_error("Could not read the FourCC of %s: %s", filepath, error_msg);
		goto err;
	}
	*format = four_cc_to_file_format(four_cc);
	fclose(file);
	return TRUE;

err:
	if (file) fclose(file);
	*format = MS_FILE_FORMAT_UNKNOWN;
	return FALSE;
}

static void _create_decoders(MSMediaPlayer *obj) {
	int sample_rate, nchannels;
	switch(obj->format) {
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
		if(obj->audio_pin_fmt.fmt) {
			obj->audio_decoder = ms_factory_create_decoder(obj->factory, obj->audio_pin_fmt.fmt->encoding);
			if(obj->audio_decoder == NULL) {
				ms_error("Could not create audio decoder for %s", obj->audio_pin_fmt.fmt->encoding);
				obj->audio_pin_fmt.fmt = NULL;
			} else {
				sample_rate = obj->audio_pin_fmt.fmt->rate;
				nchannels = obj->audio_pin_fmt.fmt->nchannels;
				ms_filter_call_method(obj->audio_decoder, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
				ms_filter_call_method(obj->audio_decoder, MS_FILTER_SET_NCHANNELS, &nchannels);
			}
		}
		if(obj->video_pin_fmt.fmt) {
			obj->video_decoder = ms_factory_create_decoder(obj->factory, obj->video_pin_fmt.fmt->encoding);
			if(obj->video_decoder == NULL) {
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
	if(obj->audio_pin_fmt.fmt && obj->snd_card) {
		sample_rate = obj->audio_pin_fmt.fmt->rate;
		nchannels = obj->audio_pin_fmt.fmt->nchannels;
		if((obj->audio_sink = ms_snd_card_create_writer(obj->snd_card))) {
			if (ms_filter_call_method(obj->audio_sink, MS_FILTER_SET_SAMPLE_RATE, &sample_rate) == -1) {
				ms_warning("The sound card (%s) does not support %dHz", obj->snd_card->name, sample_rate);
				ms_filter_call_method(obj->audio_sink, MS_FILTER_GET_SAMPLE_RATE, &sink_sample_rate);
				need_resampler = TRUE;
			}
			if (ms_filter_call_method(obj->audio_sink, MS_FILTER_SET_NCHANNELS, &nchannels) == -1) {
				ms_warning("The sound card (%s) does not support %d channels", obj->snd_card->name, nchannels);
				ms_filter_call_method(obj->audio_sink, MS_FILTER_GET_NCHANNELS, &sink_nchannels);
				need_resampler = TRUE;
			}
			if (need_resampler == TRUE) {
				ms_message("Resampling to %dHz", sink_sample_rate);
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
	if(obj->video_pin_fmt.fmt) {
		if(obj->video_display) {
			obj->video_sink = ms_factory_create_filter_from_name(obj->factory, obj->video_display);
			if(obj->video_sink) {
				if(obj->window_id) ms_filter_call_method(obj->video_sink, MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID, &obj->window_id);
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
	if(obj->player == NULL) {
		ms_error("Could not link graph. There is no playing filter");
		return FALSE;
	}
	if(obj->audio_sink == NULL && obj->video_sink == NULL) {
		ms_error("Could not link graph. There is neither audio sink nor video sink");
		return FALSE;
	}
	if(obj->audio_pin_fmt.fmt && obj->audio_sink) {
		ms_connection_helper_start(&helper);
		ms_connection_helper_link(&helper, obj->player, -1, obj->audio_pin_fmt.pin);
		if(obj->audio_decoder) ms_connection_helper_link(&helper, obj->audio_decoder, 0, 0);
		if(obj->resampler) ms_connection_helper_link(&helper, obj->resampler, 0, 0);
		ms_connection_helper_link(&helper, obj->audio_sink, 0, -1);
	}
	if(obj->video_pin_fmt.fmt && obj->video_sink) {
		ms_connection_helper_start(&helper);
		ms_connection_helper_link(&helper, obj->player, -1, obj->video_pin_fmt.pin);
		if(obj->video_decoder) ms_connection_helper_link(&helper, obj->video_decoder, 0 , 0);
		ms_connection_helper_link(&helper, obj->video_sink, 0, -1);
	}
	return TRUE;
}

static void _unlink_all(MSMediaPlayer *obj) {
	MSConnectionHelper helper;
	if(obj->audio_pin_fmt.fmt && obj->audio_sink) {
		ms_connection_helper_start(&helper);
		ms_connection_helper_unlink(&helper, obj->player, -1, obj->audio_pin_fmt.pin);
		if(obj->audio_decoder) ms_connection_helper_unlink(&helper, obj->audio_decoder, 0, 0);
		if(obj->resampler) ms_connection_helper_unlink(&helper, obj->resampler, 0, 0);
		ms_connection_helper_unlink(&helper, obj->audio_sink, 0, -1);
	}
	if(obj->video_pin_fmt.fmt && obj->video_sink) {
		ms_connection_helper_start(&helper);
		ms_connection_helper_unlink(&helper, obj->player, -1, obj->video_pin_fmt.pin);
		if(obj->video_decoder) ms_connection_helper_unlink(&helper, obj->video_decoder, 0 , 0);
		ms_connection_helper_unlink(&helper, obj->video_sink, 0, -1);
	}
}

static void _eof_filter_notify_cb(void *userdata, struct _MSFilter *f, unsigned int id, void *arg) {
	MSMediaPlayer *obj = (MSMediaPlayer *)userdata;
	ms_mutex_lock(&obj->cb_access);
	if(f == obj->player && id == MS_PLAYER_EOF && obj->eof_cb != NULL) {
		obj->eof_cb(obj->user_data_cb);
	}
	ms_mutex_unlock(&obj->cb_access);
}

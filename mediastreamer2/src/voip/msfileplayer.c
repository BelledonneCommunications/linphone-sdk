#include "../../include/mediastreamer2/fileplayer.h"
#include "../../include/mediastreamer2/msfilter.h"
#include "../../include/mediastreamer2/msticker.h"
#include "../audiofilters/waveheader.h"

#define ms_filter_destroy_and_reset(obj) \
	ms_filter_destroy(obj); \
	obj = NULL

#define ms_filter_destroy_and_reset_if_not_null(obj) \
	if(obj != NULL) { \
		ms_filter_destroy(obj); \
		obj = NULL; \
	}

#define ms_free_if_not_null(obj) if(obj != NULL) ms_free(obj)

typedef enum _FileFormat {
	FILE_FORMAT_UNKNOWN,
	FILE_FORMAT_WAVE,
	FILE_FORMAT_MATROSKA
} FileFormat;

typedef uint8_t FourCC[4];

typedef struct _FormatDesc {
	FileFormat format;
	FourCC four_cc;
} FormatDesc;

static const FormatDesc _format_desc_list[] = {
	{	FILE_FORMAT_WAVE		,	{ 'R', 'I', 'F', 'F' }		},
	{	FILE_FORMAT_MATROSKA	,	{ 0x1a, 0x45, 0xdf, 0xa3 }	},
	{	FILE_FORMAT_UNKNOWN		,	{ 0x0, 0x0, 0x0, 0x0 }		},
};

static bool_t four_cc_compare(const FourCC arg1, const FourCC arg2);
static FileFormat four_cc_to_file_format(const FourCC four_cc);

struct _MSFilePlayer {
	MSFilter *player;
	MSFilter *audio_decoder;
	MSFilter *audio_sink;
	MSFilter *video_decoder;
	MSFilter *video_sink;
	MSFilter *resampler;
	MSPinFormat audio_pin_fmt;
	MSPinFormat video_pin_fmt;
	MSTicker *ticker;
	FileFormat format;
	bool_t is_open;
	char *filename;
	MSFilePlayerEofCallback eof_cb;
	void *user_data_cb;
	ms_mutex_t cb_access;
	MSSndCard *snd_card;
	char *video_display;
};

static bool_t _get_format(const char *filepath, FileFormat *format);
static void _eof_filter_notify_cb(void *userdata, struct _MSFilter *f, unsigned int id, void *arg);
static void _create_decoders(MSFilePlayer *obj);
static void _create_sinks(MSFilePlayer *obj);
static void _destroy_graph(MSFilePlayer *obj);
static bool_t _link_all(MSFilePlayer *obj);
static void _unlink_all(MSFilePlayer *obj);

static bool_t four_cc_compare(const FourCC arg1, const FourCC arg2) {
	return arg1[0] == arg2[0]
			&& arg1[1] == arg2[1]
			&& arg1[2] == arg2[2]
			&& arg1[3] == arg2[3];
}

static FileFormat four_cc_to_file_format(const FourCC four_cc) {
	int i;
	for(i=0; _format_desc_list[i].format != FILE_FORMAT_UNKNOWN; i++) {
		if(four_cc_compare(four_cc, _format_desc_list[i].four_cc)) {
			return _format_desc_list[i].format;
		}
	}
	return FILE_FORMAT_UNKNOWN;
}

MSFilePlayer *ms_file_player_new(MSSndCard *snd_card, const char *video_display_name) {
	MSFilePlayer *obj = (MSFilePlayer *)ms_new0(MSFilePlayer, 1);
	obj->ticker = ms_ticker_new();
	ms_mutex_init(&obj->cb_access, NULL);
	obj->snd_card = snd_card;
	if(video_display_name != NULL && strlen(video_display_name) > 0) {
		obj->video_display = ms_strdup(video_display_name);
	}
	return obj;
}

void ms_file_player_free(MSFilePlayer *obj) {
	ms_file_player_close(obj);
	ms_ticker_destroy(obj->ticker);
	ms_free_if_not_null(obj->video_display);
	ms_free(obj);
}

bool_t ms_file_player_open(MSFilePlayer *obj, const char *filepath) {
	wave_header_t header;
	int fd;
	char *tmp;
	ms_message("Openning %s", filepath);
	if(access(filepath, F_OK) != 0) {
		ms_error("Cannot open %s. File does not exist", filepath);
		return FALSE;
	}
	if(!_get_format(filepath, &obj->format)) {
		ms_error("Fails to detect file format of %s", filepath);
		return FALSE;
	}
	switch(obj->format) {
	case FILE_FORMAT_WAVE:
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
		obj->player = ms_filter_new(MS_FILE_PLAYER_ID);
		break;
	case FILE_FORMAT_MATROSKA:
		if((obj->player = ms_filter_new(MS_MKV_PLAYER_ID)) == NULL) {
			ms_error("Cannot open %s. Matroska file support is disabled", filepath);
			return FALSE;
		}
		break;
	case FILE_FORMAT_UNKNOWN:
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
	ms_ticker_attach(obj->ticker, obj->player);
	obj->is_open = TRUE;
	obj->filename = ms_strdup(filepath);
	return TRUE;
}

void ms_file_player_close(MSFilePlayer *obj) {
	if(obj->is_open) {
		ms_ticker_detach(obj->ticker, obj->player);
		ms_filter_call_method_noarg(obj->player, MS_PLAYER_CLOSE);
		_unlink_all(obj);
		_destroy_graph(obj);
		obj->is_open = FALSE;
		ms_free(obj->filename); obj->filename = NULL;
	}
}

bool_t ms_file_player_start(MSFilePlayer *obj) {
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

void ms_file_player_stop(MSFilePlayer *obj) {
	int seek_pos = 0;
	if(obj->is_open) {
		ms_filter_call_method_noarg(obj->player, MS_PLAYER_PAUSE);
		ms_filter_call_method(obj->player, MS_PLAYER_SEEK_MS, &seek_pos);
	}
}

void ms_file_player_pause(MSFilePlayer *obj) {
	if(obj->is_open) {
		ms_filter_call_method_noarg(obj->player, MS_PLAYER_PAUSE);
	}
}

bool_t ms_file_player_seek(MSFilePlayer *obj, int seek_pos_ms) {
	if(obj->is_open) {
		return ms_filter_call_method(obj->player, MS_PLAYER_SEEK_MS, &seek_pos_ms) == 0 ? TRUE : FALSE;
	} else {
		return FALSE;
	}
}

MSPlayerState ms_file_player_get_state(MSFilePlayer *obj) {
	if(obj->is_open) {
		MSPlayerState state;
		ms_filter_call_method(obj->player, MS_PLAYER_GET_STATE, &state);
		return state;
	} else {
		return MS_PLAYER_CLOSE;
	}
}

int ms_file_player_get_duration(MSFilePlayer *obj) {
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

int ms_file_player_get_current_position(MSFilePlayer *obj) {
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

void ms_file_player_set_eof_callback(MSFilePlayer *obj, MSFilePlayerEofCallback cb, void *user_data) {
	ms_mutex_lock(&obj->cb_access);
	obj->eof_cb = cb;
	obj->user_data_cb = user_data;
	ms_mutex_unlock(&obj->cb_access);
}

bool_t ms_file_player_matroska_supported(void) {
#ifdef HAVE_MATROSKA
	return TRUE;
#else
	return FALSE;
#endif
}

/* Private functions */
static bool_t _get_format(const char *filepath, FileFormat *format) {
	FourCC four_cc;
	size_t data_read;
	FILE *file = fopen(filepath, "r");
	if(file == NULL) {
		ms_error("Cannot open %s", filepath);
		*format = FILE_FORMAT_UNKNOWN;
		return FALSE;
	}
	data_read = fread(four_cc, 4, 1, file);
	fclose(file);
	if(data_read < 1) {
		*format = FILE_FORMAT_UNKNOWN;
		return FALSE;
	}
	*format = four_cc_to_file_format(four_cc);
	return TRUE;
}

static void _create_decoders(MSFilePlayer *obj) {
	int sample_rate, nchannels;
	switch(obj->format) {
	case FILE_FORMAT_WAVE:
		ms_filter_call_method(obj->player, MS_FILTER_GET_SAMPLE_RATE, &sample_rate);
		ms_filter_call_method(obj->player, MS_FILTER_GET_NCHANNELS, &nchannels);
		obj->audio_pin_fmt.pin = 0;
		obj->audio_pin_fmt.fmt = ms_factory_get_audio_format(ms_factory_get_fallback(), "pcm", sample_rate, nchannels, NULL);
		break;
	case FILE_FORMAT_MATROSKA:
		obj->audio_pin_fmt.pin = 1;
		obj->video_pin_fmt.pin = 0;
		ms_filter_call_method(obj->player, MS_FILTER_GET_OUTPUT_FMT, &obj->audio_pin_fmt);
		ms_filter_call_method(obj->player, MS_FILTER_GET_OUTPUT_FMT, &obj->video_pin_fmt);
		if(obj->audio_pin_fmt.fmt) {
			obj->audio_decoder = ms_factory_create_decoder(ms_factory_get_fallback(), obj->audio_pin_fmt.fmt->encoding);
			if(obj->audio_decoder == NULL) {
				ms_error("Could not create audio decoder for %s", obj->audio_pin_fmt.fmt->encoding);
				obj->audio_pin_fmt.fmt = NULL;
			}
			sample_rate = obj->audio_pin_fmt.fmt->rate;
			nchannels = obj->audio_pin_fmt.fmt->nchannels;
			ms_filter_call_method(obj->audio_decoder, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
			ms_filter_call_method(obj->audio_decoder, MS_FILTER_SET_NCHANNELS, &nchannels);
		}
		if(obj->video_pin_fmt.fmt) {
			obj->video_decoder = ms_factory_create_decoder(ms_factory_get_fallback(), obj->video_pin_fmt.fmt->encoding);
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

static void _create_sinks(MSFilePlayer *obj) {
	int sink_sample_rate, sample_rate, nchannels;
	if(obj->audio_pin_fmt.fmt && obj->snd_card) {
		sample_rate = obj->audio_pin_fmt.fmt->rate;
		nchannels = obj->audio_pin_fmt.fmt->nchannels;
		if((obj->audio_sink = ms_snd_card_create_writer(obj->snd_card))) {
			if(ms_filter_call_method(obj->audio_sink, MS_FILTER_SET_SAMPLE_RATE, &sample_rate) == -1) {
				ms_warning("The sound card (%s) does not support %dHz", obj->snd_card->name, sample_rate);
				ms_filter_call_method(obj->audio_sink, MS_FILTER_GET_SAMPLE_RATE, &sink_sample_rate);
				if(obj->audio_decoder == NULL) {
					ms_message("Resampling to %dHz", sink_sample_rate);
					obj->resampler = ms_filter_new(MS_RESAMPLE_ID);
					ms_filter_call_method(obj->resampler, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
					ms_filter_call_method(obj->resampler, MS_FILTER_SET_OUTPUT_SAMPLE_RATE, &sink_sample_rate);
					ms_filter_call_method(obj->resampler, MS_FILTER_SET_NCHANNELS, &nchannels);
				} else {
					ms_message("Setting output sampling rate of the audio decoder to %dHz", sink_sample_rate);
					ms_filter_call_method(obj->audio_decoder, MS_FILTER_SET_SAMPLE_RATE, &sink_sample_rate);
				}
			}
			ms_filter_call_method(obj->audio_sink, MS_FILTER_SET_NCHANNELS, &nchannels);
		} else {
			ms_error("Could not create audio sink. Soundcard=%s", obj->snd_card->name);
		}
	}
	if(obj->video_pin_fmt.fmt && obj->video_display) {
		obj->video_sink = ms_filter_new_from_name(obj->video_display);
		if(obj->video_sink == NULL) {
			ms_error("Could not create video sink: %s", obj->video_display);
		}
	}
}

static void _destroy_graph(MSFilePlayer *obj) {
	ms_filter_destroy_and_reset_if_not_null(obj->player);
	ms_filter_destroy_and_reset_if_not_null(obj->audio_decoder);
	ms_filter_destroy_and_reset_if_not_null(obj->video_decoder);
	ms_filter_destroy_and_reset_if_not_null(obj->audio_sink);
	ms_filter_destroy_and_reset_if_not_null(obj->video_sink);
	ms_filter_destroy_and_reset_if_not_null(obj->resampler);
	obj->audio_pin_fmt.fmt = NULL;
	obj->video_pin_fmt.fmt = NULL;
}

static bool_t _link_all(MSFilePlayer *obj) {
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

static void _unlink_all(MSFilePlayer *obj) {
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
	MSFilePlayer *obj = (MSFilePlayer *)userdata;
	ms_mutex_lock(&obj->cb_access);
	if(f == obj->player && id == MS_PLAYER_EOF && obj->eof_cb != NULL) {
		obj->eof_cb(obj->user_data_cb);
	}
	ms_mutex_unlock(&obj->cb_access);
}

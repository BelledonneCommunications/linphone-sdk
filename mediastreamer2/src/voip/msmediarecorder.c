#include "mediastreamer2/msmediarecorder.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msextdisplay.h"
#include "../audiofilters/waveheader.h"

#ifdef _MSC_VER
#include <mmsystem.h>
#include <mmreg.h>
#endif

#include "fd_portab.h" // keep this include at the last of the inclusion sequence!

//TODO: check if mkv is available. If not, we just do wav files. Check if video is available. if not, we just do sound files (wav or mvk).
// Check given file extension. If not wav nor mkv, do nothing. Make those things work together.



#define ms_filter_destroy_and_reset(obj) \
	ms_filter_destroy(obj); \
	obj = NULL

#define ms_filter_destroy_and_reset_if_not_null(obj) \
	if(obj != NULL) { \
		ms_filter_destroy(obj); \
		obj = NULL; \
	}

#define ms_free_if_not_null(obj) if(obj != NULL) ms_free(obj)

struct _MSMediaRecorder {
	MSFactory *factory;
	MSFilter *recorder;

	//Audio filters
	MSFilter *audio_source;
	MSFilter *resampler;
	MSFilter *audio_encoder;
	//Video filters
	MSFilter *video_source;
	MSFilter *video_encoder;
	MSFilter *video_sink;

	MSPinFormat audio_pin_fmt;
	MSPinFormat video_pin_fmt;
	MSTicker *ticker;
	MSFileFormat format;
	bool_t is_open;
	char *filename;
	MSSndCard *snd_card;
	MSWebCam *web_cam;
	char *video_display;
	void *window_id;
	char *video_codec;
	int device_orientation;
};

static void _create_encoders(MSMediaRecorder *obj, int device_orientation);
static void _create_sources(MSMediaRecorder *obj);
static void _set_pin_fmt(MSMediaRecorder *obj);
static void _destroy_graph(MSMediaRecorder *obj);
static bool_t _link_all(MSMediaRecorder *obj);
static void _unlink_all(MSMediaRecorder *obj);
static void _recorder_callback(void *ud, MSFilter *f, unsigned int id, void *arg);

const char *get_filename_ext(const char *filename) {
	const char *dot = strrchr(filename, '.');
	if(!dot || dot == filename) return "";
	return dot + 1;
}


MSMediaRecorder *ms_media_recorder_new(MSFactory* factory, MSSndCard *snd_card, MSWebCam *web_cam, const char *video_display_name, void *window_id, MSFileFormat format, const char *video_codec) {
	MSMediaRecorder *obj = (MSMediaRecorder *)ms_new0(MSMediaRecorder, 1);
	obj->ticker = ms_ticker_new();
	ms_ticker_set_name(obj->ticker, "Recorder");
	obj->snd_card = snd_card;
	obj->web_cam = web_cam;
	if(video_display_name != NULL && strlen(video_display_name) > 0) {
		obj->video_display = ms_strdup(video_display_name);
		obj->window_id = window_id;
	}
	obj->factory = factory;
	obj->format = format;
	if (video_codec != NULL) {
		obj->video_codec = ms_strdup(video_codec);
	}
	return obj;
}

void ms_media_recorder_free(MSMediaRecorder *obj) {
	ms_media_recorder_close(obj);
	ms_ticker_destroy(obj->ticker);
	ms_free_if_not_null(obj->video_display);
	ms_free_if_not_null(obj->video_codec);
	ms_free(obj);
}

void * ms_media_recorder_get_window_id(const MSMediaRecorder *obj) {
	return obj->window_id;
}

bool_t ms_media_recorder_open(MSMediaRecorder *obj, const char *filepath, int device_orientation) {
	const char *file_ext = get_filename_ext(filepath);
	if (!((strcmp(file_ext, "wav") == 0 && obj->format == MS_FILE_FORMAT_WAVE) || ((strcmp(file_ext, "mkv") == 0 && obj->format == MS_FILE_FORMAT_MATROSKA)))) {
		ms_error("file format and file extension do not match, was expecting %s and got %s for filename: %s", (obj->format == MS_FILE_FORMAT_WAVE ? "wav" : "mkv"), file_ext, filepath);
		return FALSE;
	}
	char *tmp;
	ms_message("Opening %s", filepath);
	if(access(filepath, F_OK | W_OK) == 0) {
		ms_warning("Removing existing file %s", filepath);
		remove(filepath);
	}
	switch(obj->format) {
	case MS_FILE_FORMAT_WAVE:
			if((obj->recorder = ms_factory_create_filter(obj->factory, MS_FILE_REC_ID)) == NULL) {
				ms_error("Cannot create recorder for %s.", filepath);
				return FALSE;
			}
		break;
	case MS_FILE_FORMAT_MATROSKA:
		if((obj->recorder = ms_factory_create_filter(obj->factory, MS_MKV_RECORDER_ID)) == NULL) {
			ms_error("Cannot create recorder for %s.", filepath);
			return FALSE;
		}
		break;
	case MS_FILE_FORMAT_UNKNOWN:
	default:
		ms_error("Cannot open %s. Unknown format", filepath);
		return FALSE;
	}
	tmp = ms_strdup(filepath);
	if(ms_filter_call_method(obj->recorder, MS_RECORDER_OPEN, tmp) == -1) {
		ms_error("Cannot open %s", filepath);
		ms_free(tmp);
		ms_filter_destroy(obj->recorder);
		return FALSE;
	}
	ms_free(tmp);
	_create_sources(obj);
	_set_pin_fmt(obj);
	_create_encoders(obj, device_orientation);
	if(!_link_all(obj)) {
		ms_error("Cannot open %s. Could not build playing graph", filepath);
		_destroy_graph(obj);
		return FALSE;
	}
	ms_ticker_attach(obj->ticker, obj->recorder);
	obj->is_open = TRUE;
	obj->filename = ms_strdup(filepath);
	return TRUE;
}

void ms_media_recorder_close(MSMediaRecorder *obj) {
	if(obj->is_open) {
		if (obj->video_encoder)
			ms_filter_remove_notify_callback(obj->recorder, _recorder_callback, obj);
		ms_filter_call_method_noarg(obj->recorder, MS_RECORDER_CLOSE);
		ms_ticker_detach(obj->ticker, obj->recorder);
		_unlink_all(obj);
		_destroy_graph(obj);
		obj->is_open = FALSE;
		ms_free(obj->filename); obj->filename = NULL;
	}
}

bool_t ms_media_recorder_start(MSMediaRecorder *obj) {
	if(!obj->is_open) {
		ms_error("Cannot start playing. No file has been opened");
		return FALSE;
	}
	if(ms_filter_call_method_noarg(obj->recorder, MS_RECORDER_START) == -1) {
		ms_error("Could not play %s. Playing filter failed to start", obj->filename);
		return FALSE;
	}
	return TRUE;
}

void ms_media_recorder_pause(MSMediaRecorder *obj) {
	if(obj->is_open) {
		ms_filter_call_method_noarg(obj->recorder, MS_RECORDER_PAUSE);
	}
}

MSRecorderState ms_media_recorder_get_state(MSMediaRecorder *obj) {
	if(obj->is_open) {
		MSRecorderState state;
		ms_filter_call_method(obj->recorder, MS_RECORDER_GET_STATE, &state);
		return state;
	} else {
		return MSRecorderClosed;
	}
}

bool_t ms_media_recorder_matroska_supported(void) {
#ifdef HAVE_MATROSKA
	return TRUE;
#else
	return FALSE;
#endif
}

MSFileFormat ms_media_recorder_get_file_format(const MSMediaRecorder *obj) {
	return obj->format;
}

void ms_media_recorder_remove_file(MSMediaRecorder *obj, const char *filepath) {
	ms_message("Removing %s.", filepath);
	if(access(filepath, F_OK | W_OK) == 0) {
		remove(filepath);
	} else {
		ms_warning("No existing file at %s, doing nothing.", filepath);
	}
}

static void _create_encoders(MSMediaRecorder *obj, int device_orientation) {
	// In short : if wave: no encoder. If mkv, "opus" for audio, "vp8" or "h264"
	int source_sample_rate, encoder_sample_rate, sample_rate, source_nchannels, nchannels;
	switch(obj->format) {
		case MS_FILE_FORMAT_MATROSKA:
			if(obj->snd_card) {
				obj->audio_encoder = ms_factory_create_encoder(obj->factory, obj->audio_pin_fmt.fmt->encoding);
				if(obj->audio_encoder == NULL) {
					ms_error("Could not create audio encoder for %s", obj->audio_pin_fmt.fmt->encoding);
					obj->audio_pin_fmt.fmt = NULL;
				} else {
					sample_rate = obj->audio_pin_fmt.fmt->rate;
					nchannels = obj->audio_pin_fmt.fmt->nchannels;
					ms_filter_call_method(obj->audio_encoder, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
					ms_filter_call_method(obj->audio_encoder, MS_FILTER_SET_NCHANNELS, &nchannels);
					ms_filter_call_method(obj->audio_encoder, MS_FILTER_GET_SAMPLE_RATE, &encoder_sample_rate);
					ms_filter_call_method(obj->audio_source, MS_FILTER_GET_SAMPLE_RATE, &source_sample_rate);
					ms_filter_call_method(obj->audio_source, MS_FILTER_GET_NCHANNELS, &source_nchannels);

					if (source_sample_rate != sample_rate || encoder_sample_rate != source_sample_rate || source_nchannels != nchannels) {
						ms_message("Resampling to %dHz and %d channels", source_sample_rate, nchannels);
						obj->resampler = ms_factory_create_filter(obj->factory, MS_RESAMPLE_ID);
						ms_filter_call_method(obj->resampler, MS_FILTER_SET_SAMPLE_RATE, &source_sample_rate);
						ms_filter_call_method(obj->resampler, MS_FILTER_SET_OUTPUT_SAMPLE_RATE, &encoder_sample_rate);
						ms_filter_call_method(obj->resampler, MS_FILTER_SET_NCHANNELS, &source_nchannels);
						ms_filter_call_method(obj->resampler, MS_FILTER_SET_OUTPUT_NCHANNELS, &nchannels);
					}
				}
				ms_message("Configuring MKV recorder with audio format %s",ms_fmt_descriptor_to_string(obj->audio_pin_fmt.fmt));
				ms_filter_call_method(obj->recorder, MS_FILTER_SET_INPUT_FMT, &obj->audio_pin_fmt);
			}
			if(obj->web_cam && obj->video_codec) {
				obj->video_encoder = ms_factory_create_encoder(obj->factory, obj->video_codec);
				if(obj->video_encoder == NULL) {
					ms_error("Could not create video encoder for %s", obj->video_codec);
					obj->video_pin_fmt.fmt = NULL;
				} else {
					if (ms_filter_has_method(obj->video_source, MS_VIDEO_CAPTURE_SET_DEVICE_ORIENTATION))
						ms_filter_call_method(obj->video_source, MS_VIDEO_CAPTURE_SET_DEVICE_ORIENTATION, &device_orientation);
					ms_filter_add_notify_callback(obj->recorder, _recorder_callback, obj, TRUE);
					float fps = 30;
					ms_filter_call_method(obj->video_source, MS_FILTER_SET_FPS, &fps);
					ms_filter_call_method(obj->video_encoder, MS_FILTER_SET_FPS, &fps);
					MSVideoSize video_size = MS_VIDEO_SIZE_VGA;
					ms_filter_call_method(obj->video_source, MS_FILTER_SET_VIDEO_SIZE, &video_size);
					ms_filter_call_method(obj->video_source, MS_FILTER_GET_VIDEO_SIZE, &video_size);
					ms_filter_call_method(obj->video_encoder, MS_FILTER_SET_VIDEO_SIZE, &video_size);
				}
				ms_message("Configuring MKV recorder with video format %s",ms_fmt_descriptor_to_string(obj->video_pin_fmt.fmt));
				ms_filter_call_method(obj->recorder, MS_FILTER_SET_INPUT_FMT, &obj->video_pin_fmt);
			}
			break;
		default:
			break;
	}
}

static void _create_sources(MSMediaRecorder *obj) {
	switch(obj->format) {
		case MS_FILE_FORMAT_MATROSKA:
			if(obj->web_cam && obj->video_codec) {
				obj->video_source = ms_web_cam_create_reader(obj->web_cam);
				if(obj->video_source) {
					if(obj->window_id) ms_filter_call_method(obj->video_source, MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID, &obj->window_id);
				} else {
					ms_error("Could not create video source: %s", obj->web_cam->name);
				}
			} else if(obj->snd_card) {
				if ((obj->audio_source = ms_snd_card_create_reader(obj->snd_card))) {
					if (ms_filter_has_method(obj->audio_source, MS_AUDIO_CAPTURE_ENABLE_AEC)) {
						bool_t aec_enabled = FALSE;
						ms_filter_call_method(obj->audio_source, MS_AUDIO_CAPTURE_ENABLE_AEC, &aec_enabled);
					}
					if (ms_filter_has_method(obj->audio_source, MS_AUDIO_CAPTURE_ENABLE_VOICE_REC)) {
						bool_t voice_recognition = TRUE;
						ms_filter_call_method(obj->audio_source, MS_AUDIO_CAPTURE_ENABLE_VOICE_REC, &voice_recognition);
					}
				} else {
					ms_error("Could not create audio source. Soundcard=%s", obj->snd_card->name);
				}
			}
			break;
		case MS_FILE_FORMAT_WAVE:
			if(obj->snd_card) {
				if ((obj->audio_source = ms_snd_card_create_reader(obj->snd_card))) {
					if (ms_filter_has_method(obj->audio_source, MS_AUDIO_CAPTURE_ENABLE_AEC)) {
						bool_t aec_enabled = FALSE;
						ms_filter_call_method(obj->audio_source, MS_AUDIO_CAPTURE_ENABLE_AEC, &aec_enabled);
					}
					if (ms_filter_has_method(obj->audio_source, MS_AUDIO_CAPTURE_ENABLE_VOICE_REC)) {
						bool_t voice_recognition = TRUE;
						ms_filter_call_method(obj->audio_source, MS_AUDIO_CAPTURE_ENABLE_VOICE_REC, &voice_recognition);
					}
				} else {
					ms_error("Could not create audio source. Soundcard=%s", obj->snd_card->name);
				}
			}
			break;
		default:
			break;
	}
}

static void _set_pin_fmt(MSMediaRecorder *obj) {
	int nchannels, sample_rate;
	switch(obj->format) {
		case MS_FILE_FORMAT_WAVE:
			ms_filter_call_method(obj->audio_source, MS_FILTER_GET_SAMPLE_RATE, &sample_rate);
			ms_filter_call_method(obj->audio_source, MS_FILTER_GET_NCHANNELS, &nchannels);
			ms_filter_call_method(obj->recorder, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
			ms_filter_call_method(obj->recorder, MS_FILTER_SET_NCHANNELS, &nchannels);
			obj->audio_pin_fmt.pin = 0;
			obj->audio_pin_fmt.fmt = ms_factory_get_audio_format(obj->factory, "pcm", sample_rate, nchannels, NULL);
			break;
		case MS_FILE_FORMAT_MATROSKA:
			if(obj->snd_card) {
				obj->audio_pin_fmt.pin = 0;
				ms_filter_call_method(obj->audio_source, MS_FILTER_GET_SAMPLE_RATE, &sample_rate);
				ms_filter_call_method(obj->audio_source, MS_FILTER_GET_NCHANNELS, &nchannels);
				ms_filter_call_method(obj->recorder, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
				ms_filter_call_method(obj->recorder, MS_FILTER_SET_NCHANNELS, &nchannels);
				obj->audio_pin_fmt.fmt = ms_factory_get_audio_format(obj->factory, "opus", 48000, nchannels, NULL);
			}
			if(obj->web_cam && obj->video_codec) {
				obj->video_pin_fmt.pin = 1;
				MSVideoSize video_size = MS_VIDEO_SIZE_VGA;
				float fps = 30;
				obj->video_pin_fmt.fmt = ms_factory_get_video_format(obj->factory, obj->video_codec, video_size, fps, NULL);
			}
			break;
		default:
			break;
	}
}

static void _destroy_graph(MSMediaRecorder *obj) {
	ms_filter_destroy_and_reset_if_not_null(obj->recorder);
	ms_filter_destroy_and_reset_if_not_null(obj->audio_encoder);
	ms_filter_destroy_and_reset_if_not_null(obj->video_encoder);
	ms_filter_destroy_and_reset_if_not_null(obj->audio_source);
	ms_filter_destroy_and_reset_if_not_null(obj->video_source);
	ms_filter_destroy_and_reset_if_not_null(obj->resampler);
	obj->audio_pin_fmt.fmt = NULL;
	obj->video_pin_fmt.fmt = NULL;
}

static bool_t _link_all(MSMediaRecorder *obj) {
	MSConnectionHelper helper;
	if(obj->recorder == NULL) {
		ms_error("Could not link graph. There is no recording filter");
		return FALSE;
	}
	if(obj->audio_source == NULL && obj->video_source == NULL) {
		ms_error("Could not link graph. There is neither audio nor video source");
		return FALSE;
	}

	if(obj->audio_pin_fmt.fmt && obj->audio_source) {
		ms_connection_helper_start(&helper);
		ms_connection_helper_link(&helper, obj->audio_source, -1, 0);
		if(obj->resampler) ms_connection_helper_link(&helper, obj->resampler, 0, 0);
		if(obj->audio_encoder) ms_connection_helper_link(&helper, obj->audio_encoder, 0, 0);
		ms_connection_helper_link(&helper, obj->recorder, obj->audio_pin_fmt.pin, -1);
	}
	if(obj->video_pin_fmt.fmt && obj->video_source) {
		ms_connection_helper_start(&helper);
		ms_connection_helper_link(&helper, obj->video_source, -1, 0);
		if(obj->video_encoder) ms_connection_helper_link(&helper, obj->video_encoder, 0, 0);
		ms_connection_helper_link(&helper, obj->recorder, obj->video_pin_fmt.pin, -1);
	}
	return TRUE;
}

static void _unlink_all(MSMediaRecorder *obj) {
	MSConnectionHelper helper;
	if(obj->audio_pin_fmt.fmt && obj->audio_source) {
		ms_connection_helper_start(&helper);
		ms_connection_helper_unlink(&helper, obj->audio_source, -1, 0);
		if(obj->resampler) ms_connection_helper_unlink(&helper, obj->resampler, 0, 0);
		if(obj->audio_encoder) ms_connection_helper_unlink(&helper, obj->audio_encoder, 0, 0);
		ms_connection_helper_unlink(&helper, obj->recorder, obj->audio_pin_fmt.pin, -1);
	}
	if(obj->video_pin_fmt.fmt && obj->video_source) {
		ms_connection_helper_start(&helper);
		ms_connection_helper_unlink(&helper, obj->video_source, -1, 0);
		if(obj->video_encoder) ms_connection_helper_unlink(&helper, obj->video_encoder, 0, 0);
		ms_connection_helper_unlink(&helper, obj->recorder, obj->video_pin_fmt.pin, -1);
	}
}

static void _recorder_callback(void *ud, MSFilter *f, unsigned int id, void *arg) {
	MSMediaRecorder *obj = (MSMediaRecorder *)ud;
	ms_filter_call_method_noarg(obj->video_encoder, MS_VIDEO_ENCODER_REQ_VFU);
}

/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006-2013 Belledonne Communications, Grenoble

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

#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/dtmfgen.h"
#include "mediastreamer2/msfileplayer.h"
#include "mediastreamer2/msfilerec.h"
#include "mediastreamer2/msrtp.h"
#include "mediastreamer2/mstonedetector.h"
#include "mediastreamer2_tester.h"
#include "mediastreamer2_tester_private.h"

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#else
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

static int sound_card_tester_before_all(void) {
	ms_init();
	ms_filter_enable_statistics(TRUE);
	return 0;
}

static int sound_card_tester_after_all(void) {
	ms_exit();
	return 0;
}

static void configure_resampler(MSFilter *resampler, MSFilter *from, MSFilter *to) {
	int from_rate = 0, to_rate = 0;
	int from_channels = 0, to_channels = 0;
	ms_filter_call_method(from, MS_FILTER_GET_SAMPLE_RATE, &from_rate);
	ms_filter_call_method(to, MS_FILTER_GET_SAMPLE_RATE, &to_rate);
	ms_filter_call_method(resampler, MS_FILTER_SET_SAMPLE_RATE, &from_rate);
	ms_filter_call_method(resampler, MS_FILTER_SET_OUTPUT_SAMPLE_RATE, &to_rate);
	ms_filter_call_method(from, MS_FILTER_GET_NCHANNELS, &from_channels);
	ms_filter_call_method(to, MS_FILTER_GET_NCHANNELS, &to_channels);
	if (from_channels == 0) from_channels = 1;
	if (to_channels == 0) to_channels = 1;
	ms_filter_call_method(resampler, MS_FILTER_SET_NCHANNELS, &from_channels);
	ms_filter_call_method(resampler, MS_FILTER_SET_OUTPUT_NCHANNELS, &to_channels);
}

static void dtmfgen_soundwrite(void) {
	MSConnectionHelper h;
	bool_t need_resampler = FALSE;
	unsigned int filter_mask = FILTER_MASK_VOIDSOURCE | FILTER_MASK_DTMFGEN | FILTER_MASK_SOUNDWRITE;
	int sample_rate = 8000;
	int nchannels = 1;
	bool_t send_silence = TRUE;

	ms_filter_reset_statistics();
	ms_tester_create_ticker();
	ms_tester_create_filters(filter_mask, ms_factory_get_fallback());
	ms_filter_call_method(ms_tester_voidsource, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
	ms_filter_call_method(ms_tester_voidsource, MS_FILTER_SET_NCHANNELS, &nchannels);
	ms_filter_call_method(ms_tester_voidsource, MS_VOID_SOURCE_SEND_SILENCE, &send_silence);
	ms_filter_call_method(ms_tester_dtmfgen, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
	ms_filter_call_method(ms_tester_dtmfgen, MS_FILTER_SET_NCHANNELS, &nchannels);
	if (ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_SET_SAMPLE_RATE, &sample_rate) != 0) {
		need_resampler = TRUE;
	}
	if (ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_SET_NCHANNELS, &nchannels) != 0) {
		need_resampler = TRUE;
	}
	if (need_resampler == TRUE) {
		ms_tester_create_filters(FILTER_MASK_RESAMPLER, ms_factory_get_fallback());
		configure_resampler(ms_tester_resampler, ms_tester_dtmfgen, ms_tester_soundwrite);
	}
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, ms_tester_voidsource, -1, 0);
	ms_connection_helper_link(&h, ms_tester_dtmfgen, 0, 0);
	if (need_resampler == TRUE) {
		ms_connection_helper_link(&h, ms_tester_resampler, 0, 0);
	}
	ms_connection_helper_link(&h, ms_tester_soundwrite, 0, -1);
	ms_ticker_attach(ms_tester_ticker, ms_tester_voidsource);

	ms_tester_tone_generation_loop();

	ms_ticker_detach(ms_tester_ticker, ms_tester_voidsource);
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, ms_tester_voidsource, -1, 0);
	ms_connection_helper_unlink(&h, ms_tester_dtmfgen, 0, 0);
	if (need_resampler == TRUE) {
		ms_connection_helper_unlink(&h, ms_tester_resampler, 0, 0);
	}
	ms_connection_helper_unlink(&h, ms_tester_soundwrite, 0, -1);
	if (need_resampler == TRUE) {
		ms_tester_destroy_filters(FILTER_MASK_RESAMPLER);
	}
	ms_filter_log_statistics();
	ms_tester_destroy_filters(filter_mask);
	ms_tester_destroy_ticker();
}

#define CHIMES_48000_STEREO_FILE_NAME		"sounds/chimes_48000_stereo.wav"
#define BIRD_44100_STEREO_FILE_NAME			"sounds/bird_44100_stereo.wav"
#define PUNCH_16000_STEREO_FILE_NAME		"sounds/punch_16000_stereo.wav"
#define PIANO_8000_STEREO_FILE_NAME			"sounds/piano_8000_stereo.wav"
#define NYLON_48000_MONO_FILE_NAME			"sounds/nylon_48000_mono.wav"
#define OWL_44100_MONO_FILE_NAME			"sounds/owl_44100_mono.wav"
#define LASERROCKET_16000_MONO_FILE_NAME	"sounds/laserrocket_16000_mono.wav"
#define ARPEGGIO_8000_MONO_FILE_NAME		"sounds/arpeggio_8000_mono.wav"

static void fileplay_eof(void *user_data, MSFilter *f, unsigned int event, void *event_data) {
	if (event == MS_FILE_PLAYER_EOF) {
		int *done = (int *)user_data;
		*done = TRUE;
	}
	MS_UNUSED(f), MS_UNUSED(event_data);
}

static void fileplay_soundwrite(const char *filename) {
	MSConnectionHelper h;
	bool_t need_resampler = FALSE;
	unsigned int filter_mask = FILTER_MASK_FILEPLAY | FILTER_MASK_SOUNDWRITE;
	int sample_rate = 8000;
	int nchannels = 1;
	int done = FALSE;
    int elapsed = 0;

	ms_filter_reset_statistics();
	ms_tester_create_ticker();
	ms_tester_create_filters(filter_mask, ms_factory_get_fallback());
	ms_filter_add_notify_callback(ms_tester_fileplay, fileplay_eof, &done,TRUE);
	ms_filter_call_method_noarg(ms_tester_fileplay, MS_FILE_PLAYER_CLOSE);
	ms_filter_call_method(ms_tester_fileplay, MS_FILE_PLAYER_OPEN, (void *)filename);
	ms_filter_call_method(ms_tester_fileplay, MS_FILTER_GET_SAMPLE_RATE, &sample_rate);
	ms_filter_call_method(ms_tester_fileplay, MS_FILTER_GET_NCHANNELS, &nchannels);
	if (ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_SET_SAMPLE_RATE, &sample_rate) != 0) {
		int soundwrite_sample_rate = 48000;
		ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_GET_SAMPLE_RATE, &soundwrite_sample_rate);
		if (sample_rate != soundwrite_sample_rate) need_resampler = TRUE;
	}
	if (ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_SET_NCHANNELS, &nchannels) != 0) {
		int soundwrite_nchannels = 1;
		ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_GET_NCHANNELS, &soundwrite_nchannels);
		if (nchannels != soundwrite_nchannels) need_resampler = TRUE;
	}
	if (need_resampler == TRUE) {
		ms_tester_create_filters(FILTER_MASK_RESAMPLER, ms_factory_get_fallback());
		configure_resampler(ms_tester_resampler, ms_tester_fileplay, ms_tester_soundwrite);
	}
	ms_filter_call_method_noarg(ms_tester_fileplay, MS_FILE_PLAYER_START);
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, ms_tester_fileplay, -1, 0);
	if (need_resampler == TRUE) {
		ms_connection_helper_link(&h, ms_tester_resampler, 0, 0);
	}
	ms_connection_helper_link(&h, ms_tester_soundwrite, 0, -1);
	ms_ticker_attach(ms_tester_ticker, ms_tester_fileplay);


	while (done != TRUE && elapsed < 6000) {
        elapsed++;
		ms_usleep(10000);
	}

	ms_filter_call_method_noarg(ms_tester_fileplay, MS_FILE_PLAYER_CLOSE);
	ms_ticker_detach(ms_tester_ticker, ms_tester_fileplay);
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, ms_tester_fileplay, -1, 0);
	if (need_resampler == TRUE) {
		ms_connection_helper_unlink(&h, ms_tester_resampler, 0, 0);
	}
	ms_connection_helper_unlink(&h, ms_tester_soundwrite, 0, -1);
	if (need_resampler == TRUE) {
		ms_tester_destroy_filters(FILTER_MASK_RESAMPLER);
	}
	ms_filter_log_statistics();
	ms_tester_destroy_filters(filter_mask);
	ms_tester_destroy_ticker();
}

static void fileplay_soundwrite_from_file(const char *filepath){
	char* file = bc_tester_res(filepath);
    fileplay_soundwrite(file);
    free(file);
}

static void fileplay_soundwrite_48000_stereo(void) {
	fileplay_soundwrite_from_file(CHIMES_48000_STEREO_FILE_NAME);
}

static void fileplay_soundwrite_44100_stereo(void) {
	fileplay_soundwrite_from_file(BIRD_44100_STEREO_FILE_NAME);
}

static void fileplay_soundwrite_16000_stereo(void) {
	fileplay_soundwrite_from_file(PUNCH_16000_STEREO_FILE_NAME);
}

static void fileplay_soundwrite_8000_stereo(void) {
	fileplay_soundwrite_from_file(PIANO_8000_STEREO_FILE_NAME);
}

static void fileplay_soundwrite_48000_mono(void) {
	fileplay_soundwrite_from_file(NYLON_48000_MONO_FILE_NAME);
}

static void fileplay_soundwrite_44100_mono(void) {
	fileplay_soundwrite_from_file(OWL_44100_MONO_FILE_NAME);
}

static void fileplay_soundwrite_16000_mono(void) {
	fileplay_soundwrite_from_file(LASERROCKET_16000_MONO_FILE_NAME);
}

static void fileplay_soundwrite_8000_mono(void) {
	fileplay_soundwrite_from_file(ARPEGGIO_8000_MONO_FILE_NAME);
}

static void soundread_soundwrite(void) {
	MSConnectionHelper h;
	bool_t need_resampler = FALSE;
	unsigned int filter_mask = FILTER_MASK_SOUNDREAD | FILTER_MASK_SOUNDWRITE;
	int sample_rate = 8000;
	int nchannels = 1;

	ms_filter_reset_statistics();
	ms_tester_create_ticker();
	ms_tester_create_filters(filter_mask, ms_factory_get_fallback());
	ms_filter_call_method(ms_tester_soundread, MS_FILTER_GET_SAMPLE_RATE, &sample_rate);
	ms_filter_call_method(ms_tester_soundread, MS_FILTER_GET_NCHANNELS, &nchannels);
	if (ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_SET_SAMPLE_RATE, &sample_rate) != 0) {
		int soundwrite_sample_rate = 48000;
		ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_GET_SAMPLE_RATE, &soundwrite_sample_rate);
		if (sample_rate != soundwrite_sample_rate) need_resampler = TRUE;
	}
	if (ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_SET_NCHANNELS, &nchannels) != 0) {
		int soundwrite_nchannels = 1;
		ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_GET_NCHANNELS, &soundwrite_nchannels);
		if (nchannels != soundwrite_nchannels) need_resampler = TRUE;
	}
	if (need_resampler == TRUE) {
		ms_tester_create_filters(FILTER_MASK_RESAMPLER, ms_factory_get_fallback());
		configure_resampler(ms_tester_resampler, ms_tester_soundread, ms_tester_soundwrite);
	}
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, ms_tester_soundread, -1, 0);
	if (need_resampler == TRUE) {
		ms_connection_helper_link(&h, ms_tester_resampler, 0, 0);
	}
	ms_connection_helper_link(&h, ms_tester_soundwrite, 0, -1);
	ms_ticker_attach(ms_tester_ticker, ms_tester_soundread);

	ms_sleep(3);

	ms_ticker_detach(ms_tester_ticker, ms_tester_soundread);
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, ms_tester_soundread, -1, 0);
	if (need_resampler == TRUE) {
		ms_connection_helper_unlink(&h, ms_tester_resampler, 0, 0);
	}
	ms_connection_helper_unlink(&h, ms_tester_soundwrite, 0, -1);
	if (need_resampler == TRUE) {
		ms_tester_destroy_filters(FILTER_MASK_RESAMPLER);
	}
	ms_filter_log_statistics();
	ms_tester_destroy_filters(filter_mask);
	ms_tester_destroy_ticker();
}

static void fileplay_bv16enc_bv16dec_soundwrite(void) {
	if (ms_factory_codec_supported(ms_factory_get_fallback(), "bv16")) {
		MSConnectionHelper h;
		MSFilter *read_resampler = NULL, *write_resampler = NULL;
		bool_t need_read_resampler = FALSE, need_write_resampler = TRUE;
		unsigned int filter_mask = FILTER_MASK_FILEPLAY  | FILTER_MASK_ENCODER | FILTER_MASK_DECODER | FILTER_MASK_SOUNDWRITE;
		int sample_rate = 8000;
		int nchannels = 1;
		int done = FALSE;
		const char* filepath = ARPEGGIO_8000_MONO_FILE_NAME;

		char* filename = bc_tester_res(filepath);

		int elapsed = 0;

		ms_filter_reset_statistics();
		ms_tester_create_ticker();
		ms_tester_codec_mime = "bv16";
		ms_tester_create_filters(filter_mask, ms_factory_get_fallback());


		//file
		ms_filter_add_notify_callback(ms_tester_fileplay, fileplay_eof, &done,TRUE);
		ms_filter_call_method_noarg(ms_tester_fileplay, MS_FILE_PLAYER_CLOSE);

		ms_filter_call_method(ms_tester_fileplay, MS_FILE_PLAYER_OPEN, (void *)filename);
		ms_filter_call_method_noarg(ms_tester_fileplay, MS_FILE_PLAYER_START);
		ms_filter_call_method(ms_tester_fileplay, MS_FILTER_GET_SAMPLE_RATE, &sample_rate);
		ms_filter_call_method(ms_tester_fileplay, MS_FILTER_GET_NCHANNELS, &nchannels);

		ms_filter_call_method(ms_tester_encoder, MS_FILTER_GET_BITRATE, &sample_rate);
		ms_filter_call_method(ms_tester_decoder, MS_FILTER_SET_BITRATE, &sample_rate);


		if (need_read_resampler == TRUE) {
			ms_tester_create_filters(FILTER_MASK_RESAMPLER, ms_factory_get_fallback());
			configure_resampler(ms_tester_resampler, ms_tester_fileplay, ms_tester_encoder);
		}


		ms_connection_helper_start(&h);
		ms_connection_helper_link(&h, ms_tester_fileplay, -1, 0);
		if (need_read_resampler == TRUE) {
			ms_connection_helper_link(&h, ms_tester_resampler, 0, 0);
		}

		if (ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_SET_BITRATE, &sample_rate) != 0) {
			int soundwrite_sample_rate = 8000;
			ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_GET_BITRATE, &soundwrite_sample_rate);
			if (sample_rate != soundwrite_sample_rate) need_write_resampler = TRUE;
		}
		if (ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_SET_NCHANNELS, &nchannels) != 0) {
			int soundwrite_nchannels = 1;
			ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_GET_NCHANNELS, &soundwrite_nchannels);
			if (nchannels != soundwrite_nchannels) need_write_resampler = TRUE;
		}
		if (need_write_resampler == TRUE) {
			ms_tester_create_filter(&write_resampler, MS_RESAMPLE_ID, ms_factory_get_fallback());
			configure_resampler(write_resampler, ms_tester_decoder, ms_tester_soundwrite);
		}

		ms_connection_helper_link(&h, ms_tester_encoder, 0, 0);
		ms_connection_helper_link(&h, ms_tester_decoder, 0, 0);
		if (need_write_resampler == TRUE) {
			ms_connection_helper_link(&h, write_resampler, 0, 0);
		}
		ms_connection_helper_link(&h, ms_tester_soundwrite, 0, -1);
		ms_ticker_attach(ms_tester_ticker, ms_tester_fileplay);


		while (done != TRUE && elapsed < 6000) {
			elapsed++;
			ms_usleep(10000);
		}

		ms_filter_call_method_noarg(ms_tester_fileplay, MS_FILE_PLAYER_CLOSE);
		ms_ticker_detach(ms_tester_ticker, ms_tester_fileplay);
		ms_connection_helper_start(&h);
		ms_connection_helper_unlink(&h, ms_tester_fileplay, -1, 0);
		if (need_read_resampler == TRUE) {
			ms_connection_helper_unlink(&h, ms_tester_resampler, 0, 0);
		}
		ms_connection_helper_unlink(&h, ms_tester_encoder, 0, 0);
		ms_connection_helper_unlink(&h, ms_tester_decoder, 0, 0);
		if (need_write_resampler == TRUE) {
			ms_connection_helper_unlink(&h, write_resampler, 0, 0);
		}
		ms_connection_helper_unlink(&h, ms_tester_soundwrite, 0, -1);
		if (need_read_resampler == TRUE) {
			ms_tester_destroy_filter(&read_resampler);
		}
		if (need_write_resampler == TRUE) {
			ms_tester_destroy_filter(&write_resampler);
		}
		ms_filter_log_statistics();
		ms_tester_destroy_filters(filter_mask);
		ms_tester_destroy_ticker();
		free(filename);
	}
}

static void soundread_speexenc_speexdec_soundwrite(void) {
	MSConnectionHelper h;
	MSFilter *read_resampler = NULL, *write_resampler = NULL;
	bool_t need_read_resampler = FALSE, need_write_resampler = FALSE;
	unsigned int filter_mask = FILTER_MASK_SOUNDREAD | FILTER_MASK_ENCODER | FILTER_MASK_DECODER | FILTER_MASK_SOUNDWRITE;
	int sample_rate = 8000;
	int nchannels = 1;

	ms_filter_reset_statistics();
	ms_tester_create_ticker();
	ms_tester_codec_mime = "speex";
	ms_tester_create_filters(filter_mask, ms_factory_get_fallback());
	ms_filter_call_method(ms_tester_encoder, MS_FILTER_GET_BITRATE, &sample_rate);
	ms_filter_call_method(ms_tester_decoder, MS_FILTER_SET_BITRATE, &sample_rate);
	if (ms_filter_call_method(ms_tester_soundread, MS_FILTER_SET_BITRATE, &sample_rate) != 0) {
		int soundread_sample_rate = 48000;
		ms_filter_call_method(ms_tester_soundread, MS_FILTER_GET_BITRATE, &soundread_sample_rate);
		if (sample_rate != soundread_sample_rate) need_read_resampler = TRUE;
	}
	if (ms_filter_call_method(ms_tester_soundread, MS_FILTER_SET_NCHANNELS, &nchannels) != 0) {
		int soundread_nchannels = 1;
		ms_filter_call_method(ms_tester_soundread, MS_FILTER_GET_NCHANNELS, &soundread_nchannels);
		if (nchannels != soundread_nchannels) need_read_resampler = TRUE;
	}
	if (need_read_resampler == TRUE) {
		ms_tester_create_filter(&read_resampler, MS_RESAMPLE_ID, ms_factory_get_fallback());
		configure_resampler(read_resampler, ms_tester_soundread, ms_tester_encoder);
	}
	if (ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_SET_BITRATE, &sample_rate) != 0) {
		int soundwrite_sample_rate = 48000;
		ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_GET_BITRATE, &soundwrite_sample_rate);
		if (sample_rate != soundwrite_sample_rate) need_write_resampler = TRUE;
	}
	if (ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_SET_NCHANNELS, &nchannels) != 0) {
		int soundwrite_nchannels = 1;
		ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_GET_NCHANNELS, &soundwrite_nchannels);
		if (nchannels != soundwrite_nchannels) need_write_resampler = TRUE;
	}
	if (need_write_resampler == TRUE) {
		ms_tester_create_filter(&write_resampler, MS_RESAMPLE_ID, ms_factory_get_fallback());
		configure_resampler(write_resampler, ms_tester_decoder, ms_tester_soundwrite);
	}
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, ms_tester_soundread, -1, 0);
	if (need_read_resampler == TRUE) {
		ms_connection_helper_link(&h, read_resampler, 0, 0);
	}
	ms_connection_helper_link(&h, ms_tester_encoder, 0, 0);
	ms_connection_helper_link(&h, ms_tester_decoder, 0, 0);
	if (need_write_resampler == TRUE) {
		ms_connection_helper_link(&h, write_resampler, 0, 0);
	}
	ms_connection_helper_link(&h, ms_tester_soundwrite, 0, -1);
	ms_ticker_attach(ms_tester_ticker, ms_tester_soundread);

	ms_sleep(5);

	ms_ticker_detach(ms_tester_ticker, ms_tester_soundread);
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, ms_tester_soundread, -1, 0);
	if (need_read_resampler == TRUE) {
		ms_connection_helper_unlink(&h, read_resampler, 0, 0);
	}
	ms_connection_helper_unlink(&h, ms_tester_encoder, 0, 0);
	ms_connection_helper_unlink(&h, ms_tester_decoder, 0, 0);
	if (need_write_resampler == TRUE) {
		ms_connection_helper_unlink(&h, write_resampler, 0, 0);
	}
	ms_connection_helper_unlink(&h, ms_tester_soundwrite, 0, -1);
	if (need_read_resampler == TRUE) {
		ms_tester_destroy_filter(&read_resampler);
	}
	if (need_write_resampler == TRUE) {
		ms_tester_destroy_filter(&write_resampler);
	}
	ms_filter_log_statistics();
	ms_tester_destroy_filters(filter_mask);
	ms_tester_destroy_ticker();
}

#define SOUNDREAD_FILE_NAME "soundread_file.raw"

static void soundread_filerec_fileplay_soundwrite(void) {
	MSConnectionHelper h;
	unsigned int filter_mask = FILTER_MASK_SOUNDREAD | FILTER_MASK_FILEREC | FILTER_MASK_FILEPLAY | FILTER_MASK_SOUNDWRITE;
	int capture_sample_rate = 8000;
	int playback_sample_rate = 8000;
	int capture_nchannels = 1;
	int playback_nchannels = 1;
	char *writable_filename = bc_tester_file(SOUNDREAD_FILE_NAME);

	ms_filter_reset_statistics();
	ms_tester_create_ticker();
	ms_tester_create_filters(filter_mask, ms_factory_get_fallback());

	// Write audio capture to a file
	ms_filter_call_method(ms_tester_soundread, MS_FILTER_GET_SAMPLE_RATE, &capture_sample_rate);
	ms_filter_call_method(ms_tester_soundread, MS_FILTER_GET_NCHANNELS, &capture_nchannels);
	ms_filter_call_method(ms_tester_filerec, MS_FILTER_SET_SAMPLE_RATE, &capture_sample_rate);
	ms_filter_call_method(ms_tester_filerec, MS_FILTER_SET_NCHANNELS, &capture_nchannels);
	ms_filter_call_method_noarg(ms_tester_filerec, MS_FILE_REC_CLOSE);
	ms_filter_call_method(ms_tester_filerec, MS_FILE_REC_OPEN, writable_filename);
	ms_filter_call_method_noarg(ms_tester_filerec, MS_FILE_REC_START);
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, ms_tester_soundread, -1, 0);
	ms_connection_helper_link(&h, ms_tester_filerec, 0, -1);
	ms_ticker_attach(ms_tester_ticker, ms_tester_soundread);

	ms_sleep(4);

	ms_filter_call_method_noarg(ms_tester_filerec, MS_FILE_REC_CLOSE);
	ms_ticker_detach(ms_tester_ticker, ms_tester_soundread);
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, ms_tester_soundread, -1, 0);
	ms_connection_helper_unlink(&h, ms_tester_filerec, 0, -1);

	// Read the previous file and play it
	ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_GET_SAMPLE_RATE, &playback_sample_rate);
	ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_GET_NCHANNELS, &playback_nchannels);
	if ((capture_sample_rate != playback_sample_rate) || (capture_nchannels != playback_nchannels)) {
		ms_tester_create_filter(&ms_tester_resampler, MS_RESAMPLE_ID, ms_factory_get_fallback());
	}
	ms_filter_call_method_noarg(ms_tester_fileplay, MS_FILE_PLAYER_CLOSE);
	ms_filter_call_method(ms_tester_fileplay, MS_FILE_PLAYER_OPEN, writable_filename);
	ms_filter_call_method_noarg(ms_tester_fileplay, MS_FILE_PLAYER_START);
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, ms_tester_fileplay, -1, 0);
	if (ms_tester_resampler != NULL) {
		ms_connection_helper_link(&h, ms_tester_resampler, 0, 0);
		ms_filter_call_method(ms_tester_resampler, MS_FILTER_SET_SAMPLE_RATE, &capture_sample_rate);
		ms_filter_call_method(ms_tester_resampler, MS_FILTER_SET_OUTPUT_SAMPLE_RATE, &playback_sample_rate);
		ms_filter_call_method(ms_tester_resampler, MS_FILTER_SET_NCHANNELS, &capture_nchannels);
		ms_filter_call_method(ms_tester_resampler, MS_FILTER_SET_OUTPUT_NCHANNELS, &capture_nchannels);
	}
	ms_connection_helper_link(&h, ms_tester_soundwrite, 0, -1);
	ms_ticker_attach(ms_tester_ticker, ms_tester_fileplay);

	ms_sleep(4);

	ms_filter_call_method_noarg(ms_tester_fileplay, MS_FILE_PLAYER_CLOSE);
	ms_ticker_detach(ms_tester_ticker, ms_tester_fileplay);
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, ms_tester_fileplay, -1, 0);
	if (ms_tester_resampler != NULL) {
		ms_connection_helper_unlink(&h, ms_tester_resampler, 0, 0);
	}
	ms_connection_helper_unlink(&h, ms_tester_soundwrite, 0, -1);
	ms_filter_log_statistics();
	ms_tester_destroy_filters(filter_mask);
	ms_tester_destroy_ticker();

	unlink(writable_filename);
    free(writable_filename);
}


test_t sound_card_tests[] = {
	TEST_NO_TAG("dtmfgen-soundwrite", dtmfgen_soundwrite),
	TEST_NO_TAG("fileplay-soundwrite-48000-stereo", fileplay_soundwrite_48000_stereo),
	TEST_NO_TAG("fileplay-soundwrite-44100-stereo", fileplay_soundwrite_44100_stereo),
	TEST_NO_TAG("fileplay-soundwrite-16000-stereo", fileplay_soundwrite_16000_stereo),
	TEST_NO_TAG("fileplay-soundwrite-8000-stereo", fileplay_soundwrite_8000_stereo),
	TEST_NO_TAG("fileplay-soundwrite-48000-mono", fileplay_soundwrite_48000_mono),
	TEST_NO_TAG("fileplay-soundwrite-44100-mono", fileplay_soundwrite_44100_mono),
	TEST_NO_TAG("fileplay-soundwrite-16000-mono", fileplay_soundwrite_16000_mono),
	TEST_NO_TAG("fileplay-soundwrite-8000-mono", fileplay_soundwrite_8000_mono),
	TEST_NO_TAG("soundread-soundwrite", soundread_soundwrite),
	TEST_NO_TAG("soundread-speexenc-speexdec-soundwrite", soundread_speexenc_speexdec_soundwrite),
	TEST_NO_TAG("soundread-filerec-fileplay-soundwrite", soundread_filerec_fileplay_soundwrite),
	TEST_NO_TAG("fileplay-bv16enc-bv16dec-soundwrite", fileplay_bv16enc_bv16dec_soundwrite)
};

test_suite_t sound_card_test_suite = {
	"Sound Card",
	sound_card_tester_before_all,
	sound_card_tester_after_all,
	NULL,
	NULL,
	sizeof(sound_card_tests) / sizeof(sound_card_tests[0]),
	sound_card_tests
};

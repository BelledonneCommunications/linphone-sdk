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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/dtmfgen.h"
#include "mediastreamer2/msfileplayer.h"
#include "mediastreamer2/msfilerec.h"
#include "mediastreamer2/msrtp.h"
#include "mediastreamer2/mstonedetector.h"
#include "private.h"
#include "mediastreamer2_tester.h"
#include "mediastreamer2_tester_private.h"

#include <stdio.h>
#include "CUnit/Basic.h"


static int sound_card_tester_init(void) {
	ms_init();
	ms_filter_enable_statistics(TRUE);
	return 0;
}

static int sound_card_tester_cleanup(void) {
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

	ms_filter_reset_statistics();
	ms_tester_create_ticker();
	ms_tester_create_filters(filter_mask);
	ms_filter_call_method(ms_tester_voidsource, MS_FILTER_SET_BITRATE, &sample_rate);
	ms_filter_call_method(ms_tester_voidsource, MS_FILTER_SET_NCHANNELS, &nchannels);
	ms_filter_call_method(ms_tester_dtmfgen, MS_FILTER_SET_BITRATE, &sample_rate);
	ms_filter_call_method(ms_tester_dtmfgen, MS_FILTER_SET_NCHANNELS, &nchannels);
	if (ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_SET_BITRATE, &sample_rate) != 0) {
		need_resampler = TRUE;
	}
	if (ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_SET_NCHANNELS, &nchannels) != 0) {
		need_resampler = TRUE;
	}
	if (need_resampler == TRUE) {
		ms_tester_create_filters(FILTER_MASK_RESAMPLER);
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

#define CHIMES_48000_STEREO_FILE_NAME		SOUND_FILE_PATH "chimes_48000_stereo.wav"
#define BIRD_44100_STEREO_FILE_NAME			SOUND_FILE_PATH "bird_44100_stereo.wav"
#define PUNCH_16000_STEREO_FILE_NAME		SOUND_FILE_PATH "punch_16000_stereo.wav"
#define PIANO_8000_STEREO_FILE_NAME			SOUND_FILE_PATH "piano_8000_stereo.wav"
#define NYLON_48000_MONO_FILE_NAME			SOUND_FILE_PATH "nylon_48000_mono.wav"
#define OWL_44100_MONO_FILE_NAME			SOUND_FILE_PATH "owl_44100_mono.wav"
#define LASERROCKET_16000_MONO_FILE_NAME	SOUND_FILE_PATH "laserrocket_16000_mono.wav"
#define ARPEGGIO_8000_MONO_FILE_NAME		SOUND_FILE_PATH "arpeggio_8000_mono.wav"

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

	ms_filter_reset_statistics();
	ms_tester_create_ticker();
	ms_tester_create_filters(filter_mask);
	ms_filter_set_notify_callback(ms_tester_fileplay, fileplay_eof, &done);
	ms_filter_call_method_noarg(ms_tester_fileplay, MS_FILE_PLAYER_CLOSE);
	ms_filter_call_method(ms_tester_fileplay, MS_FILE_PLAYER_OPEN, (void *)filename);
	ms_filter_call_method(ms_tester_fileplay, MS_FILTER_GET_SAMPLE_RATE, &sample_rate);
	ms_filter_call_method(ms_tester_fileplay, MS_FILTER_GET_NCHANNELS, &nchannels);
	if (ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_SET_BITRATE, &sample_rate) != 0) {
		int soundwrite_sample_rate = 48000;
		ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_GET_BITRATE, &soundwrite_sample_rate);
		if (sample_rate != soundwrite_sample_rate) need_resampler = TRUE;
	}
	if (ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_SET_NCHANNELS, &nchannels) != 0) {
		int soundwrite_nchannels = 1;
		ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_GET_NCHANNELS, &soundwrite_nchannels);
		if (nchannels != soundwrite_nchannels) need_resampler = TRUE;
	}
	if (need_resampler == TRUE) {
		ms_tester_create_filters(FILTER_MASK_RESAMPLER);
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

	while (done != TRUE) {
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

static void fileplay_soundwrite_48000_stereo(void) {
	fileplay_soundwrite(CHIMES_48000_STEREO_FILE_NAME);
}

static void fileplay_soundwrite_44100_stereo(void) {
	fileplay_soundwrite(BIRD_44100_STEREO_FILE_NAME);
}

static void fileplay_soundwrite_16000_stereo(void) {
	fileplay_soundwrite(PUNCH_16000_STEREO_FILE_NAME);
}

static void fileplay_soundwrite_8000_stereo(void) {
	fileplay_soundwrite(PIANO_8000_STEREO_FILE_NAME);
}

static void fileplay_soundwrite_48000_mono(void) {
	fileplay_soundwrite(NYLON_48000_MONO_FILE_NAME);
}

static void fileplay_soundwrite_44100_mono(void) {
	fileplay_soundwrite(OWL_44100_MONO_FILE_NAME);
}

static void fileplay_soundwrite_16000_mono(void) {
	fileplay_soundwrite(LASERROCKET_16000_MONO_FILE_NAME);
}

static void fileplay_soundwrite_8000_mono(void) {
	fileplay_soundwrite(ARPEGGIO_8000_MONO_FILE_NAME);
}

static void soundread_soundwrite(void) {
	MSConnectionHelper h;
	bool_t need_resampler = FALSE;
	unsigned int filter_mask = FILTER_MASK_SOUNDREAD | FILTER_MASK_SOUNDWRITE;
	int sample_rate = 8000;
	int nchannels = 1;

	ms_filter_reset_statistics();
	ms_tester_create_ticker();
	ms_tester_create_filters(filter_mask);
	ms_filter_call_method(ms_tester_soundread, MS_FILTER_GET_SAMPLE_RATE, &sample_rate);
	ms_filter_call_method(ms_tester_soundread, MS_FILTER_GET_NCHANNELS, &nchannels);
	if (ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_SET_BITRATE, &sample_rate) != 0) {
		int soundwrite_sample_rate = 48000;
		ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_GET_BITRATE, &soundwrite_sample_rate);
		if (sample_rate != soundwrite_sample_rate) need_resampler = TRUE;
	}
	if (ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_SET_NCHANNELS, &nchannels) != 0) {
		int soundwrite_nchannels = 1;
		ms_filter_call_method(ms_tester_soundwrite, MS_FILTER_GET_NCHANNELS, &soundwrite_nchannels);
		if (nchannels != soundwrite_nchannels) need_resampler = TRUE;
	}
	if (need_resampler == TRUE) {
		ms_tester_create_filters(FILTER_MASK_RESAMPLER);
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
	ms_tester_create_filters(filter_mask);
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
		ms_tester_create_filter(&read_resampler, MS_RESAMPLE_ID);
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
		ms_tester_create_filter(&write_resampler, MS_RESAMPLE_ID);
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


test_t sound_card_tests[] = {
	{ "dtmfgen-soundwrite", dtmfgen_soundwrite },
	{ "fileplay-soundwrite-48000-stereo", fileplay_soundwrite_48000_stereo },
	{ "fileplay-soundwrite-44100-stereo", fileplay_soundwrite_44100_stereo },
	{ "fileplay-soundwrite-16000-stereo", fileplay_soundwrite_16000_stereo },
	{ "fileplay-soundwrite-8000-stereo", fileplay_soundwrite_8000_stereo },
	{ "fileplay-soundwrite-48000-mono", fileplay_soundwrite_48000_mono },
	{ "fileplay-soundwrite-44100-mono", fileplay_soundwrite_44100_mono },
	{ "fileplay-soundwrite-16000-mono", fileplay_soundwrite_16000_mono },
	{ "fileplay-soundwrite-8000-mono", fileplay_soundwrite_8000_mono },
	{ "soundread-soundwrite", soundread_soundwrite },
	{ "soundread-speexenc-speexdec-soundwrite", soundread_speexenc_speexdec_soundwrite }
};

test_suite_t sound_card_test_suite = {
	"Sound Card",
	sound_card_tester_init,
	sound_card_tester_cleanup,
	sizeof(sound_card_tests) / sizeof(sound_card_tests[0]),
	sound_card_tests
};

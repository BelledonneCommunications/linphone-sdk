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

#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/msmediaplayer.h"
#include "mediastreamer2_tester.h"
#include "mediastreamer2_tester_private.h"

#ifdef VIDEO_ENABLED
#include "mediastreamer2/msogl.h" // MS_OGL_DISPLAY_SET_EGL_TARGET_CONTEXT
#include "mediastreamer2/msogl_functions.h" // MSEGLContextDescriptor, EGLint, EGL_CONTEXT_MAJOR_VERSION, EGL_NONE, EGL_OPENGL_ES_API
#endif

#ifndef max
#define max(a, b) ((a) > (b)) ? a : b
#endif

static MSFactory *_factory = NULL;

typedef enum {
	PLAYER_TEST_NONE = 0b00000,
	PLAYER_TEST_UNSUPPORTED_FORMAT = 0b00001,
	PLAYER_TEST_SEEKING = 0b00010,
	PLAYER_TEST_PLAY_TWICE = 0b00100,
	PLAYER_TEST_LOOP = 0b01000,
	PLAYER_TEST_EGL_CONTEXT_FALLBACK = 0b10000
} PlayerTestFlags;

static int tester_before_all(void) {
	_factory = ms_tester_factory_new();
	return 0;
}

static int tester_after_all(void) {
	ms_factory_destroy(_factory);
	return 0;
}

typedef struct _Eof {
	int neof;
	ms_mutex_t mutex;
} Eof;

static void eof_init(Eof *obj) {
	obj->neof = 0;
	ms_mutex_init(&obj->mutex, NULL);
}

static void eof_callback(void *user_data) {
	Eof *obj = (Eof *)user_data;
	ms_mutex_lock(&obj->mutex);
	obj->neof++;
	ms_mutex_unlock(&obj->mutex);
}

static void wait_for_neof(Eof *obj, int neof, int refresh_time_ms, int timeout_ms) {
	int time_ms = 0;
	ms_mutex_lock(&obj->mutex);
	while (time_ms < timeout_ms && obj->neof < neof) {
		ms_mutex_unlock(&obj->mutex);
		ms_usleep(refresh_time_ms * 1000);
		time_ms += refresh_time_ms;
		ms_mutex_lock(&obj->mutex);
	}
	ms_mutex_unlock(&obj->mutex);
}

static void play_file(const char *filepath, PlayerTestFlags flags, const char *renderer) {
	bool_t succeed;
	Eof eof;
	MSMediaPlayer *file_player = NULL;
	MSSndCard *snd_card = ms_snd_card_manager_get_default_playback_card(ms_factory_get_snd_card_manager(_factory));
	int duration, timeout;
	const int seek_time = 6100;
	const double timeout_prec = 0.05;

	eof_init(&eof);

	BC_ASSERT_PTR_NOT_NULL(snd_card);
	file_player = ms_media_player_new(_factory, snd_card, renderer, 0);
	BC_ASSERT_PTR_NOT_NULL(file_player);
	if (file_player == NULL) return;

	BC_ASSERT_EQUAL(ms_media_player_get_state(file_player), MSPlayerClosed, int, "%d");
	ms_media_player_set_eof_callback(file_player, eof_callback, &eof);

	succeed = ms_media_player_open(file_player, filepath);
	if (flags & PLAYER_TEST_UNSUPPORTED_FORMAT) {
		BC_ASSERT_FALSE(succeed);
		BC_ASSERT_EQUAL(ms_media_player_get_state(file_player), MSPlayerClosed, int, "%d");
	} else {
		BC_ASSERT_TRUE(succeed);
		BC_ASSERT_EQUAL(ms_media_player_get_state(file_player), MSPlayerPaused, int, "%d");
	}
	if (!succeed) {
		ms_media_player_free(file_player);
		return;
	}

	duration = ms_media_player_get_duration(file_player);
	BC_ASSERT_GREATER(duration, 0, int, "%d");

	if (flags & PLAYER_TEST_SEEKING) {
		timeout = duration - seek_time;
	} else {
		timeout = duration;
	}
	if (flags & PLAYER_TEST_LOOP) {
		int interval_time = 2000;
		ms_media_player_set_loop(file_player, interval_time);
		timeout += (duration + interval_time);
	}
	timeout = max((int)(timeout * (1.0 + timeout_prec)), 100);

#ifdef VIDEO_ENABLED
	if (flags & PLAYER_TEST_EGL_CONTEXT_FALLBACK) {
		static const EGLint impossible_version[] = {EGL_CONTEXT_MAJOR_VERSION, -1, EGL_NONE};
		static MSEGLContextDescriptor impossible_context = {EGL_OPENGL_ES_API, impossible_version};

		ms_media_player_prepare(file_player);
		MSFilter *const video_display = ms_media_player_get_video_sink(file_player);
		ms_filter_call_method(video_display, MS_OGL_DISPLAY_SET_EGL_TARGET_CONTEXT, &impossible_context);
	}
#endif // VIDEO_ENABLED

	succeed = ms_media_player_start(file_player);
	BC_ASSERT_TRUE(succeed);
	BC_ASSERT_EQUAL(ms_media_player_get_state(file_player), MSPlayerPlaying, int, "%d");

	if (flags & PLAYER_TEST_SEEKING) {
		BC_ASSERT_TRUE(ms_media_player_seek(file_player, seek_time));
	}

	if (succeed) {
		if (flags & PLAYER_TEST_LOOP) {
			wait_for_neof(&eof, 2, 100, timeout);
			BC_ASSERT_EQUAL(eof.neof, 2, int, "%d");
		} else {
			wait_for_neof(&eof, 1, 100, timeout);
			BC_ASSERT_EQUAL(eof.neof, 1, int, "%d");
		}
	}
	ms_media_player_close(file_player);
	BC_ASSERT_EQUAL(ms_media_player_get_state(file_player), MSPlayerClosed, int, "%d");

	if (flags & PLAYER_TEST_PLAY_TWICE) {
		eof_init(&eof);
		BC_ASSERT_TRUE(ms_media_player_open(file_player, filepath));
		BC_ASSERT_TRUE(ms_media_player_start(file_player));
		wait_for_neof(&eof, 1, 100, timeout);
		ms_media_player_close(file_player);
		BC_ASSERT_EQUAL(eof.neof, 1, int, "%d");
	}
	ms_media_player_free(file_player);
}

static void play_root_file(const char *filepath, PlayerTestFlags flags) {
	char *file = bc_tester_res(filepath);
	play_file(file, flags, ms_factory_get_default_video_renderer(_factory));
	bc_free(file);
}

static void play_hello_8000_wav(void) {
	play_root_file("sounds/hello8000.wav", PLAYER_TEST_NONE);
}

static void play_hello_16000_wav(void) {
	play_root_file("sounds/hello16000.wav", PLAYER_TEST_NONE);
}

static void play_hello_pcmu_mka(void) {
	PlayerTestFlags flags = ms_media_player_matroska_supported() ? PLAYER_TEST_NONE : PLAYER_TEST_UNSUPPORTED_FORMAT;
	play_root_file("sounds/hello_pcmu.mka", flags);
}

static void play_hello_opus_mka(void) {
	PlayerTestFlags flags = ms_media_player_matroska_supported() ? PLAYER_TEST_NONE : PLAYER_TEST_UNSUPPORTED_FORMAT;
	play_root_file("sounds/hello_opus.mka", flags);
}

static void play_sintel_trailer_pcmu_h264_mkv(void) {
	PlayerTestFlags flags = ms_media_player_matroska_supported() ? PLAYER_TEST_NONE : PLAYER_TEST_UNSUPPORTED_FORMAT;
	play_root_file("sounds/sintel_trailer_pcmu_h264.mkv", flags);
}

static void play_sintel_trailer_opus_h264_mkv(void) {
	PlayerTestFlags flags = ms_media_player_matroska_supported() ? PLAYER_TEST_NONE : PLAYER_TEST_UNSUPPORTED_FORMAT;
	play_root_file("sounds/sintel_trailer_opus_h264.mkv", flags);
}

static void play_sintel_trailer_opus_vp8_mkv(void) {
	PlayerTestFlags flags = ms_media_player_matroska_supported() ? PLAYER_TEST_NONE : PLAYER_TEST_UNSUPPORTED_FORMAT;
	play_root_file("sounds/sintel_trailer_opus_vp8.mkv", flags);
}

static void seeking_sintel_trailer_opus_vp8_mkv(void) {
	PlayerTestFlags flags = ms_media_player_matroska_supported() ? PLAYER_TEST_NONE : PLAYER_TEST_UNSUPPORTED_FORMAT;
	play_root_file("sounds/sintel_trailer_opus_vp8.mkv", flags | PLAYER_TEST_SEEKING);
}

static void seeking_hello_opus_mka(void) {
	PlayerTestFlags flags = ms_media_player_matroska_supported() ? PLAYER_TEST_NONE : PLAYER_TEST_UNSUPPORTED_FORMAT;
	play_root_file("sounds/hello_opus.mka", flags | PLAYER_TEST_SEEKING);
}

static void playing_twice_test(void) {
	PlayerTestFlags flags = ms_media_player_matroska_supported() ? PLAYER_TEST_NONE : PLAYER_TEST_UNSUPPORTED_FORMAT;
	play_root_file("sounds/sintel_trailer_opus_vp8.mkv", flags | PLAYER_TEST_PLAY_TWICE);
}

static void loop_test(void) {
	PlayerTestFlags flags = ms_media_player_matroska_supported() ? PLAYER_TEST_NONE : PLAYER_TEST_UNSUPPORTED_FORMAT;
	play_root_file("sounds/sintel_trailer_opus_vp8.mkv", flags | PLAYER_TEST_LOOP);
}

#ifdef VIDEO_ENABLED
static void egl_opengl_contexts(void) {
	PlayerTestFlags flags = ms_media_player_matroska_supported() ? PLAYER_TEST_NONE : PLAYER_TEST_UNSUPPORTED_FORMAT;
	flags |= PLAYER_TEST_SEEKING;
	char *const file = bc_tester_res("sounds/sintel_trailer_opus_vp8.mkv");

	// Try playing with default config
	play_file(file, flags, "MSOGL");

	// Force fallback with impossible target context
	// (Should print "target EGL context creation failed. Falling back to OpenGL ES 2.0+")
	play_file(file, flags | PLAYER_TEST_EGL_CONTEXT_FALLBACK, "MSOGL");

	bc_free(file);
}
#endif // VIDEO_ENABLED

static test_t tests[] = {
#ifdef VIDEO_ENABLED
    TEST_NO_TAG("EGL OpenGL contexts", egl_opengl_contexts),
#endif
    TEST_NO_TAG("Play hello8000.wav", play_hello_8000_wav),
    TEST_NO_TAG("Play hello16000.wav", play_hello_16000_wav),
    TEST_NO_TAG("Play hello_pcmu.mka", play_hello_pcmu_mka),
    TEST_NO_TAG("Play hello_opus.mka", play_hello_opus_mka),
    TEST_NO_TAG("Play sintel_trailer_pcmu_h264.mkv", play_sintel_trailer_pcmu_h264_mkv),
    TEST_NO_TAG("Play sintel_trailer_opus_h264.mkv", play_sintel_trailer_opus_h264_mkv),
    TEST_NO_TAG("Play sintel_trailer_opus_vp8.mkv", play_sintel_trailer_opus_vp8_mkv),
    TEST_NO_TAG("Seeking sintel_trailer_opus_vp8.mkv", seeking_sintel_trailer_opus_vp8_mkv),
    TEST_NO_TAG("Seeking hello_opus.mka (no CUEs)", seeking_hello_opus_mka),
    TEST_NO_TAG("Playing twice", playing_twice_test),
    TEST_NO_TAG("Loop test", loop_test)};

test_suite_t player_test_suite = {
    "Player", tester_before_all, tester_after_all, NULL, NULL, sizeof(tests) / sizeof(test_t), tests, 0};

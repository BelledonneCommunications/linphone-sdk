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

#include "bctoolbox/defs.h"

#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/mseventqueue.h"
#include "mediastreamer2/msmediarecorder.h"
#include "private.h"
#include <signal.h>

static int active = 1;

/*handler called when pressing C-c*/
static void stop_handler(BCTBX_UNUSED(int signum)) {
	active--;
	if (active < 0) {
		ms_error("Brutal exit (%d)\n", active);
		exit(-1);
	}
}

static void usage(const char *prog) {
	fprintf(stderr, "Usage:\n%s <multimedia file name> [options...]\n", prog);
	fprintf(stderr, "%s --help\n", prog);
	fprintf(stderr, "with options:\n");
	fprintf(stderr, "\t --append : open existing file in append mode (overwrite is the default).\n");
	fprintf(stderr, "\t --video-codec <video-codec> : use specified video codec (VP8 is the default).\n");
	fprintf(stderr, "\t --no-video: don't record video (video is recorded by default)\n");
	fprintf(stderr, "\t --no-audio: don't record audio (audio is recorded by default)\n");
	fprintf(stderr, "\t --verbose: enable debug logs (off by default)\n");
	exit(-1);
}

int main(int argc, char *argv[]) {
	const char *filename = NULL;
	MSFactory *factory;
	MSMediaRecorder *recorder;
	const char *video_codec = "VP8";
	MSFileFormat file_format = MS_FILE_FORMAT_UNKNOWN;
	MSSndCard *sndcard = NULL;
	MSWebCam *webcam = NULL;
	uint64_t start_time;
	bool_t append_mode = FALSE;
	int i;
	bool_t video_enabled = TRUE;
	bool_t audio_enabled = TRUE;

	/*parse command line arguments*/

	if (argc < 2) usage(argv[0]);

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--verbose") == 0) {
			bctbx_set_log_level(NULL, BCTBX_LOG_MESSAGE);
		} else if (strcmp(argv[i], "--append") == 0) {
			append_mode = TRUE;
		} else if (strcmp(argv[i], "--video-codec") == 0 && i + 1 < argc) {
			video_codec = argv[i + 1];
			i++;
		} else if (strcmp(argv[i], "--no-video") == 0) {
			video_enabled = FALSE;
		} else if (strcmp(argv[i], "--no-audio") == 0) {
			audio_enabled = FALSE;
		} else if (strcmp(argv[i], "--help") == 0) {
			usage(argv[0]);
		} else if (i == 1) filename = argv[1];
	}
	if (filename == NULL) {
		fprintf(stderr, "File name not specified.\n");
		usage(argv[0]);
	}

	if (ms_path_ends_with(filename, ".mkv") || ms_path_ends_with(filename, ".mka")) {
		file_format = MS_FILE_FORMAT_MATROSKA;
	} else if (ms_path_ends_with(filename, ".wav")) {
		file_format = MS_FILE_FORMAT_WAVE;
	} else file_format = MS_FILE_FORMAT_SMFF;

	/*set a signal handler to interrupt the program cleanly*/
	signal(SIGINT, stop_handler);

	/*initialize mediastreamer2*/
	factory = ms_factory_new_with_voip();
	ms_factory_create_event_queue(factory);

	if (audio_enabled) {
		sndcard = ms_snd_card_manager_get_default_capture_card(ms_factory_get_snd_card_manager(factory));
	}
	if (video_enabled) {
		webcam = ms_web_cam_manager_get_default_cam(ms_factory_get_web_cam_manager(factory));
	}
	recorder = ms_media_recorder_new(factory, sndcard, webcam, NULL, NULL, file_format, video_codec);

	if (!ms_media_recorder_open_2(recorder, filename, append_mode)) {
		goto end;
	}
	if (video_enabled) {
		ms_media_recorder_create_window_id(recorder);
	}

	ms_media_recorder_start(recorder);
	start_time = bctbx_get_cur_time_ms();
	/*program's main loop*/
	while (active) {
		uint64_t elapsed;
		/*process event callbacks*/
		ms_event_queue_pump(ms_factory_get_event_queue(factory));
		ms_usleep(50000); /*pause 50ms to avoid busy loop*/
		elapsed = bctbx_get_cur_time_ms() - start_time;
		int minutes = (int)(elapsed / 60000ULL);
		int seconds = (int)(elapsed / 1000ULL) - (minutes * 60);
		int milliseconds = (elapsed % 1000ULL);
		fprintf(stdout, "Time: %2i mn %2i.%3i seconds\r", minutes, seconds, milliseconds);
		fflush(stdout);
	}
	ms_media_recorder_pause(recorder);
	fprintf(stdout, "\nStopped.\n");
end:
	ms_media_recorder_close(recorder);
	ms_media_recorder_free(recorder);
	ms_factory_destroy(factory);

	return 0;
}

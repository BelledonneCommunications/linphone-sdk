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
#include "mediastreamer2/msmediaplayer.h"
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
	fprintf(stderr, "Usage:\n%s <multimedia file> [--verbose]\n", prog);
	exit(-1);
}

/*callback function in which we are notified of end of file when playing an mkvfile to rtp*/
static void on_end_of_play(BCTBX_UNUSED(void *data)) {
	fprintf(stdout, "End of file reached.\n");
	/*make the program exit*/
	active = 0;
}

int main(int argc, char *argv[]) {
	const char *filename;
	MSFactory *factory;
	MSMediaPlayer *player;
	uint64_t start_time;

	/*parse command line arguments*/

	if (argc < 2) usage(argv[0]);

	filename = argv[1];
	if (argc > 2 && strcmp(argv[2], "--verbose") == 0) {
		bctbx_set_log_level(NULL, BCTBX_LOG_MESSAGE);
	}

	/*set a signal handler to interrupt the program cleanly*/
	signal(SIGINT, stop_handler);

	/*initialize mediastreamer2*/
	factory = ms_factory_new_with_voip();
	ms_factory_create_event_queue(factory);

	player = ms_media_player_new(factory, NULL, NULL, NULL);

	if (ms_media_player_open(player, filename) == FALSE) {
		goto end;
	}
	if (ms_media_player_has_video_track(player)) {
		fprintf(stdout, "Found video track.\n");
		ms_media_player_create_window_id_2(player, NULL);
	}
	ms_media_player_set_eof_callback(player, on_end_of_play, NULL);
	ms_media_player_start(player);

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
	fprintf(stdout, "\nStopped.\n");
end:
	ms_media_player_close(player);
	ms_media_player_free(player);
	ms_factory_destroy(factory);

	return 0;
}

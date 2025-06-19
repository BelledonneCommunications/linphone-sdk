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

#include <signal.h>

#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/msv4l.h"
#include "mediastreamer2/msvideoout.h"

static int stopped = FALSE;

static void stop(BCTBX_UNUSED(int signum)) {
	stopped = TRUE;
}

int main(BCTBX_UNUSED(int argc), BCTBX_UNUSED(char *argv[])) {
	VideoStream *vs;
	MSWebCam *cam;
	MSVideoSize vsize;
	MSFactory *factory;
	int i;

	vsize.width = MS_VIDEO_SIZE_CIF_W;
	vsize.height = MS_VIDEO_SIZE_CIF_H;

	ortp_init();
	ortp_set_log_level_mask(ORTP_LOG_DOMAIN, ORTP_MESSAGE | ORTP_WARNING | ORTP_ERROR | ORTP_FATAL);
	// ms_init();

	factory = ms_factory_new();
	ms_factory_init_voip(factory);
	ms_factory_init_plugins(factory);

	cam = ms_web_cam_manager_get_cam(ms_factory_get_web_cam_manager(factory), "StaticImage: Static picture");
	// cam=ms_web_cam_manager_get_cam(ms_web_cam_manager_get(),"StaticImage: Static picture");

	signal(SIGINT, stop);
	/* this is to test the sequence start/stop */
	for (i = 0; i < 1; ++i) {
		int n;
		vs = video_preview_new(factory);

		/*video_preview_set_display_filter_name(vs,"MSVideoOut");*/
		video_preview_set_size(vs, vsize);
		video_preview_start(vs, cam);

		for (n = 0; n < 60000 && !stopped; ++n) {
#ifdef _WIN32
			MSG msg;
			Sleep(100);
			while (PeekMessage(&msg, NULL, 0, 0, 1)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
#else
			struct timespec ts;
			ts.tv_sec = 0;
			ts.tv_nsec = 10000000;
			nanosleep(&ts, NULL);

			if (vs) video_stream_iterate(vs);
#endif

			/* test code */
			if (n == 400) {
				ms_ticker_detach(vs->ms.sessions.ticker, vs->source);

				vs->tee = ms_factory_create_filter(factory, MS_TEE_ID);

				ms_filter_unlink(vs->pixconv, 0, vs->output2, 0);

				ms_filter_link(vs->pixconv, 0, vs->tee, 0);
				ms_filter_link(vs->tee, 0, vs->output2, 0);
				ms_filter_link(vs->tee, 1, vs->output2, 1);

				// ms_filter_unlink(vs->tee,0,vs->output,0);
				ms_ticker_attach(vs->ms.sessions.ticker, vs->source);
			}
			if (n == 500) {
				int corner = 1;
				ms_filter_call_method(vs->output2, MS_VIDEO_DISPLAY_SET_LOCAL_VIEW_MODE, &corner);
			}
			if (n == 600) {
				int corner = 2;
				ms_filter_call_method(vs->output2, MS_VIDEO_DISPLAY_SET_LOCAL_VIEW_MODE, &corner);
			}
			if (n == 700) {
				int corner = 3;
				ms_filter_call_method(vs->output2, MS_VIDEO_DISPLAY_SET_LOCAL_VIEW_MODE, &corner);
			}
			if (n == 800) {
				int corner = -1;
				ms_filter_call_method(vs->output2, MS_VIDEO_DISPLAY_SET_LOCAL_VIEW_MODE, &corner);
			}
			if (n == 900) {
				ms_ticker_detach(vs->ms.sessions.ticker, vs->source);

				ms_filter_unlink(vs->pixconv, 0, vs->tee, 0);
				ms_filter_unlink(vs->tee, 0, vs->output2, 0);
				ms_filter_unlink(vs->tee, 1, vs->output2, 1);
				ms_filter_destroy(vs->tee);
				vs->tee = NULL;

				ms_filter_link(vs->pixconv, 0, vs->output2, 0);

				ms_ticker_attach(vs->ms.sessions.ticker, vs->source);
			}
		}
		video_preview_stop(vs);
	}
	return 0;
}

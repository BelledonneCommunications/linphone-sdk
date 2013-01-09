/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2012  Belledonne Communications, Grenoble, France

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

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include <math.h>

#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/msequalizer.h"
#include "mediastreamer2/msfileplayer.h"
#include "mediastreamer2/msvolume.h"
#ifdef VIDEO_ENABLED
#include "mediastreamer2/msv4l.h"
#endif

#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#ifndef WIN32
#include <unistd.h>
#else
#include <malloc.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ortp/b64.h>


static int cond = 1;


typedef struct _MediastreamDatas {
	int payload;
	bool_t is_verbose;
	char *playback_card;
	char *infile;
	PayloadType *pt;
	RtpProfile *profile;
	MSFilter *read;
	MSFilter *write;
	MSFilter *decoder;
	MSTicker *ticker;
} MediastreamDatas;

// MAIN METHODS
/* init default arguments */
static MediastreamDatas *init_default_args();
/* parse args */
static bool_t parse_args(int argc, char **argv, MediastreamDatas *out);
/* setup streams */
static void setup_media_streams(MediastreamDatas *args);
/* run loop */
static void run_non_interactive_loop(MediastreamDatas *args);
/* exit */
static void clear_mediastreams(MediastreamDatas *args);

// HELPER METHODS
static void stop_handler(int signum);

const char *usage = "pcap_playback --infile <pcapfile>\n"
                    "--payload <payload type number or payload name like 'audio/pmcu/8000'>\n"
                    "[ --playback-card <name> ]\n"
                    "[ --verbose (most verbose messages) ]\n"
                    ;


int main(int argc, char *argv[])
{
	MediastreamDatas *args;
	cond = 1;

	args = init_default_args();

	if (!parse_args(argc, argv, args))
		return 0;

	setup_media_streams(args);
	run_non_interactive_loop(args);
	clear_mediastreams(args);

	free(args);

	return 0;
}


static MediastreamDatas *init_default_args()
{
	MediastreamDatas *args = (MediastreamDatas *) ms_malloc0(sizeof(MediastreamDatas));

	args->payload = 0;
	args->is_verbose = FALSE;
	args->playback_card = NULL;
	args->infile = NULL;
	args->pt = NULL;
	args->profile = NULL;

	args->read = NULL;
	args->write = NULL;
	args->decoder = NULL;
	args->ticker = NULL;

	return args;
}

static bool_t parse_args(int argc, char **argv, MediastreamDatas *out)
{
	int i;

	if (argc < 2) {
		printf("%s", usage);
		return FALSE;
	}

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--payload") == 0) {
			i++;
			if (isdigit(argv[i][0])) {
				out->payload = atoi(argv[i]);
			}
		} else if (strcmp(argv[i], "--playback-card") == 0) {
			i++;
			out->playback_card = argv[i];
		} else if (strcmp(argv[i], "--infile") == 0) {
			i++;
			out->infile = argv[i];
		} else if (strcmp(argv[i], "--verbose") == 0) {
			out->is_verbose = TRUE;
		} else if (strcmp(argv[i], "--help") == 0) {
			printf("%s", usage);
			return FALSE;
		}
	}
	return TRUE;
}


static void reader_notify_cb(void *user_data, MSFilter *f, unsigned int event, void *eventdata)
{
	if (event == MS_FILE_PLAYER_EOF) {
		cond = 0;
	}
}

static void setup_media_streams(MediastreamDatas *args)
{
	MSConnectionHelper h;
	MSTickerParams params = {0};

	/*create the rtp session */
	ortp_init();
	if (args->is_verbose) {
		ortp_set_log_level_mask(ORTP_DEBUG | ORTP_MESSAGE | ORTP_WARNING | ORTP_ERROR | ORTP_FATAL);
	} else {
		ortp_set_log_level_mask(ORTP_MESSAGE | ORTP_WARNING | ORTP_ERROR | ORTP_FATAL);
	}

	rtp_profile_set_payload(&av_profile, 110, &payload_type_speex_nb);
	rtp_profile_set_payload(&av_profile, 111, &payload_type_speex_wb);
	rtp_profile_set_payload(&av_profile, 112, &payload_type_ilbc);
	rtp_profile_set_payload(&av_profile, 113, &payload_type_amr);
	rtp_profile_set_payload(&av_profile, 115, &payload_type_lpc1015);
#ifdef VIDEO_ENABLED
	rtp_profile_set_payload(&av_profile, 26, &payload_type_jpeg);
	rtp_profile_set_payload(&av_profile, 98, &payload_type_h263_1998);
	rtp_profile_set_payload(&av_profile, 97, &payload_type_theora);
	rtp_profile_set_payload(&av_profile, 99, &payload_type_mp4v);
	rtp_profile_set_payload(&av_profile, 100, &payload_type_x_snow);
	rtp_profile_set_payload(&av_profile, 102, &payload_type_h264);
	rtp_profile_set_payload(&av_profile, 103, &payload_type_vp8);
#endif
	args->profile = rtp_profile_clone_full(&av_profile);

	ms_init();
	ms_filter_enable_statistics(TRUE);
	ms_filter_reset_statistics();

	signal(SIGINT, stop_handler);
	args->pt = rtp_profile_get_payload(args->profile, args->payload);
	if (args->pt == NULL) {
		printf("Error: no payload defined with number %i.", args->payload);
		exit(-1);
	}

	if (args->pt->type == PAYLOAD_VIDEO) {
#ifdef VIDEO_ENABLED
		const char *display_name;
		MSPixFmt format;
		MSVideoSize disp_size;
		int tmp = 1;

#if defined(HAVE_GL)
		display_name = "MSGLXVideo";
#elif defined(HAVE_XV)
		display_name = "MSX11Video";
#else
		display_name = "MSVideoOut";
#endif
		args->read = ms_filter_new(MS_FILE_PLAYER_ID);
		args->write = ms_filter_new_from_name(display_name);
		args->decoder = ms_filter_create_decoder(args->pt->mime_type);
		ms_filter_call_method_noarg(args->read, MS_FILE_PLAYER_CLOSE);
		ms_filter_call_method(args->read, MS_FILE_PLAYER_OPEN, args->infile);
		ms_filter_call_method_noarg(args->read, MS_FILE_PLAYER_START);
		ms_filter_call_method(args->read, MS_FILTER_SET_SAMPLE_RATE, &args->pt->clock_rate);
		ms_filter_set_notify_callback(args->read, reader_notify_cb, NULL);

		/*force the decoder to output YUV420P */
		format = MS_YUV420P;
		ms_filter_call_method(args->decoder, MS_FILTER_SET_PIX_FMT, &format);

		/*configure the display window */
		disp_size.width = MS_VIDEO_SIZE_CIF_W;
		disp_size.height = MS_VIDEO_SIZE_CIF_H;
		ms_filter_call_method(args->write, MS_FILTER_SET_VIDEO_SIZE, &disp_size);
		ms_filter_call_method(args->write, MS_VIDEO_DISPLAY_ENABLE_AUTOFIT, &tmp);
		ms_filter_call_method(args->write, MS_FILTER_SET_PIX_FMT, &format);

		params.name = "Video MSTicker";
		params.prio  = MS_TICKER_PRIO_REALTIME;
		args->ticker = ms_ticker_new_with_params(&params);

		ms_connection_helper_start(&h);
		ms_connection_helper_link(&h, args->read, -1, 0);
		ms_connection_helper_link(&h, args->decoder, 0, 0);
		ms_connection_helper_link(&h, args->write, 0, -1);
		ms_ticker_attach(args->ticker, args->read);
#else
		printf("Error: video support not compiled.\n");
#endif
	} else {
		MSSndCardManager *manager = ms_snd_card_manager_get();
		MSSndCard *play = args->playback_card == NULL ? ms_snd_card_manager_get_default_playback_card(manager) :
		                  ms_snd_card_manager_get_card(manager, args->playback_card);

		args->read = ms_filter_new(MS_FILE_PLAYER_ID);
		args->write = ms_snd_card_create_writer(play);
		args->decoder = ms_filter_create_decoder(args->pt->mime_type);
		ms_filter_call_method_noarg(args->read, MS_FILE_PLAYER_CLOSE);
		ms_filter_call_method(args->read, MS_FILE_PLAYER_OPEN, args->infile);
		ms_filter_call_method_noarg(args->read, MS_FILE_PLAYER_START);
		ms_filter_call_method(args->read, MS_FILTER_SET_SAMPLE_RATE, &args->pt->clock_rate);
		ms_filter_call_method(args->decoder, MS_FILTER_SET_SAMPLE_RATE, &args->pt->clock_rate);
		ms_filter_call_method(args->decoder, MS_FILTER_SET_NCHANNELS, &args->pt->channels);
		ms_filter_call_method(args->write, MS_FILTER_SET_SAMPLE_RATE, &args->pt->clock_rate);
		ms_filter_call_method(args->write, MS_FILTER_SET_NCHANNELS, &args->pt->channels);

		params.name = "Audio MSTicker";
		params.prio  = MS_TICKER_PRIO_REALTIME;
		args->ticker = ms_ticker_new_with_params(&params);

		ms_connection_helper_start(&h);
		ms_connection_helper_link(&h, args->read, -1, 0);
		ms_connection_helper_link(&h, args->decoder, 0, 0);
		ms_connection_helper_link(&h, args->write, 0, -1);
		ms_ticker_attach(args->ticker, args->read);
	}
}


static void run_non_interactive_loop(MediastreamDatas *args)
{
	while (cond) {
		int n;
		for (n = 0; n < 100; ++n) {
#ifdef WIN32
			MSG msg;
			Sleep(10);
			while (PeekMessage(&msg, NULL, 0, 0, 1)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
#else
			struct timespec ts;
			ts.tv_sec = 0;
			ts.tv_nsec = 10000000;
			nanosleep(&ts, NULL);
#endif
		}
	}
}

static void clear_mediastreams(MediastreamDatas *args)
{
	MSConnectionHelper h;

	ms_message("stopping all...\n");

	if (args->read) {
		ms_ticker_detach(args->ticker, args->read);
		ms_connection_helper_start(&h);
		ms_connection_helper_unlink(&h, args->read, -1, 0);
		ms_connection_helper_unlink(&h, args->decoder, 0, 0);
		ms_connection_helper_unlink(&h, args->write, 0, -1);
	}
	rtp_profile_destroy(args->profile);

	ms_exit();
}

// HELPER METHODS
static void stop_handler(int signum)
{
	cond--;
	if (cond < 0) {
		ms_error("Brutal exit (%d)\n", cond);
		exit(-1);
	}
}

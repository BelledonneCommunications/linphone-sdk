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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include <math.h>

#include "common.h"
#include "mediastreamer2/msequalizer.h"
#include "mediastreamer2/msfileplayer.h"
#include "mediastreamer2/msvolume.h"
#include "mediastreamer2/msfilerec.h"
#ifdef VIDEO_ENABLED
#include "mediastreamer2/msv4l.h"
#endif

#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#ifndef _WIN32
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
	bool_t avpf;
	char *playback_card;
	char *outfile;
	char *infile;
	PayloadType *pt;
	RtpProfile *profile;
	MSFilter *read;
	MSFilter *write;
	MSFilter *decoder;
	MSFilter *resampler;
	MSTicker *ticker;
} MediastreamDatas;

// MAIN METHODS
/* init default arguments */
static MediastreamDatas *init_default_args(void);
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
		    		"--avpf [assume RTP/AVPF profile]\n"
                    "[ --playback-card <name> ]\n"
					"[ --outfile <name> ] limited to wav file for now\n"
                    "[ --verbose (most verbose messages) ]\n"
                    ;


int main(int argc, char *argv[])
{
	MediastreamDatas *args;
	cond = 1;

	args = init_default_args();

	if (!parse_args(argc, argv, args)) {
		printf("%s", usage);
		return 0;
	}

	setup_media_streams(args);
	run_non_interactive_loop(args);
	clear_mediastreams(args);

	free(args);

	return 0;
}


static MediastreamDatas *init_default_args(void)
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
		return FALSE;
	}

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--payload") == 0) {
			i++;
			if (isdigit(argv[i][0])) {
				out->payload = atoi(argv[i]);
			}else {
				out->payload=114;
				out->pt=ms_tools_parse_custom_payload(argv[i]);
			}
		} else if (strcmp(argv[i], "--playback-card") == 0) {
			i++;
			out->playback_card = argv[i];
		} else if (strcmp(argv[i], "--infile") == 0) {
			i++;
			out->infile = argv[i];
		} else if (strcmp(argv[i], "--outfile") == 0) {
			i++;
			out->outfile = argv[i];
		} else if (strcmp(argv[i], "--verbose") == 0) {
			out->is_verbose = TRUE;
		} else if (strcmp(argv[i], "--help") == 0) {
			return FALSE;
		}else if (strcmp(argv[i],"--avpf")==0){
			out->avpf=TRUE;
		}
	}
	if (out->playback_card && out->outfile ) {
		ms_error("Cannot define both playback card and output file, select only one");
		return FALSE;
	}
	return TRUE;
}


static void reader_notify_cb(void *user_data, MSFilter *f, unsigned int event, void *eventdata)
{
	if (event == MS_FILE_PLAYER_EOF) {
		cond = 0;
	}
}

static void video_decoder_callback(void *user_data, MSFilter *f, unsigned int event, void *eventdata){
	MSVideoCodecRPSI *rpsi;
	uint16_t picture_id=0;
	switch(event){
		case MS_VIDEO_DECODER_SEND_RPSI:
			rpsi=(MSVideoCodecRPSI*)eventdata;
			if (rpsi->bit_string_len == 8) {
				picture_id = *((uint8_t *)rpsi->bit_string);
			} else if (rpsi->bit_string_len == 16) {
				picture_id = ntohs(*((uint16_t *)rpsi->bit_string));
			}
			ms_message("Decoder would like to send RPSI for pic-id=%u",picture_id);
		break;
		case MS_VIDEO_DECODER_SEND_SLI:
			ms_message("Decoder would like to send SLI");
		break;
	}
}

static 	MSFactory *factory;

static void configure_resampler(MSFilter *resampler,MSFilter *from, MSFilter *to) {
	int from_rate = 0, to_rate = 0;
	int from_channels = 0, to_channels = 0;

	ms_filter_call_method(from, MS_FILTER_GET_SAMPLE_RATE, &from_rate);
	ms_filter_call_method(to, MS_FILTER_GET_SAMPLE_RATE, &to_rate);
	ms_filter_call_method(from, MS_FILTER_GET_NCHANNELS, &from_channels);
	ms_filter_call_method(to, MS_FILTER_GET_NCHANNELS, &to_channels);
	if (from_channels == 0) {
		from_channels = 1;
		ms_error("Filter %s does not implement the MS_FILTER_GET_NCHANNELS method", from->desc->name);
	}
	if (to_channels == 0) {
		to_channels = 1;
		ms_error("Filter %s does not implement the MS_FILTER_GET_NCHANNELS method", to->desc->name);
	}
	if (from_rate == 0) {
		ms_error("Filter %s does not implement the MS_FILTER_GET_SAMPLE_RATE method", from->desc->name);
		from_rate = 8000;
	}
	if (to_rate == 0) {
		ms_error("Filter %s does not implement the MS_FILTER_GET_SAMPLE_RATE method", to->desc->name);
		to_rate = 8000;
	}
	ms_filter_call_method(resampler, MS_FILTER_SET_SAMPLE_RATE, &from_rate);
	ms_filter_call_method(resampler, MS_FILTER_SET_OUTPUT_SAMPLE_RATE, &to_rate);
	ms_filter_call_method(resampler, MS_FILTER_SET_NCHANNELS, &from_channels);
	ms_filter_call_method(resampler, MS_FILTER_SET_OUTPUT_NCHANNELS, &to_channels);
	ms_message("configuring %s:%p-->%s:%p from rate [%i] to rate [%i] and from channel [%i] to channel [%i]",
		from->desc->name, from, to->desc->name, to, from_rate, to_rate, from_channels, to_channels);
}

static void setup_media_streams(MediastreamDatas *args) {
	MSConnectionHelper h;
	MSTickerParams params = {0};

	/*create the rtp session */
	ortp_init();
	if (args->is_verbose) {
		ortp_set_log_level_mask(ORTP_LOG_DOMAIN, ORTP_DEBUG | ORTP_MESSAGE | ORTP_WARNING | ORTP_ERROR | ORTP_FATAL);
	} else {
		ortp_set_log_level_mask(ORTP_LOG_DOMAIN, ORTP_MESSAGE | ORTP_WARNING | ORTP_ERROR | ORTP_FATAL);
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

	factory = ms_factory_new_with_voip();
	ms_factory_enable_statistics(factory, TRUE);
	ms_factory_reset_statistics(factory);

	signal(SIGINT, stop_handler);
	if (args->pt==NULL)
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
#elif __APPLE__ && !defined(__ios)
		display_name ="MSOSXGLDisplay";
#else
		display_name = "MSVideoOut";
#endif
		args->read = ms_factory_create_filter(factory, MS_FILE_PLAYER_ID);
		args->write = ms_factory_create_filter_from_name(factory, display_name);
		args->decoder = ms_factory_create_decoder(factory, args->pt->mime_type);
		if (args->decoder==NULL){
			fprintf(stderr,"No decoder available for %s.\n",args->pt->mime_type);
			exit(-1);
		}
		if (ms_filter_call_method(args->decoder,MS_VIDEO_DECODER_ENABLE_AVPF,&args->avpf)==0){
			ms_filter_add_notify_callback(args->decoder, video_decoder_callback, NULL, TRUE);
		}
		ms_filter_call_method_noarg(args->read, MS_FILE_PLAYER_CLOSE);
		ms_filter_call_method(args->read, MS_FILE_PLAYER_OPEN, args->infile);
		ms_filter_call_method(args->read, MS_FILTER_SET_SAMPLE_RATE, &args->pt->clock_rate);
		ms_filter_call_method_noarg(args->read, MS_FILE_PLAYER_START);
		ms_filter_add_notify_callback(args->read, reader_notify_cb, NULL,FALSE);

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
		MSSndCardManager *manager = ms_factory_get_snd_card_manager(factory);
		MSSndCard *play =NULL;
		if (args->outfile) {
			args->write=ms_factory_create_filter(factory, MS_FILE_REC_ID);
			if (ms_filter_call_method(args->write, MS_FILE_REC_OPEN, args->outfile)) {
				ms_error("Cannot open file [%s] in write mode",args->outfile);
				exit(-1);
			}

		} else {
			play = args->playback_card ==NULL ? ms_snd_card_manager_get_default_playback_card(manager) :
		                  ms_snd_card_manager_get_card(manager, args->playback_card);
			args->write = ms_snd_card_create_writer(play);

		}
		args->read = ms_factory_create_filter(factory, MS_FILE_PLAYER_ID);
		args->decoder = ms_factory_create_decoder(factory, args->pt->mime_type);
		args->resampler = ms_factory_create_filter(factory, MS_RESAMPLE_ID);
		ms_filter_call_method_noarg(args->read, MS_FILE_PLAYER_CLOSE);
		ms_filter_call_method(args->read, MS_FILE_PLAYER_OPEN, args->infile);
		ms_filter_call_method_noarg(args->read, MS_FILE_PLAYER_START);
		ms_filter_call_method(args->read, MS_FILTER_SET_SAMPLE_RATE, &args->pt->clock_rate);
		ms_filter_call_method(args->decoder, MS_FILTER_SET_SAMPLE_RATE, &args->pt->clock_rate);
		ms_filter_call_method(args->decoder, MS_FILTER_SET_NCHANNELS, &args->pt->channels);
		ms_filter_call_method(args->write, MS_FILTER_SET_SAMPLE_RATE, &args->pt->clock_rate);
		ms_filter_call_method(args->write, MS_FILTER_SET_NCHANNELS, &args->pt->channels);

		/* Configure the resampler */
		configure_resampler(args->resampler, args->decoder, args->write);

		if (ms_filter_get_id(args->write) == MS_FILE_REC_ID) {
			ms_filter_call_method_noarg(args->write, MS_FILE_REC_START);
		}
		params.name = "Audio MSTicker";
		params.prio  = MS_TICKER_PRIO_REALTIME;
		args->ticker = ms_ticker_new_with_params(&params);

		ms_connection_helper_start(&h);
		ms_connection_helper_link(&h, args->read, -1, 0);
		ms_connection_helper_link(&h, args->decoder, 0, 0);
		ms_connection_helper_link(&h, args->resampler, 0, 0);
		ms_connection_helper_link(&h, args->write, 0, -1);
		ms_ticker_attach(args->ticker, args->read);
	}
}


static void run_non_interactive_loop(MediastreamDatas *args)
{
	while (cond) {
		int n;
		for (n = 0; n < 100; ++n) {
#ifdef _WIN32
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
		if (ms_filter_get_id(args->write) == MS_FILE_REC_ID) {
			ms_filter_call_method_noarg(args->write, MS_FILE_REC_CLOSE);
		}
	}
	rtp_profile_destroy(args->profile);

	ms_factory_destroy(factory);
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

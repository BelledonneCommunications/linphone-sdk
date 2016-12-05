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

#include <string.h>
#include <bctoolbox/tester.h>
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msfactory.h"

static MSFactory *_factory = NULL;

static int tester_before_all(void) {
	_factory = ms_factory_new_with_voip();
	return 0;
}

static int tester_after_all(void) {
	ms_factory_destroy(_factory);
	return 0;
}

static void player_events_cb(void *userdata, MSFilter *player, unsigned int id, void *arg) {
	if(id == MS_PLAYER_EOF) {
		*(bool_t *)userdata = TRUE;
	}
}

static void decoder_events_cb(void *userdata, MSFilter *decoder, unsigned int id, void *arg) {
	if(id == MS_VIDEO_DECODER_FIRST_IMAGE_DECODED) {
		*(bool_t *)userdata = TRUE;
	}
}

static void play_scenario(MSFilterDesc *decoder_desc, const char *pcap_scenario_file) {
	MSFilter *player = ms_factory_create_filter(_factory, MS_FILE_PLAYER_ID);
	MSFilter *decoder = ms_factory_create_filter_from_desc(_factory, decoder_desc);
	MSFilter *output = ms_factory_create_filter(_factory, MS_VOID_SINK_ID);
	MSTicker *ticker = ms_ticker_new();
	bool_t eof = FALSE;
	bool_t first_frame_decoded = FALSE;
	int sample_rate = 90000;
	int err;
	
	BC_ASSERT_PTR_NOT_NULL(decoder);
	if(decoder == NULL) goto end;
	
	ms_filter_call_method(player, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
	err = ms_filter_call_method(player, MS_PLAYER_OPEN, (void *)pcap_scenario_file);
	BC_ASSERT_EQUAL(err, 0, int, "%d");
	if (err != 0) goto end;
	
	ms_filter_add_notify_callback(player, player_events_cb, &eof, TRUE);
	ms_filter_add_notify_callback(decoder, decoder_events_cb, &first_frame_decoded, TRUE);
	
	ms_filter_link(player, 0, decoder, 0);
	ms_filter_link(decoder, 0, output, 0);
	ms_ticker_attach(ticker, player);
	
	ms_filter_call_method_noarg(player, MS_PLAYER_START);
	while(!eof) {
		ms_usleep(200);
	}
	
	ms_ticker_detach(ticker, player);
	ms_filter_unlink(player, 0, decoder, 0);
	ms_filter_unlink(decoder, 0, output, 0);
	
	BC_ASSERT(first_frame_decoded);
	
end:
	if(player) ms_filter_destroy(player);
	if(decoder) ms_filter_destroy(decoder);
	if(output) ms_filter_destroy(output);
	if(ticker) ms_ticker_destroy(ticker);
}

static void play_scenario_for_all_decoders(const char *mime, const char *pcap_scenario_file) {
	MSList *it;
	for(it=_factory->desc_list; it!=NULL; it=it->next) {
		MSFilterDesc *desc = (MSFilterDesc *)it->data;
		if(desc->category == MS_FILTER_DECODER && strcasecmp(desc->enc_fmt, mime) == 0) {
			ms_message("\n");
			ms_message("Playing scenario with '%s' decoder", desc->name);
			play_scenario(desc, pcap_scenario_file);
		}
	}
}

#define scenario_test(scenario_name) \
static void scenario_name(void) { \
	char *scenario_pcap_file = ms_strdup_printf("%s/%s", \
			bc_tester_get_resource_dir_prefix(), \
			"scenarios/" #scenario_name ".pcap"); \
	play_scenario_for_all_decoders("h264", scenario_pcap_file); \
	ms_free(scenario_pcap_file); \
}

scenario_test(h264_missing_pps_in_second_i_frame)
scenario_test(h264_one_nalu_per_frame)

static test_t tests[] = {
	{ "H264: missing PPS in second i-frame scenario" , h264_missing_pps_in_second_i_frame },
	{ "H264: one NALu per frame scenario"            , h264_one_nalu_per_frame            }
};

test_suite_t codec_impl_test_suite = {
	"CodecImplTesters",
	tester_before_all,
	tester_after_all,
	NULL,
	NULL,
	sizeof(tests) / sizeof(tests[0]),
	tests
};

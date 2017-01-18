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

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/dtmfgen.h"
#include "mediastreamer2/msfileplayer.h"
#include "mediastreamer2/mspcapfileplayer.h"
#include "mediastreamer2/msfilerec.h"
#include "mediastreamer2/msrtp.h"
#include "mediastreamer2/mstonedetector.h"
#include "pcap_sender.h"
#include "private.h"
#include "mediastreamer2_tester.h"
#include "mediastreamer2_tester_private.h"

static RtpProfile *profile;
static AudioStream *receiver = NULL;

#ifdef HAVE_PCAP
static OrtpEvQueue *receiver_q = NULL;
#ifdef ENABLE_CONGESTION_TESTS
static AudioStream *sender = NULL;
#endif
static VideoStream * receiverv = NULL;
static OrtpEvQueue *receiverv_q = NULL;

#endif

int RECEIVER_RTP_PORT;

#define RECEIVER_IP "127.0.0.1"

#define PIANO_FILE  "sounds/hello16000.wav"
#define RECORD_FILE "adaptive_jitter.wav"
typedef struct _stats_t {
	rtp_stats_t rtp;
	int number_of_EndOfFile;
} stats_t;

static void set_random_ports(void){
	/*This allow multiple tester to be run on the same machine*/
	RECEIVER_RTP_PORT = 42000;//rand()%(0xffff - 1024) + 1024;
}

int drifting_ticker(void *data, uint64_t virt_ticker_time){
	ortpTimeSpec ts;
	ortp_get_cur_time(&ts);
	return (int)(1 * ((ts.tv_sec * 1000LL) + ((ts.tv_nsec + 500000LL) / 1000000LL)));
}


static MSFactory *_factory = NULL;
static int tester_before_all(void) {

	_factory = ms_factory_new();
	ms_factory_init_voip(_factory);
	ms_factory_init_plugins(_factory);

	//ms_filter_enable_statistics(TRUE);
	ms_factory_enable_statistics(_factory, TRUE);
	ortp_init();
	profile = ms_tester_create_rtp_profile();
	set_random_ports();
	return 0;
}

static int tester_after_all(void) {
	ortp_exit();

	ms_factory_destroy(_factory);
	return 0;
}

void iterate_stats_logger(MediaStream *ms, void *user_pointer) {
	ms_message("count=%u cum_loss=%ld late=%ld discarded=%lu"
			, ms->sessions.rtp_session->rtp.jittctl.count
			, (unsigned long)rtp_session_get_stats(ms->sessions.rtp_session)->cum_packet_loss
			, (unsigned long)rtp_session_get_stats(ms->sessions.rtp_session)->outoftime
			, (unsigned long)rtp_session_get_stats(ms->sessions.rtp_session)->discarded
			);
}

void test_setup(int payload, OrtpJitterBufferAlgorithm algo) {
	JBParameters params;

	/*if record file exists, delete it before starting to rewrite it*/
	if (bctbx_file_exist(RECORD_FILE) != -1) {
		unlink(RECORD_FILE);
	}

	BC_ASSERT_EQUAL(audio_stream_start_full(receiver
						, profile
						, 0
						, 0
						, 0
						, 0
						, payload
						, 50
						, NULL
						, RECORD_FILE
						, NULL
						, NULL
						, 0),0, int, "%d");


	rtp_session_get_jitter_buffer_params(receiver->ms.sessions.rtp_session, &params);
	params.buffer_algorithm = algo;
	params.refresh_ms = 5000;
	params.max_size = 1000;
	params.min_size = 20;
	//Warning: if you use an max_size of more than 500 ms, you also need to increase
	//			number of packets allowing in queue. Otherwise many packets will be
	//			discarded by the queue.
	params.max_packets = MAX(params.max_packets,3*params.max_size/10);
	rtp_session_set_jitter_buffer_params(receiver->ms.sessions.rtp_session, &params);
}

void test_clean(const char* file, int clock_rate) {
	struct rtp_stats *stats = &receiver->ms.sessions.rtp_session->stats;
	JBParameters params;
	rtp_session_get_jitter_buffer_params(receiver->ms.sessions.rtp_session, &params);

	ms_message("file=%s, algo=%s, rate=%d, refresh_ms=%d, max_size_ms=%d\n"
			"bad=%ld, cum_loss=%ld, late=%lu, discarded=%lu"
		, file, params.buffer_algorithm==OrtpJitterBufferRecursiveLeastSquare?"rls":"mean"
		, clock_rate, params.refresh_ms, params.max_size
		, (unsigned long)stats->bad
		, (unsigned long)stats->cum_packet_loss
		, (unsigned long)stats->outoftime
		, (unsigned long)stats->discarded
	);
	audio_stream_stop(receiver);
}

#ifdef HAVE_PCAP

int congestion_count = 0;

static void end_of_pcap(MSPCAPSender *s, void* user_data)  {
	bool_t *has_finished = (bool_t*)user_data;
	*has_finished = TRUE;
}

static void process_queue(OrtpEvQueue *evq) {
	OrtpEvent *ev;
	
	while((ev = ortp_ev_queue_get(evq)) != NULL){
		if (ortp_event_get_type(ev) == ORTP_EVENT_CONGESTION_STATE_CHANGED){
			OrtpEventData *evd = ortp_event_get_data(ev);
			ms_message("Congestion detected: %s", evd->info.congestion_detected ? "TRUE" : "FALSE");
			if (evd->info.congestion_detected) {
				congestion_count++;
			}
		}
		ortp_event_destroy(ev);
	}
}

typedef struct _PcapTesterParams{
	const char *file;
	OrtpJitterBufferAlgorithm algo; 
	int congestion_count_expected;
	int audio_clock_rate;
	int audio_payload;
	int video_clock_rate;
	int video_payload;
	uint32_t ts_offset;
}PcapTesterParams;

bool_t has_finished = FALSE;

rtp_stats_t final_audio_rtp_stats, final_video_rtp_stats;

static OrtpEvQueue * setup_event_queue(MediaStream *ms){
	OrtpEvQueue *evq = ortp_ev_queue_new();
	rtp_session_register_event_queue(ms->sessions.rtp_session, evq);
	return evq;
}

static void pcap_tester_streams_start(const PcapTesterParams *params,
						int audio_file_port, int audio_to_port,
						int video_file_port, int video_to_port) {
	bool_t use_audio = (audio_file_port != -1);
	bool_t use_video = (video_file_port != -1);
	MSIPPort audio_dest = { RECEIVER_IP, audio_to_port };
	MSIPPort video_dest = { RECEIVER_IP, video_to_port };

	has_finished = FALSE;
	if (use_audio) {
		receiver = audio_stream_new (_factory, audio_to_port, 0, FALSE);
		test_setup(params->audio_payload, params->algo);

		rtp_session_enable_avpf_feature(receiver->ms.sessions.rtp_session, ORTP_AVPF_FEATURE_TMMBR, TRUE);
		
		receiver_q = setup_event_queue(&receiver->ms);
	}
	if (use_video) {
		JBParameters video_params;
		receiverv = video_stream_new(_factory, video_to_port, 0, FALSE);
		video_stream_set_direction(receiverv, MediaStreamRecvOnly);
		rtp_session_enable_avpf_feature(receiverv->ms.sessions.rtp_session, ORTP_AVPF_FEATURE_TMMBR, TRUE);
		

		BC_ASSERT_EQUAL(video_stream_start(receiverv
							, profile
							, 0
							, 0
							, 0
							, 0
							, params->video_payload
							, 50
							, mediastreamer2_tester_get_mire(_factory)),0, int, "%i");
		rtp_session_get_jitter_buffer_params(receiverv->ms.sessions.rtp_session, &video_params);
		video_params.buffer_algorithm = params->algo;
		video_params.refresh_ms = 5000;
		video_params.max_size = 1000;
		video_params.min_size = 20;
		rtp_session_set_jitter_buffer_params(receiverv->ms.sessions.rtp_session, &video_params);

		receiverv_q = setup_event_queue(&receiver->ms);
	}

	if (use_audio) has_finished |= (ms_pcap_sendto(_factory, params->file, audio_file_port, &audio_dest, params->audio_clock_rate, params->ts_offset, end_of_pcap, &has_finished) == NULL);
	if (use_video) has_finished |= (ms_pcap_sendto(_factory, params->file, video_file_port, &video_dest, params->video_clock_rate, params->ts_offset, end_of_pcap, &has_finished) == NULL);

	BC_ASSERT_FALSE(has_finished);
}

void pcap_tester_iterate_until(void) {
	while (! has_finished) {
		if (receiver) {
			media_stream_iterate(&receiver->ms);
			process_queue(receiver_q);
			// iterate_stats_logger(&receiver->ms, NULL);
		}
		if (receiverv) {
			media_stream_iterate(&receiverv->ms);
			process_queue(receiverv_q);
			// iterate_stats_logger(&receiverv->ms, NULL);
		}
		ms_usleep(100000);
	}
}

static void clean_evq(MediaStream *ms, OrtpEvQueue *evq){
	rtp_session_unregister_event_queue(ms->sessions.rtp_session, evq);
	ortp_ev_queue_destroy(evq);
}

void pcap_tester_stop(const char * file, int audio_clock_rate, int congestion_count_expected) {
	BC_ASSERT_EQUAL(congestion_count, congestion_count_expected, int, "%d");
	if (receiverv) {
		clean_evq(&receiverv->ms, receiverv_q);
		receiverv_q = NULL;
		memcpy(&final_video_rtp_stats, rtp_session_get_stats(receiverv->ms.sessions.rtp_session), sizeof(rtp_stats_t));
		video_stream_stop(receiverv);
	}
	if (receiver) {
		clean_evq(&receiver->ms, receiver_q);
		receiver_q = NULL;
		memcpy(&final_audio_rtp_stats, rtp_session_get_stats(receiver->ms.sessions.rtp_session), sizeof(rtp_stats_t));
		test_clean(file, audio_clock_rate);
	}
}


void pcap_tester_audio_with_params(const PcapTesterParams *params){
	pcap_tester_streams_start(params,
						0, RECEIVER_RTP_PORT,
						-1, -1);
	pcap_tester_iterate_until();
	pcap_tester_stop(params->file, params->audio_clock_rate, params->congestion_count_expected);
}

void pcap_tester_audio(const char* file, OrtpJitterBufferAlgorithm algo, int congestion_count_expected
						, int clock_rate, int payload) {
	PcapTesterParams params = {0};
	params.file = file;
	params.algo = algo;
	params.congestion_count_expected = congestion_count_expected;
	params.audio_clock_rate = clock_rate;
	params.audio_payload = payload;
	pcap_tester_audio_with_params(&params);
}

void pcap_tester_video(const char* file
						, OrtpJitterBufferAlgorithm algo, int congestion_count_expected
						, int clock_rate, int payload) {
	PcapTesterParams params = {0};
	params.file = file;
	params.algo = algo;
	params.congestion_count_expected = congestion_count_expected;
	params.video_clock_rate = clock_rate;
	params.video_payload = payload;
	pcap_tester_streams_start(&params,-1, -1,
						0, RECEIVER_RTP_PORT);
	pcap_tester_iterate_until();
	pcap_tester_stop(file, 0, congestion_count_expected);
}

static void ideal_network_basic(void) {
	pcap_tester_audio("./scenarios/pcmu_8k_no_jitter.pcap", OrtpJitterBufferBasic, 0
				, payload_type_pcmu8000.clock_rate, 0);
	BC_ASSERT_EQUAL((int)final_audio_rtp_stats.outoftime, 0, int, "%i");
	BC_ASSERT_EQUAL((int)final_audio_rtp_stats.discarded, 0, int, "%i");
	BC_ASSERT_EQUAL((int)final_audio_rtp_stats.packet_recv, 2523, int, "%i");
}

static void ideal_network_rls(void) {
	pcap_tester_audio("./scenarios/pcmu_8k_no_jitter.pcap", OrtpJitterBufferRecursiveLeastSquare, 0
				, payload_type_pcmu8000.clock_rate, 0);
	BC_ASSERT_EQUAL((int)final_audio_rtp_stats.outoftime, 0, int, "%i");
	BC_ASSERT_EQUAL((int)final_audio_rtp_stats.discarded, 0, int, "%i");
	BC_ASSERT_EQUAL((int)final_audio_rtp_stats.packet_recv, 2523, int, "%i");
}

static void burstly_network_basic(void) {
	pcap_tester_audio("./scenarios/rtp-534late-24loss-7000total.pcapng", OrtpJitterBufferBasic, 0
				, payload_type_opus.clock_rate, OPUS_PAYLOAD_TYPE);
	BC_ASSERT_GREATER((int)final_audio_rtp_stats.outoftime, 580, int, "%i");
	BC_ASSERT_LOWER((int)final_audio_rtp_stats.outoftime, 630, int, "%i");
	BC_ASSERT_EQUAL((int)final_audio_rtp_stats.discarded, 0, int, "%i");
	BC_ASSERT_EQUAL((int)final_audio_rtp_stats.packet_recv, 7104, int, "%i");
}
static void burstly_network_rls(void) {
	pcap_tester_audio("./scenarios/rtp-534late-24loss-7000total.pcapng", OrtpJitterBufferRecursiveLeastSquare, 0
				, payload_type_opus.clock_rate, OPUS_PAYLOAD_TYPE);
	BC_ASSERT_GREATER((int)final_audio_rtp_stats.outoftime, 200, int, "%i");
	BC_ASSERT_LOWER((int)final_audio_rtp_stats.outoftime, 240, int, "%i");
	BC_ASSERT_EQUAL((int)final_audio_rtp_stats.discarded, 0, int, "%i");
	BC_ASSERT_EQUAL((int)final_audio_rtp_stats.packet_recv, 7104, int, "%i");
}

static void burstly_network_rls_with_ts_offset(void) {
	PcapTesterParams params = {0};
	params.file = "./scenarios/rtp-534late-24loss-7000total.pcapng";
	params.algo = OrtpJitterBufferRecursiveLeastSquare;
	params.audio_clock_rate = payload_type_opus.clock_rate;
	params.audio_payload = OPUS_PAYLOAD_TYPE;
	params.ts_offset = 0x7ffffffc;
	
	pcap_tester_audio_with_params(&params);
	BC_ASSERT_GREATER((int)final_audio_rtp_stats.outoftime, 200, int, "%i");
	BC_ASSERT_LOWER((int)final_audio_rtp_stats.outoftime, 240, int, "%i");
	BC_ASSERT_EQUAL((int)final_audio_rtp_stats.discarded, 0, int, "%i");
	BC_ASSERT_EQUAL((int)final_audio_rtp_stats.packet_recv, 7104, int, "%i");
}

static void chaotic_start_basic(void) {
	pcap_tester_audio("./scenarios/opus-poor-quality.pcapng", OrtpJitterBufferBasic, 0
				, payload_type_opus.clock_rate, OPUS_PAYLOAD_TYPE);
	BC_ASSERT_GREATER((int)final_audio_rtp_stats.outoftime, 200, int, "%i");
	BC_ASSERT_LOWER((int)final_audio_rtp_stats.outoftime, 240, int, "%i");
	BC_ASSERT_EQUAL((int)final_audio_rtp_stats.discarded, 0, int, "%i");
	BC_ASSERT_EQUAL((int)final_audio_rtp_stats.packet_recv, 4227, int, "%i");
}

/*
 * The RLS algorithm is lost by the chaotic start (clock ratio deviates from 1 to 2 rapidly ).
 * It takes around 10 seconds before goes back to reasonable values*/
static void chaotic_start_rls(void) {
	pcap_tester_audio("./scenarios/opus-poor-quality.pcapng", OrtpJitterBufferRecursiveLeastSquare, 0
				, payload_type_opus.clock_rate, OPUS_PAYLOAD_TYPE);
	BC_ASSERT_GREATER((int)final_audio_rtp_stats.outoftime, 400, int, "%i");
	BC_ASSERT_LOWER((int)final_audio_rtp_stats.outoftime, 440, int, "%i");
	BC_ASSERT_EQUAL((int)final_audio_rtp_stats.discarded, 0, int, "%i");
	BC_ASSERT_EQUAL((int)final_audio_rtp_stats.packet_recv, 4227, int, "%i");
}

#ifdef ENABLE_CONGESTION_TESTS

static void edge_congestion(void) {
	pcap_tester_audio("./scenarios/opus-edge-congestion20_60_40.pcapng", OrtpJitterBufferRecursiveLeastSquare, 0
				, payload_type_opus.clock_rate, OPUS_PAYLOAD_TYPE);
}

static void congestion_detector_ideal_video(void) {
	const char* file = "./scenarios/congestion/video-160-0-0a5f30.pcapng";
	pcap_tester_streams_start(file
				, OrtpJitterBufferRecursiveLeastSquare, 0
				, 7078, RECEIVER_RTP_PORT, payload_type_pcmu8000.clock_rate, 0
				, 9078, RECEIVER_RTP_PORT + 1234, payload_type_vp8.clock_rate, 96
	);
	pcap_tester_iterate_until();
	pcap_tester_stop(file, payload_type_pcmu8000.clock_rate, 0);
}
static void congestion_detector_limited_video_40kbits(void) {
	const char* file = "./scenarios/congestion/video-160-120-0f20c60.pcapng";
	// pcap_tester_streams("./scenarios/congestion/video-160-120-0f20c60.pcapng"
	// 			, 7078, RECEIVER_RTP_PORT, 9078, RECEIVER_RTP_PORT + 1234
	// 			, payload_type_pcmu8000.clock_rate, 0, payload_type_vp8.clock_rate, 96
	// 			, OrtpJitterBufferRecursiveLeastSquare, 1
	// );
	pcap_tester_streams_start(file
				, OrtpJitterBufferRecursiveLeastSquare, 1
				, -1, 0, 0, 0
				, 9078, RECEIVER_RTP_PORT + 1234, payload_type_vp8.clock_rate, 96
	);
	pcap_tester_iterate_until();
	pcap_tester_stop(file, payload_type_pcmu8000.clock_rate, 0);
}
static void congestion_detector_limited_video_10kbits(void) {
	const char* file = "./scenarios/congestion/video-160-90-0a5c30.pcapng";
	// pcap_tester_streams("./scenarios/congestion/video-160-120-0f20c60.pcapng"
	// 			, 7078, RECEIVER_RTP_PORT, 9078, RECEIVER_RTP_PORT + 1234
	// 			, payload_type_pcmu8000.clock_rate, 0, payload_type_vp8.clock_rate, 96
	// 			, OrtpJitterBufferRecursiveLeastSquare, 1
	// );
	pcap_tester_streams_start(file
				, OrtpJitterBufferRecursiveLeastSquare, 1
				, -1, 0, 0, 0
				, 9078, RECEIVER_RTP_PORT + 1234, payload_type_vp8.clock_rate, 96
	);
	pcap_tester_iterate_until();
	pcap_tester_stop(file, payload_type_pcmu8000.clock_rate, 0);
}

void congestion_adaptation(OrtpJitterBufferAlgorithm algo) {
	int pause_time = 0;
	char* hello_file = bc_tester_res(PIANO_FILE);
	unsigned init_time = 0;
	int sender_port = rand()%(0xffff - 1024) + 1024;

	OrtpNetworkSimulatorParams netsim =  {0};
	netsim.enabled = TRUE;
	netsim.max_buffer_size = 40000 * 5; // 5 secs of buffering allowed

	congestion_count = 0;
	sender = audio_stream_new (_factory,sender_port, 0,FALSE);
	receiver = audio_stream_new (_factory, RECEIVER_RTP_PORT, 0,FALSE);
	receiver_q = setup_event_queue(&receiver->ms);
	test_setup(0, algo);

	BC_ASSERT_EQUAL(audio_stream_start_full(sender
						, profile
						, RECEIVER_IP
						, RECEIVER_RTP_PORT
						, RECEIVER_IP
						, 0
						, 0
						, 50
						, hello_file
						, NULL
						, NULL
						, NULL
						, 0),0, int, "%i");
	ms_filter_call_method(sender->soundread,MS_FILE_PLAYER_LOOP,&pause_time);

	// ms_ticker_set_tick_func(sender->ms.sessions.ticker, drifting_ticker, sender->ms.sessions.ticker);
	// ms_ticker_set_tick_func(receiver->ms.sessions.ticker, drifting_ticker, receiver->ms.sessions.ticker);

	media_stream_iterate(&sender->ms);


	init_time = receiver->ms.sessions.ticker->get_cur_time_ptr(sender->ms.sessions.ticker);
	while (receiver->ms.sessions.ticker->get_cur_time_ptr(receiver->ms.sessions.ticker) - init_time < 12000) {
		media_stream_iterate(&sender->ms);
		media_stream_iterate(&receiver->ms);
		process_queue(receiver_q);
		// iterate_stats_logger(&receiver->ms, &sender->ms.sessions.rtp_session->cum_loss);
	}
	BC_ASSERT_EQUAL(congestion_count, 0, int, "%d");

	ms_message("Applying limitation");
	netsim.max_bandwidth = 40000;
	rtp_session_enable_network_simulation(receiver->ms.sessions.rtp_session,&netsim);
	// wait_for_until_with_parse_events(&receiver->ms,&sender->ms, &netsim.enabled,1e7,30000,iterate_stats_logger,&sender->ms.sessions.rtp_session->cum_loss,NULL,NULL);
	init_time = receiver->ms.sessions.ticker->get_cur_time_ptr(receiver->ms.sessions.ticker);
	while (receiver->ms.sessions.ticker->get_cur_time_ptr(receiver->ms.sessions.ticker) - init_time < 30000) {
		media_stream_iterate(&sender->ms);
		media_stream_iterate(&receiver->ms);
		process_queue(receiver_q);
		// iterate_stats_logger(&receiver->ms, &sender->ms.sessions.rtp_session->cum_loss);
	}
	BC_ASSERT_EQUAL(congestion_count, 1, int, "%d");

	ms_message("Removing limitation");
	netsim.max_bandwidth = 1000000;
	rtp_session_enable_network_simulation(receiver->ms.sessions.rtp_session,&netsim);
	// wait_for_until_with_parse_events(&receiver->ms,&sender->ms, &netsim.enabled,1e7,30000,iterate_stats_logger,&sender->ms.sessions.rtp_session->cum_loss,NULL,NULL);
	init_time = receiver->ms.sessions.ticker->get_cur_time_ptr(receiver->ms.sessions.ticker);
	while (receiver->ms.sessions.ticker->get_cur_time_ptr(receiver->ms.sessions.ticker) - init_time < 30000) {
		media_stream_iterate(&sender->ms);
		media_stream_iterate(&receiver->ms);
		process_queue(receiver_q);
		// iterate_stats_logger(&receiver->ms, &sender->ms.sessions.rtp_session->cum_loss);
	}

	audio_stream_stop(sender);
	// wait_for_until_with_parse_events(&receiver->ms,NULL, &netsim.enabled, 180, 2000,iterate_stats_logger,NULL,NULL,NULL);
	init_time = receiver->ms.sessions.ticker->get_cur_time_ptr(receiver->ms.sessions.ticker);
	while (receiver->ms.sessions.ticker->get_cur_time_ptr(receiver->ms.sessions.ticker) - init_time < 2000) {
		media_stream_iterate(&receiver->ms);
		process_queue(receiver_q);
		// iterate_stats_logger(&receiver->ms, &sender->ms.sessions.rtp_session->cum_loss);
	}

	BC_ASSERT_TRUE(labs(media_stream_get_rtp_session(&receiver->ms)->rtp.jittctl.clock_offset_ts) < 400);
	BC_ASSERT_EQUAL(congestion_count, 1, int, "%d");

	clean_evq(&receiver->ms, receiver_q);
	receiver_q = NULL;
	test_clean("congestion", 8000);
	ms_free(hello_file);

}

static void congestion_adaptation_basic(void) {
	congestion_adaptation(OrtpJitterBufferBasic);
}
static void congestion_adaptation_rls(void) {
	congestion_adaptation(OrtpJitterBufferRecursiveLeastSquare);
}
#endif

#else

static void dummy_test(void) {}

#endif

static test_t tests[] = {
#ifdef HAVE_PCAP
	{ "Ideal network basic", ideal_network_basic },
	{ "Ideal network rls", ideal_network_rls },
	{ "Burstly network basic", burstly_network_basic },
	{ "Burstly network rls", burstly_network_rls },
	{ "Burstly network rls with offset", burstly_network_rls_with_ts_offset },
	{ "Chaotic start basic", chaotic_start_basic },
	{ "Chaotic start rls", chaotic_start_rls },
#ifdef ENABLE_CONGESTION_TESTS
	{ "Edge congestion", edge_congestion },
	{ "Congestion detector ideal video", congestion_detector_ideal_video },
	{ "Congestion detector limited video 40 kbits/s", congestion_detector_limited_video_40kbits },
	{ "Congestion detector limited video 10 kbits/s", congestion_detector_limited_video_10kbits },
	{ "Congestion adaptation basic", congestion_adaptation_basic },
	{ "Congestion adaptation rls", congestion_adaptation_rls },
#endif
#else
	{ "Dummy test", dummy_test }
#endif

};

test_suite_t jitterbuffer_test_suite = {
	"Jitter buffer",
	tester_before_all,
	tester_after_all,
	NULL,
	NULL,
	sizeof(tests) / sizeof(tests[0]),
	tests
};


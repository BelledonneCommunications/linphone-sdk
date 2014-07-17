#include "mediastreamer2_tester.h"
#include "mediastreamer2_tester_private.h"
#include "../include/mediastreamer2/mediastream.h"
#include "../include/mediastreamer2/msfileplayer.h"

typedef struct {
	MSFilter *audioSource;
	MSFilter *audioEnc;
	MSFilter *videoSource;
	MSFilter *pixConverter;
	MSFilter *tee;
	MSFilter *videoSink;
	MSFilter *videoEnc;
	MSFilter *recorder;
	MSTicker *ticker;
	char *filename;
	bool_t isMute[2];
} RecordStream;

static void recorder_stream_init(RecordStream *obj, MSFilterId recorderId, const char *filename) {
	MSSndCardManager *sndCardManager;
	MSWebCamManager *webcamManager;
	MSSndCard *sndCard;
	MSWebCam *webcam;
	const char *videoRendererName;
	MSPixFmt pixFmt;
	MSVideoSize vsize;

	memset(obj, 0, sizeof(RecordStream));

	sndCardManager = ms_snd_card_manager_get();
	sndCard = ms_snd_card_manager_get_default_capture_card(sndCardManager);
	obj->audioSource = ms_snd_card_create_reader(sndCard);

	webcamManager = ms_web_cam_manager_get();
	webcam = ms_web_cam_manager_get_default_cam(webcamManager);
	obj->videoSource = ms_web_cam_create_reader(webcam);
	ms_filter_call_method(obj->videoSource, MS_FILTER_GET_PIX_FMT, &pixFmt);
	ms_filter_call_method(obj->videoSource, MS_FILTER_GET_VIDEO_SIZE, &vsize);

	videoRendererName = video_stream_get_default_video_renderer();
	obj->videoSink = ms_filter_new_from_name(videoRendererName);

	obj->tee = ms_filter_new(MS_TEE_ID);

	if(pixFmt == MS_MJPEG) {
		obj->pixConverter = ms_filter_new(MS_MJPEG_DEC_ID);
	} else {
		obj->pixConverter = ms_filter_new(MS_PIX_CONV_ID);
		ms_filter_call_method(obj->pixConverter, MS_FILTER_SET_PIX_FMT, &pixFmt);
		ms_filter_call_method(obj->pixConverter, MS_FILTER_SET_VIDEO_SIZE, &vsize);
	}
	obj->recorder = ms_filter_new(recorderId);
	obj->ticker = ms_ticker_new();
	obj->filename = strdup(filename);
}

static void recorder_stream_uninit(RecordStream *obj) {
	ms_filter_destroy(obj->audioSource);
	ms_filter_destroy(obj->videoSource);
	ms_filter_destroy(obj->pixConverter);
	ms_filter_destroy(obj->tee);
	ms_filter_destroy(obj->videoSink);
	ms_filter_destroy(obj->recorder);
	if(obj->audioEnc != NULL) ms_filter_destroy(obj->audioEnc);
	if(obj->videoEnc != NULL) ms_filter_destroy(obj->videoEnc);
	ms_ticker_destroy(obj->ticker);
	ms_free(obj->filename);
}

static void recorder_stream_set_video_codec(RecordStream *obj, const char *mime) {
	MSVideoSize vsize;
	MSPinFormat pinFmt;
	if(obj->videoEnc != NULL) {
		ms_filter_destroy(obj->videoEnc);
	}
	obj->videoEnc = ms_factory_create_encoder(ms_factory_get_fallback(), mime);
	ms_filter_call_method(obj->videoSource, MS_FILTER_GET_VIDEO_SIZE, &vsize);
	ms_filter_call_method(obj->videoEnc, MS_FILTER_SET_VIDEO_SIZE, &vsize);
	pinFmt.pin = 0;
	pinFmt.fmt = ms_factory_get_video_format(ms_factory_get_fallback(), mime, &vsize, NULL);
	ms_filter_call_method(obj->recorder, MS_FILTER_SET_INPUT_FMT, &pinFmt);
}

static void recorder_stream_set_audio_codec(RecordStream *obj, const char *mime) {
	int samplerate;
	int nchannels;
	MSPinFormat pinFmt;
	if(obj->audioEnc != NULL) {
		ms_filter_destroy(obj->audioEnc);
	}
	obj->audioEnc = ms_factory_create_encoder(ms_factory_get_fallback(), mime);
	ms_filter_call_method(obj->audioSource, MS_FILTER_GET_SAMPLE_RATE, &samplerate);
	ms_filter_call_method(obj->audioSource, MS_FILTER_GET_NCHANNELS, &nchannels);
	ms_filter_call_method(obj->audioEnc, MS_FILTER_SET_SAMPLE_RATE, &samplerate);
	ms_filter_call_method(obj->audioEnc, MS_FILTER_SET_NCHANNELS, &nchannels);
	pinFmt.pin = 1;
	if(strcmp(mime, "opus") == 0) {
		pinFmt.fmt = ms_factory_get_audio_format(ms_factory_get_fallback(), mime, 48000, nchannels, NULL);
	} else {
		pinFmt.fmt = ms_factory_get_audio_format(ms_factory_get_fallback(), mime, samplerate, nchannels, NULL);
	}
	ms_filter_call_method(obj->recorder, MS_FILTER_SET_INPUT_FMT, &pinFmt);
}

//static inline void recorder_stream_mute(RecordStream *obj, MSFormatType type) {
//	obj->isMute[type] = TRUE;
//}

static void recorder_stream_start(RecordStream *obj) {
	ms_filter_link(obj->videoSource, 0, obj->pixConverter, 0);
	ms_filter_link(obj->pixConverter, 0, obj->tee, 0);
	ms_filter_link(obj->tee, 0, obj->videoSink, 0);
	ms_filter_link(obj->tee, 1, obj->videoEnc, 0);
	if(!obj->isMute[MSVideo]) {
		ms_filter_link(obj->videoEnc, 0, obj->recorder, 0);
	}
	ms_filter_link(obj->audioSource, 0, obj->audioEnc, 0);
	if(!obj->isMute[MSAudio]) {
		ms_filter_link(obj->audioEnc, 0, obj->recorder, 1);
	}
	ms_ticker_attach(obj->ticker, obj->recorder);
	ms_filter_call_method(obj->recorder, MS_RECORDER_OPEN, obj->filename);
	ms_filter_call_method_noarg(obj->recorder, MS_RECORDER_START);
}

static void recorder_stream_stop(RecordStream *obj) {
	ms_filter_call_method_noarg(obj->recorder, MS_RECORDER_CLOSE);
	ms_ticker_detach(obj->ticker, obj->recorder);
	ms_filter_unlink(obj->videoSource, 0, obj->pixConverter, 0);
	ms_filter_unlink(obj->pixConverter, 0, obj->tee, 0);
	ms_filter_unlink(obj->tee, 0, obj->videoSink, 0);
	ms_filter_unlink(obj->tee, 1, obj->videoEnc, 0);
	if(!obj->isMute[MSVideo]) {
		ms_filter_unlink(obj->videoEnc, 0, obj->recorder, 0);
	}
	ms_filter_unlink(obj->audioSource, 0, obj->audioEnc, 0);
	if(!obj->isMute[MSAudio]) {
		ms_filter_unlink(obj->audioEnc, 0, obj->recorder, 1);
	}
}

typedef struct {
	MSFilter *player;
	MSFilter *audioDecoder;
	MSFilter *videoDecoder;
	MSFilter *audioSink;
	MSFilter *videoSink;
	MSTicker *ticker;
	char *filename;
	bool_t eof;
	uint64_t origTime;
	bool_t origTimeIsSet;
	bool_t firstVideoImage;
} PlaybackStream;

static void _playback_stream_notify_callback(void *playback, MSFilter *f, unsigned int id, void *data) {
	PlaybackStream *obj = (PlaybackStream *)playback;
	switch(id) {
		case MS_PLAYER_EOF:
			obj->eof = TRUE;
			break;
			
		case MS_VIDEO_DECODER_FIRST_IMAGE_DECODED:
			obj->firstVideoImage = TRUE;
			break;
	}
}

static void playback_stream_init(PlaybackStream *obj, MSFilterId player, const char *filename) {
	MSSndCardManager *sndCardManager;
	MSSndCard *sndCard;
	const char *displayName;

	memset(obj, 0, sizeof(PlaybackStream));

	obj->player = ms_filter_new(player);
	ms_filter_add_notify_callback(obj->player, _playback_stream_notify_callback, obj, TRUE);

	sndCardManager = ms_snd_card_manager_get();
	sndCard = ms_snd_card_manager_get_default_playback_card(sndCardManager);
	obj->audioSink = ms_snd_card_create_writer(sndCard);

	displayName = video_stream_get_default_video_renderer();
	obj->videoSink = ms_filter_new_from_name(displayName);

	obj->ticker = ms_ticker_new();
	obj->filename = strdup(filename);
	obj->eof = FALSE;
	obj->origTimeIsSet = FALSE;
	obj->firstVideoImage = FALSE;
}

static void playback_stream_uninit(PlaybackStream *obj) {
	ms_filter_destroy(obj->player);
	ms_filter_destroy(obj->audioSink);
	ms_filter_destroy(obj->videoSink);
	if(obj->audioDecoder != NULL) ms_filter_destroy(obj->audioDecoder);
	if(obj->videoDecoder != NULL) ms_filter_destroy(obj->videoDecoder);
	ms_ticker_destroy(obj->ticker);
	ms_free(obj->filename);
}

static void playback_stream_start(PlaybackStream *obj) {
	MSPinFormat pinFmt;
	int samplerate;
	
	ms_filter_call_method(obj->player, MS_PLAYER_OPEN, obj->filename);
	pinFmt.pin = 0;
	ms_filter_call_method(obj->player, MS_FILTER_GET_OUTPUT_FMT, &pinFmt);
	obj->videoDecoder = ms_factory_create_decoder(ms_factory_get_fallback(), pinFmt.fmt->encoding);
	ms_filter_add_notify_callback(obj->videoDecoder, _playback_stream_notify_callback, obj, TRUE);
	pinFmt.pin = 1;
	ms_filter_call_method(obj->player, MS_FILTER_GET_OUTPUT_FMT, &pinFmt);
	obj->audioDecoder = ms_factory_create_decoder(ms_factory_get_fallback(), pinFmt.fmt->encoding);
	ms_filter_call_method(obj->audioDecoder, MS_FILTER_GET_SAMPLE_RATE, &samplerate);
	ms_filter_call_method(obj->audioSink, MS_FILTER_SET_SAMPLE_RATE, &samplerate);
	
	ms_filter_link(obj->player, 0, obj->videoDecoder, 0);
	ms_filter_link(obj->videoDecoder, 0, obj->videoSink, 0);
	ms_filter_link(obj->player, 1, obj->audioDecoder, 0);
	ms_filter_link(obj->audioDecoder, 0, obj->audioSink, 0);
	ms_ticker_attach(obj->ticker, obj->player);
	ms_filter_call_method_noarg(obj->player, MS_PLAYER_START);
	
	obj->origTime = obj->ticker->time;
	obj->origTimeIsSet = TRUE;
}

static void playback_stream_stop(PlaybackStream *obj) {
	ms_filter_call_method_noarg(obj->player, MS_PLAYER_CLOSE);
	ms_ticker_detach(obj->ticker, obj->player);
	ms_filter_unlink(obj->player, 0, obj->videoDecoder, 0);
	ms_filter_unlink(obj->videoDecoder, 0, obj->videoSink, 0);
	ms_filter_unlink(obj->player, 1, obj->audioDecoder, 0);
	ms_filter_unlink(obj->audioDecoder, 0, obj->audioSink, 0);
	obj->origTimeIsSet = FALSE;
}

static uint64_t playback_stream_get_time(const PlaybackStream *obj) {
	if(obj->origTimeIsSet) {
		return obj->ticker->time - obj->origTime;
	} else {
		return 0;
	}
}

static int tester_init() {
	ms_init();
	return 0;
}

static int tester_cleanup() {
	ms_exit();
	return 0;
}

static void wait_until_eof(const PlaybackStream *playback, uint64_t timeout_ms, useconds_t interval) {
	while(!playback->eof && playback_stream_get_time(playback) < timeout_ms) {
		usleep(interval);
	}
}

static void mkv_recording_playing() {
	RecordStream recording;
	PlaybackStream playback;
	const char filename[] = "test1.mkv";
	const unsigned int recordingTime = 10; // seconds
	const double tolerance = 0.05;
	const uint64_t timeout_ms = recordingTime * 1000 * (1 + tolerance);

	if(access(filename, F_OK) == 0) {
		remove(filename);
	}

	recorder_stream_init(&recording, MS_MKV_RECORDER_ID, filename);
	recorder_stream_set_audio_codec(&recording, "opus");
	recorder_stream_set_video_codec(&recording, "H264");
//	recorder_stream_mute(&recording, MSVideo);

	playback_stream_init(&playback, MS_MKV_PLAYER_ID, filename);

	ms_message("mkv_recording_playing: start recording");
	recorder_stream_start(&recording);

	sleep(recordingTime);

	ms_message("mkv_recording_playing: stop recording");
	recorder_stream_stop(&recording);
	recorder_stream_uninit(&recording);

	CU_ASSERT_EQUAL(access(filename, F_OK), 0);

	ms_message("mkv_recording_playing: start playback");
	playback_stream_start(&playback);

	wait_until_eof(&playback, timeout_ms, 100000);

	ms_message("mkv_recording_playing: stop playback");
	playback_stream_stop((&playback));
	playback_stream_uninit(&playback);

	CU_ASSERT_TRUE(playback.eof);
	CU_ASSERT_TRUE(playback.firstVideoImage);
}

typedef struct {
	int rtpPort;
	int rtcpPort;
	const char *ip;
	bool_t ipv6;
} RTPParams;

static const RTPParams marielleAudioRtpParams = { 2564, 2565, "127.0.0.1", FALSE };
static const RTPParams marielleVideoRtpParams = { 2664, 2665, "127.0.0.1", FALSE };
static const RTPParams margauxAudioRtpParams = { 9864, 9865, "127.0.0.1", FALSE };
static const RTPParams margauxVideoRtpParams = { 9964, 9965, "127.0.0.1", FALSE };

static void marielle_notify_cb(void *userData, MSFilter *f, unsigned int id, void *data) {
	int *nEOF = (int *)userData;
	if(id == MS_FILE_PLAYER_EOF || id == MS_PLAYER_EOF) {
		(*nEOF)++;
	}
}

static void mkv_recording_playing_streams() {
	AudioStream *marielleAudio, *margauxAudio;
	VideoStream *marielleVideo, *margauxVideo;
	RtpProfile audioProfile, videoProfile;
	MSWebCam *marielleWebcam, *margauxWebcam;
	int nEOF = 0;
	const char filepath[] = "test2.mkv";
	const char HELLO_8K_1S_FILE[] = "sounds/hello8000-1s.wav";

	if(access(filepath, F_OK) == 0) {
		remove(filepath);
	}
	
	marielleAudio = audio_stream_new(marielleAudioRtpParams.rtpPort, marielleAudioRtpParams.rtcpPort, marielleAudioRtpParams.ipv6);
	marielleVideo = video_stream_new(marielleVideoRtpParams.rtpPort, marielleVideoRtpParams.rtcpPort, marielleVideoRtpParams.ipv6);

	margauxAudio = audio_stream_new(margauxAudioRtpParams.rtpPort, margauxAudioRtpParams.rtcpPort, margauxAudioRtpParams.ipv6);
	margauxVideo = video_stream_new(margauxVideoRtpParams.rtpPort, margauxVideoRtpParams.rtcpPort, margauxVideoRtpParams.ipv6);
	
	memset(&audioProfile, 0, sizeof(RtpProfile));
	rtp_profile_set_name(&audioProfile, "default audio profile");
	rtp_profile_set_payload(&audioProfile, 0, &payload_type_opus);
	
	memset(&videoProfile, 0, sizeof(RtpProfile));
	rtp_profile_set_name(&videoProfile, "default video profile");
	rtp_profile_set_payload(&videoProfile, 0, &payload_type_h264);
	
	marielleWebcam = ms_web_cam_manager_get_default_cam(ms_web_cam_manager_get());
	margauxWebcam = ms_web_cam_manager_get_cam(ms_web_cam_manager_get(), "MSStaticImage");
	
	CU_ASSERT_EQUAL(audio_stream_start_full(
						marielleAudio,
						&audioProfile,
						margauxAudioRtpParams.ip,
						margauxAudioRtpParams.rtpPort,
						margauxAudioRtpParams.ip,
						margauxAudioRtpParams.rtcpPort,
						0,
						50,
						HELLO_8K_1S_FILE,
						NULL,
						NULL,
						NULL,
						0
						), 0);

	CU_ASSERT_EQUAL(audio_stream_start_full(
						margauxAudio,
						&audioProfile,
						marielleAudioRtpParams.ip,
						marielleAudioRtpParams.rtpPort,
						marielleAudioRtpParams.ip,
						marielleAudioRtpParams.rtcpPort,
						0,
						50,
						HELLO_8K_1S_FILE,
						NULL,
						NULL,
						NULL,
						0
						), 0);

	audio_stream_link_video(margauxAudio, margauxVideo);
	ms_filter_add_notify_callback(marielleAudio->soundread, marielle_notify_cb, &nEOF, TRUE);
	
	CU_ASSERT_EQUAL(video_stream_start(
						marielleVideo,
						&videoProfile,
						margauxVideoRtpParams.ip,
						margauxVideoRtpParams.rtpPort,
						margauxVideoRtpParams.ip,
						margauxVideoRtpParams.rtcpPort,
						0,
						50,
						marielleWebcam
						), 0);

	CU_ASSERT_EQUAL(video_stream_start(
						margauxVideo,
						&videoProfile,
						marielleVideoRtpParams.ip,
						marielleVideoRtpParams.rtpPort,
						marielleVideoRtpParams.ip,
						marielleVideoRtpParams.rtcpPort,
						0,
						50,
						margauxWebcam
						), 0);
	
	audio_stream_mixed_record_open(margauxAudio, filepath);
	audio_stream_mixed_record_start(margauxAudio);
	
	wait_for_until(&marielleAudio->ms, &marielleVideo->ms, &nEOF, 1, 12000);
	
	audio_stream_mixed_record_stop(margauxAudio);
	
	video_stream_stop(marielleVideo);
	audio_stream_stop(marielleAudio);
	
	audio_stream_unlink_video(margauxAudio, margauxVideo);

	CU_ASSERT_EQUAL(access(filepath, F_OK), 0);
}

static test_t tests[] = {
	{	"MKV file recording and playing"	,	mkv_recording_playing	},
	{	"MKV file recording with streams"	,	mkv_recording_playing_streams	}
};

test_suite_t player_recorder_test_suite = {
	"PlayerRecorder",
	tester_init,
	tester_cleanup,
	sizeof(tests)/sizeof(test_t),
	tests
};

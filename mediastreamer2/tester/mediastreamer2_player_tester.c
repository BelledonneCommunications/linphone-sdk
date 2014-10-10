#include "mediastreamer2_tester.h"
#include "../include/mediastreamer2/fileplayer.h"

static int tester_init() {
	ortp_set_log_level_mask(ORTP_MESSAGE | ORTP_WARNING | ORTP_ERROR | ORTP_FATAL);
	ms_init();
	return 0;
}

static int tester_cleanup() {
	ms_exit();
	return 0;
}

typedef struct _Eof {
	bool_t eof;
	int time_ms;
	ms_mutex_t mutex;
} Eof;

static void eof_init(Eof *obj) {
	obj->eof = FALSE;
	obj->time_ms = 0;
	ms_mutex_init(&obj->mutex, NULL);
}

static void eof_callback(void *user_data) {
	Eof *obj = (Eof *)user_data;
	ms_mutex_lock(&obj->mutex);
	obj->eof = TRUE;
	ms_mutex_unlock(&obj->mutex);
}

static void render_callback(MSPicture *picture, void *user_data) {
	(*(int *)user_data)++;
}

static void wait_for_eof(Eof *obj, int refresh_time_ms, int timeout_ms) {
	ms_mutex_lock(&obj->mutex);
	while(obj->time_ms < timeout_ms && !obj->eof) {
		ms_mutex_unlock(&obj->mutex);
		ms_usleep((useconds_t)(refresh_time_ms) * 1000U);
		obj->time_ms += refresh_time_ms;
		ms_mutex_lock(&obj->mutex);
	}
	ms_mutex_unlock(&obj->mutex);
}

static void play_file(const char *filepath, bool_t unsupported_format, bool_t test_render_callback) {
	bool_t succeed;
	Eof eof;
	int render_count = 0;
	MSFilePlayer *file_player = NULL;
	MSSndCard *snd_card = ms_snd_card_manager_get_default_card(ms_snd_card_manager_get());
	const char *display_name = video_stream_get_default_video_renderer();

	eof_init(&eof);

	file_player = ms_file_player_new(snd_card, display_name);
	CU_ASSERT_PTR_NOT_NULL(file_player);
	if(file_player == NULL) return;

	ms_file_player_set_eof_callback(file_player, eof_callback, &eof);
	if(test_render_callback) {
		succeed = ms_file_player_set_render_callback(file_player, render_callback, &render_count);
		CU_ASSERT_TRUE(succeed);
	}

	succeed = ms_file_player_open(file_player, filepath);
	if(unsupported_format) {
		CU_ASSERT_FALSE(succeed);
	} else {
		CU_ASSERT_TRUE(succeed);
	}
	if(!succeed) {
		ms_file_player_free(file_player);
		return;
	}

	succeed = ms_file_player_start(file_player);
	CU_ASSERT_TRUE(succeed);

	if(succeed) {
		wait_for_eof(&eof, 100, 20000);
	}

	ms_file_player_close(file_player);
	ms_file_player_free(file_player);
	CU_ASSERT_TRUE(eof.eof);
	if(test_render_callback) {
		CU_ASSERT_TRUE(render_count > 0);
	}
}

static void play_hello_8000_wav(void) {
	play_file("./sounds/hello8000.wav", FALSE, FALSE);
}

static void play_hello_16000_wav(void) {
	play_file("./sounds/hello16000.wav", FALSE, FALSE);
}

static void play_hello_pcmu_mka(void) {
	play_file("./sounds/hello_pcmu.mka", !ms_file_player_matroska_supported(), FALSE);
}

static void play_hello_opus_mka(void) {
	play_file("./sounds/hello_opus.mka", !ms_file_player_matroska_supported(), FALSE);
}

static void play_hello_pcmu_h264_mkv(void) {
	play_file("./sounds/hello_pcmu_h264.mkv", !ms_file_player_matroska_supported(), FALSE);
}

static void play_hello_opus_h264_mkv(void) {
	play_file("./sounds/hello_opus_h264.mkv", !ms_file_player_matroska_supported(), FALSE);
}

static void play_sintel(void) {
	play_file("./sounds/sintel.mkv", !ms_file_player_matroska_supported(), FALSE);
}

static void play_sintel_with_render_callback(void) {
	play_file("./sounds/sintel.mkv", !ms_file_player_matroska_supported(), TRUE);
}

static test_t tests[] = {
	{	"Play hello8000.wav"                ,	play_hello_8000_wav               },
	{	"Play hello16000.wav"               ,	play_hello_16000_wav              },
	{	"Play hello_pcmu.mka"               ,	play_hello_pcmu_mka               },
	{	"Play hello_opus.mka"               ,	play_hello_opus_mka               },
	{	"Play hello_pcmu_h264.mkv"          ,	play_hello_pcmu_h264_mkv          },
	{	"Play hello_opus_h264.mkv"          ,	play_hello_opus_h264_mkv          },
	{	"Play sintel"                       ,	play_sintel                       },
	{	"Play sintel with render callback"  ,	play_sintel_with_render_callback  }
};

test_suite_t player_test_suite = {
	"Player",
	tester_init,
	tester_cleanup,
	sizeof(tests)/sizeof(test_t),
	tests
};

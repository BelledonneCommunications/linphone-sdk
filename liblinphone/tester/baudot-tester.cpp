/*
 * Copyright (c) 2010-2026 Belledonne Communications SARL.
 *
 * This file is part of Liblinphone
 * (see https://gitlab.linphone.org/BC/public/liblinphone).
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

#include <cctype>
#include <cstring>

#include "liblinphone_tester.h"
#include "tester_utils.h"

#ifdef HAVE_BAUDOT

#define BAUDOT_DETECTED_CB_BODY(mode)                                                                                  \
	LinphoneCore *core = linphone_call_get_core(call);                                                                 \
	LinphoneCoreManager *manager = (LinphoneCoreManager *)linphone_core_get_user_data(core);                           \
	manager->stat.number_of_LinphoneBaudotDetected++;                                                                  \
	linphone_call_set_baudot_mode(call, mode);                                                                         \
	switch (standard) {                                                                                                \
		case LinphoneBaudotStandardUs:                                                                                 \
			manager->stat.number_of_LinphoneBaudotUsDetected++;                                                        \
			break;                                                                                                     \
		case LinphoneBaudotStandardEurope:                                                                             \
			manager->stat.number_of_LinphoneBaudotEuropeDetected++;                                                    \
			break;                                                                                                     \
	}

static void baudot_detected_cb_tty(LinphoneCall *call, LinphoneBaudotStandard standard) {
	BAUDOT_DETECTED_CB_BODY(LinphoneBaudotModeTty)
}

static void baudot_detected_cb_voice_carryover(LinphoneCall *call, LinphoneBaudotStandard standard) {
	BAUDOT_DETECTED_CB_BODY(LinphoneBaudotModeVoiceCarryOver)
}

static void send_and_check_baudot_text(bctbx_list_t *lcs,
                                       LinphoneCall *sender_call,
                                       LinphoneCoreManager *receiver,
                                       LinphoneCall *receiver_call,
                                       const char *message) {
	LinphoneChatRoom *sender_chat_room = linphone_call_get_chat_room(sender_call);
	LinphoneChatRoom *receiver_chat_room = linphone_call_get_chat_room(receiver_call);
	BC_ASSERT_PTR_NOT_NULL(sender_chat_room);
	BC_ASSERT_PTR_NOT_NULL(receiver_chat_room);
	if (sender_chat_room && receiver_chat_room) {
		int initial_composing = receiver->stat.number_of_LinphoneIsComposingActiveReceived;
		int initial_msg_received = receiver->stat.number_of_LinphoneMessageReceived;
		LinphoneChatMessage *chat_message = linphone_chat_room_create_message_from_utf8(sender_chat_room, NULL);
		for (size_t i = 0; i < strlen(message); i++) {
			BC_ASSERT_FALSE(linphone_chat_message_put_char(chat_message, message[i]));
			BC_ASSERT_TRUE(wait_for_list(lcs, &receiver->stat.number_of_LinphoneIsComposingActiveReceived,
			                             initial_composing + (int)i + 1, liblinphone_tester_sip_timeout));
			/* To ensure readability of the xml report, do not print a non-printable in the
			 * assert, should this test case be failing.*/
			int expectedChar = toupper((unsigned char)message[i]);
			int readChar = (int)linphone_chat_room_get_char(receiver_chat_room);
			if (BC_ASSERT_TRUE(readChar >= 0 && readChar < 256) && BC_ASSERT_TRUE(isprint(readChar) != 0)) {
				BC_ASSERT_EQUAL(readChar, expectedChar, int, "%c");
			}
		}
		linphone_chat_message_send(chat_message);
		BC_ASSERT_TRUE(wait_for_list(lcs, &receiver->stat.number_of_LinphoneMessageReceived, initial_msg_received + 1,
		                             liblinphone_tester_sip_timeout));
		linphone_chat_message_unref(chat_message);
	}
}

struct BaudotTextMessageParameters {
	LinphoneBaudotMode initial_sender_baudot_mode;
	LinphoneBaudotStandard initial_sender_baudot_standard;
	LinphoneBaudotMode initial_receiver_baudot_mode;
	LinphoneBaudotStandard initial_receiver_baudot_standard;
	LinphoneBaudotMode mode_to_switch_to_on_detection;
};

static void baudot_text_message(const BaudotTextMessageParameters &params) {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_new("pauline_tcp_rc");
	LinphoneCallParams *marie_params = NULL;
	LinphoneCall *pauline_call, *marie_call;

	linphone_core_enable_baudot(marie->lc, TRUE);
	linphone_core_enable_baudot(pauline->lc, TRUE);

	// Use a silence file as input for marie to prevent gap in audio caused by the pulseaudio filter
	char *filename = bc_tester_res("sounds/silence8000.wav");
	linphone_core_set_play_file(marie->lc, filename);
	linphone_core_set_use_files(marie->lc, TRUE);
	bc_free(filename);

	marie_params = linphone_core_create_call_params(marie->lc, NULL);

	BC_ASSERT_TRUE(call_with_caller_params(marie, pauline, marie_params));
	pauline_call = linphone_core_get_current_call(pauline->lc);
	marie_call = linphone_core_get_current_call(marie->lc);
	if (pauline_call && marie_call) {
		LinphoneCallCbs *marie_call_cbs = linphone_factory_create_call_cbs(linphone_factory_get());
		linphone_call_cbs_set_baudot_detected(
		    marie_call_cbs, (params.mode_to_switch_to_on_detection == LinphoneBaudotModeVoiceCarryOver)
		                        ? baudot_detected_cb_voice_carryover
		                        : baudot_detected_cb_tty);
		linphone_call_add_callbacks(marie_call, marie_call_cbs);
		linphone_call_cbs_unref(marie_call_cbs);
		linphone_call_set_baudot_mode(pauline_call, params.initial_sender_baudot_mode);
		linphone_call_set_baudot_mode(marie_call, params.initial_receiver_baudot_mode);
		linphone_call_set_baudot_sending_standard(pauline_call, params.initial_sender_baudot_standard);
		linphone_call_set_baudot_sending_standard(marie_call, params.initial_receiver_baudot_standard);

		bctbx_list_t *lcs = bctbx_list_append(NULL, pauline->lc);
		lcs = bctbx_list_append(lcs, marie->lc);
		send_and_check_baudot_text(lcs, pauline_call, marie, marie_call, "Be l3l");
		bctbx_list_free(lcs);

		end_call(marie, pauline);
	}

	BC_ASSERT_EQUAL(marie->stat.number_of_LinphoneBaudotDetected, 1, int, "%d");
	switch (params.initial_sender_baudot_standard) {
		case LinphoneBaudotStandardUs:
			BC_ASSERT_EQUAL(marie->stat.number_of_LinphoneBaudotUsDetected, 1, int, "%d");
			BC_ASSERT_EQUAL(marie->stat.number_of_LinphoneBaudotEuropeDetected, 0, int, "%d");
			break;
		case LinphoneBaudotStandardEurope:
			BC_ASSERT_EQUAL(marie->stat.number_of_LinphoneBaudotUsDetected, 0, int, "%d");
			BC_ASSERT_EQUAL(marie->stat.number_of_LinphoneBaudotEuropeDetected, 1, int, "%d");
			break;
	}

	linphone_call_params_unref(marie_params);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
}

static void baudot_text_message_us(void) {
	BaudotTextMessageParameters params;
	params.initial_sender_baudot_mode = LinphoneBaudotModeTty;
	params.initial_sender_baudot_standard = LinphoneBaudotStandardUs;
	params.initial_receiver_baudot_mode = LinphoneBaudotModeTty;
	params.initial_receiver_baudot_standard = LinphoneBaudotStandardUs;
	params.mode_to_switch_to_on_detection = LinphoneBaudotModeTty;
	baudot_text_message(params);
}

static void baudot_text_message_europe(void) {
	BaudotTextMessageParameters params;
	params.initial_sender_baudot_mode = LinphoneBaudotModeTty;
	params.initial_sender_baudot_standard = LinphoneBaudotStandardEurope;
	params.initial_receiver_baudot_mode = LinphoneBaudotModeTty;
	params.initial_receiver_baudot_standard = LinphoneBaudotStandardEurope;
	params.mode_to_switch_to_on_detection = LinphoneBaudotModeTty;
	baudot_text_message(params);
}

static void baudot_text_message_voice_switch_tty_us(void) {
	BaudotTextMessageParameters params;
	params.initial_sender_baudot_mode = LinphoneBaudotModeTty;
	params.initial_sender_baudot_standard = LinphoneBaudotStandardUs;
	params.initial_receiver_baudot_mode = LinphoneBaudotModeVoice;
	params.initial_receiver_baudot_standard = LinphoneBaudotStandardUs;
	params.mode_to_switch_to_on_detection = LinphoneBaudotModeTty;
	baudot_text_message(params);
}

static void baudot_text_message_voice_switch_tty_europe(void) {
	BaudotTextMessageParameters params;
	params.initial_sender_baudot_mode = LinphoneBaudotModeTty;
	params.initial_sender_baudot_standard = LinphoneBaudotStandardEurope;
	params.initial_receiver_baudot_mode = LinphoneBaudotModeVoice;
	params.initial_receiver_baudot_standard = LinphoneBaudotStandardEurope;
	params.mode_to_switch_to_on_detection = LinphoneBaudotModeTty;
	baudot_text_message(params);
}

static void baudot_text_message_voice_switch_voice_carryover_us(void) {
	BaudotTextMessageParameters params;
	params.initial_sender_baudot_mode = LinphoneBaudotModeTty;
	params.initial_sender_baudot_standard = LinphoneBaudotStandardUs;
	params.initial_receiver_baudot_mode = LinphoneBaudotModeVoice;
	params.initial_receiver_baudot_standard = LinphoneBaudotStandardUs;
	params.mode_to_switch_to_on_detection = LinphoneBaudotModeVoiceCarryOver;
	baudot_text_message(params);
}

static void baudot_text_message_voice_switch_voice_carryover_europe(void) {
	BaudotTextMessageParameters params;
	params.initial_sender_baudot_mode = LinphoneBaudotModeTty;
	params.initial_sender_baudot_standard = LinphoneBaudotStandardEurope;
	params.initial_receiver_baudot_mode = LinphoneBaudotModeVoice;
	params.initial_receiver_baudot_standard = LinphoneBaudotStandardEurope;
	params.mode_to_switch_to_on_detection = LinphoneBaudotModeVoiceCarryOver;
	baudot_text_message(params);
}

struct BaudotCallConfig {
	LinphoneBaudotMode initial_mode;
	LinphoneBaudotStandard sending_standard;
	LinphoneCallCbsBaudotDetectedCb detected_cb;
};

static void baudot_call_state_changed_cb(LinphoneCore *lc,
                                         LinphoneCall *call,
                                         LinphoneCallState cstate,
                                         BCTBX_UNUSED(const char *message)) {
	if (cstate == LinphoneCallStateIncomingReceived || cstate == LinphoneCallStateOutgoingProgress) {
		LinphoneCoreCbs *core_cbs = linphone_core_get_current_callbacks(lc);
		BaudotCallConfig *cfg = (BaudotCallConfig *)linphone_core_cbs_get_user_data(core_cbs);
		if (cfg) {
			if (cfg->detected_cb) {
				LinphoneCallCbs *call_cbs = linphone_factory_create_call_cbs(linphone_factory_get());
				linphone_call_cbs_set_baudot_detected(call_cbs, cfg->detected_cb);
				linphone_call_add_callbacks(call, call_cbs);
				linphone_call_cbs_unref(call_cbs);
			}
			linphone_call_set_baudot_mode(call, cfg->initial_mode);
			linphone_call_set_baudot_sending_standard(call, cfg->sending_standard);
		}
	}
}

static bool_t wait_for_voice_received(bctbx_list_t *lcs, LinphoneCall *receiver_call, int timeout_ms) {
	const float silence_threshold = -20.0f;
	int have_sound_count = 0;
	MSTimeSpec start;

	liblinphone_tester_clock_start(&start);
	while (have_sound_count < 2 && !liblinphone_tester_clock_elapsed(&start, timeout_ms)) {
		float play_level = linphone_call_get_play_volume(receiver_call);
		if (play_level > silence_threshold) {
			++have_sound_count;
		} else {
			have_sound_count = 0;
		}
		wait_for_list(lcs, NULL, 0, 50);
	}
	return have_sound_count >= 2;
}

static bool_t wait_for_silence_received(bctbx_list_t *lcs, LinphoneCall *receiver_call, int timeout_ms) {
	const float silence_threshold = -20.0f;
	int silence_count = 0;
	MSTimeSpec start;

	liblinphone_tester_clock_start(&start);
	while (silence_count < 2 && !liblinphone_tester_clock_elapsed(&start, timeout_ms)) {
		float play_level = linphone_call_get_play_volume(receiver_call);
		if (play_level <= silence_threshold) {
			++silence_count;
		} else {
			silence_count = 0;
		}
		wait_for_list(lcs, NULL, 0, 50);
	}
	return silence_count >= 2;
}

static bool_t wait_for_conf_voice_received(bctbx_list_t *lcs, LinphoneConference *conf, int timeout_ms) {
	const float silence_threshold = -20.0f;
	int have_sound_count = 0;
	MSTimeSpec start;

	liblinphone_tester_clock_start(&start);
	while (have_sound_count < 2 && !liblinphone_tester_clock_elapsed(&start, timeout_ms)) {
		float play_level = linphone_conference_get_play_volume(conf);
		if (play_level > silence_threshold) {
			++have_sound_count;
		} else {
			have_sound_count = 0;
		}
		wait_for_list(lcs, NULL, 0, 50);
	}
	return have_sound_count >= 2;
}

static bool_t wait_for_conf_silence_received(bctbx_list_t *lcs, LinphoneConference *conf, int timeout_ms) {
	const float silence_threshold = -20.0f;
	int silence_count = 0;
	MSTimeSpec start;

	liblinphone_tester_clock_start(&start);
	while (silence_count < 2 && !liblinphone_tester_clock_elapsed(&start, timeout_ms)) {
		float play_level = linphone_conference_get_play_volume(conf);
		if (play_level <= silence_threshold) {
			++silence_count;
		} else {
			silence_count = 0;
		}
		wait_for_list(lcs, NULL, 0, 50);
	}
	return silence_count >= 2;
}

static bool_t check_no_voice_received(bctbx_list_t *lcs, LinphoneCall *receiver_call, int duration_ms) {
	const float silence_threshold = -20.0f;
	MSTimeSpec start;

	liblinphone_tester_clock_start(&start);
	while (!liblinphone_tester_clock_elapsed(&start, duration_ms)) {
		float play_level = linphone_call_get_play_volume(receiver_call);
		if (play_level > silence_threshold) {
			return FALSE;
		}
		wait_for_list(lcs, NULL, 0, 50);
	}
	return TRUE;
}

static bool_t baudot_mode_can_hear(LinphoneBaudotMode mode) {
	return mode == LinphoneBaudotModeVoice || mode == LinphoneBaudotModeHearingCarryOver;
}

static bool_t baudot_mode_can_speak(LinphoneBaudotMode mode) {
	return mode == LinphoneBaudotModeVoice || mode == LinphoneBaudotModeVoiceCarryOver;
}

/*
 * Plays a recorded voice audio file through a speaker LinphonePlayer (either from a LinphoneCall
 * or a LinphoneConference) and asserts whether each receiver (call1, call2, and/or conf_receiver)
 * correctly detects voice (playback volume above silence threshold) or remains in silence, depending
 * on the expected Baudot modes (TTY, HCO, VCO, Voice).
 * Once playback is stopped, it also verifies that audio levels return to silence on receiving ends
 * before proceeding, preventing audio leakage into subsequent test steps.
 */
static void play_voice_and_check_reception(bctbx_list_t *lcs,
                                           LinphonePlayer *speaker_player,
                                           const char *voice_file,
                                           LinphoneCall *call1,
                                           bool_t expect_voice1,
                                           LinphoneCall *call2,
                                           bool_t expect_voice2,
                                           LinphoneConference *conf_receiver,
                                           bool_t expect_conf_voice) {
	if (!BC_ASSERT_PTR_NOT_NULL(speaker_player)) return;
	LinphoneStatus open_status = linphone_player_open(speaker_player, voice_file);
	BC_ASSERT_EQUAL(open_status, 0, int, "%d");
	if (open_status != 0) return;

	LinphoneStatus start_status = linphone_player_start(speaker_player);
	BC_ASSERT_EQUAL(start_status, 0, int, "%d");
	if (start_status == 0) {
		if (call1) {
			if (expect_voice1) {
				BC_ASSERT_TRUE(wait_for_voice_received(lcs, call1, 3000));
			} else {
				BC_ASSERT_TRUE(check_no_voice_received(lcs, call1, 1500));
			}
		}
		if (call2) {
			if (expect_voice2) {
				BC_ASSERT_TRUE(wait_for_voice_received(lcs, call2, 3000));
			} else {
				BC_ASSERT_TRUE(check_no_voice_received(lcs, call2, 1500));
			}
		}
		if (conf_receiver && expect_conf_voice) {
			BC_ASSERT_TRUE(wait_for_conf_voice_received(lcs, conf_receiver, 3000));
		}
	}
	linphone_player_close(speaker_player);
	if (call1 && expect_voice1) {
		BC_ASSERT_TRUE(wait_for_silence_received(lcs, call1, 2000));
	}
	if (call2 && expect_voice2) {
		BC_ASSERT_TRUE(wait_for_silence_received(lcs, call2, 2000));
	}
	if (conf_receiver && expect_conf_voice) {
		BC_ASSERT_TRUE(wait_for_conf_silence_received(lcs, conf_receiver, 2000));
	}
}

static void baudot_local_conference_relay_service(LinphoneBaudotMode pauline_mode, LinphoneBaudotMode laure_mode) {
	// marie is the Relay Service agent, pauline is the Baudot participant, laure is the third party
	LinphoneCoreManager *marie = create_mgr_for_conference("marie_rc", TRUE, NULL);
	LinphoneCoreManager *pauline = create_mgr_for_conference("pauline_tcp_rc", TRUE, NULL);
	LinphoneCoreManager *laure =
	    create_mgr_for_conference(liblinphone_tester_ipv6_available() ? "laure_tcp_rc" : "laure_rc_udp", TRUE, NULL);

	bctbx_list_t *lcs = bctbx_list_append(NULL, marie->lc);
	lcs = bctbx_list_append(lcs, pauline->lc);
	lcs = bctbx_list_append(lcs, laure->lc);

	// Ensure marie has no conference factory address so the conference is mixed locally
	LinphoneAccount *marie_account = linphone_core_get_default_account(marie->lc);
	if (marie_account) {
		LinphoneAccountParams *params = linphone_account_params_clone(linphone_account_get_params(marie_account));
		linphone_account_params_set_conference_factory_address(params, NULL);
		linphone_account_params_set_audio_video_conference_factory_address(params, NULL);
		linphone_account_set_params(marie_account, params);
		linphone_account_params_unref(params);
	}

	char *silence_file = bc_tester_res("sounds/silence8000.wav");
	for (bctbx_list_t *it = lcs; it; it = bctbx_list_next(it)) {
		LinphoneCore *c = (LinphoneCore *)bctbx_list_get_data(it);
		linphone_core_set_play_file(c, silence_file);
		linphone_core_set_use_files(c, TRUE);
	}
	bc_free(silence_file);

	// Configure Baudot on the agent (marie), participant (pauline), and third party (laure)
	linphone_core_enable_baudot(marie->lc, TRUE);
	linphone_core_enable_baudot(pauline->lc, TRUE);
	linphone_core_enable_baudot(laure->lc, TRUE);

	BaudotCallConfig marie_baudot_cfg = {LinphoneBaudotModeVoice, LinphoneBaudotStandardUs, NULL};
	LinphoneCoreCbs *marie_cbs = linphone_factory_create_core_cbs(linphone_factory_get());
	linphone_core_cbs_set_user_data(marie_cbs, &marie_baudot_cfg);
	linphone_core_cbs_set_call_state_changed(marie_cbs, baudot_call_state_changed_cb);
	linphone_core_add_callbacks(marie->lc, marie_cbs);

	BaudotCallConfig pauline_baudot_cfg = {pauline_mode, LinphoneBaudotStandardUs,
	                                       (pauline_mode == LinphoneBaudotModeVoiceCarryOver)
	                                           ? baudot_detected_cb_voice_carryover
	                                           : baudot_detected_cb_tty};
	LinphoneCoreCbs *pauline_cbs = linphone_factory_create_core_cbs(linphone_factory_get());
	linphone_core_cbs_set_user_data(pauline_cbs, &pauline_baudot_cfg);
	linphone_core_cbs_set_call_state_changed(pauline_cbs, baudot_call_state_changed_cb);
	linphone_core_add_callbacks(pauline->lc, pauline_cbs);

	BaudotCallConfig laure_baudot_cfg = {
	    laure_mode, LinphoneBaudotStandardUs,
	    (laure_mode == LinphoneBaudotModeVoiceCarryOver) ? baudot_detected_cb_voice_carryover : baudot_detected_cb_tty};
	LinphoneCoreCbs *laure_cbs = linphone_factory_create_core_cbs(linphone_factory_get());
	linphone_core_cbs_set_user_data(laure_cbs, &laure_baudot_cfg);
	linphone_core_cbs_set_call_state_changed(laure_cbs, baudot_call_state_changed_cb);
	linphone_core_add_callbacks(laure->lc, laure_cbs);

	bctbx_list_t *participants = NULL;
	LinphoneCall *pauline_call = NULL;
	LinphoneCall *laure_call = NULL;
	LinphoneCall *marie_call_pauline = NULL;
	LinphoneCall *marie_call_laure = NULL;
	LinphoneConference *conf = NULL;
	char *voice_file = bc_tester_res("sounds/ahbahouaismaisbon.wav");
	int expected_marie_baudot = 0;
	stats initial_marie_stat;
	stats initial_pauline_stat;
	stats initial_laure_stat;

	// 1. Baudot participant (pauline) calls the agent (marie) and exchanges an initial message
	if (!BC_ASSERT_TRUE(call(pauline, marie))) goto end;
	pauline_call = linphone_core_get_current_call(pauline->lc);
	marie_call_pauline = linphone_core_get_current_call(marie->lc);
	if (!BC_ASSERT_PTR_NOT_NULL(pauline_call) || !BC_ASSERT_PTR_NOT_NULL(marie_call_pauline)) goto end;

	{
		LinphoneCallCbs *marie_pauline_cbs = linphone_factory_create_call_cbs(linphone_factory_get());
		linphone_call_cbs_set_baudot_detected(marie_pauline_cbs, (pauline_mode == LinphoneBaudotModeHearingCarryOver)
		                                                             ? baudot_detected_cb_voice_carryover
		                                                             : baudot_detected_cb_tty);
		linphone_call_add_callbacks(marie_call_pauline, marie_pauline_cbs);
		linphone_call_cbs_unref(marie_pauline_cbs);
	}

	if (pauline_mode == LinphoneBaudotModeTty || pauline_mode == LinphoneBaudotModeHearingCarryOver) {
		send_and_check_baudot_text(lcs, pauline_call, marie, marie_call_pauline, "Hi");
		BC_ASSERT_EQUAL(marie->stat.number_of_LinphoneBaudotDetected, 1, int, "%d");
		BC_ASSERT_EQUAL(marie->stat.number_of_LinphoneBaudotUsDetected, 1, int, "%d");
	} else if (pauline_mode == LinphoneBaudotModeVoiceCarryOver) {
		linphone_call_set_baudot_mode(marie_call_pauline, LinphoneBaudotModeHearingCarryOver);
		send_and_check_baudot_text(lcs, marie_call_pauline, pauline, pauline_call, "Hi");
		BC_ASSERT_EQUAL(pauline->stat.number_of_LinphoneBaudotDetected, 1, int, "%d");
		BC_ASSERT_EQUAL(pauline->stat.number_of_LinphoneBaudotUsDetected, 1, int, "%d");
	}

	// 2. Agent (marie) pauses pauline and calls the third party (laure)
	BC_ASSERT_TRUE(pause_call_1(marie, marie_call_pauline, pauline, pauline_call));
	if (!BC_ASSERT_TRUE(call(marie, laure))) goto end;
	marie_call_laure = linphone_core_get_current_call(marie->lc);
	laure_call = linphone_core_get_current_call(laure->lc);
	if (!BC_ASSERT_PTR_NOT_NULL(marie_call_laure) || !BC_ASSERT_PTR_NOT_NULL(laure_call)) goto end;

	if (laure_mode != LinphoneBaudotModeVoice) {
		LinphoneCallCbs *marie_laure_cbs = linphone_factory_create_call_cbs(linphone_factory_get());
		linphone_call_cbs_set_baudot_detected(marie_laure_cbs, (laure_mode == LinphoneBaudotModeHearingCarryOver)
		                                                           ? baudot_detected_cb_voice_carryover
		                                                           : baudot_detected_cb_tty);
		linphone_call_add_callbacks(marie_call_laure, marie_laure_cbs);
		linphone_call_cbs_unref(marie_laure_cbs);

		int initial_marie_baudot = marie->stat.number_of_LinphoneBaudotDetected;
		if (laure_mode == LinphoneBaudotModeTty || laure_mode == LinphoneBaudotModeHearingCarryOver) {
			send_and_check_baudot_text(lcs, laure_call, marie, marie_call_laure, "Yo");
			BC_ASSERT_EQUAL(marie->stat.number_of_LinphoneBaudotDetected, initial_marie_baudot + 1, int, "%d");
			BC_ASSERT_EQUAL(marie->stat.number_of_LinphoneBaudotUsDetected, initial_marie_baudot + 1, int, "%d");
		} else if (laure_mode == LinphoneBaudotModeVoiceCarryOver) {
			linphone_call_set_baudot_mode(marie_call_laure, LinphoneBaudotModeHearingCarryOver);
			send_and_check_baudot_text(lcs, marie_call_laure, laure, laure_call, "Yo");
			BC_ASSERT_EQUAL(laure->stat.number_of_LinphoneBaudotDetected, 1, int, "%d");
			BC_ASSERT_EQUAL(laure->stat.number_of_LinphoneBaudotUsDetected, 1, int, "%d");
		}
	}

	// 3. Agent (marie) puts everyone in a local conference
	participants = bctbx_list_append(NULL, laure);
	participants = bctbx_list_append(participants, pauline);

	initial_marie_stat = marie->stat;
	initial_pauline_stat = pauline->stat;
	initial_laure_stat = laure->stat;
	add_calls_to_local_conference(lcs, marie, NULL, participants, FALSE);

	BC_ASSERT_TRUE(wait_for_list(lcs, &pauline->stat.number_of_LinphoneCallStreamsRunning,
	                             initial_pauline_stat.number_of_LinphoneCallStreamsRunning + 1,
	                             liblinphone_tester_sip_timeout));
	BC_ASSERT_TRUE(wait_for_list(lcs, &laure->stat.number_of_LinphoneCallStreamsRunning,
	                             initial_laure_stat.number_of_LinphoneCallStreamsRunning + 1,
	                             liblinphone_tester_sip_timeout));
	BC_ASSERT_TRUE(wait_for_list(lcs, &marie->stat.number_of_LinphoneCallStreamsRunning,
	                             initial_marie_stat.number_of_LinphoneCallStreamsRunning + 2,
	                             liblinphone_tester_sip_timeout));

	conf = linphone_call_get_conference(marie_call_pauline);
	BC_ASSERT_PTR_NOT_NULL(conf);
	if (conf) {
		BC_ASSERT_TRUE(linphone_conference_is_in(conf));
	}
	BC_ASSERT_PTR_NOT_NULL(linphone_call_get_conference(marie_call_laure));

	// 4. Verify Baudot text exchange between the Baudot participants and the agent (marie)
	expected_marie_baudot = marie->stat.number_of_LinphoneBaudotDetected;
	switch (pauline_mode) {
		case LinphoneBaudotModeTty:
			// Bidirectional Baudot text: pauline -> marie, then marie -> pauline
			send_and_check_baudot_text(lcs, pauline_call, marie, marie_call_pauline, "Be l3l");
			expected_marie_baudot++;
			BC_ASSERT_EQUAL(marie->stat.number_of_LinphoneBaudotDetected, expected_marie_baudot, int, "%d");
			BC_ASSERT_EQUAL(marie->stat.number_of_LinphoneBaudotUsDetected, expected_marie_baudot, int, "%d");

			// Wait for pauline's detector post-send echo suppression window (300ms) to expire
			wait_for_list(lcs, NULL, 0, 500);

			send_and_check_baudot_text(lcs, marie_call_pauline, pauline, pauline_call, "Re 42");
			BC_ASSERT_EQUAL(pauline->stat.number_of_LinphoneBaudotDetected, 1, int, "%d");
			BC_ASSERT_EQUAL(pauline->stat.number_of_LinphoneBaudotUsDetected, 1, int, "%d");
			break;

		case LinphoneBaudotModeHearingCarryOver:
			// HCO participant cannot speak but can hear: sends Baudot text to agent
			send_and_check_baudot_text(lcs, pauline_call, marie, marie_call_pauline, "Be l3l");
			expected_marie_baudot++;
			BC_ASSERT_EQUAL(marie->stat.number_of_LinphoneBaudotDetected, expected_marie_baudot, int, "%d");
			BC_ASSERT_EQUAL(marie->stat.number_of_LinphoneBaudotUsDetected, expected_marie_baudot, int, "%d");
			break;

		case LinphoneBaudotModeVoiceCarryOver:
			// VCO participant can speak but cannot hear: receives Baudot text from agent
			send_and_check_baudot_text(lcs, marie_call_pauline, pauline, pauline_call, "Be l3l");
			BC_ASSERT_EQUAL(pauline->stat.number_of_LinphoneBaudotDetected, 2, int, "%d");
			BC_ASSERT_EQUAL(pauline->stat.number_of_LinphoneBaudotUsDetected, 2, int, "%d");
			break;

		case LinphoneBaudotModeVoice:
			break;
	}

	if (laure_mode != LinphoneBaudotModeVoice) {
		switch (laure_mode) {
			case LinphoneBaudotModeHearingCarryOver:
				send_and_check_baudot_text(lcs, laure_call, marie, marie_call_laure, "Ok");
				expected_marie_baudot++;
				BC_ASSERT_EQUAL(marie->stat.number_of_LinphoneBaudotDetected, expected_marie_baudot, int, "%d");
				BC_ASSERT_EQUAL(marie->stat.number_of_LinphoneBaudotUsDetected, expected_marie_baudot, int, "%d");
				break;
			case LinphoneBaudotModeVoiceCarryOver:
				send_and_check_baudot_text(lcs, marie_call_laure, laure, laure_call, "Ok");
				BC_ASSERT_EQUAL(laure->stat.number_of_LinphoneBaudotDetected, 2, int, "%d");
				BC_ASSERT_EQUAL(laure->stat.number_of_LinphoneBaudotUsDetected, 2, int, "%d");
				break;
			case LinphoneBaudotModeTty:
				send_and_check_baudot_text(lcs, laure_call, marie, marie_call_laure, "Ok");
				expected_marie_baudot++;
				BC_ASSERT_EQUAL(marie->stat.number_of_LinphoneBaudotDetected, expected_marie_baudot, int, "%d");
				wait_for_list(lcs, NULL, 0, 500);
				send_and_check_baudot_text(lcs, marie_call_laure, laure, laure_call, "Tx");
				BC_ASSERT_EQUAL(laure->stat.number_of_LinphoneBaudotDetected, 1, int, "%d");
				break;
			case LinphoneBaudotModeVoice:
				break;
		}
	}

	// Verify that laure never detected Baudot tones if in Voice or HCO mode
	if (laure_mode == LinphoneBaudotModeVoice || laure_mode == LinphoneBaudotModeHearingCarryOver) {
		BC_ASSERT_EQUAL(laure->stat.number_of_LinphoneBaudotDetected, 0, int, "%d");
	}

	// 5. Verify voice transmission according to Baudot modes
	if (conf) {
		LinphonePlayer *marie_player = linphone_conference_get_player(conf);
		LinphonePlayer *laure_player = linphone_call_get_player(laure_call);
		LinphonePlayer *pauline_player = linphone_call_get_player(pauline_call);
		bool_t pauline_hears_voice = baudot_mode_can_hear(pauline_mode);
		bool_t laure_hears_voice = baudot_mode_can_hear(laure_mode);

		// Agent (marie) speaks -> check whether pauline and laure hear voice according to their modes
		play_voice_and_check_reception(lcs, marie_player, voice_file, pauline_call, pauline_hears_voice, laure_call,
		                               laure_hears_voice, NULL, FALSE);

		// Third party (laure) speaks if capable -> check that marie hears laure in conf, and pauline if capable
		if (baudot_mode_can_speak(laure_mode)) {
			play_voice_and_check_reception(lcs, laure_player, voice_file, pauline_call, pauline_hears_voice, NULL,
			                               FALSE, conf, TRUE);
		}

		// Pauline speaks if capable -> check that marie hears pauline in conf, and laure if capable
		if (baudot_mode_can_speak(pauline_mode)) {
			play_voice_and_check_reception(lcs, pauline_player, voice_file, laure_call, laure_hears_voice, NULL, FALSE,
			                               conf, TRUE);
		}
	}

	// 6. Terminate conference and clean up
	terminate_conference(participants, marie, NULL, NULL, FALSE);
	BC_ASSERT_TRUE(wait_for_list(lcs, &pauline->stat.number_of_LinphoneCallEnd, 1, liblinphone_tester_sip_timeout));
	BC_ASSERT_TRUE(wait_for_list(lcs, &laure->stat.number_of_LinphoneCallEnd, 1, liblinphone_tester_sip_timeout));
	BC_ASSERT_TRUE(wait_for_list(lcs, &marie->stat.number_of_LinphoneCallEnd, 2, liblinphone_tester_sip_timeout));
	BC_ASSERT_TRUE(
	    wait_for_list(lcs, &pauline->stat.number_of_LinphoneCallReleased, 1, liblinphone_tester_sip_timeout));
	BC_ASSERT_TRUE(wait_for_list(lcs, &laure->stat.number_of_LinphoneCallReleased, 1, liblinphone_tester_sip_timeout));
	BC_ASSERT_TRUE(wait_for_list(lcs, &marie->stat.number_of_LinphoneCallReleased, 2, liblinphone_tester_sip_timeout));

end:
	bc_free(voice_file);
	if (participants) bctbx_list_free(participants);
	bctbx_list_free(lcs);
	linphone_core_remove_callbacks(marie->lc, marie_cbs);
	linphone_core_cbs_unref(marie_cbs);
	linphone_core_remove_callbacks(pauline->lc, pauline_cbs);
	linphone_core_cbs_unref(pauline_cbs);
	linphone_core_remove_callbacks(laure->lc, laure_cbs);
	linphone_core_cbs_unref(laure_cbs);
	destroy_mgr_in_conference(marie);
	destroy_mgr_in_conference(pauline);
	destroy_mgr_in_conference(laure);
}

static void baudot_local_conference_tty(void) {
	baudot_local_conference_relay_service(LinphoneBaudotModeTty, LinphoneBaudotModeVoice);
}

static void baudot_local_conference_hearing_carryover(void) {
	baudot_local_conference_relay_service(LinphoneBaudotModeHearingCarryOver, LinphoneBaudotModeVoice);
}

static void baudot_local_conference_voice_carryover(void) {
	baudot_local_conference_relay_service(LinphoneBaudotModeVoiceCarryOver, LinphoneBaudotModeVoice);
}

static void baudot_local_conference_hybrid_tty_hearing_carryover(void) {
	baudot_local_conference_relay_service(LinphoneBaudotModeTty, LinphoneBaudotModeHearingCarryOver);
}

static test_t baudot_tests[] = {
    TEST_NO_TAG("Baudot text message US", baudot_text_message_us),
    TEST_NO_TAG("Baudot text message Europe", baudot_text_message_europe),
    TEST_NO_TAG("Baudot text message voice switch to US TTY", baudot_text_message_voice_switch_tty_us),
    TEST_NO_TAG("Baudot text message voice switch to Europe TTY", baudot_text_message_voice_switch_tty_europe),
    TEST_NO_TAG("Baudot text message voice switch to US Voice CarryOver",
                baudot_text_message_voice_switch_voice_carryover_us),
    TEST_NO_TAG("Baudot text message voice switch to Europe Voice CarryOver",
                baudot_text_message_voice_switch_voice_carryover_europe),
    TEST_NO_TAG("Baudot in local conference TTY mode", baudot_local_conference_tty),
    TEST_NO_TAG("Baudot in local conference Hearing CarryOver mode", baudot_local_conference_hearing_carryover),
    TEST_NO_TAG("Baudot in local conference Voice CarryOver mode", baudot_local_conference_voice_carryover),
    TEST_NO_TAG("Baudot in local conference hybrid TTY Hearing CarryOver",
                baudot_local_conference_hybrid_tty_hearing_carryover),
};

test_suite_t baudot_test_suite = {"Baudot",
                                  NULL,
                                  NULL,
                                  liblinphone_tester_before_each,
                                  liblinphone_tester_after_each,
                                  sizeof(baudot_tests) / sizeof(baudot_tests[0]),
                                  baudot_tests,
                                  90,
                                  0};

#endif /* HAVE_BAUDOT */

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

#include <set>

#include "ortp/port.h"

#include "mediastreamer2/stun-message.h"
#include "mediastreamer2/stun-raw-message.h"
#include "mediastreamer2/stun.h"
#include "mediastreamer2/turn-context.h"
#include "mediastreamer2/turn-transport.h"

#include "liblinphone_tester.h"
#include "linphone/api/c-auth-info.h"
#include "linphone/api/c-nat-policy.h"
#include "linphone/core.h"
#include "shared_tester_functions.h"
#include "tester_utils.h"

// Prevent a GCC false positive warning, see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=122197#c19
#if defined(__GNUC__) && (__GNUC__ == 16) && (__GNUC_MINOR__ == 1)
#pragma GCC diagnostic warning "-Warray-bounds"
#endif

using namespace mediastreamer::nat;

static auto stun_address = "stun.example.org";

namespace {
class CallConfig {
public:
	CallConfig() {
		ipv6 = (liblinphone_tester_ipv6_available() != FALSE);
	}
	~CallConfig() = default;

	bool video_enabled = false;
	bool forced_relay = false;
	bool caller_turn_enabled = false;
	bool callee_turn_enabled = false;
	bool rtcp_mux_enabled = false;
	bool ipv6 = false;
	bool turn_udp = true;
	bool turn_tcp = false;
	bool turn_tls = false;
	bool wrong_password = false;
	LinphoneMediaEncryption mode = LinphoneMediaEncryptionNone;
};
} // namespace

static std::shared_ptr<StunRawMessage> test_stun_encode() {
	const auto request = StunMessage::createStunBindingRequest();
	auto transactionId = request->getTransactionId().asUInt96();
	transactionId.octet[0] = 11;
	request->setTransactionId(StunTransactionId(transactionId));
	return request->encode();
}

static void linphone_stun_test_encode() {
	const auto stunRawMessage = test_stun_encode();
	BC_ASSERT_PTR_NOT_NULL(stunRawMessage);
	const auto data = stunRawMessage->getData();
	BC_ASSERT(!data.empty());
	BCTBX_SLOGM << "STUN message encoded in " << data.size() << "bytes";
}

static void linphone_stun_test_grab_ip() {
	LinphoneCoreManager *lc_stun = linphone_core_manager_new_with_proxies_check("stun_rc", FALSE);
	int ping_time;
	int tmp = 0;
	char audio_addr[LINPHONE_IPADDR_SIZE] = {0};
	char video_addr[LINPHONE_IPADDR_SIZE] = {0};
	char text_addr[LINPHONE_IPADDR_SIZE] = {0};
	int audio_port = 0;
	int video_port = 0;
	int text_port = 0;

	/* This test verifies the very basic STUN support of liblinphone, which is deprecated.
	 * It works only in IPv4 mode and there is no plan to make it work over ipv6. */
	if (liblinphone_tester_ipv4_available() != FALSE) {
		goto end;
	}
	linphone_core_enable_ipv6(lc_stun->lc, FALSE);
	linphone_core_enable_realtime_text(lc_stun->lc, TRUE);
	linphone_core_set_stun_server(lc_stun->lc, stun_address);
	BC_ASSERT_STRING_EQUAL(stun_address, linphone_core_get_stun_server(lc_stun->lc));
	wait_for(lc_stun->lc, lc_stun->lc, &tmp, 1);

	ping_time = linphone_run_stun_tests(lc_stun->lc, 7078, 9078, 11078, audio_addr, &audio_port, video_addr,
	                                    &video_port, text_addr, &text_port);
	BC_ASSERT(ping_time != -1);

	ms_message("Round trip to STUN: %d ms", ping_time);

	BC_ASSERT(audio_addr[0] != '\0');
	BC_ASSERT(audio_port != 0);
#ifdef VIDEO_ENABLED
	BC_ASSERT(video_addr[0] != '\0');
	BC_ASSERT(video_port != 0);
#endif
	BC_ASSERT(text_addr[0] != '\0');
	BC_ASSERT(text_port != 0);

	ms_message("STUN test result: local audio port maps to %s:%i", audio_addr, audio_port);
#ifdef VIDEO_ENABLED
	ms_message("STUN test result: local video port maps to %s:%i", video_addr, video_port);
#endif
	ms_message("STUN test result: local text port maps to %s:%i", text_addr, text_port);

end:
	linphone_core_manager_destroy(lc_stun);
}

static void configure_nat_policy(
    LinphoneCore *lc, bool turn_enabled, bool turn_udp, bool turn_tcp, bool turn_tls, bool wrong_password) {
	LinphoneNatPolicy *nat_policy = linphone_core_create_nat_policy(lc);

	if (wrong_password) {
		LinphoneAuthInfo *auth_info = linphone_core_create_auth_info(
		    lc, "liblinphone-tester", nullptr, "wrong_password", nullptr, "sip.linphone.org", nullptr);
		linphone_core_add_auth_info(lc, auth_info);
		linphone_auth_info_unref(auth_info);
	} else if (linphone_core_get_account_creator_url(lc) != nullptr) {
		char *buffer =
		    bctbx_strdup_printf("%s%s", linphone_core_get_account_creator_url(lc), "accounts/me/services/turn");
		linphone_nat_policy_set_turn_configuration_endpoint(nat_policy, buffer);
		bctbx_free(buffer);
	}
	linphone_nat_policy_enable_ice(nat_policy, TRUE);
	if (turn_enabled) {
		linphone_nat_policy_enable_turn(nat_policy, TRUE);
		linphone_nat_policy_enable_udp_turn_transport(nat_policy, turn_udp ? TRUE : FALSE);
		linphone_nat_policy_enable_tcp_turn_transport(nat_policy, turn_tcp ? TRUE : FALSE);
		linphone_nat_policy_enable_tls_turn_transport(nat_policy, turn_tls ? TRUE : FALSE);
	} else {
		linphone_nat_policy_enable_stun(nat_policy, TRUE);
	}
	linphone_core_set_nat_policy(lc, nat_policy);
	linphone_nat_policy_unref(nat_policy);
}

static void check_turn_context_statistics_nb_allocate(const TurnContext *turnContext1,
                                                      const TurnContext *turnContext2,
                                                      const TurnTransport turnTransport,
                                                      const bool turnTransportEnabled) {
	for (const auto *turnContext : {turnContext1, turnContext2}) {
		const auto *turnTransportContext = turnContext->getTransportContext(turnTransport);
		if (turnTransportEnabled) {
			BC_ASSERT_PTR_NOT_NULL(turnTransportContext);
			if (turnTransportContext != nullptr) {
				BC_ASSERT_TRUE(turnTransportContext->getStatistics().nbSuccessfulAllocate > 0);
			}
		} else {
			BC_ASSERT_PTR_NULL(turnTransportContext);
		}
	}
}

static void check_turn_context_other_statistics(const TurnContext *turnContext1,
                                                const TurnContext *turnContext2,
                                                const TurnTransport turnTransport,
                                                const bool turnTransportEnabled,
                                                const std::set<TurnTransport> &possiblySuccessfulTransports) {
	const auto *turnTransportContext1 = turnContext1->getTransportContext(turnTransport);
	const auto *turnTransportContext2 = turnContext2->getTransportContext(turnTransport);
	if (turnTransportEnabled) {
		auto successfulTransport = false;
		if (possiblySuccessfulTransports.size() == 1) {
			successfulTransport =
			    possiblySuccessfulTransports.find(turnTransport) != possiblySuccessfulTransports.end();
		} else if (possiblySuccessfulTransports.size() == 2) {
			successfulTransport = (turnTransportContext1->getStatistics().nbReceivedChannelMsg > 0) ||
			                      (turnTransportContext2->getStatistics().nbReceivedChannelMsg > 0);
		}

		if (successfulTransport) {
			BC_ASSERT_TRUE((turnTransportContext1->getStatistics().nbReceivedChannelMsg > 0) ||
			               (turnTransportContext2->getStatistics().nbReceivedChannelMsg > 0));
			BC_ASSERT_TRUE((turnTransportContext1->getStatistics().nbSentChannelMsg > 0) ||
			               (turnTransportContext2->getStatistics().nbSentChannelMsg > 0));
			BC_ASSERT_TRUE((turnTransportContext1->getStatistics().nbSuccessfulRefresh > 0) ||
			               (turnTransportContext2->getStatistics().nbSuccessfulRefresh > 0));
			BC_ASSERT_TRUE((turnTransportContext1->getStatistics().nbSuccessfulCreatePermission > 0) ||
			               (turnTransportContext2->getStatistics().nbSuccessfulCreatePermission > 0));
			BC_ASSERT_TRUE((turnTransportContext1->getStatistics().nbSuccessfulChannelBind > 0) ||
			               (turnTransportContext2->getStatistics().nbSuccessfulChannelBind > 0));
		} else {
			BC_ASSERT_TRUE((turnTransportContext1->getStatistics().nbReceivedChannelMsg == 0) ||
			               (turnTransportContext2->getStatistics().nbReceivedChannelMsg == 0));
			BC_ASSERT_TRUE((turnTransportContext1->getStatistics().nbSentChannelMsg == 0) ||
			               (turnTransportContext2->getStatistics().nbSentChannelMsg == 0));
			BC_ASSERT_TRUE((turnTransportContext1->getStatistics().nbSuccessfulRefresh == 0) ||
			               (turnTransportContext2->getStatistics().nbSuccessfulRefresh == 0));
			BC_ASSERT_TRUE((turnTransportContext1->getStatistics().nbSuccessfulCreatePermission == 0) ||
			               (turnTransportContext2->getStatistics().nbSuccessfulCreatePermission == 0));
			BC_ASSERT_TRUE((turnTransportContext1->getStatistics().nbSuccessfulChannelBind == 0) ||
			               (turnTransportContext2->getStatistics().nbSuccessfulChannelBind == 0));
		}
	}
}

static void check_turn_context_statistics(const TurnContext *turnContext1,
                                          const TurnContext *turnContext2,
                                          const CallConfig *config) {
	check_turn_context_statistics_nb_allocate(turnContext1, turnContext2, TurnTransport::Udp, config->turn_udp);
	check_turn_context_statistics_nb_allocate(turnContext1, turnContext2, TurnTransport::Tcp, config->turn_tcp);
	check_turn_context_statistics_nb_allocate(turnContext1, turnContext2, TurnTransport::Tls, config->turn_tls);
	if (config->forced_relay) {
		// If multiple TURN transports are enabled, we are not sure which transport will be chosen by ICE and need to
		// test several
		std::set<TurnTransport> possiblySuccessfulTransports;
		if (config->turn_udp) {
			possiblySuccessfulTransports.insert(TurnTransport::Udp);
		}
		if (config->turn_tls) {
			possiblySuccessfulTransports.insert(TurnTransport::Tls);
		}
		if (config->turn_tcp && (!config->turn_udp || !config->turn_tls)) {
			possiblySuccessfulTransports.insert(TurnTransport::Tcp);
		}
		check_turn_context_other_statistics(turnContext1, turnContext2, TurnTransport::Udp, config->turn_udp,
		                                    possiblySuccessfulTransports);
		check_turn_context_other_statistics(turnContext1, turnContext2, TurnTransport::Tcp, config->turn_tcp,
		                                    possiblySuccessfulTransports);
		check_turn_context_other_statistics(turnContext1, turnContext2, TurnTransport::Tls, config->turn_tls,
		                                    possiblySuccessfulTransports);
	}
}

static void ice_turn_call_base(const CallConfig *config) {
	LinphoneIceState expected_ice_state = LinphoneIceStateHostConnection;
	LinphoneMediaDirection expected_video_dir = LinphoneMediaDirectionInactive;
	bctbx_list_t *lcs = nullptr;
	const MSTurnContext *rtp_turn_context1 = nullptr;
	const MSTurnContext *rtp_turn_context2 = nullptr;
	const MSTurnContext *rtcp_turn_context1 = nullptr;
	const MSTurnContext *rtcp_turn_context2 = nullptr;

	LinphoneCoreManager *marie = linphone_core_manager_create(
	    (transport_supported(LinphoneTransportTls) != FALSE) ? "marie_sips_rc" : "marie_rc");
	lcs = bctbx_list_append(lcs, marie->lc);
	LinphoneCoreManager *pauline = linphone_core_manager_create(
	    (transport_supported(LinphoneTransportTls) != FALSE) ? "pauline_rc" : "pauline_tcp_rc");
	lcs = bctbx_list_append(lcs, pauline->lc);

	if (config->ipv6) {
		linphone_core_enable_ipv6(marie->lc, TRUE);
		linphone_core_enable_ipv6(pauline->lc, TRUE);
	} else {
		linphone_core_enable_ipv6(marie->lc, FALSE);
		linphone_core_enable_ipv6(pauline->lc, FALSE);
	}

	linphone_config_set_int(linphone_core_get_config(marie->lc), "sip", "update_call_when_ice_completed_with_dtls", 1);
	linphone_config_set_int(linphone_core_get_config(pauline->lc), "sip", "update_call_when_ice_completed_with_dtls",
	                        1);

	if (config->forced_relay) {
		linphone_core_enable_forced_ice_relay(marie->lc, TRUE);
		linphone_core_enable_forced_ice_relay(pauline->lc, TRUE);
		linphone_core_enable_short_turn_refresh(marie->lc, TRUE);
		linphone_core_enable_short_turn_refresh(pauline->lc, TRUE);
		expected_ice_state = LinphoneIceStateRelayConnection;
	}
	if (config->rtcp_mux_enabled) {
		linphone_config_set_int(linphone_core_get_config(marie->lc), "rtp", "rtcp_mux", 1);
		linphone_config_set_int(linphone_core_get_config(pauline->lc), "rtp", "rtcp_mux", 1);
	}

	if (linphone_core_media_encryption_supported(marie->lc, config->mode) != FALSE) {
		linphone_core_set_media_encryption(marie->lc, config->mode);
		linphone_core_set_media_encryption(pauline->lc, config->mode);

		if (config->mode == LinphoneMediaEncryptionDTLS) { /* for DTLS we must access certificates or at least have a
			                                                  directory to store them */
			char *path = bc_tester_file("certificates-marie");
			linphone_core_set_user_certificates_path(marie->lc, path);
			bc_free(path);
			path = bc_tester_file("certificates-pauline");
			linphone_core_set_user_certificates_path(pauline->lc, path);
			bc_free(path);
			bctbx_mkdir(linphone_core_get_user_certificates_path(marie->lc));
			bctbx_mkdir(linphone_core_get_user_certificates_path(pauline->lc));
		}
	}

	linphone_core_manager_start(marie, TRUE);
	linphone_core_manager_start(pauline, TRUE);

	configure_nat_policy(marie->lc, config->caller_turn_enabled, config->turn_udp, config->turn_tcp, config->turn_tls,
	                     config->wrong_password);
	configure_nat_policy(pauline->lc, config->callee_turn_enabled, config->turn_udp, config->turn_tcp, config->turn_tls,
	                     config->wrong_password);

	if (config->video_enabled) {
#ifdef VIDEO_ENABLED
		linphone_core_set_video_device(pauline->lc, liblinphone_tester_mire_id);
		linphone_core_set_video_device(marie->lc, liblinphone_tester_mire_id);
		video_call_base_2(marie, pauline, FALSE, LinphoneMediaEncryptionNone, TRUE, TRUE);
		expected_video_dir = LinphoneMediaDirectionSendRecv;
#endif
	} else {
		BC_ASSERT_TRUE(call(marie, pauline));
	}

	/* Wait for the ICE reINVITE to complete */
	BC_ASSERT_TRUE(wait_for(pauline->lc, marie->lc, &pauline->stat.number_of_LinphoneCallStreamsRunning, 2));
	BC_ASSERT_TRUE(wait_for(pauline->lc, marie->lc, &marie->stat.number_of_LinphoneCallStreamsRunning, 2));
	BC_ASSERT_TRUE(check_ice(pauline, marie, expected_ice_state));
	check_nb_media_starts(AUDIO_START, pauline, marie, 1, 1);
	ms_message("Checking marie's streams");
	check_media_direction(marie, linphone_core_get_current_call(marie->lc), lcs, LinphoneMediaDirectionSendRecv,
	                      expected_video_dir);
	ms_message("Checking pauline's streams");
	check_media_direction(pauline, linphone_core_get_current_call(pauline->lc), lcs, LinphoneMediaDirectionSendRecv,
	                      expected_video_dir);
	liblinphone_tester_check_rtcp(marie, pauline);
	const LinphoneCall *lcall = linphone_core_get_current_call(marie->lc);
	BC_ASSERT_PTR_NOT_NULL(lcall);
	if (lcall != nullptr) {
		BC_ASSERT_TRUE(linphone_call_has_ice_session(lcall));
		BC_ASSERT_TRUE(linphone_call_has_ice_check_list(lcall, 0));
		rtp_turn_context1 = linphone_call_get_ice_checklist_rtp_turn_context(lcall, 0);
		rtcp_turn_context1 = linphone_call_get_ice_checklist_rtcp_turn_context(lcall, 0);
		if (config->caller_turn_enabled && !config->wrong_password) {
			BC_ASSERT_PTR_NOT_NULL(rtp_turn_context1);
			BC_ASSERT_PTR_NOT_NULL(rtcp_turn_context1);
		} else {
			BC_ASSERT_PTR_NULL(rtp_turn_context1);
			BC_ASSERT_PTR_NULL(rtcp_turn_context1);
		}
	}
	lcall = linphone_core_get_current_call(pauline->lc);
	BC_ASSERT_PTR_NOT_NULL(lcall);
	if (lcall != nullptr) {
		BC_ASSERT_TRUE(linphone_call_has_ice_session(lcall));
		BC_ASSERT_TRUE(linphone_call_has_ice_check_list(lcall, 0));
		rtp_turn_context2 = linphone_call_get_ice_checklist_rtp_turn_context(lcall, 0);
		rtcp_turn_context2 = linphone_call_get_ice_checklist_rtcp_turn_context(lcall, 0);
		if (config->callee_turn_enabled) {
			BC_ASSERT_PTR_NOT_NULL(rtp_turn_context2);
			BC_ASSERT_PTR_NOT_NULL(rtcp_turn_context2);
		} else {
			BC_ASSERT_PTR_NULL(rtp_turn_context2);
			BC_ASSERT_PTR_NULL(rtcp_turn_context2);
		}
	}
	/*
	 * We perform turn context checks to both ends at the same time.
	 * Indeed, we cannot predict which relay candidates will be used, since both sides are proposing them.
	 * We have to check that turn channel is used by either marie or pauline.
	 */
	if (!config->wrong_password && (rtp_turn_context1 != nullptr) && (rtcp_turn_context1 != nullptr) &&
	    (rtp_turn_context2 != nullptr) && (rtcp_turn_context2 != nullptr)) {
		check_turn_context_statistics(reinterpret_cast<const TurnContext *>(rtp_turn_context1),
		                              reinterpret_cast<const TurnContext *>(rtp_turn_context2), config);
		if (!config->rtcp_mux_enabled) {
			check_turn_context_statistics(reinterpret_cast<const TurnContext *>(rtcp_turn_context1),
			                              reinterpret_cast<const TurnContext *>(rtcp_turn_context2), config);
		}
	}

	end_call(marie, pauline);

	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(marie);
	bctbx_list_free(lcs);
}

static void basic_ice_turn_call() {
	CallConfig cfg{};
	cfg.caller_turn_enabled = true;
	cfg.callee_turn_enabled = true;
	ice_turn_call_base(&cfg);
}

/* In this test, TURN won't finally be used because of wrong password. This checks that in this case
 * all goes well with ICE and things terminate properly.*/
static void basic_ice_turn_call_wrong_password() {
	CallConfig cfg{};
	cfg.caller_turn_enabled = true;
	cfg.wrong_password = true;
	cfg.callee_turn_enabled = false;
	ice_turn_call_base(&cfg);
}

static void basic_ipv6_ice_turn_call() {
	if (liblinphone_tester_ipv6_available() != FALSE) {
		CallConfig cfg{};
		cfg.caller_turn_enabled = true;
		cfg.callee_turn_enabled = true;
		cfg.ipv6 = true;
		ice_turn_call_base(&cfg);
	} else {
		ms_warning("Test skipped, no ipv6 available");
	}
}

static void basic_ice_turn_call_tcp() {
	CallConfig cfg{};
	cfg.caller_turn_enabled = true;
	cfg.callee_turn_enabled = true;
	cfg.turn_udp = false;
	cfg.turn_tcp = true;
	ice_turn_call_base(&cfg);
}

static void basic_ice_turn_call_tls() {
	CallConfig cfg{};
	cfg.caller_turn_enabled = true;
	cfg.callee_turn_enabled = true;
	cfg.turn_udp = false;
	cfg.turn_tls = true;
	ice_turn_call_base(&cfg);
}

static void basic_ice_turn_call_udp_tcp() {
	CallConfig cfg{};
	cfg.caller_turn_enabled = true;
	cfg.callee_turn_enabled = true;
	cfg.turn_udp = true;
	cfg.turn_tcp = true;
	ice_turn_call_base(&cfg);
}

static void basic_ice_turn_call_udp_tls() {
	CallConfig cfg{};
	cfg.caller_turn_enabled = true;
	cfg.callee_turn_enabled = true;
	cfg.turn_udp = true;
	cfg.turn_tls = true;
	ice_turn_call_base(&cfg);
}

static void basic_ice_turn_call_tcp_tls() {
	CallConfig cfg{};
	cfg.caller_turn_enabled = true;
	cfg.callee_turn_enabled = true;
	cfg.turn_udp = false;
	cfg.turn_tcp = true;
	cfg.turn_tls = true;
	ice_turn_call_base(&cfg);
}

static void basic_ice_turn_call_udp_tcp_tls() {
	CallConfig cfg{};
	cfg.caller_turn_enabled = true;
	cfg.callee_turn_enabled = true;
	cfg.turn_udp = true;
	cfg.turn_tcp = true;
	cfg.turn_tls = true;
	ice_turn_call_base(&cfg);
}

#ifdef VIDEO_ENABLED
static void video_ice_turn_call() {
	CallConfig cfg{};
	cfg.video_enabled = true;
	cfg.caller_turn_enabled = true;
	cfg.callee_turn_enabled = true;
	ice_turn_call_base(&cfg);
}
#endif

static void relayed_ice_turn_call() {
	CallConfig cfg{};
	cfg.forced_relay = true;
	cfg.caller_turn_enabled = true;
	cfg.callee_turn_enabled = true;
	ice_turn_call_base(&cfg);
}

static void relayed_ice_turn_call_with_tcp() {
	CallConfig cfg{};
	cfg.forced_relay = true;
	cfg.caller_turn_enabled = true;
	cfg.callee_turn_enabled = true;
	cfg.turn_udp = false;
	cfg.turn_tcp = true;
	ice_turn_call_base(&cfg);
}

static void relayed_ice_turn_call_with_tls() {
	CallConfig cfg{};
	cfg.forced_relay = true;
	cfg.caller_turn_enabled = true;
	cfg.callee_turn_enabled = true;
	cfg.turn_udp = false;
	cfg.turn_tls = true;
	ice_turn_call_base(&cfg);
}

static void relayed_ice_turn_call_with_udp_tcp() {
	CallConfig cfg{};
	cfg.forced_relay = true;
	cfg.caller_turn_enabled = true;
	cfg.callee_turn_enabled = true;
	cfg.turn_udp = true;
	cfg.turn_tcp = true;
	ice_turn_call_base(&cfg);
}

static void relayed_ice_turn_call_with_udp_tls() {
	CallConfig cfg{};
	cfg.forced_relay = true;
	cfg.caller_turn_enabled = true;
	cfg.callee_turn_enabled = true;
	cfg.turn_udp = true;
	cfg.turn_tls = true;
	ice_turn_call_base(&cfg);
}

static void relayed_ice_turn_call_with_tcp_tls() {
	CallConfig cfg{};
	cfg.forced_relay = true;
	cfg.caller_turn_enabled = true;
	cfg.callee_turn_enabled = true;
	cfg.turn_udp = false;
	cfg.turn_tcp = true;
	cfg.turn_tls = true;
	ice_turn_call_base(&cfg);
}

static void relayed_ice_turn_call_with_udp_tcp_tls() {
	CallConfig cfg{};
	cfg.forced_relay = true;
	cfg.caller_turn_enabled = true;
	cfg.callee_turn_enabled = true;
	cfg.turn_udp = true;
	cfg.turn_tcp = true;
	cfg.turn_tls = true;
	ice_turn_call_base(&cfg);
}

#ifdef VIDEO_ENABLED
static void relayed_video_ice_turn_call() {
	CallConfig cfg{};
	cfg.video_enabled = true;
	cfg.forced_relay = true;
	cfg.caller_turn_enabled = true;
	cfg.callee_turn_enabled = true;
	ice_turn_call_base(&cfg);
}
#endif

static void relayed_ice_turn_call_with_rtcp_mux() {
	CallConfig cfg{};
	cfg.forced_relay = true;
	cfg.caller_turn_enabled = true;
	cfg.rtcp_mux_enabled = true;
	ice_turn_call_base(&cfg);
}

static void relayed_ice_turn_to_ice_stun_call() {
	CallConfig cfg{};
	cfg.forced_relay = true;
	cfg.caller_turn_enabled = true;
	ice_turn_call_base(&cfg);
}

static void relayed_ice_turn_call_with_srtp() {
	CallConfig cfg{};
	cfg.forced_relay = true;
	cfg.caller_turn_enabled = true;
	cfg.mode = LinphoneMediaEncryptionSRTP;
	ice_turn_call_base(&cfg);
}

static void relayed_ice_turn_tls_with_srtp() {
	CallConfig cfg{};
	cfg.forced_relay = true;
	cfg.caller_turn_enabled = true;
	cfg.callee_turn_enabled = true;
	cfg.turn_udp = false;
	cfg.turn_tls = true;
	cfg.mode = LinphoneMediaEncryptionSRTP;
	ice_turn_call_base(&cfg);
}

static void relayed_ice_turn_tls_to_ice_with_srtp() {
	CallConfig cfg{};
	cfg.forced_relay = true;
	cfg.caller_turn_enabled = true;
	cfg.rtcp_mux_enabled = true;
	cfg.turn_udp = false;
	cfg.turn_tls = true;
	cfg.mode = LinphoneMediaEncryptionSRTP;
	ice_turn_call_base(&cfg);
}

static void relayed_ice_turn_to_ice_with_dtls_srtp() {
	CallConfig cfg{};
	cfg.forced_relay = true;
	cfg.caller_turn_enabled = true;
	cfg.rtcp_mux_enabled = true;
	cfg.mode = LinphoneMediaEncryptionDTLS;
	ice_turn_call_base(&cfg);
}

/* specific test that checks that in the case of a call with DTLS-SRTP, ICE, and TURN
 * the handshake is started immediately after ICE has successfully verified the relay pair.
 */
static void ice_turn_dtls_call(const CallConfig *config) {
	LinphoneIceState expected_ice_state = LinphoneIceStateHostConnection;
	bctbx_list_t *lcs = nullptr;
	bool_t dtls_started_as_expected = FALSE;

	LinphoneCoreManager *marie = linphone_core_manager_create(
	    (transport_supported(LinphoneTransportTls) != FALSE) ? "marie_sips_rc" : "marie_rc");
	lcs = bctbx_list_append(lcs, marie->lc);
	LinphoneCoreManager *pauline = linphone_core_manager_create(
	    (transport_supported(LinphoneTransportTls) != FALSE) ? "pauline_rc" : "pauline_tcp_rc");
	lcs = bctbx_list_append(lcs, pauline->lc);

	if (config->ipv6) {
		linphone_core_enable_ipv6(marie->lc, TRUE);
		linphone_core_enable_ipv6(pauline->lc, TRUE);
	} else {
		linphone_core_enable_ipv6(marie->lc, FALSE);
		linphone_core_enable_ipv6(pauline->lc, FALSE);
	}

	linphone_config_set_int(linphone_core_get_config(marie->lc), "sip", "update_call_when_ice_completed_with_dtls", 1);
	linphone_config_set_int(linphone_core_get_config(pauline->lc), "sip", "update_call_when_ice_completed_with_dtls",
	                        1);

	if (config->forced_relay) {
		linphone_core_enable_forced_ice_relay(marie->lc, TRUE);
		linphone_core_enable_forced_ice_relay(pauline->lc, TRUE);
		linphone_core_enable_short_turn_refresh(marie->lc, TRUE);
		linphone_core_enable_short_turn_refresh(pauline->lc, TRUE);
		expected_ice_state = LinphoneIceStateRelayConnection;
	}
	if (config->rtcp_mux_enabled) {
		linphone_config_set_int(linphone_core_get_config(marie->lc), "rtp", "rtcp_mux", 1);
		linphone_config_set_int(linphone_core_get_config(pauline->lc), "rtp", "rtcp_mux", 1);
	}

	if (linphone_core_media_encryption_supported(marie->lc, config->mode) != FALSE) {
		linphone_core_set_media_encryption(marie->lc, config->mode);
		linphone_core_set_media_encryption(pauline->lc, config->mode);

		if (config->mode == LinphoneMediaEncryptionDTLS) { /* for DTLS we must access certificates or at least have a
			                                                  directory to store them */
			char *path = bc_tester_file("certificates-marie");
			linphone_core_set_user_certificates_path(marie->lc, path);
			bc_free(path);
			path = bc_tester_file("certificates-pauline");
			linphone_core_set_user_certificates_path(pauline->lc, path);
			bc_free(path);
			bctbx_mkdir(linphone_core_get_user_certificates_path(marie->lc));
			bctbx_mkdir(linphone_core_get_user_certificates_path(pauline->lc));
		}
	}

	linphone_core_manager_start(marie, TRUE);
	linphone_core_manager_start(pauline, TRUE);

	configure_nat_policy(marie->lc, config->caller_turn_enabled, config->turn_udp, config->turn_tcp, config->turn_tls,
	                     config->wrong_password);
	configure_nat_policy(pauline->lc, config->callee_turn_enabled, config->turn_udp, config->turn_tcp, config->turn_tls,
	                     config->wrong_password);

	linphone_core_set_video_device(pauline->lc, liblinphone_tester_mire_id);
	linphone_core_set_video_device(marie->lc, liblinphone_tester_mire_id);

	linphone_core_invite_address(marie->lc, pauline->identity);
	BC_ASSERT_TRUE(wait_for(pauline->lc, marie->lc, &pauline->stat.number_of_LinphoneCallIncomingReceived, 1));
	LinphoneCall *lcall = linphone_core_get_current_call(pauline->lc);
	BC_ASSERT_PTR_NOT_NULL(lcall);
	if (lcall == nullptr) {
		goto end;
	}
	linphone_call_accept(lcall);
	BC_ASSERT_TRUE(wait_for(pauline->lc, marie->lc, &pauline->stat.number_of_LinphoneCallConnected, 1));
	BC_ASSERT_TRUE(wait_for(pauline->lc, marie->lc, &marie->stat.number_of_LinphoneCallConnected, 1));
	/* Pauline shall not start dtls until the check list has verified the default pair */
	for (int attempts = 0; attempts < 100; ++attempts) {
		const LinphoneStreamInternalStats *istats =
		    _linphone_call_get_stream_internal_stats(lcall, LinphoneStreamTypeAudio);
		BC_ASSERT_PTR_NOT_NULL(istats);
		if (istats == nullptr) {
			break;
		}
		if (istats->number_of_ice_check_list_relay_pair_verified == 0) {
			BC_ASSERT_TRUE(istats->number_of_dtls_starts == 0);
		} else {
			/* relay pair verified */
			if (BC_ASSERT_TRUE(istats->number_of_dtls_starts == 1)) {
				dtls_started_as_expected = TRUE;
				BC_ASSERT_TRUE(istats->number_of_ice_check_list_processing_finished == 0);
				break;
			}
		}
		wait_for_list(lcs, nullptr, 0, 10); /* the timer must be short so that ice_check list does not finish */
	}
	BC_ASSERT_TRUE(dtls_started_as_expected);

	BC_ASSERT_TRUE(wait_for(pauline->lc, marie->lc, &pauline->stat.number_of_LinphoneCallStreamsRunning, 1));
	BC_ASSERT_TRUE(wait_for(pauline->lc, marie->lc, &marie->stat.number_of_LinphoneCallStreamsRunning, 1));
	liblinphone_tester_check_ice_default_candidates(marie, TesterIceCandidateRelay, pauline, TesterIceCandidateSflrx);

	/* Wait for the ICE reINVITE to complete */
	BC_ASSERT_TRUE(wait_for(pauline->lc, marie->lc, &pauline->stat.number_of_LinphoneCallStreamsRunning, 2));
	BC_ASSERT_TRUE(wait_for(pauline->lc, marie->lc, &marie->stat.number_of_LinphoneCallStreamsRunning, 2));
	liblinphone_tester_check_rtcp(marie, pauline);
	BC_ASSERT_TRUE(check_ice(pauline, marie, expected_ice_state));

	end_call(marie, pauline);

end:
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(marie);
	bctbx_list_free(lcs);
}

static void relayed_ice_turn_to_turn_with_dtls_srtp() {
	CallConfig cfg{};
	cfg.forced_relay = true;
	cfg.caller_turn_enabled = true;
	cfg.callee_turn_enabled = true;
	cfg.rtcp_mux_enabled = true;
	cfg.mode = LinphoneMediaEncryptionDTLS;
	ice_turn_dtls_call(&cfg);
}

static test_t stun_tests[] = {
    TEST_ONE_TAG("Basic Stun test (Ping/public IP)", linphone_stun_test_grab_ip, "STUN"),
    TEST_ONE_TAG("STUN encode", linphone_stun_test_encode, "STUN"),
    TEST_TWO_TAGS("Basic ICE+TURN call", basic_ice_turn_call, "ICE", "TURN"),
    TEST_TWO_TAGS("Basic ICE+TURN call with wrong password", basic_ice_turn_call_wrong_password, "ICE", "TURN"),
    TEST_TWO_TAGS("Basic IPv6 ICE+TURN call", basic_ipv6_ice_turn_call, "ICE", "TURN"),
    TEST_TWO_TAGS("Basic ICE+TURN call with TCP", basic_ice_turn_call_tcp, "ICE", "TURN"),
    TEST_TWO_TAGS("Basic ICE+TURN call with TLS", basic_ice_turn_call_tls, "ICE", "TURN"),
    TEST_TWO_TAGS("Basic ICE+TURN call with UDP & TCP", basic_ice_turn_call_udp_tcp, "ICE", "TURN"),
    TEST_TWO_TAGS("Basic ICE+TURN call with UDP & TLS", basic_ice_turn_call_udp_tls, "ICE", "TURN"),
    TEST_TWO_TAGS("Basic ICE+TURN call with TCP & TLS", basic_ice_turn_call_tcp_tls, "ICE", "TURN"),
    TEST_TWO_TAGS("Basic ICE+TURN call with UDP, TCP & TLS", basic_ice_turn_call_udp_tcp_tls, "ICE", "TURN"),
#ifdef VIDEO_ENABLED
    TEST_TWO_TAGS("Video ICE+TURN call", video_ice_turn_call, "ICE", "TURN"),
    TEST_TWO_TAGS("Relayed video ICE+TURN call", relayed_video_ice_turn_call, "ICE", "TURN"),
#endif
    TEST_TWO_TAGS("Relayed ICE+TURN call", relayed_ice_turn_call, "ICE", "TURN"),
    TEST_TWO_TAGS("Relayed ICE+TURN call with TCP", relayed_ice_turn_call_with_tcp, "ICE", "TURN"),
    TEST_TWO_TAGS("Relayed ICE+TURN call with TLS", relayed_ice_turn_call_with_tls, "ICE", "TURN"),
    TEST_TWO_TAGS("Relayed ICE+TURN call with UDP & TCP", relayed_ice_turn_call_with_udp_tcp, "ICE", "TURN"),
    TEST_TWO_TAGS("Relayed ICE+TURN call with UDP & TLS", relayed_ice_turn_call_with_udp_tls, "ICE", "TURN"),
    TEST_TWO_TAGS("Relayed ICE+TURN call with TCP & TLS", relayed_ice_turn_call_with_tcp_tls, "ICE", "TURN"),
    TEST_TWO_TAGS("Relayed ICE+TURN call with UDP, TCP & TLS", relayed_ice_turn_call_with_udp_tcp_tls, "ICE", "TURN"),
    TEST_TWO_TAGS("Relayed ICE+TURN call with rtcp-mux", relayed_ice_turn_call_with_rtcp_mux, "ICE", "TURN"),
    TEST_TWO_TAGS("Relayed ICE+TURN to ICE+STUN call", relayed_ice_turn_to_ice_stun_call, "ICE", "TURN"),
    TEST_TWO_TAGS("Relayed ICE+TURN call with SRTP", relayed_ice_turn_call_with_srtp, "ICE", "TURN"),
    TEST_TWO_TAGS("Relayed ICE+TURN TLS call with SRTP", relayed_ice_turn_tls_with_srtp, "ICE", "TURN"),
    TEST_TWO_TAGS("Relayed ICE+TURN TLS call to ICE with SRTP", relayed_ice_turn_tls_to_ice_with_srtp, "ICE", "TURN"),
    TEST_TWO_TAGS("Relayed ICE+TURN call to ICE with DTLS-SRTP", relayed_ice_turn_to_ice_with_dtls_srtp, "ICE", "TURN"),
    TEST_TWO_TAGS(
        "Relayed ICE+TURN relayed call with DTLS-SRTP", relayed_ice_turn_to_turn_with_dtls_srtp, "ICE", "TURN")};

test_suite_t stun_test_suite = {
    "Stun",     nullptr, nullptr, liblinphone_tester_before_each, liblinphone_tester_after_each, std::size(stun_tests),
    stun_tests, 0};

/*
 * Copyright (c) 2010-2025 Belledonne Communications SARL.
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

#include "bctoolbox/defs.h"
#include "belle_sip_tester_utils.h"

#include "http-server-utils.h"
#include "liblinphone_tester++.h"
#include "liblinphone_tester.h"
#include "shared_tester_functions.h"

static void configure_nat_policy_with_turn(LinphoneCoreManager *core_manager) {
	LinphoneNatPolicy *nat_policy = linphone_core_create_nat_policy(core_manager->lc);
	linphone_nat_policy_enable_turn(nat_policy, TRUE);
	linphone_nat_policy_enable_ice(nat_policy, TRUE);
	linphone_nat_policy_enable_stun(nat_policy, TRUE);
	std::string url = linphone_core_get_account_creator_url(core_manager->lc);
	linphone_nat_policy_set_turn_configuration_endpoint(nat_policy, (url + "accounts/me/services/turn").c_str());

	auto default_account = linphone_core_get_default_account(core_manager->lc);
	const LinphoneAccountParams *account_params = linphone_account_get_params(default_account);
	LinphoneAccountParams *new_account_params = linphone_account_params_clone(account_params);
	linphone_account_params_set_nat_policy(new_account_params, nat_policy);
	linphone_account_set_params(default_account, new_account_params);

	linphone_account_params_unref(new_account_params);
	linphone_nat_policy_unref(nat_policy);
}

static std::string check_turn_credentials(LinphoneCoreManager *core_manager, const int &ttl = 0) {
	LinphoneCall *call = linphone_core_get_current_call(core_manager->lc);
	LinphoneNatPolicy *policy = get_nat_policy_for_call(core_manager, call);
	BC_ASSERT_PTR_NOT_NULL(policy);
	const char *username = linphone_nat_policy_get_stun_server_username(policy);
	BC_ASSERT_PTR_NOT_NULL(username);
	BC_ASSERT_PTR_NOT_NULL(linphone_nat_policy_get_stun_server(policy));
	const LinphoneAuthInfo *auth_info = linphone_core_find_auth_info(core_manager->lc, nullptr, username, nullptr);
	if (BC_ASSERT_PTR_NOT_NULL(auth_info)) {
		BC_ASSERT_PTR_NOT_NULL(linphone_auth_info_get_password(auth_info));
		if (ttl > 0) {
			LinphoneAuthInfo *new_auth = linphone_auth_info_clone(auth_info);
			auto expiration = (int)(ttl * 0.8); // 80% of the given duration. Check NatPolicy.
			time_t expires = time(nullptr) + expiration;
			linphone_auth_info_set_expires(new_auth, expires);
			linphone_core_remove_auth_info(core_manager->lc, auth_info);
			linphone_core_add_auth_info(core_manager->lc, new_auth);
			linphone_auth_info_unref(new_auth);
		}
	}
	return username;
}

static void update_turn_configuration_test() {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *pauline =
	    linphone_core_manager_new(transport_supported(LinphoneTransportTls) ? "pauline_rc" : "pauline_tcp_rc");

	configure_nat_policy_with_turn(marie);
	configure_nat_policy_with_turn(pauline);

	linphone_core_enable_forced_ice_relay(marie->lc, TRUE);
	linphone_core_enable_forced_ice_relay(pauline->lc, TRUE);

	if (!BC_ASSERT_TRUE(call(pauline, marie))) return;

	/*wait for the ICE reINVITE to complete*/
	BC_ASSERT_TRUE(wait_for(pauline->lc, marie->lc, &pauline->stat.number_of_LinphoneCallStreamsRunning, 2));
	BC_ASSERT_TRUE(wait_for(pauline->lc, marie->lc, &marie->stat.number_of_LinphoneCallStreamsRunning, 2));

	BC_ASSERT_TRUE(check_ice(pauline, marie, LinphoneIceStateRelayConnection));

	check_nb_media_starts(AUDIO_START, pauline, marie, 1, 1);

	liblinphone_tester_check_rtcp(marie, pauline);

	// Reset TTL to remove auth after the call
	std::string marieUsername = check_turn_credentials(marie, 10);
	std::string paulineUsername = check_turn_credentials(pauline, 10);

	/*then close the call*/
	end_call(pauline, marie);

	wait_for_until(marie->lc, pauline->lc, nullptr, 1, 10000);
	// Auth doesn't exist anymore because of expiration
	BC_ASSERT_PTR_NULL(linphone_core_find_auth_info(marie->lc, nullptr, marieUsername.c_str(), nullptr));
	BC_ASSERT_PTR_NULL(linphone_core_find_auth_info(pauline->lc, nullptr, paulineUsername.c_str(), nullptr));

	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
}

test_t turn_server_tests[] = {
    TEST_NO_TAG("Update TURN configuration", update_turn_configuration_test),
};

test_suite_t turn_server_test_suite = {"Turn",
                                       nullptr,
                                       nullptr,
                                       liblinphone_tester_before_each,
                                       liblinphone_tester_after_each,
                                       sizeof(turn_server_tests) / sizeof(turn_server_tests[0]),
                                       turn_server_tests,
                                       0};
/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
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

#include <bctoolbox/defs.h>

#include "liblinphone_tester.h"
#include "linphone/api/c-account-params.h"
#include "linphone/api/c-account.h"
#include "linphone/api/c-address.h"
#include "linphone/api/c-auth-info.h"
#include "linphone/api/c-digest-authentication-policy.h"
#include "linphone/core.h"
#include "tester_utils.h"

static void
authentication_requested(LinphoneCore *lc, LinphoneAuthInfo *auth_info, BCTBX_UNUSED(LinphoneAuthMethod method)) {
	linphone_auth_info_set_passwd(auth_info, test_password);
	linphone_core_add_auth_info(lc, auth_info); /*add authentication info to LinphoneCore*/
	const char *algo = linphone_auth_info_get_algorithm(auth_info);
	BC_ASSERT_STRING_NOT_EQUAL(algo, ""); // Must have an algorithm
	bctbx_list_t *algos = linphone_auth_info_get_available_algorithms(auth_info);
	bool_t have_algo = FALSE;
	for (bctbx_list_t *elem = algos; !have_algo && elem != NULL; elem = algos->next)
		have_algo = (strcmp((char *)elem->data, algo) == 0);
	BC_ASSERT(have_algo); // Must have algorithm in list of available algorithms
	bctbx_list_free_with_data(algos, (bctbx_list_free_func)bctbx_free);
}

static LinphoneCoreManager *create_lcm_with_auth(unsigned int with_auth) {
	LinphoneCoreManager *lcm = linphone_core_manager_new("empty_rc");

	if (with_auth) {
		LinphoneCoreCbs *cbs = linphone_factory_create_core_cbs(linphone_factory_get());
		linphone_core_cbs_set_authentication_requested(cbs, authentication_requested);
		linphone_core_add_callbacks(lcm->lc, cbs);
		linphone_core_cbs_unref(cbs);
	}

	/*to allow testing with 127.0.0.1*/
	linphone_core_set_network_reachable(lcm->lc, TRUE);
	return lcm;
}

static LinphoneCoreManager *create_lcm(void) {
	return create_lcm_with_auth(0);
}

static void register_with_refresh_base_3_for_algo(LinphoneCore *lc,
                                                  bool_t refresh,
                                                  const char *domain,
                                                  const char *route,
                                                  bool_t late_auth_info,
                                                  LinphoneTransports *transport,
                                                  LinphoneRegistrationState expected_final_state,
                                                  const char *username) {
	LinphoneProxyConfig *proxy_cfg;
	stats *counters;
	LinphoneAddress *from;
	const char *server_addr;
	LinphoneAuthInfo *info;
	uint64_t time_begin;

	BC_ASSERT_PTR_NOT_NULL(lc);
	if (!lc) return;

	counters = get_stats(lc);
	reset_counters(counters);
	if (transport) linphone_core_set_transports(lc, transport);

	proxy_cfg = linphone_core_create_proxy_config(lc);

	from = create_linphone_address_for_algo(domain, username);

	linphone_proxy_config_set_identity_address(proxy_cfg, from);
	server_addr = linphone_address_get_domain(from);

	linphone_proxy_config_enable_register(proxy_cfg, TRUE);
	linphone_proxy_config_set_expires(proxy_cfg, 1);
	if (route) {
		linphone_proxy_config_set_route(proxy_cfg, route);
		linphone_proxy_config_set_server_addr(proxy_cfg, route);
	} else {
		linphone_proxy_config_set_server_addr(proxy_cfg, server_addr);
	}
	linphone_address_unref(from);

	linphone_core_add_proxy_config(lc, proxy_cfg);
	linphone_core_set_default_proxy_config(lc, proxy_cfg);

	time_begin = bctbx_get_cur_time_ms();
	while (counters->number_of_LinphoneRegistrationOk < 1 + (refresh != 0) &&
	       (bctbx_get_cur_time_ms() - time_begin) <
	           (7000 /*only wait 7 s if final state is progress*/ +
	            (expected_final_state == LinphoneRegistrationProgress ? 0 : 4000))) {
		linphone_core_iterate(lc);
		if (counters->number_of_auth_info_requested > 0 &&
		    linphone_proxy_config_get_state(proxy_cfg) == LinphoneRegistrationFailed && late_auth_info) {
			if (!linphone_core_get_auth_info_list(lc)) {
				BC_ASSERT_EQUAL(linphone_proxy_config_get_error(proxy_cfg), LinphoneReasonUnauthorized, int, "%d");
				info = linphone_auth_info_new(test_username, NULL, test_password, NULL, auth_domain,
				                              NULL);   /*create authentication structure from identity*/
				linphone_core_add_auth_info(lc, info); /*add authentication info to LinphoneCore*/
				linphone_auth_info_unref(info);
			}
		}
		if (linphone_proxy_config_get_error(proxy_cfg) == LinphoneReasonBadCredentials ||
		    (counters->number_of_auth_info_requested > 2 &&
		     linphone_proxy_config_get_error(proxy_cfg) ==
		         LinphoneReasonUnauthorized)) /*no need to continue if auth cannot be found*/
			break;                            /*no need to continue*/
		ms_usleep(20000);
	}

	BC_ASSERT_EQUAL(linphone_proxy_config_get_state(proxy_cfg) == LinphoneRegistrationOk,
	                expected_final_state == LinphoneRegistrationOk, int, "%d");
	BC_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationNone, 0, int, "%d");
	BC_ASSERT_TRUE(counters->number_of_LinphoneRegistrationProgress >= 1);
	if (expected_final_state == LinphoneRegistrationOk) {
		BC_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationOk, 1 + (refresh != 0), int, "%d");
		BC_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationFailed, late_auth_info ? 1 : 0, int, "%d");
	} else /*checking to be done outside this functions*/
		BC_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationCleared, 0, int, "%d");
	linphone_proxy_config_unref(proxy_cfg);
}

static void register_with_refresh_base_3(LinphoneCore *lc,
                                         bool_t refresh,
                                         const char *domain,
                                         const char *route,
                                         bool_t late_auth_info,
                                         LinphoneTransports *transport,
                                         LinphoneRegistrationState expected_final_state) {
	register_with_refresh_base_3_for_algo(lc, refresh, domain, route, late_auth_info, transport, expected_final_state,
	                                      NULL);
}

static void register_with_refresh_base_2(LinphoneCore *lc,
                                         bool_t refresh,
                                         const char *domain,
                                         const char *route,
                                         bool_t late_auth_info,
                                         LinphoneTransports *transport) {
	register_with_refresh_base_3(lc, refresh, domain, route, late_auth_info, transport, LinphoneRegistrationOk);
}

static void register_with_refresh_base_for_algo(
    LinphoneCore *lc, bool_t refresh, const char *domain, const char *route, const char *username) {
	LinphoneTransports *transport = linphone_factory_create_transports(linphone_factory_get());
	linphone_transports_set_udp_port(transport, LC_SIP_TRANSPORT_RANDOM);
	linphone_transports_set_tcp_port(transport, LC_SIP_TRANSPORT_RANDOM);
	linphone_transports_set_tls_port(transport, LC_SIP_TRANSPORT_RANDOM);
	linphone_transports_set_dtls_port(transport, 0);
	register_with_refresh_base_3_for_algo(lc, refresh, domain, route, FALSE, transport, LinphoneRegistrationOk,
	                                      username);
	linphone_transports_unref(transport);
}

static void register_with_refresh_base(LinphoneCore *lc, bool_t refresh, const char *domain, const char *route) {
	register_with_refresh_base_for_algo(lc, refresh, domain, route, NULL);
}

static void register_with_refresh_for_algo(
    LinphoneCoreManager *lcm, bool_t refresh, const char *domain, const char *route, const char *username) {
	stats *counters = &lcm->stat;
	register_with_refresh_base_for_algo(lcm->lc, refresh, domain, route, username);
	linphone_core_manager_stop(lcm);
	BC_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationCleared, 1, int, "%d");
}

static void register_with_route(LinphoneCoreManager *lcm, const char *domain, const char *route) {
	stats *lcm_counters = &lcm->stat;
	LinphoneTransports *transport = linphone_factory_create_transports(linphone_factory_get());
	linphone_transports_set_udp_port(transport, LC_SIP_TRANSPORT_RANDOM);
	linphone_transports_set_tcp_port(transport, LC_SIP_TRANSPORT_RANDOM);
	linphone_transports_set_tls_port(transport, LC_SIP_TRANSPORT_RANDOM);
	linphone_transports_set_dtls_port(transport, 0);

	LinphoneAddress *from = create_linphone_address_for_algo(domain, NULL);
	LinphoneAddress *routeAddress = linphone_address_new(route);

	if (transport) linphone_core_set_transports(lcm->lc, transport);
	LinphoneAccountParams *accountParams = linphone_core_create_account_params(lcm->lc);
	linphone_account_params_set_identity_address(accountParams, from);

	bctbx_list_t *routes = bctbx_list_new(routeAddress);
	linphone_account_params_set_routes_addresses(accountParams, routes);
	bctbx_list_free(routes);

	linphone_account_params_set_server_addr(accountParams, route);
	linphone_account_params_set_register_enabled(accountParams, TRUE);

	LinphoneAccount *account = linphone_core_create_account(lcm->lc, accountParams);
	linphone_account_params_unref(accountParams);
	if (account) {
		if (linphone_core_add_account(lcm->lc, account) != -1) {
			linphone_core_set_default_account(lcm->lc, account);
		}
	}

	BC_ASSERT_TRUE(wait_for_until(lcm->lc, NULL, &lcm_counters->number_of_LinphoneRegistrationOk, 1, 200000));

	linphone_address_unref(from);
	linphone_address_unref(routeAddress);
	linphone_account_unref(account);
	linphone_transports_unref(transport);
	linphone_core_manager_stop(lcm);
	BC_ASSERT_EQUAL(lcm_counters->number_of_LinphoneRegistrationCleared, 1, int, "%d");
}

static void register_with_refresh(LinphoneCoreManager *lcm, bool_t refresh, const char *domain, const char *route) {
	register_with_refresh_for_algo(lcm, refresh, domain, route, NULL);
}

static void register_with_refresh_with_send_error(void) {
	int retry = 0;
	LinphoneCoreManager *lcm = create_lcm_with_auth(1);
	stats *counters = &lcm->stat;
	LinphoneAuthInfo *info = linphone_auth_info_new(test_username, NULL, test_password, NULL, auth_domain,
	                                                NULL); /*create authentication structure from identity*/
	char route[256];
	sprintf(route, "sip:%s", test_route);
	linphone_core_add_auth_info(lcm->lc, info); /*add authentication info to LinphoneCore*/
	linphone_auth_info_unref(info);
	register_with_refresh_base(lcm->lc, TRUE, auth_domain, route);
	/*simultate a network error*/
	sal_set_send_error(linphone_core_get_sal(lcm->lc), -1);
	while (counters->number_of_LinphoneRegistrationProgress < 2 && retry++ < 200) {
		linphone_core_iterate(lcm->lc);
		ms_usleep(10000);
	}
	BC_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationFailed, 0, int, "%d");
	BC_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationProgress, 2, int, "%d");

	BC_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationCleared, 0, int, "%d");

	linphone_core_manager_destroy(lcm);
}

static void simple_register(void) {
	LinphoneCoreManager *lcm = create_lcm();
	stats *counters = &lcm->stat;
	register_with_refresh(lcm, FALSE, NULL, NULL);
	BC_ASSERT_EQUAL(counters->number_of_auth_info_requested, 0, int, "%d");
	linphone_core_manager_destroy(lcm);
}

static void simple_register_with_custom_refresh_period(void) {
	LinphoneCoreManager *lcm = create_lcm();

	int min_refresh = 20; // %
	int max_refresh = 50; // %
	int expires = 10;     // s;
	linphone_core_set_refresh_window(lcm->lc, min_refresh, max_refresh);
	stats *counters = &lcm->stat;
	LinphoneAccountParams *account_params = linphone_core_create_account_params(lcm->lc);

	LinphoneAddress *from = create_linphone_address_for_algo(NULL, NULL);

	linphone_account_params_set_identity_address(account_params, from);
	const char *server_addr = linphone_address_get_domain(from);

	linphone_account_params_enable_register(account_params, TRUE);
	linphone_account_params_set_expires(account_params, expires);
	linphone_account_params_set_server_addr(account_params, server_addr);
	linphone_address_unref(from);

	LinphoneAccount *account = linphone_core_create_account(lcm->lc, account_params);
	linphone_core_add_account(lcm->lc, account);
	linphone_core_set_default_account(lcm->lc, account);

	linphone_account_unref(account);
	linphone_account_params_unref(account_params);

	BC_ASSERT_TRUE(wait_for_until(lcm->lc, NULL, &counters->number_of_LinphoneRegistrationOk, 1, 5000));
	// The timeout of the timer is the upper bound of the refresh window in ms
	BC_ASSERT_TRUE(wait_for_until(lcm->lc, NULL, &counters->number_of_LinphoneRegistrationOk, 2,
	                              (1000 * max_refresh * expires) / 100));

	BC_ASSERT_EQUAL(counters->number_of_auth_info_requested, 0, int, "%d");

	linphone_core_stop(lcm->lc);
	BC_ASSERT_TRUE(wait_for_until(lcm->lc, NULL, &counters->number_of_LinphoneRegistrationCleared, 1, 5000));
	linphone_core_start(lcm->lc);
	int actual_min_refresh;
	int actual_max_refresh;
	LinphoneConfig *lcfg = linphone_core_get_config(lcm->lc);
	BC_ASSERT_TRUE(
	    linphone_config_get_range(lcfg, "sip", "refresh_window", &actual_min_refresh, &actual_max_refresh, 90, 90));
	BC_ASSERT_EQUAL(actual_min_refresh, min_refresh, int, "%d");
	BC_ASSERT_EQUAL(actual_max_refresh, max_refresh, int, "%d");

	linphone_core_manager_destroy(lcm);
}

static void register_with_custom_headers(void) {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneProxyConfig *cfg = linphone_core_get_default_proxy_config(marie->lc);
	int initial_register_ok = marie->stat.number_of_LinphoneRegistrationOk;
	const char *value;

	linphone_core_set_network_reachable(marie->lc, FALSE);
	linphone_proxy_config_set_custom_header(cfg, "ah-bah-ouais", "...mais bon.");
	/*unfortunately it is difficult to programmatically check that sent custom headers are actually sent.
	 * A server development would be required here.*/

	linphone_core_set_network_reachable(marie->lc, TRUE);
	wait_for(marie->lc, NULL, &marie->stat.number_of_LinphoneRegistrationOk, initial_register_ok + 1);
	value = linphone_proxy_config_get_custom_header(cfg, "Server");
	BC_ASSERT_PTR_NOT_NULL(value);
	if (value) BC_ASSERT_PTR_NOT_NULL(strstr(value, "Flexisip"));

	linphone_core_clear_accounts(marie->lc);
	wait_for(marie->lc, NULL, &marie->stat.number_of_LinphoneRegistrationCleared, 1);
	linphone_core_manager_destroy(marie);
}

static void simple_unregister(void) {
	LinphoneCoreManager *lcm = create_lcm();
	stats *counters = &lcm->stat;
	LinphoneProxyConfig *proxy_config;
	register_with_refresh_base(lcm->lc, FALSE, NULL, NULL);

	proxy_config = linphone_core_get_default_proxy_config(lcm->lc);

	linphone_proxy_config_edit(proxy_config);
	reset_counters(counters); /*clear stats*/

	/*nothing is supposed to arrive until done*/
	BC_ASSERT_FALSE(wait_for_until(lcm->lc, lcm->lc, &counters->number_of_LinphoneRegistrationCleared, 1, 3000));
	linphone_proxy_config_enable_register(proxy_config, FALSE);
	linphone_proxy_config_done(proxy_config);
	BC_ASSERT_TRUE(wait_for(lcm->lc, lcm->lc, &counters->number_of_LinphoneRegistrationCleared, 1));
	linphone_core_manager_destroy(lcm);
}

static void change_expires(void) {
	LinphoneCoreManager *lcm = create_lcm();
	stats *counters = &lcm->stat;
	LinphoneProxyConfig *proxy_config;
	register_with_refresh_base(lcm->lc, FALSE, NULL, NULL);

	proxy_config = linphone_core_get_default_proxy_config(lcm->lc);

	linphone_proxy_config_edit(proxy_config);

	/*nothing is supposed to arrive until done*/
	BC_ASSERT_FALSE(wait_for_until(lcm->lc, lcm->lc, &counters->number_of_LinphoneRegistrationCleared, 1, 3000));

	linphone_proxy_config_set_expires(proxy_config, 3);
	reset_counters(counters); /*clear stats*/
	linphone_proxy_config_done(proxy_config);
	BC_ASSERT_TRUE(wait_for(lcm->lc, lcm->lc, &counters->number_of_LinphoneRegistrationOk, 1));
	/*wait 2s without receive refresh*/
	BC_ASSERT_FALSE(wait_for_until(lcm->lc, lcm->lc, &counters->number_of_LinphoneRegistrationOk, 2, 2000));
	/* now, it should be ok*/
	BC_ASSERT_TRUE(wait_for(lcm->lc, lcm->lc, &counters->number_of_LinphoneRegistrationOk, 2));
	linphone_core_manager_destroy(lcm);
}

/*take care of min expires configuration from server*/
static void simple_register_with_refresh(void) {
	LinphoneCoreManager *lcm = create_lcm();
	stats *counters = &lcm->stat;
	register_with_refresh(lcm, TRUE, NULL, NULL);
	BC_ASSERT_EQUAL(counters->number_of_auth_info_requested, 0, int, "%d");
	linphone_core_manager_destroy(lcm);
}

static void simple_auth_register_with_refresh(void) {
	LinphoneCoreManager *lcm = create_lcm_with_auth(1);
	stats *counters = &lcm->stat;
	char route[256];
	sprintf(route, "sip:%s", test_route);
	register_with_refresh(lcm, TRUE, auth_domain, route);
	BC_ASSERT_EQUAL(counters->number_of_auth_info_requested, 1, int, "%d");
	linphone_core_manager_destroy(lcm);
}

static void simple_tcp_register(void) {
	char route[256];
	LinphoneCoreManager *lcm;
	sprintf(route, "sip:%s;transport=tcp", test_route);
	lcm = create_lcm();
	register_with_refresh(lcm, FALSE, test_domain, route);
	linphone_core_manager_destroy(lcm);
}

static void simple_udp_register(void) {
	char route[256];
	LinphoneCoreManager *lcm;
	sprintf(route, "sip:%s;transport=udp", "sip.buggy.example.org");
	lcm = create_lcm();
	register_with_route(lcm, "sipopen.example.org", route);
	linphone_core_manager_destroy(lcm);
}

static void simple_tcp_register2(void) {
	char route[256];
	LinphoneCoreManager *lcm;
	sprintf(route, "sip:%s;transport=tcp", "sip.buggy.example.org");
	lcm = create_lcm();
	register_with_route(lcm, "sipopen.example.org", route);
	linphone_core_manager_destroy(lcm);
}

static void simple_tcp_register_compatibility_mode(void) {
	char route[256];
	LinphoneCoreManager *lcm;
	LinphoneTransports *transport = NULL;
	sprintf(route, "sip:%s", test_route);
	lcm = create_lcm();
	transport = linphone_factory_create_transports(linphone_factory_get());
	linphone_transports_set_tcp_port(transport, LC_SIP_TRANSPORT_RANDOM);
	register_with_refresh_base_2(lcm->lc, FALSE, test_domain, route, FALSE, transport);
	linphone_transports_unref(transport);
	linphone_core_manager_destroy(lcm);
}

static void simple_tls_register(void) {
	if (transport_supported(LinphoneTransportTls)) {
		char route[256];
		LinphoneCoreManager *lcm = create_lcm();
		sprintf(route, "sip:%s;transport=tls", test_route);
		register_with_refresh(lcm, FALSE, test_domain, route);
		linphone_core_manager_destroy(lcm);
	}
}

static void simple_authenticated_register(void) {
	stats *counters;
	LinphoneCoreManager *lcm = create_lcm();
	LinphoneAuthInfo *info =
	    linphone_auth_info_new_for_algorithm(test_username, NULL, test_password, NULL, auth_domain, NULL,
	                                         NULL); /*create authentication structure from identity*/
	char route[256];
	sprintf(route, "sip:%s", test_route);
	linphone_core_add_auth_info(lcm->lc, info); /*add authentication info to LinphoneCore*/
	linphone_auth_info_unref(info);
	counters = &lcm->stat;
	register_with_refresh(lcm, FALSE, auth_domain, route);
	BC_ASSERT_EQUAL(counters->number_of_auth_info_requested, 0, int, "%d");
	linphone_core_manager_destroy(lcm);
}

static void simple_authenticated_register_for_algorithm(void) {
	stats *counters;
	LinphoneCoreManager *lcm = create_lcm();
	LinphoneAuthInfo *info =
	    linphone_auth_info_new_for_algorithm(test_sha_username, NULL, test_password, NULL, auth_domain, NULL,
	                                         "SHA-256"); /*create authentication structure from identity*/
	char route[256];
	sprintf(route, "sip:%s", test_route);
	linphone_core_add_auth_info(lcm->lc, info); /*add authentication info to LinphoneCore*/
	linphone_auth_info_unref(info);
	counters = &lcm->stat;
	register_with_refresh_for_algo(lcm, FALSE, auth_domain, route, test_sha_username);
	/* Because Flexisip asks a MD5 and SHA256 challenge, and the AuthInfo is only for SHA256, we will get one
	 * auth_info_requested per REGISTER message.*/
	BC_ASSERT_EQUAL(counters->number_of_auth_info_requested, 0, int, "%d");
	linphone_core_manager_destroy(lcm);
}

static void simple_authenticated_register_with_SHA256_from_clear_text_password(void) {
	/*
	 * In this test, the user has only a SHA256 hashed password on the server.
	 * We want to check that it can register by providing the corresponding clear text password, without the knowledge
	 * of which algorithm is used for his account.
	 */
	stats *counters;
	LinphoneCoreManager *lcm = create_lcm();
	LinphoneAuthInfo *info =
	    linphone_auth_info_new_for_algorithm(pure_sha256_user, NULL, test_password, NULL, NULL, auth_domain, NULL);
	LinphoneConfig *lcfg;
	const LinphoneAuthInfo *new_info;
	char route[256];
	const char *ha1;
	const char *cfg_ha1;

	sprintf(route, "sip:%s", test_route);
	linphone_core_add_auth_info(lcm->lc, info); /*add authentication info to LinphoneCore*/
	linphone_auth_info_unref(info);
	counters = &lcm->stat;
	register_with_refresh_base_for_algo(lcm->lc, FALSE, auth_domain, route, pure_sha256_user);
	BC_ASSERT_EQUAL(counters->number_of_auth_info_requested, 0, int, "%d");

	/* Assert that the ha1 was correctly computed, and stored in configuration. */
	new_info = linphone_core_find_auth_info(lcm->lc, NULL, pure_sha256_user, NULL);
	BC_ASSERT_PTR_NULL(linphone_auth_info_get_password(new_info));
	ha1 = linphone_auth_info_get_ha1(new_info);
	BC_ASSERT_PTR_NOT_NULL(ha1);
	lcfg = linphone_core_get_config(lcm->lc);
	cfg_ha1 = linphone_config_get_string(lcfg, "auth_info_0", "ha1", "");
	BC_ASSERT_STRING_EQUAL(cfg_ha1, ha1);
	BC_ASSERT_STRING_EQUAL(linphone_auth_info_get_realm(new_info), auth_domain);
	BC_ASSERT_STRING_EQUAL(linphone_config_get_string(lcfg, "auth_info_0", "algorithm", ""), "SHA-256");
	BC_ASSERT_PTR_NULL(linphone_config_get_string(lcfg, "auth_info_0", "passwd", NULL));
	linphone_core_manager_destroy(lcm);
}

static void ha1_authenticated_register(void) {
	stats *counters;
	LinphoneCoreManager *lcm = create_lcm();
	char ha1[33];
	LinphoneAuthInfo *info;
	char route[256];
	sal_auth_compute_ha1(test_username, auth_domain, test_password, ha1);
	info = linphone_auth_info_new_for_algorithm(test_username, NULL, NULL, ha1, auth_domain, NULL,
	                                            NULL); /*create authentication structure from identity*/
	sprintf(route, "sip:%s", test_route);
	linphone_core_add_auth_info(lcm->lc, info); /*add authentication info to LinphoneCore*/
	linphone_auth_info_unref(info);
	counters = &lcm->stat;
	register_with_refresh(lcm, FALSE, auth_domain, route);
	BC_ASSERT_EQUAL(counters->number_of_auth_info_requested, 0, int, "%d");
	linphone_core_manager_destroy(lcm);
}

static void md5_digest_rejected(void) {
	stats *counters;
	LinphoneCoreManager *lcm = create_lcm();
	char ha1[33];
	LinphoneAuthInfo *info;
	char route[256];
	const LinphoneDigestAuthenticationPolicy *default_policy;
	LinphoneDigestAuthenticationPolicy *policy =
	    linphone_factory_create_digest_authentication_policy(linphone_factory_get());

	/* The goal of this test is to make sure that if MD5 is not allowed per configuration, then authentication and
	 * registration will not take place. To ensure that the client will not use SHA-256, only an MD5 password is
	 * provided.
	 */
	sal_auth_compute_ha1(test_username, auth_domain, test_password, ha1);
	info = linphone_auth_info_new_for_algorithm(test_username, NULL, NULL, ha1, auth_domain, NULL,
	                                            NULL); /*create authentication structure from identity*/
	sprintf(route, "sip:%s", test_route);
	linphone_core_add_auth_info(lcm->lc, info); /*add authentication info to LinphoneCore*/
	linphone_auth_info_unref(info);
	counters = &lcm->stat;

	default_policy = linphone_core_get_digest_authentication_policy(lcm->lc);
	if (BC_ASSERT_PTR_NOT_NULL(default_policy)) {
		BC_ASSERT_TRUE(linphone_digest_authentication_policy_get_allow_md5(default_policy));
		BC_ASSERT_TRUE(linphone_digest_authentication_policy_get_allow_no_qop(default_policy));
	}
	linphone_digest_authentication_policy_set_allow_md5(policy, FALSE);
	linphone_core_set_digest_authentication_policy(lcm->lc, policy);
	linphone_digest_authentication_policy_unref(policy);
	register_with_refresh_base_3(lcm->lc, FALSE, auth_domain, route, FALSE, NULL, LinphoneRegistrationFailed);
	BC_ASSERT_EQUAL(counters->number_of_auth_info_requested, 0, int, "%d");
	linphone_core_manager_destroy(lcm);
}

static void ha1_authenticated_register_for_algorithm(void) {
	stats *counters;
	LinphoneCoreManager *lcm = create_lcm();
	char ha1[65];
	LinphoneAuthInfo *info;
	char route[256];
	sal_auth_compute_ha1_for_algorithm(test_sha_username, auth_domain, test_password, ha1, 65, "SHA-256");
	info = linphone_auth_info_new_for_algorithm(test_sha_username, NULL, NULL, ha1, auth_domain, NULL,
	                                            "SHA-256"); /*create authentication structure from identity*/
	sprintf(route, "sip:%s", test_route);
	linphone_core_add_auth_info(lcm->lc, info); /*add authentication info to LinphoneCore*/
	linphone_auth_info_unref(info);
	counters = &lcm->stat;
	register_with_refresh_for_algo(lcm, FALSE, auth_domain, route, test_sha_username);
	BC_ASSERT_EQUAL(counters->number_of_auth_info_requested, 0, int, "%d");
	linphone_core_manager_destroy(lcm);
}

static void authenticated_register_with_no_initial_credentials(void) {
	LinphoneCoreManager *lcm;
	LinphoneCoreCbs *cbs = linphone_factory_create_core_cbs(linphone_factory_get());
	stats *counters;
	char route[256];

	sprintf(route, "sip:%s", test_route);

	lcm = linphone_core_manager_new("empty_rc");

	linphone_core_cbs_set_authentication_requested(cbs, authentication_requested);
	linphone_core_add_callbacks(lcm->lc, cbs);
	linphone_core_cbs_unref(cbs);

	counters = get_stats(lcm->lc);
	counters->number_of_auth_info_requested = 0;
	register_with_refresh(lcm, FALSE, auth_domain, route);
	BC_ASSERT_EQUAL(counters->number_of_auth_info_requested, 1, int, "%d");
	linphone_core_manager_destroy(lcm);
}

static void authenticated_register_with_late_credentials(void) {
	LinphoneCoreManager *lcm;
	stats *counters;
	char route[256];
	LinphoneTransports *transport = NULL;

	sprintf(route, "sip:%s", test_route);

	lcm = linphone_core_manager_new("empty_rc");
	transport = linphone_factory_create_transports(linphone_factory_get());
	linphone_transports_set_udp_port(transport, LC_SIP_TRANSPORT_RANDOM);
	linphone_transports_set_tcp_port(transport, LC_SIP_TRANSPORT_RANDOM);
	linphone_transports_set_dtls_port(transport, LC_SIP_TRANSPORT_RANDOM);

	counters = get_stats(lcm->lc);
	register_with_refresh_base_2(lcm->lc, FALSE, auth_domain, route, TRUE, transport);
	linphone_transports_unref(transport);
	BC_ASSERT_EQUAL(counters->number_of_auth_info_requested, 1, int, "%d");
	linphone_core_manager_destroy(lcm);
}

static void authenticated_register_with_provided_credentials(void) {
	LinphoneCoreManager *lcm;
	stats *counters;
	LinphoneProxyConfig *cfg;
	char route[256];
	LinphoneAddress *from;
	LinphoneAuthInfo *ai;

	sprintf(route, "sip:%s", test_route);

	lcm = linphone_core_manager_new("empty_rc");

	counters = get_stats(lcm->lc);
	cfg = linphone_core_create_proxy_config(lcm->lc);
	from = create_linphone_address(auth_domain);

	linphone_proxy_config_set_identity_address(cfg, from);

	linphone_proxy_config_enable_register(cfg, TRUE);
	linphone_proxy_config_set_expires(cfg, 1);
	linphone_proxy_config_set_route(cfg, test_route);
	linphone_proxy_config_set_server_addr(cfg, test_route);
	linphone_address_unref(from);

	ai = linphone_auth_info_new(test_username, NULL, test_password, NULL, NULL, NULL);
	linphone_core_add_auth_info(lcm->lc, ai);
	linphone_auth_info_unref(ai);
	linphone_core_add_proxy_config(lcm->lc, cfg);

	BC_ASSERT_TRUE(wait_for(lcm->lc, lcm->lc, &counters->number_of_LinphoneRegistrationOk, 1));
	BC_ASSERT_EQUAL(counters->number_of_auth_info_requested, 0, int, "%d");

	BC_ASSERT_PTR_NULL(linphone_config_get_string(linphone_core_get_config(lcm->lc), "auth_info_0", "passwd", NULL));
	BC_ASSERT_PTR_NOT_NULL(linphone_config_get_string(linphone_core_get_config(lcm->lc), "auth_info_0", "ha1", NULL));

	linphone_proxy_config_unref(cfg);
	linphone_core_manager_destroy(lcm);
}

static void authenticated_register_with_provided_credentials_and_username_with_space(void) {
	LinphoneCoreManager *lcm = linphone_core_manager_new("empty_rc");
	stats *counters = get_stats(lcm->lc);
	LinphoneProxyConfig *cfg = linphone_core_create_proxy_config(lcm->lc);
	const char *username = "test username";
	LinphoneAddress *from = create_linphone_address_for_algo(auth_domain, username);

	linphone_proxy_config_set_identity_address(cfg, from);
	linphone_proxy_config_enable_register(cfg, TRUE);
	linphone_proxy_config_set_expires(cfg, 1);
	linphone_proxy_config_set_route(cfg, test_route);
	linphone_proxy_config_set_server_addr(cfg, test_route);
	linphone_address_unref(from);

	LinphoneAuthInfo *ai = linphone_auth_info_new(username, NULL, test_password, NULL, NULL, test_route);
	linphone_core_add_auth_info(lcm->lc, ai);
	linphone_auth_info_unref(ai);
	linphone_core_add_proxy_config(lcm->lc, cfg);

	BC_ASSERT_TRUE(wait_for(lcm->lc, lcm->lc, &counters->number_of_LinphoneRegistrationOk, 1));
	BC_ASSERT_EQUAL(counters->number_of_auth_info_requested, 0, int, "%d");

	BC_ASSERT_PTR_NULL(linphone_config_get_string(linphone_core_get_config(lcm->lc), "auth_info_0", "passwd", NULL));
	BC_ASSERT_PTR_NOT_NULL(linphone_config_get_string(linphone_core_get_config(lcm->lc), "auth_info_0", "ha1", NULL));

	linphone_proxy_config_unref(cfg);
	linphone_core_manager_destroy(lcm);
}

static void authenticated_register_with_wrong_late_credentials(void) {
	LinphoneCoreManager *lcm;
	stats *counters;
	LinphoneTransports *transport = NULL;
	char route[256];
	const char *saved_test_passwd = test_password;
	char *wrong_passwd = "mot de pass tout pourri";

	test_password = wrong_passwd;

	sprintf(route, "sip:%s", test_route);

	lcm = linphone_core_manager_new("empty_rc");
	transport = linphone_factory_create_transports(linphone_factory_get());
	linphone_transports_set_udp_port(transport, LC_SIP_TRANSPORT_RANDOM);
	linphone_transports_set_tcp_port(transport, LC_SIP_TRANSPORT_RANDOM);
	linphone_transports_set_tls_port(transport, LC_SIP_TRANSPORT_RANDOM);
	linphone_transports_set_dtls_port(transport, 0);

	counters = get_stats(lcm->lc);
	register_with_refresh_base_3(lcm->lc, FALSE, auth_domain, route, TRUE, transport, LinphoneRegistrationFailed);
	linphone_transports_unref(transport);
	BC_ASSERT_TRUE(wait_for(lcm->lc, NULL, &counters->number_of_auth_info_requested, 2));
	BC_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationFailed, 2, int, "%d");
	BC_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationProgress, 2, int, "%d");
	test_password = saved_test_passwd;

	linphone_core_manager_destroy(lcm);
}

static void authenticated_register_with_wrong_credentials_with_params_base(const char *user_agent,
                                                                           LinphoneCoreManager *lcm) {
	stats *counters;
	LinphoneTransports *transport = linphone_factory_create_transports(linphone_factory_get());
	LinphoneAuthInfo *info = linphone_auth_info_new(test_username, NULL, "wrong passwd", NULL, auth_domain,
	                                                NULL); /*create authentication structure from identity*/
	char route[256];

	sprintf(route, "sip:%s", test_route);
	linphone_transports_set_udp_port(transport, LC_SIP_TRANSPORT_RANDOM);
	linphone_transports_set_tcp_port(transport, LC_SIP_TRANSPORT_RANDOM);
	linphone_transports_set_tls_port(transport, LC_SIP_TRANSPORT_RANDOM);
	linphone_transports_set_dtls_port(transport, 0);

	sal_set_refresher_retry_after(linphone_core_get_sal(lcm->lc), 500);
	if (user_agent) {
		linphone_core_set_user_agent(lcm->lc, user_agent, NULL);
	}
	linphone_core_add_auth_info(lcm->lc, info); /*add wrong authentication info to LinphoneCore*/
	linphone_auth_info_unref(info);
	counters = get_stats(lcm->lc);
	register_with_refresh_base_3(lcm->lc, TRUE, auth_domain, route, FALSE, transport, LinphoneRegistrationFailed);
	linphone_transports_unref(transport);
	// BC_ASSERT_EQUAL(counters->number_of_auth_info_requested,3, int, "%d"); register_with_refresh_base_3 does not alow
	// to precisely check number of number_of_auth_info_requested
	/*wait for retry*/
	BC_ASSERT_TRUE(wait_for(lcm->lc, lcm->lc, &counters->number_of_auth_info_requested, 4));
	BC_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationFailed, 1, int, "%d");

	/*check the detailed error info */
	if (!user_agent || strcmp(user_agent, "tester-no-403") != 0) {
		LinphoneProxyConfig *cfg = NULL;
		cfg = linphone_core_get_default_proxy_config(lcm->lc);
		BC_ASSERT_PTR_NOT_NULL(cfg);
		if (cfg) {
			const LinphoneErrorInfo *ei = linphone_proxy_config_get_error_info(cfg);
			const char *phrase = linphone_error_info_get_phrase(ei);
			BC_ASSERT_PTR_NOT_NULL(phrase);
			if (phrase) BC_ASSERT_STRING_EQUAL(phrase, "Forbidden");
			BC_ASSERT_EQUAL(linphone_error_info_get_protocol_code(ei), 403, int, "%d");
			BC_ASSERT_PTR_NULL(linphone_error_info_get_warnings(ei));
		}
	}
}
static void authenticated_register_with_wrong_credentials_with_params(const char *user_agent) {
	LinphoneCoreManager *lcm = linphone_core_manager_new("empty_rc");
	authenticated_register_with_wrong_credentials_with_params_base(user_agent, lcm);
	linphone_core_manager_destroy(lcm);
}
static void authenticated_register_with_wrong_credentials(void) {
	authenticated_register_with_wrong_credentials_with_params(NULL);
}
static void authenticated_register_with_wrong_credentials_2(void) {
	LinphoneCoreManager *lcm = linphone_core_manager_new("empty_rc");
	stats *counters = get_stats(lcm->lc);
	int current_in_progress;
	LinphoneProxyConfig *proxy;

	authenticated_register_with_wrong_credentials_with_params_base(NULL, lcm);

	proxy = linphone_core_get_default_proxy_config(lcm->lc);
	/*Make sure registration attempts are stopped*/
	linphone_proxy_config_edit(proxy);
	linphone_proxy_config_enable_register(proxy, FALSE);
	linphone_proxy_config_done(proxy);
	current_in_progress = counters->number_of_LinphoneRegistrationProgress;
	BC_ASSERT_FALSE(
	    wait_for(lcm->lc, lcm->lc, &counters->number_of_LinphoneRegistrationProgress, current_in_progress + 1));

	linphone_core_manager_destroy(lcm);
}
static void authenticated_register_with_wrong_credentials_without_403(void) {
	authenticated_register_with_wrong_credentials_with_params("tester-no-403");
}
static LinphoneCoreManager *configure_lcm(void) {
	if (transport_supported(LinphoneTransportTls)) {
		LinphoneCoreManager *lcm = linphone_core_manager_new_with_proxies_check("multi_account_rc", FALSE);
		stats *counters = &lcm->stat;
		BC_ASSERT_TRUE(wait_for(lcm->lc, lcm->lc, &counters->number_of_LinphoneRegistrationOk,
		                        (int)bctbx_list_size(linphone_core_get_proxy_config_list(lcm->lc))));
		BC_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationFailed, 0, int, "%d");
		return lcm;
	}
	return NULL;
}

static void multiple_proxy(void) {
	LinphoneCoreManager *lcm = configure_lcm();
	if (lcm) {
		linphone_core_manager_destroy(lcm);
	}
}

static void network_state_change(void) {
	int register_ok;
	stats *counters;
	LinphoneCoreManager *lcm = configure_lcm();
	if (lcm) {
		LinphoneCore *lc = lcm->lc;

		counters = get_stats(lc);
		register_ok = counters->number_of_LinphoneRegistrationOk;
		linphone_core_set_network_reachable(lc, FALSE);
		BC_ASSERT_TRUE(wait_for(lc, lc, &counters->number_of_NetworkReachableFalse, 1));
		BC_ASSERT_TRUE(wait_for(lc, lc, &counters->number_of_LinphoneRegistrationNone, register_ok));
		BC_ASSERT_FALSE(wait_for_until(lc, lc, &counters->number_of_LinphoneRegistrationProgress, register_ok + 1,
		                               1000)); /*make sure no register is tried*/
		linphone_core_set_network_reachable(lc, TRUE);
		BC_ASSERT_TRUE(wait_for(lc, lc, &counters->number_of_NetworkReachableTrue, 1));
		wait_for(lc, lc, &counters->number_of_LinphoneRegistrationOk, 2 * register_ok);

		linphone_core_manager_destroy(lcm);
	}
}
static int get_number_of_udp_proxy(const LinphoneCore *lc) {
	int number_of_udp_proxy = 0;
	LinphoneProxyConfig *proxy_cfg;
	const bctbx_list_t *proxys;
	for (proxys = linphone_core_get_proxy_config_list(lc); proxys != NULL; proxys = proxys->next) {
		proxy_cfg = (LinphoneProxyConfig *)proxys->data;
		if (strcmp("udp", linphone_proxy_config_get_transport(proxy_cfg)) == 0) number_of_udp_proxy++;
	}
	return number_of_udp_proxy;
}
static void transport_change(void) {
	LinphoneCoreManager *lcm;
	LinphoneCore *lc;
	int register_ok;
	stats *counters;
	LinphoneTransports *sip_tr;
	LinphoneTransports *sip_tr_orig;
	int number_of_udp_proxy = 0;
	int total_number_of_proxies;

	lcm = configure_lcm();
	if (lcm) {
		lc = lcm->lc;
		sip_tr = linphone_factory_create_transports(linphone_factory_get());
		counters = get_stats(lc);
		register_ok = counters->number_of_LinphoneRegistrationOk;

		number_of_udp_proxy = get_number_of_udp_proxy(lc);
		total_number_of_proxies = (int)bctbx_list_size(linphone_core_get_proxy_config_list(lc));
		sip_tr_orig = linphone_core_get_transports(lc);

		linphone_transports_set_udp_port(sip_tr, linphone_transports_get_udp_port(sip_tr_orig));

		/*keep only udp*/
		linphone_core_set_transports(lc, sip_tr);
		BC_ASSERT_TRUE(
		    wait_for(lc, lc, &counters->number_of_LinphoneRegistrationOk, register_ok + number_of_udp_proxy));

		BC_ASSERT_TRUE(wait_for(lc, lc, &counters->number_of_LinphoneRegistrationFailed,
		                        total_number_of_proxies - number_of_udp_proxy));

		linphone_transports_unref(sip_tr);
		linphone_transports_unref(sip_tr_orig);
		linphone_core_manager_destroy(lcm);
	}
}

static void transport_dont_bind(void) {
	LinphoneCoreManager *pauline = linphone_core_manager_new_with_proxies_check("pauline_tcp_rc", FALSE);

	stats *counters = &pauline->stat;
	LinphoneTransports *tr = linphone_factory_create_transports(linphone_factory_get());
	linphone_transports_set_tcp_port(tr, LC_SIP_TRANSPORT_DONTBIND);
	linphone_transports_set_tls_port(tr, LC_SIP_TRANSPORT_DONTBIND);
	linphone_transports_set_udp_port(tr, LC_SIP_TRANSPORT_DISABLED);

	linphone_core_set_transports(pauline->lc, tr);

	BC_ASSERT_TRUE(wait_for(pauline->lc, pauline->lc, &counters->number_of_LinphoneRegistrationOk, 1));
	linphone_transports_unref(tr);

	tr = linphone_core_get_transports_used(pauline->lc);
	BC_ASSERT_EQUAL(linphone_transports_get_udp_port(tr), 0, int, "%i");
	BC_ASSERT_EQUAL(linphone_transports_get_tcp_port(tr), LC_SIP_TRANSPORT_DONTBIND, int, "%i");
	BC_ASSERT_EQUAL(linphone_transports_get_tls_port(tr), LC_SIP_TRANSPORT_DONTBIND, int, "%i");

	// udp
	linphone_transports_set_tcp_port(tr, LC_SIP_TRANSPORT_DISABLED);
	linphone_transports_set_tls_port(tr, LC_SIP_TRANSPORT_DISABLED);
	linphone_transports_set_udp_port(tr, LC_SIP_TRANSPORT_DONTBIND);
	linphone_core_set_transports(pauline->lc, tr);
	linphone_transports_unref(tr);

	LinphoneAccount *account = linphone_core_get_default_account(pauline->lc);
	LinphoneAccountParams *params = linphone_account_params_clone(linphone_account_get_params(account));
	linphone_account_params_set_transport(params, LinphoneTransportUdp);
	linphone_account_set_params(account, params);
	linphone_account_params_unref(params);

	BC_ASSERT_TRUE(wait_for(pauline->lc, pauline->lc, &counters->number_of_LinphoneRegistrationOk, 2));

	tr = linphone_core_get_transports_used(pauline->lc);
	BC_ASSERT_EQUAL(linphone_transports_get_udp_port(tr), LC_SIP_TRANSPORT_DONTBIND, int, "%i");
	BC_ASSERT_EQUAL(linphone_transports_get_tcp_port(tr), LC_SIP_TRANSPORT_DISABLED, int, "%i");
	BC_ASSERT_EQUAL(linphone_transports_get_tls_port(tr), LC_SIP_TRANSPORT_DISABLED, int, "%i");
	linphone_transports_unref(tr);

	linphone_core_manager_destroy(pauline);
}

#if 0
static void transport_busy(void){
	LinphoneCoreManager *pauline = linphone_core_manager_new("pauline_tcp_rc");
	LCSipTransports tr;

	memset(&tr, 0, sizeof(tr));
	tr.udp_port = LC_SIP_TRANSPORT_RANDOM;
	tr.tcp_port = LC_SIP_TRANSPORT_RANDOM;
	tr.tls_port = LC_SIP_TRANSPORT_RANDOM;

	linphone_core_set_sip_transports(pauline->lc, &tr);

	{
		LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
		linphone_core_set_sip_transports(marie->lc, &tr);
		memset(&tr, 0, sizeof(tr));
		linphone_core_get_sip_transports_used(pauline->lc, &tr);
		/*BC_ASSERT_EQUAL(tr.udp_port, 0, int, "%i");
		BC_ASSERT_EQUAL(tr.tcp_port, 0, int, "%i");
		BC_ASSERT_EQUAL(tr.tls_port, 0, int, "%i");*/
		linphone_core_manager_destroy(marie);
	}

	linphone_core_manager_destroy(pauline);
}
#endif

static void proxy_transport_change(void) {
	LinphoneCoreManager *lcm = create_lcm();
	stats *counters = &lcm->stat;
	LinphoneProxyConfig *proxy_config;
	LinphoneAddress *addr;
	char *addr_as_string;
	LinphoneAuthInfo *info = linphone_auth_info_new(test_username, NULL, test_password, NULL, auth_domain,
	                                                NULL); /*create authentication structure from identity*/
	linphone_core_add_auth_info(lcm->lc, info);            /*add authentication info to LinphoneCore*/
	linphone_auth_info_unref(info);
	register_with_refresh_base(lcm->lc, FALSE, auth_domain, NULL);

	proxy_config = linphone_core_get_default_proxy_config(lcm->lc);
	reset_counters(counters); /*clear stats*/
	linphone_proxy_config_edit(proxy_config);

	BC_ASSERT_FALSE(wait_for_until(lcm->lc, lcm->lc, &counters->number_of_LinphoneRegistrationCleared, 1, 3000));
	addr = linphone_address_new(linphone_proxy_config_get_addr(proxy_config));

	if (LinphoneTransportTcp == linphone_address_get_transport(addr)) {
		linphone_address_set_transport(addr, LinphoneTransportUdp);
	} else {
		linphone_address_set_transport(addr, LinphoneTransportTcp);
	}
	linphone_proxy_config_set_server_addr(proxy_config, addr_as_string = linphone_address_as_string(addr));

	linphone_proxy_config_done(proxy_config);

	BC_ASSERT(wait_for(lcm->lc, lcm->lc, &counters->number_of_LinphoneRegistrationOk, 1));
	/*as we change p[roxy server destination, we should'nt be notified about the clear*/
	BC_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationCleared, 0, int, "%d");
	ms_free(addr_as_string);
	linphone_address_unref(addr);
	linphone_core_manager_destroy(lcm);
}
/*
 * On ios, some firewal require to disable flow label (livebox with default firewall level).
 * sudo sysctl net.inet6.ip6.auto_flowlabel=0
 * It might be possible to found a sockopt for such purpose.
 */
static void proxy_transport_change_with_wrong_port(void) {
	LinphoneCoreManager *lcm = create_lcm();
	stats *counters = &lcm->stat;
	LinphoneProxyConfig *proxy_config;
	LinphoneAuthInfo *info = linphone_auth_info_new(test_username, NULL, test_password, NULL, auth_domain,
	                                                NULL); /*create authentication structure from identity*/
	char route[256];
	LinphoneTransports *transport = linphone_factory_create_transports(linphone_factory_get());
	sprintf(route, "sip:%s", test_route);
	linphone_transports_set_udp_port(transport, LC_SIP_TRANSPORT_RANDOM);
	linphone_transports_set_tcp_port(transport, LC_SIP_TRANSPORT_RANDOM);
	linphone_transports_set_tls_port(transport, LC_SIP_TRANSPORT_RANDOM);
	linphone_transports_set_dtls_port(transport, LC_SIP_TRANSPORT_RANDOM);

	linphone_core_add_auth_info(lcm->lc, info); /*add authentication info to LinphoneCore*/
	linphone_auth_info_unref(info);
	register_with_refresh_base_3(lcm->lc, FALSE, auth_domain, "sip2.linphone.org:5987", 0, transport,
	                             LinphoneRegistrationProgress);
	linphone_transports_unref(transport);

	proxy_config = linphone_core_get_default_proxy_config(lcm->lc);
	linphone_proxy_config_edit(proxy_config);

	BC_ASSERT_FALSE(wait_for_until(lcm->lc, lcm->lc, &counters->number_of_LinphoneRegistrationCleared, 1, 3000));
	linphone_proxy_config_set_server_addr(proxy_config, route);
	linphone_proxy_config_done(proxy_config);

	BC_ASSERT(wait_for(lcm->lc, lcm->lc, &counters->number_of_LinphoneRegistrationOk, 1));
	/*as we change proxy server destination, we should'nt be notified about the clear*/
	BC_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationCleared, 0, int, "%d");
	BC_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationOk, 1, int, "%d");
	BC_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationProgress,
	                1 + counters->number_of_LinphoneRegistrationFailed, int, "%d");

	linphone_core_manager_destroy(lcm);
}

static void proxy_transport_change_with_wrong_port_givin_up(void) {
	LinphoneCoreManager *lcm = create_lcm();
	stats *counters = &lcm->stat;
	LinphoneProxyConfig *proxy_config;
	LinphoneAuthInfo *info = linphone_auth_info_new(test_username, NULL, test_password, NULL, auth_domain,
	                                                NULL); /*create authentication structure from identity*/
	LinphoneTransports *transport = linphone_factory_create_transports(linphone_factory_get());
	linphone_transports_set_udp_port(transport, LC_SIP_TRANSPORT_RANDOM);
	linphone_transports_set_tcp_port(transport, LC_SIP_TRANSPORT_RANDOM);
	linphone_transports_set_tls_port(transport, LC_SIP_TRANSPORT_RANDOM);
	linphone_transports_set_dtls_port(transport, LC_SIP_TRANSPORT_RANDOM);

	linphone_core_add_auth_info(lcm->lc, info); /*add authentication info to LinphoneCore*/
	linphone_auth_info_unref(info);
	register_with_refresh_base_3(lcm->lc, FALSE, auth_domain, "sip2.linphone.org:5987;transport=tcp", 0, transport,
	                             LinphoneRegistrationProgress);
	linphone_transports_unref(transport);

	BC_ASSERT(wait_for_until(lcm->lc, lcm->lc, &counters->number_of_LinphoneRegistrationFailed, 1, 33000));

	proxy_config = linphone_core_get_default_proxy_config(lcm->lc);
	linphone_proxy_config_edit(proxy_config);
	linphone_proxy_config_enableregister(proxy_config, FALSE);
	linphone_proxy_config_done(proxy_config);

	BC_ASSERT(wait_for_until(lcm->lc, lcm->lc, &counters->number_of_LinphoneRegistrationNone, 1, 33000));
	BC_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationCleared, 0, int, "%d");
	BC_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationOk, 0, int, "%d");
	BC_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationProgress, 1, int, "%d");
	BC_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationFailed, 1, int, "%d");

	linphone_core_manager_destroy(lcm);
}

static void io_recv_error(void) {
	LinphoneCoreManager *lcm;
	LinphoneCore *lc;
	int register_ok;
	stats *counters;
	int number_of_udp_proxy = 0;

	lcm = configure_lcm();
	if (lcm) {
		lc = lcm->lc;
		counters = get_stats(lc);
		register_ok = counters->number_of_LinphoneRegistrationOk;
		number_of_udp_proxy = get_number_of_udp_proxy(lc);
		sal_set_recv_error(linphone_core_get_sal(lc), 0);

		BC_ASSERT_TRUE(wait_for(lc, lc, &counters->number_of_LinphoneRegistrationProgress,
		                        2 * (register_ok - number_of_udp_proxy) /*because 1 udp*/));
		BC_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationFailed, 0, int, "%d");

		sal_set_recv_error(linphone_core_get_sal(lc), 1); /*reset*/

		linphone_core_manager_destroy(lcm);
	}
}

static void io_recv_error_retry_immediatly(void) {
	LinphoneCoreManager *lcm;
	LinphoneCore *lc;
	int register_ok;
	stats *counters;
	int number_of_udp_proxy = 0;

	lcm = configure_lcm();
	if (lcm) {
		lc = lcm->lc;
		counters = get_stats(lc);
		register_ok = counters->number_of_LinphoneRegistrationOk;
		number_of_udp_proxy = get_number_of_udp_proxy(lc);
		sal_set_recv_error(linphone_core_get_sal(lc), 0);

		BC_ASSERT_TRUE(wait_for(lc, NULL, &counters->number_of_LinphoneRegistrationProgress,
		                        (register_ok - number_of_udp_proxy) + register_ok /*because 1 udp*/));
		BC_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationFailed, 0, int, "%d");
		sal_set_recv_error(linphone_core_get_sal(lc), 1); /*reset*/

		BC_ASSERT_TRUE(wait_for_until(lc, lc, &counters->number_of_LinphoneRegistrationOk,
		                              register_ok - number_of_udp_proxy + register_ok, 30000));

		linphone_core_manager_destroy(lcm);
	}
}

static void io_recv_error_late_recovery(void) {
	if (transport_supported(LinphoneTransportTls)) {
		LinphoneCoreManager *lcm;
		LinphoneCore *lc;
		int register_ok;
		stats *counters;
		int number_of_udp_proxy = 0;
		bctbx_list_t *lcs;
		lcm = linphone_core_manager_new_with_proxies_check("multi_account_rc",
		                                                   FALSE); /*to make sure iterates are not call yet*/
		lc = lcm->lc;
		sal_set_refresher_retry_after(linphone_core_get_sal(lc), 1000);
		counters = &lcm->stat;
		BC_ASSERT_TRUE(wait_for(lcm->lc, lcm->lc, &counters->number_of_LinphoneRegistrationOk,
		                        (int)bctbx_list_size(linphone_core_get_proxy_config_list(lcm->lc))));

		counters = get_stats(lc);
		register_ok = counters->number_of_LinphoneRegistrationOk;
		number_of_udp_proxy = get_number_of_udp_proxy(lc);
		/*simulate a general socket error*/
		sal_set_recv_error(linphone_core_get_sal(lc), 0);
		sal_set_send_error(linphone_core_get_sal(lc), -1);

		BC_ASSERT_TRUE(wait_for(lc, NULL, &counters->number_of_LinphoneRegistrationProgress,
		                        (register_ok - number_of_udp_proxy) + register_ok /*because 1 udp*/));
		BC_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationFailed, 0, int, "%d");

		lcs = bctbx_list_append(NULL, lc);

		BC_ASSERT_TRUE(wait_for_list(lcs, &counters->number_of_LinphoneRegistrationFailed,
		                             (register_ok - number_of_udp_proxy),
		                             sal_get_refresher_retry_after(linphone_core_get_sal(lc)) + 3000));

		sal_set_recv_error(linphone_core_get_sal(lc), 1); /*reset*/
		sal_set_send_error(linphone_core_get_sal(lc), 0);

		BC_ASSERT_TRUE(wait_for_list(lcs, &counters->number_of_LinphoneRegistrationOk,
		                             register_ok - number_of_udp_proxy + register_ok,
		                             sal_get_refresher_retry_after(linphone_core_get_sal(lc)) + 3000));

		bctbx_list_free(lcs);
		linphone_core_manager_destroy(lcm);
	}
}

static void io_recv_error_without_active_register(void) {
	LinphoneCoreManager *lcm;
	LinphoneCore *lc;
	int register_ok;
	stats *counters;
	bctbx_list_t *proxys;
	int dummy = 0;

	lcm = configure_lcm();
	if (lcm) {
		lc = lcm->lc;
		counters = get_stats(lc);

		register_ok = counters->number_of_LinphoneRegistrationOk;

		proxys = bctbx_list_copy(linphone_core_get_proxy_config_list(lc));
		for (bctbx_list_t *it = proxys; it != NULL; it = it->next) {
			LinphoneProxyConfig *proxy_cfg = (LinphoneProxyConfig *)it->data;
			linphone_proxy_config_edit(proxy_cfg);
			linphone_proxy_config_enableregister(proxy_cfg, FALSE);
			linphone_proxy_config_done(proxy_cfg);
		}
		bctbx_list_free(proxys);
		/*wait for unregistrations*/
		BC_ASSERT_TRUE(
		    wait_for(lc, lc, &counters->number_of_LinphoneRegistrationCleared, register_ok /*because 1 udp*/));

		sal_set_recv_error(linphone_core_get_sal(lc), 0);

		/*nothing should happen because no active registration*/
		wait_for_until(lc, lc, &dummy, 1, 3000);
		BC_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationProgress,
		                (int)bctbx_list_size(linphone_core_get_proxy_config_list(lc)), int, "%d");

		BC_ASSERT_EQUAL(counters->number_of_LinphoneRegistrationFailed, 0, int, "%d");

		sal_set_recv_error(linphone_core_get_sal(lc), 1); /*reset*/

		linphone_core_manager_destroy(lcm);
	}
}

static void tls_certificate_failure(void) {
	if (transport_supported(LinphoneTransportTls)) {
		LinphoneCoreManager *lcm;
		LinphoneCore *lc;
		char *rootcapath = bc_tester_res("certificates/cn/agent.pem"); /*bad root ca*/

		lcm = linphone_core_manager_new_with_proxies_check("pauline_rc", FALSE);
		lc = lcm->lc;
		linphone_core_set_root_ca(lcm->lc, rootcapath);
		linphone_core_set_network_reachable(lc, TRUE);
		BC_ASSERT_TRUE(wait_for(lcm->lc, lcm->lc, &lcm->stat.number_of_LinphoneRegistrationFailed, 1));
		linphone_core_set_root_ca(lcm->lc, NULL); /*no root ca*/
		linphone_core_refresh_registers(lcm->lc);
		BC_ASSERT_TRUE(wait_for(lc, lc, &lcm->stat.number_of_LinphoneRegistrationFailed, 2));
		bc_free(rootcapath);
		rootcapath = bc_tester_res("certificates/cn/cafile.pem"); /*good root ca*/
		linphone_core_set_root_ca(lcm->lc, rootcapath);
		linphone_core_refresh_registers(lcm->lc);
		BC_ASSERT_TRUE(wait_for(lc, lc, &lcm->stat.number_of_LinphoneRegistrationOk, 1));
		BC_ASSERT_EQUAL(lcm->stat.number_of_LinphoneRegistrationFailed, 2, int, "%d");
		linphone_core_manager_destroy(lcm);
		bc_free(rootcapath);
	}
}

static void tls_certificate_subject_check(void) {
	if (transport_supported(LinphoneTransportTls)) {
		LinphoneCoreManager *lcm;
		LinphoneCore *lc;
		char *rootcapath = bc_tester_res("certificates/cn/cafile.pem");
		lcm = linphone_core_manager_new_with_proxies_check("pauline_alt_rc", FALSE);
		lc = lcm->lc;
		linphone_core_set_root_ca(lc, rootcapath);
		/*let's search for a subject that is not in the certificate, it should fail*/
		linphone_config_set_string(linphone_core_get_config(lc), "sip", "tls_certificate_subject_regexp",
		                           "cotcotcot.org");
		linphone_core_set_network_reachable(lc, TRUE);
		BC_ASSERT_TRUE(wait_for(lcm->lc, lcm->lc, &lcm->stat.number_of_LinphoneRegistrationFailed, 1));

		/*let's search for a subject (in subjectAltNames and CN) that exist in the certificate, it should pass*/
		linphone_config_set_string(linphone_core_get_config(lc), "sip", "tls_certificate_subject_regexp",
		                           "altname.linphone.org");
		linphone_core_refresh_registers(lcm->lc);
		BC_ASSERT_TRUE(wait_for(lc, lc, &lcm->stat.number_of_LinphoneRegistrationOk, 1));
		linphone_core_set_network_reachable(lc, FALSE);

		/*let's search for a subject (in subjectAltNames and CN) that exist in the certificate, it should pass*/
		linphone_config_set_string(linphone_core_get_config(lc), "sip", "tls_certificate_subject_regexp",
		                           "Jehan Monnier");
		linphone_core_set_network_reachable(lc, TRUE);
		BC_ASSERT_TRUE(wait_for(lc, lc, &lcm->stat.number_of_LinphoneRegistrationOk, 2));

		BC_ASSERT_EQUAL(lcm->stat.number_of_LinphoneRegistrationFailed, 1, int, "%d");
		linphone_core_manager_destroy(lcm);
		bc_free(rootcapath);
	}
}

char *read_file(const char *path) {
	long numbytes = 0;
	size_t readbytes;
	char *buffer = NULL;
	FILE *infile = fopen(path, "rb");

	BC_ASSERT_PTR_NOT_NULL(infile);
	if (infile) {
		fseek(infile, 0L, SEEK_END);
		numbytes = ftell(infile);
		fseek(infile, 0L, SEEK_SET);
		buffer = (char *)ms_malloc((numbytes + 1) * sizeof(char));
		readbytes = fread(buffer, sizeof(char), numbytes, infile);
		fclose(infile);
		buffer[readbytes] = '\0';
	}
	return buffer;
}

static void tls_certificate_data(void) {
	if (transport_supported(LinphoneTransportTls)) {
		LinphoneCoreManager *lcm;
		LinphoneCore *lc;
		char *rootcapath = bc_tester_res("certificates/cn/agent.pem"); /*bad root ca*/
		char *data = read_file(rootcapath);

		lcm = linphone_core_manager_new_with_proxies_check("pauline_rc", FALSE);
		lc = lcm->lc;
		linphone_core_set_root_ca_data(lcm->lc, data);
		linphone_core_set_network_reachable(lc, TRUE);
		BC_ASSERT_TRUE(wait_for(lcm->lc, lcm->lc, &lcm->stat.number_of_LinphoneRegistrationFailed, 1));
		linphone_core_set_root_ca_data(lcm->lc, NULL); /*no root ca*/
		linphone_core_refresh_registers(lcm->lc);
		BC_ASSERT_TRUE(wait_for(lc, lc, &lcm->stat.number_of_LinphoneRegistrationFailed, 2));
		bc_free(rootcapath);
		ms_free(data);
		rootcapath = bc_tester_res("certificates/cn/cafile.pem"); /*good root ca*/
		data = read_file(rootcapath);
		linphone_core_set_root_ca_data(lcm->lc, data);
		linphone_core_refresh_registers(lcm->lc);
		BC_ASSERT_TRUE(wait_for(lc, lc, &lcm->stat.number_of_LinphoneRegistrationOk, 1));
		BC_ASSERT_EQUAL(lcm->stat.number_of_LinphoneRegistrationFailed, 2, int, "%d");
		linphone_core_manager_destroy(lcm);
		bc_free(rootcapath);
		ms_free(data);
	}
}

/*the purpose of this test is to check that will not block the proxy config during SSL handshake for entire life in case
 * of mistaken configuration*/
static void tls_with_non_tls_server(void) {
	if (transport_supported(LinphoneTransportTls)) {
		LinphoneCoreManager *lcm;
		LinphoneProxyConfig *proxy_cfg;
		LinphoneAddress *addr;
		char tmp[256];
		LinphoneCore *lc;

		lcm = linphone_core_manager_new_with_proxies_check("marie_rc", FALSE);
		lc = lcm->lc;
		sal_set_transport_timeout(linphone_core_get_sal(lc), 3000);
		proxy_cfg = linphone_core_get_default_proxy_config(lc);
		linphone_proxy_config_edit(proxy_cfg);
		addr = linphone_address_new(linphone_proxy_config_get_addr(proxy_cfg));
		snprintf(tmp, sizeof(tmp), "sip:%s:%i;transport=tls", linphone_address_get_domain(addr),
		         (linphone_address_get_port(addr) > 0 ? linphone_address_get_port(addr) : 5060));
		linphone_proxy_config_set_server_addr(proxy_cfg, tmp);
		linphone_proxy_config_done(proxy_cfg);
		linphone_address_unref(addr);
		BC_ASSERT_TRUE(wait_for_until(lc, lc, &lcm->stat.number_of_LinphoneRegistrationFailed, 1, 10000));
		linphone_core_manager_destroy(lcm);
	}
}

static void tls_alt_name_register(void) {
	if (transport_supported(LinphoneTransportTls)) {
		LinphoneCoreManager *lcm;
		LinphoneCore *lc;
		char *rootcapath = bc_tester_res("certificates/cn/cafile.pem");

		lcm = linphone_core_manager_new_with_proxies_check("pauline_alt_rc", FALSE);
		lc = lcm->lc;
		linphone_core_set_root_ca(lc, rootcapath);
		linphone_core_refresh_registers(lc);
		BC_ASSERT_TRUE(wait_for(lc, lc, &lcm->stat.number_of_LinphoneRegistrationOk, 1));
		BC_ASSERT_EQUAL(lcm->stat.number_of_LinphoneRegistrationFailed, 0, int, "%d");
		linphone_core_manager_destroy(lcm);
		bc_free(rootcapath);
	}
}

static void tls_wildcard_register(void) {
	if (transport_supported(LinphoneTransportTls)) {
		LinphoneCoreManager *lcm;
		LinphoneCore *lc;
		char *rootcapath = bc_tester_res("certificates/cn/cafile.pem");

		lcm = linphone_core_manager_new_with_proxies_check("pauline_wild_rc", FALSE);
		lc = lcm->lc;
		linphone_core_set_root_ca(lc, rootcapath);
		linphone_core_refresh_registers(lc);
		BC_ASSERT_TRUE(wait_for(lc, lc, &lcm->stat.number_of_LinphoneRegistrationOk, 2));
		BC_ASSERT_EQUAL(lcm->stat.number_of_LinphoneRegistrationFailed, 0, int, "%d");
		linphone_core_manager_destroy(lcm);
		bc_free(rootcapath);
	}
}

static void redirect(void) {
	char route[256];
	LinphoneCoreManager *lcm;
	LinphoneTransports *transport = NULL;
	sprintf(route, "sip:%s:5064", test_route);
	lcm = create_lcm();
	if (lcm) {
		transport = linphone_factory_create_transports(linphone_factory_get());
		linphone_transports_set_udp_port(transport, -1);
		linphone_core_set_user_agent(lcm->lc, "redirect", NULL);
		register_with_refresh_base_2(lcm->lc, FALSE, test_domain, route, FALSE, transport);
		linphone_transports_unref(transport);
		linphone_core_manager_destroy(lcm);
	}
}

static void tls_auth_global_client_cert(void) {
	if (transport_supported(LinphoneTransportTls)) {
		LinphoneCoreManager *manager = ms_new0(LinphoneCoreManager, 1);
		LpConfig *lpc = NULL;
		char *cert_path = bc_tester_res("certificates/client/cert.pem");
		char *key_path = bc_tester_res("certificates/client/key.pem");
		linphone_core_manager_init(manager, "pauline_tls_client_rc", NULL);
		lpc = linphone_core_get_config(manager->lc);
		linphone_config_set_string(lpc, "sip", "client_cert_chain", cert_path);
		linphone_config_set_string(lpc, "sip", "client_cert_key", key_path);
		linphone_core_manager_start(manager, TRUE);
		linphone_core_manager_destroy(manager);
		bc_free(cert_path);
		bc_free(key_path);
	}
}

static void tls_auth_global_client_cert_api(void) {
	if (transport_supported(LinphoneTransportTls)) {
		LinphoneCoreManager *pauline = linphone_core_manager_new_with_proxies_check("pauline_tls_client_rc", FALSE);
		char *cert_path = bc_tester_res("certificates/client/cert.pem");
		char *key_path = bc_tester_res("certificates/client/key.pem");
		char *cert = read_file(cert_path);
		char *key = read_file(key_path);
		LinphoneCore *lc = pauline->lc;
		linphone_core_set_tls_cert(lc, cert);
		linphone_core_set_tls_key(lc, key);
		BC_ASSERT_TRUE(wait_for(lc, lc, &pauline->stat.number_of_LinphoneRegistrationOk, 1));
		linphone_core_manager_destroy(pauline);
		ms_free(cert);
		ms_free(key);
		bc_free(cert_path);
		bc_free(key_path);
	}
}

static void tls_auth_global_client_cert_api_path(void) {
	if (transport_supported(LinphoneTransportTls)) {
		LinphoneCoreManager *pauline = linphone_core_manager_new_with_proxies_check("pauline_tls_client_rc", FALSE);
		char *cert = bc_tester_res("certificates/client/cert.pem");
		char *key = bc_tester_res("certificates/client/key.pem");
		LinphoneCore *lc = pauline->lc;
		linphone_core_set_tls_cert_path(lc, cert);
		linphone_core_set_tls_key_path(lc, key);
		BC_ASSERT_TRUE(wait_for(lc, lc, &pauline->stat.number_of_LinphoneRegistrationOk, 1));
		linphone_core_manager_destroy(pauline);
		bc_free(cert);
		bc_free(key);
	}
}

static void tls_auth_info_client_cert_api(void) {
	if (transport_supported(LinphoneTransportTls)) {
		LinphoneCoreManager *pauline = linphone_core_manager_new_with_proxies_check("pauline_tls_client_rc", FALSE);
		char *cert_path = bc_tester_res("certificates/client/cert.pem");
		char *key_path = bc_tester_res("certificates/client/key.pem");
		char *cert = read_file(cert_path);
		char *key = read_file(key_path);
		LinphoneCore *lc = pauline->lc;
		LinphoneAuthInfo *authInfo = (LinphoneAuthInfo *)bctbx_list_get_data(linphone_core_get_auth_info_list(lc));
		linphone_auth_info_set_tls_cert(authInfo, cert);
		linphone_auth_info_set_tls_key(authInfo, key);
		BC_ASSERT_TRUE(wait_for(lc, lc, &pauline->stat.number_of_LinphoneRegistrationOk, 1));
		linphone_core_manager_destroy(pauline);
		ms_free(cert);
		ms_free(key);
		bc_free(cert_path);
		bc_free(key_path);
	}
}

static void tls_auth_info_client_cert_api_path(void) {
	if (transport_supported(LinphoneTransportTls)) {
		LinphoneCoreManager *pauline = linphone_core_manager_new_with_proxies_check("pauline_tls_client_rc", FALSE);
		char *cert = bc_tester_res("certificates/client/cert.pem");
		char *key = bc_tester_res("certificates/client/key.pem");
		LinphoneCore *lc = pauline->lc;
		LinphoneAuthInfo *authInfo = (LinphoneAuthInfo *)bctbx_list_get_data(linphone_core_get_auth_info_list(lc));
		linphone_auth_info_set_tls_cert_path(authInfo, cert);
		linphone_auth_info_set_tls_key_path(authInfo, key);
		BC_ASSERT_TRUE(wait_for(lc, lc, &pauline->stat.number_of_LinphoneRegistrationOk, 1));
		linphone_core_manager_destroy(pauline);
		bc_free(cert);
		bc_free(key);
	}
}

static void register_get_gruu(void) {
	LinphoneCoreManager *marie = ms_new0(LinphoneCoreManager, 1);
	linphone_core_manager_init(marie, "marie_rc", NULL);
	linphone_core_add_supported_tag(marie->lc, "gruu");
	linphone_core_manager_start(marie, TRUE);
	LinphoneProxyConfig *cfg = linphone_core_get_default_proxy_config(marie->lc);
	if (cfg) {
		const LinphoneAddress *addr = linphone_proxy_config_get_contact(cfg);
		BC_ASSERT_PTR_NOT_NULL(addr);
		char *addrStr = linphone_address_as_string_uri_only(addr);
		BC_ASSERT_PTR_NOT_NULL(strstr(addrStr, "gr"));
		bctbx_free(addrStr);
	}
	linphone_core_manager_destroy(marie);
}

static void multi_devices_register_with_gruu(void) {
	LinphoneCoreManager *marie = ms_new0(LinphoneCoreManager, 1);
	linphone_core_manager_init(marie, "marie_rc", NULL);
	linphone_core_add_supported_tag(marie->lc, "gruu");
	linphone_core_manager_start(marie, TRUE);
	LinphoneProxyConfig *cfg = linphone_core_get_default_proxy_config(marie->lc);

	if (cfg) {
		const LinphoneAddress *addr = linphone_proxy_config_get_contact(cfg);
		BC_ASSERT_PTR_NOT_NULL(addr);
		BC_ASSERT_STRING_EQUAL(linphone_address_get_domain(addr), linphone_proxy_config_get_domain(cfg));
		BC_ASSERT_TRUE(linphone_address_has_uri_param(addr, "gr"));
	}

	linphone_core_set_network_reachable(marie->lc, FALSE); /*to make sure first instance is not unregistered*/
	linphone_core_manager_destroy(marie);

	marie = linphone_core_manager_new("marie_rc");
	cfg = linphone_core_get_default_proxy_config(marie->lc);
	if (cfg) {
		const LinphoneAddress *addr = linphone_proxy_config_get_contact(cfg);
		BC_ASSERT_PTR_NOT_NULL(addr);
		BC_ASSERT_STRING_EQUAL(linphone_address_get_domain(addr), linphone_proxy_config_get_domain(cfg));
		BC_ASSERT_TRUE(linphone_address_has_uri_param(addr, "gr"));
	}

	linphone_core_manager_destroy(marie);
}

static void update_contact_private_ip_address(void) {
	LinphoneCoreManager *lcm;
	stats *counters;
	LinphoneProxyConfig *cfg;
	char route[256];
	LinphoneAddress *from;
	LinphoneAuthInfo *ai;

	sprintf(route, "sip:%s", test_route);

	lcm = linphone_core_manager_new("empty_rc");

	/* Remove gruu for this test */
	linphone_core_remove_supported_tag(lcm->lc, "gruu");
	/* Disable ipv6 */
	linphone_core_enable_ipv6(lcm->lc, FALSE);

	counters = get_stats(lcm->lc);
	cfg = linphone_core_create_proxy_config(lcm->lc);
	from = create_linphone_address(auth_domain);

	linphone_proxy_config_set_identity_address(cfg, from);
	linphone_proxy_config_enable_register(cfg, TRUE);
	linphone_proxy_config_set_expires(cfg, 1);
	linphone_proxy_config_set_route(cfg, test_route);
	linphone_proxy_config_set_server_addr(cfg, test_route);
	linphone_address_unref(from);

	ai = linphone_auth_info_new(test_username, NULL, test_password, NULL, NULL, NULL);
	linphone_core_add_auth_info(lcm->lc, ai);
	linphone_auth_info_unref(ai);
	linphone_core_add_proxy_config(lcm->lc, cfg);

	BC_ASSERT_TRUE(wait_for(lcm->lc, lcm->lc, &counters->number_of_LinphoneRegistrationOk, 1));
	BC_ASSERT_EQUAL(counters->number_of_auth_info_requested, 0, int, "%d");

	LinphoneAddress *contact = linphone_address_clone(linphone_proxy_config_get_contact(cfg));
	BC_ASSERT_PTR_NOT_NULL(contact);
	BC_ASSERT_PTR_NOT_NULL(linphone_address_get_domain(contact));

	linphone_proxy_config_unref(cfg);
	linphone_core_manager_destroy(lcm);

	/* We have to recreate the core manager */
	lcm = linphone_core_manager_new("empty_rc");

	linphone_core_remove_supported_tag(lcm->lc, "gruu");
	linphone_core_enable_ipv6(lcm->lc, FALSE);
	/* We want this REGISTER to not be processed by flexisip's NatHelper module */
	linphone_core_set_user_agent(lcm->lc, "No NatHelper", NULL);

	counters = get_stats(lcm->lc);
	cfg = linphone_core_create_proxy_config(lcm->lc);
	from = create_linphone_address(auth_domain);

	linphone_proxy_config_set_identity_address(cfg, from);

	linphone_proxy_config_enable_register(cfg, TRUE);
	linphone_proxy_config_set_expires(cfg, 1);
	linphone_proxy_config_set_route(cfg, test_route);
	linphone_proxy_config_set_server_addr(cfg, test_route);
	linphone_address_unref(from);

	ai = linphone_auth_info_new(test_username, NULL, test_password, NULL, NULL, NULL);
	linphone_core_add_auth_info(lcm->lc, ai);
	linphone_auth_info_unref(ai);
	linphone_core_add_proxy_config(lcm->lc, cfg);

	BC_ASSERT_TRUE(wait_for_until(lcm->lc, lcm->lc, &counters->number_of_LinphoneRegistrationProgress, 1, 5000));
	const LinphoneAddress *ct = linphone_proxy_config_get_contact(cfg);
	if (!BC_ASSERT_PTR_NULL(ct)) {
		char *tmp = linphone_address_as_string(ct);
		ms_error("Contact address shall be NULL, but is %s", tmp);
		bctbx_free(tmp);
	}

	BC_ASSERT_TRUE(wait_for_until(lcm->lc, lcm->lc, &counters->number_of_LinphoneRegistrationOk, 1, 5000));
	BC_ASSERT_PTR_NOT_NULL(linphone_proxy_config_get_contact(cfg));

	/* the second REGISTER is to ensure that the contact address guessed from Via is set to the proxy */
	BC_ASSERT_TRUE(wait_for_until(lcm->lc, lcm->lc, &counters->number_of_LinphoneRegistrationOk, 2, 5000));

	const LinphoneAddress *contactUpdated = linphone_proxy_config_get_contact(cfg);
	BC_ASSERT_PTR_NOT_NULL(contactUpdated);
	BC_ASSERT_PTR_NOT_NULL(linphone_address_get_domain(contactUpdated));

	BC_ASSERT_STRING_EQUAL(linphone_address_get_domain(contactUpdated), linphone_address_get_domain(contact));

	linphone_address_unref(contact);
	linphone_proxy_config_unref(cfg);
	linphone_core_manager_destroy(lcm);
}

static void register_with_specific_client_port(void) {
#ifdef __linux__

	LinphoneCoreManager *lcm1 = linphone_core_manager_new_with_proxies_check("pauline_tcp_rc", FALSE);

	sal_set_client_bind_port(linphone_core_get_sal(lcm1->lc), 12088);
	linphone_core_manager_start(lcm1, TRUE);
	linphone_core_manager_destroy(lcm1);

	/*create a another one to make sure that port is reusable*/
	lcm1 = linphone_core_manager_new_with_proxies_check("pauline_tcp_rc", FALSE);

	sal_set_client_bind_port(linphone_core_get_sal(lcm1->lc), 12088);
	linphone_core_manager_start(lcm1, TRUE);

	linphone_core_manager_destroy(lcm1);
#else
	ms_message("Test skipped, can only run on Linux");
#endif
}

static void unreliable_channels_cleanup(void) {
	LinphoneCoreManager *lcm = linphone_core_manager_new_with_proxies_check("pauline_tcp_rc", TRUE);

	BC_ASSERT_EQUAL(lcm->stat.number_of_LinphoneRegistrationOk, 1, int, "%i");
	/* wait 4 seconds */
	wait_for_until(lcm->lc, lcm->lc, NULL, 0, 4000);
	/* simulate a push notification to be received, in order to have the unreliable connection to be closed. */
	linphone_core_ensure_registered(lcm->lc);
	/* this should result in a new register to be done */
	BC_ASSERT_TRUE(wait_for_until(lcm->lc, lcm->lc, &lcm->stat.number_of_LinphoneRegistrationOk, 2, 8000));
	linphone_core_manager_destroy(lcm);
}

static void registration_with_custom_contact(void) {
	LinphoneCoreManager *pauline = linphone_core_manager_new("pauline_rc");
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *laure = linphone_core_manager_new("laure_rc_udp");
	LinphoneAccount *account = linphone_core_get_default_account(pauline->lc);
	const LinphoneAccountParams *params = linphone_account_get_params(account);

	/* set laure as secondary contact for pauline */
	LinphoneAccountParams *new_params = linphone_account_params_clone(params);
	LinphoneAddress *custom_contact =
	    linphone_address_clone(linphone_account_get_contact_address(linphone_core_get_default_account(laure->lc)));
	bctbx_list_t *lcs = NULL;
	LinphoneCall *c;

	lcs = bctbx_list_append(lcs, pauline->lc);
	lcs = bctbx_list_append(lcs, marie->lc);
	lcs = bctbx_list_append(lcs, laure->lc);
	/* add a low priority for this secondary contact */
	linphone_address_set_param(custom_contact, "q", "0.3");

	linphone_account_params_set_custom_contact(new_params, custom_contact);
	linphone_address_unref(custom_contact);
	linphone_account_set_params(account, new_params);
	linphone_account_params_unref(new_params);

	BC_ASSERT_TRUE(wait_for_list(lcs, &pauline->stat.number_of_LinphoneRegistrationOk, 2, 10000));

	/* Marie makes a call to pauline, it should ring on pauline side, not laure, because pauline is available.*/

	if (BC_ASSERT_TRUE(call(marie, pauline))) {
		wait_for_list(lcs, NULL, 0, 3000);
		end_call(pauline, marie);
	}
	BC_ASSERT_TRUE(laure->stat.number_of_LinphoneCallIncomingReceived == 0);

	/* Now pauline is offline */
	linphone_core_set_network_reachable(pauline->lc, FALSE);
	/* Marie makes a call to pauline, laure shall receive it */
	linphone_core_invite_address(marie->lc, pauline->identity);
	if (BC_ASSERT_TRUE(wait_for_list(lcs, &laure->stat.number_of_LinphoneCallIncomingReceived, 1, 15000))) {
		linphone_call_accept(linphone_core_get_current_call(laure->lc));
		BC_ASSERT_TRUE(wait_for_list(lcs, &laure->stat.number_of_LinphoneCallStreamsRunning, 1, 10000));
		BC_ASSERT_TRUE(wait_for_list(lcs, &marie->stat.number_of_LinphoneCallStreamsRunning, 2, 10000));
		liblinphone_tester_check_rtcp(marie, laure);
		end_call(laure, marie);
	}
	/* Pauline goes online, and removes laure as secondary contact.*/
	linphone_core_set_network_reachable(pauline->lc, TRUE);
	BC_ASSERT_TRUE(wait_for_list(lcs, &pauline->stat.number_of_LinphoneRegistrationOk, 3, 10000));
	new_params = linphone_account_params_clone(linphone_account_get_params(account));
	linphone_account_params_set_custom_contact(new_params, NULL);
	linphone_account_set_params(account, new_params);
	linphone_account_params_unref(new_params);
	BC_ASSERT_TRUE(wait_for_list(lcs, &pauline->stat.number_of_LinphoneRegistrationOk, 4, 10000));

	/* Pauline goes offline. Marie makes a call, no one should receive this call.*/
	linphone_core_set_network_reachable(pauline->lc, FALSE);
	c = linphone_core_invite_address(marie->lc, pauline->identity);
	if (BC_ASSERT_PTR_NOT_NULL(c)) {
		linphone_call_ref(c);
		BC_ASSERT_FALSE(wait_for_list(lcs, &laure->stat.number_of_LinphoneCallIncomingReceived, 2, 15000));

		if (linphone_call_get_state(c) != LinphoneCallError) {
			linphone_call_terminate(c);
		}
		linphone_call_unref(c);
		BC_ASSERT_TRUE(wait_for_list(lcs, &marie->stat.number_of_LinphoneCallReleased, 3, 10000));
	}

	bctbx_list_free(lcs);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(laure);
}

static void _registration_with_ip_version_preference(bool with_ipv6) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");

	linphone_config_set_int(linphone_core_get_config(marie->lc), "sip", "prefer_ipv6", with_ipv6);

	linphone_core_remove_supported_tag(marie->lc, "gruu");
	linphone_core_enable_dns_srv(marie->lc, FALSE); /* we want to skip DNS SRV because given current flexisip-tester DNS
	    configuration, it gives only an IPv6 host */

	linphone_core_manager_start(marie, TRUE);
	LinphoneAddress *contact = linphone_account_get_contact_address(linphone_core_get_default_account(marie->lc));
	if (BC_ASSERT_PTR_NOT_NULL(contact)) {
		BC_ASSERT_EQUAL(ms_is_ipv6(linphone_address_get_domain(contact)), with_ipv6, int, "%i");
	}
	linphone_core_manager_destroy(marie);
}

static void registration_with_ip_version_preference(void) {
	if (liblinphone_tester_ipv6_available() && liblinphone_tester_ipv4_available()) {
		_registration_with_ip_version_preference(TRUE);
		_registration_with_ip_version_preference(FALSE);
	} else {
		ms_warning("Test skipped, ipv6 and ipv4 are not both available");
	}
}

/*
 * This test simulates the creation of a Linphone account and verifies that the LIME user is properly created.
 * An incorrect password is intentionally introduced to simulate the waiting period while the user awaits the
 * verification code sent via SMS.
 */
static void lime_user_creation_after_registration(void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneAuthInfo *info = (LinphoneAuthInfo *)bctbx_list_get_data(linphone_core_get_auth_info_list(marie->lc));
	char *password = bctbx_strdup(linphone_auth_info_get_password(info));
	linphone_auth_info_set_password(info, "wrongpw");
	BC_ASSERT_PTR_NOT_NULL(info);
	linphone_auth_info_ref(info);
	linphone_core_add_auth_info(marie->lc, info);
	set_lime_server_and_curve(C25519, marie);
	marie->lime_failure = TRUE;
	marie->registration_failure = TRUE;

	linphone_core_manager_start(marie, TRUE);

	// Check that LIME user is not created yet
	BC_ASSERT_TRUE(wait_for(marie->lc, NULL, &marie->stat.number_of_LinphoneRegistrationFailed, 1));
	BC_ASSERT_TRUE(wait_for(marie->lc, NULL, &marie->stat.number_of_X3dhUserCreationFailure, 1));

	const stats initialMarieStats = marie->stat;

	linphone_core_set_network_reachable(marie->lc, FALSE);
	LinphoneAuthInfo *new_info =
	    linphone_auth_info_new(linphone_auth_info_get_username(info), linphone_auth_info_get_userid(info), password,
	                           NULL, linphone_auth_info_get_realm(info), linphone_auth_info_get_domain(info));
	linphone_core_add_auth_info(marie->lc, new_info);
	linphone_core_set_network_reachable(marie->lc, TRUE);
	marie->lime_failure = FALSE;
	marie->registration_failure = FALSE;

	BC_ASSERT_TRUE(wait_for(marie->lc, NULL, &marie->stat.number_of_LinphoneRegistrationOk,
	                        initialMarieStats.number_of_LinphoneRegistrationOk + 1));
	BC_ASSERT_TRUE(wait_for(marie->lc, NULL, &marie->stat.number_of_X3dhUserCreationSuccess,
	                        initialMarieStats.number_of_X3dhUserCreationSuccess + 1));

	bctbx_free(password);
	linphone_auth_info_unref(info);
	linphone_auth_info_unref(new_info);
	linphone_core_manager_destroy(marie);
}

test_t register_tests[] = {
    TEST_NO_TAG("Simple register", simple_register), TEST_NO_TAG("Simple register unregister", simple_unregister),
    TEST_NO_TAG("TCP register", simple_tcp_register), TEST_NO_TAG("TCP register 2", simple_tcp_register2),
    TEST_NO_TAG("UDP register", simple_udp_register),
    TEST_NO_TAG("Register with custom headers", register_with_custom_headers),
    TEST_NO_TAG("TCP register compatibility mode", simple_tcp_register_compatibility_mode),
    TEST_ONE_TAG("TLS register", simple_tls_register, "CRYPTO"),
    TEST_ONE_TAG("TLS register with alt. name certificate", tls_alt_name_register, "CRYPTO"),
    TEST_ONE_TAG("TLS register with wildcard certificate", tls_wildcard_register, "CRYPTO"),
    TEST_ONE_TAG("TLS certificate not verified", tls_certificate_failure, "CRYPTO"),
    TEST_ONE_TAG("TLS certificate subjects check", tls_certificate_subject_check, "CRYPTO"),
    TEST_ONE_TAG("TLS certificate given by string instead of file", tls_certificate_data, "CRYPTO"),
    TEST_ONE_TAG("TLS with non tls server", tls_with_non_tls_server, "CRYPTO"),
    TEST_ONE_TAG("Simple authenticated register", simple_authenticated_register, "CRYPTO"),
    TEST_ONE_TAG("Simple authenticated register SHA-256", simple_authenticated_register_for_algorithm, "CRYPTO"),
    TEST_ONE_TAG("Simple authenticated register SHA-256 from cleartext password",
                 simple_authenticated_register_with_SHA256_from_clear_text_password,
                 "CRYPTO"),
    TEST_NO_TAG("Ha1 authenticated register", ha1_authenticated_register),
    TEST_ONE_TAG("Ha1 authenticated register SHA-256", ha1_authenticated_register_for_algorithm, "CRYPTO"),
    TEST_NO_TAG("Digest auth without initial credentials", authenticated_register_with_no_initial_credentials),
    TEST_NO_TAG("Digest auth with wrong credentials", authenticated_register_with_wrong_credentials),
    TEST_NO_TAG("Digest auth with wrong credentials, check if registration attempts are stopped",
                authenticated_register_with_wrong_credentials_2),
    TEST_NO_TAG("Digest auth with wrong credentials without 403",
                authenticated_register_with_wrong_credentials_without_403),
    TEST_NO_TAG("Authenticated register with wrong late credentials",
                authenticated_register_with_wrong_late_credentials),
    TEST_NO_TAG("Authenticated register with late credentials", authenticated_register_with_late_credentials),
    TEST_NO_TAG("Authenticated register with provided credentials", authenticated_register_with_provided_credentials),
    TEST_NO_TAG("Authenticated register with provided credentials, username with space",
                authenticated_register_with_provided_credentials_and_username_with_space),
    TEST_NO_TAG("Register with refresh", simple_register_with_refresh),
    TEST_NO_TAG("Register with custom refresh period", simple_register_with_custom_refresh_period),
    TEST_NO_TAG("Authenticated register with refresh", simple_auth_register_with_refresh),
    TEST_NO_TAG("Register with refresh and send error", register_with_refresh_with_send_error),
    TEST_NO_TAG("Multi account", multiple_proxy), TEST_NO_TAG("Transport changes", transport_change),
    TEST_NO_TAG("Transport configured with dontbind option", transport_dont_bind),
    // TEST_NO_TAG("Transport busy", transport_busy),
    TEST_NO_TAG("Proxy transport changes", proxy_transport_change),
    TEST_NO_TAG("Proxy transport changes with wrong address at first", proxy_transport_change_with_wrong_port),
    TEST_NO_TAG("Proxy transport changes with wrong address, giving up",
                proxy_transport_change_with_wrong_port_givin_up),
    TEST_NO_TAG("Change expires", change_expires), TEST_NO_TAG("Network state change", network_state_change),
    TEST_NO_TAG("Io recv error", io_recv_error),
    TEST_NO_TAG("Io recv error with recovery", io_recv_error_retry_immediatly),
    TEST_NO_TAG("Io recv error with late recovery", io_recv_error_late_recovery),
    TEST_NO_TAG("Io recv error without active registration", io_recv_error_without_active_register),
    TEST_NO_TAG("Simple redirect", redirect),
    TEST_ONE_TAG("Global TLS client certificate authentication", tls_auth_global_client_cert, "CRYPTO"),
    TEST_ONE_TAG("Global TLS client certificate authentication using API", tls_auth_global_client_cert_api, "CRYPTO"),
    TEST_ONE_TAG(
        "Global TLS client certificate authentication using API 2", tls_auth_global_client_cert_api_path, "CRYPTO"),
    TEST_ONE_TAG("AuthInfo TLS client certificate authentication using API", tls_auth_info_client_cert_api, "CRYPTO"),
    TEST_ONE_TAG(
        "AuthInfo TLS client certificate authentication using API 2", tls_auth_info_client_cert_api_path, "CRYPTO"),
    TEST_NO_TAG("Register get GRUU", register_get_gruu),
    TEST_NO_TAG("Register get GRUU for multi device", multi_devices_register_with_gruu),
    TEST_NO_TAG("Update contact private IP address", update_contact_private_ip_address),
    TEST_NO_TAG("Register with specific client port", register_with_specific_client_port),
    TEST_NO_TAG("Cleanup of unreliable channels", unreliable_channels_cleanup),
    TEST_NO_TAG("MD5-based digest rejected by policy", md5_digest_rejected),
    TEST_NO_TAG("Registration with custom contact", registration_with_custom_contact),
    TEST_NO_TAG("Registration with IP version preference", registration_with_ip_version_preference),
    TEST_NO_TAG("Lime user creation after registration", lime_user_creation_after_registration)};

test_suite_t register_test_suite = {"Register",
                                    NULL,
                                    NULL,
                                    liblinphone_tester_before_each,
                                    liblinphone_tester_after_each,
                                    sizeof(register_tests) / sizeof(register_tests[0]),
                                    register_tests,
                                    0};

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

#include <stdio.h>
#include <stdlib.h>

#include <bctoolbox/defs.h>

#include "liblinphone_tester.h"
#include "linphone/api/c-account-params.h"
#include "linphone/api/c-account.h"
#include "linphone/api/c-address.h"
#include "linphone/api/c-audio-device.h"
#include "linphone/api/c-call-log.h"
#include "linphone/api/c-chat-room.h"
#include "linphone/api/c-dial-plan.h"
#include "linphone/api/c-friend-phone-number.h"
#include "linphone/api/c-ldap-params.h"
#include "linphone/api/c-ldap.h"
#include "linphone/api/c-magic-search.h"
#include "linphone/api/c-search-result.h"
#include "linphone/chat.h"
#include "linphone/core.h"
#include "linphone/friend.h"
#include "linphone/friendlist.h"
#include "linphone/lpconfig.h"
#include "mediastreamer2/mscommon.h"
#include "tester_utils.h"

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif

static void linphone_version_test(void) {
	const char *version = linphone_core_get_version();
	/*make sure the git version is always included in the version number*/
	BC_ASSERT_PTR_NOT_NULL(version);
	BC_ASSERT_PTR_NULL(strstr(version, "unknown"));
	linphone_logging_service_set_domain(linphone_logging_service_get(), "test");
	unsigned int old = linphone_logging_service_get_log_level_mask(linphone_logging_service_get());
	linphone_logging_service_set_log_level_mask(linphone_logging_service_get(), LinphoneLogLevelTrace);
	linphone_logging_service_trace(linphone_logging_service_get(), "httpd_username=test-stefano%40nopmail.com");
	linphone_logging_service_set_log_level_mask(linphone_logging_service_get(), old);
}

void version_update_check_cb(LinphoneCore *core,
                             LinphoneVersionUpdateCheckResult result,
                             const char *version,
                             const char *url) {
	BC_ASSERT_STRING_EQUAL(version, "5.1.0-beta-12+af6t1i8");
	BC_ASSERT_STRING_EQUAL(url, "https://example.org/update.html");
	BC_ASSERT_EQUAL(result, LinphoneVersionUpdateCheckNewVersionAvailable, int, "%d");

	stats *stat = get_stats(core);
	stat->number_of_LinphoneCoreVersionUpdateCheck++;
}

static void linphone_version_update_test(void) {
	LinphoneCoreManager *lcm = linphone_core_manager_new(NULL);
	stats *stat = get_stats(lcm->lc);

	LinphoneConfig *config = linphone_core_get_config(lcm->lc);
	linphone_config_set_string(config, "misc", "version_check_url_root", "http://provisioning.example.org:10080/");

	LinphoneCoreCbs *cbs = linphone_factory_create_core_cbs(linphone_factory_get());
	linphone_core_cbs_set_version_update_check_result_received(cbs, version_update_check_cb);
	linphone_core_add_callbacks(lcm->lc, cbs);
	linphone_core_cbs_unref(cbs);

	linphone_core_check_for_update(lcm->lc, "5.1.0-alpha-34+fe2adf7");
	BC_ASSERT_TRUE(wait_for(lcm->lc, NULL, &stat->number_of_LinphoneCoreVersionUpdateCheck, 1));

	linphone_core_manager_destroy(lcm);
}

static void core_init_test_base(bool_t use_database, bool_t disable_main_db_only) {
	LinphoneCore *lc;
	FILE *in;
	char *empty_with_some_db_rc_path = bc_tester_res("rcfiles/empty_with_some_db_rc");
	lc = linphone_factory_create_core_3(
	    linphone_factory_get(), NULL,
	    (disable_main_db_only) ? empty_with_some_db_rc_path : liblinphone_tester_get_empty_rc(), system_context);
	if (BC_ASSERT_PTR_NOT_NULL(lc)) {
		linphone_config_set_int(linphone_core_get_config(lc), "lime", "enabled", 0);
		linphone_core_enable_database(lc, use_database);
		linphone_core_start(lc);
		if (use_database) {
			const char *uri = linphone_config_get_string(linphone_core_get_config(lc), "storage", "uri", NULL);
			BC_ASSERT_STRING_EQUAL(uri, "null");
			in = fopen(uri, "rb");
			if (!BC_ASSERT_PTR_NULL(in)) // "null" file should not exists
				fclose(in);
		}
		BC_ASSERT_TRUE(linphone_core_database_enabled(lc) == use_database);
		/* until we have good certificates on our test server... */
		linphone_core_verify_server_certificates(lc, FALSE);
		BC_ASSERT_EQUAL(linphone_core_get_global_state(lc), LinphoneGlobalOn, int, "%i");
		const char *example_plugin_name = "liblinphone_exampleplugin";
		const char *ekt_server_plugin_name = "liblinphone_ektserver";
		const bctbx_list_t *plugins = linphone_core_get_loaded_plugins(lc);
#if defined(HAVE_EXAMPLE_PLUGIN) || defined(HAVE_EKT_SERVER_PLUGIN)
#ifdef __IOS__
		BC_ASSERT_EQUAL(bctbx_list_size(plugins), 0, size_t, "%zu");
		BC_ASSERT_PTR_NULL(
		    bctbx_list_find_custom((bctbx_list_t *)plugins, (bctbx_compare_func)strcmp, example_plugin_name));
		BC_ASSERT_FALSE(linphone_core_is_plugin_loaded(lc, example_plugin_name));
		BC_ASSERT_PTR_NULL(
		    bctbx_list_find_custom((bctbx_list_t *)plugins, (bctbx_compare_func)strcmp, ekt_server_plugin_name));
		BC_ASSERT_FALSE(linphone_core_is_plugin_loaded(lc, ekt_server_plugin_name));
#else
		BC_ASSERT_GREATER_STRICT(bctbx_list_size(plugins), 0, size_t, "%zu");
#ifdef HAVE_EXAMPLE_PLUGIN
		BC_ASSERT_PTR_NOT_NULL(
		    bctbx_list_find_custom((bctbx_list_t *)plugins, (bctbx_compare_func)strcmp, example_plugin_name));
		BC_ASSERT_TRUE(linphone_core_is_plugin_loaded(lc, example_plugin_name));
#else
		BC_ASSERT_PTR_NULL(
		    bctbx_list_find_custom((bctbx_list_t *)plugins, (bctbx_compare_func)strcmp, example_plugin_name));
		BC_ASSERT_FALSE(linphone_core_is_plugin_loaded(lc, example_plugin_name));
#endif // HAVE_EXAMPLE_PLUGIN
#ifdef HAVE_EKT_SERVER_PLUGIN
		BC_ASSERT_PTR_NOT_NULL(
		    bctbx_list_find_custom((bctbx_list_t *)plugins, (bctbx_compare_func)strcmp, ekt_server_plugin_name));
		BC_ASSERT_TRUE(linphone_core_is_plugin_loaded(lc, ekt_server_plugin_name));
#else
		BC_ASSERT_PTR_NULL(
		    bctbx_list_find_custom((bctbx_list_t *)plugins, (bctbx_compare_func)strcmp, ekt_server_plugin_name));
		BC_ASSERT_FALSE(linphone_core_is_plugin_loaded(lc, ekt_server_plugin_name));
#endif // HAVE_EKT_SERVER_PLUGIN
#endif // __IOS__
#else
		BC_ASSERT_PTR_NULL(
		    bctbx_list_find_custom((bctbx_list_t *)plugins, (bctbx_compare_func)strcmp, example_plugin_name));
		BC_ASSERT_FALSE(linphone_core_is_plugin_loaded(lc, example_plugin_name));
		BC_ASSERT_PTR_NULL(
		    bctbx_list_find_custom((bctbx_list_t *)plugins, (bctbx_compare_func)strcmp, ekt_server_plugin_name));
		BC_ASSERT_FALSE(linphone_core_is_plugin_loaded(lc, ekt_server_plugin_name));
#endif // HAVE_EXAMPLE_PLUGIN || HAVE_EKT_SERVER_PLUGIN
		linphone_core_stop(lc);
		linphone_core_unref(lc);
	}
	ms_free(empty_with_some_db_rc_path);
}

static void core_init_test(void) {
	core_init_test_base(TRUE, FALSE);
}

static void core_init_test_some_database(void) {
	core_init_test_base(TRUE, TRUE);
}

static void core_init_test_no_database(void) {
	core_init_test_base(FALSE, FALSE);
}

static void core_init_test_2(void) {
	LinphoneCore *lc;
	char *rc_path = bc_tester_res("rcfiles/chloe_rc");
	lc = linphone_factory_create_core_3(linphone_factory_get(), NULL, rc_path, system_context);

	if (BC_ASSERT_PTR_NOT_NULL(lc)) {
		linphone_config_set_int(linphone_core_get_config(lc), "lime", "enabled", 0);
		linphone_core_start(lc);

		/* until we have good certificates on our test server... */
		linphone_core_verify_server_certificates(lc, FALSE);
		BC_ASSERT_EQUAL(linphone_core_get_global_state(lc), LinphoneGlobalOn, int, "%i");

		LinphoneConfig *config = linphone_core_get_config(lc);

		linphone_config_set_string(config, "test", "test", "test");
		linphone_config_sync(config);

		const char *filename = linphone_config_get_filename(config);
		const char *factory = linphone_config_get_factory_filename(config);
		const char *tmp = linphone_config_get_temporary_filename(config);
		BC_ASSERT_PTR_NULL(filename);
		BC_ASSERT_STRING_EQUAL(factory, rc_path);
		BC_ASSERT_PTR_NULL(tmp);

		BC_ASSERT_PTR_NOT_NULL(linphone_core_get_default_proxy_config(lc));
		linphone_core_stop(lc);
		linphone_core_unref(lc);
	}

	ms_free(rc_path);
}

static void core_init_test_3(void) {
	LinphoneCore *lc = linphone_factory_create_core_3(linphone_factory_get(), NULL, NULL, system_context);

	if (BC_ASSERT_PTR_NOT_NULL(lc)) {
		linphone_config_set_int(linphone_core_get_config(lc), "lime", "enabled", 0);
		linphone_core_start(lc);

		LinphoneConfig *config = linphone_core_get_config(lc);

		linphone_config_set_string(config, "test", "test", "test");
		linphone_config_sync(config);

		const char *filename = linphone_config_get_filename(config);
		const char *factory = linphone_config_get_factory_filename(config);
		const char *tmp = linphone_config_get_temporary_filename(config);
		BC_ASSERT_PTR_NULL(filename);
		BC_ASSERT_PTR_NULL(factory);
		BC_ASSERT_PTR_NULL(tmp);
		linphone_core_stop(lc);
		linphone_core_unref(lc);
	}
}

static void core_init_test_4(void) {
	// Copy the RC file into a writable directory since it will be edited
	char *rc_path = bc_tester_res("rcfiles/lise_rc");
	char *writable_rc_path = liblinphone_tester_make_unique_file_path("lise", "rc");
	BC_ASSERT_FALSE(liblinphone_tester_copy_file(rc_path, writable_rc_path));

	LinphoneCore *lc = linphone_factory_create_core_3(linphone_factory_get(), writable_rc_path, NULL, system_context);

	if (BC_ASSERT_PTR_NOT_NULL(lc)) {
		linphone_config_set_int(linphone_core_get_config(lc), "lime", "enabled", 0);
		linphone_core_start(lc);

		LinphoneConfig *config = linphone_core_get_config(lc);

		const char *filename = linphone_config_get_filename(config);
		const char *factory = linphone_config_get_factory_filename(config);
		const char *tmp = linphone_config_get_temporary_filename(config);
		BC_ASSERT_PTR_NOT_NULL(filename);
		BC_ASSERT_PTR_NULL(factory);

		BC_ASSERT_PTR_NOT_NULL(tmp);
		char test_tmp_name[1024] = {0};
		snprintf(test_tmp_name, sizeof(test_tmp_name), "%s.tmp", filename);
		BC_ASSERT_STRING_EQUAL(tmp, test_tmp_name);
		linphone_core_stop(lc);
		linphone_core_unref(lc);
	}

	unlink(writable_rc_path);
	bctbx_free(writable_rc_path);
	bctbx_free(rc_path);
}

static void core_init_stop_test(void) {
	LinphoneCore *lc;
	lc =
	    linphone_factory_create_core_3(linphone_factory_get(), NULL, liblinphone_tester_get_empty_rc(), system_context);

	if (BC_ASSERT_PTR_NOT_NULL(lc)) {
		linphone_config_set_int(linphone_core_get_config(lc), "lime", "enabled", 0);
		linphone_core_start(lc);
		/* until we have good certificates on our test server... */
		linphone_core_verify_server_certificates(lc, FALSE);
		BC_ASSERT_EQUAL(linphone_core_get_global_state(lc), LinphoneGlobalOn, int, "%i");
		linphone_core_stop(lc);
		BC_ASSERT_EQUAL(linphone_core_get_global_state(lc), LinphoneGlobalOff, int, "%i");
	}

	if (BC_ASSERT_PTR_NOT_NULL(lc)) {
		linphone_core_unref(lc);
	}
}

static void core_init_unref_test(void) {
	LinphoneCore *lc;
	lc =
	    linphone_factory_create_core_3(linphone_factory_get(), NULL, liblinphone_tester_get_empty_rc(), system_context);

	if (BC_ASSERT_PTR_NOT_NULL(lc)) {
		linphone_config_set_int(linphone_core_get_config(lc), "lime", "enabled", 0);
		linphone_core_start(lc);
		/* until we have good certificates on our test server... */
		linphone_core_verify_server_certificates(lc, FALSE);
		BC_ASSERT_EQUAL(linphone_core_get_global_state(lc), LinphoneGlobalOn, int, "%i");
		linphone_core_stop(lc);
		linphone_core_unref(lc);
	}
}

static void core_init_stop_start_test(void) {
	LinphoneCore *lc;
	lc =
	    linphone_factory_create_core_3(linphone_factory_get(), NULL, liblinphone_tester_get_empty_rc(), system_context);

	if (BC_ASSERT_PTR_NOT_NULL(lc)) {
		linphone_config_set_int(linphone_core_get_config(lc), "lime", "enabled", 0);
		linphone_core_start(lc);
		/* until we have good certificates on our test server... */
		linphone_core_verify_server_certificates(lc, FALSE);
		const char *uuid = linphone_config_get_string(linphone_core_get_config(lc), "misc", "uuid", NULL);
		BC_ASSERT_STRING_NOT_EQUAL(uuid, "");
		BC_ASSERT_EQUAL(linphone_core_get_global_state(lc), LinphoneGlobalOn, int, "%i");
		linphone_core_stop(lc);
		BC_ASSERT_EQUAL(linphone_core_get_global_state(lc), LinphoneGlobalOff, int, "%i");
		linphone_core_start(lc);
		BC_ASSERT_EQUAL(linphone_core_get_global_state(lc), LinphoneGlobalOn, int, "%i");

		const char *uuid2 = linphone_config_get_string(linphone_core_get_config(lc), "misc", "uuid", NULL);
		BC_ASSERT_STRING_NOT_EQUAL(uuid2, "");
		BC_ASSERT_STRING_EQUAL(uuid, uuid2);
		linphone_core_stop(lc);
		linphone_core_unref(lc);
	}
}

static void core_set_user_agent(void) {
	LinphoneCore *lc = linphone_factory_create_core_3(linphone_factory_get(), NULL, NULL, system_context);

	if (BC_ASSERT_PTR_NOT_NULL(lc)) {
		linphone_core_set_user_agent(lc, "part1", "part2");
		linphone_config_set_int(linphone_core_get_config(lc), "lime", "enabled", 0);
		linphone_core_start(lc);
		BC_ASSERT_EQUAL(strcmp(linphone_core_get_user_agent(lc), "part1/part2"), 0, int, "%d");

		linphone_core_stop(lc);
		linphone_core_set_user_agent(lc, "part1b", "part2b");
		linphone_core_start(lc);
		BC_ASSERT_EQUAL(strcmp(linphone_core_get_user_agent(lc), "part1b/part2b"), 0, int, "%d");
		linphone_core_stop(lc);
		linphone_core_unref(lc);
	}
}

static void linphone_address_test(void) {
	LinphoneAddress *address;
	char *str;

	linphone_address_unref(create_linphone_address(NULL));
	BC_ASSERT_PTR_NULL(linphone_address_new("sip:@sip.linphone.org"));

	linphone_address_unref(create_linphone_address(NULL));
	BC_ASSERT_PTR_NULL(linphone_address_new("sip:paul ine@sip.linphone.org"));

	address = linphone_address_new("sip:paul%20ine@90.110.127.31");
	if (!BC_ASSERT_PTR_NOT_NULL(address)) return;
	linphone_address_unref(address);

	address = linphone_address_new("sip:90.110.127.31");
	if (!BC_ASSERT_PTR_NOT_NULL(address)) return;
	linphone_address_unref(address);

	address = linphone_address_new("sip:[::ffff:90.110.127.31]");
	if (!BC_ASSERT_PTR_NOT_NULL(address)) return;
	linphone_address_unref(address);

	/* Verifies that the [sip]/force_name_addr works as expected. */
	LinphoneCoreManager *lcm = linphone_core_manager_create("empty_rc");
	linphone_config_set_int(linphone_core_get_config(lcm->lc), "sip", "force_name_addr", 1);
	bctbx_message("force_name_addr enabled");
	linphone_core_start(lcm->lc);
	address = linphone_address_new("sip:bob@example.com");
	if (BC_ASSERT_PTR_NOT_NULL(address)) {
		str = linphone_address_as_string(address);
		BC_ASSERT_STRING_EQUAL(str, "<sip:bob@example.com>");
		bctbx_free(str);
		linphone_address_unref(address);
	}
	linphone_core_stop(lcm->lc);
	linphone_config_set_int(linphone_core_get_config(lcm->lc), "sip", "force_name_addr", 0);
	linphone_core_start(lcm->lc);
	address = linphone_address_new("sip:bob@example.com");
	if (BC_ASSERT_PTR_NOT_NULL(address)) {
		str = linphone_address_as_string(address);
		BC_ASSERT_STRING_EQUAL(str, "sip:bob@example.com");
		bctbx_free(str);
		linphone_address_unref(address);
	}
	linphone_core_manager_destroy(lcm);
}

static void core_sip_transport_test(void) {
	LinphoneCore *lc;
	LCSipTransports tr;
	lc =
	    linphone_factory_create_core_3(linphone_factory_get(), NULL, liblinphone_tester_get_empty_rc(), system_context);
	if (!BC_ASSERT_PTR_NOT_NULL(lc)) return;
	linphone_config_set_int(linphone_core_get_config(lc), "lime", "enabled", 0);
	linphone_core_start(lc);
	linphone_core_get_sip_transports(lc, &tr);
	BC_ASSERT_EQUAL(tr.udp_port, -2, int, "%d"); /*default config in empty_rc*/
	BC_ASSERT_EQUAL(tr.tcp_port, -2, int, "%d"); /*default config in empty_rc*/

	tr.udp_port = LC_SIP_TRANSPORT_RANDOM;
	tr.tcp_port = LC_SIP_TRANSPORT_RANDOM;
	tr.tls_port = LC_SIP_TRANSPORT_RANDOM;

	linphone_core_set_sip_transports(lc, &tr);
	linphone_core_get_sip_transports(lc, &tr);

	BC_ASSERT_NOT_EQUAL(tr.udp_port, -2, int, "%d");
	BC_ASSERT_NOT_EQUAL(tr.tcp_port, -2, int, "%d");

	BC_ASSERT_EQUAL(linphone_config_get_int(linphone_core_get_config(lc), "sip", "sip_port", -2),
	                LC_SIP_TRANSPORT_RANDOM, int, "%d");
	BC_ASSERT_EQUAL(linphone_config_get_int(linphone_core_get_config(lc), "sip", "sip_tcp_port", -2),
	                LC_SIP_TRANSPORT_RANDOM, int, "%d");
	BC_ASSERT_EQUAL(linphone_config_get_int(linphone_core_get_config(lc), "sip", "sip_tls_port", -2),
	                LC_SIP_TRANSPORT_RANDOM, int, "%d");
	linphone_core_stop(lc);
	linphone_core_unref(lc);
}

static void linphone_interpret_url_test(void) {
	LinphoneCore *lc;
	const char *sips_address = "sips:margaux@sip.linphone.org";
	LinphoneAddress *address;
	LinphoneProxyConfig *proxy_config;
	char *tmp;
	lc =
	    linphone_factory_create_core_3(linphone_factory_get(), NULL, liblinphone_tester_get_empty_rc(), system_context);
	if (!BC_ASSERT_PTR_NOT_NULL(lc)) return;
	linphone_config_set_int(linphone_core_get_config(lc), "lime", "enabled", 0);
	linphone_core_start(lc);

	proxy_config = linphone_core_create_proxy_config(lc);
	LinphoneAddress *addr = linphone_address_new("sip:moi@sip.linphone.org");
	linphone_proxy_config_set_identity_address(proxy_config, addr);
	if (addr) linphone_address_unref(addr);
	linphone_proxy_config_enable_register(proxy_config, FALSE);
	linphone_proxy_config_set_server_addr(proxy_config, "sip:sip.linphone.org");
	linphone_core_add_proxy_config(lc, proxy_config);
	linphone_core_set_default_proxy_config(lc, proxy_config);
	linphone_proxy_config_unref(proxy_config);

	address = linphone_core_interpret_url(lc, sips_address);
	BC_ASSERT_PTR_NOT_NULL(address);
	BC_ASSERT_STRING_EQUAL(linphone_address_get_scheme(address), "sips");
	BC_ASSERT_STRING_EQUAL(linphone_address_get_username(address), "margaux");
	BC_ASSERT_STRING_EQUAL(linphone_address_get_domain(address), "sip.linphone.org");
	linphone_address_unref(address);

	address = linphone_core_interpret_url(lc, "23");
	BC_ASSERT_PTR_NOT_NULL(address);
	BC_ASSERT_STRING_EQUAL(linphone_address_get_scheme(address), "sip");
	BC_ASSERT_STRING_EQUAL(linphone_address_get_username(address), "23");
	BC_ASSERT_STRING_EQUAL(linphone_address_get_domain(address), "sip.linphone.org");
	linphone_address_unref(address);

	address = linphone_core_interpret_url(lc, "#24");
	BC_ASSERT_PTR_NOT_NULL(address);
	BC_ASSERT_STRING_EQUAL(linphone_address_get_scheme(address), "sip");
	BC_ASSERT_STRING_EQUAL(linphone_address_get_username(address), "#24");
	BC_ASSERT_STRING_EQUAL(linphone_address_get_domain(address), "sip.linphone.org");
	tmp = linphone_address_as_string(address);
	BC_ASSERT_TRUE(strcmp(tmp, "sip:%2324@sip.linphone.org") == 0);
	linphone_address_unref(address);

	address = linphone_core_interpret_url(lc, tmp);
	BC_ASSERT_STRING_EQUAL(linphone_address_get_scheme(address), "sip");
	BC_ASSERT_STRING_EQUAL(linphone_address_get_username(address), "#24");
	BC_ASSERT_STRING_EQUAL(linphone_address_get_domain(address), "sip.linphone.org");
	linphone_address_unref(address);
	ms_free(tmp);

	address = linphone_core_interpret_url(lc, "paul ine");
	BC_ASSERT_PTR_NOT_NULL(address);
	BC_ASSERT_STRING_EQUAL(linphone_address_get_scheme(address), "sip");
	BC_ASSERT_STRING_EQUAL(linphone_address_get_username(address), "paul ine");
	BC_ASSERT_STRING_EQUAL(linphone_address_get_domain(address), "sip.linphone.org");
	tmp = linphone_address_as_string(address);
	BC_ASSERT_TRUE(strcmp(tmp, "sip:paul%20ine@sip.linphone.org") == 0);
	linphone_address_unref(address);

	address = linphone_core_interpret_url(lc, tmp);
	BC_ASSERT_STRING_EQUAL(linphone_address_get_scheme(address), "sip");
	BC_ASSERT_STRING_EQUAL(linphone_address_get_username(address), "paul ine");
	BC_ASSERT_STRING_EQUAL(linphone_address_get_domain(address), "sip.linphone.org");
	linphone_address_unref(address);
	ms_free(tmp);

	linphone_core_stop(lc);
	linphone_core_unref(lc);
}

static void linphone_config_safety_test(void) {
	char *res = bc_tester_res("rcfiles/marie_rc");
	char *file = bc_tester_file("rw_marie_rc");
	char *tmpfile = bctbx_strdup_printf("%s.tmp", file);

	BC_ASSERT_EQUAL(liblinphone_tester_copy_file(res, file), 0, int, "%d");

	LinphoneConfig *cfg = linphone_config_new(file);
	BC_ASSERT_PTR_NOT_NULL(cfg);
	BC_ASSERT_STRING_EQUAL(linphone_config_get_string(cfg, "proxy_0", "realm", NULL), "sip.example.org");
	/* add new key */
	linphone_config_set_string(cfg, "misc", "somekey", "somevalue");
	linphone_config_sync(cfg);
	linphone_config_destroy(cfg);

	/* reload and check everything was written */
	cfg = linphone_config_new(file);
	BC_ASSERT_PTR_NOT_NULL(cfg);
	BC_ASSERT_STRING_EQUAL(linphone_config_get_string(cfg, "proxy_0", "realm", NULL), "sip.example.org");
	BC_ASSERT_STRING_EQUAL(linphone_config_get_string(cfg, "misc", "somekey", NULL), "somevalue");
	/* now modify something and simulate a crash during write */
	ms_message("Simulating a crash during writing.");
	linphone_config_set_string(cfg, "misc", "somekey", "someothervalue");
	linphone_config_simulate_crash_during_sync(cfg, TRUE);
	linphone_config_sync(cfg);
	linphone_config_destroy(cfg);

	/* we should have a .tmp file but the normal file shall be there, untouched. */
	BC_ASSERT_TRUE(bctbx_file_exist(tmpfile) == 0);
	BC_ASSERT_TRUE(bctbx_file_exist(file) == 0);
	cfg = linphone_config_new(file);
	BC_ASSERT_STRING_EQUAL(linphone_config_get_string(cfg, "proxy_0", "realm", NULL), "sip.example.org");
	BC_ASSERT_STRING_EQUAL(linphone_config_get_string(cfg, "misc", "somekey", NULL), "somevalue");
	linphone_config_set_string(cfg, "misc", "somekey", "someothervalue");
	linphone_config_sync(cfg);
	/* the tmp file should have gone */
	BC_ASSERT_TRUE(bctbx_file_exist(tmpfile) == -1);
	linphone_config_destroy(cfg);

	ms_message("Simulating a crash just before renaming.");
	/* simulate a crash after writing the tmp file, but before renaming to real file:
	 in that case we have only the .tmp suffixed file. */
	BC_ASSERT_TRUE(rename(file, tmpfile) == 0);
	BC_ASSERT_TRUE(bctbx_file_exist(tmpfile) == 0);
	BC_ASSERT_TRUE(bctbx_file_exist(file) == -1);
	cfg = linphone_config_new(file);
	/* the .tmp file should have been loaded */
	BC_ASSERT_STRING_EQUAL(linphone_config_get_string(cfg, "proxy_0", "realm", NULL), "sip.example.org");
	BC_ASSERT_STRING_EQUAL(linphone_config_get_string(cfg, "misc", "somekey", NULL), "someothervalue");
	linphone_config_set_string(cfg, "misc", "somekey", "somevalue");
	linphone_config_sync(cfg);
	/* the tmp file should have gone */
	BC_ASSERT_TRUE(bctbx_file_exist(tmpfile) == -1);
	linphone_config_destroy(cfg);

	cfg = linphone_config_new(file);
	BC_ASSERT_STRING_EQUAL(linphone_config_get_string(cfg, "proxy_0", "realm", NULL), "sip.example.org");
	BC_ASSERT_STRING_EQUAL(linphone_config_get_string(cfg, "misc", "somekey", NULL), "somevalue");
	linphone_config_destroy(cfg);

	linphone_config_simulate_read_failure(TRUE);
	cfg = linphone_config_new(file);
	BC_ASSERT_PTR_NULL(cfg); /* this should fail */
	linphone_config_simulate_read_failure(FALSE);
	unlink(file);
	bc_free(res);
	bc_free(file);
	bctbx_free(tmpfile);
}

static void linphone_lpconfig_from_buffer(void) {
	const char *buffer = "[buffer]\ntest=ok";
	const char *buffer_linebreaks = "[buffer_linebreaks]\n\n\n\r\n\n\r\ntest=ok";
	LpConfig *conf;

	conf = linphone_config_new_from_buffer(buffer);
	BC_ASSERT_STRING_EQUAL(linphone_config_get_string(conf, "buffer", "test", ""), "ok");
	linphone_config_destroy(conf);

	conf = linphone_config_new_from_buffer(buffer_linebreaks);
	BC_ASSERT_STRING_EQUAL(linphone_config_get_string(conf, "buffer_linebreaks", "test", ""), "ok");
	linphone_config_destroy(conf);
}

static void linphone_lpconfig_from_buffer_zerolen_value(void) {
	/* parameters that have no value should return NULL, not "". */
	const char *zerolen = "[test]\nzero_len=\nnon_zero_len=test";
	LpConfig *conf;

	conf = linphone_config_new_from_buffer(zerolen);

	BC_ASSERT_STRING_EQUAL(linphone_config_get_string(conf, "test", "zero_len", "LOL"), "LOL");
	BC_ASSERT_STRING_EQUAL(linphone_config_get_string(conf, "test", "non_zero_len", ""), "test");

	linphone_config_set_string(conf, "test", "non_zero_len", ""); /* should remove "non_zero_len" */
	BC_ASSERT_STRING_EQUAL(linphone_config_get_string(conf, "test", "non_zero_len", "LOL"), "LOL");

	linphone_config_destroy(conf);
}

static void linphone_lpconfig_from_file_zerolen_value(void) {
	/* parameters that have no value should return NULL, not "". */
	const char *zero_rc_file = "zero_length_params_rc";
	char *rc_path = ms_strdup_printf("%s/rcfiles/%s", bc_tester_get_resource_dir_prefix(), zero_rc_file);
	LpConfig *conf;

	/* not using linphone_config_new() because it expects a readable file, and iOS (for instance)
	   stores the app bundle in read-only */
	conf = linphone_config_new_with_factory(NULL, rc_path);

	BC_ASSERT_STRING_EQUAL(linphone_config_get_string(conf, "test", "zero_len", "LOL"), "LOL");

	// non_zero_len=test -> should return test
	BC_ASSERT_STRING_EQUAL(linphone_config_get_string(conf, "test", "non_zero_len", ""), "test");

	linphone_config_set_string(conf, "test", "non_zero_len", ""); /* should remove "non_zero_len" */
	BC_ASSERT_STRING_EQUAL(linphone_config_get_string(conf, "test", "non_zero_len", "LOL"), "LOL");

	ms_free(rc_path);
	linphone_config_destroy(conf);
}

void linphone_lpconfig_invalid_friend(void) {
	LinphoneCoreManager *mgr = linphone_core_manager_new_with_proxies_check("invalid_friends_rc", FALSE);
	LinphoneFriendList *friendList = linphone_core_get_default_friend_list(mgr->lc);
	const bctbx_list_t *friends = linphone_friend_list_get_friends(friendList);
	BC_ASSERT_EQUAL((int)bctbx_list_size(friends), 3, int, "%d");
	LinphoneFriend *fakeFriend = linphone_core_create_friend(mgr->lc);
	BC_ASSERT_PTR_NOT_NULL(fakeFriend);
	const bctbx_list_t *addresses = linphone_friend_get_addresses(fakeFriend);
	BC_ASSERT_EQUAL(bctbx_list_size(addresses), 0, size_t, "%0zu");
	linphone_friend_unref(fakeFriend);
	linphone_core_manager_destroy(mgr);
}

void linphone_lpconfig_invalid_friend_remote_provisioning(void) {
	LinphoneCoreManager *mgr = linphone_core_manager_new_with_proxies_check("empty_rc", FALSE);

	const char *zero_xml_file = "invalid_friends_xml";
	char *xml_path = ms_strdup_printf("%s/rcfiles/%s", bc_tester_get_resource_dir_prefix(), zero_xml_file);
	BC_ASSERT_EQUAL(linphone_remote_provisioning_load_file(mgr->lc, xml_path), 0, int, "%d");

	LinphoneFriendList *friendList = linphone_core_get_default_friend_list(mgr->lc);
	const bctbx_list_t *friends = linphone_friend_list_get_friends(friendList);
	BC_ASSERT_EQUAL((int)bctbx_list_size(friends), 3, int, "%d");
	linphone_core_manager_destroy(mgr);
	ms_free(xml_path);
}

static void linphone_lpconfig_from_xml_zerolen_value(void) {
	const char *zero_xml_file = "remote_zero_length_params_rc";
	char *xml_path = ms_strdup_printf("%s/rcfiles/%s", bc_tester_get_resource_dir_prefix(), zero_xml_file);
	LpConfig *conf;

	LinphoneCoreManager *mgr = linphone_core_manager_new_with_proxies_check("empty_rc", FALSE);

	/* BUG
	 * This test makes a provisionning by xml outside of the Configuring state of the LinphoneCore.
	 * It is leaking memory because the config is litterally erased and rewritten by the invocation
	 * of the private function linphone_remote_provisioning_load_file .
	 */

	BC_ASSERT_EQUAL(linphone_remote_provisioning_load_file(mgr->lc, xml_path), 0, int, "%d");

	conf = linphone_core_get_config(mgr->lc);

	BC_ASSERT_STRING_EQUAL(linphone_config_get_string(conf, "test", "zero_len", "LOL"), "LOL");
	BC_ASSERT_STRING_EQUAL(linphone_config_get_string(conf, "test", "non_zero_len", ""), "test");

	linphone_config_set_string(conf, "test", "non_zero_len", ""); /* should remove "non_zero_len" */
	BC_ASSERT_STRING_EQUAL(linphone_config_get_string(conf, "test", "non_zero_len", "LOL"), "LOL");

	linphone_core_manager_destroy(mgr);
	ms_free(xml_path);
}

void linphone_proxy_config_address_equal_test(void) {
	LinphoneAddress *a = linphone_address_new("sip:toto@titi");
	LinphoneAddress *b = linphone_address_new("sips:toto@titi");
	LinphoneAddress *c = linphone_address_new("sip:toto@titi;transport=tcp");
	LinphoneAddress *d = linphone_address_new("sip:toto@titu");
	LinphoneAddress *e = linphone_address_new("sip:toto@titi;transport=udp");
	LinphoneAddress *f = linphone_address_new("sip:toto@titi?X-Create-Account=yes");

	BC_ASSERT_EQUAL(linphone_proxy_config_address_equal(a, NULL), LinphoneProxyConfigAddressDifferent, int, "%d");
	BC_ASSERT_EQUAL(linphone_proxy_config_address_equal(a, b), LinphoneProxyConfigAddressDifferent, int, "%d");
	BC_ASSERT_EQUAL(linphone_proxy_config_address_equal(a, c), LinphoneProxyConfigAddressDifferent, int, "%d");
	BC_ASSERT_EQUAL(linphone_proxy_config_address_equal(a, d), LinphoneProxyConfigAddressDifferent, int, "%d");
	BC_ASSERT_EQUAL(linphone_proxy_config_address_equal(a, e), LinphoneProxyConfigAddressWeakEqual, int, "%d");
	BC_ASSERT_EQUAL(linphone_proxy_config_address_equal(NULL, NULL), LinphoneProxyConfigAddressEqual, int, "%d");
	BC_ASSERT_EQUAL(linphone_proxy_config_address_equal(a, f), LinphoneProxyConfigAddressEqual, int, "%d");
	BC_ASSERT_EQUAL(linphone_proxy_config_address_equal(c, f), LinphoneProxyConfigAddressDifferent, int, "%d");
	BC_ASSERT_EQUAL(linphone_proxy_config_address_equal(e, f), LinphoneProxyConfigAddressWeakEqual, int, "%d");

	linphone_address_unref(a);
	linphone_address_unref(b);
	linphone_address_unref(c);
	linphone_address_unref(d);
	linphone_address_unref(e);
	linphone_address_unref(f);
}

void linphone_proxy_config_is_server_config_changed_test(void) {
	LinphoneProxyConfig *proxy_config = linphone_core_create_proxy_config(NULL);

	linphone_proxy_config_done(proxy_config); /*test done without edit*/

	LinphoneAddress *addr = linphone_address_new("sip:toto@titi");
	linphone_proxy_config_set_identity_address(proxy_config, addr);
	if (addr) linphone_address_unref(addr);
	linphone_proxy_config_edit(proxy_config);
	addr = linphone_address_new("sips:toto@titi");
	linphone_proxy_config_set_identity_address(proxy_config, addr);
	linphone_proxy_config_done(proxy_config);
	if (addr) linphone_address_unref(addr);
	BC_ASSERT_EQUAL(linphone_proxy_config_is_server_config_changed(proxy_config), LinphoneProxyConfigAddressDifferent,
	                int, "%d");

	linphone_proxy_config_edit(proxy_config);
	linphone_proxy_config_set_server_addr(proxy_config, "sip:sip.linphone.org");
	linphone_proxy_config_done(proxy_config);
	linphone_proxy_config_edit(proxy_config);
	linphone_proxy_config_set_server_addr(proxy_config, "sip:toto.com");
	linphone_proxy_config_done(proxy_config);
	BC_ASSERT_EQUAL(linphone_proxy_config_is_server_config_changed(proxy_config), LinphoneProxyConfigAddressDifferent,
	                int, "%d");

	linphone_proxy_config_edit(proxy_config);
	linphone_proxy_config_set_server_addr(proxy_config, "sip:sip.linphone.org");
	linphone_proxy_config_done(proxy_config);
	linphone_proxy_config_edit(proxy_config);
	linphone_proxy_config_set_server_addr(proxy_config, "sip:sip.linphone.org:4444");
	linphone_proxy_config_done(proxy_config);
	BC_ASSERT_EQUAL(linphone_proxy_config_is_server_config_changed(proxy_config), LinphoneProxyConfigAddressDifferent,
	                int, "%d");

	linphone_proxy_config_edit(proxy_config);
	linphone_proxy_config_set_server_addr(proxy_config, "sip:sip.linphone.org");
	linphone_proxy_config_done(proxy_config);
	linphone_proxy_config_edit(proxy_config);
	linphone_proxy_config_set_server_addr(proxy_config, "sip:sip.linphone.org;transport=tcp");
	linphone_proxy_config_done(proxy_config);
	BC_ASSERT_EQUAL(linphone_proxy_config_is_server_config_changed(proxy_config), LinphoneProxyConfigAddressDifferent,
	                int, "%d");

	linphone_proxy_config_edit(proxy_config);
	linphone_proxy_config_set_server_addr(proxy_config, "sip:sip.linphone.org");
	linphone_proxy_config_done(proxy_config);
	linphone_proxy_config_edit(proxy_config);
	linphone_proxy_config_set_server_addr(proxy_config, "sip:sip.linphone.org;param=blue");
	linphone_proxy_config_done(proxy_config);
	BC_ASSERT_EQUAL(linphone_proxy_config_is_server_config_changed(proxy_config), LinphoneProxyConfigAddressEqual, int,
	                "%d");

	linphone_proxy_config_edit(proxy_config);
	linphone_proxy_config_set_contact_parameters(proxy_config, "blabla=blue");
	linphone_proxy_config_done(proxy_config);
	BC_ASSERT_EQUAL(linphone_proxy_config_is_server_config_changed(proxy_config), LinphoneProxyConfigAddressEqual, int,
	                "%d");

	linphone_proxy_config_edit(proxy_config);
	linphone_proxy_config_enable_register(proxy_config, TRUE);
	linphone_proxy_config_done(proxy_config);
	BC_ASSERT_EQUAL(linphone_proxy_config_is_server_config_changed(proxy_config), LinphoneProxyConfigAddressEqual, int,
	                "%d");

	linphone_proxy_config_unref(proxy_config);
}

static void chat_room_test(void) {
	LinphoneCore *lc;
	lc =
	    linphone_factory_create_core_3(linphone_factory_get(), NULL, liblinphone_tester_get_empty_rc(), system_context);
	if (!BC_ASSERT_PTR_NOT_NULL(lc)) return;
	linphone_config_set_int(linphone_core_get_config(lc), "lime", "enabled", 0);
	linphone_core_start(lc);
	BC_ASSERT_PTR_NOT_NULL(linphone_core_get_chat_room_from_uri(lc, "sip:toto@titi.com"));
	linphone_core_stop(lc);
	linphone_core_unref(lc);
}

static void devices_reload_test(void) {
	char *devid1;
	char *devid2;
	LinphoneCoreManager *mgr = linphone_core_manager_new_with_proxies_check("empty_rc", FALSE);

	devid1 = ms_strdup(linphone_core_get_capture_device(mgr->lc));
	linphone_core_reload_sound_devices(mgr->lc);
	devid2 = ms_strdup(linphone_core_get_capture_device(mgr->lc));
	if (devid1 && devid2) {
		BC_ASSERT_STRING_EQUAL(devid1, devid2);
	} else {
		BC_ASSERT_PTR_NULL(devid1);
		BC_ASSERT_PTR_NULL(devid2);
	}
	ms_free(devid1);
	ms_free(devid2);

	devid1 = ms_strdup(linphone_core_get_video_device(mgr->lc));
	linphone_core_reload_video_devices(mgr->lc);
	devid2 = ms_strdup(linphone_core_get_video_device(mgr->lc));

	if (devid1 && devid2) {
		BC_ASSERT_STRING_EQUAL(devid1, devid2);
	} else {
		BC_ASSERT_PTR_NULL(devid1);
		BC_ASSERT_PTR_NULL(devid2);
	}
	ms_free(devid1);
	ms_free(devid2);

	linphone_core_manager_destroy(mgr);
}

static void codec_usability_test(void) {
	LinphoneCoreManager *mgr = linphone_core_manager_new_with_proxies_check("empty_rc", FALSE);
	PayloadType *pt = linphone_core_find_payload_type(mgr->lc, "PCMU", 8000, -1);

	BC_ASSERT_PTR_NOT_NULL(pt);
	if (!pt) goto end;
	/*no limit*/
	linphone_core_set_upload_bandwidth(mgr->lc, 0);
	linphone_core_set_download_bandwidth(mgr->lc, 0);
	BC_ASSERT_TRUE(linphone_core_check_payload_type_usability(mgr->lc, pt));
	/*low limit*/
	linphone_core_set_upload_bandwidth(mgr->lc, 50);
	linphone_core_set_download_bandwidth(mgr->lc, 50);
	BC_ASSERT_FALSE(linphone_core_check_payload_type_usability(mgr->lc, pt));

	/*reasonable limit*/
	linphone_core_set_upload_bandwidth(mgr->lc, 200);
	linphone_core_set_download_bandwidth(mgr->lc, 200);
	BC_ASSERT_TRUE(linphone_core_check_payload_type_usability(mgr->lc, pt));

end:
	linphone_core_manager_destroy(mgr);
}

/*this test checks default codec list, assuming VP8 and H264 are both supported.
 * - with an empty config, the order must be as expected: VP8 first, H264 second.
 * - with a config that references only H264, VP8 must be added automatically as first codec.
 * - with a config that references only VP8, H264 must be added in second position.
 **/
static void codec_setup(void) {
	LinphoneCoreManager *mgr = linphone_core_manager_new_with_proxies_check("empty_rc", FALSE);
	PayloadType *vp8, *h264;
	const bctbx_list_t *codecs;
	if ((vp8 = linphone_core_find_payload_type(mgr->lc, "VP8", 90000, -1)) == NULL ||
	    (h264 = linphone_core_find_payload_type(mgr->lc, "H264", 90000, -1)) == NULL) {
		linphone_core_manager_destroy(mgr);
		ms_error("H264 or VP8 not available, test skipped.");
		BC_PASS("H264 or VP8 not available, test skipped.");
		return;
	}
	codecs = linphone_core_get_video_codecs(mgr->lc);
	BC_ASSERT_TRUE(bctbx_list_size(codecs) >= 2);
	BC_ASSERT_TRUE(codecs->data == vp8);
	BC_ASSERT_TRUE(codecs->next->data == h264);
	linphone_core_manager_destroy(mgr);

	mgr = linphone_core_manager_new_with_proxies_check("marie_h264_rc", FALSE);
	vp8 = linphone_core_find_payload_type(mgr->lc, "VP8", 90000, -1);
	h264 = linphone_core_find_payload_type(mgr->lc, "H264", 90000, -1);
	codecs = linphone_core_get_video_codecs(mgr->lc);
	BC_ASSERT_TRUE(bctbx_list_size(codecs) >= 2);
	BC_ASSERT_PTR_NOT_NULL(vp8);
	BC_ASSERT_PTR_NOT_NULL(h264);
	BC_ASSERT_TRUE(codecs->data == vp8);
	BC_ASSERT_TRUE(codecs->next->data == h264);
	linphone_core_manager_destroy(mgr);

	mgr = linphone_core_manager_new_with_proxies_check("marie_rc", FALSE);
	vp8 = linphone_core_find_payload_type(mgr->lc, "VP8", 90000, -1);
	h264 = linphone_core_find_payload_type(mgr->lc, "H264", 90000, -1);
	codecs = linphone_core_get_video_codecs(mgr->lc);
	BC_ASSERT_TRUE(bctbx_list_size(codecs) >= 2);
	BC_ASSERT_PTR_NOT_NULL(vp8);
	BC_ASSERT_PTR_NOT_NULL(h264);
	BC_ASSERT_TRUE(codecs->data == vp8);
	BC_ASSERT_TRUE(codecs->next->data == h264);
	linphone_core_manager_destroy(mgr);
}

static void custom_tones_setup(void) {
	LinphoneCoreManager *mgr = linphone_core_manager_new_with_proxies_check("empty_rc", FALSE);
	const char *tone;

	linphone_core_set_tone(mgr->lc, LinphoneToneCallOnHold, "callonhold.wav");
	tone = linphone_core_get_tone_file(mgr->lc, LinphoneToneCallOnHold);
	BC_ASSERT_PTR_NOT_NULL(tone);
	if (tone) {
		BC_ASSERT_STRING_EQUAL(tone, "callonhold.wav");
	}
	linphone_core_set_tone(mgr->lc, LinphoneToneCallOnHold, "callonhold2.wav");
	tone = linphone_core_get_tone_file(mgr->lc, LinphoneToneCallOnHold);
	BC_ASSERT_PTR_NOT_NULL(tone);
	if (tone) {
		BC_ASSERT_STRING_EQUAL(tone, "callonhold2.wav");
	}
	linphone_core_manager_destroy(mgr);
}

static void custom_tones_setup_before_start(void) {
	LinphoneCore *lc;
	const char *tone;

	lc =
	    linphone_factory_create_core_3(linphone_factory_get(), NULL, liblinphone_tester_get_empty_rc(), system_context);
	if (!BC_ASSERT_PTR_NOT_NULL(lc)) return;

	linphone_config_set_int(linphone_core_get_config(lc), "lime", "enabled", 0);
	BC_ASSERT_TRUE(linphone_core_get_global_state(lc) == LinphoneGlobalReady);

	linphone_core_set_tone(lc, LinphoneToneCallOnHold, "callonhold.wav");
	tone = linphone_core_get_tone_file(lc, LinphoneToneCallOnHold);
	BC_ASSERT_PTR_NOT_NULL(tone);
	if (tone) {
		BC_ASSERT_STRING_EQUAL(tone, "callonhold.wav");
	}
	linphone_core_set_tone(lc, LinphoneToneCallOnHold, "callonhold2.wav");
	tone = linphone_core_get_tone_file(lc, LinphoneToneCallOnHold);
	BC_ASSERT_PTR_NOT_NULL(tone);
	if (tone) {
		BC_ASSERT_STRING_EQUAL(tone, "callonhold2.wav");
	}
	linphone_core_start(lc);
	BC_ASSERT_TRUE(linphone_core_get_global_state(lc) == LinphoneGlobalOn);
	tone = linphone_core_get_tone_file(lc, LinphoneToneCallOnHold);
	BC_ASSERT_PTR_NOT_NULL(tone);
	if (tone) {
		BC_ASSERT_STRING_EQUAL(tone, "callonhold2.wav");
	}
	linphone_core_stop(lc);
	linphone_core_unref(lc);
}

/*the webrtc AEC implementation is brought to mediastreamer2 by a plugin.
 * We finally check here that if the plugin is correctly loaded and the right choice of echo canceller
 * implementation is made*/
static void echo_canceller_check(void) {
	LinphoneCoreManager *manager = linphone_core_manager_new_with_proxies_check("empty_rc", FALSE);
	MSFactory *factory = linphone_core_get_ms_factory(manager->lc);
	const char *expected_filter = "MSSpeexEC";
	AudioStream *as = audio_stream_new2(factory, NULL, 43000, 43001);
	const char *ec_filter = NULL;

	BC_ASSERT_PTR_NOT_NULL(as);
	if (as) {
		MSFilter *ecf = as->ec;
		BC_ASSERT_PTR_NOT_NULL(ecf);
		if (ecf) {
			ec_filter = ecf->desc->name;
		}
	}
	BC_ASSERT_PTR_NOT_NULL(ec_filter);

#if defined(__linux__) || (defined(__APPLE__) && !TARGET_OS_IPHONE) || defined(_WIN32) || defined(__ANDROID__)
	expected_filter = "MSWebRTCAEC";
#endif
	if (ec_filter) {
		BC_ASSERT_STRING_EQUAL(ec_filter, expected_filter);
	}
	audio_stream_stop(as);
	linphone_core_manager_destroy(manager);
}

extern LinphoneFriend *linphone_friend_new_from_config_file(LinphoneCore *lc, int index);
extern int linphone_friend_get_rc_index(const LinphoneFriend *lf);

static void delete_friend_from_rc(void) {
	LinphoneCoreManager *manager = linphone_core_manager_new_with_proxies_check("friends_rc", FALSE);
	LinphoneCore *core = manager->lc;
	LinphoneConfig *config = linphone_core_get_config(core);
	LinphoneFriendList *friend_list = linphone_core_get_default_friend_list(core);
	const bctbx_list_t *friends = linphone_friend_list_get_friends(friend_list);
	LinphoneFriend *francois = NULL;

	BC_ASSERT_PTR_NOT_NULL(friends);
	if (friends) {
		BC_ASSERT_EQUAL((int)bctbx_list_size(friends), 3, int, "%i");
		bctbx_list_t *it = NULL;
		int index = 2;
		for (it = (bctbx_list_t *)friends; it != NULL; it = bctbx_list_next(it)) {
			LinphoneFriend *friend = (LinphoneFriend *)bctbx_list_get_data(it);
			BC_ASSERT_EQUAL(linphone_friend_get_rc_index(friend), index, int, "%i");
			if (index == 1) {
				francois = linphone_friend_ref(friend);
			}
			index -= 1;
		}
	}

	LinphoneFriend *friend = linphone_core_create_friend_with_address(core, "sip:pauline@sip.linphone.org");
	BC_ASSERT_EQUAL(linphone_friend_get_rc_index(friend), -1, int, "%i");
	linphone_friend_list_add_friend(friend_list, friend);
	BC_ASSERT_EQUAL((int)bctbx_list_size(linphone_friend_list_get_friends(friend_list)), 4, int, "%i");
	BC_ASSERT_EQUAL(linphone_friend_get_rc_index(friend), -1, int, "%i");
	linphone_friend_list_remove_friend(friend_list, friend);
	BC_ASSERT_EQUAL((int)bctbx_list_size(linphone_friend_list_get_friends(friend_list)), 3, int, "%i");
	BC_ASSERT_EQUAL(linphone_friend_get_rc_index(friend), -1, int, "%i");
	linphone_friend_unref(friend);

	BC_ASSERT_PTR_NOT_NULL(francois);
	if (francois) {
		linphone_friend_remove(francois);
		BC_ASSERT_PTR_NULL(linphone_friend_get_friend_list(francois));
		const char *section = "friend_1";
		BC_ASSERT_EQUAL(linphone_config_has_section(config, section), 0, int, "%i");
		BC_ASSERT_PTR_NULL(linphone_friend_new_from_config_file(core, 1));
		BC_ASSERT_EQUAL((int)bctbx_list_size(linphone_friend_list_get_friends(friend_list)), 2, int, "%i");
		linphone_friend_unref(francois);
	}

	linphone_core_manager_destroy(manager);
}

static void friend_list_db_storage_base(bool_t set_friends_db_path) {
	LinphoneCoreManager *manager = linphone_core_manager_new("marie_rc");
	LinphoneCore *core = manager->lc;
	LinphoneConfig *config = linphone_core_get_config(core);

	// Disable legacy friends storage
	linphone_config_set_int(config, "misc", "store_friends", 0);

	char *friends_db = liblinphone_tester_make_unique_file_path("friends", "db");

	if (set_friends_db_path) {
		linphone_core_set_friends_database_path(core, friends_db);
	}

	LinphoneFriendList *default_fl = linphone_core_get_default_friend_list(core);
	BC_ASSERT_PTR_NOT_NULL(default_fl);
	if (default_fl) {
		BC_ASSERT_FALSE(linphone_friend_list_database_storage_enabled(default_fl));

		// Add a friend to that friend list that shoudln't be persisted in DB
		LinphoneFriend *claire_friend = linphone_core_create_friend(core);
		linphone_friend_set_name(claire_friend, "Claire");
		linphone_friend_add_phone_number(claire_friend, "+3366666666");
		LinphoneFriendListStatus status = linphone_friend_list_add_friend(default_fl, claire_friend);
		BC_ASSERT_EQUAL(status, LinphoneFriendListOK, int, "%d");
		linphone_friend_unref(claire_friend);
		ms_message("-> Claire added to default friend list");
	}

	LinphoneFriendList *db_stored_fl = linphone_core_create_friend_list(core);
	linphone_friend_list_set_display_name(db_stored_fl, "DB_STORED_FL");
	BC_ASSERT_FALSE(linphone_friend_list_database_storage_enabled(db_stored_fl));
	linphone_core_add_friend_list(core, db_stored_fl);
	BC_ASSERT_FALSE(linphone_friend_list_database_storage_enabled(db_stored_fl));

	// Adding a friend while DB storage is still not enabled
	LinphoneFriend *pauline_friend = linphone_core_create_friend(core);
	linphone_friend_set_name(pauline_friend, "Pauline");
	linphone_friend_add_phone_number(pauline_friend, "+3301020304");
	LinphoneFriendListStatus status = linphone_friend_list_add_friend(db_stored_fl, pauline_friend);
	BC_ASSERT_EQUAL(status, LinphoneFriendListOK, int, "%d");
	linphone_friend_unref(pauline_friend);
	ms_message("-> Pauline added to db stored friend list");

	linphone_friend_list_enable_database_storage(db_stored_fl, TRUE);
	BC_ASSERT_TRUE(linphone_friend_list_database_storage_enabled(db_stored_fl));

	// Adding a new friend now that DB is enabled
	LinphoneFriend *marie_friend = linphone_core_create_friend(core);
	linphone_friend_set_name(marie_friend, "Marie");
	linphone_friend_add_phone_number(marie_friend, "+3305060708");
	status = linphone_friend_list_add_friend(db_stored_fl, marie_friend);
	BC_ASSERT_EQUAL(status, LinphoneFriendListOK, int, "%d");
	linphone_friend_unref(marie_friend);
	ms_message("-> Marie added to db stored friend list");

	linphone_friend_list_unref(db_stored_fl);

	// Add a new friend list without DB storage
	LinphoneFriendList *not_db_stored_fl = linphone_core_create_friend_list(core);
	linphone_friend_list_set_display_name(not_db_stored_fl, "NOT_DB_STORED_FL");
	BC_ASSERT_FALSE(linphone_friend_list_database_storage_enabled(not_db_stored_fl));
	linphone_core_add_friend_list(core, not_db_stored_fl);

	// Add a friend to that friend list that shoudln't be persisted in DB
	LinphoneFriend *laure_friend = linphone_core_create_friend(core);
	linphone_friend_set_name(laure_friend, "Laure");
	linphone_friend_add_phone_number(laure_friend, "+3312345678");
	status = linphone_friend_list_add_friend(not_db_stored_fl, laure_friend);
	BC_ASSERT_EQUAL(status, LinphoneFriendListOK, int, "%d");
	linphone_friend_unref(laure_friend);
	ms_message("-> Laure added to memory cached friend list");

	linphone_friend_list_unref(not_db_stored_fl);

	// Check that both friends list can be found using display name
	LinphoneFriendList *found_list = linphone_core_get_friend_list_by_name(core, "DB_STORED_FL");
	BC_ASSERT_PTR_NOT_NULL(found_list);

	found_list = linphone_core_get_friend_list_by_name(core, "NOT_DB_STORED_FL");
	BC_ASSERT_PTR_NOT_NULL(found_list);

	// Check that all friends can be found by Core
	LinphoneFriend *found_friend = linphone_core_find_friend_by_phone_number(core, "+3366666666"); // Claire
	BC_ASSERT_PTR_NOT_NULL(found_friend);
	found_friend = linphone_core_find_friend_by_phone_number(core, "+3301020304"); // Pauline
	BC_ASSERT_PTR_NOT_NULL(found_friend);
	found_friend = linphone_core_find_friend_by_phone_number(core, "+3305060708"); // Marie
	BC_ASSERT_PTR_NOT_NULL(found_friend);
	found_friend = linphone_core_find_friend_by_phone_number(core, "+3312345678"); // Laure
	BC_ASSERT_PTR_NOT_NULL(found_friend);

	// Now restart the Core
	ms_message("\n-> Restarting Core\n");
	linphone_core_manager_reinit(manager);
	linphone_core_manager_start(manager, TRUE);
	core = manager->lc;

	// Don't forget to set the friends DB path again if asked for it
	if (set_friends_db_path) {
		linphone_core_set_friends_database_path(core, friends_db);
	}

	// Check that only friends list stored in can be found using display name
	found_list = linphone_core_get_friend_list_by_name(core, "DB_STORED_FL");
	BC_ASSERT_PTR_NOT_NULL(found_list);

	found_list = linphone_core_get_friend_list_by_name(core, "NOT_DB_STORED_FL");
	BC_ASSERT_PTR_NULL(found_list);

	// Check that only friends in lists that were stored in DB can be found by Core
	found_friend = linphone_core_find_friend_by_phone_number(core, "+3366666666"); // Claire
	BC_ASSERT_PTR_NULL(found_friend);

	found_friend = linphone_core_find_friend_by_phone_number(core, "+3301020304"); // Pauline
	BC_ASSERT_PTR_NOT_NULL(found_friend);

	found_friend = linphone_core_find_friend_by_phone_number(core, "+3305060708"); // Marie
	BC_ASSERT_PTR_NOT_NULL(found_friend);

	found_friend = linphone_core_find_friend_by_phone_number(core, "+3312345678"); // Laure
	BC_ASSERT_PTR_NULL(found_friend);

	unlink(friends_db);
	bctbx_free(friends_db);

	linphone_core_manager_destroy(manager);
}

static void friend_list_db_storage(void) {
	friend_list_db_storage_base(TRUE);
}

static void friend_list_db_storage_without_db(void) {
	friend_list_db_storage_base(FALSE);
}

static void dial_plan(void) {
	bctbx_list_t *dial_plans = linphone_dial_plan_get_all_list();
	bctbx_list_t *it;
	for (it = dial_plans; it != NULL; it = it->next) {
		const LinphoneDialPlan *dialplan = (LinphoneDialPlan *)it->data;
		belle_sip_object_remove_from_leak_detector((void *)dialplan);
		char *e164 = generate_random_e164_phone_from_dial_plan(dialplan);
		if (BC_ASSERT_PTR_NOT_NULL(e164)) {
			const char *calling_code = linphone_dial_plan_get_country_calling_code(dialplan);
			BC_ASSERT_EQUAL((int)(strlen(e164) - strlen(calling_code) - 1),
			                linphone_dial_plan_get_national_number_length(dialplan), int, "%i");
			BC_ASSERT_EQUAL(linphone_dial_plan_lookup_ccc_from_e164(e164), (int)strtol(calling_code, NULL, 10), int,
			                "%i");
			ms_free(e164);
		} else {
			ms_error("cannot generate e164 number for [%s]", linphone_dial_plan_get_country(dialplan));
		}
	}
	bctbx_list_free_with_data(dial_plans, (bctbx_list_free_func)linphone_dial_plan_unref);
}

static void friend_phone_number_lookup_without_plus(void) {
	LinphoneCoreManager *manager = linphone_core_manager_new("marie_rc");
	LinphoneCore *core = manager->lc;

	LinphoneFriend *lf = linphone_core_create_friend(core);
	linphone_friend_set_name(lf, "Test Number");
	linphone_friend_add_phone_number(lf, "+4912345678901");
	linphone_core_add_friend(core, lf);
	linphone_friend_unref(lf);

	LinphoneFriend *found = linphone_core_find_friend_by_phone_number(core, "4912345678901");
	BC_ASSERT_PTR_NULL(found);

	const bctbx_list_t *accounts = linphone_core_get_account_list(core);
	LinphoneAccount *account = (LinphoneAccount *)bctbx_list_get_data(accounts);
	const LinphoneAccountParams *params = linphone_account_get_params(account);
	LinphoneAccountParams *cloned_params = linphone_account_params_clone(params);
	linphone_account_params_set_international_prefix(cloned_params, "49");
	linphone_account_set_params(account, cloned_params);
	linphone_account_params_unref(cloned_params);

	found = linphone_core_find_friend_by_phone_number(core, "12345678901");
	BC_ASSERT_PTR_NOT_NULL(found);

	linphone_core_manager_destroy(manager);
}

static void audio_devices(void) {
	LinphoneCoreManager *manager = linphone_core_manager_new("marie_rc");
	LinphoneCore *core = manager->lc;

	bctbx_list_t *sound_devices = linphone_core_get_sound_devices_list(core);
	int sound_devices_count = (int)bctbx_list_size(sound_devices);
	BC_ASSERT_GREATER_STRICT(sound_devices_count, 0, int, "%d");
	bctbx_list_free_with_data(sound_devices, (bctbx_list_free_func)bctbx_free);

	// Check extended audio devices list matches legacy sound devices list
	bctbx_list_t *audio_devices = linphone_core_get_extended_audio_devices(core);
	int audio_devices_count = (int)bctbx_list_size(audio_devices);
	BC_ASSERT_EQUAL(audio_devices_count, sound_devices_count, int, "%d");
	bctbx_list_free_with_data(audio_devices, (void (*)(void *))linphone_audio_device_unref);

	// Check legacy sound card selection matches new audio devices API
	const char *capture_device = linphone_core_get_capture_device(core);
	BC_ASSERT_PTR_NOT_NULL(capture_device);
	if (capture_device) {
		const LinphoneAudioDevice *input_device = linphone_core_get_default_input_audio_device(core);
		BC_ASSERT_PTR_NOT_NULL(input_device);
		if (input_device) {
			BC_ASSERT_STRING_EQUAL(linphone_audio_device_get_id(input_device), capture_device);
		}
	}

	// Check legacy sound card selection matches new audio devices API
	const char *playback_device = linphone_core_get_playback_device(core);
	BC_ASSERT_PTR_NOT_NULL(playback_device);
	if (playback_device) {
		const LinphoneAudioDevice *output_device = linphone_core_get_default_output_audio_device(core);
		BC_ASSERT_PTR_NOT_NULL(output_device);
		if (output_device) {
			BC_ASSERT_STRING_EQUAL(linphone_audio_device_get_id(output_device), playback_device);
		}
	}

	// We are not in call so there is no current input audio device
	BC_ASSERT_PTR_NULL(linphone_core_get_input_audio_device(core));
	BC_ASSERT_PTR_NULL(linphone_core_get_output_audio_device(core));

	// Check that devices list is empty as the current one type is UNKNOWN
	audio_devices = linphone_core_get_audio_devices(core);
	audio_devices_count = (int)bctbx_list_size(audio_devices);
	BC_ASSERT_EQUAL(audio_devices_count, 0, int, "%d");
	bctbx_list_free_with_data(audio_devices, (void (*)(void *))linphone_audio_device_unref);

	// Let's add a new sound card and check it appears correctly in audio devices list
	MSFactory *factory = linphone_core_get_ms_factory(core);
	MSSndCardManager *sndcard_manager = ms_factory_get_snd_card_manager(factory);
	ms_snd_card_manager_register_desc(sndcard_manager, &dummy_test_snd_card_desc);
	linphone_core_reload_sound_devices(core);
	BC_ASSERT_EQUAL(manager->stat.number_of_LinphoneCoreAudioDevicesListUpdated, 1, int, "%d");

	audio_devices = linphone_core_get_extended_audio_devices(core);
	audio_devices_count = (int)bctbx_list_size(audio_devices);
	BC_ASSERT_EQUAL(audio_devices_count, sound_devices_count + 1, int, "%d");
	LinphoneAudioDevice *audio_device = (LinphoneAudioDevice *)bctbx_list_get_data(audio_devices);
	BC_ASSERT_PTR_NOT_NULL(audio_device);
	if (!audio_device) {
		goto end;
	}

	// Check the Audio Device object has correct values
	linphone_audio_device_ref(audio_device);
	bctbx_list_free_with_data(audio_devices, (void (*)(void *))linphone_audio_device_unref);
	BC_ASSERT_EQUAL(linphone_audio_device_get_type(audio_device), LinphoneAudioDeviceTypeBluetooth, int, "%d");
	BC_ASSERT_TRUE(linphone_audio_device_has_capability(audio_device, LinphoneAudioDeviceCapabilityPlay));
	BC_ASSERT_TRUE(linphone_audio_device_has_capability(audio_device, LinphoneAudioDeviceCapabilityRecord));
	BC_ASSERT_STRING_EQUAL(linphone_audio_device_get_device_name(audio_device), DUMMY_TEST_SOUNDCARD);

	// Check that device is in the audio devices list
	audio_devices = linphone_core_get_audio_devices(core);
	audio_devices_count = (int)bctbx_list_size(audio_devices);
	BC_ASSERT_EQUAL(audio_devices_count, 1, int, "%d");
	bctbx_list_free_with_data(audio_devices, (void (*)(void *))linphone_audio_device_unref);

	// Check that we can change the default audio device
	const LinphoneAudioDevice *input_device = linphone_core_get_default_input_audio_device(core);
	BC_ASSERT_PTR_NOT_NULL(input_device);
	if (input_device) {
		BC_ASSERT_PTR_NOT_EQUAL(audio_device, input_device);
	}
	linphone_core_set_default_input_audio_device(core, audio_device);
	input_device = linphone_core_get_default_input_audio_device(core);
	BC_ASSERT_PTR_NOT_NULL(input_device);
	if (input_device) {
		BC_ASSERT_PTR_EQUAL(audio_device, input_device);
	}

	const LinphoneAudioDevice *output_device = linphone_core_get_default_output_audio_device(core);
	BC_ASSERT_PTR_NOT_NULL(output_device);
	if (output_device) {
		BC_ASSERT_PTR_NOT_EQUAL(audio_device, output_device);
	}
	linphone_core_set_default_output_audio_device(core, audio_device);
	output_device = linphone_core_get_default_output_audio_device(core);
	BC_ASSERT_PTR_NOT_NULL(output_device);
	if (output_device) {
		BC_ASSERT_PTR_EQUAL(audio_device, output_device);
	}

	// We are not in call so this should do nothing
	linphone_core_set_input_audio_device(core, audio_device);
	BC_ASSERT_EQUAL(manager->stat.number_of_LinphoneCoreAudioDeviceChanged, 0, int, "%d");
	linphone_core_set_output_audio_device(core, audio_device);
	BC_ASSERT_EQUAL(manager->stat.number_of_LinphoneCoreAudioDeviceChanged, 0, int, "%d");

	// Let's add another bluetooth sound card
	ms_snd_card_manager_register_desc(sndcard_manager, &dummy2_test_snd_card_desc);
	linphone_core_reload_sound_devices(core);
	BC_ASSERT_EQUAL(manager->stat.number_of_LinphoneCoreAudioDevicesListUpdated, 2, int, "%d");

	// Check that device is in the extended audio devices list
	audio_devices = linphone_core_get_extended_audio_devices(core);
	audio_devices_count = (int)bctbx_list_size(audio_devices);
	BC_ASSERT_EQUAL(audio_devices_count, sound_devices_count + 2, int, "%d");
	bctbx_list_free_with_data(audio_devices, (void (*)(void *))linphone_audio_device_unref);

	// Check that device is not in the simple audio devices list as we already have a bluetooth audio device
	audio_devices = linphone_core_get_audio_devices(core);
	audio_devices_count = (int)bctbx_list_size(audio_devices);
	BC_ASSERT_EQUAL(audio_devices_count, 1, int, "%d");
	bctbx_list_free_with_data(audio_devices, (void (*)(void *))linphone_audio_device_unref);

	ms_snd_card_manager_unregister_desc(sndcard_manager, &dummy_test_snd_card_desc);
	linphone_core_reload_sound_devices(core);
	BC_ASSERT_EQUAL(manager->stat.number_of_LinphoneCoreAudioDevicesListUpdated, 3, int, "%d");

	// Check that device is no longer in the extended audio devices list
	audio_devices = linphone_core_get_extended_audio_devices(core);
	audio_devices_count = (int)bctbx_list_size(audio_devices);
	BC_ASSERT_EQUAL(audio_devices_count, sound_devices_count + 1, int, "%d");
	bctbx_list_free_with_data(audio_devices, (void (*)(void *))linphone_audio_device_unref);

	// Check that the device we removed is no longer the default
	input_device = linphone_core_get_default_input_audio_device(core);
	BC_ASSERT_PTR_NOT_NULL(input_device);
	if (input_device) {
		BC_ASSERT_STRING_NOT_EQUAL(linphone_audio_device_get_device_name(input_device), DUMMY_TEST_SOUNDCARD);
		MSSndCard *sndcard = ms_snd_card_manager_get_default_capture_card(sndcard_manager);
		BC_ASSERT_STRING_EQUAL(linphone_audio_device_get_device_name(input_device), ms_snd_card_get_name(sndcard));
	}
	output_device = linphone_core_get_default_output_audio_device(core);
	BC_ASSERT_PTR_NOT_NULL(output_device);
	if (output_device) {
		BC_ASSERT_STRING_NOT_EQUAL(linphone_audio_device_get_device_name(output_device), DUMMY_TEST_SOUNDCARD);
		MSSndCard *sndcard = ms_snd_card_manager_get_default_playback_card(sndcard_manager);
		BC_ASSERT_STRING_EQUAL(linphone_audio_device_get_device_name(input_device), ms_snd_card_get_name(sndcard));
	}

	linphone_audio_device_unref(audio_device);
end:
	linphone_core_manager_destroy(manager);
}

static void migration_from_call_history_db(void) {
	if (!linphone_factory_is_database_storage_available(linphone_factory_get())) {
		ms_warning("Test skipped, database storage is not available");
		return;
	}

	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc_call_logs_migration");
	char *src_db = bc_tester_res("db/call-history.db");
	char *tmp_db = bc_tester_file("tmp.db");

	BC_ASSERT_EQUAL(liblinphone_tester_copy_file(src_db, tmp_db), 0, int, "%d");

	// The call_history.db has 600+ calls with the very first DB scheme.
	// This will test the migration procedure
	linphone_core_set_call_logs_database_path(marie->lc, tmp_db);

	const bctbx_list_t *call_logs = linphone_core_get_call_history(marie->lc);
	BC_ASSERT(bctbx_list_size(call_logs) > 0);

	linphone_core_manager_destroy(marie);
	remove(tmp_db);
	bctbx_free(src_db);
	bctbx_free(tmp_db);
}

test_t setup_tests[] = {
    TEST_NO_TAG("Version check", linphone_version_test),
    TEST_NO_TAG("Version update check", linphone_version_update_test),
    TEST_NO_TAG("Linphone Address", linphone_address_test),
    TEST_NO_TAG("Linphone proxy config address equal (internal api)", linphone_proxy_config_address_equal_test),
    TEST_NO_TAG("Linphone proxy config server address change (internal api)",
                linphone_proxy_config_is_server_config_changed_test),
    TEST_NO_TAG("Linphone core init/uninit", core_init_test),
    TEST_NO_TAG("Linphone core init/uninit with some database", core_init_test_some_database),
    TEST_NO_TAG("Linphone core init/uninit without database", core_init_test_no_database),
    TEST_NO_TAG("Linphone core init/uninit from existing factory rc", core_init_test_2),
    TEST_NO_TAG("Linphone core init/uninit withtout any rc", core_init_test_3),
    TEST_NO_TAG("Linphone core init/uninit from existing default rc", core_init_test_4),
    TEST_NO_TAG("Linphone core init/stop/uninit", core_init_stop_test),
    TEST_NO_TAG("Linphone core init/unref", core_init_unref_test),
    TEST_NO_TAG("Linphone core init/stop/start/uninit", core_init_stop_start_test),
    TEST_NO_TAG("Linphone core set user agent", core_set_user_agent),
    TEST_NO_TAG("Linphone random transport port", core_sip_transport_test),
    TEST_NO_TAG("Linphone interpret url", linphone_interpret_url_test),
    TEST_NO_TAG("LPConfig safety test", linphone_config_safety_test),
    TEST_NO_TAG("LPConfig from buffer", linphone_lpconfig_from_buffer),
    TEST_NO_TAG("LPConfig zero_len value from buffer", linphone_lpconfig_from_buffer_zerolen_value),
    TEST_NO_TAG("LPConfig zero_len value from file", linphone_lpconfig_from_file_zerolen_value),
    TEST_NO_TAG("LPConfig zero_len value from XML", linphone_lpconfig_from_xml_zerolen_value),
    TEST_NO_TAG("LPConfig invalid friend", linphone_lpconfig_invalid_friend),
    TEST_NO_TAG("LPConfig invalid friend remote provisoning", linphone_lpconfig_invalid_friend_remote_provisioning),
    TEST_NO_TAG("Chat room", chat_room_test),
    TEST_NO_TAG("Devices reload", devices_reload_test),
    TEST_NO_TAG("Codec usability", codec_usability_test),
    TEST_NO_TAG("Codec setup", codec_setup),
    TEST_NO_TAG("Custom tones setup", custom_tones_setup),
    TEST_NO_TAG("Custom tones setup before start", custom_tones_setup_before_start),
    TEST_NO_TAG("Appropriate software echo canceller check", echo_canceller_check),
    TEST_NO_TAG("Delete friend in linphone rc", delete_friend_from_rc),
    TEST_NO_TAG("Store friends list in DB", friend_list_db_storage),
    TEST_NO_TAG("Store friends list in DB without setting path to db file", friend_list_db_storage_without_db),
    TEST_NO_TAG("Dialplan", dial_plan),
    TEST_NO_TAG("Friend phone number lookup without plus", friend_phone_number_lookup_without_plus),
    TEST_NO_TAG("Audio devices", audio_devices),
    TEST_NO_TAG("Migrate from call history database", migration_from_call_history_db),
};

test_suite_t setup_test_suite = {"Setup",
                                 NULL,
                                 NULL,
                                 liblinphone_tester_before_each,
                                 liblinphone_tester_after_each,
                                 sizeof(setup_tests) / sizeof(setup_tests[0]),
                                 setup_tests,
                                 0};

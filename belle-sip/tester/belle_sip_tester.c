/*
 * Copyright (c) 2012-2019 Belledonne Communications SARL.
 *
 * This file is part of belle-sip.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "bctoolbox/tester.h"
#include "belle_sip_tester.h"
#include "belle_sip_tester_utils.h"
#include "port.h"

/*
 * This file is the main() entry point for belle-sip-tester UNIX executable.
 */

#if !defined(__ANDROID__) && !defined(TARGET_OS_IPHONE) &&                                                             \
    !(defined(BELLE_SIP_WINDOWS_PHONE) || defined(BELLE_SIP_WINDOWS_UNIVERSAL))

static const char *belle_sip_helper =
    "\t\t\t--domain <test sip domain>\n"
    "\t\t\t--auth-domain <test auth domain>\n"
    "\t\t\t--root-ca <root ca file path>\n"
    "\t\t\t--dns-hosts </etc/hosts -like file to used to override DNS names (default: tester_hosts)>\n";

int main(int argc, char *argv[]) {
	int i;
	int ret;
	const char *root_ca_path = NULL;
	const char *env_domain = getenv("TEST_DOMAIN");
	char *default_hosts = NULL;

	belle_sip_tester_init(NULL);

#ifdef HAVE_CONFIG_H
	// If the tester is not installed we configure it, so it can be launched without installing
	if (!belle_sip_is_executable_installed(argv[0], "afl/sip_dict.txt")) {
		bc_tester_set_resource_dir_prefix(BELLE_SIP_LOCAL_RESOURCE_LOCATION);
		printf("Resource dir set to %s\n", BELLE_SIP_LOCAL_RESOURCE_LOCATION);

		belle_sip_add_belr_grammar_search_path(SDP_LOCAL_GRAMMAR_LOCATION);
	}
#endif

#ifndef _WIN32 /*this hack doesn't work for argv[0]="c:\blablab\"*/
	// this allows to launch liblinphone_tester from outside of tester directory
	if (strstr(argv[0], ".libs")) {
		int prefix_length = strstr(argv[0], ".libs") - argv[0] + 1;
		char *prefix = belle_sip_strdup_printf("%s%.*s", argv[0][0] == '/' ? "" : "./", prefix_length, argv[0]);
		// printf("Resource prefix set to %s\n", prefix);
		bc_tester_set_resource_dir_prefix(prefix);
		bc_tester_set_writable_dir_prefix(prefix);
		belle_sip_free(prefix);
	}
#endif

	if (env_domain) {
		belle_sip_tester_set_test_domain(env_domain);
	}
	bctbx_init_logger(TRUE);

	for (i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--domain") == 0) {
			CHECK_ARG("--domain", ++i, argc);
			belle_sip_tester_set_test_domain(argv[i]);
		} else if (strcmp(argv[i], "--auth-domain") == 0) {
			CHECK_ARG("--auth-domain", ++i, argc);
			belle_sip_tester_set_auth_domain(argv[i]);
		} else if (strcmp(argv[i], "--root-ca") == 0) {
			CHECK_ARG("--root-ca", ++i, argc);
			root_ca_path = argv[i];
		} else if (strcmp(argv[i], "--dns-hosts") == 0) {
			CHECK_ARG("--dns-hosts", ++i, argc);
			belle_sip_tester_set_userhostsfile(argv[i]);
		} else {
			ret = bc_tester_parse_args(argc, argv, i);
			if (ret > 0) {
				i += ret - 1;
				continue;
			} else if (ret < 0) {
				bc_tester_helper(argv[0], belle_sip_helper);
			}
			return ret;
		}
	}
	belle_sip_tester_set_root_ca_path(root_ca_path);

	ret = bc_tester_start(argv[0]);
	belle_sip_tester_uninit();
	bctbx_uninit_logger();
	if (default_hosts) bc_free(default_hosts);
	return ret;
}

#endif

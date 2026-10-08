/*
 * Copyright (c) 2024 Belledonne Communications SARL.
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

#include "bctoolbox/tester.h"
#include "belle-sip/belle-sip.h"
#include "belle_sip_test_ua.hh"
#include "belle_sip_tester.h"
#include "belle_sip_tester_utils.h"

using namespace bellesip;

static void register_and_unregister(const std::string &transport) {
	BasicRegistrar registrar("127.0.0.1", transport);
	TestUA ua(transport);

	ua.sendRegister(registrar.getAgent().getListeningUriAsString());
	BC_ASSERT_TRUE(ua.waitCountOf(&ua.mNumberOf2xxResponse, 1, 5000));

	ua.unregister();
	BC_ASSERT_TRUE(ua.waitCountOf(&ua.mNumberOf2xxResponse, 2, 5000));
}

static void register_and_unregister_udp(void) {
	register_and_unregister("UDP");
}

static void register_and_unregister_tcp(void) {
	register_and_unregister("TCP");
}

static void register_with_srv(const std::string &transport) {
	std::string domain = "sip.example.org";
	std::string srvTarget = "server1.example.org";
	BasicRegistrar registrar(domain, transport);
	int regPort = belle_sip_uri_get_port(registrar.getAgent().getListeningUri());
	TestUA ua(transport);

	belle_sip_stack_add_dns_srv_entry(ua.getStack(), "sip", transport.c_str(), domain.c_str(), srvTarget.c_str(),
	                                  regPort, 0, 0, 600);
	belle_sip_stack_add_dns_a_entry(ua.getStack(), srvTarget.c_str(), "127.0.0.1", 600);

	std::string destUri = "sip:" + domain + ";transport=" + transport;
	ua.sendRegister(destUri);
	BC_ASSERT_TRUE(ua.waitCountOf(&ua.mNumberOf2xxResponse, 1, 5000));

	ua.unregister();
	BC_ASSERT_TRUE(ua.waitCountOf(&ua.mNumberOf2xxResponse, 2, 5000));
}

static int get_unused_port(void) {
	belle_sip_socket_t sock = bctbx_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	struct sockaddr_in sin;
	socklen_t len = sizeof(sin);
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sin.sin_port = 0;
	bctbx_bind(sock, (struct sockaddr *)&sin, len);
	bctbx_getsockname(sock, (struct sockaddr *)&sin, &len);
	int port = ntohs(sin.sin_port);
	bctbx_socket_close(sock);
	return port;
}

static void register_failover_srv_tcp(void) {
	std::string domain = "sip.example.org";
	std::string primaryTarget = "primary.example.org";
	std::string secondaryTarget = "secondary.example.org";
	BasicRegistrar registrar(domain, "TCP");
	int regPort = belle_sip_uri_get_port(registrar.getAgent().getListeningUri());
	int badPort = get_unused_port();
	TestUA ua("TCP");

	// Primary SRV record points to badPort (prio 0 = highest priority)
	belle_sip_stack_add_dns_srv_entry(ua.getStack(), "sip", "tcp", domain.c_str(), primaryTarget.c_str(), badPort, 0, 0,
	                                  600);
	// Secondary SRV record points to registrar port (prio 10 = lower priority)
	belle_sip_stack_add_dns_srv_entry(ua.getStack(), "sip", "tcp", domain.c_str(), secondaryTarget.c_str(), regPort, 10,
	                                  0, 600);

	belle_sip_stack_add_dns_a_entry(ua.getStack(), primaryTarget.c_str(), "127.0.0.1", 600);
	belle_sip_stack_add_dns_a_entry(ua.getStack(), secondaryTarget.c_str(), "127.0.0.1", 600);

	std::string destUri = "sip:" + domain + ";transport=TCP";
	ua.sendRegister(destUri);
	BC_ASSERT_TRUE(ua.waitCountOf(&ua.mNumberOf2xxResponse, 1, 5000));

	ua.unregister();
	BC_ASSERT_TRUE(ua.waitCountOf(&ua.mNumberOf2xxResponse, 2, 5000));
}

static void register_with_srv_udp(void) {
	register_with_srv("UDP");
}

static void register_with_srv_tcp(void) {
	register_with_srv("TCP");
}

static test_t refresher_failover_tests[] = {
    TEST_NO_TAG("Register and unregister UDP", register_and_unregister_udp),
    TEST_NO_TAG("Register and unregister TCP", register_and_unregister_tcp),
    TEST_NO_TAG("Register with SRV UDP", register_with_srv_udp),
    TEST_NO_TAG("Register with SRV TCP", register_with_srv_tcp),
    TEST_NO_TAG("Register failover SRV TCP", register_failover_srv_tcp),
};

test_suite_t refresher_failover_test_suite = {"Refresher failover",
                                              NULL,
                                              NULL,
                                              belle_sip_tester_before_each,
                                              belle_sip_tester_after_each,
                                              sizeof(refresher_failover_tests) / sizeof(refresher_failover_tests[0]),
                                              refresher_failover_tests,
                                              0,
                                              0};

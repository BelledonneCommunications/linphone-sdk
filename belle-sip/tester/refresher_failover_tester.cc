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

static void register_retry_exponential_backoff_on_503(void) {
	std::string transport = "TCP";
	BasicRegistrar registrar("127.0.0.1", transport);
	registrar.setServiceUnavailable(true);
	TestUA ua(transport);

	// base delay = 1000ms, max delay = 4000ms
	belle_sip_stack_set_register_retry_backoff(ua.getStack(), 1000, 4000);

	ua.sendRegister(registrar.getAgent().getListeningUriAsString());

	// Initial REGISTER should fail immediately with 503
	BC_ASSERT_TRUE(ua.waitCountOf(&ua.mNumberOf5xxResponse, 1, 2000));
	uint64_t t1 = bctbx_get_cur_time_ms();

	// 1st retry: base = 1000ms, with jitter [750, 1250]ms
	BC_ASSERT_TRUE(ua.waitCountOf(&ua.mNumberOf5xxResponse, 2, 3000));
	uint64_t t2 = bctbx_get_cur_time_ms();
	uint64_t delay1 = t2 - t1;
	belle_sip_message("Exponential backoff retry 1 delay: %llu ms", (unsigned long long)delay1);
	BC_ASSERT_GREATER((int)delay1, 700, int, "%d");
	BC_ASSERT_LOWER((int)delay1, 1600, int, "%d");

	// 2nd retry: base*2 = 2000ms, with jitter [1500, 2500]ms
	BC_ASSERT_TRUE(ua.waitCountOf(&ua.mNumberOf5xxResponse, 3, 4000));
	uint64_t t3 = bctbx_get_cur_time_ms();
	uint64_t delay2 = t3 - t2;
	belle_sip_message("Exponential backoff retry 2 delay: %llu ms", (unsigned long long)delay2);
	BC_ASSERT_GREATER((int)delay2, 1400, int, "%d");
	BC_ASSERT_LOWER((int)delay2, 3000, int, "%d");
	BC_ASSERT_GREATER((int)delay2, (int)delay1, int, "%d");

	BC_ASSERT_EQUAL(registrar.getRequestCount(), 3, int, "%d");
}

static void register_retry_backoff_with_retry_after(void) {
	std::string transport = "TCP";
	BasicRegistrar registrar("127.0.0.1", transport);
	// Server returns 503 with Retry-After: 2 (seconds)
	registrar.setServiceUnavailable(true, 2);
	TestUA ua(transport);

	// Backoff base is 500ms, but Retry-After: 2s should take precedence!
	belle_sip_stack_set_register_retry_backoff(ua.getStack(), 500, 4000);

	ua.sendRegister(registrar.getAgent().getListeningUriAsString());

	BC_ASSERT_TRUE(ua.waitCountOf(&ua.mNumberOf5xxResponse, 1, 2000));
	uint64_t t1 = bctbx_get_cur_time_ms();

	// At 800ms, the retry must NOT have fired yet (if 500ms backoff had taken precedence, it would have fired)
	BC_ASSERT_FALSE(ua.waitCountOf(&ua.mNumberOf5xxResponse, 2, 800));

	// At ~2000ms, Retry-After expires and 2nd attempt occurs
	BC_ASSERT_TRUE(ua.waitCountOf(&ua.mNumberOf5xxResponse, 2, 3000));
	uint64_t t2 = bctbx_get_cur_time_ms();
	uint64_t delay = t2 - t1;
	belle_sip_message("Retry-After delay: %llu ms", (unsigned long long)delay);
	BC_ASSERT_GREATER((int)delay, 1800, int, "%d");
}

static void register_retry_backoff_after_srv_exhaustion(void) {
	std::string domain = "sip.example.org";
	std::string primaryTarget = "primary.example.org";
	std::string secondaryTarget = "secondary.example.org";
	BasicRegistrar reg1(domain, "TCP");
	BasicRegistrar reg2(domain, "TCP");
	int port1 = belle_sip_uri_get_port(reg1.getAgent().getListeningUri());
	int port2 = belle_sip_uri_get_port(reg2.getAgent().getListeningUri());
	reg1.setServiceUnavailable(true);
	reg2.setServiceUnavailable(true);

	TestUA ua("TCP");
	belle_sip_stack_set_register_retry_backoff(ua.getStack(), 1500, 6000);

	// Primary target (prio 0), Secondary target (prio 10)
	belle_sip_stack_add_dns_srv_entry(ua.getStack(), "sip", "tcp", domain.c_str(), primaryTarget.c_str(), port1, 0, 0,
	                                  600);
	belle_sip_stack_add_dns_srv_entry(ua.getStack(), "sip", "tcp", domain.c_str(), secondaryTarget.c_str(), port2, 10,
	                                  0, 600);
	belle_sip_stack_add_dns_a_entry(ua.getStack(), primaryTarget.c_str(), "127.0.0.1", 600);
	belle_sip_stack_add_dns_a_entry(ua.getStack(), secondaryTarget.c_str(), "127.0.0.1", 600);

	uint64_t t0 = bctbx_get_cur_time_ms();
	std::string destUri = "sip:" + domain + ";transport=TCP";
	ua.sendRegister(destUri);

	// Both SRV targets are tried immediately in the initial cycle
	BC_ASSERT_TRUE(ua.waitCountOf(&ua.mNumberOf5xxResponse, 2, 3000));
	uint64_t t1 = bctbx_get_cur_time_ms();
	uint64_t srvDescentTime = t1 - t0;
	belle_sip_message("SRV descent time across targets: %llu ms", (unsigned long long)srvDescentTime);
	BC_ASSERT_LOWER((int)srvDescentTime, 800, int, "%d");
	BC_ASSERT_EQUAL(reg1.getRequestCount(), 1, int, "%d");
	BC_ASSERT_EQUAL(reg2.getRequestCount(), 1, int, "%d");

	// Now that both SRV targets failed, the exponential backoff delay applies before the next cycle.
	// Base delay = 1500ms, jitter [1125, 1875]ms.
	// Next cycle will try reg1 (response 3) and reg2 (response 4).
	BC_ASSERT_TRUE(ua.waitCountOf(&ua.mNumberOf5xxResponse, 4, 4000));
	uint64_t t2 = bctbx_get_cur_time_ms();
	uint64_t cycleDelay = t2 - t1;
	belle_sip_message("Cycle backoff delay: %llu ms", (unsigned long long)cycleDelay);
	BC_ASSERT_GREATER((int)cycleDelay, 1000, int, "%d");
	BC_ASSERT_LOWER((int)cycleDelay, 2400, int, "%d");
	BC_ASSERT_EQUAL(reg1.getRequestCount(), 2, int, "%d");
	BC_ASSERT_EQUAL(reg2.getRequestCount(), 2, int, "%d");
}

static void register_retry_backoff_reset_on_200_ok(void) {
	std::string transport = "TCP";
	BasicRegistrar registrar("127.0.0.1", transport);
	registrar.setServiceUnavailable(true);
	TestUA ua(transport);

	// base delay = 1000ms, max delay = 4000ms
	belle_sip_stack_set_register_retry_backoff(ua.getStack(), 1000, 4000);

	ua.sendRegister(registrar.getAgent().getListeningUriAsString());

	// Initial attempt fails with 503
	BC_ASSERT_TRUE(ua.waitCountOf(&ua.mNumberOf5xxResponse, 1, 2000));
	uint64_t t1 = bctbx_get_cur_time_ms();

	// 1st retry: base = 1000ms
	BC_ASSERT_TRUE(ua.waitCountOf(&ua.mNumberOf5xxResponse, 2, 3000));
	uint64_t t2 = bctbx_get_cur_time_ms();
	BC_ASSERT_GREATER((int)(t2 - t1), 700, int, "%d");

	// Now registrar recovers and accepts registration!
	registrar.setServiceUnavailable(false);

	// 2nd retry receives 200 OK
	BC_ASSERT_TRUE(ua.waitCountOf(&ua.mNumberOf2xxResponse, 1, 4000));

	// Server goes down again
	registrar.setServiceUnavailable(true);

	// Trigger a re-registration
	belle_sip_refresher_refresh(ua.getRefresher(), 3600);

	// Should fail with 503
	BC_ASSERT_TRUE(ua.waitCountOf(&ua.mNumberOf5xxResponse, 3, 2000));
	uint64_t t3 = bctbx_get_cur_time_ms();

	// Next retry should wait base = 1000ms (NOT base*2^2 = 4000ms), because counter was reset on 200 OK!
	BC_ASSERT_TRUE(ua.waitCountOf(&ua.mNumberOf5xxResponse, 4, 3000));
	uint64_t t4 = bctbx_get_cur_time_ms();
	uint64_t resetDelay = t4 - t3;
	belle_sip_message("Reset backoff delay: %llu ms", (unsigned long long)resetDelay);
	BC_ASSERT_GREATER((int)resetDelay, 700, int, "%d");
	BC_ASSERT_LOWER((int)resetDelay, 1600, int, "%d");
}

static test_t refresher_failover_tests[] = {
    TEST_NO_TAG("Register and unregister UDP", register_and_unregister_udp),
    TEST_NO_TAG("Register and unregister TCP", register_and_unregister_tcp),
    TEST_NO_TAG("Register with SRV UDP", register_with_srv_udp),
    TEST_NO_TAG("Register with SRV TCP", register_with_srv_tcp),
    TEST_NO_TAG("Register failover SRV TCP", register_failover_srv_tcp),
    TEST_NO_TAG("Register retry exponential backoff on 503", register_retry_exponential_backoff_on_503),
    TEST_NO_TAG("Register retry backoff with Retry-After", register_retry_backoff_with_retry_after),
    TEST_NO_TAG("Register retry backoff after SRV exhaustion", register_retry_backoff_after_srv_exhaustion),
    TEST_NO_TAG("Register retry backoff reset on 200 OK", register_retry_backoff_reset_on_200_ok),
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

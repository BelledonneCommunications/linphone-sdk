/*
 * Copyright (c) 2012-2024 Belledonne Communications SARL.
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

#include "belle_sip_test_ua.hh"
#include "bctoolbox/port.h"
#include "bctoolbox/tester.h"

namespace bellesip {

TestUA::TestUA(const std::string &transport, const std::string &ip) {
	mStack = belle_sip_stack_new(NULL);
	mListeningPoint = belle_sip_stack_create_listening_point(mStack, ip.c_str(), BELLE_SIP_LISTENING_POINT_RANDOM_PORT,
	                                                         transport.c_str());
	mProvider = belle_sip_stack_create_provider(mStack, mListeningPoint);
}

TestUA::~TestUA() {
	if (mRefresher) {
		belle_sip_refresher_stop(mRefresher);
		belle_sip_object_unref(mRefresher);
		mRefresher = nullptr;
	}
	if (mProvider) {
		belle_sip_provider_clean_channels(mProvider);
		belle_sip_object_unref(mProvider);
		mProvider = nullptr;
	}
	if (mStack) {
		belle_sip_main_loop_sleep(belle_sip_stack_get_main_loop(mStack), 50);
		belle_sip_object_unref(mStack);
		mStack = nullptr;
	}
}

void TestUA::onRefresherListener(belle_sip_refresher_t *refresher,
                                 void *userData,
                                 unsigned int statusCode,
                                 const char *reasonPhrase,
                                 int willRetry) {
	BELLESIP_UNUSED(refresher);
	BELLESIP_UNUSED(reasonPhrase);
	BELLESIP_UNUSED(willRetry);
	TestUA *ua = static_cast<TestUA *>(userData);
	ua->mResponseCount++;
	if (statusCode >= 200 && statusCode < 300) {
		ua->mNumberOf2xxResponse++;
	} else if (statusCode >= 300 && statusCode < 400) {
		ua->mNumberOf3xxResponse++;
	} else if (statusCode >= 400 && statusCode < 500) {
		ua->mNumberOf4xxResponse++;
	} else if (statusCode >= 500 && statusCode < 600) {
		ua->mNumberOf5xxResponse++;
	} else if (statusCode >= 600) {
		ua->mNumberOf6xxResponse++;
	}
	belle_sip_main_loop_quit(belle_sip_stack_get_main_loop(ua->mStack));
}

void TestUA::sendRegister(const std::string &destUri, int expires, const std::string &domain) {
	belle_sip_uri_t *destUriObj = belle_sip_uri_parse(destUri.c_str());
	if (!destUriObj) {
		belle_sip_fatal("TestUA::sendRegister: Invalid destination URI [%s]", destUri.c_str());
		return;
	}
	std::string regDomain = domain;
	if (regDomain.empty()) {
		const char *host = belle_sip_uri_get_host(destUriObj);
		if (host) regDomain = host;
	}

	std::string reqUriStr = "sip:" + regDomain;
	belle_sip_uri_t *reqUri = belle_sip_uri_parse(reqUriStr.c_str());

	std::string identity = "Tester <sip:tester@" + regDomain + ">";

	belle_sip_request_t *req = belle_sip_request_create(
	    reqUri, "REGISTER", belle_sip_provider_create_call_id(mProvider), belle_sip_header_cseq_create(20, "REGISTER"),
	    belle_sip_header_from_create2(identity.c_str(), BELLE_SIP_RANDOM_TAG),
	    belle_sip_header_to_create2(identity.c_str(), NULL), belle_sip_header_via_new(), 70);

	belle_sip_message_add_header(BELLE_SIP_MESSAGE(req), BELLE_SIP_HEADER(belle_sip_header_expires_create(expires)));
	belle_sip_message_add_header(BELLE_SIP_MESSAGE(req), BELLE_SIP_HEADER(belle_sip_header_contact_new()));

	belle_sip_client_transaction_t *trans = belle_sip_provider_create_client_transaction(mProvider, req);
	belle_sip_client_transaction_send_request_to(trans, destUriObj);

	mRefresher = belle_sip_client_transaction_create_refresher(trans);
	belle_sip_refresher_set_listener(mRefresher, onRefresherListener, this);
}

void TestUA::unregister() {
	if (mRefresher) {
		belle_sip_refresher_refresh(mRefresher, 0);
	}
}

bool TestUA::waitCountOf(const int *counter, int expectedValue, int timeoutMs) {
	belle_sip_main_loop_t *ml = belle_sip_stack_get_main_loop(mStack);
	uint64_t startTime = bctbx_get_cur_time_ms();
	while (*counter < expectedValue) {
		uint64_t elapsed = bctbx_get_cur_time_ms() - startTime;
		if (elapsed >= (uint64_t)timeoutMs) {
			return false;
		}
		int remainingMs = static_cast<int>(timeoutMs - elapsed);
		belle_sip_main_loop_sleep(ml, remainingMs);
	}
	return (*counter >= expectedValue);
}

} // namespace bellesip

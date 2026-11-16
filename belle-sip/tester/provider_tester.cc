/*
 * Copyright (c) 2026 Belledonne Communications SARL.
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
#include "belle_sip_internal.h"
#include "belle_sip_test_ua.hh"
#include "belle_sip_tester.h"

using namespace bellesip;

struct ProviderTestContext {
	TestUA *ua = nullptr;
	int request_count = 0;
	int response_count = 0;
	int response_status = 0;
	int outgoing_hook_count = 0;
	int incoming_hook_count = 0;
	std::string last_custom_header_value;
};

static void process_response_event_cb(void *user_ctx, const belle_sip_response_event_t *event) {
	auto *ctx = static_cast<ProviderTestContext *>(user_ctx);
	ctx->response_count++;
	ctx->response_status = belle_sip_response_get_status_code(event->response);
}

static void test_outgoing_request_hook(void) {
	TestUA ua1("UDP", "127.0.0.1", BELLE_SIP_LISTENING_POINT_RANDOM_PORT);
	TestUA ua2(ua1.getStack(), "UDP", "127.0.0.1", BELLE_SIP_LISTENING_POINT_RANDOM_PORT);

	BC_ASSERT_EQUAL(belle_sip_provider_request_hooks_enabled(ua1.getProvider()), 0, int, "%d");
	ua1.enableRequestHooks(true);
	BC_ASSERT_EQUAL(belle_sip_provider_request_hooks_enabled(ua1.getProvider()), 1, int, "%d");

	ProviderTestContext ctx1;
	ctx1.ua = &ua1;

	belle_sip_listener_callbacks_t cbs1{};
	cbs1.outgoing_request_hook = [](void *user_ctx, belle_sip_request_t *req, belle_sip_channel_t *chan) {
		auto *ctx = static_cast<ProviderTestContext *>(user_ctx);
		ctx->outgoing_hook_count++;
		BC_ASSERT_PTR_NOT_NULL(chan);
		if (chan) {
			BC_ASSERT_EQUAL(belle_sip_channel_get_state(chan), BELLE_SIP_CHANNEL_READY, int, "%d");
			int port = 0;
			const char *ip = belle_sip_channel_get_public_address(chan, &port);
			BC_ASSERT_PTR_NOT_NULL(ip);
			BC_ASSERT_GREATER(port, 0, int, "%d");
			BC_ASSERT_PTR_NOT_NULL(belle_sip_channel_get_transport_name(chan));
			BC_ASSERT_PTR_NOT_NULL(belle_sip_channel_get_transport_name_lower_case(chan));
		}
		belle_sip_header_t *h = belle_sip_header_create("X-Custom-Out", "CustomOutValue42");
		belle_sip_message_add_header(BELLE_SIP_MESSAGE(req), h);
	};
	cbs1.process_response_event = process_response_event_cb;
	ua1.setListenerCallbacks(cbs1, &ctx1);

	ProviderTestContext ctx2;
	ctx2.ua = &ua2;

	belle_sip_listener_callbacks_t cbs2{};
	cbs2.process_request_event = [](void *user_ctx, const belle_sip_request_event_t *event) {
		auto *ctx = static_cast<ProviderTestContext *>(user_ctx);
		ctx->request_count++;
		belle_sip_header_t *h = belle_sip_message_get_header(BELLE_SIP_MESSAGE(event->request), "X-Custom-Out");
		if (h) {
			const char *val = belle_sip_header_extension_get_value(BELLE_SIP_HEADER_EXTENSION(h));
			if (val) ctx->last_custom_header_value = val;
		}
		belle_sip_server_transaction_t *st =
		    belle_sip_provider_create_server_transaction(ctx->ua->getProvider(), event->request);
		belle_sip_response_t *resp = belle_sip_response_create_from_request(event->request, 200);
		belle_sip_server_transaction_send_response(st, resp);
	};
	ua2.setListenerCallbacks(cbs2, &ctx2);

	belle_sip_request_t *req = ua1.createRequest("OPTIONS", ua2.getListeningUriAsString());
	BC_ASSERT_PTR_NOT_NULL(req);
	ua1.sendRequest(req);

	BC_ASSERT_TRUE(ua1.waitCountOf(&ctx1.response_count, 1, 3000));
	BC_ASSERT_EQUAL(ctx2.request_count, 1, int, "%d");
	BC_ASSERT_EQUAL(ctx1.outgoing_hook_count, 1, int, "%d");
	BC_ASSERT_STRING_EQUAL(ctx2.last_custom_header_value.c_str(), "CustomOutValue42");
	BC_ASSERT_EQUAL(ctx1.response_status, 200, int, "%d");
}

static void test_incoming_request_hook(void) {
	TestUA ua1("UDP", "127.0.0.1", BELLE_SIP_LISTENING_POINT_RANDOM_PORT);
	TestUA ua2(ua1.getStack(), "UDP", "127.0.0.1", BELLE_SIP_LISTENING_POINT_RANDOM_PORT);

	ua2.enableRequestHooks(true);
	BC_ASSERT_EQUAL(belle_sip_provider_request_hooks_enabled(ua2.getProvider()), 1, int, "%d");

	ProviderTestContext ctx1;
	ctx1.ua = &ua1;
	belle_sip_listener_callbacks_t cbs1{};
	cbs1.process_response_event = process_response_event_cb;
	ua1.setListenerCallbacks(cbs1, &ctx1);

	ProviderTestContext ctx2;
	ctx2.ua = &ua2;

	belle_sip_listener_callbacks_t cbs2{};
	cbs2.incoming_request_hook = [](void *user_ctx, belle_sip_request_t *req, belle_sip_channel_t *chan) {
		auto *ctx = static_cast<ProviderTestContext *>(user_ctx);
		ctx->incoming_hook_count++;
		BC_ASSERT_PTR_NOT_NULL(chan);
		if (chan) {
			BC_ASSERT_EQUAL(belle_sip_channel_get_state(chan), BELLE_SIP_CHANNEL_READY, int, "%d");
			int port = 0;
			const char *ip = belle_sip_channel_get_public_address(chan, &port);
			BC_ASSERT_PTR_NOT_NULL(ip);
			BC_ASSERT_GREATER(port, 0, int, "%d");
		}
		belle_sip_header_t *h = belle_sip_header_create("X-Custom-In", "CustomInValue84");
		belle_sip_message_add_header(BELLE_SIP_MESSAGE(req), h);
	};
	cbs2.process_request_event = [](void *user_ctx, const belle_sip_request_event_t *event) {
		auto *ctx = static_cast<ProviderTestContext *>(user_ctx);
		ctx->request_count++;
		// Check that the header added by incoming_request_hook is visible in process_request_event
		belle_sip_header_t *h = belle_sip_message_get_header(BELLE_SIP_MESSAGE(event->request), "X-Custom-In");
		if (h) {
			const char *val = belle_sip_header_extension_get_value(BELLE_SIP_HEADER_EXTENSION(h));
			if (val) ctx->last_custom_header_value = val;
		}
		belle_sip_server_transaction_t *st =
		    belle_sip_provider_create_server_transaction(ctx->ua->getProvider(), event->request);
		belle_sip_response_t *resp = belle_sip_response_create_from_request(event->request, 200);
		belle_sip_server_transaction_send_response(st, resp);
	};
	ua2.setListenerCallbacks(cbs2, &ctx2);

	belle_sip_request_t *req = ua1.createRequest("OPTIONS", ua2.getListeningUriAsString());
	BC_ASSERT_PTR_NOT_NULL(req);
	// ua1 does not add X-Custom-In
	ua1.sendRequest(req);

	BC_ASSERT_TRUE(ua1.waitCountOf(&ctx1.response_count, 1, 3000));
	BC_ASSERT_EQUAL(ctx2.request_count, 1, int, "%d");
	BC_ASSERT_EQUAL(ctx2.incoming_hook_count, 1, int, "%d");
	BC_ASSERT_STRING_EQUAL(ctx2.last_custom_header_value.c_str(), "CustomInValue84");
	BC_ASSERT_EQUAL(ctx1.response_status, 200, int, "%d");
}

static void test_hooks_disabled_by_default(void) {
	TestUA ua1("UDP", "127.0.0.1", BELLE_SIP_LISTENING_POINT_RANDOM_PORT);
	TestUA ua2(ua1.getStack(), "UDP", "127.0.0.1", BELLE_SIP_LISTENING_POINT_RANDOM_PORT);

	BC_ASSERT_EQUAL(belle_sip_provider_request_hooks_enabled(ua1.getProvider()), 0, int, "%d");
	BC_ASSERT_EQUAL(belle_sip_provider_request_hooks_enabled(ua2.getProvider()), 0, int, "%d");

	ProviderTestContext ctx1;
	ctx1.ua = &ua1;
	belle_sip_listener_callbacks_t cbs1{};
	cbs1.outgoing_request_hook = [](void *user_ctx, belle_sip_request_t *req, belle_sip_channel_t *) {
		auto *ctx = static_cast<ProviderTestContext *>(user_ctx);
		ctx->outgoing_hook_count++;
		belle_sip_header_t *h = belle_sip_header_create("X-Custom-Out", "ShouldNotBeCalled");
		belle_sip_message_add_header(BELLE_SIP_MESSAGE(req), h);
	};
	cbs1.process_response_event = process_response_event_cb;
	ua1.setListenerCallbacks(cbs1, &ctx1);

	ProviderTestContext ctx2;
	ctx2.ua = &ua2;
	belle_sip_listener_callbacks_t cbs2{};
	cbs2.incoming_request_hook = [](void *user_ctx, belle_sip_request_t *req, belle_sip_channel_t *) {
		auto *ctx = static_cast<ProviderTestContext *>(user_ctx);
		ctx->incoming_hook_count++;
		belle_sip_header_t *h = belle_sip_header_create("X-Custom-In", "ShouldNotBeCalled");
		belle_sip_message_add_header(BELLE_SIP_MESSAGE(req), h);
	};
	cbs2.process_request_event = [](void *user_ctx, const belle_sip_request_event_t *event) {
		auto *ctx = static_cast<ProviderTestContext *>(user_ctx);
		ctx->request_count++;
		belle_sip_server_transaction_t *st =
		    belle_sip_provider_create_server_transaction(ctx->ua->getProvider(), event->request);
		belle_sip_response_t *resp = belle_sip_response_create_from_request(event->request, 200);
		belle_sip_server_transaction_send_response(st, resp);
	};
	ua2.setListenerCallbacks(cbs2, &ctx2);

	belle_sip_request_t *req = ua1.createRequest("OPTIONS", ua2.getListeningUriAsString());
	BC_ASSERT_PTR_NOT_NULL(req);
	ua1.sendRequest(req);

	BC_ASSERT_TRUE(ua1.waitCountOf(&ctx1.response_count, 1, 3000));
	BC_ASSERT_EQUAL(ctx2.request_count, 1, int, "%d");
	BC_ASSERT_EQUAL(ctx1.outgoing_hook_count, 0, int, "%d");
	BC_ASSERT_EQUAL(ctx2.incoming_hook_count, 0, int, "%d");
}

struct ReentrancyContext {
	TestUA *ua1 = nullptr;
	TestUA *ua2 = nullptr;
	int ua1_out_hook_count = 0;
	int ua1_in_hook_count = 0;
	int ua1_req_count = 0;
	int ua1_resp_count = 0;

	int ua2_l1_req_count = 0;
	int ua2_l2_req_count = 0;
	int ua2_out_hook_count = 0;
	int ua2_in_hook_count = 0;
	int ua2_resp_count = 0;
};

static void test_reentrant_hooks(void) {
	TestUA ua1("UDP", "127.0.0.1", BELLE_SIP_LISTENING_POINT_RANDOM_PORT);
	TestUA ua2(ua1.getStack(), "UDP", "127.0.0.1", BELLE_SIP_LISTENING_POINT_RANDOM_PORT);

	ua1.enableRequestHooks(true);
	ua2.enableRequestHooks(true);

	ReentrancyContext re_ctx;
	re_ctx.ua1 = &ua1;
	re_ctx.ua2 = &ua2;

	// ua1 listener
	belle_sip_listener_callbacks_t cbs1{};
	cbs1.outgoing_request_hook = [](void *user_ctx, belle_sip_request_t *, belle_sip_channel_t *) {
		auto *ctx = static_cast<ReentrancyContext *>(user_ctx);
		ctx->ua1_out_hook_count++;
	};
	cbs1.incoming_request_hook = [](void *user_ctx, belle_sip_request_t *, belle_sip_channel_t *) {
		auto *ctx = static_cast<ReentrancyContext *>(user_ctx);
		ctx->ua1_in_hook_count++;
	};
	cbs1.process_request_event = [](void *user_ctx, const belle_sip_request_event_t *event) {
		auto *ctx = static_cast<ReentrancyContext *>(user_ctx);
		ctx->ua1_req_count++;
		belle_sip_server_transaction_t *st =
		    belle_sip_provider_create_server_transaction(ctx->ua1->getProvider(), event->request);
		belle_sip_response_t *resp = belle_sip_response_create_from_request(event->request, 200);
		belle_sip_server_transaction_send_response(st, resp);
	};
	cbs1.process_response_event = [](void *user_ctx, const belle_sip_response_event_t *) {
		auto *ctx = static_cast<ReentrancyContext *>(user_ctx);
		ctx->ua1_resp_count++;
	};
	ua1.setListenerCallbacks(cbs1, &re_ctx);

	// ua2 listener 1: on receiving request, sends a NEW request back to ua1 (reentrantly triggering
	// outgoing_request_hook)
	belle_sip_listener_callbacks_t cbs2_l1{};
	cbs2_l1.outgoing_request_hook = [](void *user_ctx, belle_sip_request_t *, belle_sip_channel_t *) {
		auto *ctx = static_cast<ReentrancyContext *>(user_ctx);
		ctx->ua2_out_hook_count++;
	};
	cbs2_l1.incoming_request_hook = [](void *user_ctx, belle_sip_request_t *, belle_sip_channel_t *) {
		auto *ctx = static_cast<ReentrancyContext *>(user_ctx);
		ctx->ua2_in_hook_count++;
	};
	cbs2_l1.process_request_event = [](void *user_ctx, const belle_sip_request_event_t *event) {
		auto *ctx = static_cast<ReentrancyContext *>(user_ctx);
		ctx->ua2_l1_req_count++;

		// Send a new request to ua1 right now from inside process_request_event!
		belle_sip_request_t *reverse_req = ctx->ua2->createRequest("MESSAGE", ctx->ua1->getListeningUriAsString());
		BC_ASSERT_PTR_NOT_NULL(reverse_req);
		ctx->ua2->sendRequest(reverse_req);

		// Now answer the incoming request
		belle_sip_server_transaction_t *st =
		    belle_sip_provider_create_server_transaction(ctx->ua2->getProvider(), event->request);
		belle_sip_response_t *resp = belle_sip_response_create_from_request(event->request, 200);
		belle_sip_server_transaction_send_response(st, resp);
	};
	cbs2_l1.process_response_event = [](void *user_ctx, const belle_sip_response_event_t *) {
		auto *ctx = static_cast<ReentrancyContext *>(user_ctx);
		ctx->ua2_resp_count++;
	};
	ua2.setListenerCallbacks(cbs2_l1, &re_ctx);

	// ua2 listener 2: must also be invoked despite the re-entrant send in listener 1!
	belle_sip_listener_callbacks_t cbs2_l2{};
	cbs2_l2.process_request_event = [](void *user_ctx, const belle_sip_request_event_t *) {
		auto *ctx = static_cast<ReentrancyContext *>(user_ctx);
		ctx->ua2_l2_req_count++;
	};
	belle_sip_listener_t *l2 = belle_sip_listener_create_from_callbacks(&cbs2_l2, &re_ctx);
	belle_sip_provider_add_sip_listener(ua2.getProvider(), l2);

	// Send initial request from ua1 to ua2
	belle_sip_request_t *req = ua1.createRequest("OPTIONS", ua2.getListeningUriAsString());
	BC_ASSERT_PTR_NOT_NULL(req);
	ua1.sendRequest(req);

	// Wait until ua1 receives 200 OK for OPTIONS and ua2 receives 200 OK for MESSAGE
	BC_ASSERT_TRUE(ua1.waitCountOf(&re_ctx.ua1_resp_count, 1, 3000));
	BC_ASSERT_TRUE(ua2.waitCountOf(&re_ctx.ua2_resp_count, 1, 3000));

	BC_ASSERT_EQUAL(re_ctx.ua1_req_count, 1, int, "%d");
	BC_ASSERT_EQUAL(re_ctx.ua2_l1_req_count, 1, int, "%d");
	BC_ASSERT_EQUAL(re_ctx.ua2_l2_req_count, 1, int, "%d"); // Listener iteration was not interrupted!
	BC_ASSERT_EQUAL(re_ctx.ua1_out_hook_count, 1, int, "%d");
	BC_ASSERT_EQUAL(re_ctx.ua2_in_hook_count, 1, int, "%d");
	BC_ASSERT_EQUAL(re_ctx.ua2_out_hook_count, 1, int, "%d"); // Reverse request triggered hook!
	BC_ASSERT_EQUAL(re_ctx.ua1_in_hook_count, 1, int, "%d");

	belle_sip_provider_remove_sip_listener(ua2.getProvider(), l2);
	belle_sip_object_unref(l2);
}

static void test_outgoing_incoming_request_hooks_tcp(void) {
	TestUA ua1("TCP", "127.0.0.1", BELLE_SIP_LISTENING_POINT_RANDOM_PORT);
	TestUA ua2(ua1.getStack(), "TCP", "127.0.0.1", BELLE_SIP_LISTENING_POINT_RANDOM_PORT);

	ua1.enableRequestHooks(true);
	ua2.enableRequestHooks(true);

	ProviderTestContext ctx1;
	ctx1.ua = &ua1;
	belle_sip_listener_callbacks_t cbs1{};
	cbs1.outgoing_request_hook = [](void *user_ctx, belle_sip_request_t *req, belle_sip_channel_t *) {
		auto *ctx = static_cast<ProviderTestContext *>(user_ctx);
		ctx->outgoing_hook_count++;
		belle_sip_header_t *h = belle_sip_header_create("X-Custom-TCP-Out", "ValTCPOut");
		belle_sip_message_add_header(BELLE_SIP_MESSAGE(req), h);
	};
	cbs1.process_response_event = process_response_event_cb;
	ua1.setListenerCallbacks(cbs1, &ctx1);

	ProviderTestContext ctx2;
	ctx2.ua = &ua2;
	belle_sip_listener_callbacks_t cbs2{};
	cbs2.incoming_request_hook = [](void *user_ctx, belle_sip_request_t *req, belle_sip_channel_t *) {
		auto *ctx = static_cast<ProviderTestContext *>(user_ctx);
		ctx->incoming_hook_count++;
		belle_sip_header_t *h = belle_sip_header_create("X-Custom-TCP-In", "ValTCPIn");
		belle_sip_message_add_header(BELLE_SIP_MESSAGE(req), h);
	};
	cbs2.process_request_event = [](void *user_ctx, const belle_sip_request_event_t *event) {
		auto *ctx = static_cast<ProviderTestContext *>(user_ctx);
		ctx->request_count++;
		belle_sip_header_t *h_out = belle_sip_message_get_header(BELLE_SIP_MESSAGE(event->request), "X-Custom-TCP-Out");
		belle_sip_header_t *h_in = belle_sip_message_get_header(BELLE_SIP_MESSAGE(event->request), "X-Custom-TCP-In");
		if (h_out && h_in) {
			const char *v_out = belle_sip_header_extension_get_value(BELLE_SIP_HEADER_EXTENSION(h_out));
			const char *v_in = belle_sip_header_extension_get_value(BELLE_SIP_HEADER_EXTENSION(h_in));
			if (v_out && v_in && strcmp(v_out, "ValTCPOut") == 0 && strcmp(v_in, "ValTCPIn") == 0) {
				ctx->last_custom_header_value = "BothPresent";
			}
		}
		belle_sip_server_transaction_t *st =
		    belle_sip_provider_create_server_transaction(ctx->ua->getProvider(), event->request);
		belle_sip_response_t *resp = belle_sip_response_create_from_request(event->request, 200);
		belle_sip_server_transaction_send_response(st, resp);
	};
	ua2.setListenerCallbacks(cbs2, &ctx2);

	belle_sip_request_t *req = ua1.createRequest("OPTIONS", ua2.getListeningUriAsString());
	BC_ASSERT_PTR_NOT_NULL(req);
	ua1.sendRequest(req);

	BC_ASSERT_TRUE(ua1.waitCountOf(&ctx1.response_count, 1, 3000));
	BC_ASSERT_EQUAL(ctx2.request_count, 1, int, "%d");
	BC_ASSERT_EQUAL(ctx1.outgoing_hook_count, 1, int, "%d");
	BC_ASSERT_EQUAL(ctx2.incoming_hook_count, 1, int, "%d");
	BC_ASSERT_STRING_EQUAL(ctx2.last_custom_header_value.c_str(), "BothPresent");
	BC_ASSERT_EQUAL(ctx1.response_status, 200, int, "%d");
}

static test_t provider_tests[] = {
    TEST_NO_TAG("Outgoing request hook", test_outgoing_request_hook),
    TEST_NO_TAG("Incoming request hook", test_incoming_request_hook),
    TEST_NO_TAG("Request hooks disabled by default", test_hooks_disabled_by_default),
    TEST_NO_TAG("Reentrant request hooks", test_reentrant_hooks),
    TEST_NO_TAG("Outgoing and incoming request hooks (TCP)", test_outgoing_incoming_request_hooks_tcp),
};

test_suite_t provider_test_suite = {
    "Provider",
    NULL,
    NULL,
    belle_sip_tester_before_each,
    belle_sip_tester_after_each,
    sizeof(provider_tests) / sizeof(provider_tests[0]),
    provider_tests,
    0,
    0,
};

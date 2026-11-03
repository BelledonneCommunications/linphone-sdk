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
#include "http/http-client.h"
#include "liblinphone_tester.h"

#include "account/account-params.h"
#include "address/address.h"
#include "core/core.h"
#include "http-server-utils.h"
#include "liblinphone_tester++.h"
#include "linphone/api/c-account-params.h"
#include "linphone/api/c-account.h"

#include <set>

// =============================================================================

using namespace std;

using namespace LinphonePrivate;

std::unique_ptr<HttpServerWithBearerAuth> httpServer;

static const char *serverRealm = "subscribe.example.org";
static const char *validToken = "xyzzyx";

static int before_suite() {
	httpServer.reset(new HttpServerWithBearerAuth(serverRealm, validToken));
	httpServer->addResource("/hello", "text/plain", "rcfiles/marie_sips_rc");
	return 0;
}

static int after_suite() {
	httpServer.reset();
	return 0;
}

static void http_get() {
	LinphoneCoreManager *lcm = linphone_core_manager_new("empty_rc");
	int responseCode = 0;
	int gotResponse = 0;
	LinphoneBearerToken *token =
	    linphone_factory_create_bearer_token(linphone_factory_get(), validToken, time(NULL) + 10);
	LinphoneAuthInfo *ai = linphone_factory_create_auth_info_3(linphone_factory_get(), nullptr, token, serverRealm);

	linphone_core_add_auth_info(lcm->lc, ai);
	linphone_auth_info_unref(ai);
	linphone_bearer_token_unref(token);

	HttpClient &httpClient = L_GET_CPP_PTR_FROM_C_OBJECT(lcm->lc)->getHttpClient();
	auto &httpRequest = httpClient.createRequest("GET", httpServer->getUrl() + "/hello");
	httpRequest.execute([&](const HttpResponse &response) {
		responseCode = response.getHttpStatusCode();
		gotResponse = 1;
	});
	BC_ASSERT_TRUE(wait_for(lcm->lc, NULL, &gotResponse, 1));
	BC_ASSERT_EQUAL(responseCode, 200, int, "%i");

	linphone_core_manager_destroy(lcm);
}

static void http_bad_uri() {
	LinphoneCoreManager *lcm = linphone_core_manager_new("empty_rc");
	int gotException = 0;

	HttpClient &httpClient = L_GET_CPP_PTR_FROM_C_OBJECT(lcm->lc)->getHttpClient();

	try {
		auto &httpRequest = httpClient.createRequest("GET", "bob@example.net");
		(void)httpRequest;
	} catch (const std::exception &) {
		gotException = 1;
	}
	BC_ASSERT_TRUE(gotException == 1);

	linphone_core_manager_destroy(lcm);
}

static void _http_needs_token_refresh(bool expiration_is_known) {
	bellesip::HttpServer authorizationServer;

	authorizationServer.Post("/oauth/token", [](BCTBX_UNUSED(const httplib::Request &req), httplib::Response &res) {
		res.status = 200;
		constexpr const char *body = R"(
{
  "access_token":"xyzzyx",
  "token_type":"Bearer",
  "expires_in":3600,
  "refresh_token":"xyz2",
  "scope":"create"
}
						)";
		res.set_content(body, "application/json");
	});
	LinphoneCoreManager *lcm = linphone_core_manager_new("empty_rc");
	int responseCode = 0;
	int gotResponse = 0;
	LinphoneBearerToken *token = linphone_factory_create_bearer_token(linphone_factory_get(), "expired",
	                                                                  expiration_is_known ? time(NULL) - 10 : 0);
	LinphoneBearerToken *refreshToken = linphone_factory_create_bearer_token(linphone_factory_get(), "abcdefgh", 0);
	LinphoneAuthInfo *ai = linphone_factory_create_auth_info_3(linphone_factory_get(), nullptr, token, serverRealm);
	linphone_auth_info_set_refresh_token(ai, refreshToken);
	linphone_auth_info_set_token_endpoint_uri(ai, (authorizationServer.mRootUrl + "/oauth/token").c_str());

	linphone_core_add_auth_info(lcm->lc, ai);
	linphone_auth_info_unref(ai);
	linphone_bearer_token_unref(token);
	linphone_bearer_token_unref(refreshToken);

	HttpClient &httpClient = L_GET_CPP_PTR_FROM_C_OBJECT(lcm->lc)->getHttpClient();
	auto &httpRequest = httpClient.createRequest("GET", httpServer->getUrl() + "/hello");
	httpRequest.execute([&](const HttpResponse &response) {
		responseCode = response.getHttpStatusCode();
		gotResponse = 1;
	});
	BC_ASSERT_TRUE(wait_for(lcm->lc, NULL, &gotResponse, 1));
	BC_ASSERT_EQUAL(responseCode, 200, int, "%i");

	linphone_core_manager_destroy(lcm);
}

static void http_needs_token_refresh() {
	_http_needs_token_refresh(true);
}

static void http_needs_token_refresh_but_expiration_is_not_known() {
	_http_needs_token_refresh(false);
}

// Disabled functionality and test: on-demand authentication for http is not supported, unless in Configuring global
// state. This may change in the future.
#if 0
static int auth_requested = 0;
static void on_auth_requested(LinphoneCore *lc, LinphoneAuthInfo *ai, LinphoneAuthMethod method){
    auth_requested++;
    (void)lc;
    (void)ai;
    (void)method;
}

static void challenged_requested_with_late_credentials() {
    LinphoneCoreManager *lcm = linphone_core_manager_new("empty_rc");
    LinphoneCoreCbs *coreCbs = linphone_factory_create_core_cbs(linphone_factory_get());
    int responseCode = 0;
    int gotResponse = 0;
    auth_requested = 0;

    linphone_core_cbs_set_authentication_requested(coreCbs, on_auth_requested);

    linphone_core_add_callbacks(lcm->lc, coreCbs);
    linphone_core_cbs_unref(coreCbs);

	HttpClient &httpClient = L_GET_CPP_PTR_FROM_C_OBJECT(lcm->lc)->getHttpClient();
    auto & httpRequest = httpClient.createRequest("GET", httpServer->getUrl() + "/hello");
    httpRequest.execute([&](const HttpResponse & response){
        responseCode = response.getHttpStatusCode();
        gotResponse = 1;
    });
	BC_ASSERT_TRUE(wait_for(lcm->lc, NULL, &auth_requested, 1));
	BC_ASSERT_EQUAL(gotResponse, 0, int, "%i");
	LinphoneBearerToken *token =
	    linphone_factory_create_bearer_token(linphone_factory_get(), validToken, time(NULL) + 10);
	LinphoneAuthInfo *ai = linphone_factory_create_auth_info_3(linphone_factory_get(), nullptr, token, serverRealm);

	linphone_core_add_auth_info(lcm->lc, ai);
	linphone_auth_info_unref(ai);


	BC_ASSERT_TRUE(wait_for(lcm->lc, NULL, &gotResponse, 1));
	BC_ASSERT_EQUAL(responseCode, 200, int, "%i");
	BC_ASSERT_EQUAL(auth_requested, 1, int, "%i");

	linphone_core_manager_destroy(lcm);
}
#endif

static void http_request_cancelled() {
	LinphoneCoreManager *lcm = linphone_core_manager_new("empty_rc");
	int responseCode = 0;
	int gotResponse = 0;
	LinphoneBearerToken *token =
	    linphone_factory_create_bearer_token(linphone_factory_get(), validToken, time(NULL) + 10);
	LinphoneAuthInfo *ai = linphone_factory_create_auth_info_3(linphone_factory_get(), nullptr, token, serverRealm);

	linphone_core_add_auth_info(lcm->lc, ai);
	linphone_auth_info_unref(ai);
	linphone_bearer_token_unref(token);

	HttpClient &httpClient = L_GET_CPP_PTR_FROM_C_OBJECT(lcm->lc)->getHttpClient();
	auto &httpRequest = httpClient.createRequest("GET", httpServer->getUrl() + "/hello");
	httpRequest.execute([&](const HttpResponse &response) {
		responseCode = response.getHttpStatusCode();
		gotResponse = 1;
	});
	for (int i = 0; i < 2; ++i)
		linphone_core_iterate(lcm->lc);
	BC_ASSERT_TRUE(gotResponse == 0);
	if (gotResponse == 0) {
		httpRequest.cancel();
		BC_ASSERT_FALSE(wait_for_until(lcm->lc, NULL, &gotResponse, 1, 1000));
		BC_ASSERT_EQUAL(responseCode, 0, int, "%i");
	}
	linphone_core_manager_destroy(lcm);
}

static void one_over_two_http_request_cancelled() {
	LinphoneCoreManager *lcm = linphone_core_manager_new("empty_rc");
	int responseCode1 = 0;
	int gotResponse1 = 0;
	int responseCode2 = 0;
	int gotResponse2 = 0;
	LinphoneBearerToken *token =
	    linphone_factory_create_bearer_token(linphone_factory_get(), validToken, time(NULL) + 10);
	LinphoneAuthInfo *ai = linphone_factory_create_auth_info_3(linphone_factory_get(), nullptr, token, serverRealm);

	linphone_core_add_auth_info(lcm->lc, ai);
	linphone_auth_info_unref(ai);
	linphone_bearer_token_unref(token);

	HttpClient &httpClient = L_GET_CPP_PTR_FROM_C_OBJECT(lcm->lc)->getHttpClient();
	auto &httpRequest1 = httpClient.createRequest("GET", httpServer->getUrl() + "/hello");
	httpRequest1.execute([&](const HttpResponse &response) {
		responseCode1 = response.getHttpStatusCode();
		gotResponse1 = 1;
	});

	auto &httpRequest2 = httpClient.createRequest("GET", httpServer->getUrl() + "/hello");
	httpRequest2.execute([&](const HttpResponse &response) {
		responseCode2 = response.getHttpStatusCode();
		gotResponse2 = 1;
	});

	for (int i = 0; i < 2; ++i)
		linphone_core_iterate(lcm->lc);
	BC_ASSERT_TRUE(gotResponse1 == 0); // normally it should not have enough time to get answered.
	if (gotResponse1 == 0) {
		httpRequest1.cancel(); // Cancel is possible only if not answered.
		BC_ASSERT_FALSE(wait_for_until(lcm->lc, NULL, &gotResponse1, 1, 1000));
		BC_ASSERT_EQUAL(responseCode1, 0, int, "%i");
	}

	BC_ASSERT_TRUE(wait_for(lcm->lc, NULL, &gotResponse2, 1));
	BC_ASSERT_EQUAL(responseCode2, 200, int, "%i");

	linphone_core_manager_destroy(lcm);
}

/*
 * In this test the challenge is not verified.
 * The purpose of the test is to ensure that liblinphone is able to answer at lest one challenge (MD5 here)
 * even if proposed for both MD5 and SHA-256.
 * Two variants are tested: one Www-authenticate with two challenges, or two Www-authenticate with one challenge each.
 * A third check is done, where algorithm parameter is enclosed within quotes.
 */
static void digest_with_double_challenge() {
	bellesip::HttpServer httpServer;
	LinphoneCoreManager *lcm = linphone_core_manager_new("empty_rc");

	LinphoneAuthInfo *ai = linphone_factory_create_auth_info_2(linphone_factory_get(), "bob", nullptr, nullptr,
	                                                           "secret_ha1", "sip.example.org", nullptr, "MD5");

	linphone_core_add_auth_info(lcm->lc, ai);
	linphone_auth_info_unref(ai);

	httpServer.Get("/index.html", [](const httplib::Request &req, httplib::Response &res) {
		if (req.has_header("Authorization")) {
			res.status = 200;
		} else {
			res.status = 401;
			std::ostringstream headerValue;
			/* two headers, with one challenge each */
			headerValue << "Digest realm=\"" << "sip.example.org" << "\""
			            << ",nonce=\"abcdefghikkl\",opaque=\"dodo\",algorithm=MD5";
			res.set_header("WWW-Authenticate", headerValue.str());

			headerValue.str("");
			headerValue << "Digest realm=\"" << "sip.example.org" << "\""
			            << ",nonce=\"abcdefghikkl\",opaque=\"dodo\",algorithm=SHA-256";
			res.set_header("WWW-Authenticate", headerValue.str());
		}
	});

	httpServer.Get("/about.html", [](const httplib::Request &req, httplib::Response &res) {
		if (req.has_header("Authorization")) {
			res.status = 200;
		} else {
			res.status = 401;
			/* one header with 2 challenges */
			std::ostringstream headerValue;
			headerValue << "Digest realm=\"" << "sip.example.org" << "\""
			            << ",nonce=\"abcdefghikkl\",opaque=\"dodo\",algorithm=MD5, ";
			headerValue << "Digest realm=\"" << "sip.example.org" << "\""
			            << ",nonce=\"abcdefghikkl\",opaque=\"dodo\",algorithm=SHA-256";
			res.set_header("WWW-Authenticate", headerValue.str());
		}
	});

	httpServer.Get("/contact.html", [](const httplib::Request &req, httplib::Response &res) {
		if (req.has_header("Authorization")) {
			res.status = 200;
		} else {
			res.status = 401;
			/* one header with 2 challenges */
			std::ostringstream headerValue;
			headerValue << "Digest realm=\"" << "sip.example.org" << "\""
			            << ",nonce=\"abcdefghikkl\",opaque=\"dodo\",algorithm=\"MD5\", ";
			headerValue << "Digest realm=\"" << "sip.example.org" << "\""
			            << ",nonce=\"abcdefghikkl\",opaque=\"dodo\",algorithm=\"SHA-256\"";
			res.set_header("WWW-Authenticate", headerValue.str());
		}
	});

	HttpClient &httpClient = L_GET_CPP_PTR_FROM_C_OBJECT(lcm->lc)->getHttpClient();
	{
		int responseCode = 0;
		int gotResponse = 0;
		auto &httpRequest = httpClient.createRequest("GET", httpServer.mRootUrl + "/index.html");
		httpRequest.execute([&](const HttpResponse &response) {
			responseCode = response.getHttpStatusCode();
			gotResponse = 1;
		});
		BC_ASSERT_TRUE(wait_for(lcm->lc, NULL, &gotResponse, 1));
		BC_ASSERT_EQUAL(responseCode, 200, int, "%i");
	}
	{
		int responseCode = 0;
		int gotResponse = 0;
		auto &httpRequest = httpClient.createRequest("GET", httpServer.mRootUrl + "/about.html");
		httpRequest.execute([&](const HttpResponse &response) {
			responseCode = response.getHttpStatusCode();
			gotResponse = 1;
		});
		BC_ASSERT_TRUE(wait_for(lcm->lc, NULL, &gotResponse, 1));
		BC_ASSERT_EQUAL(responseCode, 200, int, "%i");
	}

	/* Same but now the client has the SHA-256 ha1 instead of the MD5 one*/
	linphone_core_clear_all_auth_info(lcm->lc);
	;
	ai = linphone_factory_create_auth_info_2(linphone_factory_get(), "bob", nullptr, nullptr, "secret_ha1",
	                                         "sip.example.org", nullptr, "SHA-256");

	linphone_core_add_auth_info(lcm->lc, ai);
	linphone_auth_info_unref(ai);

	{
		int responseCode = 0;
		int gotResponse = 0;
		auto &httpRequest = httpClient.createRequest("GET", httpServer.mRootUrl + "/index.html");
		httpRequest.execute([&](const HttpResponse &response) {
			responseCode = response.getHttpStatusCode();
			gotResponse = 1;
		});
		BC_ASSERT_TRUE(wait_for(lcm->lc, NULL, &gotResponse, 1));
		BC_ASSERT_EQUAL(responseCode, 200, int, "%i");
	}

	{
		int responseCode = 0;
		int gotResponse = 0;
		auto &httpRequest = httpClient.createRequest("GET", httpServer.mRootUrl + "/about.html");
		httpRequest.execute([&](const HttpResponse &response) {
			responseCode = response.getHttpStatusCode();
			gotResponse = 1;
		});
		BC_ASSERT_TRUE(wait_for(lcm->lc, NULL, &gotResponse, 1));
		BC_ASSERT_EQUAL(responseCode, 200, int, "%i");
	}

	{
		int responseCode = 0;
		int gotResponse = 0;
		auto &httpRequest3 = httpClient.createRequest("GET", httpServer.mRootUrl + "/contact.html");
		httpRequest3.execute([&](const HttpResponse &response) {
			responseCode = response.getHttpStatusCode();
			gotResponse = 1;
		});
		BC_ASSERT_TRUE(wait_for(lcm->lc, NULL, &gotResponse, 1));
		BC_ASSERT_EQUAL(responseCode, 200, int, "%i");
	}
	linphone_core_manager_destroy(lcm);
}

struct BackupResponse {
	int code = 0;
	std::string body;
	std::string subType;
};

/*
 * Sends a request to the backup server and waits for the response.
 */
static BackupResponse sendBackupRequest(LinphoneCoreManager *lcm,
                                        const BackupHttpServer &server,
                                        const std::string &method,
                                        const std::optional<std::string> &path,
                                        const std::optional<Content> &body = std::nullopt) {
	BackupResponse result;
	int gotResponse = 0;
	HttpClient &httpClient = L_GET_CPP_PTR_FROM_C_OBJECT(lcm->lc)->getHttpClient();
	auto &httpRequest = httpClient.createRequest(method, server.getUrl() + path.value_or(""));
	if (body) httpRequest.setBody(*body);
	httpRequest.execute([&](const HttpResponse &response) {
		result.code = response.getHttpStatusCode();
		result.body = response.getBody().getBodyAsString();
		result.subType = response.getBody().getContentType().getSubType();
		gotResponse = 1;
	});
	BC_ASSERT_TRUE(wait_for(lcm->lc, NULL, &gotResponse, 1));
	return result;
}

static Content jsonContent(const std::string &json) {
	return Content(ContentType("application", "json"), json);
}

static void post_and_get_chatroom_backup() {
	BackupHttpServer backupHttpServer;
	LinphoneCoreManager *lcm = linphone_core_manager_new("empty_rc");
	const std::string sender = "sip:marie@sip.example.org";
	const std::string backup = R"({"chatrooms":["sip:chatroom-1@sip.example.org"]})";

	auto urlPath = std::string("/") + sender;
	backupHttpServer.addResource(urlPath);

	auto res = sendBackupRequest(lcm, backupHttpServer, "POST", urlPath, jsonContent(backup));
	BC_ASSERT_EQUAL(res.code, 200, int, "%i");
	const auto stored = backupHttpServer.getBody(urlPath);
	BC_ASSERT_TRUE(stored.has_value());
	if (stored) BC_ASSERT_STRING_EQUAL(stored->c_str(), backup.c_str());

	res = sendBackupRequest(lcm, backupHttpServer, "GET", urlPath);
	BC_ASSERT_EQUAL(res.code, 200, int, "%i");
	BC_ASSERT_STRING_EQUAL(res.body.c_str(), backup.c_str());
	BC_ASSERT_STRING_EQUAL(res.subType.c_str(), "json");

	auto urlPath2 = std::string("/sip:fake@sip.example.org");
	backupHttpServer.addResource(urlPath2);

	res = sendBackupRequest(lcm, backupHttpServer, "GET", urlPath2);
	BC_ASSERT_EQUAL(res.code, 200, int, "%i");
	BC_ASSERT_STRING_EQUAL(res.body.c_str(), "[]");
	BC_ASSERT_STRING_EQUAL(res.subType.c_str(), "json");

	linphone_core_manager_destroy(lcm);
}

static void chatroom_backup_errors() {
	BackupHttpServer backupHttpServer;
	LinphoneCoreManager *lcm = linphone_core_manager_new("empty_rc");
	const std::string sender = "sip:marie@sip.example.org";
	const std::string sender2 = "sip:john@sip.example.org";
	const std::string backup = R"({"chatrooms":["sip:chatroom-1@sip.example.org"]})";

	auto urlPath = std::string("/") + sender;
	backupHttpServer.addResource(urlPath);
	backupHttpServer.addResource("/");
	auto urlPath2 = std::string("^") + sender2;
	backupHttpServer.addResource(urlPath2);

	// POST without path
	auto res = sendBackupRequest(lcm, backupHttpServer, "POST", std::nullopt, jsonContent(backup));
	BC_ASSERT_EQUAL(res.code, 406, int, "%i");
	BC_ASSERT_FALSE(backupHttpServer.getBody(urlPath).has_value());

	// GET without path (defined in belle-sip/tester/belle_sip_tester_utils.cc)
	res = sendBackupRequest(lcm, backupHttpServer, "GET", std::nullopt);
	BC_ASSERT_EQUAL(res.code, 200, int, "%i");
	BC_ASSERT_STRING_EQUAL(res.body.c_str(), "Hello World!");

	// POST with a non json body
	res = sendBackupRequest(lcm, backupHttpServer, "POST", urlPath, Content(ContentType("text", "plain"), "hello"));
	BC_ASSERT_EQUAL(res.code, 415, int, "%i");
	BC_ASSERT_FALSE(backupHttpServer.getBody(urlPath).has_value());

	// GET from an unsupported path
	bool exception_thrown = false;
	try {
		res = sendBackupRequest(lcm, backupHttpServer, "GET", std::string("sip:unsupported_url_path@sip.example.org"));
	} catch (std::invalid_argument &) {
		exception_thrown = true;
	}
	BC_ASSERT_TRUE(exception_thrown);

	// GET from an unknown sender
	res = sendBackupRequest(lcm, backupHttpServer, "GET", std::string("/sip:unknown@sip.example.org"));
	BC_ASSERT_EQUAL(res.code, 404, int, "%i");

	// POST to / is not supported
	res = sendBackupRequest(lcm, backupHttpServer, "POST", std::string("/"), jsonContent(backup));
	BC_ASSERT_EQUAL(res.code, 406, int, "%i");

	// GET from / is defined in belle-sip/tester/belle_sip_tester_utils.cc
	res = sendBackupRequest(lcm, backupHttpServer, "GET", std::string("/"));
	BC_ASSERT_EQUAL(res.code, 200, int, "%i");
	BC_ASSERT_STRING_EQUAL(res.body.c_str(), "Hello World!");

	// POST to a path not starting with ? is not supported
	res = sendBackupRequest(lcm, backupHttpServer, "POST", urlPath2, jsonContent(backup));
	BC_ASSERT_EQUAL(res.code, 406, int, "%i");

	// GET from a path not starting with ? defined in belle-sip/tester/belle_sip_tester_utils.cc
	res = sendBackupRequest(lcm, backupHttpServer, "GET", urlPath2);
	BC_ASSERT_EQUAL(res.code, 200, int, "%i");
	BC_ASSERT_STRING_EQUAL(res.body.c_str(), "Hello World!");

	linphone_core_manager_destroy(lcm);
}

static void chatroom_backup_updated() {
	BackupHttpServer backupHttpServer;
	LinphoneCoreManager *lcm = linphone_core_manager_new("empty_rc");
	const std::string sender = "sip:marie@sip.example.org";
	const std::string backup = R"({"chatrooms":["sip:chatroom-1@sip.example.org"]})";
	const std::string backupUpdated =
	    R"({"chatrooms":["sip:chatroom-2@sip.example.org","sip:chatroom-3@sip.example.org"]})";

	auto urlPath = std::string("/") + sender;
	backupHttpServer.addResource(urlPath);

	auto res = sendBackupRequest(lcm, backupHttpServer, "POST", urlPath, jsonContent(backup));
	BC_ASSERT_EQUAL(res.code, 200, int, "%i");

	res = sendBackupRequest(lcm, backupHttpServer, "GET", urlPath);
	BC_ASSERT_EQUAL(res.code, 200, int, "%i");
	BC_ASSERT_STRING_EQUAL(res.body.c_str(), backup.c_str());

	// Update chatroom list
	res = sendBackupRequest(lcm, backupHttpServer, "POST", urlPath, jsonContent(backupUpdated));
	BC_ASSERT_EQUAL(res.code, 200, int, "%i");

	res = sendBackupRequest(lcm, backupHttpServer, "GET", urlPath);
	BC_ASSERT_EQUAL(res.code, 200, int, "%i");
	BC_ASSERT_STRING_EQUAL(res.body.c_str(), backupUpdated.c_str());

	linphone_core_manager_destroy(lcm);
}

/*
 * Creates a started core whose database is a copy of the given one. The core manager removes this copy when it is
 * destroyed.
 */
static LinphoneCoreManager *createCoreManagerWithChatRooms(const char *database) {
	char *dbPath = bc_tester_res(database);
	LinphoneCoreManager *lcm = linphone_core_manager_create("empty_rc");
	BC_ASSERT_FALSE(liblinphone_tester_copy_file(dbPath, lcm->database_path));
	ms_free(dbPath);
	// Conference events play no part in a backup: the loaded chat rooms stay out of the list event handler
	linphone_config_set_bool(linphone_core_get_config(lcm->lc), "misc", "conference_event_log_enabled", FALSE);
	linphone_core_manager_start(lcm, FALSE);
	return lcm;
}

/*
 * Adds an account that does not register. Its backup server is the given one or, when empty, the default one of the
 * core. The core holds the returned account.
 */
static LinphoneAccount *
addBackupAccount(LinphoneCoreManager *lcm, const std::string &identity, const std::string &backupServerUrl = "") {
	LinphoneAccountParams *params = linphone_account_params_new(lcm->lc, TRUE);
	LinphoneAddress *identityAddress = linphone_address_new(identity.c_str());
	linphone_account_params_set_identity_address(params, identityAddress);
	LinphoneAddress *serverAddress =
	    linphone_address_new(("sip:" + std::string(linphone_address_get_domain(identityAddress))).c_str());
	linphone_account_params_set_server_address(params, serverAddress);
	linphone_address_unref(serverAddress);
	linphone_address_unref(identityAddress);
	linphone_account_params_enable_register(params, FALSE);
	if (!backupServerUrl.empty()) {
		linphone_account_params_set_backup_server_url(params, backupServerUrl.c_str());
	}
	LinphoneAccount *account = linphone_core_create_account(lcm->lc, params);
	linphone_account_params_unref(params);
	BC_ASSERT_EQUAL(linphone_core_add_account(lcm->lc, account), 0, int, "%d");
	linphone_account_unref(account);
	return account;
}

/*
 * Iterates the core until the server has stored a backup at the given path.
 */
static bool waitForBackup(LinphoneCoreManager *lcm, const BackupHttpServer &server, const std::string &path) {
	return Linphone::Tester::CoreAssert({L_GET_CPP_PTR_FROM_C_OBJECT(lcm->lc)->getSharedFromThis()})
	    .wait([&server, &path] { return server.getBody(path).has_value(); });
}

/*
 * Returns the URI of each conference address listed in a backup, with its parameters such as the GRUU.
 */
static std::set<std::string> getBackupChatRooms(const std::string &backup) {
	const JsonDocument document(jsonContent(backup));
	const Json::Value &chatRoomsJson = document.getRoot()["chat_rooms"];
	BC_ASSERT_TRUE(chatRoomsJson.isArray());
	std::set<std::string> chatRooms;
	for (const auto &chatRoom : chatRoomsJson) {
		const auto address = Address::create(chatRoom["conference_address"].asString());
		if (address) chatRooms.insert(address->asStringUriOnly());
	}
	return chatRooms;
}

static void push_chatroom_list() {
	BackupHttpServer backupHttpServer;
	LinphoneCoreManager *lcm = createCoreManagerWithChatRooms("db/linphone.db");
	const std::string identity = "sip:test-1@sip.linphone.org";
	const std::string urlPath = "/" + identity;
	const std::set<std::string> groupChatRooms = {"sip:test-43@sip.linphone.org", "sip:test-44@sip.linphone.org",
	                                              "sip:test-45@sip.linphone.org", "sip:test-46@sip.linphone.org"};
	backupHttpServer.addResource(urlPath);
	// Show all chat rooms, so that those missing from the backup were excluded by the push itself
	linphone_config_set_int(linphone_core_get_config(lcm->lc), "misc", "hide_empty_chat_rooms", 0);

	const LinphoneAccount *account = addBackupAccount(lcm, identity, backupHttpServer.getUrl());
	// The account also has basic chat rooms, that are not part of the backup
	BC_ASSERT_GREATER_STRICT(bctbx_list_size(linphone_account_get_chat_rooms(account)), groupChatRooms.size(), size_t,
	                         "%zu");

	linphone_core_post_http_backup(lcm->lc);
	BC_ASSERT_TRUE(waitForBackup(lcm, backupHttpServer, urlPath));

	const auto stored = backupHttpServer.getBody(urlPath);
	BC_ASSERT_TRUE(stored.has_value());
	if (stored) {
		BC_ASSERT_TRUE(getBackupChatRooms(*stored) == groupChatRooms);

		const auto res = sendBackupRequest(lcm, backupHttpServer, "GET", urlPath);
		BC_ASSERT_EQUAL(res.code, 200, int, "%i");
		BC_ASSERT_STRING_EQUAL(res.subType.c_str(), "json");
		BC_ASSERT_STRING_EQUAL(res.body.c_str(), stored->c_str());
	}

	linphone_core_manager_destroy(lcm);
}

static void push_chatroom_list_multiple_accounts() {
	BackupHttpServer backupHttpServer;
	LinphoneCoreManager *lcm = createCoreManagerWithChatRooms("db/chatrooms.db");
	const std::string firstIdentity = "sip:user_124@sip.example.org";
	const std::string secondIdentity = "sip:user_146@sip.example.org";
	const std::string noServerIdentity = "sip:user_157@sip.example.org";
	const std::string noChatRoomIdentity = "sip:no_chat_room@sip.example.org";
	for (const auto &identity : {firstIdentity, secondIdentity, noServerIdentity, noChatRoomIdentity}) {
		backupHttpServer.addResource("/" + identity);
	}

	addBackupAccount(lcm, firstIdentity, backupHttpServer.getUrl());
	addBackupAccount(lcm, noServerIdentity);
	addBackupAccount(lcm, secondIdentity, backupHttpServer.getUrl());
	addBackupAccount(lcm, noChatRoomIdentity, backupHttpServer.getUrl());

	linphone_core_post_http_backup(lcm->lc);
	BC_ASSERT_TRUE(waitForBackup(lcm, backupHttpServer, "/" + firstIdentity));
	BC_ASSERT_TRUE(waitForBackup(lcm, backupHttpServer, "/" + secondIdentity));
	BC_ASSERT_TRUE(waitForBackup(lcm, backupHttpServer, "/" + noChatRoomIdentity));

	// Each request contains only the chat rooms of its account, with their GRUU
	const std::set<std::string> firstChatRooms = {
	    "sip:chatroom-1d~jyR59Fl~gxhbC@conf.example.org;gr=b3ab8653-8a3e-00d3-973f-dd158e277bdf",
	    "sip:chatroom-WtRDzvBYXZSYK9sL@conf.example.org;gr=b3ab8653-8a3e-00d3-973f-dd158e277bdf"};
	const std::set<std::string> secondChatRooms = {
	    "sip:chatroom-4ikGiuRbMvTNTaSc@conf.example.org;gr=b3ab8653-8a3e-00d3-973f-dd158e277bdf",
	    "sip:chatroom-DMopHf8Bo6KweVc8@conf.example.org;gr=b3ab8653-8a3e-00d3-973f-dd158e277bdf"};
	BC_ASSERT_TRUE(getBackupChatRooms(backupHttpServer.getBody("/" + firstIdentity).value_or("")) == firstChatRooms);
	BC_ASSERT_TRUE(getBackupChatRooms(backupHttpServer.getBody("/" + secondIdentity).value_or("")) == secondChatRooms);
	BC_ASSERT_TRUE(getBackupChatRooms(backupHttpServer.getBody("/" + noChatRoomIdentity).value_or("")).empty());

	// Leave time for a request that should not be sent to reach the server
	int dummy = 0;
	wait_for_until(lcm->lc, NULL, &dummy, 1, 500);
	BC_ASSERT_FALSE(backupHttpServer.getBody("/" + noServerIdentity).has_value());

	linphone_core_manager_destroy(lcm);
}

static void backup_server_of_core_and_account() {
	BackupHttpServer coreServer;
	BackupHttpServer accountServer;
	LinphoneCoreManager *lcm = createCoreManagerWithChatRooms("db/chatrooms.db");
	// The URL of the core ends with a slash and the one of the account does not: no request may double it
	const std::string coreUrl = coreServer.getUrl() + "/";
	const std::string corePath = "/sip:user_124@sip.example.org";
	const std::string accountPath = "/sip:user_146@sip.example.org";
	const std::string disabledPath = "/sip:user_157@sip.example.org";
	for (const auto &path : {corePath, accountPath, disabledPath}) {
		coreServer.addResource(path);
		accountServer.addResource(path);
	}

	linphone_core_set_backup_server_url(lcm->lc, coreUrl.c_str());
	BC_ASSERT_STRING_EQUAL(linphone_core_get_backup_server_url(lcm->lc), coreUrl.c_str());

	// The first account gets the server of the core as default value, the second one overrides it
	const LinphoneAccount *coreAccount = addBackupAccount(lcm, corePath.substr(1));
	const LinphoneAccount *account = addBackupAccount(lcm, accountPath.substr(1), accountServer.getUrl());
	BC_ASSERT_STRING_EQUAL(linphone_account_params_get_backup_server_url(linphone_account_get_params(coreAccount)),
	                       coreUrl.c_str());
	BC_ASSERT_STRING_EQUAL(linphone_account_params_get_backup_server_url(linphone_account_get_params(account)),
	                       accountServer.getUrl().c_str());

	// The third account also gets the server of the core, then the application disables its backup
	LinphoneAccount *disabledAccount = addBackupAccount(lcm, disabledPath.substr(1));
	LinphoneAccountParams *disabledParams = linphone_account_params_clone(linphone_account_get_params(disabledAccount));
	linphone_account_params_set_backup_server_url(disabledParams, "");
	linphone_account_set_params(disabledAccount, disabledParams);
	linphone_account_params_unref(disabledParams);
	BC_ASSERT_PTR_NULL(linphone_account_params_get_backup_server_url(linphone_account_get_params(disabledAccount)));

	linphone_core_post_http_backup(lcm->lc);
	BC_ASSERT_TRUE(waitForBackup(lcm, coreServer, corePath));
	BC_ASSERT_TRUE(waitForBackup(lcm, accountServer, accountPath));

	// Leave time for requests that should not be sent to reach a server
	int dummy = 0;
	wait_for_until(lcm->lc, NULL, &dummy, 1, 500);
	BC_ASSERT_FALSE(coreServer.getBody(accountPath).has_value());
	BC_ASSERT_FALSE(accountServer.getBody(corePath).has_value());
	BC_ASSERT_FALSE(coreServer.getBody(disabledPath).has_value());
	BC_ASSERT_FALSE(accountServer.getBody(disabledPath).has_value());

	// The server of an account survives its params being written to the configuration and read back, as at start-up.
	// Written over the same section, the disabled account does not get any server back.
	const auto writeAndReadBackServerUrl = [lcm](const LinphoneAccount *writtenAccount) {
		const int configIndex = 10;
		LinphoneAccountParams *writtenParams =
		    linphone_account_params_clone(linphone_account_get_params(writtenAccount));
		AccountParams::toCpp(writtenParams)->writeToConfigFile(linphone_core_get_config(lcm->lc), configIndex);
		linphone_account_params_unref(writtenParams);
		LinphoneAccountParams *readParams = linphone_account_params_new_with_config(lcm->lc, configIndex);
		const char *url = linphone_account_params_get_backup_server_url(readParams);
		const std::string readUrl = url ? url : "";
		linphone_account_params_unref(readParams);
		return readUrl;
	};
	const std::string readAccountUrl = writeAndReadBackServerUrl(account);
	BC_ASSERT_STRING_EQUAL(readAccountUrl.c_str(), accountServer.getUrl().c_str());
	BC_ASSERT_TRUE(writeAndReadBackServerUrl(disabledAccount).empty());

	linphone_core_manager_destroy(lcm);
}

static test_t http_client_tests[] = {
    {"Http get", http_get},
    {"Bad http uri", http_bad_uri},
    {"Http request needs a token refresh", http_needs_token_refresh},
    {"Http request needs a token refresh but expiration was not known",
     http_needs_token_refresh_but_expiration_is_not_known},
    //{"Challenged request, credentials comes later", challenged_requested_with_late_credentials},
    {"Http request cancelled", http_request_cancelled},
    {"One over two http request cancelled", one_over_two_http_request_cancelled},
    {"Digest with double challenge", digest_with_double_challenge},
};

static test_t backup_http_client_tests[] = {
    {"Post and get chatroom backup", post_and_get_chatroom_backup},
    {"Chatroom backup errors", chatroom_backup_errors},
    {"Chatroom backup updated", chatroom_backup_updated},
    {"Push chatroom list", push_chatroom_list},
    {"Push chatroom list with multiple accounts", push_chatroom_list_multiple_accounts},
    {"Backup server of core and account", backup_server_of_core_and_account},
};

test_suite_t http_client_test_suite = {"HTTP Client",
                                       before_suite,
                                       after_suite,
                                       liblinphone_tester_before_each,
                                       liblinphone_tester_after_each,
                                       sizeof(http_client_tests) / sizeof(http_client_tests[0]),
                                       http_client_tests,
                                       3,
                                       0};

test_suite_t backup_http_client_test_suite = {"Backup HTTP Client",
                                              before_suite,
                                              after_suite,
                                              liblinphone_tester_before_each,
                                              liblinphone_tester_after_each,
                                              sizeof(backup_http_client_tests) / sizeof(backup_http_client_tests[0]),
                                              backup_http_client_tests,
                                              3,
                                              0};

/*
 * Copyright (c) 2012-2023 Belledonne Communications SARL.
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

#include "belle_sip_tester_utils.h"
#include "bctoolbox/exception.hh"
#include "bctoolbox/tester.h"
#include "bctoolbox/utils.hh"
#include "belle_sip_tester.h"
#include "belr/grammarbuilder.h"
using namespace bellesip;

bool_t belle_sip_is_executable_installed(const char *executable, const char *resource) {
	return bctoolbox::Utils::isExecutableInstalled(std::string(executable), std::string(resource));
}

void belle_sip_add_belr_grammar_search_path(const char *path) {
	belr::GrammarLoader::get().addPath(std::string(path));
}

using namespace std;

bool Module::onRequest(const belle_sip_request_event_t *BCTBX_UNUSED(event)) {
	return true;
};
bool Module::onResponse(const belle_sip_response_event_t *BCTBX_UNUSED(event)) {
	return true;
};

BasicSipAgent::BasicSipAgent(string domain, string tranport) : mDomain(domain) {
	mStack = belle_sip_stack_new(NULL);
	mListeningPoint = belle_sip_stack_create_listening_point(mStack, "127.0.0.1", BELLE_SIP_LISTENING_POINT_RANDOM_PORT,
	                                                         tranport.c_str());
	mProv = belle_sip_stack_create_provider(mStack, mListeningPoint);

	mListenerCallbacks.process_dialog_terminated = BasicSipAgent::process_dialog_terminated;
	mListenerCallbacks.process_io_error = NULL; // BasicRegistrar::process_io_error;
	mListenerCallbacks.process_request_event = BasicSipAgent::process_request_event;
	mListenerCallbacks.process_response_event = NULL;         // BasicRegistrar::process_response_event;
	mListenerCallbacks.process_timeout = NULL;                // BasicRegistrar::process_timeout;
	mListenerCallbacks.process_transaction_terminated = NULL; // BasicRegistrar::process_transaction_terminated;
	mListenerCallbacks.process_auth_requested = NULL;         // BasicRegistrar::process_auth_requested_for_algorithm;
	mListenerCallbacks.listener_destroyed = NULL;

	mListener = belle_sip_listener_create_from_callbacks(&mListenerCallbacks, (void *)this);
	belle_sip_provider_add_sip_listener(mProv, mListener);

	mSrvThread = std::thread([&] { belle_sip_stack_main(mStack); });
}
BasicSipAgent::~BasicSipAgent() {
	belle_sip_main_loop_quit(belle_sip_stack_get_main_loop(mStack));
	belle_sip_main_loop_wake_up(belle_sip_stack_get_main_loop(mStack));
	mSrvThread.join();
	belle_sip_object_unref(mProv);
	belle_sip_object_unref(mStack);
	belle_sip_object_unref(mListener);
}
const belle_sip_uri_t *BasicSipAgent::getListeningUri() {
	return belle_sip_listening_point_get_uri(mListeningPoint);
}
string BasicSipAgent::getListeningUriAsString() {
	ostringstream os;
	os << getListeningUri();
	return os.str();
}

void BasicSipAgent::addModule(Module *module) {
	if (std::find(mModules.begin(), mModules.end(), module) != mModules.end()) return; // Module already present.
	mModules.push_front(module);
}

void BasicSipAgent::process_dialog_terminated(void *user_ctx, const belle_sip_dialog_terminated_event_t *event) {
	BELLESIP_UNUSED(user_ctx);
	BELLESIP_UNUSED(event);
	BCTBX_SLOGI << "process_dialog_terminated called";
}

void BasicSipAgent::process_io_error(void *user_ctx, const belle_sip_io_error_event_t *event) {
	BELLESIP_UNUSED(user_ctx);
	BELLESIP_UNUSED(event);
	BCTBX_SLOGI << "process_io_error";
}

void BasicSipAgent::process_request_event(void *user_ctx, const belle_sip_request_event_t *event) {
	BCTBX_SLOGI << "process_request_event";
	BasicSipAgent *thiz = static_cast<BasicSipAgent *>(user_ctx);
	for (Module *m : thiz->getModules()) {
		if (m->onRequest(event) == false) {
			BCTBX_SLOGI << "Breaking module list";
			break;
		}
	}
}

void BasicSipAgent::process_response_event(void *user_ctx, const belle_sip_response_event_t *event) {
	BCTBX_SLOGI << "process_response_event";
	BasicSipAgent *thiz = static_cast<BasicSipAgent *>(user_ctx);
	for (Module *m : thiz->getModules()) {
		if (m->onResponse(event) == false) {
			BCTBX_SLOGI << "Breaking module list";
			break;
		}
	}
}

void BasicSipAgent::process_timeout(void *user_ctx, const belle_sip_timeout_event_t *event) {
	BELLESIP_UNUSED(user_ctx);
	BELLESIP_UNUSED(event);
	BCTBX_SLOGI << "process_timeout";
}
/********************BasicRegistrar************************/
BasicRegistrar::BasicRegistrar(string domain, string tranport) : mAgent(domain, tranport) {
	mAgent.addModule(this);
}

bool BasicRegistrar::onRequest(const belle_sip_request_event_t *event) {

	belle_sip_request_t *req = belle_sip_request_event_get_request(event);
	if (string(belle_sip_request_get_method(req)) != string("REGISTER")) return true; // skip

	int response_code = 500;
	belle_sip_response_t *response_msg;
	belle_sip_server_transaction_t *trans = belle_sip_provider_create_server_transaction(mAgent.getProv(), req);
	try {
		belle_sip_header_from_t *from = belle_sip_message_get_header_by_type(req, belle_sip_header_from_t);
		belle_sip_uri_t *from_uri = belle_sip_header_address_get_uri(BELLE_SIP_HEADER_ADDRESS(from));

		if (string(belle_sip_uri_get_host(from_uri)) != mAgent.getDomain()) {
			BCTBX_SLOGE << "Unknown domain [" << belle_sip_uri_get_host(from_uri) << "]";
			response_code = 404;
		} else {
			response_code = 200;
		}
	} catch (BctbxException &e) {
		(void)e;
		response_code = 400;
	}
	response_msg = belle_sip_response_create_from_request(req, response_code);
	belle_sip_server_transaction_send_response(trans, response_msg);
	return true;
}
/********************AuthenticatedRegistrar************************/

AuthenticatedRegistrar::AuthenticatedRegistrar(std::set<std::string> users_uri,
                                               std::string domain,
                                               std::string transport)
    : mRegistrar(domain, transport) {
	for (string user : users_uri) {
		belle_sip_uri_t *uri = belle_sip_uri_parse(user.c_str());
		if (uri == NULL) {
			throw BCTBX_EXCEPTION << "bad uri [" << user << "]";
		}
		if (!belle_sip_uri_get_user(uri)) throw BCTBX_EXCEPTION << "missing username [" << user << "]";
		if (!belle_sip_uri_get_user_password(uri) &&
		    !belle_sip_parameters_has_parameter(BELLE_SIP_PARAMETERS(uri), "access_token"))
			throw BCTBX_EXCEPTION << "missing password or access_token param [" << user << "]";

		mAuthorizedUsers[belle_sip_uri_get_user(uri)] = uri;
	}
	mRegistrar.getAgent().addModule(this);
}
void AuthenticatedRegistrar::addAuthMode(belle_sip_auth_mode_t mode) {
	mAuthMode.insert(mode);
}
#define NONCE_SIZE 32

void AuthenticatedRegistrar::setAuthzServer(const std::string &authzServer) {
	mAuthzServer = authzServer;
}

bool AuthenticatedRegistrar::onRequest(const belle_sip_request_event_t *event) {
	belle_sip_request_t *req = belle_sip_request_event_get_request(event);
	char *tmp = belle_sip_uri_to_string(belle_sip_request_get_uri(req));
	bool isUa = string(belle_sip_request_get_method(req)) == "REGISTER" ? true : false;
	belle_sip_response_t *response_msg;
	string uri_as_string(tmp);
	belle_sip_free(tmp);

	list<belle_sip_header_authorization_t *> authorizations_headers;
	// add header authorization
	for (belle_sip_header_authorization_t *authorization =
	         belle_sip_message_get_header_by_type(req, belle_sip_header_authorization_t);
	     authorization != NULL; authorization = (belle_sip_header_authorization_t *)belle_sip_header_get_next(
	                                BELLE_SIP_HEADER(authorization))) {
		authorizations_headers.push_back(authorization);
	}
	// add header proxy-authorization
	for (belle_sip_header_authorization_t *authorization = BELLE_SIP_HEADER_AUTHORIZATION(
	         belle_sip_message_get_header_by_type(req, belle_sip_header_proxy_authorization_t));
	     authorization != NULL; authorization = (belle_sip_header_authorization_t *)belle_sip_header_get_next(
	                                BELLE_SIP_HEADER(authorization))) {
		authorizations_headers.push_back(authorization);
	}

	for (belle_sip_header_authorization_t *authorization : authorizations_headers) {

		if (string("Digest") == belle_sip_header_authorization_get_scheme(authorization) &&
		    mAuthMode.find(BELLE_SIP_AUTH_MODE_HTTP_DIGEST) != mAuthMode.end()) {
			if (mAuthorizedUsers.find(belle_sip_header_authorization_get_username(authorization)) ==
			    mAuthorizedUsers.end())
				continue;
			belle_sip_uri_t *authorized_user =
			    mAuthorizedUsers[belle_sip_header_authorization_get_username(authorization)];
			string secret(belle_sip_uri_get_user_password(authorized_user));
			char ha1[33], ha2[33], response[33];
			belle_sip_auth_helper_compute_ha1(belle_sip_header_authorization_get_username(authorization),
			                                  belle_sip_header_authorization_get_realm(authorization), secret.c_str(),
			                                  ha1);
			belle_sip_auth_helper_compute_ha2(belle_sip_request_get_method(req), uri_as_string.c_str(), ha2);
			belle_sip_auth_helper_compute_response(ha1, belle_sip_header_authorization_get_nonce(authorization), ha2,
			                                       response);
			if (strcmp(response, belle_sip_header_authorization_get_response(authorization)) == 0) {
				BCTBX_SLOGI << "Digest auth sucessfull";
				return true;
			}
		}
		if (string("Bearer") == belle_sip_header_authorization_get_scheme(authorization) &&
		    mAuthMode.find(BELLE_SIP_AUTH_MODE_HTTP_BEARER) != mAuthMode.end()) {
			belle_sip_uri_t *from = belle_sip_header_address_get_uri(
			    BELLE_SIP_HEADER_ADDRESS(belle_sip_message_get_header_by_type(req, belle_sip_header_from_t)));

			if (mAuthorizedUsers.find(belle_sip_uri_get_user(from)) == mAuthorizedUsers.end()) continue;
			belle_sip_uri_t *authorized_user = mAuthorizedUsers[belle_sip_uri_get_user(from)];
			const char *param =
			    belle_sip_parameters_get_parameter(BELLE_SIP_PARAMETERS(authorized_user), "access_token");
			string local_access_token = param ? param : nullptr;
			string presented_token =
			    (const char *)((belle_sip_param_pair_t *)bctbx_list_get_data(
			                       belle_sip_parameters_get_parameters(BELLE_SIP_PARAMETERS(authorization))))
			        ->name;

			if (local_access_token == presented_token) {
				belle_sip_message("Bearer auth sucessfull");
				return true;
			}
		} else {
			throw BCTBX_EXCEPTION << "Unsupported schema [" << belle_sip_header_authorization_get_scheme(authorization)
			                      << "]";
		}
	}
	// ask for auth
	belle_sip_server_transaction_t *trans = belle_sip_provider_create_server_transaction(getAgent().getProv(), req);
	response_msg = belle_sip_response_create_from_request(req, isUa ? 401 : 407);

	if (mAuthMode.find(BELLE_SIP_AUTH_MODE_HTTP_DIGEST) != mAuthMode.end()) {
		char nonce[NONCE_SIZE];
		belle_sip_random_token((nonce), NONCE_SIZE);

		belle_sip_header_www_authenticate_t *www_authenticate =
		    isUa ? belle_sip_header_www_authenticate_new()
		         : BELLE_SIP_HEADER_WWW_AUTHENTICATE(belle_sip_header_proxy_authenticate_new());

		belle_sip_header_www_authenticate_set_realm(www_authenticate, getAgent().getDomain().c_str());
		belle_sip_header_www_authenticate_set_domain(www_authenticate, ("sip:" + getAgent().getDomain()).c_str());
		belle_sip_header_www_authenticate_set_scheme(www_authenticate, "Digest");
		belle_sip_header_www_authenticate_set_nonce(www_authenticate, nonce);
		belle_sip_message_add_header(BELLE_SIP_MESSAGE(response_msg), BELLE_SIP_HEADER(www_authenticate));
	}

	if (mAuthMode.find(BELLE_SIP_AUTH_MODE_HTTP_BEARER) != mAuthMode.end()) {
		belle_sip_header_www_authenticate_t *www_authenticate = belle_sip_header_www_authenticate_new();
		belle_sip_header_www_authenticate_set_realm(www_authenticate, getAgent().getDomain().c_str());
		belle_sip_header_www_authenticate_set_scheme(www_authenticate, "Bearer");
		if (!mAuthzServer.empty()) {
			std::string authzvalue = "\"" + mAuthzServer + "\"";
			belle_sip_parameters_set_parameter(BELLE_SIP_PARAMETERS(www_authenticate), "authz_server",
			                                   authzvalue.c_str());
		}
		belle_sip_message_add_header(BELLE_SIP_MESSAGE(response_msg), BELLE_SIP_HEADER(www_authenticate));
	}

	belle_sip_server_transaction_send_response(trans, response_msg);
	return false;
}

/**************************http**************/
HttpServer::HttpServer() {
	Get("/", [](const httplib::Request &, httplib::Response &res) { res.set_content("Hello World!", "text/plain"); });
	mListeningPort = std::to_string(bind_to_any_port("127.0.0.1"));
	mRootUrl = "http://localhost:" + mListeningPort;
	set_idle_interval(0, 10000);
	set_keep_alive_max_count(1); // process one request and close - done to avoid to wait for connection timeout.
	set_keep_alive_timeout(1);

	mSrvThread = std::thread([&] { listen_after_bind(); });
	wait_until_ready();
}
HttpServer::~HttpServer() {
	stop();
	mSrvThread.join();
}

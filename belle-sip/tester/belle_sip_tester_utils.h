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

#ifndef BELLE_SIP_TESTER_UTILS_H
#define BELLE_SIP_TESTER_UTILS_H

#include "bctoolbox/tester.h"
#include "belle-sip/belle-sip.h"
#define MULTIPART_BOUNDARY "---------------------------14737809831466499882746641449"

#ifdef __cplusplus
#include "httplib.h"
#include <list>
#include <map>
#include <set>
#include <thread>

#ifdef _WIN32
// Disable C4251 triggered by need to export all stl template classes
#pragma warning(disable : 4251)
#endif // ifdef _WIN32

namespace bellesip {
class BasicSipAgent;

class BELLESIP_EXPORT Module {
public:
	Module(){};
	virtual ~Module(){};
	virtual bool onRequest(const belle_sip_request_event_t *event);
	virtual bool onResponse(const belle_sip_response_event_t *event);
	virtual BasicSipAgent &getAgent() = 0;
};

class BELLESIP_EXPORT BasicSipAgent {
public:
	BasicSipAgent(std::string domain, std::string transport = "UDP");
	virtual ~BasicSipAgent();
	const belle_sip_uri_t *getListeningUri();
	std::string getListeningUriAsString();
	belle_sip_stack_t *getStack() {
		return mStack;
	};
	belle_sip_provider_t *getProv() {
		return mProv;
	};
	;
	std::string &getDomain() {
		return mDomain;
	};
	void addModule(Module *module);
	const std::list<Module *> &getModules() const {
		return mModules;
	}

private:
	static void process_dialog_terminated(void *user_ctx, const belle_sip_dialog_terminated_event_t *event);
	static void process_io_error(void *user_ctx, const belle_sip_io_error_event_t *event);
	static void process_request_event(void *user_ctx, const belle_sip_request_event_t *event);
	static void process_response_event(void *user_ctx, const belle_sip_response_event_t *event);
	static void process_timeout(void *user_ctx, const belle_sip_timeout_event_t *event);
	std::list<Module *> mModules;
	belle_sip_stack_t *mStack;
	belle_sip_provider_t *mProv;
	belle_sip_listener_t *mListener;
	belle_sip_listening_point_t *mListeningPoint;
	belle_sip_listener_callbacks_t mListenerCallbacks;
	std::string mDomain;
	std::thread mSrvThread;
};

class QuickSipAgent : public BasicSipAgent {
public:
	using RequestHandler = std::function<bool(QuickSipAgent &, const belle_sip_request_event_t *event)>;
	using ResponseHandler = std::function<bool(QuickSipAgent &, const belle_sip_response_event_t *event)>;
	QuickSipAgent(const std::string &domain, const std::string &transport = "UDP")
	    : BasicSipAgent(domain, transport), mQuickModule(*this) {
		addModule(&mQuickModule);
	}
	void setRequestHandler(const RequestHandler &h) {
		mQuickModule.mOnRequest = h;
	}
	void setResponseHandler(const ResponseHandler &h) {
		mQuickModule.mOnResponse = h;
	}
	void doLater(const std::string &name, const std::function<void()> &action, int milliseconds) {
		belle_sip_source_t *bs = belle_sip_main_loop_create_cpp_timeout_2(
		    belle_sip_stack_get_main_loop(getStack()),
		    [action]() -> bool {
			    action();
			    return false;
		    },
		    milliseconds, name);
		belle_sip_object_unref(bs);
	}

private:
	class QuickModule : public Module {
	public:
		QuickModule(QuickSipAgent &agent) : mAgent(agent) {
		}
		virtual bool onRequest(const belle_sip_request_event_t *event) override {
			return mOnRequest ? mOnRequest(mAgent, event) : true;
		}
		virtual bool onResponse(const belle_sip_response_event_t *event) override {
			return mOnResponse ? mOnResponse(mAgent, event) : true;
		}
		virtual BasicSipAgent &getAgent() override {
			return mAgent;
		}
		RequestHandler mOnRequest;
		ResponseHandler mOnResponse;
		QuickSipAgent &mAgent;
	};
	QuickModule mQuickModule;
};

/**
 * @brief Basic sip registrar server.
 *
 *
 * sample of use:
 * 	BasicRegistrar registrar("sipopen.example.org","UDP");
 *
 *	belle_sip_request_t *req = req = belle_sip_request_create(belle_sip_uri_parse("sip:anyuser@sipopen.example.org")
 *															  , "REGISTER"
 *															  ,	belle_sip_provider_create_call_id(prov)
 *															  , belle_sip_header_cseq_create(20, "REGISTER"),
 *															  , belle_sip_header_from_create2(identity,
 *BELLE_SIP_RANDOM_TAG) ,	belle_sip_header_to_create2(identity, NULL) , belle_sip_header_via_new(), 70);
 *
 *	belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),
 *BELLE_SIP_HEADER(belle_sip_header_route_parse((registrar.getAgent().getListeningUriAsString())));
 *	belle_sip_provider_send_request(prov, req,);
 *
 *
 *
 */
class BELLESIP_EXPORT BasicRegistrar : public Module {
public:
	/**
	 * @brief ctr
	 * @param domain sip domain for wich registration request are accepted
	 * @param sip transport registrar is listening to. Possible value are either "udp" or "tcp".
	 */
	BasicRegistrar(std::string domain, std::string transport = "UDP");
	virtual BasicSipAgent &getAgent() override {
		return mAgent;
	};

protected:
	virtual bool onRequest(const belle_sip_request_event_t *event) override;

private:
	BasicSipAgent mAgent;
};
/**
 * @brief Basic Authenticated sip registrar server.
 * Derivated from @BasicRegistrar but with autheticated REGISTER.
 *
 *
 * sample of use:
 * 	AuthenticatedRegistrar registrar({"sip:user@sip.example.org;access_token=ae5fesycneb"}, "sip.example.org","TCP");
 *	registrar.addAuthMode(BELLE_SIP_AUTH_MODE_HTTP_BEARER);

 *
 *	belle_sip_request_t *req = req = belle_sip_request_create(belle_sip_uri_parse("sip:user@sip.example.org")
 *															  , "REGISTER"
 *															  ,	belle_sip_provider_create_call_id(prov)
 *															  , belle_sip_header_cseq_create(20, "REGISTER"),
 *															  , belle_sip_header_from_create2(identity,
 BELLE_SIP_RANDOM_TAG) *															  ,
 belle_sip_header_to_create2(identity, NULL) *															  ,
 belle_sip_header_via_new(), 70);
 *
 *	belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),
 BELLE_SIP_HEADER(belle_sip_header_authorisation_parse("sip:Authorizarion: Bearer ae5fesycneb)));
 belle_sip_message_add_header(BELLE_SIP_MESSAGE(req),
 BELLE_SIP_HEADER(belle_sip_header_route_parse((registrar.getAgent().getListeningUriAsString())));
 *
 belle_sip_provider_send_request(prov, req,);
 *
 *
 *
 */

class BELLESIP_EXPORT AuthenticatedRegistrar : public Module {
public:
	class Nonce {
		std::string value;
		// unsigned int count = 0;
	};
	/**
	 * @brief ctr
	 * @param users_uri list of uris representing users. For digest auth, username and password must be filled. For
	 * Bearer, acces_token parameter shal be provided. ex for Digest: sip:user:secret@sip.example.org ex for Bearer:
	 * sip:user@sip.example.org;access_token=ae5fesycneb
	 * @param domain sip domain for wich registration request are accepted
	 * @param sip transport registrar is listening to. Possible value are either "udp" or "tcp".
	 */
	AuthenticatedRegistrar(std::set<std::string> users_uri, std::string domain, std::string transport = "UDP");
	virtual ~AuthenticatedRegistrar(){};
	/*
	 Configure auth mode, currently only Digest and Bearer are implemented
	 */
	void addAuthMode(belle_sip_auth_mode_t mode);
	void setAuthzServer(const std::string &authzServer);
	virtual BasicSipAgent &getAgent() override {
		return mRegistrar.getAgent();
	};

private:
	virtual bool onRequest(const belle_sip_request_event_t *event) override;
	std::map<std::string, belle_sip_uri_t *> mAuthorizedUsers;
	std::set<belle_sip_auth_mode_t> mAuthMode;
	std::map<std::string, Nonce> mNonces;
	std::string mAuthzServer;
	BasicRegistrar mRegistrar;
};

/*
 *Not impelmented yet
 */
class BELLESIP_EXPORT ProxyRegistrar : BasicRegistrar {
	ProxyRegistrar(std::string domain, std::string transport = "UDP");
	virtual ~ProxyRegistrar(){};
};

// HTTP
class BELLESIP_EXPORT HttpServer : public httplib::Server {
public:
	HttpServer();
	~HttpServer();
	std::string mListeningPort;
	std::string mRootUrl;

private:
	std::thread mSrvThread;
};

} // namespace bellesip
#endif

#endif // BELLE_SIP_TESTER_UTILS_H

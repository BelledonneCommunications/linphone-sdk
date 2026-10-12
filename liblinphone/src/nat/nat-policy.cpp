/*
 * Copyright (c) 2010-2026 Belledonne Communications SARL.
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

#include "nat-policy.h"

#include <regex>
#include <stdexcept>

#include "auth-info/auth-info.h"
#include "core/core.h"
#include "mediastreamer2/turn-transport.h"
#include "private.h"

using namespace mediastreamer::nat;

LINPHONE_BEGIN_NAMESPACE

NatPolicy::AsyncHandle NatPolicy::sAsyncHandleGenerator = 0;

NatPolicy::NatPolicy(const std::shared_ptr<Core> &core, const ConstructionMethod method, const std::string &value)
    : CoreAccessor(core) {
	LpConfig *config = linphone_core_get_config(core->getCCore());
	char ref[17] = {0};
	belle_sip_random_token(ref, 16);
	mRef = ref;

	if (method == ConstructionMethod::Default) {
		enableTurnUdp(true); // If not specified explicitly, activate at least UDP transport for TURN
	} else if (method == ConstructionMethod::FromSectionName) {
		initFromSection(config, value.c_str());
	} else if (method == ConstructionMethod::FromRefName) {
		for (int index = 0;; index++) {
			std::ostringstream section;
			section << "nat_policy_" << index;
			if (linphone_config_has_section(config, section.str().c_str()) != FALSE) {
				const char *config_ref = linphone_config_get_string(config, section.str().c_str(), "ref", nullptr);
				if ((config_ref != nullptr) && (strcmp(config_ref, value.c_str()) == 0)) {
					initFromSection(config, section.str().c_str());
					break;
				}
			} else {
				lError() << "There is no NatPolicy with ref [" << value << "]";
				break;
			}
		}
	}
}

NatPolicy::NatPolicy(const NatPolicy &other)
    : HybridObject<LinphoneNatPolicy, NatPolicy>(other),
      std::enable_shared_from_this<LinphonePrivate::NatPolicy>(other), CoreAccessor(other.getCore()) {
	mStunServer = other.mStunServer;
	mStunServerUsername = other.mStunServerUsername;
	mRef = other.mRef;
	if (mRef.empty()) {
		char ref[17] = {0};
		belle_sip_random_token(ref, 16);
		mRef = ref;
	}
	mStunEnabled = other.mStunEnabled;
	mTurnEnabled = other.mTurnEnabled;
	mIceEnabled = other.mIceEnabled;
	mUpnpEnabled = other.mUpnpEnabled;
	mTurnConfigurationEndpoint = other.mTurnConfigurationEndpoint;
	mTurnTransports = other.mTurnTransports;
}

NatPolicy::~NatPolicy() {
	clearResolverContexts();
}

NatPolicy *NatPolicy::clone() const {
	return new NatPolicy(*this);
}

void NatPolicy::clearResolverContexts() {
	for (const auto transport : {TurnTransport::Udp, TurnTransport::Tcp, TurnTransport::Tls}) {
		const auto itContext = mStunResolverContexts.find(transport);
		if (itContext != mStunResolverContexts.end()) {
			itContext->second.cancel();
			itContext->second.reset();
		}
		if (mResolverResults[transport]) {
			belle_sip_object_unref(mResolverResults[transport].value());
			mResolverResults[transport] = std::nullopt;
		}
	}
}

/* Simply cancel pending DNS resolution, as the core is going to shutdown.*/
void NatPolicy::release() {
	clearResolverContexts();
	mResolverResultsFunctions.clear();
}

bool NatPolicy::stunServerActivated() const {
	return !mStunServer.empty() && (mStunEnabled || mTurnEnabled);
}

void NatPolicy::saveToConfig(LinphoneConfig *config, int index) const {
	char *section = belle_sip_strdup_printf("nat_policy_%i", index);
	linphone_config_set_string(config, section, "ref", mRef.c_str());
	linphone_config_set_string(config, section, "stun_server", mStunServer.c_str());
	linphone_config_set_string(config, section, "stun_server_username", mStunServerUsername.c_str());
	bctbx_list_t *l = nullptr;
	if (mUpnpEnabled) {
		l = bctbx_list_append(l, (void *)"upnp");
	} else {
		if (mStunEnabled) {
			l = bctbx_list_append(l, (void *)"stun");
		}
		if (mTurnEnabled) {
			l = bctbx_list_append(l, (void *)"turn");
		}
		if (mIceEnabled) {
			l = bctbx_list_append(l, (void *)"ice");
		}
	}
	linphone_config_set_string_list(config, section, "protocols", l);
	belle_sip_free(section);
	bctbx_list_free(l);
}

void NatPolicy::clearConfigFromIndex(LinphoneConfig *config, int index) {
	int purged = 0;
	while (true) {
		std::ostringstream ostr;
		ostr << "nat_policy_" << index;
		if (linphone_config_has_section(config, ostr.str().c_str()) != FALSE) {
			linphone_config_clean_section(config, ostr.str().c_str());
			++purged;
		} else {
			break;
		}
		++index;
	}
	/* Warn about suspicious number of orphan NatPolicy sections.
	 * A bug occurred December 2022 caused a massive leak of nat_policy_X sections in configuration file.
	 */
	if (purged > 5) {
		lWarning() << "Purged [" << purged << "] unused NatPolicy sections from config file.";
	}
}

void NatPolicy::clear() {
	clearResolverContexts();
	mStunServer.clear();
	mStunServerUsername.clear();
	mStunEnabled = false;
	mTurnEnabled = false;
	mIceEnabled = false;
	mUpnpEnabled = false;
	mTurnTransports.clear();
}

void NatPolicy::setStunServer(const std::string &stunServer) {
	mStunServer = stunServer;
	clearResolverContexts();
}

const std::string &NatPolicy::getStunServer() const {
	return mStunServer;
}

void NatPolicy::setStunServerUsername(const std::string &stunServerUsername) {
	mStunServerUsername = stunServerUsername;
}
const std::string &NatPolicy::getStunServerUsername() const {
	return mStunServerUsername;
}

void NatPolicy::setNatV4Address(const std::string &natV4Address) {
	mNatV4Address = natV4Address;
}

const std::string &NatPolicy::getNatV4Address() const {
	return mNatV4Address;
}

void NatPolicy::setNatV6Address(const std::string &natV6Address) {
	mNatV6Address = natV6Address;
}
const std::string &NatPolicy::getNatV6Address() const {
	return mNatV6Address;
}

void NatPolicy::stunServerResolved(const TurnTransport transport, belle_sip_resolver_results_t *results) {
	if (mResolverResults[transport]) {
		belle_sip_object_unref(mResolverResults[transport].value());
		mResolverResults[transport] = std::nullopt;
	}

	if (belle_sip_resolver_results_get_addrinfos(results) == nullptr) {
		lWarning() << (mTurnEnabled ? "Turn" : "Stun") << (mTurnEnabled ? " " + turnTransportAsString(transport) : "")
		           << " server resolution failed.";
		mResolverResults[transport] = nullptr;
	} else {
		lInfo() << (mTurnEnabled ? "Turn" : "Stun") << (mTurnEnabled ? " " + turnTransportAsString(transport) : "")
		        << " server resolution successful.";
		belle_sip_object_ref(results);
		mResolverResults[transport] = results;
	}
	if (mStunResolverContexts[transport]) {
		mStunResolverContexts[transport].reset();
	}

	if (resolutionCompletedForAllTransports()) {
		const auto resolutionResults = getResolutionResults();
		/*
		 * - Make a copy of the notify functions to invoke, to prevent a risk that the map is being
		 * modified while being iterated.
		 * - Protect against a possible deletion of the NatPolicy object while invoking the notify functions.
		 */
		ref();
		const auto copyOfResultFunctions = mResolverResultsFunctions;
		for (const auto &[_handle, func] : copyOfResultFunctions) {
			func(resolutionResults);
		}
		mResolverResultsFunctions.clear();
		unref();
	}
}

bool NatPolicy::resolveStunServer() {
	LinphoneCore *lc = getCore()->getCCore();

	if (stunServerActivated() && (lc->sal != nullptr) && mStunResolverContexts.empty()) {
		const char *service = nullptr;
		char host[NI_MAXHOST];
		int port = 0;
		linphone_parse_host_port(mStunServer.c_str(), host, sizeof(host), &port);
		if (mTurnEnabled) {
			service = "turn";
		} else if (mStunEnabled) {
			service = "stun";
		}
		if (service != nullptr) {
			int family = AF_INET;
			if (linphone_core_ipv6_enabled(lc) == TRUE) {
				family = AF_INET6;
			}
			lInfo() << "Starting " << service << " server resolution [" << host << "]";
			const auto lambda = [this](belle_sip_resolver_results_t *results) {
				this->stunServerResolved(TurnTransport::Udp, results);
			};
			if (port == 0) {
				// Use default ports
				if (mTurnEnabled) {
					for (const auto transport : mTurnTransports) {
						const auto turnLambda = [this, transport](belle_sip_resolver_results_t *results) {
							this->stunServerResolved(transport, results);
						};
						mStunResolverContexts[transport] = lc->sal->resolve(
						    service, turnTransportAsString(transport), host,
						    (transport == TurnTransport::Tls) ? sDefaultTurnTlsServerPort : sDefaultTurnServerPort,
						    family, turnLambda);
					}
				} else if (mStunEnabled) {
					mStunResolverContexts[TurnTransport::Udp] =
					    lc->sal->resolve(service, turnTransportAsString(TurnTransport::Udp), host,
					                     sDefaultStunServerPort, family, lambda);
				}
			} else {
				mStunResolverContexts[TurnTransport::Udp] = lc->sal->resolveA(host, port, family, lambda);
			}
			return true;
		}
	}
	return false;
}

std::map<TurnTransport, const struct addrinfo *> NatPolicy::getStunServerAddrinfo() {
	/*
	 * It is critical not to block for a long time if it can't be resolved, otherwise this stucks the main thread when
	 * making a call. On the contrary, a fully asynchronous call initiation is complex to develop. The compromise is
	 * then:
	 *  - have a cache of the stun server addrinfo
	 *  - this cached value is returned when it is non-null
	 *  - an asynchronous resolution is asked each time this function is called to ensure frequent refreshes of the
	 * cached value.
	 *  - if no cached value exists, block for a short time; this case must be unprobable because the resolution will be
	 * asked each time the stun server value is changed.
	 */
	if (stunServerActivated() && !resolutionCompletedForAllTransports()) {
		int wait_ms = 0;
		constexpr int wait_limit = 1000;
		resolveStunServer();
		while (!resolutionCompletedForAllTransports() && !mStunResolverContexts.empty() && (wait_ms < wait_limit)) {
			getCore()->getCCore()->sal->iterate();
			ms_usleep(10000);
			wait_ms += 10;
		}
	}
	return getResolutionResults();
}

NatPolicy::AsyncHandle NatPolicy::getStunServerAddrinfoAsync(const ResolverResultsFn &onResults) {
	if (stunServerActivated() && !resolutionCompletedForAllTransports()) {
		/* need to request resolution */
		mResolverResultsFunctions[++sAsyncHandleGenerator] = onResults;
		if (resolveStunServer()) {
			return sAsyncHandleGenerator;
		}
	} else {
		/* results already available, invoke the lambda immediately.*/
		onResults(getResolutionResults());
	}
	return 0;
}

void NatPolicy::cancelAsync(AsyncHandle h) {
	const auto it = mResolverResultsFunctions.find(h);
	if (it != mResolverResultsFunctions.end()) {
		mResolverResultsFunctions.erase(it);
	} else {
		lError() << "NatPolicy: no AsyncHandle with id " << h;
	}
}

void NatPolicy::initFromSection(const LinphoneConfig *config, const char *section) {
	const char *ref = linphone_config_get_string(config, section, "ref", nullptr);
	if (ref != nullptr) {
		mRef = ref;
	}
	mStunServer = linphone_config_get_string(config, section, "stun_server", "");
	mStunServerUsername = linphone_config_get_string(config, section, "stun_server_username", "");
	enableTurnUdp(linphone_config_get_bool(config, section, "turn_enable_udp", TRUE) != FALSE);
	enableTurnTcp(linphone_config_get_bool(config, section, "turn_enable_tcp", FALSE) != FALSE);
	enableTurnTls(linphone_config_get_bool(config, section, "turn_enable_tls", FALSE) != FALSE);
	bctbx_list_t *l = linphone_config_get_string_list(config, section, "protocols", nullptr);

	if (l != nullptr) {
		for (const bctbx_list_t *elem = l; elem != nullptr; elem = elem->next) {
			const std::string value(static_cast<const char *>(elem->data));
			if (value == "stun") {
				enableStun(true);
			} else if (value == "turn") {
				enableTurn(true);
			} else if (value == "ice") {
				enableIce(true);
			} else if (value == "upnp") {
				mUpnpEnabled = true;
			}
		}
		bctbx_list_free_with_data(l, bctbx_free);
	}
}

const std::string &NatPolicy::getTurnConfigurationEndpoint() const {
	return mTurnConfigurationEndpoint;
}

void NatPolicy::setTurnConfigurationEndpoint(const std::string &endpoint) {
	mTurnConfigurationEndpoint = endpoint;
}

bool NatPolicy::processJsonConfigurationResponse(const HttpResponse &response) {
	bool ret = true;
	try {
		if (response.getHttpStatusCode() != 200) {
			lError() << "Failed to fetch TURN credentials. HTTP Status Code: " << response.getHttpStatusCode();
			throw std::runtime_error("Failed to fetch TURN credentials: Invalid HTTP status code.");
		}

		const auto &responseBody = response.getBody();
		JsonDocument doc(responseBody);
		const Json::Value &root = doc.getRoot();

		if (root.isNull() || !root.isObject()) {
			lError() << "Invalid JSON response for TURN credentials: " << responseBody;
			throw std::runtime_error("Invalid JSON response for TURN credentials.");
		}

		constexpr auto usernameKey = "username";
		constexpr auto passwordKey = "password";
		constexpr auto timeToLeaveKey = "ttl";
		constexpr auto urisKey = "uris";

		if (!root.isMember(usernameKey) || !root[usernameKey].isString() || !root.isMember(passwordKey) ||
		    !root[passwordKey].isString()) {
			lError() << "Missing required fields in TURN credentials JSON response: " << responseBody;
			throw std::runtime_error("Missing required fields in TURN credentials JSON response.");
		}

		auto turnConfiguration =
		    AuthInfo::create(root[usernameKey].asString(), "", root[passwordKey].asString(), "", "", "", "");

		if (root.isMember(timeToLeaveKey) && root[timeToLeaveKey].isInt()) {
			// To avoid potential configuration expiration before a new request can be made, the expiration time
			// is set to 80% of the given duration. This ensures a buffer period for timely renewal.
			auto expiration = static_cast<int>(root[timeToLeaveKey].asInt() * 0.8);
			time_t expires = time(nullptr) + expiration;
			turnConfiguration->setExpires(expires);
		}

		linphone_core_add_auth_info(getCore()->getCCore(), turnConfiguration->toC());

		if (root.isMember(urisKey) && root[urisKey].isArray() && !root[urisKey].empty()) {
			auto firstUri = root[urisKey].begin();
			if (firstUri->isString()) {
				auto uri = firstUri->asString();
				/* This regex pattern extracts the part after "turn:" and before "?" in a TURN URI.
				 * ^turn:      -> Matches the literal "turn:" at the beginning of the string.
				 * ([^?]+)     -> Captures everything after "turn:" up to (but not including) the first "?".
				 * The captured group (match[1]) contains the desired hostname and port.
				 * */
				std::regex pattern(R"(^turn:([^?]+))");
				std::smatch match;
				if (std::regex_search(uri, match, pattern)) {
					mStunServer = match[1].str();
				}
			}
		}

		mStunServerUsername = root[usernameKey].asString();

		lInfo() << "TURN credentials successfully updated.";

	} catch (const std::exception &e) {
		lError() << "Exception while updating TURN credentials: " << e.what();
		ret = false;
	}
	return ret;
}

void NatPolicy::updateTurnConfiguration(const std::optional<std::shared_ptr<Address>> &from,
                                        const std::function<void(bool)> &completionRoutine) {
	try {
		mCompletionRoutine = completionRoutine;
		auto &httpClient = getCore()->getHttpClient();
		auto &httpRequest = httpClient.createRequest("GET", mTurnConfigurationEndpoint);
		if (from) {
			auto *fromAddress = from.value()->clone();
			fromAddress->clean();          // Remove gruu
			fromAddress->setScheme("sip"); // TODO: FAM v2.0.6 doesn't support sips. Force to sip.
			httpRequest.setAuthInfo(fromAddress->getUsername(), fromAddress->getDomain());
			httpRequest.addHeader("From", fromAddress->asStringUriOnly());
			delete fromAddress;
		}
		std::weak_ptr<NatPolicy> natPolicyRef = shared_from_this();
		httpRequest.execute([natPolicyRef](const HttpResponse &response) -> void {
			auto sharedNatPolicy = natPolicyRef.lock();
			if (sharedNatPolicy) {
				bool ret = sharedNatPolicy->processJsonConfigurationResponse(response);
				if (sharedNatPolicy->mCompletionRoutine) {
					sharedNatPolicy->mCompletionRoutine(ret);
				}
			}
		});
	} catch (const std::exception &e) {
		lError() << __func__
		         << ": Error while creating or sending HTTP request to update TURN configuration : " << e.what();
	}
}

bool NatPolicy::needToUpdateTurnConfiguration() const {
	if (!mTurnConfigurationEndpoint.empty()) {
		const auto *ai =
		    linphone_core_find_auth_info(getCore()->getCCore(), nullptr, mStunServerUsername.c_str(), nullptr);
		if (ai == nullptr) {
			return true;
		}
	}
	return false;
}

void NatPolicy::cancelTurnConfigurationUpdate() {
	mCompletionRoutine = nullptr;
}

void NatPolicy::enableTurnTransport(const TurnTransport transport, const bool enable) {
	if (enable) {
		mTurnTransports.insert(transport);
	} else {
		const auto it = mTurnTransports.find(transport);
		if (it != mTurnTransports.end()) {
			mTurnTransports.erase(it);
		}
	}
}

bool NatPolicy::turnTransportEnabled(const TurnTransport transport) const {
	return mTurnTransports.find(transport) != mTurnTransports.end();
}

bool NatPolicy::resolutionCompletedForAllTransports() const {
	try {
		if (mTurnEnabled) {
			return std::all_of(mTurnTransports.begin(), mTurnTransports.end(),
			                   [this](const auto transport) { return mResolverResults.at(transport).has_value(); });
		}
		if (mStunEnabled) {
			return mResolverResults.at(TurnTransport::Udp).has_value();
		}
	} catch (std::out_of_range &) {
		return false;
	}
	return true;
}

std::map<TurnTransport, const struct addrinfo *> NatPolicy::getResolutionResults() const {
	std::map<TurnTransport, const struct addrinfo *> results;
	if (mTurnEnabled) {
		for (const auto transport : mTurnTransports) {
			try {
				if (mResolverResults.at(transport)) {
					results[transport] =
					    belle_sip_resolver_results_get_addrinfos(mResolverResults.at(transport).value());
				}
			} catch (std::out_of_range &) {
			}
		}
	} else if (mStunEnabled) {
		try {
			if (mResolverResults.at(TurnTransport::Udp)) {
				results[TurnTransport::Udp] =
				    belle_sip_resolver_results_get_addrinfos(mResolverResults.at(TurnTransport::Udp).value());
			}
		} catch (std::out_of_range &) {
		}
	}
	return results;
}

LINPHONE_END_NAMESPACE

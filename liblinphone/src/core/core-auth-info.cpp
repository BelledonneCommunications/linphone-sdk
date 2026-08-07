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

#include "auth-info/auth-info.h"
#include "core-p.h"
#include "core.h"

using namespace std;
LINPHONE_BEGIN_NAMESPACE

shared_ptr<AuthInfo> Core::findAuthInfoToBeReplaced(const std::shared_ptr<AuthInfo> &authInfo) const {
	const auto &authInfos = mAuthInfos.mList;
	for (auto &coreAuthInfo : authInfos) {
		if (coreAuthInfo->isEqualButAlgorithms(authInfo.get())) {
			if (bool(coreAuthInfo->getAccessToken()) == bool(authInfo->getAccessToken())) return coreAuthInfo;
		}
	}
	return nullptr;
}

void Core::abortAuthentication(BCTBX_UNUSED(const std::shared_ptr<AuthInfo> &authInfo)) {
	size_t count = getHttpClient().abortPendingRequests();
	lInfo() << "Core::abortAuthentication: Aborting " << count << " requests awaiting authentication information.";
}

shared_ptr<AuthInfo> Core::createAuthInfo(const std::string &username,
                                          const std::string &userid,
                                          const std::string &password,
                                          const std::string &ha1,
                                          const std::string &realm,
                                          const std::string &domain) {
	return AuthInfo::create(username, userid, password, ha1, realm, domain);
}

shared_ptr<AuthInfo>
Core::findAuthInfo(const std::string &username, const std::string &realm, const std::string &domain) {
	if (username.empty() && realm.empty() && domain.empty()) {
		lError() << "Core::findAuthInfo: Looking for an auth info but all search criteria are null!";
		return nullptr;
	}
	shared_ptr<AuthInfo> bestAuthInfo = nullptr;
	int bestScore = 0;
	const auto authInfos = mAuthInfos.mList;
	for (auto &authInfo : authInfos) {
		// remove expired auth-infos
		if (authInfo->getExpires() != 0 && authInfo->getExpires() <= time(nullptr)) {
			removeAuthInfo(authInfo);
			continue;
		}
		int score = 0;
		if (!username.empty()) {
			if (authInfo->getUsername().empty() || authInfo->getUsername() != username) continue;
		}
		++score;
		if (!realm.empty() && !authInfo->getRealm().empty()) {
			if ((Utils::unquote(realm, '"', '"')) != authInfo->getRealm()) ++score;
		}
		if (!authInfo->getDomain().empty() && !domain.empty() &&
		    (Utils::isDomainMatchingWildcard(authInfo->getDomain(), domain)))
			++score;
		if (score > bestScore) {
			bestScore = score;
			bestAuthInfo = authInfo;
		}
	}
	return bestAuthInfo;
};

std::optional<TlsCertificate> Core::findTlsCertInIndexedAuthInfosWithSubject(const std::string &username,
                                                                             const std::string &domain,
                                                                             const std::string &subject) const {
	const auto &authInfos = mAuthInfos.mList;
	TlsCertificate tlsCertificate;

	for (auto &authInfo : authInfos) {
		if (authInfo->isSuitableForChallenge(LinphoneAuthTls, username, "", domain, "") > 0) {
			auto tlsCert = authInfo->getTlsCert();
			auto tlsCertPath = authInfo->getTlsCertPath();
			auto tlsKey = authInfo->getTlsKey();
			auto tlsKeyPath = authInfo->getTlsKeyPath();
			belle_sip_certificates_chain_t *bs_cert_chain = nullptr;
			belle_sip_signing_key_t *bs_key = nullptr;

			if (!tlsCert.empty() && !tlsKey.empty()) {
				bs_cert_chain = belle_sip_certificates_chain_parse(L_STRING_TO_C(tlsCert), tlsCert.size(),
				                                                   BELLE_SIP_CERTIFICATE_RAW_FORMAT_PEM);
				bs_key = belle_sip_signing_key_parse(L_STRING_TO_C(tlsKey), tlsKey.size(), nullptr);
			} else if (!tlsCertPath.empty() && !tlsKeyPath.empty()) {
				// if auth info holds a tls_cert_path and key_path, it is assumed they are files
				bs_cert_chain = belle_sip_certificates_chain_parse_file(L_STRING_TO_C(tlsCertPath),
				                                                        BELLE_SIP_CERTIFICATE_RAW_FORMAT_PEM);
				bs_key = belle_sip_signing_key_parse_file(L_STRING_TO_C(tlsKeyPath), nullptr);
			}
			if (bs_cert_chain && bs_key) {
				if (belle_sip_certificate_subject_match(bs_cert_chain, L_STRING_TO_C(subject)) == TRUE) {
					char *pemCert = nullptr;
					char *pemKey = nullptr;
					char *fp = nullptr;
					pemCert = belle_sip_certificates_chain_get_pem(bs_cert_chain);
					pemKey = belle_sip_signing_key_get_pem(bs_key);
					fp = belle_sip_certificates_chain_get_fingerprint(bs_cert_chain);
					tlsCertificate.certificatePem = L_C_TO_STRING(pemCert);
					tlsCertificate.keyPem = L_C_TO_STRING(pemKey);
					tlsCertificate.fingerprint = L_C_TO_STRING(fp);
					belle_sip_object_unref(bs_cert_chain);
					belle_sip_object_unref(bs_key);
					belle_sip_free(pemCert);
					belle_sip_free(pemKey);
					belle_sip_free(fp);
					return tlsCertificate;
				}
				belle_sip_object_unref(bs_cert_chain);
				belle_sip_object_unref(bs_key);
			} else {
				if (bs_cert_chain) belle_sip_object_unref(bs_cert_chain);
				if (bs_key) belle_sip_object_unref(bs_key);
			}
		}
	}
	return nullopt;
}

const list<shared_ptr<AuthInfo>> &Core::getAuthInfos() const {
	return mAuthInfos.mList;
}

const bctbx_list_t *Core::getAuthInfosCList() const {
	return mAuthInfos.getCList();
}

void Core::removeAuthInfo(const std::shared_ptr<AuthInfo> &authInfo) {
	auto &authInfos = mAuthInfos.mList;
	const auto authInfoIt = find(authInfos.begin(), authInfos.end(), authInfo);
	if (authInfoIt == authInfos.cend()) {
		lError() << "Core::removeAuthInfo: AuthInfo [ " << authInfo << "] is not known by Core [" << this
		         << "] (programming error?)";
		return;
	}
	authInfos.erase(authInfoIt);
	writeAuthInfos();
};

void Core::clearAuthInfos() {
	auto authInfos = mAuthInfos.mList;
	for (auto &authInfo : authInfos) {
		removeAuthInfo(authInfo);
	}
}

int Core::cleanAuthInfos() {
	int count = 0;
	auto authInfos = mAuthInfos.mList;
	for (auto &authInfo : authInfos) {
		if (authInfo->getExpires() != 0 && authInfo->getExpires() <= time(nullptr)) {
			removeAuthInfo(authInfo);
			++count;
		}
	}
	return count;
}

void Core::addAuthInfo(const std::shared_ptr<AuthInfo> &authInfo) {
	bool updating = false;
	size_t restartedOperationCount = 0;

	if ((authInfo->getTlsCert().empty() || authInfo->getTlsKey().empty()) &&
	    (authInfo->getTlsCertPath().empty() || authInfo->getTlsKeyPath().empty()) && authInfo->getHa1().empty() &&
	    authInfo->getPassword().empty() && !authInfo->getAccessToken() && !authInfo->getRefreshToken() &&
	    authInfo->getClientId().empty()) {
		lError() << "addAuthInfo(): info supplied with empty password, ha1, TLS client/key or bearer token.";
		return;
	}

	/* find if we are attempting to modify an existing auth info */
	auto existingAuthInfo = findAuthInfoToBeReplaced(authInfo);
	if (existingAuthInfo) {
		lInfo() << "addAuthInfo(): replacing existing auth info.";
		removeAuthInfo(existingAuthInfo);
		updating = true;
	}
	mAuthInfos.mList.push_back(authInfo->clone()->toSharedPtr());

	/* retry pending authentication operations */
	const auto pendingAuths = getPrivate()->getSal()->getPendingAuths();
	if (!pendingAuths.empty()) {
		lInfo() << "addAuthInfo(): Restarting operations pending authentications...";
	}

	for (const auto &operation : pendingAuths) {
		const SalAuthInfo *salAuthInfo = operation->getAuthRequested();
		const LinphoneAuthMethod method = AuthInfo::fromSalAuthMode(salAuthInfo->mode);
		auto bestAuthInfo = findBestAuthInfoForChallenge(
		    method, L_C_TO_STRING(salAuthInfo->username), L_C_TO_STRING(salAuthInfo->realm),
		    L_C_TO_STRING(salAuthInfo->domain), L_C_TO_STRING(salAuthInfo->algorithm));
		if (bestAuthInfo) {
			const auto &accounts = mAccounts.mList;
			for (const auto &account : accounts) {
				if (account->toC() == operation->getUserPointer()) {
					account->setState(LinphoneRegistrationProgress, "addAuthInfo(): Authentication...");
					break;
				}
			}
			operation->authenticate();
			++restartedOperationCount;
		}
	}
	restartedOperationCount += getHttpClient().retryPendingRequests();

	if (!pendingAuths.empty() || restartedOperationCount > 0) {
		lInfo() << "Core::addAuthInfo(): restarted " << restartedOperationCount << " operation(s) after "
		        << (updating ? "updating" : "adding") << " auth info for \n"
		        << " username: [" << authInfo->getUsername() << "] \n"
		        << " domain: [" << authInfo->getDomain() << "] \n"
		        << " realm: [" << authInfo->getRealm() << "] \n";
	}
	writeAuthInfos();
};

shared_ptr<AuthInfo> Core::findBestAuthInfoForChallenge(const LinphoneAuthMethod method,
                                                        const std::string &username,
                                                        const std::string &realm,
                                                        const std::string &domain,
                                                        const std::string &algorithm) {
	bool equality = false;
	const auto authInfos = mAuthInfos.mList;
	shared_ptr<AuthInfo> bestAuthInfo = nullptr;
	int bestAuthInfoScore = 0;
	for (const auto &authInfo : authInfos) {
		// remove expired auth-infos
		if (authInfo->getExpires() != 0 && authInfo->getExpires() <= time(nullptr)) {
			removeAuthInfo(authInfo);
			continue;
		}
		int authInfoScore = authInfo->isSuitableForChallenge(method, username, realm, domain, algorithm);
		if (authInfoScore > 0) {
			if (authInfoScore == bestAuthInfoScore) {
				equality = true;
				continue;
			}
			if (authInfoScore > bestAuthInfoScore) {
				if (equality) equality = false;
				bestAuthInfo = authInfo;
				bestAuthInfoScore = authInfoScore;
			}
		}
	}
	if (bestAuthInfo) {
		if (equality) {
			lWarning() << "Core::findBestAuthInfoForChallenge(): multiple auth infos found with the same score ("
			           << bestAuthInfoScore << "), returning the first one found. \n";
		}
		lInfo() << "Core::findBestAuthInfoForChallenge(): returning auth info: \n"
		        << "username: [" << bestAuthInfo->getUsername() << "] \n"
		        << "domain: [" << bestAuthInfo->getDomain() << "] \n"
		        << "realm: [" << bestAuthInfo->getRealm() << "] \n";
	}
	return bestAuthInfo;
};

/**
 * @brief Fill the requested authentication event from the given linphone core
 * If the authentication event is a TLS one, username(optionnal) and domain should be given as parameter
 *
 * @Warning This function assumes the authentication information were already provided to the core (at application start
 * or when registering on flexisip) and won't call the linphone_core_notify_authentication_requested to give the
 * application an other chance to fill the auth_info into the core.
 *
 * @param[in/out]	event		The belle sip auth event to fill with requested authentication credentials
 * @param[in]		username	Used only when auth mode is TLS to get a matching client certificate. If empty, look for
 * any certificate matching domain nane
 * @param[in]		domain		Used only when auth mode is TLS to get a matching client certificate.
 * @return an AuthStatus value
 * @note It is not possible to know whether the TLS server requested a client certificate: true is always returned for
 * TLS authentication.
 */
AuthStatus
Core::fillBelleSipAuthEvent(belle_sip_auth_event *event, const std::string &username, const std::string &domain) {
	auto status = AuthStatus::NoAuth;
	const std::string aeRealm = L_C_TO_STRING(belle_sip_auth_event_get_realm(event));
	const std::string aeUsername = L_C_TO_STRING(belle_sip_auth_event_get_username(event));
	const std::string aeDomain = L_C_TO_STRING(belle_sip_auth_event_get_domain(event));

	LinphoneAuthMethod requestedMethod = LinphoneAuthHttpDigest;
	int tryCount = belle_sip_auth_event_get_try_count(event);
	int maxTries = 2;

	switch (belle_sip_auth_event_get_mode(event)) {
		case BELLE_SIP_AUTH_MODE_HTTP_DIGEST: {
			const std::string aeAlgorithm = L_C_TO_STRING(belle_sip_auth_event_get_algorithm(event));
			const auto authInfo =
			    findBestAuthInfoForChallenge(LinphoneAuthHttpDigest, aeUsername, aeRealm, aeDomain, aeAlgorithm);
			if (authInfo) {
				authInfo->fillBelleSipEvent(event);
				/* If the the algorithm was not set in the auth-info, now set it so that the clear text
				 * password can be transformed in ha1 during auth-info storage. */
				if (!aeAlgorithm.empty() && authInfo->getAlgorithm().empty()) authInfo->setAlgorithm(aeAlgorithm);
				status = AuthStatus::Done;
			}
			maxTries = 2; /* server nonce may change, we can two consecutive 401.*/
			requestedMethod = LinphoneAuthHttpDigest;
		} break;
		case BELLE_SIP_AUTH_MODE_HTTP_BASIC: {
			const auto authInfo = findBestAuthInfoForChallenge(LinphoneAuthBasic, aeUsername, aeRealm, aeDomain, "");
			if (authInfo) {
				authInfo->fillBelleSipEvent(event);
				status = AuthStatus::Done;
			}
			maxTries = 1;
			requestedMethod = LinphoneAuthBasic;
		} break;
		case BELLE_SIP_AUTH_MODE_HTTP_BEARER: {
			maxTries = 1;
			const auto authInfo = findBestAuthInfoForChallenge(LinphoneAuthBearer, aeUsername, aeRealm, aeDomain, "");
			if (authInfo) {
				const auto accessToken = authInfo->getAccessToken();
				const auto refreshToken = authInfo->getRefreshToken();
				if (refreshToken && accessToken &&
				    (accessToken->isExpired() || (accessToken->getExpirationTime() == 0 && tryCount >= 1))) {
					/* We know that the access token is expired, or we have a failure with the access token.
					 * As we have a refresh token, make a refresh attempt */
					lInfo() << "Core::fillBelleSipAuthEvent: Token is or might be expired, using refresh token to get "
					           "new one.";
					if (refreshTokens(authInfo)) {
						status = AuthStatus::Pending;
					} else status = AuthStatus::NoAuth;
				} else {
					authInfo->fillBelleSipEvent(event);
					status = AuthStatus::Done;
				}
				maxTries = refreshToken ? 2 : 1;
			}
			requestedMethod = LinphoneAuthBearer;
		} break;
		case BELLE_SIP_AUTH_MODE_TLS: {
			/* extract username and domain from the GRUU stored in userData->username */
			string tlsCert;
			string tlsKey;
			string tlsCertPath;
			string tlsKeyPath;
			string effectiveDomain = domain.empty() ? aeDomain : domain;
			const auto authInfo = findBestAuthInfoForChallenge(LinphoneAuthTls, username, aeRealm, effectiveDomain, "");

			if (authInfo) { /* tls_auth_info found something */
				tlsCert = authInfo->getTlsCert();
				tlsKey = authInfo->getTlsKey();
				if (tlsCert.empty() || tlsKey.empty()) {
					tlsCertPath = authInfo->getTlsCertPath();
					tlsKeyPath = authInfo->getTlsKeyPath();
				}
			} else { /* get directly from linphonecore
				    or given in the sip/client_tlsCert in the config file, no username/domain associated with it,
				   last resort try it shall work if we have only one user */
				tlsCert = L_C_TO_STRING(linphone_core_get_tls_cert(getCCore()));
				tlsKey = L_C_TO_STRING(linphone_core_get_tls_key(getCCore()));
				if (tlsCert.empty() || tlsKey.empty()) {
					tlsCertPath = L_C_TO_STRING(linphone_core_get_tls_cert_path(getCCore()));
					tlsKeyPath = L_C_TO_STRING(linphone_core_get_tls_key_path(getCCore()));
				}
			}

			if (!tlsCert.empty() && !tlsKey.empty()) {
				belle_sip_certificates_chain_t *bs_tlsCert = belle_sip_certificates_chain_parse(
				    tlsCert.c_str(), tlsCert.size(), BELLE_SIP_CERTIFICATE_RAW_FORMAT_PEM);
				belle_sip_signing_key_t *bs_key = belle_sip_signing_key_parse(tlsKey.c_str(), tlsKey.size(), nullptr);
				if (bs_tlsCert && bs_key) {
					belle_sip_auth_event_set_signing_key(event, bs_key);
					belle_sip_auth_event_set_client_certificates_chain(event, bs_tlsCert);
				}
			} else if (!tlsCertPath.empty() && !tlsKeyPath.empty()) {
				belle_sip_certificates_chain_t *bs_tlsCert =
				    belle_sip_certificates_chain_parse_file(tlsCertPath.c_str(), BELLE_SIP_CERTIFICATE_RAW_FORMAT_PEM);
				belle_sip_signing_key_t *bs_key = belle_sip_signing_key_parse_file(tlsKeyPath.c_str(), nullptr);
				if (bs_tlsCert && bs_key) {
					belle_sip_auth_event_set_signing_key(event, bs_key);
					belle_sip_auth_event_set_client_certificates_chain(event, bs_tlsCert);
				}
			} else {
				lInfo() << "Core::fillBelleSipAuthEvent: No TLS client certificate to propose.";
				// To enable callback:
				//  - create an AuthInfo object with username and domain
				//  - call linphone_core_notify_authentication_requested on it to give the app a chance to fill the
				//  auth_info.
				//  - call again _linphone_core_find_indexed_tls_auth_info to retrieve the auth_info set by the
				//  callback. Not done as we assume that authentication on Flexisip server was performed before so the
				//  application layer already got a chance to set the correct auth_info in the core
				//  THIS IS NOT TRUE ANYMORE: Flexisip auth is performed after the access to the lime server as the
				//  register is performed after the lime user creation.
			}
			status =
			    AuthStatus::Done; // since we can't know if server requested a client certificate, assume all is good.
			requestedMethod = LinphoneAuthTls;
			maxTries = 1;
		} break;
		default:
			lError() << "Core::fillBelleSipAuthEvent: Connection gets an auth event of unexpected type";
			status = AuthStatus::NoAuth;
			break;
	}
	if (status == AuthStatus::NoAuth || tryCount >= maxTries) {
		/* We do not prompt the application for authentication info for http services, because it may happen
		 * for various kind of events but user is not expected to supply them on the fly.
		 * However, for provisioning process, for which bearer or digest is likely to be requested,
		 * we notify the application.
		 * The provisioning is then suspended until authentication information is supplied, or
		 * authentication is aborted using linphone_core_abort_authentication().
		 */
		if (linphone_core_get_global_state(getCCore()) == LinphoneGlobalConfiguring) {
			auto authInfo = createAuthInfo(aeUsername, "", "", "", aeRealm, aeDomain);
			authInfo->setAlgorithm(L_C_TO_STRING(belle_sip_auth_event_get_algorithm(event)));
			authInfo->setAuthorizationServer(L_C_TO_STRING(belle_sip_auth_event_get_authz_server(event)));
			authInfo->setRequestedMethod(requestedMethod);
			linphone_core_notify_authentication_requested(getCCore(), authInfo->toC(), requestedMethod);
			status = AuthStatus::Pending;
		}
	}
	return status;
}
/*the auth info is expected to be in the core's list*/
void Core::writeAuthInfo(const std::shared_ptr<AuthInfo> &authInfo) const {
	auto cCore = getCCore();
	if (!cCore->sip_conf.save_auth_info || linphone_config_is_readonly(cCore->config)) return;
	auto &authInfos = mAuthInfos.mList;
	int i = 0;
	for (auto &coreAuthInfo : authInfos) {
		if (coreAuthInfo == authInfo) {
			authInfo->writeConfig(cCore->config, i);
			break;
		}
		++i;
	}
};
void Core::writeAuthInfos() const {
	auto cCore = getCCore();
	auto state = linphone_core_get_global_state(cCore);

	if (state == LinphoneGlobalShutdown || state == LinphoneGlobalOn) {
		if (!cCore->sip_conf.save_auth_info || linphone_config_is_readonly(cCore->config)) return;
		int i = 0;
		auto &authInfos = mAuthInfos.mList;
		for (auto &authInfo : authInfos) {
			authInfo->writeConfig(cCore->config, i);
			++i;
		}
		linphone_auth_info_write_config(cCore->config, nullptr, i); /* mark the end */
	}
};

LINPHONE_END_NAMESPACE
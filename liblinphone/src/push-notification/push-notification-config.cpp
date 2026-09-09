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

#include "push-notification-config.h"
#include "address/address.h"
#include "linphone/lpconfig.h"
#include "linphone/utils/utils.h"
#include "logger/logger.h"

#include <algorithm>

using namespace std;

LINPHONE_BEGIN_NAMESPACE

const std::string PushNotificationConfig::kDefaultTeamId = "ABCD1234";

PushNotificationConfig::PushNotificationConfig() {
#ifdef __ANDROID__
	mPushParams[PushConfigProviderKey] = "fcm";
#elif TARGET_OS_IPHONE
	mPushParams[PushConfigProviderKey] = "apns";
#else
	mPushParams[PushConfigProviderKey] = "";
#endif
	mPushParams[PushConfigParamKey] = "";
	mPushParams[PushConfigPridKey] = "";
	mPushParams[PushConfigTimeoutKey] = "0";
	mPushParams[PushConfigSilentKey] = "1";
	mPushParams[PushConfigMsgStrKey] = "IM_MSG";
	mPushParams[PushConfigCallStrKey] = "IC_MSG";
	mPushParams[PushConfigGroupChatStrKey] = "GC_MSG";
	mPushParams[PushConfigCallSoundKey] = "notes_of_the_optimistic.caf";
	mPushParams[PushConfigMsgSoundKey] = "msg.caf";
	mPushParams[PushConfigRemotePushIntervalKey] = "";

	mTeamId = PushNotificationConfig::kDefaultTeamId;
	mBundleIdentifer = "";
	mVoipToken = "";
	mRemoteToken = "";
}

PushNotificationConfig::PushNotificationConfig(const PushNotificationConfig &other) : HybridObject(other) {
	mPushParams = other.mPushParams;
	mTeamId = other.mTeamId;
	mBundleIdentifer = other.mBundleIdentifer;
	mVoipToken = other.mVoipToken;
	mRemoteToken = other.mRemoteToken;
	mTokensHaveChanged = other.mTokensHaveChanged;
}

PushNotificationConfig *PushNotificationConfig::clone() const {
	return new PushNotificationConfig(*this);
}

PushNotificationConfig &PushNotificationConfig::operator=(const PushNotificationConfig &other) {
	if (this != &other) {
		mPushParams = other.mPushParams;
		mTeamId = other.mTeamId;
		mBundleIdentifer = other.mBundleIdentifer;
		mVoipToken = other.mVoipToken;
		mRemoteToken = other.mRemoteToken;
		mTokensHaveChanged = other.mTokensHaveChanged;
	}
	return *this;
}

bool PushNotificationConfig::isEqual(const PushNotificationConfig &other) const {
	return mPushParams == other.mPushParams && mTeamId == other.mTeamId && mBundleIdentifer == other.mBundleIdentifer &&
	       mVoipToken == other.mVoipToken && mRemoteToken == other.mRemoteToken;
}

const string &PushNotificationConfig::getProvider() const {
	return mPushParams.at(PushConfigProviderKey);
}
void PushNotificationConfig::setProvider(const string &provider) {
	mPushParams[PushConfigProviderKey] = provider;
}

const string &PushNotificationConfig::getMsgStr() const {
	return mPushParams.at(PushConfigMsgStrKey);
}
void PushNotificationConfig::setMsgStr(const string &msgStr) {
	mPushParams[PushConfigMsgStrKey] = msgStr;
}

const string &PushNotificationConfig::getCallStr() const {
	return mPushParams.at(PushConfigCallStrKey);
}
void PushNotificationConfig::setCallStr(const string &callStr) {
	mPushParams[PushConfigCallStrKey] = callStr;
}

const string &PushNotificationConfig::getGroupChatStr() const {
	return mPushParams.at(PushConfigGroupChatStrKey);
}
void PushNotificationConfig::setGroupChatStr(const string &groupChatStr) {
	mPushParams[PushConfigGroupChatStrKey] = groupChatStr;
}

const string &PushNotificationConfig::getPrid() const {
	return mPushParams.at(PushConfigPridKey);
}
void PushNotificationConfig::setPrid(const string &prid) {
	mPushParams[PushConfigPridKey] = prid;
}

const string &PushNotificationConfig::getCallSnd() const {
	return mPushParams.at(PushConfigCallSoundKey);
}
void PushNotificationConfig::setCallSnd(const string &callSnd) {
	mPushParams[PushConfigCallSoundKey] = callSnd;
}

const string &PushNotificationConfig::getMsgSnd() const {
	return mPushParams.at(PushConfigMsgSoundKey);
}
void PushNotificationConfig::setMsgSnd(const string &msgSnd) {
	mPushParams[PushConfigMsgSoundKey] = msgSnd;
}

const string &PushNotificationConfig::getParam() const {
	return mPushParams.at(PushConfigParamKey);
}
void PushNotificationConfig::setParam(const string &param) {
	mPushParams[PushConfigParamKey] = param;
}

const string &PushNotificationConfig::getBundleIdentifer() const {
	return mBundleIdentifer;
}
void PushNotificationConfig::setBundleIdentifer(const string &bundleIdentifer) {
	mBundleIdentifer = bundleIdentifer;
}

const string &PushNotificationConfig::getVoipToken() const {
	return mVoipToken;
}
void PushNotificationConfig::setVoipToken(const string &voipToken) {
	if (mVoipToken != voipToken) {
		mTokensHaveChanged = true;
		mVoipToken = voipToken;
	}
}

const string &PushNotificationConfig::getRemoteToken() const {
	return mRemoteToken;
}
void PushNotificationConfig::setRemoteToken(const string &remoteToken) {
	if (mRemoteToken != remoteToken) {
		mTokensHaveChanged = true;
		mRemoteToken = remoteToken;
	}
}

const string &PushNotificationConfig::getTeamId() const {
	return mTeamId;
}
void PushNotificationConfig::setTeamId(const string &teamId) {
	mTeamId = teamId;
}

const string &PushNotificationConfig::getRemotePushInterval() const {
	return mPushParams.at(PushConfigRemotePushIntervalKey);
}
void PushNotificationConfig::setRemotePushInterval(const string &remotePushInterval) {
	mPushParams[PushConfigRemotePushIntervalKey] = remotePushInterval;
}

// An APNs token is suffixed with the service it is for, like "hextoken:voip".
static bool isPushTokenForService(string const &token, string const &service) {
	string suffix = ":" + service;
	return token.size() > suffix.size() && Utils::endsWith(token, suffix);
}

// iOS: pn-prid is made of '&'-separated APNs tokens suffixed with their service, like "hextoken:voip&hextoken:remote".
// Android: An opaque FCM token has no specific suffix.
static bool pridHasTokenForService(string const &prid, string const &service) {
	for (const auto &token : bctoolbox::Utils::split(prid, "&"))
		if (isPushTokenForService(token, service)) return true;
	return false;
}

// iOS: pn-param="teamId.bundleId.${SERVICES}" (RFC 8599), with SERVICES being like "voip", "remote", or "voip&remote".
// Android: An FCM project has no services and no "."
static bool pnParamHasService(string const &pnParam, string const &service) {
	size_t lastDotPos = pnParam.find_last_of('.');
	if (lastDotPos == string::npos) return false;
	auto services = bctoolbox::Utils::split(pnParam.substr(lastDotPos + 1), "&");
	return find(services.begin(), services.end(), service) != services.end();
}

static bool doesParamNeedUpdate(bool hasVoip, bool hasRemote, bool voipPushAllowed, bool remotePushAllowed) {
	return hasVoip != voipPushAllowed || hasRemote != remotePushAllowed;
}

void PushNotificationConfig::generatePushParams(bool voipPushAllowed, bool remotePushAllowed) {
	if (mPushParams[PushConfigProviderKey].empty()) {
#ifdef __ANDROID__
		mPushParams[PushConfigProviderKey] = "fcm";
#elif TARGET_OS_IPHONE
		mPushParams[PushConfigProviderKey] = "apns";
#endif
	}
	/* Push notification may be allowed and requested to the system, but sometimes the tokens are not
	 * yet available. Modify the enablement so that we don't generate an ill-formed push parameter line.
	 */
	if (mRemoteToken.empty() && remotePushAllowed) {
		lWarning() << "[PushNotificationConfig::generatePushParams]: remote push is enabled but no remote token is "
		              "set, so we have disable it for push param generation";
		remotePushAllowed = false;
	}
	if (mVoipToken.empty() && voipPushAllowed) {
		lWarning() << "[PushNotificationConfig::generatePushParams]: voip push is enabled but no voip token is set, so "
		              "we have disable it for push param generation";
		voipPushAllowed = false;
	}

	const string &pnParam = mPushParams[PushConfigParamKey];
	if (pnParam.empty() ||
	    doesParamNeedUpdate(pnParamHasService(pnParam, "voip"), pnParamHasService(pnParam, "remote"), voipPushAllowed,
	                        remotePushAllowed) ||
	    (mTokensHaveChanged && (!mVoipToken.empty() || !mRemoteToken.empty()))) {
		string services;
		if (voipPushAllowed) {
			services += "voip";
			if (remotePushAllowed) services += "&";
		}
		if (remotePushAllowed) services += "remote";

		mPushParams[PushConfigParamKey] = mTeamId + "." + mBundleIdentifer + "." + services;
	}

	const string &prid = mPushParams[PushConfigPridKey];
	if (prid.empty() ||
	    doesParamNeedUpdate(pridHasTokenForService(prid, "voip"), pridHasTokenForService(prid, "remote"),
	                        voipPushAllowed, remotePushAllowed) ||
	    (mTokensHaveChanged && (!mVoipToken.empty() || !mRemoteToken.empty()))) {
		string newPrid;
		if (voipPushAllowed) {
			newPrid += mVoipToken;
			if (remotePushAllowed) newPrid += "&";
		}
		if (remotePushAllowed) newPrid += mRemoteToken;
		mPushParams[PushConfigPridKey] = newPrid;
	}
	mTokensHaveChanged = false;
}

map<string, string> const &PushNotificationConfig::getPushParamsMap() {
	return mPushParams;
}

string PushNotificationConfig::asString(bool withRemoteSpecificParams) const {
	string serializedConfig;

	auto appendParam = [&](string const &paramName) {
		if (!mPushParams.at(paramName).empty()) serializedConfig += paramName + "=" + mPushParams.at(paramName) + ";";
	};

	appendParam(PushConfigPridKey);
	appendParam(PushConfigProviderKey);
	appendParam(PushConfigParamKey);
	appendParam(PushConfigSilentKey);
	appendParam(PushConfigTimeoutKey);

	if (withRemoteSpecificParams) {
		appendParam(PushConfigMsgStrKey);
		appendParam(PushConfigCallStrKey);
		appendParam(PushConfigGroupChatStrKey);
		appendParam(PushConfigCallSoundKey);
		appendParam(PushConfigMsgSoundKey);
		appendParam(PushConfigRemotePushIntervalKey);
	}

	return serializedConfig;
}

bool PushNotificationConfig::isApnsProvider() const {
	// "apns" or "apns.dev"
	return getProvider().rfind("apns", 0) == 0;
}

void PushNotificationConfig::readTokensFromPrid(const string &prid) {
	// iOS: pn-prid is made of the '&'-separated tokens "hextoken:voip" and "hextoken:remote", in any order.
	// Android: no token to extract.
	string voipToken;
	string remoteToken;
	for (const auto &element : bctoolbox::Utils::split(prid, "&")) {
		if (isPushTokenForService(element, "voip")) voipToken = element;
		else if (isPushTokenForService(element, "remote")) remoteToken = element;
	}
	if (!voipToken.empty()) mVoipToken = voipToken;
	if (!remoteToken.empty()) mRemoteToken = remoteToken;

	if (voipToken.empty() && remoteToken.empty() && isApnsProvider()) {
		lError() << "[PushNotificationConfig::readPushParamsFromString]: error when parsing the push parameter string: "
		            "pn-prid '"
		         << prid << "' could not find push tokens";
	}
}

void PushNotificationConfig::readTeamIdAndBundleIdentifierFromParam(const string &param) {
	// According to RFC8599: https://datatracker.ietf.org/doc/html/rfc8599#page-30
	// iOS: pn-param must be of the form "TeamId.BundleID.services"
	// Example: pn-param=DEF123GHIJ.com.example.yourexampleapp.voip
	// Android: pn-param is a project ID with no specific suffix.
	size_t firstDotPos = param.find_first_of(".");
	size_t lastDotPos = param.find_last_of(".");
	if (firstDotPos == string::npos || firstDotPos == lastDotPos) {
		if (isApnsProvider()) {
			lError() << "[PushNotificationConfig::readPushParamsFromString]: error when parsing the push parameter "
			            "string: pn-param '"
			         << param << "' should be of the form teamID.bundleIdentifier.services";
		}
		return;
	}

	string teamId = param.substr(0, firstDotPos);
	string bundleIdentifier = param.substr(firstDotPos + 1, lastDotPos - firstDotPos - 1);
	if (teamId.empty() || bundleIdentifier.empty()) {
		lError() << "[PushNotificationConfig::readPushParamsFromString]: error when parsing the push parameter string: "
		            "empty team ID or bundle identifier in pn-param '"
		         << param << "'";
		return;
	}
	mTeamId = teamId;
	mBundleIdentifer = bundleIdentifier;
}

void PushNotificationConfig::readPushParamsFromString(string const &serializedConfig) {
	if (serializedConfig.empty()) return;

	std::shared_ptr<Address> pushParamsWrapper = Address::create("sip:dummy;" + serializedConfig);
	if (!pushParamsWrapper || !pushParamsWrapper->isValid()) {
		lError() << "[PushNotificationConfig::readPushParamsFromString]: could not parse the push parameter string '"
		         << serializedConfig << "'";
		return;
	}
	for (auto &param : mPushParams) {
		string paramValue = pushParamsWrapper->getUriParamValue(param.first);
		if (!paramValue.empty()) param.second = paramValue;
	}

	string prid = pushParamsWrapper->getUriParamValue(PushConfigPridKey);
	if (!prid.empty()) readTokensFromPrid(prid);
	string param = pushParamsWrapper->getUriParamValue(PushConfigParamKey);
	if (!param.empty()) readTeamIdAndBundleIdentifierFromParam(param);
}

void PushNotificationConfig::readFromConfig(LinphoneConfig *config, const std::string &section) {
	const char *c_section = section.c_str();
	if (linphone_config_has_section(config, c_section)) {
		readPushParamsFromString(linphone_config_get_string(config, c_section, "push_parameters", ""));
	}
}

LINPHONE_END_NAMESPACE

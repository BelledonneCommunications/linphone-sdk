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

#include "call-log.h"

#include <ctime>
#include <sstream>

#include "address/address.h"
#include "c-wrapper/internal/c-tools.h"
#include "call/call.h"
#include "chat/chat-room/abstract-chat-room.h"
#include "core/core-p.h"
#include "linphone/types.h"
#include "linphone/utils/utils.h"
#include "private.h"

// =============================================================================

using namespace std;

LINPHONE_BEGIN_NAMESPACE

CallLog::CallLog(shared_ptr<Core> core,
                 LinphoneCallDir direction,
                 const std::shared_ptr<const Address> &from,
                 const std::shared_ptr<const Address> &to)
    : CoreAccessor(core) {
	mDirection = direction;
	if (from) setFromAddress(from);
	if (to) setToAddress(to);

	mStartTime = std::time(nullptr);
	mStartDate = Utils::getTimeAsString("%c", mStartTime);

	mReporting.reports[LINPHONE_CALL_STATS_AUDIO] = linphone_reporting_new();
	mReporting.reports[LINPHONE_CALL_STATS_VIDEO] = linphone_reporting_new();
	mReporting.reports[LINPHONE_CALL_STATS_TEXT] = linphone_reporting_new();
}

CallLog::CallLog(const CallLog &other) : HybridObject(other), CoreAccessor(other.getCore()) {
	mCallId = other.mCallId;
	mStartTime = other.mStartTime;
	mFrom = other.mFrom->clone()->toSharedPtr();
	mTo = other.mTo->clone()->toSharedPtr();
	mDirection = other.mDirection;
	mStatus = other.mStatus;
	mDuration = other.mDuration;
}

CallLog::~CallLog() {
	if (mReporting.reports[LINPHONE_CALL_STATS_AUDIO] != nullptr)
		linphone_reporting_destroy(mReporting.reports[LINPHONE_CALL_STATS_AUDIO]);
	if (mReporting.reports[LINPHONE_CALL_STATS_VIDEO] != nullptr)
		linphone_reporting_destroy(mReporting.reports[LINPHONE_CALL_STATS_VIDEO]);
	if (mReporting.reports[LINPHONE_CALL_STATS_TEXT] != nullptr)
		linphone_reporting_destroy(mReporting.reports[LINPHONE_CALL_STATS_TEXT]);
	if (mErrorInfo != nullptr) linphone_error_info_unref(mErrorInfo);
}

CallLog *CallLog::clone() const {
	return new CallLog(*this);
}
// =============================================================================

LinphoneCallDir CallLog::getDirection() const {
	return mDirection;
}

void CallLog::setDirection(LinphoneCallDir direction) {
	mDirection = direction;
}

int CallLog::getDuration() const {
	return mDuration;
}

void CallLog::setDuration(int duration) {
	mDuration = duration;
}

float CallLog::getQuality() const {
	return mQuality;
}

void CallLog::setQuality(float quality) {
	mQuality = quality;
}

const std::shared_ptr<Address> &CallLog::getFromAddress() const {
	return mFrom;
}

void CallLog::setFromAddress(const std::shared_ptr<const Address> &address) {
	mFrom = address->clone()->toSharedPtr();
}

const std::shared_ptr<Address> &CallLog::getToAddress() const {
	return mTo;
}

void CallLog::setToAddress(const std::shared_ptr<const Address> &address) {
	mTo = address->clone()->toSharedPtr();
}

const string &CallLog::getCallId() const {
	return mCallId;
}

void CallLog::setCallId(const string &callId) {
	mCallId = callId;
}

const string &CallLog::getRefKey() const {
	return mRefKey;
}

void CallLog::setRefKey(const string &refKey) {
	mRefKey = refKey;
}

time_t CallLog::getStartTime() const {
	return mStartTime;
}

void CallLog::setStartTime(time_t startTime) {
	mStartTime = startTime;
	mStartDate = Utils::getTimeAsString("%c", mStartTime);
}

time_t CallLog::getConnectedTime() const {
	return mConnectedTime;
}

void CallLog::setConnectedTime(time_t connectedTime) {
	mConnectedTime = connectedTime;
}

LinphoneCallStatus CallLog::getStatus() const {
	return mStatus;
}

void CallLog::setStatus(LinphoneCallStatus status) {
	mStatus = status;
}

bool CallLog::isVideoEnabled() const {
	return mVideoEnabled;
}

void CallLog::setVideoEnabled(bool enabled) {
	mVideoEnabled = enabled;
}

bool CallLog::wasConference() {
	return getConferenceInfo() != nullptr;
}

const LinphoneErrorInfo *CallLog::getErrorInfo() const {
	return mErrorInfo;
}

void CallLog::setErrorInfo(LinphoneErrorInfo *errorInfo) {
	if (mErrorInfo) linphone_error_info_unref(mErrorInfo);
	mErrorInfo = linphone_error_info_clone(errorInfo);
}

const std::shared_ptr<Address> &CallLog::getRemoteAddress() const {
	return (mDirection == LinphoneCallIncoming) ? mFrom : mTo;
}

void CallLog::setRemoteAddress(const std::shared_ptr<Address> &remoteAddress) {
	if (mDirection == LinphoneCallIncoming) {
		setFromAddress(remoteAddress);
	} else {
		setToAddress(remoteAddress);
	}
}

void *CallLog::getUserData() const {
	return mUserData;
}

void CallLog::setUserData(void *userData) {
	mUserData = userData;
}

const std::shared_ptr<Address> &CallLog::getLocalAddress() const {
	return (mDirection == LinphoneCallIncoming) ? mTo : mFrom;
}

const std::string &CallLog::getStartTimeString() const {
	return mStartDate;
}

LinphoneQualityReporting *CallLog::getQualityReporting() {
	return &mReporting;
}

void CallLog::setConferenceInfoId(long long conferenceInfoId) {
	mConferenceInfoId = conferenceInfoId;
}

// =============================================================================

void CallLog::setConferenceInfo(std::shared_ptr<ConferenceInfo> conferenceInfo) {
	mConferenceInfo = conferenceInfo;
}

std::shared_ptr<ConferenceInfo> CallLog::getConferenceInfo() const {
	// The conference info stored in the database is always up to date, therefore try to update the cache all the time
	// if there id is valid Nonetheless, the cache variable is required if the core disables the storage of information
	// in the database.
	if (auto db = getCore()->getDatabase()) {
		if (mConferenceInfoId != -1) {
			mConferenceInfo = db.value().get().getConferenceInfo(mConferenceInfoId);
		} else if (mTo && !mConferenceInfo) {
			// Try to find the conference info based on the to address
			// We enter this branch of the if-else statement only if the call cannot be started right away, for example
			// when ICE candidates need to be gathered first
			mConferenceInfo = db.value().get().getConferenceInfoFromURI(getRemoteAddress());
		}
	}

	return mConferenceInfo;
}

const std::shared_ptr<AbstractChatRoom> CallLog::getChatRoom() const {
	auto conferenceInfo = getConferenceInfo();
	if (conferenceInfo && conferenceInfo->getCapability(LinphoneStreamTypeText)) {
		return getCore()->getPrivate()->searchChatRoom(nullptr, nullptr, conferenceInfo->getUri(), {});
	}
	return nullptr;
}

// =============================================================================

string CallLog::toString() const {
	ostringstream os;
	std::string direction = Call::callDirToText(mDirection);
	direction[0] = (char)toupper(direction[0]);
	os << direction << " call" << " with call-id: " << mCallId << " at " << mStartDate << "\n";

	os << "From: " << *mFrom << "\nTo: " << *mTo << "\n";

	os << "Status: " << Call::callStatusToText(mStatus) << "\nDuration: " << (mDuration / 60) << " mn "
	   << (mDuration % 60) << " sec\n";

	return os.str();
}

// See getJsonHelp() for definitions
string CallLog::toJson() const {
	Json::StreamWriterBuilder builder;
	Json::Value json;
	json["call_id"] = mCallId;
	json["start_dt"] = Utils::timeToIso8601(mStartTime);
	json["from"] = mFrom->asStringUriOnly();
	json["to"] = mTo->asStringUriOnly();
	json["direction"] = Call::callDirToText(mDirection);
	json["status"] = Call::callStatusToText(mStatus);
	json["duration_s"] = mDuration;
	json["version"] = 1;
	return string(Json::writeString(builder, json));
}

// Only overwrite if JSON item exists.
int CallLog::fromJson(const string &json) {
	Json::CharReaderBuilder builder;
	Json::Value root;
	JSONCPP_STRING err;

	const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

	if (!reader->parse(json.c_str(), json.c_str() + json.length(), &root, &err)) {
		lError() << err;
		return -1;
	} else {
		// It's JSON that can be written by external. We need to check the validity.
		int version = 1;
		if (root.isMember("version")) version = root["version"].asInt();
		else {
			lWarning() << "Parsing JSON for CallLog: `version` key is missing. Parsing like a version 1.";
		}
		if (version >= 1) {
			if (root.isMember("direction")) setDirection(Call::textToCallDir(root["direction"].asString()));
			if (root.isMember("call_id")) setCallId(root["call_id"].asString());
			if (root.isMember("start_dt")) setStartTime(Utils::iso8601ToTime(root["start_dt"].asString()));
			if (root.isMember("from")) setFromAddress(Address::create(root["from"].asString()));
			if (root.isMember("to")) setToAddress(Address::create(root["to"].asString()));
			if (root.isMember("status")) setStatus(Call::textToCallStatus(root["status"].asString()));
			if (root.isMember("duration_s")) setDuration(root["duration_s"].asInt());
		}
	}
	return 0;
}

std::map<std::string, std::string> CallLog::getJsonHelp() {
	std::map<std::string, std::string> help;
	help["version"] = "The version number of the JSON. (Currently V1)";
	help["direction"] = "The text value of LinphoneCallDir";
	help["call_id"] = "The call ID";
	help["start_dt"] = "The start date time in ISO8601";
	help["from"] = "The FROM address of the call log";
	help["to"] = "The TO address of the call log";
	help["status"] = "The text value of LinphoneCallStatus";
	help["duration_s"] = "The duration in seconds";
	return help;
}

LINPHONE_END_NAMESPACE

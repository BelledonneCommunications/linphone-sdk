/*
 * Copyright (c) 2010-2026 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
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

#pragma once

#include <array>
#include <memory>
#include <optional>
#include <vector>

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/stun-address.h"
#include "mediastreamer2/stun-error.h"
#include "mediastreamer2/stun-transaction-id.h"

namespace mediastreamer::nat {

class StunRawMessage;

class MS2_PUBLIC StunMessage {
public:
	friend class StunRawMessage;

	enum class Type {
		Request = 0x0000,
		Indication = 0x0010,
		SuccessResponse = 0x0100,
		ErrorResponse = 0x0110,
	};

	enum class Method {
		Binding = 0x01,
		SharedSecret = 0x02, // Deprecated, now reserved
		TurnAllocate = 0x03,
		TurnRefresh = 0x04,
		TurnSend = 0x06,
		TurnData = 0x07,
		TurnCreatePermission = 0x08,
		TurnChannelBind = 0x09,
	};

	static constexpr size_t kMessageIntegrityLength = 20;
	static constexpr std::array<uint8_t, kMessageIntegrityLength> kDummyMessageIntegrity = {
	    'h', 'm', 'a', 'c', '-', 'n', 'o', 't', '-', 'i', 'm', 'p', 'l', 'e', 'm', 'e', 'n', 't', 'e', 'd'};
	static constexpr uint16_t kFirstChannelNumber = 0x4000;

	virtual ~StunMessage() = default;

	void enableChangeIp(const bool enable) {
		mHasChangeIp = enable;
	}
	void enableChangePort(const bool enable) {
		mHasChangePort = enable;
	}
	void enableDummyMessageIntegrity(const bool enable) {
		mHasDummyMessageIntegrity = enable;
	}
	void enableFingerprint(const bool enable) {
		mHasFingerprint = enable;
	}
	void enableMessageIntegrity(const bool enable) {
		mHasMessageIntegrity = enable;
	}
	[[nodiscard]] std::shared_ptr<StunRawMessage> encode();
	[[nodiscard]] std::optional<uint16_t> getChannelNumber() const {
		return mChannelNumber;
	}
	[[nodiscard]] const std::optional<std::vector<uint8_t>> &getData() const {
		return mData;
	}
	[[nodiscard]] const std::optional<StunError> &getError() const {
		return mError;
	}
	[[nodiscard]] const std::optional<std::string> &getHa1() const {
		return mHa1;
	}
	[[nodiscard]] std::optional<uint64_t> getIceControlled() const {
		return mIceControlled;
	}
	[[nodiscard]] std::optional<uint64_t> getIceControlling() const {
		return mIceControlling;
	}
	[[nodiscard]] std::optional<uint32_t> getLifetime() const {
		return mLifetime;
	}
	[[nodiscard]] const std::optional<StunAddress> &getMappedAddress() const {
		return mMappedAddress;
	}
	[[nodiscard]] const std::optional<std::array<uint8_t, kMessageIntegrityLength>> &getMessageIntegrity() const {
		return mMessageIntegrity;
	}
	[[nodiscard]] Method getMethod() const {
		return mMethod;
	}
	[[nodiscard]] const std::optional<std::string> &getNonce() const {
		return mNonce;
	}
	[[nodiscard]] const std::optional<std::string> &getPassword() const {
		return mPassword;
	}
	[[nodiscard]] std::optional<uint32_t> getPriority() const {
		return mPriority;
	}
	[[nodiscard]] const std::shared_ptr<StunRawMessage> &getRawMessage() const {
		return mRawMessage;
	}
	[[nodiscard]] const std::optional<std::string> &getRealm() const {
		return mRealm;
	}
	[[nodiscard]] std::optional<uint8_t> getRequestedAddressFamily() const {
		return mRequestedAddressFamily;
	}
	[[nodiscard]] std::optional<uint8_t> getRequestedTransport() const {
		return mRequestedTransport;
	}
	[[nodiscard]] const std::optional<std::string> &getSoftware() const {
		return mSoftware;
	}
	[[nodiscard]] const StunTransactionId &getTransactionId() const {
		return mTransactionId;
	}
	[[nodiscard]] Type getType() const {
		return mType;
	}
	[[nodiscard]] bool getUseCandidate() const {
		return mHasUseCandidate;
	}
	[[nodiscard]] const std::optional<std::string> &getUsername() const {
		return mUsername;
	}
	[[nodiscard]] const std::optional<StunAddress> &getXorMappedAddress() const {
		return mXorMappedAddress;
	}
	[[nodiscard]] const std::optional<StunAddress> &getXorPeerAddress() const {
		return mXorPeerAddress;
	}
	[[nodiscard]] const std::optional<StunAddress> &getXorRelayedAddress() const {
		return mXorRelayedAddress;
	}
	[[nodiscard]] bool hasChangeIp() const {
		return mHasChangeIp;
	}
	[[nodiscard]] bool hasChangePort() const {
		return mHasChangePort;
	}
	[[nodiscard]] bool hasDummyMessageIntegrity() const {
		return mHasDummyMessageIntegrity;
	}
	[[nodiscard]] bool hasFingerprint() const {
		return mHasFingerprint;
	}
	[[nodiscard]] bool hasMessageIntegrity() const {
		return mHasMessageIntegrity;
	}
	void includeUsernameAttribute(const bool include) {
		mIncludeUsernameAttribute = include;
	}
	[[nodiscard]] bool isErrorResponse() const {
		return mType == Type::ErrorResponse;
	}
	[[nodiscard]] bool isIndication() const {
		return mType == Type::Indication;
	}
	[[nodiscard]] bool isRequest() const {
		return mType == Type::Request;
	}
	[[nodiscard]] bool isSuccessResponse() const {
		return mType == Type::SuccessResponse;
	}
	void setChannelNumber(const uint16_t channelNumber) {
		mChannelNumber = channelNumber;
	}
	void setData(const char *data, size_t length);
	void setError(const StunError &error) {
		mError = error;
	}
	void setHa1(const std::string &ha1) {
		mHa1 = ha1;
	}
	void setIceControlled(const uint64_t iceControlled) {
		mIceControlled = iceControlled;
	}
	void setIceControlling(const uint64_t iceControlling) {
		mIceControlling = iceControlling;
	}
	void setLifetime(const uint32_t lifetime) {
		mLifetime = lifetime;
	}
	void setMappedAddress(const StunAddress &mappedAddress) {
		mMappedAddress = mappedAddress;
	}
	void setNonce(const std::string &nonce);
	void setPassword(const std::string &password) {
		mPassword = password;
	}
	void setPriority(const uint32_t priority) {
		mPriority = priority;
	}
	void setRealm(const std::string &realm);
	void setRequestedAddressFamily(const uint8_t requestedAddressFamily) {
		mRequestedAddressFamily = requestedAddressFamily;
	}
	void setRequestedTransport(const uint8_t requestedTransport) {
		mRequestedTransport = requestedTransport;
	}
	void setSoftware(const std::string &software) {
		mSoftware = software;
	}
	void setTransactionId(const StunTransactionId &transactionId) {
		mTransactionId = transactionId;
	}
	void setUseCandidate(bool useCandidate) {
		mHasUseCandidate = useCandidate;
	}
	void setUsername(const std::string &username);
	void setXorMappedAddress(const StunAddress &xorMappedAddress) {
		mXorMappedAddress = xorMappedAddress;
	}
	void setXorPeerAddress(const StunAddress &xorPeerAddress) {
		mXorPeerAddress = xorPeerAddress;
	}
	void setXorRelayedAddress(const StunAddress &xorRelayedAddress) {
		mXorRelayedAddress = xorRelayedAddress;
	}

	[[nodiscard]] static std::shared_ptr<StunMessage> parse(const char *data, size_t len);
	[[nodiscard]] static std::shared_ptr<StunMessage> createStunBindingRequest() {
		return std::shared_ptr<StunMessage>(new StunMessage(Type::Request, Method::Binding));
	}
	[[nodiscard]] static std::shared_ptr<StunMessage> createStunBindingSuccessResponse() {
		return std::shared_ptr<StunMessage>(new StunMessage(Type::SuccessResponse, Method::Binding));
	}
	[[nodiscard]] static std::shared_ptr<StunMessage> createStunBindingErrorResponse() {
		return std::shared_ptr<StunMessage>(new StunMessage(Type::ErrorResponse, Method::Binding));
	}
	[[nodiscard]] static std::shared_ptr<StunMessage> createStunBindingIndication() {
		return std::shared_ptr<StunMessage>(new StunMessage(Type::Indication, Method::Binding));
	}
	[[nodiscard]] static std::shared_ptr<StunMessage> createTurnAllocateRequest();
	[[nodiscard]] static std::shared_ptr<StunMessage> createTurnChannelBindRequest(const StunAddress &peerAddress,
	                                                                               uint16_t channelNumber);
	[[nodiscard]] static std::shared_ptr<StunMessage> createTurnCreatePermissionRequest(const StunAddress &peerAddress);
	[[nodiscard]] static std::shared_ptr<StunMessage> createTurnRefreshRequest(uint32_t lifetime);
	[[nodiscard]] static std::shared_ptr<StunMessage> createTurnSendIndication(const StunAddress &peerAddress);

private:
	StunMessage() = default;
	StunMessage(const Type type, const Method method) : mType(type), mMethod(method) {
	}

	Type mType = Type::Request;
	Method mMethod = Method::Binding;
	StunTransactionId mTransactionId = StunTransactionId::random();
	std::optional<std::vector<uint8_t>> mData = std::nullopt;
	std::optional<std::string> mUsername = std::nullopt;
	std::optional<std::string> mPassword = std::nullopt;
	std::optional<std::string> mHa1 = std::nullopt;
	std::optional<std::string> mRealm = std::nullopt;
	std::optional<std::string> mNonce = std::nullopt;
	std::optional<std::string> mSoftware = std::nullopt;
	std::optional<std::array<uint8_t, kMessageIntegrityLength>> mMessageIntegrity = std::nullopt;
	std::optional<StunError> mError = std::nullopt;
	std::optional<StunAddress> mMappedAddress = std::nullopt;
	std::optional<StunAddress> mXorMappedAddress = std::nullopt;
	std::optional<StunAddress> mXorPeerAddress = std::nullopt;
	std::optional<StunAddress> mXorRelayedAddress = std::nullopt;
	std::optional<uint32_t> mPriority = std::nullopt;
	std::optional<uint64_t> mIceControlling = std::nullopt;
	std::optional<uint64_t> mIceControlled = std::nullopt;
	std::optional<uint32_t> mLifetime = std::nullopt;
	std::optional<uint16_t> mChannelNumber = std::nullopt;
	std::optional<uint8_t> mRequestedTransport = std::nullopt;
	std::optional<uint8_t> mRequestedAddressFamily = std::nullopt;
	bool mHasChangeIp = false;
	bool mHasChangePort = false;
	bool mHasDummyMessageIntegrity = false;
	bool mHasFingerprint = false;
	bool mHasMessageIntegrity = false;
	bool mHasUseCandidate = false;
	bool mIncludeUsernameAttribute = true;
	std::shared_ptr<StunRawMessage> mRawMessage;
};

}; // namespace mediastreamer::nat
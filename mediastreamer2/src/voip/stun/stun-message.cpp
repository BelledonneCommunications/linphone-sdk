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

#include "mediastreamer2/stun-message.h"
#include "mediastreamer2/stun-raw-message.h"

namespace mediastreamer::nat {

static constexpr uint8_t kIanaProtocolNumbersUdp = 17;

std::shared_ptr<StunRawMessage> StunMessage::encode() {
	mRawMessage = std::shared_ptr<StunRawMessage>(new StunRawMessage());
	mRawMessage->addHeader(mType, mMethod, mTransactionId);

	if (mMappedAddress.has_value()) {
		mRawMessage->addAttribute(StunRawMessage::Attribute::StunMappedAddress, mMappedAddress.value());
	}
	if (mHasChangeIp || mHasChangePort) {
		mRawMessage->addAttribute(StunRawMessage::Attribute::StunChangeRequest,
		                          (mHasChangeIp ? StunRawMessage::kFlagChangeIp : 0) |
		                              (mHasChangePort ? StunRawMessage::kFlagChangePort : 0));
	}
	if (mUsername.has_value() && mIncludeUsernameAttribute) {
		mRawMessage->addAttribute(StunRawMessage::Attribute::StunUsername, mUsername.value());
	}
	if (mRealm.has_value()) {
		mRawMessage->addAttribute(StunRawMessage::Attribute::StunRealm, mRealm.value());
	}
	if (mNonce.has_value()) {
		mRawMessage->addAttribute(StunRawMessage::Attribute::StunNonce, mNonce.value());
	}
	if (mError.has_value()) {
		mRawMessage->addAttribute(StunRawMessage::Attribute::StunErrorCode, mError.value());
	}
	if (mXorMappedAddress.has_value()) {
		mRawMessage->addAttribute(StunRawMessage::Attribute::StunXorMappedAddress,
		                          mXorMappedAddress.value().toXor(mTransactionId));
	}
	if (mXorPeerAddress.has_value()) {
		mRawMessage->addAttribute(StunRawMessage::Attribute::TurnXorPeerAddress,
		                          mXorPeerAddress.value().toXor(mTransactionId));
	}
	if (mXorRelayedAddress.has_value()) {
		mRawMessage->addAttribute(StunRawMessage::Attribute::TurnXorRelayedAddress,
		                          mXorRelayedAddress.value().toXor(mTransactionId));
	}
	if (mRequestedTransport.has_value()) {
		mRawMessage->addAttribute(StunRawMessage::Attribute::TurnRequestedTransport, mRequestedTransport.value());
	}
	if (mRequestedAddressFamily.has_value()) {
		mRawMessage->addAttribute(StunRawMessage::Attribute::TurnRequestedAddressFamily,
		                          mRequestedAddressFamily.value());
	}
	if (mLifetime.has_value()) {
		mRawMessage->addAttribute(StunRawMessage::Attribute::TurnLifetime, mLifetime.value());
	}
	if (mChannelNumber.has_value()) {
		mRawMessage->addAttribute(StunRawMessage::Attribute::TurnChannelNumber, mChannelNumber.value());
	}
	if (mData.has_value()) {
		mRawMessage->addAttribute(StunRawMessage::Attribute::TurnData, mData.value());
	}
	if (mPriority.has_value()) {
		mRawMessage->addAttribute(StunRawMessage::Attribute::IcePriority, mPriority.value());
	}
	if (mHasUseCandidate) {
		mRawMessage->addAttribute(StunRawMessage::Attribute::IceUseCandidate, true);
	}
	if (mIceControlled.has_value()) {
		mRawMessage->addAttribute(StunRawMessage::Attribute::IceControlled, mIceControlled.value());
	}
	if (mIceControlling.has_value()) {
		mRawMessage->addAttribute(StunRawMessage::Attribute::IceControlling, mIceControlling.value());
	}
	if (mHasMessageIntegrity) {
		if (mHa1.has_value()) {
			if (mHa1.has_value()) {
				mRawMessage->addLongTermIntegrityFromHa1(mHa1.value());
			}
		} else if (mUsername.has_value() && mPassword.has_value()) {
			if (mRealm.has_value()) {
				mRawMessage->addLongTermIntegrity(mRealm.value(), mUsername.value(), mPassword.value());
			} else {
				mRawMessage->addShortTermIntegrity(mPassword.value(), mHasDummyMessageIntegrity);
			}
		}
	}
	if (mHasFingerprint) {
		mRawMessage->addFingerprint();
	}

	mRawMessage->updateMessageLength();

	return mRawMessage;
}

void StunMessage::setData(const char *data, size_t length) {
	mData = std::vector<uint8_t>();
	mData->assign(data, data + length);
}

void StunMessage::setNonce(const std::string &nonce) {
	mNonce = nonce;
	if (mNonce->size() > StunRawMessage::kMaxNonceLength) {
		mNonce->resize(StunRawMessage::kMaxNonceLength);
	}
}

void StunMessage::setRealm(const std::string &realm) {
	mRealm = realm;
	if (mRealm->size() > StunRawMessage::kMaxRealmLength) {
		mRealm->resize(StunRawMessage::kMaxRealmLength);
	}
}

void StunMessage::setUsername(const std::string &username) {
	mUsername = username;
	if (mUsername->size() > StunRawMessage::kMaxUsernameLength) {
		mUsername->resize(StunRawMessage::kMaxUsernameLength);
	}
}

std::shared_ptr<StunMessage> StunMessage::parse(const char *data, size_t len) {
	if (len < StunRawMessage::kMessageHeaderLength) {
		BCTBX_SLOGW << "STUN message too short!";
		return nullptr;
	}

	auto stunRawMessage = std::shared_ptr<StunRawMessage>(new StunRawMessage(data, len));
	const auto stunMessage = stunRawMessage->parse();
	if (stunMessage != nullptr) {
		stunMessage->mRawMessage = stunRawMessage;
	}
	return stunMessage;
}

std::shared_ptr<StunMessage> StunMessage::createTurnAllocateRequest() {
	auto stunMessage =
	    std::shared_ptr<StunMessage>(new StunMessage(StunMessage::Type::Request, StunMessage::Method::TurnAllocate));
	stunMessage->mRequestedTransport = kIanaProtocolNumbersUdp;
	return stunMessage;
}

std::shared_ptr<StunMessage> StunMessage::createTurnChannelBindRequest(const StunAddress &peerAddress,
                                                                       const uint16_t channelNumber) {
	auto stunMessage =
	    std::shared_ptr<StunMessage>(new StunMessage(StunMessage::Type::Request, StunMessage::Method::TurnChannelBind));
	stunMessage->mXorPeerAddress = peerAddress;
	stunMessage->mChannelNumber = channelNumber;
	return stunMessage;
}

std::shared_ptr<StunMessage> StunMessage::createTurnCreatePermissionRequest(const StunAddress &peerAddress) {
	auto stunMessage = std::shared_ptr<StunMessage>(
	    new StunMessage(StunMessage::Type::Request, StunMessage::Method::TurnCreatePermission));
	stunMessage->mXorPeerAddress = peerAddress;
	return stunMessage;
}

std::shared_ptr<StunMessage> StunMessage::createTurnRefreshRequest(const uint32_t lifetime) {
	auto stunMessage =
	    std::shared_ptr<StunMessage>(new StunMessage(StunMessage::Type::Request, StunMessage::Method::TurnRefresh));
	stunMessage->mLifetime = lifetime;
	return stunMessage;
}

std::shared_ptr<StunMessage> StunMessage::createTurnSendIndication(const StunAddress &peerAddress) {
	auto stunMessage =
	    std::shared_ptr<StunMessage>(new StunMessage(StunMessage::Type::Indication, StunMessage::Method::TurnSend));
	stunMessage->mXorPeerAddress = peerAddress;
	return stunMessage;
}

} // namespace mediastreamer::nat

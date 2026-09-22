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

#ifdef _MSC_VER
#define NOMINMAX
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cinttypes>

#include "mediastreamer2/ice-candidate-pair.h"

#include "mediastreamer2/ice-constants.h"
#include "mediastreamer2/ice-utils.h"
#include "mediastreamer2/stun-message.h"
#include "mediastreamer2/stun-raw-message.h"

namespace mediastreamer::nat {

bool IceCandidatePair::operator==(const IceCandidatePair &other) const {
	return (*mLocalCandidate == *other.mLocalCandidate) && (*mRemoteCandidate == *other.mRemoteCandidate);
}

//------------------------------------------------------------------------------

IceCandidatePair::IceCandidatePair(const std::shared_ptr<IceCandidate> &localCandidate,
                                   const std::shared_ptr<IceCandidate> &remoteCandidate,
                                   const IceRole role,
                                   const bool retryWithDummyMessageIntegrity)
    : mRole(role), mLocalCandidate(localCandidate), mRemoteCandidate(remoteCandidate),
      mRetryWithDummyMessageIntegrity(retryWithDummyMessageIntegrity) {
	if (mLocalCandidate->isDefault() && mRemoteCandidate->isDefault()) {
		mIsDefault = true;
	}
	computePriority(mRole);
}

void IceCandidatePair::computePriority(const IceRole role) {
	// Use formula defined in 5.7.2 to compute pair priority.
	uint64_t g = 0;
	uint64_t d = 0;

	switch (role) {
		case IceRole::Controlling:
			g = mLocalCandidate->getPriority();
			d = mRemoteCandidate->getPriority();
			break;
		case IceRole::Controlled:
			g = mRemoteCandidate->getPriority();
			d = mLocalCandidate->getPriority();
			break;
	}
	mPriority = (std::min(g, d) << 32) | (std::max(g, d) << 1) | (g > d ? 1 : 0);
}

void IceCandidatePair::dump(const unsigned int index) const {
	BCTBX_SLOGM << "\t" << index << " [" << this << "]: " << (isDefault() ? "*" : " ") << "state=" << getStateStr()
	            << " use=" << (mUseCandidate ? 1 : 0) << " nominated=" << (isNominated() ? 1 : 0)
	            << " priority=" << mPriority;
	mLocalCandidate->dump("\t\tLocal: ");
	mRemoteCandidate->dump("\t\tRemote: ");
}

[[nodiscard]] const std::string &IceCandidatePair::getStateStr() const {
	static const std::array<std::string, 5> stateStrs = {
	    "Waiting", "In-Progress", "Succeeded", "Failed", "Frozen",
	};
	return stateStrs[static_cast<size_t>(mState)];
}

[[nodiscard]] bool
IceCandidatePair::hasSameComponentIdAndTransportAddress(const std::shared_ptr<IceCandidatePair> &otherPair) {
	return (getLocalCandidate()->getComponentId() == otherPair->getLocalCandidate()->getComponentId()) &&
	       (getRemoteCandidate()->getComponentId() == otherPair->getRemoteCandidate()->getComponentId()) &&
	       (getLocalCandidate()->getTransportAddress() == otherPair->getLocalCandidate()->getTransportAddress()) &&
	       (getRemoteCandidate()->getTransportAddress() == otherPair->getRemoteCandidate()->getTransportAddress());
}

void IceCandidatePair::increaseRetransmissionTimer() {
	mRto = mRto * 2;
}

void IceCandidatePair::initializeRetransmissionTimer() {
	mRto = kIceDefaultRtoDuration;
	mRetransmissions = 0;
}

bool IceCandidatePair::isRetransmissionPending() const {
	return (mState == State::InProgress) && (mRetransmissions <= kIceMaxRetransmissions);
}

void IceCandidatePair::replaceSrflxCandidateByBase() {
	if (mLocalCandidate->getType() == IceCandidate::Type::ServerReflexive) {
		mLocalCandidate = mLocalCandidate->getBase();
	}
}

void IceCandidatePair::sendIndication(const RtpSession *rtpSession) {
	auto *rtpTransport = getRtpTransport(rtpSession, mLocalCandidate->getComponentId());
	if (rtpTransport == nullptr) {
		return;
	}

	const auto localCandidateTransportAddress = mLocalCandidate->getTransportAddress();
	const auto sourceStunAddress = localCandidateTransportAddress.toStunAddress();
	const auto remoteCandidateTransportAddress = mRemoteCandidate->getTransportAddress();
	const auto destStunAddress = remoteCandidateTransportAddress.toStunAddress();
	const auto indication = StunMessage::createStunBindingIndication();
	indication->enableFingerprint(true);
	// For backward compatibility
	indication->enableDummyMessageIntegrity(mUseDummyHmac);

	const auto stunRawMessage = indication->encode();
	if (stunRawMessage != nullptr) {
		BCTBX_SLOGM << "ice: Send indication for pair " << this << ": " << localCandidateTransportAddress.asString()
		            << ":" << mLocalCandidate->getTypeStr() << " --> " << remoteCandidateTransportAddress.asString()
		            << ":" << mRemoteCandidate->getTypeStr();
		const auto data = stunRawMessage->getData();
		std::ignore = sendMessageToStunAddress(rtpTransport, reinterpret_cast<const char *>(data.data()), data.size(),
		                                       sourceStunAddress, destStunAddress);
	}
}

void IceCandidatePair::setState(const State state) {
	mState = state;
}

std::shared_ptr<IceCandidatePair> IceCandidatePair::create(const std::shared_ptr<IceCandidate> &localCandidate,
                                                           const std::shared_ptr<IceCandidate> &remoteCandidate,
                                                           const IceRole role,
                                                           const bool retryWithDummyMessageIntegrity) {
	return std::shared_ptr<IceCandidatePair>(
	    new IceCandidatePair(localCandidate, remoteCandidate, role, retryWithDummyMessageIntegrity));
}

} // namespace mediastreamer::nat

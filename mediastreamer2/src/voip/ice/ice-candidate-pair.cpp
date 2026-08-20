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

#define NOMINMAX

#include <algorithm>
#include <array>
#include <chrono>
#include <cinttypes>

#include "mediastreamer2/ice-candidate-pair.h"

#include "mediastreamer2/ice-constants.h"
#include "mediastreamer2/ice-utils.h"

namespace ms2 {

bool IceCandidatePair::operator==(const IceCandidatePair &other) const {
	return (mLocalCandidate == other.mLocalCandidate) && (mRemoteCandidate == other.mRemoteCandidate);
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
	ms_message("\t%u [%p]: %sstate=%s use=%d nominated=%d priority=%" PRIu64, index, this, isDefault() ? "* " : "  ",
	           getStateStr().c_str(), mUseCandidate ? 1 : 0, mIsNominated ? 1 : 0, mPriority);
	mLocalCandidate->dump("\t\tLocal: ");
	mRemoteCandidate->dump("\t\tRemote: ");
}

[[nodiscard]] const std::string &IceCandidatePair::getStateStr() const {
	static const std::array<std::string, 5> stateStrs = {
	    "Waiting", "In-Progress", "Succeeded", "Failed", "Frozen",
	};
	return stateStrs[static_cast<size_t>(mState)];
}

void IceCandidatePair::increaseRetransmissionTimer() {
	mRto = mRto * 2;
}

void IceCandidatePair::initializeRetransmissionTimer() {
	mRto = ICE_DEFAULT_RTO_DURATION;
	mRetransmissions = 0;
}

void IceCandidatePair::initializeTransmissionTime() {
	mTransmissionTime = std::chrono::steady_clock::now();
}

bool IceCandidatePair::isRetransmissionPending() const {
	return (mState == State::InProgress) && (mRetransmissions <= ICE_MAX_RETRANSMISSIONS);
}

void IceCandidatePair::replaceSrflxCandidateByBase() {
	if (mLocalCandidate->getType() == IceCandidate::Type::ServerReflexive) {
		mLocalCandidate = mLocalCandidate->getBase();
	}
}

void IceCandidatePair::sendIndication(const RtpSession *rtpSession) {
	RtpTransport *rtpTransport = nullptr;
	if (mLocalCandidate->getComponentId() == ICE_RTP_COMPONENT_ID) {
		rtp_session_get_transports(rtpSession, &rtpTransport, nullptr);
	} else if (mLocalCandidate->getComponentId() == ICE_RTCP_COMPONENT_ID) {
		rtp_session_get_transports(rtpSession, nullptr, &rtpTransport);
	} else {
		return;
	}

	const auto localCandidateTransportAddress = mLocalCandidate->getTransportAddress();
	const auto sourceStunAddress = localCandidateTransportAddress.toStunAddress();
	const auto remoteCandidateTransportAddress = mRemoteCandidate->getTransportAddress();
	const auto destStunAddress = remoteCandidateTransportAddress.toStunAddress();
	auto *indication = ms_stun_binding_indication_create();
	ms_stun_message_enable_fingerprint(indication, TRUE);
	// For backward compatibility
	ms_stun_message_enable_dummy_message_integrity(indication, mUseDummyHmac ? TRUE : FALSE);

	char *buf = nullptr;
	const auto len = ms_stun_message_encode(indication, &buf);
	if (len > 0) {
		ms_message("ice: Send indication for pair %p: %s:%s --> %s:%s", this,
		           localCandidateTransportAddress.asString().c_str(), mLocalCandidate->getTypeStr().c_str(),
		           remoteCandidateTransportAddress.asString().c_str(), mRemoteCandidate->getTypeStr().c_str());
		IceUtils::sendMessageToStunAddress(rtpTransport, buf, len, sourceStunAddress, destStunAddress);
	}
	if (buf != nullptr) {
		ms_free(buf);
	}
	ms_free(indication);
}

void IceCandidatePair::setState(const State state) {
	mState = state;
}

} // namespace ms2

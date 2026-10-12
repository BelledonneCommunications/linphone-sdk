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

#include <array>

#include "mediastreamer2/ice-candidate.h"
#include "mediastreamer2/ice-utils.h"
#include "mediastreamer2/turn-transport.h"

namespace mediastreamer::nat {

static constexpr std::array<uint32_t, 3> kLocalPreferenceValues = {
    65535, // TurnTransport::Udp - Value recommended for non-multihomed hosts in 4.1.2.1 of RFC5245
    32767, // TurnTransport::Tcp
    49151, // TurnTransport::Tls
};

/**
 * ICE candidate type preference values as recommended in 4.1.1.2 of RFC5245.
 */
static constexpr std::array<uint8_t, 4> kTypePreferenceValues = {
    126, // Type::Host
    100, // Type::ServerReflexive
    110, // Type::PeerReflexive
    0    // Type::Relayed
};

bool IceCandidate::operator==(const IceCandidate &other) const {
	return (mType == other.mType) && (mComponentId == other.mComponentId) && (mPriority == other.mPriority) &&
	       (mTurnTransport == other.mTurnTransport) && (mTransportAddress == other.mTransportAddress);
}

bool IceCandidate::isRelay() const {
	if (mType == Type::Relayed) {
		return true;
	}

	// Case where the TURN server is itself behind a firewall.
	// A peer reflexive candidate is then discovered thanks to the binding response received from the remote.
	// This peer reflexive candidate has the relay candidate as base.
	const auto base = mBase.lock();
	return (base != nullptr) && (base->getType() == Type::Relayed);
}

void IceCandidate::setBase(const std::shared_ptr<IceCandidate> &base) {
	if (base == nullptr) {
		mBase.reset();
	} else {
		mBase = base;
	}
}

std::shared_ptr<IceCandidate> IceCandidate::create(const Type type,
                                                   const IceTransportAddress &transportAddress,
                                                   const ComponentId componentId,
                                                   const TurnTransport turnTransport) {
	auto candidate =
	    std::shared_ptr<IceCandidate>(new IceCandidate(type, transportAddress, componentId, turnTransport));

	if (candidate->isHost()) {
		candidate->setBase(candidate);
	}

	return candidate;
}

std::optional<IceCandidate::Type> IceCandidate::getTypeFromStr(const std::string &typeStr) {
	if (typeStr == "host") {
		return Type::Host;
	}
	if (typeStr == "srflx") {
		return Type::ServerReflexive;
	}
	if (typeStr == "prflx") {
		return Type::PeerReflexive;
	}
	if (typeStr == "relay") {
		return Type::Relayed;
	}
	return std::nullopt;
}

//------------------------------------------------------------------------------

IceCandidate::IceCandidate(const Type type,
                           const IceTransportAddress &transportAddress,
                           const ComponentId componentId,
                           const TurnTransport turnTransport)
    : mType(type), mTransportAddress(transportAddress), mComponentId(componentId), mTurnTransport(turnTransport) {
	computePriority();
}

void IceCandidate::computePriority() {
	const uint32_t localPreference = getLocalPreferenceValue(mTurnTransport);
	const uint32_t afPreference = (mTransportAddress.getFamily() == AF_INET6) ? 1 << 7 : 0;
	mPriority = (getTypePreferenceValue(mType) << 24) | (localPreference << 8) | afPreference |
	            (128 - static_cast<int>(mComponentId));
}

void IceCandidate::dump(const std::string &prefix) const {
	const auto base = mBase.lock();
	BCTBX_SLOGM << prefix << "[" << this << "]: " << (isDefault() ? "*" : " ") << " type=" << getTypeStr()
	            << " transport=" << turnTransportAsString(mTurnTransport) << " ip=" << mTransportAddress.getIp()
	            << " port=" << mTransportAddress.getPort() << " componentID=" << componentIdAsString(mComponentId)
	            << " priority=" << mPriority << " foundation=" << mFoundation
	            << " base=" << ((base == nullptr) ? nullptr : base.get());
}

std::shared_ptr<IceCandidate> IceCandidate::getBase() const {
	return mBase.lock();
}

uint32_t IceCandidate::getLocalPreferenceValue(const TurnTransport transport) {
	return kLocalPreferenceValues[static_cast<int>(transport)];
}

uint8_t IceCandidate::getTypePreferenceValue(const Type type) {
	return kTypePreferenceValues[static_cast<int>(type)];
}

const std::string &IceCandidate::getTypeStr(const Type type) {
	static const std::array<std::string, 4> typeStrs = {
	    "host",
	    "srflx",
	    "prflx",
	    "relay",
	};
	return typeStrs[static_cast<size_t>(type)];
}

} // namespace mediastreamer::nat

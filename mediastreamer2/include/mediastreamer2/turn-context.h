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

#include <optional>
#include <vector>

#include "ortp/rtpsession.h"

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/sockaddr.h"
#include "mediastreamer2/stun-address.h"
#include "mediastreamer2/stun.h"
#include "mediastreamer2/turn-tcp-client.h"

namespace mediastreamer::nat {

class IceCheckList;
class TurnTcpClient;

class MS2_PUBLIC TurnContext {
public:
	friend class IceCheckList;

	enum class State {
		Idle,
		CreatingAllocation,
		AllocationCreated,
		CreatingPermissions,
		PermissionsCreated,
		BindingChannel,
		ChannelBound,
	};

	enum class Transport {
		Udp,
		Tcp,
		Tls,
	};

	enum class Type {
		Rtp,
		Rtcp,
	};

	TurnContext(const Type type, RtpSession *rtpSession) : mRtpSession(rtpSession), mType(type) {
	}
	~TurnContext();

	void allowPeerAddress(const StunAddress &address);
	RtpTransport *createEndpoint();
	void enableForcedRtpSendingViaRelay(const bool force) {
		mForcedRtpSendingViaRelay = force;
	}
	[[nodiscard]] std::optional<uint16_t> getChannelNumber() const {
		return mChannelNumber;
	}
	[[nodiscard]] const std::optional<std::string> &getCn() const {
		return mCn;
	}
	[[nodiscard]] const std::optional<std::string> &getHa1() const {
		return mHa1;
	}
	[[nodiscard]] std::optional<uint32_t> getLifetime() const {
		return mLifetime;
	}
	[[nodiscard]] const std::optional<std::string> &getNonce() const {
		return mNonce;
	}
	[[nodiscard]] const std::shared_ptr<TurnTcpClient> &getOrCreateTcpClient();
	[[nodiscard]] const std::optional<std::string> &getPassword() const {
		return mPassword;
	}
	[[nodiscard]] const std::optional<std::string> &getRealm() const {
		return mRealm;
	}
	[[nodiscard]] const std::optional<std::string> &getRootCertificatePath() const {
		return mRootCertificatePath;
	}
	[[nodiscard]] const SockAddr &getServerSockAddr() const {
		return mServerSockAddr;
	}
	[[nodiscard]] State getState() const {
		return mState;
	}
	[[nodiscard]] const MSTurnContextStatistics &getStatistics() const {
		return mStatistics;
	}
	[[nodiscard]] Transport getTransport() const {
		return mTransport;
	}
	[[nodiscard]] const std::optional<std::string> &getUsername() const {
		return mUsername;
	}
	[[nodiscard]] bool hasForcedRtpSendingViaRelay() const {
		return mForcedRtpSendingViaRelay;
	}
	void setAllocatedRelayAddress(const StunAddress &relayAddress) {
		mRelayAddress = relayAddress;
	}
	void setChannelNumber(const uint16_t channelNumber) {
		mChannelNumber = channelNumber;
	}
	void setCn(const std::string &cn) {
		mCn = cn;
	}
	void setHa1(const std::string &ha1) {
		mHa1 = ha1;
	}
	void setLifetime(const uint32_t lifetime) {
		mLifetime = lifetime;
	}
	void setNonce(const std::string &nonce) {
		mNonce = nonce;
	}
	void setPassword(const std::string &password) {
		mPassword = password;
	}
	void setRealm(const std::string &realm) {
		mRealm = realm;
	}
	void setRootCertificatePath(const std::string &rootCertificatePath) {
		mRootCertificatePath = rootCertificatePath;
	}
	void setServerAddress(const SockAddr &sockAddr);
	void setState(State state);
	void setTransport(const Transport transport) {
		mTransport = transport;
	}
	void setUsername(const std::string &username) {
		mUsername = username;
	}

private:
	[[nodiscard]] std::string getStateStr() const;
	[[nodiscard]] std::string getTypeStr() const;
	[[nodiscard]] bool isPeerAddressAllowed(const StunAddress &address) const;
	[[nodiscard]] bool rtpEndpointShouldBeSentToTurnServer(const SockAddr &toAddr) const;
	[[nodiscard]] bool rtpEndpointShouldSendViaTurnRelay(const SockAddr &fromAddr) const;

	static int
	rtpEndpointRecvfrom(RtpTransport *rtpTransport, mblk_t *msg, int flags, struct sockaddr *from, socklen_t *fromLen);
	static int
	rtpEndpointSendto(RtpTransport *rtpTransport, mblk_t *msg, int flags, const struct sockaddr *to, socklen_t toLen);
	static void rtpEndpointClose(RtpTransport *rtpTransport);
	static void rtpEndpointDestroy(RtpTransport *rtpTransport);

	RtpSession *mRtpSession = nullptr;
	RtpTransport *mEndpoint = nullptr;
	std::vector<StunAddress> mAllowedPeerAddresses;
	std::optional<std::string> mRealm = std::nullopt;
	std::optional<std::string> mNonce = std::nullopt;
	std::optional<std::string> mUsername = std::nullopt;
	std::optional<std::string> mPassword = std::nullopt;
	std::optional<std::string> mHa1 = std::nullopt;
	std::optional<std::string> mRootCertificatePath = std::nullopt;
	std::optional<std::string> mCn = std::nullopt;
	std::optional<uint32_t> mLifetime = std::nullopt;
	std::optional<uint16_t> mChannelNumber = std::nullopt;
	State mState = State::Idle;
	Type mType = Type::Rtp;
	Transport mTransport = Transport::Udp;
	StunAddress mRelayAddress;
	SockAddr mServerSockAddr;
	bool mForcedRtpSendingViaRelay = false;
	MSTurnContextStatistics mStatistics{};
	std::shared_ptr<TurnTcpClient> mTurnTcpClient;
};

}; // namespace mediastreamer::nat
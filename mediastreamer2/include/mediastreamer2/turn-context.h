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

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "ortp/rtpsession.h"

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/sockaddr.h"
#include "mediastreamer2/stun-address.h"
#include "mediastreamer2/turn-transport.h"

namespace mediastreamer::nat {

class IceCheckList;
class TurnClient;
class TurnContext;

class MS2_PUBLIC TurnTransportContext {
public:
	friend class IceCheckList;
	friend class TurnContext;

	enum class State {
		Idle,
		CreatingAllocation,
		AllocationCreated,
		CreatingPermissions,
		PermissionsCreated,
		Deallocating,
		BindingChannel,
		ChannelBound,
	};

	struct Statistics {
		uint32_t nbSendIndication;
		uint32_t nbDataIndication;
		uint32_t nbReceivedChannelMsg;
		uint32_t nbSentChannelMsg;
		uint16_t nbSuccessfulAllocate;
		uint16_t nbSuccessfulRefresh;
		uint16_t nbSuccessfulCreatePermission;
		uint16_t nbSuccessfulChannelBind;
	};

	TurnTransportContext(TurnTransport transport, TurnContext *context) : mTransport(transport), mContext(context) {
	}
	~TurnTransportContext() = default;

	void allowPeerAddress(const StunAddress &address);
	[[nodiscard]] std::optional<uint16_t> getChannelNumber() const {
		return mChannelNumber;
	}
	[[nodiscard]] std::optional<std::string> getHa1() const {
		return mHa1;
	}
	[[nodiscard]] std::optional<uint32_t> getLifetime() const {
		return mLifetime;
	}
	[[nodiscard]] std::optional<std::string> getNonce() const {
		return mNonce;
	}
	[[nodiscard]] std::optional<std::string> getPassword() const {
		return mPassword;
	}
	[[nodiscard]] std::optional<std::string> getRealm() const {
		return mRealm;
	}
	[[nodiscard]] const SockAddr &getServerSockAddr() const {
		return mServerSockAddr;
	}
	[[nodiscard]] State getState() const {
		return mState;
	}
	[[nodiscard]] const Statistics &getStatistics() const {
		return mStatistics;
	}
	[[nodiscard]] std::optional<std::string> getUsername() const {
		return mUsername;
	}
	void setAllocatedRelayAddress(const StunAddress &relayAddress) {
		mRelayAddress = relayAddress;
	}
	void setChannelNumber(const uint16_t channelNumber) {
		mChannelNumber = channelNumber;
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
	void setState(State state);
	void setUsername(const std::string &username) {
		mUsername = username;
	}

private:
	[[nodiscard]] StunAddress getRelayAddress() const {
		return mRelayAddress;
	}
	[[nodiscard]] std::string getStateStr() const;
	[[nodiscard]] bool isPeerAddressAllowed(const StunAddress &address) const;

	TurnTransport mTransport = TurnTransport::Udp;
	TurnContext *mContext = nullptr;
	std::vector<StunAddress> mAllowedPeerAddresses;
	std::optional<std::string> mRealm;
	std::optional<std::string> mNonce;
	std::optional<std::string> mUsername;
	std::optional<std::string> mPassword;
	std::optional<std::string> mHa1;
	std::optional<uint32_t> mLifetime;
	std::optional<uint16_t> mChannelNumber;
	State mState = State::Idle;
	StunAddress mRelayAddress;
	SockAddr mServerSockAddr;
	std::shared_ptr<TurnClient> mTurnClient;
	Statistics mStatistics{};
};

class MS2_PUBLIC TurnContext {
public:
	friend class IceCheckList;
	friend class TurnTransportContext;

	enum class Type {
		Rtp,
		Rtcp,
	};

	TurnContext(Type type, RtpSession *rtpSession);
	~TurnContext();

	[[nodiscard]] bool allStatesAreIdle() const;
	void connect();
	RtpTransport *createEndpoint();
	void enableForcedRtpSendingViaRelay(const bool force) {
		mForcedRtpSendingViaRelay = force;
	}
	[[nodiscard]] const std::optional<std::string> &getCn() const {
		return mCn;
	}
	[[nodiscard]] const std::optional<std::string> &getRootCertificatePath() const {
		return mRootCertificatePath;
	}
	[[nodiscard]] RtpSession *getRtpSession() const {
		return mRtpSession;
	}
	[[nodiscard]] TurnTransportContext *getTransportContext(TurnTransport transport) const;
	[[nodiscard]] Type getType() const {
		return mType;
	}
	[[nodiscard]] bool hasForcedRtpSendingViaRelay() const {
		return mForcedRtpSendingViaRelay;
	}
	void setCn(const std::string &cn) {
		mCn = cn;
	}
	void setRootCertificatePath(const std::string &rootCertificatePath) {
		mRootCertificatePath = rootCertificatePath;
	}
	void setServerAddresses(const std::map<TurnTransport, SockAddr> &serverSockAddrMap);

	static TurnTransport getTurnTransportFromMsg(const mblk_t *msg);
	static void setTurnTransportInMsg(mblk_t *msg, TurnTransport turnTransport);

private:
	[[nodiscard]] std::shared_ptr<TurnClient> getTurnClient(TurnTransport transport) const;
	[[nodiscard]] std::string getTypeStr() const;
	[[nodiscard]] bool rtpEndpointShouldBeSentToTurnServer(const SockAddr &toAddr, TurnTransport transport) const;
	[[nodiscard]] bool rtpEndpointShouldSendViaTurnRelay(TurnTransport transport, const SockAddr &fromAddr) const;
	[[nodiscard]] int recvFrom(mblk_t *msg, int flags, struct sockaddr *from, socklen_t *fromLen) const;
	[[nodiscard]] int sendTo(mblk_t *msg, int flags, const struct sockaddr *to, socklen_t toLen) const;

	static int
	rtpEndpointRecvfrom(RtpTransport *rtpTransport, mblk_t *msg, int flags, struct sockaddr *from, socklen_t *fromLen);
	static int
	rtpEndpointSendto(RtpTransport *rtpTransport, mblk_t *msg, int flags, const struct sockaddr *to, socklen_t toLen);
	static void rtpEndpointClose(RtpTransport *rtpTransport);
	static void rtpEndpointDestroy(RtpTransport *rtpTransport);

	RtpSession *mRtpSession = nullptr;
	RtpTransport *mEndpoint = nullptr;
	std::optional<std::string> mRootCertificatePath = std::nullopt;
	std::optional<std::string> mCn = std::nullopt;
	Type mType = Type::Rtp;
	std::map<TurnTransport, TurnTransportContext> mTransportContextMap;
	bool mForcedRtpSendingViaRelay = false;
};

}; // namespace mediastreamer::nat
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

#include <memory>

#include "bctoolbox/crypto.h"

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/stun-address.h"
#include "mediastreamer2/turn-socket.h"

namespace mediastreamer::nat {

class TurnContext;
class TurnSocket;

class MS2_PUBLIC TurnClient {
public:
	virtual ~TurnClient() = 0;

	virtual void connect() = 0;
	[[nodiscard]] virtual TurnTransport getTransport() const = 0;
	virtual int recvfrom(mblk_t *msg, int flags, struct sockaddr *from, socklen_t *fromlen) = 0;
	virtual int sendto(mblk_t *msg, int flags, const struct sockaddr *to, socklen_t tolen) = 0;
};

class MS2_PUBLIC TurnUdpClient : public TurnClient {
public:
	explicit TurnUdpClient(TurnContext *context);
	~TurnUdpClient() override = default;

	TurnUdpClient(const TurnUdpClient &) = delete;
	TurnUdpClient(TurnUdpClient &&) = delete;

	void connect() override {
	}
	[[nodiscard]] TurnTransport getTransport() const override {
		return TurnTransport::Udp;
	}
	int recvfrom(mblk_t *msg, int flags, struct sockaddr *from, socklen_t *fromlen) override;
	int sendto(mblk_t *msg, int flags, const struct sockaddr *to, socklen_t tolen) override;

private:
	TurnContext *mContext = nullptr;
};

class MS2_PUBLIC TurnTcpClient : public TurnClient {
	friend class TurnSocket;

public:
	explicit TurnTcpClient(TurnContext *context);
	~TurnTcpClient() override;

	TurnTcpClient(const TurnTcpClient &) = delete;
	TurnTcpClient(TurnTcpClient &&) = delete;

	void connect() override;
	[[nodiscard]] TurnTransport getTransport() const override {
		return TurnTransport::Tcp;
	}
	int recvfrom(mblk_t *msg, int flags, struct sockaddr *from, socklen_t *fromlen) override;
	int sendto(mblk_t *msg, int flags, const struct sockaddr *to, socklen_t tolen) override;

private:
	[[nodiscard]] const TurnContext *getContext() const {
		return mContext;
	}
	[[nodiscard]] bctbx_rng_context_t *getRng() const {
		return mRng;
	}

	TurnContext *mContext = nullptr;
	std::unique_ptr<TurnSocket> mTurnConnection;
	StunAddress mTurnAddress;
	bctbx_rng_context_t *mRng = nullptr;
};

class MS2_PUBLIC TurnTlsClient : public TurnTcpClient {
public:
	explicit TurnTlsClient(TurnContext *context);
	~TurnTlsClient() override = default;

	TurnTlsClient(const TurnTlsClient &) = delete;
	TurnTlsClient(TurnTlsClient &&) = delete;

	[[nodiscard]] TurnTransport getTransport() const override {
		return TurnTransport::Tls;
	}
	int recvfrom(mblk_t *msg, int flags, struct sockaddr *from, socklen_t *fromlen) override;
};

} // namespace mediastreamer::nat

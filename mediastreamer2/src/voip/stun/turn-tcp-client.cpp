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

#include <memory>
#include <utility>

#if !defined(WIN32) && !defined(_WIN32_WCE)
#include <netinet/tcp.h>
#else
#include <winsock2.h>
#endif

#include "bctoolbox/crypto.h"
#include "bctoolbox/defs.h"

#include "mediastreamer2/turn-tcp-client.h"

namespace mediastreamer::nat {

TurnTcpClient::TurnTcpClient(TurnContext *context) : mContext(context) {

	mRng = bctbx_rng_context_new();
}

TurnTcpClient::~TurnTcpClient() {
	if (mTurnConnection != nullptr) {
		mTurnConnection->stop();
	}
	if (mRng != nullptr) {
		bctbx_rng_context_free(mRng);
	}
}

void TurnTcpClient::connect() {
	if (mTurnConnection != nullptr) {
		return;
	}

	try {
		mTurnConnection = std::make_unique<TurnSocket>(this);
		mTurnConnection->start();
	} catch (std::exception &e) {
		BCTBX_SLOGE << "TurnClient: could not create TurnSocket: " << e.what();
	}
}

int TurnTcpClient::recvfrom(mblk_t *msg, BCTBX_UNUSED(int flags), struct sockaddr *from, socklen_t *fromlen) {
	if (mTurnConnection == nullptr) {
		return 0;
	}

	std::unique_ptr<Packet> p = nullptr;
	mTurnConnection->mReceivingLock.lock();
	if (!mTurnConnection->mReceivingQueue.empty()) {
		p = std::move(mTurnConnection->mReceivingQueue.front());
		mTurnConnection->mReceivingQueue.pop();
	}
	mTurnConnection->mReceivingLock.unlock();
	if (p == nullptr) {
		return 0;
	}

	const auto bufsz = static_cast<size_t>(msg->b_datap->db_lim - msg->b_datap->db_base);
	if (p->length() > bufsz) {
		/* This should not happen, but the copy must be protected against buffer overflow. */
		BCTBX_SLOGW << "TurnClient::recvfrom(): truncating packet of size " << p->length() << " to " << bufsz
		            << " bytes";
		p->setLength(bufsz);
	}
	memcpy(msg->b_wptr, p->data(), p->length());

	// Set from and fromlen to the turn server address
	*fromlen = mContext->getServerSockAddr().getLen();
	memcpy(from, mContext->getServerSockAddr().asStructSockAddr(), *fromlen);

	// Set the net_addr for the modifiers
	memcpy(&msg->net_addr, from, *fromlen);
	msg->net_addrlen = *fromlen;

	// Set the recv_addr
	struct sockaddr_storage addr{};
	socklen_t addrlen = sizeof(addr);
	getsockname(mTurnConnection->mSocket, reinterpret_cast<struct sockaddr *>(&addr), &addrlen);
	ortp_sockaddr_to_recvaddr(reinterpret_cast<struct sockaddr *>(&addr), &msg->recv_addr);

	return static_cast<int>(p->length());
}

int TurnTcpClient::sendto(mblk_t *msg,
                          BCTBX_UNUSED(int flags),
                          BCTBX_UNUSED(const struct sockaddr *to),
                          BCTBX_UNUSED(socklen_t tolen)) {
	if ((mTurnConnection == nullptr) || !mTurnConnection->isRunning()) {
		return -1;
	}
	auto p = std::make_unique<Packet>(msg, true); // Add padding at this point.
	p->setTimestampCurrent();

	const auto length = static_cast<int>(p->length());

	mTurnConnection->addToSendingQueue(std::move(p));

	return length;
}

} // namespace mediastreamer::nat

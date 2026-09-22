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

#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

#include "bctoolbox/crypto.h"
#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/turn-context.h"

#ifdef WIN32

#define TURN_EWOULDBLOCK WSAEWOULDBLOCK
#define TURN_EINPROGRESS WSAEINPROGRESS
#define TURN_EINTR WSAEINTR

#else

#include <poll.h>

#define TURN_EWOULDBLOCK EWOULDBLOCK
#define TURN_EINPROGRESS EINPROGRESS
#define TURN_EINTR EINTR

#ifndef INVALID_SOCKET
#define INVALID_SOCKET static_cast<ortp_socket_t>(-1)
#endif

#endif

namespace mediastreamer::nat {

class TurnContext;

/* A simple class that encapsulate the mblk_t for the purpose of our Turn client/sockets */
class Packet {
public:
	explicit Packet(size_t size);
	Packet(const uint8_t *buffer, size_t size);
	/* Create a packet from a mblk_t, possibly adding necessary padding (because STUN/TURN packets must be 4-bytes
	 * padded). */
	Packet(mblk_t *msg, bool withPadding);

	~Packet();

	[[nodiscard]] uint8_t *data() const {
		return mMblk->b_rptr;
	}

	void addReadOffset(const size_t off) const {
		mMblk->b_rptr += off;
	}

	[[nodiscard]] size_t length() const {
		return msgdsize(mMblk);
	}
	void setLength(const size_t size) const {
		mMblk->b_wptr = mMblk->b_rptr + size;
	}

	void concat(const std::unique_ptr<Packet> &other, size_t size = static_cast<size_t>(-1)) const;

	[[nodiscard]] uint64_t timestamp() const {
		return mTimestamp;
	}
	void setTimestampCurrent();

private:
	mblk_t *mMblk;
	uint64_t mTimestamp;
};

class PacketReader {
public:
	explicit PacketReader(TurnContext *context);
	~PacketReader() = default;

	PacketReader(const PacketReader &) = delete;
	PacketReader(PacketReader &&) = delete;

	void reset();

	int parseData(std::unique_ptr<Packet> rawPacket);

	std::unique_ptr<Packet> getTurnPacket();

private:
	enum State { WaitingHeader, Continuation } mState;

	int parsePacket(std::unique_ptr<Packet> packet);
	int processContinuationPacket(std::unique_ptr<Packet> packet);

	TurnContext *mContext = nullptr;

	std::unique_ptr<Packet> mCurPacket = nullptr;
	std::list<std::unique_ptr<Packet>> mTurnPackets;
	size_t mRemainingBytes = 0; // When in continuation state
};

// -------------------------------------------------------------------------------------------------------

class SslContext {
	friend class TurnSocket;

public:
	SslContext(ortp_socket_t socket,
	           const std::string &rootCertificatePath,
	           const std::string &cn,
	           bctbx_rng_context_t *rng);
	~SslContext();

	SslContext(const SslContext &) = delete;
	SslContext(SslContext &&) = delete;

	[[nodiscard]] int connect();
	[[nodiscard]] int close() const;

	[[nodiscard]] int read(unsigned char *buffer, size_t length) const;
	[[nodiscard]] int write(const unsigned char *buffer, size_t length) const;

private:
	bctbx_ssl_context_t *mContext;
	bctbx_ssl_config_t *mConfig;
	bctbx_x509_certificate_t *mRootCertificate;
	ortp_socket_t mSocket;
};

// -------------------------------------------------------------------------------------------------------

// This is an simple encapsulation to ease the code and prevent a spurious wakeup
class Condition {
public:
	Condition() = default;
	~Condition() = default;

	Condition(const Condition &) = delete;
	Condition(Condition &&) = delete;

	void wait(std::unique_lock<std::mutex> &lock) {
		condition.wait(lock, [this] { return ready; });
		ready = false;
	}

	void signal() {
		ready = true;
		condition.notify_all();
	}

private:
	std::condition_variable condition;
	bool ready = false;
};

class TurnTcpClient;

class SocketException : public std::runtime_error {
public:
	explicit SocketException(const char *message);
};

class ControlSocketPair {
public:
	ControlSocketPair();
	~ControlSocketPair();

	void cleanEvent() const;
	[[nodiscard]] ortp_socket_t getSocket() const;
	void notifyEvent() const;

private:
	ortp_socket_t mEmitter = INVALID_SOCKET, mReaderMother = INVALID_SOCKET, mReader = INVALID_SOCKET;
};

class TurnSocket {
	friend class TurnTcpClient;

public:
	TurnSocket(TurnTcpClient *client);
	~TurnSocket();

	TurnSocket(const TurnSocket &) = delete;
	TurnSocket(TurnSocket &&) = delete;

	[[nodiscard]] int connect();
	void close();

	void start();
	void stop();

	void processRead();

	[[nodiscard]] int send(const std::unique_ptr<Packet> &p);

	void addToSendingQueue(std::unique_ptr<Packet> p);
	void addToReceivingQueue(std::unique_ptr<Packet> p);

	[[nodiscard]] int getPort() const;
	[[nodiscard]] bool isRunning() const {
		return mRunning;
	}
	[[nodiscard]] static int turnPoll(ortp_socket_t socket, int milliseconds, int events);

private:
	/* wait an event on the supplied socket.
	 * The ControlSocketPair is also added in the poll(), so that it can be interrupted promptly by
	 * simply calling ControlSocketPair::notify().
	 * return value: 1-> something happened on the socket;  0->timeout; -1; controller has been notified.
	 */
	[[nodiscard]] static int
	waitSocketEvent(const ControlSocketPair &controller, ortp_socket_t socket, int milliseconds, int events);

	void runSend();
	void runRead();

	// The control socket pair is just to control the recv thread
	ControlSocketPair mRecvControlSocket;
	TurnTcpClient *mClient = nullptr;

	bool mRunning = false;
	bool mSendThreadSleeping = false;
	bool mReady = false;
	bool mError = false;
	bool mThreadsJoined = false;

	std::thread mSendingThread;
	std::thread mReceivingThread;
	ortp_socket_t mSocket = INVALID_SOCKET;

	std::mutex mSslLock;
	std::unique_ptr<SslContext> mSsl;

	std::mutex mSendingLock;
	Condition mQueueCond;
	std::queue<std::unique_ptr<Packet>> mSendingQueue;

	std::mutex mReceivingLock;
	std::queue<std::unique_ptr<Packet>> mReceivingQueue;

	PacketReader mPacketReader;
	static constexpr int kDefaultPollTimeoutMs = 30000;
};

} // namespace mediastreamer::nat

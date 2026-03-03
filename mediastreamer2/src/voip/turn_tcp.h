/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
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

#ifndef MS_TURN_TCP_H
#define MS_TURN_TCP_H

#include <condition_variable>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

#include "bctoolbox/crypto.h"
#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/stun.h"

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

namespace ms2 {

namespace turn {

/* A simple class that encapsulate the mblk_t for the purpose of our Turn client/sockets */
class Packet {
public:
	Packet(size_t size);
	Packet(const uint8_t *buffer, size_t size);
	/* Create a packet from a mblk_t, possibly adding necessary padding (because STUN/TURN packets must be 4-bytes
	 * padded). */
	Packet(mblk_t *msg, bool withPadding);

	~Packet();

	uint8_t *data() const {
		return mMblk->b_rptr;
	}

	void addReadOffset(size_t off) {
		mMblk->b_rptr += off;
	}

	size_t length() const {
		return msgdsize(mMblk);
	}
	void setLength(size_t size) {
		mMblk->b_wptr = mMblk->b_rptr + size;
	}

	void concat(const std::unique_ptr<Packet> &other, size_t size = -1);

	uint64_t timestamp() const {
		return mTimestamp;
	}
	void setTimestampCurrent();

private:
	mblk_t *mMblk;
	uint64_t mTimestamp;
};

class PacketReader {
public:
	PacketReader(MSTurnContext *context);
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

	MSTurnContext *mContext;

	std::unique_ptr<Packet> mCurPacket;
	std::list<std::unique_ptr<Packet>> mTurnPackets;
	size_t mRemainingBytes = 0; /*when in continuation state*/
};

// -------------------------------------------------------------------------------------------------------

class SslContext {
	friend class TurnSocket;

public:
	SslContext(ortp_socket_t socket, std::string rootCertificatePath, std::string cn, bctbx_rng_context_t *rng);
	~SslContext();

	SslContext(const SslContext &) = delete;
	SslContext(SslContext &&) = delete;

	int connect();
	int close();

	int read(unsigned char *buffer, size_t length);
	int write(const unsigned char *buffer, size_t length);

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

class TurnClient;

class SocketException : public std::runtime_error {
public:
	SocketException(const char *message);
};

class ControlSocketPair {
public:
	ControlSocketPair();
	~ControlSocketPair();
	void notifyEvent();
	void cleanEvent();
	ortp_socket_t getSocket();

private:
	ortp_socket_t mEmitter = INVALID_SOCKET, mReaderMother = INVALID_SOCKET, mReader = INVALID_SOCKET;
};

class TurnSocket {
	friend class TurnClient;

public:
	TurnSocket(TurnClient *client, int port);
	~TurnSocket();

	TurnSocket(const TurnSocket &) = delete;
	TurnSocket(TurnSocket &&) = delete;

	int connect();
	void close();

	void start();
	void stop();

	void processRead();

	int send(std::unique_ptr<Packet> p);

	void addToSendingQueue(std::unique_ptr<Packet> p);
	void addToReceivingQueue(std::unique_ptr<Packet> p);

	int getPort() const {
		return mPort;
	}
	bool isRunning() const {
		return mRunning;
	}
	static int turnPoll(ortp_socket_t socket, int milliseconds, int events);

private:
	/* wait an event on the supplied socket.
	 * The ControlSocketPair is also added in the poll(), so that it can be interrupted promptly by
	 * simply calling ControlSocketPair::notify().
	 * return value: 1-> something happened on the socket;  0->timeout; -1; controller has been notified.
	 */
	int waitSocketEvent(ControlSocketPair &controller, ortp_socket_t socket, int milliseconds, int events);
	void runSend();
	void runRead();

	/* the control socket pair is just to control the recv thread */
	ControlSocketPair mRecvControlSocket;
	TurnClient *mClient;
	int mPort;

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
	static const constexpr int defaultPollTimeoutMs = 30000;
};

class TurnClient {
	friend class TurnSocket;

public:
	TurnClient(MSTurnContext *context, bool useSsl, std::string rootCertificatePath = "");
	~TurnClient();

	TurnClient(const TurnClient &) = delete;
	TurnClient(TurnClient &&) = delete;

	void connect();

	int recvfrom(mblk_t *msg, int flags, struct sockaddr *from, socklen_t *fromlen);
	int sendto(mblk_t *msg, int flags, const struct sockaddr *to, socklen_t tolen);

private:
	void runRead();

	MSTurnContext *mContext;

	std::unique_ptr<TurnSocket> mTurnConnection;

	MSStunAddress mTurnAddress;
	std::string mTurnServerCn;
	std::string mTurnServerIp;
	int mTurnServerPort;

	bool mUseSsl;
	std::string mRootCertificatePath;

	bctbx_rng_context_t *mRng;
};

} // namespace turn

} // namespace ms2

#endif /* MS_TURN_TCP_H */

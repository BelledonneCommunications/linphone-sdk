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
#include <memory>
#include <sys/stat.h>

#include "bctoolbox/logging.h"

#if !defined(WIN32) && !defined(_WIN32_WCE)
#include <netinet/tcp.h>
#else
#include <winsock2.h>
#endif

#include <bctoolbox/crypto.h>

#include "mediastreamer2/turn-socket.h"

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/turn-tcp-client.h"

static constexpr unsigned int MTU_MAX = 1500;
static constexpr uint64_t FLOW_CONTROL_MAX_TIME = 3000;

namespace ms2::nat {

Packet::Packet(const size_t size) : mTimestamp(0) {
	mMblk = allocb(size, 0);
}

Packet::Packet(const uint8_t *buffer, const size_t size) : mTimestamp(0) {
	mMblk = allocb(size, 0);
	memcpy(mMblk->b_wptr, buffer, size);
	mMblk->b_wptr += size;
}

Packet::Packet(mblk_t *msg, const bool withPadding) : mTimestamp(0) {
	const size_t size = msgdsize(msg);
	const size_t paddedSize = (size + 3) & (~0x3);

	if (msg->b_cont != nullptr || (paddedSize != size && withPadding)) {
		msgpullup(msg, paddedSize);
		msg->b_wptr = msg->b_rptr + paddedSize;
	}
	mMblk = dupb(msg);
}

void Packet::concat(const std::unique_ptr<Packet> &other, size_t size) const {
	if (size == static_cast<size_t>(-1)) {
		size = other->length();
	}
	msgappend(mMblk, reinterpret_cast<const char *>(other->mMblk->b_rptr), size, FALSE);
	if (mMblk->b_cont != nullptr) {
		msgpullup(mMblk, static_cast<size_t>(-1));
	}
}

void Packet::setTimestampCurrent() {
	mTimestamp = ms_get_cur_time_ms();
}

Packet::~Packet() {
	freemsg(mMblk);
}

PacketReader::PacketReader(TurnContext *context) : mState(WaitingHeader), mContext(context) {
}

void PacketReader::reset() {
	if (mCurPacket) {
		mCurPacket.reset();
	}
	mState = WaitingHeader;
	mRemainingBytes = 0;
}

int PacketReader::parseData(std::unique_ptr<Packet> rawPacket) {
	switch (mState) {
		case WaitingHeader:
			return parsePacket(std::move(rawPacket));
			break;
		case Continuation:
			return processContinuationPacket(std::move(rawPacket));
			break;
	}
	return 0;
}

std::unique_ptr<Packet> PacketReader::getTurnPacket() {
	if (!mTurnPackets.empty()) {
		auto packet = std::move(mTurnPackets.front());
		mTurnPackets.pop_front();

		return packet;
	}

	return nullptr;
}

int PacketReader::parsePacket(std::unique_ptr<Packet> packet) {
	uint8_t *p = packet->data();
	size_t paddedLen;
	const uint8_t *pEnd = p + packet->length();
	int foundPackets = 0;
	bool channelData = false;

	while (p < pEnd) {
		channelData = (mContext->getState() >= TurnContext::State::BindingChannel) && ((*p & 0x40) != 0);

		uint8_t *header = p;
		const size_t headerSize = channelData ? 4 : 20;
		const size_t datalen = paddedLen = ntohs(*reinterpret_cast<uint16_t *>(p + sizeof(uint16_t)));

		if (channelData && (datalen + 4) % 4 != 0) {
			// The size of a channelData in TCP/TLS is rounded to a multiple of 4
			// and not reflected in the length field
			const size_t round = 4 + datalen;
			paddedLen = round - (round % 4);
		}

		p += headerSize;
		auto remainingSize = static_cast<size_t>(pEnd - p);

		if (paddedLen > remainingSize) {
			mState = Continuation;
			mRemainingBytes = paddedLen - remainingSize;
			packet->addReadOffset(header - packet->data());
			mCurPacket = std::move(packet);
			break;
		}

		p += paddedLen;
		foundPackets++;

		if (p == pEnd && foundPackets == 1) {
			if (channelData && datalen < paddedLen) {
				// Set packet length to actual data length to get rid of padding
				packet->setLength(headerSize + datalen);
			}

			mTurnPackets.push_back(std::move(packet));
			break;
		}
		if (header != nullptr) {
			// Use datalen instead of paddedLen to get rid of padding
			mTurnPackets.push_back(std::make_unique<Packet>(header, headerSize + datalen));
		}
	}

	return 0;
}

int PacketReader::processContinuationPacket(std::unique_ptr<Packet> packet) {
	const size_t to_read = std::min<size_t>(packet->length(), mRemainingBytes);
	mRemainingBytes -= to_read;

	mCurPacket->concat(packet, to_read);

	if (mRemainingBytes == 0) {
		mTurnPackets.push_back(std::move(mCurPacket));
		mCurPacket = nullptr;
		mState = WaitingHeader;
		// Check if they are remaining bytes not used in the packet
		if (to_read < packet->length()) {
			packet->addReadOffset(to_read);
			return parsePacket(std::move(packet));
		}
	}
	return 0;
}

// -------------------------------------------------------------------------------------------------------

static int tls_callback_certificate_verify(void *data, bctbx_x509_certificate_t *cert, int depth, uint32_t *flags) {
	constexpr int tmp_size = 2048;
	constexpr int flags_str_size = 256;
	auto *tmp = static_cast<char *>(malloc(tmp_size));
	auto *flags_str = static_cast<char *>(malloc(flags_str_size));

	bctbx_x509_certificate_get_info_string(tmp, tmp_size - 1, "", cert);
	bctbx_x509_certificate_flags_to_string(flags_str, flags_str_size - 1, *flags);

	BCTBX_SLOGM << "SslContext [" << data << "]: found certificate depth=[" << depth << "], flags=[" << flags_str
	            << "]:\n"
	            << tmp;

	free(flags_str);
	free(tmp);

	return 0;
}

static int random_generator(void *ctx, unsigned char *ptr, size_t size) {
	auto *rng = static_cast<bctbx_rng_context_t *>(ctx);
	bctbx_rng_get(rng, ptr, size);

	return 0;
}

static int tls_callback_read(void *ctx, unsigned char *buf, size_t len) {
	const auto *socket = static_cast<ortp_socket_t *>(ctx);

	const auto ret = recv(*socket, reinterpret_cast<char *>(buf), static_cast<int>(len), 0);
	if (ret < 0) {
		const int socketError = getSocketErrorCode();
		if (socketError == TURN_EWOULDBLOCK || socketError == TURN_EINPROGRESS || socketError == TURN_EINTR) {
			return BCTBX_ERROR_NET_WANT_READ;
		}
		return BCTBX_ERROR_NET_CONN_RESET;
	}
	return static_cast<int>(ret);
}

static int tls_callback_write(void *ctx, const unsigned char *buf, size_t len) {
	const auto *socket = static_cast<ortp_socket_t *>(ctx);

	const auto ret = send(*socket, reinterpret_cast<const char *>(buf), static_cast<int>(len), 0);
	if (ret < 0) {
		const int socketError = getSocketErrorCode();
		if (socketError == TURN_EWOULDBLOCK || socketError == TURN_EINPROGRESS || socketError == TURN_EINTR) {
			return BCTBX_ERROR_NET_WANT_WRITE;
		}
		return BCTBX_ERROR_NET_CONN_RESET;
	}
	return static_cast<int>(ret);
}

SslContext::SslContext(const ortp_socket_t socket,
                       const std::string &rootCertificatePath,
                       const std::string &cn,
                       bctbx_rng_context_t *rng)
    : mSocket(socket) {
	mContext = bctbx_ssl_context_new();
	mConfig = bctbx_ssl_config_new();

	bctbx_ssl_config_defaults(mConfig, BCTBX_SSL_IS_CLIENT, BCTBX_SSL_TRANSPORT_STREAM);

	if (!rootCertificatePath.empty()) {
		struct stat statbuf{};
		if (stat(rootCertificatePath.c_str(), &statbuf) == 0) {
			mRootCertificate = bctbx_x509_certificate_new();

			if ((statbuf.st_mode & S_IFDIR) != 0) {
				if (bctbx_x509_certificate_parse_path(mRootCertificate, rootCertificatePath.c_str()) < 0) {
					BCTBX_SLOGE << "SslContext [" << this
					            << "]: Failed to load ca from directory: " << rootCertificatePath;
					bctbx_x509_certificate_free(mRootCertificate);
					mRootCertificate = nullptr;
				}
			} else {
				if (bctbx_x509_certificate_parse_file(mRootCertificate, rootCertificatePath.c_str()) < 0) {
					BCTBX_SLOGE << "SslContext [" << this << "]: Failed to load ca from file: " << rootCertificatePath;
					bctbx_x509_certificate_free(mRootCertificate);
					mRootCertificate = nullptr;
				}
			}

			BCTBX_SLOGM << "SslContext [" << this << "]: get root certificate from: " << rootCertificatePath;
		} else {
			BCTBX_SLOGE << "SslContext [" << this << "]: could not load root ca from: " << rootCertificatePath << " ("
			            << strerror(errno) << ")";
		}

		bctbx_ssl_config_set_ca_chain(mConfig, mRootCertificate);
		bctbx_ssl_config_set_authmode(mConfig, BCTBX_SSL_VERIFY_REQUIRED);
		bctbx_ssl_config_set_callback_verify(mConfig, tls_callback_certificate_verify, this);
	} else {
		bctbx_ssl_config_set_authmode(mConfig, BCTBX_SSL_VERIFY_NONE);
		mRootCertificate = nullptr;
	}

	bctbx_ssl_config_set_rng(mConfig, random_generator, rng);
	bctbx_ssl_context_setup(mContext, mConfig);
	bctbx_ssl_set_io_callbacks(mContext, &mSocket, tls_callback_write, tls_callback_read);

	if (!cn.empty()) {
		bctbx_ssl_set_hostname(mContext, cn.c_str());
	}
}

SslContext::~SslContext() {
	std::ignore = close();

	bctbx_ssl_context_free(mContext);
	bctbx_ssl_config_free(mConfig);
	bctbx_x509_certificate_free(mRootCertificate);
}

int SslContext::connect() {
	int error = bctbx_ssl_handshake(mContext);
	if (error == BCTBX_ERROR_NET_WANT_READ || error == BCTBX_ERROR_NET_WANT_WRITE) {
		return error;
	}
	if (error < 0) {
		char errbuf[1024] = {0};
		bctbx_strerror(error, errbuf, sizeof(errbuf) - 1);
		BCTBX_SLOGE << "SslContext [" << this << "]: ssl_handshake failed (" << error << "): " << errbuf;
		return -1;
	}
	return error;
}

int SslContext::close() const {
	return bctbx_ssl_close_notify(mContext);
}

int SslContext::read(unsigned char *buffer, const size_t length) const {
	return bctbx_ssl_read(mContext, buffer, length);
}

int SslContext::write(const unsigned char *buffer, const size_t length) const {
	return bctbx_ssl_write(mContext, buffer, length);
}

// -------------------------------------------------------------------------------------------------------

SocketException::SocketException(const char *message)
    : std::runtime_error(std::string(message + std::string(getSocketError()))) {
}

/*
 * Creates two TCP sockets connected through local loopback.
 * This is portable way to control execution of a thread that waits in poll().
 */
ControlSocketPair::ControlSocketPair() {
	struct sockaddr_in listeningAddr{};
	socklen_t socket_size = sizeof(listeningAddr);
	mEmitter = socket(AF_INET, SOCK_STREAM, 0);
	mReaderMother = socket(AF_INET, SOCK_STREAM, 0);

	if (mEmitter == INVALID_SOCKET || mReaderMother == INVALID_SOCKET) {
		throw SocketException("Failure to create sockets");
	}
	listeningAddr.sin_family = AF_INET;
	listeningAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	listeningAddr.sin_port = 0; /* let the system choose */
	int err = bind(mReaderMother, reinterpret_cast<struct sockaddr *>(&listeningAddr), sizeof(listeningAddr));
	if (err == -1) {
		throw SocketException("Failure to bind socket");
	}
	err = getsockname(mReaderMother, reinterpret_cast<struct sockaddr *>(&listeningAddr), &socket_size);
	if (err == -1) {
		throw SocketException("Failure to get socket address");
	}
	err = listen(mReaderMother, 1);
	if (err == -1) {
		throw SocketException("Failure to listen on socket");
	}
	set_non_blocking_socket(mReaderMother);
	set_non_blocking_socket(mEmitter);
	err = ::connect(mEmitter, reinterpret_cast<struct sockaddr *>(&listeningAddr), socket_size);
	if (TurnSocket::turnPoll(mReaderMother, 2000, POLLIN) != 1) {
		throw SocketException("Failure to listen on socket");
	}
	struct sockaddr_in ignored{};
	socklen_t ignored_size = sizeof(ignored);
	mReader = ::accept(mReaderMother, reinterpret_cast<struct sockaddr *>(&ignored), &ignored_size);
	if (mReader == INVALID_SOCKET) {
		throw SocketException("Failure to accept connection");
	}
	if (TurnSocket::turnPoll(mEmitter, 2000, POLLIN | POLLOUT) != 1) {
		throw SocketException("Failure to connect");
	}
	set_non_blocking_socket(mReader);
}

ControlSocketPair::~ControlSocketPair() {
	if (mEmitter != INVALID_SOCKET) {
		close_socket(mEmitter);
	}
	if (mReaderMother != INVALID_SOCKET) {
		close_socket(mReaderMother);
	}
	if (mReader != INVALID_SOCKET) {
		close_socket(mReader);
	}
}

void ControlSocketPair::cleanEvent() const {
	std::array<uint8_t, 16> buffer{};
	while (::recv(mReader, reinterpret_cast<char *>(buffer.data()), static_cast<int>(buffer.size()), 0) > 0) {
		// Purge data, otherwise the socket may immediately declare that there is something to read
	}
}
ortp_socket_t ControlSocketPair::getSocket() const {
	return mReader;
}

void ControlSocketPair::notifyEvent() const {
	uint8_t data = 0;
	const auto err = ::send(mEmitter, reinterpret_cast<char *>(&data), 1, 0);
	if (err != 1) {
		BCTBX_SLOGE << "ControlSocketPair::notifyEvent failure: " << err << getSocketError();
	}
}

// -------------------------------------------------------------------------------------------------------

TurnSocket::TurnSocket(TurnTcpClient *client) : mClient(client), mPacketReader(client->mContext) {
}

TurnSocket::~TurnSocket() {
	stop();
}

int TurnSocket::connect() {
	const auto [ip, port] = mClient->getContext()->getServerSockAddr().getIpPort();
	struct addrinfo *ai = bctbx_name_to_addrinfo(AF_UNSPEC, SOCK_STREAM, ip.c_str(), port);
	if (ai == nullptr) {
		BCTBX_SLOGE << "TurnSocket [" << this << "]: getaddrinfo failed for " << ip << ":" << port;
		bctbx_freeaddrinfo(ai);
		return -1;
	}

	mSocket = ::socket(ai->ai_family, SOCK_STREAM, 0);
	if (mSocket == -1) {
		BCTBX_SLOGE << "TurnSocket [" << this << "]: could not create socket";
		bctbx_freeaddrinfo(ai);
		return -1;
	}

	int optVal = 1;
	if (setsockopt(mSocket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char *>(&optVal), sizeof(optVal)) != 0) {
		BCTBX_SLOGE << "TurnSocket [" << this << "]: failed to activate TCP_NODELAY: " << getSocketError();
	}
	optVal = 0;
	if (setsockopt(mSocket, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<char *>(&optVal), sizeof(optVal)) != 0) {
		BCTBX_SLOGE << "TurnSocket [" << this << "]: failed to enable dual-stack mode: " << getSocketError();
	}

	set_non_blocking_socket(mSocket);
	BCTBX_SLOGM << "TurnSocket [" << this << "]: trying to connect to " << ip << ":" << port;

	int error = ::connect(mSocket, ai->ai_addr, static_cast<int>(ai->ai_addrlen));
	if (error != 0 && getSocketErrorCode() != TURN_EWOULDBLOCK && getSocketErrorCode() != TURN_EINPROGRESS) {
		BCTBX_SLOGE << "TurnSocket [" << this << "]: connect failed: " << getSocketError();
		bctbx_freeaddrinfo(ai);
		close();
		return -1;
	}

	bctbx_freeaddrinfo(ai);

	error = waitSocketEvent(mRecvControlSocket, mSocket, defaultPollTimeoutMs, POLLIN | POLLOUT);
	if (error == 0) {
		BCTBX_SLOGE << "TurnSocket [" << this << "]: connect time-out";
		close();
		return -1;
	}
	if (error < 0) {
		BCTBX_SLOGM << "TurnSocket [" << this << "]: need to exit now.";
		close();
		return -1;
	}

	optVal = 0;
	socklen_t optLen = sizeof(optVal);
	error = getsockopt(mSocket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&optVal), &optLen);
	if (error != 0) {
		BCTBX_SLOGE << "TurnSocket [" << this << "]: failed to retrieve connection status: " << getSocketError();
		close();
		return -1;
	}
	if (optVal != 0) {
		BCTBX_SLOGE << "TurnSocket [" << this << "]: failed to connect to server (" << optVal
		            << "): " << getSocketErrorWithCode(optVal);
		close();
		return -1;
	}
	BCTBX_SLOGM << "TurnSocket [" << this << "]: connected at TCP level.";

	// TODO: Add HTTP Proxy connection here if needed

	if (mClient->getContext()->getTransport() == TurnContext::Transport::Tls) {
		const auto rootCertificatePath = mClient->getContext()->getRootCertificatePath();
		const auto cn = mClient->getContext()->getCn();
		mSsl = std::make_unique<SslContext>(mSocket, rootCertificatePath.has_value() ? rootCertificatePath.value() : "",
		                                    cn.has_value() ? cn.value() : "", mClient->getRng());

		do {
			error = mSsl->connect();
			if (error == BCTBX_ERROR_NET_WANT_READ || error == BCTBX_ERROR_NET_WANT_WRITE) {
				int waitError = waitSocketEvent(mRecvControlSocket, mSocket, defaultPollTimeoutMs,
				                                error == BCTBX_ERROR_NET_WANT_READ ? POLLIN : POLLOUT);
				if (waitError == -1) {
					BCTBX_SLOGM << "TurnSocket::connect(): need to abort TLS handshake";
					break;
				}
				if (waitError == 0) {
					BCTBX_SLOGM << "TurnSocket::connect(): timeout during TLS handshake";
					break;
				}
				BCTBX_SLOGM << "TurnSocket::connect(): TLS handshake in progress...";
			} else {
				break;
			}
		} while (true);
		if (error < 0) {
			BCTBX_SLOGE << "TurnSocket [" << this << "]: SSL handshake failed";
			mSsl.reset();
			close();
			return -1;
		}
	}

	// Set a low sndbuf because we don't want flow control, we prefer loosing packets
	optVal = 1200 * 8;
	error = setsockopt(mSocket, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char *>(&optVal), sizeof(optVal));
	if (error != 0) {
		BCTBX_SLOGE << "TurnSocket [" << this << "]: setsockopt SO_SNDBUF failed: " << getSocketError();
	}

	// Set a timeout for output operation
	struct timeval tv{};
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	error = setsockopt(mSocket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<char *>(&tv), sizeof(tv));
	if (error != 0) {
		BCTBX_SLOGE << "TurnSocket [" << this << "]: setsockopt SO_SNDTIMEO failed: " << getSocketError();
	}

	BCTBX_SLOGM << "TurnSocket [" << this << "]: connected to turn server " << ip << ":" << port;
	mReady = true;

	return 0;
}

void TurnSocket::close() {
	mReady = false;

	if (mSsl) {
		std::ignore = mSsl->close();
		mSsl.reset();
	}

	if (mSocket != INVALID_SOCKET) {
		close_socket(mSocket);
		mSocket = INVALID_SOCKET;
	}

	mPacketReader.reset();
}

void TurnSocket::addToSendingQueue(std::unique_ptr<Packet> p) {
	mSendingLock.lock();
	mSendingQueue.push(std::move(p));
	if (mSendThreadSleeping) {
		// Manual unlocking is done before notifying, to avoid waking up
		// the waiting thread only to block again
		// https://en.cppreference.com/w/cpp/thread/condition_variable
		mSendingLock.unlock();
		mQueueCond.signal();
		return;
	}
	mSendingLock.unlock();
}

void TurnSocket::addToReceivingQueue(std::unique_ptr<Packet> p) {
	std::lock_guard<std::mutex> lk(mReceivingLock);
	mReceivingQueue.push(std::move(p));
}

void TurnSocket::start() {
	if (!mRunning) {
		mThreadsJoined = false;
		mRunning = true;
		mSendingThread = std::thread(&TurnSocket::runSend, this);
		mReceivingThread = std::thread(&TurnSocket::runRead, this);
	}
}

void TurnSocket::stop() {
	if (mRunning) {
		mRunning = false;
		// make the recv thread exit from poll()
		mRecvControlSocket.notifyEvent();
	}

	mSendingLock.lock();
	if (mSendThreadSleeping) {
		mQueueCond.signal();
	}
	mSendingLock.unlock();

	if (!mThreadsJoined) {
		mSendingThread.join();
		mReceivingThread.join();
		close();
		mThreadsJoined = true;
	}

	while (!mSendingQueue.empty()) {
		mSendingQueue.pop();
	}

	while (!mReceivingQueue.empty()) {
		mReceivingQueue.pop();
	}
}

int TurnSocket::turnPoll(const ortp_socket_t socket, const int milliseconds, const int events) {
	struct pollfd pfd{};

	pfd.fd = socket;
	pfd.events = static_cast<short>(events);
	pfd.revents = 0;
#ifdef WIN32
	return WSAPoll(&pfd, 1, milliseconds);
#else
	return poll(&pfd, 1, milliseconds);
#endif
}

int TurnSocket::waitSocketEvent(const ControlSocketPair &controller,
                                const ortp_socket_t socket,
                                const int milliseconds,
                                const int events) {
	struct pollfd pfd[2] = {};
	int err;

	pfd[0].fd = socket;
	pfd[0].events = static_cast<short>(events);
	pfd[0].revents = 0;
	pfd[1].fd = controller.getSocket();
	pfd[1].events = POLLIN;
	pfd[1].revents = 0;

#ifdef WIN32
	err = WSAPoll(pfd, 2, milliseconds);
#else
	err = poll(pfd, 2, milliseconds);
#endif
	if (err == 0) {
		return 0;
	}
	if (err == -1) {
		BCTBX_SLOGE << "TurnSocket: error in poll(): " << getSocketError();
		return -1;
	}
	if (pfd[1].revents != 0) {
		controller.cleanEvent();
		return -1;
	}
	if (pfd[0].revents != 0) {
		return 1;
	}
	BCTBX_SLOGE << "TurnSocket: should not happen." << getSocketError();
	return -1;
}

void TurnSocket::processRead() {
	const int err = waitSocketEvent(mRecvControlSocket, mSocket, defaultPollTimeoutMs, POLLIN);
	if (err == 1) {
		int bytes = -1;
		auto p = std::make_unique<Packet>(MTU_MAX);

		if (mSsl) {
			bytes = mSsl->read(p->data(), MTU_MAX);
		} else {
			bytes = static_cast<int>(::recv(mSocket, reinterpret_cast<char *>(p->data()), MTU_MAX, 0));
		}

		if (bytes < 0) {
			if (getSocketErrorCode() != TURN_EWOULDBLOCK) {
				if (mSsl) {
					if (bytes == BCTBX_ERROR_SSL_PEER_CLOSE_NOTIFY) {
						BCTBX_SLOGM << "TurnSocket [" << this << "]: connection closed by remote.";
					} else {
						BCTBX_SLOGE << "TurnSocket [" << this << "]: SSL error while reading: " << bytes;
					}
				} else {
					BCTBX_SLOGE << "TurnSocket [" << this << "]: read error: " << getSocketError();
				}
				mError = true;
			}
		} else if (bytes == 0) {
			BCTBX_SLOGW << "TurnSocket [" << this << "]: closed by remote";
			mError = true;
		} else {
			p->setLength(bytes);
			mPacketReader.parseData(std::move(p));
			while ((p = mPacketReader.getTurnPacket()) != nullptr) {
				addToReceivingQueue(std::move(p));
			}
		}
	} else if (err == -1) {
		BCTBX_SLOGM << "TurnSocket::processRead: need to exit.";
		mError = true;
	}
}

int TurnSocket::send(const std::unique_ptr<Packet> &packet) {
	int error;

	if (mSsl) {
		error = mSsl->write(packet->data(), packet->length());
	} else {
		error = static_cast<int>(
		    ::send(mSocket, reinterpret_cast<const char *>(packet->data()), static_cast<int>(packet->length()), 0));
	}

	if (error <= 0) {
		if (getSocketErrorCode() != TURN_EWOULDBLOCK) {
			if (mSsl) {
				if (error == BCTBX_ERROR_NET_CONN_RESET) {
					BCTBX_SLOGW << "TurnSocket [" << this << "]: server disconnected us";
				} else {
					BCTBX_SLOGE << "TurnSocket [" << this << "]: SSL error while sending: " << error;
				}
			} else {
				if (error == -1) {
					BCTBX_SLOGE << "TurnSocket [" << this << "]: fail to send: " << getSocketError();
				} else {
					BCTBX_SLOGW << "TurnSocket [" << this << "]: server disconnected us";
				}
			}
		} else {
			error = -TURN_EWOULDBLOCK;
		}
	}

	return error;
}

void TurnSocket::runSend() {
	bool purging = false;

	while (mRunning) {
		std::unique_lock<std::mutex> lk(mSendingLock);
		mSendThreadSleeping = false;
		if (!mSendingQueue.empty()) {
			auto p = std::move(mSendingQueue.front());
			mSendingQueue.pop();
			lk.unlock();

			const uint64_t lPacketAge = ms_get_cur_time_ms() - p->timestamp();
			if (!purging && (lPacketAge > FLOW_CONTROL_MAX_TIME || mError)) {
				if (mError) {
					BCTBX_SLOGW << "TurnSocket [" << this << "]: purging queue on send error";
				} else {
					BCTBX_SLOGW << "TurnSocket [" << this << "]: purging queue packet age [" << lPacketAge << "]";
				}
				purging = true;
			}

			if (!purging && mReady) {
				mSslLock.lock();
				const int error = send(p);
				mSslLock.unlock();

				if (error == -TURN_EWOULDBLOCK) {
					continue; // Will retry
				}
				if (error < 0) {
					mError = true;
				}
			}
		} else {
			purging = false;
			if (mRunning) {
				mSendThreadSleeping = true;
				mQueueCond.wait(lk);
				mSendThreadSleeping = false;
			}
			lk.unlock();
		}
	}
}

void TurnSocket::runRead() {
	while (mRunning) {
		if (mSocket == INVALID_SOCKET) {
			if (connect() < 0) {
				ms_usleep(500000);
			}
		} else {
			processRead();
			if (mError) {
				mSslLock.lock();
				close();
				mError = false;
				mSslLock.unlock();
				/*
				 * No need to reconnect, this should not happen.
				 * TODO: notify the error to the upper layer, as the Turn context may decide to restart entirely if the
				 * disconnection is not expected.
				 */
				mRunning = false;
			}
		}
	}
}

int TurnSocket::getPort() const {
	const auto [_ip, port] = mClient->getContext()->getServerSockAddr().getIpPort();
	return port;
}

} // namespace ms2::nat

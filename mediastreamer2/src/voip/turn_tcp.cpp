/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <memory>
#include <sys/stat.h>
#include <sys/types.h>

#if !defined(WIN32) && !defined(_WIN32_WCE)
#include <netinet/tcp.h>
#else
#include <winsock2.h>
#endif

#include <bctoolbox/crypto.h>
#include <mediastreamer2/mscommon.h>
#include <mediastreamer2/stun.h>

#include "turn_tcp.h"

extern "C" MSTurnTCPClient *ms_turn_tcp_client_new(MSTurnContext *context, bool_t use_ssl,
												   const char *root_certificate_path) {
	return (MSTurnTCPClient *)(new TurnClient(context, use_ssl,
											  root_certificate_path == NULL ? std::string() : root_certificate_path));
}

extern "C" void ms_turn_tcp_client_destroy(MSTurnTCPClient *turn_tcp_client) {
	delete ((TurnClient *)turn_tcp_client);
}

extern "C" void ms_turn_tcp_client_connect(MSTurnTCPClient *turn_tcp_client) {
	((TurnClient *)turn_tcp_client)->connect();
}

extern "C" int ms_turn_tcp_client_recvfrom(MSTurnTCPClient *turn_tcp_client, mblk_t *msg, int flags,
										   struct sockaddr *from, socklen_t *fromlen) {
	return ((TurnClient *)turn_tcp_client)->recvfrom(msg, flags, from, fromlen);
}

extern "C" int ms_turn_tcp_client_sendto(MSTurnTCPClient *turn_tcp_client, mblk_t *msg, int flags,
										 const struct sockaddr *to, socklen_t tolen) {
	return ((TurnClient *)turn_tcp_client)->sendto(msg, flags, to, tolen);
}

// -------------------------------------------------------------------------------------------------------

Packet::Packet(size_t size) : mSize(size), mPhysSize(size), mTimestamp(0) {
	mData = mBuf = (uint8_t *)ms_malloc(size);
}

Packet::Packet(uint8_t *buffer, size_t size) : mSize(size), mPhysSize(size), mTimestamp(0) {
	mData = mBuf = (uint8_t *)ms_malloc(size);
	memcpy(mData, buffer, size);
}

Packet::Packet(mblk_t *msg) : mTimestamp(0) {
	if (msg->b_cont != NULL) {
		msgpullup(msg, -1);
	}

	mSize = mPhysSize = (size_t)(msg->b_wptr - msg->b_rptr);
	mData = mBuf = (uint8_t *)ms_malloc(mSize);
	memcpy(mData, msg->b_rptr, mSize);
}

void Packet::concat(const std::unique_ptr<Packet> &other, size_t size) {
	if (size == (size_t)-1) {
		size = other->mSize;
	}

	size_t newSize = mPhysSize + size;
	size_t offset = mData - mBuf;
	mBuf = (uint8_t *)realloc(mBuf, newSize);
	mData = mBuf + offset;
	memcpy(mData + mSize, other->mData, size);
	mPhysSize = newSize;
	mSize += size;
}

void Packet::setTimestampCurrent() {
	mTimestamp = ms_get_cur_time_ms();
}

Packet::~Packet() {
	ms_free(mBuf);
}

PacketReader::PacketReader(MSTurnContext *context) : mState(WaitingHeader), mContext(context) {
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
	uint8_t *header;
	size_t headerSize;
	size_t datalen;
	uint8_t *pEnd = p + packet->length();
	int foundPackets = 0;
	bool channelData = false;

	while (p < pEnd) {
		size_t remainingSize;

		channelData = (ms_turn_context_get_state(mContext) >= MS_TURN_CONTEXT_STATE_BINDING_CHANNEL) && (*p & 0x40);

		header = p;
		headerSize = channelData ? 4 : 20;
		datalen = ntohs(*((uint16_t *)(p + sizeof(uint16_t))));

		if (channelData && (datalen + 4) % 4 != 0) {
			// The size of a channelData in TCP/TLS is rounded to a multiple of 4
			// and not reflected in the length field
			size_t round = 4 + datalen;
			datalen = round - (round % 4);
		}

		p += headerSize;
		remainingSize = (size_t)(pEnd - p);

		if (datalen > remainingSize) {
			mState = Continuation;
			mRemainingBytes = datalen - remainingSize;
			packet->setOffset(header - packet->data());
			mCurPacket = std::move(packet);
			break;
		}

		p += datalen;
		foundPackets++;

		if (p == pEnd && foundPackets == 1) {
			mTurnPackets.push_back(std::move(packet));
			break;
		} else {
			if (header) {
				mTurnPackets.push_back(std::make_unique<Packet>(header, headerSize + datalen));
			}
		}
	}

	return 0;
}

int PacketReader::processContinuationPacket(std::unique_ptr<Packet> packet) {
	size_t to_read = std::min<size_t>(packet->length(), mRemainingBytes);
	mRemainingBytes -= to_read;

	mCurPacket->concat(packet, to_read);

	if (mRemainingBytes == 0) {
		mTurnPackets.push_back(std::move(mCurPacket));
		mCurPacket = nullptr;
		mState = WaitingHeader;
		/* check if they are remaining bytes not used in the packet*/
		if (to_read < packet->length()) {
			packet->setOffset(to_read);
			return parsePacket(std::move(packet));
		}
	}
	return 0;
}

// -------------------------------------------------------------------------------------------------------

static int tls_callback_certificate_verify(void *data, bctbx_x509_certificate_t *cert, int depth, uint32_t *flags) {
	const int tmp_size = 2048, flags_str_size = 256;
	char *tmp = (char *)malloc(tmp_size);
	char *flags_str = (char *)malloc(flags_str_size);

	bctbx_x509_certificate_get_info_string(tmp, tmp_size - 1, "", cert);
	bctbx_x509_certificate_flags_to_string(flags_str, flags_str_size - 1, *flags);

	ms_message("SslContext [%p]: found certificate depth=[%i], flags=[%s]:\n%s", (SslContext *)data, depth, flags_str,
			   tmp);

	free(flags_str);
	free(tmp);

	return 0;
}

static int random_generator(void *ctx, unsigned char *ptr, size_t size) {
	bctbx_rng_context_t *rng = (bctbx_rng_context_t *)ctx;
	bctbx_rng_get(rng, ptr, size);

	return 0;
}

static int tls_callback_read(void *ctx, unsigned char *buf, size_t len) {
	ortp_socket_t *socket = (ortp_socket_t *)ctx;

	int ret = recv(*socket, (char *)buf, len, 0);
	if (ret < 0) {
		ret = -ret;
		if (ret == TURN_EWOULDBLOCK || ret == TURN_EINPROGRESS || ret == TURN_EINTR)
			return BCTBX_ERROR_NET_WANT_READ;
		return BCTBX_ERROR_NET_CONN_RESET;
	}
	return ret;
}

static int tls_callback_write(void *ctx, const unsigned char *buf, size_t len) {
	ortp_socket_t *socket = (ortp_socket_t *)ctx;

	int ret = send(*socket, (const char *)buf, len, 0);
	if (ret < 0) {
		ret = -ret;
		if (ret == TURN_EWOULDBLOCK || ret == TURN_EINPROGRESS || ret == TURN_EINTR)
			return BCTBX_ERROR_NET_WANT_WRITE;
		return BCTBX_ERROR_NET_CONN_RESET;
	}
	return ret;
}

SslContext::SslContext(ortp_socket_t socket, std::string rootCertificatePath, std::string cn, bctbx_rng_context_t *rng)
	: mSocket(socket) {
	mContext = bctbx_ssl_context_new();
	mConfig = bctbx_ssl_config_new();

	bctbx_ssl_config_defaults(mConfig, BCTBX_SSL_IS_CLIENT, BCTBX_SSL_TRANSPORT_STREAM);

	if (!rootCertificatePath.empty()) {
		struct stat statbuf;
		if (stat(rootCertificatePath.c_str(), &statbuf) == 0) {
			mRootCertificate = bctbx_x509_certificate_new();

			if (statbuf.st_mode & S_IFDIR) {
				if (bctbx_x509_certificate_parse_path(mRootCertificate, rootCertificatePath.c_str()) < 0) {
					ms_error("SslContext [%p]: Failed to load ca from directory: %s", this,
							 rootCertificatePath.c_str());
					bctbx_x509_certificate_free(mRootCertificate);
					mRootCertificate = NULL;
				}
			} else {
				if (bctbx_x509_certificate_parse_file(mRootCertificate, rootCertificatePath.c_str()) < 0) {
					ms_error("SslContext [%p]: Failed to load ca from file: %s", this, rootCertificatePath.c_str());
					bctbx_x509_certificate_free(mRootCertificate);
					mRootCertificate = NULL;
				}
			}

			ms_message("SslContext [%p]: get root certificate from: %s", this, rootCertificatePath.c_str());
		} else {
			ms_error("SslContext [%p]: could not load root ca from: %s (%s)", this, rootCertificatePath.c_str(),
					 strerror(errno));
		}

		bctbx_ssl_config_set_ca_chain(mConfig, mRootCertificate);
		bctbx_ssl_config_set_authmode(mConfig, BCTBX_SSL_VERIFY_REQUIRED);
		bctbx_ssl_config_set_callback_verify(mConfig, tls_callback_certificate_verify, this);
	} else {
		bctbx_ssl_config_set_authmode(mConfig, BCTBX_SSL_VERIFY_NONE);
		mRootCertificate = NULL;
	}

	bctbx_ssl_config_set_rng(mConfig, random_generator, rng);
	bctbx_ssl_set_io_callbacks(mContext, &mSocket, tls_callback_write, tls_callback_read);
	bctbx_ssl_context_setup(mContext, mConfig);

	if (!cn.empty()) {
		bctbx_ssl_set_hostname(mContext, cn.c_str());
	}
}

SslContext::~SslContext() {
	close();

	bctbx_ssl_context_free(mContext);
	bctbx_ssl_config_free(mConfig);
	bctbx_x509_certificate_free(mRootCertificate);
}

int SslContext::connect() {
	int error = bctbx_ssl_handshake(mContext);
	if (error < 0) {
		char errbuf[1024] = {0};
		bctbx_strerror(error, errbuf, sizeof(errbuf) - 1);
		ms_error("SslContext [%p]: ssl_handshake failed (%i): %s", this, error, errbuf);
		return -1;
	}
	return error;
}

int SslContext::close() {
	return bctbx_ssl_close_notify(mContext);
}

int SslContext::read(unsigned char *buffer, size_t length) {
	return bctbx_ssl_read(mContext, buffer, length);
}

int SslContext::write(const unsigned char *buffer, size_t length) {
	return bctbx_ssl_write(mContext, buffer, length);
}

// -------------------------------------------------------------------------------------------------------

TurnSocket::TurnSocket(TurnClient *client, int port) : mClient(client), mPort(port), mPacketReader(client->mContext) {
}

TurnSocket::~TurnSocket() {
	stop();
}

int TurnSocket::connect() {
	struct addrinfo *ai =
		bctbx_name_to_addrinfo(AF_UNSPEC, SOCK_STREAM, mClient->mTurnServerIp.c_str(), mClient->mTurnServerPort);
	if (!ai) {
		ms_error("TurnSocket [%p]: getaddrinfo failed for %s:%d", this, mClient->mTurnServerIp.c_str(),
				 mClient->mTurnServerPort);
		bctbx_freeaddrinfo(ai);
		return -1;
	}

	mSocket = ::socket(ai->ai_family, SOCK_STREAM, 0);
	if (mSocket == -1) {
		ms_error("TurnSocket [%p]: could not create socket", this);
		bctbx_freeaddrinfo(ai);
		return -1;
	}

	int optVal = 1;
	if (setsockopt(mSocket, IPPROTO_TCP, TCP_NODELAY, (char *)&optVal, sizeof(optVal)) != 0) {
		ms_error("TurnSocket [%p]: failed to activate TCP_NODELAY: %s", this, getSocketError());
	}

	set_non_blocking_socket(mSocket);
	ms_message("TurnSocket [%p]: trying to connect to %s:%d", this, mClient->mTurnServerIp.c_str(),
			   mClient->mTurnServerPort);

	int error = ::connect(mSocket, ai->ai_addr, ai->ai_addrlen);
	if (error != 0 && getSocketErrorCode() != TURN_EWOULDBLOCK && getSocketErrorCode() != TURN_EINPROGRESS) {
		ms_error("TurnSocket [%p]: connect failed: %s", this, getSocketError());
		bctbx_freeaddrinfo(ai);
		close();
		return -1;
	}

	bctbx_freeaddrinfo(ai);

	error = turnPoll(mSocket, 5, true);
	if (error == 0) {
		ms_error("TurnSocket [%p]: connect time-out", this);
		close();
		return -1;
	} else if (error < 0) {
		ms_error("TurnSocket [%p]: unexpected error: %s", this, getSocketError());
		close();
		return -1;
	}

	optVal = 0;
	socklen_t optLen = sizeof(optVal);
	error = getsockopt(mSocket, SOL_SOCKET, SO_ERROR, (char *)&optVal, &optLen);
	if (error != 0) {
		ms_error("TurnSocket [%p]: failed to retrieve connection status: %s", this, getSocketError());
		close();
		return -1;
	} else if (optVal != 0) {
		ms_error("TurnSocket [%p]: failed to connect to server (%d): %s", this, optVal, getSocketErrorWithCode(optVal));
		close();
		return -1;
	}

	// Add HTTP Proxy connection here if needed

	set_blocking_socket(mSocket);

	if (mClient->mUseSsl) {
		mSsl =
			std::make_unique<SslContext>(mSocket, mClient->mRootCertificatePath, mClient->mTurnServerCn, mClient->mRng);

		error = mSsl->connect();
		if (error < 0) {
			ms_error("TurnSocket [%p]: SSL handshake failed", this);
			mSsl.reset();
			close();
			return -1;
		}
	}

	// Set a low sndbuf because we don't want flow control, we prefer loosing packets
	optVal = 1200 * 8;
	error = setsockopt(mSocket, SOL_SOCKET, SO_SNDBUF, (char *)&optVal, sizeof(optVal));
	if (error != 0) {
		ms_error("TurnSocket [%p]: setsockopt SO_SNDBUF failed: %s", this, getSocketError());
	}

	// Set a timeout for output operation
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	error = setsockopt(mSocket, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv));
	if (error != 0) {
		ms_error("TurnSocket [%p]: setsockopt SO_SNDTIMEO failed: %s", this, getSocketError());
	}

	ms_message("TurnSocket [%p]: connected to turn server %s:%d", this, mClient->mTurnServerIp.c_str(),
			   mClient->mTurnServerPort);
	mReady = true;

	return 0;
}

void TurnSocket::close() {
	mReady = false;

	if (mSsl) {
		mSsl->close();
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
	if (!mSendRunning) {
		// Manual unlocking is done before notifying, to avoid waking up
		// the waiting thread only to block again
		// https://en.cppreference.com/w/cpp/thread/condition_variable
		mSendingLock.unlock();
		mQueueCond.signal();
	}
	mSendingLock.unlock();
}

void TurnSocket::addToReceivingQueue(std::unique_ptr<Packet> p) {
	std::lock_guard<std::mutex> lk(mReceivingLock);
	mReceivingQueue.push(std::move(p));
}

void TurnSocket::start() {
	if (!mRunning) {
		mRunning = true;
		mSendingThread = std::thread(&TurnSocket::runSend, this);
		mReceivingThread = std::thread(&TurnSocket::runRead, this);
	}
}

void TurnSocket::stop() {
	if (mRunning) {
		mRunning = false;

		// Clear sending queue
		mSendingLock.lock();

		while (!mSendingQueue.empty()) {
			mSendingQueue.pop();
		}

		if (!mSendRunning) {
			mQueueCond.signal();
		}

		mSendingLock.unlock();

		// Clear receiving queue
		mReceivingLock.lock();

		while (!mReceivingQueue.empty()) {
			mReceivingQueue.pop();
		}

		mReceivingLock.unlock();

		mSendingThread.join();
		mReceivingThread.join();
		close();
	}
}

void TurnSocket::processRead() {
	int bytes = -1;

	if (turnPoll(mSocket, 5, false) == 1) {
		auto p = std::make_unique<Packet>(MTU_MAX);

		if (mSsl) {
			bytes = mSsl->read(p->data(), MTU_MAX);
		} else {
			bytes = ::recv(mSocket, (char *)p->data(), MTU_MAX, 0);
		}

		if (bytes < 0) {
			if (getSocketErrorCode() != TURN_EWOULDBLOCK) {
				if (mSsl) {
					ms_error("TurnSocket [%p]: SSL error while reading: %i", this, bytes);
				} else
					ms_error("TurnSocket [%p]: read error: %s", this, getSocketError());
				mError = true;
			}
		} else if (bytes == 0) {
			ms_warning("TurnSocket [%p]: closed by remote", this);
			mError = true;
		} else {
			p->setLength(bytes);
			mPacketReader.parseData(std::move(p));
			while ((p = mPacketReader.getTurnPacket()) != nullptr) {
				addToReceivingQueue(std::move(p));
			}
		}
	}
}

int TurnSocket::send(std::unique_ptr<Packet> packet) {
	int error;

	if (mSsl) {
		error = mSsl->write(packet->data(), packet->length());
	} else {
		error = ::send(mSocket, (const char *)packet->data(), packet->length(), 0);
	}

	if (error <= 0) {
		if (getSocketErrorCode() != TURN_EWOULDBLOCK) {
			if (mSsl) {
				switch (error) {
				case BCTBX_ERROR_NET_CONN_RESET:
					ms_warning("TurnSocket [%p]: server disconnected us", this);
					break;
				default:
					ms_error("TurnSocket [%p]: SSL error while sending: %i", this, error);
					break;
				}
			} else {
				if (error == -1)
					ms_error("TurnSocket [%p]: fail to send: %s", this, getSocketError());
				else {
					ms_warning("TurnSocket [%p]: server disconnected us", this);
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
		mSendRunning = true;
		if (!mSendingQueue.empty()) {
			auto p = std::move(mSendingQueue.front());
			mSendingQueue.pop();
			lk.unlock();

			uint64_t lPacketAge = ms_get_cur_time_ms() - p->timestamp();
			if (!purging && (lPacketAge > flowControlMaxTime || mError)) {
				if (mError) {
					ms_warning("TurnSocket [%p]: purging queue on send error", this);
				} else {
					ms_warning("TurnSocket [%p]: purging queue packet age [%llu]", this,
							   (unsigned long long)lPacketAge);
				}
				purging = true;
			}

			if (!purging && mReady) {
				mSslLock.lock();
				int error = send(std::move(p));
				mSslLock.unlock();

				if (error == -TURN_EWOULDBLOCK) {
					continue; /*will retry */
				} else if (error < 0) {
					mError = true;
				}
			}
		} else {
			purging = false;
			mSendRunning = false;
			if (mRunning) {
				mQueueCond.wait(lk);
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
			}
		}
	}
}

// -------------------------------------------------------------------------------------------------------

TurnClient::TurnClient(MSTurnContext *context, bool useSsl, std::string rootCertificate)
	: mContext(context), mUseSsl(useSsl), mRootCertificatePath(rootCertificate) {
	mTurnServerCn = std::string(context->cn);

	char ip[64] = {0};
	bctbx_sockaddr_to_ip_address((struct sockaddr *)&context->turn_server_addr, context->turn_server_addrlen, ip,
								 sizeof(ip), &mTurnServerPort);
	mTurnServerIp = std::string(ip);

	mTurnConnection = nullptr;

	mRng = bctbx_rng_context_new();
}

TurnClient::~TurnClient() {
	if (mRng)
		bctbx_rng_context_free(mRng);
}

void TurnClient::connect() {
	if (mTurnConnection == nullptr) {
		mTurnConnection = std::make_unique<TurnSocket>(this, mTurnServerPort);
		mTurnConnection->start();
	}
}

int TurnClient::recvfrom(mblk_t *msg, int flags, struct sockaddr *from, socklen_t *fromlen) {
	std::unique_ptr<Packet> p = nullptr;

	mTurnConnection->mReceivingLock.lock();
	if (!mTurnConnection->mReceivingQueue.empty()) {
		p = std::move(mTurnConnection->mReceivingQueue.front());
		mTurnConnection->mReceivingQueue.pop();
	}
	mTurnConnection->mReceivingLock.unlock();

	if (p != nullptr) {
		memcpy(msg->b_rptr, p->data(), p->length());

		// Set from and fromlen to the turn server address
		memcpy(from, (struct sockaddr *)&mContext->turn_server_addr, mContext->turn_server_addrlen);
		*fromlen = mContext->turn_server_addrlen;

		// Set the net_addr for the modifiers
		memcpy(&msg->net_addr, from, *fromlen);
		msg->net_addrlen = *fromlen;

		// Set the recv_addr
		struct sockaddr_in addr;
		socklen_t addrlen = sizeof(addr);
		getsockname(mTurnConnection->mSocket, (struct sockaddr *)&addr, &addrlen);
		msg->recv_addr.family = addr.sin_family;
		msg->recv_addr.addr.ipi_addr = addr.sin_addr;
		msg->recv_addr.port = addr.sin_port;

		return p->length();
	}

	return 0;
}

int TurnClient::sendto(mblk_t *msg, int flags, const struct sockaddr *to, socklen_t tolen) {
	auto p = std::make_unique<Packet>(msg);
	p->setTimestampCurrent();

	int length = p->length();

	mTurnConnection->addToSendingQueue(std::move(p));

	return length;
}

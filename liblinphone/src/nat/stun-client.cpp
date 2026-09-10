/*
 * Copyright (c) 2010-2026 Belledonne Communications SARL.
 *
 * This file is part of Liblinphone
 * (see https://gitlab.linphone.org/BC/public/liblinphone).
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

#include <mediastreamer2/stun-message.h>
#include <mediastreamer2/stun-raw-message.h>

#include "logger/logger.h"

#include "c-wrapper/internal/c-tools.h"
#include "stun-client.h"

// =============================================================================

using namespace std;
using namespace ms2::nat;

LINPHONE_BEGIN_NAMESPACE

int StunClient::run(const int audioPort, const int videoPort, const int textPort) {
	stunDiscoveryDone = false;
	if (linphone_core_ipv6_enabled(getCore()->getCCore()) != FALSE) {
		lWarning() << "STUN support is not implemented for ipv6";
		return -1;
	}
	if (linphone_core_get_stun_server(getCore()->getCCore()) == nullptr) {
		return -1;
	}
	const struct addrinfo *ai = linphone_core_get_stun_server_addrinfo(getCore()->getCCore());
	if (ai == nullptr) {
		lError() << "Could not obtain STUN server addrinfo";
		return -1;
	}

	/* Create the RTP sockets and send STUN messages to the STUN server */
	const ortp_socket_t sockAudio = createStunSocket(audioPort);
	if (sockAudio == -1) {
		return -1;
	}
	ortp_socket_t sockVideo = -1;
	if (linphone_core_video_enabled(getCore()->getCCore()) != FALSE) {
		sockVideo = createStunSocket(videoPort);
		if (sockVideo == -1) {
			return -1;
		}
	}
	ortp_socket_t sockText = -1;
	if (linphone_core_realtime_text_enabled(getCore()->getCCore()) != FALSE) {
		sockText = createStunSocket(textPort);
		if (sockText == -1) {
			return -1;
		}
	}

	int ret = 0;
	int loops = 0;
	bool gotAudio = false;
	bool gotVideo = false;
	bool gotText = false;
	bool coneAudio = false;
	bool coneVideo = false;
	bool coneText = false;
	double elapsed;
	struct timeval init{};
	bctbx_gettimeofday(&init, nullptr);

	do {
		int id;
		if ((loops % 20) == 0) {
			lInfo() << "Sending STUN requests...";
			sendStunRequest(sockAudio, ai->ai_addr, static_cast<socklen_t>(ai->ai_addrlen), 11, true);
			sendStunRequest(sockAudio, ai->ai_addr, static_cast<socklen_t>(ai->ai_addrlen), 1, false);
			if (sockVideo != -1) {
				sendStunRequest(sockVideo, ai->ai_addr, static_cast<socklen_t>(ai->ai_addrlen), 22, true);
				sendStunRequest(sockVideo, ai->ai_addr, static_cast<socklen_t>(ai->ai_addrlen), 2, false);
			}
			if (sockText != -1) {
				sendStunRequest(sockText, ai->ai_addr, static_cast<socklen_t>(ai->ai_addrlen), 33, true);
				sendStunRequest(sockText, ai->ai_addr, static_cast<socklen_t>(ai->ai_addrlen), 3, false);
			}
		}
		ms_usleep(10000);

		if (recvStunResponse(sockAudio, audioCandidate, id) > 0) {
			lInfo() << "STUN test result: local audio port maps to " << audioCandidate.address << ":"
			        << audioCandidate.port;
			if (id == 11) {
				coneAudio = true;
			}
			gotAudio = true;
		}
		if (recvStunResponse(sockVideo, videoCandidate, id) > 0) {
			lInfo() << "STUN test result: local video port maps to " << videoCandidate.address << ":"
			        << videoCandidate.port;
			if (id == 22) {
				coneVideo = true;
			}
			gotVideo = true;
		}
		if (recvStunResponse(sockText, textCandidate, id) > 0) {
			lInfo() << "STUN test result: local text port maps to " << textCandidate.address << ":"
			        << textCandidate.port;
			if (id == 33) {
				coneText = true;
			}
			gotText = true;
		}
		struct timeval cur{};
		bctbx_gettimeofday(&cur, nullptr);
		elapsed = (static_cast<double>(cur.tv_sec - init.tv_sec) * 1000) +
		          (static_cast<double>(cur.tv_usec - init.tv_usec) / 1000);
		if (elapsed > 2000.) {
			lInfo() << "STUN responses timeout, going ahead";
			ret = -1;
			break;
		}
		loops++;
	} while (!gotAudio || (!gotVideo && sockVideo != -1) || (!gotText && sockText != -1));

	if (ret == 0) {
		ret = static_cast<int>(elapsed);
	}

	if (!gotAudio) {
		lError() << "No STUN server response for audio port";
	} else if (!coneAudio) {
		lInfo() << "NAT is symmetric for audio port";
	}

	if (sockVideo != -1) {
		if (!gotVideo) {
			lError() << "No STUN server response for video port";
		} else if (!coneVideo) {
			lInfo() << "NAT is symmetric for video port";
		}
	}

	if (sockText != -1) {
		if (!gotText) {
			lError() << "No STUN server response for text port";
		} else if (!coneText) {
			lInfo() << "NAT is symmetric for text port";
		}
	}

	close_socket(sockAudio);
	if (sockVideo != -1) {
		close_socket(sockVideo);
	}
	if (sockText != -1) {
		close_socket(sockText);
	}
	stunDiscoveryDone = true;
	return ret;
}

void StunClient::updateMediaDescription(const std::shared_ptr<SalMediaDescription> &md) const {
	if (!stunDiscoveryDone) {
		return;
	}
	for (auto &stream : md->streams) {
		if (!stream.enabled()) {
			continue;
		}
		if (stream.getType() == SalAudio && audioCandidate.port != 0) {
			stream.rtp_addr = audioCandidate.address;
			stream.rtp_port = audioCandidate.port;
			if ((!audioCandidate.address.empty() && !videoCandidate.address.empty() &&
			     audioCandidate.address == videoCandidate.address) ||
			    md->getNbActiveStreams() == 1) {
				md->addr = audioCandidate.address;
			}
		} else if (stream.type == SalVideo && videoCandidate.port != 0) {
			stream.rtp_addr = videoCandidate.address;
			stream.rtp_port = videoCandidate.port;
		} else if (stream.type == SalText && textCandidate.port != 0) {
			stream.rtp_addr = textCandidate.address;
			stream.rtp_port = textCandidate.port;
		}
	}
}

// -----------------------------------------------------------------------------

ortp_socket_t StunClient::createStunSocket(int localPort) {
	if (localPort < 0) {
		return -1;
	}
	const ortp_socket_t sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
		lError() << "Fail to create socket";
		return -1;
	}
	struct sockaddr_in laddr{};
	laddr.sin_family = AF_INET;
	laddr.sin_addr.s_addr = INADDR_ANY;
	laddr.sin_port = htons(static_cast<uint16_t>(localPort));
	if (::bind(sock, reinterpret_cast<struct sockaddr *>(&laddr), sizeof(laddr)) < 0) {
		lError() << "Bind socket to 0.0.0.0:" << localPort << " failed: " << getSocketError();
		close_socket(sock);
		return -1;
	}
	int optval = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char *>(&optval), sizeof(optval)) < 0) {
		lWarning() << "Fail to set SO_REUSEADDR";
	}
	set_non_blocking_socket(sock);
	return sock;
}

int StunClient::recvStunResponse(const ortp_socket_t sock, Candidate &candidate, int &id) {
	std::array<uint8_t, StunRawMessage::MAX_RAW_MESSAGE_LENGTH> buf{};

	ssize_t len = recv(sock, reinterpret_cast<char *>(buf.data()), static_cast<int>(buf.size()), 0);
	if (len > 0) {
		const auto response = StunMessage::parse(reinterpret_cast<const char *>(buf.data()), static_cast<size_t>(len));
		if (response != nullptr) {
			struct in_addr ia{};
			const auto transactionId = response->getTransactionId();
			id = transactionId.asUInt96().octet[0];
			auto stunAddr = response->getXorMappedAddress();
			if (stunAddr.has_value()) {
				candidate.port = stunAddr.value().getPort();
				ia.s_addr = htonl(stunAddr.value().getIpV4Address().value());
			} else {
				stunAddr = response->getMappedAddress();
				if (stunAddr.has_value()) {
					candidate.port = stunAddr.value().getPort();
					ia.s_addr = htonl(stunAddr.value().getIpV4Address().value());
				} else {
					len = -1;
				}
			}
			if (len > 0) {
				candidate.address = L_C_TO_STRING(inet_ntoa(ia));
			}
		}
	}
	return static_cast<int>(len);
}

int StunClient::sendStunRequest(const ortp_socket_t sock,
                                const struct sockaddr *server,
                                const socklen_t addrlen,
                                const int id,
                                const bool changeAddr) {
	const auto request = StunMessage::createStunBindingRequest();
	auto transactionId = request->getTransactionId().asUInt96();
	transactionId.octet[0] = static_cast<unsigned char>(id);
	request->setTransactionId(StunTransactionId(transactionId));
	request->enableChangeIp(changeAddr);
	request->enableChangePort(changeAddr);

	const auto stunRawMessage = request->encode();
	if (stunRawMessage == nullptr) {
		lError() << "Failed to encode STUN message";
		return -1;
	}
	const auto data = stunRawMessage->getData();
	auto err = static_cast<int>(bctbx_sendto(sock, data.data(), data.size(), 0, server, addrlen));
	if (err < 0) {
		lError() << "sendto failed: " << strerror(errno);
		err = -1;
	}
	return err;
}

LINPHONE_END_NAMESPACE

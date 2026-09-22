/*
 * Copyright (c) 2010-2025 Belledonne Communications SARL.
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

#ifdef _MSC_VER
#define NOMINMAX
#endif

#include "bctoolbox/defs.h"

#include "mediastreamer2/ice-credentials.h"
#include "mediastreamer2/ice-transport-address.h"
#include "mediastreamer2/ice-utils.h"

#include "conference/session/media-session-p.h"
#include "conference/session/streams.h"
#include "ice-service.h"
#include "utils/if-addrs.h"

#if defined(__APPLE__)
#include "TargetConditionals.h"
#endif

using namespace ::std;
using namespace mediastreamer::nat;

LINPHONE_BEGIN_NAMESPACE

IceService::IceService(StreamsGroup &sg) : mStreamsGroup(sg) {
	const LinphoneConfig *config = linphone_core_get_config(getCCore());
	mAllowLateIce = linphone_config_get_int(config, "net", "allow_late_ice", 0) != 0;
	mEnableIntegrityCheck =
	    linphone_config_get_int(config, "net", "ice_session_enable_message_integrity_check", 1) != 0;
	mDontDefaultToStunCandidates = linphone_config_get_int(config, "net", "dont_default_to_stun_candidates", 0) != 0;
}

IceService::~IceService() {
	deleteSession();
}

bool IceService::isActive() const {
	return mIceSession != nullptr;
}

bool IceService::isRunning() const {
	if (!isActive()) {
		return false; // No running because it is not active
	}
	return mIceSession->getState() == IceSession::State::Running;
}

bool IceService::hasCompleted() const {
	if (!isActive()) {
		return true; // Completed because nothing to do.
	}
	return mIceSession->getState() == IceSession::State::Completed;
}

MediaSessionPrivate &IceService::getMediaSessionPrivate() const {
	return mStreamsGroup.getMediaSessionPrivate();
}

bool IceService::iceFoundInMediaDescription(const std::shared_ptr<SalMediaDescription> &md) {
	if ((!md->ice_pwd.empty()) && (!md->ice_ufrag.empty())) {
		return true;
	}
	return std::any_of(md->streams.begin(), md->streams.end(),
	                   [](const auto &stream) { return !stream.getIcePwd().empty() && !stream.getIceUfrag().empty(); });
}

void IceService::checkSession(const IceRole role, const bool preferIpv6DefaultCandidates) {
	const auto natPolicy = getMediaSessionPrivate().getNatPolicy();
	if (!natPolicy || !natPolicy->iceEnabled()) {
		return;
	}

	if (!mIceSession && mIceWasDisabled) {
		/*
		 * No ICE session because it was disabled previously.
		 * Unless allow_late_ice is TRUE, don't re-create the session.
		 */
		if (!mAllowLateIce) {
			return;
		}
	}

	// Already created.
	if (mIceSession) {
		return;
	}

	mIceSession = std::make_shared<IceSession>();
	mIceSession->setDefaultCandidatesPreferIpv6(preferIpv6DefaultCandidates);
	// For backward compatibility purposes, shall be enabled by default in the future.
	mIceSession->enableMessageIntegrityCheck(mEnableIntegrityCheck);
	mIceSession->setRole(role);
}

bool IceService::hasRelayCandidates(const SalMediaDescription &md) {
	for (const auto &stream : md.streams) {
		if (stream.rtp_port == 0) {
			continue;
		}
		if (std::none_of(stream.ice_candidates.begin(), stream.ice_candidates.end(), [](const auto &candidate) {
			    return candidate.type == IceCandidate::getTypeStr(IceCandidate::Type::Relayed);
		    })) {
			return false;
		}
	}
	return true;
}

void IceService::chooseDefaultCandidates(const OfferAnswerContext &ctx) const {
	std::vector<IceCandidate::Type> candidatesTypes;

	if (mDontDefaultToStunCandidates) {
		candidatesTypes.push_back(IceCandidate::Type::Host);
		candidatesTypes.push_back(IceCandidate::Type::Relayed);
	} else {
		/* In the case of an offer from remote, if the offer has relay candidates, prefer STUN as default candidate
		 * so that the TURN relay is used one side only.
		 * Otherwise, prefer the Relay as default candidate since it is supposed to always work.
		 */
		if (!ctx.localIsOfferer && ctx.remoteMediaDescription && hasRelayCandidates(*ctx.remoteMediaDescription)) {
			candidatesTypes.push_back(IceCandidate::Type::ServerReflexive);
			candidatesTypes.push_back(IceCandidate::Type::Relayed);
		} else {
			candidatesTypes.push_back(IceCandidate::Type::Relayed);
			candidatesTypes.push_back(IceCandidate::Type::ServerReflexive);
		}
		candidatesTypes.push_back(IceCandidate::Type::Host);
	}
	mIceSession->setDefaultCandidatesTypes(candidatesTypes);

	mIceSession->chooseDefaultLocalCandidates();
}

void IceService::fillLocalMediaDescription(OfferAnswerContext &ctx) {
	if (!mIceSession) {
		/* fillLocalMediaDescription() is invoked multiple times. If ICE decides to shutdown in between, make sure
		 * everything set previously is cleared.*/
		ctx.localMediaDescription->ice_ufrag.clear();
		ctx.localMediaDescription->ice_pwd.clear();
		for (auto &stream : ctx.localMediaDescription->streams) {
			stream.ice_ufrag.clear();
			stream.ice_pwd.clear();
			stream.ice_candidates.clear();
		}
		return;
	}

	if (mGatheringFinished) {
		if (ctx.remoteMediaDescription) {
			clearUnusedIceCandidates(ctx.localMediaDescription, ctx.remoteMediaDescription, ctx.localIsOfferer);
		}

		mIceSession->computeCandidatesFoundations();
		mIceSession->eliminateRedundantCandidates();
		chooseDefaultCandidates(ctx);
		mGatheringFinished = false;
	}
	updateLocalMediaDescriptionFromIce(ctx.localMediaDescription);
}

void IceService::createStreams(const OfferAnswerContext &params) {
	checkSession(params.localIsOfferer ? IceRole::Controlling : IceRole::Controlled,
	             getMediaSessionPrivate().getAf() == AF_INET6);

	if (!mIceSession) {
		return;
	}

	const auto &streams = mStreamsGroup.getStreams();
	for (const auto &stream : streams) {
		if (!stream) {
			continue;
		}

		size_t index = stream->getIndex();
		params.scopeStreamToIndex(index);

		const auto &streamDesc = params.getLocalStreamDescription();
		bool streamActive = streamDesc.enabled() && (streamDesc.getDirection() != SalStreamInactive);

		/* When rtp bundle is activated or going to be activated, we don't need ICE for the stream.*/
		if (!params.localIsOfferer) {
			int bundleOwnerIndex =
			    params.remoteMediaDescription->getIndexOfTransportOwner(params.getRemoteStreamDescription());
			if (params.localMediaDescription->accept_bundles && bundleOwnerIndex != -1 &&
			    bundleOwnerIndex != static_cast<int>(index)) {
				lInfo() << *stream << " is part of a bundle as secondary stream, ICE not needed.";
				streamActive = false;
			}
		} else {
			auto *i = dynamic_cast<RtpInterface *>(stream.get());
			if (streamDesc.isBundleOnly() || ((i != nullptr) && !i->isTransportOwner())) {
				lInfo() << *stream << " is currently part of a bundle as secondary stream, ICE not needed.";
				streamActive = false;
			}
		}

		auto checklist = mIceSession->getNthCheckList(index);

		if (!checklist && streamActive) {
			checklist = std::make_shared<::mediastreamer::nat::IceCheckList>();
			mIceSession->addCheckList(checklist, index);
			lInfo() << "Created new ICE check list " << checklist << " for stream #" << index;
		} else if (checklist && !streamActive) {
			mIceSession->removeCheckList(index);
			checklist = nullptr;
		}
		stream->setIceCheckList(checklist);
		stream->iceStateChanged();
	}

	if (!params.localIsOfferer) {
		if (params.remoteMediaDescription) {
			// This may delete the ice session.
			updateFromRemoteMediaDescription(params.localMediaDescription, params.remoteMediaDescription, true);
		}
	}
	if (!mIceSession) {
		/* ICE was disabled. */
		mIceWasDisabled = true;
	}
}

bool IceService::needIceGathering() {
	// Start ICE gathering if needed.
	if (!mIceSession->areCandidatesGathered()) {
		mInsideGatherIceCandidates = true;
		int err = gatherIceCandidates();
		mInsideGatherIceCandidates = false;
		if (err == 0) {
			// Ice candidates gathering wasn't started, but we can proceed with the call anyway.
			return false;
		}
		if (err == -1) {
			deleteSession();
			return false;
		}
		return true;
	}
	return false;
}

bool IceService::prepare() {
	if (!mIceSession) {
		return false;
	}

	const auto natPolicy = getMediaSessionPrivate().getNatPolicy();
	if (natPolicy && (natPolicy->turnEnabled() || natPolicy->stunEnabled()) &&
	    natPolicy->needToUpdateTurnConfiguration()) {
		const auto &account = getMediaSessionPrivate().getDestAccount();
		natPolicy->updateTurnConfiguration(account ? account->getAccountParams()->getIdentityAddress() : nullptr,
		                                   [this](BCTBX_UNUSED(bool ignored)) {
			                                   if (!needIceGathering()) {
				                                   notifyEndOfPrepare();
			                                   }
		                                   });
		return true;
	}
	return needIceGathering();
}

LinphoneCore *IceService::getCCore() const {
	return mStreamsGroup.getCCore();
}

int IceService::gatherLocalCandidates() const {
	list<string> localAddrs = IfAddrs::fetchLocalAddresses();
	const bool ipv6Allowed = linphone_core_ipv6_enabled(getCCore()) != FALSE;
	const auto &mediaLocalIp = getMediaSessionPrivate().getMediaLocalIp();
	const auto it = std::find(localAddrs.cbegin(), localAddrs.cend(), mediaLocalIp);
	if (it == localAddrs.cend()) {
		// Add media local IP address if not already in the list in order to always include the default candidate
		localAddrs.push_back(mediaLocalIp);
	}

#if defined(__APPLE__) && TARGET_OS_IPHONE
	if (getPlatformHelpers(getCCore())->getNetworkType() == PlatformHelpers::NetworkType::Wifi &&
	    !hasLocalNetworkPermission(localAddrs))
		return -1;
#endif
	const auto &streams = mStreamsGroup.getStreams();
	for (const auto &stream : streams) {
		if (!stream) {
			continue;
		}
		const size_t index = stream->getIndex();
		auto checklist = mIceSession->getNthCheckList(index);
		if (checklist) {
			if (getMediaSessionPrivate().mandatoryRtpBundleEnabled()) {
				lInfo() << "Rtp bundle is mandatory, rtcp-mux enabled and RTCP candidates skipped.";
				rtp_session_enable_rtcp_mux(checklist->getRtpSession(), TRUE);
			}
			if ((checklist->getState() != ::mediastreamer::nat::IceCheckList::State::Completed) &&
			    !checklist->areCandidatesGathered()) {
				for (const string &addr : localAddrs) {
					const int family = addr.find(':') != string::npos ? AF_INET6 : AF_INET;
					if (family == AF_INET6 && !ipv6Allowed) {
						continue;
					}
					checklist->addLocalCandidate(IceCandidate::Type::Host,
					                             IceTransportAddress(family, addr, stream->getPortConfig().rtpPort),
					                             ComponentId::Rtp, nullptr);
					if (rtp_session_rtcp_mux_enabled(checklist->getRtpSession()) == FALSE) {
						checklist->addLocalCandidate(
						    IceCandidate::Type::Host,
						    IceTransportAddress(family, addr, stream->getPortConfig().rtcpPort), ComponentId::Rtcp,
						    nullptr);
					}
				}
			}
		}
	}
	return 0;
}

void IceService::addPredefinedSflrxCandidates(const std::shared_ptr<NatPolicy> &natPolicy) const {
	if (!natPolicy) {
		return;
	}
	const bool ipv6Allowed = linphone_core_ipv6_enabled(getCCore()) != FALSE;
	const string &ipv4 = natPolicy->getNatV4Address();
	const string &ipv6 = natPolicy->getNatV6Address();
	if (ipv4.empty() && ipv6.empty()) {
		return;
	}
	const auto &streams = mStreamsGroup.getStreams();
	for (const auto &stream : streams) {
		if (!stream) {
			continue;
		}
		const size_t index = stream->getIndex();
		const auto checklist = mIceSession->getNthCheckList(index);
		if (checklist && checklist->getState() != ::mediastreamer::nat::IceCheckList::State::Completed &&
		    !checklist->areCandidatesGathered()) {
			if (!ipv4.empty()) {
				checklist->addLocalCandidate(IceCandidate::Type::ServerReflexive,
				                             IceTransportAddress(AF_INET, ipv4, stream->getPortConfig().rtpPort),
				                             ComponentId::Rtp, nullptr);
			}
			if (!ipv6.empty() && ipv6Allowed) {
				checklist->addLocalCandidate(IceCandidate::Type::ServerReflexive,
				                             IceTransportAddress(AF_INET6, ipv6, stream->getPortConfig().rtpPort),
				                             ComponentId::Rtp, nullptr);
			}
			if (rtp_session_rtcp_mux_enabled(checklist->getRtpSession()) == FALSE) {
				if (!ipv4.empty()) {
					checklist->addLocalCandidate(IceCandidate::Type::ServerReflexive,
					                             IceTransportAddress(AF_INET, ipv4, stream->getPortConfig().rtcpPort),
					                             ComponentId::Rtcp, nullptr);
				}
				if (!ipv6.empty() && ipv6Allowed) {
					checklist->addLocalCandidate(IceCandidate::Type::ServerReflexive,
					                             IceTransportAddress(AF_INET6, ipv6, stream->getPortConfig().rtcpPort),
					                             ComponentId::Rtcp, nullptr);
				}
			}
		}
	}
	mIceSession->setBaseForSrflxCandidates();
	lInfo() << "Configuration-defined server reflexive candidates added to check lists.";
}

int IceService::gatherSflrxIceCandidates(const struct addrinfo *stunServerAi) {
	int err = 0;
	const auto &natPolicy = getMediaSessionPrivate().getNatPolicy();
	if (stunServerAi != nullptr) {
		stunServerAi = getIcePreferredStunServerAddrinfo(stunServerAi);
	} else {
		lWarning() << "Failed to resolve STUN server for ICE gathering, continuing without STUN";
	}
	if (stunServerAi != nullptr) {
		LinphoneCore *core = getCCore();
		const string &server = natPolicy->getStunServer();
		lInfo() << "ICE: gathering candidates from [" << server << "] using "
		        << (natPolicy->turnEnabled() ? "TURN" : "STUN");
		// Gather local srflx candidates.
		if (natPolicy->turnEnabled()) {
			mIceSession->enableTurn(true);

			if (natPolicy->turnTlsEnabled()) {
				mIceSession->setTurnTransport(TurnContext::Transport::Tls);
			} else if (natPolicy->turnTcpEnabled()) {
				mIceSession->setTurnTransport(TurnContext::Transport::Tcp);
			} else {
				mIceSession->setTurnTransport(TurnContext::Transport::Udp);
			}

			mIceSession->setTurnRootCertificatePath(linphone_core_get_root_ca(core));

			char host[NI_MAXHOST];
			int port = 0;
			linphone_parse_host_port(server.c_str(), host, sizeof(host), &port);
			mIceSession->setTurnCn(host);
		}
		mIceSession->setStunAuthListener(&getMediaSessionPrivate());
		err = mIceSession->gatherCandidates(
		          SockAddr(stunServerAi->ai_addr, static_cast<socklen_t>(stunServerAi->ai_addrlen)))
		          ? 1
		          : 0;
	} else {
		lInfo() << "ICE: bypass server-reflexive candidates gathering";
	}
	if (err == 0) {
		gatheringFinished();
	}
	return err;
}

/** Return values:
 *  1: STUN gathering is started
 *  0: no STUN gathering is started, but it's ok to proceed with ICE anyway (with local candidates only or because STUN
 * gathering was already done before) -1: no gathering started and something went wrong with local candidates. There is
 * no way to start the ICE session.
 */
int IceService::gatherIceCandidates() {
	const LinphoneCore *core = getCCore();

	// Gather local host candidates.
	if (gatherLocalCandidates() == -1) {
		lError() << "Local network permission is not granted, ICE must be disabled.";
		return -1;
	}
	mIceSession->enableForcedRelay(core->forced_ice_relay != FALSE);
	mIceSession->enableShortTurnRefresh(core->short_turn_refresh != FALSE);

	const auto &natPolicy = getMediaSessionPrivate().getNatPolicy();
	addPredefinedSflrxCandidates(natPolicy);
	if (natPolicy && natPolicy->stunServerActivated()) {
		mSflrxGatheringStatus = 1; // Assume gathering is in progress.
		mAsyncStunResolverHandle = natPolicy->getStunServerAddrinfoAsync([this](const struct addrinfo *stunServerAi) {
			mAsyncStunResolverHandle = 0;
			mSflrxGatheringStatus = gatherSflrxIceCandidates(stunServerAi);
			if (mSflrxGatheringStatus != 1 && !mInsideGatherIceCandidates) {
				/* case where ICE gathering is finally not in progress, but prepare() returned true to
				 * indicate an in-progress operation because of the DNS resolution of stun server.*/
				notifyEndOfPrepare();
			}
		});
		return mSflrxGatheringStatus;
	}

	lInfo() << "ICE is used without STUN server";
	gatheringFinished();
	return 0;
}

bool IceService::checkForIceRestartAndSetRemoteCredentials(const std::shared_ptr<SalMediaDescription> &md,
                                                           const bool isOffer) const {
	bool iceRestarted = false;
	if ((md->addr == "0.0.0.0") || (md->addr == "::0")) {
		restartSession(isOffer ? IceRole::Controlled : IceRole::Controlling);
		iceRestarted = true;
	} else {
		for (size_t i = 0; i < md->streams.size(); i++) {
			const auto &stream = md->streams[i];
			const auto checklist = mIceSession->getNthCheckList(i);
			if (checklist && (stream.rtp_addr == "0.0.0.0")) {
				restartSession(isOffer ? IceRole::Controlled : IceRole::Controlling);
				iceRestarted = true;
				break;
			}
		}
	}
	const auto remoteCredentials = IceCredentials(md->ice_ufrag, md->ice_pwd);
	if (mIceSession->getRemoteCredentials() == std::nullopt) {
		if (!md->ice_ufrag.empty() && !md->ice_pwd.empty()) {
			mIceSession->setRemoteCredentials(remoteCredentials);
		}
	} else if (mIceSession->getRemoteCredentials().value() != remoteCredentials) {
		if (!iceRestarted) {
			restartSession(isOffer ? IceRole::Controlled : IceRole::Controlling);
			iceRestarted = true;
		}
		if (!md->ice_ufrag.empty() && !md->ice_pwd.empty()) {
			mIceSession->setRemoteCredentials(remoteCredentials);
		}
	}
	for (size_t i = 0; i < md->streams.size(); i++) {
		const auto &stream = md->streams[i];
		const auto checklist = mIceSession->getNthCheckList(i);
		if (checklist && (!stream.getIcePwd().empty()) && (!stream.getIceUfrag().empty())) {
			const auto remoteCredentials = IceCredentials(stream.getIceUfrag(), stream.getIcePwd());
			if (checklist->getRemoteCredentials() != remoteCredentials) {
				if (!iceRestarted && (checklist->getRemoteCredentials() != std::nullopt)) {
					// Restart only if remote ufrag/paswd was already set.
					restartSession(isOffer ? IceRole::Controlled : IceRole::Controlling);
					iceRestarted = true;
				}
				checklist->setRemoteCredentials(remoteCredentials);
			}
		}
	}
	return iceRestarted;
}

void IceService::getIceDefaultAddrAndPort(const uint16_t componentID,
                                          const std::shared_ptr<SalMediaDescription> &md,
                                          const SalStreamDescription &stream,
                                          std::string &addr,
                                          int &port) {
	if (componentID == 1) {
		addr = stream.rtp_addr;
		port = stream.rtp_port;
	} else if (componentID == 2) {
		addr = stream.rtcp_addr;
		port = stream.rtcp_port;
	} else {
		return;
	}
	if (addr.empty()) {
		addr = md->addr;
	}
}

void IceService::createIceCheckListsAndParseIceAttributes(const std::shared_ptr<SalMediaDescription> &md,
                                                          const bool iceRestarted) const {
	for (size_t i = 0; i < md->streams.size(); i++) {
		const auto &stream = md->streams[i];
		const auto checklist = mIceSession->getNthCheckList(i);
		if (!checklist) {
			continue;
		}
		if (stream.getIceMismatch()) {
			checklist->setState(::mediastreamer::nat::IceCheckList::State::Failed);
			continue;
		}
		if ((stream.rtp_port == 0) || (stream.getDirection() == SalStreamInactive)) {
			mIceSession->removeCheckList(checklist);
			mStreamsGroup.getStream(i)->setIceCheckList(nullptr);
			continue;
		}
		if ((!stream.getIcePwd().empty()) && (!stream.getIceUfrag().empty())) {
			checklist->setRemoteCredentials(
			    mediastreamer::nat::IceCredentials(stream.getIceUfrag(), stream.getIcePwd()));
		}
		for (const auto &candidate : stream.ice_candidates) {
			bool defaultCandidate = false;
			if (candidate.addr[0] == '\0') {
				break;
			}
			const auto optionalComponentId = getComponentIdFromInt(static_cast<uint16_t>(candidate.componentID));
			const auto optionalCandidateType = IceCandidate::getTypeFromStr(candidate.type);
			if (!optionalComponentId.has_value() || !optionalCandidateType.has_value()) {
				continue;
			}
			std::string addr;
			int port = 0;
			getIceDefaultAddrAndPort(static_cast<uint16_t>(candidate.componentID), md, stream, addr, port);
			if (!addr.empty() && (candidate.port == port) && (addr == candidate.addr)) {
				defaultCandidate = true;
			}
			int family = AF_INET;
			if (candidate.addr.find(':') != std::string::npos) {
				family = AF_INET6;
			}
			checklist->addRemoteCandidate(
			    optionalCandidateType.value(), IceTransportAddress(family, candidate.addr, candidate.port),
			    optionalComponentId.value(), candidate.priority, candidate.foundation, defaultCandidate);
		}
		if (!iceRestarted) {
			bool losingPairsAdded = false;
			for (int j = 0; j < static_cast<int>(stream.ice_remote_candidates.size()); j++) {
				const auto &remoteCandidate = stream.getIceRemoteCandidateAtIndex(static_cast<size_t>(j));
				if (remoteCandidate.addr.empty()) {
					break;
				}
				std::string addr;
				int port = 0;
				const int componentID = j + 1;
				getIceDefaultAddrAndPort(static_cast<uint16_t>(componentID), md, stream, addr, port);

				// If we receive a re-invite with remote-candidates, supply these pairs to the ice check list.
				// They might be valid pairs already selected, or losing pairs.

				int remoteFamily = AF_INET;
				if (remoteCandidate.addr.find(':') != std::string::npos) {
					remoteFamily = AF_INET6;
				}
				int family = AF_INET;
				if (addr.find(':') != std::string::npos) {
					family = AF_INET6;
				}
				const auto optionalComponentId = getComponentIdFromInt(static_cast<uint16_t>(j + 1));
				if (optionalComponentId.has_value()) {
					checklist->addLosingPair(
					    optionalComponentId.value(),
					    IceTransportAddress(remoteFamily, remoteCandidate.addr, remoteCandidate.port),
					    IceTransportAddress(family, addr, port));
					losingPairsAdded = true;
				}
			}
			if (losingPairsAdded) {
				checklist->checkCompleted();
			}
		}
	}
}

void IceService::clearUnusedIceCandidates(const std::shared_ptr<SalMediaDescription> &localDesc,
                                          const std::shared_ptr<SalMediaDescription> &remoteDesc,
                                          const bool localIsOfferer) const {
	for (size_t i = 0; i < std::min(remoteDesc->streams.size(), localDesc->streams.size()); i++) {
		const auto checklist = mIceSession->getNthCheckList(i);
		if (!checklist) {
			continue;
		}
		const auto &localStream = localDesc->streams[i];
		const auto &stream = remoteDesc->streams[i];
		if ((stream.getChosenConfiguration().rtcp_mux && localStream.getChosenConfiguration().rtcp_mux) ||
		    (!localIsOfferer && stream.getChosenConfiguration().rtcp_mux &&
		     !stream.getChosenConfiguration().mid.empty() && localDesc->accept_bundles)) {
			/* RTCP candidates must be dropped under these two ORd conditions:
			 * - rtcp_mux is advertised locally and remotely
			 * - when answering to an offer, when rtcp_mux is advertised together with RTP bundle remotely and we accept
			 * RTP bundle (because rtcp-mux is mandatory with bundles)
			 */
			checklist->removeRtcpCandidates();
			rtp_session_enable_rtcp_mux(checklist->getRtpSession(), TRUE);
		}
	}
}

void IceService::updateFromRemoteMediaDescription(const std::shared_ptr<SalMediaDescription> &localDesc,
                                                  const std::shared_ptr<SalMediaDescription> &remoteDesc,
                                                  const bool isOffer) {
	if (!mIceSession) {
		return;
	}

	if (!iceFoundInMediaDescription(remoteDesc)) {
		// Response from remote does not contain mandatory ICE attributes, delete the session.
		deleteSession();
		return;
	}

	// Check for ICE restart and set remote credentials.
	const bool iceRestarted = checkForIceRestartAndSetRemoteCredentials(remoteDesc, isOffer);

	// Create ICE check lists if needed and parse ICE attributes.
	createIceCheckListsAndParseIceAttributes(remoteDesc, iceRestarted);
	for (size_t i = 0; i < mStreamsGroup.getStreams().size(); i++) {
		const auto checklist = mIceSession->getNthCheckList(i);
		if (!checklist) {
			continue;
		}
		if (i < remoteDesc->streams.size()) {
			const auto &remoteDescStream = remoteDesc->streams[i];
			if (remoteDescStream.enabled() && remoteDescStream.getRtpPort() != 0 &&
			    remoteDescStream.getDirection() != SalStreamInactive) {
				/*
				 * rtp_port == 0 is true when it is a secondary stream part of bundle.
				 */
				/* Stream still needs ICE */
				continue;
			}
		}
		/* This stream is unused or no longer needs ICE, remove its check list */
		mIceSession->removeCheckList(i);
		auto *stream = mStreamsGroup.getStream(i);
		stream->setIceCheckList(nullptr);
		stream->iceStateChanged();
	}
	clearUnusedIceCandidates(localDesc, remoteDesc, !isOffer);
	mIceSession->checkMismatch();

	if (mIceSession->getNbCheckLists() == 0) {
		deleteSession();
	}
}

void IceService::updateLocalMediaDescriptionFromIce(std::shared_ptr<SalMediaDescription> &desc) const {
	if (!mIceSession) {
		return;
	}
	std::shared_ptr<IceCandidate> rtpCandidate;
	std::shared_ptr<IceCandidate> rtcpCandidate;
	bool result = false;
	const bool usePerStreamUfragPassword =
	    linphone_config_get_bool(linphone_core_get_config(getCCore()), "sip", "ice_password_ufrag_in_media_description",
	                             FALSE) != FALSE;

	if (mIceSession->getState() == IceSession::State::Completed) {
		std::shared_ptr<::mediastreamer::nat::IceCheckList> firstChecklist = nullptr;
		for (size_t i = 0; i < desc->streams.size(); i++) {
			const auto checklist = mIceSession->getNthCheckList(i);
			if (checklist) {
				firstChecklist = checklist;
				break;
			}
		}
		if (firstChecklist) {
			rtpCandidate = firstChecklist->getSelectedValidLocalCandidateForRtp();
		}
		if (rtpCandidate != nullptr) {
			desc->addr = rtpCandidate->getTransportAddress().getIp();
		} else {
			lWarning() << "If ICE has completed successfully, rtp_candidate should be set!";
			firstChecklist->dumpValidList();
		}
	}

	if (!usePerStreamUfragPassword) {
		desc->ice_ufrag = mIceSession->getLocalCredentials().getUfrag();
		desc->ice_pwd = mIceSession->getLocalCredentials().getPwd();
	}

	for (size_t i = 0; i < desc->streams.size(); i++) {
		auto &stream = desc->streams[i];
		const auto checklist = mIceSession->getNthCheckList(i);
		rtpCandidate = rtcpCandidate = nullptr;
		if (!stream.enabled() || !checklist || (stream.getRtpPort() == 0) ||
		    (stream.getDirection() == SalStreamInactive)) {
			continue;
		}
		if (checklist->getState() == ::mediastreamer::nat::IceCheckList::State::Completed) {
			rtpCandidate = checklist->getSelectedValidLocalCandidateForRtp();
			auto optionalRtcpCandidate = checklist->getSelectedValidLocalCandidateForRtcp();
			result = ((rtpCandidate != nullptr) &&
			          (!optionalRtcpCandidate.has_value() || (*optionalRtcpCandidate != nullptr)));
			if (optionalRtcpCandidate.has_value()) {
				rtcpCandidate = *optionalRtcpCandidate;
			}
			if (!result) {
				lError() << "No selected valid local candidate but check list is completed, this is a bug.";
			}
		} else {
			rtpCandidate = checklist->getDefaultLocalCandidateForRtp();
			auto optionalRtcpCandidate = checklist->getDefaultLocalCandidateForRtcp();
			result = ((rtpCandidate != nullptr) &&
			          (!optionalRtcpCandidate.has_value() || (*optionalRtcpCandidate != nullptr)));
			if (optionalRtcpCandidate.has_value()) {
				rtcpCandidate = *optionalRtcpCandidate;
			}
			if (result) {
				lInfo() << "RTP default candidate is " << rtpCandidate->getTransportAddress().getIp();
			} else {
				lWarning() << "No RTP default candidate.";
			}
		}
		if (result) {
			stream.rtp_addr = rtpCandidate->getTransportAddress().getIp();
			stream.rtp_port = rtpCandidate->getTransportAddress().getPort();
			if (rtcpCandidate) {
				stream.rtcp_addr = rtcpCandidate->getTransportAddress().getIp();
				stream.rtcp_port = rtcpCandidate->getTransportAddress().getPort();
			}
		} else {
			stream.rtp_addr.clear();
			stream.rtcp_addr.clear();
		}

		if (desc->ice_pwd != checklist->getLocalCredentials().getPwd() || usePerStreamUfragPassword) {
			stream.ice_pwd = checklist->getLocalCredentials().getPwd();
		} else {
			stream.ice_pwd.clear();
		}

		if (desc->ice_ufrag != checklist->getLocalCredentials().getUfrag() || usePerStreamUfragPassword) {
			stream.ice_ufrag = checklist->getLocalCredentials().getUfrag();
		} else {
			stream.ice_ufrag.clear();
		}

		stream.ice_mismatch = checklist->isMismatch();
		std::list<std::shared_ptr<IceCandidate>> candidatesToInclude;
		if ((checklist->getState() == ::mediastreamer::nat::IceCheckList::State::Running)) {
			// Include all candidates
			candidatesToInclude = checklist->getLocalCandidates();
		} else if (checklist->getState() == ::mediastreamer::nat::IceCheckList::State::Completed) {
			// Only include the nominated candidates.
			if (rtpCandidate) {
				candidatesToInclude.push_back(rtpCandidate);
			}
			/* In rtcp-mux or bundle mode, the rtcpCandidate returned as the same componentID as the rtpCandidate. It
			 * doesn't need to be included in the offer.*/
			if (rtcpCandidate && (!rtpCandidate || rtcpCandidate->getComponentId() != rtpCandidate->getComponentId())) {
				candidatesToInclude.push_back(rtcpCandidate);
			}
		}
		if (!candidatesToInclude.empty()) {
			stream.ice_candidates.clear();
			for (const auto &iceCandidate : candidatesToInclude) {
				SalIceCandidate salCandidate;
				salCandidate.foundation = iceCandidate->getFoundation();
				salCandidate.componentID = static_cast<unsigned int>(iceCandidate->getComponentId());
				salCandidate.priority = iceCandidate->getPriority();
				salCandidate.type = iceCandidate->getTypeStr();
				salCandidate.addr = iceCandidate->getTransportAddress().getIp();
				salCandidate.port = iceCandidate->getTransportAddress().getPort();
				if (iceCandidate->getBase() && (iceCandidate->getBase() != iceCandidate)) {
					salCandidate.raddr = iceCandidate->getBase()->getTransportAddress().getIp();
					salCandidate.rport = iceCandidate->getBase()->getTransportAddress().getPort();
				}
				stream.ice_candidates.push_back(salCandidate);
			}
		}

		if ((checklist->getState() == ::mediastreamer::nat::IceCheckList::State::Completed) &&
		    (mIceSession->getRole() == IceRole::Controlling)) {
			stream.ice_remote_candidates.clear();
			rtpCandidate = checklist->getSelectedValidRemoteCandidateForRtp();
			auto optionalRtcpCandidate = checklist->getSelectedValidRemoteCandidateForRtcp();
			if (optionalRtcpCandidate.has_value()) {
				rtcpCandidate = *optionalRtcpCandidate;
			}
			if ((rtpCandidate != nullptr) &&
			    (!optionalRtcpCandidate.has_value() || (*optionalRtcpCandidate != nullptr))) {
				SalIceRemoteCandidate rtp_remote_candidate;
				rtp_remote_candidate.addr = rtpCandidate->getTransportAddress().getIp();
				rtp_remote_candidate.port = rtpCandidate->getTransportAddress().getPort();
				stream.ice_remote_candidates.push_back(rtp_remote_candidate);
				if (rtcpCandidate) {
					SalIceRemoteCandidate rtcp_remote_candidate;
					rtcp_remote_candidate.addr = rtcpCandidate->getTransportAddress().getIp();
					rtcp_remote_candidate.port = rtcpCandidate->getTransportAddress().getPort();
					stream.ice_remote_candidates.push_back(rtcp_remote_candidate);
				}
			} else {
				lError() << "IceService: Selected valid remote candidates should be present if the check list is in "
				            "the Completed state. This is a BUG !";
			}
		} else {
			for (auto &ice_remote_candidate : stream.ice_remote_candidates) {
				ice_remote_candidate.addr.clear();
				ice_remote_candidate.port = 0;
			}
		}
	}
}

void IceService::gatheringFinished() {
	if (!mIceSession) {
		return;
	}

	const auto averageRoundTripTime = mIceSession->getAverageGatheringRoundTripTime();
	const int pingTime = (averageRoundTripTime == std::nullopt) ? 0 : static_cast<int>(averageRoundTripTime->count());
	if (pingTime >= 0) {
		/* FIXME: is ping time still useful for the MediaSession ? */
		getMediaSessionPrivate().setPingTime(pingTime);
	}
	mGatheringFinished = true;
}

/**
 * Choose the preferred IP address to use to contact the STUN server from the list of IP addresses
 * the DNS resolution returned. Choose an address according to the following priorities:
 * - IPv4
 * - IPv6 NAT 64
 * - IPv6 V4 mapped
 * - IPv6
 */
const struct addrinfo *IceService::getIcePreferredStunServerAddrinfo(const struct addrinfo *ai) {
	const struct addrinfo *ipv4 = nullptr;
	const struct addrinfo *ipv6 = nullptr;
	const struct addrinfo *ipv6_nat64 = nullptr;
	const struct addrinfo *ipv6_v4_mapped = nullptr;

	for (const struct addrinfo *it = ai; it != nullptr; it = it->ai_next) {
		char ip_port[128] = {0};
		bctbx_addrinfo_to_printable_ip_address(it, ip_port, sizeof(ip_port) - 1);
		if (it->ai_family == AF_INET) {
			if (ipv4 == nullptr) {
				ipv4 = it;
			}
		} else if (bctbx_sockaddr_is_nat64(it->ai_addr) != FALSE) {
			if (ipv6_nat64 == nullptr) {
				ipv6_nat64 = it;
			}
		} else if (bctbx_sockaddr_is_v4_mapped(it->ai_addr) != FALSE) {
			if (ipv6_v4_mapped == nullptr) {
				ipv6_v4_mapped = it;
			}
		} else if (it->ai_family == AF_INET6) {
			if (ipv6 == nullptr) {
				ipv6 = it;
			}
		}
	}

	if (ipv4 != nullptr) {
		return ipv4;
	}
	if (ipv6_nat64 != nullptr) {
		return ipv6_nat64;
	}
	if (ipv6_v4_mapped != nullptr) {
		return ipv6_v4_mapped;
	}
	if (ipv6 != nullptr) {
		return ipv6;
	}
	return nullptr;
}

void IceService::finishPrepare() {
	if (!mIceSession) {
		return;
	}
	const auto natPolicy = getMediaSessionPrivate().getNatPolicy();
	if (natPolicy) {
		natPolicy->cancelTurnConfigurationUpdate();
	}
	gatheringFinished();
}

void IceService::render(const OfferAnswerContext &ctx, BCTBX_UNUSED(CallSession::State state)) {
	if (!mIceSession) {
		return;
	}

	updateFromRemoteMediaDescription(ctx.localMediaDescription, ctx.remoteMediaDescription, !ctx.localIsOfferer);
	if (mIceSession && mIceSession->getState() != IceSession::State::Completed) {
		mIceSession->startConnectivityChecks();
	}

	if (!mIceSession) {
		/* ICE was disabled. */
		mIceWasDisabled = true;
	}
}

void IceService::sessionConfirmed(BCTBX_UNUSED(const OfferAnswerContext &ctx)) {
}

void IceService::stop() {
	// Nothing to do. The ice session can survive.
}

void IceService::finish() {
	deleteSession();
}

void IceService::deleteSession() {
	if (!mIceSession) {
		return;
	}
	if (mAsyncStunResolverHandle != 0) {
		const auto natPolicy = getMediaSessionPrivate().getNatPolicy();
		if (natPolicy) {
			natPolicy->cancelAsync(mAsyncStunResolverHandle);
		}
		mAsyncStunResolverHandle = 0;
	}
	/* clear all check lists */
	for (const auto &stream : mStreamsGroup.getStreams()) {
		if (stream) {
			stream->setIceCheckList(nullptr);
		}
	}
	mIceSession = nullptr;
}

void IceService::setListener(IceServiceListener *listener) {
	mListener = listener;
}

void IceService::restartSession(const IceRole role) const {
	if (!mIceSession) {
		return;
	}
	/* We use ice_session_reset(), which is similar to ice_session_restart() but it also clears local candidates.
	 * Indeed, the local candidates are always added back after restart.
	 * This avoids previously discovered and possibly non-working peer-reflexive candidates to be accumulated after
	 * successive restarts.
	 */
	mIceSession->reset(role);
}

void IceService::resetSession() const {
	if (!mIceSession) {
		return;
	}
	mIceSession->reset(IceRole::Controlling);
}

bool IceService::hasCompletedCheckList() const {
	if (!mIceSession) {
		return false;
	}
	switch (mIceSession->getState()) {
		case IceSession::State::Completed:
		case IceSession::State::Failed:
			return mIceSession->hasCompletedCheckList();
		default:
			return false;
	}
}

void IceService::notifyEndOfPrepare() {
	mStreamsGroup.finishPrepare();
	if (mListener != nullptr) {
		mListener->onGatheringFinished(*this);
	}
}

void IceService::handleIceEvent(const OrtpEvent *ev) {
	const OrtpEventType evt = ortp_event_get_type(ev);
	const OrtpEventData *evd = ortp_event_get_data(const_cast<OrtpEvent *>(ev));
	switch (evt) {
		case ORTP_EVENT_ICE_SESSION_PROCESSING_FINISHED:
			if (hasCompletedCheckList()) {
				if (mListener != nullptr) {
					mListener->onIceCompleted(*this);
				}
			}
			break;
		case ORTP_EVENT_ICE_GATHERING_FINISHED:
			if (evd->info.ice_processing_successful == FALSE) {
				lWarning() << "No STUN answer from [" << getMediaSessionPrivate().getNatPolicy()->getStunServer()
				           << "], continuing without STUN";
			}
			notifyEndOfPrepare();
			break;
		case ORTP_EVENT_ICE_LOSING_PAIRS_COMPLETED:
			if (mListener != nullptr) {
				mListener->onLosingPairsCompleted(*this);
			}
			break;
		case ORTP_EVENT_ICE_RESTART_NEEDED:
			if (mListener != nullptr) {
				mListener->onIceRestartNeeded(*this);
			}
			break;
		case ORTP_EVENT_ICE_CHECK_LIST_PROCESSING_FINISHED:
		case ORTP_EVENT_ICE_CHECK_LIST_DEFAULT_CANDIDATE_VERIFIED:
			break;
		default:
			lError() << "IceService::handleIceEvent() is passed with a non-ICE event.";
			break;
	}
	/* Notify all the streams of the ICE state change, so that they can update their stats and so on. */
	for (const auto &stream : mStreamsGroup.getStreams()) {
		if (stream) {
			stream->iceStateChanged();
		}
	}
}

bool IceService::isControlling() const {
	if (!mIceSession) {
		return false;
	}
	return mIceSession->getRole() == IceRole::Controlling;
}

bool IceService::reinviteNeedsDeferedResponse(const std::shared_ptr<SalMediaDescription> &remoteMd) const {
	if (!mIceSession || (mIceSession->getState() != IceSession::State::Running)) {
		return false;
	}

	for (size_t i = 0; i < remoteMd->streams.size(); i++) {
		const auto &stream = remoteMd->streams[i];
		const auto checklist = mIceSession->getNthCheckList(i);
		if (!checklist) {
			continue;
		}

		if (stream.getIceMismatch()) {
			return false;
		}
		if ((stream.rtp_port == 0) || (checklist->getState() != ::mediastreamer::nat::IceCheckList::State::Running)) {
			continue;
		}

		for (const auto &ice_remote_candidate : stream.ice_remote_candidates) {
			if (!ice_remote_candidate.addr.empty()) {
				return true;
			}
		}
	}
	return false;
}

bool IceService::hasLocalNetworkPermission() {
	return hasLocalNetworkPermission(IfAddrs::fetchLocalAddresses());
}

bool IceService::checkLocalNetworkPermission(const string &localAddr) {
	struct addrinfo *res = nullptr;
	struct addrinfo hints = {0};
	auto sock = static_cast<ortp_socket_t>(-1);
	struct sockaddr_storage selfAddr{};
	socklen_t selfAddrLen = sizeof(selfAddr);
	static constexpr int timeout = 200; // ms
	const string message("coucou");
	uint64_t begin;
	bool result = false;

	lInfo() << "Checking local network permission with address " << localAddr;

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_NUMERICHOST;

	ssize_t error = bctbx_getaddrinfo(localAddr.c_str(), "0", &hints, &res);
	if (error != 0) {
		lError() << "bctbx_getaddrinfo() failed with error [" << gai_strerror(static_cast<int>(error))
		         << "], unable to check local network permission.";
		goto end;
	}
	sock = bctbx_socket(res->ai_family, res->ai_socktype, IPPROTO_UDP);
	if (sock == static_cast<ortp_socket_t>(-1)) {
		lError() << "Socket creation failed: " << getSocketError();
		goto end;
	}
	bctbx_socket_set_non_blocking(sock);
	error = bctbx_bind(sock, res->ai_addr, static_cast<socklen_t>(res->ai_addrlen));
	if (error == -1) {
		lError() << "Cannot bind socket:" << getSocketError();
		goto end;
	}
	error = bctbx_getsockname(sock, reinterpret_cast<struct sockaddr *>(&selfAddr), &selfAddrLen);
	if (error == -1) {
		lError() << "getsockname() failed:" << getSocketError();
		goto end;
	}

	begin = ms_get_cur_time_ms();
	do {
		uint8_t buffer[128];
		struct sockaddr_storage ss{};
		socklen_t slen = sizeof(ss);

		error = bctbx_sendto(sock, message.c_str(), message.size(), 0, reinterpret_cast<struct sockaddr *>(&selfAddr),
		                     selfAddrLen);
		if (error == -1) {
			lError() << "Cannot sendto():" << getSocketError();
			goto end;
		}
		ms_usleep(1000);
		error = bctbx_recvfrom(sock, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr *>(&ss), &slen);
		if (error > 0) {
			result = true;
			break;
		}
		if (error == -1 && (getSocketErrorCode() != BCTBX_EWOULDBLOCK) && (getSocketErrorCode() != EAGAIN)) {
			lError() << "recvfrom() failed: " << getSocketError();
			break;
		}

	} while (ms_get_cur_time_ms() - begin < timeout);
end:
	if (sock != 1) {
		bctbx_socket_close(sock);
	}
	if (res != nullptr) {
		bctbx_freeaddrinfo(res);
	}
	return result;
}

/*
 * The local network permission check is done by simply sending a packet to itself.
 */
bool IceService::hasLocalNetworkPermission(const std::list<std::string> &localAddrs) {
	string localAddr4;
	string localAddr6;

	if (localAddrs.empty()) {
		lError() << "Cannot check the local network permission because the local network addresses are unknown.";
		return false;
	}
	/* Select the first IPv4 and IPv6 addresses */
	for (const auto &addr : localAddrs) {
		if (addr.find(':') == string::npos && localAddr4.empty()) {
			/* not an IPv6 address */
			localAddr4 = addr;
		} else if (addr.find(':') != string::npos && localAddr6.empty()) {
			localAddr6 = addr;
		}
	}
	if (checkLocalNetworkPermission(localAddr4)) {
		lInfo() << "Local network permission is apparently granted (checked with " << localAddr4 << " )";
		return true;
	}
	if (checkLocalNetworkPermission(localAddr6)) {
		lInfo() << "Local network permission is apparently granted (checked with " << localAddr4 << " )";
		return true;
	}
	lInfo() << "Local network permission seems not granted.";
	return false;
}

LINPHONE_END_NAMESPACE

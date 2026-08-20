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

#include <algorithm>
#include <chrono>

#include "mediastreamer2/ice-checklist.h"

#include "mediastreamer2/ice-candidate-pair.h"
#include "mediastreamer2/ice-constants.h"
#include "mediastreamer2/ice-session.h"
#include "mediastreamer2/ice-utils.h"

namespace ms2 {

IceCheckList::~IceCheckList() {
	destroyTurnContexts();
}

std::shared_ptr<IceCandidate> IceCheckList::addLocalCandidate(const IceCandidate::Type type,
                                                              const IceTransportAddress &transportAddress,
                                                              const uint16_t componentId,
                                                              const std::shared_ptr<IceCandidate> &base) {
	if (mLocalCandidates.size() >= ICE_MAX_NB_CANDIDATES) {
		ms_error("ice: Candidate list limited to %d candidates", ICE_MAX_NB_CANDIDATES);
		return nullptr;
	}

	auto candidate = IceCandidate::create(type, transportAddress, componentId);
	if (candidate->getBase() == nullptr) {
		candidate->setBase(base);
	}

	const auto it =
	    std::find_if(mLocalCandidates.begin(), mLocalCandidates.end(), [candidate](const auto &localCandidate) {
		    return (candidate->getType() == localCandidate->getType()) &&
		           (candidate->getComponentId() == localCandidate->getComponentId()) &&
		           (candidate->getPriority() == localCandidate->getPriority()) &&
		           (candidate->getTransportAddress() == localCandidate->getTransportAddress());
	    });
	if (it != mLocalCandidates.end()) {
		// This candidate is already in the list, do not add it again
		return nullptr;
	}

	mLocalComponentsIds.insert(candidate->getComponentId());
	mLocalCandidates.push_back(candidate);

	return candidate;
}

void IceCheckList::addLosingPair(const uint16_t componentId,
                                 const IceTransportAddress &localTransportAddress,
                                 const IceTransportAddress &remoteTransportAddress) {
	bool addedMissingRelayCandidate = false;
	std::shared_ptr<IceCandidate> localCandidate = nullptr;
	std::shared_ptr<IceCandidate> remoteCandidate = nullptr;
	std::shared_ptr<IceCandidatePair> candidatePair = nullptr;

	// Search for the local candidate that matches componentId, localAddress, and localPort as they are provided in the
	// received remote-candidate attribute.
	const auto itLocalCandidate = std::find_if(mLocalCandidates.begin(), mLocalCandidates.end(),
	                                           [componentId, localTransportAddress](const auto &candidate) {
		                                           return (candidate->getComponentId() == componentId) &&
		                                                  (candidate->getTransportAddress() == localTransportAddress);
	                                           });
	if (itLocalCandidate == mLocalCandidates.end()) {
		// Workaround to detect if the local candidate that has not been found has been added by the proxy server.
		// If that is the case, add it to the local candidates now.
		const auto itRemoteCandidate = std::find_if(
		    mRemoteCandidates.begin(), mRemoteCandidates.end(), [localTransportAddress](const auto &candidate) {
			    return candidate->getTransportAddress().getIp() == localTransportAddress.getIp();
		    });
		if (itRemoteCandidate != mRemoteCandidates.end()) {
			const auto itSrflxCandidate =
			    std::find_if(mRemoteCandidates.begin(), mRemoteCandidates.end(), [componentId](const auto &candidate) {
				    return (candidate->getComponentId() == componentId) &&
				           (candidate->getType() == IceCandidate::Type::ServerReflexive);
			    });
			if (itSrflxCandidate == mRemoteCandidates.end()) {
				ms_warning("ice: Local candidate %s should have been found", localTransportAddress.asString().c_str());
				return;
			}
			ms_message("ice: Add missing local candidate %s:relay", localTransportAddress.asString().c_str());
			addedMissingRelayCandidate = true;
			localCandidate =
			    addLocalCandidate(IceCandidate::Type::Relayed, localTransportAddress, componentId, *itSrflxCandidate);
			computeCandidateFoundation(localCandidate);
		}
	} else {
		localCandidate = *itLocalCandidate;
	}

	const auto itRemoteCandidate = std::find_if(mRemoteCandidates.begin(), mRemoteCandidates.end(),
	                                            [componentId, remoteTransportAddress](const auto &candidate) {
		                                            return (candidate->getComponentId() == componentId) &&
		                                                   (candidate->getTransportAddress() == remoteTransportAddress);
	                                            });
	if (itRemoteCandidate == mRemoteCandidates.end()) {
		ms_warning("ice: Remote candidate %s should have been found", remoteTransportAddress.asString().c_str());
		return;
	}
	remoteCandidate = *itRemoteCandidate;

	if (addedMissingRelayCandidate) {
		// If we just added a missing relay candidate, also add the candidate pair.
		candidatePair = std::shared_ptr<IceCandidatePair>(
		    new IceCandidatePair(localCandidate, remoteCandidate, mSession->getRole()));
		mPairs.push_back(candidatePair);
	}

	const auto itCandidatePair =
	    std::find_if(mPairs.begin(), mPairs.end(), [localCandidate, remoteCandidate](const auto &pair) {
		    return (pair->getLocalCandidate() == localCandidate) && (pair->getRemoteCandidate() == remoteCandidate);
	    });
	if (itCandidatePair == mPairs.end()) {
		if (addedMissingRelayCandidate) {
			return;
		}
		// Candidate pair has not been created but the candidates exist. It must be that the local candidate is a
		// reflexive or relayed candidate. Therefore create this pair and use it.
		candidatePair = std::shared_ptr<IceCandidatePair>(
		    new IceCandidatePair(localCandidate, remoteCandidate, mSession->getRole()));
		mPairs.push_back(candidatePair);
	} else {
		candidatePair = *itCandidatePair;
	}

	const auto itValidCandidatePair =
	    std::find_if(mValidList.begin(), mValidList.end(), [candidatePair](const auto &validCandidatePair) {
		    return (validCandidatePair->getValid()->getLocalCandidate()->getComponentId() ==
		            candidatePair->getLocalCandidate()->getComponentId()) &&
		           (validCandidatePair->getValid()->getRemoteCandidate()->getComponentId() ==
		            candidatePair->getRemoteCandidate()->getComponentId()) &&
		           (validCandidatePair->getValid()->getLocalCandidate()->getTransportAddress() ==
		            candidatePair->getLocalCandidate()->getTransportAddress()) &&
		           (validCandidatePair->getValid()->getRemoteCandidate()->getTransportAddress() ==
		            candidatePair->getRemoteCandidate()->getTransportAddress());
	    });
	if (itValidCandidatePair == mValidList.end()) {
		// The pair has not been found in the valid list, therefore it is a losing pair.
		const auto losingRemoteCandidate = candidatePair->getRemoteCandidate();
		bool failedCandidates = false;
		bool inProgressCandidates = false;
		for (const auto &pair : mCheckList) {
			if (pair->getRemoteCandidate() == losingRemoteCandidate) {
				if (pair->getState() == IceCandidatePair::State::InProgress) {
					inProgressCandidates = true;
				} else if (pair->getState() == IceCandidatePair::State::Failed) {
					failedCandidates = true;
				}
			}
		}

		if (!inProgressCandidates && failedCandidates) {
			// A network failure, such as a network partition or serious packet loss has most likely occurred, restart
			// ICE after some delay.
			ms_warning("ice: ICE restart is needed!");
			mSession->programEventSending(ORTP_EVENT_ICE_RESTART_NEEDED, std::chrono::milliseconds(1000));
		} else if (inProgressCandidates) {
			// Wait for the in progress checks to complete.
			ms_message("ice: Added losing pair, wait for InProgress checks to complete");
			if (std::find(mLosingPairs.begin(), mLosingPairs.end(), candidatePair) == mLosingPairs.end()) {
				mLosingPairs.push_back(candidatePair);
			}
		}
	} else {
		setSelectedValidCandidatePair(*itValidCandidatePair);
		ms_message("ice: Select losing valid pair: cl=%p, componentID=%u, local_addr=%s, local_port=%d, "
		           "remote_addr=%s, remote_port=%d",
		           this, componentId, localTransportAddress.getIp().c_str(), localTransportAddress.getPort(),
		           remoteTransportAddress.getIp().c_str(), remoteTransportAddress.getPort());
	}
}

std::shared_ptr<IceCandidate> IceCheckList::addRemoteCandidate(const IceCandidate::Type type,
                                                               const IceTransportAddress &transportAddress,
                                                               const uint16_t componentId,
                                                               const uint32_t priority,
                                                               const std::string &foundation,
                                                               const bool isDefault) {
	if (mRemoteCandidates.size() >= ICE_MAX_NB_CANDIDATES) {
		ms_error("ice: Candidate list limited to %d candidates", ICE_MAX_NB_CANDIDATES);
		return nullptr;
	}

	auto candidate = IceCandidate::create(type, transportAddress, componentId);
	// If the priority is 0, compute it. It is used for debugging purpose in mediastream to set priorities of remote
	// candidates.
	if (priority != 0) {
		candidate->setPriority(priority);
	}

	const auto it =
	    std::find_if(mRemoteCandidates.begin(), mRemoteCandidates.end(), [candidate](const auto &remoteCandidate) {
		    return (candidate->getType() == remoteCandidate->getType()) &&
		           (candidate->getComponentId() == remoteCandidate->getComponentId()) &&
		           (candidate->getPriority() == remoteCandidate->getPriority()) &&
		           (candidate->getTransportAddress() == remoteCandidate->getTransportAddress());
	    });
	if (it != mRemoteCandidates.end()) {
		// This candidate is already in the list, do not add it again
		return nullptr;
	}

	candidate->setFoundation(foundation);
	candidate->setDefault(isDefault);

	mRemoteComponentIds.insert(componentId);
	mRemoteCandidates.push_back(candidate);

	return candidate;
}

void IceCheckList::checkCompleted() {
	if (mState == State::Completed) {
		return;
	}

	for (const auto &componentId : mLocalComponentsIds) {
		const auto &validCandidatePair = getSelectedValidCandidatePair(componentId);
		if (validCandidatePair == nullptr) {
			// This component ID is not present in the valid list, no need to look further
			return;
		}
	}

	setState(State::Completed);
}

void IceCheckList::dumpCandidatePairs() const {
	unsigned int index = 1;
	ms_message("Candidate pairs:");
	for (const auto &pair : mPairs) {
		pair->dump(index++);
	}
}

void IceCheckList::dumpCandidatePairsFoundations() const {
	ms_message("Candidate pairs foundations:");
	for_each(mFoundations.begin(), mFoundations.end(), [](const auto &foundation) { foundation.dump(); });
}

void IceCheckList::dumpCandidates() const {
	ms_message("Local candidates:");
	for_each(mLocalCandidates.begin(), mLocalCandidates.end(), [](const auto &candidate) { candidate->dump("\t"); });
	ms_message("Remote candidates:");
	for_each(mRemoteCandidates.begin(), mRemoteCandidates.end(), [](const auto &candidate) { candidate->dump("\t"); });
}

void IceCheckList::dumpCheckList() const {
	unsigned int index = 1;
	ms_message("Check list:");
	for (const auto &pair : mCheckList) {
		pair->dump(index++);
	}
}

void IceCheckList::dumpComponentIds() const {
	ms_message("Component IDs:");
	for (const auto &componentId : mLocalComponentsIds) {
		ms_message("\t%u", componentId);
	}
}

void IceCheckList::dumpTriggeredChecksQueue() const {
	int index = 1;
	ms_message("Triggered checks queue:");
	for (const auto &pair : mTriggeredChecksQueue) {
		pair->dump(index++);
	}
}

void IceCheckList::dumpValidList() const {
	int index = 1;
	ms_message("Valid list:");
	for (const auto &validPair : mValidList) {
		validPair->dump(index++);
	}
}

std::optional<std::shared_ptr<IceCandidate>> IceCheckList::getDefaultLocalCandidateForRtcp() const {
	if (!hasLocalComponentId(ICE_RTCP_COMPONENT_ID)) {
		return std::nullopt;
	}

	const auto it = std::find_if(mLocalCandidates.begin(), mLocalCandidates.end(), [](const auto &candidate) {
		return (candidate->getComponentId() == ICE_RTCP_COMPONENT_ID) && candidate->isDefault();
	});
	if (it == mLocalCandidates.end()) {
		return nullptr;
	}
	return *it;
}

std::shared_ptr<IceCandidate> IceCheckList::getDefaultLocalCandidateForRtp() const {
	const auto it = std::find_if(mLocalCandidates.begin(), mLocalCandidates.end(), [](const auto &candidate) {
		return (candidate->getComponentId() == ICE_RTP_COMPONENT_ID) && candidate->isDefault();
	});
	if (it == mLocalCandidates.end()) {
		return nullptr;
	}
	return *it;
}

const IceCredentials &IceCheckList::getLocalCredentials() const {
	return mSession->getLocalCredentials();
}

const std::optional<IceCredentials> &IceCheckList::getRemoteCredentials() const {
	if (mRemoteCredentials.has_value()) {
		return mRemoteCredentials;
	}
	return mSession->getRemoteCredentials();
}

IceCandidate::Type IceCheckList::getSelectedValidCandidateType() const {
	const auto &validCandidatePair = getSelectedValidCandidatePair(ICE_RTP_COMPONENT_ID);
	if ((validCandidatePair == nullptr) || (validCandidatePair->getValid()->getLocalCandidate()->isRelay())) {
		return IceCandidate::Type::Relayed;
	}

	const auto validPair = validCandidatePair->getValid();
	auto type = validPair->getRemoteCandidate()->getType();
	// If the pair is reflexive, check if there is a pair with the same addresses and componentID that is of host type
	// to report host connection instead of reflexive connection. This might happen if the ICE checks discover
	// reflexives candidates before the signaling layer has communicated the host candidates to the other peer.
	if ((type == IceCandidate::Type::ServerReflexive) || (type == IceCandidate::Type::PeerReflexive)) {
		const auto it = std::find_if(mPairs.begin(), mPairs.end(), [validPair](const auto &pair) {
			return (pair->getRemoteCandidate()->getType() == IceCandidate::Type::Host) &&
			       (pair->getLocalCandidate()->getComponentId() == validPair->getLocalCandidate()->getComponentId()) &&
			       (pair->getRemoteCandidate()->getComponentId() ==
			        validPair->getRemoteCandidate()->getComponentId()) &&
			       (pair->getLocalCandidate()->getTransportAddress() ==
			        validPair->getLocalCandidate()->getTransportAddress()) &&
			       (pair->getRemoteCandidate()->getTransportAddress() ==
			        validPair->getRemoteCandidate()->getTransportAddress());
		});
		if (it != mPairs.end()) {
			return IceCandidate::Type::Host;
		}
	}
	return type;
}

std::shared_ptr<IceCandidate> IceCheckList::getSelectedValidLocalBaseCandidateForRtcp() const {
	uint16_t componentId = ICE_RTCP_COMPONENT_ID;
	if (rtp_session_rtcp_mux_enabled(mRtpSession) == TRUE) {
		componentId = ICE_RTP_COMPONENT_ID;
	}

	const auto &validCandidatePair = getSelectedValidCandidatePair(componentId);
	if (validCandidatePair == nullptr) {
		return nullptr;
	}
	std::shared_ptr<IceCandidate> candidate = validCandidatePair->getGeneratedFrom()->getLocalCandidate();
	if (candidate == nullptr) {
		candidate = validCandidatePair->getValid()->getLocalCandidate();
	}
	return candidate;
}

std::shared_ptr<IceCandidate> IceCheckList::getSelectedValidLocalBaseCandidateForRtp() const {
	const auto &validCandidatePair = getSelectedValidCandidatePair(ICE_RTP_COMPONENT_ID);
	if (validCandidatePair == nullptr) {
		return nullptr;
	}
	std::shared_ptr<IceCandidate> candidate = validCandidatePair->getGeneratedFrom()->getLocalCandidate();
	if (candidate == nullptr) {
		candidate = validCandidatePair->getValid()->getLocalCandidate();
	}
	return candidate;
}

std::optional<std::shared_ptr<IceCandidate>> IceCheckList::getSelectedValidLocalCandidateForRtcp() const {
	if (!hasLocalComponentId(ICE_RTCP_COMPONENT_ID)) {
		return std::nullopt;
	}

	const auto &validCandidatePair = getSelectedValidCandidatePair(ICE_RTCP_COMPONENT_ID);
	if (validCandidatePair == nullptr) {
		return nullptr;
	}
	return validCandidatePair->getValid()->getLocalCandidate();
}

std::shared_ptr<IceCandidate> IceCheckList::getSelectedValidLocalCandidateForRtp() const {
	const auto &validCandidatePair = getSelectedValidCandidatePair(ICE_RTP_COMPONENT_ID);
	if (validCandidatePair == nullptr) {
		ms_warning("No selected valid RTP local candidate.");
		return nullptr;
	}
	return validCandidatePair->getValid()->getLocalCandidate();
}

std::optional<std::shared_ptr<IceCandidate>> IceCheckList::getSelectedValidRemoteCandidateForRtcp() const {
	if (rtp_session_rtcp_mux_enabled(mRtpSession) == TRUE) {
		return std::nullopt;
	}

	const auto &validCandidatePair = getSelectedValidCandidatePair(ICE_RTCP_COMPONENT_ID);
	if (validCandidatePair == nullptr) {
		ms_error("Rtcp-mux is not used but there is no selected valid RTCP remote candidate.");
		return nullptr;
	}
	return validCandidatePair->getValid()->getRemoteCandidate();
}

std::shared_ptr<IceCandidate> IceCheckList::getSelectedValidRemoteCandidateForRtp() const {
	const auto &validCandidatePair = getSelectedValidCandidatePair(ICE_RTP_COMPONENT_ID);
	if (validCandidatePair == nullptr) {
		ms_error("There are no selected valid remote candidates for RTP.");
		return nullptr;
	}
	return validCandidatePair->getValid()->getRemoteCandidate();
}

const std::string &IceCheckList::getStateStr() const {
	static const std::array<std::string, 3> stateStrs = {
	    "running",
	    "completed",
	    "failed",
	};
	return stateStrs[static_cast<size_t>(mState)];
}

void IceCheckList::handleStunPacket(RtpSession *rtpSession, const OrtpEventData *eventData) {
	if (mSession == nullptr) {
		return;
	}

	const mblk_t *mp = eventData->packet;
	auto *msg = ms_stun_message_create_from_buffer_parsing(mp->b_rptr, static_cast<ssize_t>(mp->b_wptr - mp->b_rptr));
	if (msg == nullptr) {
		ms_warning("ice: Received an invalid STUN packet");
		return;
	}

	const auto transactionId = ms_stun_message_get_tr_id(msg);
	const auto transactionIdStr = IceUtils::getTransactionIdStr(transactionId);
	const auto recvAddress = IceUtils::SockAddr(&eventData->packet->recv_addr).ipv6toIpv4();
	const auto recvAddressStr = recvAddress.asString();
	const auto sourceAddress = IceUtils::SockAddr(reinterpret_cast<const struct sockaddr *>(&eventData->source_addr),
	                                              eventData->source_addrlen)
	                               .ipv6toIpv4();
	const auto sourceAddressStr = sourceAddress.asString();
	const auto sourceStunAddress = sourceAddress.toStunAddress();

	if (ms_stun_message_is_request(msg) == TRUE) {
		ms_message("ice: Recv binding request: %s <-- %s [%s] (flags:%s)", recvAddressStr.c_str(),
		           sourceAddressStr.c_str(), transactionIdStr.c_str(),
		           (ms_stun_message_use_candidate_enabled(msg) == TRUE) ? "use-candidate" : "none");
		handleReceivedBindingRequest(rtpSession, eventData, msg, sourceStunAddress);
	} else if (ms_stun_message_is_success_response(msg) == TRUE) {
		setTransactionResponseTime(transactionId, eventData->ts);
		switch (ms_stun_message_get_method(msg)) {
			case MS_STUN_METHOD_BINDING:
				ms_message("ice: Recv binding response: %s <-- %s [%s]", recvAddressStr.c_str(),
				           sourceAddressStr.c_str(), transactionIdStr.c_str());
				handleReceivedBindingResponse(rtpSession, eventData, msg, sourceStunAddress);
				break;
			case MS_TURN_METHOD_ALLOCATE:
				ms_message("ice: Recv TURN allocate success response: %s <-- %s [%s]", recvAddressStr.c_str(),
				           sourceAddressStr.c_str(), transactionIdStr.c_str());
				handleReceivedTurnAllocateSuccessResponse(rtpSession, eventData, msg, sourceStunAddress);
				break;
			case MS_TURN_METHOD_CREATE_PERMISSION:
				ms_message("ice: Recv TURN create permission success response: %s <-- %s [%s]", recvAddressStr.c_str(),
				           sourceAddressStr.c_str(), transactionIdStr.c_str());
				handleReceivedTurnCreatePermissionSuccessResponse(eventData, msg);
				break;
			case MS_TURN_METHOD_REFRESH:
				ms_message("ice: Recv TURN refresh success response: %s <-- %s [%s]", recvAddressStr.c_str(),
				           sourceAddressStr.c_str(), transactionIdStr.c_str());
				handleReceivedTurnRefreshSuccessResponse(eventData, msg);
				break;
			case MS_TURN_METHOD_CHANNEL_BIND:
				ms_message("ice: Recv TURN channel bind success response: %s <-- %s [%s]", recvAddressStr.c_str(),
				           sourceAddressStr.c_str(), transactionIdStr.c_str());
				handleReceivedTurnChannelBindSuccessResponse(eventData, msg);
				break;
			default:
				ms_warning("ice: Recv unknown STUN success response: %s <-- %s [%s]", recvAddressStr.c_str(),
				           sourceAddressStr.c_str(), transactionIdStr.c_str());
				break;
		}
	} else if (ms_stun_message_is_error_response(msg) == TRUE) {
		setTransactionResponseTime(transactionId, eventData->ts);
		ms_message("ice: Recv error response: %s <-- %s [%s]", recvAddressStr.c_str(), sourceAddressStr.c_str(),
		           transactionIdStr.c_str());
		handleReceivedErrorResponse(rtpSession, eventData, msg);
	} else if (ms_stun_message_is_indication(msg) == TRUE) {
		ms_message("ice: Recv indication: %s <-- %s [%s]", recvAddressStr.c_str(), sourceAddressStr.c_str(),
		           transactionIdStr.c_str());
	} else {
		ms_warning("ice: STUN message type not handled");
	}

	ms_stun_message_destroy(msg);
}

bool IceCheckList::haveRemoteCredentialsChanged(const IceCredentials &newCredentials) const {
	return (getRemoteCredentials() != newCredentials);
}

void IceCheckList::printRoute(const std::string &message) const {
	if (mState != State::Completed) {
		return;
	}

	std::string localRtpTransportAddress;
	std::string localRtcpTransportAddress;
	std::string remoteRtpTransportAddress;
	std::string remoteRtcpTransportAddress;
	for (const auto &validPair : getValidPairs()) {
		if (validPair->getLocalCandidate()->getComponentId() == ICE_RTP_COMPONENT_ID) {
			localRtpTransportAddress = validPair->getLocalCandidate()->getTransportAddress().asString();
			remoteRtpTransportAddress = validPair->getRemoteCandidate()->getTransportAddress().asString();
		} else if (validPair->getLocalCandidate()->getComponentId() == ICE_RTCP_COMPONENT_ID) {
			localRtcpTransportAddress = validPair->getLocalCandidate()->getTransportAddress().asString();
			remoteRtcpTransportAddress = validPair->getRemoteCandidate()->getTransportAddress().asString();
		}
	}

	ms_message("%s", message.c_str());
	ms_message("\tRTP: %s --> %s", localRtpTransportAddress.c_str(), remoteRtpTransportAddress.c_str());
	ms_message("\tRTCP: %s --> %s", localRtcpTransportAddress.c_str(), remoteRtcpTransportAddress.c_str());
}

// Schedule checks as defined in 5.8.
void IceCheckList::process(RtpSession *rtpSession) {
	if (mSession == nullptr) {
		return;
	}

	auto currentTime = std::chrono::steady_clock::now();

	// Check for gathering timeout
	if (mGatheringCandidates && checkGatheringTimeout(rtpSession, currentTime)) {
		ms_message("ice: Gathering timeout for checklist [%p]", this);
	}

	// Send STUN/TURN server requests (to gather candidates, create/refresh TURN permissions, refresh TURN allocations
	// or bind TURN channels).
	sendStunRequests();

	// Send event if needed.
	if (mSession->shouldSendEvent() && (currentTime >= mSession->getEventTime())) {
		mSession->stopEventSending();
		auto *const event = ortp_event_new(mSession->getEventValue());
		ortp_event_get_data(event)->info.ice_processing_successful = (mState == State::Completed) ? TRUE : FALSE;
		rtp_session_dispatch_event(rtpSession, event);
	}

	if ((mSession->getState() == IceSession::State::Stopped) || (mSession->getState() == IceSession::State::Failed)) {
		return;
	}

	switch (mState) {
		case State::Completed:
			// Handle keepalives when check list has completed - long periods
			if ((currentTime - mKeepAliveTime) >= mSession->getKeepAliveTimeout()) {
				sendKeepAlivePackets(rtpSession);
				mKeepAliveTime = currentTime;
			}

			// Check if some retransmissions are needed.
			retransmitConnectivityChecks(currentTime, rtpSession);
			if ((currentTime - mTaTime) < mSession->getTa()) {
				return;
			}
			mTaTime = currentTime;

			// Send a triggered connectivity check if there is one.
			if (sendTriggeredCheck(rtpSession) != nullptr) {
				return;
			}
			break;
		case State::Running:
			// Handle keepalives when check list is running, sent only on succeeded pair, to keep them alive until we
			// conclude.
			sendKeepAlivePackets(rtpSession);

			// Check nomination delay.
			if (mNominationDelayRunning && ((currentTime - mNominationDelayStartTime) >= ICE_NOMINATION_DELAY)) {
				ms_message("ice: Nomination delay timeout, select the potential relayed candidate anyway");
				concludeProcessing(rtpSession, true);
				if (mSession->getState() == IceSession::State::Completed) {
					return;
				}
			}

			// Check if some retransmissions are needed.
			retransmitConnectivityChecks(currentTime, rtpSession);
			if ((currentTime - mTaTime) < mSession->getTa()) {
				return;
			}
			mTaTime = currentTime;
			// Send a triggered connectivity check if there is one.
			if (sendTriggeredCheck(rtpSession) != nullptr) {
				return;
			}

			// Send ordinary connectivity checks only when the check list is Running and active.
			if (isFrozen()) {
				// Begin processing on this check list.
				computePairsStates();
			} else {
				// Send an ordinary connectivity check for the pair in the Waiting state and with the highest priority
				// if there is one.
				const auto itWaiting =
				    std::find_if(mCheckList.begin(), mCheckList.end(), [](const auto &candidatePair) {
					    return candidatePair->getState() == IceCandidatePair::State::Waiting;
				    });
				if (itWaiting != mCheckList.end()) {
					sendBindingRequest(*itWaiting, rtpSession);
					return;
				}

				// Send an ordinary connectivity check for the pair in the Frozen state and with the highest priority if
				// there is one.
				const auto itFrozen = std::find_if(mCheckList.begin(), mCheckList.end(), [](const auto &candidatePair) {
					return candidatePair->getState() == IceCandidatePair::State::Frozen;
				});
				if (itFrozen != mCheckList.end()) {
					sendBindingRequest(*itFrozen, rtpSession);
					return;
				}

				// Check if there are some retransmissions pending.
				if (std::none_of(mCheckList.begin(), mCheckList.end(),
				                 [](const auto &candidatePair) { return candidatePair->isRetransmissionPending(); })) {
					ms_message("ice: There is no connectivity check left to be sent and no retransmissions pending, "
					           "concluding checklist [%p]",
					           this);
					concludeProcessing(rtpSession, false);
				}
			}

			break;
		case State::Failed:
			// Nothing to be done.
			break;
	}
}

void IceCheckList::removeRtcpCandidates() {
	mLocalComponentsIds.erase(ICE_RTCP_COMPONENT_ID);
	mRemoteComponentIds.erase(ICE_RTCP_COMPONENT_ID);

	// Remove pairs with rtcp component ID
	removeRtcpCandidatePairs();

	for (auto it = mLocalCandidates.begin(); it != mLocalCandidates.end();) {
		if ((*it)->getComponentId() == ICE_RTCP_COMPONENT_ID) {
			it = mLocalCandidates.erase(it);
		} else {
			++it;
		}
	}
	for (auto it = mRemoteCandidates.begin(); it != mRemoteCandidates.end();) {
		if ((*it)->getComponentId() == ICE_RTCP_COMPONENT_ID) {
			it = mRemoteCandidates.erase(it);
		} else {
			++it;
		}
	}
}

void IceCheckList::setState(const State state) {
	if (mState == state) {
		return;
	}

	mState = state;
	if (mSession->findCheckListFromState(State::Running) != nullptr) {
		return;
	}
	if (mSession->findCheckListFromState(State::Failed) != nullptr) {
		// Set the state of the session to Failed if at least one check list is in the Failed state.
		mSession->setState(IceSession::State::Failed);
	} else {
		// All the check lists are in the Completed state, set the state of the session to Completed.
		mSession->setState(IceSession::State::Completed);
	}
}

//------------------------------------------------------------------------------

void IceCheckList::addStunRequest(const std::shared_ptr<IceStunRequest> &request) {
	mStunRequests.push_back(request);
}

bool IceCheckList::checkGatheringTimeout(RtpSession *rtpSession,
                                         const std::chrono::steady_clock::time_point currentTime) const {
	bool timeout = false;
	mSession->forEachValidCheckList([currentTime, &timeout](const auto &checklist) {
		if (checklist->isGatheringCandidates() &&
		    ((currentTime - checklist->getGatheringStartTime()) >= ICE_GATHERING_CANDIDATES_TIMEOUT)) {
			timeout = true;
		}
	});
	if (timeout) {
		mSession->forEachValidCheckList([](const auto &checklist) { checklist->stopGathering(); });
		// Notify the application that the gathering process has timed out.
		auto *event = ortp_event_new(ORTP_EVENT_ICE_GATHERING_FINISHED);
		ortp_event_get_data(event)->info.ice_processing_successful = FALSE;
		rtp_session_dispatch_event(rtpSession, event);
	}
	return timeout;
}

void IceCheckList::checkMismatch() {
	for (const auto componentId : mRemoteComponentIds) {
		const auto it =
		    std::find_if(mRemoteCandidates.begin(), mRemoteCandidates.end(), [componentId](const auto &candidate) {
			    return candidate->isDefault() && (candidate->getComponentId() == componentId);
		    });
		if (it == mRemoteCandidates.end()) {
			ms_error("ICE mismatch for checklist [%p], due to default remote candidate not found for component ID [%i]",
			         this, componentId);
			mMismatch = true;
			mState = State::Failed;
		}
	}
}

// Check that the mandatory attributes of a connectivity check binding request are present.
bool IceCheckList::checkReceivedBindingRequestAttributes(const RtpSession *rtpSession,
                                                         const OrtpEventData *eventData,
                                                         const MSStunMessage *msg,
                                                         const MSStunAddress &remoteStunAddress) {
	if (ms_stun_message_message_integrity_enabled(msg) == FALSE) {
		ms_warning("ice: Received binding request missing MESSAGE-INTEGRITY attribute");
		IceUtils::sendErrorResponse(rtpSession, eventData, msg, remoteStunAddress, MS_STUN_ERROR_CODE_BAD_REQUEST,
		                            "Missing MESSAGE-INTEGRITY attribute");
		return false;
	}
	if (ms_stun_message_get_username(msg) == nullptr) {
		ms_warning("ice: Received binding request missing USERNAME attribute");
		IceUtils::sendErrorResponse(rtpSession, eventData, msg, remoteStunAddress, MS_STUN_ERROR_CODE_BAD_REQUEST,
		                            "Missing USERNAME attribute");
		return false;
	}
	if (ms_stun_message_fingerprint_enabled(msg) == FALSE) {
		ms_warning("ice: Received binding request missing FINGERPRINT attribute");
		IceUtils::sendErrorResponse(rtpSession, eventData, msg, remoteStunAddress, MS_STUN_ERROR_CODE_BAD_REQUEST,
		                            "Missing FINGERPRINT attribute");
		return false;
	}
	if (ms_stun_message_has_priority(msg) == FALSE) {
		ms_warning("ice: Received binding request missing PRIORITY attribute");
		IceUtils::sendErrorResponse(rtpSession, eventData, msg, remoteStunAddress, MS_STUN_ERROR_CODE_BAD_REQUEST,
		                            "Missing PRIORITY attribute");
		return false;
	}
	if ((ms_stun_message_has_ice_controlling(msg) == FALSE) && (ms_stun_message_has_ice_controlled(msg) == FALSE)) {
		ms_warning("ice: Received binding request missing ICE-CONTROLLING or ICE-CONTROLLED attribute");
		IceUtils::sendErrorResponse(rtpSession, eventData, msg, remoteStunAddress, MS_STUN_ERROR_CODE_BAD_REQUEST,
		                            "Missing ICE-CONTROLLING or ICE-CONTROLLED attribute");
		return false;
	}
	return true;
}

bool IceCheckList::checkReceivedBindingRequestIntegrity(const RtpSession *rtpSession,
                                                        const OrtpEventData *eventData,
                                                        const MSStunMessage *msg,
                                                        const MSStunAddress &remoteStunAddress) {
	bool result = true;
	const mblk_t *mp = eventData->packet;

	// Check the message integrity: first remove length of fingerprint...
	char *lenPos = reinterpret_cast<char *>(mp->b_rptr) + sizeof(uint16_t);
	uint16_t newLen = htons(ms_stun_message_get_length(msg) - 8);
	memcpy(lenPos, &newLen, sizeof(uint16_t));
	auto *hmac = ms_stun_calculate_integrity_short_term(reinterpret_cast<char *>(mp->b_rptr),
	                                                    static_cast<size_t>(mp->b_wptr - mp->b_rptr - 24 - 8),
	                                                    mSession->getLocalCredentials().getPwd().c_str());

	// ... and then restore the length with fingerprint.
	newLen = htons(ms_stun_message_get_length(msg));
	memcpy(lenPos, &newLen, sizeof(uint16_t));
	if (strcmp(ms_stun_message_get_message_integrity(msg), hmac) != 0) {
		ms_error("ice: Wrong MESSAGE-INTEGRITY in received binding request");
		if (!mSession->isMessageIntegrityCheckEnabled() &&
		    (ms_stun_message_dummy_message_integrity_enabled(msg) == TRUE)) {
			ms_message("ice: skipping message integrity check for cl [%p]", this);
		} else {
			IceUtils::sendErrorResponse(rtpSession, eventData, msg, remoteStunAddress, MS_STUN_ERROR_CODE_UNAUTHORIZED,
			                            "Wrong MESSAGE-INTEGRITY attribute");
			result = false;
		}
	}
	ms_free(hmac);
	return result;
}

bool IceCheckList::checkReceivedBindingRequestRoleConflict(const RtpSession *rtpSession,
                                                           const OrtpEventData *eventData,
                                                           const MSStunMessage *msg,
                                                           const MSStunAddress &remoteStunAddress) const {
	// Detect and repair role conflicts according to 7.2.1.1.
	if ((mSession->getRole() == IceRole::Controlling) && (ms_stun_message_has_ice_controlling(msg) == TRUE)) {
		ms_warning("ice: Role conflict, both agents are CONTROLLING");
		if (mSession->getTieBreaker() >= ms_stun_message_get_ice_controlling(msg)) {
			IceUtils::sendErrorResponse(rtpSession, eventData, msg, remoteStunAddress, MS_ICE_ERROR_CODE_ROLE_CONFLICT,
			                            "Role Conflict");
			return false;
		}
		ms_message("ice: Switch to the CONTROLLED role");
		mSession->setRole(IceRole::Controlled);
	} else if ((mSession->getRole() == IceRole::Controlled) && (ms_stun_message_has_ice_controlled(msg) == TRUE)) {
		ms_warning("ice: Role conflict, both agents are CONTROLLED");
		if (mSession->getTieBreaker() >= ms_stun_message_get_ice_controlled(msg)) {
			ms_message("ice: Switch to the CONTROLLING role");
			mSession->setRole(IceRole::Controlling);
		} else {
			IceUtils::sendErrorResponse(rtpSession, eventData, msg, remoteStunAddress, MS_ICE_ERROR_CODE_ROLE_CONFLICT,
			                            "Role Conflict");
			return false;
		}
	}
	return true;
}

bool IceCheckList::checkReceivedBindingRequestUsername(const RtpSession *rtpSession,
                                                       const OrtpEventData *eventData,
                                                       const MSStunMessage *msg,
                                                       const MSStunAddress &remoteStunAddress) const {
	const std::string username = ms_stun_message_get_username(msg);
	const auto colonPos = username.find(':');
	if ((colonPos == std::string::npos) ||
	    (username.substr(0, colonPos) != mSession->getLocalCredentials().getUfrag())) {
		ms_error("ice: Wrong USERNAME attribute");
		IceUtils::sendErrorResponse(rtpSession, eventData, msg, remoteStunAddress, MS_STUN_ERROR_CODE_UNAUTHORIZED,
		                            "Wrong USERNAME attribute");
		return false;
	}
	return true;
}

bool IceCheckList::checkReceivedBindingResponseAddresses(const OrtpEventData *eventData,
                                                         const std::shared_ptr<IceCandidatePair> &candidatePair,
                                                         const MSStunAddress &remoteStunAddress) {
	const auto pairRemoteStunAddress = candidatePair->getRemoteCandidate()->getTransportAddress().toStunAddress();
	const auto pairLocalStunAddress = candidatePair->getLocalCandidate()->getTransportAddress().toStunAddress();
	const auto recvStunAddress = IceUtils::SockAddr(&eventData->packet->recv_addr).ipv6toIpv4().toStunAddress();
	if ((ms_compare_stun_addresses(&remoteStunAddress, &pairRemoteStunAddress) != FALSE) ||
	    (ms_compare_stun_addresses(&recvStunAddress, &pairLocalStunAddress) != FALSE)) {
		// Non-symmetric addresses, set the state of the pair to Failed as defined in 7.1.3.1.
		ms_warning("ice: Non symmetric addresses, set state of pair %p to Failed", candidatePair.get());
		candidatePair->setState(IceCandidatePair::State::Failed);
		return false;
	}
	return true;
}

bool IceCheckList::checkReceivedBindingResponseAttributes(const MSStunMessage *msg) const {
	if (ms_stun_message_message_integrity_enabled(msg) != TRUE) {
		ms_warning("ice: Received binding response missing MESSAGE-INTEGRITY attribute");
		if (mSession->isMessageIntegrityCheckEnabled()) {
			return false;
		}
	}
	if (ms_stun_message_fingerprint_enabled(msg) != TRUE) {
		ms_warning("ice: Received binding response missing FINGERPRINT attribute");
		return false;
	}
	if (ms_stun_message_get_xor_mapped_address(msg) == nullptr) {
		ms_warning("ice: Received binding response missing XOR-MAPPED-ADDRESS attribute");
		return false;
	}
	return true;
}

void IceCheckList::chooseDefaultLocalCandidates() const {
	if (mState == State::Running) {
		chooseLocalOrRemoteDefaultCandidates(mLocalCandidates);
	}
}

void IceCheckList::chooseDefaultRemoteCandidates() const {
	chooseLocalOrRemoteDefaultCandidates(mRemoteCandidates);
}

// Choose the default candidate for each componentID as defined in 4.1.4.
void IceCheckList::chooseLocalOrRemoteDefaultCandidates(
    const std::list<std::shared_ptr<IceCandidate>> &candidates) const {
	for (int componentId = MIN_COMPONENT_ID; componentId <= MAX_COMPONENT_ID; componentId++) {
		std::shared_ptr<IceCandidate> candidate = nullptr;

		for (const auto type : mSession->getDefaultCandidatesTypes()) {
			const auto inetCandidate = findCandidate(candidates, type, componentId, AF_INET);
			const auto inet6Candidate = findCandidate(candidates, type, componentId, AF_INET6);
			if ((inetCandidate != nullptr) &&
			    !((inet6Candidate != nullptr) && mSession->getDefaultCandidatesPreferIpv6())) {
				candidate = inetCandidate;
			} else {
				candidate = inet6Candidate;
			}
			if (candidate != nullptr) {
				break;
			}
		}
		if (candidate != nullptr) {
			candidate->setDefault(true);
			if (mSession->isTurnEnabled()) {
				ms_turn_context_set_force_rtp_sending_via_relay(
				    getTurnContextFromComponentId(componentId),
				    (candidate->getType() == IceCandidate::Type::Relayed) ? TRUE : FALSE);
			}
		}
	}
}

void IceCheckList::collectGatheringRoundTripTimes() {
	mRtt = IceStunRequest::RoundTripTime();
	for (const auto &request : mStunRequests) {
		for (const auto &rtt : request->getGatheringRoundTripTimes()) {
			mRtt << rtt;
		}
	}
}

void IceCheckList::computeCandidateFoundation(const std::shared_ptr<IceCandidate> &candidate) {
	const auto it =
	    std::find_if(mLocalCandidates.begin(), mLocalCandidates.end(), [candidate](const auto &otherCandidate) {
		    return (candidate != otherCandidate) && candidate->getBase() && otherCandidate->getBase() &&
		           (candidate->getType() == otherCandidate->getType()) &&
		           (candidate->getBase()->getTransportAddress().getIp() ==
		            otherCandidate->getBase()->getTransportAddress().getIp());
	    });
	if (it != mLocalCandidates.end()) {
		// We found a candidate that should have the same foundation, so copy it from this candidate.
		const auto &otherCandidate = *it;
		if (!otherCandidate->getFoundation().empty()) {
			candidate->setFoundation(otherCandidate->getFoundation());
			return;
		}
		// If the foundation of the other candidate is empty we need to assign a new one, so continue.
	}

	// No candidate that should have the same foundation has been found, assign a new one.
	std::ostringstream oss;
	oss << mFoundationGenerator;
	candidate->setFoundation(oss.str());
	mFoundationGenerator++;
}

void IceCheckList::computeCandidatesFoundations() {
	if (mState != State::Running) {
		return;
	}

	for (const auto &candidate : mLocalCandidates) {
		computeCandidateFoundation(candidate);
	}
}

void IceCheckList::computePairPriorities() {
	std::for_each(mPairs.begin(), mPairs.end(),
	              [this](const auto &pair) { pair->computePriority(mSession->getRole()); });
}

void IceCheckList::computePairsStates() const {
	// Compute pairs states according to 5.7.4.
	for (const auto &foundation : mFoundations) {
		std::optional<uint16_t> componentId = std::nullopt;
		uint64_t priority = 0;
		std::shared_ptr<IceCandidatePair> foundPair = nullptr;
		for (const auto &pair : mCheckList) {
			if ((pair->getLocalCandidate()->getFoundation() == foundation.getLocal()) &&
			    (pair->getRemoteCandidate()->getFoundation() == foundation.getRemote()) &&
			    (!componentId.has_value() ||
			     ((pair->getLocalCandidate()->getComponentId() < *componentId) && (pair->getPriority() > priority)))) {
				componentId = pair->getLocalCandidate()->getComponentId();
				priority = pair->getPriority();
				foundPair = pair;
			}
		}

		if (foundPair != nullptr) {
			foundPair->setState(IceCandidatePair::State::Waiting);
		}
	}
}

void IceCheckList::concludeWaitingFrozenAndInProgressPairs() {
	for (const auto &validCandidatePair : mValidList) {
		if (validCandidatePair->getValid()->isNominated()) {
			// Remove Waiting and Frozen pairs from mCheckList and mTriggeredChecksQueue
			for (auto it = mCheckList.begin(); it != mCheckList.end();) {
				const auto pair = *it;
				if (((pair->getState() == IceCandidatePair::State::Waiting) ||
				     (pair->getState() == IceCandidatePair::State::Frozen)) &&
				    (pair->getLocalCandidate()->getComponentId() ==
				     validCandidatePair->getValid()->getLocalCandidate()->getComponentId())) {
					it = mCheckList.erase(it);
				} else {
					++it;
				}
			}
			for (auto it = mTriggeredChecksQueue.begin(); it != mTriggeredChecksQueue.end();) {
				const auto pair = *it;
				if (((pair->getState() == IceCandidatePair::State::Waiting) ||
				     (pair->getState() == IceCandidatePair::State::Frozen)) &&
				    (pair->getLocalCandidate()->getComponentId() ==
				     validCandidatePair->getValid()->getLocalCandidate()->getComponentId())) {
					it = mTriggeredChecksQueue.erase(it);
				} else {
					++it;
				}
			}

			for (const auto &candidatePair : mCheckList) {
				if ((candidatePair->getState() == IceCandidatePair::State::InProgress) &&
				    (candidatePair->getLocalCandidate()->getComponentId() ==
				     validCandidatePair->getValid()->getLocalCandidate()->getComponentId()) &&
				    candidatePair->getPriority() < validCandidatePair->getValid()->getPriority()) {
					// Set the retransmission number to the max to stop retransmissions for this pair.
					candidatePair->mRetransmissions = ICE_MAX_RETRANSMISSIONS;
				}
			}
		}
	}
}

// Conclude ICE processing as defined in 8.1.
void IceCheckList::concludeProcessing(RtpSession *rtpSession, bool nominationDelayExpired) {
	if (mState != State::Running) {
		return;
	}

	if (mSession->getRole() == IceRole::Controlling) {
		performNominations(nominationDelayExpired);
	}

	concludeWaitingFrozenAndInProgressPairs();

	for (const auto componentId : mLocalComponentsIds) {
		const auto it =
		    std::find_if(mValidList.begin(), mValidList.end(), [componentId](const auto &validCandidatePair) {
			    return validCandidatePair->getValid()->isNominated() &&
			           (validCandidatePair->getValid()->getLocalCandidate()->getComponentId() == componentId);
		    });
		if (it == mValidList.end()) {
			// This component ID is not present in the valid list, stop here.
			return;
		}
	}

	if ((mState == State::Completed) || (getNbLosingPairs() != 0)) {
		return;
	}

	mState = State::Completed;
	mNominationInProgress = false;
	mNominationDelayRunning = false;
	selectCandidates();
	ms_message("ice: Finished ICE check list [%p] processing successfully!", this);
	mConnectivityChecksRunning = false;
	dumpValidList();
	// Initialise keepalive time.
	mKeepAliveTime = std::chrono::steady_clock::now();
	// Stop all running transactions (since nomination is finished).
	stopRetransmissions();

	const auto rtpRemoteCandidate = getSelectedValidRemoteCandidateForRtp();
	const auto optionalRtcpRemoteCandidate = getSelectedValidRemoteCandidateForRtcp();
	if ((rtpRemoteCandidate == nullptr) ||
	    (optionalRtcpRemoteCandidate.has_value() && (*optionalRtcpRemoteCandidate == nullptr))) {
		ms_error("Cannot get remote candidate for check list [%p]", this);
	} else {
		// Switch the destination of the mediastream to the destination selected by ICE
		const std::shared_ptr<IceCandidate> rtcpRemoteCandidate =
		    optionalRtcpRemoteCandidate.has_value() ? *optionalRtcpRemoteCandidate : rtpRemoteCandidate;
		rtp_session_set_remote_addr_full(rtpSession, rtpRemoteCandidate->getTransportAddress().getIp().c_str(),
		                                 rtpRemoteCandidate->getTransportAddress().getPort(),
		                                 rtcpRemoteCandidate->getTransportAddress().getIp().c_str(),
		                                 rtcpRemoteCandidate->getTransportAddress().getPort());
		auto rtpLocalCandidate = getSelectedValidLocalBaseCandidateForRtp();
		auto rtcpLocalCandidate = getSelectedValidLocalBaseCandidateForRtcp();
		if ((rtpLocalCandidate != nullptr) || (rtcpLocalCandidate != nullptr)) {
			// Switch the source of the mediastream to the source selected by ICE.
			rtp_session_use_local_addr(
			    rtpSession,
			    (rtpLocalCandidate == nullptr) ? "" : rtpLocalCandidate->getTransportAddress().getIp().c_str(),
			    (rtcpLocalCandidate == nullptr) ? "" : rtcpLocalCandidate->getTransportAddress().getIp().c_str());
		}
		if (mSession->isTurnEnabled()) {
			rtpLocalCandidate = getSelectedValidLocalCandidateForRtp();
			if (rtpLocalCandidate != nullptr) {
				ms_turn_context_set_force_rtp_sending_via_relay(getTurnContextFromComponentId(ICE_RTP_COMPONENT_ID),
				                                                rtpLocalCandidate->isRelay() ? TRUE : FALSE);
				if (rtpLocalCandidate->isRelay()) {
					RtpTransport *rtpTransport = nullptr;
					rtp_session_get_transports(mRtpSession, &rtpTransport, nullptr);
					createTurnChannel(rtpTransport, reinterpret_cast<struct sockaddr *>(&mRtpSession->rtp.gs.loc_addr),
					                  mRtpSession->rtp.gs.loc_addrlen, rtpRemoteCandidate->getTransportAddress(),
					                  ICE_RTP_COMPONENT_ID);
				} else {
					deallocateRtpTurnCandidate();
				}
			}
			rtcpLocalCandidate = getSelectedValidLocalBaseCandidateForRtcp();
			if (rtcpLocalCandidate != nullptr) {
				ms_turn_context_set_force_rtp_sending_via_relay(getTurnContextFromComponentId(ICE_RTCP_COMPONENT_ID),
				                                                rtcpLocalCandidate->isRelay() ? TRUE : FALSE);
				if (rtcpLocalCandidate->isRelay() && optionalRtcpRemoteCandidate.has_value() &&
				    (*optionalRtcpRemoteCandidate != nullptr)) {
					RtpTransport *rtpTransport = nullptr;
					rtp_session_get_transports(mRtpSession, nullptr, &rtpTransport);
					createTurnChannel(rtpTransport, reinterpret_cast<struct sockaddr *>(&mRtpSession->rtcp.gs.loc_addr),
					                  mRtpSession->rtcp.gs.loc_addrlen,
					                  (*optionalRtcpRemoteCandidate)->getTransportAddress(), ICE_RTCP_COMPONENT_ID);
				} else {
					deallocateRtcpTurnCandidate();
				}
			}
		}
	}

	// Notify the application of the successful processing.
	auto *event = ortp_event_new(ORTP_EVENT_ICE_CHECK_LIST_PROCESSING_FINISHED);
	ortp_event_get_data(event)->info.ice_processing_successful = TRUE;
	rtp_session_dispatch_event(rtpSession, event);
	mSession->notifyProcessingFinished();
}

// Construct a valid ICE candidate pair as defined in 7.1.3.2.2.
std::shared_ptr<IceCandidatePair>
IceCheckList::constructValidPair(RtpSession *rtpSession,
                                 const std::shared_ptr<IceCandidate> &candidate,
                                 const std::shared_ptr<IceCandidatePair> &succeededPair) {
	std::shared_ptr<IceCandidatePair> candidatePair = nullptr;
	const auto itCandidatePair =
	    std::find_if(mCheckList.begin(), mCheckList.end(), [candidate, succeededPair](const auto &pair) {
		    return (pair->getLocalCandidate() == candidate) &&
		           (pair->getRemoteCandidate() == succeededPair->getRemoteCandidate());
	    });
	if (itCandidatePair == mCheckList.end()) {
		// The candidate pair is not a known candidate pair, compute its priority and add it to the valid list.
		candidatePair = std::shared_ptr<IceCandidatePair>(
		    new IceCandidatePair(candidate, succeededPair->getRemoteCandidate(), mSession->getRole()));
		mPairs.push_back(candidatePair);
	} else {
		// The candidate pair is already in the check list, add it to the valid list.
		candidatePair = *itCandidatePair;
	}

	const auto validCandidatePair =
	    std::shared_ptr<IceValidCandidatePair>(new IceValidCandidatePair(candidatePair, succeededPair));
	const auto localAddressStr = candidatePair->getLocalCandidate()->getTransportAddress().asString();
	const auto remoteAddressStr = candidatePair->getRemoteCandidate()->getTransportAddress().asString();
	const auto itValidPair = std::find_if(mValidList.begin(), mValidList.end(), [validCandidatePair](const auto &pair) {
		return (*(pair->getValid()) == *(validCandidatePair->getValid())) &&
		       (*(pair->getGeneratedFrom()) == *(validCandidatePair->getGeneratedFrom()));
	});
	if (itValidPair == mValidList.end()) {
		if (candidatePair->isDefault()) {
			ms_message("ice: succeeded pair with the local default candidate.");
			// Notify the application that a pair using the default local candidate was verified, which is helpful to
			// know that stream should be now working.
			auto *event = ortp_event_new(ORTP_EVENT_ICE_CHECK_LIST_DEFAULT_CANDIDATE_VERIFIED);
			rtp_session_dispatch_event(mRtpSession, event);
		}
		mValidList.push_back(validCandidatePair);
		std::sort(mValidList.begin(), mValidList.end(), [](const auto &a, const auto &b) {
			return a->getValid()->getPriority() > b->getValid()->getPriority();
		});
		ms_message("ice: Added pair %p to the valid list: %s:%s --> %s:%s", candidatePair.get(),
		           localAddressStr.c_str(), candidatePair->getLocalCandidate()->getTypeStr().c_str(),
		           remoteAddressStr.c_str(), candidatePair->getRemoteCandidate()->getTypeStr().c_str());
		// dumpValidList();
		const auto itLosingPair =
		    std::find_if(mLosingPairs.begin(), mLosingPairs.end(), [candidate, succeededPair](const auto &losingPair) {
			    return (losingPair->getLocalCandidate() == candidate) &&
			           (losingPair->getRemoteCandidate() == succeededPair->getRemoteCandidate());
		    });
		if (itLosingPair != mLosingPairs.end()) {
			mLosingPairs.erase(itLosingPair);
			// Select the losing pair that has just become a valid pair.
			setSelectedValidCandidatePair(validCandidatePair);
			if (mSession->getNbLosingPairs() == 0) {
				// Notify the application that the checks for losing pairs have completed. The answer can now be sent.
				setState(IceCheckList::State::Completed);
				auto *event = ortp_event_new(ORTP_EVENT_ICE_LOSING_PAIRS_COMPLETED);
				ortp_event_get_data(event)->info.ice_processing_successful = TRUE;
				rtp_session_dispatch_event(rtpSession, event);
			}
		}
		return candidatePair;
	}

	ms_message("ice: Pair already in the valid list: %s:%s --> %s:%s", localAddressStr.c_str(),
	           candidatePair->getLocalCandidate()->getTypeStr().c_str(), remoteAddressStr.c_str(),
	           candidatePair->getRemoteCandidate()->getTypeStr().c_str());
	return (*itValidPair)->getValid();
}

std::shared_ptr<IceTransaction> IceCheckList::createTransaction(const std::shared_ptr<IceCandidatePair> &candidatePair,
                                                                const UInt96 transactionId) {
	auto transaction = std::shared_ptr<IceTransaction>(new IceTransaction(candidatePair, transactionId));
	mTransactionList.push_front(transaction);
	return transaction;
}

void IceCheckList::createTurnChannel(RtpTransport *rtpTransport,
                                     const struct sockaddr *localAddress,
                                     const socklen_t localAddressLen,
                                     const IceTransportAddress &remoteTransportAddress,
                                     const uint16_t componentId) {
	auto *const turnContext = getTurnContextFromComponentId(componentId);
	const auto transportAddress = IceTransportAddress(localAddress, localAddressLen);
	const auto request =
	    IceStunRequest::create(turnContext, rtpTransport, transportAddress, MS_TURN_METHOD_CHANNEL_BIND);
	if (request == nullptr) {
		return;
	}
	request->setPeerAddress(remoteTransportAddress.toStunAddress());
	request->setChannelNumber(0x4000 | componentId);
	ms_turn_context_set_channel_number(turnContext, request->getChannelNumber());
	ms_turn_context_set_state(turnContext, MS_TURN_CONTEXT_STATE_BINDING_CHANNEL);
	request->programNextTransmission(std::chrono::steady_clock::now() + ICE_DEFAULT_RTO_DURATION);
	request->addTransaction(request->send(mSession->getSockAddr()));
	addStunRequest(request);
}

void IceCheckList::createTurnContexts() {
	if (mRtpTurnContext == nullptr) {
		mRtpTurnContext = ms_turn_context_new(MS_TURN_CONTEXT_TYPE_RTP, mRtpSession);
	}
	if (mRtcpTurnContext == nullptr) {
		mRtcpTurnContext = ms_turn_context_new(MS_TURN_CONTEXT_TYPE_RTCP, mRtpSession);
	}
}

void IceCheckList::createTurnPermissions() {
	if (!mSession->isTurnEnabled()) {
		return;
	}

	for (const auto &remoteCandidate : mRemoteCandidates) {
		const auto localCandidate =
		    findCandidate(mLocalCandidates, IceCandidate::Type::Relayed, remoteCandidate->getComponentId(),
		                  remoteCandidate->getTransportAddress().getFamily());
		if (localCandidate == nullptr) {
			ms_message("ice: no relay candidate to reach %s for checklist [%p]",
			           remoteCandidate->getTransportAddress().getIp().c_str(), this);
		} else {
			if (localCandidate->getBase() == nullptr) {
				ms_error("ice: Local relay candidate has no base!");
				continue;
			}

			RtpTransport *rtpTransport = getRtpTransport(remoteCandidate->getComponentId());
			if (rtpTransport == nullptr) {
				ms_error("ice: No RTP transport");
				continue;
			}

			MSStunAddress peerAddress = remoteCandidate->getTransportAddress().toStunAddress();
			if (peerAddress.family == MS_STUN_ADDR_FAMILY_IPV6) {
				peerAddress.ip.v6.port = 0;
			} else {
				peerAddress.ip.v4.port = 0;
			}
			auto request = IceStunRequest::create(getTurnContextFromComponentId(remoteCandidate->getComponentId()),
			                                      rtpTransport, localCandidate->getBase()->getTransportAddress(),
			                                      MS_TURN_METHOD_CREATE_PERMISSION);
			if (request == nullptr) {
				ms_error("ice: could not build turn request for checklist [%p]", this);
			} else {
				request->setPeerAddress(peerAddress);
				request->programNextTransmission(std::chrono::steady_clock::now() + ICE_DEFAULT_RTO_DURATION);
				request->addTransaction(request->send(mSession->getSockAddr()));
				addStunRequest(request);
			}
		}
	}
}

void IceCheckList::deallocateRtcpTurnCandidate() const {
	RtpTransport *rtpTransport = nullptr;
	rtp_session_get_transports(mRtpSession, nullptr, &rtpTransport);
	deallocateTurnCandidate(mRtcpTurnContext, rtpTransport, &mRtpSession->rtcp.gs);
}

void IceCheckList::deallocateRtpTurnCandidate() const {
	RtpTransport *rtpTransport = nullptr;
	rtp_session_get_transports(mRtpSession, &rtpTransport, nullptr);
	deallocateTurnCandidate(mRtpTurnContext, rtpTransport, &mRtpSession->rtp.gs);
}

void IceCheckList::deallocateTurnCandidate(MSTurnContext *turnContext,
                                           RtpTransport *rtpTransport,
                                           OrtpStream *stream) const {
	if (turnContext == nullptr) {
		return;
	}

	if (rtpTransport == nullptr) {
		ms_error("ice: no rtp socket found for session [%p]", mRtpSession);
		return;
	}

	if (ms_turn_context_get_state(turnContext) >= MS_TURN_CONTEXT_STATE_ALLOCATION_CREATED) {
		ms_turn_context_set_lifetime(turnContext, 0);
		const auto transportAddress =
		    IceTransportAddress(reinterpret_cast<struct sockaddr *>(&stream->loc_addr), stream->loc_addrlen);
		ms_message("ice: about to deallocate turn candidate for %s:%i", transportAddress.getIp().c_str(),
		           transportAddress.getPort());
		const auto request =
		    IceStunRequest::create(turnContext, rtpTransport, transportAddress, MS_TURN_METHOD_REFRESH);
		if (request != nullptr) {
			const auto _transaction = request->send(mSession->getSockAddr());
		}
	}
	meta_rtp_transport_set_endpoint(rtpTransport, nullptr); // Endpoint is later freed
}

void IceCheckList::deallocateTurnCandidates() {
	deallocateRtpTurnCandidate();
	deallocateRtcpTurnCandidate();
}

void IceCheckList::destroyTurnContexts() {
	deallocateTurnCandidates();
	if (mRtpTurnContext != nullptr) {
		ms_turn_context_destroy(mRtpTurnContext);
		mRtpTurnContext = nullptr;
	}
	if (mRtcpTurnContext != nullptr) {
		ms_turn_context_destroy(mRtcpTurnContext);
		mRtcpTurnContext = nullptr;
	}
}

std::shared_ptr<IceCandidate>
IceCheckList::discoverPeerReflexiveCandidate(const std::shared_ptr<IceCandidatePair> &candidatePair,
                                             const MSStunMessage *msg) {
	const MSStunAddress *xorMappedAddress = ms_stun_message_get_xor_mapped_address(msg);
	const auto transportAddress = IceTransportAddress(xorMappedAddress);
	std::shared_ptr<IceCandidate> candidate = nullptr;
	const auto itCandidate = std::find_if(mLocalCandidates.begin(), mLocalCandidates.end(),
	                                      [candidatePair, transportAddress](const auto &localCandidate) {
		                                      return (localCandidate->getComponentId() ==
		                                              candidatePair->getLocalCandidate()->getComponentId()) &&
		                                             (localCandidate->getTransportAddress() == transportAddress);
	                                      });
	if (itCandidate == mLocalCandidates.end()) {
		ms_message("ice: Discovered peer reflexive candidate %s for componentID %d",
		           transportAddress.asString().c_str(), candidatePair->getLocalCandidate()->getComponentId());
		// Add peer reflexive candidate to the local candidates list.
		candidate =
		    addLocalCandidate(IceCandidate::Type::PeerReflexive, transportAddress,
		                      candidatePair->getLocalCandidate()->getComponentId(), candidatePair->getLocalCandidate());
		computeCandidateFoundation(candidate);
	} else {
		candidate = *itCandidate;
	}
	return candidate;
}

void IceCheckList::eliminateRedundantCandidates() {
	if (mState != State::Running) {
		return;
	}

	std::vector<std::shared_ptr<IceCandidate>> candidatesToRemove;

	for (const auto &candidate : mLocalCandidates) {
		const auto itOtherCandidate =
		    std::find_if(mLocalCandidates.begin(), mLocalCandidates.end(),
		                 [candidate, &candidatesToRemove](const auto &otherCandidate) {
			                 return (std::find(candidatesToRemove.begin(), candidatesToRemove.end(), otherCandidate) ==
			                         candidatesToRemove.end()) &&
			                        (candidate != otherCandidate) &&
			                        (candidate->getTransportAddress() == otherCandidate->getTransportAddress()) &&
			                        (candidate->getBase() == otherCandidate->getBase());
		                 });
		if (itOtherCandidate != mLocalCandidates.end()) {
			const auto &otherCandidate = *itOtherCandidate;
			if (otherCandidate->getPriority() < candidate->getPriority()) {
				candidatesToRemove.push_back(otherCandidate);
			} else {
				candidatesToRemove.push_back(candidate);
			}
		}
	}

	for (const auto &candidate : candidatesToRemove) {
		mLocalCandidates.erase(std::find(mLocalCandidates.begin(), mLocalCandidates.end(), candidate));
	}
}

std::shared_ptr<IceTransaction> IceCheckList::findTransaction(const std::shared_ptr<IceCandidatePair> &candidatePair) {
	const auto it =
	    std::find_if(mTransactionList.begin(), mTransactionList.end(), [candidatePair](const auto &transaction) {
		    return (transaction->getPair() == candidatePair) && !transaction->isCanceled();
	    });
	if (it != mTransactionList.end()) {
		return *it;
	}
	return nullptr;
}

void IceCheckList::formCandidatePairs() {
	for (const auto &localCandidate : mLocalCandidates) {
		for (const auto &remoteCandidate : mRemoteCandidates) {
			if ((localCandidate->getComponentId() == remoteCandidate->getComponentId()) &&
			    (localCandidate->getTransportAddress().getFamily() ==
			     remoteCandidate->getTransportAddress().getFamily())) {
				const auto itCandidate =
				    std::find_if(mPairs.begin(), mPairs.end(), [localCandidate, remoteCandidate](const auto &pair) {
					    return (pair->getLocalCandidate() == localCandidate) &&
					           (pair->getRemoteCandidate() == remoteCandidate);
				    });
				if (itCandidate == mPairs.end()) {
					mPairs.push_back(std::shared_ptr<IceCandidatePair>(
					    new IceCandidatePair(localCandidate, remoteCandidate, mSession->getRole(),
					                         !mSession->isMessageIntegrityCheckEnabled())));
				}
			}
		}
	}
}

bool IceCheckList::gatherCandidates(size_t &checkListIndex) {
	if ((mRtpSession == nullptr) || mGatheringCandidates || (mState == State::Completed) || !isGatheringNeeded()) {
		if (!mGatheringCandidates) {
			ms_message("ice: candidate gathering skipped for rtp session [%p] with check list [%p] in state [%s]",
			           mRtpSession, this, getStateStr().c_str());
		}

		return mGatheringCandidates;
	}

	mGatheringCandidates = true;
	const auto currentTime = std::chrono::steady_clock::now();
	mGatheringStartTime = currentTime;

	RtpTransport *rtpTransport = nullptr;
	rtp_session_get_transports(mRtpSession, &rtpTransport, nullptr);
	if (rtpTransport == nullptr) {
		ms_error("ice: no RTP socket found for session [%p]", mRtpSession);
	} else {
		if (mSession->isTurnEnabled()) {
			// Define the RTP endpoint that will perform STUN encapsulation/decapsulation for TURN data
			meta_rtp_transport_set_endpoint(rtpTransport, ms_turn_context_create_endpoint(mRtpTurnContext));
			ms_turn_context_set_server_addr(mRtpTurnContext,
			                                const_cast<struct sockaddr *>(mSession->getSockAddr().asStructSockAddr()),
			                                mSession->getSockAddr().getLen());

			// Start turn tcp client now if needed
			if (mRtpTurnContext->transport != MS_TURN_CONTEXT_TRANSPORT_UDP) {
				if (mRtpTurnContext->turn_tcp_client == nullptr) {
					mRtpTurnContext->turn_tcp_client = ms_turn_tcp_client_new(
					    mRtpTurnContext, (mRtpTurnContext->transport == MS_TURN_CONTEXT_TRANSPORT_TLS) ? TRUE : FALSE,
					    mRtpTurnContext->root_certificate);
				}
				ms_turn_tcp_client_connect(mRtpTurnContext->turn_tcp_client);
			}
		}

		const auto transportAddress = IceTransportAddress(
		    reinterpret_cast<struct sockaddr *>(&mRtpSession->rtp.gs.loc_addr), mRtpSession->rtp.gs.loc_addrlen);
		const auto request =
		    IceStunRequest::create(mRtpTurnContext, rtpTransport, transportAddress,
		                           mSession->isTurnEnabled() ? MS_TURN_METHOD_ALLOCATE : MS_STUN_METHOD_BINDING);
		if (request == nullptr) {
			mGatheringCandidates = false;
			return mGatheringCandidates;
		}
		request->setGathering(true);

		if (checkListIndex == 0) {
			request->programNextTransmission(currentTime + ICE_DEFAULT_RTO_DURATION);
			if (mSession->isTurnEnabled()) {
				ms_turn_context_set_state(mRtpTurnContext, MS_TURN_CONTEXT_STATE_CREATING_ALLOCATION);
			}
			request->addTransaction(request->send(mSession->getSockAddr()));
		} else {
			request->programNextTransmission(currentTime + 2 * checkListIndex * ICE_DEFAULT_TA_DURATION);
		}
		addStunRequest(request);
	}

	rtpTransport = nullptr;
	rtp_session_get_transports(mRtpSession, nullptr, &rtpTransport);
	if ((rtp_session_rtcp_mux_enabled(mRtpSession) == TRUE) || (rtpTransport == nullptr)) {
		ms_message("ice: no RTCP socket for session [%p]", mRtpSession);
	} else {
		if (mSession->isTurnEnabled()) {
			// Define the RTP endpoint that will perform STUN encapsulation/decapsulation for TURN data
			meta_rtp_transport_set_endpoint(rtpTransport, ms_turn_context_create_endpoint(mRtcpTurnContext));
			ms_turn_context_set_server_addr(mRtcpTurnContext,
			                                const_cast<struct sockaddr *>(mSession->getSockAddr().asStructSockAddr()),
			                                mSession->getSockAddr().getLen());

			// Start turn tcp client now if needed
			if (mRtcpTurnContext->transport != MS_TURN_CONTEXT_TRANSPORT_UDP) {
				if (mRtcpTurnContext->turn_tcp_client == nullptr) {
					mRtcpTurnContext->turn_tcp_client = ms_turn_tcp_client_new(
					    mRtcpTurnContext, (mRtcpTurnContext->transport == MS_TURN_CONTEXT_TRANSPORT_TLS) ? TRUE : FALSE,
					    mRtcpTurnContext->root_certificate);
				}
				ms_turn_tcp_client_connect(mRtcpTurnContext->turn_tcp_client);
			}
		}

		const auto transportAddress = IceTransportAddress(
		    reinterpret_cast<struct sockaddr *>(&mRtpSession->rtcp.gs.loc_addr), mRtpSession->rtcp.gs.loc_addrlen);
		const auto request =
		    IceStunRequest::create(mRtcpTurnContext, rtpTransport, transportAddress,
		                           mSession->isTurnEnabled() ? MS_TURN_METHOD_ALLOCATE : MS_STUN_METHOD_BINDING);
		if (request == nullptr) {
			mGatheringCandidates = false;
			return mGatheringCandidates;
		}
		request->setGathering(true);
		request->programNextTransmission(currentTime + (2 * checkListIndex * ICE_DEFAULT_TA_DURATION) +
		                                 ICE_DEFAULT_TA_DURATION);
		if (mSession->isTurnEnabled()) {
			ms_turn_context_set_state(mRtcpTurnContext, MS_TURN_CONTEXT_STATE_CREATING_ALLOCATION);
		}
		addStunRequest(request);
	}

	checkListIndex++;

	return mGatheringCandidates;
}

std::string IceCheckList::generateArbitraryFoundation() const {
	std::string foundation;
	std::list<std::shared_ptr<IceCandidate>>::const_iterator it;
	do {
		const uint64_t r =
		    (static_cast<uint64_t>(bctbx_random()) << 32) | (static_cast<uint64_t>(bctbx_random()) & 0xffffffff);
		std::ostringstream oss;
		oss << r;
		foundation = oss.str();
		it = std::find_if(mRemoteCandidates.begin(), mRemoteCandidates.end(),
		                  [foundation](const auto &candidate) { return (candidate->getFoundation() == foundation); });
	} while (it != mRemoteCandidates.end());
	return foundation;
}

RtpTransport *IceCheckList::getRtpTransport(const uint16_t componentId) const {
	RtpTransport *rtpTransport = nullptr;
	if (componentId == ICE_RTP_COMPONENT_ID) {
		rtp_session_get_transports(mRtpSession, &rtpTransport, nullptr);
	} else if (componentId == ICE_RTCP_COMPONENT_ID) {
		rtp_session_get_transports(mRtpSession, nullptr, &rtpTransport);
	}
	return rtpTransport;
}

std::shared_ptr<IceValidCandidatePair> IceCheckList::getSelectedValidCandidatePair(const uint16_t componentId) const {
	const auto it = std::find_if(mValidList.begin(), mValidList.end(), [componentId](const auto &validCandidatePair) {
		return (validCandidatePair->isSelected() &&
		        (validCandidatePair->getValid()->getLocalCandidate()->getComponentId() == componentId));
	});
	if (it == mValidList.end()) {
		return nullptr;
	}
	return *it;
}

std::shared_ptr<IceStunRequest> IceCheckList::getStunRequest(const UInt96 &transactionId) const {
	const auto it = std::find_if(mStunRequests.begin(), mStunRequests.end(), [transactionId](const auto &request) {
		return request->getTransaction(transactionId) != nullptr;
	});
	if (it == mStunRequests.end()) {
		return nullptr;
	}
	return *it;
}

MSTurnContext *IceCheckList::getTurnContextFromComponentId(const uint16_t componentId) const {
	if (componentId == ICE_RTP_COMPONENT_ID) {
		return mRtpTurnContext;
	}
	if (componentId == ICE_RTCP_COMPONENT_ID) {
		return mRtcpTurnContext;
	}
	return nullptr;
}

[[nodiscard]] std::vector<std::shared_ptr<IceCandidatePair>> IceCheckList::getValidPairs() const {
	std::vector<std::shared_ptr<IceCandidatePair>> validPairs;

	for (const auto componentId : mLocalComponentsIds) {
		const auto validPairsForComponentId = getValidPairs(componentId);
		validPairs.insert(validPairs.end(), validPairsForComponentId.begin(), validPairsForComponentId.end());
	}
	return validPairs;
}

[[nodiscard]] std::vector<std::shared_ptr<IceCandidatePair>> IceCheckList::getValidPairs(uint16_t componentId) const {
	std::vector<std::shared_ptr<IceCandidatePair>> validPairs;

	const auto it = std::find_if(mValidList.begin(), mValidList.end(), [componentId](const auto &validCandidatePair) {
		return validCandidatePair->getValid()->isNominated() &&
		       (validCandidatePair->getValid()->getLocalCandidate()->getComponentId() == componentId);
	});
	if (it != mValidList.end()) {
		const auto &validCandidatePair = *it;
		validPairs.push_back(validCandidatePair->getValid());
	}
	return validPairs;
}

void IceCheckList::handleReceivedBindingRequest(RtpSession *rtpSession,
                                                const OrtpEventData *eventData,
                                                const MSStunMessage *msg,
                                                const MSStunAddress &remoteAddress) {
	if (!checkReceivedBindingRequestAttributes(rtpSession, eventData, msg, remoteAddress) ||
	    !checkReceivedBindingRequestIntegrity(rtpSession, eventData, msg, remoteAddress) ||
	    !checkReceivedBindingRequestUsername(rtpSession, eventData, msg, remoteAddress) ||
	    !checkReceivedBindingRequestRoleConflict(rtpSession, eventData, msg, remoteAddress)) {
		return;
	}

	const auto transportAddress = IceTransportAddress(&remoteAddress);
	auto peerReflexiveCandidate = learnPeerReflexiveCandidate(eventData, msg, transportAddress);
	auto candidatePair = triggerConnectivityCheckOnBindingRequest(eventData, peerReflexiveCandidate, transportAddress);
	if (candidatePair != nullptr) {
		updateNominatedFlagOnBindingRequest(msg, candidatePair);
	}
	sendBindingResponse(rtpSession, eventData, msg, remoteAddress);
	concludeProcessing(rtpSession, false);
}

void IceCheckList::handleReceivedBindingResponse(RtpSession *rtpSession,
                                                 const OrtpEventData *eventData,
                                                 const MSStunMessage *msg,
                                                 const MSStunAddress &remoteAddress) {
	if (mGatheringCandidates && handleReceivedTurnAllocateSuccessResponse(rtpSession, eventData, msg, remoteAddress)) {
		return;
	}

	const UInt96 transactionId = ms_stun_message_get_tr_id(msg);
	const auto transactionIdStr = IceUtils::getTransactionIdStr(transactionId);
	const auto itTransaction =
	    std::find_if(mTransactionList.begin(), mTransactionList.end(), [transactionId](const auto &transaction) {
		    const auto id = transaction->getId();
		    return memcmp(&id, &transactionId, sizeof(transactionId)) == 0;
	    });
	if (itTransaction == mTransactionList.end()) {
		// We received an a binding response concerning an unknown binding request, ignore it...
		ms_warning("ice: Received a binding response for an unknown transaction ID: %s", transactionIdStr.c_str());
		return;
	}

	const auto &transaction = *itTransaction;
	if (transaction->isCanceled()) {
		// We received a binding response concerning a canceled binding request transaction
		ms_message("ice: Received a binding response for a canceled transaction ID: %s", transactionIdStr.c_str());
		// It has to be processed anyway. According to 7.3.1.4 , cancelation just stops retransmission and do not
		// consider the lack of response as a failure.
	}

	const auto succeededPair = transaction->getPair();
	if (!checkReceivedBindingResponseAddresses(eventData, succeededPair, remoteAddress) ||
	    !checkReceivedBindingResponseAttributes(msg)) {
		return;
	}
	const auto candidate = discoverPeerReflexiveCandidate(succeededPair, msg);
	const auto validPair = constructValidPair(rtpSession, candidate, succeededPair);
	updatePairStatesOnBindingResponse(succeededPair);
	updateNominatedFlagOnBindingResponse(validPair, succeededPair);
	concludeProcessing(rtpSession, false);
}

void IceCheckList::handleReceivedErrorResponse(RtpSession *rtpSession,
                                               const OrtpEventData *eventData,
                                               const MSStunMessage *msg) {
	if (mGatheringCandidates) {
		handleStunErrorResponse(rtpSession, eventData, msg);
	} else {
		const auto transactionId = ms_stun_message_get_tr_id(msg);
		const auto itTransaction =
		    std::find_if(mTransactionList.begin(), mTransactionList.end(), [transactionId](const auto &transaction) {
			    auto id = transaction->getId();
			    return (memcmp(&id, &transactionId, sizeof(transactionId)) == 0);
		    });
		if (itTransaction == mTransactionList.end()) {
			// We received an error response concerning an unknown binding request, ignore it...
			return;
		}

		const auto candidatePair = (*itTransaction)->getPair();
		if ((ms_stun_message_has_error_code(msg) == TRUE) &&
		    (ms_stun_message_get_error_code(msg, nullptr) == MS_STUN_ERROR_CODE_UNAUTHORIZED) &&
		    candidatePair->shouldRetryWithDummyMessageIntegrity()) {
			ms_warning("ice pair [%p], retry skipping message integrity for compatibility with older version",
			           candidatePair.get());
			candidatePair->setRetryWithDummyMessageIntegrity(false);
			candidatePair->setUseDummyHmac(true);
			return;
		}

		candidatePair->setState(IceCandidatePair::State::Failed);
		ms_message("ice: Error response, set state to Failed for pair %p: %s:%s --> %s:%s", candidatePair.get(),
		           candidatePair->getLocalCandidate()->getTransportAddress().asString().c_str(),
		           candidatePair->getLocalCandidate()->getTypeStr().c_str(),
		           candidatePair->getRemoteCandidate()->getTransportAddress().asString().c_str(),
		           candidatePair->getRemoteCandidate()->getTypeStr().c_str());

		if ((ms_stun_message_has_error_code(msg) == TRUE) &&
		    (ms_stun_message_get_error_code(msg, nullptr) == MS_ICE_ERROR_CODE_ROLE_CONFLICT)) {
			// Handle error 487 (Role Conflict) according to 7.1.3.1.
			switch (candidatePair->getRole()) {
				case IceRole::Controlling:
					ms_message("ice: Switch to the CONTROLLED role");
					mSession->setRole(IceRole::Controlled);
					break;
				case IceRole::Controlled:
					ms_message("ice: Switch to the CONTROLLING role");
					mSession->setRole(IceRole::Controlling);
					break;
			}

			// Set the state of the pair to Waiting and trigger a check.
			candidatePair->setState(IceCandidatePair::State::Waiting);
			queueTriggeredCheck(candidatePair);
		}

		concludeProcessing(rtpSession, false);
	}
}

bool IceCheckList::handleReceivedTurnAllocateSuccessResponse(RtpSession *rtpSession,
                                                             const OrtpEventData *eventData,
                                                             const MSStunMessage *msg,
                                                             const MSStunAddress &remoteStunAddress) {
	bool stunServerResponse = false;
	const auto servStunAddress = mSession->getSockAddr().ipv6toIpv4().toStunAddress();
	if (ms_compare_stun_addresses(&remoteStunAddress, &servStunAddress) == TRUE) {
		return false;
	}

	const UInt96 transactionId = ms_stun_message_get_tr_id(msg);
	const auto request = getStunRequest(transactionId);
	if (request != nullptr) {
		const auto componentId = IceUtils::getComponentIdFromEventData(eventData);
		const auto [serverReflexiveStunAddress, relayStunAddress] = parseStunResponse(msg);
		if ((componentId != ICE_INVALID_COMPONENT_ID) && (serverReflexiveStunAddress != nullptr)) {
			const auto recvAddr = IceUtils::SockAddr(&eventData->packet->recv_addr).ipv6toIpv4();
			const auto transportAddress = IceTransportAddress(recvAddr.asStructSockAddr(), recvAddr.getLen());
			auto itBaseCandidate = std::find_if(mLocalCandidates.begin(), mLocalCandidates.end(),
			                                    [componentId, transportAddress](const auto &candidate) {
				                                    return (candidate->getComponentId() == componentId) &&
				                                           (candidate->getTransportAddress() == transportAddress);
			                                    });
			if ((mRtpTurnContext != nullptr) &&
			    (ms_turn_context_get_transport(mRtpTurnContext) != MS_TURN_CONTEXT_TRANSPORT_UDP)) {
				// Fallback code for TCP/TLS, where recvAddr is not needed
				int family = (serverReflexiveStunAddress->family == MS_STUN_ADDR_FAMILY_IPV6) ? AF_INET6 : AF_INET;
				itBaseCandidate = std::find_if(mLocalCandidates.begin(), mLocalCandidates.end(),
				                               [componentId, family](const auto &candidate) {
					                               return (candidate->getType() == IceCandidate::Type::Host) &&
					                                      (candidate->getComponentId() == componentId) &&
					                                      (candidate->getTransportAddress().getFamily() == family);
				                               });
				if ((itBaseCandidate == mLocalCandidates.end()) && (family == AF_INET)) {
					// Handle NAT64 case where the local candidate is IPv6 but the reflexive candidate returned by STUN
					// is IPv4.
					itBaseCandidate = std::find_if(
					    mLocalCandidates.begin(), mLocalCandidates.end(), [componentId](const auto &candidate) {
						    return (candidate->getType() == IceCandidate::Type::Host) &&
						           (candidate->getComponentId() == componentId) &&
						           (candidate->getTransportAddress().getFamily() == AF_INET6);
					    });
				}
			}

			if (itBaseCandidate == mLocalCandidates.end()) {
				ms_error("Received allocate response through an unknown local interface.");
			} else {
				const auto &baseCandidate = *itBaseCandidate;
				const auto serverReflexiveTransportAddress = IceTransportAddress(serverReflexiveStunAddress);
				if (serverReflexiveTransportAddress.getPort() != 0) {
					addLocalCandidate(IceCandidate::Type::ServerReflexive, serverReflexiveTransportAddress, componentId,
					                  baseCandidate);
					ms_message("ice: Add candidate obtained by STUN/TURN: %s:srflx",
					           serverReflexiveTransportAddress.asString().c_str());

					if (mSession->isTurnEnabled()) {
						request->getTurnContext()->stats.nb_successful_allocate++;
						scheduleTurnAllocationRefresh(componentId, ms_stun_message_get_lifetime(msg));
					}
					if ((relayStunAddress != nullptr) && (relayStunAddress->family != 0)) {
						const auto relayTransportAddress = IceTransportAddress(relayStunAddress);
						if (relayTransportAddress.getPort() != 0) {
							if (mSession->isTurnEnabled()) {
								ms_turn_context_set_allocated_relay_addr(request->getTurnContext(), *relayStunAddress);
							}
							addLocalCandidate(IceCandidate::Type::Relayed, relayTransportAddress, componentId,
							                  baseCandidate);
							ms_message("ice: Add candidate obtained by STUN/TURN: %s:relay",
							           relayTransportAddress.asString().c_str());
						}
					}
				}
			}
			request->setResponded(true);
			if (mSession->isTurnEnabled()) {
				ms_turn_context_set_state(request->getTurnContext(), MS_TURN_CONTEXT_STATE_ALLOCATION_CREATED);
				if (ms_stun_message_has_lifetime(msg) == TRUE) {
					ms_turn_context_set_lifetime(request->getTurnContext(), ms_stun_message_get_lifetime(msg));
				}
			}
		}
		stunServerResponse = true;
	}

	if (std::none_of(mStunRequests.begin(), mStunRequests.end(),
	                 [](const auto &request) { return request->isGathering() && !request->isResponded(); })) {
		stopGathering();
		ms_message("ice: Finished candidates gathering for check list %p", this);
		dumpCandidates();
		if (mSession->findCheckListGatheringCandidates() == nullptr) {
			// Notify the application when there is no longer any check list gathering candidates.
			auto *event = ortp_event_new(ORTP_EVENT_ICE_GATHERING_FINISHED);
			ortp_event_get_data(event)->info.ice_processing_successful = TRUE;
			mSession->setGatheringEndTs(eventData->ts);
			rtp_session_dispatch_event(rtpSession, event);
		}
	}

	return stunServerResponse;
}

void IceCheckList::handleReceivedTurnChannelBindSuccessResponse(const OrtpEventData *eventData,
                                                                const MSStunMessage *msg) {
	const auto componentId = IceUtils::getComponentIdFromEventData(eventData);
	const auto transactionId = ms_stun_message_get_tr_id(msg);
	const auto request = getStunRequest(transactionId);
	if ((request == nullptr) || (componentId == ICE_INVALID_COMPONENT_ID)) {
		return;
	}
	auto *turnContext = getTurnContextFromComponentId(componentId);
	ms_turn_context_set_state(turnContext, MS_TURN_CONTEXT_STATE_CHANNEL_BOUND);
	removeStunRequest(transactionId);
	scheduleTurnChannelBindRefresh(componentId, request->getChannelNumber(), request->getPeerAddress());
}

void IceCheckList::handleReceivedTurnCreatePermissionSuccessResponse(const OrtpEventData *eventData,
                                                                     const MSStunMessage *msg) {
	const auto componentId = IceUtils::getComponentIdFromEventData(eventData);
	if (componentId == ICE_INVALID_COMPONENT_ID) {
		return;
	}
	const UInt96 transactionId = ms_stun_message_get_tr_id(msg);
	const auto request = getStunRequest(transactionId);
	if (request == nullptr) {
		return;
	}

	const auto peerAddress = request->getPeerAddress();
	removeStunRequest(transactionId);
	ms_turn_context_allow_peer_address(getTurnContextFromComponentId(componentId), &peerAddress);
	scheduleTurnPermissionRefresh(componentId, peerAddress);
}

void IceCheckList::handleReceivedTurnRefreshSuccessResponse(const OrtpEventData *eventData, const MSStunMessage *msg) {
	const auto componentId = IceUtils::getComponentIdFromEventData(eventData);
	if (componentId == ICE_INVALID_COMPONENT_ID) {
		return;
	}

	// First remove the now terminated STUN transaction
	const auto transactionId = ms_stun_message_get_tr_id(msg);
	removeStunRequest(transactionId);
	// Then Update related TURN context
	auto *const turnContext = getTurnContextFromComponentId(componentId);
	if (turnContext == nullptr) {
		ms_warning("ice: no turn context while receiving refresh success response");
		return;
	}

	if (ms_turn_context_get_lifetime(turnContext) == 0) {
		// TURN deallocation success
		ms_turn_context_set_state(turnContext, MS_TURN_CONTEXT_STATE_IDLE);
	} else {
		scheduleTurnAllocationRefresh(componentId, ms_stun_message_get_lifetime(msg));
		turnContext->stats.nb_successful_refresh++;
	}
}

void IceCheckList::handleStunErrorResponse(const RtpSession *rtpSession,
                                           const OrtpEventData *eventData,
                                           const MSStunMessage *msg) {
	auto *rtpTransport = IceUtils::getTransportFromRtpSession(rtpSession, eventData);
	const auto itRequest =
	    std::find_if(mStunRequests.begin(), mStunRequests.end(),
	                 [rtpTransport](const auto &request) { return request->getRtpTransport() == rtpTransport; });
	if (itRequest == mStunRequests.end()) {
		return;
	}

	const auto &request = *itRequest;
	if ((ms_stun_message_get_error_code(msg, nullptr) != 401) || (mSession->mStunAuthRequestedCb == nullptr)) {
		return;
	}

	const char *username = nullptr;
	const char *password = nullptr;
	const char *ha1 = nullptr;
	const char *realm = ms_stun_message_get_realm(msg);
	const char *nonce = ms_stun_message_get_nonce(msg);
	mSession->mStunAuthRequestedCb(mSession->mStunAuthRequestedUserdata, realm, nonce, &username, &password, &ha1);
	if ((username == nullptr) || !mSession->isTurnEnabled()) {
		return;
	}

	auto *turnContext = getTurnContextFromComponentId(IceUtils::getComponentIdFromEventData(eventData));
	ms_turn_context_set_realm(turnContext, realm);
	ms_turn_context_set_nonce(turnContext, nonce);
	ms_turn_context_set_username(turnContext, username);
	ms_turn_context_set_password(turnContext, password);
	ms_turn_context_set_ha1(turnContext, ha1);
	request->programNextTransmission(std::chrono::steady_clock::now() + ICE_DEFAULT_RTO_DURATION);
	request->addTransaction(request->sendTurnAllocateRequest(mSession->getSockAddr()));
}

bool IceCheckList::hasLocalComponentId(const uint16_t componentId) const {
	return std::find(mLocalComponentsIds.begin(), mLocalComponentsIds.end(), componentId) != mLocalComponentsIds.end();
}

bool IceCheckList::isFrozen() const {
	return std::none_of(mCheckList.begin(), mCheckList.end(), [](const auto &candidatePair) {
		return (candidatePair->getState() != IceCandidatePair::State::Frozen);
	});
}

std::shared_ptr<IceCandidate> IceCheckList::learnPeerReflexiveCandidate(const OrtpEventData *eventData,
                                                                        const MSStunMessage *msg,
                                                                        const IceTransportAddress &transportAddress) {
	const auto componentId = IceUtils::getComponentIdFromEventData(eventData);
	if (componentId == ICE_INVALID_COMPONENT_ID) {
		return nullptr;
	}

	const auto itRemoteCandidate = std::find_if(mRemoteCandidates.begin(), mRemoteCandidates.end(),
	                                            [componentId, transportAddress](const auto &candidate) {
		                                            return (candidate->getComponentId() == componentId) &&
		                                                   (candidate->getTransportAddress() == transportAddress);
	                                            });
	if (itRemoteCandidate == mRemoteCandidates.end()) {
		ms_message("ice: Learned peer reflexive candidate %s:%d for componentID %d", transportAddress.getIp().c_str(),
		           transportAddress.getPort(), componentId);
		// Add peer reflexive candidate to the remote candidates list.
		const auto foundation = generateArbitraryFoundation();
		return addRemoteCandidate(IceCandidate::Type::PeerReflexive, transportAddress, componentId,
		                          ms_stun_message_get_priority(msg), foundation, false);
	}
	return nullptr;
}

std::shared_ptr<IceCandidatePair>
IceCheckList::lookupPossibleValidPair(const std::shared_ptr<IceCandidatePair> &candidatePair) const {
	for (const auto &validPair : mValidList) {
		if (validPair->getValid() == candidatePair) {
			return candidatePair;
		}
		if (validPair->getGeneratedFrom() == candidatePair) {
			ms_message("ice: found the reflexive candidate corresponding to the candidate pair on which the binding "
			           "request was received.");
			return validPair->getValid();
		}
	}
	return nullptr;
}

void IceCheckList::nominate(const std::vector<std::shared_ptr<IceValidCandidatePair>> &bestValidCandidatePairs) {
	for (const auto &validCandidatePair : bestValidCandidatePairs) {
		if (!validCandidatePair->getGeneratedFrom()->hasUseCandidate()) {
			validCandidatePair->getGeneratedFrom()->setUseCandidate(true);
			queueTriggeredCheck(validCandidatePair->getGeneratedFrom());
		}
	}
	mNominationInProgress = true;
}

void IceCheckList::pairCandidates() {
	if (mState != State::Running) {
		return;
	}

	mConnectivityChecksRunning = true;
	createTurnPermissions();
	ms_message("ice: connectivity checks are going to start for check list %p", this);
	formCandidatePairs();
	pruneCandidatePairs();

	// Generate pair foundations list
	for (const auto &pair : mCheckList) {
		mFoundations.insert(
		    IcePairFoundation(pair->getLocalCandidate()->getFoundation(), pair->getRemoteCandidate()->getFoundation()));
	}
}

void IceCheckList::performNominations(const bool nominationDelayExpired) {
	if (mNominationInProgress) {
		return;
	}

	std::vector<std::shared_ptr<IceValidCandidatePair>> bestValidCandidatePairs;
	bool concludable = true;
	bool needMoreTime = false;
	int nbNominationsToDo = 0;

	for (const auto componentId : mLocalComponentsIds) {
		std::vector<std::shared_ptr<IceValidCandidatePair>> validCandidatePairs;
		std::copy_if(mValidList.begin(), mValidList.end(), std::back_inserter(validCandidatePairs),
		             [componentId](const auto &validCandidatePair) {
			             return validCandidatePair->getValid()->getLocalCandidate()->getComponentId() == componentId;
		             });
		if (validCandidatePairs.empty()) {
			ms_message("IceCheckList::performNominations(cl=%p): no valid pairs yet for componentID %i", this,
			           componentId);
			concludable = false;
			break;
		}
		std::sort(validCandidatePairs.begin(), validCandidatePairs.end(), [](const auto &a, const auto &b) {
			return a->getValid()->getPriority() > b->getValid()->getPriority();
		});
		// Normally the first element in the list is the best one to nominate. However if nomination is failing,
		// we'll nominate the next one and so one.
		std::shared_ptr<IceValidCandidatePair> validCandidatePair = nullptr;
		for (const auto &pair : validCandidatePairs) {
			validCandidatePair = pair;
			if (pair->getGeneratedFrom()->isNominationFailing()) {
				ms_message("IceCheckList::performNominations: a nominated pair is not responding");
			} else {
				break;
			}
		}
		if (validCandidatePair == nullptr) {
			ms_warning("IceCheckList::performNominations(cl=%p): no more pair to nominate for componentID %i", this,
			           componentId);
		} else {
			if ((validCandidatePair->getGeneratedFrom()->getRemoteCandidate()->getType() ==
			     IceCandidate::Type::Relayed) ||
			    (validCandidatePair->getGeneratedFrom()->getLocalCandidate()->getType() ==
			     IceCandidate::Type::Relayed)) {
				needMoreTime = true;
			}
			bestValidCandidatePairs.push_back(validCandidatePair);
			if (!validCandidatePair->getGeneratedFrom()->hasUseCandidate()) {
				nbNominationsToDo++;
			}
		}
	}

	if (concludable && (nbNominationsToDo > 0)) {
		ms_message("IceCheckList::performNominations: check list is concludable");
		if (needMoreTime && !mNominationDelayRunning) {
			ms_message("IceCheckList::performNominations(cl=%p): for a component, the best candidate is a relay one, "
			           "let's wait a bit before performing nomination",
			           this);
			mNominationDelayRunning = true;
			mNominationDelayStartTime = std::chrono::steady_clock::now();
		}
		if (nominationDelayExpired || !needMoreTime) {
			ms_message("IceCheckList::performNominations(cl=%p): nominating the best valid pair for each component",
			           this);
			mNominationDelayRunning = false;
			nominate(bestValidCandidatePairs);
		}
	}
}

// Prune pairs according to 5.7.3.
void IceCheckList::pruneCandidatePairs() {
	for_each(mPairs.begin(), mPairs.end(), [](const auto &pair) { pair->replaceSrflxCandidateByBase(); });
	for (auto it = mPairs.begin(); it != mPairs.end();) {
		const auto pair = *it;
		const auto itOther = std::find_if(mPairs.begin(), mPairs.end(), [pair](const auto &otherPair) {
			return (pair->getLocalCandidate() == otherPair->getLocalCandidate()) &&
			       (pair->getRemoteCandidate() == otherPair->getRemoteCandidate());
		});
		if (itOther == mPairs.end()) {
			++it;
		} else {
			const auto &otherPair = *itOther;
			if (otherPair->getPriority() > pair->getPriority()) {
				// Found duplicate with higher priority so prune current pair.
				it = mPairs.erase(it);
			} else {
				++it;
			}
		}
	}

	// Create the check list
	mCheckList.clear();
	for_each(mPairs.begin(), mPairs.end(), [this](const auto &pair) { mCheckList.push_back(pair); });
	std::sort(mCheckList.begin(), mCheckList.end(),
	          [](const auto &a, const auto &b) { return a->getPriority() > b->getPriority(); });

	// Limit the number of connectivity checks
	const auto nbPairs = mCheckList.size();
	if (nbPairs > mSession->getMaxConnectivityChecks()) {
		mCheckList.resize(mSession->getMaxConnectivityChecks());
	}
}

void IceCheckList::queueTriggeredCheck(const std::shared_ptr<IceCandidatePair> &pair) {
	if (std::find(mTriggeredChecksQueue.begin(), mTriggeredChecksQueue.end(), pair) == mTriggeredChecksQueue.end()) {
		mTriggeredChecksQueue.push_back(pair);
	} else {
		// The pair is already in the triggered checks queue, do not add it again
	}
}

void IceCheckList::removeGatheringStunRequests() {
	for (auto it = mStunRequests.begin(); it != mStunRequests.end();) {
		const auto request = *it;
		if (request->isGathering()) {
			it = mStunRequests.erase(it);
		} else {
			++it;
		}
	}
}

void IceCheckList::removeRtcpCandidatePairs() {
	for (auto it = mPairs.begin(); it != mPairs.end();) {
		const auto pair = *it;
		if (pair->getLocalCandidate()->getComponentId() == ICE_RTCP_COMPONENT_ID) {
			// Remove possible transaction using this pair
			removeTransactionUsingPair(pair);
			// Remove pair from triggered check queue
			const auto itTriggeredChecksQueue =
			    std::find(mTriggeredChecksQueue.begin(), mTriggeredChecksQueue.end(), pair);
			if (itTriggeredChecksQueue != mTriggeredChecksQueue.end()) {
				mTriggeredChecksQueue.erase(itTriggeredChecksQueue);
			}
			// Remove from losing pair
			const auto itLosingPair = std::find(mLosingPairs.begin(), mLosingPairs.end(), pair);
			if (itLosingPair != mLosingPairs.end()) {
				mLosingPairs.erase(itLosingPair);
			}
			it = mPairs.erase(it);
		} else {
			++it;
		}
	}
}

void IceCheckList::removeStunRequest(const UInt96 &transactionId) {
	for (auto it = mStunRequests.begin(); it != mStunRequests.end();) {
		if ((*it)->getTransaction(transactionId) != nullptr) {
			it = mStunRequests.erase(it);
		} else {
			++it;
		}
	}
}

void IceCheckList::removeTransactionUsingPair(const std::shared_ptr<IceCandidatePair> &pair) {
	for (auto it = mTransactionList.begin(); it != mTransactionList.end();) {
		if ((*it)->getPair() == pair) {
			it = mTransactionList.erase(it);
		} else {
			++it;
		}
	}
}

void IceCheckList::restart() {
	mRemoteCredentials = std::nullopt;
	rtp_session_use_local_addr(mRtpSession, "", ""); // Reset the sources of rtp_session
	mStunRequests.clear();
	mTransactionList.clear();
	mFoundations.clear();
	mRemoteComponentIds.clear();
	mValidList.clear();
	mCheckList.clear();
	mTriggeredChecksQueue.clear();
	mLosingPairs.clear();
	mPairs.clear();
	mRemoteCandidates.clear();
	mState = State::Running;
	mMismatch = false;
	mGatheringCandidates = false;
	mGatheringFinished = false;
	mNominationDelayRunning = false;
	mTaTime = std::chrono::steady_clock::now();
	mNominationInProgress = false;
	mKeepAliveTime = std::chrono::steady_clock::time_point();
	mGatheringStartTime = std::chrono::steady_clock::time_point();
	mNominationDelayStartTime = std::chrono::steady_clock::time_point();
}

void IceCheckList::retransmitConnectivityChecks(const std::chrono::steady_clock::time_point currentTime,
                                                const RtpSession *rtpSession) {
	for (const auto &candidatePair : mCheckList) {
		if (mNominationInProgress && !candidatePair->hasUseCandidate()) {
			// No need to retransmit anything during nomination
			return;
		}
		if ((candidatePair->getState() == IceCandidatePair::State::InProgress) &&
		    ((currentTime - candidatePair->getTransmissionTime()) >= candidatePair->getRto())) {
			sendBindingRequest(candidatePair, rtpSession);
		}
	}
}

void IceCheckList::scheduleTurnAllocationRefresh(const uint16_t componentId, const uint32_t lifetime) {
	auto *turnContext = getTurnContextFromComponentId(componentId);
	auto *rtpTransport = getRtpTransport(componentId);
	const auto *stream = IceUtils::getOrtpStreamFromRtpSessionAndComponentId(mRtpSession, componentId);
	const auto transportAddress =
	    IceTransportAddress(reinterpret_cast<const struct sockaddr *>(&stream->loc_addr), stream->loc_addrlen);
	const auto request = IceStunRequest::create(turnContext, rtpTransport, transportAddress, MS_TURN_METHOD_REFRESH);
	if (request == nullptr) {
		return;
	}

	if (mSession->isShortTurnRefreshEnabled()) {
		request->programNextTransmission(std::chrono::steady_clock::now() + std::chrono::seconds(5));
	} else {
		request->programNextTransmission(std::chrono::steady_clock::now() +
		                                 std::chrono::milliseconds(static_cast<uint32_t>((lifetime * .9f) * 1000)));
	}
	addStunRequest(request);
}

void IceCheckList::scheduleTurnChannelBindRefresh(const uint16_t componentId,
                                                  const uint16_t channelNumber,
                                                  const MSStunAddress &peerAddress) {
	auto *turnContext = getTurnContextFromComponentId(componentId);
	auto *rtpTransport = getRtpTransport(componentId);
	const auto *stream = IceUtils::getOrtpStreamFromRtpSessionAndComponentId(mRtpSession, componentId);
	const auto transportAddress =
	    IceTransportAddress(reinterpret_cast<const struct sockaddr *>(&stream->loc_addr), stream->loc_addrlen);
	const auto request =
	    IceStunRequest::create(turnContext, rtpTransport, transportAddress, MS_TURN_METHOD_CHANNEL_BIND);
	if (request == nullptr) {
		return;
	}

	request->setChannelNumber(channelNumber);
	request->setPeerAddress(peerAddress);
	if (mSession->isShortTurnRefreshEnabled()) {
		request->programNextTransmission(std::chrono::steady_clock::now() + std::chrono::seconds(5));
	} else {
		request->programNextTransmission(std::chrono::steady_clock::now() + std::chrono::minutes(9));
	}
	addStunRequest(request);
}

void IceCheckList::scheduleTurnPermissionRefresh(const uint16_t componentId, const MSStunAddress &peerAddress) {
	auto *turnContext = getTurnContextFromComponentId(componentId);
	auto *const rtpTransport = getRtpTransport(componentId);
	auto *const stream = IceUtils::getOrtpStreamFromRtpSessionAndComponentId(mRtpSession, componentId);
	const auto transportAddress =
	    IceTransportAddress(reinterpret_cast<struct sockaddr *>(&stream->loc_addr), stream->loc_addrlen);
	const auto request =
	    IceStunRequest::create(turnContext, rtpTransport, transportAddress, MS_TURN_METHOD_CREATE_PERMISSION);
	if (request == nullptr) {
		return;
	}
	request->setPeerAddress(peerAddress);
	if (mSession->isShortTurnRefreshEnabled()) {
		request->programNextTransmission(std::chrono::steady_clock::now() + std::chrono::seconds(5));
	} else {
		request->programNextTransmission(std::chrono::steady_clock::now() + std::chrono::minutes(4));
	}
	addStunRequest(request);
}

void IceCheckList::selectCandidates() {
	if (mState != State::Completed) {
		return;
	}

	for_each(mValidList.begin(), mValidList.end(),
	         [](const auto &validCandidatePair) { validCandidatePair->setSelected(false); });
	for (uint16_t componentId : {ICE_RTP_COMPONENT_ID, ICE_RTCP_COMPONENT_ID}) {
		const auto it =
		    std::find_if(mValidList.begin(), mValidList.end(), [componentId](const auto &validCandidatePair) {
			    return validCandidatePair->getValid()->isNominated() &&
			           (validCandidatePair->getValid()->getLocalCandidate()->getComponentId() == componentId);
		    });
		if (it == mValidList.end()) {
			continue;
		}
		(*it)->setSelected(true);
	}
}

// Send a STUN binding request for ICE connectivity checks according to 7.1.2.
void IceCheckList::sendBindingRequest(const std::shared_ptr<IceCandidatePair> &candidatePair,
                                      const RtpSession *rtpSession) {
	auto transaction = findTransaction(candidatePair);

	if (candidatePair->getState() == IceCandidatePair::State::InProgress) {
		if (transaction == nullptr) {
			ms_error("ice: No transaction found for InProgress pair");
			return;
		}
		// This is a retransmission: update the number of retransmissions, the retransmission timer value, and the
		// transmission time.
		candidatePair->incrementRetransmissions();
		if ((mSession->getRole() == IceRole::Controlling) && candidatePair->hasUseCandidate() &&
		    !candidatePair->isNominationFailing() &&
		    (candidatePair->getNbRetransmissions() > ICE_MAX_RETRANSMISSIONS_FOR_NOMINATIONS)) {
			// The nomination process is abnormally long: possibly the nat association has been accidentally closed
			// by the media stream. Nominate an alternate pair if possible.
			candidatePair->setNominationFailing(true);
			mNominationInProgress = false;
			performNominations(false);
			// Despite we've started a new nomination, we continue the retransmissions for that pair, in case a
			// response is finally received.
		}

		if (candidatePair->getNbRetransmissions() > ICE_MAX_RETRANSMISSIONS) {
			// Too much retransmissions, stop sending connectivity checks for this pair.
			candidatePair->setState(IceCandidatePair::State::Failed);
			return;
		}
		candidatePair->increaseRetransmissionTimer();
	}
	candidatePair->setTransmissionTime(std::chrono::steady_clock::now());

	RtpTransport *rtpTransport = nullptr;
	if (candidatePair->getLocalCandidate()->getComponentId() == ICE_RTP_COMPONENT_ID) {
		rtp_session_get_transports(rtpSession, &rtpTransport, nullptr);
	} else if (candidatePair->getLocalCandidate()->getComponentId() == ICE_RTCP_COMPONENT_ID) {
		rtp_session_get_transports(rtpSession, nullptr, &rtpTransport);
	} else {
		return;
	}

	const auto sourceStunAddress = candidatePair->getLocalCandidate()->getTransportAddress().toStunAddress();
	const auto destStunAddress = candidatePair->getRemoteCandidate()->getTransportAddress().toStunAddress();
	auto *msg = ms_stun_binding_request_create();
	const auto username = (getRemoteCredentials().has_value() ? getRemoteCredentials()->getUfrag() : "") + ":" +
	                      getLocalCredentials().getUfrag();
	ms_stun_message_set_username(msg, username.c_str());
	ms_stun_message_set_password(msg,
	                             getRemoteCredentials().has_value() ? getRemoteCredentials()->getPwd().c_str() : "");
	ms_stun_message_enable_message_integrity(msg, TRUE);
	ms_stun_message_enable_fingerprint(msg, TRUE);

	// Set the PRIORITY attribute as defined in 7.1.2.1.
	ms_stun_message_set_priority(msg,
	                             (candidatePair->getLocalCandidate()->getPriority() & 0x00ffffff) |
	                                 (IceCandidate::getTypePreferenceValue(IceCandidate::Type::PeerReflexive) << 24));

	// Include the USE-CANDIDATE attribute if the pair is nominated and the agent has the controlling role, as
	// defined in 7.1.2.1.
	if ((mSession->getRole() == IceRole::Controlling) && candidatePair->hasUseCandidate()) {
		ms_stun_message_enable_use_candidate(msg, TRUE);
	}

	// Include the ICE-CONTROLLING or ICE-CONTROLLED attribute depending on the role of the agent, as defined
	// in 7.1.2.2.
	switch (mSession->getRole()) {
		case IceRole::Controlling:
			ms_stun_message_set_ice_controlling(msg, mSession->getTieBreaker());
			break;
		case IceRole::Controlled:
			ms_stun_message_set_ice_controlled(msg, mSession->getTieBreaker());
			break;
	}

	// Keep the same transaction ID for retransmission.
	if (candidatePair->getState() == IceCandidatePair::State::InProgress) {
		ms_stun_message_set_tr_id(msg, transaction->getId());
	} else {
		transaction = createTransaction(candidatePair, ms_stun_message_get_tr_id(msg));
	}

	// For backward compatibility
	ms_stun_message_enable_dummy_message_integrity(msg, candidatePair->shouldUseDummyHmac() ? TRUE : FALSE);

	char *buf = nullptr;
	auto len = ms_stun_message_encode(msg, &buf);
	if (len > 0) {
		if (candidatePair->getState() == IceCandidatePair::State::InProgress) {
			ms_message("ice: Retransmit (%d) binding request for pair %p: %s:%s --> %s:%s [%s]",
			           candidatePair->getNbRetransmissions(), candidatePair.get(),
			           candidatePair->getLocalCandidate()->getTransportAddress().asString().c_str(),
			           candidatePair->getLocalCandidate()->getTypeStr().c_str(),
			           candidatePair->getRemoteCandidate()->getTransportAddress().asString().c_str(),
			           candidatePair->getRemoteCandidate()->getTypeStr().c_str(), transaction->getIdStr().c_str());
		} else {
			ms_message("ice: Send binding request for %s pair %p: %s:%s --> %s:%s [%s] (flags:%s)",
			           candidatePair->getStateStr().c_str(), candidatePair.get(),
			           candidatePair->getLocalCandidate()->getTransportAddress().asString().c_str(),
			           candidatePair->getLocalCandidate()->getTypeStr().c_str(),
			           candidatePair->getRemoteCandidate()->getTransportAddress().asString().c_str(),
			           candidatePair->getRemoteCandidate()->getTypeStr().c_str(), transaction->getIdStr().c_str(),
			           candidatePair->hasUseCandidate() ? "use-candidate" : "none");
		}

		if (mSession->isForcedRelayEnabled() &&
		    (candidatePair->getRemoteCandidate()->getType() != IceCandidate::Type::Relayed) &&
		    (candidatePair->getLocalCandidate()->getType() != IceCandidate::Type::Relayed)) {
			ms_message("ice: Forced relay, did not send binding request for %s pair %p: %s:%s --> %s:%s [%s]",
			           candidatePair->getStateStr().c_str(), candidatePair.get(),
			           candidatePair->getLocalCandidate()->getTransportAddress().asString().c_str(),
			           candidatePair->getLocalCandidate()->getTypeStr().c_str(),
			           candidatePair->getRemoteCandidate()->getTransportAddress().asString().c_str(),
			           candidatePair->getRemoteCandidate()->getTypeStr().c_str(), transaction->getIdStr().c_str());
		} else {
			IceUtils::sendMessageToStunAddress(rtpTransport, buf, len, sourceStunAddress, destStunAddress);
		}

		if (candidatePair->getState() != IceCandidatePair::State::InProgress) {
			// First transmission of the request, initialize the retransmission timer.
			candidatePair->initializeRetransmissionTimer();
			// Save the role of the agent.
			candidatePair->setRole(mSession->getRole());
			// Change the state of the pair.
			candidatePair->setState(IceCandidatePair::State::InProgress);
		}
	}
	if (buf != nullptr) {
		ms_free(buf);
	}
	ms_stun_message_destroy(msg);
}

void IceCheckList::sendBindingResponse(const RtpSession *rtpSession,
                                       const OrtpEventData *eventData,
                                       const MSStunMessage *msg,
                                       const MSStunAddress &remoteAddress) const {

	auto *const rtpTransport = IceUtils::getTransportFromRtpSession(rtpSession, eventData);
	if (rtpTransport == nullptr) {
		return;
	}

	// Create the binding response, copying the transaction ID from the request.
	const UInt96 transactionId = ms_stun_message_get_tr_id(msg);
	auto *response = ms_stun_binding_success_response_create();
	ms_stun_message_set_tr_id(response, transactionId);
	ms_stun_message_enable_message_integrity(response, TRUE);
	ms_stun_message_enable_fingerprint(response, TRUE);
	// For backward compatibility
	ms_stun_message_enable_dummy_message_integrity(response, ms_stun_message_dummy_message_integrity_enabled(msg));

	// Add username for message integrity
	auto username = mSession->getLocalCredentials().getUfrag() + ":" +
	                (mSession->getRemoteCredentials().has_value() ? mSession->getRemoteCredentials()->getUfrag() : "");
	ms_stun_message_set_username(response, username.c_str());
	if ((ms_stun_message_dummy_message_integrity_enabled(msg) == TRUE) && !mSession->isMessageIntegrityCheckEnabled()) {
		// Legacy case, include username for backward compatibility
	} else {
		ms_stun_message_include_username_attribute(response, FALSE);
	}

	// Add password for message integrity
	ms_stun_message_set_password(response, mSession->getLocalCredentials().getPwd().c_str());

	// Add the mapped address to the response.
	ms_stun_message_set_xor_mapped_address(response, remoteAddress);

	char *buf = nullptr;
	auto len = ms_stun_message_encode(response, &buf);
	if (len > 0) {
		const auto destAddress = IceUtils::SockAddr(remoteAddress);
		const auto sourceAddress = IceUtils::SockAddr(&eventData->packet->recv_addr).ipv6toIpv4();
		ms_message("ice: Send binding response: %s --> %s [%s]", sourceAddress.asString().c_str(),
		           destAddress.asString().c_str(), IceUtils::getTransactionIdStr(transactionId).c_str());
		IceUtils::sendMessageToSocket(rtpTransport, buf, len, sourceAddress.asStructSockAddr(),
		                              destAddress.asStructSockAddr(), destAddress.getLen());
	}
	if (buf != nullptr) {
		ms_free(buf);
	}
	ms_stun_message_destroy(response);
}

void IceCheckList::sendKeepAlivePackets(RtpSession *rtpSession) const {
	if (mState == State::Completed) {
		for (const auto componentId : mLocalComponentsIds) {
			const auto selectedValidCandidatePair = getSelectedValidCandidatePair(componentId);
			if (selectedValidCandidatePair != nullptr) {
				selectedValidCandidatePair->getValid()->sendIndication(rtpSession);
			}
		}
	} else if (mState == State::Running) {
		// Refresh pairs on the valid list, to keep them alive until conclusion
		const auto currentTime = std::chrono::steady_clock::now();
		for (const auto &validCandidatePair : mValidList) {
			validCandidatePair->checkKeepAlive(currentTime, rtpSession);
		}
	}
}

void IceCheckList::sendStunRequests() {
	for (auto it = mStunRequests.begin(); it != mStunRequests.end();) {
		const auto stunRequest = *it;
		const auto currentTime = std::chrono::steady_clock::now();
		if (stunRequest->isResponded() || (currentTime < stunRequest->getNextTransmissionTime())) {
			it++;
			continue;
		}
		if (stunRequest->getNbTransactions() < ICE_MAX_STUN_REQUEST_RETRANSMISSIONS) {
			stunRequest->programNextTransmission(currentTime + ICE_DEFAULT_RTO_DURATION);
			const auto transaction = stunRequest->send(mSession->getSockAddr());
			if (transaction == nullptr) {
				it = mStunRequests.erase(it);
			} else {
				stunRequest->addTransaction(transaction);
				it++;
			}
		} else {
			it++;
		}
	}
}

std::shared_ptr<IceCandidatePair> IceCheckList::sendTriggeredCheck(const RtpSession *rtpSession) {
	if (mTriggeredChecksQueue.empty()) {
		return nullptr;
	}

	auto candidatePair = mTriggeredChecksQueue.front();
	mTriggeredChecksQueue.pop_front();
	sendBindingRequest(candidatePair, rtpSession);
	return candidatePair;
}

void IceCheckList::setBaseForSrflxCandidates() {
	for_each(mLocalComponentsIds.begin(), mLocalComponentsIds.end(),
	         [this](const auto &componentId) { setBaseForSrflxCandidates(componentId); });
}

void IceCheckList::setBaseForSrflxCandidates(const uint16_t componentId) {
	for (const auto family : {AF_INET, AF_INET6}) {
		const auto it = std::find_if(mLocalCandidates.begin(), mLocalCandidates.end(),
		                             [componentId, family](const auto &candidate) {
			                             return (candidate->getType() == IceCandidate::Type::Host) &&
			                                    (candidate->getComponentId() == componentId) &&
			                                    (candidate->getTransportAddress().getFamily() == family);
		                             });
		if (it != mLocalCandidates.end()) {
			const auto &base = *it;
			for (const auto &candidate : mLocalCandidates) {
				if ((candidate->getType() == IceCandidate::Type::ServerReflexive) &&
				    (candidate->getBase() == nullptr) && (candidate->getComponentId() == base->getComponentId()) &&
				    (candidate->getTransportAddress().getFamily() == base->getTransportAddress().getFamily())) {
					candidate->setBase(base);
				}
			}
		}
	}
}

// This method sets the supplied validCandidatePair as selected, but insuring that the previously selected valid
// pair for that componentId is unselected.
void IceCheckList::setSelectedValidCandidatePair(
    const std::shared_ptr<IceValidCandidatePair> &validCandidatePair) const {
	for (const auto &pair : mValidList) {
		if (pair->isSelected() && (pair != validCandidatePair) &&
		    (pair->getValid()->getLocalCandidate()->getComponentId() ==
		     validCandidatePair->getValid()->getLocalCandidate()->getComponentId())) {
			pair->setSelected(false);
		}
	}
	validCandidatePair->setSelected(true);
}

void IceCheckList::setTransactionResponseTime(const UInt96 &transactionId, const MSTimeSpec responseTime) {
	const auto it = std::find_if(mStunRequests.begin(), mStunRequests.end(), [&transactionId](const auto &request) {
		return request->getTransaction(transactionId) != nullptr;
	});
	if (it == mStunRequests.end()) {
		return;
	}
	(*it)->getTransaction(transactionId)->setResponseTime(responseTime);
}

void IceCheckList::stopGathering() {
	mGatheringCandidates = false;
	mGatheringFinished = true;
	collectGatheringRoundTripTimes();
	removeGatheringStunRequests();
}

void IceCheckList::stopRetransmissions() {
	for (const auto &candidatePair : mCheckList) {
		if (candidatePair->getState() == IceCandidatePair::State::InProgress) {
			candidatePair->setState(IceCandidatePair::State::Failed);
			const auto it = std::find(mTriggeredChecksQueue.begin(), mTriggeredChecksQueue.end(), candidatePair);
			if (it != mTriggeredChecksQueue.end()) {
				mTriggeredChecksQueue.erase(it);
			}
		}
	}
}

// Trigger checks as defined in 7.2.1.4.
std::shared_ptr<IceCandidatePair>
IceCheckList::triggerConnectivityCheckOnBindingRequest(const OrtpEventData *eventData,
                                                       const std::shared_ptr<IceCandidate> &peerReflexiveCandidate,
                                                       const IceTransportAddress &remoteTransportAddress) {
	const auto recvAddr = IceUtils::SockAddr(&eventData->packet->recv_addr).ipv6toIpv4();
	const auto localTransportAddress = IceTransportAddress(recvAddr.asStructSockAddr(), recvAddr.getLen());
	const auto itLocalCandidate =
	    std::find_if(mLocalCandidates.begin(), mLocalCandidates.end(), [localTransportAddress](const auto &candidate) {
		    return candidate->getTransportAddress() == localTransportAddress;
	    });
	if (itLocalCandidate == mLocalCandidates.end()) {
		ms_error("ice: Local candidate %s not found!", localTransportAddress.asString().c_str());
		return nullptr;
	}

	const auto &localCandidate = *itLocalCandidate;
	std::shared_ptr<IceCandidate> remoteCandidate = nullptr;
	if (peerReflexiveCandidate != nullptr) {
		remoteCandidate = peerReflexiveCandidate;
	} else {
		const auto itRemoteCandidate =
		    std::find_if(mRemoteCandidates.begin(), mRemoteCandidates.end(),
		                 [remoteTransportAddress, localCandidate](const auto &candidate) {
			                 return (candidate->getComponentId() == localCandidate->getComponentId()) &&
			                        (candidate->getTransportAddress() == remoteTransportAddress);
		                 });
		if (itRemoteCandidate == mRemoteCandidates.end()) {
			ms_error("ice: Remote candidate %s not found!", remoteTransportAddress.asString().c_str());
			return nullptr;
		}
		remoteCandidate = *itRemoteCandidate;
	}

	std::shared_ptr<IceCandidatePair> candidatePair = nullptr;
	auto itCandidatePair = std::find_if(mCheckList.begin(), mCheckList.end(),
	                                    [localCandidate, remoteCandidate](const auto &candidatePair) {
		                                    return (candidatePair->getLocalCandidate() == localCandidate) &&
		                                           (candidatePair->getRemoteCandidate() == remoteCandidate);
	                                    });
	if (itCandidatePair == mCheckList.end()) {
		// The pair is not in the check list yet.
		ms_message("ice: Add new candidate pair [%p - %p] in the check list", localCandidate.get(),
		           remoteCandidate.get());
		// Check if the pair is in the list of pairs even if it is not in the check list.
		itCandidatePair =
		    std::find_if(mPairs.begin(), mPairs.end(), [localCandidate, remoteCandidate](const auto &candidatePair) {
			    return (candidatePair->getLocalCandidate() == localCandidate) &&
			           (candidatePair->getRemoteCandidate() == remoteCandidate);
		    });
		if (itCandidatePair == mPairs.end()) {
			candidatePair = std::shared_ptr<IceCandidatePair>(
			    new IceCandidatePair(localCandidate, remoteCandidate, mSession->getRole()));
			mPairs.push_back(candidatePair);
		} else {
			candidatePair = *itCandidatePair;
		}
		itCandidatePair = std::find(mCheckList.begin(), mCheckList.end(), candidatePair);
		if (itCandidatePair == mCheckList.end()) {
			mCheckList.push_back(candidatePair);
			std::sort(mCheckList.begin(), mCheckList.end(),
			          [](const auto &a, const auto &b) { return a->getPriority() > b->getPriority(); });
		}
		// Set the state of the pair to Waiting and trigger a check.
		candidatePair->setState(IceCandidatePair::State::Waiting);
		queueTriggeredCheck(candidatePair);
	} else {
		// The pair has been found in the check list.
		candidatePair = *itCandidatePair;
		switch (candidatePair->getState()) {
			case IceCandidatePair::State::Waiting:
			case IceCandidatePair::State::Frozen:
			case IceCandidatePair::State::Failed:
				candidatePair->setState(IceCandidatePair::State::Waiting);
				queueTriggeredCheck(candidatePair);
				break;
			case IceCandidatePair::State::InProgress:
				ms_message("ice: we are receiving a STUN request on pair %p, for which an outgoing STUN transaction is "
				           "running.",
				           candidatePair.get());
				if (!candidatePair->hasCanceledTransaction()) {
					// Cancel the transaction, but this may happen only once
					auto transaction = findTransaction(candidatePair);
					if (transaction != nullptr) {
						ms_message("ice: transaction is canceled, a new binding request sent.");
						transaction->cancel();
						// And queue a new triggered check
						candidatePair->setState(IceCandidatePair::State::Waiting);
						queueTriggeredCheck(candidatePair);
						candidatePair->setHasCanceledTransaction(true);
					}
				}
				break;
			case IceCandidatePair::State::Succeeded:
				// Nothing to be done.
				break;
		}
	}

	return candidatePair;
}

// Update the nominated flag of a candidate pair according to 7.2.1.5.
void IceCheckList::updateNominatedFlagOnBindingRequest(const MSStunMessage *msg,
                                                       const std::shared_ptr<IceCandidatePair> &candidatePair) {
	if ((ms_stun_message_use_candidate_enabled(msg) == FALSE) || (mSession->getRole() == IceRole::Controlling)) {
		return;
	}

	auto validPair = lookupPossibleValidPair(candidatePair);
	switch (candidatePair->getState()) {
		case IceCandidatePair::State::Succeeded:
			if (validPair == nullptr) {
				ms_warning("ice: receiving a binding request with use-candidate flag on succeeded pair that is not "
				           "in the valid list.");
				candidatePair->setIsNominated(true);
			} else {
				ms_message("ice: receiving a binding request with use-candidate flag on succeeded pair");
				validPair->setIsNominated(true);
			}
			break;
		case IceCandidatePair::State::Waiting:
		case IceCandidatePair::State::Frozen:
		case IceCandidatePair::State::InProgress:
			// Normally valid should be null if go here
			ms_message("ice: receiving a binding request with nominated flag on non-succeeded pair");
			// We cannot accept the nomination immediately. We will wait for our pair to complete its bind requests, and
			// then the pair will be officially nominated
			candidatePair->setNominationPending(true);
			break;
		case IceCandidatePair::State::Failed:
			ms_error("ice: receiving a binding request with nominated flag on failed pair. This should not happen.");
			break;
	}
}

// Update the nominated flag of a candidate pair according to 7.1.3.2.4.
void IceCheckList::updateNominatedFlagOnBindingResponse(const std::shared_ptr<IceCandidatePair> &validPair,
                                                        const std::shared_ptr<IceCandidatePair> &succeededPair) const {
	switch (mSession->getRole()) {
		case IceRole::Controlling:
			if (succeededPair->hasUseCandidate()) {
				validPair->setNominationFailing(false);
				validPair->setIsNominated(true);
				// dumpValidList();
			}
			break;
		case IceRole::Controlled:
			if (succeededPair->isNominationPending()) {
				validPair->setIsNominated(true);
				succeededPair->setNominationPending(false);
			}
			break;
	}
}

// Update the pair states according to 7.1.3.2.3.
void IceCheckList::updatePairStatesOnBindingResponse(const std::shared_ptr<IceCandidatePair> &candidatePair) const {
	// Set the state of the pair that generated the check to Succeeded.
	candidatePair->setState(IceCandidatePair::State::Succeeded);

	// Change the state of all Frozen pairs with the same foundation to Waiting.
	for (const auto &pair : mCheckList) {
		if ((pair != candidatePair) && (pair->getState() == IceCandidatePair::State::Frozen) &&
		    (pair->getLocalCandidate()->getFoundation() == candidatePair->getLocalCandidate()->getFoundation()) &&
		    (pair->getRemoteCandidate()->getFoundation() == candidatePair->getRemoteCandidate()->getFoundation())) {
			ms_message("ice: Change state of pair %p from Frozen to Waiting", pair.get());
			pair->setState(IceCandidatePair::State::Waiting);
		}
	}
}

std::shared_ptr<IceCandidate> IceCheckList::findCandidate(const std::list<std::shared_ptr<IceCandidate>> &candidates,
                                                          const IceCandidate::Type type,
                                                          const uint16_t componentId,
                                                          int family) {
	const auto itInet =
	    std::find_if(candidates.begin(), candidates.end(), [type, componentId, family](const auto &candidate) {
		    return (candidate->getType() == type) && (candidate->getComponentId() == componentId) &&
		           (candidate->getTransportAddress().getFamily() == family);
	    });
	return (itInet == candidates.end()) ? nullptr : *itInet;
}

std::pair<const MSStunAddress *, const MSStunAddress *> IceCheckList::parseStunResponse(const MSStunMessage *msg) {
	std::pair<const MSStunAddress *, const MSStunAddress *> result = {nullptr, nullptr};
	result.first = ms_stun_message_get_xor_mapped_address(msg);
	if (result.first == nullptr) {
		result.first = ms_stun_message_get_mapped_address(msg);
	}
	if (result.first == nullptr) {
		return result;
	}
	result.second = ms_stun_message_get_xor_relayed_address(msg);
	return result;
}

} // namespace ms2

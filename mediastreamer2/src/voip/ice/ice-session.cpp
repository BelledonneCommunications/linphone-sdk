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
#include <cinttypes>
#include <iomanip>
#include <sstream>

#include "mediastreamer2/ice-checklist.h"
#include "mediastreamer2/ice-credentials.h"
#include "mediastreamer2/ice-session.h"

namespace mediastreamer::nat {

IceSession::IceSession() {
	mChecklists.fill(nullptr);
	setDefaultCandidatesTypes(
	    {IceCandidate::Type::Relayed, IceCandidate::Type::ServerReflexive, IceCandidate::Type::Host});
	generateTieBreaker();
	generateLocalCredentials();
}

//------------------------------------------------------------------------------

void IceSession::addCheckList(const std::shared_ptr<IceCheckList> &checklist, const size_t index) {
	if (index >= mChecklists.size()) {
		BCTBX_SLOGE << "IceSession::addCheckList: Wrong index parameter";
		return;
	}
	if (mChecklists[index] != nullptr) {
		BCTBX_SLOGE << "IceSession::addCheckList: Existing check list at index " << index << ", remove it first";
		return;
	}
	mChecklists[index] = checklist;
	checklist->setSession(shared_from_this());
	if (checklist->getState() == IceCheckList::State::Running) {
		mState = State::Running;
	}
}

bool IceSession::areCandidatesGathered() const {
	return std::none_of(mChecklists.begin(), mChecklists.end(), [](const auto &checklist) {
		return (checklist != nullptr) && !checklist->areCandidatesGathered();
	});
}

void IceSession::checkMismatch() const {
	forEachValidCheckList([](const auto &checklist) { checklist->checkMismatch(); });
}

void IceSession::chooseDefaultLocalCandidates() const {
	forEachValidCheckList([](const auto &checklist) { checklist->chooseDefaultLocalCandidates(); });
}

void IceSession::chooseDefaultRemoteCandidates() const {
	forEachValidCheckList([](const auto &checklist) { checklist->chooseDefaultRemoteCandidates(); });
}

void IceSession::computeCandidatesFoundations() const {
	forEachValidCheckList([](const auto &checklist) { checklist->computeCandidatesFoundations(); });
}

void IceSession::dump() const {
	BCTBX_SLOGM << "Session:\n"
	            << "\trole=" << getRoleStr() << " tie-breaker=" << mTieBreaker << "\n"
	            << "\tlocal_ufrag=" << mLocalCredentials.getUfrag() << " local_pwd=" << mLocalCredentials.getPwd()
	            << "\n"
	            << "\tremote_ufrag=" << (mRemoteCredentials.has_value() ? mRemoteCredentials->getUfrag().c_str() : "")
	            << " remote_pwd=" << (mRemoteCredentials.has_value() ? mRemoteCredentials->getPwd().c_str() : "");
}

void IceSession::eliminateRedundantCandidates() const {
	forEachValidCheckList([](const auto &checklist) { checklist->eliminateRedundantCandidates(); });
}

void IceSession::enableTurn(const bool enable) {
	mTurnEnabled = enable;
	if (mTurnEnabled) {
		forEachValidCheckList([](const auto &checklist) { checklist->createTurnContexts(); });
	}
}

bool IceSession::gatherCandidates(const SockAddr &stunServerAddress) {
	bool gatheringInProgress = false;

	mSockAddr = stunServerAddress;
	mGatheringStartTs = std::chrono::steady_clock::now();
	if (isGatheringNeeded()) {
		size_t index = 0;
		forEachValidCheckList([&index, &gatheringInProgress](const auto &checklist) {
			if (checklist->gatherCandidates(index)) {
				gatheringInProgress = true;
			}
		});
	} else {
		/* Notify end of gathering since it has already been done. */
		OrtpEvent *ev = ortp_event_new(ORTP_EVENT_ICE_GATHERING_FINISHED);
		ortp_event_get_data(ev)->info.ice_processing_successful = TRUE;
		mGatheringEndTs = mGatheringStartTs;
		rtp_session_dispatch_event(getFirstCheckList()->mRtpSession, ev);
	}

	return gatheringInProgress;
}

std::optional<std::chrono::milliseconds> IceSession::getAverageGatheringRoundTripTime() const {
	if ((mGatheringStartTs == std::chrono::steady_clock::time_point()) ||
	    (mGatheringEndTs == std::chrono::steady_clock::time_point())) {
		return std::nullopt;
	}

	IceStunRequest::RoundTripTime rtt;
	forEachValidCheckList([&rtt](const auto &checklist) { rtt << checklist->getRoundTripTime(); });
	return rtt.getAverage();
}

std::shared_ptr<IceCheckList> IceSession::getNthCheckList(const size_t n) const {
	if (n >= mChecklists.size()) {
		return nullptr;
	}
	return mChecklists[n];
}

std::optional<std::chrono::milliseconds> IceSession::getGatheringDuration() const {
	if ((mGatheringStartTs == std::chrono::steady_clock::time_point()) ||
	    (mGatheringEndTs == std::chrono::steady_clock::time_point())) {
		return std::nullopt;
	}
	return std::chrono::duration_cast<std::chrono::milliseconds>(mGatheringEndTs - mGatheringStartTs);
}

size_t IceSession::getNbCheckLists() const {
	return std::count_if(mChecklists.begin(), mChecklists.end(),
	                     [](const auto &checklist) { return checklist != nullptr; });
}

unsigned int IceSession::getNbLosingPairs() const {
	int nbLosingPairs = 0;
	forEachValidCheckList([&nbLosingPairs](const auto &checklist) { nbLosingPairs += checklist->getNbLosingPairs(); });
	return nbLosingPairs;
}

const std::string &IceSession::getRoleStr() const {
	static const std::array<std::string, 2> roleStrs = {
	    "Controlling",
	    "Controlled",
	};
	return roleStrs[static_cast<size_t>(mRole)];
}

bool IceSession::hasCompletedCheckList() const {
	return std::any_of(mChecklists.begin(), mChecklists.end(), [](const auto &checklist) {
		if (checklist == nullptr) {
			return false;
		}
		return checklist->getState() == IceCheckList::State::Completed;
	});
}

void IceSession::removeCheckList(const std::shared_ptr<IceCheckList> &checklistToRemove) {
	if (checklistToRemove == nullptr) {
		return;
	}

	const auto it = std::find_if(mChecklists.begin(), mChecklists.end(),
	                             [checklistToRemove](const auto &checklist) { return checklist == checklistToRemove; });
	if (it != mChecklists.end()) {
		*it = nullptr;
	}

	// If all remaining check lists have completed, set the session state to completed
	if (findUnsuccessfulCheckList() == nullptr) {
		mState = State::Completed;
	}
}

void IceSession::removeCheckList(const size_t index) {
	if (index >= mChecklists.size()) {
		BCTBX_SLOGE << "IceSession::removeCheckList: Wrong index parameter";
		return;
	}
	removeCheckList(mChecklists[index]);
}

void IceSession::reset(const IceRole role) {
	restart(role);
	forEachValidCheckList([](const auto &checklist) {
		checklist->clearLocalCandidates();
		checklist->clearLocalComponentsIds();
	});
}

void IceSession::restart(const IceRole role) {
	BCTBX_SLOGW << "ICE session restart";

	mState = State::Stopped;
	generateTieBreaker();
	generateLocalCredentials();
	mRemoteCredentials = std::nullopt;
	mEventTime = std::chrono::steady_clock::time_point();
	mSendEvent = false;

	forEachValidCheckList([](const auto &checklist) { checklist->restart(); });
	setRole(role);
}

void IceSession::selectCandidates() const {
	forEachValidCheckList([](const auto &checklist) { checklist->selectCandidates(); });
}

void IceSession::setBaseForSrflxCandidates() const {
	forEachValidCheckList([](const auto &checklist) { checklist->setBaseForSrflxCandidates(); });
}

void IceSession::setKeepAliveTimeout(const std::chrono::seconds keepAliveTimeout) {
	mKeepAliveTimeout =
	    (keepAliveTimeout < kIceDefaultKeepaliveTimeout) ? kIceDefaultKeepaliveTimeout : keepAliveTimeout;
}

void IceSession::setRole(const IceRole role) {
	if (mRole != role) {
		// Compute new candidate pair priorities if the role changes.
		mRole = role;
		computePairPriorities();
	}
}

void IceSession::setTurnCn(const std::string &cn) const {
	forEachTurnContextOfEachValidCheckList([cn](const auto &turnContext) { turnContext->setCn(cn); });
}

void IceSession::setTurnRootCertificatePath(const std::string &rootCertificatePath) const {
	forEachTurnContextOfEachValidCheckList(
	    [rootCertificatePath](const auto &turnContext) { turnContext->setRootCertificatePath(rootCertificatePath); });
}

void IceSession::setTurnTransport(const TurnContext::Transport transport) const {
	forEachTurnContextOfEachValidCheckList(
	    [transport](const auto &turnContext) { turnContext->setTransport(transport); });
}

void IceSession::startConnectivityChecks() {
	pairCandidates();
	mState = State::Running;
	mConnectivityChecksStartTs = std::chrono::steady_clock::now();
}

//------------------------------------------------------------------------------

void IceSession::computePairPriorities() const {
	forEachValidCheckList([](const auto &checklist) { checklist->computePairPriorities(); });
}

bool IceSession::containsCheckList(const std::shared_ptr<IceCheckList> &checklist) const {
	if (checklist == nullptr) {
		return false;
	}
	return std::find(mChecklists.begin(), mChecklists.end(), checklist) != mChecklists.end();
}

std::shared_ptr<IceCheckList> IceSession::findCheckListFromState(IceCheckList::State state) const {
	const auto it = std::find_if(mChecklists.begin(), mChecklists.end(), [state](const auto &checklist) {
		return (checklist != nullptr) && (checklist->getState() == state);
	});
	return (it == mChecklists.end()) ? nullptr : *it;
}

std::shared_ptr<IceCheckList> IceSession::findCheckListGatheringCandidates() const {
	const auto it = std::find_if(mChecklists.begin(), mChecklists.end(), [](const auto &checklist) {
		return (checklist != nullptr) && checklist->isGatheringCandidates();
	});
	return (it == mChecklists.end()) ? nullptr : *it;
}

std::shared_ptr<IceCheckList> IceSession::findRunningCheckList() const {
	return findCheckListFromState(IceCheckList::State::Running);
}

std::shared_ptr<IceCheckList> IceSession::findUnsuccessfulCheckList() const {
	const auto it = std::find_if(mChecklists.begin(), mChecklists.end(), [](const auto &checklist) {
		return (checklist != nullptr) && (checklist->getState() != IceCheckList::State::Completed);
	});
	return (it == mChecklists.end()) ? nullptr : *it;
}

void IceSession::forEachTurnContextOfEachValidCheckList(
    const std::function<void(const std::shared_ptr<TurnContext> &)> &callback) const {
	if (!mTurnEnabled) {
		return;
	}

	forEachValidCheckList([callback](const auto &checklist) {
		for (const auto &turnContext : {checklist->getRtpTurnContext(), checklist->getRtcpTurnContext()}) {
			if (turnContext != nullptr) {
				callback(turnContext);
			}
		}
	});
}

void IceSession::forEachValidCheckList(
    const std::function<void(const std::shared_ptr<IceCheckList> &)> &callback) const {
	for (const auto &checklist : mChecklists) {
		if (checklist != nullptr) {
			callback(checklist);
		}
	}
}

void IceSession::generateLocalCredentials() {
	std::ostringstream ossUfrag;
	ossUfrag << std::hex << std::setfill('0') << std::setw(8) << static_cast<int>(bctbx_random());
	std::ostringstream ossPwd;
	ossPwd << std::hex << std::setfill('0') << std::setw(8) << static_cast<int>(bctbx_random())
	       << static_cast<int>(bctbx_random()) << static_cast<int>(bctbx_random());
	mLocalCredentials = IceCredentials(ossUfrag.str(), ossPwd.str());
}

void IceSession::generateTieBreaker() {
	mTieBreaker = (static_cast<uint64_t>(bctbx_random()) << 32) | (static_cast<uint64_t>(bctbx_random()) & 0xffffffff);
}

std::shared_ptr<IceCheckList> IceSession::getFirstCheckList() const {
	const auto it = std::find_if(mChecklists.begin(), mChecklists.end(),
	                             [](const auto &checklist) { return checklist != nullptr; });
	return (it == mChecklists.end()) ? nullptr : *it;
}

bool IceSession::isGatheringNeeded() const {
	return std::any_of(mChecklists.begin(), mChecklists.end(), [](const auto &checklist) {
		return (checklist != nullptr) && (checklist->isGatheringNeeded());
	});
}

void IceSession::notifyProcessingFinished() {
	if (findRunningCheckList() != nullptr) {
		return;
	}

	// There is no longer any running check list
	if (findUnsuccessfulCheckList() == nullptr) {
		// All the check lists of the session have completed successfully.
		mState = State::Completed;
	} else {
		// Some check lists have failed, consider the session to be a failure.
		mState = State::Failed;
	}
	programEventSending(ORTP_EVENT_ICE_SESSION_PROCESSING_FINISHED, std::chrono::milliseconds(1000));
}

void IceSession::pairCandidates() const {
	const auto checklist = findRunningCheckList();
	if (checklist != nullptr) {
		forEachValidCheckList([](const auto &checklist) { checklist->pairCandidates(); });
		checklist->computePairsStates();
		checklist->dumpCandidatePairsFoundations();
		checklist->dumpCandidatePairs();
		checklist->dumpCheckList();
	}
}

void IceSession::programEventSending(const OrtpEventType eventType, const std::chrono::milliseconds delay) {
	mEventTime = std::chrono::steady_clock::now() + delay;
	mEventType = eventType;
	mSendEvent = true;
}

void IceSession::setGatheringEndTs(const ortpTimeSpec ts) {
	const auto duration = std::chrono::seconds(ts.tv_sec) + std::chrono::nanoseconds(ts.tv_nsec);
	mGatheringEndTs = std::chrono::steady_clock::time_point{
	    std::chrono::duration_cast<std::chrono::steady_clock::duration>(duration)};
}

} // namespace mediastreamer::nat

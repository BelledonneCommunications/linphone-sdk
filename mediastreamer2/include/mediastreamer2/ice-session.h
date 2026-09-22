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

#include <array>
#include <chrono>
#include <functional>
#include <optional>

#include "ortp/port.h"

#include "mediastreamer2/ice-candidate.h"
#include "mediastreamer2/ice-checklist.h"
#include "mediastreamer2/ice-constants.h"
#include "mediastreamer2/ice-credentials.h"
#include "mediastreamer2/ice-role.h"
#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/sockaddr.h"
#include "mediastreamer2/stun-auth-listener.h"

namespace mediastreamer::nat {

/**
 * Represents an ICE session.
 */
class MS2_PUBLIC IceSession : public std::enable_shared_from_this<IceSession> {
public:
	friend class IceCheckList;

	/**
	 * ICE session state.
	 */
	enum class State { Stopped, Running, Completed, Failed };

	/**
	 * Allocate a new ICE session.
	 * This must be performed for each media session that is to use ICE.
	 */
	IceSession();

	/**
	 * Destroy a previously allocated ICE session.
	 * To be used when a media session using ICE is tore down.
	 */
	~IceSession() = default;

	/**
	 * Add an ICE check list to an ICE session.
	 * @param checklist The check list to assign to the session
	 * @param index The index of the check list to add
	 */
	void addCheckList(const std::shared_ptr<IceCheckList> &checklist, size_t index);

	/**
	 * Tell whether ICE local candidates have been gathered for an ICE session or not.
	 * @return true if local candidates have been gathered for the session, false otherwise.
	 */
	[[nodiscard]] bool areCandidatesGathered() const;

	/**
	 * Check whether all the ICE check lists of the session includes a default candidate for each component ID in its
	 * remote candidates list.
	 */
	void checkMismatch() const;

	/**
	 * Choose the default candidates of an ICE session.
	 * This function is to be called at the end of the local candidates gathering process, before sending
	 * the SDP to the remote agent.
	 */
	void chooseDefaultLocalCandidates() const;

	/**
	 * Choose the default remote candidates of an ICE session.
	 * This function SHOULD not be used. Instead, the default remote candidates MUST be defined as default
	 * when creating them with IceCheckList::addRemoteCandidate().
	 * However, this function is used by mediastream for testing purpose.
	 */
	void chooseDefaultRemoteCandidates() const;

	/**
	 * Compute the foundations of the local candidates of an ICE session.
	 * This function is to be called at the end of the local candidates gathering process, before sending
	 * the SDP to the remote agent.
	 */
	void computeCandidatesFoundations() const;

	/**
	 * Dump an ICE session in the traces (debug function).
	 */
	void dump() const;

	/**
	 * Eliminate the redundant candidates of an ICE session.
	 * This function is to be called at the end of the local candidates gathering process, before sending
	 * the SDP to the remote agent.
	 */
	void eliminateRedundantCandidates() const;

	/**
	 * Enable forced relay for tests.
	 * @param enable A boolean value telling whether to force relay or not.
	 * The local and reflexive candidates are changed so that these paths do not work to force the use of the relay.
	 */
	void enableForcedRelay(bool enable) {
		mForcedRelay = enable;
	}

	/**
	 * Disable/enable strong message integrity check. Used for backward compatibility only.
	 * It is enabled by default.
	 * @param enable Boolean value telling whether to enable message integrity check or not.
	 */
	void enableMessageIntegrityCheck(bool enable) {
		mCheckMessageIntegrity = enable;
	}

	/**
	 * Enable short TURN refresh for tests.
	 * @param enable A boolean value telling whether to use short turn refresh.
	 * This changes the delay to send allocation refresh, create permission, and channel bind requests.
	 */
	void enableShortTurnRefresh(bool enable) {
		mShortTurnRefresh = enable;
	}

	/**
	 * Enable TURN protocol.
	 * @param enable A boolean value telling whether to enable TURN protocol or not.
	 */
	void enableTurn(bool enable);

	/**
	 * Gather ICE local candidates for an ICE session.
	 * @param stunServerAddress The STUN server address
	 * @return true if the gathering is in progress, false if no gathering is happening.
	 */
	bool gatherCandidates(const SockAddr &stunServerAddress);

	/**
	 * Tell the average round trip time during the gathering process for an ICE session in milliseconds.
	 * @return std::nullopt if gathering has not been run, the average round trip time in milliseconds otherwise.
	 */
	[[nodiscard]] std::optional<std::chrono::milliseconds> getAverageGatheringRoundTripTime() const;

	/**
	 * Get the nth check list of an ICE session.
	 * @param n The index of the check list to access
	 * @return A pointer to the nth check list of the session if it exists, nullptr otherwise
	 */
	[[nodiscard]] std::shared_ptr<IceCheckList> getNthCheckList(size_t n) const;

	/**
	 * Tell the duration of the gathering process for an ICE session in ms.
	 * @return std::nullopt if gathering has not been run, the duration of the gathering process in milliseconds
	 * otherwise.
	 */
	[[nodiscard]] std::optional<std::chrono::milliseconds> getGatheringDuration() const;

	/**
	 * Get the timeout between each keepalive packet in seconds.
	 * @return The duration of the keepalive timeout in seconds
	 */
	[[nodiscard]] std::chrono::seconds getKeepAliveTimeout() const {
		return mKeepAliveTimeout;
	}

	/**
	 * Get the local credentials of an ICE session.
	 * @return A reference to the local credentials of the session
	 */
	[[nodiscard]] const IceCredentials &getLocalCredentials() const {
		return mLocalCredentials;
	}

	/**
	 * Get the number of check lists in an ICE session.
	 * @return The number of check lists in the ICE session
	 */
	[[nodiscard]] size_t getNbCheckLists() const;

	/**
	 * Get the number of losing candidate pairs for an ICE session.
	 * @return The number of losing candidate pairs for the session.
	 */
	[[nodiscard]] unsigned int getNbLosingPairs() const;

	/**
	 * Get the remote credentials of an ICE session.
	 * @return A reference to the remote credentials of the session
	 */
	[[nodiscard]] const std::optional<IceCredentials> &getRemoteCredentials() const {
		return mRemoteCredentials;
	}

	/**
	 * Get the role of the agent for an ICE session.
	 * @return The role of the agent for the session
	 */
	[[nodiscard]] IceRole getRole() const {
		return mRole;
	}

	/**
	 * Get the role of the agent for an ICE session as a human-readable string.
	 * @return The role of the agent for the session as a human-readable string.
	 */
	[[nodiscard]] const std::string &getRoleStr() const;

	/**
	 * Get the state of an ICE session.
	 * @return The state of the session
	 */
	[[nodiscard]] State getState() const {
		return mState;
	}

	/**
	 * Tell whether an ICE session has at least one completed check list.
	 * @return true if the session has at least one completed check list, false otherwise
	 */
	[[nodiscard]] bool hasCompletedCheckList() const;

	/**
	 * Remove an ICE check list from an ICE session.
	 * @param checklistToRemove The check list to remove from the session
	 */
	void removeCheckList(const std::shared_ptr<IceCheckList> &checklistToRemove);

	/**
	 * Remove an ICE check list from an ICE session given its index.
	 * @param index The index of the check list in the ICE session
	 */
	void removeCheckList(size_t index);

	/**
	 * Reset an ICE session.
	 * It has the same effect as a session restart but also clears the local candidates.
	 * @param role The role of the agent after the session restart
	 */
	void reset(IceRole role);

	/**
	 * Restart an ICE session.
	 * @param role The role of the agent after the session restart
	 */
	void restart(IceRole role);

	/**
	 * Select ICE candidates that will be used and notified in the SDP.
	 * This function is to be used by the Controlling agent when ICE processing has finished.
	 */
	void selectCandidates() const;

	/**
	 * Set the base for the local server reflexive candidates of an ICE session.
	 * This function is usually not necessary, because the base candidate is automatically set during gathering.
	 * However, it is required when server-reflexive candidates are added manually into the session,
	 * which happens at least in these two cases:
	 * - in 'mediastream' tool for testing purpose to
	 *   work around the fact that it does not use candidates gathering.
	 * - when server-reflexive candidates are defined by configuration, for example
	 *   when ICE is used in a server software deployed behind a NAT.
	 * It is to be called before starting the connectivity checks.
	 */
	void setBaseForSrflxCandidates() const;

	/**
	 * Set the AF_INET/AF_INET6 preference for electing the default candidates, when both are available.
	 */
	void setDefaultCandidatesPreferIpv6(bool preferIpv6) {
		mDefaultCandidatesPreferIpv6 = preferIpv6;
	}

	/**
	 * Set the preferred type for default candidates, as defined in rfc5245#section-4.1.4.
	 **/
	void setDefaultCandidatesTypes(const std::vector<IceCandidate::Type> &candidatesTypes) {
		mDefaultCandidatesTypes = candidatesTypes;
	}

	/**
	 * Define the timeout between each keepalive packet in seconds.
	 * @param keepAliveTimeout The duration of the keepalive timeout in seconds
	 * The default keepalive timeout is set to 15 seconds.
	 */
	void setKeepAliveTimeout(std::chrono::seconds keepAliveTimeout);

	/**
	 * Set the local credentials of an ICE session.
	 * This method SHOULD not be used. However, it is used by mediastream for testing purpose to
	 * apply the same credentials for local and remote agents because the SDP exchange is bypassed.
	 */
	void setLocalCredentials(const IceCredentials &credentials) {
		mLocalCredentials = credentials;
	}

	/**
	 * Define the maximum number of connectivity checks that will be performed by the agent.
	 * @param value The maximum number of connectivity checks to perform
	 * This function is to be called just after the creation of the session, before any connectivity check is performed.
	 * The default number of connectivity checks is 128.
	 */
	void setMaxConnectivityChecks(const uint8_t value) {
		mMaxConnectivityChecks = value;
	}

	/**
	 * Set the remote credentials of an ICE session.
	 * @param credentials The remote credentials
	 * This function is to be called once the remote credentials have been received via SDP.
	 */
	void setRemoteCredentials(const IceCredentials &credentials) {
		mRemoteCredentials = credentials;
	}

	/**
	 * Set the role of the agent for an ICE session.
	 * @param role The role to set the session to
	 */
	void setRole(IceRole role);

	void setStunAuthListener(StunAuthListener *listener) {
		mStunAuthListener = listener;
	}

	/**
	 * Set TURN CN when using TLS.
	 * @param cn The CN.
	 */
	void setTurnCn(const std::string &cn) const;

	/**
	 * Set TURN root certificate path when using TLS.
	 * @param rootCertificatePath The path of the root certificate.
	 */
	void setTurnRootCertificatePath(const std::string &rootCertificatePath) const;

	/**
	 * Set TURN transport.
	 * @param transport The transport that TURN should use.
	 */
	void setTurnTransport(TurnContext::Transport transport) const;

	/**
	 * Pair the local and the remote candidates for an ICE session and start sending connectivity checks.
	 */
	void startConnectivityChecks();

private:
	void computePairPriorities() const;
	[[nodiscard]] bool containsCheckList(const std::shared_ptr<IceCheckList> &checklist) const;
	[[nodiscard]] std::shared_ptr<IceCheckList> findCheckListFromState(IceCheckList::State state) const;
	[[nodiscard]] std::shared_ptr<IceCheckList> findCheckListGatheringCandidates() const;
	[[nodiscard]] std::shared_ptr<IceCheckList> findRunningCheckList() const;
	[[nodiscard]] std::shared_ptr<IceCheckList> findUnsuccessfulCheckList() const;
	void forEachTurnContextOfEachValidCheckList(
	    const std::function<void(const std::shared_ptr<TurnContext> &)> &callback) const;
	void forEachValidCheckList(const std::function<void(const std::shared_ptr<IceCheckList> &)> &callback) const;
	void generateLocalCredentials();
	void generateTieBreaker();
	[[nodiscard]] bool getDefaultCandidatesPreferIpv6() const {
		return mDefaultCandidatesPreferIpv6;
	}
	[[nodiscard]] std::vector<IceCandidate::Type> getDefaultCandidatesTypes() const {
		return mDefaultCandidatesTypes;
	}
	[[nodiscard]] std::chrono::steady_clock::time_point getEventTime() const {
		return mEventTime;
	}
	[[nodiscard]] OrtpEventType getEventType() const {
		return mEventType;
	}
	[[nodiscard]] std::shared_ptr<IceCheckList> getFirstCheckList() const;
	[[nodiscard]] size_t getMaxConnectivityChecks() const {
		return mMaxConnectivityChecks;
	}

	[[nodiscard]] const SockAddr &getSockAddr() const {
		return mSockAddr;
	}
	[[nodiscard]] StunAuthListener *getStunAuthListener() const {
		return mStunAuthListener;
	}
	[[nodiscard]] std::chrono::milliseconds getTa() const {
		return mTa;
	}
	[[nodiscard]] uint64_t getTieBreaker() const {
		return mTieBreaker;
	}
	[[nodiscard]] bool isForcedRelayEnabled() const {
		return mForcedRelay;
	}
	[[nodiscard]] bool isGatheringNeeded() const;
	[[nodiscard]] bool isMessageIntegrityCheckEnabled() const {
		return mCheckMessageIntegrity;
	}
	[[nodiscard]] bool isShortTurnRefreshEnabled() const {
		return mShortTurnRefresh;
	}
	[[nodiscard]] bool isTurnEnabled() const {
		return mTurnEnabled;
	}
	void notifyProcessingFinished();
	void pairCandidates() const;
	void programEventSending(OrtpEventType eventType, std::chrono::milliseconds delay);
	void setGatheringEndTs(ortpTimeSpec ts);
	void setState(const State state) {
		mState = state;
	}
	[[nodiscard]] bool shouldSendEvent() const {
		return mSendEvent;
	}
	void stopEventSending() {
		mSendEvent = false;
	}

	std::array<std::shared_ptr<IceCheckList>, kIceMaxNbCheckLists> mChecklists; /**< Table of IceChecklist structure
	                                                          pointers. Each element represents a media stream */
	StunAuthListener *mStunAuthListener = nullptr; /**< Listener called when authentication is
	 requested */
	IceCredentials mLocalCredentials; /**< Local credentials for the session (assigned during the session creation) */
	std::optional<IceCredentials> mRemoteCredentials =
	    std::nullopt;                     /**< Remote credentials for the session (provided via SDP by the peer) */
	IceRole mRole = IceRole::Controlling; /**< Role played by the agent for this session */
	State mState = State::Stopped;        /**< State of the session */
	uint64_t mTieBreaker = 0; /**< Random number used to resolve role conflicts (see paragraph 5.2 of the RFC 5245) */
	std::chrono::milliseconds mTa =
	    kIceDefaultTaDuration;                        /**< Duration of timer for sending connectivity checks in ms */
	OrtpEventType mEventType = 0;                     /** Value of the event to send */
	std::chrono::steady_clock::time_point mEventTime; /**< Time when an event must be sent */
	SockAddr mSockAddr; /**< STUN server address to use for the candidates gathering process */
	std::chrono::steady_clock::time_point mGatheringStartTs;
	std::chrono::steady_clock::time_point mGatheringEndTs;
	std::chrono::steady_clock::time_point mConnectivityChecksStartTs;
	std::vector<IceCandidate::Type> mDefaultCandidatesTypes;
	bool mCheckMessageIntegrity = true; /*set to false for backward compatibility only*/
	bool mSendEvent = false;            /**< Boolean value telling whether an event must be sent or not */
	uint8_t mMaxConnectivityChecks = kIceMaxNbCandidatePairs; /**< Configuration parameter to limit the number of
	                                    connectivity checks performed by the agent (default is 128) */
	std::chrono::seconds mKeepAliveTimeout = kIceDefaultKeepaliveTimeout; /**< Configuration parameter to define the
	                              timeout between each keepalive packets (default is 15s) */
	bool mForcedRelay = false;                /**< Force use of relay by modifying the local and reflexive
	           candidates */
	bool mTurnEnabled = false;                /**< TURN protocol enabled */
	bool mShortTurnRefresh = false;           /**< Short TURN refresh for tests */
	bool mDefaultCandidatesPreferIpv6 = true; /** < Whether ipv6 candidates should be preferred compared to their ipv4
	                                          equivalent as "default candidate" */
};

} // namespace mediastreamer::nat

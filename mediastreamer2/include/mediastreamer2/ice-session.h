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

#include "mediastreamer2/ice-candidate.h"
#include "mediastreamer2/ice-checklist.h"
#include "mediastreamer2/ice-constants.h"
#include "mediastreamer2/ice-credentials.h"
#include "mediastreamer2/ice-role.h"
#include "ortp/port.h"

namespace ms2 {

/**
 * Represents an ICE session.
 */
class IceSession {
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
	MS2_PUBLIC IceSession();

	/**
	 * Destroy a previously allocated ICE session.
	 * To be used when a media session using ICE is tore down.
	 */
	MS2_PUBLIC ~IceSession() = default;

	/**
	 * Add an ICE check list to an ICE session.
	 * @param checklist The check list to assign to the session
	 * @param index The index of the check list to add
	 */
	MS2_PUBLIC void addCheckList(const std::shared_ptr<IceCheckList> &checklist, size_t index);

	/**
	 * Tell whether ICE local candidates have been gathered for an ICE session or not.
	 * @return true if local candidates have been gathered for the session, false otherwise.
	 */
	[[nodiscard]] MS2_PUBLIC bool areCandidatesGathered() const;

	/**
	 * Check whether all the ICE check lists of the session includes a default candidate for each component ID in its
	 * remote candidates list.
	 */
	MS2_PUBLIC void checkMismatch() const;

	/**
	 * Choose the default candidates of an ICE session.
	 * This function is to be called at the end of the local candidates gathering process, before sending
	 * the SDP to the remote agent.
	 */
	MS2_PUBLIC void chooseDefaultLocalCandidates() const;

	/**
	 * Choose the default remote candidates of an ICE session.
	 * This function SHOULD not be used. Instead, the default remote candidates MUST be defined as default
	 * when creating them with IceCheckList::addRemoteCandidate().
	 * However, this function is used by mediastream for testing purpose.
	 */
	MS2_PUBLIC void chooseDefaultRemoteCandidates() const;

	/**
	 * Compute the foundations of the local candidates of an ICE session.
	 * This function is to be called at the end of the local candidates gathering process, before sending
	 * the SDP to the remote agent.
	 */
	MS2_PUBLIC void computeCandidatesFoundations() const;

	/**
	 * Dump an ICE session in the traces (debug function).
	 */
	MS2_PUBLIC void dump() const;

	/**
	 * Eliminate the redundant candidates of an ICE session.
	 * This function is to be called at the end of the local candidates gathering process, before sending
	 * the SDP to the remote agent.
	 */
	MS2_PUBLIC void eliminateRedundantCandidates() const;

	/**
	 * Enable forced relay for tests.
	 * @param enable A boolean value telling whether to force relay or not.
	 * The local and reflexive candidates are changed so that these paths do not work to force the use of the relay.
	 */
	MS2_PUBLIC void enableForcedRelay(bool enable) {
		mForcedRelay = enable;
	}

	/**
	 * Disable/enable strong message integrity check. Used for backward compatibility only.
	 * It is enabled by default.
	 * @param enable Boolean value telling whether to enable message integrity check or not.
	 */
	MS2_PUBLIC void enableMessageIntegrityCheck(bool enable) {
		mCheckMessageIntegrity = enable;
	}

	/**
	 * Enable short TURN refresh for tests.
	 * @param enable A boolean value telling whether to use short turn refresh.
	 * This changes the delay to send allocation refresh, create permission, and channel bind requests.
	 */
	MS2_PUBLIC void enableShortTurnRefresh(bool enable) {
		mShortTurnRefresh = enable;
	}

	/**
	 * Enable TURN protocol.
	 * @param enable A boolean value telling whether to enable TURN protocol or not.
	 */
	MS2_PUBLIC void enableTurn(bool enable);

	/**
	 * Gather ICE local candidates for an ICE session.
	 * @param ss The STUN server address
	 * @param ssLen The length of the STUN server address
	 * @return true if the gathering is in progress, false if no gathering is happening.
	 */
	MS2_PUBLIC bool gatherCandidates(const struct sockaddr *ss, socklen_t ssLen);

	/**
	 * Tell the average round trip time during the gathering process for an ICE session in milliseconds.
	 * @return std::nullopt if gathering has not been run, the average round trip time in milliseconds otherwise.
	 */
	[[nodiscard]] MS2_PUBLIC std::optional<std::chrono::milliseconds> getAverageGatheringRoundTripTime() const;

	/**
	 * Get the nth check list of an ICE session.
	 * @param n The index of the check list to access
	 * @return A pointer to the nth check list of the session if it exists, nullptr otherwise
	 */
	[[nodiscard]] MS2_PUBLIC std::shared_ptr<IceCheckList> getCheckList(size_t n) const;

	/**
	 * Tell the duration of the gathering process for an ICE session in ms.
	 * @return std::nullopt if gathering has not been run, the duration of the gathering process in milliseconds
	 * otherwise.
	 */
	[[nodiscard]] MS2_PUBLIC std::optional<std::chrono::milliseconds> getGatheringDuration() const;

	/**
	 * Get the timeout between each keepalive packet in seconds.
	 * @return The duration of the keepalive timeout in seconds
	 */
	[[nodiscard]] MS2_PUBLIC std::chrono::seconds getKeepAliveTimeout() const {
		return mKeepAliveTimeout;
	}

	/**
	 * Get the local credentials of an ICE session.
	 * @return A reference to the local credentials of the session
	 */
	[[nodiscard]] MS2_PUBLIC const IceCredentials &getLocalCredentials() const {
		return mLocalCredentials;
	}

	/**
	 * Get the number of check lists in an ICE session.
	 * @return The number of check lists in the ICE session
	 */
	[[nodiscard]] MS2_PUBLIC size_t getNbCheckLists() const;

	/**
	 * Get the number of losing candidate pairs for an ICE session.
	 * @return The number of losing candidate pairs for the session.
	 */
	[[nodiscard]] MS2_PUBLIC unsigned int getNbLosingPairs() const;

	/**
	 * Get the remote username fragment of an ICE session.
	 * @return A reference to the remote credentials of the session
	 */
	[[nodiscard]] MS2_PUBLIC const std::optional<IceCredentials> &getRemoteCredentials() const {
		return mRemoteCredentials;
	}

	/**
	 * Get the role of the agent for an ICE session.
	 * @return The role of the agent for the session
	 */
	[[nodiscard]] MS2_PUBLIC IceRole getRole() const {
		return mRole;
	}

	/**
	 * Get the role of the agent for an ICE session as a human-readable string.
	 * @return The role of the agent for the session as a human-readable string.
	 */
	[[nodiscard]] MS2_PUBLIC const std::string &getRoleStr() const;

	/**
	 * Get the state of an ICE session.
	 * @return The state of the session
	 */
	[[nodiscard]] MS2_PUBLIC State getState() const {
		return mState;
	}

	/**
	 * Tell whether an ICE session has at least one completed check list.
	 * @return true if the session has at least one completed check list, false otherwise
	 */
	[[nodiscard]] MS2_PUBLIC bool hasCompletedCheckList() const;

	/**
	 * Tell if remote credentials of an ICE session have changed or not.
	 * @param newCredentials The new remote credentials
	 * @return true if the remote credentials of the session have changed, false otherwise.
	 */
	[[nodiscard]] MS2_PUBLIC bool haveRemoteCredentialsChanged(const IceCredentials &newCredentials) const;

	/**
	 * Remove an ICE check list from an ICE session.
	 * @param checklistToRemove The check list to remove from the session
	 */
	MS2_PUBLIC void removeCheckList(const std::shared_ptr<IceCheckList> &checklistToRemove);

	/**
	 * Remove an ICE check list from an ICE session given its index.
	 * @param index The index of the check list in the ICE session
	 */
	MS2_PUBLIC void removeCheckList(size_t index);

	/**
	 * Reset an ICE session.
	 * It has the same effect as a session restart but also clears the local candidates.
	 * @param role The role of the agent after the session restart
	 */
	MS2_PUBLIC void reset(IceRole role);

	/**
	 * Restart an ICE session.
	 * @param role The role of the agent after the session restart
	 */
	MS2_PUBLIC void restart(IceRole role);

	/**
	 * Select ICE candidates that will be used and notified in the SDP.
	 * This function is to be used by the Controlling agent when ICE processing has finished.
	 */
	MS2_PUBLIC void selectCandidates() const;

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
	MS2_PUBLIC void setBaseForSrflxCandidates() const;

	/**
	 * Set the AF_INET/AF_INET6 preference for electing the default candidates, when both are available.
	 */
	MS2_PUBLIC void setDefaultCandidatesPreferIpv6(bool preferIpv6) {
		mDefaultCandidatesPreferIpv6 = preferIpv6;
	}

	/**
	 * Set the preferred type for default candidates, as defined in rfc5245#section-4.1.4.
	 **/
	MS2_PUBLIC void setDefaultCandidatesTypes(const std::vector<IceCandidate::Type> &candidatesTypes) {
		mDefaultCandidatesTypes = candidatesTypes;
	}

	/**
	 * Define the timeout between each keepalive packet in seconds.
	 * @param keepAliveTimeout The duration of the keepalive timeout in seconds
	 * The default keepalive timeout is set to 15 seconds.
	 */
	MS2_PUBLIC void setKeepAliveTimeout(std::chrono::seconds keepAliveTimeout);

	/**
	 * Set the local credentials of an ICE session.
	 * This method SHOULD not be used. However, it is used by mediastream for testing purpose to
	 * apply the same credentials for local and remote agents because the SDP exchange is bypassed.
	 */
	MS2_PUBLIC void setLocalCredentials(const IceCredentials &credentials) {
		mLocalCredentials = credentials;
	}

	/**
	 * Define the maximum number of connectivity checks that will be performed by the agent.
	 * @param value The maximum number of connectivity checks to perform
	 * This function is to be called just after the creation of the session, before any connectivity check is performed.
	 * The default number of connectivity checks is 128.
	 */
	MS2_PUBLIC void setMaxConnectivityChecks(const uint8_t value) {
		mMaxConnectivityChecks = value;
	}

	/**
	 * Set the remote credentials of an ICE session.
	 * @param credentials The remote credentials
	 * This function is to be called once the remote credentials have been received via SDP.
	 */
	MS2_PUBLIC void setRemoteCredentials(const IceCredentials &credentials) {
		mRemoteCredentials = credentials;
	}

	/**
	 * Set the role of the agent for an ICE session.
	 * @param role The role to set the session to
	 */
	MS2_PUBLIC void setRole(IceRole role);

	MS2_PUBLIC void setStunAuthRequestedCb(MSStunAuthRequestedCb cb, void *userdata);

	/**
	 * Set TURN CN when using TLS.
	 * @param cn The CN.
	 */
	MS2_PUBLIC void setTurnCn(const std::string &cn) const;

	/**
	 * Set TURN root certificate path when using TLS.
	 * @param rootCertificate The path of the root certificate.
	 */
	MS2_PUBLIC void setTurnRootCertificate(const std::string &rootCertificate) const;

	/**
	 * Set TURN transport.
	 * @param transportStr The transport that TURN should use (should be UDP, TCP or TLS).
	 */
	MS2_PUBLIC void setTurnTransport(const std::string &transportStr) const;

	/**
	 * Pair the local and the remote candidates for an ICE session and start sending connectivity checks.
	 */
	MS2_PUBLIC void startConnectivityChecks();

private:
	void computePairPriorities() const;
	[[nodiscard]] bool containsCheckList(const std::shared_ptr<IceCheckList> &checklist) const;
	[[nodiscard]] std::shared_ptr<IceCheckList> findCheckListFromState(IceCheckList::State state) const;
	[[nodiscard]] std::shared_ptr<IceCheckList> findCheckListGatheringCandidates() const;
	[[nodiscard]] std::shared_ptr<IceCheckList> findRunningCheckList() const;
	[[nodiscard]] std::shared_ptr<IceCheckList> findUnsuccessfulCheckList() const;
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
	[[nodiscard]] int getEventValue() const {
		return mEventValue;
	}
	[[nodiscard]] std::shared_ptr<IceCheckList> getFirstCheckList() const;
	[[nodiscard]] size_t getMaxConnectivityChecks() const {
		return mMaxConnectivityChecks;
	}

	[[nodiscard]] const IceUtils::SockAddr &getSockAddr() const {
		return mSockAddr;
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
	void programEventSending(int eventValue, std::chrono::milliseconds delay);
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

	std::array<std::shared_ptr<IceCheckList>, ICE_MAX_NB_CHECK_LISTS> mChecklists; /**< Table of IceChecklist structure
	                                                          pointers. Each element represents a media stream */
	MSStunAuthRequestedCb mStunAuthRequestedCb = nullptr; /**< Callback called when authentication is
	 requested */
	void *mStunAuthRequestedUserdata = nullptr;           /**< Userdata to pass to the STUN authentication requested
	              callback */
	IceCredentials mLocalCredentials; /**< Local credentials for the session (assigned during the session creation) */
	std::optional<IceCredentials> mRemoteCredentials =
	    std::nullopt;                     /**< Remote credentials for the session (provided via SDP by the peer) */
	IceRole mRole = IceRole::Controlling; /**< Role played by the agent for this session */
	State mState = State::Stopped;        /**< State of the session */
	uint64_t mTieBreaker = 0; /**< Random number used to resolve role conflicts (see paragraph 5.2 of the RFC 5245) */

	std::chrono::milliseconds mTa = ICE_DEFAULT_TA_DURATION; /**< Duration of timer for sending connectivity checks
	// in ms */
	int mEventValue = 0;                                     /** Value of the event to send */

	std::chrono::steady_clock::time_point mEventTime; /**< Time when an event must be sent */
	IceUtils::SockAddr mSockAddr; /**< STUN server address to use for the candidates gathering process */
	std::chrono::steady_clock::time_point mGatheringStartTs;
	std::chrono::steady_clock::time_point mGatheringEndTs;
	std::chrono::steady_clock::time_point mConnectivityChecksStartTs;
	std::vector<IceCandidate::Type> mDefaultCandidatesTypes;
	bool mCheckMessageIntegrity = true; /*set to false for backward compatibility only*/
	bool mSendEvent = false;            /**< Boolean value telling whether an event must be sent or not */
	uint8_t mMaxConnectivityChecks = ICE_MAX_NB_CANDIDATE_PAIRS; /**< Configuration parameter to limit the number of
	                                    connectivity checks performed by the agent (default is 100) */
	std::chrono::seconds mKeepAliveTimeout = ICE_DEFAULT_KEEPALIVE_TIMEOUT; /**< Configuration parameter to define the
	                              timeout between each keepalive packets (default is 15s) */
	bool mForcedRelay = false;                /**< Force use of relay by modifying the local and reflexive
	           candidates */
	bool mTurnEnabled = false;                /**< TURN protocol enabled */
	bool mShortTurnRefresh = false;           /**< Short TURN refresh for tests */
	bool mDefaultCandidatesPreferIpv6 = true; /** < Whether ipv6 candidates should be preferred compared to their ipv4
	                                          equivalent as "default candidate" */
};

} // namespace ms2

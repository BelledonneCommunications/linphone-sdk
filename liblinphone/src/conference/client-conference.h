/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
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

#ifndef _L_REMOTE_CONFERENCE_H_
#define _L_REMOTE_CONFERENCE_H_

#include "conference/conference.h"
#include "core/core-accessor.h"
#ifdef HAVE_ADVANCED_IM
#include "conference/encryption/client-ekt-manager.h"
#endif // HAVE_ADVANCED_IM

// =============================================================================

LINPHONE_BEGIN_NAMESPACE

class ClientConferenceEventHandler;
class Core;

class LINPHONE_PUBLIC ClientConference : public Conference, public ConferenceListenerInterface {
	friend class ClientChatRoom;

public:
	ClientConference(const std::shared_ptr<Core> &core,
	                 std::shared_ptr<CallSessionListener> listener,
	                 const std::shared_ptr<const ConferenceParams> params);
	virtual ~ClientConference();

	void initFromDb(const std::shared_ptr<Participant> &me,
	                const ConferenceId conferenceId,
	                const unsigned int lastNotifyId,
	                bool hasBeenLeft) override;
	void initWithFocus(const std::shared_ptr<const Address> &focusAddr,
	                   const std::shared_ptr<CallSession> &focusSession,
	                   SalCallOp *op = nullptr,
	                   ConferenceListener *confListener = nullptr);
	int inviteAddresses(const std::list<std::shared_ptr<Address>> &addresses,
	                    const LinphoneCallParams *params) override;
	bool dialOutAddresses(const std::list<std::shared_ptr<Address>> &addressList) override;
	bool addParticipant(const std::shared_ptr<ParticipantInfo> &info) override;
	bool addParticipants(const std::list<std::shared_ptr<Call>> &call) override;
	bool addParticipants(const std::list<std::shared_ptr<Address>> &addresses) override;
	bool addParticipant(const std::shared_ptr<Call> call) override;
	bool addParticipant(const std::shared_ptr<Address> &participantAddress) override;
	std::shared_ptr<ParticipantDevice> createParticipantDevice(std::shared_ptr<Participant> &participant,
	                                                           const std::shared_ptr<Call> &call) override;
	bool finalizeParticipantAddition(std::shared_ptr<Call> call) override;

	int removeParticipant(const std::shared_ptr<CallSession> &session, const bool preserveSession) override;
	int removeParticipant(const std::shared_ptr<Address> &addr) override;
	bool removeParticipant(const std::shared_ptr<Participant> &participant) override;
	int terminate() override;
	void init(SalCallOp *op = nullptr, ConferenceListener *confListener = nullptr) override;
	void finalizeCreation() override;

	int enter() override;
	void join(const std::shared_ptr<Address> &participantAddress) override;
	void leave() override;
	bool isIn() const override;
	const std::shared_ptr<Address> getOrganizer() const override;

	int startRecording(const std::string &path) override;

	void setLocalParticipantStreamCapability(const LinphoneMediaDirection &direction,
	                                         const LinphoneStreamType type) override;

	AudioControlInterface *getAudioControlInterface() const override;
	VideoControlInterface *getVideoControlInterface() const override;
	AudioStream *getAudioStream() override;

	void multipartNotifyReceived(const std::shared_ptr<Event> &notifyLev, const Content &content);
	void notifyReceived(const std::shared_ptr<Event> &notifyLev, const Content &content);

	int participantDeviceMediaCapabilityChanged(const std::shared_ptr<CallSession> &session) override;
	int participantDeviceMediaCapabilityChanged(const std::shared_ptr<Address> &addr) override;
	int participantDeviceMediaCapabilityChanged(const std::shared_ptr<Participant> &participant,
	                                            const std::shared_ptr<ParticipantDevice> &device) override;
	int participantDeviceSsrcChanged(const std::shared_ptr<CallSession> &session,
	                                 const LinphoneStreamType type,
	                                 uint32_t ssrc) override;
	int participantDeviceSsrcChanged(const std::shared_ptr<CallSession> &session,
	                                 uint32_t audioSsrc,
	                                 uint32_t videoSsrc) override;

	int participantDeviceAlerting(const std::shared_ptr<CallSession> &session) override;
	int participantDeviceAlerting(const std::shared_ptr<Participant> &participant,
	                              const std::shared_ptr<ParticipantDevice> &device) override;
	int participantDeviceJoined(const std::shared_ptr<CallSession> &session) override;
	int participantDeviceJoined(const std::shared_ptr<Participant> &participant,
	                            const std::shared_ptr<ParticipantDevice> &device) override;
	int participantDeviceLeft(const std::shared_ptr<CallSession> &session) override;
	int participantDeviceLeft(const std::shared_ptr<Participant> &participant,
	                          const std::shared_ptr<ParticipantDevice> &device) override;

	int getParticipantDeviceVolume(const std::shared_ptr<ParticipantDevice> &device) override;

	void onStateChanged(ConferenceInterface::State state) override;
	void onParticipantAdded(const std::shared_ptr<ConferenceParticipantEvent> &event,
	                        const std::shared_ptr<Participant> &participant) override;
	void onParticipantRemoved(const std::shared_ptr<ConferenceParticipantEvent> &event,
	                          const std::shared_ptr<Participant> &participant) override;

	void onParticipantDeviceAdded(const std::shared_ptr<ConferenceParticipantDeviceEvent> &event,
	                              const std::shared_ptr<ParticipantDevice> &device) override;
	void onParticipantDeviceRemoved(const std::shared_ptr<ConferenceParticipantDeviceEvent> &event,
	                                const std::shared_ptr<ParticipantDevice> &device) override;
	void onParticipantDeviceStateChanged(const std::shared_ptr<ConferenceParticipantDeviceEvent> &event,
	                                     const std::shared_ptr<ParticipantDevice> &device) override;
	void onParticipantDeviceMediaAvailabilityChanged(const std::shared_ptr<ConferenceParticipantDeviceEvent> &event,
	                                                 const std::shared_ptr<ParticipantDevice> &device) override;
	void onAvailableMediaChanged(const std::shared_ptr<ConferenceAvailableMediaEvent> &event) override;

	void onParticipantDeviceScreenSharingChanged(const std::shared_ptr<ConferenceParticipantDeviceEvent> &event,
	                                             const std::shared_ptr<ParticipantDevice> &device) override;

	void setParticipantAdminStatus(const std::shared_ptr<Participant> &participant, bool isAdmin) override;

	void onSubjectChanged(const std::shared_ptr<ConferenceSubjectEvent> &event) override;
	void onParticipantSetRole(const std::shared_ptr<ConferenceParticipantEvent> &event,
	                          const std::shared_ptr<Participant> &participant) override;
	void onParticipantSetAdmin(const std::shared_ptr<ConferenceParticipantEvent> &event,
	                           const std::shared_ptr<Participant> &participant) override;
	bool update(const ConferenceParamsInterface &params) override;

	void notifyStateChanged(ConferenceInterface::State state) override;

	void setMainSession(const std::shared_ptr<CallSession> &session);
	std::shared_ptr<Call> getCall() const override;

	virtual bool delayTimerExpired() const override;
	bool isSubscriptionUnderWay() const override;

	void onConferenceCreated(const std::shared_ptr<Address> &addr) override;
	void onConferenceKeywordsChanged(const std::vector<std::string> &keywords) override;
	void onConferenceTerminated(const std::shared_ptr<Address> &addr) override;
	void onParticipantsCleared() override;
	void onSecurityEvent(const std::shared_ptr<ConferenceSecurityEvent> &event) override;
	void onFirstNotifyReceived(const std::shared_ptr<Address> &addr) override;
	void onFullStateReceived() override;

	void onEphemeralModeChanged(const std::shared_ptr<ConferenceEphemeralMessageEvent> &event) override;
	void onEphemeralMessageEnabled(const std::shared_ptr<ConferenceEphemeralMessageEvent> &event) override;
	void onEphemeralLifetimeChanged(const std::shared_ptr<ConferenceEphemeralMessageEvent> &event) override;

#ifdef HAVE_ADVANCED_IM
	std::shared_ptr<ClientConferenceEventHandler> mEventHandler;
#endif // HAVE_ADVANCED_IM

	void requestFullState();
	void sendPendingMessages();

	/* Report the csrc included in the video stream, so that we can notify who is presented on the screen.*/
	void notifyDisplayedSpeaker(uint32_t csrc);
	void notifyLouderSpeaker(uint32_t ssrc);

	void setConferenceId(const ConferenceId &conferenceId);
	void confirmJoining(SalCallOp *op);
	void attachCall(const std::shared_ptr<CallSession> &session);
	AbstractChatRoom::SecurityLevel
	getSecurityLevelExcept(const std::shared_ptr<ParticipantDevice> &ignoredDevice) const;

	void createFocus(const std::shared_ptr<const Address> &focusAddr,
	                 const std::shared_ptr<CallSession> focusSession = nullptr);

	std::pair<bool, LinphoneMediaDirection> getMainStreamVideoDirection(const std::shared_ptr<CallSession> &session,
	                                                                    bool localIsOfferer,
	                                                                    bool useLocalParams) const override;

	void onCallSessionSetReleased(const std::shared_ptr<CallSession> &session) override;
	void onCallSessionSetTerminated(const std::shared_ptr<CallSession> &session) override;
	void onCallSessionStateChanged(const std::shared_ptr<CallSession> &session,
	                               CallSession::State state,
	                               const std::string &message) override;
	void setUtf8Subject(const std::string &subject) override;

	void subscribe(bool addToListEventHandler, bool unsubscribeFirst = true);
	void unsubscribe();

#ifdef HAVE_ADVANCED_IM
	std::shared_ptr<LinphonePrivate::ClientEktManager> getClientEktManager() const;
#endif // HAVE_ADVANCED_IM

protected:
	void onCallSessionTransferStateChanged(const std::shared_ptr<CallSession> &session,
	                                       CallSession::State state) override;

private:
	std::shared_ptr<ClientConferenceEventHandler> getEventHandler() const;
	void acceptSession(const std::shared_ptr<CallSession> &session);
	std::shared_ptr<CallSession> createSessionTo(const std::shared_ptr<const Address> &sessionTo);
	std::shared_ptr<CallSession> createSession();
	std::shared_ptr<CallSession> getMainSession() const override;
	std::shared_ptr<ConferenceInfo> createConferenceInfo() const override;
	void updateAndSaveConferenceInformations();
	bool focusIsReady() const;
	bool transferToFocus(std::shared_ptr<Call> call);
	void reset();
	void endConference();

	void onFocusCallStateChanged(CallSession::State state, const std::string &message);
	void onPendingCallStateChanged(std::shared_ptr<Call> call, CallSession::State callState);
	std::list<Address> cleanAddressesList(const std::list<std::shared_ptr<Address>> &addresses) const;

	void configure(SalCallOp *op) override;

	bool hasBeenLeft() const;

	void createEventHandler(ConferenceListener *confListener = nullptr, bool addToListEventHandler = false) override;
	void initializeHandlers(ConferenceListener *confListener, bool addToListEventHandler);

	void handleRefer(SalReferOp *op,
	                 const std::shared_ptr<LinphonePrivate::Address> &referAddr,
	                 const std::string method) override;
	bool sessionParamsAllowThumbnails() const override;

	void callFocus();

	bool mFinalized = false;
	bool mScheduleUpdate = false;
	bool mFullStateUpdate = false;
	bool mFullStateReceived = false;
	std::string mPendingSubject;
	std::shared_ptr<Participant> mFocus;
	std::list<std::shared_ptr<Call>> mPendingCalls;
	std::list<std::shared_ptr<Call>> mTransferingCalls;
	MediaSessionParams *mJoiningParams = nullptr;

	uint32_t mDisplayedSpeaker = 0;
	uint32_t mLouderSpeaker = 0;
	uint32_t mLastNotifiedSsrc = 0;

#ifdef HAVE_ADVANCED_IM
	// end-to-end encryption
	std::shared_ptr<LinphonePrivate::ClientEktManager> mClientEktManager = nullptr;
#endif // HAVE_ADVANCED_IM

	// end-to-end encryption
	std::vector<uint8_t> mEktKey;
};

LINPHONE_END_NAMESPACE

#endif // ifndef _L_REMOTE_CONFERENCE_H_

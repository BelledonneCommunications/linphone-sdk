/*
 * conference.h
 * Copyright (C) 2010-2017 Belledonne Communications SARL
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef _CONFERENCE_H_
#define _CONFERENCE_H_

#include "linphone/types.h"

#include "address/address.h"
#include "call/call-listener.h"
#include "conference/conference-interface.h"
#include "conference/params/call-session-params.h"
#include "conference/participant.h"
#include "conference/session/call-session-listener.h"

// =============================================================================

LINPHONE_BEGIN_NAMESPACE

class CallSessionPrivate;

class LINPHONE_PUBLIC Conference : public ConferenceInterface, public CallSessionListener {
	friend class CallSessionPrivate;

public:
	virtual ~Conference() = default;

	std::shared_ptr<Participant> getActiveParticipant () const;
	std::shared_ptr<Participant> getMe () const { return me; }

	LinphoneCore * getCore () const { return core; }

	std::shared_ptr<Participant> findParticipant (const Address &addr) const;
	std::shared_ptr<Participant> findParticipant (const std::shared_ptr<const CallSession> &session) const;

public:
	/* ConferenceInterface */
	void addParticipant (const Address &addr, const CallSessionParams *params, bool hasMedia) override;
	void addParticipants (const std::list<Address> &addresses, const CallSessionParams *params, bool hasMedia) override;
	bool canHandleParticipants () const override;
	const Address *getConferenceAddress () const override;
	int getNbParticipants () const override;
	std::list<std::shared_ptr<Participant>> getParticipants () const override;
	const std::string &getSubject () const override;
	void join () override;
	void leave () override;
	void removeParticipant (const std::shared_ptr<const Participant> &participant) override;
	void removeParticipants (const std::list<std::shared_ptr<Participant>> &participants) override;
	void setSubject (const std::string &subject) override;

private:
	/* CallSessionListener */
	void onAckBeingSent (const std::shared_ptr<const CallSession> &session, LinphoneHeaders *headers) override;
	void onAckReceived (const std::shared_ptr<const CallSession> &session, LinphoneHeaders *headers) override;
	void onCallSessionAccepted (const std::shared_ptr<const CallSession> &session) override;
	void onCallSessionSetReleased (const std::shared_ptr<const CallSession> &session) override;
	void onCallSessionSetTerminated (const std::shared_ptr<const CallSession> &session) override;
	void onCallSessionStateChanged (const std::shared_ptr<const CallSession> &session, LinphoneCallState state, const std::string &message) override;
	void onCheckForAcceptation (const std::shared_ptr<const CallSession> &session) override;
	void onIncomingCallSessionStarted (const std::shared_ptr<const CallSession> &session) override;
	void onEncryptionChanged (const std::shared_ptr<const CallSession> &session, bool activated, const std::string &authToken) override;
	void onStatsUpdated (const LinphoneCallStats *stats) override;
	void onResetCurrentSession (const std::shared_ptr<const CallSession> &session) override;
	void onSetCurrentSession (const std::shared_ptr<const CallSession> &session) override;
	void onFirstVideoFrameDecoded (const std::shared_ptr<const CallSession> &session) override;
	void onResetFirstVideoFrameDecoded (const std::shared_ptr<const CallSession> &session) override;

protected:
	explicit Conference (LinphoneCore *core, const Address &myAddress, CallListener *listener = nullptr);

	bool isMe (const Address &addr) const ;

protected:
	LinphoneCore *core = nullptr;
	CallListener *callListener = nullptr;

	std::shared_ptr<Participant> activeParticipant;
	std::shared_ptr<Participant> me;
	std::list<std::shared_ptr<Participant>> participants;
	Address conferenceAddress;
	std::string subject;

private:
	L_DISABLE_COPY(Conference);
};

LINPHONE_END_NAMESPACE

#endif // ifndef _CONFERENCE_H_

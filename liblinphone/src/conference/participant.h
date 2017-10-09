/*
 * participant.h
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

#ifndef _PARTICIPANT_H_
#define _PARTICIPANT_H_

#include "address/address.h"

#include "object/object.h"
#include "conference/params/call-session-params.h"

// =============================================================================

LINPHONE_BEGIN_NAMESPACE

class ClientGroupChatRoom;
class ParticipantPrivate;

class Participant : public Object {
	friend class Call;
	friend class CallPrivate;
	friend class ClientGroupChatRoom;
	friend class ClientGroupChatRoomPrivate;
	friend class Conference;
	friend class LocalConference;
	friend class LocalConferenceEventHandler;
	friend class LocalConferenceEventHandlerPrivate;
	friend class MediaSessionPrivate;
	friend class RemoteConference;

public:
	Participant (const Address &address);
	Participant (Address &&address);

	const Address& getAddress () const;

	bool isAdmin () const;
	void setAdmin (bool isAdmin);

private:
	L_DECLARE_PRIVATE(Participant);
	L_DISABLE_COPY(Participant);
};

std::ostream & operator<< (std::ostream &strm, const std::shared_ptr<Participant> &participant);

LINPHONE_END_NAMESPACE

#endif // ifndef _PARTICIPANT_H_

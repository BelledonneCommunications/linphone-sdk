/*
 * conference-interface.h
 * Copyright (C) 2017  Belledonne Communications SARL
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _CONFERENCE_INTERFACE_H_
#define _CONFERENCE_INTERFACE_H_

#include <list>
#include <memory>

#include "address/address.h"
#include "conference/participant.h"
#include "conference/params/call-session-params.h"

// =============================================================================

LINPHONE_BEGIN_NAMESPACE

class ConferenceInterface {
public:
	virtual std::shared_ptr<Participant> addParticipant (const Address &addr, const std::shared_ptr<CallSessionParams> params, bool hasMedia) = 0;
	virtual void addParticipants (const std::list<Address> &addresses, const std::shared_ptr<CallSessionParams> params, bool hasMedia) = 0;
	virtual const std::string& getId () const = 0;
	virtual int getNbParticipants () const = 0;
	virtual std::list<std::shared_ptr<Participant>> getParticipants () const = 0;
	virtual void removeParticipant (const std::shared_ptr<Participant> participant) = 0;
	virtual void removeParticipants (const std::list<std::shared_ptr<Participant>> participants) = 0;
};

LINPHONE_END_NAMESPACE

#endif // ifndef _CONFERENCE_INTERFACE_H_

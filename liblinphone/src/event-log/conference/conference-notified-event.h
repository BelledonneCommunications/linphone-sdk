/*
 * conference-notified-event.h
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

#ifndef _CONFERENCE_NOTIFIED_EVENT_H_
#define _CONFERENCE_NOTIFIED_EVENT_H_

#include "conference-event.h"

// =============================================================================

LINPHONE_BEGIN_NAMESPACE

class Address;
class ConferenceNotifiedEventPrivate;

class LINPHONE_PUBLIC ConferenceNotifiedEvent : public EventLog {
public:
	ConferenceNotifiedEvent (Type type, std::time_t time, const Address &conferenceAddress);
	ConferenceNotifiedEvent (const ConferenceNotifiedEvent &src);

	ConferenceNotifiedEvent &operator= (const ConferenceNotifiedEvent &src);

	const Address &getConferenceAddress () const;

protected:
	ConferenceNotifiedEvent (ConferenceNotifiedEventPrivate &p, Type type, std::time_t time, const Address &conferenceAddress);

private:
	L_DECLARE_PRIVATE(ConferenceNotifiedEvent);
};

LINPHONE_END_NAMESPACE

#endif // ifndef _CONFERENCE_NOTIFIED_EVENT_H_

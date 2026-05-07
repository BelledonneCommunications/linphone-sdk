/*
 * Copyright (c) 2010-2026 Belledonne Communications SARL.
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

#ifndef _L_CONFERENCE_ALTERNATIVE_ADDRESS_EVENT_H_
#define _L_CONFERENCE_ALTERNATIVE_ADDRESS_EVENT_H_

#include <string>

#include "conference-notified-event.h"

// =============================================================================

LINPHONE_BEGIN_NAMESPACE

class Address;
class ConferenceAlternativeAddressEventPrivate;

class LINPHONE_PUBLIC ConferenceAlternativeAddressEvent : public ConferenceNotifiedEvent {
	friend class Conference;

public:
	ConferenceAlternativeAddressEvent(time_t creationTime,
	                                  const ConferenceId &conferenceId,
	                                  const std::shared_ptr<Address> &alternativeAddress);

	const std::shared_ptr<Address> &getAlternativeAddress() const;

private:
	L_DECLARE_PRIVATE(ConferenceAlternativeAddressEvent);
	L_DISABLE_COPY(ConferenceAlternativeAddressEvent);
};

LINPHONE_END_NAMESPACE

#endif // ifndef _L_CONFERENCE_ALTERNATIVE_ADDRESS_EVENT_H_

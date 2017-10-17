/*
 * event-log.h
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

#ifndef _EVENT_LOG_H_
#define _EVENT_LOG_H_

#include <ctime>

#include "linphone/enums/event-log-enums.h"
#include "linphone/utils/enum-generator.h"

#include "object/clonable-object.h"

// =============================================================================

LINPHONE_BEGIN_NAMESPACE

class EventLogPrivate;

class LINPHONE_PUBLIC EventLog : public ClonableObject {
	friend class MainDb;

public:
	L_DECLARE_ENUM(Type, L_ENUM_VALUES_EVENT_LOG_TYPE);

	EventLog ();
	EventLog (const EventLog &src);

	EventLog &operator= (const EventLog &src);

	Type getType () const;
	std::time_t getTime () const;

protected:
	EventLog (EventLogPrivate &p, Type type, const std::time_t &time);

private:
	L_DECLARE_PRIVATE(EventLog);
};

LINPHONE_END_NAMESPACE

#endif // ifndef _EVENT_LOG_H_

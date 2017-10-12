/*
 * remote-conference-event-handler-p.h
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

#ifndef _REMOTE_CONFERENCE_EVENT_HANDLER_P_H_
#define _REMOTE_CONFERENCE_EVENT_HANDLER_P_H_

#include "address/address.h"
#include "object/object-p.h"
#include "remote-conference-event-handler.h"

// =============================================================================

LINPHONE_BEGIN_NAMESPACE

class RemoteConferenceEventHandlerPrivate : public ObjectPrivate {
public:
	inline unsigned int getLastNotify () const {
		return lastNotify;
	};

private:
	LinphoneCore *core = nullptr;
	ConferenceListener *listener = nullptr;
	Address confAddress;
	LinphoneEvent *lev = nullptr;
	unsigned int lastNotify = 0;

	L_DECLARE_PUBLIC(RemoteConferenceEventHandler);
};

LINPHONE_END_NAMESPACE

#endif // ifndef _REMOTE_CONFERENCE_EVENT_HANDLER_P_H_

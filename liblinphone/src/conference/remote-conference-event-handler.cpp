/*
 * remote-conference-event-handler.cpp
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

#include "remote-conference-event-handler.h"
#include "logger/logger.h"
#include "object/object-p.h"

#include "private.h"

#include "xml/conference-info.h"

// =============================================================================

using namespace std;

LINPHONE_BEGIN_NAMESPACE

using namespace Xsd::ConferenceInfo;

class RemoteConferenceEventHandlerPrivate : public ObjectPrivate {
public:
	LinphoneCore *core = nullptr;
	ConferenceListener *listener = nullptr;
	Address confAddress;
	LinphoneEvent *lev = nullptr;
};

// -----------------------------------------------------------------------------

RemoteConferenceEventHandler::RemoteConferenceEventHandler(LinphoneCore *core, ConferenceListener *listener)
	: Object(*new RemoteConferenceEventHandlerPrivate) {
	L_D();
	xercesc::XMLPlatformUtils::Initialize();
	d->core = core;
	d->listener = listener;
}

RemoteConferenceEventHandler::~RemoteConferenceEventHandler() {
	xercesc::XMLPlatformUtils::Terminate();
}

// -----------------------------------------------------------------------------

void RemoteConferenceEventHandler::subscribe(const Address &addr) {
	L_D();
	d->confAddress = addr;
	LinphoneAddress *lAddr = linphone_address_new(d->confAddress.asString().c_str());
	d->lev = linphone_core_create_subscribe(d->core, lAddr, "conference", 600);
	linphone_address_unref(lAddr);
	linphone_event_set_internal(d->lev, TRUE);
	linphone_event_set_user_data(d->lev, this);
	linphone_event_send_subscribe(d->lev, nullptr);
}

void RemoteConferenceEventHandler::unsubscribe() {
	L_D();
	if (d->lev) {
		linphone_event_terminate(d->lev);
		d->lev = nullptr;
	}
}

void RemoteConferenceEventHandler::notifyReceived(string xmlBody) {
	L_D();
	lInfo() << "NOTIFY received for conference " << d->confAddress.asString();
	istringstream data(xmlBody);
	unique_ptr<ConferenceType> confInfo = parseConferenceInfo(data, Xsd::XmlSchema::Flags::dont_validate);
	Address cleanedConfAddress = d->confAddress;
	cleanedConfAddress.setPort(0);
	if (confInfo->getEntity() == cleanedConfAddress.asString()) {
		for (const auto &user : confInfo->getUsers()->getUser()) {
			LinphoneAddress *cAddr = linphone_core_interpret_url(d->core, user.getEntity()->c_str());
			char *cAddrStr = linphone_address_as_string(cAddr);
			Address addr(cAddrStr);
			bctbx_free(cAddrStr);
			if (user.getState() == "deleted")
				d->listener->onParticipantRemoved(addr);
			else {
				bool isAdmin = false;
				if (user.getRoles()) {
					for (const auto &entry : user.getRoles()->getEntry()) {
						if (entry == "admin") {
							isAdmin = true;
							break;
						}
					}
				}
				if (user.getState() == "full")
					d->listener->onParticipantAdded(addr);
				d->listener->onParticipantSetAdmin(addr, isAdmin);
			}
			linphone_address_unref(cAddr);
		}
	}
}

// -----------------------------------------------------------------------------

const Address &RemoteConferenceEventHandler::getConfAddress() {
	L_D();
	return d->confAddress;
}

LINPHONE_END_NAMESPACE

/*
 * Copyright (c) 2010-2023 Belledonne Communications SARL.
 *
 * This file is part of the Liblinphone EKT server plugin
 * (see https://gitlab.linphone.org/BC/private/gc).
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

#include "ekt-server-main.h"

// =============================================================================

using namespace std;
using namespace linphone;

// -----------------------------------------------------------------------------

shared_ptr<EktServerPlugin::ServerEktManager>
EktServerPlugin::EktServerMain::findServerEktManager(const shared_ptr<const Address> &conferenceAddress) {
	if (auto search = mConferenceByAddress.find(conferenceAddress); search != mConferenceByAddress.end()) {
		auto conf = search->second;
		if (conf != nullptr) {
			auto &sem = conf->getData<ServerEktManager>(kDataKey);
			return sem.shared_from_this();
		}
	}
	return nullptr;
}

void EktServerPlugin::EktServerMain::onSubscribeReceived(const shared_ptr<Core> &core,
                                                         const shared_ptr<Event> &linphoneEvent,
                                                         const string &subscribeEvent,
                                                         const shared_ptr<const Content> &body) {
	if (linphoneEvent->getName() == "ekt") {
		auto confAddress = linphoneEvent->getToAddress();
		auto ektManager = findServerEktManager(confAddress);
		auto conference = core->searchConference(confAddress);
		shared_ptr<ParticipantDevice> participantDevice = nullptr;
		if (conference) {
			auto deviceList = conference->getParticipantDeviceList();
			for (auto device : deviceList) {
				if (device->getAddress()->equal(linphoneEvent->getRemoteContact())) {
					participantDevice = device;
				}
			}
			if (!ektManager) {
				mConferenceByAddress.insert(make_pair(conference->getConferenceAddress(), conference));
				ektManager = make_shared<ServerEktManager>(conference);
				conference->setData(kDataKey, *ektManager.get());
				conference->addListener(ektManager);
			}
		}
		if (ektManager) ektManager->subscribeReceived(linphoneEvent, participantDevice);
	}
}

void EktServerPlugin::EktServerMain::onPublishReceived(const shared_ptr<Core> &core,
                                                       const shared_ptr<Event> &linphoneEvent,
                                                       const string &publishEvent,
                                                       const shared_ptr<const Content> &body) {
	if (linphoneEvent->getName() == "ekt") {
		auto confAddress = linphoneEvent->getToAddress();
		auto ektManager = findServerEktManager(confAddress);
		if (ektManager) ektManager->publishReceived(linphoneEvent, body);
	}
}

void EktServerPlugin::EktServerMain::onConferenceStateChanged(const shared_ptr<Core> &core,
                                                              const shared_ptr<Conference> &conference,
                                                              Conference::State state) {
	try {
		shared_ptr<ServerEktManager> sem = conference->getData<shared_ptr<ServerEktManager>>(kDataKey);
		switch (state) {
			case Conference::State::TerminationPending:
				if (sem) {
					conference->removeListener(sem);
					mConferenceByAddress.erase(conference->getConferenceAddress());
				}
				break;
			default:
				break;
		}
	} catch (std::out_of_range) {
	}
}

void EktServerPlugin::EktServerMain::onGlobalStateChanged(const std::shared_ptr<linphone::Core> &core,
                                                          linphone::GlobalState state,
                                                          const std::string &message) {
	switch (state) {
		case GlobalState::Shutdown:
			clear(core);
			break;
		default:
			break;
	}
}

void EktServerPlugin::EktServerMain::onNetworkReachable(const std::shared_ptr<linphone::Core> &core, bool reachable) {
	if (!reachable) clear(core);
}

void EktServerPlugin::EktServerMain::clear(const std::shared_ptr<linphone::Core> &core) {
	for (auto [conferenceAddress, conference] : mConferenceByAddress) {
		conference->removeListener(conference->getData<shared_ptr<ServerEktManager>>(kDataKey));
	}
	mConferenceByAddress.clear();
	core->removeListener(this->shared_from_this());
}
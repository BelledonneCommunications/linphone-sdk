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
		if (ektManager) ektManager->subscribeReceived(linphoneEvent);
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
	shared_ptr<ServerEktManager> serverEktManager = nullptr;
	switch (state) {
		case Conference::State::CreationPending:
			mConferenceByAddress.insert(make_pair(conference->getConferenceAddress(), conference));
			serverEktManager = make_shared<ServerEktManager>(conference);
			conference->setData(kDataKey, *serverEktManager.get());
			conference->addListener(serverEktManager);
			break;
		case Conference::State::TerminationPending:
			conference->removeListener(conference->getData<shared_ptr<ServerEktManager>>(kDataKey));
			mConferenceByAddress.erase(conference->getConferenceAddress());
			break;
		default:
			break;
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
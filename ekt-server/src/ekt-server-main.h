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

#include <map>

#include "linphone++/conference.hh"
#include "linphone++/core_listener.hh"

#include "server-ekt-manager.h"

namespace EktServerPlugin {

class AddressCompare {
public:
	bool operator()(const std::shared_ptr<const linphone::Address> &address,
	                const std::shared_ptr<const linphone::Address> &other) const {
		return address->lesser(other);
	}
};

class EktServerMain : public std::enable_shared_from_this<EktServerMain>, public linphone::CoreListener {
public:
	void onSubscribeReceived(const std::shared_ptr<linphone::Core> &core,
	                         const std::shared_ptr<linphone::Event> &linphoneEvent,
	                         const std::string &subscribeEvent,
	                         const std::shared_ptr<const linphone::Content> &body) override;
	void onPublishReceived(const std::shared_ptr<linphone::Core> &core,
	                       const std::shared_ptr<linphone::Event> &linphoneEvent,
	                       const std::string &publishEvent,
	                       const std::shared_ptr<const linphone::Content> &body) override;

	void onConferenceStateChanged(const std::shared_ptr<linphone::Core> &core,
	                              const std::shared_ptr<linphone::Conference> &conference,
	                              linphone::Conference::State state) override;
	void onGlobalStateChanged(const std::shared_ptr<linphone::Core> &core,
	                          linphone::GlobalState state,
	                          const std::string &message) override;
	void onNetworkReachable(const std::shared_ptr<linphone::Core> &core, bool reachable);
	void clear(const std::shared_ptr<linphone::Core> &core);

private:
	std::shared_ptr<ServerEktManager>
	findServerEktManager(const std::shared_ptr<const linphone::Address> &localConferenceAddress);

	std::map<const std::shared_ptr<const linphone::Address>, std::shared_ptr<linphone::Conference>, AddressCompare>
	    mConferenceByAddress;

	const char *kDataKey = "ServerEktManager";
};

} // namespace EktServerPlugin
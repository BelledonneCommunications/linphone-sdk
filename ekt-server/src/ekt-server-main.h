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

#include <unordered_map>

#include "linphone++/conference.hh"
#include "linphone++/core_listener.hh"

#include "server-ekt-manager.h"

namespace EktServerPlugin {

class EktServerMain : public std::enable_shared_from_this<EktServerMain>, public linphone::CoreListener {
public:
	EktServerMain(const std::shared_ptr<linphone::Core> &core);
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
	void clear();

private:
	std::shared_ptr<ServerEktManager>
	findServerEktManager(const std::shared_ptr<const linphone::Address> &localConferenceAddress);

	struct ConferenceAddressHash {
		size_t operator()(const std::shared_ptr<const linphone::Address> &address) const {
			auto tmp = address->clone();
			tmp->removeUriParam("gr");
			return std::hash<std::string>()(tmp->asStringUriOnlyOrdered());
		}
	};

	struct ConferenceAddressEqual {
		size_t operator()(const std::shared_ptr<const linphone::Address> &address1,
		                  const std::shared_ptr<const linphone::Address> &address2) const {
			auto tmp1 = address1->clone();
			tmp1->removeUriParam("gr");
			auto tmp2 = address2->clone();
			tmp2->removeUriParam("gr");
			return tmp1->equal(tmp2);
		}
	};

	std::unordered_map<const std::shared_ptr<const linphone::Address>,
	                   std::shared_ptr<linphone::Conference>,
	                   ConferenceAddressHash,
	                   ConferenceAddressEqual>
	    mConferenceByAddress;

	const char *kDataKey = "ServerEktManager";
	std::shared_ptr<linphone::Core> mCore;
};

} // namespace EktServerPlugin

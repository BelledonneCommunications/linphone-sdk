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

#include "bctoolbox/crypto.hh"

#include "linphone++/address.hh"
#include "linphone++/conference.hh"
#include "linphone++/conference_listener.hh"
#include "linphone++/content.hh"
#include "linphone++/ekt_info.hh"
#include "linphone++/event.hh"
#include "linphone++/event_listener.hh"

// =============================================================================

namespace EktServerPlugin {

class ServerEktManager : public linphone::ConferenceListener, public std::enable_shared_from_this<ServerEktManager> {
public:
	explicit ServerEktManager(const std::shared_ptr<linphone::Conference> &localConf);
	ServerEktManager(const ServerEktManager &serverEktManager) = delete;

	void onParticipantDeviceStateChanged(const std::shared_ptr<linphone::Conference> &conference,
	                                     const std::shared_ptr<const linphone::ParticipantDevice> &device,
	                                     linphone::ParticipantDevice::State state) override;
	void onAllowedParticipantListChanged(const std::shared_ptr<linphone::Conference> &conference) override;

	int subscribeReceived(const std::shared_ptr<linphone::Event> &ev,
	                      const std::shared_ptr<linphone::ParticipantDevice> &device);
	void sendNotifyAcceptedEkt(const std::shared_ptr<linphone::Event> &ev) const;
	void sendNotify(const std::shared_ptr<linphone::Event> &ev,
	                const std::shared_ptr<const linphone::Address> &from,
	                const std::shared_ptr<linphone::Buffer> &cipher,
	                const std::list<std::shared_ptr<const linphone::Address>> &addresses) const;
	void
	sendNotifyWithParticipantDeviceList(const std::shared_ptr<linphone::Event> &ev,
	                                    const std::list<std::shared_ptr<const linphone::Address>> &addresses) const;
	void publishReceived(const std::shared_ptr<linphone::Event> &ev,
	                     const std::shared_ptr<const linphone::Content> &content);
	void publishReceived(const std::shared_ptr<linphone::Event> &ev,
	                     const std::shared_ptr<const linphone::EktInfo> &ei);

	void generateSSpi();

	void clearData();

private:
	class ParticipantDeviceContext : public linphone::EventListener,
	                                 public std::enable_shared_from_this<ParticipantDeviceContext> {
	public:
		explicit ParticipantDeviceContext(const std::shared_ptr<ServerEktManager> &serverEktManager);

		void onSubscribeStateChanged(const std::shared_ptr<linphone::Event> &event,
		                             linphone::SubscriptionState state) override;
		void onPublishReceived(const std::shared_ptr<linphone::Event> &event,
		                       const std::shared_ptr<linphone::Content> &content) override;
		void onPublishStateChanged(const std::shared_ptr<linphone::Event> &event,
		                           linphone::PublishState state) override;

		const std::shared_ptr<linphone::Event> &getEventSubscribe() const;
		void setEventSubscribe(const std::shared_ptr<linphone::Event> &ev);

		const std::shared_ptr<linphone::Event> &getEventPublish() const;
		void setEventPublish(const std::shared_ptr<linphone::Event> &ev);

		bool knowsEkt() const;
		void setKnowsEkt(bool knowsEkt);

	private:
		std::weak_ptr<ServerEktManager> mServerEktManager;

		std::shared_ptr<linphone::Event> mEventSubscribe = nullptr;
		std::shared_ptr<linphone::Event> mEventPublish = nullptr;

		bool mKnowsEkt = false;
	};

	bctoolbox::RNG mRng;

	std::weak_ptr<linphone::Conference> mLocalConf;

	std::map<const std::shared_ptr<const linphone::ParticipantDevice>, std::shared_ptr<ParticipantDeviceContext>>
	    mParticipantDevices;

	std::vector<uint8_t> mCSpi = {};
	uint16_t mSSpi = 0;
};

} // namespace EktServerPlugin

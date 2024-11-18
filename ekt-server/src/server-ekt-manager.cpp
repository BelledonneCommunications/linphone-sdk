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

#include "server-ekt-manager.h"

#include "bctoolbox/logging.h"

#include "linphone++/buffer.hh"
#include "linphone++/core.hh"
#include "linphone++/dictionary.hh"
#include "linphone++/factory.hh"
#include "linphone++/participant_device.hh"

// =============================================================================

using namespace std;
using namespace linphone;

// -----------------------------------------------------------------------------

EktServerPlugin::ServerEktManager::ServerEktManager(shared_ptr<Conference> localConf) {
	mLocalConf = localConf;
}

void EktServerPlugin::ServerEktManager::onParticipantDeviceStateChanged(
    const shared_ptr<Conference> &conference,
    const shared_ptr<const ParticipantDevice> &device,
    ParticipantDevice::State state) {
	switch (state) {
		case ParticipantDevice::State::ScheduledForLeaving:
			bctbx_message("ServerEktManager::onParticipantDeviceStateChanged : [%s] is leaving",
			              device->getAddress()->asStringUriOnly().c_str());
			if (auto search = mParticipantDevices.find(device); search != mParticipantDevices.end()) {
				auto sub = search->second->getEventSubscribe();
				auto pub = search->second->getEventPublish();
				if (sub) {
					sub->removeListener(search->second);
					sub->terminate();
				}
				if (pub) {
					pub->removeListener(search->second);
					pub->terminate();
				}
			}
			mParticipantDevices.erase(device);
			if (mParticipantDevices.empty()) {
				bctbx_message("ServerEktManager::onParticipantDeviceStateChanged : No participants found in the list. "
				              "Clearing EKT data.");
				clearData();
			}
			break;
		default:
			break;
	}
}

int EktServerPlugin::ServerEktManager::subscribeReceived(const shared_ptr<Event> &ev,
                                                         const shared_ptr<ParticipantDevice> device) {
	bool deviceFound = false;

	if (ev->getName() != "ekt") {
		// Accept only EKT events
		ev->denySubscription(Reason::BadEvent);
		return 1;
	}

	ev->acceptSubscription();

	bctbx_message("ServerEktManager::subscribeReceived : Event subscribe EKT [%p] received from [%s]", ev.get(),
	              ev->getRemoteContact()->asStringUriOnly().c_str());

	if (device) {
		for (auto &[participantDevice, participantDeviceCtx] : mParticipantDevices) {
			if (participantDevice->getAddress()->equal(device->getAddress())) {
				deviceFound = true;
			}
		}
		if (!deviceFound) {
			mParticipantDevices.insert(make_pair(device, make_shared<ParticipantDeviceContext>(shared_from_this())));
			bctbx_message("ServerEktManager::subscribeReceived : [%s] added to the EKT Manager",
			              device->getAddress()->asStringUriOnly().c_str());
		}
	}

	if (ev->getSubscriptionState() == linphone::SubscriptionState::Active) {
		for (auto &[participantDevice, participantDeviceCtx] : mParticipantDevices) {
			if (participantDevice->getAddress()->equal(ev->getRemoteContact())) {
				auto oldEv = participantDeviceCtx->getEventSubscribe();
				participantDeviceCtx->setEventSubscribe(ev);
				participantDeviceCtx->getEventSubscribe()->addListener(participantDeviceCtx);
				if (oldEv) {
					oldEv->removeListener(participantDeviceCtx);
					oldEv->terminate();
				}
				deviceFound = true;
			}
		}
	}

	if (mSSpi == 0) {
		generateSSpi();
	}

	if (mParticipantDevices.empty()) {
		bctbx_error("ServerEktManager::subscribeReceived : The participant device list of ServerEktManager [%p] "
		            "(conference address [%s]) is empty.",
		            this, ev->getToAddress()->asStringUriOnly().c_str());
		return 0;
	}

	if (deviceFound) {       // We found the device
		if (mCSpi.empty()) { // The participant device must generate the EKT
			list<shared_ptr<const Address>> participantDeviceAddresses = {};
			bctbx_message("ServerEktManager::subscribeReceived : No EKT has yet been selected by the server. [%s] has "
			              "to generate it.",
			              ev->getRemoteContact()->asStringUriOnly().c_str());
			for (auto &[participantDevice, participantDeviceCtx] : mParticipantDevices) {
				if (!participantDevice->getAddress()->equal(ev->getRemoteContact())) {
					participantDeviceAddresses.push_back(
					    participantDevice
					        ->getAddress()); // Add the address of all participants except the sender of the SUBSCRIBE
				}
			}
			if (participantDeviceAddresses.empty()) {
				sendNotifyAcceptedEkt(ev);
			} else {
				sendNotifyWithParticipantDeviceList(ev, participantDeviceAddresses);
			}
		} else { // An EKT has already been selected
			bctbx_message("ServerEktManager::subscribeReceived : Ask the other participants for the EKT.");
			for (auto &[participantDevice, participantDeviceCtx] : mParticipantDevices) {
				// Send a NOTIFY to all devices who know EKT
				if (!participantDevice->getAddress()->equal(ev->getRemoteContact()) &&
				    participantDeviceCtx->getEventSubscribe() != nullptr && participantDeviceCtx->knowsEkt()) {
					sendNotifyWithParticipantDeviceList(participantDeviceCtx->getEventSubscribe(),
					                                    {ev->getRemoteContact()});
				}
			}
		}
	}

	return 0;
}

void EktServerPlugin::ServerEktManager::sendNotifyAcceptedEkt(const shared_ptr<Event> &ev) {
	sendNotify(ev, nullptr, {}, {});
}

void EktServerPlugin::ServerEktManager::sendNotifyWithParticipantDeviceList(
    const shared_ptr<Event> &ev, const list<shared_ptr<const Address>> &addresses) {
	if (addresses.empty()) {
		bctbx_message("ServerEktManager::sendNotifyWithParticipantDeviceList : No need to ask EKT");
	} else {
		sendNotify(ev, nullptr, {}, addresses);
	}
}

/**
 * Brief : Send an EKT NOTIFY
 * @param ev		Subscribe event
 * @param from		Address of the device who generated the cipher
 * @param to		Address of the device to who the cipher is sent
 * @param cipher	Ciphertext containing the encrypted EKT
 * @param addresses	Addresses of the devices for who the EKT must be encrypted
 */
void EktServerPlugin::ServerEktManager::sendNotify(const shared_ptr<Event> &ev,
                                                   const shared_ptr<const Address> &from,
                                                   const shared_ptr<Buffer> &cipher,
                                                   const list<shared_ptr<const Address>> &addresses) {
	if (!ev) return;

	shared_ptr<EktInfo> ei = Factory::get()->createEktInfo();
	ei->setSspi(mSSpi);
	if (!mCSpi.empty()) {
		shared_ptr<Buffer> cspi = Factory::get()->createBufferFromData(mCSpi.data(), mCSpi.size());
		ei->setCspi(cspi);
	}
	ei->setFromAddress(from);
	if (cipher) {
		ei->addCipher(ev->getRemoteContact()->asStringUriOnly(), cipher);
	}
	if (!addresses.empty()) {
		for (auto addr : addresses) {
			ei->addCipher(addr->asStringUriOnly(), Factory::get()->createBuffer());
		}
	}

	string xmlBody = mLocalConf.lock()->getCore()->createXmlFromEktInfo(ei);
	auto content = Factory::get()->createContent();
	content->setType("application");
	content->setSubtype("xml");
	content->setUtf8Text(xmlBody);
	ev->notify(content);
}

void EktServerPlugin::ServerEktManager::publishReceived(const shared_ptr<Event> &ev,
                                                        const shared_ptr<const Content> &content) {
	if (ev && content) {
		auto ei = mLocalConf.lock()->getCore()->createEktInfoFromXml(content->getUtf8Text());
		auto eiFrom = ei->getFromAddress();
		for (auto [participantDevice, participantDeviceCtx] : mParticipantDevices) {
			if (participantDevice->getAddress()->equal(eiFrom)) {
				bctbx_message("ServerEktManager::publishReceived : Event publish EKT [%p] received from [%s]", &ev,
				              participantDevice->getAddress()->asStringUriOnly().c_str());
				auto participantEvent = participantDeviceCtx->getEventPublish();
				if (participantEvent != ev) {
					if (participantEvent != nullptr) {
						participantEvent->removeListener(participantDeviceCtx);
						participantEvent->terminate();
					}
					participantDeviceCtx->setEventPublish(ev);
					participantDeviceCtx->getEventPublish()->addListener(participantDeviceCtx);
				}
				if (participantDeviceCtx->getEventSubscribe() && ev->getPublishState() != PublishState::Ok) {
					publishReceived(ev, ei);
				}
			}
		}
	}
}

void EktServerPlugin::ServerEktManager::publishReceived(const shared_ptr<Event> &ev,
                                                        const shared_ptr<const EktInfo> &ei) {
	if (!ei->getFromAddress()) {
		ev->denyPublish(Reason::BadEvent);
		bctbx_error("ServerEktManager::publishReceived : Missing source address");
		return;
	}
	auto sspi = ei->getSspi();
	if (sspi == 0) {
		ev->denyPublish(Reason::BadEvent);
		bctbx_error("ServerEktManager::publishReceived : Unexpected EKT PUBLISH format");
		return;
	} else if (sspi != mSSpi) {
		ev->denyPublish(Reason::BadEvent);
		bctbx_error("ServerEktManager::publishReceived : Wrong SSPI");
		return;
	}
	auto eiCspi = ei->getCspi();
	auto sizeCSpi = eiCspi->getSize();
	auto contentCSPi = eiCspi->getContent();
	vector<uint8_t> cspi(sizeCSpi);
	memcpy(cspi.data(), contentCSPi, sizeCSpi);
	if (cspi.empty()) {
		ev->denyPublish(Reason::BadEvent);
		bctbx_error("ServerEktManager::publishReceived : Missing CSPI");
		return;
	}

	ev->acceptPublish();

	auto from = ei->getFromAddress();
	auto ciphers = ei->getCiphers();
	list<shared_ptr<const Address>> participantDeviceAddressList = {};
	shared_ptr<ParticipantDeviceContext> senderCtx = nullptr;
	// TODO: Refactor the code when adding the EKT regeneration feature
	if (mCSpi.empty()) {
		mCSpi = cspi;
		for (auto [participantDevice, participantDeviceCtx] : mParticipantDevices) {
			auto participantDeviceAddress = participantDevice->getAddress();
			if (participantDeviceAddress->equal(from)) { // PUBLISH sender
				senderCtx = participantDeviceCtx;
				sendNotifyAcceptedEkt(
				    participantDeviceCtx
				        ->getEventSubscribe()); // Inform the ParticipantDevice that their EKT has been selected
				participantDeviceCtx->setKnowsEkt(true);
				bctbx_message("ServerEktManager::publishReceived : [%s] EKT selected",
				              participantDeviceAddress->asStringUriOnly().c_str());
			} else { // Other participants
				bool ektFound = true;
				if (ciphers) {
					if (auto cipher = ciphers->getBuffer(participantDeviceAddress->asStringUriOnly())) {
						auto evSub = participantDeviceCtx->getEventSubscribe();
						if (evSub) {
							sendNotify(evSub, from, cipher, {}); // Distribute the EKT to ParticipantDevices
							participantDeviceCtx->setKnowsEkt(true);
							bctbx_message("ServerEktManager::publishReceived : EKT (just selected) sent to [%s]",
							              participantDeviceAddress->asStringUriOnly().c_str());
						}
					} else {
						ektFound = false;
					}
				} else {
					ektFound = false;
				}
				if (!ektFound) {
					bctbx_message("ServerEktManager::publishReceived : EKT (just selected) not received for [%s]",
					              participantDeviceAddress->asStringUriOnly().c_str());
					participantDeviceAddressList.push_back(participantDevice->getAddress());
				}
			}
		}
	} else if (mCSpi == cspi) {
		for (auto [participantDevice, participantDeviceCtx] : mParticipantDevices) {
			auto participantDeviceAddress = participantDevice->getAddress();
			if (participantDeviceAddress->equal(from)) {
				senderCtx = participantDeviceCtx;
			} else {
				if (!participantDeviceCtx->knowsEkt()) {
					if (auto cipher = ciphers->getBuffer(participantDeviceAddress->asStringUriOnly())) {
						auto evSub = participantDeviceCtx->getEventSubscribe();
						if (evSub) {
							sendNotify(evSub, from, cipher, {}); // Distribute the EKT to ParticipantDevices
							participantDeviceCtx->setKnowsEkt(true);
							bctbx_message(
							    "ServerEktManager::publishReceived : EKT sent to the new participant device [%s]",
							    participantDeviceAddress->asStringUriOnly().c_str());
						}
					} else {
						bctbx_message("ServerEktManager::publishReceived : EKT not received for [%s]",
						              participantDeviceAddress->asStringUriOnly().c_str());
						participantDeviceAddressList.push_back(participantDevice->getAddress());
					}
				}
			}
		}
	}
	if (!participantDeviceAddressList.empty()) {
		sendNotifyWithParticipantDeviceList(senderCtx->getEventSubscribe(), participantDeviceAddressList);
	}
	return;
}

void EktServerPlugin::ServerEktManager::generateSSpi() {
	vector<uint8_t> sspi;
	do {
		sspi = mRng.randomize(2);
		memcpy(&mSSpi, &sspi[0], sizeof(uint16_t));
	} while (mSSpi == 0);
}

void EktServerPlugin::ServerEktManager::clearData() {
	mSSpi = 0;
	bctbx_clean(mCSpi.data(), mCSpi.size());
	mCSpi.clear();
}

// -----------------------------------------------------------------------------

EktServerPlugin::ServerEktManager::ParticipantDeviceContext::ParticipantDeviceContext(
    const shared_ptr<ServerEktManager> &serverEktManager)
    : mServerEktManager(serverEktManager) {
}

void EktServerPlugin::ServerEktManager::ParticipantDeviceContext::onSubscribeStateChanged(
    const shared_ptr<Event> &event, SubscriptionState state) {
	if (!mEventSubscribe) {
		bctbx_error("Event subscribe EKT NULL");
		return;
	}
	switch (state) {
		case SubscriptionState::Terminated:
			bctbx_message("ParticipantDeviceContext::onSubscribeStateChanged : Terminated");
			mEventSubscribe->removeListener(this->shared_from_this());
			mEventSubscribe = nullptr;
			break;
		case SubscriptionState::IncomingReceived:
			bctbx_message("ParticipantDeviceContext::onSubscribeStateChanged : IncomingReceived");
			if (event->getToAddress() == mEventSubscribe->getToAddress() &&
			    event->getRemoteContact() == mEventSubscribe->getRemoteContact()) {
				mServerEktManager.lock()->sendNotifyAcceptedEkt(mEventSubscribe);
			}
			break;
		default:
			break;
	}
}

void EktServerPlugin::ServerEktManager::ParticipantDeviceContext::onPublishReceived(
    const shared_ptr<Event> &event, const shared_ptr<Content> &content) {
	if (mEventPublish == nullptr) {
		bctbx_message("ParticipantDeviceContext::onPublishReceived : First publish");
	} else if (mEventPublish != event) {
		bctbx_message("ParticipantDeviceContext::onPublishReceived : New publish");
	}
	mEventPublish = event;
	auto sem = mServerEktManager.lock();
	if (sem && event->getPublishState() != PublishState::Ok) {
		sem->publishReceived(event, content);
	}
}

void EktServerPlugin::ServerEktManager::ParticipantDeviceContext::onPublishStateChanged(const shared_ptr<Event> &event,
                                                                                        PublishState state) {
	if (state == PublishState::Cleared) {
		bctbx_message("ParticipantDeviceContext::onPublishStateChanged : Cleared");
		mEventPublish->removeListener(this->shared_from_this());
		mEventPublish = nullptr;
	}
}

const shared_ptr<Event> &EktServerPlugin::ServerEktManager::ParticipantDeviceContext::getEventSubscribe() const {
	return mEventSubscribe;
}

void EktServerPlugin::ServerEktManager::ParticipantDeviceContext::setEventSubscribe(const shared_ptr<Event> &ev) {
	mEventSubscribe = ev;
}

const shared_ptr<Event> &EktServerPlugin::ServerEktManager::ParticipantDeviceContext::getEventPublish() const {
	return mEventPublish;
}

void EktServerPlugin::ServerEktManager::ParticipantDeviceContext::setEventPublish(const shared_ptr<Event> &ev) {
	mEventPublish = ev;
}

bool EktServerPlugin::ServerEktManager::ParticipantDeviceContext::knowsEkt() const {
	return mKnowsEkt;
}

void EktServerPlugin::ServerEktManager::ParticipantDeviceContext::setKnowsEkt(bool knowsEkt) {
	mKnowsEkt = knowsEkt;
}

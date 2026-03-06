/*
 * Copyright (c) 2010-2023 Belledonne Communications SARL.
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

#include "bctoolbox/defs.h"

#include "linphone/api/c-event.h"
#include "linphone/core.h"
#include "linphone/utils/utils.h"

#include "account/account.h"
#include "address/address.h"
#include "c-wrapper/c-wrapper.h"
#include "client-conference-event-handler.h"
#include "client-conference-list-event-handler.h"
#include "content/content-manager.h"
#include "content/content-type.h"
#include "core/core-p.h"
#include "event/event-subscribe.h"
#include "logger/logger.h"
#include "xml/conference-info.h"
#include "xml/resource-lists.h"
#include "xml/rlmi.h"

// TODO: Remove me later.
#include "private.h"

// =============================================================================

using namespace std;

LINPHONE_BEGIN_NAMESPACE

// -----------------------------------------------------------------------------

ClientConferenceListEventHandler::ClientConferenceListEventHandler(const std::shared_ptr<Core> &core)
    : ClientConferenceEventHandlerBase(core) {
}

ClientConferenceListEventHandler::~ClientConferenceListEventHandler() {
	unsubscribe();
}

// -----------------------------------------------------------------------------
bool ClientConferenceListEventHandler::subscribe() {
	try {
		const auto &accounts = getCore()->getAccounts();
		bool ret = !accounts.empty();
		for (const auto &account : accounts) {
			ret &= subscribe(account);
		}
		return ret;
	} catch (const bad_weak_ptr &) {
		// Exception thrown by CoreAccessor::getCore()
	}
	return false;
}

std::optional<std::shared_ptr<EventSubscribe>>
ClientConferenceListEventHandler::subscribe(const std::shared_ptr<Event> &eventSubscribe,
                                            const std::map<string, std::shared_ptr<Address>> &addresses) {
	try {
		const auto &core = getCore();
		auto content = Content::create();
		content->setContentType(ContentType::ResourceLists);

		Xsd::ResourceLists::ResourceLists rl = Xsd::ResourceLists::ResourceLists();
		Xsd::ResourceLists::ListType l = Xsd::ResourceLists::ListType();

		const auto &from = eventSubscribe->getFrom();
		auto account = core->findAccountByContactAddress(from);
		auto evSub = createEventSubscribe(eventSubscribe->getTo(), account);

		bool entryAdded = false;
		const auto conferenceIdParams = core->createConferenceIdParams();
		for (const auto &[instanceId, address] : addresses) {
			ConferenceId id(address, from, conferenceIdParams);
			const auto &handler = findHandler(id);

			auto peerAddress = address->getUriWithoutGruu();
			peerAddress.removeUriParam(Conference::kConfIdParameter);
			Address addr = id.getPeerAddress()->getUri();
			const auto lastNotify = handler->getLastNotify();
			addr.setUriParam("Last-Notify", Utils::toString(lastNotify));
			handler->setInitialSubscriptionUnderWayFlag(!handler->alreadySubscribed());
			Xsd::ResourceLists::EntryType entry = Xsd::ResourceLists::EntryType(addr.asStringUriOnly());
			l.getEntry().push_back(entry);
			entryAdded = true;
			handler->setManagedByListEventHandler(true);
			// Assign the event to the ClientConferenceEventHandler object so that it can be notified when a
			// NOTIFY is received. This is particularly useful to allow clients send queued messages in
			// encrypted chatrooms.
			handler->setEvent(evSub);
		}
		if (!entryAdded) {
			return std::nullopt;
		}
		if (populateAndSendEvent(evSub, from, l)) {
			return evSub;
		}
	} catch (const bad_weak_ptr &) {
		// Exception thrown by CoreAccessor::getCore()
	}
	return std::nullopt;
}

std::optional<std::shared_ptr<EventSubscribe>> ClientConferenceListEventHandler::subscribe(
    const std::shared_ptr<Account> &account, const Address &to, bool isFactoryUri) {
	if (!account) {
		lError() << "ClientConferenceListEventHandler [" << this
		         << "] is unable to subscribe to the conference event package (RFC 4575) because the account the event "
		            "handler is trying to subscribe for is NULL";
		return std::nullopt;
	}

	const auto &from = account->getContactAddress();

	const auto toAddr = Address::create(to);
	if (alreadySubscribed(from, toAddr)) {
		lDebug() << "ClientConferenceListEventHandler [" << this << "]: " << *account
		         << " has already an active subscription to " << to;
		return std::nullopt;
	}

	Xsd::ResourceLists::ListType l = Xsd::ResourceLists::ListType();

	auto evSub = createEventSubscribe(toAddr, account);

	bool entryAdded = false;
	auto &handlers = (isFactoryUri) ? mLegacyChatRoomHandlers : mHandlers;
	for (const auto &[key, handlerWkPtr] : handlers) {
		const std::shared_ptr<ClientConferenceEventHandler> handler(handlerWkPtr);
		const ConferenceId &conferenceId = handler->getConferenceId();
		const auto &localAddress = conferenceId.getLocalAddress();
		if (from->weakEqual(*localAddress)) {
			auto peerAddress = conferenceId.getPeerAddress()->getUriWithoutGruu();
			bool hasConfIdParams = peerAddress.hasUriParam(Conference::kConfIdParameter);
			peerAddress.removeUriParam(Conference::kConfIdParameter);
			// If the event To address is the factory URI, then list all chatroom that do not have a conf-id
			// parameter in their conference address. If the event To address is not the factory URI, then
			// compare the ordered string because servers may add custom parameters which may cause a false
			// positive. For instance, according to RFC3261, sip:bob@example.net;a=xxx and
			// sip:bob@example.one;b=aaa are equal. In our case, we want to make sure that a comparison using
			// the addresses from the above example doesn't return true as it means that two different
			// conference servers may be handling the chatrooms.
			if ((isFactoryUri && !hasConfIdParams) ||
			    (hasConfIdParams && (peerAddress.toStringUriOnlyOrdered() == to.toStringUriOnlyOrdered()))) {
				try {
					const auto &core = getCore();
					shared_ptr<AbstractChatRoom> cr = core->findChatRoom(conferenceId, false);
					if (!cr) {
						lError() << "ClientConferenceListEventHandler [" << this << "]: Couldn't add chat room "
						         << conferenceId
						         << " in the chat room list subscription because chat room couldn't be found";
						continue;
					}
					if (cr->hasBeenLeft()) continue;

					Address addr = conferenceId.getPeerAddress()->getUri();
					const auto lastNotify = handler->getLastNotify();
					addr.setUriParam("Last-Notify", Utils::toString(lastNotify));
					handler->setInitialSubscriptionUnderWayFlag(!handler->alreadySubscribed());
					Xsd::ResourceLists::EntryType entry = Xsd::ResourceLists::EntryType(addr.asStringUriOnly());
					l.getEntry().push_back(entry);
					entryAdded = true;
					handler->setManagedByListEventHandler(true);
					// Assign the event to the ClientConferenceEventHandler object so that it can be notified when a
					// NOTIFY is received. This is particularly useful to allow clients send queued messages in
					// encrypted chatrooms.
					handler->setEvent(evSub);
				} catch (const bad_weak_ptr &) {
					// Exception thrown by CoreAccessor::getCore()
				}
			}
		}
	}
	if (!entryAdded) {
		evSub->unref();
		return std::nullopt;
	}
	if (populateAndSendEvent(evSub, from, l)) {
		return evSub;
	}
	return std::nullopt;
}

bool ClientConferenceListEventHandler::populateAndSendEvent(std::shared_ptr<EventSubscribe> &evSub,
                                                            const std::shared_ptr<Address> &from,
                                                            Xsd::ResourceLists::ListType &l) {
	Xsd::ResourceLists::ResourceLists rl = Xsd::ResourceLists::ResourceLists();
	rl.getList().push_back(l);

	Xsd::XmlSchema::NamespaceInfomap map;
	stringstream xmlBody;
	serializeResourceLists(xmlBody, rl, map);

	auto content = Content::create();
	content->setContentType(ContentType::ResourceLists);
	content->setBodyFromUtf8(xmlBody.str());

	evSub->getOp()->setFromAddress(from->getImpl());
	evSub->setInternal(true);
	evSub->addCustomHeader("Require", "recipient-list-subscribe");
	evSub->addCustomHeader("Accept", "multipart/related, application/conference-info+xml, application/rlmi+xml");
	evSub->addCustomHeader("Content-Disposition", "recipient-list");
	if (linphone_core_content_encoding_supported(getCore()->getCCore(), "deflate")) {
		content->setContentEncoding("deflate");
		evSub->addCustomHeader("Accept-Encoding", "deflate");
	}
	evSub->setProperty("event-handler-private", this);
	shared_ptr<EventCbs> cbs = EventCbs::create();
	cbs->setUserData(this);
	cbs->subscribeStateChangedCb = subscribeStateChangedCb;
	evSub->addCallbacks(cbs);
	return (evSub->send(content) == 0);
}

bool ClientConferenceListEventHandler::subscribe(const shared_ptr<Account> &account, bool unsubscribeFirst) {
	if (unsubscribeFirst) {
		unsubscribe(account);
	}

	if (mHandlers.empty() && mLegacyChatRoomHandlers.empty()) {
		lError() << "ClientConferenceListEventHandler [" << this
		         << "]: Unable to subscribe to the conference event package (RFC 4575) of all chatrooms linked to "
		         << *account << " because no handler is available";
		return false;
	}

	auto accountState = account->getState();
	if ((accountState != LinphoneRegistrationRefreshing) && (accountState != LinphoneRegistrationOk)) {
		lError() << "ClientConferenceListEventHandler [" << this
		         << "]: Unable to subscribe to the conference event package (RFC 4575) of all chatrooms linked to "
		         << *account << " because the account has not registered yet (its current state is "
		         << linphone_registration_state_to_string(accountState) << ")";
		return false;
	}

	const auto &accountParams = account->getAccountParams();
	if (!accountParams) {
		lError() << "ClientConferenceListEventHandler [" << this << "]: ClientConferenceListEventHandler [" << this
		         << "] is unable to subscribe to the conference event package (RFC 4575) of all chatrooms linked to "
		         << *account << " because the account parameters are unknown";
		return false;
	}
	const auto &factoryUri = accountParams->getConferenceFactoryAddress();
	bool factoryUriIsValid = factoryUri && factoryUri->isValid();

	const auto &from = account->getContactAddress();
	if (!from || !from->isValid()) {
		lError() << "ClientConferenceListEventHandler [" << this << "]: ClientConferenceListEventHandler [" << this
		         << "] is unable to subscribe to the conference event package (RFC 4575) of all chatrooms linked to "
		         << *account
		         << " because the account contact address is unknown, hence the SUBSCRIBE from header cannot be set";
		return false;
	}

	std::list<std::shared_ptr<EventSubscribe>> accountEvs;
	auto accountEvsOpt = findEvents(from);
	if (accountEvsOpt) {
		accountEvs = accountEvsOpt.value();
	}

	bool success = (mUniqueFocuses.size() > 0) || factoryUriIsValid;
	for (const auto &to : mUniqueFocuses) {
		auto evSub = subscribe(account, to, false);
		if (evSub) {
			accountEvs.push_back(evSub.value());
		} else {
			success = false;
		}
	}

	if (factoryUriIsValid) {
		auto evSub = subscribe(account, *factoryUri, true);
		if (evSub) {
			accountEvs.push_back(evSub.value());
		} else {
			success = false;
		}
	}
	mEvents.insert_or_assign(*from, accountEvs);
	startWaitNotifyTimer();

	return success;
}

void ClientConferenceListEventHandler::unsubscribe() {
	for (auto &[address, events] : mEvents) {
		for (auto &eventSubscribe : events) {
			eventSubscribe->terminate();
		}
	}
	for (const auto &handlers : {mHandlers, mLegacyChatRoomHandlers}) {
		for (const auto &[key, handlerWkPtr] : handlers) {
			try {
				std::shared_ptr<ClientConferenceEventHandler> handler(handlerWkPtr);
				handler->setEvent(nullptr);
			} catch (const bad_weak_ptr &) {
				// Nothing to do because the handler has already been destroyed
			}
		}
	}
	mEvents.clear();
	stopWaitNotifyTimer();
}

void ClientConferenceListEventHandler::unsubscribe(const std::shared_ptr<Account> &account) {
	if (!account || !account->getContactAddress()) return;
	const auto &contactAddress = account->getContactAddress();
	auto accountEvsOpt = findEvents(contactAddress);
	if (accountEvsOpt) {
		auto accountEvs = accountEvsOpt.value();
		for (auto &ev : accountEvs) {
			ev->terminate();
		}
		mEvents.erase(*contactAddress);
	}

	for (const auto &handlers : {mHandlers, mLegacyChatRoomHandlers}) {
		for (const auto &[key, handlerWkPtr] : handlers) {
			if (key.getLocalAddress()->weakEqual(*contactAddress)) {
				try {
					std::shared_ptr<ClientConferenceEventHandler> handler(handlerWkPtr);
					handler->setEvent(nullptr);
				} catch (const bad_weak_ptr &) {
					// Nothing to do because the handler has already been destroyed
				}
			}
		}
	}

	stopWaitNotifyTimer();
}

bool ClientConferenceListEventHandler::alreadySubscribed(const std::shared_ptr<Address> &address,
                                                         const std::shared_ptr<Address> &peer) const {
	const auto &subscriptionState = getSubscriptionState(address, peer);
	return (subscriptionState == LinphoneSubscriptionActive) ||
	       (subscriptionState == LinphoneSubscriptionOutgoingProgress);
}

LinphoneSubscriptionState
ClientConferenceListEventHandler::getSubscriptionState(const std::shared_ptr<Address> &address,
                                                       const std::shared_ptr<Address> &peer) const {
	auto state = LinphoneSubscriptionNone;
	auto eventSubscribe = findEvent(address, peer);
	if (eventSubscribe) {
		state = eventSubscribe.value()->getState();
	}
	return state;
}

void ClientConferenceListEventHandler::invalidateSubscription() {
	mEvents.clear();
}

void ClientConferenceListEventHandler::subscribeStateChangedCb(LinphoneEvent *lev, LinphoneSubscriptionState state) {
	if (state == LinphoneSubscriptionError) {
		auto ev = dynamic_pointer_cast<EventSubscribe>(Event::toCpp(lev)->getSharedFromThis());
		const auto &from = ev->getFrom();
		auto cbs = ev->getCurrentCallbacks();
		ClientConferenceListEventHandler *handler = static_cast<ClientConferenceListEventHandler *>(cbs->getUserData());
		handler->subscriptionDone(from);
	}
}

void ClientConferenceListEventHandler::notifyReceived(std::shared_ptr<Event> notifyEv,
                                                      const std::shared_ptr<const Content> &notifyContent) {
	try {
		stopWaitNotifyTimer();
		auto needFullState = false;
		if (notifyContent) {
			auto core = getCore();
			const auto conferenceIdParams = core->createConferenceIdParams();
			auto eventSubscribe = findEvent(notifyEv);
			const auto evFound = (eventSubscribe.has_value());
			const auto &from = notifyEv->getFrom();
			if (notifyContent->getContentType() == ContentType::ConferenceInfo) {
				// Simple notify received directly from a chat-room
				const string &xmlBody = notifyContent->getBodyAsUtf8String();
				istringstream data(xmlBody);
				std::unique_ptr<Xsd::ConferenceInfo::ConferenceType> confInfo =
				    Xsd::ConferenceInfo::parseConferenceInfo(data, Xsd::XmlSchema::Flags::dont_validate);

				std::shared_ptr<Address> entityAddress = Address::create(confInfo->getEntity().c_str());
				ConferenceId id(entityAddress, from, conferenceIdParams);
				auto handler = findHandler(id);
				if (!handler) return;

				needFullState = (handler->notifyReceived(*notifyContent) ==
				                 ClientConferenceEventHandlerBase::NotifyParsingResult::NeedFullState);
				if (!needFullState && handler->getInitialSubscriptionUnderWayFlag()) {
					handler->setInitialSubscriptionUnderWayFlag(!evFound);
				}
			} else {
				map<string, std::shared_ptr<Address>> addresses;
				list<Content> contents = ContentManager::multipartToContentList(*notifyContent);
				auto rlmiIt = std::find_if(contents.begin(), contents.end(), [](const auto &content) {
					const ContentType &contentType = content.getContentType();
					return (contentType == ContentType::Rlmi);
				});
				if (rlmiIt != contents.end()) {
					auto rlmi = (*rlmiIt);
					const string &body = rlmi.getBodyAsUtf8String();
					addresses = parseRlmi(body);
				}

				for (const auto &content : contents) {
					const ContentType &contentType = content.getContentType();
					if ((contentType != ContentType::Multipart) && (contentType != ContentType::ConferenceInfo)) {
						continue;
					}

					string cid = content.getHeader("Content-Id").getValue();
					if (cid.empty()) continue;
					cid = Utils::unquote(cid, '<');
					map<string, std::shared_ptr<Address>>::const_iterator it = addresses.find(cid);
					if (it == addresses.cend()) continue;

					std::shared_ptr<Address> peer = it->second;
					ConferenceId id(peer, from, conferenceIdParams);
					auto handler = findHandler(id);
					if (!handler) continue;

					auto result = ClientConferenceEventHandlerBase::NotifyParsingResult::Success;
					if (contentType == ContentType::Multipart) result = handler->multipartNotifyReceived(content);
					else if (contentType == ContentType::ConferenceInfo) result = handler->notifyReceived(content);
					if (result == ClientConferenceEventHandlerBase::NotifyParsingResult::NeedFullState) {
						needFullState = true;
					}
				}
				if (needFullState) {
					lInfo() << "ClientConferenceListEventHandler [" << this << "]: At least one conference handled by "
					        << *notifyEv << " needs a NOTIFY full state. Terminating it and reSUBSCRIBing";
					notifyEv->terminate();
					deleteEvent(notifyEv);
					auto ev = subscribe(notifyEv, addresses);
					if (ev) {
						addEvent(ev.value());
					} else {
						lError() << "ClientConferenceListEventHandler [" << this << "]: Unable to subscribe again to "
						         << *notifyEv->getTo();
					}
				} else {
					subscriptionDone(from, addresses);
				}
			}
		}
	} catch (const bad_weak_ptr &) {
		// Exception thrown by CoreAccessor::getCore()
	} catch (const exception &e) {
		lError() << "ClientConferenceListEventHandler [" << this << "]: exception " << e.what()
		         << " has been caught while parsing conference-info in conferences notify of " << *notifyEv;
		return;
	}
}

void ClientConferenceListEventHandler::subscriptionDone(
    const std::shared_ptr<Address> &from, const std::map<std::string, std::shared_ptr<Address>> &addresses) {
	if (!from) {
		lError() << "ClientConferenceListEventHandler [" << this
		         << "] is unable to notify that a subscription is completed because the from address is null";
		return;
	}
	if (addresses.empty()) {
		// Remove subscription underway flag from all handlers matching the account that sent the subscription
		for (const auto &handlers : {mHandlers, mLegacyChatRoomHandlers}) {
			for (const auto &[key, handlerWkPtr] : handlers) {
				try {
					std::shared_ptr<ClientConferenceEventHandler> handler(handlerWkPtr);
					const ConferenceId &conferenceId = handler->getConferenceId();
					if (from->weakEqual(*conferenceId.getLocalAddress())) {
						handler->notifySubscriptionUnderwayDone();
					}
				} catch (const bad_weak_ptr &) {
				}
			}
		}
	} else {
		try {
			// Notify that a NOTIFY has been received to all handlers in the Resource List Meta-Information (RLMI)
			// content, even those that have not received any updates
			const auto conferenceIdParams = getCore()->createConferenceIdParams();
			for (const auto &[instanceId, address] : addresses) {
				ConferenceId id(address, from, conferenceIdParams);
				auto handler = findHandler(id);
				if (!handler) continue;
				handler->notifySubscriptionUnderwayDone();
			}
		} catch (const bad_weak_ptr &) {
			// Exception thrown by CoreAccessor::getCore()
		}
	}
}

// -----------------------------------------------------------------------------

std::optional<std::list<std::shared_ptr<EventSubscribe>>>
ClientConferenceListEventHandler::findEvents(const std::shared_ptr<Address> &address) const {
	auto eventsIt = mEvents.find(*address);
	if (eventsIt == mEvents.end()) {
		return std::nullopt;
	}
	return (eventsIt->second);
}

std::optional<std::shared_ptr<EventSubscribe>>
ClientConferenceListEventHandler::findEvent(const std::shared_ptr<Address> &address,
                                            const std::shared_ptr<Address> &resource) const {
	std::list<std::shared_ptr<EventSubscribe>> accountEvs;
	const auto eventsOpt = findEvents(address);
	if (eventsOpt) {
		const auto addressEvs = eventsOpt.value();
		Address resourceNoGruu(resource->getUriWithoutGruu());
		// Ignore GRUU as the current one may not be the same as the one used to create the chatroom.
		auto it = std::find_if(addressEvs.begin(), addressEvs.end(), [&resourceNoGruu](const auto &ev) {
			return ev->getResource()->getUriWithoutGruu() == resourceNoGruu;
		});

		if (it != addressEvs.end()) {
			return *it;
		}
	}
	return std::nullopt;
}

void ClientConferenceListEventHandler::deleteEvent(const std::shared_ptr<Event> &eventSubscribe) {
	const auto &fromAddr = eventSubscribe->getFrom();
	const auto eventsOpt = findEvents(fromAddr);
	if (eventsOpt) {
		const auto accountEvs = eventsOpt.value();
		auto addressEvs = accountEvs;
		auto it = std::find_if(addressEvs.begin(), addressEvs.end(),
		                       [callId = eventSubscribe->getOp()->getCallId()](const auto &ev) {
			                       return ev->getOp()->getCallId() == callId;
		                       });

		if (it != addressEvs.end()) {
			addressEvs.erase(it);
			mEvents.insert_or_assign(*fromAddr, addressEvs);
		}
	}
}

void ClientConferenceListEventHandler::addEvent(const std::shared_ptr<EventSubscribe> &eventSubscribe) {
	const auto &fromAddr = eventSubscribe->getFrom();
	const auto eventsOpt = findEvents(fromAddr);
	std::list<std::shared_ptr<EventSubscribe>> addressEvs;
	if (eventsOpt) {
		const auto accountEvs = eventsOpt.value();
		addressEvs = accountEvs;
	}
	addressEvs.push_back(eventSubscribe);
	mEvents.insert_or_assign(*fromAddr, addressEvs);
}

std::optional<std::shared_ptr<EventSubscribe>>
ClientConferenceListEventHandler::findEvent(const std::shared_ptr<Event> &eventSubscribe) const {
	const auto &fromAddr = eventSubscribe->getFrom();
	const auto eventsOpt = findEvents(fromAddr);
	if (eventsOpt) {
		const auto addressEvs = eventsOpt.value();
		auto it = std::find_if(addressEvs.begin(), addressEvs.end(),
		                       [callId = eventSubscribe->getOp()->getCallId()](const auto &ev) {
			                       return ev->getOp()->getCallId() == callId;
		                       });

		if (it != addressEvs.end()) {
			return *it;
		}
	}
	return std::nullopt;
}

bool ClientConferenceListEventHandler::handlesEvent(const std::shared_ptr<Event> &eventSubscribe) const {
	auto ev = findEvent(eventSubscribe);
	if (ev) {
		return ev.value() != nullptr;
	}
	return false;
}

std::shared_ptr<ClientConferenceEventHandler>
ClientConferenceListEventHandler::findHandler(const ConferenceId &conferenceId) const {
	for (const auto &handlers : {mHandlers, mLegacyChatRoomHandlers}) {
		auto it = handlers.find(conferenceId);
		if (it != handlers.end()) {
			try {
				std::shared_ptr<ClientConferenceEventHandler> handler = (*it).second.lock();
				return handler;
			} catch (const bad_weak_ptr &) {
			}
		}
	}
	return nullptr;
}

void ClientConferenceListEventHandler::addHandler(std::shared_ptr<ClientConferenceEventHandler> handler) {
	if (!handler) {
		lError() << "ClientConferenceListEventHandler [" << this << "]: Unable to add a handler whose pointer is null";
		return;
	}

	const ConferenceId &conferenceId = handler->getConferenceId();
	if (!conferenceId.isValid()) {
		lError() << "ClientConferenceListEventHandler [" << this << "]: Unable to add handler [" << this
		         << "] because its conference id is not valid";
		return;
	}

	if (findHandler(conferenceId)) {
		lWarning() << "Trying to insert an already present handler into the ClientConferenceListEventHandler [" << this
		           << "]: " << conferenceId;
		return;
	}

	const auto &localAddress = conferenceId.getLocalAddress();
	const auto &peerAddress = conferenceId.getPeerAddress();
	bool hasConfIdParams = peerAddress->hasUriParam(Conference::kConfIdParameter);
	if (hasConfIdParams) {
		auto focusUri = peerAddress->getUriWithoutGruu();
		focusUri.removeUriParam(Conference::kConfIdParameter);
		mUniqueFocuses.insert(focusUri);
		mHandlers.insert({conferenceId, handler});
	} else {
		try {
			const auto conferenceFactoryUri = Core::getConferenceFactoryAddress(getCore(), localAddress);
			if (!conferenceFactoryUri || !conferenceFactoryUri->isValid()) {
				lDebug() << "ClientConferenceListEventHandler [" << this << "]: Account with local address ["
				         << *localAddress << "] hasn't a conference factory URI defined.";
				return;
			}

			mLegacyChatRoomHandlers.insert({conferenceId, handler});
		} catch (const bad_weak_ptr &) {
		}
	}
	handler->setManagedByListEventHandler(true);
}

void ClientConferenceListEventHandler::removeHandler(std::shared_ptr<ClientConferenceEventHandler> handler) {
	if (!handler) {
		return;
	}

	const ConferenceId &conferenceId = handler->getConferenceId();
	if (!conferenceId.isValid()) {
		lError() << "ClientConferenceListEventHandler [" << this << "]: Unable to remove handler [" << this
		         << "] because its conference id is not valid";
		return;
	}

	const auto &peerAddress = conferenceId.getPeerAddress();
	bool hasConfIdParams = peerAddress->hasUriParam(Conference::kConfIdParameter);
	auto &handlers = (hasConfIdParams) ? mHandlers : mLegacyChatRoomHandlers;
	auto it = handlers.find(conferenceId);
	if (it != handlers.end()) {
		handler->setManagedByListEventHandler(false);
		handler->setEvent(nullptr);
		handlers.erase(it);
		lInfo() << "ClientConferenceListEventHandler [" << this
		        << "]: Client Conference Event Handler with conference id " << conferenceId << " [" << handler
		        << "] has been removed.";
	} else {
		lError() << "ClientConferenceListEventHandler [" << this
		         << "]: Client Conference Event Handler with conference id " << conferenceId << " has not been found.";
	}
}

void ClientConferenceListEventHandler::clearHandlers() {
	mHandlers.clear();
	mLegacyChatRoomHandlers.clear();
}

map<string, std::shared_ptr<Address>> ClientConferenceListEventHandler::parseRlmi(const string &xmlBody) const {
	istringstream data(xmlBody);
	map<string, std::shared_ptr<Address>> addresses;
	unique_ptr<Xsd::Rlmi::List> rlmi;
	try {
		rlmi = Xsd::Rlmi::parseList(data, Xsd::XmlSchema::Flags::dont_validate);
	} catch (const exception &) {
		lError() << "ClientConferenceListEventHandler [" << this << "]: Error while parsing RLMI in conferences notify";
		return addresses;
	}
	for (const auto &resource : rlmi->getResource()) {
		if (resource.getInstance().empty()) continue;

		const string &uri = string(resource.getUri());
		if (uri.empty()) continue;

		std::shared_ptr<Address> peer = Address::create(uri);
		for (const auto &instance : resource.getInstance()) {
			string cid = string(instance.getId());
			if (cid.empty()) continue;
			cid = Utils::unquote(cid, '<');

			addresses.emplace(cid, peer);
		}
	}
	return addresses;
}

// -----------------------------------------------------------------------------

void ClientConferenceListEventHandler::onNetworkReachable(bool sipNetworkReachable,
                                                          BCTBX_UNUSED(bool mediaNetworkReachable)) {
	if (sipNetworkReachable) {
		subscribe();
	} else {
		unsubscribe();
	}
}

void ClientConferenceListEventHandler::onAccountRegistrationStateChanged(std::shared_ptr<Account> account,
                                                                         LinphoneRegistrationState state,
                                                                         BCTBX_UNUSED(const std::string &message)) {
	if (state == LinphoneRegistrationOk) {
		// Do not subscribe again to focus the SDK has already a subscription for
		subscribe(account, false);
	} else if (state == LinphoneRegistrationCleared) { // On cleared, restart subscription if the cleared proxy config
		                                               // is the current subscription
		// If no subscription is found, then unsubscribe the account
		unsubscribe(account);
	}
}

void ClientConferenceListEventHandler::onEnteringBackground() {
	unsubscribe();
}

void ClientConferenceListEventHandler::onEnteringForeground() {
	subscribe();
}

void ClientConferenceListEventHandler::onNotifyWaitExpired() {
	// Remove subscription underway flag from all child handlers.
	for (const auto &handlers : {mHandlers, mLegacyChatRoomHandlers}) {
		for (const auto &[key, handlerWkPtr] : handlers) {
			try {
				std::shared_ptr<ClientConferenceEventHandler> handler(handlerWkPtr);
				handler->notifySubscriptionUnderwayDone();
			} catch (const bad_weak_ptr &) {
				// ignored
			}
		}
	}
}

LINPHONE_END_NAMESPACE

/*
 * client-group-chat-room.cpp
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

#include "linphone/utils/utils.h"

#include "address/address-p.h"
#include "c-wrapper/c-wrapper.h"
#include "client-group-chat-room-p.h"
#include "conference/handlers/remote-conference-event-handler.h"
#include "conference/participant-p.h"
#include "conference/remote-conference-p.h"
#include "conference/session/call-session-p.h"
#include "core/core-p.h"
#include "event-log/events.h"
#include "logger/logger.h"
#include "sal/refer-op.h"

// =============================================================================

using namespace std;

LINPHONE_BEGIN_NAMESPACE

// -----------------------------------------------------------------------------

list<IdentityAddress> ClientGroupChatRoomPrivate::cleanAddressesList (const list<IdentityAddress> &addresses) const {
	L_Q();
	list<IdentityAddress> cleanedList(addresses);
	cleanedList.sort();
	cleanedList.unique();
	for (auto it = cleanedList.begin(); it != cleanedList.end();) {
		if (q->findParticipant(*it) || (q->getMe()->getAddress() == *it))
			it = cleanedList.erase(it);
		else
			it++;
	}
	return cleanedList;
}

shared_ptr<CallSession> ClientGroupChatRoomPrivate::createSession () {
	L_Q();
	L_Q_T(RemoteConference, qConference);

	CallSessionParams csp;
	csp.addCustomHeader("Require", "recipient-list-invite");
	csp.addCustomContactParameter("text");

	shared_ptr<Participant> focus = qConference->getPrivate()->focus;
	shared_ptr<CallSession> session = focus->getPrivate()->createSession(*q, &csp, false, callSessionListener);
	const Address &myAddress = q->getMe()->getAddress();
	Address myCleanedAddress(myAddress);
	myCleanedAddress.removeUriParam("gr"); // Remove gr parameter for INVITE
	session->configure(LinphoneCallOutgoing, nullptr, nullptr, myCleanedAddress, focus->getPrivate()->getDevices().front()->getAddress());
	session->initiateOutgoing();
	return session;
}

void ClientGroupChatRoomPrivate::notifyReceived (const string &body) {
	L_Q_T(RemoteConference, qConference);
	qConference->getPrivate()->eventHandler->notifyReceived(body);
}

void ClientGroupChatRoomPrivate::multipartNotifyReceived (const string &body) {
	L_Q_T(RemoteConference, qConference);
	qConference->getPrivate()->eventHandler->multipartNotifyReceived(body);
}

// -----------------------------------------------------------------------------

void ClientGroupChatRoomPrivate::setCallSessionListener (CallSessionListener *listener) {
	L_Q();
	L_Q_T(RemoteConference, qConference);
	callSessionListener = listener;
	shared_ptr<CallSession> session = qConference->getPrivate()->focus->getPrivate()->getSession();
	if (session)
		session->getPrivate()->setCallSessionListener(listener);
	for (const auto &participant : q->getParticipants()) {
		session = participant->getPrivate()->getSession();
		if (session)
			session->getPrivate()->setCallSessionListener(listener);
	}
}

// -----------------------------------------------------------------------------

void ClientGroupChatRoomPrivate::onChatRoomInsertRequested (const shared_ptr<AbstractChatRoom> &chatRoom) {
	L_Q();
	q->getCore()->getPrivate()->insertChatRoom(chatRoom);
}

void ClientGroupChatRoomPrivate::onChatRoomInsertInDatabaseRequested (const shared_ptr<AbstractChatRoom> &chatRoom) {
	L_Q();
	q->getCore()->getPrivate()->insertChatRoomWithDb(chatRoom);
}

// -----------------------------------------------------------------------------

void ClientGroupChatRoomPrivate::onCallSessionSetReleased (const shared_ptr<const CallSession> &session) {
	L_Q_T(RemoteConference, qConference);

	ParticipantPrivate *participantPrivate = qConference->getPrivate()->focus->getPrivate();
	if (session == participantPrivate->getSession())
		participantPrivate->removeSession();
}

void ClientGroupChatRoomPrivate::onCallSessionStateChanged (
	const shared_ptr<const CallSession> &session,
	LinphoneCallState newState,
	const string &message
) {
	L_Q();
	L_Q_T(RemoteConference, qConference);

	if (newState == LinphoneCallConnected) {
		if (q->getState() == ChatRoom::State::CreationPending) {
			IdentityAddress addr(session->getRemoteContactAddress()->asStringUriOnly());
			q->onConferenceCreated(addr);
			if (session->getRemoteContactAddress()->hasParam("isfocus"))
				qConference->getPrivate()->eventHandler->subscribe(q->getChatRoomId());
		} else if (q->getState() == ChatRoom::State::TerminationPending)
			qConference->getPrivate()->focus->getPrivate()->getSession()->terminate();
	} else if ((newState == LinphoneCallReleased) && (q->getState() == ChatRoom::State::TerminationPending)) {
		q->onConferenceTerminated(q->getConferenceAddress());
	} else if ((newState == LinphoneCallError) && (q->getState() == ChatRoom::State::CreationPending)) {
		setState(ChatRoom::State::CreationFailed);
	}
}

// =============================================================================

ClientGroupChatRoom::ClientGroupChatRoom (
	const shared_ptr<Core> &core,
	const string &uri,
	const IdentityAddress &me,
	const string &subject
) : ChatRoom(*new ClientGroupChatRoomPrivate, core, ChatRoomId(IdentityAddress(), me)),
RemoteConference(core, me, nullptr) {
	L_D_T(RemoteConference, dConference);
	IdentityAddress focusAddr(uri);
	dConference->focus = make_shared<Participant>(focusAddr);
	dConference->focus->getPrivate()->addDevice(focusAddr);
	RemoteConference::setSubject(subject);
}

ClientGroupChatRoom::ClientGroupChatRoom (
	const shared_ptr<Core> &core,
	const ChatRoomId &chatRoomId,
	shared_ptr<Participant> &me,
	const string &subject,
	list<shared_ptr<Participant>> &&participants,
	unsigned int lastNotifyId
) : ChatRoom(*new ClientGroupChatRoomPrivate, core, chatRoomId),
RemoteConference(core, me->getAddress(), nullptr) {
	L_D_T(RemoteConference, dConference);

	const IdentityAddress &peerAddress = chatRoomId.getPeerAddress();
	dConference->focus = make_shared<Participant>(peerAddress);
	dConference->focus->getPrivate()->addDevice(peerAddress);
	dConference->conferenceAddress = peerAddress;
	dConference->subject = subject;
	dConference->participants = move(participants);

	getMe()->getPrivate()->setAdmin(me->isAdmin());

	dConference->eventHandler->setLastNotify(lastNotifyId);
	dConference->eventHandler->subscribe(getChatRoomId());
}

shared_ptr<Core> ClientGroupChatRoom::getCore () const {
	return ChatRoom::getCore();
}

ClientGroupChatRoom::CapabilitiesMask ClientGroupChatRoom::getCapabilities () const {
	return Capabilities::Conference;
}

bool ClientGroupChatRoom::hasBeenLeft () const {
	return getState() != State::Created;
}

bool ClientGroupChatRoom::canHandleParticipants () const {
	return RemoteConference::canHandleParticipants();
}

const IdentityAddress &ClientGroupChatRoom::getConferenceAddress () const {
	return RemoteConference::getConferenceAddress();
}

void ClientGroupChatRoom::addParticipant (const IdentityAddress &addr, const CallSessionParams *params, bool hasMedia) {
	list<IdentityAddress> addresses;
	addresses.push_back(addr);
	addParticipants(addresses, params, hasMedia);
}

void ClientGroupChatRoom::addParticipants (
	const list<IdentityAddress> &addresses,
	const CallSessionParams *params,
	bool hasMedia
) {
	L_D();
	L_D_T(RemoteConference, dConference);

	list<IdentityAddress> addressesList = d->cleanAddressesList(addresses);
	if (addressesList.empty())
		return;

	if ((getState() != ChatRoom::State::Instantiated) && (getState() != ChatRoom::State::Created)) {
		lError() << "Cannot add participants to the ClientGroupChatRoom in a state other than Instantiated or Created";
		return;
	}

	Content content;
	content.setBody(getResourceLists(addressesList));
	content.setContentType("application/resource-lists+xml");
	content.setContentDisposition("recipient-list");

	shared_ptr<CallSession> session = dConference->focus->getPrivate()->getSession();
	if (session)
		session->update(nullptr, getSubject(), &content);
	else {
		session = d->createSession();
		session->startInvite(nullptr, getSubject(), &content);
		if (getState() == ChatRoom::State::Instantiated)
			d->setState(ChatRoom::State::CreationPending);
	}
}

void ClientGroupChatRoom::removeParticipant (const shared_ptr<const Participant> &participant) {
	LinphoneCore *cCore = getCore()->getCCore();

	SalReferOp *referOp = new SalReferOp(cCore->sal);
	LinphoneAddress *lAddr = linphone_address_new(getConferenceAddress().asString().c_str());
	linphone_configure_op(cCore, referOp, lAddr, nullptr, false);
	linphone_address_unref(lAddr);
	Address referToAddr = participant->getAddress();
	referToAddr.setParam("text");
	referToAddr.setUriParam("method", "BYE");
	referOp->send_refer(referToAddr.getPrivate()->getInternalAddress());
	referOp->unref();
}

void ClientGroupChatRoom::removeParticipants (const list<shared_ptr<Participant>> &participants) {
	RemoteConference::removeParticipants(participants);
}

shared_ptr<Participant> ClientGroupChatRoom::findParticipant (const IdentityAddress &addr) const {
	return RemoteConference::findParticipant(addr);
}

shared_ptr<Participant> ClientGroupChatRoom::getMe () const {
	return RemoteConference::getMe();
}

int ClientGroupChatRoom::getParticipantCount () const {
	return RemoteConference::getParticipantCount();
}

const list<shared_ptr<Participant>> &ClientGroupChatRoom::getParticipants () const {
	return RemoteConference::getParticipants();
}

void ClientGroupChatRoom::setParticipantAdminStatus (const shared_ptr<Participant> &participant, bool isAdmin) {
	if (isAdmin == participant->isAdmin())
		return;

	if (!getMe()->isAdmin()) {
		lError() << "Cannot change the participant admin status because I am not admin";
		return;
	}

	LinphoneCore *cCore = getCore()->getCCore();

	SalReferOp *referOp = new SalReferOp(cCore->sal);
	LinphoneAddress *lAddr = linphone_address_new(getConferenceAddress().asString().c_str());
	linphone_configure_op(cCore, referOp, lAddr, nullptr, false);
	linphone_address_unref(lAddr);
	Address referToAddr = participant->getAddress();
	referToAddr.setParam("text");
	referToAddr.setParam("admin", Utils::toString(isAdmin));
	referOp->send_refer(referToAddr.getPrivate()->getInternalAddress());
	referOp->unref();
}

const string &ClientGroupChatRoom::getSubject () const {
	return RemoteConference::getSubject();
}

void ClientGroupChatRoom::setSubject (const string &subject) {
	L_D();
	L_D_T(RemoteConference, dConference);

	if (getState() != ChatRoom::State::Created) {
		lError() << "Cannot change the ClientGroupChatRoom subject in a state other than Created";
		return;
	}

	if (!getMe()->isAdmin()) {
		lError() << "Cannot change the ClientGroupChatRoom subject because I am not admin";
		return;
	}

	shared_ptr<CallSession> session = dConference->focus->getPrivate()->getSession();
	if (session)
		session->update(nullptr, subject);
	else {
		session = d->createSession();
		session->startInvite(nullptr, subject, nullptr);
	}
}

void ClientGroupChatRoom::join () {
	L_D();
	L_D_T(RemoteConference, dConference);

	shared_ptr<CallSession> session = dConference->focus->getPrivate()->getSession();
	if (!session && ((getState() == ChatRoom::State::Instantiated) || (getState() == ChatRoom::State::Terminated))) {
		session = d->createSession();
		session->startInvite(nullptr, "", nullptr);
		d->setState(ChatRoom::State::CreationPending);
	}
}

void ClientGroupChatRoom::leave () {
	L_D();
	L_D_T(RemoteConference, dConference);

	dConference->eventHandler->unsubscribe();

	shared_ptr<CallSession> session = dConference->focus->getPrivate()->getSession();
	if (session)
		session->terminate();
	else {
		session = d->createSession();
		session->startInvite(nullptr, "", nullptr);
	}

	d->setState(ChatRoom::State::TerminationPending);
}

// -----------------------------------------------------------------------------

void ClientGroupChatRoom::onConferenceCreated (const IdentityAddress &addr) {
	L_D();
	L_D_T(RemoteConference, dConference);
	dConference->conferenceAddress = addr;
	dConference->focus->getPrivate()->setAddress(addr);
	dConference->focus->getPrivate()->clearDevices();
	dConference->focus->getPrivate()->addDevice(addr);
	d->chatRoomId = ChatRoomId(addr, d->chatRoomId.getLocalAddress());
	d->chatRoomListener->onChatRoomInsertRequested(getSharedFromThis());
}

void ClientGroupChatRoom::onConferenceTerminated (const IdentityAddress &addr) {
	L_D();
	L_D_T(RemoteConference, dConference);
	dConference->eventHandler->resetLastNotify();
	d->setState(ChatRoom::State::Terminated);
	getCore()->getPrivate()->mainDb->addEvent(make_shared<ConferenceEvent>(
		EventLog::Type::ConferenceTerminated,
		time(nullptr),
		d->chatRoomId
	));
}

void ClientGroupChatRoom::onFirstNotifyReceived (const IdentityAddress &addr) {
	L_D();
	d->setState(ChatRoom::State::Created);

	d->chatRoomListener->onChatRoomInsertInDatabaseRequested(getSharedFromThis());

	// TODO: Bug. Event is inserted many times.
	// Avoid this in the future. Deal with signals/slots system.
	#if 0
	getCore()->getPrivate()->mainDb->addEvent(make_shared<ConferenceEvent>(
		EventLog::Type::ConferenceCreated,
		time(nullptr),
		d->chatRoomId
	));
	#endif
}

void ClientGroupChatRoom::onParticipantAdded (const shared_ptr<ConferenceParticipantEvent> &event, bool isFullState) {
	L_D_T(RemoteConference, dConference);

	const IdentityAddress &addr = event->getParticipantAddress();
	if (isMe(addr))
		return;

	shared_ptr<Participant> participant = findParticipant(addr);
	if (participant) {
		lWarning() << "Participant " << participant << " added but already in the list of participants!";
		return;
	}

	participant = make_shared<Participant>(addr);
	dConference->participants.push_back(participant);

	if (isFullState)
		return;

	getCore()->getPrivate()->mainDb->addEvent(event);

	LinphoneChatRoom *cr = L_GET_C_BACK_PTR(this);
	LinphoneChatRoomCbs *cbs = linphone_chat_room_get_callbacks(cr);
	LinphoneChatRoomCbsParticipantAddedCb cb = linphone_chat_room_cbs_get_participant_added(cbs);
	if (cb)
		cb(cr, L_GET_C_BACK_PTR(event));
}

void ClientGroupChatRoom::onParticipantRemoved (const shared_ptr<ConferenceParticipantEvent> &event, bool isFullState) {
	(void)isFullState;

	L_D_T(RemoteConference, dConference);

	const IdentityAddress &addr = event->getParticipantAddress();
	shared_ptr<Participant> participant = findParticipant(addr);
	if (!participant) {
		lWarning() << "Participant " << addr.asString() << " removed but not in the list of participants!";
		return;
	}

	dConference->participants.remove(participant);
	getCore()->getPrivate()->mainDb->addEvent(event);

	LinphoneChatRoom *cr = L_GET_C_BACK_PTR(this);
	LinphoneChatRoomCbs *cbs = linphone_chat_room_get_callbacks(cr);
	LinphoneChatRoomCbsParticipantRemovedCb cb = linphone_chat_room_cbs_get_participant_removed(cbs);
	if (cb)
		cb(cr, L_GET_C_BACK_PTR(event));
}

void ClientGroupChatRoom::onParticipantSetAdmin (const shared_ptr<ConferenceParticipantEvent> &event, bool isFullState) {
	const IdentityAddress &addr = event->getParticipantAddress();
	shared_ptr<Participant> participant;
	if (isMe(addr))
		participant = getMe();
	else
		participant = findParticipant(addr);
	if (!participant) {
		lWarning() << "Participant " << participant << " admin status has been changed but is not in the list of participants!";
		return;
	}

	bool isAdmin = event->getType() == EventLog::Type::ConferenceParticipantSetAdmin;
	if (participant->isAdmin() == isAdmin)
		return; // No change in the local admin status, do not notify
	participant->getPrivate()->setAdmin(isAdmin);

	if (isFullState)
		return;

	getCore()->getPrivate()->mainDb->addEvent(event);

	LinphoneChatRoom *cr = L_GET_C_BACK_PTR(this);
	LinphoneChatRoomCbs *cbs = linphone_chat_room_get_callbacks(cr);
	LinphoneChatRoomCbsParticipantAdminStatusChangedCb cb = linphone_chat_room_cbs_get_participant_admin_status_changed(cbs);
	if (cb)
		cb(cr, L_GET_C_BACK_PTR(event));
}

void ClientGroupChatRoom::onSubjectChanged (const shared_ptr<ConferenceSubjectEvent> &event, bool isFullState) {
	if (getSubject() == event->getSubject())
		return; // No change in the local subject, do not notify
	RemoteConference::setSubject(event->getSubject());

	if (isFullState)
		return;

	getCore()->getPrivate()->mainDb->addEvent(event);

	LinphoneChatRoom *cr = L_GET_C_BACK_PTR(this);
	LinphoneChatRoomCbs *cbs = linphone_chat_room_get_callbacks(cr);
	LinphoneChatRoomCbsSubjectChangedCb cb = linphone_chat_room_cbs_get_subject_changed(cbs);
	if (cb)
		cb(cr, L_GET_C_BACK_PTR(event));
}

void ClientGroupChatRoom::onParticipantDeviceAdded (const shared_ptr<ConferenceParticipantDeviceEvent> &event, bool isFullState) {
	const IdentityAddress &addr = event->getParticipantAddress();
	shared_ptr<Participant> participant;
	if (isMe(addr))
		participant = getMe();
	else
		participant = findParticipant(addr);
	if (!participant) {
		lWarning() << "Participant " << participant << " added a device but is not in the list of participants!";
		return;
	}
	participant->getPrivate()->addDevice(event->getDeviceAddress());

	if (isFullState)
		return;

	getCore()->getPrivate()->mainDb->addEvent(event);

	LinphoneChatRoom *cr = L_GET_C_BACK_PTR(this);
	LinphoneChatRoomCbs *cbs = linphone_chat_room_get_callbacks(cr);
	LinphoneChatRoomCbsParticipantDeviceAddedCb cb = linphone_chat_room_cbs_get_participant_device_added(cbs);
	if (cb)
		cb(cr, L_GET_C_BACK_PTR(event));
}

void ClientGroupChatRoom::onParticipantDeviceRemoved (const shared_ptr<ConferenceParticipantDeviceEvent> &event, bool isFullState) {
	(void)isFullState;

	const IdentityAddress &addr = event->getParticipantAddress();
	shared_ptr<Participant> participant;
	if (isMe(addr))
		participant = getMe();
	else
		participant = findParticipant(addr);
	if (!participant) {
		lWarning() << "Participant " << participant << " removed a device but is not in the list of participants!";
		return;
	}
	participant->getPrivate()->removeDevice(event->getDeviceAddress());
	getCore()->getPrivate()->mainDb->addEvent(event);

	LinphoneChatRoom *cr = L_GET_C_BACK_PTR(this);
	LinphoneChatRoomCbs *cbs = linphone_chat_room_get_callbacks(cr);
	LinphoneChatRoomCbsParticipantDeviceRemovedCb cb = linphone_chat_room_cbs_get_participant_device_removed(cbs);
	if (cb)
		cb(cr, L_GET_C_BACK_PTR(event));
}

LINPHONE_END_NAMESPACE

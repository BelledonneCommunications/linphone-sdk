/*
 * copyright (c) 2010-2022 belledonne communications sarl.
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

#include <random>

#ifdef HAVE_SOCI
#include <soci/soci.h>
#endif // HAVE_SOCI

#include "chat/chat-message/chat-message-p.h"
#include "chat/encryption/encryption-engine.h"
#include "conference/conference.h"
#include "conference/participant.h"
#include "core/core-p.h"
#include "linphone/api/c-chat-message.h"
#include "linphone/api/c-chat-room-params.h"
#include "linphone/api/c-participant-imdn-state.h"
#include "linphone/api/c-participant.h"
#include "linphone/chat.h"
#include "local-conference-tester-functions.h"

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

namespace LinphoneTest {

void sendEphemeralMessageInAdminMode(Focus &focus,
                                     ClientConference &sender,
                                     ClientConference &recipient,
                                     LinphoneChatRoom *senderCr,
                                     LinphoneChatRoom *recipientCr,
                                     const std::string basicText,
                                     const size_t noMsg) {

	bctbx_list_t *coresList = bctbx_list_append(NULL, focus.getLc());
	coresList = bctbx_list_append(coresList, sender.getLc());
	coresList = bctbx_list_append(coresList, recipient.getLc());

	bctbx_list_t *senderHistory = linphone_chat_room_get_history(senderCr, 0);
	auto initialSenderMessages = bctbx_list_size(senderHistory);
	bctbx_list_free_with_data(senderHistory, (bctbx_list_free_func)linphone_chat_message_unref);

	bctbx_list_t *recipientHistory = linphone_chat_room_get_history(recipientCr, 0);
	auto initialRecipientMessages = bctbx_list_size(recipientHistory);
	bctbx_list_free_with_data(recipientHistory, (bctbx_list_free_func)linphone_chat_message_unref);

	int initialUnreadMessages = linphone_chat_room_get_unread_messages_count(recipientCr);

	auto sender_stat = sender.getStats();
	auto recipient_stat = recipient.getStats();

	std::list<LinphoneChatMessage *> messages;
	// Marie sends messages
	for (size_t i = 0; i < noMsg; i++) {
		const std::string text = basicText + std::to_string(i);
		messages.push_back(_send_message_ephemeral(senderCr, text.c_str(), TRUE));
	}

	auto expectedSenderHistoryCount = (noMsg + initialSenderMessages);
	BC_ASSERT_TRUE(
	    CoreManagerAssert({focus, sender, recipient}).wait([&senderHistory, &expectedSenderHistoryCount, senderCr] {
		    senderHistory = linphone_chat_room_get_history(senderCr, 0);
		    return (bctbx_list_size(senderHistory) == expectedSenderHistoryCount);
	    }));
	set_ephemeral_cbs(senderHistory);

	BC_ASSERT_TRUE(wait_for_list(coresList, &recipient.getStats().number_of_LinphoneMessageReceived,
	                             recipient_stat.number_of_LinphoneMessageReceived + static_cast<int>(noMsg), 11000));

	// Check that the message has been delivered to Pauline
	for (const auto &msg : messages) {
		BC_ASSERT_TRUE(CoreManagerAssert({focus, sender, recipient}).wait([msg] {
			return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
		}));
	}

	BC_ASSERT_TRUE(CoreManagerAssert({focus, sender, recipient}).wait([&, recipientCr] {
		return linphone_chat_room_get_unread_messages_count(recipientCr) ==
		       (static_cast<int>(noMsg) + initialUnreadMessages);
	}));

	auto expectedRecipientHistoryCount = (noMsg + initialRecipientMessages);
	BC_ASSERT_TRUE(CoreManagerAssert({focus, sender, recipient})
	                   .wait([&recipientHistory, &expectedRecipientHistoryCount, recipientCr] {
		                   recipientHistory = linphone_chat_room_get_history(recipientCr, 0);
		                   return (bctbx_list_size(recipientHistory) == expectedRecipientHistoryCount);
	                   }));
	BC_ASSERT_EQUAL(bctbx_list_size(recipientHistory), expectedRecipientHistoryCount, size_t, "%zu");
	set_ephemeral_cbs(recipientHistory);

	BC_ASSERT_TRUE(wait_for_list(coresList, &sender.getStats().number_of_LinphoneMessageDeliveredToUser,
	                             sender_stat.number_of_LinphoneMessageDeliveredToUser + static_cast<int>(noMsg),
	                             liblinphone_tester_sip_timeout));

	// Pauline marks the message as read, check that the state is now displayed on Marie's side
	linphone_chat_room_mark_as_read(recipientCr);
	BC_ASSERT_TRUE(wait_for_list(coresList, &sender.getStats().number_of_LinphoneMessageDisplayed,
	                             sender_stat.number_of_LinphoneMessageDisplayed + static_cast<int>(noMsg),
	                             liblinphone_tester_sip_timeout));

	BC_ASSERT_TRUE(wait_for_list(coresList, &sender.getStats().number_of_LinphoneChatRoomEphemeralTimerStarted,
	                             sender_stat.number_of_LinphoneChatRoomEphemeralTimerStarted + static_cast<int>(noMsg),
	                             liblinphone_tester_sip_timeout));
	BC_ASSERT_TRUE(
	    wait_for_list(coresList, &recipient.getStats().number_of_LinphoneChatRoomEphemeralTimerStarted,
	                  recipient_stat.number_of_LinphoneChatRoomEphemeralTimerStarted + static_cast<int>(noMsg),
	                  liblinphone_tester_sip_timeout));

	BC_ASSERT_TRUE(wait_for_list(coresList, &sender.getStats().number_of_LinphoneMessageEphemeralTimerStarted,
	                             sender_stat.number_of_LinphoneMessageEphemeralTimerStarted + static_cast<int>(noMsg),
	                             liblinphone_tester_sip_timeout));
	BC_ASSERT_TRUE(
	    wait_for_list(coresList, &recipient.getStats().number_of_LinphoneMessageEphemeralTimerStarted,
	                  recipient_stat.number_of_LinphoneMessageEphemeralTimerStarted + static_cast<int>(noMsg),
	                  liblinphone_tester_sip_timeout));

	BC_ASSERT_TRUE(wait_for_list(coresList, &sender.getStats().number_of_LinphoneChatRoomEphemeralDeleted,
	                             sender_stat.number_of_LinphoneChatRoomEphemeralDeleted + static_cast<int>(noMsg),
	                             15000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &recipient.getStats().number_of_LinphoneChatRoomEphemeralDeleted,
	                             recipient_stat.number_of_LinphoneChatRoomEphemeralDeleted + static_cast<int>(noMsg),
	                             15000));

	BC_ASSERT_TRUE(wait_for_list(coresList, &sender.getStats().number_of_LinphoneMessageEphemeralDeleted,
	                             sender_stat.number_of_LinphoneMessageEphemeralDeleted + static_cast<int>(noMsg),
	                             15000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &recipient.getStats().number_of_LinphoneMessageEphemeralDeleted,
	                             recipient_stat.number_of_LinphoneMessageEphemeralDeleted + static_cast<int>(noMsg),
	                             15000));

	bctbx_list_free_with_data(recipientHistory, (bctbx_list_free_func)linphone_chat_message_unref);
	bctbx_list_free_with_data(senderHistory, (bctbx_list_free_func)linphone_chat_message_unref);

	// wait a bit longer to detect side effect if any
	CoreManagerAssert({focus, sender, recipient}).waitUntil(chrono::seconds(2), [] { return false; });

	recipientHistory = linphone_chat_room_get_history(recipientCr, 0);
	BC_ASSERT_EQUAL(bctbx_list_size(recipientHistory), initialRecipientMessages, size_t, "%zu");
	senderHistory = linphone_chat_room_get_history(senderCr, 0);
	BC_ASSERT_EQUAL(bctbx_list_size(senderHistory), initialSenderMessages, size_t, "%zu");

	for (auto &msg : messages) {
		linphone_chat_message_unref(msg);
	}

	bctbx_list_free_with_data(recipientHistory, (bctbx_list_free_func)linphone_chat_message_unref);
	bctbx_list_free_with_data(senderHistory, (bctbx_list_free_func)linphone_chat_message_unref);
	bctbx_list_free(coresList);
}

bool checkChatroomCreation(const ConfCoreManager &core,
                           const int nbChatRooms,
                           const int participantNumber,
                           const std::string subject) {
	auto &chatRooms = core.getCore().getChatRooms();
	if (chatRooms.size() != static_cast<size_t>(nbChatRooms)) {
		return false;
	}
	for (auto chatRoom : chatRooms) {
		if (chatRoom->getState() != ConferenceInterface::State::Created) {
			return false;
		}
		if ((participantNumber >= 0) && (chatRoom->getConference()->getParticipantCount() != participantNumber)) {
			return false;
		}
		if (!subject.empty() && (chatRoom->getSubject() == subject)) {
			return false;
		}
	}
	return true;
}

bool checkChatroom(Focus &focus, const ConfCoreManager &core, const time_t baseJoiningTime) {
	const auto &chatRooms = core.getCore().getChatRooms();
	if (chatRooms.size() < 1) {
		return false;
	}

	for (const auto &chatRoom : chatRooms) {
		auto participants = chatRoom->getParticipants();
		if (focus.getLc() != core.getLc()) {
			participants.push_back(chatRoom->getMe());
		}
		for (const auto &participant : participants) {
			for (const auto &device : participant->getDevices()) {
				if (device->getState() != ParticipantDevice::State::Present) {
					return false;
				}
				if (device->isInConference() == false) {
					return false;
				}
				if ((baseJoiningTime >= 0) && (device->getTimeOfJoining() >= baseJoiningTime)) {
					return false;
				}
			}
		}
	}
	return true;
}

void sendMessageAndCheckReception(std::initializer_list<std::reference_wrapper<CoreManager>> coreMgrs,
                                  std::map<LinphoneCoreManager *, int> members,
                                  std::string text,
                                  std::shared_ptr<AbstractChatRoom> chatRoom) {
	LinphoneChatMessage *msg = ClientConference::sendTextMsg(chatRoom->toC(), text);
	BC_ASSERT_TRUE(CoreManagerAssert(coreMgrs).wait(
	    [msg] { return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered); }));

	const auto &confAddr = chatRoom->getConferenceAddress();
	const auto confAddrString = confAddr->toString();
	// TODO: uncomment when C++20 will be supported
	// for (const auto &[mgr, unreadCount] : members) {
	for (const auto &p : members) {
		auto mgr = p.first;
		auto unreadCount = p.second;
		ms_message("Checking that %s has received message '%s' in chatroom %s", linphone_core_get_identity(mgr->lc),
		           text.c_str(), confAddrString.c_str());
		LinphoneChatRoom *cr = linphone_core_search_chat_room(mgr->lc, nullptr, nullptr, confAddr->toC(), nullptr);
		BC_ASSERT_PTR_NOT_NULL(cr);
		if (cr) {
			BC_ASSERT_TRUE(CoreManagerAssert(coreMgrs).wait(
			    [cr, unreadCount] { return linphone_chat_room_get_unread_messages_count(cr) == unreadCount; }));
			LinphoneChatMessage *lastMsg = mgr->stat.last_received_chat_message;
			BC_ASSERT_PTR_NOT_NULL(lastMsg);
			if (lastMsg) {
				BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_utf8_text(lastMsg), text.c_str());
			}
		}
	}
	linphone_chat_message_unref(msg);
}

static const std::string message_begin("Network came back so I can chat again on chatroom ");
static void send_message_on_network_reachable(LinphoneCore *lc, bool_t reachable) {
	if (reachable) {
		const bctbx_list_t *chat_rooms = linphone_core_get_chat_rooms(lc);
		for (const bctbx_list_t *chat_room_it = chat_rooms; chat_room_it != NULL; chat_room_it = chat_room_it->next) {
			LinphoneChatRoom *chat_room = static_cast<LinphoneChatRoom *>(bctbx_list_get_data(chat_room_it));
			BC_ASSERT_PTR_NOT_NULL(chat_room);
			if (chat_room) {
				std::string msg_text = message_begin +
				                       AbstractChatRoom::toCpp(chat_room)->getConferenceAddress()->toString() +
				                       std::string(" Cheers, ") + linphone_core_get_identity(lc);
				LinphoneChatMessage *msg = ClientConference::sendTextMsg(chat_room, msg_text);
				linphone_chat_message_unref(msg);
			}
		}
	}
}

void group_chat_room_with_client_restart_base(bool encrypted,
                                              bool server_restart_before_participant_addition,
                                              bool change_conference_server_contact_address) {
	Focus focus("chloe_rc");
	{ // to make sure focus is destroyed after clients.
		const LinphoneTesterLimeAlgo lime_algo = encrypted ? C25519 : UNSET;
		ClientConference marie("marie_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference michelle("michelle_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference laure("laure_tcp_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference berthe("berthe_rc", focus.getConferenceFactoryAddress(), lime_algo);

		focus.registerAsParticipantDevice(marie);
		focus.registerAsParticipantDevice(michelle);
		focus.registerAsParticipantDevice(laure);
		focus.registerAsParticipantDevice(berthe);

		linphone_core_enable_gruu_in_conference_address(focus.getLc(), TRUE);
		linphone_core_set_add_admin_information_to_contact(marie.getLc(), TRUE);
		linphone_core_set_add_admin_information_to_contact(laure.getLc(), TRUE);

		bctbx_list_t *coresList = bctbx_list_append(NULL, focus.getLc());
		coresList = bctbx_list_append(coresList, marie.getLc());
		coresList = bctbx_list_append(coresList, michelle.getLc());
		coresList = bctbx_list_append(coresList, laure.getLc());
		coresList = bctbx_list_append(coresList, berthe.getLc());
		bctbx_list_t *participantsAddresses = NULL;
		Address michelleAddr = michelle.getIdentity();
		participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_ref(michelleAddr.toC()));
		Address bertheAddr = berthe.getIdentity();
		participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_ref(bertheAddr.toC()));

		stats initialMarieStats = marie.getStats();
		stats initialMichelleStats = michelle.getStats();
		stats initialBertheStats = berthe.getStats();
		stats initialLaureStats = laure.getStats();

		if (encrypted) {
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(marie.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(michelle.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(berthe.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(laure.getLc()));
		}

		int subscribe_expires_value_s = 15;
		linphone_config_set_int(linphone_core_get_config(marie.getLc()), "sip", "conference_subscribe_expires",
		                        subscribe_expires_value_s);
		linphone_config_set_int(linphone_core_get_config(laure.getLc()), "sip", "conference_subscribe_expires",
		                        subscribe_expires_value_s);
		linphone_config_set_int(linphone_core_get_config(michelle.getLc()), "sip", "conference_subscribe_expires",
		                        subscribe_expires_value_s);
		linphone_config_set_int(linphone_core_get_config(berthe.getLc()), "sip", "conference_subscribe_expires",
		                        subscribe_expires_value_s);

		// Marie creates a new group chat room
		const char *initialSubject = "Colleagues (characters: $ £ çà)";
		LinphoneChatRoom *marieCr = create_chat_room_client_side_with_expected_number_of_participants(
		    coresList, marie.getCMgr(), &initialMarieStats, participantsAddresses, initialSubject, 2, encrypted,
		    LinphoneChatRoomEphemeralModeDeviceManaged);
		LinphoneAddress *confAddr = linphone_address_clone(linphone_chat_room_get_conference_address(marieCr));

		// Check that the chat room is correctly created on Michelle's side and that the participants are added
		LinphoneChatRoom *michelleCr = check_creation_chat_room_client_side(
		    coresList, michelle.getCMgr(), &initialMichelleStats, confAddr, initialSubject, 2, FALSE);

		LinphoneChatRoom *bertheCr = check_creation_chat_room_client_side(
		    coresList, berthe.getCMgr(), &initialBertheStats, confAddr, initialSubject, 2, FALSE);

		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, laure, berthe}).wait([&focus] {
			for (const auto &chatRoom : focus.getCore().getChatRooms()) {
				for (const auto &participant : chatRoom->getParticipants()) {
					for (const auto &device : participant->getDevices()) {
						if (device->getState() != ParticipantDevice::State::Present) {
							return false;
						}
					}
				}
			}
			return true;
		}));

		BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneConferenceStateCreated,
		                             initialMichelleStats.number_of_LinphoneConferenceStateCreated + 1,
		                             liblinphone_tester_sip_timeout));

		BC_ASSERT_TRUE(wait_for_list(coresList, &berthe.getStats().number_of_LinphoneConferenceStateCreated,
		                             initialBertheStats.number_of_LinphoneConferenceStateCreated + 1,
		                             liblinphone_tester_sip_timeout));

		// Marie now changes the subject
		const char *newSubject = "Let's go drink a beer";
		linphone_chat_room_set_subject(marieCr, newSubject);
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_subject_changed,
		                             initialMarieStats.number_of_subject_changed + 1, liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_subject_changed,
		                             initialMichelleStats.number_of_subject_changed + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &berthe.getStats().number_of_subject_changed,
		                             initialBertheStats.number_of_subject_changed + 1, liblinphone_tester_sip_timeout));
		BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(marieCr), newSubject);
		BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(michelleCr), newSubject);
		BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(bertheCr), newSubject);

		BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(marieCr), 2, int, "%d");
		BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(michelleCr), 2, int, "%d");
		BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(bertheCr), 2, int, "%d");

		const std::initializer_list<std::reference_wrapper<ConfCoreManager>> cores2{focus, marie, michelle, berthe};
		for (const ConfCoreManager &core : cores2) {
			BC_ASSERT_TRUE(
			    CoreManagerAssert({focus, marie, michelle, berthe, laure})
			        .waitUntil(chrono::seconds(10), [&focus, &core] { return checkChatroom(focus, core, -1); }));
		};

		ClientConference michelle2("michelle_rc", focus.getConferenceFactoryAddress(), lime_algo);
		linphone_core_enable_gruu_in_conference_address(michelle2.getLc(), TRUE);
		linphone_config_set_int(linphone_core_get_config(michelle2.getLc()), "sip", "conference_subscribe_expires",
		                        subscribe_expires_value_s);
		stats initialMichelle2Stats = michelle2.getStats();
		coresList = bctbx_list_append(coresList, michelle2.getLc());
		if (encrypted) {
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(michelle2.getLc()));
		}

		if (change_conference_server_contact_address) {
			LinphoneAddress *new_contact_address = linphone_address_new("sip:tom.tommy.time@sip.tommy.org");
			linphone_account_set_contact_address(linphone_core_get_default_account(focus.getLc()), new_contact_address);
			linphone_address_unref(new_contact_address);
		}

		focus.registerAsParticipantDevice(michelle2);

		LinphoneAddress *michelle2Contact = linphone_address_clone(
		    linphone_account_get_contact_address(linphone_core_get_default_account(michelle2.getLc())));
		char *michelle2ContactString = linphone_address_as_string(michelle2Contact);
		ms_message("%s is adding device %s", linphone_core_get_identity(focus.getLc()), michelle2ContactString);
		ms_free(michelle2ContactString);
		focus.registerAsParticipantDevice(michelle2);

		// Notify chat room that a participant has registered
		bctbx_list_t *devices = NULL;
		const LinphoneAddress *deviceAddr = linphone_proxy_config_get_contact(michelle.getDefaultProxyConfig());
		LinphoneParticipantDeviceIdentity *identity =
		    linphone_factory_create_participant_device_identity(linphone_factory_get(), deviceAddr, "");
		bctbx_list_t *specs = linphone_core_get_linphone_specs_list(michelle.getLc());
		linphone_participant_device_identity_set_capability_descriptor_2(identity, specs);
		bctbx_list_free_with_data(specs, ms_free);
		devices = bctbx_list_append(devices, identity);

		deviceAddr = linphone_proxy_config_get_contact(michelle2.getDefaultProxyConfig());
		identity = linphone_factory_create_participant_device_identity(linphone_factory_get(), deviceAddr, "");
		specs = linphone_core_get_linphone_specs_list(michelle2.getLc());
		linphone_participant_device_identity_set_capability_descriptor_2(identity, specs);
		bctbx_list_free_with_data(specs, ms_free);
		devices = bctbx_list_append(devices, identity);

		for (auto chatRoom : focus.getCore().getChatRooms()) {
			linphone_chat_room_set_participant_devices(chatRoom->toC(), michelle.getCMgr()->identity, devices);
		}
		bctbx_list_free_with_data(devices, (bctbx_list_free_func)belle_sip_object_unref);

		LinphoneChatRoom *michelle2Cr = check_creation_chat_room_client_side(
		    coresList, michelle2.getCMgr(), &initialMichelle2Stats, confAddr, newSubject, 2, FALSE);
		BC_ASSERT_PTR_NOT_NULL(michelle2Cr);
		BC_ASSERT_TRUE(wait_for_list(coresList, &michelle2.getStats().number_of_LinphoneConferenceStateCreated,
		                             initialMichelle2Stats.number_of_LinphoneConferenceStateCreated + 1,
		                             liblinphone_tester_sip_timeout));

		const std::initializer_list<std::reference_wrapper<ConfCoreManager>> cores3{focus, marie, michelle, michelle2,
		                                                                            berthe};
		for (const ConfCoreManager &core : cores3) {
			BC_ASSERT_TRUE(
			    CoreManagerAssert({focus, marie, michelle, michelle2, berthe, laure})
			        .waitUntil(chrono::seconds(10), [&focus, &core] { return checkChatroom(focus, core, -1); }));

			const bctbx_list_t *chat_rooms = linphone_core_get_chat_rooms(core.getLc());
			for (const bctbx_list_t *chat_room_it = chat_rooms; chat_room_it != NULL;
			     chat_room_it = chat_room_it->next) {
				const LinphoneChatRoom *chat_room =
				    static_cast<const LinphoneChatRoom *>(bctbx_list_get_data(chat_room_it));
				BC_ASSERT_PTR_NOT_NULL(chat_room);
				if (chat_room) {
					const char *chat_room_identifier = linphone_chat_room_get_identifier(chat_room);
					LinphoneChatRoom *found_chat_room =
					    linphone_core_search_chat_room_by_identifier(core.getLc(), chat_room_identifier);
					BC_ASSERT_PTR_NOT_NULL(found_chat_room);
					BC_ASSERT_PTR_EQUAL(chat_room, found_chat_room);
				}
			}
		};

		// Test invalid peer address
		BC_ASSERT_PTR_NULL(linphone_core_search_chat_room_by_identifier(
		    marie.getLc(), "==sip:toto@sip.conference.org##sip:me@sip.local.org"));
		// Test inexistent chat room identifier
		BC_ASSERT_PTR_NULL(linphone_core_search_chat_room_by_identifier(
		    marie.getLc(), "sip:toto@sip.conference.org##sip:me@sip.local.org"));

		if (server_restart_before_participant_addition) {
			initialMarieStats = marie.getStats();
			initialMichelleStats = michelle.getStats();
			initialMichelle2Stats = michelle2.getStats();
			initialBertheStats = berthe.getStats();

			ms_message("%s is restarting its core before %s adds %s to the chatroom",
			           linphone_core_get_identity(focus.getLc()), linphone_core_get_identity(marie.getLc()),
			           linphone_core_get_identity(laure.getLc()));
			coresList = bctbx_list_remove(coresList, focus.getLc());
			// Restart flexisip
			focus.reStart();
			coresList = bctbx_list_append(coresList, focus.getLc());

			// Wait for subscriptions between the clients and the server to expire.
			// In fact clients are not notified that the server restarted so from the server standpoint, the events are
			// in the Terminated state
			CoreManagerAssert({focus, marie, berthe, michelle, michelle2, laure})
			    .waitUntil(chrono::seconds(subscribe_expires_value_s + 1), [] { return false; });

			BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneSubscriptionActive,
			                             initialMarieStats.number_of_LinphoneSubscriptionActive + 1,
			                             (subscribe_expires_value_s * 1000)));

			BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneSubscriptionActive,
			                             initialMichelleStats.number_of_LinphoneSubscriptionActive + 1,
			                             (subscribe_expires_value_s * 1000)));

			BC_ASSERT_TRUE(wait_for_list(coresList, &michelle2.getStats().number_of_LinphoneSubscriptionActive,
			                             initialMichelle2Stats.number_of_LinphoneSubscriptionActive + 1,
			                             (subscribe_expires_value_s * 1000)));

			BC_ASSERT_TRUE(wait_for_list(coresList, &berthe.getStats().number_of_LinphoneSubscriptionActive,
			                             initialBertheStats.number_of_LinphoneSubscriptionActive + 1,
			                             (subscribe_expires_value_s * 1000)));
		}

		initialMarieStats = marie.getStats();
		initialMichelleStats = michelle.getStats();
		initialMichelle2Stats = michelle2.getStats();
		initialBertheStats = berthe.getStats();
		initialLaureStats = laure.getStats();

		Address laureAddr = laure.getIdentity();
		participantsAddresses = bctbx_list_append(NULL, linphone_address_ref(laureAddr.toC()));
		ms_message("%s is adding participant %s", linphone_core_get_identity(marie.getLc()),
		           linphone_core_get_identity(laure.getLc()));
		linphone_chat_room_add_participants(marieCr, participantsAddresses);
		bctbx_list_free(participantsAddresses);

		LinphoneChatRoom *laureCr = check_creation_chat_room_client_side(coresList, laure.getCMgr(), &initialLaureStats,
		                                                                 confAddr, newSubject, 3, FALSE);
		BC_ASSERT_PTR_NOT_NULL(laureCr);

		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneConferenceStateCreationPending,
		                             initialLaureStats.number_of_LinphoneConferenceStateCreationPending + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneConferenceStateCreated,
		                             initialLaureStats.number_of_LinphoneConferenceStateCreated + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneChatRoomConferenceJoined,
		                             initialLaureStats.number_of_LinphoneChatRoomConferenceJoined + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_chat_room_participants_added,
		                             initialMarieStats.number_of_chat_room_participants_added + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &berthe.getStats().number_of_chat_room_participants_added,
		                             initialBertheStats.number_of_chat_room_participants_added + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_chat_room_participants_added,
		                             initialMichelleStats.number_of_chat_room_participants_added + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &michelle2.getStats().number_of_chat_room_participants_added,
		                             initialMichelle2Stats.number_of_chat_room_participants_added + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_chat_room_participant_devices_added,
		                             initialMarieStats.number_of_chat_room_participant_devices_added + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &berthe.getStats().number_of_chat_room_participant_devices_added,
		                             initialBertheStats.number_of_chat_room_participant_devices_added + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_chat_room_participant_devices_added,
		                             initialMichelleStats.number_of_chat_room_participant_devices_added + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &michelle2.getStats().number_of_chat_room_participant_devices_added,
		                             initialMichelle2Stats.number_of_chat_room_participant_devices_added + 1,
		                             liblinphone_tester_sip_timeout));
		if (marieCr) {
			BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(marieCr), 3, int, "%d");
		}
		if (bertheCr) {
			BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(bertheCr), 3, int, "%d");
		}
		if (michelleCr) {
			BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(michelleCr), 3, int, "%d");
		}
		if (michelle2Cr) {
			BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(michelle2Cr), 3, int, "%d");
		}
		if (laureCr) {
			BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(laureCr), 3, int, "%d");
		}

		const std::initializer_list<std::reference_wrapper<ConfCoreManager>> cores{focus,     marie, michelle,
		                                                                           michelle2, laure, berthe};
		for (const ConfCoreManager &core : cores) {
			BC_ASSERT_TRUE(
			    CoreManagerAssert({focus, marie, michelle, michelle2, laure, berthe})
			        .waitUntil(chrono::seconds(10), [&focus, &core] { return checkChatroom(focus, core, -1); }));
			for (auto chatRoom : core.getCore().getChatRooms()) {
				BC_ASSERT_EQUAL(chatRoom->getParticipants().size(), ((focus.getLc() == core.getLc())) ? 4 : 3, size_t,
				                "%zu");
				BC_ASSERT_STRING_EQUAL(chatRoom->getSubject().c_str(), newSubject);
			}
		};

		// wait a bit longer to detect side effect if any
		CoreManagerAssert({focus, marie, michelle, michelle2, laure, berthe}).waitUntil(chrono::seconds(1), [] {
			return false;
		});

		time_t participantAddedTime = ms_time(nullptr);

		// wait a bit longer to detect side effect if any
		CoreManagerAssert({focus, marie, michelle, michelle2, laure, berthe}).waitUntil(chrono::seconds(10), [] {
			return false;
		});

		ms_message("%s is restarting its core", linphone_core_get_identity(focus.getLc()));
		coresList = bctbx_list_remove(coresList, focus.getLc());
		// Restart flexisip
		focus.reStart();
		coresList = bctbx_list_append(coresList, focus.getLc());
		BC_ASSERT_EQUAL(focus.getCore().getChatRooms().size(), 1, size_t, "%zu");
		for (auto chatRoom : focus.getCore().getChatRooms()) {
			BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(chatRoom->toC()), 4, int, "%d");
		}

		// Michelle2 mutes all chat rooms
		for (auto chatRoom : michelle2.getCore().getChatRooms()) {
			chatRoom->setIsMuted(true);
		}
		michelle2ContactString = linphone_address_as_string(michelle2Contact);
		ms_message("%s is restarting its core", michelle2ContactString);
		ms_free(michelle2ContactString);
		coresList = bctbx_list_remove(coresList, michelle2.getLc());
		// Restart michelle
		michelle2.reStart();
		coresList = bctbx_list_append(coresList, michelle2.getLc());

		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, laure, berthe, michelle, michelle2}).wait([&michelle2] {
			return checkChatroomCreation(michelle2, 1);
		}));

		LinphoneAddress *michelle2DeviceAddr = linphone_account_get_contact_address(michelle2.getDefaultAccount());
		michelle2Cr = michelle2.searchChatRoom(michelle2DeviceAddr, confAddr);
		BC_ASSERT_PTR_NOT_NULL(michelle2Cr);
		for (const ConfCoreManager &core : cores) {
			BC_ASSERT_TRUE(checkChatroom(focus, core, participantAddedTime));
			for (auto chatRoom : core.getCore().getChatRooms()) {
				BC_ASSERT_EQUAL(chatRoom->getParticipants().size(), ((focus.getLc() == core.getLc())) ? 4 : 3, size_t,
				                "%zu");
				BC_ASSERT_STRING_EQUAL(chatRoom->getSubject().c_str(), newSubject);
			}
		}

		LinphoneChatMessage *msg = ClientConference::sendTextMsg(michelle2Cr, "back with you");
		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, michelle2, laure, berthe}).wait([msg] {
			return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
		}));
		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, michelle2, laure, berthe}).wait([marieCr] {
			return linphone_chat_room_get_unread_messages_count(marieCr) == 1;
		}));
		linphone_chat_message_unref(msg);
		msg = NULL;

		msg = ClientConference::sendTextMsg(marieCr, "welcome back");
		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, michelle2, laure, berthe}).wait([msg] {
			return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
		}));
		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, michelle2, laure, berthe}).wait([michelleCr] {
			return linphone_chat_room_get_unread_messages_count(michelleCr) == 1;
		}));
		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, michelle2, laure, berthe}).wait([michelle2Cr] {
			return linphone_chat_room_get_unread_messages_count(michelle2Cr) == 1;
		}));
		linphone_chat_message_unref(msg);
		msg = NULL;

		msg = ClientConference::sendTextMsg(michelleCr, "my new device");
		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, michelle2, laure, berthe}).wait([msg] {
			return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
		}));
		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, michelle2, laure, berthe}).wait([marieCr] {
			return linphone_chat_room_get_unread_messages_count(marieCr) == 2;
		}));
		linphone_chat_message_unref(msg);

		CoreManagerAssert({focus, marie, michelle, michelle2, laure, berthe}).waitUntil(std::chrono::seconds(2), [] {
			return false;
		});

		ms_message("%s is turning its network off and sends a message immediately after the network comes up",
		           linphone_core_get_identity(marie.getLc()));
		linphone_core_set_network_reachable(marie.getLc(), FALSE);

		// wait a bit longer to detect side effect if any
		CoreManagerAssert({focus, marie, michelle, michelle2, laure, berthe}).waitUntil(chrono::seconds(2), [] {
			return false;
		});

		// wait until the chatroom is loaded
		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, michelle2, laure, berthe}).wait([&marie] {
			return marie.getCore().getChatRooms().size() == 1;
		}));

		const LinphoneAccount *marie_default_account = linphone_core_get_default_account(marie.getLc());
		BC_ASSERT_FALSE(CoreManagerAssert({focus, marie, michelle, michelle2, laure, berthe})
		                    .waitUntil(chrono::seconds(2), [marie_default_account] {
			                    LinphoneRegistrationState account_state =
			                        linphone_account_get_state(marie_default_account);
			                    return (account_state != LinphoneRegistrationNone);
		                    }));

		BC_ASSERT_FALSE(linphone_core_is_network_reachable(marie.getLc()));

		LinphoneCoreCbs *marie_core_cbs = linphone_factory_create_core_cbs(linphone_factory_get());
		linphone_core_cbs_set_network_reachable(marie_core_cbs, send_message_on_network_reachable);
		linphone_core_add_callbacks(marie.getLc(), marie_core_cbs);
		linphone_core_cbs_unref(marie_core_cbs);

		initialMarieStats = marie.getStats();
		linphone_core_set_network_reachable(marie.getLc(), TRUE);

		for (auto chatRoom : marie.getCore().getChatRooms()) {
			std::shared_ptr<ChatMessage> msg = chatRoom->getLastChatMessageInHistory();
			std::string msgText(linphone_chat_message_get_utf8_text(L_GET_C_BACK_PTR(msg)));
			size_t pos = msgText.find(message_begin);
			BC_ASSERT_EQUAL(pos, 0, size_t, "%0zu");
			ChatMessage::State expected_msg_state =
			    (encrypted) ? ChatMessage::State::Queued : ChatMessage::State::Delivered;
			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, michelle2, laure, berthe})
			                   .waitUntil(chrono::seconds(10),
			                              [msg, expected_msg_state] { return msg->getState() == expected_msg_state; }));
			BC_ASSERT_PTR_NOT_NULL(linphone_chat_message_get_event_log(L_GET_C_BACK_PTR(msg)));
		}

		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, michelle2, laure, berthe})
		                   .waitUntil(chrono::seconds(10), [marie_default_account] {
			                   LinphoneRegistrationState account_state =
			                       linphone_account_get_state(marie_default_account);
			                   return (account_state == LinphoneRegistrationOk);
		                   }));

		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneSubscriptionActive,
		                             initialMarieStats.number_of_LinphoneSubscriptionActive + 1,
		                             liblinphone_tester_sip_timeout));

		for (auto chatRoom : marie.getCore().getChatRooms()) {
			std::shared_ptr<ChatMessage> msg = chatRoom->getLastChatMessageInHistory();
			std::string msgText(linphone_chat_message_get_utf8_text(L_GET_C_BACK_PTR(msg)));
			size_t pos = msgText.find(message_begin);
			BC_ASSERT_EQUAL(pos, 0, size_t, "%0zu");
			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, michelle2, laure, berthe})
			                   .waitUntil(chrono::seconds(10),
			                              [msg] { return msg->getState() == ChatMessage::State::Delivered; }));
		}

		const std::initializer_list<std::reference_wrapper<ConfCoreManager>> cores4{michelle, michelle2, laure, berthe};
		for (const ConfCoreManager &core : cores4) {
			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, michelle2, berthe, laure})
			                   .waitUntil(chrono::seconds(10), [&core] {
				                   LinphoneChatMessage *lastMsg = core.getStats().last_received_chat_message;
				                   bool ret = false;
				                   if (lastMsg) {
					                   std::string msgText(linphone_chat_message_get_utf8_text(lastMsg));
					                   ret = (msgText.find(message_begin) == 0);
				                   }
				                   return ret;
			                   }));
		}

		stats initialFocusStats = focus.getStats();
		coresList = bctbx_list_remove(coresList, marie.getLc());
		// Restart Marie
		ms_message("%s is restarting its core", linphone_core_get_identity(marie.getLc()));
		marie.reStart();
		coresList = bctbx_list_append(coresList, marie.getLc());

		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneSubscriptionActive, 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneSubscriptionActive,
		                             initialFocusStats.number_of_LinphoneSubscriptionActive + 1,
		                             liblinphone_tester_sip_timeout));

		// wait a bit longer to detect side effect if any
		CoreManagerAssert({focus, marie, michelle, michelle2, laure, berthe}).waitUntil(chrono::seconds(2), [] {
			return false;
		});

		LinphoneAddress *marieDeviceAddress =
		    linphone_account_get_contact_address(linphone_core_get_default_account(marie.getLc()));
		marieCr = marie.searchChatRoom(marieDeviceAddress, confAddr);
		BC_ASSERT_PTR_NOT_NULL(marieCr);

		if (marieCr) {
			std::string marieMsgText("> It's me again");
			msg = ClientConference::sendTextMsg(marieCr, marieMsgText);
			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, michelle2, laure, berthe}).wait([msg] {
				return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
			}));
			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, michelle2, laure, berthe}).wait([michelleCr] {
				return linphone_chat_room_get_unread_messages_count(michelleCr) == 3;
			}));
			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, michelle2, laure, berthe}).wait([michelle2Cr] {
				return linphone_chat_room_get_unread_messages_count(michelle2Cr) == 3;
			}));
			linphone_chat_message_unref(msg);
			msg = NULL;

			for (const ConfCoreManager &core : cores4) {
				BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, michelle2, berthe, laure})
				                   .waitUntil(chrono::seconds(10), [&core, &marieMsgText] {
					                   LinphoneChatMessage *lastMsg = core.getStats().last_received_chat_message;
					                   bool ret = false;
					                   if (lastMsg) {
						                   std::string msgText(linphone_chat_message_get_utf8_text(lastMsg));
						                   ret = (msgText == marieMsgText);
					                   }
					                   return ret;
				                   }));
			}
		}

		initialFocusStats = focus.getStats();
		coresList = bctbx_list_remove(coresList, marie.getLc());
		// Restart Laure
		ms_message("%s is restarting its core", linphone_core_get_identity(marie.getLc()));
		marie.reStart();
		coresList = bctbx_list_append(coresList, marie.getLc());

		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneSubscriptionActive, 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneSubscriptionActive,
		                             initialFocusStats.number_of_LinphoneSubscriptionActive + 1,
		                             liblinphone_tester_sip_timeout));

		// wait a bit longer to detect side effect if any
		CoreManagerAssert({focus, marie, michelle, michelle2, laure, berthe}).waitUntil(chrono::seconds(2), [] {
			return false;
		});

		marieDeviceAddress = linphone_account_get_contact_address(linphone_core_get_default_account(marie.getLc()));
		marieCr = marie.searchChatRoom(marieDeviceAddress, confAddr);
		BC_ASSERT_PTR_NOT_NULL(marieCr);

		if (marieCr) {
			std::string marieMsgText("> It's me again after restarting my core");
			msg = ClientConference::sendTextMsg(marieCr, marieMsgText);
			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, michelle2, laure, berthe}).wait([msg] {
				return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
			}));
			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, michelle2, laure, berthe}).wait([michelleCr] {
				return linphone_chat_room_get_unread_messages_count(michelleCr) == 4;
			}));
			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, michelle2, laure, berthe}).wait([michelle2Cr] {
				return linphone_chat_room_get_unread_messages_count(michelle2Cr) == 4;
			}));
			linphone_chat_message_unref(msg);
			msg = NULL;

			for (const ConfCoreManager &core : cores4) {
				BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, michelle2, berthe, laure})
				                   .waitUntil(chrono::seconds(10), [&core, &marieMsgText] {
					                   LinphoneChatMessage *lastMsg = core.getStats().last_received_chat_message;
					                   bool ret = false;
					                   if (lastMsg) {
						                   std::string msgText(linphone_chat_message_get_utf8_text(lastMsg));
						                   ret = (msgText == marieMsgText);
					                   }
					                   return ret;
				                   }));
			}
		}

		for (auto chatRoom : focus.getCore().getChatRooms()) {
			for (auto participant : chatRoom->getParticipants()) {
				//  force deletion by removing devices
				std::shared_ptr<Address> participantAddress = participant->getAddress();
				linphone_chat_room_set_participant_devices(chatRoom->toC(), participantAddress->toC(), NULL);
			}
		}

		// wait until chatroom is deleted server side
		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, michelle2, laure, berthe}).wait([&focus] {
			return focus.getCore().getChatRooms().size() == 0;
		}));

		// wait a bit longer to detect side effect if any
		CoreManagerAssert({focus, marie, michelle, michelle2, laure, berthe}).waitUntil(chrono::seconds(2), [] {
			return false;
		});

		// to avoid creation attempt of a new chatroom
		auto config = focus.getDefaultProxyConfig();
		linphone_proxy_config_edit(config);
		linphone_proxy_config_set_conference_factory_uri(config, NULL);
		linphone_proxy_config_done(config);

		linphone_address_unref(confAddr);
		linphone_address_unref(michelle2Contact);
		bctbx_list_free(coresList);
	}
}

static void server_chat_room_participant_added_sip_error(LinphoneChatRoom *cr,
                                                         BCTBX_UNUSED(const LinphoneEventLog *event_log)) {
	LinphoneCoreManager *mgr = (LinphoneCoreManager *)linphone_chat_room_get_user_data(cr);
	if (mgr) {
		bctbx_list_t *participants = linphone_chat_room_get_participants(cr);
		if (linphone_chat_room_find_participant(cr, mgr->identity)) {
			ms_message("Turning off network for core %s", linphone_core_get_identity(mgr->lc));
			linphone_core_set_network_reachable(mgr->lc, FALSE);
			linphone_chat_room_set_user_data(cr, NULL);
		}
		if (participants) {
			bctbx_list_free_with_data(participants, (bctbx_list_free_func)linphone_participant_unref);
		}
	}
}

static void server_core_chat_room_state_changed_sip_error(BCTBX_UNUSED(LinphoneCore *core),
                                                          LinphoneChatRoom *cr,
                                                          LinphoneChatRoomState state) {
	switch (state) {
		case LinphoneChatRoomStateInstantiated: {
			LinphoneChatRoomCbs *cbs = linphone_factory_create_chat_room_cbs(linphone_factory_get());
			linphone_chat_room_cbs_set_participant_added(cbs, server_chat_room_participant_added_sip_error);
			linphone_chat_room_add_callbacks(cr, cbs);
			linphone_chat_room_cbs_unref(cbs);
			break;
		}
		default:
			break;
	}
}

static void client_chat_room_session_state_changed_sip_error(LinphoneChatRoom *cr,
                                                             LinphoneCallState cstate,
                                                             BCTBX_UNUSED(const char *msg)) {
	if ((linphone_chat_room_get_user_data(cr) == (void *)1) && (cstate == LinphoneCallConnected)) {
		LinphoneCore *core = linphone_chat_room_get_core(cr);
		ms_message("Turning off network for core %s", linphone_core_get_identity(core));
		linphone_core_set_network_reachable(core, FALSE);
		linphone_chat_room_set_user_data(cr, (void *)0);
	}
}

static void client_core_chat_room_state_changed_sip_error(BCTBX_UNUSED(LinphoneCore *core),
                                                          LinphoneChatRoom *cr,
                                                          LinphoneChatRoomState state) {
	switch (state) {
		case LinphoneChatRoomStateInstantiated: {
			linphone_chat_room_set_user_data(cr, (void *)1);
			LinphoneChatRoomCbs *cbs = linphone_factory_create_chat_room_cbs(linphone_factory_get());
			linphone_chat_room_cbs_set_session_state_changed(cbs, client_chat_room_session_state_changed_sip_error);
			linphone_chat_room_add_callbacks(cr, cbs);
			linphone_chat_room_cbs_unref(cbs);
			break;
		}
		default:
			break;
	}
}

void group_chat_room_with_sip_errors_base(bool invite_error, bool subscribe_error, bool encrypted, bool organizer) {
	Focus focus("chloe_rc");
	{ // to make sure focus is destroyed after clients.
		const LinphoneTesterLimeAlgo lime_algo = encrypted ? C25519 : UNSET;
		ClientConference marie("marie_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference pauline("pauline_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference laure("laure_tcp_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference berthe("berthe_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference michelle("michelle_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference michelle2("michelle_rc", focus.getConferenceFactoryAddress(), lime_algo);

		stats initialFocusStats = focus.getStats();
		stats initialMarieStats = marie.getStats();
		stats initialMichelleStats = michelle.getStats();
		stats initialMichelle2Stats = michelle2.getStats();
		stats initialLaureStats = laure.getStats();
		stats initialBertheStats = berthe.getStats();
		stats initialPaulineStats = pauline.getStats();

		bctbx_list_t *coresList = bctbx_list_append(NULL, focus.getLc());
		coresList = bctbx_list_append(coresList, marie.getLc());
		coresList = bctbx_list_append(coresList, michelle.getLc());
		coresList = bctbx_list_append(coresList, michelle2.getLc());
		coresList = bctbx_list_append(coresList, pauline.getLc());
		coresList = bctbx_list_append(coresList, laure.getLc());
		coresList = bctbx_list_append(coresList, berthe.getLc());

		if (encrypted) {
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(marie.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(pauline.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(laure.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(michelle.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(michelle2.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(berthe.getLc()));
		}

		linphone_core_set_network_reachable(marie.getLc(), FALSE);
		linphone_core_set_network_reachable(berthe.getLc(), FALSE);

		char *spec = bctbx_strdup_printf("groupchat/1.1");
		linphone_core_remove_linphone_spec(marie.getLc(), "groupchat");
		linphone_core_add_linphone_spec(marie.getLc(), spec);
		linphone_core_remove_linphone_spec(berthe.getLc(), "groupchat");
		linphone_core_add_linphone_spec(berthe.getLc(), spec);
		bctbx_free(spec);

		linphone_core_set_network_reachable(marie.getLc(), TRUE);
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneRegistrationOk,
		                             initialMarieStats.number_of_LinphoneRegistrationOk + 1,
		                             liblinphone_tester_sip_timeout));
		linphone_core_set_network_reachable(berthe.getLc(), TRUE);
		BC_ASSERT_TRUE(wait_for_list(coresList, &berthe.getStats().number_of_LinphoneRegistrationOk,
		                             initialBertheStats.number_of_LinphoneRegistrationOk + 1,
		                             liblinphone_tester_sip_timeout));

		initialMarieStats = marie.getStats();
		initialBertheStats = berthe.getStats();

		ClientConference &sipErrorClient = (organizer) ? marie : pauline;
		std::list<LinphoneCoreManager *> shutdownNetworkClients;
		std::list<stats> initialStatsList;
		if (invite_error) {
			shutdownNetworkClients.push_back(michelle2.getCMgr());
			initialStatsList.push_back(michelle2.getStats());
			shutdownNetworkClients.push_back(berthe.getCMgr());
			initialStatsList.push_back(berthe.getStats());
		} else if (subscribe_error) {
			shutdownNetworkClients.push_back(sipErrorClient.getCMgr());
			initialStatsList.push_back(marie.getStats());
			shutdownNetworkClients.push_back(michelle2.getCMgr());
			initialStatsList.push_back(michelle2.getStats());
			shutdownNetworkClients.push_back(berthe.getCMgr());
			initialStatsList.push_back(berthe.getStats());
		}

		stats initialSipErrorClientStats = sipErrorClient.getStats();

		focus.registerAsParticipantDevice(marie);
		focus.registerAsParticipantDevice(michelle);
		focus.registerAsParticipantDevice(michelle2);
		focus.registerAsParticipantDevice(pauline);
		focus.registerAsParticipantDevice(berthe);

		if (invite_error) {
			LinphoneCoreCbs *cbs = linphone_factory_create_core_cbs(linphone_factory_get());
			if (organizer) {
				linphone_core_cbs_set_chat_room_state_changed(cbs, server_core_chat_room_state_changed_sip_error);
				linphone_core_add_callbacks(focus.getLc(), cbs);
			} else {
				linphone_core_cbs_set_chat_room_state_changed(cbs, client_core_chat_room_state_changed_sip_error);
				linphone_core_add_callbacks(sipErrorClient.getLc(), cbs);
			}
			linphone_core_cbs_unref(cbs);
		}

		linphone_im_notif_policy_enable_all(linphone_core_get_im_notif_policy(marie.getLc()));
		linphone_im_notif_policy_enable_all(linphone_core_get_im_notif_policy(michelle.getLc()));
		linphone_im_notif_policy_enable_all(linphone_core_get_im_notif_policy(michelle2.getLc()));
		linphone_im_notif_policy_enable_all(linphone_core_get_im_notif_policy(pauline.getLc()));
		linphone_im_notif_policy_enable_all(linphone_core_get_im_notif_policy(laure.getLc()));
		linphone_im_notif_policy_enable_all(linphone_core_get_im_notif_policy(berthe.getLc()));

		bctbx_list_t *participantsAddresses = NULL;
		Address michelleAddr = michelle.getIdentity();
		participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_ref(michelleAddr.toC()));
		Address bertheAddr = berthe.getIdentity();
		participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_ref(bertheAddr.toC()));
		Address paulineAddr = pauline.getIdentity();
		participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_ref(paulineAddr.toC()));

		const char *initialSubject = "Colleagues (characters: $ £ çà)";
		int participantsAddressesSize = static_cast<int>(bctbx_list_size(participantsAddresses));
		LinphoneChatRoomParams *params = linphone_core_create_default_chat_room_params(marie.getLc());

		linphone_chat_room_params_enable_encryption(params, encrypted);
		linphone_chat_room_params_set_ephemeral_mode(params, LinphoneChatRoomEphemeralModeDeviceManaged);
		linphone_chat_room_params_set_backend(params, LinphoneChatRoomBackendFlexisipChat);
		linphone_chat_room_params_enable_group(params, participantsAddressesSize > 1 ? TRUE : FALSE);
		// Marie creates a new group chat room
		LinphoneChatRoom *marieCr =
		    linphone_core_create_chat_room_2(marie.getLc(), params, initialSubject, participantsAddresses);
		linphone_chat_room_params_unref(params);
		if (marieCr) linphone_chat_room_unref(marieCr);

		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe}).wait([&focus] {
			return focus.getCore().getChatRooms().size() == 1;
		}));

		LinphoneAddress *confAddr = NULL;
		for (auto chatRoom : focus.getCore().getChatRooms()) {
			linphone_chat_room_set_user_data(chatRoom->toC(), sipErrorClient.getCMgr());
			confAddr = linphone_address_clone(linphone_chat_room_get_conference_address(chatRoom->toC()));
		}

		for (const auto &client : shutdownNetworkClients) {
			stats &initialStats = initialStatsList.front();
			BC_ASSERT_TRUE(wait_for_list(coresList, &client->stat.number_of_LinphoneConferenceStateCreated,
			                             initialStats.number_of_LinphoneConferenceStateCreated + 1,
			                             liblinphone_tester_sip_timeout));
			char *proxy_contact_str = linphone_address_as_string(
			    linphone_account_get_contact_address(linphone_core_get_default_account(client->lc)));
			ms_message("Disabling network of core %s (contact %s)", linphone_core_get_identity(client->lc),
			           proxy_contact_str);
			ms_free(proxy_contact_str);
			linphone_core_set_network_reachable(client->lc, FALSE);
			initialStatsList.pop_front();
		}

		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_chat_room_participants_added,
		                             initialFocusStats.number_of_chat_room_participants_added + 4, 5000));
		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_chat_room_participant_devices_added,
		                             initialFocusStats.number_of_chat_room_participant_devices_added + 5, 5000));

		if (!organizer) {
			BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneChatRoomSessionConnected,
			                             initialMarieStats.number_of_LinphoneChatRoomSessionConnected + 1,
			                             liblinphone_tester_sip_timeout));
		}

		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneChatRoomSessionConnected,
		                             initialPaulineStats.number_of_LinphoneChatRoomSessionConnected + 1,
		                             liblinphone_tester_sip_timeout));

		check_create_chat_room_client_side(coresList, marie.getCMgr(), marieCr, &initialMarieStats,
		                                   participantsAddresses, initialSubject, 3);

		// Check that the chat room is correctly created on Pauline's, Berthe's and Michelle's side and that the
		// participants are added
		LinphoneChatRoom *michelleCr = check_creation_chat_room_client_side(
		    coresList, michelle.getCMgr(), &initialMichelleStats, confAddr, initialSubject, 3, FALSE);
		BC_ASSERT_PTR_NOT_NULL(michelleCr);
		BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneConferenceStateCreated,
		                             initialMichelleStats.number_of_LinphoneConferenceStateCreated + 1,
		                             liblinphone_tester_sip_timeout));

		LinphoneChatRoom *michelle2Cr = check_creation_chat_room_client_side(
		    coresList, michelle2.getCMgr(), &initialMichelle2Stats, confAddr, initialSubject, 3, FALSE);
		BC_ASSERT_PTR_NOT_NULL(michelle2Cr);
		BC_ASSERT_TRUE(wait_for_list(coresList, &michelle2.getStats().number_of_LinphoneConferenceStateCreated,
		                             initialMichelle2Stats.number_of_LinphoneConferenceStateCreated + 1,
		                             liblinphone_tester_sip_timeout));

		LinphoneChatRoom *bertheCr = check_creation_chat_room_client_side(
		    coresList, berthe.getCMgr(), &initialBertheStats, confAddr, initialSubject, 3, FALSE);
		BC_ASSERT_PTR_NOT_NULL(bertheCr);
		BC_ASSERT_TRUE(wait_for_list(coresList, &berthe.getStats().number_of_LinphoneConferenceStateCreated,
		                             initialBertheStats.number_of_LinphoneConferenceStateCreated + 1,
		                             liblinphone_tester_sip_timeout));

		LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(
		    coresList, pauline.getCMgr(), &initialPaulineStats, confAddr, initialSubject, 3, FALSE);
		BC_ASSERT_PTR_NOT_NULL(paulineCr);
		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneConferenceStateCreated,
		                             initialPaulineStats.number_of_LinphoneConferenceStateCreated + 1,
		                             liblinphone_tester_sip_timeout));

		// wait a bit longer to detect side effect if any
		CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe})
		    .waitUntil(chrono::seconds(40), [] { return false; });

		std::string msg_text = "message michelle blabla";
		std::string msg_text_some_member_offline = msg_text;
		LinphoneChatMessage *msg = ClientConference::sendTextMsg(michelleCr, msg_text);
		BC_ASSERT_PTR_NOT_NULL(msg);
		if (msg) {
			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe}).wait([msg] {
				return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
			}));
		}
		if (invite_error || subscribe_error) {
			ClientConference &activeClient = (organizer) ? pauline : marie;
			ms_message("Checking that %s has received message '%s'", linphone_core_get_identity(activeClient.getLc()),
			           msg_text.c_str());
			auto activeClientCr = activeClient.searchChatRoom(nullptr, confAddr);
			BC_ASSERT_PTR_NOT_NULL(activeClientCr);
			if (activeClientCr) {
				BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe})
				                   .wait([activeClientCr] {
					                   return linphone_chat_room_get_unread_messages_count(activeClientCr) == 1;
				                   }));
				linphone_chat_room_mark_as_read(activeClientCr);
			}
			auto activeClientLastMsg = activeClient.getStats().last_received_chat_message;
			BC_ASSERT_PTR_NOT_NULL(activeClientLastMsg);
			if (activeClientLastMsg) {
				BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_utf8_text(activeClientLastMsg), msg_text.c_str());
			}

			BC_ASSERT_FALSE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneMessageDeliveredToUser,
			                              initialMichelleStats.number_of_LinphoneMessageDeliveredToUser + 1, 3000));
			BC_ASSERT_FALSE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneMessageDisplayed,
			                              initialMichelleStats.number_of_LinphoneMessageDisplayed + 1, 500));

			BC_ASSERT_PTR_NULL(sipErrorClient.getStats().last_received_chat_message);
			int expected_chat_session_released = 1;
			if (invite_error) {
				initialSipErrorClientStats = sipErrorClient.getStats();
				BC_ASSERT_EQUAL(sipErrorClient.getStats().number_of_LinphoneChatRoomSessionReleased,
				                expected_chat_session_released, int, "%0d");
			}
			char *sipErrorClient_proxy_contact_str = linphone_address_as_string(
			    linphone_account_get_contact_address(linphone_core_get_default_account(sipErrorClient.getLc())));
			LinphoneChatRoom *sipErrorClientCr = NULL;
			if (organizer) {
				ms_message("Enabling network of core %s (contact %s)",
				           linphone_core_get_identity(sipErrorClient.getLc()), sipErrorClient_proxy_contact_str);
				linphone_core_set_network_reachable(sipErrorClient.getLc(), TRUE);
				BC_ASSERT_TRUE(wait_for_list(coresList, &sipErrorClient.getStats().number_of_LinphoneRegistrationOk,
				                             initialSipErrorClientStats.number_of_LinphoneRegistrationOk + 1,
				                             liblinphone_tester_sip_timeout));
				sipErrorClientCr = check_creation_chat_room_client_side(coresList, sipErrorClient.getCMgr(),
				                                                        &initialSipErrorClientStats, confAddr,
				                                                        initialSubject, 3, organizer);
				marieCr = sipErrorClientCr;
			} else {
				ms_message("Core %s (contact %s) is restarting", linphone_core_get_identity(sipErrorClient.getLc()),
				           sipErrorClient_proxy_contact_str);
				coresList = bctbx_list_remove(coresList, sipErrorClient.getLc());
				// Restart the core
				sipErrorClient.reStart();
				linphone_im_notif_policy_enable_all(linphone_core_get_im_notif_policy(sipErrorClient.getLc()));

				// Notify chat room that a participant has registered
				const LinphoneAddress *sipErrorClient_contact_address =
				    linphone_account_get_contact_address(linphone_core_get_default_account(sipErrorClient.getLc()));
				focus.notifyParticipantDeviceRegistration(confAddr, sipErrorClient_contact_address);

				// The counter is reset when restarting the core
				expected_chat_session_released = 0;
				linphone_im_notif_policy_enable_all(linphone_core_get_im_notif_policy(sipErrorClient.getLc()));
				coresList = bctbx_list_append(coresList, sipErrorClient.getLc());
				BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe})
				                   .wait([&sipErrorClient, &confAddr] {
					                   auto sipErrorClientCr = sipErrorClient.searchChatRoom(nullptr, confAddr);
					                   return (sipErrorClientCr && (linphone_chat_room_get_state(sipErrorClientCr) ==
					                                                LinphoneChatRoomStateCreated));
				                   }));
				sipErrorClientCr = sipErrorClient.searchChatRoom(nullptr, confAddr);
				paulineCr = sipErrorClientCr;
			}
			ms_free(sipErrorClient_proxy_contact_str);

			BC_ASSERT_TRUE(
			    CoreManagerAssert({focus, marie, michelle, michelle2, pauline, laure, berthe})
			        .waitUntil(chrono::seconds(10), [&focus, &shutdownNetworkClients, &sipErrorClient] {
				        LinphoneAddress *sipErrorClient_contact_address = linphone_account_get_contact_address(
				            linphone_core_get_default_account(sipErrorClient.getLc()));
				        for (const auto &chatRoom : focus.getCore().getChatRooms()) {
					        if (chatRoom->getConference()->getParticipantCount() != 4) {
						        return false;
					        }
					        for (const auto &participant : chatRoom->getParticipants()) {
						        const auto &devices = participant->getDevices();
						        if (devices.size() == 0) {
							        return false;
						        }
						        for (const auto &device : devices) {
							        const auto &address = device->getAddress();
							        bool deviceIsOff = false;
							        for (const auto &offMgr : shutdownNetworkClients) {
								        LinphoneAddress *offMgr_contact_address = linphone_account_get_contact_address(
								            linphone_core_get_default_account(offMgr->lc));
								        if (!linphone_address_equal(offMgr_contact_address,
								                                    sipErrorClient_contact_address) &&
								            linphone_address_equal(offMgr_contact_address, address->toC())) {
									        deviceIsOff = true;
									        break;
								        }
							        }
							        auto expectedDeviceState = (deviceIsOff)
							                                       ? ParticipantDevice::State::ScheduledForJoining
							                                       : ParticipantDevice::State::Present;
							        if (device->getState() != expectedDeviceState) {
								        return false;
							        }
						        }
					        }
				        }
				        return true;
			        }));

			BC_ASSERT_PTR_NOT_NULL(sipErrorClientCr);
			if (invite_error) {
				BC_ASSERT_FALSE(wait_for_list(coresList,
				                              &sipErrorClient.getStats().number_of_LinphoneChatRoomSessionReleased,
				                              expected_chat_session_released + 1, 1000));
				BC_ASSERT_EQUAL(sipErrorClient.getStats().number_of_LinphoneChatRoomSessionReleased,
				                expected_chat_session_released, int, "%0d");
			}
			BC_ASSERT_FALSE(wait_for_list(
			    coresList, &sipErrorClient.getStats().number_of_LinphoneChatRoomSessionUpdating, 1, 1000));
			BC_ASSERT_EQUAL(sipErrorClient.getStats().number_of_LinphoneChatRoomSessionUpdating, 0, int, "%0d");

			if (sipErrorClientCr) {
				ms_message("Checking that %s has received message '%s'",
				           linphone_core_get_identity(sipErrorClient.getLc()), msg_text.c_str());
				BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe})
				                   .wait([sipErrorClientCr] {
					                   return linphone_chat_room_get_unread_messages_count(sipErrorClientCr) == 1;
				                   }));
			}
			LinphoneChatMessage *sipErrorClientLastMsg = sipErrorClient.getStats().last_received_chat_message;
			BC_ASSERT_PTR_NOT_NULL(sipErrorClientLastMsg);
			if (sipErrorClientLastMsg) {
				BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_utf8_text(sipErrorClientLastMsg), msg_text.c_str());
			}
		} else {
			const std::initializer_list<std::reference_wrapper<ClientConference>> cores{marie, pauline, michelle,
			                                                                            michelle2, berthe};
			for (ClientConference &core : cores) {
				ms_message("Checking that %s has received message '%s'", linphone_core_get_identity(core.getLc()),
				           msg_text.c_str());
				auto cr = core.searchChatRoom(nullptr, confAddr);
				BC_ASSERT_PTR_NOT_NULL(cr);
				if (cr) {
					int expected_unread_messages = 1;
					BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe})
					                   .wait([cr, expected_unread_messages] {
						                   return linphone_chat_room_get_unread_messages_count(cr) ==
						                          expected_unread_messages;
					                   }));
					if (core.getLc() != marie.getLc()) {
						linphone_chat_room_mark_as_read(cr);
					}
				}
				auto lastMsg = core.getStats().last_received_chat_message;
				BC_ASSERT_PTR_NOT_NULL(lastMsg);
				if (lastMsg) {
					BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_utf8_text(lastMsg), msg_text.c_str());
				}
			}

			BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneMessageDeliveredToUser,
			                             initialMichelleStats.number_of_LinphoneMessageDeliveredToUser + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_FALSE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneMessageDisplayed,
			                              initialMichelleStats.number_of_LinphoneMessageDisplayed + 1, 500));
		}

		marieCr = marie.searchChatRoom(nullptr, confAddr);
		BC_ASSERT_PTR_NOT_NULL(marieCr);
		initialLaureStats = laure.getStats();
		initialFocusStats = focus.getStats();
		if (marieCr) {
			ms_message("%s adds %s to the chatroom", linphone_core_get_identity(marie.getLc()),
			           linphone_core_get_identity(laure.getLc()));
			focus.registerAsParticipantDevice(laure);
			linphone_chat_room_add_participant(marieCr, laure.getCMgr()->identity);
		}
		LinphoneChatRoom *laureCr = check_creation_chat_room_client_side(coresList, laure.getCMgr(), &initialLaureStats,
		                                                                 confAddr, initialSubject, 4, FALSE);
		BC_ASSERT_PTR_NOT_NULL(laureCr);

		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_chat_room_participants_added,
		                             initialFocusStats.number_of_chat_room_participants_added + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_chat_room_participant_devices_added,
		                             initialFocusStats.number_of_chat_room_participant_devices_added + 1,
		                             liblinphone_tester_sip_timeout));

		BC_ASSERT_TRUE(
		    CoreManagerAssert({focus, marie, michelle, michelle2, pauline, laure, berthe})
		        .waitUntil(chrono::seconds(40), [&focus, &shutdownNetworkClients, &sipErrorClient] {
			        LinphoneAddress *sipErrorClient_contact_address =
			            linphone_account_get_contact_address(linphone_core_get_default_account(sipErrorClient.getLc()));
			        for (const auto &chatRoom : focus.getCore().getChatRooms()) {
				        if (chatRoom->getConference()->getParticipantCount() != 5) {
					        return false;
				        }
				        for (const auto &participant : chatRoom->getParticipants()) {
					        const auto &devices = participant->getDevices();
					        if (devices.size() == 0) {
						        return false;
					        }
					        for (const auto &device : devices) {
						        const auto &address = device->getAddress();
						        bool deviceIsOff = false;
						        for (const auto &offMgr : shutdownNetworkClients) {
							        LinphoneAddress *offMgr_contact_address = linphone_account_get_contact_address(
							            linphone_core_get_default_account(offMgr->lc));
							        if (!linphone_address_equal(offMgr_contact_address,
							                                    sipErrorClient_contact_address) &&
							            linphone_address_equal(offMgr_contact_address, address->toC())) {
								        deviceIsOff = true;
								        break;
							        }
						        }
						        auto expectedDeviceState = (deviceIsOff) ? ParticipantDevice::State::ScheduledForJoining
						                                                 : ParticipantDevice::State::Present;
						        if (device->getState() != expectedDeviceState) {
							        return false;
						        }
					        }
				        }
			        }
			        return true;
		        }));

		msg_text = "message laure blabla";
		LinphoneChatMessage *msg2 = ClientConference::sendTextMsg(laureCr, msg_text);
		BC_ASSERT_PTR_NOT_NULL(msg2);
		if (msg2) {
			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe}).wait([msg2] {
				return (linphone_chat_message_get_state(msg2) == LinphoneChatMessageStateDelivered);
			}));

			const std::initializer_list<std::reference_wrapper<ClientConference>> cores{marie, pauline, michelle};
			for (ClientConference &core : cores) {
				ms_message("Checking that %s has received message '%s'", linphone_core_get_identity(core.getLc()),
				           msg_text.c_str());
				auto cr = core.searchChatRoom(nullptr, confAddr);
				BC_ASSERT_PTR_NOT_NULL(cr);
				if (cr) {
					int expected_unread_messages = (core.getCMgr() == sipErrorClient.getCMgr()) ? 2 : 1;
					BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe})
					                   .wait([cr, expected_unread_messages] {
						                   return linphone_chat_room_get_unread_messages_count(cr) ==
						                          expected_unread_messages;
					                   }));
					linphone_chat_room_mark_as_read(cr);
				}
				auto lastMsg = core.getStats().last_received_chat_message;
				BC_ASSERT_PTR_NOT_NULL(lastMsg);
				if (lastMsg) {
					BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_utf8_text(lastMsg), msg_text.c_str());
				}
			}

			BC_ASSERT_FALSE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneMessageDisplayed,
			                              initialLaureStats.number_of_LinphoneMessageDisplayed + 1, 3000));

			if (!invite_error && !subscribe_error) {
				const std::initializer_list<std::reference_wrapper<ClientConference>> cores{michelle2, berthe};
				for (ClientConference &core : cores) {
					ms_message("Checking that %s has received message '%s'", linphone_core_get_identity(core.getLc()),
					           msg_text.c_str());
					auto cr = core.searchChatRoom(nullptr, confAddr);
					BC_ASSERT_PTR_NOT_NULL(cr);
					if (cr) {
						int expected_unread_messages = (core.getCMgr() == marie.getCMgr()) ? 2 : 1;
						BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe})
						                   .wait([cr, expected_unread_messages] {
							                   return linphone_chat_room_get_unread_messages_count(cr) ==
							                          expected_unread_messages;
						                   }));
						linphone_chat_room_mark_as_read(cr);
					}
					auto lastMsg = core.getStats().last_received_chat_message;
					BC_ASSERT_PTR_NOT_NULL(lastMsg);
					if (lastMsg) {
						BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_utf8_text(lastMsg), msg_text.c_str());
					}
				}

				BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneMessageDisplayed,
				                             initialLaureStats.number_of_LinphoneMessageDisplayed + 1,
				                             liblinphone_tester_sip_timeout));
			}

			// If an INVITE or SUBSCRIBE error occurred, only Marie and Pauline read the message, otherwise Berthe did
			// so too
			size_t expected_msg_displayed_count = (invite_error || subscribe_error) ? 2 : 3;
			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe})
			                   .wait([msg, expected_msg_displayed_count] {
				                   bctbx_list_t *displayed_list = linphone_chat_message_get_participants_by_imdn_state(
				                       msg, LinphoneChatMessageStateDisplayed);
				                   size_t displayed = bctbx_list_size(displayed_list);
				                   bctbx_list_free_with_data(
				                       displayed_list, (bctbx_list_free_func)linphone_participant_imdn_state_unref);
				                   return (displayed == expected_msg_displayed_count);
			                   }));

			// If an INVITE or SUBSCRIBE error occurred, only Marie, Michelle and Pauline read the message, otherwise
			// Berthe did so too
			size_t expected_msg2_displayed_count = (invite_error || subscribe_error) ? 3 : 4;
			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe})
			                   .wait([msg2, expected_msg2_displayed_count] {
				                   bctbx_list_t *displayed_list = linphone_chat_message_get_participants_by_imdn_state(
				                       msg2, LinphoneChatMessageStateDisplayed);
				                   size_t displayed = bctbx_list_size(displayed_list);
				                   bctbx_list_free_with_data(
				                       displayed_list, (bctbx_list_free_func)linphone_participant_imdn_state_unref);
				                   return (displayed == expected_msg2_displayed_count);
			                   }));
		}

		if (invite_error || subscribe_error) {
			char *michelle2_proxy_contact_str = linphone_address_as_string(
			    linphone_account_get_contact_address(linphone_core_get_default_account(michelle2.getLc())));
			ms_message("Enabling network of core %s (contact %s)", linphone_core_get_identity(michelle2.getLc()),
			           michelle2_proxy_contact_str);
			ms_free(michelle2_proxy_contact_str);
			initialMichelle2Stats = michelle2.getStats();
			linphone_core_set_network_reachable(michelle2.getLc(), TRUE);
			BC_ASSERT_TRUE(wait_for_list(coresList, &michelle2.getStats().number_of_LinphoneRegistrationOk,
			                             initialMichelle2Stats.number_of_LinphoneRegistrationOk + 1,
			                             liblinphone_tester_sip_timeout));
			michelle2Cr = check_creation_chat_room_client_side(coresList, michelle2.getCMgr(), &initialMichelle2Stats,
			                                                   confAddr, initialSubject, 4, FALSE);
			BC_ASSERT_PTR_NOT_NULL(michelle2Cr);
			BC_ASSERT_FALSE(
			    wait_for_list(coresList, &michelle2.getStats().number_of_LinphoneChatRoomSessionUpdating, 1, 1000));
			BC_ASSERT_EQUAL(michelle2.getStats().number_of_LinphoneChatRoomSessionUpdating, 0, int, "%0d");

			char *berthe_proxy_contact_str = linphone_address_as_string(
			    linphone_account_get_contact_address(linphone_core_get_default_account(berthe.getLc())));
			ms_message("Enabling network of core %s (contact %s)", linphone_core_get_identity(berthe.getLc()),
			           berthe_proxy_contact_str);
			ms_free(berthe_proxy_contact_str);
			initialBertheStats = berthe.getStats();
			linphone_core_set_network_reachable(berthe.getLc(), TRUE);
			BC_ASSERT_TRUE(wait_for_list(coresList, &berthe.getStats().number_of_LinphoneRegistrationOk,
			                             initialBertheStats.number_of_LinphoneRegistrationOk + 1,
			                             liblinphone_tester_sip_timeout));
			bertheCr = check_creation_chat_room_client_side(coresList, berthe.getCMgr(), &initialBertheStats, confAddr,
			                                                initialSubject, 4, FALSE);
			BC_ASSERT_PTR_NOT_NULL(bertheCr);
			BC_ASSERT_FALSE(
			    wait_for_list(coresList, &berthe.getStats().number_of_LinphoneChatRoomSessionUpdating, 1, 1000));
			BC_ASSERT_EQUAL(berthe.getStats().number_of_LinphoneChatRoomSessionUpdating, 0, int, "%0d");
		}

		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, michelle2, pauline, laure, berthe})
		                   .waitUntil(chrono::seconds(10), [&focus] {
			                   auto expectedDeviceState = ParticipantDevice::State::Present;
			                   for (const auto &chatRoom : focus.getCore().getChatRooms()) {
				                   if (chatRoom->getConference()->getParticipantCount() != 5) {
					                   return false;
				                   }
				                   for (const auto &participant : chatRoom->getParticipants()) {
					                   const auto &devices = participant->getDevices();
					                   if (devices.size() == 0) {
						                   return false;
					                   }
					                   for (const auto &device : devices) {
						                   if (device->getState() != expectedDeviceState) {
							                   return false;
						                   }
					                   }
				                   }
			                   }
			                   return true;
		                   }));

		LinphoneChatMessage *michelle2LastMsg = NULL;
		if (!invite_error) {
			BC_ASSERT_TRUE(
			    CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe}).wait([michelle2Cr] {
				    return linphone_chat_room_get_history_size(michelle2Cr) == 2;
			    }));
			michelle2LastMsg = michelle2.getStats().last_received_chat_message;
			BC_ASSERT_PTR_NOT_NULL(michelle2LastMsg);
			if (michelle2LastMsg) {
				BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_utf8_text(michelle2LastMsg), msg_text.c_str());
			}
			linphone_chat_room_mark_as_read(michelle2Cr);
		}

		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe}).wait([bertheCr] {
			return linphone_chat_room_get_history_size(bertheCr) == 2;
		}));

		LinphoneChatMessage *bertheLastMsg = berthe.getStats().last_received_chat_message;
		BC_ASSERT_PTR_NOT_NULL(bertheLastMsg);
		if (bertheLastMsg) {
			BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_utf8_text(bertheLastMsg), msg_text.c_str());
		}
		linphone_chat_room_mark_as_read(bertheCr);

		BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneMessageDisplayed,
		                             initialMichelleStats.number_of_LinphoneMessageDisplayed + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneMessageDisplayed,
		                             initialLaureStats.number_of_LinphoneMessageDisplayed + 1,
		                             liblinphone_tester_sip_timeout));

		if (msg) {
			linphone_chat_message_unref(msg);
			msg = nullptr;
		}
		if (msg2) {
			linphone_chat_message_unref(msg2);
			msg2 = nullptr;
		}

		initialMarieStats = marie.getStats();
		initialMichelleStats = michelle.getStats();
		initialMichelle2Stats = michelle2.getStats();
		initialLaureStats = laure.getStats();
		initialBertheStats = berthe.getStats();
		initialPaulineStats = pauline.getStats();

		// Marie now changes the subject
		const char *newSubject = "Let's go drink a beer";
		linphone_chat_room_set_subject(marieCr, newSubject);
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_subject_changed,
		                             initialMarieStats.number_of_subject_changed + 1, liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_subject_changed,
		                             initialMichelleStats.number_of_subject_changed + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &michelle2.getStats().number_of_subject_changed,
		                             initialMichelle2Stats.number_of_subject_changed + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_chat_room_subject_changed,
		                             initialPaulineStats.number_of_chat_room_subject_changed + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_subject_changed,
		                             initialLaureStats.number_of_subject_changed + 1, liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &berthe.getStats().number_of_subject_changed,
		                             initialBertheStats.number_of_subject_changed + 1, liblinphone_tester_sip_timeout));

		const std::initializer_list<std::reference_wrapper<ClientConference>> members{marie,   michelle, michelle2,
		                                                                              pauline, laure,    berthe};
		for (ClientConference &core : members) {
			ms_message("Checking that %s has received the NOTIFY message to update the subject to '%s'",
			           linphone_core_get_identity(core.getLc()), newSubject);
			auto cr = core.searchChatRoom(nullptr, confAddr);
			BC_ASSERT_PTR_NOT_NULL(cr);
			if (cr) {
				BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(cr), newSubject);
				BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(cr), 4, int, "%d");
				bctbx_list_t *messages = linphone_chat_room_get_history(cr, 0);
				for (const bctbx_list_t *messageIt = messages; messageIt != NULL; messageIt = messageIt->next) {
					LinphoneChatMessage *message = static_cast<LinphoneChatMessage *>(bctbx_list_get_data(messageIt));
					BC_ASSERT_PTR_NOT_NULL(message);
					if (message) {
						const char *text = linphone_chat_message_get_utf8_text(message);
						// The first message sent by Michelle is not marked as Displayed by berthe and michelle2 because
						// the chatroom didn't fully created yet by the time it was sent
						LinphoneChatMessageState expectedState =
						    ((msg_text_some_member_offline == std::string(text)) &&
						     ((core.getLc() == berthe.getLc()) || (core.getLc() == michelle2.getLc())))
						        ? LinphoneChatMessageStateDelivered
						        : LinphoneChatMessageStateDisplayed;
						BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe})
						                   .wait([&message, &expectedState] {
							                   return (linphone_chat_message_get_state(message) == expectedState);
						                   }));
					}
				}
				bctbx_list_free_with_data(messages, (bctbx_list_free_func)linphone_chat_message_unref);
			}
		}

		msg_text = "message marie blabla";
		msg = ClientConference::sendTextMsg(marieCr, msg_text);
		BC_ASSERT_PTR_NOT_NULL(msg);
		if (msg) {
			const std::initializer_list<std::reference_wrapper<ClientConference>> cores{michelle, michelle2, pauline,
			                                                                            laure, berthe};
			for (ClientConference &core : cores) {
				ms_message("Checking that %s has received message '%s'", linphone_core_get_identity(core.getLc()),
				           msg_text.c_str());
				auto cr = core.searchChatRoom(nullptr, confAddr);
				BC_ASSERT_PTR_NOT_NULL(cr);
				if (cr) {
					int expected_unread_messages = 1;
					BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe})
					                   .wait([cr, expected_unread_messages] {
						                   return linphone_chat_room_get_unread_messages_count(cr) ==
						                          expected_unread_messages;
					                   }));
					linphone_chat_room_mark_as_read(cr);
				}
				auto lastMsg = core.getStats().last_received_chat_message;
				BC_ASSERT_PTR_NOT_NULL(lastMsg);
				if (lastMsg) {
					BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_utf8_text(lastMsg), msg_text.c_str());
				}
			}
			BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneMessageDisplayed,
			                             initialMarieStats.number_of_LinphoneMessageDisplayed + 1,
			                             liblinphone_tester_sip_timeout));
			linphone_chat_message_unref(msg);
			msg = nullptr;
		}

		msg_text = "message michelle2 blabla";
		msg = ClientConference::sendTextMsg(michelle2Cr, msg_text);
		BC_ASSERT_PTR_NOT_NULL(msg);
		if (msg) {
			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe}).wait([msg] {
				return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
			}));

			const std::initializer_list<std::reference_wrapper<ClientConference>> cores{marie, pauline, laure, berthe};
			for (ClientConference &core : cores) {
				ms_message("Checking that %s has received message '%s'", linphone_core_get_identity(core.getLc()),
				           msg_text.c_str());
				auto cr = core.searchChatRoom(nullptr, confAddr);
				BC_ASSERT_PTR_NOT_NULL(cr);
				if (cr) {
					int expected_unread_messages = 1;
					BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe})
					                   .wait([cr, expected_unread_messages] {
						                   return linphone_chat_room_get_unread_messages_count(cr) ==
						                          expected_unread_messages;
					                   }));
					linphone_chat_room_mark_as_read(cr);
				}
				auto lastMsg = core.getStats().last_received_chat_message;
				BC_ASSERT_PTR_NOT_NULL(lastMsg);
				if (lastMsg) {
					BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_utf8_text(lastMsg), msg_text.c_str());
				}
			}

			BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneMessageDisplayed,
			                             initialMichelleStats.number_of_LinphoneMessageDisplayed + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(coresList, &michelle2.getStats().number_of_LinphoneMessageDisplayed,
			                             initialMichelle2Stats.number_of_LinphoneMessageDisplayed + 1,
			                             liblinphone_tester_sip_timeout));
			linphone_chat_message_unref(msg);
			msg = nullptr;
		}

		msg_text = "message pauline blabla";
		msg = ClientConference::sendTextMsg(paulineCr, msg_text);
		BC_ASSERT_PTR_NOT_NULL(msg);
		if (msg) {
			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe}).wait([msg] {
				return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
			}));

			const std::initializer_list<std::reference_wrapper<ClientConference>> cores{michelle, michelle2, marie,
			                                                                            laure, berthe};
			for (ClientConference &core : cores) {
				ms_message("Checking that %s has received message '%s'", linphone_core_get_identity(core.getLc()),
				           msg_text.c_str());
				auto cr = core.searchChatRoom(nullptr, confAddr);
				BC_ASSERT_PTR_NOT_NULL(cr);
				if (cr) {
					int expected_unread_messages = 1;
					BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe})
					                   .wait([cr, expected_unread_messages] {
						                   return linphone_chat_room_get_unread_messages_count(cr) ==
						                          expected_unread_messages;
					                   }));
					linphone_chat_room_mark_as_read(cr);
				}
				auto lastMsg = core.getStats().last_received_chat_message;
				BC_ASSERT_PTR_NOT_NULL(lastMsg);
				if (lastMsg) {
					BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_utf8_text(lastMsg), msg_text.c_str());
				}
			}
			BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneMessageDisplayed,
			                             initialPaulineStats.number_of_LinphoneMessageDisplayed + 1,
			                             liblinphone_tester_sip_timeout));
			linphone_chat_message_unref(msg);
			msg = nullptr;
		}

		for (ClientConference &core : members) {
			ms_message("Checking that %s has received all messages and IMDN", linphone_core_get_identity(core.getLc()));
			auto cr = core.searchChatRoom(nullptr, confAddr);
			BC_ASSERT_PTR_NOT_NULL(cr);
			if (cr) {
				bctbx_list_t *messages = linphone_chat_room_get_history(cr, 0);
				for (const bctbx_list_t *messageIt = messages; messageIt != NULL; messageIt = messageIt->next) {
					LinphoneChatMessage *message = static_cast<LinphoneChatMessage *>(bctbx_list_get_data(messageIt));
					BC_ASSERT_PTR_NOT_NULL(message);
					if (message) {
						const char *text = linphone_chat_message_get_utf8_text(message);
						// The first message sent by Michelle is not marked as Displayed by berthe and michelle2 because
						// the chatroom didn't fully created yet by the time it was sent
						LinphoneChatMessageState expectedState =
						    ((msg_text_some_member_offline == std::string(text)) &&
						     ((core.getLc() == berthe.getLc()) || (core.getLc() == michelle2.getLc())))
						        ? LinphoneChatMessageStateDelivered
						        : LinphoneChatMessageStateDisplayed;
						BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe})
						                   .wait([&message, &expectedState] {
							                   return (linphone_chat_message_get_state(message) == expectedState);
						                   }));
					}
				}
				bctbx_list_free_with_data(messages, (bctbx_list_free_func)linphone_chat_message_unref);
			}
		}

		CoreManagerAssert({focus, marie}).waitUntil(std::chrono::seconds(1), [] { return false; });

		for (auto chatRoom : focus.getCore().getChatRooms()) {
			for (auto participant : chatRoom->getParticipants()) {
				//  force deletion by removing devices
				std::shared_ptr<Address> participantAddress = participant->getAddress();
				linphone_chat_room_set_participant_devices(chatRoom->toC(), participantAddress->toC(), NULL);
			}
		}

		// wait until chatroom is deleted server side
		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe}).wait([&focus] {
			return focus.getCore().getChatRooms().size() == 0;
		}));

		// wait a bit longer to detect side effect if any
		CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe})
		    .waitUntil(chrono::seconds(2), [] { return false; });

		linphone_address_unref(confAddr);

		// to avoid creation attempt of a new chatroom
		LinphoneProxyConfig *config = linphone_core_get_default_proxy_config(focus.getLc());
		linphone_proxy_config_edit(config);
		linphone_proxy_config_set_conference_factory_uri(config, NULL);
		linphone_proxy_config_done(config);

		bctbx_list_free(coresList);
	}
}

static void server_chat_room_session_state_changed_shutdown(LinphoneChatRoom *cr,
                                                            LinphoneCallState cstate,
                                                            BCTBX_UNUSED(const char *msg)) {
	if ((linphone_chat_room_get_user_data(cr) == (void *)1) && (cstate == LinphoneCallOutgoingProgress)) {
		LinphoneCore *core = linphone_chat_room_get_core(cr);
		ms_message("Turning off network for core %s", linphone_core_get_identity(core));
		linphone_core_set_network_reachable(core, FALSE);
		linphone_chat_room_set_user_data(cr, (void *)0);
	}
}

void group_chat_room_with_focus_shutdown_during_invitation_base(bool encrypted) {
	Focus focus("chloe_rc");
	{ // to make sure focus is destroyed after clients.
		const LinphoneTesterLimeAlgo lime_algo = encrypted ? C25519 : UNSET;
		ClientConference marie("marie_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference pauline("pauline_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference laure("laure_tcp_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference berthe("berthe_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference michelle("michelle_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference michelle2("michelle_rc", focus.getConferenceFactoryAddress(), lime_algo);

		stats initialFocusStats = focus.getStats();
		stats initialMarieStats = marie.getStats();
		stats initialMichelleStats = michelle.getStats();
		stats initialMichelle2Stats = michelle2.getStats();
		stats initialLaureStats = laure.getStats();
		stats initialBertheStats = berthe.getStats();
		stats initialPaulineStats = pauline.getStats();

		bctbx_list_t *coresList = bctbx_list_append(NULL, focus.getLc());
		coresList = bctbx_list_append(coresList, marie.getLc());
		coresList = bctbx_list_append(coresList, michelle.getLc());
		coresList = bctbx_list_append(coresList, michelle2.getLc());
		coresList = bctbx_list_append(coresList, pauline.getLc());
		coresList = bctbx_list_append(coresList, laure.getLc());
		coresList = bctbx_list_append(coresList, berthe.getLc());

		if (encrypted) {
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(marie.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(pauline.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(laure.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(michelle.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(michelle2.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(berthe.getLc()));
		}

		focus.registerAsParticipantDevice(marie);
		focus.registerAsParticipantDevice(michelle);
		focus.registerAsParticipantDevice(michelle2);
		focus.registerAsParticipantDevice(pauline);
		focus.registerAsParticipantDevice(berthe);

		linphone_im_notif_policy_enable_all(linphone_core_get_im_notif_policy(marie.getLc()));
		linphone_im_notif_policy_enable_all(linphone_core_get_im_notif_policy(michelle.getLc()));
		linphone_im_notif_policy_enable_all(linphone_core_get_im_notif_policy(michelle2.getLc()));
		linphone_im_notif_policy_enable_all(linphone_core_get_im_notif_policy(pauline.getLc()));
		linphone_im_notif_policy_enable_all(linphone_core_get_im_notif_policy(laure.getLc()));
		linphone_im_notif_policy_enable_all(linphone_core_get_im_notif_policy(berthe.getLc()));

		bctbx_list_t *participantsAddresses = NULL;
		Address michelleAddr = michelle.getIdentity();
		participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_ref(michelleAddr.toC()));
		Address bertheAddr = berthe.getIdentity();
		participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_ref(bertheAddr.toC()));
		Address paulineAddr = pauline.getIdentity();
		participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_ref(paulineAddr.toC()));

		const char *initialSubject = "Colleagues (characters: $ £ çà)";
		int participantsAddressesSize = static_cast<int>(bctbx_list_size(participantsAddresses));
		LinphoneChatRoomParams *params = linphone_core_create_default_chat_room_params(marie.getLc());

		linphone_chat_room_params_enable_encryption(params, encrypted);
		linphone_chat_room_params_set_ephemeral_mode(params, LinphoneChatRoomEphemeralModeDeviceManaged);
		linphone_chat_room_params_set_backend(params, LinphoneChatRoomBackendFlexisipChat);
		linphone_chat_room_params_enable_group(params, participantsAddressesSize > 1 ? TRUE : FALSE);
		// Marie creates a new group chat room
		LinphoneChatRoom *marieCr =
		    linphone_core_create_chat_room_2(marie.getLc(), params, initialSubject, participantsAddresses);
		linphone_chat_room_params_unref(params);
		if (marieCr) linphone_chat_room_unref(marieCr);

		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe}).wait([&focus] {
			return focus.getCore().getChatRooms().size() == 1;
		}));

		LinphoneAddress *confAddr = NULL;
		for (auto chatRoom : focus.getCore().getChatRooms()) {
			confAddr = linphone_address_clone(linphone_chat_room_get_conference_address(chatRoom->toC()));
		}

		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_chat_room_participants_added,
		                             initialFocusStats.number_of_chat_room_participants_added + 4, 5000));
		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_chat_room_participant_devices_added,
		                             initialFocusStats.number_of_chat_room_participant_devices_added + 5, 5000));

		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneChatRoomSessionConnected,
		                             initialMarieStats.number_of_LinphoneChatRoomSessionConnected + 1,
		                             liblinphone_tester_sip_timeout));

		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneChatRoomSessionConnected,
		                             initialPaulineStats.number_of_LinphoneChatRoomSessionConnected + 1,
		                             liblinphone_tester_sip_timeout));

		check_create_chat_room_client_side(coresList, marie.getCMgr(), marieCr, &initialMarieStats,
		                                   participantsAddresses, initialSubject, 3);

		// Check that the chat room is correctly created on Pauline's, Berthe's and Michelle's side and that the
		// participants are added
		LinphoneChatRoom *michelleCr = check_creation_chat_room_client_side(
		    coresList, michelle.getCMgr(), &initialMichelleStats, confAddr, initialSubject, 3, FALSE);
		BC_ASSERT_PTR_NOT_NULL(michelleCr);
		BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneConferenceStateCreated,
		                             initialMichelleStats.number_of_LinphoneConferenceStateCreated + 1,
		                             liblinphone_tester_sip_timeout));

		LinphoneChatRoom *michelle2Cr = check_creation_chat_room_client_side(
		    coresList, michelle2.getCMgr(), &initialMichelle2Stats, confAddr, initialSubject, 3, FALSE);
		BC_ASSERT_PTR_NOT_NULL(michelle2Cr);
		BC_ASSERT_TRUE(wait_for_list(coresList, &michelle2.getStats().number_of_LinphoneConferenceStateCreated,
		                             initialMichelle2Stats.number_of_LinphoneConferenceStateCreated + 1,
		                             liblinphone_tester_sip_timeout));

		LinphoneChatRoom *bertheCr = check_creation_chat_room_client_side(
		    coresList, berthe.getCMgr(), &initialBertheStats, confAddr, initialSubject, 3, FALSE);
		BC_ASSERT_PTR_NOT_NULL(bertheCr);
		BC_ASSERT_TRUE(wait_for_list(coresList, &berthe.getStats().number_of_LinphoneConferenceStateCreated,
		                             initialBertheStats.number_of_LinphoneConferenceStateCreated + 1,
		                             liblinphone_tester_sip_timeout));

		LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(
		    coresList, pauline.getCMgr(), &initialPaulineStats, confAddr, initialSubject, 3, FALSE);
		BC_ASSERT_PTR_NOT_NULL(paulineCr);
		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneConferenceStateCreated,
		                             initialPaulineStats.number_of_LinphoneConferenceStateCreated + 1,
		                             liblinphone_tester_sip_timeout));

		std::string msg_text = "message michelle blabla";
		LinphoneChatMessage *msg = ClientConference::sendTextMsg(michelleCr, msg_text);
		BC_ASSERT_PTR_NOT_NULL(msg);
		if (msg) {
			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe}).wait([msg] {
				return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
			}));

			const std::initializer_list<std::reference_wrapper<ClientConference>> cores{marie, pauline, berthe};
			for (ClientConference &core : cores) {
				ms_message("Checking that %s has received message '%s'", linphone_core_get_identity(core.getLc()),
				           msg_text.c_str());
				auto cr = core.searchChatRoom(nullptr, confAddr);
				BC_ASSERT_PTR_NOT_NULL(cr);
				if (cr) {
					int expected_unread_messages = 1;
					BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe})
					                   .wait([cr, expected_unread_messages] {
						                   return linphone_chat_room_get_unread_messages_count(cr) ==
						                          expected_unread_messages;
					                   }));
					linphone_chat_room_mark_as_read(cr);
				}
				auto lastMsg = core.getStats().last_received_chat_message;
				BC_ASSERT_PTR_NOT_NULL(lastMsg);
				if (lastMsg) {
					BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_utf8_text(lastMsg), msg_text.c_str());
				}
			}
			for (ClientConference &core : cores) {
				ms_message("Checking that %s has received all IMDNs concerning message '%s'",
				           linphone_core_get_identity(core.getLc()), msg_text.c_str());
				auto lastMsg = core.getStats().last_received_chat_message;
				BC_ASSERT_PTR_NOT_NULL(lastMsg);
				if (lastMsg) {
					BC_ASSERT_TRUE(
					    CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe}).wait([&lastMsg] {
						    return (linphone_chat_message_get_state(lastMsg) == LinphoneChatMessageStateDisplayed);
					    }));
				}
			}
			BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneMessageDisplayed,
			                             initialMichelleStats.number_of_LinphoneMessageDisplayed + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(coresList, &michelle2.getStats().number_of_LinphoneMessageDisplayed,
			                             initialMichelle2Stats.number_of_LinphoneMessageDisplayed + 1,
			                             liblinphone_tester_sip_timeout));
			linphone_chat_message_unref(msg);
			msg = nullptr;
		}

		msg_text = "message marie blabla";
		msg = ClientConference::sendTextMsg(marieCr, msg_text);
		BC_ASSERT_PTR_NOT_NULL(msg);
		if (msg) {
			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe}).wait([msg] {
				return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
			}));

			const std::initializer_list<std::reference_wrapper<ClientConference>> cores{michelle, michelle2, pauline,
			                                                                            berthe};
			for (ClientConference &core : cores) {
				ms_message("Checking that %s has received message '%s'", linphone_core_get_identity(core.getLc()),
				           msg_text.c_str());
				auto cr = core.searchChatRoom(nullptr, confAddr);
				BC_ASSERT_PTR_NOT_NULL(cr);
				if (cr) {
					int expected_unread_messages = 1;
					BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe})
					                   .wait([cr, expected_unread_messages] {
						                   return linphone_chat_room_get_unread_messages_count(cr) ==
						                          expected_unread_messages;
					                   }));
					linphone_chat_room_mark_as_read(cr);
				}
				auto lastMsg = core.getStats().last_received_chat_message;
				BC_ASSERT_PTR_NOT_NULL(lastMsg);
				if (lastMsg) {
					BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_utf8_text(lastMsg), msg_text.c_str());
				}
			}
			for (ClientConference &core : cores) {
				ms_message("Checking that %s has received all IMDNs concerning message '%s'",
				           linphone_core_get_identity(core.getLc()), msg_text.c_str());
				auto lastMsg = core.getStats().last_received_chat_message;
				BC_ASSERT_PTR_NOT_NULL(lastMsg);
				if (lastMsg) {
					BC_ASSERT_TRUE(
					    CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe}).wait([&lastMsg] {
						    return (linphone_chat_message_get_state(lastMsg) == LinphoneChatMessageStateDisplayed);
					    }));
				}
			}
			BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneMessageDisplayed,
			                             initialMarieStats.number_of_LinphoneMessageDisplayed + 1,
			                             liblinphone_tester_sip_timeout));
			linphone_chat_message_unref(msg);
			msg = nullptr;
		}

		for (const auto &chatRoom : focus.getCore().getChatRooms()) {
			const auto &cr = chatRoom->toC();
			linphone_chat_room_set_user_data(cr, (void *)1);
			LinphoneChatRoomCbs *cbs = linphone_factory_create_chat_room_cbs(linphone_factory_get());
			linphone_chat_room_cbs_set_session_state_changed(cbs, server_chat_room_session_state_changed_shutdown);
			linphone_chat_room_add_callbacks(cr, cbs);
			linphone_chat_room_cbs_unref(cbs);
		}

		initialLaureStats = laure.getStats();
		initialFocusStats = focus.getStats();
		if (marieCr) {
			ms_message("%s adds %s to the chatroom", linphone_core_get_identity(marie.getLc()),
			           linphone_core_get_identity(laure.getLc()));
			focus.registerAsParticipantDevice(laure);
			linphone_chat_room_add_participant(marieCr, laure.getCMgr()->identity);
		}
		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_chat_room_participants_added,
		                             initialFocusStats.number_of_chat_room_participants_added + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_chat_room_participant_devices_added,
		                             initialFocusStats.number_of_chat_room_participant_devices_added + 1,
		                             liblinphone_tester_sip_timeout));

		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, michelle2, pauline, laure, berthe})
		                   .waitUntil(chrono::seconds(40), [&focus, &laure] {
			                   for (const auto &chatRoom : focus.getCore().getChatRooms()) {
				                   if (chatRoom->getConference()->getParticipantCount() != 5) {
					                   return false;
				                   }
				                   for (const auto &participant : chatRoom->getParticipants()) {
					                   const auto &address = participant->getAddress();
					                   bool participantAdditionIncomplete =
					                       (linphone_address_weak_equal(address->toC(), laure.getCMgr()->identity));
					                   auto expectedDeviceState = (participantAdditionIncomplete)
					                                                  ? ParticipantDevice::State::ScheduledForJoining
					                                                  : ParticipantDevice::State::Present;
					                   const auto &devices = participant->getDevices();
					                   if (devices.size() == 0) {
						                   return false;
					                   }
					                   for (const auto &device : devices) {
						                   if (device->getState() != expectedDeviceState) {
							                   return false;
						                   }
					                   }
				                   }
			                   }
			                   return true;
		                   }));

		// wait a little bit to detect any side effects
		CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe})
		    .waitUntil(chrono::seconds(2), [] { return false; });
		;

		ms_message("%s turns its network on again", linphone_core_get_identity(focus.getLc()));
		linphone_core_set_network_reachable(focus.getLc(), TRUE);

		if (encrypted) {
			// wait for the subscription to expire in order to allow chatroom to send their IMDN correctly
			CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe})
			    .waitUntil(chrono::seconds(linphone_config_get_int(linphone_core_get_config(marie.getLc()), "sip",
			                                                       "conference_subscribe_expires", 60) +
			                               1),
			               [] { return false; });
			;
		}

		ms_message("%s restarts its core", linphone_core_get_identity(laure.getLc()));
		// Restart pauline so that she has to send an INVITE and BYE it to exit the chatroom
		coresList = bctbx_list_remove(coresList, laure.getLc());
		laure.reStart();
		coresList = bctbx_list_append(coresList, laure.getLc());

		linphone_im_notif_policy_enable_all(linphone_core_get_im_notif_policy(laure.getLc()));

		// Notify chat room that a participant has registered
		const LinphoneAddress *laure_contact_address =
		    linphone_account_get_contact_address(linphone_core_get_default_account(laure.getLc()));
		focus.notifyParticipantDeviceRegistration(confAddr, laure_contact_address);

		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, michelle2, pauline, laure, berthe})
		                   .waitUntil(chrono::seconds(10), [&focus] {
			                   for (const auto &chatRoom : focus.getCore().getChatRooms()) {
				                   auto expectedDeviceState = ParticipantDevice::State::Present;
				                   if (chatRoom->getConference()->getParticipantCount() != 5) {
					                   return false;
				                   }
				                   for (const auto &participant : chatRoom->getParticipants()) {
					                   for (const auto &device : participant->getDevices()) {
						                   if (device->getState() != expectedDeviceState) {
							                   return false;
						                   }
					                   }
				                   }
			                   }
			                   return true;
		                   }));

		LinphoneChatRoom *laureCr = laure.searchChatRoom(nullptr, confAddr);
		BC_ASSERT_PTR_NOT_NULL(laureCr);

		msg_text = "message laure blabla";
		msg = ClientConference::sendTextMsg(laureCr, msg_text);
		BC_ASSERT_PTR_NOT_NULL(msg);
		if (msg) {
			const std::initializer_list<std::reference_wrapper<ClientConference>> cores{michelle, michelle2, pauline,
			                                                                            marie, berthe};
			for (ClientConference &core : cores) {
				ms_message("Checking that %s has received message '%s'", linphone_core_get_identity(core.getLc()),
				           msg_text.c_str());
				auto cr = core.searchChatRoom(nullptr, confAddr);
				BC_ASSERT_PTR_NOT_NULL(cr);
				if (cr) {
					int expected_unread_messages = 1;
					BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe})
					                   .wait([cr, expected_unread_messages] {
						                   return linphone_chat_room_get_unread_messages_count(cr) ==
						                          expected_unread_messages;
					                   }));
					linphone_chat_room_mark_as_read(cr);
				}
				auto lastMsg = core.getStats().last_received_chat_message;
				BC_ASSERT_PTR_NOT_NULL(lastMsg);
				if (lastMsg) {
					BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_utf8_text(lastMsg), msg_text.c_str());
				}
			}
			for (ClientConference &core : cores) {
				ms_message("Checking that %s has received all IMDNs concerning message '%s'",
				           linphone_core_get_identity(core.getLc()), msg_text.c_str());
				auto lastMsg = core.getStats().last_received_chat_message;
				BC_ASSERT_PTR_NOT_NULL(lastMsg);
				if (lastMsg) {
					BC_ASSERT_TRUE(
					    CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe}).wait([&lastMsg] {
						    return (linphone_chat_message_get_state(lastMsg) == LinphoneChatMessageStateDisplayed);
					    }));
				}
			}
			BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneMessageDisplayed,
			                             initialMarieStats.number_of_LinphoneMessageDisplayed + 1,
			                             liblinphone_tester_sip_timeout));
			linphone_chat_message_unref(msg);
			msg = nullptr;
		}

		msg_text = "message michelle2 blabla";
		msg = ClientConference::sendTextMsg(michelle2Cr, msg_text);
		BC_ASSERT_PTR_NOT_NULL(msg);
		if (msg) {
			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe}).wait([msg] {
				return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
			}));

			const std::initializer_list<std::reference_wrapper<ClientConference>> cores{marie, pauline, laure, berthe};
			for (ClientConference &core : cores) {
				ms_message("Checking that %s has received message '%s'", linphone_core_get_identity(core.getLc()),
				           msg_text.c_str());
				auto cr = core.searchChatRoom(nullptr, confAddr);
				BC_ASSERT_PTR_NOT_NULL(cr);
				if (cr) {
					int expected_unread_messages = 1;
					BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe})
					                   .wait([cr, expected_unread_messages] {
						                   return linphone_chat_room_get_unread_messages_count(cr) ==
						                          expected_unread_messages;
					                   }));
					linphone_chat_room_mark_as_read(cr);
				}
				auto lastMsg = core.getStats().last_received_chat_message;
				BC_ASSERT_PTR_NOT_NULL(lastMsg);
				if (lastMsg) {
					BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_utf8_text(lastMsg), msg_text.c_str());
				}
			}
			for (ClientConference &core : cores) {
				ms_message("Checking that %s has received all IMDNs concerning message '%s'",
				           linphone_core_get_identity(core.getLc()), msg_text.c_str());
				auto lastMsg = core.getStats().last_received_chat_message;
				BC_ASSERT_PTR_NOT_NULL(lastMsg);
				if (lastMsg) {
					BC_ASSERT_TRUE(
					    CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe}).wait([&lastMsg] {
						    return (linphone_chat_message_get_state(lastMsg) == LinphoneChatMessageStateDisplayed);
					    }));
				}
			}
			BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneMessageDisplayed,
			                             initialMichelleStats.number_of_LinphoneMessageDisplayed + 1,
			                             liblinphone_tester_sip_timeout));

			BC_ASSERT_TRUE(wait_for_list(coresList, &michelle2.getStats().number_of_LinphoneMessageDisplayed,
			                             initialMichelle2Stats.number_of_LinphoneMessageDisplayed + 1,
			                             liblinphone_tester_sip_timeout));
			linphone_chat_message_unref(msg);
			msg = nullptr;
		}

		for (auto chatRoom : focus.getCore().getChatRooms()) {
			for (auto participant : chatRoom->getParticipants()) {
				//  force deletion by removing devices
				std::shared_ptr<Address> participantAddress = participant->getAddress();
				linphone_chat_room_set_participant_devices(chatRoom->toC(), participantAddress->toC(), NULL);
			}
		}

		// wait until chatroom is deleted server side
		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe}).wait([&focus] {
			return focus.getCore().getChatRooms().size() == 0;
		}));

		// wait a bit longer to detect side effect if any
		CoreManagerAssert({focus, marie, pauline, michelle, michelle2, laure, berthe})
		    .waitUntil(chrono::seconds(2), [] { return false; });

		linphone_address_unref(confAddr);

		// to avoid creation attempt of a new chatroom
		LinphoneProxyConfig *config = linphone_core_get_default_proxy_config(focus.getLc());
		linphone_proxy_config_edit(config);
		linphone_proxy_config_set_conference_factory_uri(config, NULL);
		linphone_proxy_config_done(config);

		bctbx_list_free(coresList);
	}
}

void group_chat_room_lime_server_message(bool encrypted) {
	Focus focus("chloe_rc");
	LinphoneChatMessage *msg;
	{ // to make sure focus is destroyed after clients.
		const LinphoneTesterLimeAlgo lime_algo = encrypted ? C25519 : UNSET;

		ClientConference marie("marie_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference pauline("pauline_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference laure("laure_tcp_rc", focus.getConferenceFactoryAddress(), lime_algo);

		ms_message("%s reinitializes its core to force angle brackets on addresses",
		           linphone_core_get_identity(laure.getLc()));
		linphone_core_manager_reinit(laure.getCMgr());

		// Force angle bracket on all SIP outputs
		linphone_config_set_int(linphone_core_get_config(laure.getLc()), "sip", "force_name_addr", 1);

		ms_message("%s configures and starts again its core", linphone_core_get_identity(laure.getLc()));
		laure.configure(focus.getConferenceFactoryAddress());
		linphone_core_manager_start(laure.getCMgr(), TRUE);

		ms_message("%s reinitializes its core to force angle brackets on addresses",
		           linphone_core_get_identity(marie.getLc()));
		linphone_core_manager_reinit(marie.getCMgr());

		// Force angle bracket on all SIP outputs
		linphone_config_set_int(linphone_core_get_config(marie.getLc()), "sip", "force_name_addr", 1);

		ms_message("%s configures and starts again its core", linphone_core_get_identity(marie.getLc()));
		marie.configure(focus.getConferenceFactoryAddress(), lime_algo);
		linphone_core_manager_start(marie.getCMgr(), TRUE);

		ms_message("%s reinitializes its core to force angle brackets on addresses",
		           linphone_core_get_identity(focus.getLc()));
		linphone_core_manager_reinit(focus.getCMgr());

		// Force angle bracket on all SIP outputs
		linphone_config_set_int(linphone_core_get_config(focus.getLc()), "sip", "force_name_addr", 1);

		ms_message("%s configures and starts again its core", linphone_core_get_identity(focus.getLc()));
		focus.configureFocus();
		linphone_core_enable_lime_x3dh(focus.getLc(), true);
		linphone_core_manager_start(focus.getCMgr(), TRUE);

		focus.registerAsParticipantDevice(marie);
		focus.registerAsParticipantDevice(pauline);
		focus.registerAsParticipantDevice(laure);

		stats marie_stat = marie.getStats();
		stats pauline_stat = pauline.getStats();
		stats laure_stat = laure.getStats();
		bctbx_list_t *coresList = bctbx_list_append(NULL, focus.getLc());
		coresList = bctbx_list_append(coresList, marie.getLc());
		coresList = bctbx_list_append(coresList, pauline.getLc());
		coresList = bctbx_list_append(coresList, laure.getLc());

		if (encrypted) {
			auto rawEncryptionSuccess = 0;

			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(marie.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(pauline.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(laure.getLc()));

			// Test the raw encryption/decryption
			auto marieEncryptionEngine = L_GET_CPP_PTR_FROM_C_OBJECT(marie.getCMgr()->lc)->getEncryptionEngine();
			char *deviceId = linphone_address_as_string_uri_only(
			    linphone_account_get_contact_address(linphone_core_get_default_account(marie.getLc())));
			std::string marieAddressString{deviceId};
			bctbx_free(deviceId);
			auto paulineEncryptionEngine = L_GET_CPP_PTR_FROM_C_OBJECT(pauline.getCMgr()->lc)->getEncryptionEngine();
			deviceId = linphone_address_as_string_uri_only(
			    linphone_account_get_contact_address(linphone_core_get_default_account(pauline.getLc())));
			std::string paulineAddressString{deviceId};
			bctbx_free(deviceId);

			std::string messageString = "This is my message to you Rudy";
			std::string ADString = "These are my AD to you Rudy";
			std::vector<uint8_t> message(messageString.cbegin(), messageString.cend());
			std::vector<uint8_t> AD(ADString.cbegin(), ADString.cend());
			std::vector<uint8_t> cipherText{};

			marieEncryptionEngine->rawEncrypt(
			    marieAddressString, std::list<std::string>{paulineAddressString}, message, AD,
			    [&rawEncryptionSuccess, &cipherText, paulineAddressString](
			        const bool status, std::unordered_map<std::string, std::vector<uint8_t>> cipherTexts) {
				    auto search = cipherTexts.find(paulineAddressString);
				    if (status && search != cipherTexts.end()) {
					    rawEncryptionSuccess++;
					    cipherText = cipherTexts[paulineAddressString];
				    }
			    });

			BC_ASSERT_TRUE(wait_for_list(coresList, &rawEncryptionSuccess, 1, x3dhServer_creationTimeout));
			if (rawEncryptionSuccess == 1) {
				// try to decrypt only if encryption was a success
				std::vector<uint8_t> plainText{};
				BC_ASSERT_TRUE(paulineEncryptionEngine->rawDecrypt(paulineAddressString, marieAddressString, AD,
				                                                   cipherText, plainText));
				std::string plainTextString{plainText.cbegin(), plainText.cend()};
				BC_ASSERT_TRUE(plainTextString == messageString);
			}
		}

		Address paulineAddr = pauline.getIdentity();
		Address laureAddr = laure.getIdentity();
		bctbx_list_t *participantsAddresses = bctbx_list_append(NULL, linphone_address_ref(paulineAddr.toC()));
		participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_ref(laureAddr.toC()));

		// Marie creates a new group chat room
		const char *initialSubject = "Colleagues";
		LinphoneChatRoom *marieCr =
		    create_chat_room_client_side(coresList, marie.getCMgr(), &marie_stat, participantsAddresses, initialSubject,
		                                 encrypted, LinphoneChatRoomEphemeralModeDeviceManaged);
		BC_ASSERT_PTR_NOT_NULL(marieCr);
		const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

		// Check that the chat room is correctly created on Pauline's side and that the participants are added
		LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline.getCMgr(), &pauline_stat,
		                                                                   confAddr, initialSubject, 2, FALSE);
		BC_ASSERT_PTR_NOT_NULL(paulineCr);
		LinphoneChatRoom *laureCr = check_creation_chat_room_client_side(coresList, laure.getCMgr(), &laure_stat,
		                                                                 confAddr, initialSubject, 2, FALSE);
		BC_ASSERT_PTR_NOT_NULL(laureCr);
		if (paulineCr && laureCr) {
			// Marie sends the message
			const char *marieMessage = "Hey ! What's up ?";
			msg = _send_message_ephemeral(marieCr, marieMessage, FALSE);
			BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneMessageReceived,
			                             pauline_stat.number_of_LinphoneMessageReceived + 1,
			                             liblinphone_tester_sip_timeout));
			LinphoneChatMessage *paulineLastMsg = pauline.getStats().last_received_chat_message;
			BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneMessageReceived,
			                             laure_stat.number_of_LinphoneMessageReceived + 1,
			                             liblinphone_tester_sip_timeout));
			LinphoneChatMessage *laureLastMsg = laure.getStats().last_received_chat_message;
			linphone_chat_message_unref(msg);
			if (paulineLastMsg && laureLastMsg) {
				// Check that the message was correctly decrypted if encrypted
				BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_utf8_text(paulineLastMsg), marieMessage);
				LinphoneAddress *marieAddr = marie.getCMgr()->identity;
				BC_ASSERT_TRUE(
				    linphone_address_weak_equal(marieAddr, linphone_chat_message_get_from_address(paulineLastMsg)));
				BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_utf8_text(laureLastMsg), marieMessage);
				BC_ASSERT_TRUE(
				    linphone_address_weak_equal(marieAddr, linphone_chat_message_get_from_address(laureLastMsg)));
			}
		}

		for (auto chatRoom : focus.getCore().getChatRooms()) {
			for (auto participant : chatRoom->getParticipants()) {
				//  force deletion by removing devices
				std::shared_ptr<Address> participantAddress = participant->getAddress();
				linphone_chat_room_set_participant_devices(chatRoom->toC(), participantAddress->toC(), NULL);
			}
		}

		// wait until chatroom is deleted server side
		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, laure}).wait([&focus] {
			return focus.getCore().getChatRooms().size() == 0;
		}));

		// wait a bit longer to detect side effect if any
		CoreManagerAssert({focus, marie, pauline, laure}).waitUntil(chrono::seconds(2), [] { return false; });

		// to avoid creation attempt of a new chatroom
		auto config = focus.getDefaultProxyConfig();
		linphone_proxy_config_edit(config);
		linphone_proxy_config_set_conference_factory_uri(config, NULL);
		linphone_proxy_config_done(config);

		bctbx_list_free(coresList);
	}
}

void one_on_one_chat_room_deletion_by_server_client_base(bool encrypted) {
	Focus focus("chloe_rc");
	LinphoneChatMessage *msg;
	{ // to make sure focus is destroyed after clients.
		const LinphoneTesterLimeAlgo lime_algo = encrypted ? C25519 : UNSET;

		linphone_core_enable_lime_x3dh(focus.getLc(), true);

		ClientConference marie("marie_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference pauline("pauline_rc", focus.getConferenceFactoryAddress(), lime_algo);

		focus.registerAsParticipantDevice(marie);
		focus.registerAsParticipantDevice(pauline);

		stats marie_stat = marie.getStats();
		stats pauline_stat = pauline.getStats();
		bctbx_list_t *coresList = bctbx_list_append(NULL, focus.getLc());
		coresList = bctbx_list_append(coresList, marie.getLc());
		coresList = bctbx_list_append(coresList, pauline.getLc());

		if (encrypted) {
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(marie.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(pauline.getLc()));
		}

		Address paulineAddr = pauline.getIdentity();
		bctbx_list_t *participantsAddresses = bctbx_list_append(NULL, linphone_address_ref(paulineAddr.toC()));

		// Marie creates a new group chat room
		const char *initialSubject = "Colleagues";
		LinphoneChatRoom *marieCr =
		    create_chat_room_client_side(coresList, marie.getCMgr(), &marie_stat, participantsAddresses, initialSubject,
		                                 encrypted, LinphoneChatRoomEphemeralModeDeviceManaged);
		BC_ASSERT_PTR_NOT_NULL(marieCr);
		const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

		// Check that the chat room is correctly created on Pauline's side and that the participants are added
		LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline.getCMgr(), &pauline_stat,
		                                                                   confAddr, initialSubject, 1, FALSE);
		BC_ASSERT_PTR_NOT_NULL(paulineCr);

		if (paulineCr && marieCr) {
			// Marie sends the message
			std::string marieMessage("Hey ! What's up ?");
			msg = ClientConference::sendTextMsg(marieCr, marieMessage);
			BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneMessageReceived,
			                             pauline_stat.number_of_LinphoneMessageReceived + 1,
			                             liblinphone_tester_sip_timeout));
			LinphoneChatMessage *paulineLastMsg = pauline.getStats().last_received_chat_message;
			linphone_chat_message_unref(msg);
			BC_ASSERT_PTR_NOT_NULL(paulineLastMsg);

			// Check that the message was correctly decrypted if encrypted
			BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_utf8_text(paulineLastMsg), marieMessage.c_str());
			LinphoneAddress *marieAddr = marie.getCMgr()->identity;
			BC_ASSERT_TRUE(
			    linphone_address_weak_equal(marieAddr, linphone_chat_message_get_from_address(paulineLastMsg)));

			LinphoneAddress *paulineLocalAddr = linphone_address_clone(linphone_chat_room_get_local_address(paulineCr));
			LinphoneAddress *paulinePeerAddr = linphone_address_clone(linphone_chat_room_get_peer_address(paulineCr));

			// Restart pauline so that she has to send an INVITE and BYE it to exit the chatroom
			coresList = bctbx_list_remove(coresList, pauline.getLc());
			pauline.reStart();
			coresList = bctbx_list_append(coresList, pauline.getLc());
			paulineCr = linphone_core_search_chat_room(pauline.getLc(), NULL, paulineLocalAddr, paulinePeerAddr, NULL);
			BC_ASSERT_PTR_NOT_NULL(paulineCr);

			LinphoneChatRoom *focusCr =
			    linphone_core_search_chat_room(focus.getLc(), NULL, NULL, paulinePeerAddr, NULL);
			BC_ASSERT_PTR_NOT_NULL(focusCr);

			LinphoneParticipantDevice *paulineDevice = NULL;
			if (focusCr) {
				LinphoneParticipant *paulineParticipant =
				    linphone_chat_room_find_participant(focusCr, paulineLocalAddr);
				BC_ASSERT_PTR_NOT_NULL(paulineParticipant);
				if (paulineParticipant) {
					paulineDevice = linphone_participant_find_device(paulineParticipant, paulineLocalAddr);
					BC_ASSERT_PTR_NOT_NULL(paulineDevice);
				}
			}

			linphone_address_unref(paulineLocalAddr);
			linphone_address_unref(paulinePeerAddr);

			OrtpNetworkSimulatorParams simparams = {0};
			simparams.mode = OrtpNetworkSimulatorOutbound;
			simparams.enabled = TRUE;
			simparams.max_bandwidth = 430000; /*we first limit to 430 kbit/s*/
			simparams.max_buffer_size = static_cast<int>(simparams.max_bandwidth);
			simparams.latency = 60;
			simparams.loss_rate = 90;
			linphone_core_set_network_simulator_params(pauline.getLc(), &simparams);
			linphone_core_set_network_simulator_params(focus.getLc(), &simparams);

			linphone_core_delete_chat_room(marie.getLc(), marieCr);
			if (paulineDevice) {
				// wait until the participant moves to the Leaving state on the server
				BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline}).wait([&paulineDevice] {
					return (linphone_participant_device_get_state(paulineDevice) ==
					        LinphoneParticipantDeviceStateLeaving);
				}));
			}

			BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneConferenceStateDeleted,
			                             marie_stat.number_of_LinphoneConferenceStateDeleted + 1,
			                             liblinphone_tester_sip_timeout));

			BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneChatRoomSessionReleased, 1,
			                             liblinphone_tester_sip_timeout));

			linphone_core_delete_chat_room(pauline.getLc(), paulineCr);
			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline}).wait([&pauline] {
				auto &chatRooms = pauline.getCore().getChatRooms();
				for (auto chatRoom : chatRooms) {
					if (chatRoom->getState() != ConferenceInterface::State::Deleted) {
						return false;
					}
				}
				return true;
			}));
		}

		bctbx_list_free(coresList);
	}
}

void group_chat_room_with_client_removed_while_stopped_base(const bool_t set_conference_factory_on_restart,
                                                            bool encrypted) {
	Focus focus("chloe_rc");
	{ // to make sure focus is destroyed after clients.
		const LinphoneTesterLimeAlgo lime_algo = encrypted ? C25519 : UNSET;
		ClientConference marie("marie_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference michelle("michelle_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference berthe("berthe_rc", focus.getConferenceFactoryAddress(), lime_algo);

		focus.registerAsParticipantDevice(marie);
		focus.registerAsParticipantDevice(michelle);
		focus.registerAsParticipantDevice(berthe);

		if (encrypted) {
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(marie.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(michelle.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(berthe.getLc()));
		}

		bctbx_list_t *coresList = bctbx_list_append(NULL, focus.getLc());
		coresList = bctbx_list_append(coresList, marie.getLc());
		coresList = bctbx_list_append(coresList, michelle.getLc());
		coresList = bctbx_list_append(coresList, berthe.getLc());
		bctbx_list_t *participantsAddresses = NULL;
		Address michelleAddr = michelle.getIdentity();
		participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_ref(michelleAddr.toC()));
		Address bertheAddr = berthe.getIdentity();
		participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_ref(bertheAddr.toC()));

		stats initialMarieStats = marie.getStats();
		stats initialMichelleStats = michelle.getStats();
		stats initialBertheStats = berthe.getStats();
		//
		// Marie creates a new group chat room
		const char *initialSubject = "Colleagues (characters: $ £ çà)";
		LinphoneChatRoom *marieCr = create_chat_room_client_side_with_expected_number_of_participants(
		    coresList, marie.getCMgr(), &initialMarieStats, participantsAddresses, initialSubject, 2, encrypted,
		    LinphoneChatRoomEphemeralModeDeviceManaged);
		const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

		// Check that the chat room is correctly created on Michelle's side and that the participants are added
		LinphoneChatRoom *michelleCr = check_creation_chat_room_client_side(
		    coresList, michelle.getCMgr(), &initialMichelleStats, confAddr, initialSubject, 2, FALSE);

		LinphoneChatRoom *bertheCr = check_creation_chat_room_client_side(
		    coresList, berthe.getCMgr(), &initialBertheStats, confAddr, initialSubject, 2, FALSE);

		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, berthe}).wait([&focus] {
			for (const auto &chatRoom : focus.getCore().getChatRooms()) {
				for (const auto &participant : chatRoom->getParticipants()) {
					for (const auto &device : participant->getDevices()) {
						if (device->getState() != ParticipantDevice::State::Present) {
							return false;
						}
					}
				}
			}
			return true;
		}));

		BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneChatRoomStateCreated,
		                             initialMichelleStats.number_of_LinphoneChatRoomStateCreated + 1,
		                             liblinphone_tester_sip_timeout));

		BC_ASSERT_TRUE(wait_for_list(coresList, &berthe.getStats().number_of_LinphoneChatRoomStateCreated,
		                             initialBertheStats.number_of_LinphoneChatRoomStateCreated + 1,
		                             liblinphone_tester_sip_timeout));

		// Marie now changes the subject
		const char *newSubject = "Let's go drink a beer";
		linphone_chat_room_set_subject(marieCr, newSubject);
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_subject_changed,
		                             initialMarieStats.number_of_subject_changed + 1, liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_subject_changed,
		                             initialMichelleStats.number_of_subject_changed + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &berthe.getStats().number_of_subject_changed,
		                             initialBertheStats.number_of_subject_changed + 1, liblinphone_tester_sip_timeout));
		BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(marieCr), newSubject);
		BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(michelleCr), newSubject);
		BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(bertheCr), newSubject);

		BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(marieCr), 2, int, "%d");
		BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(michelleCr), 2, int, "%d");
		BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(bertheCr), 2, int, "%d");

		const std::initializer_list<std::reference_wrapper<ConfCoreManager>> cores2{focus, marie, michelle, berthe};
		for (const ConfCoreManager &core : cores2) {
			BC_ASSERT_TRUE(
			    CoreManagerAssert({focus, marie, michelle, berthe}).waitUntil(chrono::seconds(10), [&focus, &core] {
				    return checkChatroom(focus, core, -1);
			    }));
		};

		initialMarieStats = marie.getStats();
		initialMichelleStats = michelle.getStats();
		initialBertheStats = berthe.getStats();

		LinphoneAddress *michelleContact = linphone_address_clone(
		    linphone_proxy_config_get_contact(linphone_core_get_default_proxy_config(michelle.getLc())));

		// Restart Michelle
		const char *uuid = linphone_config_get_string(linphone_core_get_config(michelle.getLc()), "misc", "uuid", NULL);
		char *uuid_copy = NULL;
		if (uuid) {
			uuid_copy = bctbx_strdup(uuid);
		}

		char *michelleIdentity = ms_strdup(linphone_core_get_identity(michelle.getLc()));
		ms_message("%s stops its core", michelleIdentity);
		coresList = bctbx_list_remove(coresList, michelle.getLc());
		linphone_core_manager_stop(michelle.getCMgr());

		ms_message("All %s's devices are removed", michelleIdentity);
		for (auto chatRoom : focus.getCore().getChatRooms()) {
			auto participant =
			    chatRoom->findParticipant(Address::toCpp(michelle.getCMgr()->identity)->getSharedFromThis());
			BC_ASSERT_PTR_NOT_NULL(participant);
			if (participant) {
				const auto &devices = participant->getDevices();
				BC_ASSERT_GREATER_STRICT(devices.size(), 0, size_t, "%zu");
				for (const auto &device : devices) {
					auto deviceAddress = device->getAddress();
					ms_message("Delete device %s from the database", deviceAddress->toString().c_str());
					L_GET_CPP_PTR_FROM_C_OBJECT(focus.getLc())
					    ->getDatabase()
					    .value()
					    .get()
					    .deleteChatRoomParticipantDevice(chatRoom, device);
				}
				ConfCoreManager::deleteAllDevices(participant);
			}
		}

		// Marie removes Michelle from the chat room
		LinphoneParticipant *michelleParticipant = linphone_chat_room_find_participant(marieCr, michelleContact);
		BC_ASSERT_PTR_NOT_NULL(michelleParticipant);
		linphone_chat_room_remove_participant(marieCr, michelleParticipant);

		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_participants_removed,
		                             initialMarieStats.number_of_participants_removed + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &berthe.getStats().number_of_participants_removed,
		                             initialBertheStats.number_of_participants_removed + 1,
		                             liblinphone_tester_sip_timeout));

		ms_message("%s is restarting its core", linphone_core_get_identity(focus.getLc()));
		coresList = bctbx_list_remove(coresList, focus.getLc());

		// Restart flexisip
		focus.reStart();
		coresList = bctbx_list_append(coresList, focus.getLc());

		// For a SUBSCRIBE from the clients
		const std::initializer_list<std::reference_wrapper<ConfCoreManager>> cores3{marie, berthe};
		for (ConfCoreManager &core : cores3) {
			stats initialStats = core.getStats();
			ms_message("%s toggles its network", linphone_core_get_identity(core.getLc()));
			linphone_core_set_network_reachable(core.getLc(), FALSE);
			BC_ASSERT_TRUE(wait_for_list(coresList, &core.getStats().number_of_LinphoneSubscriptionTerminated,
			                             initialStats.number_of_LinphoneSubscriptionTerminated + 1,
			                             liblinphone_tester_sip_timeout));
			linphone_core_set_network_reachable(core.getLc(), TRUE);
			BC_ASSERT_TRUE(wait_for_list(coresList, &core.getStats().number_of_LinphoneRegistrationOk,
			                             initialStats.number_of_LinphoneRegistrationOk + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(coresList, &core.getStats().number_of_LinphoneSubscriptionActive,
			                             initialStats.number_of_LinphoneSubscriptionActive + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_FALSE(wait_for_list(coresList, &core.getStats().number_of_participant_devices_added,
			                              initialStats.number_of_participant_devices_added + 1, 2000));
			BC_ASSERT_FALSE(wait_for_list(coresList, &core.getStats().number_of_participants_added,
			                              initialStats.number_of_participants_added + 1, 200));
		}

		ms_message("%s starts again its core", michelleIdentity);
		ms_free(michelleIdentity);
		linphone_core_manager_configure(michelle.getCMgr());
		michelle.configure((!!set_conference_factory_on_restart) ? focus.getConferenceFactoryAddress() : Address(),
		                   lime_algo);
		// Make sure gruu is preserved
		linphone_config_set_string(linphone_core_get_config(michelle.getLc()), "misc", "uuid", uuid_copy);
		linphone_core_manager_start(michelle.getCMgr(), TRUE);
		coresList = bctbx_list_append(coresList, michelle.getLc());
		if (uuid_copy) {
			bctbx_free(uuid_copy);
		}

		// Notify chat room that a participant has registered
		bctbx_list_t *devices = NULL;
		bctbx_list_t *specs = linphone_core_get_linphone_specs_list(michelle.getLc());
		const LinphoneAddress *deviceAddr =
		    linphone_proxy_config_get_contact(linphone_core_get_default_proxy_config(michelle.getLc()));
		LinphoneParticipantDeviceIdentity *identity =
		    linphone_factory_create_participant_device_identity(linphone_factory_get(), deviceAddr, "");
		linphone_participant_device_identity_set_capability_descriptor_2(identity, specs);
		bctbx_list_free_with_data(specs, ms_free);
		devices = bctbx_list_append(devices, identity);

		for (auto chatRoom : focus.getCore().getChatRooms()) {
			linphone_chat_room_set_participant_devices(chatRoom->toC(), michelle.getCMgr()->identity, devices);
		}
		bctbx_list_free_with_data(devices, (bctbx_list_free_func)belle_sip_object_unref);

		BC_ASSERT_FALSE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneSubscriptionActive,
		                              initialMichelleStats.number_of_LinphoneSubscriptionActive + 1, 2000));
		BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneSubscriptionError,
		                             initialMichelleStats.number_of_LinphoneSubscriptionError + 1,
		                             liblinphone_tester_sip_timeout));

		michelleCr = linphone_core_search_chat_room(michelle.getLc(), NULL, michelleContact, confAddr, NULL);
		BC_ASSERT_PTR_NOT_NULL(michelleCr);

		if (michelleCr) {
			std::string msg_text = std::string("Attempting to send message to participants of ") +
			                       ChatRoom::toCpp(michelleCr)->getConferenceAddress()->toString();
			LinphoneChatMessage *msg = ClientConference::sendTextMsg(michelleCr, msg_text);
			LinphoneChatMessageState expected_state = LinphoneChatMessageStateNotDelivered;
			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, berthe}).wait([&msg, expected_state] {
				return (linphone_chat_message_get_state(msg) == expected_state);
			}));
			linphone_chat_message_unref(msg);

			BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneMessageNotDelivered,
			                             initialMichelleStats.number_of_LinphoneMessageNotDelivered + 1,
			                             liblinphone_tester_sip_timeout));
			if (encrypted) {
				if (!set_conference_factory_on_restart) { // With conference factory, we only expect
					                                      // LinphoneMessageNotDelivered, which has already been checked
					BC_ASSERT_TRUE(wait_for_list(coresList,
					                             &michelle.getStats().number_of_LinphoneChatRoomMessageEarlyFailure,
					                             initialMichelleStats.number_of_LinphoneChatRoomMessageEarlyFailure + 1,
					                             liblinphone_tester_sip_timeout));
				}
			} else {
				// New behaviour: do not exit group chatrooms on receiving 403/404
				BC_ASSERT_FALSE(
				    wait_for_list(coresList, &michelle.getStats().number_of_LinphoneChatRoomMessageEarlyFailure,
				                  initialMichelleStats.number_of_LinphoneChatRoomMessageEarlyFailure + 1, 3000));
			}
		}

		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, berthe}).wait([&michelle] {
			return michelle.getCore().getChatRooms().size() == 1;
		}));

		initialMarieStats = marie.getStats();
		initialBertheStats = berthe.getStats();

		// A second device for Berthe is added in order to verify that the server will not send a NOTIFY full state
		// where Michelle is still a participant but she has no devices associated
		ClientConference berthe2("berthe_rc", focus.getConferenceFactoryAddress(), lime_algo);
		stats initialBerthe2Stats = berthe2.getStats();
		coresList = bctbx_list_append(coresList, berthe2.getLc());

		if (encrypted) {
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(marie.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(michelle.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(berthe.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(berthe2.getLc()));
		}

		LinphoneAddress *berthe2Contact = linphone_address_clone(
		    linphone_proxy_config_get_contact(linphone_core_get_default_proxy_config(berthe2.getLc())));
		ms_message("%s is adding device %s", linphone_core_get_identity(focus.getLc()),
		           linphone_address_as_string(berthe2Contact));
		focus.registerAsParticipantDevice(berthe2);

		// Notify chat room that a participant has registered
		devices = NULL;
		specs = linphone_core_get_linphone_specs_list(berthe.getLc());
		deviceAddr = linphone_proxy_config_get_contact(linphone_core_get_default_proxy_config(berthe.getLc()));
		identity = linphone_factory_create_participant_device_identity(linphone_factory_get(), deviceAddr, "");
		linphone_participant_device_identity_set_capability_descriptor_2(identity, specs);
		bctbx_list_free_with_data(specs, ms_free);
		devices = bctbx_list_append(devices, identity);

		specs = linphone_core_get_linphone_specs_list(berthe2.getLc());
		deviceAddr = linphone_proxy_config_get_contact(linphone_core_get_default_proxy_config(berthe2.getLc()));
		identity = linphone_factory_create_participant_device_identity(linphone_factory_get(), deviceAddr, "");
		linphone_participant_device_identity_set_capability_descriptor_2(identity, specs);
		bctbx_list_free_with_data(specs, ms_free);
		devices = bctbx_list_append(devices, identity);

		for (auto chatRoom : focus.getCore().getChatRooms()) {
			linphone_chat_room_set_participant_devices(chatRoom->toC(), berthe.getCMgr()->identity, devices);
		}
		bctbx_list_free_with_data(devices, (bctbx_list_free_func)belle_sip_object_unref);

		LinphoneChatRoom *berthe2Cr = check_creation_chat_room_client_side(
		    coresList, berthe2.getCMgr(), &initialBerthe2Stats, confAddr, newSubject, 1, FALSE);
		BC_ASSERT_PTR_NOT_NULL(berthe2Cr);
		BC_ASSERT_TRUE(wait_for_list(coresList, &berthe2.getStats().number_of_LinphoneChatRoomStateCreated,
		                             initialBerthe2Stats.number_of_LinphoneChatRoomStateCreated + 1,
		                             liblinphone_tester_sip_timeout));

		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_participant_devices_added,
		                             initialMarieStats.number_of_participant_devices_added + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &berthe.getStats().number_of_participant_devices_added,
		                             initialBertheStats.number_of_participant_devices_added + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_FALSE(wait_for_list(coresList, &michelle.getStats().number_of_participant_devices_added,
		                              initialMichelleStats.number_of_participant_devices_added + 1, 2000));

		const std::initializer_list<std::reference_wrapper<ConfCoreManager>> cores{focus, marie, berthe, berthe2};
		for (const ConfCoreManager &core : cores) {
			BC_ASSERT_TRUE(
			    CoreManagerAssert({focus, marie, michelle, berthe}).waitUntil(chrono::seconds(10), [&focus, &core] {
				    return checkChatroom(focus, core, -1);
			    }));
			for (auto chatRoom : core.getCore().getChatRooms()) {
				BC_ASSERT_EQUAL(chatRoom->getParticipants().size(), ((focus.getLc() == core.getLc())) ? 2 : 1, size_t,
				                "%zu");
				BC_ASSERT_STRING_EQUAL(chatRoom->getSubject().c_str(), newSubject);
			}
		};

		initialMichelleStats = michelle.getStats();
		LinphoneChatMessage *msg = ClientConference::sendTextMsg(michelleCr, "back with you");
		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, berthe, berthe2}).wait([msg] {
			return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateNotDelivered);
		}));

		bool successfulWaitForEarlyFailure =
		    wait_for_list(coresList, &michelle.getStats().number_of_LinphoneChatRoomMessageEarlyFailure,
		                  initialMichelleStats.number_of_LinphoneChatRoomMessageEarlyFailure + 1, 3000);
		if (encrypted) {
			BC_ASSERT_TRUE(successfulWaitForEarlyFailure);
		} else {
			BC_ASSERT_FALSE(successfulWaitForEarlyFailure);
		}

		linphone_chat_message_unref(msg);
		msg = NULL;

		msg = ClientConference::sendTextMsg(marieCr, "Michelle, I can't receive your messages .... :(");
		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, berthe, berthe2}).wait([msg] {
			return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
		}));
		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, berthe, berthe2}).wait([bertheCr] {
			return linphone_chat_room_get_unread_messages_count(bertheCr) == 1;
		}));
		if (berthe2Cr) {
			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, berthe, berthe2}).wait([berthe2Cr] {
				return linphone_chat_room_get_unread_messages_count(berthe2Cr) == 1;
			}));
		}
		linphone_chat_message_unref(msg);
		msg = NULL;

		CoreManagerAssert({focus, marie, michelle, berthe}).waitUntil(std::chrono::seconds(2), [] { return false; });

		for (auto chatRoom : focus.getCore().getChatRooms()) {
			for (auto participant : chatRoom->getParticipants()) {
				//  force deletion by removing devices
				auto participantAddress = participant->getAddress();
				linphone_chat_room_set_participant_devices(chatRoom->toC(), participantAddress->toC(), NULL);
			}
		}

		// wait until chatroom is deleted server side
		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, berthe, berthe2}).wait([&focus] {
			return focus.getCore().getChatRooms().size() == 0;
		}));

		// wait a bit longer to detect side effect if any
		CoreManagerAssert({focus, marie, michelle, berthe, berthe2}).waitUntil(chrono::seconds(2), [] {
			return false;
		});

		// to avoid creation attempt of a new chatroom
		LinphoneProxyConfig *config = linphone_core_get_default_proxy_config(focus.getLc());
		linphone_proxy_config_edit(config);
		linphone_proxy_config_set_conference_factory_uri(config, NULL);
		linphone_proxy_config_done(config);

		linphone_address_unref(michelleContact);
		bctbx_list_free(coresList);
	}
}

void group_chat_room_with_client_removed_and_reinvinted_base(bool encrypted,
                                                             bool corrupt_database,
                                                             bool restart_core_after_corruption) {
	Focus focus("chloe_rc");
	{ // to make sure focus is destroyed after clients.

		const LinphoneTesterLimeAlgo lime_algo = encrypted ? C25519 : UNSET;
		linphone_core_enable_lime_x3dh(focus.getLc(), !!encrypted);

		ClientConference marie("marie_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference pauline("pauline_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference laure("laure_tcp_rc", focus.getConferenceFactoryAddress(), lime_algo);

		focus.registerAsParticipantDevice(marie);
		focus.registerAsParticipantDevice(pauline);
		focus.registerAsParticipantDevice(laure);

		bctbx_list_t *coresList = bctbx_list_append(NULL, focus.getLc());
		coresList = bctbx_list_append(coresList, marie.getLc());
		coresList = bctbx_list_append(coresList, pauline.getLc());
		coresList = bctbx_list_append(coresList, laure.getLc());

		Address paulineAddr = pauline.getIdentity();
		Address laureAddr = laure.getIdentity();
		bctbx_list_t *participantsAddresses = bctbx_list_append(NULL, linphone_address_ref(paulineAddr.toC()));
		participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_ref(laureAddr.toC()));

		stats initialMarieStats = marie.getStats();
		stats initialPaulineStats = pauline.getStats();
		stats initialLaureStats = laure.getStats();

		linphone_core_enable_gruu_in_conference_address(marie.getLc(), TRUE);
		linphone_core_enable_gruu_in_conference_address(laure.getLc(), FALSE);

		if (encrypted) {
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(marie.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(pauline.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(laure.getLc()));
		}

		// Marie creates a new group chat room
		const char *initialSubject = "Girls";
		LinphoneChatRoom *marieCr =
		    create_chat_room_client_side(coresList, marie.getCMgr(), &initialMarieStats, participantsAddresses,
		                                 initialSubject, encrypted, LinphoneChatRoomEphemeralModeDeviceManaged);
		BC_ASSERT_PTR_NOT_NULL(marieCr);
		const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);
		char *confAddrString = linphone_address_as_string(confAddr);

		// Check that the chat room is correctly created on Pauline's side and that the participants are added
		LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(
		    coresList, pauline.getCMgr(), &initialPaulineStats, confAddr, initialSubject, 2, FALSE);
		BC_ASSERT_PTR_NOT_NULL(paulineCr);
		LinphoneChatRoom *laureCr = check_creation_chat_room_client_side(coresList, laure.getCMgr(), &initialLaureStats,
		                                                                 confAddr, initialSubject, 2, FALSE);
		BC_ASSERT_PTR_NOT_NULL(laureCr);

		int msg_cnt = 0;
		const std::initializer_list<std::reference_wrapper<ClientConference>> cores{marie, laure, pauline};
		for (ClientConference &core : cores) {
			BC_ASSERT_EQUAL(core.getCore().getChatRooms().size(), 1, size_t, "%zu");
			for (auto chatRoom : core.getCore().getChatRooms()) {
				std::string msg_text = std::string("Welcome to all to chatroom ") +
				                       chatRoom->getConferenceAddress()->toString() + std::string(" by ") +
				                       core.getIdentity().toString();
				LinphoneChatMessage *msg = ClientConference::sendTextMsg(chatRoom->toC(), msg_text);

				BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, laure}).wait([&msg] {
					return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
				}));
				linphone_chat_message_unref(msg);

				msg_cnt++;

				for (ClientConference &core2 : cores) {
					LinphoneChatRoom *cr = linphone_core_search_chat_room(
					    core2.getLc(), NULL, NULL, chatRoom->getConferenceAddress()->toC(), NULL);
					BC_ASSERT_PTR_NOT_NULL(cr);
					if (cr) {
						BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, laure})
						                   .waitUntil(std::chrono::seconds(10), [&cr, msg_cnt] {
							                   return linphone_chat_room_get_history_size(cr) == msg_cnt;
						                   }));
					}
				}
			}
		}

		CoreManagerAssert({focus, marie, pauline, laure}).waitUntil(std::chrono::seconds(1), [] { return false; });

		if (corrupt_database) {
			for (auto chatRoom : laure.getCore().getChatRooms()) {
				auto info = ConferenceInfo::create();
				info->setOrganizer(Address::toCpp(marie.getCMgr()->identity)->getSharedFromThis());
				info->setUri(chatRoom->getConferenceAddress());
				info->setCapability(LinphoneStreamTypeAudio, true);
				info->setCapability(LinphoneStreamTypeVideo, true);
				info->setCapability(LinphoneStreamTypeText, false);
				laure.getDatabase().value().get().insertConferenceInfo(info);
				laure.getDatabase().value().get().insertChatRoom(chatRoom, chatRoom->getConference()->getLastNotify(),
				                                                 true);
			}

			if (restart_core_after_corruption) {
				coresList = bctbx_list_remove(coresList, laure.getLc());
				// Restart Laure
				ms_message("%s is restarting its core just after its database has been corrupted",
				           linphone_core_get_identity(laure.getLc()));
				laure.reStart();
				coresList = bctbx_list_append(coresList, laure.getLc());

				if (encrypted) {
					BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(laure.getLc()));
				}

				BC_ASSERT_EQUAL(laure.getCore().getChatRooms().size(), 1, size_t, "%zu");
				for (auto chatRoom : laure.getCore().getChatRooms()) {
					BC_ASSERT_FALSE(linphone_chat_room_has_been_left(chatRoom->toC()));
					BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(chatRoom->toC()), 2, int, "%d");
					BC_ASSERT_EQUAL(linphone_chat_room_get_history_size(chatRoom->toC()), msg_cnt, int, "%0d");
				}
				laureCr = linphone_core_search_chat_room(laure.getLc(), NULL, NULL, confAddr, NULL);
				BC_ASSERT_PTR_NOT_NULL(laureCr);
				LinphoneConference *conference = linphone_core_search_conference_2(laure.getLc(), confAddr);
				BC_ASSERT_PTR_NOT_NULL(conference);
				if (conference) {
					const LinphoneConferenceParams *conference_params =
					    linphone_conference_get_current_params(conference);
					BC_ASSERT_FALSE(linphone_conference_params_audio_enabled(conference_params));
					BC_ASSERT_FALSE(linphone_conference_params_video_enabled(conference_params));
					BC_ASSERT_TRUE(linphone_conference_params_chat_enabled(conference_params));
					BC_ASSERT_EQUAL(linphone_conference_get_state(conference), LinphoneConferenceStateCreated, int,
					                "%i");
				}
				BC_ASSERT_PTR_EQUAL(laureCr, linphone_conference_get_chat_room(conference));
				if (laureCr) {
					BC_ASSERT_FALSE(linphone_chat_room_has_been_left(laureCr));
				}
			}
		}

		for (ClientConference &core : cores) {
			size_t expected_chatrooms = 1;
			BC_ASSERT_EQUAL(core.getCore().getChatRooms().size(), expected_chatrooms, size_t, "%zu");
			for (auto chatRoom : core.getCore().getChatRooms()) {
				std::string msg_text = std::string("Testing if the chat service is up and running. Chatroom ") +
				                       chatRoom->getConferenceAddress()->toString() + std::string(" client ") +
				                       core.getIdentity().toString();
				LinphoneChatMessage *msg = ClientConference::sendTextMsg(chatRoom->toC(), msg_text);

				BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, laure}).wait([&msg] {
					return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
				}));
				linphone_chat_message_unref(msg);

				msg_cnt++;

				for (ClientConference &core2 : cores) {
					LinphoneChatRoom *cr = linphone_core_search_chat_room(
					    core2.getLc(), NULL, NULL, chatRoom->getConferenceAddress()->toC(), NULL);
					BC_ASSERT_PTR_NOT_NULL(cr);
					if (cr) {
						BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, laure})
						                   .waitUntil(std::chrono::seconds(10), [&cr, msg_cnt] {
							                   return linphone_chat_room_get_history_size(cr) == msg_cnt;
						                   }));
					}
				}
			}
		}

		initialMarieStats = marie.getStats();
		initialPaulineStats = pauline.getStats();
		initialLaureStats = laure.getStats();

		// Remove Laure from the chat room
		ms_message("%s removes %s from %s", linphone_core_get_identity(marie.getLc()),
		           linphone_core_get_identity(laure.getLc()), confAddrString);
		LinphoneParticipant *laureParticipant = linphone_chat_room_find_participant(marieCr, laureAddr.toC());
		BC_ASSERT_PTR_NOT_NULL(laureParticipant);
		linphone_chat_room_remove_participant(marieCr, laureParticipant);

		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_participants_removed,
		                             initialMarieStats.number_of_participants_removed + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_participants_removed,
		                             initialPaulineStats.number_of_participants_removed + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneChatRoomStateTerminated,
		                             initialLaureStats.number_of_LinphoneChatRoomStateTerminated + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_FALSE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneChatRoomStateDeleted,
		                              initialLaureStats.number_of_LinphoneChatRoomStateDeleted + 1, 2000));

		coresList = bctbx_list_remove(coresList, laure.getLc());
		// Restart Laure
		ms_message("%s is restarting its core", linphone_core_get_identity(laure.getLc()));
		laure.reStart();
		coresList = bctbx_list_append(coresList, laure.getLc());

		if (encrypted) {
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(laure.getLc()));
		}

		size_t expected_chatrooms = 1;
		BC_ASSERT_EQUAL(laure.getCore().getChatRooms().size(), expected_chatrooms, size_t, "%zu");
		for (auto chatRoom : laure.getCore().getChatRooms()) {
			LinphoneChatRoomState state = LinphoneChatRoomStateNone;
			if (!corrupt_database || restart_core_after_corruption) {
				BC_ASSERT_TRUE(linphone_chat_room_has_been_left(chatRoom->toC()));
				state = LinphoneChatRoomStateTerminated;
			} else {
				// At restart, the core figures out the database has been corrupted, therefore it tries to fix it by
				// removing the has_been_left flag and deleting the conference information
				BC_ASSERT_FALSE(linphone_chat_room_has_been_left(chatRoom->toC()));
				state = LinphoneChatRoomStateCreated;
			}
			BC_ASSERT_EQUAL(linphone_chat_room_get_state(chatRoom->toC()), state, int, "%i");
			BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(chatRoom->toC()), 2, int, "%d");
			BC_ASSERT_EQUAL(linphone_chat_room_get_history_size(chatRoom->toC()), msg_cnt, int, "%0d");
		}

		laureCr = linphone_core_search_chat_room(laure.getLc(), NULL, NULL, confAddr, NULL);
		BC_ASSERT_PTR_NOT_NULL(laureCr);
		if (corrupt_database) {
			LinphoneConference *conference = linphone_core_search_conference_2(laure.getLc(), confAddr);
			BC_ASSERT_PTR_NOT_NULL(conference);
			if (conference) {
				const LinphoneConferenceParams *conference_params = linphone_conference_get_current_params(conference);
				BC_ASSERT_FALSE(linphone_conference_params_audio_enabled(conference_params));
				BC_ASSERT_FALSE(linphone_conference_params_video_enabled(conference_params));
				BC_ASSERT_TRUE(linphone_conference_params_chat_enabled(conference_params));
				LinphoneConferenceState state = (restart_core_after_corruption) ? LinphoneConferenceStateTerminated
				                                                                : LinphoneConferenceStateCreated;
				BC_ASSERT_EQUAL(linphone_conference_get_state(conference), state, int, "%i");
				BC_ASSERT_PTR_EQUAL(laureCr, linphone_conference_get_chat_room(conference));
				if (laureCr) {
					if (restart_core_after_corruption) {
						BC_ASSERT_TRUE(linphone_chat_room_has_been_left(laureCr));
					} else {
						// At restart, the core figures out the database has been corrupted, therefore it tries to fix
						// it by removing the has_been_left flag and deleting the conference information
						BC_ASSERT_FALSE(linphone_chat_room_has_been_left(laureCr));
					}
				}
			}
		}

		if (corrupt_database && !restart_core_after_corruption) {
			BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneSubscriptionError, 1,
			                             liblinphone_tester_sip_timeout));
		} else {
			BC_ASSERT_FALSE(
			    wait_for_list(coresList, &laure.getStats().number_of_LinphoneSubscriptionOutgoingProgress, 1, 1000));
		}

		initialMarieStats = marie.getStats();
		initialPaulineStats = pauline.getStats();
		initialLaureStats = laure.getStats();

		// Marie adds laure again
		ms_message("%s adds again %s to %s", linphone_core_get_identity(marie.getLc()),
		           linphone_core_get_identity(laure.getLc()), confAddrString);
		linphone_chat_room_add_participant(marieCr, laure.getCMgr()->identity);
		BC_ASSERT_TRUE(
		    CoreManagerAssert({focus, marie, pauline, laure}).waitUntil(std::chrono::seconds(10), [&laure, &confAddr] {
			    return linphone_core_search_chat_room(laure.getLc(), NULL, NULL, confAddr, NULL);
		    }));
		laureCr = linphone_core_search_chat_room(laure.getLc(), NULL, NULL, confAddr, NULL);
		BC_ASSERT_PTR_NOT_NULL(laureCr);

		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneChatRoomStateCreationPending,
		                             initialLaureStats.number_of_LinphoneChatRoomStateCreationPending + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneChatRoomStateCreated,
		                             initialLaureStats.number_of_LinphoneChatRoomStateCreated + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_participants_added,
		                             initialMarieStats.number_of_participants_added + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_participants_added,
		                             initialPaulineStats.number_of_participants_added + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneSubscriptionActive,
		                             initialLaureStats.number_of_LinphoneSubscriptionActive + 1,
		                             liblinphone_tester_sip_timeout));

		LinphoneChatRoom *createdLaureCr = check_creation_chat_room_client_side(
		    coresList, laure.getCMgr(), &initialLaureStats, confAddr, initialSubject, 2, FALSE);
		BC_ASSERT_PTR_NOT_NULL(createdLaureCr);
		BC_ASSERT_PTR_EQUAL(laureCr, createdLaureCr);

		if (laureCr) {
			BC_ASSERT_TRUE(
			    CoreManagerAssert({focus, marie, pauline, laure}).waitUntil(std::chrono::seconds(10), [&laureCr] {
				    return (linphone_chat_room_get_state(laureCr) == LinphoneChatRoomStateCreated);
			    }));
		}

		// Verify that no new chatroom is created
		BC_ASSERT_EQUAL(laure.getCore().getChatRooms().size(), 1, size_t, "%zu");

		for (ClientConference &core : cores) {
			BC_ASSERT_EQUAL(core.getCore().getChatRooms().size(), 1, size_t, "%zu");
			for (auto chatRoom : core.getCore().getChatRooms()) {
				const auto &conferenceAddress = chatRoom->getConferenceAddress();
				std::string msg_text = std::string("Thank you girls in chatroom ") + conferenceAddress->toString() +
				                       std::string(" by ") + core.getIdentity().toString();
				LinphoneChatMessage *msg = ClientConference::sendTextMsg(chatRoom->toC(), msg_text);

				BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, laure}).wait([&msg] {
					return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
				}));
				linphone_chat_message_unref(msg);

				msg_cnt++;

				for (ClientConference &core2 : cores) {
					LinphoneChatRoom *cr =
					    linphone_core_search_chat_room(core2.getLc(), NULL, NULL, conferenceAddress->toC(), NULL);
					ms_message("Checking that message [%s] has been successfully added to chatroom %p [%s] of core %s",
					           msg_text.c_str(), cr, conferenceAddress->toString().c_str(),
					           linphone_core_get_identity(core2.getLc()));
					BC_ASSERT_PTR_NOT_NULL(cr);
					if (cr) {
						BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, laure})
						                   .waitUntil(std::chrono::seconds(10), [&cr, msg_cnt] {
							                   return linphone_chat_room_get_history_size(cr) == msg_cnt;
						                   }));
					}
				}
			}
		}

		for (auto chatRoom : focus.getCore().getChatRooms()) {
			for (auto participant : chatRoom->getParticipants()) {
				//  force deletion by removing devices
				std::shared_ptr<Address> participantAddress = participant->getAddress();
				linphone_chat_room_set_participant_devices(chatRoom->toC(), participantAddress->toC(), NULL);
			}
		}

		// wait until chatroom is deleted server side
		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, laure}).wait([&focus] {
			return focus.getCore().getChatRooms().size() == 0;
		}));

		// wait a bit longer to detect side effect if any
		CoreManagerAssert({focus, marie, pauline, laure}).waitUntil(chrono::seconds(2), [] { return false; });

		ms_free(confAddrString);

		// to avoid creation attempt of a new chatroom
		auto focus_account = focus.getDefaultAccount();
		LinphoneAccountParams *params = linphone_account_params_clone(linphone_account_get_params(focus_account));
		linphone_account_params_set_conference_factory_uri(params, NULL);
		linphone_account_set_params(focus_account, params);
		linphone_account_params_unref(params);

		bctbx_list_free(coresList);
	}
}

void group_chat_room_with_duplications_base(bool encrypted) {
	Focus focus("chloe_rc");
	{ // to make sure focus is destroyed after clients.

		const LinphoneTesterLimeAlgo lime_algo = encrypted ? C25519 : UNSET;
		linphone_core_enable_lime_x3dh(focus.getLc(), !!encrypted);

		ClientConference marie("marie_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference michelle("michelle_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference pauline("pauline_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference laure("laure_tcp_rc", focus.getConferenceFactoryAddress(), lime_algo);

		linphone_core_enable_gruu_in_conference_address(laure.getLc(), TRUE);

		if (encrypted) {
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(marie.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(pauline.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(laure.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(michelle.getLc()));
		}

		int subscribe_expires_value_s = 15;
		linphone_config_set_int(linphone_core_get_config(marie.getLc()), "sip", "conference_subscribe_expires",
		                        subscribe_expires_value_s);
		linphone_config_set_int(linphone_core_get_config(pauline.getLc()), "sip", "conference_subscribe_expires",
		                        subscribe_expires_value_s);
		linphone_config_set_int(linphone_core_get_config(michelle.getLc()), "sip", "conference_subscribe_expires",
		                        subscribe_expires_value_s);

		focus.registerAsParticipantDevice(marie);
		focus.registerAsParticipantDevice(michelle);
		focus.registerAsParticipantDevice(laure);
		focus.registerAsParticipantDevice(pauline);

		stats marie_stat = marie.getStats();
		stats pauline_stat = pauline.getStats();
		stats laure_stat = laure.getStats();
		stats michelle_stat = michelle.getStats();
		bctbx_list_t *coresList = bctbx_list_append(NULL, focus.getLc());
		coresList = bctbx_list_append(coresList, marie.getLc());
		coresList = bctbx_list_append(coresList, pauline.getLc());
		coresList = bctbx_list_append(coresList, laure.getLc());
		coresList = bctbx_list_append(coresList, michelle.getLc());

		int nbChatRooms = 10;
		for (int idx = 0; idx < nbChatRooms; idx++) {
			marie_stat = marie.getStats();
			pauline_stat = pauline.getStats();
			laure_stat = laure.getStats();
			michelle_stat = michelle.getStats();

			// Marie creates a new group chat room
			char *initialSubject = bctbx_strdup_printf("test subject for chatroom idx %d", idx);
			bctbx_list_t *participantsAddresses = NULL;
			Address michelleAddr = michelle.getIdentity();
			participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_ref(michelleAddr.toC()));
			Address laureAddr = laure.getIdentity();
			participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_ref(laureAddr.toC()));
			Address paulineAddr = pauline.getIdentity();
			participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_ref(paulineAddr.toC()));

			LinphoneChatRoom *marieCr = create_chat_room_client_side_with_expected_number_of_participants(
			    coresList, marie.getCMgr(), &marie_stat, participantsAddresses, initialSubject, 3, encrypted,
			    LinphoneChatRoomEphemeralModeDeviceManaged);
			BC_ASSERT_PTR_NOT_NULL(marieCr);
			const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);
			BC_ASSERT_PTR_NOT_NULL(confAddr);

			// Check that the chat room is correctly created on Michelle's side and that the participants are added
			LinphoneChatRoom *michelleCr = check_creation_chat_room_client_side(
			    coresList, michelle.getCMgr(), &michelle_stat, confAddr, initialSubject, 3, FALSE);
			BC_ASSERT_PTR_NOT_NULL(michelleCr);

			// Check that the chat room is correctly created on Pauline's side and that the participants are added
			LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(
			    coresList, pauline.getCMgr(), &pauline_stat, confAddr, initialSubject, 3, FALSE);
			BC_ASSERT_PTR_NOT_NULL(paulineCr);

			// Check that the chat room is correctly created on Laure's side and that the participants are added
			LinphoneChatRoom *laureCr = check_creation_chat_room_client_side(coresList, laure.getCMgr(), &laure_stat,
			                                                                 confAddr, initialSubject, 3, FALSE);
			BC_ASSERT_PTR_NOT_NULL(laureCr);
			ms_free(initialSubject);
		}

		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, laure}).wait([&focus] {
			for (auto chatRoom : focus.getCore().getChatRooms()) {
				for (auto participant : chatRoom->getParticipants()) {
					for (auto device : participant->getDevices()) {
						if (device->getState() != ParticipantDevice::State::Present) {
							return false;
						}
					}
				}
			}
			return true;
		}));

		BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneChatRoomStateCreated,
		                             nbChatRooms, liblinphone_tester_sip_timeout));

		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneChatRoomStateCreated, nbChatRooms,
		                             liblinphone_tester_sip_timeout));

		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneChatRoomStateCreated, nbChatRooms,
		                             liblinphone_tester_sip_timeout));

		marie_stat = marie.getStats();
		pauline_stat = pauline.getStats();
		laure_stat = laure.getStats();
		michelle_stat = michelle.getStats();

		std::list<ConferenceId> oldConferenceIds;

		for (auto chatRoom : laure.getCore().getChatRooms()) {
			// Store conference ID to verify that no events are left matching it after restarting the core
			oldConferenceIds.push_back(chatRoom->getConferenceId());
			std::string msg_text =
			    std::string("Welcome to all to chatroom ") + chatRoom->getConferenceAddress()->toString();
			LinphoneChatMessage *msg = ClientConference::sendTextMsg(chatRoom->toC(), msg_text);
			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, laure}).wait([&msg] {
				return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
			}));
			linphone_chat_message_unref(msg);
		}

		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneMessageSent,
		                             laure_stat.number_of_LinphoneMessageSent + nbChatRooms,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneMessageReceived,
		                             pauline_stat.number_of_LinphoneMessageReceived + nbChatRooms,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneMessageReceived,
		                             marie_stat.number_of_LinphoneMessageReceived + nbChatRooms,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneMessageReceived,
		                             michelle_stat.number_of_LinphoneMessageReceived + nbChatRooms,
		                             liblinphone_tester_sip_timeout));

		// wait a bit longer to detect side effect if any
		CoreManagerAssert({focus, marie, pauline, michelle, laure}).waitUntil(chrono::seconds(5), [] { return false; });

		ms_message("%s reinitializes its core", linphone_core_get_identity(laure.getLc()));
		coresList = bctbx_list_remove(coresList, laure.getLc());
		linphone_core_manager_reinit(laure.getCMgr());
		laure.configure(focus.getConferenceFactoryAddress(), lime_algo);
		linphone_core_enable_gruu_in_conference_address(laure.getLc(), TRUE);
		linphone_config_set_string(linphone_core_get_config(laure.getLc()), "misc", "uuid", NULL);
		linphone_core_remove_linphone_spec(laure.getLc(), "groupchat");
		const char *spec = "groupchat/1.2";
		linphone_core_add_linphone_spec(laure.getLc(), spec);

		laure_stat = laure.getStats();

		ms_message("%s starts again its core", linphone_core_get_identity(laure.getLc()));
		linphone_core_enable_send_message_after_notify(laure.getLc(), FALSE);
		linphone_core_manager_start(laure.getCMgr(), TRUE);
		focus.registerAsParticipantDevice(laure);
		coresList = bctbx_list_append(coresList, laure.getLc());

		if (!!encrypted) {
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(laure.getLc()));
		}

		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneRegistrationOk,
		                             laure_stat.number_of_LinphoneRegistrationOk + 1, liblinphone_tester_sip_timeout));

		// Notify chat room that a participant has registered
		bctbx_list_t *devices = NULL;

		LinphoneAccount *account = linphone_core_get_default_account(laure.getLc());
		const LinphoneAddress *laureDeviceAddress = linphone_account_get_contact_address(account);
		bctbx_list_t *specs = linphone_core_get_linphone_specs_list(laure.getLc());

		LinphoneParticipantDeviceIdentity *identity =
		    linphone_factory_create_participant_device_identity(linphone_factory_get(), laureDeviceAddress, "");
		linphone_participant_device_identity_set_capability_descriptor_2(identity, specs);
		devices = bctbx_list_append(devices, identity);
		bctbx_list_free_with_data(specs, ms_free);

		for (auto chatRoom : focus.getCore().getChatRooms()) {
			linphone_chat_room_set_participant_devices(chatRoom->toC(), laure.getCMgr()->identity, devices);
		}
		bctbx_list_free_with_data(devices, (bctbx_list_free_func)belle_sip_object_unref);

		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneSubscriptionActive,
		                             laure_stat.number_of_LinphoneSubscriptionActive + 1,
		                             liblinphone_tester_sip_timeout));

		BC_ASSERT_FALSE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneSubscriptionActive,
		                              laure_stat.number_of_LinphoneSubscriptionActive + 2, 2000));

		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, laure})
		                   .waitUntil(chrono::seconds(20),
		                              [&laure, &nbChatRooms] { return checkChatroomCreation(laure, nbChatRooms); }));

		BC_ASSERT_EQUAL(laure.getCore().getChatRooms().size(), nbChatRooms, size_t, "%zu");

		marie_stat = marie.getStats();
		pauline_stat = pauline.getStats();
		michelle_stat = michelle.getStats();
		laure_stat = laure.getStats();

		for (auto chatRoom : laure.getCore().getChatRooms()) {
			auto params = chatRoom->getCurrentParams();
			BC_ASSERT_TRUE(params->getChatParams()->isEncrypted() == !!encrypted);
			LinphoneChatRoom *cr = chatRoom->toC();
			std::string msg_text = std::string("I am back in chatroom ") + chatRoom->getConferenceAddress()->toString();
			LinphoneChatMessage *msg = ClientConference::sendTextMsg(cr, msg_text);

			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, laure}).wait([&msg] {
				return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
			}));
			linphone_chat_message_unref(msg);
		}

		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneMessageSent,
		                             laure_stat.number_of_LinphoneMessageSent + nbChatRooms,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneMessageReceived,
		                             pauline_stat.number_of_LinphoneMessageReceived + nbChatRooms,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneMessageReceived,
		                             marie_stat.number_of_LinphoneMessageReceived + nbChatRooms,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneMessageReceived,
		                             michelle_stat.number_of_LinphoneMessageReceived + nbChatRooms,
		                             liblinphone_tester_sip_timeout));

		for (auto chatRoom : michelle.getCore().getChatRooms()) {
			ms_message("%s is deleting chatroom %s", linphone_core_get_identity(michelle.getLc()),
			           chatRoom->getConferenceAddress()->toString().c_str());
			linphone_core_delete_chat_room(michelle.getLc(), chatRoom->toC());
		}
		BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneChatRoomStateDeleted,
		                             michelle_stat.number_of_LinphoneChatRoomStateDeleted + nbChatRooms,
		                             3 * liblinphone_tester_sip_timeout));
		const std::initializer_list<std::reference_wrapper<ConfCoreManager>> cores{marie, pauline, laure};
		for (const ConfCoreManager &core : cores) {
			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, laure})
			                   .waitUntil(chrono::seconds(20), [&core, &nbChatRooms] {
				                   return checkChatroomCreation(core, nbChatRooms, 2);
			                   }));
		}

		laure_stat = laure.getStats();
		ms_message("%s enters background", linphone_core_get_identity(laure.getLc()));
		linphone_core_enter_background(laure.getLc());

		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneSubscriptionTerminated,
		                             laure_stat.number_of_LinphoneSubscriptionTerminated + 1,
		                             liblinphone_tester_sip_timeout));

		BC_ASSERT_FALSE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneSubscriptionTerminated,
		                              laure_stat.number_of_LinphoneSubscriptionTerminated + 2, 2000));

		ms_message("%s enters foreground", linphone_core_get_identity(laure.getLc()));
		linphone_core_enter_foreground(laure.getLc());

		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneSubscriptionActive,
		                             laure_stat.number_of_LinphoneSubscriptionActive + 1,
		                             liblinphone_tester_sip_timeout));

		BC_ASSERT_FALSE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneSubscriptionActive,
		                              laure_stat.number_of_LinphoneSubscriptionActive + 2, 2000));

		if (encrypted) {
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(laure.getLc()));
		}

		marie_stat = marie.getStats();
		pauline_stat = pauline.getStats();
		michelle_stat = michelle.getStats();
		laure_stat = laure.getStats();

		for (auto chatRoom : laure.getCore().getChatRooms()) {
			LinphoneChatRoom *cr = chatRoom->toC();
			std::string msg_text =
			    std::string("Background/foreground test in chatroom ") + chatRoom->getConferenceAddress()->toString();
			LinphoneChatMessage *msg = ClientConference::sendTextMsg(cr, msg_text);

			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, laure}).wait([&msg] {
				return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
			}));
			linphone_chat_message_unref(msg);
		}

		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneMessageSent,
		                             laure_stat.number_of_LinphoneMessageSent + nbChatRooms,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneMessageReceived,
		                             pauline_stat.number_of_LinphoneMessageReceived + nbChatRooms,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneMessageReceived,
		                             marie_stat.number_of_LinphoneMessageReceived + nbChatRooms,
		                             liblinphone_tester_sip_timeout));

		laure_stat = laure.getStats();
		ms_message("%s enters background before simulating network issues", linphone_core_get_identity(laure.getLc()));
		linphone_core_enter_background(laure.getLc());

		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneSubscriptionTerminated,
		                             laure_stat.number_of_LinphoneSubscriptionTerminated + 1,
		                             liblinphone_tester_sip_timeout));

		BC_ASSERT_FALSE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneSubscriptionTerminated,
		                              laure_stat.number_of_LinphoneSubscriptionTerminated + 2, 2000));

		ms_message("%s simulates network issues", linphone_core_get_identity(laure.getLc()));
		laure.getLc()->sal->setSendError(-1);
		linphone_core_refresh_registers(laure.getLc());
		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneRegistrationFailed,
		                             laure_stat.number_of_LinphoneRegistrationFailed + 1,
		                             liblinphone_tester_sip_timeout));
		laure.getLc()->sal->setSendError(0);

		ms_message("%s enters foreground after simulating network issues", linphone_core_get_identity(laure.getLc()));
		linphone_core_enter_foreground(laure.getLc());

		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneSubscriptionActive,
		                             laure_stat.number_of_LinphoneSubscriptionActive + 1,
		                             liblinphone_tester_sip_timeout));

		BC_ASSERT_FALSE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneSubscriptionActive,
		                              laure_stat.number_of_LinphoneSubscriptionActive + 2, 2000));

		if (encrypted) {
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(laure.getLc()));
		}

		marie_stat = marie.getStats();
		pauline_stat = pauline.getStats();
		michelle_stat = michelle.getStats();
		laure_stat = laure.getStats();

		for (auto chatRoom : laure.getCore().getChatRooms()) {
			LinphoneChatRoom *cr = chatRoom->toC();
			std::string msg_text = std::string("Background/foreground  test after network issues in chatroom ") +
			                       chatRoom->getConferenceAddress()->toString();
			LinphoneChatMessage *msg = ClientConference::sendTextMsg(cr, msg_text);

			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, laure}).wait([&msg] {
				return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
			}));
			linphone_chat_message_unref(msg);
		}

		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneMessageSent,
		                             laure_stat.number_of_LinphoneMessageSent + nbChatRooms,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneMessageReceived,
		                             pauline_stat.number_of_LinphoneMessageReceived + nbChatRooms,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneMessageReceived,
		                             marie_stat.number_of_LinphoneMessageReceived + nbChatRooms,
		                             liblinphone_tester_sip_timeout));

		laure_stat = laure.getStats();
		ms_message("%s enters background again", linphone_core_get_identity(laure.getLc()));
		linphone_core_enter_background(laure.getLc());

		ms_message("%s shuts down its network when in background", linphone_core_get_identity(laure.getLc()));
		linphone_core_set_network_reachable(laure.getLc(), FALSE);

		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneSubscriptionTerminated,
		                             laure_stat.number_of_LinphoneSubscriptionTerminated + 1,
		                             liblinphone_tester_sip_timeout));

		BC_ASSERT_FALSE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneSubscriptionTerminated,
		                              laure_stat.number_of_LinphoneSubscriptionTerminated + 2, 2000));

		ms_message("%s enters foreground again", linphone_core_get_identity(laure.getLc()));
		linphone_core_enter_foreground(laure.getLc());

		ms_message("%s turns on its network after going back in foreground", linphone_core_get_identity(laure.getLc()));
		linphone_core_set_network_reachable(laure.getLc(), TRUE);

		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneSubscriptionActive,
		                             laure_stat.number_of_LinphoneSubscriptionActive + 1,
		                             liblinphone_tester_sip_timeout));

		BC_ASSERT_FALSE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneSubscriptionActive,
		                              laure_stat.number_of_LinphoneSubscriptionActive + 2, 2000));

		marie_stat = marie.getStats();
		pauline_stat = pauline.getStats();
		michelle_stat = michelle.getStats();
		laure_stat = laure.getStats();

		for (auto chatRoom : laure.getCore().getChatRooms()) {
			LinphoneChatRoom *cr = chatRoom->toC();
			std::string msg_text = std::string("Background/foreground with network toggling test in chatroom ") +
			                       chatRoom->getConferenceAddress()->toString();
			LinphoneChatMessage *msg = ClientConference::sendTextMsg(cr, msg_text);

			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, laure}).wait([&msg] {
				return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
			}));
			linphone_chat_message_unref(msg);
		}

		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneMessageSent,
		                             laure_stat.number_of_LinphoneMessageSent + nbChatRooms,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneMessageReceived,
		                             pauline_stat.number_of_LinphoneMessageReceived + nbChatRooms,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneMessageReceived,
		                             marie_stat.number_of_LinphoneMessageReceived + nbChatRooms,
		                             liblinphone_tester_sip_timeout));

		char *uuid = NULL;
		if (linphone_config_get_string(linphone_core_get_config(laure.getLc()), "misc", "uuid", NULL)) {
			uuid =
			    bctbx_strdup(linphone_config_get_string(linphone_core_get_config(laure.getLc()), "misc", "uuid", NULL));
		}

		ms_message("%s reinitializes one last time its core", linphone_core_get_identity(laure.getLc()));
		coresList = bctbx_list_remove(coresList, laure.getLc());
		linphone_core_manager_reinit(laure.getCMgr());
		linphone_core_enable_gruu_in_conference_address(laure.getLc(), FALSE);
		// Keep the same uuid
		linphone_config_set_string(linphone_core_get_config(laure.getLc()), "misc", "uuid", uuid);
		if (uuid) {
			bctbx_free(uuid);
		}

		linphone_core_remove_linphone_spec(laure.getLc(), "groupchat");
		linphone_core_add_linphone_spec(laure.getLc(), spec);

		marie_stat = marie.getStats();
		pauline_stat = pauline.getStats();
		michelle_stat = michelle.getStats();

		Address michelleAddr = michelle.getIdentity();
		for (auto chatRoom : marie.getCore().getChatRooms()) {
			stats michelle_stat2 = michelle.getStats();
			LinphoneChatRoom *cChatRoom = chatRoom->toC();
			linphone_chat_room_add_participant(cChatRoom, michelleAddr.toC());
			BC_ASSERT_PTR_NOT_NULL(check_creation_chat_room_client_side(
			    coresList, michelle.getCMgr(), &michelle_stat2, linphone_chat_room_get_conference_address(cChatRoom),
			    linphone_chat_room_get_subject(cChatRoom), 3, FALSE));
		}

		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_participant_devices_added,
		                             marie_stat.number_of_participant_devices_added + nbChatRooms,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_participant_devices_added,
		                             pauline_stat.number_of_participant_devices_added + nbChatRooms,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_participants_added,
		                             marie_stat.number_of_participants_added + nbChatRooms,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_participants_added,
		                             pauline_stat.number_of_participants_added + nbChatRooms,
		                             liblinphone_tester_sip_timeout));

		BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneChatRoomStateCreated,
		                             michelle_stat.number_of_LinphoneChatRoomStateCreated + nbChatRooms,
		                             liblinphone_tester_sip_timeout));

		laure_stat = laure.getStats();

		ms_message("%s starts one last time its core", linphone_core_get_identity(laure.getLc()));
		laure.configure(focus.getConferenceFactoryAddress(), lime_algo);
		linphone_config_set_int(linphone_core_get_config(laure.getLc()), "sip", "conference_subscribe_expires",
		                        subscribe_expires_value_s);

		linphone_core_manager_start(laure.getCMgr(), TRUE);
		focus.registerAsParticipantDevice(laure);
		coresList = bctbx_list_append(coresList, laure.getLc());

		if (encrypted) {
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(laure.getLc()));
		}

		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneRegistrationOk,
		                             laure_stat.number_of_LinphoneRegistrationOk + 1, liblinphone_tester_sip_timeout));

		BC_ASSERT_EQUAL(laure.getCore().getChatRooms().size(), nbChatRooms, size_t, "%zu");
		auto laureMainDb = laure.getDatabase();
		BC_ASSERT_EQUAL(laureMainDb.value().get().getChatRooms().size(), nbChatRooms, size_t, "%zu");

		const std::initializer_list<std::reference_wrapper<ConfCoreManager>> cores2{marie, pauline, laure, michelle};
		for (const ConfCoreManager &core : cores2) {
			int expectedHistorySize = (core.getCMgr() == michelle.getCMgr()) ? 0 : 5;
			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, laure})
			                   .wait([&core, &nbChatRooms, &encrypted, &expectedHistorySize] {
				                   const auto &coreChatRooms = core.getCore().getChatRooms();
				                   if (coreChatRooms.size() < static_cast<size_t>(nbChatRooms)) {
					                   return false;
				                   }
				                   for (auto chatRoom : coreChatRooms) {
					                   if (chatRoom->getState() != ConferenceInterface::State::Created) {
						                   return false;
					                   }
					                   auto params = chatRoom->getCurrentParams();
					                   if (params->getChatParams()->isEncrypted() != !!encrypted) {
						                   return false;
					                   }
					                   auto historySize = chatRoom->getMessageHistorySize();
					                   if (historySize != expectedHistorySize) {
						                   return false;
					                   }
					                   auto coreMainDb = core.getDatabase();
					                   if (coreMainDb.value().get().getChatMessageCount(chatRoom->getConferenceId()) !=
					                       historySize) {
						                   return false;
					                   }
				                   }
				                   return true;
			                   }));
		}

		for (const auto &conferenceId : oldConferenceIds) {
			const auto chatRoom = laure.getCore().searchChatRoom(nullptr, conferenceId.getLocalAddress(),
			                                                     conferenceId.getPeerAddress(), {});
			BC_ASSERT_PTR_NOT_NULL(chatRoom);
			if (chatRoom) {
				BC_ASSERT_EQUAL(
				    laureMainDb.value().get().getConferenceNotifiedEvents(conferenceId, 0).size(),
				    laureMainDb.value().get().getConferenceNotifiedEvents(chatRoom->getConferenceId(), 0).size(),
				    size_t, "%zu");
			}
		}

		for (auto chatRoom : laure.getCore().getChatRooms()) {
			const ConferenceId conferenceId = chatRoom->getConferenceId();
			const auto &oldConferenceIdIt = std::find_if(
			    oldConferenceIds.begin(), oldConferenceIds.end(), [&conferenceId](const auto &oldConferenceId) {
				    const auto oldPeerAddress = oldConferenceId.getPeerAddress()->getUriWithoutGruu();
				    const auto newPeerAddress = conferenceId.getPeerAddress()->getUriWithoutGruu();
				    return (oldPeerAddress == newPeerAddress);
			    });
			BC_ASSERT_TRUE(oldConferenceIdIt != oldConferenceIds.end());
			if (oldConferenceIdIt != oldConferenceIds.end()) {
				BC_ASSERT_EQUAL(laureMainDb.value().get().getConferenceNotifiedEvents(conferenceId, 0).size(),
				                laureMainDb.value().get().getConferenceNotifiedEvents(*oldConferenceIdIt, 0).size(),
				                size_t, "%zu");
			}
		}

		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneSubscriptionActive,
		                             laure_stat.number_of_LinphoneSubscriptionActive + 1,
		                             liblinphone_tester_sip_timeout));

		marie_stat = marie.getStats();
		pauline_stat = pauline.getStats();
		stats focus_stat = focus.getStats();
		const std::initializer_list<std::reference_wrapper<ClientConference>> cores3{marie, pauline};
		for (ClientConference &core : cores3) {
			ms_message("%s restarts its core", linphone_core_get_identity(core.getLc()));
			coresList = bctbx_list_remove(coresList, core.getLc());
			linphone_core_manager_reinit(core.getCMgr());
			core.configure(focus.getConferenceFactoryAddress(), lime_algo);
			linphone_config_set_int(linphone_core_get_config(core.getLc()), "sip", "conference_subscribe_expires",
			                        subscribe_expires_value_s);
			linphone_core_manager_start(core.getCMgr(), TRUE);
			coresList = bctbx_list_append(coresList, core.getLc());
		}

		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneSubscriptionActive, 1,
		                             liblinphone_tester_sip_timeout));

		BC_ASSERT_FALSE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneSubscriptionActive, 2, 1000));

		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneSubscriptionActive, 1,
		                             liblinphone_tester_sip_timeout));

		BC_ASSERT_FALSE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneSubscriptionActive, 2, 1000));

		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneSubscriptionActive,
		                             focus_stat.number_of_LinphoneSubscriptionActive + 2,
		                             liblinphone_tester_sip_timeout));

		const std::initializer_list<std::reference_wrapper<CoreManager>> msgCores{focus, marie, pauline, michelle,
		                                                                          laure};
		for (auto chatRoom : marie.getCore().getChatRooms()) {
			marie_stat = marie.getStats();
			pauline_stat = pauline.getStats();
			laure_stat = laure.getStats();
			michelle_stat = michelle.getStats();

			const auto &confAddr = chatRoom->getConferenceAddress();
			const auto confAddrString = confAddr->toString();
			std::string msgText =
			    std::string("Let's see if this chat is up and running - Another message in chatroom ") + confAddrString;
			std::map<LinphoneCoreManager *, int> members{
			    {pauline.getCMgr(), 6}, {laure.getCMgr(), 1}, {michelle.getCMgr(), 1}};
			sendMessageAndCheckReception(msgCores, members, msgText, chatRoom);

			std::string newSubject = std::string("New subject of chatroom ") + confAddrString;
			ms_message("%s changes the subject of chatroom %s to '%s'", linphone_core_get_identity(marie.getLc()),
			           chatRoom->getConferenceAddress()->toString().c_str(), newSubject.c_str());
			linphone_chat_room_set_subject(chatRoom->toC(), newSubject.c_str());
			BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_chat_room_subject_changed,
			                             marie_stat.number_of_chat_room_subject_changed + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_chat_room_subject_changed,
			                             pauline_stat.number_of_chat_room_subject_changed + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_chat_room_subject_changed,
			                             laure_stat.number_of_chat_room_subject_changed + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_chat_room_subject_changed,
			                             michelle_stat.number_of_chat_room_subject_changed + 1,
			                             liblinphone_tester_sip_timeout));
		}

		marie_stat = marie.getStats();
		pauline_stat = pauline.getStats();
		michelle_stat = michelle.getStats();
		laure_stat = laure.getStats();
		focus_stat = focus.getStats();
		ms_message("%s shuts down its network", linphone_core_get_identity(focus.getLc()));
		linphone_core_set_network_reachable(focus.getLc(), FALSE);

		// Wait for a little while to spot any errors
		CoreManagerAssert({focus, marie, pauline, michelle, laure})
		    .waitUntil(chrono::seconds(subscribe_expires_value_s + 1), [] { return false; });

		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneSubscriptionOutgoingProgress,
		                             marie_stat.number_of_LinphoneSubscriptionOutgoingProgress + 1,
		                             (subscribe_expires_value_s * 1000)));

		BC_ASSERT_FALSE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneSubscriptionOutgoingProgress,
		                              marie_stat.number_of_LinphoneSubscriptionOutgoingProgress + 2, 1000));

		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneSubscriptionOutgoingProgress,
		                             pauline_stat.number_of_LinphoneSubscriptionOutgoingProgress + 1,
		                             (subscribe_expires_value_s * 1000)));

		BC_ASSERT_FALSE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneSubscriptionOutgoingProgress,
		                              pauline_stat.number_of_LinphoneSubscriptionOutgoingProgress + 2, 1000));

		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneSubscriptionOutgoingProgress,
		                             laure_stat.number_of_LinphoneSubscriptionOutgoingProgress + 1,
		                             (subscribe_expires_value_s * 1000)));

		BC_ASSERT_FALSE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneSubscriptionOutgoingProgress,
		                              laure_stat.number_of_LinphoneSubscriptionOutgoingProgress + 2, 1000));

		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneSubscriptionTerminated,
		                             focus_stat.number_of_LinphoneSubscriptionTerminated + 3,
		                             (subscribe_expires_value_s * 1000)));

		marie_stat = marie.getStats();
		pauline_stat = pauline.getStats();
		michelle_stat = michelle.getStats();
		laure_stat = laure.getStats();
		focus_stat = focus.getStats();

		ms_message("%s turns on its network", linphone_core_get_identity(focus.getLc()));
		linphone_core_set_network_reachable(focus.getLc(), TRUE);

		// Wait for subscriptions between the clients and the server to expire.
		// In fact clients are not notified that the server restarted so from the server standpoint, the events are in
		// the Terminated state
		CoreManagerAssert({focus, marie, pauline, michelle, laure})
		    .waitUntil(chrono::seconds(subscribe_expires_value_s + 1), [] { return false; });

		// Pauline, Marie and Laure will send two SUBSCRIBE messages each. The first SUBSCRIBE will have the same body
		// as the one the client tries to recover from. The server will therefore resend the last event (subject change
		// in the test). As this event has already been received by the client, the latter will request a full state for
		// all chatrooms belonging to the list
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneSubscriptionActive,
		                             marie_stat.number_of_LinphoneSubscriptionActive + 2,
		                             (subscribe_expires_value_s * 1000)));

		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneSubscriptionTerminated,
		                             marie_stat.number_of_LinphoneSubscriptionTerminated + 1,
		                             liblinphone_tester_sip_timeout));

		BC_ASSERT_FALSE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneSubscriptionActive,
		                              marie_stat.number_of_LinphoneSubscriptionActive + 3, 1000));

		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneSubscriptionActive,
		                             pauline_stat.number_of_LinphoneSubscriptionActive + 2,
		                             (subscribe_expires_value_s * 1000)));

		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneSubscriptionTerminated,
		                             pauline_stat.number_of_LinphoneSubscriptionTerminated + 1,
		                             liblinphone_tester_sip_timeout));

		BC_ASSERT_FALSE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneSubscriptionActive,
		                              pauline_stat.number_of_LinphoneSubscriptionActive + 3, 1000));

		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneSubscriptionActive,
		                             laure_stat.number_of_LinphoneSubscriptionActive + 2,
		                             (subscribe_expires_value_s * 1000)));

		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneSubscriptionTerminated,
		                             laure_stat.number_of_LinphoneSubscriptionTerminated + 1,
		                             liblinphone_tester_sip_timeout));

		BC_ASSERT_FALSE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneSubscriptionActive,
		                              laure_stat.number_of_LinphoneSubscriptionActive + 3, 1000));

		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneSubscriptionActive,
		                             focus_stat.number_of_LinphoneSubscriptionActive + 6,
		                             (subscribe_expires_value_s * 1000)));

		marie_stat = marie.getStats();
		pauline_stat = pauline.getStats();
		michelle_stat = michelle.getStats();
		laure_stat = laure.getStats();

		for (auto chatRoom : laure.getCore().getChatRooms()) {
			LinphoneChatRoom *cr = chatRoom->toC();
			std::string msg_text = std::string("Message after SUBSCRIBE expires after the conference server briefly "
			                                   "was not reacheable - chatroom ") +
			                       chatRoom->getConferenceAddress()->toString();
			LinphoneChatMessage *msg = ClientConference::sendTextMsg(cr, msg_text);

			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, laure}).wait([&msg] {
				return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
			}));
			linphone_chat_message_unref(msg);
		}

		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneMessageSent,
		                             laure_stat.number_of_LinphoneMessageSent + nbChatRooms,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneMessageReceived,
		                             pauline_stat.number_of_LinphoneMessageReceived + nbChatRooms,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneMessageReceived,
		                             marie_stat.number_of_LinphoneMessageReceived + nbChatRooms,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneMessageReceived,
		                             michelle_stat.number_of_LinphoneMessageReceived + nbChatRooms,
		                             liblinphone_tester_sip_timeout));

		BC_ASSERT_EQUAL(marie.getCore().getChatRooms().size(), nbChatRooms, size_t, "%zu");

		// Delete Laure's chatrooms by retrieving their conference address from Marie's ones.
		// This will allow to verify that there is no duplicate chatroom in stored by Laure's core
		for (auto chatRoom : marie.getCore().getChatRooms()) {
			stats marie_stat = marie.getStats();
			stats pauline_stat = pauline.getStats();
			const auto &conferenceAddress = chatRoom->getConferenceAddress();
			LinphoneAddress *laureLocalAddress =
			    linphone_account_get_contact_address(linphone_core_get_default_account(laure.getLc()));
			LinphoneChatRoom *laureCr = laure.searchChatRoom(laureLocalAddress, conferenceAddress->toC());
			ms_message("%s is deleting chatroom %s", linphone_core_get_identity(laure.getLc()),
			           conferenceAddress->toString().c_str());
			linphone_core_manager_delete_chat_room(laure.getCMgr(), laureCr, coresList);

			BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_chat_room_participants_removed,
			                             marie_stat.number_of_chat_room_participants_removed + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_chat_room_participants_removed,
			                             pauline_stat.number_of_chat_room_participants_removed + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_chat_room_participants_removed,
			                             michelle_stat.number_of_chat_room_participants_removed + 1,
			                             liblinphone_tester_sip_timeout));
		}

		BC_ASSERT_EQUAL(laure.getCore().getChatRooms().size(), 0, size_t, "%zu");
		for (auto chatRoom : focus.getCore().getChatRooms()) {
			for (auto participant : chatRoom->getParticipants()) {
				//  force deletion by removing devices
				std::shared_ptr<Address> participantAddress = participant->getAddress();
				linphone_chat_room_set_participant_devices(chatRoom->toC(), participantAddress->toC(), NULL);
			}
		}

		// wait until chatroom is deleted server side
		BC_ASSERT_TRUE(
		    CoreManagerAssert({focus, marie, pauline, michelle, laure}).waitUntil(chrono::seconds(120), [&focus] {
			    return focus.getCore().getChatRooms().size() == 0;
		    }));

		// wait a bit longer to detect side effect if any
		CoreManagerAssert({focus, marie, pauline, michelle, laure}).waitUntil(chrono::seconds(2), [] { return false; });

		// to avoid creation attempt of a new chatroom
		auto focus_account = focus.getDefaultAccount();
		LinphoneAccountParams *params = linphone_account_params_clone(linphone_account_get_params(focus_account));
		linphone_account_params_set_conference_factory_uri(params, NULL);
		linphone_account_set_params(focus_account, params);
		linphone_account_params_unref(params);

		bctbx_list_free(coresList);
	}
}

void chat_rooms_with_deletion_spaced_out_base(bool encrypted) {
	Focus focus("chloe_rc");
	{ // to make sure focus is destroyed after clients.
		const LinphoneTesterLimeAlgo lime_algo = encrypted ? C25519 : UNSET;
		ClientConference marie("marie_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference michelle("michelle_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference laure("laure_tcp_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference berthe("berthe_rc", focus.getConferenceFactoryAddress(), lime_algo);

		linphone_core_enable_lime_x3dh(focus.getLc(), encrypted);

		focus.registerAsParticipantDevice(marie);
		focus.registerAsParticipantDevice(michelle);
		focus.registerAsParticipantDevice(laure);
		focus.registerAsParticipantDevice(berthe);

		linphone_core_enable_gruu_in_conference_address(focus.getLc(), TRUE);
		linphone_core_set_add_admin_information_to_contact(marie.getLc(), TRUE);
		linphone_core_set_add_admin_information_to_contact(laure.getLc(), TRUE);

		bctbx_list_t *coresList = bctbx_list_append(NULL, focus.getLc());
		coresList = bctbx_list_append(coresList, marie.getLc());
		coresList = bctbx_list_append(coresList, michelle.getLc());
		coresList = bctbx_list_append(coresList, laure.getLc());
		coresList = bctbx_list_append(coresList, berthe.getLc());

		if (encrypted) {
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(marie.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(michelle.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(berthe.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(laure.getLc()));
		}

		int subscribe_expires_value_s = 15;
		linphone_config_set_int(linphone_core_get_config(marie.getLc()), "sip", "conference_subscribe_expires",
		                        subscribe_expires_value_s);
		linphone_config_set_int(linphone_core_get_config(laure.getLc()), "sip", "conference_subscribe_expires",
		                        subscribe_expires_value_s);
		linphone_config_set_int(linphone_core_get_config(michelle.getLc()), "sip", "conference_subscribe_expires",
		                        subscribe_expires_value_s);
		linphone_config_set_int(linphone_core_get_config(berthe.getLc()), "sip", "conference_subscribe_expires",
		                        subscribe_expires_value_s);

		stats initialMarieStats = marie.getStats();
		stats initialMichelleStats = michelle.getStats();
		stats initialBertheStats = berthe.getStats();
		stats initialLaureStats = laure.getStats();

		size_t nbGroupChatRooms = 3;
		for (size_t idx = 0; idx < nbGroupChatRooms; idx++) {
			initialMarieStats = marie.getStats();
			initialMichelleStats = michelle.getStats();
			initialBertheStats = berthe.getStats();
			initialLaureStats = laure.getStats();

			// Marie creates a new group chat room
			char *initialSubject = bctbx_strdup_printf("Chatroom burst: %0zu out of %0zu", idx, nbGroupChatRooms);
			bctbx_list_t *participantsAddresses = NULL;
			Address laureAddr = laure.getIdentity();
			participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_ref(laureAddr.toC()));
			Address michelleAddr = michelle.getIdentity();
			participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_ref(michelleAddr.toC()));
			Address bertheAddr = berthe.getIdentity();
			participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_ref(bertheAddr.toC()));

			LinphoneConferenceParams *params = linphone_core_create_conference_params_2(marie.getLc(), NULL);
			linphone_conference_params_enable_chat(params, TRUE);
			linphone_conference_params_set_security_level(params, (encrypted) ? LinphoneConferenceSecurityLevelEndToEnd
			                                                                  : LinphoneConferenceSecurityLevelNone);
			linphone_conference_params_set_subject(params, initialSubject);
			linphone_conference_params_enable_group(params, TRUE);
			LinphoneChatParams *chat_params = linphone_conference_params_get_chat_params(params);
			linphone_chat_params_set_backend(chat_params, LinphoneChatRoomBackendFlexisipChat);
			LinphoneChatRoom *marieCr = linphone_core_create_chat_room_7(marie.getLc(), params, participantsAddresses);
			BC_ASSERT_PTR_NOT_NULL(marieCr);
			bctbx_list_free_with_data(participantsAddresses, (bctbx_list_free_func)linphone_address_unref);
			linphone_conference_params_unref(params);
			participantsAddresses = NULL;
			BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneChatRoomStateInstantiated,
			                             initialMarieStats.number_of_LinphoneChatRoomStateInstantiated + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneChatRoomStateCreationPending,
			                             initialMarieStats.number_of_LinphoneChatRoomStateCreationPending + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneChatRoomStateCreated,
			                             initialMarieStats.number_of_LinphoneChatRoomStateCreated + 1,
			                             liblinphone_tester_sip_timeout));

			const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);
			BC_ASSERT_PTR_NOT_NULL(confAddr);

			// Check that the chat room is correctly created on Michelle's side and that the participants are added
			LinphoneChatRoom *michelleCr = check_creation_chat_room_client_side(
			    coresList, michelle.getCMgr(), &initialMichelleStats, confAddr, initialSubject, 3, FALSE);
			BC_ASSERT_PTR_NOT_NULL(michelleCr);

			// Check that the chat room is correctly created on Berthe's side and that the participants are added
			LinphoneChatRoom *bertheCr = check_creation_chat_room_client_side(
			    coresList, berthe.getCMgr(), &initialBertheStats, confAddr, initialSubject, 3, FALSE);
			BC_ASSERT_PTR_NOT_NULL(bertheCr);

			// Check that the chat room is correctly created on Laure's side and that the participants are added
			LinphoneChatRoom *laureCr = check_creation_chat_room_client_side(
			    coresList, laure.getCMgr(), &initialLaureStats, confAddr, initialSubject, 3, FALSE);
			BC_ASSERT_PTR_NOT_NULL(laureCr);
			ms_free(initialSubject);

			auto cServerChatRoom = focus.searchChatRoom(nullptr, confAddr);
			BC_ASSERT_PTR_NOT_NULL(cServerChatRoom);
			if (cServerChatRoom) {
				auto chatRoom = AbstractChatRoom::toCpp(cServerChatRoom)->getSharedFromThis();
				BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, berthe, michelle, laure}).wait([&chatRoom] {
					for (auto participant : chatRoom->getParticipants()) {
						for (auto device : participant->getDevices()) {
							if (device->getState() != ParticipantDevice::State::Present) {
								return false;
							}
						}
					}
					return true;
				}));
			}

			BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneConferenceStateCreated,
			                             initialLaureStats.number_of_LinphoneConferenceStateCreated + 1,
			                             liblinphone_tester_sip_timeout));

			BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneConferenceStateCreated,
			                             initialMichelleStats.number_of_LinphoneConferenceStateCreated + 1,
			                             liblinphone_tester_sip_timeout));

			BC_ASSERT_TRUE(wait_for_list(coresList, &berthe.getStats().number_of_LinphoneConferenceStateCreated,
			                             initialBertheStats.number_of_LinphoneConferenceStateCreated + 1,
			                             liblinphone_tester_sip_timeout));
		}

		const std::initializer_list<std::reference_wrapper<ClientConference>> cores{berthe, laure, marie, michelle};
		for (ClientConference &core : cores) {
			if (core.getLc() != michelle.getLc()) {
				initialMichelleStats = michelle.getStats();
				stats initialPeerStats = core.getStats();

				// Marie creates a new group chat room
				char *initialSubject =
				    bctbx_strdup_printf("One on one chatroom with %s", linphone_core_get_identity(core.getLc()));
				bctbx_list_t *participantsAddresses = NULL;
				Address peerAddr = core.getIdentity();
				participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_ref(peerAddr.toC()));

				LinphoneConferenceParams *params = linphone_core_create_conference_params_2(michelle.getLc(), NULL);
				linphone_conference_params_enable_chat(params, TRUE);
				linphone_conference_params_set_security_level(params, (encrypted)
				                                                          ? LinphoneConferenceSecurityLevelEndToEnd
				                                                          : LinphoneConferenceSecurityLevelNone);
				linphone_conference_params_set_subject(params, initialSubject);
				linphone_conference_params_enable_group(params, FALSE);
				LinphoneChatParams *chat_params = linphone_conference_params_get_chat_params(params);
				linphone_chat_params_set_backend(chat_params, LinphoneChatRoomBackendFlexisipChat);
				LinphoneChatRoom *michelleCr =
				    linphone_core_create_chat_room_7(michelle.getLc(), params, participantsAddresses);
				BC_ASSERT_PTR_NOT_NULL(michelleCr);
				bctbx_list_free_with_data(participantsAddresses, (bctbx_list_free_func)linphone_address_unref);
				linphone_conference_params_unref(params);
				participantsAddresses = NULL;
				BC_ASSERT_TRUE(wait_for_list(coresList,
				                             &michelle.getStats().number_of_LinphoneChatRoomStateInstantiated,
				                             initialMichelleStats.number_of_LinphoneChatRoomStateInstantiated + 1,
				                             liblinphone_tester_sip_timeout));
				BC_ASSERT_TRUE(wait_for_list(coresList,
				                             &michelle.getStats().number_of_LinphoneChatRoomStateCreationPending,
				                             initialMichelleStats.number_of_LinphoneChatRoomStateCreationPending + 1,
				                             liblinphone_tester_sip_timeout));
				BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneChatRoomStateCreated,
				                             initialMichelleStats.number_of_LinphoneChatRoomStateCreated + 1,
				                             liblinphone_tester_sip_timeout));

				const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(michelleCr);
				BC_ASSERT_PTR_NOT_NULL(confAddr);

				// Check that the chat room is correctly created on the peer side and that the participants are added
				LinphoneChatRoom *peerCr = check_creation_chat_room_client_side(
				    coresList, core.getCMgr(), &initialPeerStats, confAddr, initialSubject, 1, FALSE);
				BC_ASSERT_PTR_NOT_NULL(peerCr);
				ms_free(initialSubject);

				auto cServerChatRoom = focus.searchChatRoom(nullptr, confAddr);
				BC_ASSERT_PTR_NOT_NULL(cServerChatRoom);
				if (cServerChatRoom) {
					auto chatRoom = AbstractChatRoom::toCpp(cServerChatRoom)->getSharedFromThis();
					BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, berthe, michelle, laure}).wait([&chatRoom] {
						for (auto participant : chatRoom->getParticipants()) {
							for (auto device : participant->getDevices()) {
								if (device->getState() != ParticipantDevice::State::Present) {
									return false;
								}
							}
						}
						return true;
					}));
				}

				BC_ASSERT_TRUE(wait_for_list(coresList, &core.getStats().number_of_LinphoneConferenceStateCreated,
				                             initialPeerStats.number_of_LinphoneConferenceStateCreated + 1,
				                             liblinphone_tester_sip_timeout));
			}
		}

		initialMarieStats = marie.getStats();
		initialMichelleStats = michelle.getStats();
		initialBertheStats = berthe.getStats();
		initialLaureStats = laure.getStats();

		for (int idx = 0; idx < 2; idx++) {
			for (ClientConference &msg_core : cores) {
				for (auto chatRoom : msg_core.getCore().getChatRooms()) {
					const auto &conferenceAddress = chatRoom->getConferenceAddress();
					// Store conference ID to verify that no events are left matching it after restarting the core
					std::string msg_text = std::string("Core [") + msg_core.getIdentity().toString() +
					                       std::string("] Message[") + std::to_string(idx) +
					                       std::string("]: Welcome to all to chatroom ") +
					                       conferenceAddress->toString();
					LinphoneChatMessage *msg = ClientConference::sendTextMsg(chatRoom->toC(), msg_text);

					BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, berthe, michelle, laure}).wait([&msg] {
						return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
					}));
					linphone_chat_message_unref(msg);

					for (ClientConference &core : cores) {
						if (core.searchChatRoom(nullptr, conferenceAddress->toC()) &&
						    (msg_core.getLc() != core.getLc())) {
							BC_ASSERT_TRUE(
							    CoreManagerAssert({focus, marie, michelle, berthe, laure})
							        .waitUntil(chrono::seconds(10), [&core, &msg_text] {
								        LinphoneChatMessage *lastMsg = core.getStats().last_received_chat_message;
								        bool ret = false;
								        if (lastMsg) {
									        std::string msgText(linphone_chat_message_get_utf8_text(lastMsg));
									        ret = (msgText == msg_text);
								        }
								        return ret;
							        }));
						}
					}
				}
			}
		}

		stats initialFocusStats = focus.getStats();
		// Restart all cores so that all chatroom handlers are now attached to the ClientConferenceListEventHandler
		for (ClientConference &core : cores) {
			ms_message("%s is restarting its core before one-on-one chatroom being deleted",
			           linphone_core_get_identity(core.getLc()));
			coresList = bctbx_list_remove(coresList, core.getLc());
			linphone_core_manager_reinit(core.getCMgr());
			core.configure(focus.getConferenceFactoryAddress(), lime_algo);
			linphone_config_set_int(linphone_core_get_config(core.getLc()), "sip", "conference_subscribe_expires",
			                        subscribe_expires_value_s);
			linphone_core_manager_start(core.getCMgr(), TRUE);
			coresList = bctbx_list_append(coresList, core.getLc());
			BC_ASSERT_TRUE(wait_for_list(coresList, &core.getStats().number_of_LinphoneSubscriptionActive, 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(
			    CoreManagerAssert({focus, marie, michelle, berthe, laure})
			        .waitUntil(chrono::seconds(10), [&focus, &core] { return checkChatroom(focus, core, -1); }));
		}

		for (int idx = 0; idx < 2; idx++) {
			for (const ClientConference &msg_core : cores) {
				for (auto chatRoom : msg_core.getCore().getChatRooms()) {
					const auto &conferenceAddress = chatRoom->getConferenceAddress();
					// Store conference ID to verify that no events are left matching it after restarting the core
					std::string msg_text = std::string("Core [") + msg_core.getIdentity().toString() +
					                       std::string("] Message[") + std::to_string(idx) +
					                       std::string("]: Chatroom ") + conferenceAddress->toString() +
					                       std::string(" is working so well");
					LinphoneChatMessage *msg = ClientConference::sendTextMsg(chatRoom->toC(), msg_text);

					BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, berthe, michelle, laure}).wait([&msg] {
						return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
					}));
					linphone_chat_message_unref(msg);

					for (ClientConference &core : cores) {
						if (core.searchChatRoom(nullptr, conferenceAddress->toC()) &&
						    (msg_core.getLc() != core.getLc())) {
							BC_ASSERT_TRUE(
							    CoreManagerAssert({focus, marie, michelle, berthe, laure})
							        .waitUntil(chrono::seconds(10), [&core, &msg_text] {
								        LinphoneChatMessage *lastMsg = core.getStats().last_received_chat_message;
								        bool ret = false;
								        if (lastMsg) {
									        std::string msgText(linphone_chat_message_get_utf8_text(lastMsg));
									        ret = (msgText == msg_text);
								        }
								        return ret;
							        }));
						}
					}
				}
			}
		}

		// Allow all subscriptions to expire
		CoreManagerAssert({focus, marie, berthe, michelle, laure})
		    .waitUntil(chrono::seconds(subscribe_expires_value_s), [] { return false; });

		initialFocusStats = focus.getStats();
		initialMarieStats = marie.getStats();
		initialMichelleStats = michelle.getStats();
		initialBertheStats = berthe.getStats();
		initialLaureStats = laure.getStats();

		auto totalNbChatRooms = focus.getCore().getChatRooms().size();

		bctbx_list_t *participantsAddresses = NULL;
		Address michelleAddr = michelle.getIdentity();
		participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_ref(michelleAddr.toC()));

		auto oneOnOneChatRoom =
		    linphone_core_search_chat_room(marie.getLc(), NULL, marie.getIdentity().toC(), NULL, participantsAddresses);
		bctbx_list_free_with_data(participantsAddresses, (bctbx_list_free_func)linphone_address_unref);
		participantsAddresses = NULL;
		BC_ASSERT_PTR_NOT_NULL(oneOnOneChatRoom);
		if (oneOnOneChatRoom) {
			ms_message("%s deletes one-on-one chat room %s", linphone_core_get_identity(marie.getLc()),
			           AbstractChatRoom::toCpp(oneOnOneChatRoom)->getConferenceAddress()->toString().c_str());
			linphone_core_delete_chat_room(marie.getLc(), oneOnOneChatRoom);
		}

		// wait until chatroom is deleted server side
		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, michelle, laure, berthe}).wait([&focus, &totalNbChatRooms] {
			return focus.getCore().getChatRooms().size() == (totalNbChatRooms - 1);
		}));

		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneChatRoomStateDeleted,
		                             initialFocusStats.number_of_LinphoneChatRoomStateDeleted + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneChatRoomStateDeleted,
		                             initialMarieStats.number_of_LinphoneChatRoomStateDeleted + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneChatRoomStateTerminated,
		                             initialMichelleStats.number_of_LinphoneChatRoomStateTerminated + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_FALSE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneChatRoomStateTerminated,
		                              initialLaureStats.number_of_LinphoneChatRoomStateTerminated + 1, 1000));
		BC_ASSERT_FALSE(wait_for_list(coresList, &berthe.getStats().number_of_LinphoneChatRoomStateTerminated,
		                              initialBertheStats.number_of_LinphoneChatRoomStateTerminated + 1, 1000));

		BC_ASSERT_FALSE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneSubscriptionTerminated,
		                              initialFocusStats.number_of_LinphoneSubscriptionTerminated + 1, 100));
		BC_ASSERT_FALSE(wait_for_list(coresList, &berthe.getStats().number_of_LinphoneSubscriptionTerminated,
		                              initialBertheStats.number_of_LinphoneSubscriptionTerminated + 1, 100));
		BC_ASSERT_FALSE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneSubscriptionTerminated,
		                              initialLaureStats.number_of_LinphoneSubscriptionTerminated + 1, 100));
		BC_ASSERT_FALSE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneSubscriptionTerminated,
		                              initialMichelleStats.number_of_LinphoneSubscriptionTerminated + 1, 100));
		BC_ASSERT_FALSE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneSubscriptionTerminated,
		                              initialMarieStats.number_of_LinphoneSubscriptionTerminated + 1, 100));

		// wait a bit longer to detect side effect if any
		CoreManagerAssert({focus, marie, michelle, berthe, laure}).waitUntil(chrono::seconds(2), [] { return false; });

		for (int idx = 0; idx < 2; idx++) {
			for (const ClientConference &msg_core : cores) {
				for (auto chatRoom : msg_core.getCore().getChatRooms()) {
					const auto &conferenceAddress = chatRoom->getConferenceAddress();
					// Store conference ID to verify that no events are left matching it after restarting the core
					std::string msg_text = std::string("Core [") + msg_core.getIdentity().toString() +
					                       std::string("] Message[") + std::to_string(idx) +
					                       std::string("]: Is chatroom ") + conferenceAddress->toString() +
					                       std::string(" still operational?");
					LinphoneChatMessage *msg = ClientConference::sendTextMsg(chatRoom->toC(), msg_text);

					BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, berthe, michelle, laure}).wait([&msg] {
						return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
					}));
					linphone_chat_message_unref(msg);

					for (ClientConference &core : cores) {
						if (core.searchChatRoom(nullptr, conferenceAddress->toC()) &&
						    (msg_core.getLc() != core.getLc())) {
							BC_ASSERT_TRUE(
							    CoreManagerAssert({focus, marie, michelle, berthe, laure})
							        .waitUntil(chrono::seconds(10), [&core, &msg_text] {
								        LinphoneChatMessage *lastMsg = core.getStats().last_received_chat_message;
								        bool ret = false;
								        if (lastMsg) {
									        std::string msgText(linphone_chat_message_get_utf8_text(lastMsg));
									        ret = (msgText == msg_text);
								        }
								        return ret;
							        }));
						}
					}
				}
			}
		}

		initialFocusStats = focus.getStats();
		// Restart all cores so that all chatroom handlers are now attached to the ClientConferenceListEventHandler
		for (ClientConference &core : cores) {
			ms_message("%s is restarting its core before group chatroom being deleted",
			           linphone_core_get_identity(core.getLc()));
			coresList = bctbx_list_remove(coresList, core.getLc());
			// Restart core
			core.reStart();
			coresList = bctbx_list_append(coresList, core.getLc());
			BC_ASSERT_TRUE(wait_for_list(coresList, &core.getStats().number_of_LinphoneSubscriptionActive, 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(
			    CoreManagerAssert({focus, marie, michelle, berthe, laure})
			        .waitUntil(chrono::seconds(10), [&focus, &core] { return checkChatroom(focus, core, -1); }));
		}

		for (int idx = 0; idx < 2; idx++) {
			for (const ClientConference &msg_core : cores) {
				for (auto chatRoom : msg_core.getCore().getChatRooms()) {
					const auto &conferenceAddress = chatRoom->getConferenceAddress();
					// Store conference ID to verify that no events are left matching it after restarting the core
					std::string msg_text =
					    std::string("Core [") + msg_core.getIdentity().toString() + std::string("] Message[") +
					    std::to_string(idx) +
					    std::string("]: Double checking that messages can be sent and received through chatroom ") +
					    conferenceAddress->toString();
					LinphoneChatMessage *msg = ClientConference::sendTextMsg(chatRoom->toC(), msg_text);

					BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, berthe, michelle, laure}).wait([&msg] {
						return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
					}));
					linphone_chat_message_unref(msg);

					for (ClientConference &core : cores) {
						if (core.searchChatRoom(nullptr, conferenceAddress->toC()) &&
						    (msg_core.getLc() != core.getLc())) {
							BC_ASSERT_TRUE(
							    CoreManagerAssert({focus, marie, michelle, berthe, laure})
							        .waitUntil(chrono::seconds(10), [&core, &msg_text] {
								        LinphoneChatMessage *lastMsg = core.getStats().last_received_chat_message;
								        bool ret = false;
								        if (lastMsg) {
									        std::string msgText(linphone_chat_message_get_utf8_text(lastMsg));
									        ret = (msgText == msg_text);
								        }
								        return ret;
							        }));
						}
					}
				}
			}
		}

		// Allow all subscriptions to expire
		CoreManagerAssert({focus, marie, berthe, michelle, laure})
		    .waitUntil(chrono::seconds(subscribe_expires_value_s), [] { return false; });

		initialFocusStats = focus.getStats();
		initialMarieStats = marie.getStats();
		initialMichelleStats = michelle.getStats();
		initialBertheStats = berthe.getStats();
		initialLaureStats = laure.getStats();

		std::shared_ptr<AbstractChatRoom> groupChatRoomToDelete;
		for (auto chatRoom : laure.getCore().getChatRooms()) {
			if (chatRoom->getCurrentParams()->isGroup()) {
				groupChatRoomToDelete = chatRoom;
				break;
			}
		}
		BC_ASSERT_PTR_NOT_NULL(groupChatRoomToDelete);
		if (oneOnOneChatRoom) {
			ms_message("%s deletes group chat room %s", linphone_core_get_identity(laure.getLc()),
			           groupChatRoomToDelete->getConferenceAddress()->toString().c_str());
			linphone_core_delete_chat_room(laure.getLc(), groupChatRoomToDelete->toC());
		}

		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_participants_removed,
		                             initialFocusStats.number_of_participants_removed, liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_participants_removed,
		                             initialMarieStats.number_of_participants_removed, liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_participants_removed,
		                             initialMichelleStats.number_of_participants_removed,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &berthe.getStats().number_of_participants_removed,
		                             initialBertheStats.number_of_participants_removed,
		                             liblinphone_tester_sip_timeout));

		BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_participant_devices_removed,
		                             initialFocusStats.number_of_participant_devices_removed,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_participant_devices_removed,
		                             initialMarieStats.number_of_participant_devices_removed,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_participant_devices_removed,
		                             initialMichelleStats.number_of_participant_devices_removed,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &berthe.getStats().number_of_participant_devices_removed,
		                             initialBertheStats.number_of_participant_devices_removed,
		                             liblinphone_tester_sip_timeout));

		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneChatRoomStateTerminated,
		                             initialLaureStats.number_of_LinphoneChatRoomStateTerminated + 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneChatRoomStateDeleted,
		                             initialLaureStats.number_of_LinphoneChatRoomStateDeleted + 1,
		                             liblinphone_tester_sip_timeout));

		BC_ASSERT_FALSE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneSubscriptionTerminated,
		                              initialFocusStats.number_of_LinphoneSubscriptionTerminated + 1, 100));
		BC_ASSERT_FALSE(wait_for_list(coresList, &berthe.getStats().number_of_LinphoneSubscriptionTerminated,
		                              initialBertheStats.number_of_LinphoneSubscriptionTerminated + 1, 100));
		BC_ASSERT_FALSE(wait_for_list(coresList, &laure.getStats().number_of_LinphoneSubscriptionTerminated,
		                              initialLaureStats.number_of_LinphoneSubscriptionTerminated + 1, 100));
		BC_ASSERT_FALSE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneSubscriptionTerminated,
		                              initialMichelleStats.number_of_LinphoneSubscriptionTerminated + 1, 100));
		BC_ASSERT_FALSE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneSubscriptionTerminated,
		                              initialMarieStats.number_of_LinphoneSubscriptionTerminated + 1, 100));

		// wait a bit longer to detect side effect if any
		CoreManagerAssert({focus, marie, michelle, berthe, laure}).waitUntil(chrono::seconds(2), [] { return false; });

		for (int idx = 0; idx < 2; idx++) {
			for (const ClientConference &msg_core : cores) {
				for (auto chatRoom : msg_core.getCore().getChatRooms()) {
					const auto &conferenceAddress = chatRoom->getConferenceAddress();
					// Store conference ID to verify that no events are left matching it after restarting the core
					std::string msg_text = std::string("Core [") + msg_core.getIdentity().toString() +
					                       std::string("] Message[") + std::to_string(idx) +
					                       std::string("]: Verifying again if chatroom ") +
					                       conferenceAddress->toString() + std::string(" is functioning");
					LinphoneChatMessage *msg = ClientConference::sendTextMsg(chatRoom->toC(), msg_text);

					BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, berthe, michelle, laure}).wait([&msg] {
						return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
					}));
					linphone_chat_message_unref(msg);

					for (ClientConference &core : cores) {
						if (core.searchChatRoom(nullptr, conferenceAddress->toC()) &&
						    (msg_core.getLc() != core.getLc())) {
							BC_ASSERT_TRUE(
							    CoreManagerAssert({focus, marie, michelle, berthe, laure})
							        .waitUntil(chrono::seconds(10), [&core, &msg_text] {
								        LinphoneChatMessage *lastMsg = core.getStats().last_received_chat_message;
								        bool ret = false;
								        if (lastMsg) {
									        std::string msgText(linphone_chat_message_get_utf8_text(lastMsg));
									        ret = (msgText == msg_text);
								        }
								        return ret;
							        }));
						}
					}
				}
			}
		}

		for (auto chatRoom : focus.getCore().getChatRooms()) {
			for (auto participant : chatRoom->getParticipants()) {
				//  force deletion by removing devices
				std::shared_ptr<Address> participantAddress = participant->getAddress();
				linphone_chat_room_set_participant_devices(chatRoom->toC(), participantAddress->toC(), NULL);
			}
		}

		// wait until chatroom is deleted server side
		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, berthe, michelle, laure}).wait([&focus] {
			return focus.getCore().getChatRooms().size() == 0;
		}));

		// wait a bit longer to detect side effect if any
		CoreManagerAssert({focus, marie, berthe, michelle, laure}).waitUntil(chrono::seconds(2), [] { return false; });

		// to avoid creation attempt of a new chatroom
		LinphoneProxyConfig *config = linphone_core_get_default_proxy_config(focus.getLc());
		linphone_proxy_config_edit(config);
		linphone_proxy_config_set_conference_factory_uri(config, NULL);
		linphone_proxy_config_done(config);

		bctbx_list_free(coresList);
	}
}

void one_on_one_chat_room_deleted_before_200ok_base(bool encrypted, bool server_restart) {
	Focus focus("chloe_rc");
	LinphoneChatMessage *msg;
	{ // to make sure focus is destroyed after clients.
		const LinphoneTesterLimeAlgo lime_algo = encrypted ? C25519 : UNSET;

		linphone_core_enable_lime_x3dh(focus.getLc(), true);

		ClientConference marie("marie_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference pauline("pauline_rc", focus.getConferenceFactoryAddress(), lime_algo);

		focus.registerAsParticipantDevice(marie);
		focus.registerAsParticipantDevice(pauline);

		stats marie_stat = marie.getStats();
		stats pauline_stat = pauline.getStats();
		bctbx_list_t *coresList = bctbx_list_append(NULL, focus.getLc());
		coresList = bctbx_list_append(coresList, marie.getLc());
		coresList = bctbx_list_append(coresList, pauline.getLc());

		if (encrypted) {
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(marie.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(pauline.getLc()));
		}

		Address paulineAddr = pauline.getIdentity();
		bctbx_list_t *participantsAddresses = bctbx_list_append(NULL, linphone_address_ref(paulineAddr.toC()));

		// Marie creates a new group chat room
		const char *initialSubject = "One on one";
		LinphoneChatRoom *marieCr =
		    create_chat_room_client_side(coresList, marie.getCMgr(), &marie_stat, participantsAddresses, initialSubject,
		                                 encrypted, LinphoneChatRoomEphemeralModeDeviceManaged);
		BC_ASSERT_PTR_NOT_NULL(marieCr);
		const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);
		char *conference_address_string = linphone_address_as_string(confAddr);

		// Check that the chat room is correctly created on Pauline's side and that the participants are added
		LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline.getCMgr(), &pauline_stat,
		                                                                   confAddr, initialSubject, 1, FALSE);
		BC_ASSERT_PTR_NOT_NULL(paulineCr);

		if (paulineCr && marieCr) {
			// Marie sends the message
			const char *msg_text = "Hello Paully";
			msg = _send_message(marieCr, msg_text);
			BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneMessageReceived,
			                             pauline_stat.number_of_LinphoneMessageReceived + 1,
			                             liblinphone_tester_sip_timeout));
			LinphoneChatMessage *paulineLastMsg = pauline.getStats().last_received_chat_message;
			linphone_chat_message_unref(msg);
			BC_ASSERT_PTR_NOT_NULL(paulineLastMsg);

			// Check that the message was correctly decrypted if encrypted
			BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_utf8_text(paulineLastMsg), msg_text);
			LinphoneAddress *marieAddr = marie.getCMgr()->identity;
			BC_ASSERT_TRUE(
			    linphone_address_weak_equal(marieAddr, linphone_chat_message_get_from_address(paulineLastMsg)));

			LinphoneAddress *paulineLocalAddr = linphone_address_clone(linphone_chat_room_get_local_address(paulineCr));
			LinphoneAddress *paulinePeerAddr = linphone_address_clone(linphone_chat_room_get_peer_address(paulineCr));

			if (!!server_restart) {
				ms_message("%s restarts its core", linphone_core_get_identity(focus.getLc()));
				coresList = bctbx_list_remove(coresList, focus.getLc());
				focus.reStart();
				coresList = bctbx_list_append(coresList, focus.getLc());
			}

			// Restart pauline so that she has to send an INVITE and BYE it to exit the chatroom
			ms_message("%s restarts its core", linphone_core_get_identity(pauline.getLc()));
			coresList = bctbx_list_remove(coresList, pauline.getLc());
			pauline.reStart();
			coresList = bctbx_list_append(coresList, pauline.getLc());
			paulineCr = linphone_core_search_chat_room(pauline.getLc(), NULL, paulineLocalAddr, paulinePeerAddr, NULL);
			BC_ASSERT_PTR_NOT_NULL(paulineCr);

			LinphoneChatRoom *focusCr =
			    linphone_core_search_chat_room(focus.getLc(), NULL, NULL, paulinePeerAddr, NULL);
			BC_ASSERT_PTR_NOT_NULL(focusCr);

			if (focusCr) {
				LinphoneParticipant *paulineParticipant =
				    linphone_chat_room_find_participant(focusCr, paulineLocalAddr);
				BC_ASSERT_PTR_NOT_NULL(paulineParticipant);
				if (paulineParticipant) {
					LinphoneParticipantDevice *paulineDevice =
					    linphone_participant_find_device(paulineParticipant, paulineLocalAddr);
					BC_ASSERT_PTR_NOT_NULL(paulineDevice);
				}
			}

			linphone_address_unref(paulineLocalAddr);
			linphone_address_unref(paulinePeerAddr);

			pauline_stat = pauline.getStats();
			stats focus_stat = focus.getStats();

			LinphoneChatRoom *paulineCrReffed = linphone_chat_room_ref(paulineCr);

			ms_message("%s leaves chatroom %s", linphone_core_get_identity(pauline.getLc()), conference_address_string);
			linphone_chat_room_leave(paulineCr);
			BC_ASSERT_TRUE(wait_for_list(
			    coresList, &pauline.getStats().number_of_LinphoneChatRoomStateTerminationPending,
			    pauline_stat.number_of_LinphoneChatRoomStateTerminationPending + 1, liblinphone_tester_sip_timeout));

			BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneChatRoomSessionConnected,
			                             focus_stat.number_of_LinphoneChatRoomSessionConnected + 1,
			                             liblinphone_tester_sip_timeout));

			BC_ASSERT_EQUAL(pauline.getStats().number_of_LinphoneChatRoomStateTerminated,
			                pauline_stat.number_of_LinphoneChatRoomStateTerminated, int, "%d");

			linphone_core_delete_chat_room(pauline.getLc(), paulineCr);
			BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneChatRoomStateDeleted,
			                             pauline_stat.number_of_LinphoneChatRoomStateDeleted + 1,
			                             liblinphone_tester_sip_timeout));

			BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneChatRoomStateTerminated,
			                             pauline_stat.number_of_LinphoneChatRoomStateTerminated + 1,
			                             liblinphone_tester_sip_timeout));

			BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneChatRoomSessionEnd,
			                             pauline_stat.number_of_LinphoneChatRoomSessionEnd + 1,
			                             liblinphone_tester_sip_timeout));

			// wait until chatroom is deleted on Pauline's side
			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline}).wait([&pauline] {
				return pauline.getCore().getChatRooms().size() == 0;
			}));

			Address marieCoreAddr = marie.getIdentity();
			participantsAddresses = bctbx_list_append(NULL, linphone_address_ref(marieCoreAddr.toC()));

			pauline_stat = pauline.getStats();
			// Marie creates a new group chat room
			const char *initialSubject = "One on one with Marie";
			paulineCr =
			    create_chat_room_client_side(coresList, pauline.getCMgr(), &pauline_stat, participantsAddresses,
			                                 initialSubject, encrypted, LinphoneChatRoomEphemeralModeDeviceManaged);
			BC_ASSERT_PTR_NOT_NULL(paulineCr);

			const std::initializer_list<std::reference_wrapper<ConfCoreManager>> cores{focus, marie, pauline};
			for (const ConfCoreManager &core : cores) {
				BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline}).wait([&core, &focus] {
					auto &chatRooms = core.getCore().getChatRooms();
					if (chatRooms.size() != 1) {
						return false;
					}
					for (auto chatRoom : chatRooms) {
						if (chatRoom->getState() != ConferenceInterface::State::Created) {
							return false;
						}
						size_t expectedParticipantNb = (core.getLc() == focus.getLc()) ? 2 : 1;
						const auto &participants = chatRoom->getParticipants();
						if (participants.size() != expectedParticipantNb) {
							return false;
						}
						for (const auto &participant : participants) {
							for (const auto &device : participant->getDevices()) {
								if (device->getState() != ParticipantDevice::State::Present) {
									return false;
								}
							}
						}
					}
					return true;
				}));
			}

			linphone_chat_room_unref(paulineCrReffed);
			pauline_stat = pauline.getStats();
			// Marie sends the message
			msg_text = "Paully, are you back?";
			msg = _send_message(marieCr, msg_text);
			BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneMessageReceived,
			                             pauline_stat.number_of_LinphoneMessageReceived + 1,
			                             liblinphone_tester_sip_timeout));
			paulineLastMsg = pauline.getStats().last_received_chat_message;
			linphone_chat_message_unref(msg);
			BC_ASSERT_PTR_NOT_NULL(paulineLastMsg);
			if (paulineLastMsg) {
				BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_utf8_text(paulineLastMsg), msg_text);
			}

			marie_stat = marie.getStats();
			// Marie sends the message
			msg_text = "Yes, Mary, I am back in";
			msg = _send_message(paulineCr, msg_text);
			BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneMessageReceived,
			                             marie_stat.number_of_LinphoneMessageReceived + 1,
			                             liblinphone_tester_sip_timeout));
			LinphoneChatMessage *marieLastMsg = marie.getStats().last_received_chat_message;
			linphone_chat_message_unref(msg);
			BC_ASSERT_PTR_NOT_NULL(marieLastMsg);
			if (marieLastMsg) {
				BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_utf8_text(marieLastMsg), msg_text);
			}

			for (auto chatRoom : focus.getCore().getChatRooms()) {
				for (auto participant : chatRoom->getParticipants()) {
					//  force deletion by removing devices
					std::shared_ptr<Address> participantAddress = participant->getAddress();
					linphone_chat_room_set_participant_devices(chatRoom->toC(), participantAddress->toC(), NULL);
				}
			}

			// wait until chatroom is deleted server side
			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline}).waitUntil(chrono::seconds(120), [&focus] {
				return focus.getCore().getChatRooms().size() == 0;
			}));

			// wait a bit longer to detect side effect if any
			CoreManagerAssert({focus, marie, pauline}).waitUntil(chrono::seconds(2), [] { return false; });

			// to avoid creation attempt of a new chatroom
			auto focus_account = focus.getDefaultAccount();
			LinphoneAccountParams *params = linphone_account_params_clone(linphone_account_get_params(focus_account));
			linphone_account_params_set_conference_factory_uri(params, NULL);
			linphone_account_set_params(focus_account, params);
			linphone_account_params_unref(params);

			bctbx_list_free(coresList);
			ms_free(conference_address_string);
		}
	}
}

static void server_core_chat_room_conference_legacy_address_generation(LinphoneChatRoom *cr) {
	Focus *focus =
	    (Focus *)(((LinphoneCoreManager *)linphone_core_get_user_data(linphone_chat_room_get_core(cr)))->user_info);
	LinphoneAddress *conference_address =
	    linphone_address_clone(linphone_account_get_contact_address(linphone_core_get_default_account(focus->getLc())));
	char identifier[50];
	belle_sip_random_token(identifier, sizeof(identifier));
	char *username = ms_strdup_printf("chatroom-%s-legacy", identifier);
	linphone_address_set_username(conference_address, username);
	ms_free(username);
	linphone_chat_room_set_conference_address(cr, conference_address);
	linphone_address_unref(conference_address);
}

static void
legacy_server_core_chat_room_state_changed(LinphoneCore *core, LinphoneChatRoom *cr, LinphoneChatRoomState state) {
	Focus *focus = (Focus *)(((LinphoneCoreManager *)linphone_core_get_user_data(core))->user_info);
	switch (state) {
		case LinphoneChatRoomStateInstantiated: {
			LinphoneChatRoomCbs *cbs = linphone_factory_create_chat_room_cbs(linphone_factory_get());
			linphone_chat_room_cbs_set_conference_address_generation(
			    cbs, server_core_chat_room_conference_legacy_address_generation);
			setup_chat_room_callbacks(cbs);
			linphone_chat_room_add_callbacks(cr, cbs);
			linphone_chat_room_cbs_set_user_data(cbs, focus);
			linphone_chat_room_cbs_unref(cbs);
			break;
		}
		default:
			break;
	}
}

static std::string generate_random_alphanum_string(size_t length) {
	const std::string characters = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	std::random_device randomDevice;
	std::mt19937 generator(randomDevice());
	std::uniform_int_distribution<> distribution(0, static_cast<int>(characters.size() - 1));
	std::string result;
	for (size_t i = 0; i < length; ++i) {
		result += characters[distribution(generator)];
	}
	return result;
}

static LinphoneAccount *add_account_using_domain_registration(ConfCoreManager &core, bool set_as_default) {
	int old_registration_ok = core.getStats().number_of_LinphoneRegistrationOk;
	char *full_domain = ms_strdup_printf("test.%s.%s", generate_random_alphanum_string(20).c_str(),
	                                     test_domain_registration_base_domain);
	LinphoneAuthInfo *ai =
	    linphone_auth_info_new(NULL, NULL, "domain-registration-secret", NULL, full_domain, full_domain);
	linphone_core_add_auth_info(core.getLc(), ai);
	linphone_auth_info_unref(ai);

	LinphoneAccountParams *params = linphone_core_create_account_params(core.getLc());

	// Needs to be set before setting the identity address as it will disable the check on the identity
	// address's username.
	linphone_account_params_use_domain_registrations(params, TRUE);

	// Identity address
	char *identity_str = ms_strdup_printf("sip:%s", full_domain);
	LinphoneAddress *identity = linphone_address_new(identity_str);
	ms_free(identity_str);
	BC_ASSERT_PTR_NOT_NULL(identity);
	linphone_account_params_set_identity_address(params, identity);

	// Server address
	char *server_address_str = ms_strdup_printf("sip:%s;transport=tcp", domain_registration_auth_domain);
	LinphoneAddress *server_address = linphone_address_new(server_address_str);
	ms_free(server_address_str);
	BC_ASSERT_PTR_NOT_NULL(server_address);
	linphone_account_params_set_server_address(params, server_address);
	linphone_address_unref(server_address);

	// Route
	char *route_str = ms_strdup_printf("sip:%s;transport=tcp", domain_registration_auth_domain);
	LinphoneAddress *route = linphone_address_new(route_str);
	ms_free(route_str);
	BC_ASSERT_PTR_NOT_NULL(route);
	bctbx_list_t *routes = bctbx_list_append(NULL, route);
	linphone_account_params_set_routes_addresses(params, routes);
	bctbx_list_free_with_data(routes, (bctbx_list_free_func)linphone_address_unref);

	// Conference factory
	char *factory_str = ms_strdup_printf("sip:conference-factory@%s", full_domain);
	LinphoneAddress *factory = linphone_address_new(factory_str);
	ms_free(factory_str);
	BC_ASSERT_PTR_NOT_NULL(factory);
	linphone_account_params_set_conference_factory_address(params, factory);
	linphone_address_unref(factory);

	linphone_account_params_enable_register(params, TRUE);

	LinphoneAccount *account = linphone_core_create_account(core.getLc(), params);
	linphone_core_add_account(core.getLc(), account);
	if (set_as_default) {
		linphone_core_set_default_account(core.getLc(), account);
	}
	linphone_account_params_unref(params);
	linphone_account_unref(account);
	ms_free(full_domain);

	BC_ASSERT_TRUE(wait_for_until(core.getLc(), NULL, &core.getStats().number_of_LinphoneRegistrationOk,
	                              old_registration_ok + 1, 20000));

	LinphoneAccount *added_account = linphone_core_lookup_account_by_identity_strict(core.getLc(), identity);
	BC_ASSERT_PTR_NOT_NULL(added_account);
	linphone_address_unref(identity);

	return added_account;
}

static void createChatRooms(int number,
                            std::initializer_list<std::reference_wrapper<CoreManager>> coreMgrs,
                            std::initializer_list<std::reference_wrapper<ClientConference>> participantMgrs,
                            Focus &focus,
                            LinphoneCoreManager *organizer,
                            std::string baseSubject,
                            bool encrypted,
                            bool isLegacy,
                            bool sendMessage) {
	bctbx_list_t *coresList = NULL;
	for (CoreManager &core : coreMgrs) {
		LinphoneCore *c_core = core.getCore().getCCore();
		coresList = bctbx_list_append(coresList, c_core);
	}
	Address focusFactoryAddress = focus.getIdentity();
	for (int idx = 0; idx < number; idx++) {
		bctbx_list_t *participantsAddresses = NULL;
		std::map<LinphoneCoreManager *, stats> managerStatsMap;
		for (const ClientConference &mgr : participantMgrs) {
			Address addr = mgr.getIdentity();
			participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_clone(addr.toC()));
			managerStatsMap.insert(std::make_pair(mgr.getCMgr(), mgr.getStats()));
		}
		stats organizerStats = organizer->stat;
		// Marie creates a new group chat room
		std::string subject = baseSubject + std::string(": chatroom number ") + std::to_string(idx) +
		                      std::string(" on ") + focusFactoryAddress.toString();
		ms_message("%s creates chat on conference server %s with subject '%s'",
		           linphone_core_get_identity(organizer->lc), focusFactoryAddress.toString().c_str(), subject.c_str());

		LinphoneChatRoom *organizerCr =
		    create_chat_room_client_side(coresList, organizer, &organizerStats, participantsAddresses, subject.c_str(),
		                                 encrypted, LinphoneChatRoomEphemeralModeDeviceManaged);
		const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(organizerCr);
		char *confAddrString = nullptr;
		BC_ASSERT_PTR_NOT_NULL(confAddr);
		if (confAddr) {
			if (isLegacy) {
				BC_ASSERT_FALSE(linphone_address_has_uri_param(confAddr, Conference::kConfIdParameter.c_str()));
			} else {
				BC_ASSERT_TRUE(linphone_address_has_uri_param(confAddr, Conference::kConfIdParameter.c_str()));
			}
			confAddrString = linphone_address_as_string(confAddr);
		} else {
			confAddrString = ms_strdup("sip:");
		}

		int participantNumber = static_cast<int>(participantMgrs.size());
		for (const auto &[mgr, statTuple] : managerStatsMap) {
			stats stat = statTuple;
			// Check that the chat room is correctly created on Pauline's side and that the participants are added
			LinphoneChatRoom *participantCr = check_creation_chat_room_client_side(
			    coresList, mgr, &stat, confAddr, subject.c_str(), participantNumber, FALSE);
			BC_ASSERT_PTR_NOT_NULL(participantCr);
		}

		BC_ASSERT_TRUE(CoreManagerAssert(coreMgrs).wait([&focus, &organizer] {
			for (auto chatRoom : focus.getCore().getChatRooms()) {
				for (auto participant : chatRoom->getParticipants()) {
					for (auto device : participant->getDevices()) {
						if (device->getState() != ParticipantDevice::State::Present) {
							return false;
						}
					}
					auto organizer_participant =
					    linphone_chat_room_find_participant(chatRoom->toC(), organizer->identity);
					BC_ASSERT_PTR_NOT_NULL(organizer_participant);
					if (organizer_participant) {
						BC_ASSERT_TRUE(linphone_participant_is_admin(organizer_participant));
					}
				}
			}
			return true;
		}));

		if (sendMessage) {
			std::string msgText = baseSubject + std::string(": message in chatroom ") + confAddrString +
			                      std::string(" subject ") + subject;

			LinphoneChatMessage *msg = ClientConference::sendTextMsg(organizerCr, msgText);
			BC_ASSERT_TRUE(CoreManagerAssert(coreMgrs).wait(
			    [msg] { return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered); }));

			for (ClientConference &mgr : participantMgrs) {
				auto participantCr = mgr.searchChatRoom(nullptr, confAddr);
				BC_ASSERT_PTR_NOT_NULL(participantCr);
				if (participantCr) {
					BC_ASSERT_TRUE(CoreManagerAssert(coreMgrs).wait(
					    [participantCr] { return linphone_chat_room_get_unread_messages_count(participantCr) == 1; }));
				}
			}
			linphone_chat_message_unref(msg);
		}
		ms_free(confAddrString);
	}
	bctbx_list_free(coresList);
}

void legacy_and_new_chatrooms_mixed_up_base(bool encrypted) {
	Focus focus("chloe_rc");
	{ // to make sure focus is destroyed after clients.

		const LinphoneTesterLimeAlgo lime_algo = encrypted ? C25519 : UNSET;
		linphone_core_enable_lime_x3dh(focus.getLc(), !!encrypted);

		BC_ASSERT_PTR_NOT_NULL(add_account_using_domain_registration(focus, true));

		ClientConference marie("marie_domain_registration_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference pauline("pauline_domain_registration_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference michelle("michelle_domain_registration_rc", focus.getConferenceFactoryAddress(), lime_algo);

		focus.registerAsParticipantDevice(marie);
		focus.registerAsParticipantDevice(pauline);
		focus.registerAsParticipantDevice(michelle);

		if (encrypted) {
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(marie.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(pauline.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(michelle.getLc()));
		}

		bctbx_list_t *coresList = bctbx_list_append(NULL, focus.getLc());
		coresList = bctbx_list_append(coresList, marie.getLc());
		coresList = bctbx_list_append(coresList, pauline.getLc());
		coresList = bctbx_list_append(coresList, michelle.getLc());

		stats initialMarieStats = marie.getStats();
		stats initialPaulineStats = pauline.getStats();
		stats initialMichelleStats = michelle.getStats();

		Address paulineAddr = pauline.getIdentity();
		Address michelleAddr = michelle.getIdentity();

		LinphoneCoreCbs *cbs = linphone_factory_create_core_cbs(linphone_factory_get());
		linphone_core_cbs_set_chat_room_state_changed(cbs, legacy_server_core_chat_room_state_changed);
		linphone_core_add_callbacks(focus.getLc(), cbs);

		int nbLegacyChatRooms = 3;
		const std::initializer_list<std::reference_wrapper<CoreManager>> coreMgrs{marie, pauline, focus, michelle};
		const std::initializer_list<std::reference_wrapper<ClientConference>> participants{pauline, michelle};
		createChatRooms(nbLegacyChatRooms, coreMgrs, participants, focus, marie.getCMgr(), std::string("Legacy"),
		                encrypted, true, true);

		ms_message("%s is restarting its core", linphone_core_get_identity(pauline.getLc()));
		coresList = bctbx_list_remove(coresList, pauline.getLc());
		pauline.reStart();
		coresList = bctbx_list_append(coresList, pauline.getLc());

		// Verify that only one subscription is sent out to the conference server
		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneSubscriptionOutgoingProgress, 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_FALSE(
		    wait_for_list(coresList, &pauline.getStats().number_of_LinphoneSubscriptionOutgoingProgress, 2, 5000));

		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneSubscriptionActive, 1,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_FALSE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneSubscriptionActive, 2, 500));

		BC_ASSERT_EQUAL(pauline.getCore().getChatRooms().size(), static_cast<size_t>(nbLegacyChatRooms), size_t, "%zu");

		// Add now the conference id to the chatroom addresses
		linphone_core_remove_callbacks(focus.getLc(), cbs);
		linphone_core_cbs_unref(cbs);

		int nbChatRooms = 2;
		createChatRooms(nbChatRooms, coreMgrs, participants, focus, marie.getCMgr(), std::string("New"), encrypted,
		                false, true);

		ms_message("%s is restarting its core", linphone_core_get_identity(michelle.getLc()));
		coresList = bctbx_list_remove(coresList, michelle.getLc());
		michelle.reStart();
		coresList = bctbx_list_append(coresList, michelle.getLc());

		// Verify that only two subscriptions are sent out to the conference server (one towards the conference factory
		// and the other one for the conference focus)
		BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneSubscriptionOutgoingProgress, 2,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_FALSE(
		    wait_for_list(coresList, &michelle.getStats().number_of_LinphoneSubscriptionOutgoingProgress, 3, 5000));

		BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneSubscriptionActive, 2,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_FALSE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneSubscriptionActive, 3, 500));

		BC_ASSERT_EQUAL(michelle.getCore().getChatRooms().size(), static_cast<size_t>(nbChatRooms + nbLegacyChatRooms),
		                size_t, "%zu");

		Address focusFactoryAddress = focus.getIdentity();
		const std::initializer_list<std::reference_wrapper<CoreManager>> msgCores{focus, marie, pauline, michelle};
		for (auto chatRoom : marie.getCore().getChatRooms()) {
			initialMarieStats = marie.getStats();
			initialPaulineStats = pauline.getStats();
			initialMichelleStats = michelle.getStats();

			const auto &confAddr = chatRoom->getConferenceAddress();
			const auto confAddrString = confAddr->toString();
			std::string msgText = std::string("Trying out messaging - Another message in chatroom ") + confAddrString;
			std::map<LinphoneCoreManager *, int> members{{pauline.getCMgr(), 2}, {michelle.getCMgr(), 2}};
			sendMessageAndCheckReception(msgCores, members, msgText, chatRoom);

			std::string newSubject = std::string("New subject of chatroom ") + confAddrString + std::string(" on ") +
			                         focusFactoryAddress.toString();
			ms_message("%s changes the subject of chatroom %s to '%s'", linphone_core_get_identity(marie.getLc()),
			           chatRoom->getConferenceAddress()->toString().c_str(), newSubject.c_str());
			linphone_chat_room_set_subject(chatRoom->toC(), newSubject.c_str());
			BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_subject_changed,
			                             initialMarieStats.number_of_subject_changed + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_chat_room_subject_changed,
			                             initialPaulineStats.number_of_chat_room_subject_changed + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_chat_room_subject_changed,
			                             initialMichelleStats.number_of_chat_room_subject_changed + 1,
			                             liblinphone_tester_sip_timeout));
		}

		ms_message("%s is restarting its core", linphone_core_get_identity(marie.getLc()));
		coresList = bctbx_list_remove(coresList, marie.getLc());
		marie.reStart();
		coresList = bctbx_list_append(coresList, marie.getLc());

		// Verify that only two subscriptions are sent out to the conference server (one towards the conference factory
		// and the other one for the conference focus)
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneSubscriptionOutgoingProgress, 2,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_FALSE(
		    wait_for_list(coresList, &marie.getStats().number_of_LinphoneSubscriptionOutgoingProgress, 3, 5000));

		BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneSubscriptionActive, 2,
		                             liblinphone_tester_sip_timeout));
		BC_ASSERT_FALSE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneSubscriptionActive, 3, 500));

		BC_ASSERT_EQUAL(marie.getCore().getChatRooms().size(), static_cast<size_t>(nbChatRooms + nbLegacyChatRooms),
		                size_t, "%zu");

		for (auto chatRoom : marie.getCore().getChatRooms()) {
			initialMarieStats = marie.getStats();
			initialPaulineStats = pauline.getStats();
			initialMichelleStats = michelle.getStats();

			const auto &confAddr = chatRoom->getConferenceAddress();
			const auto confAddrString = confAddr->toString();
			std::string msgText = std::string("Let's see if you receive this message in chatroom ") + confAddrString;
			std::map<LinphoneCoreManager *, int> members{{pauline.getCMgr(), 3}, {michelle.getCMgr(), 3}};
			sendMessageAndCheckReception(msgCores, members, msgText, chatRoom);

			std::string newSubject = std::string("After restart: New subject of chatroom ") +
			                         chatRoom->getConferenceAddress()->toString() + std::string(" on ") +
			                         focusFactoryAddress.toString();
			ms_message("%s changes the subject of chatroom %s to '%s'", linphone_core_get_identity(marie.getLc()),
			           confAddrString.c_str(), newSubject.c_str());
			linphone_chat_room_set_subject(chatRoom->toC(), newSubject.c_str());
			BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_chat_room_subject_changed,
			                             initialMarieStats.number_of_chat_room_subject_changed + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_chat_room_subject_changed,
			                             initialPaulineStats.number_of_chat_room_subject_changed + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_chat_room_subject_changed,
			                             initialMichelleStats.number_of_chat_room_subject_changed + 1,
			                             liblinphone_tester_sip_timeout));
		}

		CoreManagerAssert({focus, marie, pauline}).waitUntil(std::chrono::seconds(2), [] { return false; });

		for (auto chatRoom : focus.getCore().getChatRooms()) {
			for (auto participant : chatRoom->getParticipants()) {
				//  force deletion by removing devices
				auto participantAddress = participant->getAddress();
				linphone_chat_room_set_participant_devices(chatRoom->toC(), participantAddress->toC(), NULL);
			}
		}

		// wait until chatroom is deleted server side
		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline}).wait([&focus] {
			return focus.getCore().getChatRooms().size() == 0;
		}));

		// wait bit more to detect side effect if any
		CoreManagerAssert({focus, marie, pauline}).waitUntil(chrono::seconds(2), [] { return false; });

		// to avoid creation attempt of a new chatroom
		LinphoneProxyConfig *config = linphone_core_get_default_proxy_config(focus.getLc());
		linphone_proxy_config_edit(config);
		linphone_proxy_config_set_conference_factory_uri(config, NULL);
		linphone_proxy_config_done(config);
		bctbx_list_free(coresList);
	}
}

void check_chat_message_after_migration(LinphoneChatMessage *msg, const std::shared_ptr<Address> &confAddr) {
	auto direction = L_GET_PRIVATE_FROM_C_OBJECT(msg)->getRawDirection();
	bool isOutgoing = (direction == ChatMessage::Direction::Outgoing);
	std::string alternativeAddressHeaderName;
	if (isOutgoing) {
		alternativeAddressHeaderName = Conference::kXAlternativeAddressClientHeaderName;
	} else {
		alternativeAddressHeaderName = Conference::kXAlternativeAddressServerHeaderName;
	}
	const char *alternative_address_header =
	    linphone_chat_message_get_custom_header(msg, alternativeAddressHeaderName.c_str());
	BC_ASSERT_PTR_NOT_NULL(alternative_address_header);
	if (alternative_address_header) {
		auto confAddrString = confAddr->toStringUriOnlyOrdered();
		BC_ASSERT_STRING_EQUAL(confAddrString.c_str(), alternative_address_header);
	}
	auto op = L_GET_PRIVATE_FROM_C_OBJECT(msg)->getSalOp();
	if (op) {
		auto requestUri = op->getRequestUri();
		BC_ASSERT_FALSE(requestUri.empty());
		const LinphoneAddress *msg_from = linphone_chat_message_get_from_address(msg);
		BC_ASSERT_PTR_NOT_NULL(msg_from);
		if (msg_from) {
			char *msg_to_string = linphone_address_as_string_uri_only(msg_from);
			BC_ASSERT_STRING_NOT_EQUAL(msg_to_string, requestUri.c_str());
			ms_free(msg_to_string);
		}
		if (alternative_address_header) {
			BC_ASSERT_STRING_EQUAL(alternative_address_header, requestUri.c_str());
		}
	}
}

void check_media_session_after_migration(LinphoneChatRoom *cr,
                                         const std::shared_ptr<Address> &assignedConferenceAddress,
                                         const std::shared_ptr<Address> &alternativeConferenceAddress) {
	// The request URI is expected to match:
	// - the conference address
	// - the value of custom header Conference::kXAlternativeAddressClientHeaderName if the call is outgoing,
	// Conference::kXAlternativeAddressServerHeaderName otherwise The INVITE remote address is expected to match:
	// - the legacy conference address
	auto cppCr = AbstractChatRoom::toCpp(cr);
	auto mainSession = cppCr->getConference()->getMainSession();
	if (mainSession) {
		auto op = mainSession->getOp();
		if (op) {
			bool isOutgoing = (mainSession->getDirection() == LinphoneCallOutgoing);
			std::string remote_address_string = isOutgoing ? op->getTo() : op->getFrom();
			BC_ASSERT_FALSE(remote_address_string.empty());
			// Create a LinphoneAddress to get rid of the angle brackets (<>) around the address through the
			// linphone_address_as_string_uri_only method
			LinphoneAddress *remote_address = linphone_address_new(remote_address_string.c_str());
			BC_ASSERT_PTR_NOT_NULL(remote_address);
			if (remote_address) {
				auto confAddr = cppCr->getConferenceAddress();
				auto confAddrString = confAddr->toStringUriOnlyOrdered();
				auto crAssignedConferenceAddress = cppCr->getAssignedConferenceAddress();
				BC_ASSERT_PTR_NOT_NULL(crAssignedConferenceAddress.get());
				std::string crAssignedConferenceAddressString;
				if (crAssignedConferenceAddress) {
					crAssignedConferenceAddressString = crAssignedConferenceAddress->asStringUriOnly();
				}
				std::string assignedConferenceAddressString;
				if (assignedConferenceAddress) {
					assignedConferenceAddressString = assignedConferenceAddress->asStringUriOnly();
				}

				auto crAlternativeConferenceAddress = cppCr->getAlternativeConferenceAddress();
				BC_ASSERT_PTR_NOT_NULL(crAlternativeConferenceAddress.get());
				std::string crAlternativeConferenceAddressString;
				if (crAlternativeConferenceAddress) {
					crAlternativeConferenceAddressString = crAlternativeConferenceAddress->asStringUriOnly();
				}
				std::string alternativeConferenceAddressString;
				if (alternativeConferenceAddress) {
					alternativeConferenceAddressString = alternativeConferenceAddress->asStringUriOnly();
				}
				char *remote_address_uri_string = linphone_address_as_string_uri_only_ordered(remote_address);
				linphone_address_unref(remote_address);
				auto requestUri = op->getRequestUri();
				BC_ASSERT_STRING_NOT_EQUAL(confAddrString.c_str(), remote_address_uri_string);
				if (isOutgoing) {
					BC_ASSERT_FALSE(requestUri.empty());
					BC_ASSERT_STRING_NOT_EQUAL(remote_address_uri_string, requestUri.c_str());
					BC_ASSERT_STRING_EQUAL(confAddrString.c_str(), requestUri.c_str());
				}
				auto customHeaders = (isOutgoing) ? op->getSentCustomHeaders() : op->getRecvCustomHeaders();
				BC_ASSERT_PTR_NOT_NULL(customHeaders);
				if (customHeaders) {
					std::string alternativeAddressHeaderName;
					if (isOutgoing) {
						alternativeAddressHeaderName = Conference::kXAlternativeAddressClientHeaderName;
					} else {
						alternativeAddressHeaderName = Conference::kXAlternativeAddressServerHeaderName;
					}
					const char *alternative_address_header =
					    sal_custom_header_find(customHeaders, alternativeAddressHeaderName.c_str());
					BC_ASSERT_PTR_NOT_NULL(alternative_address_header);
					if (alternative_address_header) {
						if (!requestUri.empty()) {
							BC_ASSERT_STRING_EQUAL(alternative_address_header, requestUri.c_str());
						}
						if (crAlternativeConferenceAddress) {
							BC_ASSERT_STRING_EQUAL(crAlternativeConferenceAddressString.c_str(),
							                       alternative_address_header);
						}

						if (crAlternativeConferenceAddress && crAssignedConferenceAddress) {
							BC_ASSERT_STRING_NOT_EQUAL(crAssignedConferenceAddressString.c_str(),
							                           alternative_address_header);
						}
					}
				}

				// The remote address of the INVITE message should be the original
				// conference address
				if (crAssignedConferenceAddress && assignedConferenceAddress) {
					BC_ASSERT_STRING_EQUAL(assignedConferenceAddressString.c_str(),
					                       crAssignedConferenceAddressString.c_str());
					if (!requestUri.empty()) {
						BC_ASSERT_STRING_NOT_EQUAL(requestUri.c_str(), crAssignedConferenceAddressString.c_str());
					}
				}
				if (crAlternativeConferenceAddress && alternativeConferenceAddress) {
					BC_ASSERT_STRING_EQUAL(alternativeConferenceAddressString.c_str(),
					                       crAlternativeConferenceAddressString.c_str());
					if (!requestUri.empty()) {
						BC_ASSERT_STRING_EQUAL(requestUri.c_str(), crAlternativeConferenceAddressString.c_str());
					}
				}
				if (crAlternativeConferenceAddress && crAssignedConferenceAddress) {
					BC_ASSERT_STRING_NOT_EQUAL(crAssignedConferenceAddressString.c_str(),
					                           crAlternativeConferenceAddressString.c_str());
				}
				ms_free(remote_address_uri_string);
			}
		}
	}
}

void linphone_subscribe_received_body_check(LinphoneCore *lc,
                                            BCTBX_UNUSED(LinphoneEvent *lev),
                                            BCTBX_UNUSED(const char *eventname),
                                            const LinphoneContent *content) {
	if (content) {
		if (strcmp(linphone_content_get_subtype(content), "resource-lists+xml") == 0) {
			// Count how many times the conf-id parameter is found in the body of the SUBSCRIBE message to make sure the
			// client migrated all its chatrooms to the new conference address
			size_t conf_id_count = 0;
			const char *text = linphone_content_get_utf8_text(content);
			if (text) {
				const char *tmp = text;
				while ((tmp = strstr(tmp, Conference::kConfIdParameter.c_str()))) {
					conf_id_count++;
					tmp++;
				}
			}
			const bctbx_list_t *rooms = linphone_core_get_chat_rooms(lc);
			BC_ASSERT_EQUAL(conf_id_count, bctbx_list_size(rooms), size_t, "%zu");
		}
	}
}

void sendMesageAndCheckHistory(std::initializer_list<std::reference_wrapper<CoreManager>> coreMgrs,
                               std::initializer_list<std::reference_wrapper<ConfCoreManager>> participants,
                               std::shared_ptr<AbstractChatRoom> &chatRoom,
                               const std::string msgText,
                               std::map<LinphoneCoreManager *, int> historySizeMap,
                               std::set<std::shared_ptr<Address>> migratedAddresses) {
	LinphoneChatMessage *msg = ClientConference::sendTextMsg(chatRoom->toC(), msgText);
	BC_ASSERT_TRUE(CoreManagerAssert(coreMgrs).wait(
	    [msg] { return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered); }));
	auto hasMigrated = (migratedAddresses.find(chatRoom->getAssignedConferenceAddress()) != migratedAddresses.end());
	auto confAddr = chatRoom->getConferenceAddress();
	if (hasMigrated) {
		check_chat_message_after_migration(msg, confAddr);
	}
	linphone_chat_message_unref(msg);
	for (ConfCoreManager &recvCore : participants) {
		auto cr = recvCore.searchChatRoom(nullptr, confAddr->toC());
		BC_ASSERT_PTR_NOT_NULL(cr);
		if (cr) {
			auto actual_history_size = historySizeMap[recvCore.getCMgr()];
			BC_ASSERT_TRUE(CoreManagerAssert(coreMgrs).wait([&cr, &actual_history_size] {
				return linphone_chat_room_get_history_size(cr) == actual_history_size;
			}));
			if (recvCore.getLc() != chatRoom->getCore()->getCCore()) {
				LinphoneChatMessage *lastMsg = recvCore.getStats().last_received_chat_message;
				BC_ASSERT_PTR_NOT_NULL(lastMsg);
				if (lastMsg) {
					BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_utf8_text(lastMsg), msgText.c_str());
					if (hasMigrated) {
						check_chat_message_after_migration(lastMsg, confAddr);
					}
				}
			}
		}
	}
}

// Test the following:
// - migration of a chatroom with 3 members
// - addition of a participant after migration
// - send messages after migration
// - restart cores and resubscribe to chatrooms
// - change subject after migration (INVITE session establiment if done after core restart)
// - designate another admin (REFER message testing)
void legacy_chat_room_migration_base(ChatRoomMigrationParams const &params) {
	bool encrypted = params.encrypted;
	bool unification_at_startup = params.unification_at_startup;
	ChatRoomMigrationMethod method = params.migration_method;

	Focus focus("chloe_rc");
	{ // to make sure focus is destroyed after clients.

		const LinphoneTesterLimeAlgo lime_algo = encrypted ? C25519 : UNSET;
		linphone_core_enable_lime_x3dh(focus.getLc(), !!encrypted);

		LinphoneAccount *domain_registration_account = add_account_using_domain_registration(focus, true);
		BC_ASSERT_PTR_NOT_NULL(domain_registration_account);
		if (domain_registration_account) {
			linphone_account_ref(domain_registration_account);
		}

		ClientConference marie("marie_domain_registration_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference pauline("pauline_domain_registration_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference michelle("michelle_domain_registration_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference berthe("berthe_domain_registration_rc", focus.getConferenceFactoryAddress(), lime_algo);

		focus.registerAsParticipantDevice(marie);
		focus.registerAsParticipantDevice(pauline);
		focus.registerAsParticipantDevice(michelle);
		focus.registerAsParticipantDevice(berthe);

		if (encrypted) {
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(marie.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(pauline.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(michelle.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(berthe.getLc()));
		}

		int subscribe_expires_s = 15;
		linphone_config_set_int(linphone_core_get_config(marie.getLc()), "sip", "conference_subscribe_expires",
		                        subscribe_expires_s);
		linphone_config_set_int(linphone_core_get_config(pauline.getLc()), "sip", "conference_subscribe_expires",
		                        subscribe_expires_s);
		linphone_config_set_int(linphone_core_get_config(berthe.getLc()), "sip", "conference_subscribe_expires",
		                        subscribe_expires_s);
		linphone_config_set_int(linphone_core_get_config(michelle.getLc()), "sip", "conference_subscribe_expires",
		                        subscribe_expires_s);

		bctbx_list_t *coresList = bctbx_list_append(NULL, focus.getLc());
		coresList = bctbx_list_append(coresList, marie.getLc());
		coresList = bctbx_list_append(coresList, pauline.getLc());
		coresList = bctbx_list_append(coresList, michelle.getLc());
		coresList = bctbx_list_append(coresList, berthe.getLc());

		stats initialMarieStats = marie.getStats();
		stats initialPaulineStats = pauline.getStats();
		stats initialMichelleStats = michelle.getStats();

		Address paulineAddr = pauline.getIdentity();
		Address michelleAddr = michelle.getIdentity();

		LinphoneCoreCbs *cbs = linphone_factory_create_core_cbs(linphone_factory_get());
		linphone_core_cbs_set_chat_room_state_changed(cbs, legacy_server_core_chat_room_state_changed);
		linphone_core_cbs_set_subscribe_received(cbs, linphone_subscribe_received_body_check);
		_linphone_core_add_callbacks(focus.getLc(), cbs, TRUE);
		linphone_core_cbs_unref(cbs);

		int nbLegacyChatRooms = 3;
		const std::initializer_list<std::reference_wrapper<CoreManager>> coreMgrs{marie, pauline, focus, berthe,
		                                                                          michelle};
		const std::initializer_list<std::reference_wrapper<ClientConference>> participants{pauline, michelle};
		createChatRooms(nbLegacyChatRooms, coreMgrs, participants, focus, marie.getCMgr(), std::string("Legacy"),
		                encrypted, true, true);

		BC_ASSERT_EQUAL(marie.getCore().getChatRooms().size(), static_cast<size_t>(nbLegacyChatRooms), size_t, "%zu");

		std::list<std::shared_ptr<Address>> addresses;
		for (auto chatRoom : marie.getCore().getChatRooms()) {
			const auto &address = chatRoom->getConferenceAddress();
			BC_ASSERT_PTR_NOT_NULL(address);
			const auto &alternativeAddress = chatRoom->getAlternativeConferenceAddress();
			BC_ASSERT_PTR_NULL(alternativeAddress);
			addresses.push_back(address);
		}

		std::set<std::shared_ptr<Address>> migratedAddresses;

		stats initialFocusStats = focus.getStats();
		initialMarieStats = marie.getStats();
		initialPaulineStats = pauline.getStats();
		initialMichelleStats = michelle.getStats();
		// Start chatroom migration
		if (unification_at_startup) {
			coresList = bctbx_list_remove(coresList, focus.getLc());
			ms_message("%s reinitializes its core to unify the chat room addresses",
			           linphone_core_get_identity(focus.getLc()));
			linphone_core_manager_reinit(focus.getCMgr());

			if (method == ChatRoomMigrationMethod::API) {
				linphone_core_enable_chat_room_address_unification(focus.getLc(), TRUE);
				for (const auto &address : addresses) {
					migratedAddresses.insert(address);
				}
			} else {
#ifdef HAVE_SOCI
				try {
					soci::session sql("sqlite3", focus.getCMgr()->database_path); // open the DB
					if (const auto &address = addresses.front(); address) {
						std::string addressString = address->asStringUriOnly();
						ms_message("%s is manually migrating chatroom %s", linphone_core_get_identity(focus.getLc()),
						           addressString.c_str());
						sql << "UPDATE chat_room SET to_migrate = 1 FROM sip_address WHERE "
						       "chat_room.peer_sip_address_id = sip_address.id AND sip_address.value = \""
						    << addressString << "\"";
						migratedAddresses.insert(address);
					}
				} catch (std::exception &e) { // swallow any error on DB
					lWarning() << "Cannot manually trigger chat room migration in database "
					           << focus.getCMgr()->database_path << ". Error is " << e.what();
				}
				BC_ASSERT_FALSE(linphone_core_chat_room_address_unification_enabled(focus.getLc()));
#endif // HAVE_SOCI
			}
			if (domain_registration_account) {
				linphone_core_add_account(focus.getLc(), domain_registration_account);
				linphone_core_set_default_account(focus.getLc(), domain_registration_account);
			}

			ms_message("%s configures and starts again its core", linphone_core_get_identity(focus.getLc()));
			focus.configureFocus();
			linphone_core_enable_lime_x3dh(focus.getLc(), encrypted);
			if (method == ChatRoomMigrationMethod::API) {
				BC_ASSERT_TRUE(linphone_core_chat_room_address_unification_enabled(focus.getLc()));
			} else {
				BC_ASSERT_FALSE(linphone_core_chat_room_address_unification_enabled(focus.getLc()));
			}
			linphone_core_manager_start(focus.getCMgr(), TRUE);
			coresList = bctbx_list_append(coresList, focus.getLc());

			// The chat room address unification flag is cleared after the action takes place
			BC_ASSERT_FALSE(linphone_core_chat_room_address_unification_enabled(focus.getLc()));

			// Wait for all subscription transaction to expire
			BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneSubscriptionActive,
			                             3 * nbLegacyChatRooms, 80000));
			BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_LinphoneSubscriptionActive,
			                             initialMarieStats.number_of_LinphoneSubscriptionActive + nbLegacyChatRooms,
			                             (subscribe_expires_s * 1000)));
			BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneSubscriptionActive,
			                             initialMichelleStats.number_of_LinphoneSubscriptionActive + nbLegacyChatRooms,
			                             (subscribe_expires_s * 1000)));
			BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneSubscriptionActive,
			                             initialPaulineStats.number_of_LinphoneSubscriptionActive + nbLegacyChatRooms,
			                             (subscribe_expires_s * 1000)));
			BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_NotifyFullStateReceived,
			                             initialMarieStats.number_of_NotifyFullStateReceived + nbLegacyChatRooms,
			                             (subscribe_expires_s * 1000)));
			BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_NotifyFullStateReceived,
			                             initialMichelleStats.number_of_NotifyFullStateReceived + nbLegacyChatRooms,
			                             (subscribe_expires_s * 1000)));
			BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_NotifyFullStateReceived,
			                             initialPaulineStats.number_of_NotifyFullStateReceived + nbLegacyChatRooms,
			                             (subscribe_expires_s * 1000)));

		} else {
			BC_ASSERT_TRUE(linphone_core_unify_chat_rooms_address(focus.getLc()));
			// The chat room address unification flag is cleared after the action takes place
			BC_ASSERT_FALSE(linphone_core_chat_room_address_unification_enabled(focus.getLc()));

			BC_ASSERT_TRUE(
			    wait_for_list(coresList, &focus.getStats().number_of_LinphoneChatRoomAlternativeAddressChanged,
			                  initialFocusStats.number_of_LinphoneChatRoomAlternativeAddressChanged + nbLegacyChatRooms,
			                  liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(
			    wait_for_list(coresList, &marie.getStats().number_of_LinphoneChatRoomAlternativeAddressChanged,
			                  initialMarieStats.number_of_LinphoneChatRoomAlternativeAddressChanged + nbLegacyChatRooms,
			                  liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(
			    coresList, &pauline.getStats().number_of_LinphoneChatRoomAlternativeAddressChanged,
			    initialPaulineStats.number_of_LinphoneChatRoomAlternativeAddressChanged + nbLegacyChatRooms,
			    liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(
			    coresList, &michelle.getStats().number_of_LinphoneChatRoomAlternativeAddressChanged,
			    initialMichelleStats.number_of_LinphoneChatRoomAlternativeAddressChanged + nbLegacyChatRooms,
			    liblinphone_tester_sip_timeout));

			for (const auto &address : addresses) {
				migratedAddresses.insert(address);
			}
		}

		BC_ASSERT_EQUAL(marie.getCore().getChatRooms().size(), nbLegacyChatRooms, size_t, "%zu");
		auto addressIt = addresses.cbegin();
		for (auto chatRoom : marie.getCore().getChatRooms()) {
			const auto assignedAddress = *addressIt;
			BC_ASSERT_PTR_NOT_NULL(assignedAddress);
			bool hasMigrated = (migratedAddresses.find(assignedAddress) != migratedAddresses.end());
			const auto &address = chatRoom->getConferenceAddress();
			BC_ASSERT_PTR_NOT_NULL(address);
			const auto &alternativeAddress = chatRoom->getAlternativeConferenceAddress();
			if (hasMigrated) {
				BC_ASSERT_PTR_NOT_NULL(alternativeAddress);
				if (alternativeAddress) {
					BC_ASSERT_TRUE(alternativeAddress->hasUriParam(Conference::kConfIdParameter));
				}
			} else {
				BC_ASSERT_TRUE(address->toStringUriOnlyOrdered() == assignedAddress->toStringUriOnlyOrdered());
				BC_ASSERT_PTR_NULL(alternativeAddress);
			}
			// ChatRoom::getConferenceAddress must return the migrated address
			if (address && alternativeAddress) {
				BC_ASSERT_TRUE(address->toStringUriOnlyOrdered() == alternativeAddress->toStringUriOnlyOrdered());
			}
			if (assignedAddress && alternativeAddress) {
				BC_ASSERT_FALSE(assignedAddress->weakEqual(*alternativeAddress));
				BC_ASSERT_FALSE(assignedAddress->toStringUriOnlyOrdered() ==
				                alternativeAddress->toStringUriOnlyOrdered());
			}
			addressIt++;
		}

		// Add berthe
		for (auto chatRoom : marie.getCore().getChatRooms()) {
			initialMarieStats = marie.getStats();
			initialMichelleStats = michelle.getStats();
			initialPaulineStats = pauline.getStats();
			stats initialBertheStats = berthe.getStats();

			auto conferenceAddress = chatRoom->getConferenceAddress();
			Address bertheAddr = berthe.getIdentity();
			bctbx_list_t *participantsAddresses = bctbx_list_append(NULL, linphone_address_ref(bertheAddr.toC()));
			ms_message("%s is adding participant %s to chatroom %s", linphone_core_get_identity(marie.getLc()),
			           linphone_core_get_identity(berthe.getLc()), conferenceAddress->toString().c_str());
			linphone_chat_room_add_participants(chatRoom->toC(), participantsAddresses);
			bctbx_list_free_with_data(participantsAddresses, (bctbx_list_free_func)linphone_address_unref);

			LinphoneChatRoom *bertheCr = check_creation_chat_room_client_side(
			    coresList, berthe.getCMgr(), &initialBertheStats, conferenceAddress->toC(),
			    chatRoom->getSubjectUtf8().c_str(), 3, FALSE);
			BC_ASSERT_PTR_NOT_NULL(bertheCr);

			BC_ASSERT_TRUE(wait_for_list(coresList, &berthe.getStats().number_of_LinphoneConferenceStateCreationPending,
			                             initialBertheStats.number_of_LinphoneConferenceStateCreationPending + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(coresList, &berthe.getStats().number_of_LinphoneConferenceStateCreated,
			                             initialBertheStats.number_of_LinphoneConferenceStateCreated + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(coresList, &berthe.getStats().number_of_LinphoneChatRoomConferenceJoined,
			                             initialBertheStats.number_of_LinphoneChatRoomConferenceJoined + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_chat_room_participants_added,
			                             initialMarieStats.number_of_chat_room_participants_added + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_chat_room_participants_added,
			                             initialBertheStats.number_of_chat_room_participants_added + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_chat_room_participants_added,
			                             initialMichelleStats.number_of_chat_room_participants_added + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_chat_room_participant_devices_added,
			                             initialMarieStats.number_of_chat_room_participant_devices_added + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_chat_room_participant_devices_added,
			                             initialBertheStats.number_of_chat_room_participant_devices_added + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_chat_room_participant_devices_added,
			                             initialMichelleStats.number_of_chat_room_participant_devices_added + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(chatRoom->toC()), 3, int, "%d");

			if (migratedAddresses.find(chatRoom->getAssignedConferenceAddress()) != migratedAddresses.end()) {
				check_media_session_after_migration(bertheCr, chatRoom->getAssignedConferenceAddress(),
				                                    chatRoom->getAlternativeConferenceAddress());
			}
		}

		std::initializer_list<std::reference_wrapper<ConfCoreManager>> cores{focus, marie, michelle, pauline, berthe};
		for (const ConfCoreManager &core : cores) {
			BC_ASSERT_TRUE(
			    CoreManagerAssert({focus, marie, michelle, berthe, pauline})
			        .waitUntil(chrono::seconds(10), [&focus, &core] { return checkChatroom(focus, core, -1); }));
			for (auto chatRoom : core.getCore().getChatRooms()) {
				BC_ASSERT_EQUAL(chatRoom->getParticipants().size(), ((focus.getLc() == core.getLc())) ? 4 : 3, size_t,
				                "%zu");
			}
		};

		// Send message
		std::initializer_list<std::reference_wrapper<ConfCoreManager>> members{marie, michelle, pauline, berthe};
		int expected_history_size = 1;
		for (const ConfCoreManager &sendCore : members) {
			expected_history_size++;
			std::map<LinphoneCoreManager *, int> historySizeMap;
			for (const ConfCoreManager &core : members) {
				auto actual_history_size = expected_history_size;
				if (core.getCMgr() == berthe.getCMgr()) {
					actual_history_size--;
				}
				historySizeMap.insert(std::make_pair(core.getCMgr(), actual_history_size));
			}
			for (auto chatRoom : sendCore.getCore().getChatRooms()) {
				auto confAddr = chatRoom->getConferenceAddress();
				auto confAddrString = confAddr->toStringUriOnlyOrdered();
				std::string msgText = std::string("Migrated: message in chatroom ") + confAddrString.c_str() +
				                      std::string(" subject ") + chatRoom->getSubjectUtf8().c_str() +
				                      std::string(" from ") + sendCore.getIdentity().toString();
				sendMesageAndCheckHistory(coreMgrs, members, chatRoom, msgText, historySizeMap, migratedAddresses);
			}
		}

		int maxIterations = 3;
		// Restart the core multiple time to verify that no database corruption occurs
		for (int idx = 0; idx < maxIterations; idx++) {
			stats overallInitialFocusStats = focus.getStats();
			for (ConfCoreManager &core : cores) {
				if (core.getCMgr() == focus.getCMgr()) {
					continue;
				}

				stats initialFocusStats = focus.getStats();
				coresList = bctbx_list_remove(coresList, core.getLc());
				// Restart Marie
				ms_message("[Iteration %0d]: %s is restarting its core", idx, linphone_core_get_identity(core.getLc()));
				core.reStart();
				coresList = bctbx_list_append(coresList, core.getLc());

				BC_ASSERT_TRUE(wait_for_list(coresList, &core.getStats().number_of_LinphoneSubscriptionActive, 1,
				                             liblinphone_tester_sip_timeout));
				BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneSubscriptionActive,
				                             initialFocusStats.number_of_LinphoneSubscriptionActive + 1,
				                             liblinphone_tester_sip_timeout));
				BC_ASSERT_EQUAL(core.getCore().getChatRooms().size(), static_cast<size_t>(nbLegacyChatRooms), size_t,
				                "%zu");

				if (core.getCMgr() == marie.getCMgr()) {
					for (auto chatRoom : core.getCore().getChatRooms()) {
						initialMarieStats = marie.getStats();
						initialPaulineStats = pauline.getStats();
						initialMichelleStats = michelle.getStats();
						stats initialBertheStats = berthe.getStats();
						auto confAddr = chatRoom->getConferenceAddress();
						auto confAddrString = confAddr->toStringUriOnlyOrdered();

						// Change subject
						std::string subject = std::string("[Iteration ") + std::to_string(idx) +
						                      std::string("]: Subject of chatroom ") + confAddrString +
						                      std::string(" after migration");
						ms_message("[Iteration %0d]: %s changes the subject of chatroom %s to '%s'", idx,
						           linphone_core_get_identity(marie.getLc()), confAddrString.c_str(), subject.c_str());
						linphone_chat_room_set_subject(chatRoom->toC(), subject.c_str());
						for (ConfCoreManager &subjectChangeCore : cores) {
							auto cr = subjectChangeCore.searchChatRoom(nullptr, confAddr->toC());
							BC_ASSERT_PTR_NOT_NULL(cr);
							if (cr) {
								BC_ASSERT_TRUE(
								    CoreManagerAssert({focus, marie, pauline, michelle, berthe}).wait([&cr, &subject] {
									    return strcmp(linphone_chat_room_get_subject_utf8(cr), subject.c_str()) == 0;
								    }));
							}
						}
						BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_chat_room_subject_changed,
						                             initialMarieStats.number_of_chat_room_subject_changed + 1,
						                             liblinphone_tester_sip_timeout));
						BC_ASSERT_TRUE(wait_for_list(coresList,
						                             &michelle.getStats().number_of_chat_room_subject_changed,
						                             initialMichelleStats.number_of_chat_room_subject_changed + 1,
						                             liblinphone_tester_sip_timeout));
						BC_ASSERT_TRUE(wait_for_list(coresList, &berthe.getStats().number_of_chat_room_subject_changed,
						                             initialBertheStats.number_of_chat_room_subject_changed + 1,
						                             liblinphone_tester_sip_timeout));
						BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_chat_room_subject_changed,
						                             initialPaulineStats.number_of_chat_room_subject_changed + 1,
						                             liblinphone_tester_sip_timeout));

						auto assignedConferenceAddress = chatRoom->getAssignedConferenceAddress();
						BC_ASSERT_PTR_NOT_NULL(assignedConferenceAddress.get());

						auto hasMigrated = (migratedAddresses.find(chatRoom->getAssignedConferenceAddress()) !=
						                    migratedAddresses.end());
						if (hasMigrated) {
							check_media_session_after_migration(chatRoom->toC(), assignedConferenceAddress,
							                                    chatRoom->getAlternativeConferenceAddress());
						}

						LinphoneChatRoom *focus_chat_room = focus.searchChatRoom(confAddr->toC(), confAddr->toC());
						BC_ASSERT_PTR_NOT_NULL(focus_chat_room);
						LinphoneChatRoom *focus_chat_room_original_address =
						    focus.searchChatRoom(assignedConferenceAddress->toC(), assignedConferenceAddress->toC());
						BC_ASSERT_PTR_NOT_NULL(focus_chat_room_original_address);
						BC_ASSERT_PTR_EQUAL(focus_chat_room, focus_chat_room_original_address);
						if (focus_chat_room) {
							auto marie_participant =
							    linphone_chat_room_find_participant(focus_chat_room, marie.getCMgr()->identity);
							BC_ASSERT_PTR_NOT_NULL(marie_participant);
							if (marie_participant) {
								BC_ASSERT_TRUE(linphone_participant_is_admin(marie_participant));
							}
						}

						// Give admin rights to Berthe
						auto berthe_participant =
						    linphone_chat_room_find_participant(chatRoom->toC(), berthe.getCMgr()->identity);
						BC_ASSERT_PTR_NOT_NULL(berthe_participant);
						if (berthe_participant) {
							initialMarieStats = marie.getStats();
							initialPaulineStats = pauline.getStats();
							initialMichelleStats = michelle.getStats();
							initialBertheStats = berthe.getStats();
							bool_t make_admin = ((idx % 2) == 0);
							ms_message("[Iteration %0d]: %s %s admin rights to %s in chatroom %s", idx,
							           linphone_core_get_identity(marie.getLc()), (make_admin) ? "gives" : "removes",
							           linphone_core_get_identity(berthe.getLc()), confAddrString.c_str());
							linphone_chat_room_set_participant_admin_status(chatRoom->toC(), berthe_participant,
							                                                make_admin);
							BC_ASSERT_TRUE(wait_for_list(
							    coresList, &pauline.getStats().number_of_chat_room_participant_admin_statuses_changed,
							    initialPaulineStats.number_of_chat_room_participant_admin_statuses_changed + 1,
							    liblinphone_tester_sip_timeout));
							BC_ASSERT_TRUE(wait_for_list(
							    coresList, &michelle.getStats().number_of_chat_room_participant_admin_statuses_changed,
							    initialMichelleStats.number_of_chat_room_participant_admin_statuses_changed + 1,
							    liblinphone_tester_sip_timeout));
							BC_ASSERT_TRUE(wait_for_list(
							    coresList, &marie.getStats().number_of_chat_room_participant_admin_statuses_changed,
							    initialMarieStats.number_of_chat_room_participant_admin_statuses_changed + 1,
							    liblinphone_tester_sip_timeout));
							BC_ASSERT_TRUE(wait_for_list(
							    coresList, &berthe.getStats().number_of_chat_room_participant_admin_statuses_changed,
							    initialBertheStats.number_of_chat_room_participant_admin_statuses_changed + 1,
							    liblinphone_tester_sip_timeout));

							if (make_admin) {
								initialMarieStats = marie.getStats();
								initialPaulineStats = pauline.getStats();
								initialMichelleStats = michelle.getStats();
								initialBertheStats = berthe.getStats();
								// Change subject again
								std::string newSubject = std::string("[Iteration ") + std::to_string(idx) +
								                         std::string("]: My turn to change subject of chatroom ") +
								                         confAddrString;
								ms_message("[Iteration %0d]: %s changes the subject of chatroom %s to '%s'", idx,
								           linphone_core_get_identity(berthe.getLc()), confAddrString.c_str(),
								           newSubject.c_str());
								LinphoneChatRoom *bertheCr = berthe.searchChatRoom(nullptr, confAddr->toC());
								BC_ASSERT_PTR_NOT_NULL(bertheCr);
								if (bertheCr) {
									linphone_chat_room_set_subject(bertheCr, newSubject.c_str());
									for (ConfCoreManager &subjectChangeCore : cores) {
										auto cr = subjectChangeCore.searchChatRoom(nullptr, confAddr->toC());
										BC_ASSERT_PTR_NOT_NULL(cr);
										if (cr) {
											BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, berthe})
											                   .wait([&cr, &newSubject] {
												                   return strcmp(
												                              linphone_chat_room_get_subject_utf8(cr),
												                              newSubject.c_str()) == 0;
											                   }));
										}
									}
									BC_ASSERT_TRUE(
									    wait_for_list(coresList, &marie.getStats().number_of_chat_room_subject_changed,
									                  initialMarieStats.number_of_chat_room_subject_changed + 1,
									                  liblinphone_tester_sip_timeout));
									BC_ASSERT_TRUE(wait_for_list(
									    coresList, &michelle.getStats().number_of_chat_room_subject_changed,
									    initialMichelleStats.number_of_chat_room_subject_changed + 1,
									    liblinphone_tester_sip_timeout));
									BC_ASSERT_TRUE(
									    wait_for_list(coresList, &berthe.getStats().number_of_chat_room_subject_changed,
									                  initialBertheStats.number_of_chat_room_subject_changed + 1,
									                  liblinphone_tester_sip_timeout));
									BC_ASSERT_TRUE(wait_for_list(
									    coresList, &pauline.getStats().number_of_chat_room_subject_changed,
									    initialPaulineStats.number_of_chat_room_subject_changed + 1,
									    liblinphone_tester_sip_timeout));

									if (hasMigrated) {
										check_media_session_after_migration(
										    bertheCr, assignedConferenceAddress,
										    chatRoom->getAlternativeConferenceAddress());
									}
								}
							}
						}
					}
				}
			}

			for (const ConfCoreManager &core : cores) {
				if (core.getCMgr() == focus.getCMgr()) {
					continue;
				}
				BC_ASSERT_EQUAL(core.getStats().number_of_LinphoneSubscriptionActive, 1, int, "%0d");
				BC_ASSERT_EQUAL(focus.getStats().number_of_LinphoneSubscriptionActive,
				                overallInitialFocusStats.number_of_LinphoneSubscriptionActive +
				                    static_cast<int>(cores.size() - 1),
				                int, "%0d");
			}
		}

		for (auto chatRoom : marie.getCore().getChatRooms()) {
			// Give admin rights to Pauline
			auto pauline_participant =
			    linphone_chat_room_find_participant(chatRoom->toC(), pauline.getCMgr()->identity);
			BC_ASSERT_PTR_NOT_NULL(pauline_participant);
			if (pauline_participant) {
				initialMarieStats = marie.getStats();
				initialPaulineStats = pauline.getStats();
				initialMichelleStats = michelle.getStats();
				stats initialBertheStats = berthe.getStats();
				auto confAddr = chatRoom->getConferenceAddress();
				auto confAddrString = confAddr->toStringUriOnlyOrdered();
				ms_message("%s gives admin rights to %s in chatroom %s after all cores restarted %0d times",
				           linphone_core_get_identity(marie.getLc()), linphone_core_get_identity(pauline.getLc()),
				           confAddrString.c_str(), maxIterations);
				linphone_chat_room_set_participant_admin_status(chatRoom->toC(), pauline_participant, TRUE);
				BC_ASSERT_TRUE(
				    wait_for_list(coresList, &pauline.getStats().number_of_chat_room_participant_admin_statuses_changed,
				                  initialPaulineStats.number_of_chat_room_participant_admin_statuses_changed + 1,
				                  liblinphone_tester_sip_timeout));
				BC_ASSERT_TRUE(wait_for_list(
				    coresList, &michelle.getStats().number_of_chat_room_participant_admin_statuses_changed,
				    initialMichelleStats.number_of_chat_room_participant_admin_statuses_changed + 1,
				    liblinphone_tester_sip_timeout));
				BC_ASSERT_TRUE(
				    wait_for_list(coresList, &marie.getStats().number_of_chat_room_participant_admin_statuses_changed,
				                  initialMarieStats.number_of_chat_room_participant_admin_statuses_changed + 1,
				                  liblinphone_tester_sip_timeout));
				BC_ASSERT_TRUE(
				    wait_for_list(coresList, &berthe.getStats().number_of_chat_room_participant_admin_statuses_changed,
				                  initialBertheStats.number_of_chat_room_participant_admin_statuses_changed + 1,
				                  liblinphone_tester_sip_timeout));

				initialMarieStats = marie.getStats();
				initialPaulineStats = pauline.getStats();
				initialMichelleStats = michelle.getStats();
				initialBertheStats = berthe.getStats();
				// Change subject again
				std::string newSubject =
				    std::string("Got admin rights: changing subject of chatroom ") + confAddrString;
				ms_message("%s changes the subject of chatroom %s to '%s'", linphone_core_get_identity(pauline.getLc()),
				           confAddrString.c_str(), newSubject.c_str());
				LinphoneChatRoom *paulineCr = pauline.searchChatRoom(nullptr, confAddr->toC());
				BC_ASSERT_PTR_NOT_NULL(paulineCr);
				if (paulineCr) {
					linphone_chat_room_set_subject(paulineCr, newSubject.c_str());
					for (ConfCoreManager &subjectChangeCore : cores) {
						auto cr = subjectChangeCore.searchChatRoom(nullptr, confAddr->toC());
						BC_ASSERT_TRUE(
						    CoreManagerAssert({focus, marie, pauline, michelle, berthe}).wait([&cr, &newSubject] {
							    return strcmp(linphone_chat_room_get_subject_utf8(cr), newSubject.c_str()) == 0;
						    }));
					}
					BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_chat_room_subject_changed,
					                             initialMarieStats.number_of_chat_room_subject_changed + 1,
					                             liblinphone_tester_sip_timeout));
					BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_chat_room_subject_changed,
					                             initialMichelleStats.number_of_chat_room_subject_changed + 1,
					                             liblinphone_tester_sip_timeout));
					BC_ASSERT_TRUE(wait_for_list(coresList, &berthe.getStats().number_of_chat_room_subject_changed,
					                             initialBertheStats.number_of_chat_room_subject_changed + 1,
					                             liblinphone_tester_sip_timeout));
					BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_chat_room_subject_changed,
					                             initialPaulineStats.number_of_chat_room_subject_changed + 1,
					                             liblinphone_tester_sip_timeout));

					if (migratedAddresses.find(ChatRoom::toCpp(paulineCr)->getAssignedConferenceAddress()) !=
					    migratedAddresses.end()) {
						check_media_session_after_migration(paulineCr, chatRoom->getAssignedConferenceAddress(),
						                                    chatRoom->getAlternativeConferenceAddress());
					}
				}
			}
		}

		// Send message
		for (const ConfCoreManager &sendCore : members) {
			expected_history_size++;
			std::map<LinphoneCoreManager *, int> historySizeMap;
			for (const ConfCoreManager &core : members) {
				auto actual_history_size = expected_history_size;
				if (core.getCMgr() == berthe.getCMgr()) {
					actual_history_size--;
				}
				historySizeMap.insert(std::make_pair(core.getCMgr(), actual_history_size));
			}
			for (auto chatRoom : sendCore.getCore().getChatRooms()) {
				auto confAddr = chatRoom->getConferenceAddress();
				auto confAddrString = confAddr->toStringUriOnlyOrdered();
				std::string msgText = std::string("After restart: message in chatroom ") + confAddrString +
				                      std::string(" subject ") + chatRoom->getSubjectUtf8() + std::string(" from ") +
				                      sendCore.getIdentity().toString();
				sendMesageAndCheckHistory(coreMgrs, members, chatRoom, msgText, historySizeMap, migratedAddresses);
			}
		}

		// wait a bit longer to detect side effect if any
		CoreManagerAssert({focus, marie, michelle, berthe, pauline}).waitUntil(chrono::seconds(1), [] {
			return false;
		});

		for (auto chatRoom : focus.getCore().getChatRooms()) {
			for (auto participant : chatRoom->getParticipants()) {
				//  force deletion by removing devices
				std::shared_ptr<Address> participantAddress = participant->getAddress();
				linphone_chat_room_set_participant_devices(chatRoom->toC(), participantAddress->toC(), NULL);
			}
		}

		// wait until chatroom is deleted server side
		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, berthe, michelle, pauline}).wait([&focus] {
			return focus.getCore().getChatRooms().size() == 0;
		}));

		// wait a bit longer to detect side effect if any
		CoreManagerAssert({focus, marie, berthe, michelle, pauline}).waitUntil(chrono::seconds(2), [] {
			return false;
		});

		// to avoid creation attempt of a new chatroom
		LinphoneProxyConfig *config = linphone_core_get_default_proxy_config(focus.getLc());
		linphone_proxy_config_edit(config);
		linphone_proxy_config_set_conference_factory_uri(config, NULL);
		linphone_proxy_config_done(config);

		if (domain_registration_account) {
			linphone_account_unref(domain_registration_account);
		}

		bctbx_list_free(coresList);
	}
}

// Test that behaviour of a migration occurring when a core is offline. Upon startup, it will subscribe to the chatroom
// with the orginal address only to get a notification that the address changed. It will still be able to receive and
// associate messages to the right chatroom even when they are sent after migration because the From address is the
// original address and the new address is advertised only in the X-Alt-From header
void legacy_chat_room_migration_client_offline_base(ChatRoomMigrationClientOfflineParams const &params) {

	bool encrypted = params.encrypted;
	bool server_restart = params.server_restart;

	Focus focus("chloe_rc");
	{ // to make sure focus is destroyed after clients.

		const LinphoneTesterLimeAlgo lime_algo = encrypted ? C25519 : UNSET;
		linphone_core_enable_lime_x3dh(focus.getLc(), !!encrypted);

		LinphoneAccount *domain_registration_account = add_account_using_domain_registration(focus, true);
		BC_ASSERT_PTR_NOT_NULL(domain_registration_account);
		if (domain_registration_account) {
			linphone_account_ref(domain_registration_account);
		}

		ClientConference marie("marie_domain_registration_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference pauline("pauline_domain_registration_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference michelle("michelle_domain_registration_rc", focus.getConferenceFactoryAddress(), lime_algo);
		ClientConference berthe("berthe_domain_registration_rc", focus.getConferenceFactoryAddress(), lime_algo);

		focus.registerAsParticipantDevice(marie);
		focus.registerAsParticipantDevice(pauline);
		focus.registerAsParticipantDevice(michelle);
		focus.registerAsParticipantDevice(berthe);

		if (encrypted) {
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(marie.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(pauline.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(michelle.getLc()));
			BC_ASSERT_TRUE(linphone_core_lime_x3dh_enabled(berthe.getLc()));
		}

		bctbx_list_t *coresList = bctbx_list_append(NULL, focus.getLc());
		coresList = bctbx_list_append(coresList, marie.getLc());
		coresList = bctbx_list_append(coresList, pauline.getLc());
		coresList = bctbx_list_append(coresList, michelle.getLc());
		coresList = bctbx_list_append(coresList, berthe.getLc());

		stats initialMarieStats = marie.getStats();
		stats initialPaulineStats = pauline.getStats();
		stats initialMichelleStats = michelle.getStats();
		stats initialBertheStats = berthe.getStats();

		Address paulineAddr = pauline.getIdentity();
		Address michelleAddr = michelle.getIdentity();
		Address bertheAddr = berthe.getIdentity();

		LinphoneCoreCbs *cbs = linphone_factory_create_core_cbs(linphone_factory_get());
		linphone_core_cbs_set_chat_room_state_changed(cbs, legacy_server_core_chat_room_state_changed);
		linphone_core_cbs_set_subscribe_received(cbs, linphone_subscribe_received_body_check);
		_linphone_core_add_callbacks(focus.getLc(), cbs, TRUE);
		linphone_core_cbs_unref(cbs);

		int nbLegacyChatRooms = 3;
		const std::initializer_list<std::reference_wrapper<CoreManager>> coreMgrs{marie, pauline, focus, berthe,
		                                                                          michelle};
		const std::initializer_list<std::reference_wrapper<ClientConference>> participants{pauline, berthe, michelle};
		createChatRooms(nbLegacyChatRooms, coreMgrs, participants, focus, marie.getCMgr(), std::string("Legacy"),
		                encrypted, true, false);

		ms_message("%s is turning its network off before the server unifies the chatrooms address",
		           linphone_core_get_identity(berthe.getLc()));
		linphone_core_set_network_reachable(berthe.getLc(), FALSE);

		std::initializer_list<std::reference_wrapper<ConfCoreManager>> onlineClientCores{marie, michelle, pauline};
		int expected_history_size = 1;
		for (auto chatRoom : marie.getCore().getChatRooms()) {
			initialPaulineStats = pauline.getStats();
			initialMichelleStats = michelle.getStats();
			initialBertheStats = berthe.getStats();

			auto confAddr = chatRoom->getConferenceAddress();
			std::string confAddrString = confAddr->toString();
			// Test messaging out to ensure everybody is up and running
			std::string msgText = std::string("[Before unification]: message in chatroom ") + confAddrString +
			                      std::string(" subject ") + chatRoom->getSubjectUtf8() + std::string(" from ") +
			                      marie.getIdentity().toString();
			LinphoneChatMessage *msg = ClientConference::sendTextMsg(chatRoom->toC(), msgText);
			BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, berthe}).wait([msg] {
				return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
			}));
			for (ConfCoreManager &recvCore : onlineClientCores) {
				auto cr = recvCore.searchChatRoom(nullptr, confAddr->toC());
				BC_ASSERT_PTR_NOT_NULL(cr);
				if (cr) {
					BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, berthe})
					                   .wait([&cr, &expected_history_size] {
						                   return linphone_chat_room_get_history_size(cr) == expected_history_size;
					                   }));
					if (recvCore.getCMgr() != marie.getCMgr()) {
						LinphoneChatMessage *lastMsg = recvCore.getStats().last_received_chat_message;
						BC_ASSERT_PTR_NOT_NULL(lastMsg);
						if (lastMsg) {
							BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_utf8_text(lastMsg), msgText.c_str());
						}
					}
				}
			}
			linphone_chat_message_unref(msg);

			BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_LinphoneMessageReceived,
			                             initialPaulineStats.number_of_LinphoneMessageReceived + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_LinphoneMessageReceived,
			                             initialMichelleStats.number_of_LinphoneMessageReceived + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_FALSE(wait_for_list(coresList, &berthe.getStats().number_of_LinphoneMessageReceived,
			                              initialBertheStats.number_of_LinphoneMessageReceived + 1, 1000));

			initialMarieStats = marie.getStats();
			initialPaulineStats = pauline.getStats();
			initialMichelleStats = michelle.getStats();
			initialBertheStats = berthe.getStats();

			// Change subject
			std::string subject =
			    std::string("Subject of chatroom ") + confAddrString + std::string(" before migration");
			ms_message("%s changes the subject of chatroom %s to '%s'", linphone_core_get_identity(marie.getLc()),
			           confAddrString.c_str(), subject.c_str());
			linphone_chat_room_set_subject(chatRoom->toC(), subject.c_str());
			for (ConfCoreManager &subjectChangeCore : onlineClientCores) {
				auto cr = subjectChangeCore.searchChatRoom(nullptr, confAddr->toC());
				BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, berthe}).wait([&cr, &subject] {
					return strcmp(linphone_chat_room_get_subject_utf8(cr), subject.c_str()) == 0;
				}));
			}
			BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_chat_room_subject_changed,
			                             initialMarieStats.number_of_chat_room_subject_changed + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_chat_room_subject_changed,
			                             initialMichelleStats.number_of_chat_room_subject_changed + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_chat_room_subject_changed,
			                             initialPaulineStats.number_of_chat_room_subject_changed + 1,
			                             liblinphone_tester_sip_timeout));
			BC_ASSERT_FALSE(wait_for_list(coresList, &berthe.getStats().number_of_chat_room_subject_changed,
			                              initialBertheStats.number_of_chat_room_subject_changed + 1, 1000));
		}

		stats initialFocusStats = focus.getStats();
		initialMarieStats = marie.getStats();
		initialPaulineStats = pauline.getStats();
		initialMichelleStats = michelle.getStats();
		initialBertheStats = berthe.getStats();
		// Start chatroom migration
		BC_ASSERT_TRUE(linphone_core_unify_chat_rooms_address(focus.getLc()));

		BC_ASSERT_TRUE(
		    wait_for_list(coresList, &focus.getStats().number_of_LinphoneChatRoomAlternativeAddressChanged,
		                  initialFocusStats.number_of_LinphoneChatRoomAlternativeAddressChanged + nbLegacyChatRooms,
		                  liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(
		    wait_for_list(coresList, &marie.getStats().number_of_LinphoneChatRoomAlternativeAddressChanged,
		                  initialMarieStats.number_of_LinphoneChatRoomAlternativeAddressChanged + nbLegacyChatRooms,
		                  liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(
		    wait_for_list(coresList, &pauline.getStats().number_of_LinphoneChatRoomAlternativeAddressChanged,
		                  initialPaulineStats.number_of_LinphoneChatRoomAlternativeAddressChanged + nbLegacyChatRooms,
		                  liblinphone_tester_sip_timeout));
		BC_ASSERT_TRUE(
		    wait_for_list(coresList, &michelle.getStats().number_of_LinphoneChatRoomAlternativeAddressChanged,
		                  initialMichelleStats.number_of_LinphoneChatRoomAlternativeAddressChanged + nbLegacyChatRooms,
		                  liblinphone_tester_sip_timeout));
		BC_ASSERT_FALSE(wait_for_list(
		    coresList, &berthe.getStats().number_of_LinphoneChatRoomAlternativeAddressChanged,
		    initialBertheStats.number_of_LinphoneChatRoomAlternativeAddressChanged + nbLegacyChatRooms, 1000));

		// Send message
		for (const ConfCoreManager &sendCore : onlineClientCores) {
			expected_history_size++;
			for (auto chatRoom : sendCore.getCore().getChatRooms()) {
				initialBertheStats = berthe.getStats();
				auto confAddr = chatRoom->getConferenceAddress();
				auto confAddrString = confAddr->toStringUriOnlyOrdered();
				std::string msgText = std::string("[After unification]: message in chatroom ") + confAddrString +
				                      std::string(" subject ") + chatRoom->getSubjectUtf8() + std::string(" from ") +
				                      sendCore.getIdentity().toString();
				LinphoneChatMessage *msg = ClientConference::sendTextMsg(chatRoom->toC(), msgText);
				BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, berthe}).wait([msg] {
					return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
				}));
				linphone_chat_message_unref(msg);
				for (ConfCoreManager &recvCore : onlineClientCores) {
					auto cr = recvCore.searchChatRoom(nullptr, confAddr->toC());
					BC_ASSERT_PTR_NOT_NULL(cr);
					if (cr) {
						BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, berthe})
						                   .wait([&cr, &expected_history_size] {
							                   return linphone_chat_room_get_history_size(cr) == expected_history_size;
						                   }));
						if (recvCore.getCMgr() != sendCore.getCMgr()) {
							LinphoneChatMessage *lastMsg = recvCore.getStats().last_received_chat_message;
							BC_ASSERT_PTR_NOT_NULL(lastMsg);
							if (lastMsg) {
								BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_utf8_text(lastMsg), msgText.c_str());
							}
						}
					}
				}
				BC_ASSERT_FALSE(wait_for_list(coresList, &berthe.getStats().number_of_LinphoneMessageReceived,
				                              initialBertheStats.number_of_LinphoneMessageReceived + 1, 1000));

				if (sendCore.getCMgr() == marie.getCMgr()) {
					initialMarieStats = marie.getStats();
					initialPaulineStats = pauline.getStats();
					initialMichelleStats = michelle.getStats();
					initialBertheStats = berthe.getStats();

					// Change subject
					std::string subject =
					    std::string("Subject of chatroom ") + confAddrString + std::string(" after migration");
					ms_message("%s changes the subject of chatroom %s to '%s'",
					           linphone_core_get_identity(marie.getLc()), confAddrString.c_str(), subject.c_str());
					linphone_chat_room_set_subject(chatRoom->toC(), subject.c_str());
					for (ConfCoreManager &subjectChangeCore : onlineClientCores) {
						auto cr = subjectChangeCore.searchChatRoom(nullptr, confAddr->toC());
						BC_ASSERT_TRUE(
						    CoreManagerAssert({focus, marie, pauline, michelle, berthe}).wait([&cr, &subject] {
							    return strcmp(linphone_chat_room_get_subject_utf8(cr), subject.c_str()) == 0;
						    }));
					}
					BC_ASSERT_TRUE(wait_for_list(coresList, &marie.getStats().number_of_chat_room_subject_changed,
					                             initialMarieStats.number_of_chat_room_subject_changed + 1,
					                             liblinphone_tester_sip_timeout));
					BC_ASSERT_TRUE(wait_for_list(coresList, &michelle.getStats().number_of_chat_room_subject_changed,
					                             initialMichelleStats.number_of_chat_room_subject_changed + 1,
					                             liblinphone_tester_sip_timeout));
					BC_ASSERT_TRUE(wait_for_list(coresList, &pauline.getStats().number_of_chat_room_subject_changed,
					                             initialPaulineStats.number_of_chat_room_subject_changed + 1,
					                             liblinphone_tester_sip_timeout));
					BC_ASSERT_FALSE(wait_for_list(coresList, &berthe.getStats().number_of_chat_room_subject_changed,
					                              initialBertheStats.number_of_chat_room_subject_changed + 1, 1000));
				}
			}
		}

		if (server_restart) {
			ms_message("%s is restarting its core just before %s comes back online",
			           linphone_core_get_identity(focus.getLc()), linphone_core_get_identity(berthe.getLc()));
			coresList = bctbx_list_remove(coresList, focus.getLc());
			// Restart flexisip
			focus.reStart();
			coresList = bctbx_list_append(coresList, focus.getLc());
			if (domain_registration_account) {
				initialFocusStats = focus.getStats();
				linphone_core_add_account(focus.getLc(), domain_registration_account);
				linphone_core_set_default_account(focus.getLc(), domain_registration_account);
				BC_ASSERT_TRUE(wait_for_list(coresList, &focus.getStats().number_of_LinphoneRegistrationOk,
				                             initialFocusStats.number_of_LinphoneRegistrationOk + 1,
				                             liblinphone_tester_sip_timeout));
			}
		}

		initialBertheStats = berthe.getStats();
		ms_message("%s is turning its network on after the server unifies the chatrooms address",
		           linphone_core_get_identity(berthe.getLc()));
		linphone_core_set_network_reachable(berthe.getLc(), TRUE);
		BC_ASSERT_TRUE(
		    wait_for_list(coresList, &berthe.getStats().number_of_LinphoneChatRoomAlternativeAddressChanged,
		                  initialBertheStats.number_of_LinphoneChatRoomAlternativeAddressChanged + nbLegacyChatRooms,
		                  liblinphone_tester_sip_timeout));

		BC_ASSERT_TRUE(wait_for_list(coresList, &berthe.getStats().number_of_chat_room_subject_changed,
		                             initialBertheStats.number_of_chat_room_subject_changed + 2 * nbLegacyChatRooms,
		                             liblinphone_tester_sip_timeout));

		for (auto chatRoom : berthe.getCore().getChatRooms()) {
			BC_ASSERT_TRUE(
			    CoreManagerAssert({focus, marie, pauline, michelle, berthe}).wait([&chatRoom, &expected_history_size] {
				    return linphone_chat_room_get_history_size(chatRoom->toC()) == expected_history_size;
			    }));
			auto marieCr = marie.searchChatRoom(nullptr, chatRoom->getConferenceAddress()->toC());
			BC_ASSERT_PTR_NOT_NULL(marieCr);
			if (marieCr) {
				BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject_utf8(marieCr),
				                       linphone_chat_room_get_subject_utf8(chatRoom->toC()));
			}
		}

		// Send message
		std::initializer_list<std::reference_wrapper<ConfCoreManager>> clientCores{marie, michelle, pauline, berthe};
		for (const ConfCoreManager &sendCore : clientCores) {
			expected_history_size++;
			for (auto chatRoom : sendCore.getCore().getChatRooms()) {
				auto confAddr = chatRoom->getConferenceAddress();
				auto confAddrString = confAddr->toStringUriOnlyOrdered();
				std::string msgText = std::string("Is this chat still working?\n Message in chatroom ") +
				                      confAddrString.c_str() + std::string(" subject ") +
				                      chatRoom->getSubjectUtf8().c_str() + std::string(" from ") +
				                      sendCore.getIdentity().toString();
				LinphoneChatMessage *msg = ClientConference::sendTextMsg(chatRoom->toC(), msgText);
				BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, berthe}).wait([msg] {
					return (linphone_chat_message_get_state(msg) == LinphoneChatMessageStateDelivered);
				}));
				check_chat_message_after_migration(msg, confAddr);
				linphone_chat_message_unref(msg);
				for (ConfCoreManager &recvCore : clientCores) {
					auto cr = recvCore.searchChatRoom(nullptr, confAddr->toC());
					BC_ASSERT_PTR_NOT_NULL(cr);
					if (cr) {
						BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, pauline, michelle, berthe})
						                   .wait([&cr, &expected_history_size] {
							                   return linphone_chat_room_get_history_size(cr) == expected_history_size;
						                   }));
						if (recvCore.getCMgr() != sendCore.getCMgr()) {
							LinphoneChatMessage *lastMsg = recvCore.getStats().last_received_chat_message;
							BC_ASSERT_PTR_NOT_NULL(lastMsg);
							if (lastMsg) {
								BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_utf8_text(lastMsg), msgText.c_str());
								check_chat_message_after_migration(lastMsg, confAddr);
							}
						}
					}
				}
			}
		}

		// wait a bit longer to detect side effect if any
		CoreManagerAssert({focus, marie, michelle, berthe, pauline}).waitUntil(chrono::seconds(1), [] {
			return false;
		});

		for (auto chatRoom : focus.getCore().getChatRooms()) {
			for (auto participant : chatRoom->getParticipants()) {
				//  force deletion by removing devices
				std::shared_ptr<Address> participantAddress = participant->getAddress();
				linphone_chat_room_set_participant_devices(chatRoom->toC(), participantAddress->toC(), NULL);
			}
		}

		// wait until chatroom is deleted server side
		BC_ASSERT_TRUE(CoreManagerAssert({focus, marie, berthe, michelle, pauline}).wait([&focus] {
			return focus.getCore().getChatRooms().size() == 0;
		}));

		// wait a bit longer to detect side effect if any
		CoreManagerAssert({focus, marie, berthe, michelle, pauline}).waitUntil(chrono::seconds(2), [] {
			return false;
		});

		// to avoid creation attempt of a new chatroom
		LinphoneProxyConfig *config = linphone_core_get_default_proxy_config(focus.getLc());
		linphone_proxy_config_edit(config);
		linphone_proxy_config_set_conference_factory_uri(config, NULL);
		linphone_proxy_config_done(config);

		if (domain_registration_account) {
			linphone_account_unref(domain_registration_account);
		}

		bctbx_list_free(coresList);
	}
}

} // namespace LinphoneTest

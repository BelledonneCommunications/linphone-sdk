/*
	liblinphone_tester - liblinphone test suite
	Copyright (C) 2013  Belledonne Communications SARL

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/



#include "linphone/core.h"
#include "tester_utils.h"
#include "liblinphone_tester.h"

#if __clang__ || ((__GNUC__ == 4 && __GNUC_MINOR__ >= 6) || __GNUC__ > 4)
#pragma GCC diagnostic push
#endif
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

static const char sFactoryUri[] = "sip:conference-factory@conf.example.org";


static void chat_room_is_composing_received (LinphoneChatRoom *cr, const LinphoneAddress *remoteAddr, bool_t isComposing) {
	LinphoneCore *core = linphone_chat_room_get_core(cr);
	LinphoneCoreManager *manager = (LinphoneCoreManager *)linphone_core_get_user_data(core);
	if (isComposing)
		manager->stat.number_of_LinphoneIsComposingActiveReceived++;
	else
		manager->stat.number_of_LinphoneIsComposingIdleReceived++;
}

static void chat_room_participant_added (LinphoneChatRoom *cr, const LinphoneEventLog *event_log) {
	LinphoneCore *core = linphone_chat_room_get_core(cr);
	LinphoneCoreManager *manager = (LinphoneCoreManager *)linphone_core_get_user_data(core);
	manager->stat.number_of_participants_added++;
}

static void chat_room_participant_admin_status_changed (LinphoneChatRoom *cr, const LinphoneEventLog *event_log) {
	LinphoneCore *core = linphone_chat_room_get_core(cr);
	LinphoneCoreManager *manager = (LinphoneCoreManager *)linphone_core_get_user_data(core);
	manager->stat.number_of_participant_admin_statuses_changed++;
}

static void chat_room_participant_removed (LinphoneChatRoom *cr, const LinphoneEventLog *event_log) {
	LinphoneCore *core = linphone_chat_room_get_core(cr);
	LinphoneCoreManager *manager = (LinphoneCoreManager *)linphone_core_get_user_data(core);
	manager->stat.number_of_participants_removed++;
}

static void chat_room_state_changed (LinphoneChatRoom *cr, LinphoneChatRoomState newState) {
	LinphoneCore *core = linphone_chat_room_get_core(cr);
	LinphoneCoreManager *manager = (LinphoneCoreManager *)linphone_core_get_user_data(core);
	switch (newState) {
		case LinphoneChatRoomStateNone:
			break;
		case LinphoneChatRoomStateInstantiated:
			manager->stat.number_of_LinphoneChatRoomStateInstantiated++;
			break;
		case LinphoneChatRoomStateCreationPending:
			manager->stat.number_of_LinphoneChatRoomStateCreationPending++;
			break;
		case LinphoneChatRoomStateCreated:
			manager->stat.number_of_LinphoneChatRoomStateCreated++;
			break;
		case LinphoneChatRoomStateTerminationPending:
			manager->stat.number_of_LinphoneChatRoomStateTerminationPending++;
			break;
		case LinphoneChatRoomStateTerminated:
			manager->stat.number_of_LinphoneChatRoomStateTerminated++;
			break;
		case LinphoneChatRoomStateCreationFailed:
			manager->stat.number_of_LinphoneChatRoomStateCreationFailed++;
			break;
	}
}

static void chat_room_subject_changed (LinphoneChatRoom *cr, const LinphoneEventLog *event_log) {
	LinphoneCore *core = linphone_chat_room_get_core(cr);
	LinphoneCoreManager *manager = (LinphoneCoreManager *)linphone_core_get_user_data(core);
	manager->stat.number_of_subject_changed++;
}

static void core_chat_room_state_changed (LinphoneCore *core, LinphoneChatRoom *cr, LinphoneChatRoomState state) {
	if (state == LinphoneChatRoomStateInstantiated) {
		LinphoneChatRoomCbs *cbs = linphone_chat_room_get_callbacks(cr);
		linphone_chat_room_cbs_set_is_composing_received(cbs, chat_room_is_composing_received);
		linphone_chat_room_cbs_set_participant_added(cbs, chat_room_participant_added);
		linphone_chat_room_cbs_set_participant_admin_status_changed(cbs, chat_room_participant_admin_status_changed);
		linphone_chat_room_cbs_set_participant_removed(cbs, chat_room_participant_removed);
		linphone_chat_room_cbs_set_state_changed(cbs, chat_room_state_changed);
		linphone_chat_room_cbs_set_subject_changed(cbs, chat_room_subject_changed);
	}
}

static void configure_core_for_conference (LinphoneCore *core, const char* username, const LinphoneAddress *factoryAddr, bool_t server) {
	const char *identity = linphone_core_get_identity(core);
	const char *new_username;
	LinphoneAddress *addr = linphone_address_new(identity);
	if (!username) {
		new_username = linphone_address_get_username(addr);
	}
	linphone_address_set_username(addr, (username) ? username : new_username);
	char *newIdentity = linphone_address_as_string_uri_only(addr);
	linphone_address_unref(addr);
	linphone_core_set_primary_contact(core, newIdentity);
	bctbx_free(newIdentity);
	linphone_core_enable_conference_server(core, server);
	char *factoryUri = linphone_address_as_string(factoryAddr);
	linphone_core_set_conference_factory_uri(core, factoryUri);
	bctbx_free(factoryUri);
	linphone_config_set_int(linphone_core_get_config(core), "sip", "use_cpim", 1);
}

static void _configure_core_for_conference (LinphoneCoreManager *lcm, LinphoneAddress *factoryAddr) {
	configure_core_for_conference(lcm->lc, NULL, factoryAddr, FALSE);
}

static void _configure_core_for_callbacks(LinphoneCoreManager *lcm, LinphoneCoreCbs *cbs) {
	linphone_core_add_callbacks(lcm->lc, cbs);
	linphone_core_set_user_data(lcm->lc, lcm);
}

// Configure list of core manager for conference and add the listener
static bctbx_list_t * init_core_for_conference(bctbx_list_t *coreManagerList) {
	bctbx_list_t *coresList = NULL;
	LinphoneAddress *factoryAddr = linphone_address_new(sFactoryUri);
	bctbx_list_for_each2(coreManagerList, (void (*)(void *, void*))_configure_core_for_conference, (void*) factoryAddr);
	linphone_address_unref(factoryAddr);

	LinphoneCoreCbs *cbs = linphone_factory_create_core_cbs(linphone_factory_get());
	linphone_core_cbs_set_chat_room_state_changed(cbs, core_chat_room_state_changed);
	bctbx_list_for_each2(coreManagerList, (void (*)(void *, void*))_configure_core_for_callbacks, (void*) cbs);
	linphone_core_cbs_unref(cbs);
	bctbx_list_t *iterator = coreManagerList;
	while(iterator != bctbx_list_last_elem(coreManagerList)) {
		if (!coresList) {
			coresList = bctbx_list_new(((LinphoneCoreManager*)(bctbx_list_get_data(iterator)))->lc);
		} else {
			bctbx_list_append(coresList, ((LinphoneCoreManager*)(bctbx_list_get_data(iterator)))->lc);
		}
		iterator = bctbx_list_next(iterator);
	}
	bctbx_list_append(coresList, ((LinphoneCoreManager*)(bctbx_list_get_data(iterator)))->lc);
	return coresList;
}

static LinphoneChatRoom * check_creation_chat_room_client_side(bctbx_list_t *lcs, LinphoneCoreManager *lcm, stats *initialStats, const LinphoneAddress *confAddr, const char* subject, int participantNumber, bool_t isAdmin) {
	BC_ASSERT_TRUE(wait_for_list(lcs, &lcm->stat.number_of_LinphoneChatRoomStateCreationPending, initialStats->number_of_LinphoneChatRoomStateCreationPending + 1, 5000));
	BC_ASSERT_TRUE(wait_for_list(lcs, &lcm->stat.number_of_LinphoneChatRoomStateCreated, initialStats->number_of_LinphoneChatRoomStateCreated + 1, 5000));
	char *deviceIdentity = linphone_core_get_device_identity(lcm->lc);
	LinphoneAddress *localAddr = linphone_address_new(deviceIdentity);
	bctbx_free(deviceIdentity);
	LinphoneChatRoom *chatRoom = linphone_core_find_chat_room(lcm->lc, confAddr, localAddr);
	linphone_address_unref(localAddr);
	BC_ASSERT_PTR_NOT_NULL(chatRoom);
	if (chatRoom) {
		BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(chatRoom), participantNumber, int, "%d");
		LinphoneParticipant *participant = linphone_chat_room_get_me(chatRoom);
		BC_ASSERT_PTR_NOT_NULL(participant);
		BC_ASSERT(isAdmin == linphone_participant_is_admin(participant));
		BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(chatRoom), subject);
	}
	return chatRoom;
}

static LinphoneChatRoom * create_chat_room_client_side(bctbx_list_t *lcs, LinphoneCoreManager *lcm, stats *initialStats, bctbx_list_t *participantsAddresses, const char* initialSubject) {
	LinphoneChatRoom *chatRoom = linphone_core_create_client_group_chat_room(lcm->lc, initialSubject);
	if (!chatRoom) return NULL;

	BC_ASSERT_TRUE(wait_for_list(lcs, &lcm->stat.number_of_LinphoneChatRoomStateInstantiated, initialStats->number_of_LinphoneChatRoomStateInstantiated + 1, 100));

	// Add participants
	linphone_chat_room_add_participants(chatRoom, participantsAddresses);

	// Check that the chat room is correctly created on Marie's side and that the participants are added
	BC_ASSERT_TRUE(wait_for_list(lcs, &lcm->stat.number_of_LinphoneChatRoomStateCreationPending, initialStats->number_of_LinphoneChatRoomStateCreationPending + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(lcs, &lcm->stat.number_of_LinphoneChatRoomStateCreated, initialStats->number_of_LinphoneChatRoomStateCreated + 1, 10000));
	BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(chatRoom), bctbx_list_size(participantsAddresses), int, "%d");
	LinphoneParticipant *participant = linphone_chat_room_get_me(chatRoom);
	BC_ASSERT_PTR_NOT_NULL(participant);
	if (participant)
		BC_ASSERT_TRUE(linphone_participant_is_admin(participant));
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(chatRoom), initialSubject);

	bctbx_list_free_with_data(participantsAddresses, (bctbx_list_free_func)linphone_address_unref);
	participantsAddresses = NULL;

	return chatRoom;
}

static void group_chat_room_creation_local (void) {
	LinphoneCoreManager *focus = linphone_core_manager_new("empty_rc");
	LinphoneCoreManager *marie = linphone_core_manager_new("empty_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_new("empty_rc");
	LinphoneCoreManager *laure = linphone_core_manager_new("empty_rc");
	LinphoneCoreManager *chloe = linphone_core_manager_new("empty_rc");
	bctbx_list_t *coresList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	int dummy = 0;
	coresList = bctbx_list_append(coresList, focus->lc);
	coresList = bctbx_list_append(coresList, marie->lc);
	coresList = bctbx_list_append(coresList, pauline->lc);
	coresList = bctbx_list_append(coresList, laure->lc);
	coresList = bctbx_list_append(coresList, chloe->lc);
	const char *factoryUri = linphone_core_get_identity(focus->lc);
	LinphoneAddress *factoryAddr = linphone_address_new(factoryUri);
	linphone_address_set_username(factoryAddr, "focus");
	configure_core_for_conference(focus->lc, "focus", factoryAddr, TRUE);
	configure_core_for_conference(marie->lc, "marie", factoryAddr, FALSE);
	configure_core_for_conference(pauline->lc, "pauline", factoryAddr, FALSE);
	configure_core_for_conference(laure->lc, "laure", factoryAddr, FALSE);
	configure_core_for_conference(chloe->lc, "chloe", factoryAddr, FALSE);
	linphone_address_unref(factoryAddr);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	LinphoneCoreCbs *cbs = linphone_factory_create_core_cbs(linphone_factory_get());
	linphone_core_cbs_set_chat_room_state_changed(cbs, core_chat_room_state_changed);
	linphone_core_add_callbacks(marie->lc, cbs);
	linphone_core_add_callbacks(pauline->lc, cbs);
	linphone_core_add_callbacks(laure->lc, cbs);
	linphone_core_add_callbacks(chloe->lc, cbs);
	linphone_core_cbs_unref(cbs);
	linphone_core_set_user_data(marie->lc, marie);
	linphone_core_set_user_data(pauline->lc, pauline);
	linphone_core_set_user_data(laure->lc, laure);
	linphone_core_set_user_data(chloe->lc, chloe);
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialLaureStats = laure->stat;
	stats initialChloeStats = chloe->stat;

	// Marie creates a new group chat room and adds Pauline and Laure as participants
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject);
	participantsAddresses = NULL;
	LinphoneAddress *paulineAddr = linphone_address_new(linphone_core_get_identity(pauline->lc));
	LinphoneParticipant *paulineParticipant = linphone_chat_room_find_participant(marieCr, paulineAddr);
	linphone_address_unref(paulineAddr);
	BC_ASSERT_PTR_NOT_NULL(paulineParticipant);
	BC_ASSERT_FALSE(linphone_participant_is_admin(paulineParticipant));
	const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline, &initialPaulineStats, confAddr, initialSubject, 2, 0);
	LinphoneAddress *marieAddr = linphone_address_new(linphone_core_get_identity(marie->lc));
	LinphoneParticipant *marieParticipant = linphone_chat_room_find_participant(paulineCr, marieAddr);
	linphone_address_unref(marieAddr);
	BC_ASSERT_PTR_NOT_NULL(marieParticipant);
	BC_ASSERT_TRUE(linphone_participant_is_admin(marieParticipant));

	// Check that the chat room is correctly created on Laure's side and that the participants are added
	LinphoneChatRoom *laureCr = check_creation_chat_room_client_side(coresList, laure, &initialLaureStats, confAddr, initialSubject, 2, 0);
	marieAddr = linphone_address_new(linphone_core_get_identity(marie->lc));
	marieParticipant = linphone_chat_room_find_participant(paulineCr, marieAddr);
	linphone_address_unref(marieAddr);
	BC_ASSERT_PTR_NOT_NULL(marieParticipant);
	BC_ASSERT_TRUE(linphone_participant_is_admin(marieParticipant));

	// Pauline tries to change the subject but is not admin so it fails
	const char *newSubject = "Let's go drink a beer";
	linphone_chat_room_set_subject(paulineCr, newSubject);
	wait_for_list(coresList, &dummy, 1, 1000);
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(marieCr), initialSubject);
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(paulineCr), initialSubject);
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(laureCr), initialSubject);

	// Marie now changes the subject
	linphone_chat_room_set_subject(marieCr, newSubject);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_subject_changed, initialMarieStats.number_of_subject_changed + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_subject_changed, initialPaulineStats.number_of_subject_changed + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_subject_changed, initialLaureStats.number_of_subject_changed + 1, 1000));
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(marieCr), newSubject);
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(paulineCr), newSubject);
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(laureCr), newSubject);

	// Marie designates Pauline as admin
	paulineAddr = linphone_address_new(linphone_core_get_identity(pauline->lc));
	paulineParticipant = linphone_chat_room_find_participant(marieCr, paulineAddr);
	linphone_address_unref(paulineAddr);
	BC_ASSERT_PTR_NOT_NULL(paulineParticipant);
	linphone_chat_room_set_participant_admin_status(marieCr, paulineParticipant, TRUE);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_participant_admin_statuses_changed, initialMarieStats.number_of_participant_admin_statuses_changed + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_participant_admin_statuses_changed, initialPaulineStats.number_of_participant_admin_statuses_changed + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_participant_admin_statuses_changed, initialLaureStats.number_of_participant_admin_statuses_changed + 1, 1000));
	BC_ASSERT_TRUE(linphone_participant_is_admin(paulineParticipant));

	// Pauline adds Chloe to the chat room
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(chloe->lc)));
	linphone_chat_room_add_participants(paulineCr, participantsAddresses);
	bctbx_list_free_with_data(participantsAddresses, (bctbx_list_free_func)linphone_address_unref);
	participantsAddresses = NULL;

	// Check that the chat room is correctly created on Chloe's side and that she was added everywhere
	BC_ASSERT_TRUE(wait_for_list(coresList, &chloe->stat.number_of_LinphoneChatRoomStateCreated, initialChloeStats.number_of_LinphoneChatRoomStateCreated + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_participants_added, initialMarieStats.number_of_participants_added + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_participants_added, initialPaulineStats.number_of_participants_added + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_participants_added, initialLaureStats.number_of_participants_added + 1, 1000));
	BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(marieCr), 3, int, "%d");
	BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(paulineCr), 3, int, "%d");
	BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(laureCr), 3, int, "%d");
	char *chloeDeviceIdentity = linphone_core_get_device_identity(chloe->lc);
	LinphoneAddress *chloeAddr = linphone_address_new(chloeDeviceIdentity);
	bctbx_free(chloeDeviceIdentity);
	LinphoneChatRoom *chloeCr = linphone_core_find_chat_room(chloe->lc, confAddr, chloeAddr);
	linphone_address_unref(chloeAddr);
	BC_ASSERT_PTR_NOT_NULL(chloeCr);
	BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(chloeCr), 3, int, "%d");
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(chloeCr), newSubject);

	// Pauline revokes the admin status of Marie
	marieAddr = linphone_address_new(linphone_core_get_identity(marie->lc));
	marieParticipant = linphone_chat_room_find_participant(paulineCr, marieAddr);
	linphone_address_unref(marieAddr);
	BC_ASSERT_PTR_NOT_NULL(marieParticipant);
	linphone_chat_room_set_participant_admin_status(paulineCr, marieParticipant, FALSE);
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_participant_admin_statuses_changed, initialPaulineStats.number_of_participant_admin_statuses_changed + 2, 1000));
	BC_ASSERT_FALSE(linphone_participant_is_admin(marieParticipant));

	// Marie tries to change the subject again but is not admin, so it is not changed
	linphone_chat_room_set_subject(marieCr, initialSubject);
	wait_for_list(coresList, &dummy, 1, 1000);

	// Chloe begins composing a message
	linphone_chat_room_compose(chloeCr);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneIsComposingActiveReceived, initialMarieStats.number_of_LinphoneIsComposingActiveReceived + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneIsComposingActiveReceived, initialPaulineStats.number_of_LinphoneIsComposingActiveReceived + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_LinphoneIsComposingActiveReceived, initialLaureStats.number_of_LinphoneIsComposingActiveReceived + 1, 1000));
	const char *chloeMessage = "Hello";
	LinphoneChatMessage *msg = linphone_chat_room_create_message(chloeCr, chloeMessage);
	LinphoneChatMessageCbs *msgCbs = linphone_chat_message_get_callbacks(msg);
	linphone_chat_message_cbs_set_msg_state_changed(msgCbs, liblinphone_tester_chat_message_msg_state_changed);
	linphone_chat_message_send(msg);
	BC_ASSERT_TRUE(wait_for_list(coresList, &chloe->stat.number_of_LinphoneMessageDelivered, initialChloeStats.number_of_LinphoneMessageDelivered + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneMessageReceived, initialMarieStats.number_of_LinphoneMessageReceived + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneMessageReceived, initialPaulineStats.number_of_LinphoneMessageReceived + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_LinphoneMessageReceived, initialLaureStats.number_of_LinphoneMessageReceived + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneIsComposingIdleReceived, initialMarieStats.number_of_LinphoneIsComposingIdleReceived + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneIsComposingIdleReceived, initialPaulineStats.number_of_LinphoneIsComposingIdleReceived + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_LinphoneIsComposingIdleReceived, initialLaureStats.number_of_LinphoneIsComposingIdleReceived + 1, 1000));
	BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_text(marie->stat.last_received_chat_message), chloeMessage);
	chloeAddr = linphone_address_new(linphone_core_get_identity(chloe->lc));
	BC_ASSERT_TRUE(linphone_address_weak_equal(chloeAddr, linphone_chat_message_get_from_address(marie->stat.last_received_chat_message)));
	linphone_address_unref(chloeAddr);

	// Pauline removes Laure from the chat room
	LinphoneAddress *laureAddr = linphone_address_new(linphone_core_get_identity(laure->lc));
	LinphoneParticipant *laureParticipant = linphone_chat_room_find_participant(paulineCr, laureAddr);
	linphone_address_unref(laureAddr);
	BC_ASSERT_PTR_NOT_NULL(laureParticipant);
	linphone_chat_room_remove_participant(paulineCr, laureParticipant);
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(marieCr), newSubject);
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_LinphoneChatRoomStateTerminated, initialLaureStats.number_of_LinphoneChatRoomStateTerminated + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &chloe->stat.number_of_participants_removed, initialChloeStats.number_of_participants_removed + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_participants_removed, initialMarieStats.number_of_participants_removed + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_participants_removed, initialPaulineStats.number_of_participants_removed + 1, 1000));

	// Pauline removes Marie and Chloe from the chat room
	marieAddr = linphone_address_new(linphone_core_get_identity(marie->lc));
	marieParticipant = linphone_chat_room_find_participant(paulineCr, marieAddr);
	linphone_address_unref(marieAddr);
	BC_ASSERT_PTR_NOT_NULL(marieParticipant);
	chloeAddr = linphone_address_new(linphone_core_get_identity(chloe->lc));
	LinphoneParticipant *chloeParticipant = linphone_chat_room_find_participant(paulineCr, chloeAddr);
	linphone_address_unref(chloeAddr);
	BC_ASSERT_PTR_NOT_NULL(chloeParticipant);
	bctbx_list_t *participantsToRemove = NULL;
	participantsToRemove = bctbx_list_append(participantsToRemove, marieParticipant);
	participantsToRemove = bctbx_list_append(participantsToRemove, chloeParticipant);
	linphone_chat_room_remove_participants(paulineCr, participantsToRemove);
	bctbx_list_free_with_data(participantsToRemove, (bctbx_list_free_func)linphone_participant_unref);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneChatRoomStateTerminated, initialMarieStats.number_of_LinphoneChatRoomStateTerminated + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &chloe->stat.number_of_LinphoneChatRoomStateTerminated, initialChloeStats.number_of_LinphoneChatRoomStateTerminated + 1, 1000));
	BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(paulineCr), 0, int, "%d");

	// Pauline leaves the chat room
	wait_for_list(coresList, &dummy, 1, 1000);
	linphone_chat_room_leave(paulineCr);
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneChatRoomStateTerminationPending, initialPaulineStats.number_of_LinphoneChatRoomStateTerminationPending + 1, 100));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneChatRoomStateTerminated, initialPaulineStats.number_of_LinphoneChatRoomStateTerminated + 1, 1000));

	wait_for_list(coresList, &dummy, 1, 1000);
	bctbx_list_free(coresList);
	linphone_core_manager_destroy(focus);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
	linphone_core_manager_destroy(chloe);
}

static void group_chat_room_creation_server (void) {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_new("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_new("laure_tcp_rc");
	LinphoneCoreManager *chloe = linphone_core_manager_new("chloe_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	int dummy = 0;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	coresManagerList = bctbx_list_append(coresManagerList, chloe);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);

	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialLaureStats = laure->stat;
	stats initialChloeStats = chloe->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject);
	const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline, &initialPaulineStats, confAddr, initialSubject, 2, 0);

	// Check that the chat room is correctly created on Laure's side and that the participants are added
	LinphoneChatRoom *laureCr = check_creation_chat_room_client_side(coresList, laure, &initialLaureStats, confAddr, initialSubject, 2, 0);

	// Pauline tries to change the subject but is not admin so it fails
	const char *newSubject = "Let's go drink a beer";
	linphone_chat_room_set_subject(paulineCr, newSubject);
	wait_for_list(coresList, &dummy, 1, 1000);
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(marieCr), initialSubject);
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(paulineCr), initialSubject);
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(laureCr), initialSubject);

	// Marie now changes the subject
	linphone_chat_room_set_subject(marieCr, newSubject);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_subject_changed, initialMarieStats.number_of_subject_changed + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_subject_changed, initialPaulineStats.number_of_subject_changed + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_subject_changed, initialLaureStats.number_of_subject_changed + 1, 10000));
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(marieCr), newSubject);
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(paulineCr), newSubject);
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(laureCr), newSubject);

	// Marie designates Pauline as admin
	LinphoneAddress *paulineAddr = linphone_address_new(linphone_core_get_identity(pauline->lc));
	LinphoneParticipant *paulineParticipant = linphone_chat_room_find_participant(marieCr, paulineAddr);
	linphone_address_unref(paulineAddr);
	BC_ASSERT_PTR_NOT_NULL(paulineParticipant);
	linphone_chat_room_set_participant_admin_status(marieCr, paulineParticipant, TRUE);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_participant_admin_statuses_changed, initialMarieStats.number_of_participant_admin_statuses_changed + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_participant_admin_statuses_changed, initialPaulineStats.number_of_participant_admin_statuses_changed + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_participant_admin_statuses_changed, initialLaureStats.number_of_participant_admin_statuses_changed + 1, 10000));
	BC_ASSERT_TRUE(linphone_participant_is_admin(paulineParticipant));

	// Pauline adds Chloe to the chat room
	participantsAddresses = NULL;
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(chloe->lc)));
	linphone_chat_room_add_participants(paulineCr, participantsAddresses);
	bctbx_list_free_with_data(participantsAddresses, (bctbx_list_free_func)linphone_address_unref);
	participantsAddresses = NULL;

	// Check that the chat room is correctly created on Chloe's side and that she was added everywhere
	LinphoneChatRoom *chloeCr = check_creation_chat_room_client_side(coresList, chloe, &initialChloeStats, confAddr, newSubject, 3, 0);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_participants_added, initialMarieStats.number_of_participants_added + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_participants_added, initialPaulineStats.number_of_participants_added + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_participants_added, initialLaureStats.number_of_participants_added + 1, 10000));
	BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(marieCr), 3, int, "%d");
	BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(paulineCr), 3, int, "%d");
	BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(laureCr), 3, int, "%d");

	// Pauline revokes the admin status of Marie
	LinphoneAddress *marieAddr = linphone_address_new(linphone_core_get_identity(marie->lc));
	LinphoneParticipant *marieParticipant = linphone_chat_room_find_participant(paulineCr, marieAddr);
	linphone_address_unref(marieAddr);
	BC_ASSERT_PTR_NOT_NULL(marieParticipant);
	linphone_chat_room_set_participant_admin_status(paulineCr, marieParticipant, FALSE);
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_participant_admin_statuses_changed, initialPaulineStats.number_of_participant_admin_statuses_changed + 2, 10000));
	BC_ASSERT_FALSE(linphone_participant_is_admin(marieParticipant));

	// Marie tries to change the subject again but is not admin, so it is not changed
	linphone_chat_room_set_subject(marieCr, initialSubject);
	wait_for_list(coresList, &dummy, 1, 1000);

	// Chloe begins composing a message
	linphone_chat_room_compose(chloeCr);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneIsComposingActiveReceived, initialMarieStats.number_of_LinphoneIsComposingActiveReceived + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneIsComposingActiveReceived, initialPaulineStats.number_of_LinphoneIsComposingActiveReceived + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_LinphoneIsComposingActiveReceived, initialLaureStats.number_of_LinphoneIsComposingActiveReceived + 1, 10000));
	const char *chloeMessage = "Hello";
	LinphoneChatMessage *msg = linphone_chat_room_create_message(chloeCr, chloeMessage);
	LinphoneChatMessageCbs *msgCbs = linphone_chat_message_get_callbacks(msg);
	linphone_chat_message_cbs_set_msg_state_changed(msgCbs, liblinphone_tester_chat_message_msg_state_changed);
	linphone_chat_message_send(msg);
	BC_ASSERT_TRUE(wait_for_list(coresList, &chloe->stat.number_of_LinphoneMessageDelivered, initialChloeStats.number_of_LinphoneMessageDelivered + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneMessageReceived, initialMarieStats.number_of_LinphoneMessageReceived + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneMessageReceived, initialPaulineStats.number_of_LinphoneMessageReceived + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_LinphoneMessageReceived, initialLaureStats.number_of_LinphoneMessageReceived + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneIsComposingIdleReceived, initialMarieStats.number_of_LinphoneIsComposingIdleReceived + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneIsComposingIdleReceived, initialPaulineStats.number_of_LinphoneIsComposingIdleReceived + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_LinphoneIsComposingIdleReceived, initialLaureStats.number_of_LinphoneIsComposingIdleReceived + 1, 10000));
	BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_text(marie->stat.last_received_chat_message), chloeMessage);
	LinphoneAddress *chloeAddr = linphone_address_new(linphone_core_get_identity(chloe->lc));
	BC_ASSERT_TRUE(linphone_address_weak_equal(chloeAddr, linphone_chat_message_get_from_address(marie->stat.last_received_chat_message)));
	linphone_address_unref(chloeAddr);

	// Pauline removes Laure from the chat room
	LinphoneAddress *laureAddr = linphone_address_new(linphone_core_get_identity(laure->lc));
	LinphoneParticipant *laureParticipant = linphone_chat_room_find_participant(paulineCr, laureAddr);
	linphone_address_unref(laureAddr);
	BC_ASSERT_PTR_NOT_NULL(laureParticipant);
	linphone_chat_room_remove_participant(paulineCr, laureParticipant);
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(marieCr), newSubject);
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_LinphoneChatRoomStateTerminated, initialLaureStats.number_of_LinphoneChatRoomStateTerminated + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &chloe->stat.number_of_participants_removed, initialChloeStats.number_of_participants_removed + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_participants_removed, initialMarieStats.number_of_participants_removed + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_participants_removed, initialPaulineStats.number_of_participants_removed + 1, 10000));

	// Pauline removes Marie and Chloe from the chat room
	marieAddr = linphone_address_new(linphone_core_get_identity(marie->lc));
	marieParticipant = linphone_chat_room_find_participant(paulineCr, marieAddr);
	linphone_address_unref(marieAddr);
	BC_ASSERT_PTR_NOT_NULL(marieParticipant);
	chloeAddr = linphone_address_new(linphone_core_get_identity(chloe->lc));
	LinphoneParticipant *chloeParticipant = linphone_chat_room_find_participant(paulineCr, chloeAddr);
	linphone_address_unref(chloeAddr);
	BC_ASSERT_PTR_NOT_NULL(chloeParticipant);
	bctbx_list_t *participantsToRemove = NULL;
	participantsToRemove = bctbx_list_append(participantsToRemove, marieParticipant);
	participantsToRemove = bctbx_list_append(participantsToRemove, chloeParticipant);
	linphone_chat_room_remove_participants(paulineCr, participantsToRemove);
	bctbx_list_free_with_data(participantsToRemove, (bctbx_list_free_func)linphone_participant_unref);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneChatRoomStateTerminated, initialMarieStats.number_of_LinphoneChatRoomStateTerminated + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &chloe->stat.number_of_LinphoneChatRoomStateTerminated, initialChloeStats.number_of_LinphoneChatRoomStateTerminated + 1, 10000));
	BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(paulineCr), 0, int, "%d");

	// Pauline leaves the chat room
	wait_for_list(coresList, &dummy, 1, 1000);
	linphone_chat_room_leave(paulineCr);
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneChatRoomStateTerminationPending, initialPaulineStats.number_of_LinphoneChatRoomStateTerminationPending + 1, 100));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneChatRoomStateTerminated, initialPaulineStats.number_of_LinphoneChatRoomStateTerminated + 1, 10000));

	wait_for_list(coresList, &dummy, 1, 1000);
	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
	linphone_core_manager_destroy(chloe);
}

static void group_chat_room_send_message (void) {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_new("pauline_rc");
	LinphoneCoreManager *chloe = linphone_core_manager_new("chloe_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	int dummy = 0;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, chloe);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(chloe->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialChloeStats = chloe->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject);
	const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	check_creation_chat_room_client_side(coresList, pauline, &initialPaulineStats, confAddr, initialSubject, 2, 0);

	// Check that the chat room is correctly created on Chloe's side and that the participants are added
	LinphoneChatRoom *chloeCr = check_creation_chat_room_client_side(coresList, chloe, &initialChloeStats, confAddr, initialSubject, 2, 0);

	// Chloe begins composing a message
	linphone_chat_room_compose(chloeCr);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneIsComposingActiveReceived, initialMarieStats.number_of_LinphoneIsComposingActiveReceived + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneIsComposingActiveReceived, initialPaulineStats.number_of_LinphoneIsComposingActiveReceived + 1, 10000));
	const char *chloeMessage = "Hello";
	LinphoneChatMessage *msg = linphone_chat_room_create_message(chloeCr, chloeMessage);
	LinphoneChatMessageCbs *msgCbs = linphone_chat_message_get_callbacks(msg);
	linphone_chat_message_cbs_set_msg_state_changed(msgCbs, liblinphone_tester_chat_message_msg_state_changed);
	linphone_chat_message_send(msg);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneMessageReceived, initialMarieStats.number_of_LinphoneMessageReceived + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneMessageReceived, initialPaulineStats.number_of_LinphoneMessageReceived + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneIsComposingIdleReceived, initialMarieStats.number_of_LinphoneIsComposingIdleReceived + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneIsComposingIdleReceived, initialPaulineStats.number_of_LinphoneIsComposingIdleReceived + 1, 10000));
	BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_text(marie->stat.last_received_chat_message), chloeMessage);
	LinphoneAddress *chloeAddr = linphone_address_new(linphone_core_get_identity(chloe->lc));
	BC_ASSERT_TRUE(linphone_address_weak_equal(chloeAddr, linphone_chat_message_get_from_address(marie->stat.last_received_chat_message)));
	linphone_address_unref(chloeAddr);

	wait_for_list(coresList, &dummy, 1, 1000);
	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(chloe);
}

static void group_chat_room_invite_multi_register_account (void) {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *pauline1 = linphone_core_manager_new("pauline_rc");
	LinphoneCoreManager *pauline2 = linphone_core_manager_new("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_new("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	int dummy = 0;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline1);
	coresManagerList = bctbx_list_append(coresManagerList, pauline2);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline1->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPauline1Stats = pauline1->stat;
	stats initialPauline2Stats = pauline2->stat;
	stats initialLaureStats = laure->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject);

	const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

	// Check that the chat room is correctly created on Pauline1's side and that the participants are added
	check_creation_chat_room_client_side(coresList, pauline1, &initialPauline1Stats, confAddr, initialSubject, 2, 0);

	// Check that the chat room is correctly created on Pauline2's side and that the participants are added
	check_creation_chat_room_client_side(coresList, pauline2, &initialPauline2Stats, confAddr, initialSubject, 2, 0);

	// Check that the chat room is correctly created on Laure's side and that the participants are added
	check_creation_chat_room_client_side(coresList, laure, &initialLaureStats, confAddr, initialSubject, 2, 0);

	wait_for_list(coresList, &dummy, 1, 1000);
	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline1);
	linphone_core_manager_destroy(pauline2);
	linphone_core_manager_destroy(laure);
}

static void group_chat_room_add_admin (void) {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_new("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_new("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	int dummy = 0;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialLaureStats = laure->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject);

	const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	check_creation_chat_room_client_side(coresList, pauline, &initialPaulineStats, confAddr, initialSubject, 2, 0);

	// Check that the chat room is correctly created on Laure's side and that the participants are added
	check_creation_chat_room_client_side(coresList, laure, &initialLaureStats, confAddr, initialSubject, 2, 0);

	// Marie designates Pauline as admin
	LinphoneAddress *paulineAddr = linphone_address_new(linphone_core_get_identity(pauline->lc));
	LinphoneParticipant *paulineParticipant = linphone_chat_room_find_participant(marieCr, paulineAddr);
	linphone_address_unref(paulineAddr);
	BC_ASSERT_PTR_NOT_NULL(paulineParticipant);
	linphone_chat_room_set_participant_admin_status(marieCr, paulineParticipant, TRUE);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_participant_admin_statuses_changed, initialMarieStats.number_of_participant_admin_statuses_changed + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_participant_admin_statuses_changed, initialPaulineStats.number_of_participant_admin_statuses_changed + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_participant_admin_statuses_changed, initialLaureStats.number_of_participant_admin_statuses_changed + 1, 1000));
	BC_ASSERT_TRUE(linphone_participant_is_admin(paulineParticipant));

	wait_for_list(coresList, &dummy, 1, 1000);
	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
}

static void group_chat_room_add_admin_non_admin (void) {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_new("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_new("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	int dummy = 0;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialLaureStats = laure->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject);

	const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline, &initialPaulineStats, confAddr, initialSubject, 2, 0);

	// Check that the chat room is correctly created on Laure's side and that the participants are added
	check_creation_chat_room_client_side(coresList, laure, &initialLaureStats, confAddr, initialSubject, 2, 0);

	// Pauline designates Laure as admin
	LinphoneAddress *laureAddr = linphone_address_new(linphone_core_get_identity(laure->lc));
	LinphoneParticipant *laureParticipant = linphone_chat_room_find_participant(paulineCr, laureAddr);
	linphone_address_unref(laureAddr);
	BC_ASSERT_PTR_NOT_NULL(laureParticipant);
	linphone_chat_room_set_participant_admin_status(paulineCr, laureParticipant, TRUE);
	BC_ASSERT_FALSE(linphone_participant_is_admin(laureParticipant));

	wait_for_list(coresList, &dummy, 1, 1000);
	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
}

static void group_chat_room_remove_admin (void) {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_new("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_new("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	int dummy = 0;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialLaureStats = laure->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject);

	const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline, &initialPaulineStats, confAddr, initialSubject, 2, 0);

	// Check that the chat room is correctly created on Laure's side and that the participants are added
	check_creation_chat_room_client_side(coresList, laure, &initialLaureStats, confAddr, initialSubject, 2, 0);

	// Marie designates Pauline as admin
	LinphoneAddress *paulineAddr = linphone_address_new(linphone_core_get_identity(pauline->lc));
	LinphoneParticipant *paulineParticipant = linphone_chat_room_find_participant(marieCr, paulineAddr);
	linphone_address_unref(paulineAddr);
	BC_ASSERT_PTR_NOT_NULL(paulineParticipant);
	linphone_chat_room_set_participant_admin_status(marieCr, paulineParticipant, TRUE);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_participant_admin_statuses_changed, initialMarieStats.number_of_participant_admin_statuses_changed + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_participant_admin_statuses_changed, initialPaulineStats.number_of_participant_admin_statuses_changed + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_participant_admin_statuses_changed, initialLaureStats.number_of_participant_admin_statuses_changed + 1, 1000));
	BC_ASSERT_TRUE(linphone_participant_is_admin(paulineParticipant));

	// Pauline revokes the admin status of Marie
	LinphoneAddress *marieAddr = linphone_address_new(linphone_core_get_identity(marie->lc));
	LinphoneParticipant *marieParticipant = linphone_chat_room_find_participant(paulineCr, marieAddr);
	linphone_address_unref(marieAddr);
	BC_ASSERT_PTR_NOT_NULL(marieParticipant);
	linphone_chat_room_set_participant_admin_status(paulineCr, marieParticipant, FALSE);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_participant_admin_statuses_changed, initialMarieStats.number_of_participant_admin_statuses_changed + 2, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_participant_admin_statuses_changed, initialPaulineStats.number_of_participant_admin_statuses_changed + 2, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_participant_admin_statuses_changed, initialLaureStats.number_of_participant_admin_statuses_changed + 2, 1000));
	BC_ASSERT_FALSE(linphone_participant_is_admin(marieParticipant));

	wait_for_list(coresList, &dummy, 1, 1000);
	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
}


static void group_chat_room_change_subject (void) {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_new("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_new("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	int dummy = 0;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialLaureStats = laure->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	const char *newSubject = "New subject";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject);

	const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline, &initialPaulineStats, confAddr, initialSubject, 2, 0);

	// Check that the chat room is correctly created on Laure's side and that the participants are added
	LinphoneChatRoom *laureCr = check_creation_chat_room_client_side(coresList, laure, &initialLaureStats, confAddr, initialSubject, 2, 0);

	// Marie now changes the subject
	linphone_chat_room_set_subject(marieCr, newSubject);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_subject_changed, initialMarieStats.number_of_subject_changed + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_subject_changed, initialPaulineStats.number_of_subject_changed + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_subject_changed, initialLaureStats.number_of_subject_changed + 1, 10000));
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(marieCr), newSubject);
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(paulineCr), newSubject);
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(laureCr), newSubject);

	wait_for_list(coresList, &dummy, 1, 1000);
	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
}

static void group_chat_room_change_subject_non_admin (void) {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_new("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_new("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	int dummy = 0;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialLaureStats = laure->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	const char *newSubject = "New subject";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject);

	const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline, &initialPaulineStats, confAddr, initialSubject, 2, 0);

	// Check that the chat room is correctly created on Laure's side and that the participants are added
	LinphoneChatRoom *laureCr = check_creation_chat_room_client_side(coresList, laure, &initialLaureStats, confAddr, initialSubject, 2, 0);

	// Marie now changes the subject
	linphone_chat_room_set_subject(paulineCr, newSubject);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_subject_changed, initialMarieStats.number_of_subject_changed, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_subject_changed, initialPaulineStats.number_of_subject_changed, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_subject_changed, initialLaureStats.number_of_subject_changed, 10000));
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(marieCr), initialSubject);
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(paulineCr), initialSubject);
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(laureCr), initialSubject);

	wait_for_list(coresList, &dummy, 1, 1000);
	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
}


static void group_chat_room_remove_participant (void) {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_new("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_new("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	int dummy = 0;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialLaureStats = laure->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject);

	const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	check_creation_chat_room_client_side(coresList, pauline, &initialPaulineStats, confAddr, initialSubject, 2, 0);

	// Check that the chat room is correctly created on Laure's side and that the participants are added
	check_creation_chat_room_client_side(coresList, laure, &initialLaureStats, confAddr, initialSubject, 2, 0);

	// Marie removes Laure from the chat room
	LinphoneAddress *laureAddr = linphone_address_new(linphone_core_get_identity(laure->lc));
	LinphoneParticipant *laureParticipant = linphone_chat_room_find_participant(marieCr, laureAddr);
	linphone_address_unref(laureAddr);
	BC_ASSERT_PTR_NOT_NULL(laureParticipant);
	linphone_chat_room_remove_participant(marieCr, laureParticipant);
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_LinphoneChatRoomStateTerminated, initialLaureStats.number_of_LinphoneChatRoomStateTerminated + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_participants_removed, initialMarieStats.number_of_participants_removed + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_participants_removed, initialPaulineStats.number_of_participants_removed + 1, 1000));

	wait_for_list(coresList, &dummy, 1, 1000);
	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
}

static void group_chat_room_leave (void) {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_new("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_new("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	int dummy = 0;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialLaureStats = laure->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject);

	const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline, &initialPaulineStats, confAddr, initialSubject, 2, 0);

	// Check that the chat room is correctly created on Laure's side and that the participants are added
	check_creation_chat_room_client_side(coresList, laure, &initialLaureStats, confAddr, initialSubject, 2, 0);

	linphone_chat_room_leave(paulineCr);
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneChatRoomStateTerminated, initialPaulineStats.number_of_LinphoneChatRoomStateTerminated + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_participants_removed, initialMarieStats.number_of_participants_removed + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_participants_removed, initialLaureStats.number_of_participants_removed + 1, 1000));

	wait_for_list(coresList, &dummy, 1, 1000);
	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
}

static void group_chat_room_come_back_after_disconnection (void) {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_new("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_new("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	int dummy = 0;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialLaureStats = laure->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	const char *newSubject = "New subject";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject);

	const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline, &initialPaulineStats, confAddr, initialSubject, 2, 0);

	// Check that the chat room is correctly created on Laure's side and that the participants are added
	LinphoneChatRoom *laureCr = check_creation_chat_room_client_side(coresList, laure, &initialLaureStats, confAddr, initialSubject, 2, 0);

	linphone_core_set_network_reachable(marie->lc, FALSE);

	wait_for_list(coresList, &dummy, 1, 1000);

	linphone_core_set_network_reachable(marie->lc, TRUE);

	wait_for_list(coresList, &dummy, 1, 1000);

	// Marie now changes the subject
	linphone_chat_room_set_subject(marieCr, newSubject);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_subject_changed, initialMarieStats.number_of_subject_changed + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_subject_changed, initialPaulineStats.number_of_subject_changed + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_subject_changed, initialLaureStats.number_of_subject_changed + 1, 10000));
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(marieCr), newSubject);
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(paulineCr), newSubject);
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(laureCr), newSubject);

	wait_for_list(coresList, &dummy, 1, 1000);
	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
}

static void group_chat_room_create_room_with_disconnected_friends (void) {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_new("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_new("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	int dummy = 0;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialLaureStats = laure->stat;

	// Disconnect pauline and laure
	linphone_core_set_network_reachable(pauline->lc, FALSE);
	linphone_core_set_network_reachable(laure->lc, FALSE);

	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);

	wait_for_list(coresList, &dummy, 1, 3000);

	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject);

	const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

	wait_for_list(coresList, &dummy, 1, 4000);

	// Reconnect pauline and laure
	linphone_core_set_network_reachable(pauline->lc, TRUE);
	linphone_core_set_network_reachable(laure->lc, TRUE);

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	/*LinphoneChatRoom *paulineCr = */check_creation_chat_room_client_side(coresList, pauline, &initialPaulineStats, confAddr, initialSubject, 2, 0);

	// Check that the chat room is correctly created on Laure's side and that the participants are added
	/*LinphoneChatRoom *laureCr = */check_creation_chat_room_client_side(coresList, laure, &initialLaureStats, confAddr, initialSubject, 2, 0);

	wait_for_list(coresList, &dummy, 1, 1000);
	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
}

static void group_chat_room_reinvited_after_removed (void) {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_new("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_new("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	int dummy = 0;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialLaureStats = laure->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject);
	participantsAddresses = NULL;
	const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline, &initialPaulineStats, confAddr, initialSubject, 2, 0);

	// Check that the chat room is correctly created on Laure's side and that the participants are added
	LinphoneChatRoom *laureCr = check_creation_chat_room_client_side(coresList, laure, &initialLaureStats, confAddr, initialSubject, 2, 0);

	// Marie removes Laure from the chat room
	LinphoneAddress *laureAddr = linphone_address_new(linphone_core_get_identity(laure->lc));
	LinphoneParticipant *laureParticipant = linphone_chat_room_find_participant(marieCr, laureAddr);
	linphone_address_unref(laureAddr);
	BC_ASSERT_PTR_NOT_NULL(laureParticipant);
	linphone_chat_room_remove_participant(marieCr, laureParticipant);
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_LinphoneChatRoomStateTerminated, initialLaureStats.number_of_LinphoneChatRoomStateTerminated + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_participants_removed, initialMarieStats.number_of_participants_removed + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_participants_removed, initialPaulineStats.number_of_participants_removed + 1, 1000));

	// Marie adds Laure to the chat room
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	linphone_chat_room_add_participants(marieCr, participantsAddresses);
	bctbx_list_free_with_data(participantsAddresses, (bctbx_list_free_func)linphone_address_unref);
	participantsAddresses = NULL;
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_LinphoneChatRoomStateCreationPending, initialLaureStats.number_of_LinphoneChatRoomStateCreationPending + 2, 5000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_LinphoneChatRoomStateCreated, initialLaureStats.number_of_LinphoneChatRoomStateCreated + 2, 5000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_participants_added, initialMarieStats.number_of_participants_added + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_participants_added, initialPaulineStats.number_of_participants_added + 1, 1000));
	BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(marieCr), 2, int, "%d");
	BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(paulineCr), 2, int, "%d");
	laureAddr = linphone_address_new(linphone_core_get_device_identity(laure->lc));
	LinphoneChatRoom *newLaureCr = linphone_core_find_chat_room(laure->lc, confAddr, laureAddr);
	linphone_address_unref(laureAddr);
	BC_ASSERT_PTR_EQUAL(newLaureCr, laureCr);
	BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(newLaureCr), 2, int, "%d");
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(newLaureCr), initialSubject);

	wait_for_list(coresList, &dummy, 1, 1000);
	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
}

<<<<<<< HEAD
static void group_chat_room_notify_after_disconnection (void) {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_new("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_new("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	int dummy = 0;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialLaureStats = laure->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject);
	participantsAddresses = NULL;
	const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline, &initialPaulineStats, confAddr, initialSubject, 2);

	// Check that the chat room is correctly created on Laure's side and that the participants are added
	LinphoneChatRoom *laureCr = check_creation_chat_room_client_side(coresList, laure, &initialLaureStats, confAddr, initialSubject, 2);

	// Marie now changes the subject
	const char *newSubject = "New subject";
	linphone_chat_room_set_subject(marieCr, newSubject);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_subject_changed, initialMarieStats.number_of_subject_changed + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_subject_changed, initialPaulineStats.number_of_subject_changed + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_subject_changed, initialLaureStats.number_of_subject_changed + 1, 10000));
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(marieCr), newSubject);
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(paulineCr), newSubject);
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(laureCr), newSubject);

	linphone_core_set_network_reachable(pauline->lc, FALSE);
	wait_for_list(coresList, &dummy, 1, 1000);

	// Marie now changes the subject
	newSubject = "Let's go drink a beer";
	linphone_chat_room_set_subject(marieCr, newSubject);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_subject_changed, initialMarieStats.number_of_subject_changed + 2, 10000));
	BC_ASSERT_FALSE(wait_for_list(coresList, &pauline->stat.number_of_subject_changed, initialPaulineStats.number_of_subject_changed + 2, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_subject_changed, initialLaureStats.number_of_subject_changed + 2, 10000));
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(marieCr), newSubject);
	BC_ASSERT_STRING_NOT_EQUAL(linphone_chat_room_get_subject(paulineCr), newSubject);
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(laureCr), newSubject);

	linphone_core_set_network_reachable(pauline->lc, TRUE);
	wait_for_list(coresList, &dummy, 1, 1000);

	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_subject_changed, initialPaulineStats.number_of_subject_changed + 2, 10000));

	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(paulineCr), newSubject);

	// Test with more than one missed notify
	linphone_core_set_network_reachable(pauline->lc, FALSE);
	wait_for_list(coresList, &dummy, 1, 1000);

	// Marie now changes the subject
	newSubject = "Let's go drink a mineral water !";
	linphone_chat_room_set_subject(marieCr, newSubject);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_subject_changed, initialMarieStats.number_of_subject_changed + 3, 10000));
	BC_ASSERT_FALSE(wait_for_list(coresList, &pauline->stat.number_of_subject_changed, initialPaulineStats.number_of_subject_changed + 3, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_subject_changed, initialLaureStats.number_of_subject_changed + 3, 10000));
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(marieCr), newSubject);
	BC_ASSERT_STRING_NOT_EQUAL(linphone_chat_room_get_subject(paulineCr), newSubject);
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(laureCr), newSubject);

	// Marie designates Laure as admin
	LinphoneAddress *laureAddr = linphone_address_new(linphone_core_get_identity(laure->lc));
	LinphoneParticipant *laureParticipant = linphone_chat_room_find_participant(marieCr, laureAddr);
	LinphoneParticipant *laureParticipantFromPauline = linphone_chat_room_find_participant(paulineCr, laureAddr);
	linphone_address_unref(laureAddr);
	BC_ASSERT_PTR_NOT_NULL(laureParticipant);
	BC_ASSERT_PTR_NOT_NULL(laureParticipantFromPauline);
	linphone_chat_room_set_participant_admin_status(marieCr, laureParticipant, TRUE);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_participant_admin_statuses_changed, initialMarieStats.number_of_participant_admin_statuses_changed + 1, 1000));
	BC_ASSERT_FALSE(wait_for_list(coresList, &pauline->stat.number_of_participant_admin_statuses_changed, initialPaulineStats.number_of_participant_admin_statuses_changed + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_participant_admin_statuses_changed, initialLaureStats.number_of_participant_admin_statuses_changed + 1, 1000));
	BC_ASSERT_TRUE(linphone_participant_is_admin(laureParticipant));
	BC_ASSERT_FALSE(linphone_participant_is_admin(laureParticipantFromPauline));

	linphone_core_set_network_reachable(pauline->lc, TRUE);
	wait_for_list(coresList, &dummy, 1, 1000);

	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_subject_changed, initialPaulineStats.number_of_subject_changed + 3, 10000));
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(paulineCr), newSubject);
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_participant_admin_statuses_changed, initialPaulineStats.number_of_participant_admin_statuses_changed + 1, 1000));
	BC_ASSERT_TRUE(linphone_participant_is_admin(laureParticipantFromPauline));

	wait_for_list(coresList, &dummy, 1, 1000);
	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
}
=======
    static void group_chat_room_send_refer_to_all_devices (void) {
        LinphoneCoreManager *marie1 = linphone_core_manager_new("marie_rc");
        LinphoneCoreManager *marie2 = linphone_core_manager_new("marie_rc");
        LinphoneCoreManager *pauline1 = linphone_core_manager_new("pauline_rc");
        LinphoneCoreManager *pauline2 = linphone_core_manager_new("pauline_rc");
        LinphoneCoreManager *laure1 = linphone_core_manager_new("laure_tcp_rc");
        //LinphoneCoreManager *laure2 = linphone_core_manager_new("laure_tcp_rc");
        bctbx_list_t *coresManagerList = NULL;
        bctbx_list_t *participantsAddresses = NULL;
        int dummy = 0;
        coresManagerList = bctbx_list_append(coresManagerList, marie1);
        coresManagerList = bctbx_list_append(coresManagerList, marie2);
        coresManagerList = bctbx_list_append(coresManagerList, pauline1);
        coresManagerList = bctbx_list_append(coresManagerList, pauline2);
        coresManagerList = bctbx_list_append(coresManagerList, laure1);
        //coresManagerList = bctbx_list_append(coresManagerList, laure2);
        bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
        participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline1->lc)));
        participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure1->lc)));
        stats initialMarie1Stats = marie1->stat;
        stats initialMarie2Stats = marie2->stat;
        stats initialPauline1Stats = pauline1->stat;
        stats initialPauline2Stats = pauline2->stat;
        stats initialLaure1Stats = laure1->stat;
        //stats initialLaure2Stats = laure2->stat;

        // Marie creates a new group chat room
        const char *initialSubject = "Colleagues";
        LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie1, &initialMarie1Stats, participantsAddresses, initialSubject);
        participantsAddresses = NULL;
        const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

        /*LinphoneChatRoom *marie2Cr= */check_creation_chat_room_client_side(coresList, marie2, &initialMarie2Stats, confAddr, initialSubject, 2, 1);
        // Check that the chat room is correctly created on Pauline's side and that the participants are added
        LinphoneChatRoom *pauline1Cr = check_creation_chat_room_client_side(coresList, pauline1, &initialPauline1Stats, confAddr, initialSubject, 2, 0);
        LinphoneChatRoom *pauline2Cr = check_creation_chat_room_client_side(coresList, pauline2, &initialPauline2Stats, confAddr, initialSubject, 2, 0);

        // Check that the chat room is correctly created on Laure's side and that the participants are added
        /*LinphoneChatRoom *laure1Cr = */check_creation_chat_room_client_side(coresList, laure1, &initialLaure1Stats, confAddr, initialSubject, 2, 0);
        /*LinphoneChatRoom *laure2Cr = *///check_creation_chat_room_client_side(coresList, laure2, &initialLaure2Stats, confAddr, initialSubject, 2);

        // Marie removes Laure from the chat room
        LinphoneAddress *laureAddr = linphone_address_new(linphone_core_get_identity(laure1->lc));
        LinphoneParticipant *laureParticipant = linphone_chat_room_find_participant(marieCr, laureAddr);
        linphone_address_unref(laureAddr);
        BC_ASSERT_PTR_NOT_NULL(laureParticipant);
        linphone_chat_room_remove_participant(marieCr, laureParticipant);
        BC_ASSERT_TRUE(wait_for_list(coresList, &laure1->stat.number_of_LinphoneChatRoomStateTerminated, initialLaure1Stats.number_of_LinphoneChatRoomStateTerminated + 1, 1000));
        //BC_ASSERT_TRUE(wait_for_list(coresList, &laure2->stat.number_of_LinphoneChatRoomStateTerminated, initialLaure2Stats.number_of_LinphoneChatRoomStateTerminated + 1, 1000));
        BC_ASSERT_TRUE(wait_for_list(coresList, &marie1->stat.number_of_participants_removed, initialMarie1Stats.number_of_participants_removed + 1, 1000));
        BC_ASSERT_TRUE(wait_for_list(coresList, &marie2->stat.number_of_participants_removed, initialMarie2Stats.number_of_participants_removed + 1, 1000));
        BC_ASSERT_TRUE(wait_for_list(coresList, &pauline1->stat.number_of_participants_removed, initialPauline1Stats.number_of_participants_removed + 1, 1000));
        BC_ASSERT_TRUE(wait_for_list(coresList, &pauline2->stat.number_of_participants_removed, initialPauline2Stats.number_of_participants_removed + 1, 1000));

        BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(marieCr), 1, int, "%d");
        BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(pauline1Cr), 1, int, "%d");
        BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(pauline2Cr), 1, int, "%d");

        wait_for_list(coresList, &dummy, 1, 1000);
        bctbx_list_free(coresList);
        bctbx_list_free(coresManagerList);
        linphone_core_manager_destroy(marie1);
        linphone_core_manager_destroy(marie2);
        linphone_core_manager_destroy(pauline1);
        linphone_core_manager_destroy(pauline2);
        linphone_core_manager_destroy(laure1);
        //linphone_core_manager_destroy(laure2);
    }
>>>>>>> Adding fetching of all participant devices

test_t group_chat_tests[] = {
	TEST_TWO_TAGS("Group chat room creation local", group_chat_room_creation_local, "Local", "LeaksMemory"),
	TEST_TWO_TAGS("Group chat room creation server", group_chat_room_creation_server, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Send message", group_chat_room_send_message, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Send invite on a multi register account", group_chat_room_invite_multi_register_account, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Add admin", group_chat_room_add_admin, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Add admin with a non admin", group_chat_room_add_admin_non_admin, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Remove admin", group_chat_room_remove_admin, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Change subject", group_chat_room_change_subject, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Change subject with a non admin", group_chat_room_change_subject_non_admin, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Remove participant", group_chat_room_remove_participant, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Leave group chat room", group_chat_room_leave, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Come back on a group chat room after a disconnection", group_chat_room_come_back_after_disconnection, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Create chat room with disconnected friends", group_chat_room_create_room_with_disconnected_friends, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Reinvited after removed from group chat room", group_chat_room_reinvited_after_removed, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Notify after disconnection", group_chat_room_notify_after_disconnection, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Send refer to all participants devices", group_chat_room_send_refer_to_all_devices, "Server", "LeaksMemory")
};

test_suite_t group_chat_test_suite = {
	"Group Chat",
	NULL,
	NULL,
	liblinphone_tester_before_each,
	liblinphone_tester_after_each,
	sizeof(group_chat_tests) / sizeof(group_chat_tests[0]), group_chat_tests
};

#if __clang__ || ((__GNUC__ == 4 && __GNUC_MINOR__ >= 6) || __GNUC__ > 4)
#pragma GCC diagnostic pop
#endif

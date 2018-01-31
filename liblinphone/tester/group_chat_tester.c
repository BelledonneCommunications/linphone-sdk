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
#include "linphone/wrapper_utils.h"
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

static void chat_room_participant_device_added (LinphoneChatRoom *cr, const LinphoneEventLog *event_log) {
	LinphoneCore *core = linphone_chat_room_get_core(cr);
	LinphoneCoreManager *manager = (LinphoneCoreManager *)linphone_core_get_user_data(core);
	manager->stat.number_of_participant_devices_added++;
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
		case LinphoneChatRoomStateCreationFailed:
			manager->stat.number_of_LinphoneChatRoomStateCreationFailed++;
			break;
		case LinphoneChatRoomStateTerminationPending:
			manager->stat.number_of_LinphoneChatRoomStateTerminationPending++;
			break;
		case LinphoneChatRoomStateTerminated:
			manager->stat.number_of_LinphoneChatRoomStateTerminated++;
			break;
		case LinphoneChatRoomStateTerminationFailed:
			manager->stat.number_of_LinphoneChatRoomStateTerminationFailed++;
			break;
		case LinphoneChatRoomStateDeleted:
			manager->stat.number_of_LinphoneChatRoomStateDeleted++;
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
		linphone_chat_room_cbs_set_participant_device_added(cbs, chat_room_participant_device_added);
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
	linphone_core_set_linphone_specs(core, "groupchat");
}

static void _configure_core_for_conference (LinphoneCoreManager *lcm, LinphoneAddress *factoryAddr) {
	configure_core_for_conference(lcm->lc, NULL, factoryAddr, FALSE);
}

static void _configure_core_for_callbacks(LinphoneCoreManager *lcm, LinphoneCoreCbs *cbs) {
	linphone_core_add_callbacks(lcm->lc, cbs);
	linphone_core_set_user_data(lcm->lc, lcm);
}

static void _start_core(LinphoneCoreManager *lcm) {
	linphone_core_manager_start(lcm, TRUE);
}

static void _send_message(LinphoneChatRoom *chatRoom, const char *message) {
	LinphoneChatMessage *msg = linphone_chat_room_create_message(chatRoom, message);
	LinphoneChatMessageCbs *msgCbs = linphone_chat_message_get_callbacks(msg);
	linphone_chat_message_cbs_set_msg_state_changed(msgCbs, liblinphone_tester_chat_message_msg_state_changed);
	linphone_chat_message_send(msg);
}

static void _send_file(LinphoneChatRoom* cr, const char *sendFilepath) {
	LinphoneChatMessage *msg;
	LinphoneChatMessageCbs *cbs;
	LinphoneContent *content = linphone_core_create_content(linphone_chat_room_get_core(cr));
	belle_sip_object_set_name(BELLE_SIP_OBJECT(content), "sintel trailer content");
	linphone_content_set_type(content,"video");
	linphone_content_set_subtype(content,"mkv");
	linphone_content_set_name(content,"sintel_trailer_opus_h264.mkv");

	msg = linphone_chat_room_create_file_transfer_message(cr, content);
	linphone_chat_message_set_file_transfer_filepath(msg, sendFilepath);
	cbs = linphone_chat_message_get_callbacks(msg);
	linphone_chat_message_cbs_set_file_transfer_send(cbs, tester_file_transfer_send);
	linphone_chat_message_cbs_set_msg_state_changed(cbs,liblinphone_tester_chat_message_msg_state_changed);
	linphone_chat_message_cbs_set_file_transfer_progress_indication(cbs, file_transfer_progress_indication);
	linphone_chat_room_send_chat_message_2(cr, msg);
	linphone_content_unref(content);
}

static void _send_file_plus_text(LinphoneChatRoom* cr, const char *sendFilepath, const char *text) {
	LinphoneChatMessage *msg;
	LinphoneChatMessageCbs *cbs;
	LinphoneContent *content = linphone_core_create_content(linphone_chat_room_get_core(cr));
	belle_sip_object_set_name(BELLE_SIP_OBJECT(content), "sintel trailer content");
	linphone_content_set_type(content,"video");
	linphone_content_set_subtype(content,"mkv");
	linphone_content_set_name(content,"sintel_trailer_opus_h264.mkv");

	msg = linphone_chat_room_create_file_transfer_message(cr, content);
	linphone_chat_message_set_file_transfer_filepath(msg, sendFilepath);
	cbs = linphone_chat_message_get_callbacks(msg);
	linphone_chat_message_cbs_set_file_transfer_send(cbs, tester_file_transfer_send);
	linphone_chat_message_cbs_set_msg_state_changed(cbs,liblinphone_tester_chat_message_msg_state_changed);
	linphone_chat_message_cbs_set_file_transfer_progress_indication(cbs, file_transfer_progress_indication);
	linphone_chat_room_send_chat_message_2(cr, msg);
	linphone_content_unref(content);
}

static void _receive_file(bctbx_list_t *coresList, LinphoneCoreManager *lcm, stats *receiverStats, const char *receive_filepath, const char *sendFilepath) {
	if (BC_ASSERT_TRUE(wait_for_list(coresList, &lcm->stat.number_of_LinphoneMessageReceivedWithFile, receiverStats->number_of_LinphoneMessageReceivedWithFile+1, 10000))) {
		LinphoneChatMessageCbs *cbs;
		LinphoneChatMessage *msg = lcm->stat.last_received_chat_message;

		cbs = linphone_chat_message_get_callbacks(msg);
		linphone_chat_message_cbs_set_msg_state_changed(cbs, liblinphone_tester_chat_message_msg_state_changed);
		linphone_chat_message_cbs_set_file_transfer_recv(cbs, file_transfer_received);
		linphone_chat_message_cbs_set_file_transfer_progress_indication(cbs, file_transfer_progress_indication);
		linphone_chat_message_set_file_transfer_filepath(msg, receive_filepath);
		linphone_chat_message_download_file(msg);

		if (BC_ASSERT_TRUE(wait_for_list(coresList, &lcm->stat.number_of_LinphoneFileTransferDownloadSuccessful,receiverStats->number_of_LinphoneFileTransferDownloadSuccessful + 1, 20000))) {
			compare_files(sendFilepath, receive_filepath);
		}
	}
}

static void _receive_file_plus_text(bctbx_list_t *coresList, LinphoneCoreManager *lcm, stats *receiverStats, const char *receive_filepath, const char *sendFilepath, const char *text) {
	if (BC_ASSERT_TRUE(wait_for_list(coresList, &lcm->stat.number_of_LinphoneMessageReceivedWithFile, receiverStats->number_of_LinphoneMessageReceivedWithFile+1, 10000))) {
		LinphoneChatMessageCbs *cbs;
		LinphoneChatMessage *msg = lcm->stat.last_received_chat_message;

		BC_ASSERT_TRUE(linphone_chat_message_has_text_content(msg));
		BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_text_content(msg), text);

		cbs = linphone_chat_message_get_callbacks(msg);
		linphone_chat_message_cbs_set_msg_state_changed(cbs, liblinphone_tester_chat_message_msg_state_changed);
		linphone_chat_message_cbs_set_file_transfer_recv(cbs, file_transfer_received);
		linphone_chat_message_cbs_set_file_transfer_progress_indication(cbs, file_transfer_progress_indication);
		linphone_chat_message_set_file_transfer_filepath(msg, receive_filepath);
		linphone_chat_message_download_file(msg);

		if (BC_ASSERT_TRUE(wait_for_list(coresList, &lcm->stat.number_of_LinphoneFileTransferDownloadSuccessful,receiverStats->number_of_LinphoneFileTransferDownloadSuccessful + 1, 20000))) {
			compare_files(sendFilepath, receive_filepath);
		}
	}
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

static void start_core_for_conference(bctbx_list_t *coreManagerList) {
	bctbx_list_for_each(coreManagerList, (void (*)(void *))_start_core);
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

static LinphoneChatRoom * create_chat_room_client_side(bctbx_list_t *lcs, LinphoneCoreManager *lcm, stats *initialStats, bctbx_list_t *participantsAddresses, const char* initialSubject, int expectedParticipantSize) {
	LinphoneChatRoom *chatRoom = linphone_core_create_client_group_chat_room(lcm->lc, initialSubject);
	if (!chatRoom) return NULL;

	BC_ASSERT_TRUE(wait_for_list(lcs, &lcm->stat.number_of_LinphoneChatRoomStateInstantiated, initialStats->number_of_LinphoneChatRoomStateInstantiated + 1, 100));

	// Add participants
	linphone_chat_room_add_participants(chatRoom, participantsAddresses);

	// Check that the chat room is correctly created on Marie's side and that the participants are added
	BC_ASSERT_TRUE(wait_for_list(lcs, &lcm->stat.number_of_LinphoneChatRoomStateCreationPending, initialStats->number_of_LinphoneChatRoomStateCreationPending + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(lcs, &lcm->stat.number_of_LinphoneChatRoomStateCreated, initialStats->number_of_LinphoneChatRoomStateCreated + 1, 10000));
	BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(chatRoom),
		(expectedParticipantSize >= 0) ? expectedParticipantSize : (int)bctbx_list_size(participantsAddresses),
		int, "%d");
	LinphoneParticipant *participant = linphone_chat_room_get_me(chatRoom);
	BC_ASSERT_PTR_NOT_NULL(participant);
	if (participant)
		BC_ASSERT_TRUE(linphone_participant_is_admin(participant));
	BC_ASSERT_STRING_EQUAL(linphone_chat_room_get_subject(chatRoom), initialSubject);

	bctbx_list_free_with_data(participantsAddresses, (bctbx_list_free_func)linphone_address_unref);
	participantsAddresses = NULL;

	return chatRoom;
}

static void group_chat_room_creation_server (void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_create("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_create("laure_tcp_rc");
	LinphoneCoreManager *chloe = linphone_core_manager_create("chloe_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	int dummy = 0;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	coresManagerList = bctbx_list_append(coresManagerList, chloe);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	start_core_for_conference(coresManagerList);

	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialLaureStats = laure->stat;
	stats initialChloeStats = chloe->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject, -1);
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
	_send_message(chloeCr, chloeMessage);
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
	initialPaulineStats = pauline->stat;
	participantsToRemove = bctbx_list_append(participantsToRemove, marieParticipant);
	participantsToRemove = bctbx_list_append(participantsToRemove, chloeParticipant);
	linphone_chat_room_remove_participants(paulineCr, participantsToRemove);
	bctbx_list_free_with_data(participantsToRemove, (bctbx_list_free_func)linphone_participant_unref);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneChatRoomStateTerminated, initialMarieStats.number_of_LinphoneChatRoomStateTerminated + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &chloe->stat.number_of_LinphoneChatRoomStateTerminated, initialChloeStats.number_of_LinphoneChatRoomStateTerminated + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_participants_removed, initialPaulineStats.number_of_participants_removed + 2, 1000));
	BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(paulineCr), 0, int, "%d");

	// Pauline leaves the chat room
	wait_for_list(coresList, &dummy, 1, 1000);
	linphone_chat_room_leave(paulineCr);
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneChatRoomStateTerminationPending, initialPaulineStats.number_of_LinphoneChatRoomStateTerminationPending + 1, 100));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneChatRoomStateTerminated, initialPaulineStats.number_of_LinphoneChatRoomStateTerminated + 1, 10000));

	// Clean db from chat room
	linphone_core_manager_delete_chat_room(marie, marieCr, coresList);
	linphone_core_manager_delete_chat_room(laure, laureCr, coresList);
	linphone_core_manager_delete_chat_room(pauline, paulineCr, coresList);
	linphone_core_manager_delete_chat_room(chloe, chloeCr, coresList);

	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
	linphone_core_manager_destroy(chloe);
}

static void group_chat_room_send_message (void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_create("pauline_rc");
	LinphoneCoreManager *chloe = linphone_core_manager_create("chloe_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, chloe);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	start_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(chloe->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialChloeStats = chloe->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject, -1);
	const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline, &initialPaulineStats, confAddr, initialSubject, 2, 0);

	// Check that the chat room is correctly created on Chloe's side and that the participants are added
	LinphoneChatRoom *chloeCr = check_creation_chat_room_client_side(coresList, chloe, &initialChloeStats, confAddr, initialSubject, 2, 0);

	// Chloe begins composing a message
	linphone_chat_room_compose(chloeCr);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneIsComposingActiveReceived, initialMarieStats.number_of_LinphoneIsComposingActiveReceived + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneIsComposingActiveReceived, initialPaulineStats.number_of_LinphoneIsComposingActiveReceived + 1, 10000));
	const char *chloeMessage = "Hello";
	_send_message(chloeCr, chloeMessage);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneMessageReceived, initialMarieStats.number_of_LinphoneMessageReceived + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneMessageReceived, initialPaulineStats.number_of_LinphoneMessageReceived + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneIsComposingIdleReceived, initialMarieStats.number_of_LinphoneIsComposingIdleReceived + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneIsComposingIdleReceived, initialPaulineStats.number_of_LinphoneIsComposingIdleReceived + 1, 10000));
	BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_text(marie->stat.last_received_chat_message), chloeMessage);
	LinphoneAddress *chloeAddr = linphone_address_new(linphone_core_get_identity(chloe->lc));
	BC_ASSERT_TRUE(linphone_address_weak_equal(chloeAddr, linphone_chat_message_get_from_address(marie->stat.last_received_chat_message)));
	linphone_address_unref(chloeAddr);

	// Pauline begins composing a messagewith some accents
	linphone_chat_room_compose(paulineCr);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneIsComposingActiveReceived, initialMarieStats.number_of_LinphoneIsComposingActiveReceived + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &chloe->stat.number_of_LinphoneIsComposingActiveReceived, initialChloeStats.number_of_LinphoneIsComposingActiveReceived + 1, 10000));
	const char *paulineMessage = "Héllö Dàrling";
	_send_message(paulineCr, paulineMessage);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneMessageReceived, initialMarieStats.number_of_LinphoneMessageReceived + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &chloe->stat.number_of_LinphoneMessageReceived, initialChloeStats.number_of_LinphoneMessageReceived + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneIsComposingIdleReceived, initialMarieStats.number_of_LinphoneIsComposingIdleReceived + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &chloe->stat.number_of_LinphoneIsComposingIdleReceived, initialChloeStats.number_of_LinphoneIsComposingIdleReceived + 1, 10000));
	BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_text(marie->stat.last_received_chat_message), paulineMessage);
	LinphoneAddress *paulineAddr = linphone_address_new(linphone_core_get_identity(pauline->lc));
	BC_ASSERT_TRUE(linphone_address_weak_equal(paulineAddr, linphone_chat_message_get_from_address(marie->stat.last_received_chat_message)));
	linphone_address_unref(paulineAddr);

	// Clean db from chat room
	linphone_core_manager_delete_chat_room(marie, marieCr, coresList);
	linphone_core_manager_delete_chat_room(chloe, chloeCr, coresList);
	linphone_core_manager_delete_chat_room(pauline, paulineCr, coresList);

	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(chloe);
}

static void group_chat_room_invite_multi_register_account (void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline1 = linphone_core_manager_create("pauline_rc");
	LinphoneCoreManager *pauline2 = linphone_core_manager_create("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_create("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline1);
	coresManagerList = bctbx_list_append(coresManagerList, pauline2);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	start_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline1->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPauline1Stats = pauline1->stat;
	stats initialPauline2Stats = pauline2->stat;
	stats initialLaureStats = laure->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject, -1);

	const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

	// Check that the chat room is correctly created on Pauline1's side and that the participants are added
	LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline1, &initialPauline1Stats, confAddr, initialSubject, 2, 0);

	// Check that the chat room is correctly created on Pauline2's side and that the participants are added
	LinphoneChatRoom *paulineCr2 = check_creation_chat_room_client_side(coresList, pauline2, &initialPauline2Stats, confAddr, initialSubject, 2, 0);

	// Check that the chat room is correctly created on Laure's side and that the participants are added
	LinphoneChatRoom *laureCr = check_creation_chat_room_client_side(coresList, laure, &initialLaureStats, confAddr, initialSubject, 2, 0);

	// Clean db from chat room
	linphone_core_manager_delete_chat_room(marie, marieCr, coresList);
	linphone_core_manager_delete_chat_room(laure, laureCr, coresList);
	linphone_core_manager_delete_chat_room(pauline1, paulineCr, coresList);
	linphone_core_delete_chat_room(pauline2->lc, paulineCr2);

	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline1);
	linphone_core_manager_destroy(pauline2);
	linphone_core_manager_destroy(laure);
}

static void group_chat_room_add_admin (void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_create("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_create("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	start_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialLaureStats = laure->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject, -1);

	const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline, &initialPaulineStats, confAddr, initialSubject, 2, 0);

	// Check that the chat room is correctly created on Laure's side and that the participants are added
	LinphoneChatRoom *laureCr = check_creation_chat_room_client_side(coresList, laure, &initialLaureStats, confAddr, initialSubject, 2, 0);

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

	// Clean db from chat room
	linphone_core_manager_delete_chat_room(marie, marieCr, coresList);
	linphone_core_manager_delete_chat_room(laure, laureCr, coresList);
	linphone_core_manager_delete_chat_room(pauline, paulineCr, coresList);

	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
}

static void group_chat_room_add_admin_lately_notified (void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_create("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_create("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	start_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialLaureStats = laure->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject, -1);

	const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline, &initialPaulineStats, confAddr, initialSubject, 2, 0);

	// Check that the chat room is correctly created on Laure's side and that the participants are added
	LinphoneChatRoom *laureCr = check_creation_chat_room_client_side(coresList, laure, &initialLaureStats, confAddr, initialSubject, 2, 0);

	//simulate pauline has disapeared
	linphone_core_set_network_reachable(pauline->lc, FALSE);

	// Marie designates Pauline as admin
	LinphoneAddress *paulineAddr = linphone_address_new(linphone_core_get_identity(pauline->lc));
	LinphoneParticipant *paulineParticipant = linphone_chat_room_find_participant(marieCr, paulineAddr);
	linphone_address_unref(paulineAddr);
	BC_ASSERT_PTR_NOT_NULL(paulineParticipant);
	linphone_chat_room_set_participant_admin_status(marieCr, paulineParticipant, TRUE);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_participant_admin_statuses_changed, initialMarieStats.number_of_participant_admin_statuses_changed + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_participant_admin_statuses_changed, initialLaureStats.number_of_participant_admin_statuses_changed + 1, 1000));

	//make sure pauline is not notified
	BC_ASSERT_FALSE(wait_for_list(coresList, &pauline->stat.number_of_participant_admin_statuses_changed, initialPaulineStats.number_of_participant_admin_statuses_changed + 1, 1000));
	linphone_core_set_network_reachable(pauline->lc, TRUE);
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_participant_admin_statuses_changed, initialPaulineStats.number_of_participant_admin_statuses_changed + 1, 3000));
	BC_ASSERT_TRUE(linphone_participant_is_admin(paulineParticipant));

	// Clean db from chat room
	linphone_core_manager_delete_chat_room(marie, marieCr, coresList);
	linphone_core_manager_delete_chat_room(laure, laureCr, coresList);
	linphone_core_manager_delete_chat_room(pauline, paulineCr, coresList);

	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
}



static void group_chat_room_add_admin_non_admin (void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_create("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_create("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	start_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialLaureStats = laure->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject, -1);

	const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline, &initialPaulineStats, confAddr, initialSubject, 2, 0);

	// Check that the chat room is correctly created on Laure's side and that the participants are added
	LinphoneChatRoom *laureCr = check_creation_chat_room_client_side(coresList, laure, &initialLaureStats, confAddr, initialSubject, 2, 0);

	// Pauline designates Laure as admin
	LinphoneAddress *laureAddr = linphone_address_new(linphone_core_get_identity(laure->lc));
	LinphoneParticipant *laureParticipant = linphone_chat_room_find_participant(paulineCr, laureAddr);
	linphone_address_unref(laureAddr);
	BC_ASSERT_PTR_NOT_NULL(laureParticipant);
	linphone_chat_room_set_participant_admin_status(paulineCr, laureParticipant, TRUE);
	BC_ASSERT_FALSE(linphone_participant_is_admin(laureParticipant));

	// Clean db from chat room
	linphone_core_manager_delete_chat_room(marie, marieCr, coresList);
	linphone_core_manager_delete_chat_room(laure, laureCr, coresList);
	linphone_core_manager_delete_chat_room(pauline, paulineCr, coresList);

	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
}

static void group_chat_room_remove_admin (void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_create("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_create("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	start_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialLaureStats = laure->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject, -1);

	const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline, &initialPaulineStats, confAddr, initialSubject, 2, 0);

	// Check that the chat room is correctly created on Laure's side and that the participants are added
	LinphoneChatRoom *laureCr = check_creation_chat_room_client_side(coresList, laure, &initialLaureStats, confAddr, initialSubject, 2, 0);

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

	// Clean db from chat room
	linphone_core_manager_delete_chat_room(marie, marieCr, coresList);
	linphone_core_manager_delete_chat_room(laure, laureCr, coresList);
	linphone_core_manager_delete_chat_room(pauline, paulineCr, coresList);

	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
}


static void group_chat_room_change_subject (void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_create("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_create("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	start_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialLaureStats = laure->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	const char *newSubject = "New subject";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject, -1);

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

	// Clean db from chat room
	linphone_core_manager_delete_chat_room(marie, marieCr, coresList);
	linphone_core_manager_delete_chat_room(laure, laureCr, coresList);
	linphone_core_manager_delete_chat_room(pauline, paulineCr, coresList);

	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
}

static void group_chat_room_change_subject_non_admin (void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_create("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_create("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	start_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialLaureStats = laure->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	const char *newSubject = "New subject";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject, -1);

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

	// Clean db from chat room
	linphone_core_manager_delete_chat_room(marie, marieCr, coresList);
	linphone_core_manager_delete_chat_room(laure, laureCr, coresList);
	linphone_core_manager_delete_chat_room(pauline, paulineCr, coresList);

	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
}

static void group_chat_room_remove_participant (void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_create("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_create("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	start_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialLaureStats = laure->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject, -1);

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

	// Clean db from chat room
	linphone_core_manager_delete_chat_room(marie, marieCr, coresList);
	linphone_core_manager_delete_chat_room(laure, laureCr, coresList);
	linphone_core_manager_delete_chat_room(pauline, paulineCr, coresList);

	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
}

static void group_chat_room_send_message_with_participant_removed (void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_create("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_create("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	start_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialLaureStats = laure->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject, -1);

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

	// Laure try to send a message with the chat room where she was removed
	const char *laureMessage = "Hello";
	_send_message(laureCr, laureMessage);

	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_LinphoneMessageReceived, initialMarieStats.number_of_LinphoneMessageDelivered, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneMessageReceived, initialMarieStats.number_of_LinphoneMessageReceived, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneMessageReceived, initialPaulineStats.number_of_LinphoneMessageReceived, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneIsComposingIdleReceived, initialMarieStats.number_of_LinphoneIsComposingIdleReceived, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneIsComposingIdleReceived, initialPaulineStats.number_of_LinphoneIsComposingIdleReceived, 10000));

	// Clean db from chat room
	linphone_core_manager_delete_chat_room(marie, marieCr, coresList);
	linphone_core_manager_delete_chat_room(laure, laureCr, coresList);
	linphone_core_manager_delete_chat_room(pauline, paulineCr, coresList);

	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
}

static void group_chat_room_leave (void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_create("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_create("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	start_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialLaureStats = laure->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject, -1);

	const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline, &initialPaulineStats, confAddr, initialSubject, 2, 0);

	// Check that the chat room is correctly created on Laure's side and that the participants are added
	LinphoneChatRoom *laureCr = check_creation_chat_room_client_side(coresList, laure, &initialLaureStats, confAddr, initialSubject, 2, 0);

	linphone_chat_room_leave(paulineCr);
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneChatRoomStateTerminated, initialPaulineStats.number_of_LinphoneChatRoomStateTerminated + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_participants_removed, initialMarieStats.number_of_participants_removed + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_participants_removed, initialLaureStats.number_of_participants_removed + 1, 1000));

	// Clean db from chat room
	linphone_core_manager_delete_chat_room(marie, marieCr, coresList);
	linphone_core_manager_delete_chat_room(laure, laureCr, coresList);
	linphone_core_manager_delete_chat_room(pauline, paulineCr, coresList);

	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
}

static void group_chat_room_come_back_after_disconnection (void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_create("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_create("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	int dummy = 0;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	start_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialLaureStats = laure->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	const char *newSubject = "New subject";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject, -1);

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

	// Clean db from chat room
	linphone_core_manager_delete_chat_room(marie, marieCr, coresList);
	linphone_core_manager_delete_chat_room(laure, laureCr, coresList);
	linphone_core_manager_delete_chat_room(pauline, paulineCr, coresList);

	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
}

static void group_chat_room_create_room_with_disconnected_friends (void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_create("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_create("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	int dummy = 0;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialLaureStats = laure->stat;

	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	start_core_for_conference(coresManagerList);

	wait_for_list(coresList, &dummy, 1, 2000);

	// Disconnect pauline and laure
	linphone_core_set_network_reachable(pauline->lc, FALSE);
	linphone_core_set_network_reachable(laure->lc, FALSE);

	wait_for_list(coresList, &dummy, 1, 2000);

	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject, -1);

	const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

	wait_for_list(coresList, &dummy, 1, 4000);

	// Reconnect pauline and laure
	linphone_core_set_network_reachable(pauline->lc, TRUE);
	linphone_core_set_network_reachable(laure->lc, TRUE);

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline, &initialPaulineStats, confAddr, initialSubject, 2, 0);

	// Check that the chat room is correctly created on Laure's side and that the participants are added
	LinphoneChatRoom *laureCr = check_creation_chat_room_client_side(coresList, laure, &initialLaureStats, confAddr, initialSubject, 2, 0);

	// Clean db from chat room
	linphone_core_manager_delete_chat_room(marie, marieCr, coresList);
	linphone_core_manager_delete_chat_room(laure, laureCr, coresList);
	linphone_core_manager_delete_chat_room(pauline, paulineCr, coresList);

	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
}

static void group_chat_room_reinvited_after_removed (void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_create("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_create("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	start_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialLaureStats = laure->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject, -1);
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

	// Clean db from chat room
	linphone_core_manager_delete_chat_room(marie, marieCr, coresList);
	linphone_core_manager_delete_chat_room(laure, laureCr, coresList);
	linphone_core_manager_delete_chat_room(pauline, paulineCr, coresList);

	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
}

static void group_chat_room_notify_after_disconnection (void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_create("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_create("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	int dummy = 0;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	start_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialLaureStats = laure->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject, -1);
	participantsAddresses = NULL;
	const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline, &initialPaulineStats, confAddr, initialSubject, 2, FALSE);

	// Check that the chat room is correctly created on Laure's side and that the participants are added
	LinphoneChatRoom *laureCr = check_creation_chat_room_client_side(coresList, laure, &initialLaureStats, confAddr, initialSubject, 2, FALSE);

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

	// Clean db from chat room
	linphone_core_manager_delete_chat_room(marie, marieCr, coresList);
	linphone_core_manager_delete_chat_room(laure, laureCr, coresList);
	linphone_core_manager_delete_chat_room(pauline, paulineCr, coresList);

	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
}

static void group_chat_room_send_refer_to_all_devices (void) {
	LinphoneCoreManager *marie1 = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *marie2 = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline1 = linphone_core_manager_create("pauline_rc");
	LinphoneCoreManager *pauline2 = linphone_core_manager_create("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_create("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	coresManagerList = bctbx_list_append(coresManagerList, marie1);
	coresManagerList = bctbx_list_append(coresManagerList, marie2);
	coresManagerList = bctbx_list_append(coresManagerList, pauline1);
	coresManagerList = bctbx_list_append(coresManagerList, pauline2);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	start_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline1->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarie1Stats = marie1->stat;
	stats initialMarie2Stats = marie2->stat;
	stats initialPauline1Stats = pauline1->stat;
	stats initialPauline2Stats = pauline2->stat;
	stats initialLaureStats = laure->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie1, &initialMarie1Stats, participantsAddresses, initialSubject, -1);
	participantsAddresses = NULL;
	const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

	// Check that the chat room is correctly created on second Marie's device
	LinphoneChatRoom *marieCr2 = check_creation_chat_room_client_side(coresList, marie2, &initialMarie2Stats, confAddr, initialSubject, 2, 1);

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline1, &initialPauline1Stats, confAddr, initialSubject, 2, 0);
	LinphoneChatRoom *paulineCr2 = check_creation_chat_room_client_side(coresList, pauline2, &initialPauline2Stats, confAddr, initialSubject, 2, 0);

	// Check that the chat room is correctly created on Laure's side and that the participants are added
	LinphoneChatRoom *laureCr = check_creation_chat_room_client_side(coresList, laure, &initialLaureStats, confAddr, initialSubject, 2, 0);

	// Check that added Marie's device didn't change her admin status
	LinphoneAddress *marieAddr = linphone_address_new(linphone_core_get_identity(marie1->lc));
	LinphoneParticipant *marieParticipant = linphone_chat_room_get_me(marieCr);
	BC_ASSERT_PTR_NOT_NULL(marieParticipant);
	BC_ASSERT_TRUE(linphone_participant_is_admin(marieParticipant));
	marieParticipant = linphone_chat_room_get_me(marieCr2);
	BC_ASSERT_PTR_NOT_NULL(marieParticipant);
	BC_ASSERT_TRUE(linphone_participant_is_admin(marieParticipant));
	marieParticipant = linphone_chat_room_find_participant(paulineCr, marieAddr);
	BC_ASSERT_PTR_NOT_NULL(marieParticipant);
	BC_ASSERT_TRUE(linphone_participant_is_admin(marieParticipant));
	marieParticipant = linphone_chat_room_find_participant(paulineCr2, marieAddr);
	BC_ASSERT_PTR_NOT_NULL(marieParticipant);
	BC_ASSERT_TRUE(linphone_participant_is_admin(marieParticipant));
	marieParticipant = linphone_chat_room_find_participant(laureCr, marieAddr);
	BC_ASSERT_PTR_NOT_NULL(marieParticipant);
	BC_ASSERT_TRUE(linphone_participant_is_admin(marieParticipant));
	linphone_address_unref(marieAddr);

	// Marie removes Laure from the chat room
	LinphoneAddress *laureAddr = linphone_address_new(linphone_core_get_identity(laure->lc));
	LinphoneParticipant *laureParticipant = linphone_chat_room_find_participant(marieCr, laureAddr);
	linphone_address_unref(laureAddr);
	BC_ASSERT_PTR_NOT_NULL(laureParticipant);
	linphone_chat_room_remove_participant(marieCr, laureParticipant);
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_LinphoneChatRoomStateTerminated, initialLaureStats.number_of_LinphoneChatRoomStateTerminated + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie1->stat.number_of_participants_removed, initialMarie1Stats.number_of_participants_removed + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie2->stat.number_of_participants_removed, initialMarie2Stats.number_of_participants_removed + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline1->stat.number_of_participants_removed, initialPauline1Stats.number_of_participants_removed + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline2->stat.number_of_participants_removed, initialPauline2Stats.number_of_participants_removed + 1, 1000));

	BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(marieCr), 1, int, "%d");
	BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(paulineCr), 1, int, "%d");
	BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(paulineCr2), 1, int, "%d");

	// Clean db from chat room
	linphone_core_manager_delete_chat_room(marie1, marieCr, coresList);
	linphone_core_delete_chat_room(marie2->lc, marieCr2);
	linphone_core_manager_delete_chat_room(laure, laureCr, coresList);
	linphone_core_manager_delete_chat_room(pauline1, paulineCr, coresList);
	linphone_core_delete_chat_room(pauline2->lc, paulineCr2);

	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie1);
	linphone_core_manager_destroy(marie2);
	linphone_core_manager_destroy(pauline1);
	linphone_core_manager_destroy(pauline2);
	linphone_core_manager_destroy(laure);
}

static void group_chat_room_add_device (void) {
	LinphoneCoreManager *marie1 = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline1 = linphone_core_manager_create("pauline_rc");
	LinphoneCoreManager *pauline2 = linphone_core_manager_create("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_create("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	coresManagerList = bctbx_list_append(coresManagerList, marie1);
	coresManagerList = bctbx_list_append(coresManagerList, pauline1);
	coresManagerList = bctbx_list_append(coresManagerList, pauline2);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	start_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline1->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarie1Stats = marie1->stat;
	stats initialPauline1Stats = pauline1->stat;
	stats initialPauline2Stats = pauline2->stat;
	stats initialLaureStats = laure->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie1, &initialMarie1Stats, participantsAddresses, initialSubject, -1);
	participantsAddresses = NULL;
	const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline1, &initialPauline1Stats, confAddr, initialSubject, 2, 0);
	LinphoneChatRoom *paulineCr2 = check_creation_chat_room_client_side(coresList, pauline2, &initialPauline2Stats, confAddr, initialSubject, 2, 0);

	// Check that the chat room is correctly created on Laure's side and that the participants are added
	LinphoneChatRoom *laureCr = check_creation_chat_room_client_side(coresList, laure, &initialLaureStats, confAddr, initialSubject, 2, 0);

	// Marie adds a new device
	LinphoneCoreManager *marie2 = linphone_core_manager_create("marie_rc");
	coresManagerList = bctbx_list_append(coresManagerList, marie2);
	LinphoneAddress *factoryAddr = linphone_address_new(sFactoryUri);
	_configure_core_for_conference(marie2, factoryAddr);
	linphone_address_unref(factoryAddr);
	LinphoneCoreCbs *cbs = linphone_factory_create_core_cbs(linphone_factory_get());
	linphone_core_cbs_set_chat_room_state_changed(cbs, core_chat_room_state_changed);
	_configure_core_for_callbacks(marie2, cbs);
	linphone_core_cbs_unref(cbs);
	coresList = bctbx_list_append(coresList, marie2->lc);
	_start_core(marie2);
	stats initialMarie2Stats = marie2->stat;
	// Check that the chat room is correctly created on second Marie's device
	LinphoneChatRoom *marieCr2 = check_creation_chat_room_client_side(coresList, marie2, &initialMarie2Stats, confAddr, initialSubject, 2, 1);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie1->stat.number_of_participant_devices_added, initialMarie1Stats.number_of_participant_devices_added + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline1->stat.number_of_participant_devices_added, initialMarie1Stats.number_of_participant_devices_added + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline2->stat.number_of_participant_devices_added, initialMarie1Stats.number_of_participant_devices_added + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_participant_devices_added, initialMarie1Stats.number_of_participant_devices_added + 1, 1000));

	// Check that adding Marie's device didn't change her admin status
	LinphoneParticipant *marieParticipant = linphone_chat_room_get_me(marieCr);
	BC_ASSERT_PTR_NOT_NULL(marieParticipant);
	BC_ASSERT_TRUE(linphone_participant_is_admin(marieParticipant));
	marieParticipant = linphone_chat_room_get_me(marieCr2);
	BC_ASSERT_PTR_NOT_NULL(marieParticipant);
	BC_ASSERT_TRUE(linphone_participant_is_admin(marieParticipant));
	LinphoneAddress *marieAddr = linphone_address_new(linphone_core_get_identity(marie1->lc));
	marieParticipant = linphone_chat_room_find_participant(paulineCr, marieAddr);
	BC_ASSERT_PTR_NOT_NULL(marieParticipant);
	BC_ASSERT_TRUE(linphone_participant_is_admin(marieParticipant));
	marieParticipant = linphone_chat_room_find_participant(paulineCr2, marieAddr);
	BC_ASSERT_PTR_NOT_NULL(marieParticipant);
	BC_ASSERT_TRUE(linphone_participant_is_admin(marieParticipant));
	marieParticipant = linphone_chat_room_find_participant(laureCr, marieAddr);
	BC_ASSERT_PTR_NOT_NULL(marieParticipant);
	BC_ASSERT_TRUE(linphone_participant_is_admin(marieParticipant));
	linphone_address_unref(marieAddr);

	// Marie removes Laure from the chat room
	LinphoneAddress *laureAddr = linphone_address_new(linphone_core_get_identity(laure->lc));
	LinphoneParticipant *laureParticipant = linphone_chat_room_find_participant(marieCr, laureAddr);
	linphone_address_unref(laureAddr);
	BC_ASSERT_PTR_NOT_NULL(laureParticipant);
	linphone_chat_room_remove_participant(marieCr, laureParticipant);
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_LinphoneChatRoomStateTerminated, initialLaureStats.number_of_LinphoneChatRoomStateTerminated + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie1->stat.number_of_participants_removed, initialMarie1Stats.number_of_participants_removed + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie2->stat.number_of_participants_removed, initialMarie2Stats.number_of_participants_removed + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline1->stat.number_of_participants_removed, initialPauline1Stats.number_of_participants_removed + 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline2->stat.number_of_participants_removed, initialPauline2Stats.number_of_participants_removed + 1, 1000));

	BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(marieCr), 1, int, "%d");
	BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(paulineCr), 1, int, "%d");
	BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(paulineCr2), 1, int, "%d");

	// Clean db from chat room
	linphone_core_manager_delete_chat_room(marie1, marieCr, coresList);
	linphone_core_delete_chat_room(marie2->lc, marieCr2);
	linphone_core_manager_delete_chat_room(laure, laureCr, coresList);
	linphone_core_manager_delete_chat_room(pauline1, paulineCr, coresList);
	linphone_core_delete_chat_room(pauline2->lc, paulineCr2);

	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie1);
	linphone_core_manager_destroy(marie2);
	linphone_core_manager_destroy(pauline1);
	linphone_core_manager_destroy(pauline2);
	linphone_core_manager_destroy(laure);
}

static void multiple_is_composing_notification(void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_create("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_create("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	const bctbx_list_t *composing_addresses = NULL;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	start_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialLaureStats = laure->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject, -1);
	participantsAddresses = NULL;
	const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline, &initialPaulineStats, confAddr, initialSubject, 2, FALSE);

	// Check that the chat room is correctly created on Laure's side and that the participants are added
	LinphoneChatRoom *laureCr = check_creation_chat_room_client_side(coresList, laure, &initialLaureStats, confAddr, initialSubject, 2, FALSE);

	// Only one is composing
	linphone_chat_room_compose(paulineCr);

	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneIsComposingActiveReceived, 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_LinphoneIsComposingActiveReceived, 1, 1000));

	// Laure side
	composing_addresses = linphone_chat_room_get_composing_addresses(laureCr);
	BC_ASSERT_EQUAL(bctbx_list_size(composing_addresses), 1, int, "%i");
	if (bctbx_list_size(composing_addresses) == 1) {
		LinphoneAddress *addr = (LinphoneAddress *)bctbx_list_get_data(composing_addresses);
		BC_ASSERT_STRING_EQUAL(linphone_address_get_username(addr), linphone_address_get_username(pauline->identity));
	}

	// Marie side
	composing_addresses = linphone_chat_room_get_composing_addresses(marieCr);
	BC_ASSERT_EQUAL(bctbx_list_size(composing_addresses), 1, int, "%i");
	if (bctbx_list_size(composing_addresses) == 1) {
		LinphoneAddress *addr = (LinphoneAddress *)bctbx_list_get_data(composing_addresses);
		BC_ASSERT_STRING_EQUAL(linphone_address_get_username(addr), linphone_address_get_username(pauline->identity));
	}

	// Pauline side
	composing_addresses = linphone_chat_room_get_composing_addresses(paulineCr);
	BC_ASSERT_EQUAL(bctbx_list_size(composing_addresses), 0, int, "%i");

	wait_for_list(coresList,0, 1, 1500);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneIsComposingIdleReceived, 1, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_LinphoneIsComposingIdleReceived, 1, 1000));

	composing_addresses = linphone_chat_room_get_composing_addresses(marieCr);
	BC_ASSERT_EQUAL(bctbx_list_size(composing_addresses), 0, int, "%i");
	composing_addresses = linphone_chat_room_get_composing_addresses(laureCr);
	BC_ASSERT_EQUAL(bctbx_list_size(composing_addresses), 0, int, "%i");

	// multiple is composing
	linphone_chat_room_compose(paulineCr);
	linphone_chat_room_compose(marieCr);

	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneIsComposingActiveReceived, 2, 1000)); // + 1
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_LinphoneIsComposingActiveReceived, 3, 1000)); // + 2
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneIsComposingActiveReceived, 1, 1000));

	// Laure side
	composing_addresses = linphone_chat_room_get_composing_addresses(laureCr);
	BC_ASSERT_EQUAL(bctbx_list_size(composing_addresses), 2, int, "%i");
	if (bctbx_list_size(composing_addresses) == 2) {
			while (composing_addresses) {
				LinphoneAddress *addr = (LinphoneAddress *)bctbx_list_get_data(composing_addresses);
				bool_t equal = strcmp(linphone_address_get_username(addr), linphone_address_get_username(pauline->identity)) == 0
							|| strcmp(linphone_address_get_username(addr), linphone_address_get_username(marie->identity)) == 0;
				BC_ASSERT_TRUE(equal);
				composing_addresses = bctbx_list_next(composing_addresses);
			}
	}

	// Marie side
	composing_addresses = linphone_chat_room_get_composing_addresses(marieCr);
	BC_ASSERT_EQUAL(bctbx_list_size(composing_addresses), 1, int, "%i");
	if (bctbx_list_size(composing_addresses) == 1) {
		LinphoneAddress *addr = (LinphoneAddress *)bctbx_list_get_data(composing_addresses);
		BC_ASSERT_STRING_EQUAL(linphone_address_get_username(addr), linphone_address_get_username(pauline->identity));
	}

	// Pauline side
	composing_addresses = linphone_chat_room_get_composing_addresses(paulineCr);
	BC_ASSERT_EQUAL(bctbx_list_size(composing_addresses), 1, int, "%i");
	if (bctbx_list_size(composing_addresses) == 1) {
		LinphoneAddress *addr = (LinphoneAddress *)bctbx_list_get_data(composing_addresses);
		BC_ASSERT_STRING_EQUAL(linphone_address_get_username(addr), linphone_address_get_username(marie->identity));
	}

	wait_for_list(coresList,0, 1, 1500);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneIsComposingIdleReceived, 2, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &laure->stat.number_of_LinphoneIsComposingIdleReceived, 3, 1000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneIsComposingIdleReceived, 1, 1000));

	composing_addresses = linphone_chat_room_get_composing_addresses(marieCr);
	BC_ASSERT_EQUAL(bctbx_list_size(composing_addresses), 0, int, "%i");
	composing_addresses = linphone_chat_room_get_composing_addresses(laureCr);
	BC_ASSERT_EQUAL(bctbx_list_size(composing_addresses), 0, int, "%i");
	composing_addresses = linphone_chat_room_get_composing_addresses(paulineCr);
	BC_ASSERT_EQUAL(bctbx_list_size(composing_addresses), 0, int, "%i");

	// Clean db from chat room
	linphone_core_manager_delete_chat_room(marie, marieCr, coresList);
	linphone_core_manager_delete_chat_room(laure, laureCr, coresList);
	linphone_core_manager_delete_chat_room(pauline, paulineCr, coresList);

	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
}

static void group_chat_room_fallback_to_basic_chat_room (void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_create("pauline_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	linphone_core_set_linphone_specs(pauline->lc, NULL);
	start_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;

	// Marie creates a new group chat room
	LinphoneChatRoom *marieCr = linphone_core_create_client_group_chat_room(marie->lc, "Fallback");
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneChatRoomStateInstantiated, initialMarieStats.number_of_LinphoneChatRoomStateInstantiated + 1, 100));

	// Add participants
	linphone_chat_room_add_participants(marieCr, participantsAddresses);

	// Check that the group chat room creation fails and that a fallback to a basic chat room is done
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneChatRoomStateCreationPending, initialMarieStats.number_of_LinphoneChatRoomStateCreationPending + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneChatRoomStateCreated, initialMarieStats.number_of_LinphoneChatRoomStateCreated + 1, 10000));
	BC_ASSERT_EQUAL(marie->stat.number_of_LinphoneChatRoomStateCreationFailed, initialMarieStats.number_of_LinphoneChatRoomStateCreationFailed, int, "%d");
	BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(marieCr), 1, int, "%d");
	BC_ASSERT_TRUE(linphone_chat_room_get_capabilities(marieCr) & LinphoneChatRoomCapabilitiesBasic);
	bctbx_list_free_with_data(participantsAddresses, (bctbx_list_free_func)linphone_address_unref);
	participantsAddresses = NULL;

	// Send a message and check that a basic chat room is create on Pauline's side
	LinphoneChatMessage *msg = linphone_chat_room_create_message(marieCr, "Hey Pauline!");
	linphone_chat_message_send(msg);
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneMessageReceived, initialPaulineStats.number_of_LinphoneMessageReceived + 1, 1000));
	BC_ASSERT_PTR_NOT_NULL(pauline->stat.last_received_chat_message);
	if (pauline->stat.last_received_chat_message)
		BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_content_type(pauline->stat.last_received_chat_message), "text/plain");
	LinphoneChatRoom *paulineCr = linphone_core_get_chat_room(pauline->lc, linphone_chat_room_get_local_address(marieCr));
	BC_ASSERT_PTR_NOT_NULL(paulineCr);
	if (paulineCr)
		BC_ASSERT_TRUE(linphone_chat_room_get_capabilities(paulineCr) & LinphoneChatRoomCapabilitiesBasic);

	// Clean db from chat room
	linphone_core_manager_delete_chat_room(marie, marieCr, coresList);
	linphone_core_manager_delete_chat_room(pauline, paulineCr, coresList);

	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
}

static void group_chat_room_creation_fails_if_invited_participants_dont_support_it (void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_create("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_create("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	linphone_core_set_linphone_specs(pauline->lc, NULL);
	linphone_core_set_linphone_specs(laure->lc, NULL);
	start_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarieStats = marie->stat;

	// Marie creates a new group chat room
	LinphoneChatRoom *marieCr = linphone_core_create_client_group_chat_room(marie->lc, "Hello there");
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneChatRoomStateInstantiated, initialMarieStats.number_of_LinphoneChatRoomStateInstantiated + 1, 100));

	// Add participants
	linphone_chat_room_add_participants(marieCr, participantsAddresses);

	// Check that the group chat room creation fails and that a fallback to a basic chat room is done
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneChatRoomStateCreationPending, initialMarieStats.number_of_LinphoneChatRoomStateCreationPending + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneChatRoomStateCreationFailed, initialMarieStats.number_of_LinphoneChatRoomStateCreationFailed + 1, 10000));
	bctbx_list_free_with_data(participantsAddresses, (bctbx_list_free_func)linphone_address_unref);
	participantsAddresses = NULL;
	BC_ASSERT_EQUAL(linphone_chat_room_get_state(marieCr), LinphoneChatRoomStateCreationFailed, int, "%d");

	// Clean db from chat room
	linphone_core_manager_delete_chat_room(marie, marieCr, coresList);

	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
}

static void group_chat_room_creation_successful_if_at_least_one_invited_participant_supports_it (void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_create("pauline_rc");
	LinphoneCoreManager *laure = linphone_core_manager_create("laure_tcp_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, laure);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	linphone_core_set_linphone_specs(laure->lc, NULL);
	start_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(laure->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialLaureStats = laure->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject, 1);
	participantsAddresses = NULL;
	const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline, &initialPaulineStats, confAddr, initialSubject, 1, FALSE);

	// Check that the chat room has not been created on Laure's side
	BC_ASSERT_EQUAL(initialLaureStats.number_of_LinphoneChatRoomStateCreated, laure->stat.number_of_LinphoneChatRoomStateCreated, int, "%d");

	// Clean db from chat room
	linphone_core_manager_delete_chat_room(marie, marieCr, coresList);
	linphone_core_manager_delete_chat_room(pauline, paulineCr, coresList);

	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(laure);
}

static void group_chat_room_migrate_from_basic_chat_room (void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_create("pauline_rc");
	bctbx_list_t *coresManagerList = NULL;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	start_core_for_conference(coresManagerList);
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;

	// Create a basic chat room
	LinphoneAddress *paulineAddr = linphone_address_new(linphone_core_get_identity(pauline->lc));
	LinphoneChatRoom *marieCr = linphone_core_get_chat_room(marie->lc, paulineAddr);

	// Send a message and check that a basic chat room is create on Pauline's side
	LinphoneChatMessage *msg = linphone_chat_room_create_message(marieCr, "Hey Pauline!");
	linphone_chat_message_send(msg);
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneMessageReceived, initialPaulineStats.number_of_LinphoneMessageReceived + 1, 1000));
	BC_ASSERT_PTR_NOT_NULL(pauline->stat.last_received_chat_message);
	if (pauline->stat.last_received_chat_message)
		BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_content_type(pauline->stat.last_received_chat_message), "text/plain");
	LinphoneChatRoom *paulineCr = linphone_core_get_chat_room(pauline->lc, linphone_chat_room_get_local_address(marieCr));
	BC_ASSERT_PTR_NOT_NULL(paulineCr);
	if (paulineCr)
		BC_ASSERT_TRUE(linphone_chat_room_get_capabilities(paulineCr) & LinphoneChatRoomCapabilitiesBasic);

	// Enable chat room migration and restart core for Marie
	_linphone_chat_room_enable_migration(marieCr, TRUE);
	coresList = bctbx_list_remove(coresList, marie->lc);
	linphone_core_manager_restart(marie, TRUE);
	bctbx_list_t *tmpCoresManagerList = bctbx_list_append(NULL, marie);
	init_core_for_conference(tmpCoresManagerList);
	bctbx_list_free(tmpCoresManagerList);
	coresList = bctbx_list_append(coresList, marie->lc);

	// Send a new message to initiate chat room migration
	marieCr = linphone_core_get_chat_room(marie->lc, paulineAddr);
	BC_ASSERT_PTR_NOT_NULL(marieCr);
	if (marieCr) {
		initialMarieStats = marie->stat;
		initialPaulineStats = pauline->stat;
		BC_ASSERT_EQUAL(linphone_chat_room_get_capabilities(marieCr), LinphoneChatRoomCapabilitiesBasic | LinphoneChatRoomCapabilitiesProxy | LinphoneChatRoomCapabilitiesMigratable, int, "%d");
		msg = linphone_chat_room_create_message(marieCr, "Did you migrate?");
		linphone_chat_message_send(msg);
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneChatRoomStateCreationPending, initialMarieStats.number_of_LinphoneChatRoomStateCreationPending + 1, 10000));
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneChatRoomStateCreated, initialMarieStats.number_of_LinphoneChatRoomStateCreated + 1, 10000));
		BC_ASSERT_TRUE(linphone_chat_room_get_capabilities(marieCr) & LinphoneChatRoomCapabilitiesConference);
		BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(marieCr), 1, int, "%d");
		BC_ASSERT_EQUAL(linphone_chat_room_get_history_size(marieCr), 2, int, "%d");

		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneChatRoomStateCreationPending, initialPaulineStats.number_of_LinphoneChatRoomStateCreationPending + 1, 10000));
		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneChatRoomStateCreated, initialPaulineStats.number_of_LinphoneChatRoomStateCreated + 1, 10000));
		BC_ASSERT_TRUE(linphone_chat_room_get_capabilities(paulineCr) & LinphoneChatRoomCapabilitiesConference);
		BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(paulineCr), 1, int, "%d");
		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneMessageReceived, initialPaulineStats.number_of_LinphoneMessageReceived + 1, 1000));
		BC_ASSERT_EQUAL(linphone_chat_room_get_history_size(paulineCr), 2, int, "%d");

		msg = linphone_chat_room_create_message(marieCr, "Let's go drink a beer");
		linphone_chat_message_send(msg);
		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneMessageReceived, initialPaulineStats.number_of_LinphoneMessageReceived + 2, 1000));
		BC_ASSERT_EQUAL(linphone_chat_room_get_history_size(marieCr), 3, int, "%d");
		BC_ASSERT_EQUAL(linphone_chat_room_get_history_size(paulineCr), 3, int, "%d");

		msg = linphone_chat_room_create_message(paulineCr, "Let's go drink mineral water instead");
		linphone_chat_message_send(msg);
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneMessageReceived, initialMarieStats.number_of_LinphoneMessageReceived + 1, 1000));
		BC_ASSERT_EQUAL(linphone_chat_room_get_history_size(marieCr), 4, int, "%d");
		BC_ASSERT_EQUAL(linphone_chat_room_get_history_size(paulineCr), 4, int, "%d");
	}

	// Clean db from chat room
	linphone_core_manager_delete_chat_room(marie, marieCr, coresList);
	linphone_core_manager_delete_chat_room(pauline, paulineCr, coresList);

	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
}

static void group_chat_room_migrate_from_basic_to_client_fail (void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_create("pauline_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	int dummy = 0;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	linphone_core_set_linphone_specs(pauline->lc, NULL);
	start_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;

	// Marie creates a new group chat room
	LinphoneChatRoom *marieCr = linphone_core_create_client_group_chat_room(marie->lc, "Fallback");
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneChatRoomStateInstantiated, initialMarieStats.number_of_LinphoneChatRoomStateInstantiated + 1, 100));

	// Add participants
	linphone_chat_room_add_participants(marieCr, participantsAddresses);

	// Check that the group chat room creation fails and that a fallback to a basic chat room is done
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneChatRoomStateCreationPending, initialMarieStats.number_of_LinphoneChatRoomStateCreationPending + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneChatRoomStateCreated, initialMarieStats.number_of_LinphoneChatRoomStateCreated + 1, 10000));
	BC_ASSERT_EQUAL(marie->stat.number_of_LinphoneChatRoomStateCreationFailed, initialMarieStats.number_of_LinphoneChatRoomStateCreationFailed, int, "%d");
	BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(marieCr), 1, int, "%d");
	BC_ASSERT_TRUE(linphone_chat_room_get_capabilities(marieCr) & LinphoneChatRoomCapabilitiesBasic);
	bctbx_list_free_with_data(participantsAddresses, (bctbx_list_free_func)linphone_address_unref);
	participantsAddresses = NULL;

	// Send a message and check that a basic chat room is create on Pauline's side
	LinphoneChatMessage *msg = linphone_chat_room_create_message(marieCr, "Hey Pauline!");
	linphone_chat_message_send(msg);
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneMessageReceived, initialPaulineStats.number_of_LinphoneMessageReceived + 1, 1000));
	BC_ASSERT_PTR_NOT_NULL(pauline->stat.last_received_chat_message);
	if (pauline->stat.last_received_chat_message)
		BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_content_type(pauline->stat.last_received_chat_message), "text/plain");
	LinphoneChatRoom *paulineCr = linphone_core_get_chat_room(pauline->lc, linphone_chat_room_get_local_address(marieCr));
	BC_ASSERT_PTR_NOT_NULL(paulineCr);
	if (paulineCr)
		BC_ASSERT_TRUE(linphone_chat_room_get_capabilities(paulineCr) & LinphoneChatRoomCapabilitiesBasic);

	// Enable chat room migration and restart core for Marie
	_linphone_chat_room_enable_migration(marieCr, TRUE);
	coresList = bctbx_list_remove(coresList, marie->lc);
	linphone_core_manager_restart(marie, TRUE);
	bctbx_list_t *tmpCoresManagerList = bctbx_list_append(NULL, marie);
	init_core_for_conference(tmpCoresManagerList);
	bctbx_list_free(tmpCoresManagerList);
	coresList = bctbx_list_append(coresList, marie->lc);

	// Send a new message to initiate chat room migration
	LinphoneAddress *paulineAddr = linphone_address_new(linphone_core_get_identity(pauline->lc));
	marieCr = linphone_core_get_chat_room(marie->lc, paulineAddr);
	BC_ASSERT_PTR_NOT_NULL(marieCr);
	if (marieCr) {
		initialMarieStats = marie->stat;
		initialPaulineStats = pauline->stat;
		BC_ASSERT_EQUAL(linphone_chat_room_get_capabilities(marieCr), LinphoneChatRoomCapabilitiesBasic | LinphoneChatRoomCapabilitiesProxy | LinphoneChatRoomCapabilitiesMigratable, int, "%d");
		msg = linphone_chat_room_create_message(marieCr, "Did you migrate?");
		linphone_chat_message_send(msg);
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneChatRoomStateCreationPending, initialMarieStats.number_of_LinphoneChatRoomStateCreationPending + 1, 3000));
		BC_ASSERT_FALSE(wait_for_list(coresList, &marie->stat.number_of_LinphoneChatRoomStateCreated, initialMarieStats.number_of_LinphoneChatRoomStateCreated + 1, 3000));
		BC_ASSERT_TRUE(linphone_chat_room_get_capabilities(marieCr) & LinphoneChatRoomCapabilitiesBasic);
		BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(marieCr), 1, int, "%d");
		BC_ASSERT_EQUAL(linphone_chat_room_get_history_size(marieCr), 2, int, "%d");

		BC_ASSERT_FALSE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneChatRoomStateCreationPending, initialPaulineStats.number_of_LinphoneChatRoomStateCreationPending + 1, 3000));
		BC_ASSERT_FALSE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneChatRoomStateCreated, initialPaulineStats.number_of_LinphoneChatRoomStateCreated + 1, 3000));
		BC_ASSERT_TRUE(linphone_chat_room_get_capabilities(paulineCr) & LinphoneChatRoomCapabilitiesBasic);
		BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(paulineCr), 1, int, "%d");
		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneMessageReceived, initialPaulineStats.number_of_LinphoneMessageReceived + 1, 1000));
		BC_ASSERT_EQUAL(linphone_chat_room_get_history_size(paulineCr), 2, int, "%d");

		msg = linphone_chat_room_create_message(marieCr, "Let's go drink a beer");
		linphone_chat_message_send(msg);
		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneMessageReceived, initialPaulineStats.number_of_LinphoneMessageReceived + 2, 1000));
		BC_ASSERT_EQUAL(linphone_chat_room_get_history_size(marieCr), 3, int, "%d");
		BC_ASSERT_EQUAL(linphone_chat_room_get_history_size(paulineCr), 3, int, "%d");

		msg = linphone_chat_room_create_message(paulineCr, "Let's go drink mineral water instead");
		linphone_chat_message_send(msg);
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneMessageReceived, initialMarieStats.number_of_LinphoneMessageReceived + 1, 1000));
		BC_ASSERT_EQUAL(linphone_chat_room_get_history_size(marieCr), 4, int, "%d");
		BC_ASSERT_EQUAL(linphone_chat_room_get_history_size(paulineCr), 4, int, "%d");

		// Activate groupchat on Pauline's side and wait for 5 seconds, the migration should now be done on next message sending
		linphone_core_set_linphone_specs(pauline->lc, "groupchat");
		linphone_core_set_network_reachable(pauline->lc, FALSE);
		wait_for_list(coresList, &dummy, 1, 1000);
		linphone_core_set_network_reachable(pauline->lc, TRUE);
		wait_for_list(coresList, &dummy, 1, 5000);
		msg = linphone_chat_room_create_message(marieCr, "And now, did you migrate?");
		linphone_chat_message_send(msg);
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneChatRoomStateCreationPending, initialMarieStats.number_of_LinphoneChatRoomStateCreationPending + 2, 10000));
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneChatRoomStateCreated, initialMarieStats.number_of_LinphoneChatRoomStateCreated + 1, 10000));
		BC_ASSERT_TRUE(linphone_chat_room_get_capabilities(marieCr) & LinphoneChatRoomCapabilitiesConference);
		BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(marieCr), 1, int, "%d");
		BC_ASSERT_EQUAL(linphone_chat_room_get_history_size(marieCr), 5, int, "%d");

		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneChatRoomStateCreationPending, initialPaulineStats.number_of_LinphoneChatRoomStateCreationPending + 1, 10000));
		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneChatRoomStateCreated, initialPaulineStats.number_of_LinphoneChatRoomStateCreated + 1, 10000));
		BC_ASSERT_TRUE(linphone_chat_room_get_capabilities(paulineCr) & LinphoneChatRoomCapabilitiesConference);
		BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(paulineCr), 1, int, "%d");
		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneMessageReceived, initialPaulineStats.number_of_LinphoneMessageReceived + 3, 1000));
		BC_ASSERT_EQUAL(linphone_chat_room_get_history_size(paulineCr), 5, int, "%d");
	}

	// Clean db from chat room
	linphone_core_manager_delete_chat_room(marie, marieCr, coresList);
	linphone_core_manager_delete_chat_room(pauline, paulineCr, coresList);

	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
}

static void group_chat_donot_room_migrate_from_basic_chat_room (void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_create("pauline_rc");
	bctbx_list_t *coresManagerList = NULL;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	start_core_for_conference(coresManagerList);
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;

	// Create a basic chat room
	LinphoneAddress *paulineAddr = linphone_address_new(linphone_core_get_identity(pauline->lc));
	LinphoneChatRoom *marieCr = linphone_core_get_chat_room(marie->lc, paulineAddr);

	// Send a message and check that a basic chat room is create on Pauline's side
	LinphoneChatMessage *msg = linphone_chat_room_create_message(marieCr, "Hey Pauline!");
	linphone_chat_message_send(msg);
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneMessageReceived, initialPaulineStats.number_of_LinphoneMessageReceived + 1, 1000));
	BC_ASSERT_PTR_NOT_NULL(pauline->stat.last_received_chat_message);
	if (pauline->stat.last_received_chat_message)
		BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_content_type(pauline->stat.last_received_chat_message), "text/plain");
	LinphoneChatRoom *paulineCr = linphone_core_get_chat_room(pauline->lc, linphone_chat_room_get_local_address(marieCr));
	BC_ASSERT_PTR_NOT_NULL(paulineCr);
	if (paulineCr)
		BC_ASSERT_TRUE(linphone_chat_room_get_capabilities(paulineCr) & LinphoneChatRoomCapabilitiesBasic);

	// Enable chat room migration and restart core for Marie
	coresList = bctbx_list_remove(coresList, marie->lc);
	linphone_core_manager_restart(marie, TRUE);
	bctbx_list_t *tmpCoresManagerList = bctbx_list_append(NULL, marie);
	init_core_for_conference(tmpCoresManagerList);
	bctbx_list_free(tmpCoresManagerList);
	coresList = bctbx_list_append(coresList, marie->lc);

	// Send a new message to initiate chat room migration
	marieCr = linphone_core_get_chat_room(marie->lc, paulineAddr);
	BC_ASSERT_PTR_NOT_NULL(marieCr);
	if (marieCr) {
		initialMarieStats = marie->stat;
		initialPaulineStats = pauline->stat;
		BC_ASSERT_EQUAL(linphone_chat_room_get_capabilities(marieCr), LinphoneChatRoomCapabilitiesBasic, int, "%d");
		msg = linphone_chat_room_create_message(marieCr, "Did you migrate?");
		linphone_chat_message_send(msg);
		BC_ASSERT_FALSE(wait_for_list(coresList, &marie->stat.number_of_LinphoneChatRoomStateCreationPending, initialMarieStats.number_of_LinphoneChatRoomStateCreationPending + 1, 10000));
		BC_ASSERT_FALSE(wait_for_list(coresList, &marie->stat.number_of_LinphoneChatRoomStateCreated, initialMarieStats.number_of_LinphoneChatRoomStateCreated + 1, 10000));
		BC_ASSERT_TRUE(linphone_chat_room_get_capabilities(marieCr) & LinphoneChatRoomCapabilitiesBasic);
		BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(marieCr), 1, int, "%d");
		BC_ASSERT_EQUAL(linphone_chat_room_get_history_size(marieCr), 2, int, "%d");

		BC_ASSERT_FALSE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneChatRoomStateCreationPending, initialPaulineStats.number_of_LinphoneChatRoomStateCreationPending + 1, 10000));
		BC_ASSERT_FALSE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneChatRoomStateCreated, initialPaulineStats.number_of_LinphoneChatRoomStateCreated + 1, 10000));
		BC_ASSERT_TRUE(linphone_chat_room_get_capabilities(paulineCr) & LinphoneChatRoomCapabilitiesBasic);
		BC_ASSERT_EQUAL(linphone_chat_room_get_nb_participants(paulineCr), 1, int, "%d");
		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneMessageReceived, initialPaulineStats.number_of_LinphoneMessageReceived + 1, 1000));
		BC_ASSERT_EQUAL(linphone_chat_room_get_history_size(paulineCr), 2, int, "%d");

		msg = linphone_chat_room_create_message(marieCr, "Let's go drink a beer");
		linphone_chat_message_send(msg);
		BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneMessageReceived, initialPaulineStats.number_of_LinphoneMessageReceived + 2, 1000));
		BC_ASSERT_EQUAL(linphone_chat_room_get_history_size(marieCr), 3, int, "%d");
		BC_ASSERT_EQUAL(linphone_chat_room_get_history_size(paulineCr), 3, int, "%d");

		msg = linphone_chat_room_create_message(paulineCr, "Let's go drink mineral water instead");
		linphone_chat_message_send(msg);
		BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneMessageReceived, initialMarieStats.number_of_LinphoneMessageReceived + 1, 1000));
		BC_ASSERT_EQUAL(linphone_chat_room_get_history_size(marieCr), 4, int, "%d");
		BC_ASSERT_EQUAL(linphone_chat_room_get_history_size(paulineCr), 4, int, "%d");
	}

	// Clean db from chat room
	linphone_core_delete_chat_room(marie->lc, marieCr);
	linphone_core_delete_chat_room(pauline->lc, paulineCr);

	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
}

static void group_chat_room_send_file_with_or_without_text (bool_t with_text) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_create("pauline_rc");
	LinphoneCoreManager *chloe = linphone_core_manager_create("chloe_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	int dummy = 0;
	char *sendFilepath = bc_tester_res("sounds/sintel_trailer_opus_h264.mkv");
	char *receivePaulineFilepath = bc_tester_file("receive_file_pauline.dump");
	char *receiveChloeFilepath = bc_tester_file("receive_file_chloe.dump");
	const char *text = "Hello Group !";

	/* Globally configure an http file transfer server. */
	linphone_core_set_file_transfer_server(marie->lc, "https://www.linphone.org:444/lft.php");
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	coresManagerList = bctbx_list_append(coresManagerList, chloe);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	start_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(chloe->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;
	stats initialChloeStats = chloe->stat;

	/* Remove any previously downloaded file */
	remove(receivePaulineFilepath);
	remove(receiveChloeFilepath);

	// Marie creates a new group chat room
	const char *initialSubject = "Colleagues";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject, -1);
	const LinphoneAddress *confAddr = linphone_chat_room_get_conference_address(marieCr);

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline, &initialPaulineStats, confAddr, initialSubject, 2, 0);

	// Check that the chat room is correctly created on Chloe's side and that the participants are added
	LinphoneChatRoom *chloeCr = check_creation_chat_room_client_side(coresList, chloe, &initialChloeStats, confAddr, initialSubject, 2, 0);

	// Sending file
	if (with_text) {
		_send_file_plus_text(marieCr, sendFilepath, text);
	} else {
		_send_file(marieCr, sendFilepath);
	}

	wait_for_list(coresList, &dummy, 1, 10000);

	// Check that chat rooms have received the file
	if (with_text) {
		_receive_file_plus_text(coresList, pauline, &initialPaulineStats, receivePaulineFilepath, sendFilepath, text);
		_receive_file_plus_text(coresList, chloe, &initialChloeStats, receiveChloeFilepath, sendFilepath, text);
	} else {
		_receive_file(coresList, pauline, &initialPaulineStats, receivePaulineFilepath, sendFilepath);
		_receive_file(coresList, chloe, &initialChloeStats, receiveChloeFilepath, sendFilepath);
	}

	// Clean db from chat room
	linphone_core_manager_delete_chat_room(marie, marieCr, coresList);
	linphone_core_manager_delete_chat_room(chloe, chloeCr, coresList);
	linphone_core_manager_delete_chat_room(pauline, paulineCr, coresList);
	bc_free(sendFilepath);
	bc_free(receivePaulineFilepath);
	bc_free(receiveChloeFilepath);

	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(chloe);
}

static void group_chat_room_send_file (void) {
	group_chat_room_send_file_with_or_without_text(FALSE);
}

static void group_chat_room_send_file_plus_text (void) {
	group_chat_room_send_file_with_or_without_text(TRUE);
}

static void group_chat_room_unique_one_to_one_chat_room (void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_create("pauline_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	start_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Pauline";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject, -1);
	BC_ASSERT_TRUE(linphone_chat_room_get_capabilities(marieCr) & LinphoneChatRoomCapabilitiesOneToOne);

	LinphoneAddress *confAddr = linphone_address_clone(linphone_chat_room_get_conference_address(marieCr));

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline, &initialPaulineStats, confAddr, initialSubject, 1, 0);
	BC_ASSERT_TRUE(linphone_chat_room_get_capabilities(paulineCr) & LinphoneChatRoomCapabilitiesOneToOne);

	// Marie sends a message
	const char *message = "Hello";
	_send_message(marieCr, message);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneMessageDelivered, initialMarieStats.number_of_LinphoneMessageDelivered + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneMessageReceived, initialPaulineStats.number_of_LinphoneMessageReceived + 1, 10000));
	BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_text(pauline->stat.last_received_chat_message), message);

	// Marie deletes the chat room
	linphone_core_manager_delete_chat_room(marie, marieCr, coresList);
	wait_for_list(coresList, 0, 1, 2000);
	BC_ASSERT_EQUAL(pauline->stat.number_of_participants_removed, initialPaulineStats.number_of_participants_removed, int, "%d");

	// Marie creates the chat room again
	initialMarieStats = marie->stat;
	initialPaulineStats = pauline->stat;
	participantsAddresses = bctbx_list_append(NULL, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject, -1);

	// Marie sends a new message
	message = "Hey again";
	_send_message(marieCr, message);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneMessageDelivered, initialMarieStats.number_of_LinphoneMessageDelivered + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneMessageReceived, initialPaulineStats.number_of_LinphoneMessageReceived + 1, 10000));
	BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_text(pauline->stat.last_received_chat_message), message);

	// Check that the created address is the same as before
	const LinphoneAddress *newConfAddr = linphone_chat_room_get_conference_address(marieCr);
	BC_ASSERT_TRUE(linphone_address_weak_equal(confAddr, newConfAddr));

	// Clean db from chat room
	linphone_core_manager_delete_chat_room(marie, marieCr, coresList);
	linphone_core_manager_delete_chat_room(pauline, paulineCr, coresList);

	linphone_address_unref(confAddr);
	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
}

static void group_chat_room_unique_one_to_one_chat_room_recreated_from_message (void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_create("pauline_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	start_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Pauline";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject, -1);
	BC_ASSERT_TRUE(linphone_chat_room_get_capabilities(marieCr) & LinphoneChatRoomCapabilitiesOneToOne);

	LinphoneAddress *confAddr = linphone_address_clone(linphone_chat_room_get_conference_address(marieCr));

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline, &initialPaulineStats, confAddr, initialSubject, 1, 0);
	BC_ASSERT_TRUE(linphone_chat_room_get_capabilities(paulineCr) & LinphoneChatRoomCapabilitiesOneToOne);

	// Marie sends a message
	const char *message = "Hello";
	_send_message(marieCr, message);
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneMessageDelivered, initialMarieStats.number_of_LinphoneMessageDelivered + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneMessageReceived, initialPaulineStats.number_of_LinphoneMessageReceived + 1, 10000));
	BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_text(pauline->stat.last_received_chat_message), message);

	// Marie deletes the chat room
	linphone_core_manager_delete_chat_room(marie, marieCr, coresList);
	wait_for_list(coresList, 0, 1, 2000);
	BC_ASSERT_EQUAL(pauline->stat.number_of_participants_removed, initialPaulineStats.number_of_participants_removed, int, "%d");

	// Pauline sends a new message
	initialMarieStats = marie->stat;
	initialPaulineStats = pauline->stat;

	// Marie sends a new message
	message = "Hey you";
	_send_message(paulineCr, message);
	BC_ASSERT_TRUE(wait_for_list(coresList, &pauline->stat.number_of_LinphoneMessageDelivered, initialPaulineStats.number_of_LinphoneMessageDelivered + 1, 10000));
	BC_ASSERT_TRUE(wait_for_list(coresList, &marie->stat.number_of_LinphoneMessageReceived, initialMarieStats.number_of_LinphoneMessageReceived + 1, 10000));
	BC_ASSERT_STRING_EQUAL(linphone_chat_message_get_text(marie->stat.last_received_chat_message), message);

	// Check that the chat room has been correctly recreated on Marie's side
	marieCr = check_creation_chat_room_client_side(coresList, marie, &initialMarieStats, confAddr, initialSubject, 1, 0);
	BC_ASSERT_TRUE(linphone_chat_room_get_capabilities(paulineCr) & LinphoneChatRoomCapabilitiesOneToOne);

	// Clean db from chat room
	linphone_core_manager_delete_chat_room(marie, marieCr, coresList);
	linphone_core_manager_delete_chat_room(pauline, paulineCr, coresList);

	linphone_address_unref(confAddr);
	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
}

static void group_chat_room_new_unique_one_to_one_chat_room_after_both_participants_left (void) {
	LinphoneCoreManager *marie = linphone_core_manager_create("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_create("pauline_rc");
	bctbx_list_t *coresManagerList = NULL;
	bctbx_list_t *participantsAddresses = NULL;
	coresManagerList = bctbx_list_append(coresManagerList, marie);
	coresManagerList = bctbx_list_append(coresManagerList, pauline);
	bctbx_list_t *coresList = init_core_for_conference(coresManagerList);
	start_core_for_conference(coresManagerList);
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	stats initialMarieStats = marie->stat;
	stats initialPaulineStats = pauline->stat;

	// Marie creates a new group chat room
	const char *initialSubject = "Pauline";
	LinphoneChatRoom *marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject, -1);
	BC_ASSERT_TRUE(linphone_chat_room_get_capabilities(marieCr) & LinphoneChatRoomCapabilitiesOneToOne);

	LinphoneAddress *firstConfAddr = linphone_address_clone(linphone_chat_room_get_conference_address(marieCr));

	// Check that the chat room is correctly created on Pauline's side and that the participants are added
	LinphoneChatRoom *paulineCr = check_creation_chat_room_client_side(coresList, pauline, &initialPaulineStats, firstConfAddr, initialSubject, 1, 0);
	BC_ASSERT_TRUE(linphone_chat_room_get_capabilities(paulineCr) & LinphoneChatRoomCapabilitiesOneToOne);

	// Both participants delete the chat room
	linphone_core_manager_delete_chat_room(marie, marieCr, coresList);
	linphone_core_manager_delete_chat_room(pauline, paulineCr, coresList);

	// Marie re-creates a chat room with Pauline
	initialMarieStats = marie->stat;
	initialPaulineStats = pauline->stat;
	participantsAddresses = NULL;
	participantsAddresses = bctbx_list_append(participantsAddresses, linphone_address_new(linphone_core_get_identity(pauline->lc)));
	marieCr = create_chat_room_client_side(coresList, marie, &initialMarieStats, participantsAddresses, initialSubject, -1);
	BC_ASSERT_TRUE(linphone_chat_room_get_capabilities(marieCr) & LinphoneChatRoomCapabilitiesOneToOne);

	LinphoneAddress *secondConfAddr = linphone_address_clone(linphone_chat_room_get_conference_address(marieCr));

	// Check that the chat room has been correctly recreated on Marie's side
	paulineCr = check_creation_chat_room_client_side(coresList, pauline, &initialPaulineStats, secondConfAddr, initialSubject, 1, 0);
	BC_ASSERT_TRUE(linphone_chat_room_get_capabilities(paulineCr) & LinphoneChatRoomCapabilitiesOneToOne);

	BC_ASSERT_FALSE(linphone_address_equal(firstConfAddr, secondConfAddr));

	// Clean db from chat room
	linphone_core_manager_delete_chat_room(marie, marieCr, coresList);
	linphone_core_manager_delete_chat_room(pauline, paulineCr, coresList);

	linphone_address_unref(firstConfAddr);
	linphone_address_unref(secondConfAddr);
	bctbx_list_free(coresList);
	bctbx_list_free(coresManagerList);
	linphone_core_manager_destroy(marie);
	linphone_core_manager_destroy(pauline);
}

test_t group_chat_tests[] = {
	TEST_TWO_TAGS("Group chat room creation server", group_chat_room_creation_server, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Send message", group_chat_room_send_message, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Send invite on a multi register account", group_chat_room_invite_multi_register_account, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Add admin", group_chat_room_add_admin, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Add admin lately notified", group_chat_room_add_admin_lately_notified, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Add admin with a non admin", group_chat_room_add_admin_non_admin, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Remove admin", group_chat_room_remove_admin, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Change subject", group_chat_room_change_subject, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Change subject with a non admin", group_chat_room_change_subject_non_admin, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Remove participant", group_chat_room_remove_participant, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Send message with a participant removed", group_chat_room_send_message_with_participant_removed, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Leave group chat room", group_chat_room_leave, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Come back on a group chat room after a disconnection", group_chat_room_come_back_after_disconnection, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Create chat room with disconnected friends", group_chat_room_create_room_with_disconnected_friends, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Reinvited after removed from group chat room", group_chat_room_reinvited_after_removed, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Notify after disconnection", group_chat_room_notify_after_disconnection, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Send refer to all participants devices", group_chat_room_send_refer_to_all_devices, "Server", "LeaksMemory"),
	// TODO: Use when we support adding a new device in created conf
	//TEST_TWO_TAGS("Admin add device and doesn't lose admin status", group_chat_room_add_device, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Send multiple is composing", multiple_is_composing_notification, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Fallback to basic chat room", group_chat_room_fallback_to_basic_chat_room, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Group chat room creation fails if invited participants don't support it", group_chat_room_creation_fails_if_invited_participants_dont_support_it, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Group chat room creation succesful if at least one invited participant supports it", group_chat_room_creation_successful_if_at_least_one_invited_participant_supports_it, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Migrate basic chat room to client group chat room", group_chat_room_migrate_from_basic_chat_room, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Migrate basic chat room to client group chat room failure", group_chat_room_migrate_from_basic_to_client_fail, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Migrate basic chat room to client group chat room not needed", group_chat_donot_room_migrate_from_basic_chat_room, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Send file", group_chat_room_send_file, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Send file + text", group_chat_room_send_file_plus_text, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Unique one-to-one chatroom", group_chat_room_unique_one_to_one_chat_room, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("Unique one-to-one chatroom recreated from message", group_chat_room_unique_one_to_one_chat_room_recreated_from_message, "Server", "LeaksMemory"),
	TEST_TWO_TAGS("New unique one-to-one chatroom after both participants left", group_chat_room_new_unique_one_to_one_chat_room_after_both_participants_left, "Server", "LeaksMemory")
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

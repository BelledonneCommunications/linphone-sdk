/*
 * Copyright (c) 2010-2026 Belledonne Communications SARL.
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

#include "bctoolbox/utils.hh"

#include "address/address.h"
#include "call/call-log.h"
#include "call/call.h"
#include "chat/chat-message/chat-message.h"
#include "conference/conference-id.h"
#include "content/content-type.h"
#include "liblinphone_tester.h"
#include "linphone/utils/utils.h"
#include "private_functions.h"
#include "tester_utils.h"

// =============================================================================
//		Tests on Synchronizations mechanisms like call logs, backups, messages
// =============================================================================

using namespace LinphonePrivate;
using namespace LinphonePrivate::Utils;

static bool _check_call_log(LinphoneCoreManager *account, const std::shared_ptr<CallLog> &target) {
	auto callLogs = linphone_core_get_call_logs(account->lc);
	if (bctbx_list_size(callLogs) > 0) {
		auto callLog = CallLog::toCpp((LinphoneCallLog *)bctbx_list_get_data(callLogs));
		bool error = !BC_ASSERT_TRUE(callLog->getDirection() == target->getDirection());
		error = !BC_ASSERT_TRUE(callLog->getStatus() == target->getStatus()) || error;
		error = !BC_ASSERT_TRUE(callLog->getCallId() == target->getCallId()) || error;
		error = !BC_ASSERT_TRUE(callLog->getFromAddress()->weakEqual(target->getFromAddress())) || error;
		error = !BC_ASSERT_TRUE(callLog->getToAddress()->weakEqual(target->getToAddress())) || error;

		if (error) {
			ms_error("%s\n\tVS\n%s", target->toJson().c_str(), callLog->toJson().c_str());
		}
		return !error;
	}
	return true;
}

static bool _check_call_log_count(LinphoneCoreManager *account, size_t count) {
	auto logs = linphone_core_get_call_logs(account->lc);
	if (!BC_ASSERT_TRUE(bctbx_list_size(logs) == count)) {
		// Debug assert
		lWarning() << "[SMCL] Count log expected " << count << " but was " << bctbx_list_size(logs);
		auto it = logs;
		while (it) {
			auto log = CallLog::toCpp((LinphoneCallLog *)bctbx_list_get_data(it));
			lWarning() << log->toString();
			it = bctbx_list_next(it);
		}
		return false;
	}
	return true;
}

static void sync_message_api(void) {
	LinphoneCoreManager *marie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_new("pauline_rc");

	// Check New
	auto log = linphone_call_log_new(marie->lc, LinphoneCallOutgoing, marie->identity, pauline->identity);
	auto log2 = linphone_call_log_new(marie->lc, LinphoneCallIncoming, marie->identity, pauline->identity);
	BC_ASSERT_EQUAL(linphone_call_log_get_dir(log), LinphoneCallOutgoing, int, "%i");
	BC_ASSERT_TRUE(linphone_address_equal(linphone_call_log_get_from_address(log), marie->identity));
	BC_ASSERT_TRUE(linphone_address_equal(linphone_call_log_get_to_address(log), pauline->identity));

	// Check clone
	auto logBis = linphone_call_log_clone(log);
	auto json = linphone_call_log_to_json(log);
	auto jsonBis = linphone_call_log_to_json(logBis);
	auto json2 = linphone_call_log_to_json(log2);
	BC_ASSERT_STRING_EQUAL(json, jsonBis);

	// Check JSON
	linphone_call_log_from_json(log, json2);
	bctbx_free(json);
	json = linphone_call_log_to_json(log);
	BC_ASSERT_STRING_EQUAL(json, json2);

	// Check help
	// If tests fail, update also documentation in linphone_call_log_from_json()
	auto jsonHelp = CallLog::toCpp(log)->getJsonHelp();
	BC_ASSERT_TRUE(jsonHelp.size() == 8);
	BC_ASSERT_TRUE(jsonHelp.count("version") > 0);
	BC_ASSERT_TRUE(jsonHelp.count("direction") > 0);
	BC_ASSERT_TRUE(jsonHelp.count("call_id") > 0);
	BC_ASSERT_TRUE(jsonHelp.count("start_dt") > 0);
	BC_ASSERT_TRUE(jsonHelp.count("from") > 0);
	BC_ASSERT_TRUE(jsonHelp.count("to") > 0);
	BC_ASSERT_TRUE(jsonHelp.count("status") > 0);
	BC_ASSERT_TRUE(jsonHelp.count("duration_s") > 0);

	// Check enums regressions and docs
	// Do not put 'default' to allow breaking build.
	// If the build breaks or assert: update Call::kCallStatusStrings
	switch (const auto status = CallLog::toCpp(log)->getStatus()) {
		case LinphoneCallSuccess:
			BC_ASSERT_TRUE(Call::callStatusToText(status).size() > 0);
			break;
		case LinphoneCallDeclined:
			BC_ASSERT_TRUE(Call::callStatusToText(status).size() > 0);
			break;
		case LinphoneCallMissed:
			BC_ASSERT_TRUE(Call::callStatusToText(status).size() > 0);
			break;
		case LinphoneCallAborted:
			BC_ASSERT_TRUE(Call::callStatusToText(status).size() > 0);
			break;
		case LinphoneCallAcceptedElsewhere:
			BC_ASSERT_TRUE(Call::callStatusToText(status).size() > 0);
			break;
		case LinphoneCallDeclinedElsewhere:
			BC_ASSERT_TRUE(Call::callStatusToText(status).size() > 0);
			break;
		case LinphoneCallEarlyAborted:
			BC_ASSERT_TRUE(Call::callStatusToText(status).size() > 0);
			break;
	}
	// If the build breaks or assert: update Call::kCallDirStrings
	switch (const auto direction = CallLog::toCpp(log)->getDirection()) {
		case LinphoneCallIncoming:
			BC_ASSERT_TRUE(Call::callDirToText(direction).size() > 0);
			break;
		case LinphoneCallOutgoing:
			BC_ASSERT_TRUE(Call::callDirToText(direction).size() > 0);
			break;
	}

	bctbx_free(json2);
	bctbx_free(jsonBis);
	bctbx_free(json);
	linphone_call_log_unref(logBis);
	linphone_call_log_unref(log2);
	linphone_call_log_unref(log);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(marie);
}

static void sync_message_call_logs(bool_t with_call_log_server) {
	LinphoneCoreManager *dummyMarie = linphone_core_manager_new("marie_rc"); // Use dummy to be able to send message
	LinphoneCoreManager *onlineMarie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *offlineMarie = linphone_core_manager_new("marie_rc");
	LinphoneCoreManager *pauline = linphone_core_manager_new("pauline_rc");
	bctbx_list_t *lcs = bctbx_list_append(NULL, pauline->lc);
	lcs = bctbx_list_append(lcs, dummyMarie->lc);
	lcs = bctbx_list_append(lcs, onlineMarie->lc);
	lcs = bctbx_list_append(lcs, offlineMarie->lc);
	lcs = bctbx_list_append(lcs, pauline->lc);

	enum TestCases { MISSED_CASE = 0, DECLINED_CASE, ACCEPTED_CASE };

	for (int testCaseIdx = MISSED_CASE; testCaseIdx <= ACCEPTED_CASE; ++testCaseIdx) {
		// offlineMarie goes to Offline
		linphone_core_set_network_reachable(offlineMarie->lc, FALSE);

		// Counts
		int incomingCount = onlineMarie->stat.number_of_LinphoneCallIncomingReceived;
		int outgoingCount = pauline->stat.number_of_LinphoneCallOutgoingProgress;
		size_t offlineLogCount = bctbx_list_size(linphone_core_get_call_logs(offlineMarie->lc));
		size_t onlineLogCount = bctbx_list_size(linphone_core_get_call_logs(onlineMarie->lc));
		size_t paulineLogCount = bctbx_list_size(linphone_core_get_call_logs(pauline->lc));
		int callLogUpdated = offlineMarie->stat.number_of_LinphoneCoreCallLogUpdated;

		// Call generation
		LinphoneCall *paulineCall = linphone_core_invite_address(pauline->lc, offlineMarie->identity);
		BC_ASSERT_TRUE(
		    wait_for_list(lcs, &onlineMarie->stat.number_of_LinphoneCallIncomingReceived, incomingCount + 1, 10000));
		BC_ASSERT_TRUE(
		    wait_for_list(lcs, &pauline->stat.number_of_LinphoneCallOutgoingProgress, outgoingCount + 1, 5000));
		wait_for_list(lcs, NULL, 0, 1000);

		BC_ASSERT_TRUE(_check_call_log_count(offlineMarie, offlineLogCount));
		BC_ASSERT_TRUE(_check_call_log_count(onlineMarie, onlineLogCount + 1));
		BC_ASSERT_TRUE(_check_call_log_count(pauline, paulineLogCount + 1));

		LinphoneCall *marieCall = linphone_core_get_current_call(onlineMarie->lc);

		// Action
		switch (testCaseIdx) {
			case MISSED_CASE:
				linphone_call_terminate(paulineCall);
				break;
			case DECLINED_CASE:
				linphone_call_decline(marieCall, LinphoneReasonDeclined);
				break;
			case ACCEPTED_CASE:
				linphone_call_accept(marieCall);
				wait_for_list(lcs, NULL, 0, 100);
				linphone_call_terminate(marieCall);
				break;
			default: {
			}
		}

		BC_ASSERT_TRUE(_check_call_log_count(offlineMarie, offlineLogCount));
		BC_ASSERT_TRUE(_check_call_log_count(onlineMarie, onlineLogCount + 1));

		auto paulineCallLog = CallLog::toCpp(linphone_call_get_call_log(paulineCall));
		wait_for_list(lcs, NULL, 0, 1000); // 1s to be able to check if start_time has changed

		// Build a missed call log
		auto onlineCallLogRef = paulineCallLog->clone()->toSharedPtr();
		auto offlineCallLogRef = paulineCallLog->clone()->toSharedPtr();

		switch (testCaseIdx) {
			case MISSED_CASE:
				onlineCallLogRef->setDirection(LinphoneCallIncoming);
				onlineCallLogRef->setStatus(LinphoneCallMissed);
				offlineCallLogRef->setDirection(LinphoneCallIncoming);
				offlineCallLogRef->setStatus(LinphoneCallMissed);
				break;
			case DECLINED_CASE:
				onlineCallLogRef->setDirection(LinphoneCallIncoming);
				onlineCallLogRef->setStatus(LinphoneCallDeclined);
				offlineCallLogRef->setDirection(LinphoneCallIncoming);
				offlineCallLogRef->setStatus(LinphoneCallDeclinedElsewhere);
				break;
			case ACCEPTED_CASE:
				onlineCallLogRef->setDirection(LinphoneCallIncoming);
				onlineCallLogRef->setStatus(LinphoneCallSuccess);
				offlineCallLogRef->setDirection(LinphoneCallIncoming);
				offlineCallLogRef->setStatus(LinphoneCallAcceptedElsewhere);
				break;
			default: {
			}
		}
		LinphoneChatRoom *room = nullptr;
		if (!with_call_log_server) {
			// Get the Chat room service
			room = linphone_core_get_chat_room(dummyMarie->lc, offlineMarie->identity);
			if (!room) {
				auto chatRoomParams = linphone_core_create_default_chat_room_params(dummyMarie->lc);
				bctbx_list_t *participants = NULL;
				bctbx_list_append(participants, offlineMarie->identity);
				room = linphone_core_create_chat_room(dummyMarie->lc, chatRoomParams, offlineMarie->identity,
				                                      "CallLogsSync", participants);
				linphone_chat_room_params_unref(chatRoomParams);
			}

			// Send the call log
			auto chatMessage = linphone_chat_room_create_message_from_call_log(room, offlineCallLogRef->toC());
			BC_ASSERT_TRUE(linphone_chat_message_has_call_log_json_content(chatMessage));
			auto contents = linphone_chat_message_get_contents(chatMessage);
			while (contents) {
				auto c = static_cast<LinphoneContent *>(bctbx_list_get_data(contents));
				BC_ASSERT_TRUE(linphone_content_is_call_log_json(c));
				contents = bctbx_list_next(contents);
			}
			linphone_chat_room_send_chat_message(room, chatMessage);
			linphone_chat_message_unref(chatMessage);
		}
		// Check log count before reconnection
		BC_ASSERT_TRUE(_check_call_log_count(offlineMarie, offlineLogCount));
		BC_ASSERT_TRUE(_check_call_log_count(onlineMarie, onlineLogCount + 1));

		// RECONNECTION
		int offlineRegisterCount = offlineMarie->stat.number_of_LinphoneRegistrationOk;
		linphone_core_set_network_reachable(offlineMarie->lc, TRUE);
		BC_ASSERT_TRUE(wait_for(offlineMarie->lc, NULL, &offlineMarie->stat.number_of_LinphoneRegistrationOk,
		                        offlineRegisterCount + 1));
		if (!BC_ASSERT_TRUE(wait_for_list(lcs, &offlineMarie->stat.number_of_LinphoneCoreCallLogUpdated,
		                                  callLogUpdated + 1, 10000)))
			lWarning() << "[SMCL] offline number_of_LinphoneCoreCallLogUpdated="
			           << offlineMarie->stat.number_of_LinphoneCoreCallLogUpdated;

		// Check Offline: no messages in chat room (=call log has been processed) and have the sent call log.
		BC_ASSERT_TRUE(!!(room = linphone_core_get_chat_room(offlineMarie->lc, offlineMarie->identity)));
		BC_ASSERT_EQUAL(linphone_chat_room_get_history_size(room), 0, int, "%i");
		// Check Online: no messages in chat room (=call log has been processed) and have a new call log.
		BC_ASSERT_TRUE(!!(room = linphone_core_get_chat_room(onlineMarie->lc, onlineMarie->identity)));
		BC_ASSERT_EQUAL(linphone_chat_room_get_history_size(room), 0, int, "%i");
		// Check: Log count
		BC_ASSERT_TRUE(_check_call_log_count(offlineMarie, offlineLogCount + 1));
		BC_ASSERT_TRUE(_check_call_log_count(onlineMarie, onlineLogCount + 1));

		// Duration can be different (duration at 0 first and then having updated data)
		const bctbx_list_t *offlineMarieLogs = linphone_core_get_call_logs(offlineMarie->lc);
		if (offlineMarieLogs) {
			offlineCallLogRef->setDuration(
			    CallLog::toCpp(static_cast<LinphoneCallLog *>(bctbx_list_get_data(offlineMarieLogs)))->getDuration());
		}
		const bctbx_list_t *onlineMarieLogs = linphone_core_get_call_logs(onlineMarie->lc);
		if (onlineMarieLogs) {
			onlineCallLogRef->setDuration(
			    CallLog::toCpp(static_cast<LinphoneCallLog *>(bctbx_list_get_data(onlineMarieLogs)))->getDuration());
		}
		BC_ASSERT_TRUE(_check_call_log(offlineMarie, offlineCallLogRef));
		BC_ASSERT_TRUE(_check_call_log(onlineMarie, onlineCallLogRef));
	}

	bctbx_list_free(lcs);
	linphone_core_manager_destroy(pauline);
	linphone_core_manager_destroy(offlineMarie);
	linphone_core_manager_destroy(onlineMarie);
	linphone_core_manager_destroy(dummyMarie);
}

static void sync_message_call_logs_standalone() {
	sync_message_call_logs(FALSE);
}

static void sync_message_call_logs_with_call_log_server() {
	sync_message_call_logs(TRUE);
}

// clang-format off
static test_t sync_message_tests[] = {
	TEST_NO_TAG("Sync message API", sync_message_api),
	TEST_NO_TAG("Sync message call logs standalone", sync_message_call_logs_standalone),	// Internal call log generation
	TEST_NO_TAG("Sync message call logs with call log server", sync_message_call_logs_with_call_log_server)	// With call log server
};
// clang-format on

test_suite_t sync_test_suite = {"Sync Message",
                                NULL,
                                NULL,
                                liblinphone_tester_before_each,
                                liblinphone_tester_after_each,
                                sizeof(sync_message_tests) / sizeof(sync_message_tests[0]),
                                sync_message_tests,
                                0};

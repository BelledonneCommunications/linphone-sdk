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

#include "bctoolbox/defs.h"

#include "c-wrapper/c-wrapper.h"
#include "chat/chat-room/abstract-chat-room.h"
#include "chat/chat-room/basic-chat-room.h"
#include "conference/conference-params.h"
#include "core/core-p.h"
#include "linphone/api/c-address.h"
#include "linphone/api/c-chat-room-params.h"
#include "linphone/chat.h"
#include "linphone/core.h"
#include "linphone/lpconfig.h"
#include "linphone/utils/utils.h"
#include "linphone/wrapper_utils.h"
#include "private.h"

using namespace std;
using namespace LinphonePrivate;

void linphone_core_disable_chat(LinphoneCore *lc, LinphoneReason deny_reason) {
	lc->chat_deny_code = deny_reason;
}

void linphone_core_enable_chat(LinphoneCore *lc) {
	lc->chat_deny_code = LinphoneReasonNone;
}

bool_t linphone_core_chat_enabled(const LinphoneCore *lc) {
	return lc->chat_deny_code != LinphoneReasonNone;
}

const bctbx_list_t *linphone_core_get_chat_rooms(LinphoneCore *lc) {
	return L_GET_CPP_PTR_FROM_C_OBJECT(lc)->getChatRoomsCList();
}

LinphoneChatRoom *linphone_core_create_client_group_chat_room(LinphoneCore *lc, const char *subject, bool_t fallback) {
	return linphone_core_create_client_group_chat_room_2(lc, subject, fallback, FALSE);
}

// Deprecated see linphone_core_create_chat_room_7
LinphoneChatRoom *linphone_core_create_client_group_chat_room_2(LinphoneCore *lc,
                                                                const char *subject,
                                                                bool_t fallback,
                                                                bool_t encrypted) {
	return L_GET_PRIVATE_FROM_C_OBJECT(lc)
	    ->createClientChatRoom(L_C_TO_STRING(subject), !!fallback, !!encrypted)
	    ->toC();
}

// Deprecated see linphone_core_create_chat_room_7
LinphoneChatRoom *linphone_core_create_chat_room(LinphoneCore *lc,
                                                 const LinphoneChatRoomParams *params,
                                                 const LinphoneAddress *localAddr,
                                                 const char *subject,
                                                 const bctbx_list_t *participants) {
	LinphoneChatRoomParams *params2 = linphone_chat_room_params_clone(params);
	linphone_chat_room_params_set_subject(params2, subject);
	LinphoneChatRoom *result = linphone_core_create_chat_room_6(lc, params2, localAddr, participants);
	linphone_chat_room_params_unref(params2);
	return result;
}

// Deprecated see linphone_core_create_chat_room_7
LinphoneChatRoom *linphone_core_create_chat_room_2(LinphoneCore *lc,
                                                   const LinphoneChatRoomParams *params,
                                                   const char *subject,
                                                   const bctbx_list_t *participants) {
	LinphoneChatRoomParams *params2 = linphone_chat_room_params_clone(params);
	linphone_chat_room_params_set_subject(params2, subject);
	LinphoneChatRoom *result = linphone_core_create_chat_room_6(lc, params2, NULL, participants);
	linphone_chat_room_params_unref(params2);
	return result;
}

// Deprecated see linphone_core_create_chat_room_7
LinphoneChatRoom *
linphone_core_create_chat_room_3(LinphoneCore *lc, const char *subject, const bctbx_list_t *participants) {
	LinphoneChatRoomParams *params = linphone_core_create_default_chat_room_params(lc);
	linphone_chat_room_params_set_subject(params, subject);
	LinphoneChatRoom *result = linphone_core_create_chat_room_6(lc, params, NULL, participants);
	linphone_chat_room_params_unref(params);
	return result;
}

// Deprecated see linphone_core_create_chat_room_7
LinphoneChatRoom *linphone_core_create_chat_room_4(LinphoneCore *lc,
                                                   const LinphoneChatRoomParams *params,
                                                   const LinphoneAddress *localAddr,
                                                   const LinphoneAddress *participant) {
	bctbx_list_t *paricipants = bctbx_list_prepend(NULL, (LinphoneAddress *)participant);
	LinphoneChatRoom *result = linphone_core_create_chat_room_6(lc, params, localAddr, paricipants);
	bctbx_list_free(paricipants);
	return result;
}

// Deprecated see linphone_core_create_chat_room_7
LinphoneChatRoom *linphone_core_create_chat_room_5(LinphoneCore *lc, const LinphoneAddress *participant) {
	bctbx_list_t *paricipants = bctbx_list_prepend(NULL, (LinphoneAddress *)participant);
	LinphoneChatRoomParams *params = linphone_core_create_default_chat_room_params(lc);
	LinphoneChatRoom *result = linphone_core_create_chat_room_6(lc, params, NULL, paricipants);
	linphone_chat_room_params_unref(params);
	bctbx_list_free(paricipants);
	return result;
}

// Deprecated see linphone_core_create_chat_room_7
LinphoneChatRoom *linphone_core_create_chat_room_6(LinphoneCore *lc,
                                                   const LinphoneChatRoomParams *params,
                                                   const LinphoneAddress *localAddr,
                                                   const bctbx_list_t *participants) {
	LinphoneConferenceParams *cloned_params = NULL;
	if (params) {
		cloned_params = linphone_conference_params_clone(params);
	} else {
		cloned_params = linphone_core_create_conference_params_2(lc, NULL);
	}
	if (!linphone_conference_params_get_account(cloned_params) && localAddr) {
		linphone_conference_params_set_account(cloned_params, linphone_core_lookup_account_by_identity(lc, localAddr));
	}
	LinphoneChatRoom *chat_room = linphone_core_create_chat_room_7(lc, cloned_params, participants);
	if (cloned_params) {
		linphone_conference_params_unref(cloned_params);
	}
	return chat_room;
}

LinphoneChatRoom *linphone_core_create_chat_room_7(LinphoneCore *lc,
                                                   const LinphoneConferenceParams *params,
                                                   const bctbx_list_t *participants) {
	CoreLogContextualizer logContextualizer(lc);
	shared_ptr<LinphonePrivate::ConferenceParams> conferenceParams =
	    params ? LinphonePrivate::ConferenceParams::toCpp(params)->clone()->toSharedPtr() : nullptr;
	const list<std::shared_ptr<LinphonePrivate::Address>> participantsList =
	    LinphonePrivate::Utils::bctbxListToCppSharedPtrList<LinphoneAddress, Address>(participants);
	shared_ptr<LinphonePrivate::AbstractChatRoom> room =
	    L_GET_PRIVATE_FROM_C_OBJECT(lc)->createChatRoom(conferenceParams, participantsList);
	if (room) {
		auto cRoom = room->toC();
		linphone_chat_room_ref(cRoom);
		return cRoom;
	}
	return NULL;
}

LinphoneChatRoom *linphone_core_search_chat_room(const LinphoneCore *lc,
                                                 const LinphoneChatRoomParams *params,
                                                 const LinphoneAddress *localAddr,
                                                 const LinphoneAddress *remoteAddr,
                                                 const bctbx_list_t *participants) {
	return linphone_core_search_chat_room_2(lc, params, localAddr, remoteAddr, participants);
}

LinphoneChatRoom *linphone_core_search_chat_room_2(const LinphoneCore *lc,
                                                   const LinphoneConferenceParams *params,
                                                   const LinphoneAddress *localAddr,
                                                   const LinphoneAddress *remoteAddr,
                                                   const bctbx_list_t *participants) {

	CoreLogContextualizer logContextualizer(lc);
	shared_ptr<LinphonePrivate::ConferenceParams> conferenceParams =
	    params ? LinphonePrivate::ConferenceParams::toCpp(params)->clone()->toSharedPtr() : nullptr;
	const list<std::shared_ptr<LinphonePrivate::Address>> participantsList =
	    LinphonePrivate::Utils::bctbxListToCppSharedPtrList<LinphoneAddress, Address>(participants);
	shared_ptr<const LinphonePrivate::Address> localAddress =
	    localAddr ? LinphonePrivate::Address::toCpp(localAddr)->getSharedFromThis() : nullptr;
	shared_ptr<const LinphonePrivate::Address> remoteAddress =
	    remoteAddr ? LinphonePrivate::Address::toCpp(remoteAddr)->getSharedFromThis() : nullptr;
	shared_ptr<LinphonePrivate::AbstractChatRoom> room = L_GET_CPP_PTR_FROM_C_OBJECT(lc)->searchChatRoom(
	    conferenceParams, localAddress, remoteAddress, participantsList);
	if (room) return room->toC();
	return NULL;
}

LinphoneChatRoom *linphone_core_search_chat_room_by_identifier(const LinphoneCore *lc, const char *identifier) {
	shared_ptr<LinphonePrivate::AbstractChatRoom> room =
	    L_GET_CPP_PTR_FROM_C_OBJECT(lc)->searchChatRoom(L_C_TO_STRING(identifier));
	if (room) return room->toC();
	return NULL;
}

LinphoneChatRoomParams *linphone_core_create_default_chat_room_params(LinphoneCore *lc) {
	auto params = linphone_chat_room_params_new_and_init(lc);
	return params;
}

static LinphoneChatRoomParams *_linphone_core_create_default_chat_room_params() {
	auto params = linphone_chat_room_params_new();
	return params;
}

void linphone_core_delete_chat_room(LinphoneCore *lc, LinphoneChatRoom *cr) {
	CoreLogContextualizer logContextualizer(lc);
	LinphonePrivate::AbstractChatRoom::toCpp(cr)->deleteFromDb();
}

// Deprecated see linphone_core_search_chat_room_2
LinphoneChatRoom *linphone_core_get_chat_room(LinphoneCore *lc, const LinphoneAddress *peerAddr) {
	return linphone_core_get_chat_room_2(lc, peerAddr, NULL);
}

// Deprecated see linphone_core_search_chat_room_2
LinphoneChatRoom *
linphone_core_get_chat_room_2(LinphoneCore *lc, const LinphoneAddress *peer_addr, const LinphoneAddress *local_addr) {
	LinphoneChatRoom *result = linphone_core_search_chat_room_2(lc, NULL, local_addr, peer_addr, NULL);
	if (result == NULL) {
		bctbx_list_t *paricipants = bctbx_list_prepend(NULL, (LinphoneAddress *)peer_addr);
		LinphoneChatRoomParams *params = linphone_core_create_default_chat_room_params(lc);
		linphone_chat_room_params_set_backend(params, LinphoneChatRoomBackendBasic);
		linphone_chat_room_params_enable_group(params, FALSE);
		result = linphone_core_create_chat_room_6(lc, params, local_addr, paricipants);
		if (result) linphone_chat_room_unref(result);
		linphone_chat_room_params_unref(params);
		bctbx_list_free(paricipants);
	}
	return result;
}

// Deprecated see linphone_core_search_chat_room_2
LinphoneChatRoom *linphone_core_get_chat_room_from_uri(LinphoneCore *lc, const char *to) {
	LinphoneAddress *addr = linphone_core_interpret_url(lc, to);
	LinphoneChatRoom *room = linphone_core_get_chat_room(lc, addr);
	if (addr) linphone_address_unref(addr);
	return room;
}

// Deprecated see linphone_core_search_chat_room_2
LinphoneChatRoom *linphone_core_find_chat_room(const LinphoneCore *lc,
                                               const LinphoneAddress *peer_addr,
                                               const LinphoneAddress *local_addr) {
	LinphoneChatRoom *result = linphone_core_search_chat_room_2(lc, NULL, local_addr, peer_addr, NULL);
	return result;
}

// Deprecated see linphone_core_search_chat_room_2
LinphoneChatRoom *linphone_core_find_one_to_one_chat_room(const LinphoneCore *lc,
                                                          const LinphoneAddress *local_addr,
                                                          const LinphoneAddress *participant_addr) {
	bctbx_list_t *paricipants = bctbx_list_prepend(NULL, (LinphoneAddress *)participant_addr);
	LinphoneChatRoomParams *params = _linphone_core_create_default_chat_room_params();
	linphone_chat_room_params_set_backend(params, LinphoneChatRoomBackendFlexisipChat);
	linphone_chat_room_params_enable_group(params, FALSE);
	LinphoneChatRoom *result = linphone_core_search_chat_room(lc, params, local_addr, NULL, paricipants);
	if (!result) {
		linphone_chat_room_params_set_backend(params, LinphoneChatRoomBackendBasic);
		result = linphone_core_search_chat_room(lc, params, local_addr, participant_addr, NULL);
	}
	linphone_chat_room_params_unref(params);
	bctbx_list_free(paricipants);
	return result;
}

// Deprecated see linphone_core_search_chat_room_2
LinphoneChatRoom *linphone_core_find_one_to_one_chat_room_2(const LinphoneCore *lc,
                                                            const LinphoneAddress *local_addr,
                                                            const LinphoneAddress *participant_addr,
                                                            bool_t encrypted) {
	bctbx_list_t *paricipants = bctbx_list_prepend(NULL, (LinphoneAddress *)participant_addr);
	LinphoneChatRoomParams *params = _linphone_core_create_default_chat_room_params();
	linphone_chat_room_params_set_backend(params, LinphoneChatRoomBackendFlexisipChat);
	linphone_chat_room_params_enable_group(params, FALSE);
	linphone_chat_room_params_enable_encryption(params, encrypted);
	LinphoneChatRoom *result = linphone_core_search_chat_room(lc, params, local_addr, NULL, paricipants);
	if (!result && !encrypted) {
		linphone_chat_room_params_set_backend(params, LinphoneChatRoomBackendBasic);
		result = linphone_core_search_chat_room(lc, params, local_addr, participant_addr, NULL);
	}
	linphone_chat_room_params_unref(params);
	bctbx_list_free(paricipants);
	return result;
}

LinphoneReason linphone_core_message_received(LinphoneCore *lc, LinphonePrivate::SalOp *op, const SalMessage *sal_msg) {
	return L_GET_CPP_PTR_FROM_C_OBJECT(lc)->onSipMessageReceived(op, sal_msg);
}

unsigned int linphone_chat_message_store(BCTBX_UNUSED(LinphoneChatMessage *msg)) {
	// DO nothing, just for old JNI compat...
	return 1;
}

unsigned int linphone_core_get_remaining_download_file_count(LinphoneCore *lc) {
	return L_GET_CPP_PTR_FROM_C_OBJECT(lc)->getRemainingDownloadFileCount();
}

unsigned int linphone_core_get_remaining_upload_file_count(LinphoneCore *lc) {
	return L_GET_CPP_PTR_FROM_C_OBJECT(lc)->getRemainingUploadFileCount();
}

void linphone_core_set_queued_message_resend_period(LinphoneCore *lc, long seconds) {
	CoreLogContextualizer logContextualizer(lc);
	L_GET_CPP_PTR_FROM_C_OBJECT(lc)->setQueuedMessageResendPeriod(seconds);
}

long linphone_core_get_queued_message_resend_period(const LinphoneCore *lc) {
	return L_GET_CPP_PTR_FROM_C_OBJECT(lc)->getQueuedMessageResendPeriod();
}

void linphone_core_set_imdn_resend_period(LinphoneCore *lc, long seconds) {
	CoreLogContextualizer logContextualizer(lc);
	L_GET_CPP_PTR_FROM_C_OBJECT(lc)->setImdnResendPeriod(seconds);
}

long linphone_core_get_imdn_resend_period(const LinphoneCore *lc) {
	return L_GET_CPP_PTR_FROM_C_OBJECT(lc)->getImdnResendPeriod();
}

int linphone_core_get_imdn_to_everybody_threshold(const LinphoneCore *core) {
	return L_GET_CPP_PTR_FROM_C_OBJECT(core)->getImdnToEverybodyThreshold();
}

void linphone_core_set_imdn_to_everybody_threshold(LinphoneCore *core, int threshold) {
	linphone_config_set_int(core->config, "chat", "imdn_to_everybody_threshold", threshold);
	L_GET_CPP_PTR_FROM_C_OBJECT(core)->setImdnToEverybodyThreshold(threshold);
}

bool_t linphone_core_empty_chatrooms_deletion_enabled(const LinphoneCore *core) {
	return L_GET_CPP_PTR_FROM_C_OBJECT(core)->emptyChatroomsDeletionEnabled();
}

void linphone_core_enable_empty_chatrooms_deletion(LinphoneCore *core, bool_t enable) {
	linphone_config_set_bool(core->config, "misc", "empty_chat_room_deletion", enable);
	L_GET_CPP_PTR_FROM_C_OBJECT(core)->enableEmptyChatroomsDeletion(enable);
}

int linphone_core_get_max_participants_per_chatroom(const LinphoneCore *core) {
	return linphone_config_get_int(linphone_core_get_config(core), "chat", "max_participants_per_chatroom", -1);
}

void linphone_core_set_max_participants_per_chatroom(LinphoneCore *core, int max_participants) {
	linphone_config_set_int(linphone_core_get_config(core), "chat", "max_participants_per_chatroom", max_participants);
}

bool_t linphone_core_is_sender_name_hidden_in_forward_message(LinphoneCore *lc) {
	return lc->sender_name_hidden_in_forward_message;
}

void linphone_core_enable_sender_name_hidden_in_forward_message(LinphoneCore *lc, bool_t enable) {
	lc->sender_name_hidden_in_forward_message = enable;
	linphone_config_set_int(lc->config, "app", "sender_name_hidden_in_forward_message", enable);
}

int linphone_core_get_unread_chat_message_count(const LinphoneCore *lc) {
	return L_GET_CPP_PTR_FROM_C_OBJECT(lc)->getUnreadChatMessageCount();
}

int linphone_core_get_unread_chat_message_count_from_local(const LinphoneCore *lc, const LinphoneAddress *address) {
	const auto addr = LinphonePrivate::Address::toCpp(address)->getSharedFromThis();
	return L_GET_CPP_PTR_FROM_C_OBJECT(lc)->getUnreadChatMessageCount(addr);
}

int linphone_core_get_unread_chat_message_count_from_active_locals(const LinphoneCore *lc) {
	return L_GET_CPP_PTR_FROM_C_OBJECT(lc)->getUnreadChatMessageCountFromActiveLocals();
}

bool_t linphone_core_send_message_after_notify_enabled(const LinphoneCore *core) {
	return linphone_config_get_bool(linphone_core_get_config(core), "chat", "send_message_after_notify", 0);
}

void linphone_core_enable_send_message_after_notify(LinphoneCore *core, bool_t enable) {
	linphone_config_set_bool(linphone_core_get_config(core), "chat", "send_message_after_notify", enable);
}

int linphone_core_get_message_sending_delay(const LinphoneCore *core) {
	return linphone_config_get_int(linphone_core_get_config(core), "misc", "delay_message_send_s", 10);
}

void linphone_core_set_message_sending_delay(LinphoneCore *core, int duration) {
	linphone_config_set_int(linphone_core_get_config(core), "misc", "delay_message_send_s", duration);
}

int linphone_core_get_message_sending_delay_app_ext(const LinphoneCore *core) {
	return linphone_config_get_int(linphone_core_get_config(core), "misc", "delay_message_send_app_ext_s", 3);
}

void linphone_core_set_message_sending_delay_app_ext(LinphoneCore *core, int duration) {
	linphone_config_set_int(linphone_core_get_config(core), "misc", "delay_message_send_app_ext_s", duration);
}

int linphone_core_get_chat_room_load_chunk_size(const LinphoneCore *core) {
	return linphone_config_get_int(linphone_core_get_config(core), "chat", "chat_room_load_chunk_size", -1);
}

void linphone_core_set_chat_room_load_chunk_size(LinphoneCore *core, int size) {
	linphone_config_set_int(linphone_core_get_config(core), "chat", "chat_room_load_chunk_size", size);
}

int linphone_core_get_message_automatic_resending_delay(const LinphoneCore *core) {
	return linphone_config_get_int(linphone_core_get_config(core), "chat", "message_automatic_resending_s", 10);
}

void linphone_core_set_message_automatic_resending_delay(LinphoneCore *core, int duration) {
	linphone_config_set_int(linphone_core_get_config(core), "chat", "message_automatic_resending_s", duration);
}

bool_t linphone_core_unify_chat_rooms_address(LinphoneCore *core) {
	return L_GET_CPP_PTR_FROM_C_OBJECT(core)->unifyChatRoomAddresses();
}

void linphone_core_enable_chat_room_address_unification(LinphoneCore *core, bool_t enable) {
	L_GET_CPP_PTR_FROM_C_OBJECT(core)->enableChatRoomAddressUnification(!!enable);
}

bool_t linphone_core_chat_room_address_unification_enabled(const LinphoneCore *core) {
	return L_GET_CPP_PTR_FROM_C_OBJECT(core)->chatRoomAddressUnificationEnabled();
}
void linphone_core_set_ephemeral_chat_message_policy(LinphoneCore *core,
                                                     const LinphoneEphemeralChatMessagePolicy policy) {
	CoreLogContextualizer logContextualizer(core);
	L_GET_CPP_PTR_FROM_C_OBJECT(core)->setEphemeralChatMessagePolicy(policy);
}

LinphoneEphemeralChatMessagePolicy linphone_core_get_ephemeral_chat_message_policy(const LinphoneCore *core) {
	CoreLogContextualizer logContextualizer(core);
	return L_GET_CPP_PTR_FROM_C_OBJECT(core)->getEphemeralChatMessagePolicy();
}

void linphone_core_enable_chat_message_files_deletion(LinphoneCore *core, bool_t enabled) {
	CoreLogContextualizer logContextualizer(core);
	L_GET_CPP_PTR_FROM_C_OBJECT(core)->enableChatMessageFileDeletion(!!enabled);
}

bool_t linphone_core_chat_message_files_deletion_enabled(LinphoneCore *core) {
	CoreLogContextualizer logContextualizer(core);
	return L_GET_CPP_PTR_FROM_C_OBJECT(core)->chatMessageFileDeletionEnabled() ? TRUE : FALSE;
}

void linphone_core_set_chat_message_files_directories(LinphoneCore *core, const bctbx_list_t *directories) {
	CoreLogContextualizer logContextualizer(core);
	L_GET_CPP_PTR_FROM_C_OBJECT(core)->setFileContentsDirectories(ListHolder<string>::buildFromCList(directories));
}

const bctbx_list_t *linphone_core_get_chat_message_files_directories(const LinphoneCore *core) {
	CoreLogContextualizer logContextualizer(core);
	return L_GET_CPP_PTR_FROM_C_OBJECT(core)->getFileContentsDirectories().getCList();
}

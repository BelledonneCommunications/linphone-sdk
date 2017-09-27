/*
 * c-chat-room.h
 * Copyright (C) 2017  Belledonne Communications SARL
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _C_CHAT_ROOM_H_
#define _C_CHAT_ROOM_H_

#include "linphone/api/c-types.h"

// =============================================================================

#ifdef __cplusplus
	extern "C" {
#endif // ifdef __cplusplus

/**
 * @addtogroup chatroom
 * @{
 */

/**
 * Acquire a reference to the chat room.
 * @param[in] cr The chat room.
 * @return The same chat room.
**/
LINPHONE_PUBLIC LinphoneChatRoom *linphone_chat_room_ref(LinphoneChatRoom *cr);

/**
 * Release reference to the chat room.
 * @param[in] cr The chat room.
**/
LINPHONE_PUBLIC void linphone_chat_room_unref(LinphoneChatRoom *cr);

/**
 * Retrieve the user pointer associated with the chat room.
 * @param[in] cr The chat room.
 * @return The user pointer associated with the chat room.
**/
LINPHONE_PUBLIC void *linphone_chat_room_get_user_data(const LinphoneChatRoom *cr);

/**
 * Assign a user pointer to the chat room.
 * @param[in] cr The chat room.
 * @param[in] ud The user pointer to associate with the chat room.
**/
LINPHONE_PUBLIC void linphone_chat_room_set_user_data(LinphoneChatRoom *cr, void *ud);

/**
 * Create a message attached to a dedicated chat room;
 * @param cr the chat room.
 * @param message text message, NULL if absent.
 * @return a new #LinphoneChatMessage
 */
LINPHONE_PUBLIC LinphoneChatMessage* linphone_chat_room_create_message(LinphoneChatRoom *cr,const char* message);

/**
 * Create a message attached to a dedicated chat room;
 * @param cr the chat room.
 * @param message text message, NULL if absent.
 * @param external_body_url the URL given in external body or NULL.
 * @param state the LinphoneChatMessage.State of the message.
 * @param time the time_t at which the message has been received/sent.
 * @param is_read TRUE if the message should be flagged as read, FALSE otherwise.
 * @param is_incoming TRUE if the message has been received, FALSE otherwise.
 * @return a new #LinphoneChatMessage
 */
LINPHONE_PUBLIC LinphoneChatMessage* linphone_chat_room_create_message_2(LinphoneChatRoom *cr, const char* message, const char* external_body_url, LinphoneChatMessageState state, time_t time, bool_t is_read, bool_t is_incoming);

 /**
 * Create a message attached to a dedicated chat room with a particular content.
 * Use #linphone_chat_room_send_message to initiate the transfer
 * @param cr the chat room.
 * @param initial_content #LinphoneContent initial content. #LinphoneCoreVTable.file_transfer_send is invoked later to notify file transfer progress and collect next chunk of the message if LinphoneContent.data is NULL.
 * @return a new #LinphoneChatMessage
 */
LINPHONE_PUBLIC LinphoneChatMessage* linphone_chat_room_create_file_transfer_message(LinphoneChatRoom *cr, const LinphoneContent* initial_content);

/**
 * get peer address \link linphone_core_get_chat_room() associated to \endlink this #LinphoneChatRoom
 * @param cr #LinphoneChatRoom object
 * @return #LinphoneAddress peer address
 */
LINPHONE_PUBLIC const LinphoneAddress* linphone_chat_room_get_peer_address(LinphoneChatRoom *cr);

/**
 * Send a message to peer member of this chat room.
 * @deprecated Use linphone_chat_room_send_chat_message() instead.
 * @param cr #LinphoneChatRoom object
 * @param msg message to be sent
 * @donotwrap
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED void linphone_chat_room_send_message(LinphoneChatRoom *cr, const char *msg);

/**
 * Send a message to peer member of this chat room.
 * @param cr #LinphoneChatRoom object
 * @param msg #LinphoneChatMessage message to be sent
 * @param status_cb LinphoneChatMessageStateChangeCb status callback invoked when message is delivered or could not be delivered. May be NULL
 * @param ud user data for the status cb.
 * @deprecated Use linphone_chat_room_send_chat_message() instead.
 * @note The LinphoneChatMessage must not be destroyed until the the callback is called.
 * The LinphoneChatMessage reference is transfered to the function and thus doesn't need to be unref'd by the application.
 * @donotwrap
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED void linphone_chat_room_send_message2(LinphoneChatRoom *cr, LinphoneChatMessage* msg,LinphoneChatMessageStateChangedCb status_cb,void* ud);

/**
 * Send a message to peer member of this chat room.
 * @param[in] cr LinphoneChatRoom object
 * @param[in] msg LinphoneChatMessage object
 * The state of the message sending will be notified via the callbacks defined in the LinphoneChatMessageCbs object that can be obtained
 * by calling linphone_chat_message_get_callbacks().
 * The LinphoneChatMessage reference is transfered to the function and thus doesn't need to be unref'd by the application.
 * @donotwrap
 */
LINPHONE_PUBLIC void linphone_chat_room_send_chat_message(LinphoneChatRoom *cr, LinphoneChatMessage *msg);

/**
 * Mark all messages of the conversation as read
 * @param[in] cr The #LinphoneChatRoom object corresponding to the conversation.
 */
LINPHONE_PUBLIC void linphone_chat_room_mark_as_read(LinphoneChatRoom *cr);

/**
 * Delete a message from the chat room history.
 * @param[in] cr The #LinphoneChatRoom object corresponding to the conversation.
 * @param[in] msg The #LinphoneChatMessage object to remove.
 */

LINPHONE_PUBLIC void linphone_chat_room_delete_message(LinphoneChatRoom *cr, LinphoneChatMessage *msg);

/**
 * Delete all messages from the history
 * @param[in] cr The #LinphoneChatRoom object corresponding to the conversation.
 */
LINPHONE_PUBLIC void linphone_chat_room_delete_history(LinphoneChatRoom *cr);

/**
 * Gets the number of messages in a chat room.
 * @param[in] cr The #LinphoneChatRoom object corresponding to the conversation for which size has to be computed
 * @return the number of messages.
 */
LINPHONE_PUBLIC int linphone_chat_room_get_history_size(LinphoneChatRoom *cr);

/**
 * Gets nb_message most recent messages from cr chat room, sorted from oldest to most recent.
 * @param[in] cr The #LinphoneChatRoom object corresponding to the conversation for which messages should be retrieved
 * @param[in] nb_message Number of message to retrieve. 0 means everything.
 * @return \bctbx_list{LinphoneChatMessage}
 */
LINPHONE_PUBLIC bctbx_list_t *linphone_chat_room_get_history(LinphoneChatRoom *cr,int nb_message);

/**
 * Gets the partial list of messages in the given range, sorted from oldest to most recent.
 * @param[in] cr The #LinphoneChatRoom object corresponding to the conversation for which messages should be retrieved
 * @param[in] begin The first message of the range to be retrieved. History most recent message has index 0.
 * @param[in] end The last message of the range to be retrieved. History oldest message has index of history size - 1 (use #linphone_chat_room_get_history_size to retrieve history size)
 * @return \bctbx_list{LinphoneChatMessage}
 */
LINPHONE_PUBLIC bctbx_list_t *linphone_chat_room_get_history_range(LinphoneChatRoom *cr, int begin, int end);

LINPHONE_PUBLIC LinphoneChatMessage * linphone_chat_room_find_message(LinphoneChatRoom *cr, const char *message_id);

/**
 * Notifies the destination of the chat message being composed that the user is typing a new message.
 * @param[in] cr The #LinphoneChatRoom object corresponding to the conversation for which a new message is being typed.
 */
LINPHONE_PUBLIC void linphone_chat_room_compose(LinphoneChatRoom *cr);

/**
 * Tells whether the remote is currently composing a message.
 * @param[in] cr The #LinphoneChatRoom object corresponding to the conversation.
 * @return TRUE if the remote is currently composing a message, FALSE otherwise.
 */
LINPHONE_PUBLIC bool_t linphone_chat_room_is_remote_composing(const LinphoneChatRoom *cr);

/**
 * Gets the number of unread messages in the chatroom.
 * @param[in] cr The #LinphoneChatRoom object corresponding to the conversation.
 * @return the number of unread messages.
 */
LINPHONE_PUBLIC int linphone_chat_room_get_unread_messages_count(LinphoneChatRoom *cr);

/**
 * Returns back pointer to #LinphoneCore object.
**/
LINPHONE_PUBLIC LinphoneCore* linphone_chat_room_get_core(const LinphoneChatRoom *cr);

/**
 * When realtime text is enabled #linphone_call_params_realtime_text_enabled, #LinphoneCoreIsComposingReceivedCb is call everytime a char is received from peer.
 * At the end of remote typing a regular #LinphoneChatMessage is received with committed data from #LinphoneCoreMessageReceivedCb.
 * @param[in] cr #LinphoneChatRoom object
 * @returns  RFC 4103/T.140 char
 */
LINPHONE_PUBLIC uint32_t linphone_chat_room_get_char(const LinphoneChatRoom *cr);

/**
 * Returns true if lime is available for given peer
 *
 * @return true if zrtp secrets have already been shared and ready to use
 */
LINPHONE_PUBLIC bool_t linphone_chat_room_lime_available(LinphoneChatRoom *cr);

/**
 * get Curent Call associated to this chatroom if any
 * To commit a message, use #linphone_chat_room_send_message
 * @param[in] room LinphoneChatRomm
 * @returns LinphoneCall or NULL.
 */
LINPHONE_PUBLIC LinphoneCall *linphone_chat_room_get_call(const LinphoneChatRoom *room);

/**
 * Get the LinphoneChatRoomCbs object associated with the LinphoneChatRoom.
 * @param[in] cr LinphoneChatRoom object
 * @return The LinphoneChatRoomCbs object associated with the LinphoneChatRoom
 */
LINPHONE_PUBLIC LinphoneChatRoomCbs * linphone_chat_room_get_callbacks (const LinphoneChatRoom *cr);

/**
 * Get the state of the chat room.
 * @param[in] cr LinphoneChatRoom object
 * @return The state of the chat room
 */
LINPHONE_PUBLIC LinphoneChatRoomState linphone_chat_room_get_state (const LinphoneChatRoom *cr);

/**
 * Add a participant to a chat room. This may fail if this type of chat room does not handle participants.
 * Use linphone_chat_room_can_handle_participants() to know if this chat room handles participants.
 * @param[in] cr A LinphoneChatRoom object
 * @param[in] addr The address of the participant to add to the chat room
 * @return The newly added participant or NULL in case of failure
 */
LINPHONE_PUBLIC LinphoneParticipant * linphone_chat_room_add_participant (LinphoneChatRoom *cr, const LinphoneAddress *addr);

/**
 * Add several participants to a chat room at once. This may fail if this type of chat room does not handle participants.
 * Use linphone_chat_room_can_handle_participants() to know if this chat room handles participants.
 * @param[in] cr A LinphoneChatRoom object
 * @param[in] addresses \bctbx_list{LinphoneAddress}
 */
LINPHONE_PUBLIC void linphone_chat_room_add_participants (LinphoneChatRoom *cr, const bctbx_list_t *addresses);

/**
 * Tells whether a chat room is able to handle participants.
 * @param[in] cr A LinphoneChatRoom object
 * @return A boolean value telling whether the chat room can handle participants or not
 */
LINPHONE_PUBLIC bool_t linphone_chat_room_can_handle_participants (const LinphoneChatRoom *cr);

/**
 * Get the conference address of the chat room.
 * @param[in] cr A LinphoneChatRoom object
 * @return The conference address of the chat room or NULL if this type of chat room is not conference based
 */
LINPHONE_PUBLIC const LinphoneAddress *linphone_chat_room_get_conference_address (const LinphoneChatRoom *cr);

/**
 * Get the number of participants in the chat room (that is without ourselves).
 * @param[in] cr A LinphoneChatRoom object
 * @return The number of participants in the chat room
 */
LINPHONE_PUBLIC int linphone_chat_room_get_nb_participants (const LinphoneChatRoom *cr);

/**
 * Get the list of participants of a chat room.
 * @param[in] cr A LinphoneChatRoom object
 * @return \bctbx_list{LinphoneParticipant}
 */
LINPHONE_PUBLIC bctbx_list_t * linphone_chat_room_get_participants (const LinphoneChatRoom *cr);

/**
 * Remove a participant of a chat room.
 * @param[in] cr A LinphoneChatRoom object
 * @param[in] participant The participant to remove from the chat room
 */
LINPHONE_PUBLIC void linphone_chat_room_remove_participant (LinphoneChatRoom *cr, LinphoneParticipant *participant);

/**
 * Remove several participants of a chat room at once.
 * @param[in] cr A LinphoneChatRoom object
 * @param[in] participants \bctbx_list{LinphoneParticipant}
 */
LINPHONE_PUBLIC void linphone_chat_room_remove_participants (LinphoneChatRoom *cr, const bctbx_list_t *participants);

/**
 * Returns back pointer to #LinphoneCore object.
 * @deprecated use linphone_chat_room_get_core()
 * @donotwrap
**/
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneCore* linphone_chat_room_get_lc(const LinphoneChatRoom *cr);

/**
 * Destroy a LinphoneChatRoom.
 * @param cr #LinphoneChatRoom object
 * @deprecated Use linphone_chat_room_unref() instead.
 * @donotwrap
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED void linphone_chat_room_destroy(LinphoneChatRoom *cr);

/**
 * @}
 */

#ifdef __cplusplus
	}
#endif // ifdef __cplusplus

#endif // ifndef _C_CHAT_ROOM_H_

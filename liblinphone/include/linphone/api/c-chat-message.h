/*
 * c-chat-message.h
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

#ifndef _C_CHAT_MESSAGE_H_
#define _C_CHAT_MESSAGE_H_
 
#include "linphone/api/c-types.h"
#include "linphone/api/c-chat-message-cbs.h"

#ifdef SQLITE_STORAGE_ENABLED
#include <sqlite3.h>
#endif

// =============================================================================

typedef enum _ImdnType {
	ImdnTypeDelivery,
	ImdnTypeDisplay
} ImdnType;

typedef enum _LinphoneChatMessageDir{
	LinphoneChatMessageIncoming,
	LinphoneChatMessageOutgoing
} LinphoneChatMessageDir;
 
// =============================================================================
 
#ifdef __cplusplus
    extern "C" {
#endif // ifdef __cplusplus
 
/**
 * @addtogroup chatmessage
 * @{
 */

/**
 * Acquire a reference to the chat message.
 * @param[in] cr The chat message.
 * @return The same chat message.
**/
LINPHONE_PUBLIC LinphoneChatMessage *linphone_chat_message_ref(LinphoneChatMessage *msg);

/**
 * Release reference to the chat message.
 * @param[in] cr The chat message.
**/
LINPHONE_PUBLIC void linphone_chat_message_unref(LinphoneChatMessage *msg);

/**
 * Retrieve the user pointer associated with the chat message.
 * @param[in] cr The chat message.
 * @return The user pointer associated with the chat message.
**/
LINPHONE_PUBLIC void *linphone_chat_message_get_user_data(const LinphoneChatMessage *msg);

/**
 * Assign a user pointer to the chat message.
 * @param[in] cr The chat message.
 * @param[in] ud The user pointer to associate with the chat message.
**/
LINPHONE_PUBLIC void linphone_chat_message_set_user_data(LinphoneChatMessage *msg, void *ud);

// =============================================================================

LINPHONE_PUBLIC const char * linphone_chat_message_get_external_body_url(const LinphoneChatMessage *msg);

LINPHONE_PUBLIC void linphone_chat_message_set_external_body_url(LinphoneChatMessage *msg, const char *external_body_url);

/**
 * Get the time the message was sent.
 */
LINPHONE_PUBLIC time_t linphone_chat_message_get_time(const LinphoneChatMessage* msg);

/**
 * Returns TRUE if the message has been sent, returns FALSE if the message has been received.
 * @param message the message
**/
LINPHONE_PUBLIC bool_t linphone_chat_message_is_outgoing(LinphoneChatMessage* msg);

/**
 * Set origin of the message
 * @param[in] message #LinphoneChatMessage obj
 * @param[in] from #LinphoneAddress origin of this message (copied)
 */
LINPHONE_PUBLIC void linphone_chat_message_set_from_address(LinphoneChatMessage* msg, const LinphoneAddress* from);

/**
 * Get origin of the message
 * @param[in] message #LinphoneChatMessage obj
 * @return #LinphoneAddress
 */
LINPHONE_PUBLIC const LinphoneAddress* linphone_chat_message_get_from_address(const LinphoneChatMessage* msg);

/**
 * Set destination of the message
 * @param[in] message #LinphoneChatMessage obj
 * @param[in] addr #LinphoneAddress destination of this message (copied)
 */
LINPHONE_PUBLIC void linphone_chat_message_set_to_address(LinphoneChatMessage* msg, const LinphoneAddress* addr);

/**
 * Get destination of the message
 * @param[in] message #LinphoneChatMessage obj
 * @return #LinphoneAddress
 */
LINPHONE_PUBLIC const LinphoneAddress* linphone_chat_message_get_to_address(const LinphoneChatMessage* msg);

/**
 * Get the content type of a chat message.
 * @param[in] message LinphoneChatMessage object
 * @return The content type of the chat message
 */
LINPHONE_PUBLIC const char * linphone_chat_message_get_content_type(const LinphoneChatMessage *msg);

/**
 * Set the content type of a chat message.
 * This content type must match a content that is text representable, such as text/plain, text/html or image/svg+xml.
 * @param[in] message LinphoneChatMessage object
 * @param[in] content_type The new content type of the chat message
 */
LINPHONE_PUBLIC void linphone_chat_message_set_content_type(LinphoneChatMessage *msg, const char *content_type);

/**
 * Get text part of this message
 * @return text or NULL if no text.
 */
LINPHONE_PUBLIC const char* linphone_chat_message_get_text(const LinphoneChatMessage* msg);

/**
 * Returns the id used to identify this message in the storage database
 * @param message the message
 * @return the id
 */
LINPHONE_PUBLIC unsigned int linphone_chat_message_get_storage_id(LinphoneChatMessage* msg);

/**
 * Get the message identifier.
 * It is used to identify a message so that it can be notified as delivered and/or displayed.
 * @param[in] cm LinphoneChatMessage object
 * @return The message identifier.
 */
LINPHONE_PUBLIC const char* linphone_chat_message_get_message_id(const LinphoneChatMessage *msg);

/**
 * Linphone message has an app-specific field that can store a text. The application might want
 * to use it for keeping data over restarts, like thumbnail image path.
 * @param message #LinphoneChatMessage
 * @return the application-specific data or NULL if none has been stored.
 */
LINPHONE_PUBLIC const char* linphone_chat_message_get_appdata(const LinphoneChatMessage* message);

/**
 * Linphone message has an app-specific field that can store a text. The application might want
 * to use it for keeping data over restarts, like thumbnail image path.
 *
 * Invoking this function will attempt to update the message storage to reflect the changeif it is
 * enabled.
 *
 * @param message #LinphoneChatMessage
 * @param data the data to store into the message
 */
LINPHONE_PUBLIC void linphone_chat_message_set_appdata(LinphoneChatMessage* message, const char* data);

/**
 * Returns the chatroom this message belongs to.
**/
LINPHONE_PUBLIC LinphoneChatRoom* linphone_chat_message_get_chat_room(const LinphoneChatMessage *msg);

/**
 * Get the path to the file to read from or write to during the file transfer.
 * @param[in] msg LinphoneChatMessage object
 * @return The path to the file to use for the file transfer.
 */
LINPHONE_PUBLIC const char * linphone_chat_message_get_file_transfer_filepath(LinphoneChatMessage *msg);

// =============================================================================

LINPHONE_PUBLIC unsigned int linphone_chat_message_store(LinphoneChatMessage *msg);

/**
 * Get the state of the message
 *@param message #LinphoneChatMessage obj
 *@return #LinphoneChatMessageState
 */
LINPHONE_PUBLIC LinphoneChatMessageState linphone_chat_message_get_state(const LinphoneChatMessage* message);

/**
 * Duplicate a LinphoneChatMessage
**/
LINPHONE_PUBLIC LinphoneChatMessage* linphone_chat_message_clone(const LinphoneChatMessage* message);

/**
 * Get if the message was encrypted when transfered
 * @param[in] message #LinphoneChatMessage obj
 * @return whether the message was encrypted when transfered or not
 */
LINPHONE_PUBLIC bool_t linphone_chat_message_is_secured(LinphoneChatMessage *msg);

/**
 * Linphone message can carry external body as defined by rfc2017
 * @param message #LinphoneChatMessage
 * @return external body url or NULL if not present.
 */
LINPHONE_PUBLIC const char* linphone_chat_message_get_external_body_url(const LinphoneChatMessage* message);

/**
 * Linphone message can carry external body as defined by rfc2017
 *
 * @param message a LinphoneChatMessage
 * @param url ex: access-type=URL; URL="http://www.foo.com/file"
 */
LINPHONE_PUBLIC void linphone_chat_message_set_external_body_url(LinphoneChatMessage* message,const char* url);

/**
 * Get the file_transfer_information (used by call backs to recover informations during a rcs file transfer)
 *
 * @param message #LinphoneChatMessage
 * @return a pointer to the LinphoneContent structure or NULL if not present.
 */
LINPHONE_PUBLIC LinphoneContent* linphone_chat_message_get_file_transfer_information(LinphoneChatMessage* message);

/**
 * Return whether or not a chat message is a file tranfer.
 * @param[in] message LinphoneChatMessage object
 * @return Whether or not the message is a file tranfer
 */
LINPHONE_PUBLIC bool_t linphone_chat_message_is_file_transfer(const LinphoneChatMessage *message);

/**
 * Return whether or not a chat message is a text.
 * @param[in] message LinphoneChatMessage object
 * @return Whether or not the message is a text
 */
LINPHONE_PUBLIC bool_t linphone_chat_message_is_text(const LinphoneChatMessage *message);

/**
 * Get if a chat message is to be stored.
 * @param[in] message LinphoneChatMessage object
 * @return Whether or not the message is to be stored
 */
LINPHONE_PUBLIC bool_t linphone_chat_message_get_to_be_stored(const LinphoneChatMessage *message);

/**
 * Set if a chat message is to be stored.
 * This content type must match a content that is text representable, such as text/plain, text/html or image/svg+xml.
 * @param[in] message LinphoneChatMessage object
 * @param[in] to_be_stored Whether or not the chat message is to be stored
 */
LINPHONE_PUBLIC void linphone_chat_message_set_to_be_stored(LinphoneChatMessage *message, bool_t to_be_stored);

/**
 * Start the download of the file from remote server
 *
 * @param message #LinphoneChatMessage
 * @param status_cb LinphoneChatMessageStateChangeCb status callback invoked when file is downloaded or could not be downloaded
 * @param ud user data
 * @deprecated Use linphone_chat_message_download_file() instead.
 * @donotwrap
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED void linphone_chat_message_start_file_download(LinphoneChatMessage* message, LinphoneChatMessageStateChangedCb status_cb, void* ud);

/**
 * Start the download of the file referenced in a LinphoneChatMessage from remote server.
 * @param[in] message LinphoneChatMessage object.
 */
LINPHONE_PUBLIC LinphoneStatus linphone_chat_message_download_file(LinphoneChatMessage *message);

/**
 * Cancel an ongoing file transfer attached to this message.(upload or download)
 * @param msg	#LinphoneChatMessage
 */
LINPHONE_PUBLIC void linphone_chat_message_cancel_file_transfer(LinphoneChatMessage* msg);

/**
 * Resend a chat message if it is in the 'not delivered' state for whatever reason.
 * @param[in] msg LinphoneChatMessage object
 * @donotwrap
 */
LINPHONE_PUBLIC void linphone_chat_message_resend(LinphoneChatMessage *msg);

LINPHONE_PUBLIC const LinphoneAddress* linphone_chat_message_get_peer_address(LinphoneChatMessage *msg);

/**
 * Returns the origin address of a message if it was a outgoing message, or the destination address if it was an incoming message.
 *@param message #LinphoneChatMessage obj
 *@return #LinphoneAddress
 */
LINPHONE_PUBLIC LinphoneAddress *linphone_chat_message_get_local_address(const LinphoneChatMessage* message);

/**
 * Add custom headers to the message.
 * @param message the message
 * @param header_name name of the header
 * @param header_value header value
**/
LINPHONE_PUBLIC void linphone_chat_message_add_custom_header(LinphoneChatMessage* message, const char *header_name, const char *header_value);

/**
 * Retrieve a custom header value given its name.
 * @param message the message
 * @param header_name header name searched
**/
LINPHONE_PUBLIC const char * linphone_chat_message_get_custom_header(LinphoneChatMessage* message, const char *header_name);

/**
 * Removes a custom header from the message.
 * @param msg the message
 * @param header_name name of the header to remove
**/
LINPHONE_PUBLIC void linphone_chat_message_remove_custom_header(LinphoneChatMessage *msg, const char *header_name);

/**
 * Returns TRUE if the message has been read, otherwise returns FALSE.
 * @param message the message
**/
LINPHONE_PUBLIC bool_t linphone_chat_message_is_read(LinphoneChatMessage* message);

LINPHONE_PUBLIC LinphoneReason linphone_chat_message_get_reason(LinphoneChatMessage* msg);

/**
 * Get full details about delivery error of a chat message.
 * @param msg a LinphoneChatMessage
 * @return a LinphoneErrorInfo describing the details.
**/
LINPHONE_PUBLIC const LinphoneErrorInfo *linphone_chat_message_get_error_info(const LinphoneChatMessage *msg);

/**
 * Set the path to the file to read from or write to during the file transfer.
 * @param[in] msg LinphoneChatMessage object
 * @param[in] filepath The path to the file to use for the file transfer.
 */
LINPHONE_PUBLIC void linphone_chat_message_set_file_transfer_filepath(LinphoneChatMessage *msg, const char *filepath);

/**
 * Fulfill a chat message char by char. Message linked to a Real Time Text Call send char in realtime following RFC 4103/T.140
 * To commit a message, use #linphone_chat_room_send_message
 * @param[in] msg LinphoneChatMessage
 * @param[in] character T.140 char
 * @returns 0 if succeed.
 */
LINPHONE_PUBLIC LinphoneStatus linphone_chat_message_put_char(LinphoneChatMessage *msg,uint32_t character);

/**
 * Get the LinphoneChatMessageCbs object associated with the LinphoneChatMessage.
 * @param[in] msg LinphoneChatMessage object
 * @return The LinphoneChatMessageCbs object associated with the LinphoneChatMessage.
 */
LINPHONE_PUBLIC LinphoneChatMessageCbs * linphone_chat_message_get_callbacks(const LinphoneChatMessage *msg);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif // ifdef __cplusplus

#endif // ifndef _C_CHAT_MESSAGE_H_
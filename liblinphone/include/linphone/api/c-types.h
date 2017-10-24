/*
 * c-types.h
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

#ifndef _C_TYPES_H_
#define _C_TYPES_H_

// TODO: Remove me in the future.
#include "linphone/types.h"

#include "linphone/enums/chat-message-enums.h"
#include "linphone/enums/chat-room-enums.h"
#include "linphone/enums/event-log-enums.h"
#include "linphone/utils/enum-generator.h"

// =============================================================================

#ifdef __cplusplus
	extern "C" {
#endif // ifdef __cplusplus

// =============================================================================
// Misc.
// =============================================================================

#ifdef TRUE
	#undef TRUE
#endif

#ifdef FALSE
	#undef FALSE
#endif

#define TRUE 1
#define FALSE 0

// =============================================================================
// C Structures.
// =============================================================================

/**
 * Object that represents a SIP address.
 *
 * The LinphoneAddress is an opaque object to represents SIP addresses, ie
 * the content of SIP's 'from' and 'to' headers.
 * A SIP address is made of display name, username, domain name, port, and various
 * uri headers (such as tags). It looks like 'Alice <sip:alice@example.net>'.
 * The LinphoneAddress has methods to extract and manipulate all parts of the address.
 * When some part of the address (for example the username) is empty, the accessor methods
 * return NULL.
 * @ingroup linphone_address
 */
typedef struct _LinphoneAddress LinphoneAddress;

/**
 * The LinphoneCall object represents a call issued or received by the LinphoneCore
 * @ingroup call_control
**/
typedef struct _LinphoneCall LinphoneCall;

/** Callback prototype */
typedef void (*LinphoneCallCbFunc) (LinphoneCall *call, void *ud);

/**
 * That class holds all the callbacks which are called by LinphoneCall objects.
 *
 * Use linphone_factory_create_call_cbs() to create an instance. Then, call the
 * callback setters on the events you need to monitor and pass the object to
 * a LinphoneCall instance through linphone_call_add_callbacks().
 * @ingroup call_control
 */
typedef struct _LinphoneCallCbs LinphoneCallCbs;

/**
 * A chat room is the place where text messages are exchanged.
 * Can be created by linphone_core_create_chat_room().
 * @ingroup chatroom
 */
typedef struct _LinphoneChatRoom LinphoneChatRoom;

/**
 * An object to handle the callbacks for the handling a LinphoneChatRoom objects.
 * @ingroup chatroom
 */
typedef struct _LinphoneChatRoomCbs LinphoneChatRoomCbs;

/**
 * An chat message is the object that is sent and received through LinphoneChatRooms.
 * @ingroup chatroom
 */
typedef struct _LinphoneChatMessage LinphoneChatMessage;

/**
 * An object to handle the callbacks for the handling a LinphoneChatMessage objects.
 * @ingroup chatroom
 */
typedef struct _LinphoneChatMessageCbs LinphoneChatMessageCbs;

/**
* The LinphoneParticipant object represents a participant of a conference.
* @ingroup misc
**/
typedef struct _LinphoneParticipant LinphoneParticipant;

/**
* Represents a dial plan
* @ingroup misc
**/
typedef struct _LinphoneDialPlan LinphoneDialPlan;

// -----------------------------------------------------------------------------
// EventLog.
// -----------------------------------------------------------------------------

typedef struct _LinphoneCallEvent LinphoneCallEvent;
typedef struct _LinphoneConferenceChatMessageEvent LinphoneConferenceChatMessageEvent;
typedef struct _LinphoneConferenceEvent LinphoneConferenceEvent;
typedef struct _LinphoneConferenceParticipantDeviceEvent LinphoneConferenceParticipantDeviceEvent;
typedef struct _LinphoneConferenceParticipantEvent LinphoneConferenceParticipantEvent;
typedef struct _LinphoneConferenceSubjectEvent LinphoneConferenceSubjectEvent;
typedef struct _LinphoneEventLog LinphoneEventLog;

// =============================================================================
// C Enums.
// =============================================================================

/**
 * LinphoneChatMessageState is used to notify if messages have been succesfully delivered or not.
 * @ingroup chatroom
 */
L_DECLARE_C_ENUM(ChatMessageState, L_ENUM_VALUES_CHAT_MESSAGE_STATE);

/**
 * LinphoneChatMessageDirection is used to indicate if a message is outgoing or incoming.
 * @ingroup chatroom
 */
L_DECLARE_C_ENUM(ChatMessageDirection, L_ENUM_VALUES_CHAT_MESSAGE_DIRECTION);

L_DECLARE_C_ENUM(ChatRoomState, L_ENUM_VALUES_CHAT_ROOM_STATE);
L_DECLARE_C_ENUM(EventLogType, L_ENUM_VALUES_EVENT_LOG_TYPE);

#ifdef __cplusplus
	}
#endif // ifdef __cplusplus

#endif // ifndef _C_TYPES_H_

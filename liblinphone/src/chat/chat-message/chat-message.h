/*
 * chat-message.h
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

#ifndef _CHAT_MESSAGE_H_
#define _CHAT_MESSAGE_H_

#include <list>

#include "linphone/api/c-types.h"
#include "linphone/enums/chat-message-enums.h"

#include "object/object.h"

// =============================================================================

LINPHONE_BEGIN_NAMESPACE

class Address;
class ChatRoom;
class Content;
class ChatMessagePrivate;

class LINPHONE_PUBLIC ChatMessage : public Object {
	friend class ChatRoom;
	friend class ChatRoomPrivate;
	friend class RealTimeTextChatRoomPrivate;

public:
	L_OVERRIDE_SHARED_FROM_THIS(ChatMessage);

	L_DECLARE_ENUM(State, L_ENUM_VALUES_CHAT_MESSAGE_STATE);
	L_DECLARE_ENUM(Direction, L_ENUM_VALUES_CHAT_MESSAGE_DIRECTION);

	ChatMessage (const std::shared_ptr<ChatRoom> &chatRoom);

	// ----- TODO: Remove me.
	const std::string &getFileTransferFilepath () const;
	void setFileTransferFilepath (const std::string &path);
	const std::string &getAppdata () const;
	void setAppdata (const std::string &appData);
	const std::string &getExternalBodyUrl () const;
	void setExternalBodyUrl (const std::string &url);
	int uploadFile ();
	int downloadFile ();
	void cancelFileTransfer ();
	int putCharacter (uint32_t character);
	void updateState (State state);
	void sendDeliveryNotification (LinphoneReason reason);
	void sendDisplayNotification ();
	void setImdnMessageId (const std::string &imdnMessageId);
	void setIsSecured (bool isSecured);
	void setFromAddress (Address from);
	void setToAddress (Address to);
	void store ();
	// ----- TODO: Remove me.

	std::shared_ptr<ChatRoom> getChatRoom () const;

	void send ();

	time_t getTime () const;

	bool isSecured () const;
	State getState () const;
	Direction getDirection () const;

	const std::string &getImdnMessageId () const;

	const Address &getFromAddress () const;
	const Address &getToAddress () const;

	const LinphoneErrorInfo *getErrorInfo () const;

	bool isRead () const;
	bool isReadOnly () const;

	const std::list<Content> &getContents () const;
	void addContent (const Content &content);
	void removeContent (const Content &content);

	const Content &getInternalContent () const;
	void setInternalContent (const Content &content);

	std::string getCustomHeaderValue (const std::string &headerName) const;
	void addCustomHeader (const std::string &headerName, const std::string &headerValue);
	void removeCustomHeader (const std::string &headerName);

private:
	L_DECLARE_PRIVATE(ChatMessage);
	L_DISABLE_COPY(ChatMessage);
};

LINPHONE_END_NAMESPACE

#endif // ifndef _CHAT_MESSAGE_H_

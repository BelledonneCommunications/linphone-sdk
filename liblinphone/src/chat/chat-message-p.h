/*
 * chat-message-p.h
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

#ifndef _CHAT_MESSAGE_P_H_
#define _CHAT_MESSAGE_P_H_

#include "chat-message.h"
#include "db/events-db.h"
#include "object/object-p.h"

// =============================================================================

LINPHONE_BEGIN_NAMESPACE

class ChatMessagePrivate : public ObjectPrivate {
	friend class CpimChatMessageModifier;
	friend class MultipartChatMessageModifier;

public:
	ChatMessagePrivate (const std::shared_ptr<ChatRoom> &room);
	virtual ~ChatMessagePrivate ();

private:
	std::shared_ptr<ChatRoom> chatRoom;
	std::string externalBodyUrl;
	ChatMessage::Direction direction = ChatMessage::Incoming;
	ChatMessage::State state = ChatMessage::Idle;
	unsigned int storageId;
	std::shared_ptr<Address> from;
	std::shared_ptr<Address> to;
	time_t time = 0;
	std::string id;
	std::string appData;
	std::string fileTransferFilePath;
	bool isSecured = false;
	bool isReadOnly = false;
	bool isToBeStored = false;
	std::list<std::shared_ptr<Content> > contents;
	std::shared_ptr<Content> internalContent;
	std::unordered_map<std::string, std::string> customHeaders;
	std::shared_ptr<EventsDb> eventsDb;
	mutable LinphoneErrorInfo * errorInfo;
	belle_http_request_t *httpRequest;
	SalOp *salOp;
	SalCustomHeader *salCustomHeaders;
	// Used for compatibility with previous C API
	std::string cContentType;
	std::string cText;
	LinphoneContent *cFileTransferInformation;
	L_DECLARE_PUBLIC(ChatMessage);
};

LINPHONE_END_NAMESPACE

#endif // ifndef _CHAT_MESSAGE_P_H_

/*
 * imdn.h
 * Copyright (C) 2010-2018 Belledonne Communications SARL
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

#ifndef _L_IMDN_H_
#define _L_IMDN_H_

#include "linphone/utils/general.h"

#include "private.h"

// =============================================================================

LINPHONE_BEGIN_NAMESPACE

class ChatMessage;
class ChatRoom;

class Imdn {
public:
	enum class Type {
		Delivery,
		Display
	};

	struct MessageReason {
		MessageReason (const std::shared_ptr<ChatMessage> &message, LinphoneReason reason)
			: message(message), reason(reason) {}

		const std::shared_ptr<ChatMessage> message;
		LinphoneReason reason;
	};

	Imdn (ChatRoom *chatRoom);
	~Imdn ();

	void notifyDelivery (const std::shared_ptr<ChatMessage> &message);
	void notifyDeliveryError (const std::shared_ptr<ChatMessage> &message, LinphoneReason reason);
	void notifyDisplay (const std::shared_ptr<ChatMessage> &message);

	static std::string createXml (const std::string &id, time_t time, Imdn::Type imdnType, LinphoneReason reason);
	static void parse (const std::shared_ptr<ChatMessage> &chatMessage);

private:
	static void parse (const std::shared_ptr<ChatMessage> &chatMessage, xmlparsing_context_t *xmlCtx);
	static int timerExpired (void *data, unsigned int revents);

	void send ();
	void startTimer ();
	void stopTimer ();

private:
	static const std::string imdnPrefix;

	ChatRoom *chatRoom = nullptr;
	std::list<const std::shared_ptr<ChatMessage>> deliveredMessages;
	std::list<const std::shared_ptr<ChatMessage>> displayedMessages;
	std::list<MessageReason> nonDeliveredMessages;
	belle_sip_source_t *timer = nullptr;
};

LINPHONE_END_NAMESPACE

#endif // ifndef _L_IMDN_H_

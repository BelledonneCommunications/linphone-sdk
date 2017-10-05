/*
 * encryption-chat-message-modifier.h
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

#ifndef _ENCRYPTION_CHAT_MESSAGE_MODIFIER_H_
#define _ENCRYPTION_CHAT_MESSAGE_MODIFIER_H_

#include "chat-message-modifier.h"

// =============================================================================

LINPHONE_BEGIN_NAMESPACE

class EncryptionChatMessageModifier : public ChatMessageModifier {
public:
	EncryptionChatMessageModifier () = default;

	Result encode (ChatMessagePrivate *messagePrivate, int *errorCode) override;
	Result decode (ChatMessagePrivate *messagePrivate, int *errorCode) override;
};

LINPHONE_END_NAMESPACE

#endif // ifndef _ENCRYPTION_CHAT_MESSAGE_MODIFIER_H_

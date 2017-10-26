/*
 * core.h
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

#ifndef _CORE_H_
#define _CORE_H_

#include <list>

#include "object/object.h"

// =============================================================================

L_DECL_C_STRUCT(LinphoneCore);

LINPHONE_BEGIN_NAMESPACE

class Address;
class ChatRoom;
class CorePrivate;

class LINPHONE_PUBLIC Core : public Object {
	friend class ClientGroupChatRoom;

public:
	L_OVERRIDE_SHARED_FROM_THIS(Core);

	Core (LinphoneCore *cCore);

	// ---------------------------------------------------------------------------
	// Paths.
	// ---------------------------------------------------------------------------

	std::string getDataPath() const;
	std::string getConfigPath() const;

	// ---------------------------------------------------------------------------
	// ChatRoom.
	// ---------------------------------------------------------------------------

	const std::list<std::shared_ptr<ChatRoom>> &getChatRooms () const;
	std::shared_ptr<ChatRoom> findChatRoom (const Address &peerAddress) const;
	std::shared_ptr<ChatRoom> createClientGroupChatRoom (const std::string &subject);
	std::shared_ptr<ChatRoom> getOrCreateBasicChatRoom (const Address &peerAddress, bool isRtt = false);
	std::shared_ptr<ChatRoom> getOrCreateBasicChatRoom (const std::string &peerAddress, bool isRtt = false);

	static void deleteChatRoom (const std::shared_ptr<const ChatRoom> &chatRoom);

private:
	L_DECLARE_PRIVATE(Core);
	L_DISABLE_COPY(Core);
};

LINPHONE_END_NAMESPACE

#endif // ifndef _CORE_H_

/*
LinphoneChatRoom.java
Copyright (C) 2010  Belledonne Communications, Grenoble, France

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
package org.linphone.core;

import org.linphone.core.LinphoneChatMessage.State;

/**
 * 
 * A chat room is the place where text messages are exchanged. 
Can be created by linphone_core_create_chat_room().
 *
 */
public interface LinphoneChatRoom {
	/**
	 * get peer address associated to this LinphoneChatRoom
	 *
	 * @return LinphoneAddress peer address
	 */
	LinphoneAddress getPeerAddress();
	/**
	* send a message to peer member of this chat room.
	* @param  	message to be sent
	*/
	void sendMessage(String message);
	/**
	 * Send a message to peer member of this chat room.
	 * @param chat message
	 */
	void sendMessage(LinphoneChatMessage message, LinphoneChatMessage.StateListener listener);
	
	/**
	 * Create a LinphoneChatMessage
	 * @param chatRoom chat room associated to the message
	 * @param message message to send
	 * @return LinphoneChatMessage object
	 */
	LinphoneChatMessage createLinphoneChatMessage(String message);
	
	/**
	 * Returns the chat history associated with the peer address associated with this chat room
	 * @return an array of LinphoneChatMessage
	 */
	LinphoneChatMessage[] getHistory();
	
	/**
	 * Returns the chat history associated with the peer address associated with this chat room
	 * @param limit the maximum number of messages to fetch
	 * @return an array of LinphoneChatMessage
	 */
	LinphoneChatMessage[] getHistory(int limit);
	
	/**
	 * Destroys a LinphoneChatRoom.
	 */
	void destroy();
	
	/**
	 * Returns the amount of unread messages associated with the peer of this chatRoom.
	 * @return the amount of unread messages
	 */
	int getUnreadMessagesCount();
	
	/**
	 * Deletes all the messages associated with the peer of this chat room
	 */
	void deleteHistory();

	/**
	 * Notify the destination of the chat message being composed that the user is typing a new message.
	 */
	void compose();

	/**
	 * Tells whether the remote is currently composing a message.
	 * @return true if the remote is currently composing a message, false otherwise.
	 */
	boolean isRemoteComposing();
	
	/**
	 * Marks all the messages in this conversation as read
	 */
	void markAsRead();
	
	/**
	 * Deletes a message
	 * @param message the message to delete
	 */
	void deleteMessage(LinphoneChatMessage message);
	
	/**
	 * Update the value stored in the database for the external_body_url field
	 * @param message to update
	 */
	void updateUrl(LinphoneChatMessage message);
	
	/**
	 * Create a LinphoneChatMessage
	 * @return LinphoneChatMessage object
	 */
	LinphoneChatMessage createLinphoneChatMessage(String message, String url, State state, long timestamp, boolean isRead, boolean isIncoming);
}

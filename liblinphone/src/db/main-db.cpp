/*
 * main-db.cpp
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

#include <algorithm>
#include <ctime>

#ifdef SOCI_ENABLED
	#include <soci/soci.h>
#endif // ifdef SOCI_ENABLED

#include "linphone/utils/utils.h"

#include "chat/chat-message/chat-message-p.h"
#include "chat/chat-room/chat-room-p.h"
#include "chat/chat-room/client-group-chat-room.h"
#include "chat/chat-room/server-group-chat-room.h"
#include "conference/participant-p.h"
#include "content/content-type.h"
#include "content/content.h"
#include "core/core-p.h"
#include "db/session/db-session-provider.h"
#include "event-log/event-log-p.h"
#include "event-log/events.h"
#include "logger/logger.h"
#include "main-db-key-p.h"
#include "main-db-p.h"

// =============================================================================

// See: http://soci.sourceforge.net/doc/3.2/exchange.html
// Part: Object lifetime and immutability

// -----------------------------------------------------------------------------

using namespace std;

LINPHONE_BEGIN_NAMESPACE

// -----------------------------------------------------------------------------

MainDb::MainDb (const shared_ptr<Core> &core) : AbstractDb(*new MainDbPrivate), CoreAccessor(core) {}

#ifdef SOCI_ENABLED

// -----------------------------------------------------------------------------
// Soci backend.
// -----------------------------------------------------------------------------

	template<typename T>
	struct EnumToSql {
		T first;
		const char *second;
	};

	template<typename T>
	static constexpr const char *mapEnumToSql (const EnumToSql<T> enumToSql[], size_t n, T key) {
		return n == 0 ? "" : (
			enumToSql[n - 1].first == key ? enumToSql[n - 1].second : mapEnumToSql(enumToSql, n - 1, key)
		);
	}

	static constexpr EnumToSql<MainDb::Filter> eventFilterToSql[] = {
		{ MainDb::ConferenceCallFilter, "3, 4" },
		{ MainDb::ConferenceChatMessageFilter, "5" },
		{ MainDb::ConferenceInfoFilter, "1, 2, 6, 7, 8, 9, 10, 11, 12" }
	};

	static constexpr const char *mapEventFilterToSql (MainDb::Filter filter) {
		return mapEnumToSql(
			eventFilterToSql, sizeof eventFilterToSql / sizeof eventFilterToSql[0], filter
		);
	}

// -----------------------------------------------------------------------------

	static string buildSqlEventFilter (
		const list<MainDb::Filter> &filters,
		MainDb::FilterMask mask,
		const string &condKeyWord = "WHERE"
	) {
		L_ASSERT(
			find_if(filters.cbegin(), filters.cend(), [](const MainDb::Filter &filter) {
				return filter == MainDb::NoFilter;
			}) == filters.cend()
		);

		if (mask == MainDb::NoFilter)
			return "";

		bool isStart = true;
		string sql;
		for (const auto &filter : filters) {
			if (!(mask & filter))
				continue;

			if (isStart) {
				isStart = false;
				sql += " " + condKeyWord + " type IN (";
			} else
				sql += ", ";
			sql += mapEventFilterToSql(filter);
		}

		if (!isStart)
			sql += ") ";

		return sql;
	}

// -----------------------------------------------------------------------------

static inline vector<char> blobToVector (soci::blob &in) {
	size_t len = in.get_len();
	if (!len)
		return vector<char>();
	vector<char> out(len);
	in.read(0, &out[0], len);
	return out;
}

static inline string blobToString (soci::blob &in) {
	vector<char> out = blobToVector(in);
	return string(out.begin(), out.end());
}

// -----------------------------------------------------------------------------

	long long MainDbPrivate::resolveId (const soci::row &row, int col) const {
		L_Q();
		// See: http://soci.sourceforge.net/doc/master/backends/
		// `row id` is not supported by soci on Sqlite3. It's necessary to cast id to int...
		return q->getBackend() == AbstractDb::Sqlite3
			? static_cast<long long>(row.get<int>(0))
			: static_cast<long long>(row.get<unsigned long long>(0));
	}

// -----------------------------------------------------------------------------

	long long MainDbPrivate::insertSipAddress (const string &sipAddress) {
		L_Q();
		soci::session *session = dbSession.getBackendSession<soci::session>();

		long long id = selectSipAddressId(sipAddress);
		if (id >= 0)
			return id;

		lInfo() << "Insert new sip address in database: `" << sipAddress << "`.";
		*session << "INSERT INTO sip_address (value) VALUES (:sipAddress)", soci::use(sipAddress);
		return q->getLastInsertId();
	}

	void MainDbPrivate::insertContent (long long eventId, const Content &content) {
		L_Q();
		soci::session *session = dbSession.getBackendSession<soci::session>();

		const long long &contentTypeId = insertContentType(content.getContentType().asString());
		const string &body = content.getBodyAsString();
		*session << "INSERT INTO chat_message_content (event_id, content_type_id, body) VALUES"
			" (:eventId, :contentTypeId, :body)", soci::use(eventId), soci::use(contentTypeId),
			soci::use(body);

		const long long &chatMessageContentId = q->getLastInsertId();
		if (content.getContentType().isFile()) {
			const FileContent &fileContent = static_cast<const FileContent &>(content);
			const string &name = fileContent.getFileName();
			const size_t &size = fileContent.getFileSize();
			const string &path = fileContent.getFilePath();
			*session << "INSERT INTO chat_message_file_content (chat_message_content_id, name, size, path) VALUES "
				" (:chatMessageContentId, :name, :size, :path)",
				soci::use(chatMessageContentId), soci::use(name), soci::use(size), soci::use(path);
		}

		for (const auto &appData : content.getAppDataMap())
			*session << "INSERT INTO chat_message_content_app_data (chat_message_content_id, name, data) VALUES"
				" (:chatMessageContentId, :name, :data)",
				soci::use(chatMessageContentId), soci::use(appData.first), soci::use(appData.second);
	}

	long long MainDbPrivate::insertContentType (const string &contentType) {
		L_Q();
		soci::session *session = dbSession.getBackendSession<soci::session>();

		long long id;
		*session << "SELECT id FROM content_type WHERE value = :contentType", soci::use(contentType), soci::into(id);
		if (session->got_data())
			return id;

		lInfo() << "Insert new content type in database: `" << contentType << "`.";
		*session << "INSERT INTO content_type (value) VALUES (:contentType)", soci::use(contentType);
		return q->getLastInsertId();
	}

	long long MainDbPrivate::insertOrUpdateBasicChatRoom (
		long long peerSipAddressId,
		long long localSipAddressId,
		const tm &creationTime
	) {
		L_Q();

		soci::session *session = dbSession.getBackendSession<soci::session>();

		long long id = selectChatRoomId(peerSipAddressId, localSipAddressId);
		if (id >= 0) {
			*session << "UPDATE chat_room SET last_update_time = :lastUpdateTime WHERE id = :id",
				soci::use(creationTime), soci::use(id);
			return id;
		}

		static const int capabilities = static_cast<int>(ChatRoom::Capabilities::Basic);
		lInfo() << "Insert new chat room in database: (peer=" << peerSipAddressId <<
			", local=" << localSipAddressId << ", capabilities=" << capabilities << ").";
		*session << "INSERT INTO chat_room ("
			"  peer_sip_address_id, local_sip_address_id, creation_time, last_update_time, capabilities"
			") VALUES (:peerSipAddressId, :localSipAddressId, :creationTime, :lastUpdateTime, :capabilities)",
			soci::use(peerSipAddressId), soci::use(localSipAddressId), soci::use(creationTime), soci::use(creationTime),
			soci::use(capabilities);

		return q->getLastInsertId();
	}

	long long MainDbPrivate::insertChatRoom (const shared_ptr<AbstractChatRoom> &chatRoom) {
		L_Q();

		soci::session *session = dbSession.getBackendSession<soci::session>();

		const ChatRoomId &chatRoomId = chatRoom->getChatRoomId();
		const long long &peerSipAddressId = insertSipAddress(chatRoomId.getPeerAddress().asString());
		const long long &localSipAddressId = insertSipAddress(chatRoomId.getLocalAddress().asString());

		long long id = selectChatRoomId(peerSipAddressId, localSipAddressId);
		if (id >= 0) {
			lError() << "Unable to insert chat room (it already exists): (peer=" << peerSipAddressId <<
				", local=" << localSipAddressId << ").";
			return id;
		}

		lInfo() << "Insert new chat room in database: (peer=" << peerSipAddressId <<
			", local=" << localSipAddressId << ").";

		const tm &creationTime = Utils::getTimeTAsTm(chatRoom->getCreationTime());
		const int &capabilities = static_cast<int>(chatRoom->getCapabilities());
		const string &subject = chatRoom->getSubject();
		const int &flags = chatRoom->hasBeenLeft();
		*session << "INSERT INTO chat_room ("
			"  peer_sip_address_id, local_sip_address_id, creation_time, last_update_time, capabilities, subject, flags"
			") VALUES (:peerSipAddressId, :localSipAddressId, :creationTime, :lastUpdateTime, :capabilities, :subject, :flags)",
			soci::use(peerSipAddressId), soci::use(localSipAddressId), soci::use(creationTime), soci::use(creationTime),
			soci::use(capabilities), soci::use(subject), soci::use(flags);

		id = q->getLastInsertId();
		if (!chatRoom->canHandleParticipants())
			return id;

		// Do not add 'me' when creating a server-group-chat-room.
		if (chatRoomId.getLocalAddress() != chatRoomId.getPeerAddress()) {
			shared_ptr<Participant> me = chatRoom->getMe();
			long long meId = insertChatRoomParticipant(
				id,
				insertSipAddress(me->getAddress().asString()),
				me->isAdmin()
			);
			for (const auto &device : me->getPrivate()->getDevices())
				insertChatRoomParticipantDevice(meId, insertSipAddress(device->getAddress().asString()));
		}

		for (const auto &participant : chatRoom->getParticipants()) {
			long long participantId = insertChatRoomParticipant(
				id,
				insertSipAddress(participant->getAddress().asString()),
				participant->isAdmin()
			);
			for (const auto &device : participant->getPrivate()->getDevices())
				insertChatRoomParticipantDevice(participantId, insertSipAddress(device->getAddress().asString()));
		}

		return id;
	}

	long long MainDbPrivate::insertChatRoomParticipant (
		long long chatRoomId,
		long long participantSipAddressId,
		bool isAdmin
	) {
		L_Q();

		soci::session *session = dbSession.getBackendSession<soci::session>();
		long long id = selectChatRoomParticipantId(chatRoomId, participantSipAddressId);
		if (id >= 0) {
			// See: https://stackoverflow.com/a/15299655 (cast to reference)
			*session << "UPDATE chat_room_participant SET is_admin = :isAdmin WHERE id = :id",
				soci::use(static_cast<const int &>(isAdmin)), soci::use(id);
			return id;
		}

		lInfo() << "Insert new chat room participant in database: `" << participantSipAddressId <<
			"` (isAdmin=" << isAdmin << ").";
		*session << "INSERT INTO chat_room_participant (chat_room_id, participant_sip_address_id, is_admin)"
			" VALUES (:chatRoomId, :participantSipAddressId, :isAdmin)",
			soci::use(chatRoomId), soci::use(participantSipAddressId), soci::use(static_cast<const int &>(isAdmin));

		return q->getLastInsertId();
	}

	void MainDbPrivate::insertChatRoomParticipantDevice (
		long long participantId,
		long long participantDeviceSipAddressId
	) {
		soci::session *session = dbSession.getBackendSession<soci::session>();
		long long count;
		*session << "SELECT COUNT(*) FROM chat_room_participant_device"
			" WHERE chat_room_participant_id = :participantId"
			" AND participant_device_sip_address_id = :participantDeviceSipAddressId",
			soci::into(count), soci::use(participantId), soci::use(participantDeviceSipAddressId);
		if (count)
			return;

		lInfo() << "Insert new chat room participant device in database: `" << participantDeviceSipAddressId << "`.";
		*session << "INSERT INTO chat_room_participant_device (chat_room_participant_id, participant_device_sip_address_id)"
			" VALUES (:participantId, :participantDeviceSipAddressId)",
			soci::use(participantId), soci::use(participantDeviceSipAddressId);
	}

	void MainDbPrivate::insertChatMessageParticipant (long long eventId, long long sipAddressId, int state) {
		// TODO: Deal with read messages.
		// Remove if displayed? Think a good alorithm for mark as read.
		soci::session *session = dbSession.getBackendSession<soci::session>();
		soci::statement statement = (
			session->prepare << "UPDATE chat_message_participant SET state = :state"
				" WHERE event_id = :eventId AND participant_sip_address_id = :sipAddressId",
				soci::use(state), soci::use(eventId), soci::use(sipAddressId)
		);
		statement.execute();
		if (statement.get_affected_rows() == 0 && state != static_cast<int>(ChatMessage::State::Displayed))
			*session << "INSERT INTO chat_message_participant (event_id, participant_sip_address_id, state)"
				" VALUES (:eventId, :sipAddressId, :state)",
				soci::use(eventId), soci::use(sipAddressId), soci::use(state);
	}

// -----------------------------------------------------------------------------

	long long MainDbPrivate::selectSipAddressId (const string &sipAddress) const {
		soci::session *session = dbSession.getBackendSession<soci::session>();

		long long id;
		*session << "SELECT id FROM sip_address WHERE value = :sipAddress", soci::use(sipAddress), soci::into(id);
		return session->got_data() ? id : -1;
	}

	long long MainDbPrivate::selectChatRoomId (long long peerSipAddressId, long long localSipAddressId) const {
		soci::session *session = dbSession.getBackendSession<soci::session>();

		long long id;
		*session << "SELECT id FROM chat_room"
			"  WHERE peer_sip_address_id = :peerSipAddressId AND local_sip_address_id = :localSipAddressId",
			soci::use(peerSipAddressId), soci::use(localSipAddressId), soci::into(id);
		return session->got_data() ? id : -1;
	}

	long long MainDbPrivate::selectChatRoomId (const ChatRoomId &chatRoomId) const {
		long long peerSipAddressId = selectSipAddressId(chatRoomId.getPeerAddress().asString());
		if (peerSipAddressId < 0)
			return -1;

		long long localSipAddressId = selectSipAddressId(chatRoomId.getLocalAddress().asString());
		if (localSipAddressId < 0)
			return -1;

		return selectChatRoomId(peerSipAddressId, localSipAddressId);
	}

	long long MainDbPrivate::selectChatRoomParticipantId (long long chatRoomId, long long participantSipAddressId) const {
		long long id;
		soci::session *session = dbSession.getBackendSession<soci::session>();
		*session << "SELECT id from chat_room_participant"
			" WHERE chat_room_id = :chatRoomId AND participant_sip_address_id = :participantSipAddressId",
			soci::into(id), soci::use(chatRoomId), soci::use(participantSipAddressId);
		return session->got_data() ? id : -1;
	}

// -----------------------------------------------------------------------------

	void MainDbPrivate::deleteContents (long long messageEventId) {
		soci::session *session = dbSession.getBackendSession<soci::session>();
		*session << "DELETE FROM chat_message_content WHERE event_id = :messageEventId", soci::use(messageEventId);
	}

	void MainDbPrivate::deleteChatRoomParticipant (long long chatRoomId, long long participantSipAddressId) {
		soci::session *session = dbSession.getBackendSession<soci::session>();
		*session << "DELETE FROM chat_room_participant"
			"  WHERE chat_room_id = :chatRoomId AND participant_sip_address_id = :participantSipAddressId",
			soci::use(chatRoomId), soci::use(participantSipAddressId);
	}

	void MainDbPrivate::deleteChatRoomParticipantDevice (
		long long participantId,
		long long participantDeviceSipAddressId
	) {
		soci::session *session = dbSession.getBackendSession<soci::session>();
		*session << "DELETE FROM chat_room_participant_device"
			"  WHERE chat_room_participant_id = :participantId"
			"  AND participant_device_sip_address_id = :participantDeviceSipAddressId",
			soci::use(participantId), soci::use(participantDeviceSipAddressId);
	}

// -----------------------------------------------------------------------------

	shared_ptr<EventLog> MainDbPrivate::selectGenericConferenceEvent (
		long long eventId,
		EventLog::Type type,
		time_t creationTime,
		const ChatRoomId &chatRoomId
	) const {
		shared_ptr<EventLog> eventLog;

		switch (type) {
			case EventLog::Type::None:
				return nullptr;

			case EventLog::Type::ConferenceCreated:
			case EventLog::Type::ConferenceTerminated:
				eventLog = selectConferenceEvent(eventId, type, creationTime, chatRoomId);
				break;

			case EventLog::Type::ConferenceCallStart:
			case EventLog::Type::ConferenceCallEnd:
				eventLog = selectConferenceCallEvent(eventId, type, creationTime, chatRoomId);
				break;

			case EventLog::Type::ConferenceChatMessage:
				eventLog = selectConferenceChatMessageEvent(eventId, type, creationTime, chatRoomId);
				break;

			case EventLog::Type::ConferenceParticipantAdded:
			case EventLog::Type::ConferenceParticipantRemoved:
			case EventLog::Type::ConferenceParticipantSetAdmin:
			case EventLog::Type::ConferenceParticipantUnsetAdmin:
				eventLog = selectConferenceParticipantEvent(eventId, type, creationTime, chatRoomId);
				break;

			case EventLog::Type::ConferenceParticipantDeviceAdded:
			case EventLog::Type::ConferenceParticipantDeviceRemoved:
				eventLog = selectConferenceParticipantDeviceEvent(eventId, type, creationTime, chatRoomId);
				break;

			case EventLog::Type::ConferenceSubjectChanged:
				eventLog = selectConferenceSubjectEvent(eventId, type, creationTime, chatRoomId);
				break;
		}

		if (eventLog)
			cache(eventLog, eventId);

		return eventLog;
	}

	shared_ptr<EventLog> MainDbPrivate::selectConferenceEvent (
		long long,
		EventLog::Type type,
		time_t creationTime,
		const ChatRoomId &chatRoomId
	) const {
		return make_shared<ConferenceEvent>(
			type,
			creationTime,
			chatRoomId
		);
	}

	shared_ptr<EventLog> MainDbPrivate::selectConferenceCallEvent (
		long long eventId,
		EventLog::Type type,
		time_t creationTime,
		const ChatRoomId &chatRoomId
	) const {
		// TODO.
		return nullptr;
	}

	shared_ptr<EventLog> MainDbPrivate::selectConferenceChatMessageEvent (
		long long eventId,
		EventLog::Type type,
		time_t creationTime,
		const ChatRoomId &chatRoomId
	) const {
		L_Q();

		shared_ptr<Core> core = q->getCore();
		shared_ptr<AbstractChatRoom> chatRoom = core->findChatRoom(chatRoomId);
		if (!chatRoom) {
			lError() << "Unable to find chat room storage id of (peer=" +
				chatRoomId.getPeerAddress().asString() +
				", local=" + chatRoomId.getLocalAddress().asString() + "`).";
			return nullptr;
		}

		bool hasFileTransferContent = false;

		// 1 - Fetch chat message.
		shared_ptr<ChatMessage> chatMessage = getChatMessageFromCache(eventId);
		if (chatMessage)
			goto end;
		{
			string fromSipAddress;
			string toSipAddress;

			tm messageTime;

			string imdnMessageId;

			int state;
			int direction;
			int isSecured;

			soci::session *session = dbSession.getBackendSession<soci::session>();
			*session << "SELECT from_sip_address.value, to_sip_address.value, time, imdn_message_id, state, direction, is_secured"
				" FROM event, conference_chat_message_event, sip_address AS from_sip_address, sip_address AS to_sip_address"
				" WHERE event_id = :eventId"
				" AND event_id = event.id"
				" AND from_sip_address_id = from_sip_address.id"
				" AND to_sip_address_id = to_sip_address.id", soci::into(fromSipAddress), soci::into(toSipAddress),
				soci::into(messageTime), soci::into(imdnMessageId), soci::into(state), soci::into(direction),
				soci::into(isSecured), soci::use(eventId);

			chatMessage = shared_ptr<ChatMessage>(new ChatMessage(
				chatRoom,
				static_cast<ChatMessage::Direction>(direction)
			));
			chatMessage->setIsSecured(static_cast<bool>(isSecured));

			ChatMessagePrivate *dChatMessage = chatMessage->getPrivate();
			dChatMessage->setState(static_cast<ChatMessage::State>(state), true);

			dChatMessage->forceFromAddress(IdentityAddress(fromSipAddress));
			dChatMessage->forceToAddress(IdentityAddress(toSipAddress));

			dChatMessage->setTime(Utils::getTmAsTimeT(messageTime));
			dChatMessage->setImdnMessageId(imdnMessageId);
		}

		// 2 - Fetch contents.
		{
			soci::session *session = dbSession.getBackendSession<soci::session>();
			static const string query = "SELECT chat_message_content.id, content_type.id, content_type.value, body"
				" FROM chat_message_content, content_type"
				" WHERE event_id = :eventId AND content_type_id = content_type.id";
			soci::rowset<soci::row> rows = (session->prepare << query, soci::use(eventId));
			for (const auto &row : rows) {
				ContentType contentType(row.get<string>(2));
				const long long &contentId = resolveId(row, 0);
				Content *content;

				if (contentType == ContentType::FileTransfer) {
					hasFileTransferContent = true;
					content = new FileTransferContent();
				}
				else if (contentType.isFile()) {
					// 2.1 - Fetch contents' file informations.
					string name;
					int size;
					string path;

					*session << "SELECT name, size, path FROM chat_message_file_content"
						" WHERE chat_message_content_id = :contentId",
						soci::into(name), soci::into(size), soci::into(path), soci::use(contentId);

					FileContent *fileContent = new FileContent();
					fileContent->setFileName(name);
					fileContent->setFileSize(static_cast<size_t>(size));
					fileContent->setFilePath(path);

					content = fileContent;
				} else
					content = new Content();

				content->setContentType(contentType);
				content->setBody(row.get<string>(3));

				// 2.2 - Fetch contents' app data.
				static const string query = "SELECT name, data FROM chat_message_content_app_data"
					" WHERE chat_message_content_id = :contentId";

				string name;
				soci::blob data(*session);
				soci::statement statement = (session->prepare << query, soci::use(contentId), soci::into(name), soci::into(data));
				statement.execute();
				while (statement.fetch())
					content->setAppData(name, blobToString(data));

				chatMessage->addContent(*content);
			}
		}

		// 3 - Load external body url from body into FileTransferContent if needed.
		if (hasFileTransferContent)
			chatMessage->getPrivate()->loadFileTransferUrlFromBodyToContent();

		cache(chatMessage, eventId);

	end:
		return make_shared<ConferenceChatMessageEvent>(
			creationTime,
			chatMessage
		);
	}

	shared_ptr<EventLog> MainDbPrivate::selectConferenceParticipantEvent (
		long long eventId,
		EventLog::Type type,
		time_t creationTime,
		const ChatRoomId &chatRoomId
	) const {
		unsigned int notifyId;
		string participantAddress;

		soci::session *session = dbSession.getBackendSession<soci::session>();
		*session << "SELECT notify_id, participant_address.value"
			"  FROM conference_notified_event, conference_participant_event, sip_address as participant_address"
			"  WHERE conference_participant_event.event_id = :eventId"
			"    AND conference_notified_event.event_id = conference_participant_event.event_id"
			"    AND participant_address.id = participant_sip_address_id",
			soci::into(notifyId), soci::into(participantAddress), soci::use(eventId);

		return make_shared<ConferenceParticipantEvent>(
			type,
			creationTime,
			chatRoomId,
			notifyId,
			IdentityAddress(participantAddress)
		);
	}

	shared_ptr<EventLog> MainDbPrivate::selectConferenceParticipantDeviceEvent (
		long long eventId,
		EventLog::Type type,
		time_t creationTime,
		const ChatRoomId &chatRoomId
	) const {
		unsigned int notifyId;
		string participantAddress;
		string deviceAddress;

		soci::session *session = dbSession.getBackendSession<soci::session>();
		*session << "SELECT notify_id, participant_address.value, device_address.value"
			"  FROM conference_notified_event, conference_participant_event, conference_participant_device_event,"
			"    sip_address AS participant_address, sip_address AS device_address"
			"  WHERE conference_participant_device_event.event_id = :eventId"
			"    AND conference_participant_event.event_id = conference_participant_device_event.event_id"
			"    AND conference_notified_event.event_id = conference_participant_event.event_id"
			"    AND participant_address.id = participant_sip_address_id"
			"    AND device_address.id = device_sip_address_id",
			soci::into(notifyId), soci::into(participantAddress), soci::into(deviceAddress), soci::use(eventId);

		return make_shared<ConferenceParticipantDeviceEvent>(
			type,
			creationTime,
			chatRoomId,
			notifyId,
			IdentityAddress(participantAddress),
			IdentityAddress(deviceAddress)
		);
	}

	shared_ptr<EventLog> MainDbPrivate::selectConferenceSubjectEvent (
		long long eventId,
		EventLog::Type type,
		time_t creationTime,
		const ChatRoomId &chatRoomId
	) const {
		unsigned int notifyId;
		string subject;

		soci::session *session = dbSession.getBackendSession<soci::session>();
		*session << "SELECT notify_id, subject"
			" FROM conference_notified_event, conference_subject_event"
			" WHERE conference_subject_event.event_id = :eventId"
			" AND conference_notified_event.event_id = conference_subject_event.event_id",
			soci::into(notifyId), soci::into(subject), soci::use(eventId);

		return make_shared<ConferenceSubjectEvent>(
			creationTime,
			chatRoomId,
			notifyId,
			subject
		);
	}

// -----------------------------------------------------------------------------

	long long MainDbPrivate::insertEvent (const shared_ptr<EventLog> &eventLog) {
		L_Q();
		soci::session *session = dbSession.getBackendSession<soci::session>();

		const int &type = static_cast<int>(eventLog->getType());
		const tm &creationTime = Utils::getTimeTAsTm(eventLog->getCreationTime());
		*session << "INSERT INTO event (type, creation_time) VALUES (:type, :creationTime)",
			soci::use(type),
			soci::use(creationTime);

		return q->getLastInsertId();
	}

	long long MainDbPrivate::insertConferenceEvent (const shared_ptr<EventLog> &eventLog, long long *chatRoomId) {
		shared_ptr<ConferenceEvent> conferenceEvent = static_pointer_cast<ConferenceEvent>(eventLog);

		long long eventId = -1;
		const long long &curChatRoomId = selectChatRoomId(conferenceEvent->getChatRoomId());
		if (curChatRoomId < 0) {
			// A conference event can be inserted in database only if chat room exists.
			// Otherwise it's an error.
			const ChatRoomId &chatRoomId = conferenceEvent->getChatRoomId();
			lError() << "Unable to find chat room storage id of (peer=" +
				chatRoomId.getPeerAddress().asString() +
				", local=" + chatRoomId.getLocalAddress().asString() + "`).";
		} else {
			eventId = insertEvent(eventLog);

			soci::session *session = dbSession.getBackendSession<soci::session>();
			*session << "INSERT INTO conference_event (event_id, chat_room_id)"
				" VALUES (:eventId, :chatRoomId)", soci::use(eventId), soci::use(curChatRoomId);

			const tm &lastUpdateTime = Utils::getTimeTAsTm(eventLog->getCreationTime());
			*session << "UPDATE chat_room SET last_update_time = :lastUpdateTime"
				" WHERE id = :chatRoomId", soci::use(lastUpdateTime),
				soci::use(curChatRoomId);

			if (eventLog->getType() == EventLog::Type::ConferenceTerminated)
				*session << "UPDATE chat_room SET flags = 1 WHERE id = :chatRoomId", soci::use(curChatRoomId);
		}

		if (chatRoomId)
			*chatRoomId = curChatRoomId;

		return eventId;
	}

	long long MainDbPrivate::insertConferenceCallEvent (const shared_ptr<EventLog> &eventLog) {
		// TODO.
		return 0;
	}

	long long MainDbPrivate::insertConferenceChatMessageEvent (const shared_ptr<EventLog> &eventLog) {
		shared_ptr<ChatMessage> chatMessage = static_pointer_cast<ConferenceChatMessageEvent>(eventLog)->getChatMessage();
		shared_ptr<AbstractChatRoom> chatRoom = chatMessage->getChatRoom();
		if (!chatRoom) {
			lError() << "Unable to get a valid chat room. It was removed from database.";
			return -1;
		}

		const long long &eventId = insertConferenceEvent(eventLog);
		if (eventId < 0)
			return -1;

		soci::session *session = dbSession.getBackendSession<soci::session>();

		const long long &fromSipAddressId = insertSipAddress(chatMessage->getFromAddress().asString());
		const long long &toSipAddressId = insertSipAddress(chatMessage->getToAddress().asString());
		const tm &messageTime = Utils::getTimeTAsTm(chatMessage->getTime());
		const int &state = static_cast<int>(chatMessage->getState());
		const int &direction = static_cast<int>(chatMessage->getDirection());
		const string &imdnMessageId = chatMessage->getImdnMessageId();
		const int &isSecured = chatMessage->isSecured() ? 1 : 0;

		*session << "INSERT INTO conference_chat_message_event ("
			"  event_id, from_sip_address_id, to_sip_address_id,"
			"  time, state, direction, imdn_message_id, is_secured"
			") VALUES ("
			"  :eventId, :localSipaddressId, :remoteSipaddressId,"
			"  :time, :state, :direction, :imdnMessageId, :isSecured"
			")", soci::use(eventId), soci::use(fromSipAddressId), soci::use(toSipAddressId),
			soci::use(messageTime), soci::use(state), soci::use(direction),
			soci::use(imdnMessageId), soci::use(isSecured);

		for (const Content *content : chatMessage->getContents())
			insertContent(eventId, *content);

		return eventId;
	}

	void MainDbPrivate::updateConferenceChatMessageEvent (const shared_ptr<EventLog> &eventLog) {
		shared_ptr<ChatMessage> chatMessage = static_pointer_cast<ConferenceChatMessageEvent>(eventLog)->getChatMessage();
		shared_ptr<AbstractChatRoom> chatRoom = chatMessage->getChatRoom();
		if (!chatRoom) {
			lError() << "Unable to get a valid chat room. It was removed from database.";
			return;
		}

		const EventLogPrivate *dEventLog = eventLog->getPrivate();
		MainDbKeyPrivate *dEventKey = static_cast<MainDbKey &>(dEventLog->dbKey).getPrivate();
		const long long &eventId = dEventKey->storageId;

		soci::session *session = dbSession.getBackendSession<soci::session>();
		const int &state = static_cast<int>(chatMessage->getState());
		const string &imdnMessageId = chatMessage->getImdnMessageId();
		*session << "UPDATE conference_chat_message_event SET state = :state, imdn_message_id = :imdnMessageId"
			" WHERE event_id = :eventId",
			soci::use(state), soci::use(imdnMessageId), soci::use(eventId);

		deleteContents(eventId);
		for (const auto &content : chatMessage->getContents())
			insertContent(eventId, *content);
	}

	long long MainDbPrivate::insertConferenceNotifiedEvent (const shared_ptr<EventLog> &eventLog, long long *chatRoomId) {
		long long curChatRoomId;
		const long long &eventId = insertConferenceEvent(eventLog, &curChatRoomId);
		if (eventId < 0)
			return -1;

		const unsigned int &lastNotifyId = static_pointer_cast<ConferenceNotifiedEvent>(eventLog)->getNotifyId();

		soci::session *session = dbSession.getBackendSession<soci::session>();
		*session << "INSERT INTO conference_notified_event (event_id, notify_id)"
			" VALUES (:eventId, :notifyId)", soci::use(eventId), soci::use(lastNotifyId);
		*session << "UPDATE chat_room SET last_notify_id = :lastNotifyId WHERE id = :chatRoomId",
			soci::use(lastNotifyId), soci::use(curChatRoomId);

		if (chatRoomId)
			*chatRoomId = curChatRoomId;

		return eventId;
	}

	long long MainDbPrivate::insertConferenceParticipantEvent (
		const shared_ptr<EventLog> &eventLog,
		long long *chatRoomId
	) {
		long long curChatRoomId;
		const long long &eventId = insertConferenceNotifiedEvent(eventLog, &curChatRoomId);
		if (eventId < 0)
			return -1;

		shared_ptr<ConferenceParticipantEvent> participantEvent =
			static_pointer_cast<ConferenceParticipantEvent>(eventLog);

		const long long &participantAddressId = insertSipAddress(
			participantEvent->getParticipantAddress().asString()
		);

		soci::session *session = dbSession.getBackendSession<soci::session>();
		*session << "INSERT INTO conference_participant_event (event_id, participant_sip_address_id)"
			"  VALUES (:eventId, :participantAddressId)", soci::use(eventId), soci::use(participantAddressId);

		bool isAdmin = eventLog->getType() == EventLog::Type::ConferenceParticipantSetAdmin;
		switch (eventLog->getType()) {
			case EventLog::Type::ConferenceParticipantAdded:
			case EventLog::Type::ConferenceParticipantSetAdmin:
			case EventLog::Type::ConferenceParticipantUnsetAdmin:
				insertChatRoomParticipant(curChatRoomId, participantAddressId, isAdmin);
				break;

			case EventLog::Type::ConferenceParticipantRemoved:
				deleteChatRoomParticipant(curChatRoomId, participantAddressId);
				break;

			default:
				break;
		}

		if (chatRoomId)
			*chatRoomId = curChatRoomId;

		return eventId;
	}

	long long MainDbPrivate::insertConferenceParticipantDeviceEvent (const shared_ptr<EventLog> &eventLog) {
		long long chatRoomId;
		const long long &eventId = insertConferenceParticipantEvent(eventLog, &chatRoomId);
		if (eventId < 0)
			return -1;

		shared_ptr<ConferenceParticipantDeviceEvent> participantDeviceEvent =
			static_pointer_cast<ConferenceParticipantDeviceEvent>(eventLog);

		const string participantAddress = participantDeviceEvent->getParticipantAddress().asString();
		const long long &participantAddressId = selectSipAddressId(participantAddress);
		if (participantAddressId < 0) {
			lError() << "Unable to find sip address id of: `" << participantAddress << "`.";
			return -1;
		}
		const long long &participantId = selectChatRoomParticipantId(chatRoomId, participantAddressId);
		if (participantId < 0) {
			lError() << "Unable to find valid participant id in database with chat room id = " << chatRoomId <<
			" and participant address id = " << participantId;
			return -1;
		}
		const long long &deviceAddressId = insertSipAddress(
			participantDeviceEvent->getDeviceAddress().asString()
		);

		soci::session *session = dbSession.getBackendSession<soci::session>();
		*session << "INSERT INTO conference_participant_device_event (event_id, device_sip_address_id)"
			" VALUES (:eventId, :deviceAddressId)", soci::use(eventId), soci::use(deviceAddressId);

		switch (eventLog->getType()) {
			case EventLog::Type::ConferenceParticipantDeviceAdded:
				insertChatRoomParticipantDevice(participantId, deviceAddressId);
				break;

			case EventLog::Type::ConferenceParticipantDeviceRemoved:
				deleteChatRoomParticipantDevice(participantId, deviceAddressId);
				break;

			default:
				break;
		}

		return eventId;
	}

	long long MainDbPrivate::insertConferenceSubjectEvent (const shared_ptr<EventLog> &eventLog) {
		long long chatRoomId;
		const long long &eventId = insertConferenceNotifiedEvent(eventLog, &chatRoomId);
		if (eventId < 0)
			return -1;

		const string &subject = static_pointer_cast<ConferenceSubjectEvent>(eventLog)->getSubject();

		soci::session *session = dbSession.getBackendSession<soci::session>();
		*session << "INSERT INTO conference_subject_event (event_id, subject)"
			" VALUES (:eventId, :subject)", soci::use(eventId), soci::use(subject);

		*session << "UPDATE chat_room SET subject = :subject"
			" WHERE id = :chatRoomId", soci::use(subject), soci::use(chatRoomId);

		return eventId;
	}

// -----------------------------------------------------------------------------

	shared_ptr<EventLog> MainDbPrivate::getEventFromCache (long long storageId) const {
		auto it = storageIdToEvent.find(storageId);
		if (it == storageIdToEvent.cend())
			return nullptr;

		shared_ptr<EventLog> eventLog = it->second.lock();
		// Must exist. If not, implementation bug.
		L_ASSERT(eventLog);
		return eventLog;
	}

	shared_ptr<ChatMessage> MainDbPrivate::getChatMessageFromCache (long long storageId) const {
		auto it = storageIdToChatMessage.find(storageId);
		if (it == storageIdToChatMessage.cend())
			return nullptr;

		shared_ptr<ChatMessage> chatMessage = it->second.lock();
		// Must exist. If not, implementation bug.
		L_ASSERT(chatMessage);
		return chatMessage;
	}

	void MainDbPrivate::cache (const shared_ptr<EventLog> &eventLog, long long storageId) const {
		L_Q();

		EventLogPrivate *dEventLog = eventLog->getPrivate();
		L_ASSERT(!dEventLog->dbKey.isValid());
		dEventLog->dbKey = MainDbEventKey(q->getCore(), storageId);
		storageIdToEvent[storageId] = eventLog;
		L_ASSERT(dEventLog->dbKey.isValid());
	}

	void MainDbPrivate::cache (const shared_ptr<ChatMessage> &chatMessage, long long storageId) const {
		L_Q();

		ChatMessagePrivate *dChatMessage = chatMessage->getPrivate();
		L_ASSERT(!dChatMessage->dbKey.isValid());
		dChatMessage->dbKey = MainDbChatMessageKey(q->getCore(), storageId);
		storageIdToChatMessage[storageId] = chatMessage;
		L_ASSERT(dChatMessage->dbKey.isValid());
	}

	void MainDbPrivate::invalidConferenceEventsFromQuery (const string &query, long long chatRoomId) {
		soci::session *session = dbSession.getBackendSession<soci::session>();
		soci::rowset<soci::row> rows = (session->prepare << query, soci::use(chatRoomId));
		for (const auto &row : rows) {
			long long eventId = resolveId(row, 0);
			shared_ptr<EventLog> eventLog = getEventFromCache(eventId);
			if (eventLog) {
				const EventLogPrivate *dEventLog = eventLog->getPrivate();
				L_ASSERT(dEventLog->dbKey.isValid());
				dEventLog->dbKey = MainDbEventKey();
			}
			// TODO: Try to add a better code here...
			shared_ptr<ChatMessage> chatMessage = getChatMessageFromCache(eventId);
			if (chatMessage) {
				const ChatMessagePrivate *dChatMessage = chatMessage->getPrivate();
				L_ASSERT(dChatMessage->dbKey.isValid());
				dChatMessage->dbKey = MainDbChatMessageKey();
			}
		}
	}

// -----------------------------------------------------------------------------

	void MainDb::init () {
		L_D();

		const string charset = getBackend() == Mysql ? "DEFAULT CHARSET=utf8" : "";
		soci::session *session = d->dbSession.getBackendSession<soci::session>();

		*session <<
			"CREATE TABLE IF NOT EXISTS sip_address ("
			"  id" + primaryKeyStr("BIGINT UNSIGNED") + ","
			"  value VARCHAR(255) UNIQUE NOT NULL"
			") " + charset;

		*session <<
			"CREATE TABLE IF NOT EXISTS content_type ("
			"  id" + primaryKeyStr("SMALLINT UNSIGNED") + ","
			"  value VARCHAR(255) UNIQUE NOT NULL"
			") " + charset;

		*session <<
			"CREATE TABLE IF NOT EXISTS event ("
			"  id" + primaryKeyStr("BIGINT UNSIGNED") + ","
			"  type TINYINT UNSIGNED NOT NULL,"
			"  creation_time" + timestampType() + " NOT NULL"
			") " + charset;

		*session <<
			"CREATE TABLE IF NOT EXISTS chat_room ("
			"  id" + primaryKeyStr("BIGINT UNSIGNED") + ","

			// Server (for conference) or user sip address.
			"  peer_sip_address_id" + primaryKeyRefStr("BIGINT UNSIGNED") + " NOT NULL,"

			"  local_sip_address_id" + primaryKeyRefStr("BIGINT UNSIGNED") + " NOT NULL,"

			// Dialog creation time.
			"  creation_time" + timestampType() + " NOT NULL,"

			// Last event time (call, message...).
			"  last_update_time" + timestampType() + " NOT NULL,"

			// ConferenceChatRoom, BasicChatRoom, RTT...
			"  capabilities TINYINT UNSIGNED NOT NULL,"

			// Chatroom subject.
			"  subject VARCHAR(255),"

			"  last_notify_id INT UNSIGNED DEFAULT 0,"

			"  flags INT UNSIGNED DEFAULT 0,"

			"  UNIQUE (peer_sip_address_id, local_sip_address_id),"

			"  FOREIGN KEY (peer_sip_address_id)"
			"    REFERENCES sip_address(id)"
			"    ON DELETE CASCADE,"
			"  FOREIGN KEY (local_sip_address_id)"
			"    REFERENCES sip_address(id)"
			"    ON DELETE CASCADE"
			") " + charset;

		*session <<
			"CREATE TABLE IF NOT EXISTS chat_room_participant ("
			"  id" + primaryKeyStr("BIGINT UNSIGNED") + ","

			"  chat_room_id" + primaryKeyRefStr("BIGINT UNSIGNED") + ","
			"  participant_sip_address_id" + primaryKeyRefStr("BIGINT UNSIGNED") + ","

			"  is_admin BOOLEAN NOT NULL,"

			"  UNIQUE (chat_room_id, participant_sip_address_id),"

			"  FOREIGN KEY (chat_room_id)"
			"    REFERENCES chat_room(id)"
			"    ON DELETE CASCADE,"
			"  FOREIGN KEY (participant_sip_address_id)"
			"    REFERENCES sip_address(id)"
			"    ON DELETE CASCADE"
			") " + charset;

		*session <<
			"CREATE TABLE IF NOT EXISTS chat_room_participant_device ("
			"  chat_room_participant_id" + primaryKeyRefStr("BIGINT UNSIGNED") + ","
			"  participant_device_sip_address_id" + primaryKeyRefStr("BIGINT UNSIGNED") + ","

			"  PRIMARY KEY (chat_room_participant_id, participant_device_sip_address_id),"

			"  FOREIGN KEY (chat_room_participant_id)"
			"    REFERENCES chat_room_participant(id)"
			"    ON DELETE CASCADE,"
			"  FOREIGN KEY (participant_device_sip_address_id)"
			"    REFERENCES sip_address(id)"
			"    ON DELETE CASCADE"
			") " + charset;

		*session <<
			"CREATE TABLE IF NOT EXISTS conference_event ("
			"  event_id" + primaryKeyStr("BIGINT UNSIGNED") + ","

			"  chat_room_id" + primaryKeyRefStr("BIGINT UNSIGNED") + " NOT NULL,"

			"  FOREIGN KEY (event_id)"
			"    REFERENCES event(id)"
			"    ON DELETE CASCADE,"
			"  FOREIGN KEY (chat_room_id)"
			"    REFERENCES chat_room(id)"
			"    ON DELETE CASCADE"
			") " + charset;

		*session <<
			"CREATE TABLE IF NOT EXISTS conference_notified_event ("
			"  event_id" + primaryKeyStr("BIGINT UNSIGNED") + ","

			"  notify_id INT UNSIGNED NOT NULL,"

			"  FOREIGN KEY (event_id)"
			"    REFERENCES conference_event(event_id)"
			"    ON DELETE CASCADE"
			") " + charset;

		*session <<
			"CREATE TABLE IF NOT EXISTS conference_participant_event ("
			"  event_id" + primaryKeyStr("BIGINT UNSIGNED") + ","

			"  participant_sip_address_id" + primaryKeyRefStr("BIGINT UNSIGNED") + " NOT NULL,"

			"  FOREIGN KEY (event_id)"
			"    REFERENCES conference_notified_event(event_id)"
			"    ON DELETE CASCADE,"
			"  FOREIGN KEY (participant_sip_address_id)"
			"    REFERENCES sip_address(id)"
			"    ON DELETE CASCADE"
			") " + charset;

		*session <<
			"CREATE TABLE IF NOT EXISTS conference_participant_device_event ("
			"  event_id" + primaryKeyStr("BIGINT UNSIGNED") + ","

			"  device_sip_address_id" + primaryKeyRefStr("BIGINT UNSIGNED") + " NOT NULL,"

			"  FOREIGN KEY (event_id)"
			"    REFERENCES conference_participant_event(event_id)"
			"    ON DELETE CASCADE,"
			"  FOREIGN KEY (device_sip_address_id)"
			"    REFERENCES sip_address(id)"
			"    ON DELETE CASCADE"
			") " + charset;

		*session <<
			"CREATE TABLE IF NOT EXISTS conference_subject_event ("
			"  event_id" + primaryKeyStr("BIGINT UNSIGNED") + ","

			"  subject VARCHAR(255) NOT NULL,"

			"  FOREIGN KEY (event_id)"
			"    REFERENCES conference_notified_event(event_id)"
			"    ON DELETE CASCADE"
			") " + charset;

		*session <<
			"CREATE TABLE IF NOT EXISTS conference_chat_message_event ("
			"  event_id" + primaryKeyStr("BIGINT UNSIGNED") + ","

			"  from_sip_address_id" + primaryKeyRefStr("BIGINT UNSIGNED") + " NOT NULL,"
			"  to_sip_address_id" + primaryKeyRefStr("BIGINT UNSIGNED") + " NOT NULL,"

			"  time" + timestampType() + " ,"

			// See: https://tools.ietf.org/html/rfc5438#section-6.3
			"  imdn_message_id VARCHAR(255) NOT NULL,"

			"  state TINYINT UNSIGNED NOT NULL,"
			"  direction TINYINT UNSIGNED NOT NULL,"
			"  is_secured BOOLEAN NOT NULL,"

			"  FOREIGN KEY (event_id)"
			"    REFERENCES conference_event(event_id)"
			"    ON DELETE CASCADE,"
			"  FOREIGN KEY (from_sip_address_id)"
			"    REFERENCES sip_address(id)"
			"    ON DELETE CASCADE,"
			"  FOREIGN KEY (to_sip_address_id)"
			"    REFERENCES sip_address(id)"
			"    ON DELETE CASCADE"
			") " + charset;

		*session <<
			"CREATE TABLE IF NOT EXISTS chat_message_participant ("
			"  event_id" + primaryKeyRefStr("BIGINT UNSIGNED") + ","
			"  participant_sip_address_id" + primaryKeyRefStr("BIGINT UNSIGNED") + ","
			"  state TINYINT UNSIGNED NOT NULL,"

			"  PRIMARY KEY (event_id, participant_sip_address_id),"

			"  FOREIGN KEY (event_id)"
			"    REFERENCES conference_chat_message_event(event_id)"
			"    ON DELETE CASCADE,"
			"  FOREIGN KEY (participant_sip_address_id)"
			"    REFERENCES sip_address(id)"
			"    ON DELETE CASCADE"
			") " + charset;

		*session <<
			"CREATE TABLE IF NOT EXISTS chat_message_content ("
			"  id" + primaryKeyStr("BIGINT UNSIGNED") + ","

			"  event_id" + primaryKeyRefStr("BIGINT UNSIGNED") + " NOT NULL,"
			"  content_type_id" + primaryKeyRefStr("SMALLINT UNSIGNED") + " NOT NULL,"
			"  body TEXT NOT NULL,"

			"  UNIQUE (id, event_id),"

			"  FOREIGN KEY (event_id)"
			"    REFERENCES conference_chat_message_event(event_id)"
			"    ON DELETE CASCADE,"
			"  FOREIGN KEY (content_type_id)"
			"    REFERENCES content_type(id)"
			"    ON DELETE CASCADE"
			") " + charset;

		*session <<
			"CREATE TABLE IF NOT EXISTS chat_message_file_content ("
			"  chat_message_content_id" + primaryKeyStr("BIGINT UNSIGNED") + ","

			"  name VARCHAR(256) NOT NULL,"
			"  size INT UNSIGNED NOT NULL,"
			"  path VARCHAR(512) NOT NULL,"

			"  FOREIGN KEY (chat_message_content_id)"
			"    REFERENCES chat_message_content(id)"
			"    ON DELETE CASCADE"
			") " + charset;

		*session <<
			"CREATE TABLE IF NOT EXISTS chat_message_content_app_data ("
			"  chat_message_content_id" + primaryKeyRefStr("BIGINT UNSIGNED") + ","

			"  name VARCHAR(255),"
			"  data BLOB NOT NULL,"

			"  PRIMARY KEY (chat_message_content_id, name),"
			"  FOREIGN KEY (chat_message_content_id)"
			"    REFERENCES chat_message_content(id)"
			"    ON DELETE CASCADE"
			") " + charset;

		*session <<
			"CREATE TABLE IF NOT EXISTS conference_message_crypto_data ("
			"  event_id" + primaryKeyRefStr("BIGINT UNSIGNED") + ","

			"  name VARCHAR(255),"
			"  data BLOB NOT NULL,"

			"  PRIMARY KEY (event_id, name),"
			"  FOREIGN KEY (event_id)"
			"    REFERENCES conference_chat_message_event(event_id)"
			"    ON DELETE CASCADE"
			") " + charset;

		// Trigger to delete participant_message cache entries.
		// TODO: Fix me in the future. (Problem on Mysql backend.)
		#if 0
		string displayedId = Utils::toString(static_cast<int>(ChatMessage::State::Displayed));
		string participantMessageDeleter =
			"CREATE TRIGGER IF NOT EXISTS chat_message_participant_deleter"
			"  AFTER UPDATE OF state ON chat_message_participant FOR EACH ROW"
			"  WHEN NEW.state = ";
		participantMessageDeleter += displayedId;
		participantMessageDeleter += " AND (SELECT COUNT(*) FROM ("
			"    SELECT state FROM chat_message_participant WHERE"
			"    NEW.event_id = chat_message_participant.event_id"
			"    AND state <> ";
		participantMessageDeleter += displayedId;
		participantMessageDeleter += "    LIMIT 1"
			"  )) = 0"
			"  BEGIN"
			"  DELETE FROM chat_message_participant WHERE NEW.event_id = chat_message_participant.event_id;"
			"  UPDATE conference_chat_message_event SET state = ";
		participantMessageDeleter += displayedId;
		participantMessageDeleter += " WHERE event_id = NEW.event_id;"
			"  END";
		#endif

		*session << participantMessageDeleter;
	}

	bool MainDb::addEvent (const shared_ptr<EventLog> &eventLog) {
		L_D();

		if (!isConnected()) {
			lWarning() << "Unable to add event. Not connected.";
			return false;
		}

		const EventLogPrivate *dEventLog = eventLog->getPrivate();
		if (dEventLog->dbKey.isValid()) {
			lWarning() << "Unable to add an event twice!!!";
			return false;
		}

		bool soFarSoGood = false;
		long long storageId = 0;

		L_BEGIN_LOG_EXCEPTION

		soci::transaction tr(*d->dbSession.getBackendSession<soci::session>());

		EventLog::Type type = eventLog->getType();
		switch (type) {
			case EventLog::Type::None:
				return false;

			case EventLog::Type::ConferenceCreated:
			case EventLog::Type::ConferenceTerminated:
				storageId = d->insertConferenceEvent(eventLog);
				break;

			case EventLog::Type::ConferenceCallStart:
			case EventLog::Type::ConferenceCallEnd:
				storageId = d->insertConferenceCallEvent(eventLog);
				break;

			case EventLog::Type::ConferenceChatMessage:
				storageId = d->insertConferenceChatMessageEvent(eventLog);
				break;

			case EventLog::Type::ConferenceParticipantAdded:
			case EventLog::Type::ConferenceParticipantRemoved:
			case EventLog::Type::ConferenceParticipantSetAdmin:
			case EventLog::Type::ConferenceParticipantUnsetAdmin:
				storageId = d->insertConferenceParticipantEvent(eventLog);
				break;

			case EventLog::Type::ConferenceParticipantDeviceAdded:
			case EventLog::Type::ConferenceParticipantDeviceRemoved:
				storageId = d->insertConferenceParticipantDeviceEvent(eventLog);
				break;

			case EventLog::Type::ConferenceSubjectChanged:
				storageId = d->insertConferenceSubjectEvent(eventLog);
				break;
		}

		if (storageId >= 0) {
			tr.commit();
			d->cache(eventLog, storageId);

			if (type == EventLog::Type::ConferenceChatMessage)
				d->cache(static_pointer_cast<ConferenceChatMessageEvent>(eventLog)->getChatMessage(), storageId);

			soFarSoGood = true;
		}

		L_END_LOG_EXCEPTION

		return soFarSoGood;
	}

	bool MainDb::updateEvent (const shared_ptr<EventLog> &eventLog) {
		L_D();

		if (!isConnected()) {
			lWarning() << "Unable to update event. Not connected.";
			return false;
		}

		const EventLogPrivate *dEventLog = eventLog->getPrivate();
		if (!dEventLog->dbKey.isValid()) {
			lWarning() << "Unable to update an event that wasn't inserted yet!!!";
			return false;
		}

		L_BEGIN_LOG_EXCEPTION

		soci::transaction tr(*d->dbSession.getBackendSession<soci::session>());

		switch (eventLog->getType()) {
			case EventLog::Type::None:
				return false;

			case EventLog::Type::ConferenceChatMessage:
				d->updateConferenceChatMessageEvent(eventLog);
				break;

			case EventLog::Type::ConferenceCreated:
			case EventLog::Type::ConferenceTerminated:
			case EventLog::Type::ConferenceCallStart:
			case EventLog::Type::ConferenceCallEnd:
			case EventLog::Type::ConferenceParticipantAdded:
			case EventLog::Type::ConferenceParticipantRemoved:
			case EventLog::Type::ConferenceParticipantSetAdmin:
			case EventLog::Type::ConferenceParticipantUnsetAdmin:
			case EventLog::Type::ConferenceParticipantDeviceAdded:
			case EventLog::Type::ConferenceParticipantDeviceRemoved:
			case EventLog::Type::ConferenceSubjectChanged:
				return false;
		}

		tr.commit();

		return true;

		L_END_LOG_EXCEPTION

		// Error.
		return false;
	}

	bool MainDb::deleteEvent (const shared_ptr<const EventLog> &eventLog) {
		const EventLogPrivate *dEventLog = eventLog->getPrivate();
		if (!dEventLog->dbKey.isValid()) {
			lWarning() << "Unable to delete invalid event.";
			return false;
		}

		MainDbKeyPrivate *dEventKey = static_cast<MainDbKey &>(dEventLog->dbKey).getPrivate();
		shared_ptr<Core> core = dEventKey->core.lock();
		L_ASSERT(core);

		MainDb &mainDb = *core->getPrivate()->mainDb.get();
		if (!mainDb.isConnected()) {
			lWarning() << "Unable to delete event. Not connected.";
			return false;
		}

		L_BEGIN_LOG_EXCEPTION

		soci::session *session = mainDb.getPrivate()->dbSession.getBackendSession<soci::session>();
		*session << "DELETE FROM event WHERE id = :id", soci::use(dEventKey->storageId);

		dEventLog->dbKey = MainDbEventKey();

		if (eventLog->getType() == EventLog::Type::ConferenceChatMessage)
			static_pointer_cast<const ConferenceChatMessageEvent>(
				eventLog
			)->getChatMessage()->getPrivate()->dbKey = MainDbChatMessageKey();

		return true;

		L_END_LOG_EXCEPTION

		// Error.
		return false;
	}

	int MainDb::getEventCount (FilterMask mask) const {
		L_D();

		if (!isConnected()) {
			lWarning() << "Unable to get events count. Not connected.";
			return 0;
		}

		string query = "SELECT COUNT(*) FROM event" +
			buildSqlEventFilter({ ConferenceCallFilter, ConferenceChatMessageFilter, ConferenceInfoFilter }, mask);
		int count = 0;

		DurationLogger durationLogger(
			"Get events count with mask=" + Utils::toString(static_cast<int>(mask)) + "."
		);

		L_BEGIN_LOG_EXCEPTION

		soci::session *session = d->dbSession.getBackendSession<soci::session>();
		*session << query, soci::into(count);

		L_END_LOG_EXCEPTION

		return count;
	}

	shared_ptr<EventLog> MainDb::getEventFromKey (const MainDbKey &dbKey) {
		shared_ptr<EventLog> event;

		if (!dbKey.isValid()) {
			lWarning() << "Unable to get event from invalid key.";
			return event;
		}

		unique_ptr<MainDb> &q = dbKey.getPrivate()->core.lock()->getPrivate()->mainDb;
		MainDbPrivate *d = q->getPrivate();

		if (!q->isConnected()) {
			lWarning() << "Unable to get event from key. Not connected.";
			return event;
		}

		const long long &storageId = dbKey.getPrivate()->storageId;
		event = d->getEventFromCache(storageId);
		if (event)
			return event;

		// TODO: Improve. Deal with all events in the future.
		static const string query = "SELECT peer_sip_address.value, local_sip_address.value, type, event.creation_time"
			"  FROM event, conference_event, chat_room, sip_address AS peer_sip_address, sip_address as local_sip_address"
			"  WHERE event.id = :eventId"
			"  AND conference_event.event_id = event.id"
			"  AND conference_event.chat_room_id = chat_room.id"
			"  AND chat_room.peer_sip_address_id = peer_sip_address.id"
			"  AND chat_room.local_sip_address_id = local_sip_address.id";

		L_BEGIN_LOG_EXCEPTION

		soci::session *session = d->dbSession.getBackendSession<soci::session>();
		soci::transaction tr(*session);

		string peerSipAddress;
		string localSipAddress;
		int type;
		tm creationTime;
		*session << query, soci::into(peerSipAddress), soci::into(localSipAddress), soci::into(type),
			soci::into(creationTime), soci::use(storageId);

		event = d->selectGenericConferenceEvent(
			storageId,
			static_cast<EventLog::Type>(type),
			Utils::getTmAsTimeT(creationTime),
			ChatRoomId(IdentityAddress(peerSipAddress), IdentityAddress(localSipAddress))
		);

		L_END_LOG_EXCEPTION

		return event;
	}

	list<shared_ptr<EventLog>> MainDb::getConferenceNotifiedEvents (
		const ChatRoomId &chatRoomId,
		unsigned int lastNotifyId
	) const {
		static const string query = "SELECT id, type, creation_time FROM event"
			"  WHERE id IN ("
			"    SELECT event_id FROM conference_notified_event WHERE event_id IN ("
			"      SELECT event_id FROM conference_event WHERE chat_room_id = :chatRoomId"
			"    ) AND notify_id > :lastNotifyId"
			"  )";

		L_D();

		list<shared_ptr<EventLog>> events;

		if (!isConnected()) {
			lWarning() << "Unable to get conference notified events. Not connected.";
			return events;
		}

		DurationLogger durationLogger(
			"Get conference notified events of: (peer=" + chatRoomId.getPeerAddress().asString() +
			", local=" + chatRoomId.getLocalAddress().asString() +
			", lastNotifyId=" + Utils::toString(lastNotifyId) + ")."
		);

		L_BEGIN_LOG_EXCEPTION

		soci::session *session = d->dbSession.getBackendSession<soci::session>();
		soci::transaction tr(*session);

		const long long &dbChatRoomId = d->selectChatRoomId(chatRoomId);

		soci::rowset<soci::row> rows = (session->prepare << query, soci::use(dbChatRoomId), soci::use(lastNotifyId));
		for (const auto &row : rows) {
			long long eventId = d->resolveId(row, 0);
			shared_ptr<EventLog> eventLog = d->getEventFromCache(eventId);

			events.push_back(eventLog ? eventLog : d->selectGenericConferenceEvent(
				eventId,
				static_cast<EventLog::Type>(row.get<int>(1)),
				Utils::getTmAsTimeT(row.get<tm>(2)),
				chatRoomId
			));
		}

		return events;

		L_END_LOG_EXCEPTION

		// Error.
		return list<shared_ptr<EventLog>>();
	}

	int MainDb::getChatMessageCount (const ChatRoomId &chatRoomId) const {
		L_D();

		if (!isConnected()) {
			lWarning() << "Unable to get messages count. Not connected.";
			return 0;
		}

		int count = 0;

		DurationLogger durationLogger(
			"Get chat messages count of: (peer=" + chatRoomId.getPeerAddress().asString() +
			", local=" + chatRoomId.getLocalAddress().asString() + ")."
		);

		L_BEGIN_LOG_EXCEPTION

		soci::session *session = d->dbSession.getBackendSession<soci::session>();

		string query = "SELECT COUNT(*) FROM conference_chat_message_event";
		if (!chatRoomId.isValid())
			*session << query, soci::into(count);
		else {
			query += "  WHERE event_id IN ("
				"  SELECT event_id FROM conference_event WHERE chat_room_id = :chatRoomId"
				")";

			const long long &dbChatRoomId = d->selectChatRoomId(chatRoomId);
			*session << query, soci::use(dbChatRoomId), soci::into(count);
		}

		L_END_LOG_EXCEPTION

		return count;
	}

	int MainDb::getUnreadChatMessageCount (const ChatRoomId &chatRoomId) const {
		L_D();

		if (!isConnected()) {
			lWarning() << "Unable to get unread messages count. Not connected.";
			return 0;
		}

		int count = 0;

		string query = "SELECT COUNT(*) FROM conference_chat_message_event WHERE";
		if (chatRoomId.isValid())
			query += " event_id IN ("
				"  SELECT event_id FROM conference_event WHERE chat_room_id = :chatRoomId"
				") AND";

		query += " direction = " + Utils::toString(static_cast<int>(ChatMessage::Direction::Incoming)) +
			+ " AND state <> " + Utils::toString(static_cast<int>(ChatMessage::State::Displayed));

		DurationLogger durationLogger(
			"Get unread chat messages count of: (peer=" + chatRoomId.getPeerAddress().asString() +
			", local=" + chatRoomId.getLocalAddress().asString() + ")."
		);

		L_BEGIN_LOG_EXCEPTION

		soci::session *session = d->dbSession.getBackendSession<soci::session>();

		if (!chatRoomId.isValid())
			*session << query, soci::into(count);
		else {
			const long long &dbChatRoomId = d->selectChatRoomId(chatRoomId);
			*session << query, soci::use(dbChatRoomId), soci::into(count);
		}

		L_END_LOG_EXCEPTION

		return count;
	}

	void MainDb::markChatMessagesAsRead (const ChatRoomId &chatRoomId) const {
		L_D();

		if (!isConnected()) {
			lWarning() << "Unable to mark messages as read. Not connected.";
			return;
		}

		if (getUnreadChatMessageCount(chatRoomId) == 0)
			return;

		string query = "UPDATE conference_chat_message_event"
			"  SET state = " + Utils::toString(static_cast<int>(ChatMessage::State::Displayed)) + " ";
		query += "WHERE";
		if (chatRoomId.isValid())
			query += " event_id IN ("
				"  SELECT event_id FROM conference_event WHERE chat_room_id = :chatRoomId"
				") AND";
		query += " direction = " + Utils::toString(static_cast<int>(ChatMessage::Direction::Incoming));

		DurationLogger durationLogger(
			"Mark chat messages as read of: (peer=" + chatRoomId.getPeerAddress().asString() +
			", local=" + chatRoomId.getLocalAddress().asString() + ")."
		);

		L_BEGIN_LOG_EXCEPTION

		soci::session *session = d->dbSession.getBackendSession<soci::session>();

		if (!chatRoomId.isValid())
			*session << query;
		else {
			const long long &dbChatRoomId = d->selectChatRoomId(chatRoomId);
			*session << query, soci::use(dbChatRoomId);
		}

		L_END_LOG_EXCEPTION
	}

	list<shared_ptr<ChatMessage>> MainDb::getUnreadChatMessages (const ChatRoomId &chatRoomId) const {
		L_D();

		list<shared_ptr<ChatMessage>> chatMessages;

		if (!isConnected()) {
			lWarning() << "Unable to get unread chat messages. Not connected.";
			return chatMessages;
		}

		string query = "SELECT id, creation_time FROM event WHERE"
			"  id IN ("
			"    SELECT conference_event.event_id FROM conference_event, conference_chat_message_event"
			"    WHERE";
		if (chatRoomId.isValid())
			query += "    chat_room_id = :chatRoomId AND ";
		query += "    conference_event.event_id = conference_chat_message_event.event_id"
			"    AND direction = " + Utils::toString(static_cast<int>(ChatMessage::Direction::Incoming)) +
			"    AND state <> " + Utils::toString(static_cast<int>(ChatMessage::State::Displayed)) +
			")";

		DurationLogger durationLogger(
			"Get unread chat messages: (peer=" + chatRoomId.getPeerAddress().asString() +
			", local=" + chatRoomId.getLocalAddress().asString() + ")."
		);

		L_BEGIN_LOG_EXCEPTION

		soci::session *session = d->dbSession.getBackendSession<soci::session>();
		soci::transaction tr(*session);

		long long dbChatRoomId;
		if (chatRoomId.isValid())
			dbChatRoomId = d->selectChatRoomId(chatRoomId);

		soci::rowset<soci::row> rows = chatRoomId.isValid()
			? (session->prepare << query, soci::use(dbChatRoomId))
			: (session->prepare << query);

		for (const auto &row : rows) {
			long long eventId = d->resolveId(row, 0);
			shared_ptr<EventLog> event = d->getEventFromCache(eventId);

			if (!event)
				event = d->selectGenericConferenceEvent(
					eventId,
					EventLog::Type::ConferenceChatMessage,
					Utils::getTmAsTimeT(row.get<tm>(1)),
					chatRoomId
				);

			if (event)
				chatMessages.push_back(static_pointer_cast<ConferenceChatMessageEvent>(event)->getChatMessage());
		}

		return chatMessages;

		L_END_LOG_EXCEPTION

		// Error.
		return list<shared_ptr<ChatMessage>>();
	}

	shared_ptr<ChatMessage> MainDb::getLastChatMessage (const ChatRoomId &chatRoomId) const {
		list<shared_ptr<EventLog>> chatList = getHistory(chatRoomId, 1, Filter::ConferenceChatMessageFilter);
		if (chatList.empty())
			return nullptr;

		return static_pointer_cast<ConferenceChatMessageEvent>(chatList.front())->getChatMessage();
	}

	list<shared_ptr<ChatMessage>> MainDb::findChatMessages (
		const ChatRoomId &chatRoomId,
		const string &imdnMessageId
	) const {
		L_D();

		list<shared_ptr<ChatMessage>> chatMessages;

		if (!isConnected()) {
			lWarning() << "Unable to find chat messages. Not connected.";
			return chatMessages;
		}

		static const string query = "SELECT id, type, creation_time FROM event"
			"  WHERE id IN ("
			"    SELECT event_id FROM conference_event"
			"    WHERE event_id IN (SELECT event_id FROM conference_chat_message_event WHERE imdn_message_id = :imdnMessageId)"
			"    AND chat_room_id = :chatRoomId"
			"  )";

		DurationLogger durationLogger(
			"Find chat messages: (peer=" + chatRoomId.getPeerAddress().asString() +
			", local=" + chatRoomId.getLocalAddress().asString() + ")."
		);

		L_BEGIN_LOG_EXCEPTION

		soci::session *session = d->dbSession.getBackendSession<soci::session>();
		soci::transaction tr(*session);

		const long long &dbChatRoomId = d->selectChatRoomId(chatRoomId);
		soci::rowset<soci::row> rows = (session->prepare << query, soci::use(imdnMessageId), soci::use(dbChatRoomId));
		for (const auto &row : rows) {
			long long eventId = d->resolveId(row, 0);
			shared_ptr<EventLog> event = d->getEventFromCache(eventId);

			if (!event)
				event = d->selectGenericConferenceEvent(
					eventId,
					static_cast<EventLog::Type>(row.get<int>(1)),
					Utils::getTmAsTimeT(row.get<tm>(2)),
					chatRoomId
				);

			if (event) {
				L_ASSERT(event->getType() == EventLog::Type::ConferenceChatMessage);
				chatMessages.push_back(static_pointer_cast<ConferenceChatMessageEvent>(event)->getChatMessage());
			} else
				lWarning() << "Unable to fetch event: " << eventId;
		}

		return chatMessages;

		L_END_LOG_EXCEPTION

		// Error.
		return list<shared_ptr<ChatMessage>>();
	}

	list<shared_ptr<EventLog>> MainDb::getHistory (const ChatRoomId &chatRoomId, int nLast, FilterMask mask) const {
		return getHistoryRange(chatRoomId, 0, nLast, mask);
	}

	list<shared_ptr<EventLog>> MainDb::getHistoryRange (
		const ChatRoomId &chatRoomId,
		int begin,
		int end,
		FilterMask mask
	) const {
		L_D();

		list<shared_ptr<EventLog>> events;

		if (!isConnected()) {
			lWarning() << "Unable to get history. Not connected.";
			return events;
		}

		if (begin < 0)
			begin = 0;

		if (end > 0 && begin > end) {
			lWarning() << "Unable to get history. Invalid range.";
			return events;
		}

		string query = "SELECT id, type, creation_time FROM event"
			"  WHERE id IN ("
			"    SELECT event_id FROM conference_event WHERE chat_room_id = :chatRoomId"
			"  )";
		query += buildSqlEventFilter({
			ConferenceCallFilter, ConferenceChatMessageFilter, ConferenceInfoFilter
		}, mask, "AND");
		query += "  ORDER BY creation_time DESC";

		if (end > 0)
			query += "  LIMIT " + Utils::toString(end - begin);
		else
			query += "  LIMIT -1";

		if (begin > 0)
			query += "  OFFSET " + Utils::toString(begin);

		DurationLogger durationLogger(
			"Get history range of: (peer=" + chatRoomId.getPeerAddress().asString() +
			", local=" + chatRoomId.getLocalAddress().asString() +
			", begin=" + Utils::toString(begin) + ", end=" + Utils::toString(end) + ")."
		);

		L_BEGIN_LOG_EXCEPTION

		soci::session *session = d->dbSession.getBackendSession<soci::session>();
		soci::transaction tr(*session);

		const long long &dbChatRoomId = d->selectChatRoomId(chatRoomId);
		soci::rowset<soci::row> rows = (session->prepare << query, soci::use(dbChatRoomId));
		for (const auto &row : rows) {
			long long eventId = d->resolveId(row, 0);
			shared_ptr<EventLog> event = d->getEventFromCache(eventId);

			if (!event)
				event = d->selectGenericConferenceEvent(
					eventId,
					static_cast<EventLog::Type>(row.get<int>(1)),
					Utils::getTmAsTimeT(row.get<tm>(2)),
					chatRoomId
				);

			if (event)
				events.push_front(event);
			else
				lWarning() << "Unable to fetch event: " << eventId;
		}

		return events;

		L_END_LOG_EXCEPTION

		// Error.
		return list<shared_ptr<EventLog>>();
	}

	int MainDb::getHistorySize (const ChatRoomId &chatRoomId, FilterMask mask) const {
		L_D();

		if (!isConnected()) {
			lWarning() << "Unable to get history size. Not connected.";
			return 0;
		}

		int count = 0;

		const string query = "SELECT COUNT(*) FROM event, conference_event"
			"  WHERE chat_room_id = :chatRoomId"
			"  AND event_id = event.id" +
			buildSqlEventFilter({
				ConferenceCallFilter, ConferenceChatMessageFilter, ConferenceInfoFilter
			}, mask, "AND");

		L_BEGIN_LOG_EXCEPTION

		const long long &dbChatRoomId = d->selectChatRoomId(chatRoomId);

		soci::session *session = d->dbSession.getBackendSession<soci::session>();
		*session << query, soci::into(count), soci::use(dbChatRoomId);

		L_END_LOG_EXCEPTION

		return count;
	}

	void MainDb::cleanHistory (const ChatRoomId &chatRoomId, FilterMask mask) {
		L_D();

		if (!isConnected()) {
			lWarning() << "Unable to clean history. Not connected.";
			return;
		}

		const string query = "SELECT event_id FROM conference_event WHERE chat_room_id = :chatRoomId" +
			buildSqlEventFilter({
				ConferenceCallFilter, ConferenceChatMessageFilter, ConferenceInfoFilter
			}, mask);

		DurationLogger durationLogger(
			"Clean history of: (peer=" + chatRoomId.getPeerAddress().asString() +
			", local=" + chatRoomId.getLocalAddress().asString() +
			", mask=" + Utils::toString(static_cast<int>(mask)) + ")."
		);

		L_BEGIN_LOG_EXCEPTION

		const long long &dbChatRoomId = d->selectChatRoomId(chatRoomId);

		d->invalidConferenceEventsFromQuery(query, dbChatRoomId);
		soci::session *session = d->dbSession.getBackendSession<soci::session>();
		*session << "DELETE FROM event WHERE id IN (" + query + ")", soci::use(dbChatRoomId);

		L_END_LOG_EXCEPTION
	}

// -----------------------------------------------------------------------------

	list<shared_ptr<AbstractChatRoom>> MainDb::getChatRooms () const {
		static const string query = "SELECT chat_room.id, peer_sip_address.value, local_sip_address.value, "
			"creation_time, last_update_time, capabilities, subject, last_notify_id, flags"
			"  FROM chat_room, sip_address AS peer_sip_address, sip_address AS local_sip_address"
			"  WHERE chat_room.peer_sip_address_id = peer_sip_address.id AND chat_room.local_sip_address_id = local_sip_address.id"
			"  ORDER BY last_update_time DESC";

		L_D();

		list<shared_ptr<AbstractChatRoom>> chatRooms;

		if (!isConnected()) {
			lWarning() << "Unable to get chat rooms. Not connected.";
			return chatRooms;
		}

		shared_ptr<Core> core = getCore();

		DurationLogger durationLogger("Get chat rooms.");

		L_BEGIN_LOG_EXCEPTION

		soci::session *session = d->dbSession.getBackendSession<soci::session>();

		soci::rowset<soci::row> rows = (session->prepare << query);
		for (const auto &row : rows) {
			ChatRoomId chatRoomId = ChatRoomId(
				IdentityAddress(row.get<string>(1)),
				IdentityAddress(row.get<string>(2))
			);
			shared_ptr<AbstractChatRoom> chatRoom = core->findChatRoom(chatRoomId);
			if (chatRoom) {
				chatRooms.push_back(chatRoom);
				continue;
			}

			tm creationTime = row.get<tm>(3);
			tm lastUpdateTime = row.get<tm>(4);
			int capabilities = row.get<int>(5);
			string subject = row.get<string>(6, "");
			unsigned int lastNotifyId = (getBackend() == Backend::Mysql)
				? row.get<unsigned int>(7, 0)
				: static_cast<unsigned int>(row.get<int>(7, 0));

			if (capabilities & static_cast<int>(ChatRoom::Capabilities::Basic)) {
				chatRoom = core->getPrivate()->createBasicChatRoom(
					chatRoomId,
					capabilities & static_cast<int>(ChatRoom::Capabilities::RealTimeText)
				);
				chatRoom->setSubject(subject);
			} else if (capabilities & static_cast<int>(ChatRoom::Capabilities::Conference)) {
				list<shared_ptr<Participant>> participants;

				const long long &dbChatRoomId = d->resolveId(row, 0);
				static const string query = "SELECT sip_address.value, is_admin"
					"  FROM sip_address, chat_room, chat_room_participant"
					"  WHERE chat_room.id = :chatRoomId"
					"  AND sip_address.id = chat_room_participant.participant_sip_address_id"
					"  AND chat_room_participant.chat_room_id = chat_room.id";

				soci::rowset<soci::row> rows = (session->prepare << query, soci::use(dbChatRoomId));
				shared_ptr<Participant> me;
				for (const auto &row : rows) {
					shared_ptr<Participant> participant = make_shared<Participant>(IdentityAddress(row.get<string>(0)));
					participant->getPrivate()->setAdmin(!!row.get<int>(1));

					if (participant->getAddress() == chatRoomId.getLocalAddress().getAddressWithoutGruu())
						me = participant;
					else
						participants.push_back(participant);
				}

				if (!linphone_core_conference_server_enabled(core->getCCore())) {
					bool hasBeenLeft = !!row.get<int>(8, 0);
					if (!me) {
						lError() << "Unable to find me in: (peer=" + chatRoomId.getPeerAddress().asString() +
							", local=" + chatRoomId.getLocalAddress().asString() + ").";
						continue;
					}
					chatRoom = make_shared<ClientGroupChatRoom>(
						core,
						chatRoomId,
						me,
						subject,
						move(participants),
						lastNotifyId
					);
					chatRoom->getPrivate()->setState(LinphonePrivate::ChatRoom::State::Instantiated);
					chatRoom->getPrivate()->setState(hasBeenLeft
						? ChatRoom::State::Terminated
						: ChatRoom::State::Created
					);
				} else {
					chatRoom = make_shared<ServerGroupChatRoom>(
						core,
						chatRoomId.getPeerAddress(),
						subject,
						move(participants),
						lastNotifyId
					);
					chatRoom->getPrivate()->setState(LinphonePrivate::ChatRoom::State::Instantiated);
					chatRoom->getPrivate()->setState(LinphonePrivate::ChatRoom::State::Created);
				}
			}

			if (!chatRoom)
				continue; // Not fetched.

			AbstractChatRoomPrivate *dChatRoom = chatRoom->getPrivate();
			dChatRoom->setCreationTime(Utils::getTmAsTimeT(creationTime));
			dChatRoom->setLastUpdateTime(Utils::getTmAsTimeT(lastUpdateTime));

			lInfo() << "Found chat room in DB: (peer=" <<
				chatRoomId.getPeerAddress().asString() << ", local=" << chatRoomId.getLocalAddress().asString() << ").";

			chatRooms.push_back(chatRoom);
		}

		return chatRooms;

		L_END_LOG_EXCEPTION

		// Error.
		return list<shared_ptr<AbstractChatRoom>>();
	}

	void MainDb::insertChatRoom (const shared_ptr<AbstractChatRoom> &chatRoom) {
		L_D();

		if (!isConnected()) {
			lWarning() << "Unable to insert chat room. Not connected.";
			return;
		}

		const ChatRoomId &chatRoomId = chatRoom->getChatRoomId();
		DurationLogger durationLogger(
			"Insert chat room: (peer=" + chatRoomId.getPeerAddress().asString() +
			", local=" + chatRoomId.getLocalAddress().asString() + ")."
		);

		L_BEGIN_LOG_EXCEPTION

		soci::transaction tr(*d->dbSession.getBackendSession<soci::session>());

		d->insertChatRoom(chatRoom);

		tr.commit();

		L_END_LOG_EXCEPTION
	}

	void MainDb::deleteChatRoom (const ChatRoomId &chatRoomId) {
		L_D();

		if (!isConnected()) {
			lWarning() << "Unable to delete chat room. Not connected.";
			return;
		}

		DurationLogger durationLogger(
			"Delete chat room: (peer=" + chatRoomId.getPeerAddress().asString() +
			", local=" + chatRoomId.getLocalAddress().asString() + "`)."
		);

		L_BEGIN_LOG_EXCEPTION

		const long long &dbChatRoomId = d->selectChatRoomId(chatRoomId);

		d->invalidConferenceEventsFromQuery(
			"SELECT event_id FROM conference_event WHERE chat_room_id = :chatRoomId",
			dbChatRoomId
		);

		soci::session *session = d->dbSession.getBackendSession<soci::session>();
		*session << "DELETE FROM chat_room WHERE id = :chatRoomId", soci::use(dbChatRoomId);

		L_END_LOG_EXCEPTION
	}

// -----------------------------------------------------------------------------

	#define LEGACY_MESSAGE_COL_LOCAL_ADDRESS 1
	#define LEGACY_MESSAGE_COL_REMOTE_ADDRESS 2
	#define LEGACY_MESSAGE_COL_DIRECTION 3
	#define LEGACY_MESSAGE_COL_TEXT 4
	#define LEGACY_MESSAGE_COL_STATE 7
	#define LEGACY_MESSAGE_COL_URL 8
	#define LEGACY_MESSAGE_COL_DATE 9
	#define LEGACY_MESSAGE_COL_APP_DATA 10
	#define LEGACY_MESSAGE_COL_CONTENT_ID 11
	#define LEGACY_MESSAGE_COL_IMDN_MESSAGE_ID 12
	#define LEGACY_MESSAGE_COL_CONTENT_TYPE 13
	#define LEGACY_MESSAGE_COL_IS_SECURED 14

	template<typename T>
	static T getValueFromLegacyMessage (const soci::row &message, int index, bool &isNull) {
		isNull = false;

		try {
			return message.get<T>(static_cast<size_t>(index));
		} catch (const exception &) {
			isNull = true;
		}

		return T();
	}

	bool MainDb::import (Backend, const string &parameters) {
		L_D();

		if (!isConnected()) {
			lWarning() << "Unable to import data. Not connected.";
			return 0;
		}

		// Backend is useless, it's sqlite3. (Only available legacy backend.)
		const string uri = "sqlite3://" + parameters;
		DbSession inDbSession = DbSessionProvider::getInstance()->getSession(uri);

		if (!inDbSession) {
			lWarning() << "Unable to connect to: `" << uri << "`.";
			return false;
		}

		soci::session *inSession = inDbSession.getBackendSession<soci::session>();

		// Import messages.
		try {
			soci::rowset<soci::row> messages = (inSession->prepare << "SELECT * FROM history");
			try {
				soci::transaction tr(*d->dbSession.getBackendSession<soci::session>());

				for (const auto &message : messages) {
					const int direction = message.get<int>(LEGACY_MESSAGE_COL_DIRECTION);
					if (direction != 0 && direction != 1) {
						lWarning() << "Unable to import legacy message with invalid direction.";
						continue;
					}

					const int &state = message.get<int>(
						LEGACY_MESSAGE_COL_STATE, static_cast<int>(ChatMessage::State::Displayed)
					);
					if (state < 0 || state > static_cast<int>(ChatMessage::State::Displayed)) {
						lWarning() << "Unable to import legacy message with invalid state.";
						continue;
					}

					const tm &creationTime = Utils::getTimeTAsTm(message.get<int>(LEGACY_MESSAGE_COL_DATE, 0));

					bool isNull;
					getValueFromLegacyMessage<string>(message, LEGACY_MESSAGE_COL_URL, isNull);

					const int &contentId = message.get<int>(LEGACY_MESSAGE_COL_CONTENT_ID, -1);
					ContentType contentType(message.get<string>(LEGACY_MESSAGE_COL_CONTENT_TYPE, ""));
					if (!contentType.isValid())
						contentType = contentId != -1
							? ContentType::FileTransfer
							: (isNull ? ContentType::PlainText : ContentType::ExternalBody);
					if (contentType == ContentType::ExternalBody) {
						lInfo() << "Import of external body content is skipped.";
						continue;
					}

					const string &text = getValueFromLegacyMessage<string>(message, LEGACY_MESSAGE_COL_TEXT, isNull);

					Content content;
					content.setContentType(contentType);
					if (contentType == ContentType::PlainText) {
						if (isNull) {
							lWarning() << "Unable to import legacy message with no text.";
							continue;
						}
						content.setBody(text);
					} else {
						if (contentType != ContentType::FileTransfer) {
							lWarning() << "Unable to import unsupported legacy content.";
							continue;
						}

						const string appData = getValueFromLegacyMessage<string>(message, LEGACY_MESSAGE_COL_APP_DATA, isNull);
						if (isNull) {
							lWarning() << "Unable to import legacy file message without app data.";
							continue;
						}

						content.setAppData("legacy", appData);
					}

					soci::session *session = d->dbSession.getBackendSession<soci::session>();
					const int &eventType = static_cast<int>(EventLog::Type::ConferenceChatMessage);
					*session << "INSERT INTO event (type, creation_time) VALUES (:type, :creationTime)",
						soci::use(eventType), soci::use(creationTime);

					const long long &eventId = getLastInsertId();
					const long long &localSipAddressId = d->insertSipAddress(message.get<string>(LEGACY_MESSAGE_COL_LOCAL_ADDRESS));
					const long long &remoteSipAddressId = d->insertSipAddress(message.get<string>(LEGACY_MESSAGE_COL_REMOTE_ADDRESS));
					const long long &chatRoomId = d->insertOrUpdateBasicChatRoom(
						remoteSipAddressId,
						localSipAddressId,
						creationTime
					);
					const int &isSecured = message.get<int>(LEGACY_MESSAGE_COL_IS_SECURED, 0);

					*session << "INSERT INTO conference_event (event_id, chat_room_id)"
						"  VALUES (:eventId, :chatRoomId)", soci::use(eventId), soci::use(chatRoomId);

					*session << "INSERT INTO conference_chat_message_event ("
						"  event_id, from_sip_address_id, to_sip_address_id,"
						"  time, state, direction, imdn_message_id, is_secured"
						") VALUES ("
						"  :eventId, :localSipAddressId, :remoteSipAddressId,"
						"  :creationTime, :state, :direction, '', :isSecured"
						")", soci::use(eventId), soci::use(localSipAddressId), soci::use(remoteSipAddressId),
						soci::use(creationTime), soci::use(state), soci::use(direction),
						soci::use(isSecured);

					d->insertContent(eventId, content);
					d->insertChatRoomParticipant(chatRoomId, remoteSipAddressId, false);

					if (state != static_cast<int>(ChatMessage::State::Displayed))
						d->insertChatMessageParticipant(eventId, remoteSipAddressId, state);
				}

				tr.commit();
			} catch (const exception &e) {
				lInfo() << "Failed to import legacy messages from: `" << uri << "`. (" << e.what() << ")";
				return false;
			}
			lInfo() << "Successful import of legacy messages from: `" << uri << "`.";
		} catch (const exception &) {
			// Table doesn't exist.
			return false;
		}

		return true;
	}

// -----------------------------------------------------------------------------
// No backend.
// -----------------------------------------------------------------------------

#else

	void MainDb::init () {}

	bool MainDb::addEvent (const shared_ptr<EventLog> &) {
		return false;
	}

	bool MainDb::updateEvent (const std::shared_ptr<EventLog> &) {
		return false;
	}

	bool MainDb::deleteEvent (const shared_ptr<const EventLog> &) {
		return false;
	}

	int MainDb::getEventCount (FilterMask) const {
		return 0;
	}

	shared_ptr<EventLog> MainDb::getEventFromKey (const MainDbKey &) {
		return nullptr;
	}

	list<shared_ptr<EventLog>> MainDb::getConferenceNotifiedEvents (
		const ChatRoomId &,
		unsigned int
	) const {
		return list<shared_ptr<EventLog>>();
	}

	int MainDb::getChatMessageCount (const ChatRoomId &) const {
		return 0;
	}

	int MainDb::getUnreadChatMessageCount (const ChatRoomId &) const {
		return 0;
	}

	shared_ptr<ChatMessage> MainDb::getLastChatMessage (const ChatRoomId &) const {
		return nullptr;
	}

	list<std::shared_ptr<ChatMessage>> MainDb::findChatMessages (const ChatRoomId &, const std::string &) const {
		return list<shared_ptr<ChatMessage>>();
	}

	void MainDb::markChatMessagesAsRead (const ChatRoomId &) const {}

	list<shared_ptr<ChatMessage>> MainDb::getUnreadChatMessages (const ChatRoomId &) const {
		return list<shared_ptr<ChatMessage>>();
	}

	list<shared_ptr<EventLog>> MainDb::getHistory (const ChatRoomId &, int, FilterMask) const {
		return list<shared_ptr<EventLog>>();
	}

	list<shared_ptr<EventLog>> MainDb::getHistoryRange (const ChatRoomId &, int, int, FilterMask) const {
		return list<shared_ptr<EventLog>>();
	}

	int getHistorySize (const ChatRoomId &, FilterMask) const {
		return 0;
	}

	list<shared_ptr<AbstractChatRoom>> MainDb::getChatRooms () const {
		return list<shared_ptr<AbstractChatRoom>>();
	}

	void MainDb::insertChatRoom (const shared_ptr<AbstractChatRoom> &) {}

	void MainDb::deleteChatRoom (const ChatRoomId &) {}

	void MainDb::cleanHistory (const ChatRoomId &, FilterMask) {}

	bool MainDb::import (Backend, const string &) {
		return false;
	}

#endif // ifdef SOCI_ENABLED

LINPHONE_END_NAMESPACE

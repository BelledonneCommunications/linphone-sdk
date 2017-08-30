/*
message_storage.c
Copyright (C) 2012  Belledonne Communications, Grenoble, France

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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "private.h"
#include "linphone/core.h"


#ifdef SQLITE_STORAGE_ENABLED

#include "chat/chat-room.h"

#ifndef _WIN32
#if !defined(__QNXNTO__) && !defined(__ANDROID__)
#include <ctype.h>
#include <langinfo.h>
#include <locale.h>
#include <iconv.h>
#include <string.h>
#endif
#else
#include <Windows.h>
#endif

#define MAX_PATH_SIZE 1024

#include "sqlite3.h"
#include <assert.h>


extern LinphonePrivate::ChatRoom& linphone_chat_room_get_cpp_obj(LinphoneChatRoom *cr);


static char *utf8_convert(const char *filename){
	char db_file_utf8[MAX_PATH_SIZE] = "";
#if defined(_WIN32)
	wchar_t db_file_utf16[MAX_PATH_SIZE]={0};
	MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, filename, -1, db_file_utf16, MAX_PATH_SIZE);
	WideCharToMultiByte(CP_UTF8, 0, db_file_utf16, -1, db_file_utf8, sizeof(db_file_utf8), NULL, NULL);
#elif defined(__QNXNTO__) || defined(__ANDROID__)
	strncpy(db_file_utf8, filename, MAX_PATH_SIZE - 1);
#else
	char db_file_locale[MAX_PATH_SIZE] = {'\0'};
	char *inbuf=db_file_locale, *outbuf=db_file_utf8;
	size_t inbyteleft = MAX_PATH_SIZE, outbyteleft = MAX_PATH_SIZE;
	iconv_t cb;

	if (strcasecmp("UTF-8", nl_langinfo(CODESET)) == 0) {
		strncpy(db_file_utf8, filename, MAX_PATH_SIZE - 1);
	} else {
		strncpy(db_file_locale, filename, MAX_PATH_SIZE-1);
		cb = iconv_open("UTF-8", nl_langinfo(CODESET));
		if (cb != (iconv_t)-1) {
			int ret;
			ret = iconv(cb, &inbuf, &inbyteleft, &outbuf, &outbyteleft);
			if(ret == -1) db_file_utf8[0] = '\0';
			iconv_close(cb);
		}
	}
#endif
	return ms_strdup(db_file_utf8);
}


int _linphone_sqlite3_open(const char *db_file, sqlite3 **db) {
	char* errmsg = NULL;
	int ret;
	int flags = SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE;

#if TARGET_OS_IPHONE
	/* the secured filesystem of the iPHone doesn't allow writing while the app is in background mode, which is problematic.
	 * We workaround by asking that the open is made with no protection*/
	flags |= SQLITE_OPEN_FILEPROTECTION_NONE;
#endif

	/*since we plug our vfs into sqlite, we convert to UTF-8.
	 * On Windows, the filename has to be converted back to windows native charset.*/
	char *utf8_filename = utf8_convert(db_file);
	ret = sqlite3_open_v2(utf8_filename, db, flags, LINPHONE_SQLITE3_VFS);
	ms_free(utf8_filename);

	if (ret != SQLITE_OK) return ret;
	// Some platforms do not provide a way to create temporary files which are needed
	// for transactions... so we work in memory only
	// see http ://www.sqlite.org/compile.html#temp_store
	ret = sqlite3_exec(*db, "PRAGMA temp_store=MEMORY", NULL, NULL, &errmsg);
	if (ret != SQLITE_OK) {
		ms_error("Cannot set sqlite3 temporary store to memory: %s.", errmsg);
		sqlite3_free(errmsg);
	}

	/* the lines below have been disabled because they are likely an
	 * outdated hack */
#if 0 && TARGET_OS_IPHONE
	ret = sqlite3_exec(*db, "PRAGMA journal_mode = OFF", NULL, NULL, &errmsg);
	if (ret != SQLITE_OK) {
		ms_error("Cannot set sqlite3 journal_mode to off: %s.", errmsg);
		sqlite3_free(errmsg);
	}
#endif
	return ret;
}
#endif



#ifdef SQLITE_STORAGE_ENABLED

/* DB layout:
 * | 0  | storage_id
 * | 1  | type
 * | 2  | subtype
 * | 3  | name
 * | 4  | encoding
 * | 5  | size
 * | 6  | data (currently not stored)
 * | 7  | key size
 * | 8  | key
 */
// Callback for sql request when getting linphone content
static int callback_content(void *data, int argc, char **argv, char **colName) {
	LinphoneChatMessage *message = (LinphoneChatMessage *)data;

	if (message->file_transfer_information) {
		linphone_content_unref(message->file_transfer_information);
		message->file_transfer_information = NULL;
	}
	message->file_transfer_information = linphone_content_new();
	if (argv[1]) linphone_content_set_type(message->file_transfer_information, argv[1]);
	if (argv[2]) linphone_content_set_subtype(message->file_transfer_information, argv[2]);
	if (argv[3]) linphone_content_set_name(message->file_transfer_information, argv[3]);
	if (argv[4]) linphone_content_set_encoding(message->file_transfer_information, argv[4]);
	linphone_content_set_size(message->file_transfer_information, (size_t)atoi(argv[5]));
	if (argv[8]) linphone_content_set_key(message->file_transfer_information, argv[8], (size_t)atol(argv[7]));

	return 0;
}

void linphone_chat_message_fetch_content_from_database(sqlite3 *db, LinphoneChatMessage *message, int content_id) {
	char* errmsg = NULL;
	int ret;
	char * buf;

	buf = sqlite3_mprintf("SELECT * FROM content WHERE id = %i", content_id);
	ret = sqlite3_exec(db, buf, callback_content, message, &errmsg);
	if (ret != SQLITE_OK) {
		ms_error("Error in creation: %s.", errmsg);
		sqlite3_free(errmsg);
	}
	sqlite3_free(buf);
}

// Called when fetching all conversations from database
static int callback_all(void *data, int argc, char **argv, char **colName){
	LinphoneCore* lc = (LinphoneCore*) data;
	char* address = argv[0];
	LinphoneAddress *addr = linphone_address_new(address);
	if (addr){
		linphone_core_get_chat_room(lc, addr);
		linphone_address_unref(addr);
	}
	return 0;
}

int linphone_sql_request(sqlite3* db,const char *stmt){
	char* errmsg=NULL;
	int ret;
	ret=sqlite3_exec(db,stmt,NULL,NULL,&errmsg);
	if(ret != SQLITE_OK) {
		ms_error("linphone_sql_request: statement %s -> error sqlite3_exec(): %s.", stmt, errmsg);
		sqlite3_free(errmsg);
	}
	return ret;
}

// Process the request to fetch all chat contacts
void linphone_sql_request_all(sqlite3* db,const char *stmt, LinphoneCore* lc){
	char* errmsg=NULL;
	int ret;
	ret=sqlite3_exec(db,stmt,callback_all,lc,&errmsg);
	if(ret != SQLITE_OK) {
		ms_error("linphone_sql_request_all: error sqlite3_exec(): %s.", errmsg);
		sqlite3_free(errmsg);
	}
}

static int linphone_chat_message_store_content(LinphoneChatMessage *msg) {
	LinphoneCore *lc = linphone_chat_room_get_core(msg->chat_room);
	int id = -1;
	if (lc->db) {
		LinphoneContent *content = msg->file_transfer_information;
		char *buf = sqlite3_mprintf("INSERT INTO content VALUES(NULL,%Q,%Q,%Q,%Q,%i,%Q,%lld,%Q);",
						linphone_content_get_type(content),
						linphone_content_get_subtype(content),
						linphone_content_get_name(content),
						linphone_content_get_encoding(content),
						linphone_content_get_size(content),
						NULL,
						(int64_t)linphone_content_get_key_size(content),
						linphone_content_get_key(content)
					);
		linphone_sql_request(lc->db, buf);
		sqlite3_free(buf);
		id = (unsigned int) sqlite3_last_insert_rowid (lc->db);
	}
	return id;
}

unsigned int linphone_chat_message_store(LinphoneChatMessage *msg){
	LinphoneCore *lc=linphone_chat_room_get_core(msg->chat_room);
	int id = 0;

	if (lc->db){
		int content_id = -1;
		char *peer;
		char *local_contact;
		char *buf;
		if (msg->file_transfer_information) {
			content_id = linphone_chat_message_store_content(msg);
		}

		peer=linphone_address_as_string_uri_only(linphone_chat_room_get_peer_address(msg->chat_room));
		local_contact=linphone_address_as_string_uri_only(linphone_chat_message_get_local_address(msg));
		buf = sqlite3_mprintf("INSERT INTO history VALUES(NULL,%Q,%Q,%i,%Q,%Q,%i,%i,%Q,%lld,%Q,%i,%Q,%Q,%i);",
						local_contact,
						peer,
						msg->dir,
						msg->message,
						"-1", /* use UTC field now */
						FALSE, /* use state == LinphoneChatMessageStateDisplayed now */
						msg->state,
						msg->external_body_url,
						(int64_t)msg->time,
						msg->appdata,
						content_id,
						msg->message_id,
						msg->content_type,
						(int)msg->is_secured
					);
		linphone_sql_request(lc->db,buf);
		sqlite3_free(buf);
		ms_free(local_contact);
		ms_free(peer);
		id = (unsigned int) sqlite3_last_insert_rowid (lc->db);
	}
	return id;
}

void linphone_chat_message_store_update(LinphoneChatMessage *msg) {
	LinphoneCore *lc = linphone_chat_room_get_core(msg->chat_room);

	if (lc->db) {
		char *peer;
		char *local_contact;
		char *buf;

		peer = linphone_address_as_string_uri_only(linphone_chat_room_get_peer_address(msg->chat_room));
		local_contact = linphone_address_as_string_uri_only(linphone_chat_message_get_local_address(msg));
		buf = sqlite3_mprintf("UPDATE history SET"
			" localContact = %Q,"
			" remoteContact = %Q,"
			" message = %Q,"
			" status = %i,"
			" appdata = %Q,"
			" messageId = %Q,"
			" content_type = %Q"
			" WHERE (id = %u);",
			local_contact,
			peer,
			msg->message,
			msg->state,
			msg->appdata,
			msg->message_id,
			msg->content_type,
			msg->storage_id
		);
		linphone_sql_request(lc->db, buf);
		sqlite3_free(buf);
		ms_free(local_contact);
		ms_free(peer);
	}
}

void linphone_chat_message_store_state(LinphoneChatMessage *msg){
	LinphoneCore *lc=linphone_chat_room_get_core(msg->chat_room);
	if (lc->db){
		char *buf=sqlite3_mprintf("UPDATE history SET status=%i WHERE (id = %u);",
								  msg->state,msg->storage_id);
		linphone_sql_request(lc->db,buf);
		sqlite3_free(buf);
	}
}

void linphone_chat_message_store_appdata(LinphoneChatMessage* msg){
	LinphoneCore *lc=linphone_chat_room_get_core(msg->chat_room);
	if (lc->db){
		char *buf=sqlite3_mprintf("UPDATE history SET appdata=%Q WHERE id=%u;",
								  msg->appdata,msg->storage_id);
		linphone_sql_request(lc->db,buf);
		sqlite3_free(buf);
	}
}

void linphone_chat_room_mark_as_read(LinphoneChatRoom *cr) {
	linphone_chat_room_get_cpp_obj(cr).markAsRead();
}

int linphone_chat_room_get_unread_messages_count(LinphoneChatRoom *cr) {
	return linphone_chat_room_get_cpp_obj(cr).getUnreadMessagesCount();
}

int linphone_chat_room_get_history_size(LinphoneChatRoom *cr) {
	return linphone_chat_room_get_cpp_obj(cr).getHistorySize();
}

void linphone_chat_room_delete_message(LinphoneChatRoom *cr, LinphoneChatMessage *msg) {
	linphone_chat_room_get_cpp_obj(cr).deleteMessage(msg);
}

void linphone_chat_room_delete_history(LinphoneChatRoom *cr) {
	linphone_chat_room_get_cpp_obj(cr).deleteHistory();
}

bctbx_list_t *linphone_chat_room_get_history_range(LinphoneChatRoom *cr, int startm, int endm) {
	std::list<LinphoneChatMessage *> l = linphone_chat_room_get_cpp_obj(cr).getHistoryRange(startm, endm);
	bctbx_list_t *result = nullptr;
	for (auto it = l.begin(); it != l.end(); it++)
		result = bctbx_list_append(result, *it);
	return result;
}

bctbx_list_t *linphone_chat_room_get_history(LinphoneChatRoom *cr, int nb_message) {
	std::list<LinphoneChatMessage *> l = linphone_chat_room_get_cpp_obj(cr).getHistory(nb_message);
	bctbx_list_t *result = nullptr;
	for (auto it = l.begin(); it != l.end(); it++)
		result = bctbx_list_append(result, *it);
	return result;
}

LinphoneChatMessage * linphone_chat_room_find_message(LinphoneChatRoom *cr, const char *message_id) {
	return linphone_chat_room_get_cpp_obj(cr).findMessage(message_id);
}

static void linphone_create_history_table(sqlite3* db){
	char* errmsg=NULL;
	int ret;
	ret=sqlite3_exec(db,"CREATE TABLE IF NOT EXISTS history ("
							 "id            INTEGER PRIMARY KEY AUTOINCREMENT,"
							 "localContact  TEXT NOT NULL,"
							 "remoteContact TEXT NOT NULL,"
							 "direction     INTEGER,"
							 "message       TEXT,"
							 "time          TEXT NOT NULL,"
							 "read          INTEGER,"
							 "status        INTEGER"
						");",
			0,0,&errmsg);
	if(ret != SQLITE_OK) {
		ms_error("Error in creation: %s.\n", errmsg);
		sqlite3_free(errmsg);
	}
}


static const char *days[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
static const char *months[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
static time_t parse_time_from_db( const char* time ){
	/* messages used to be stored in the DB by using string-based time */
	struct tm ret={0};
	char tmp1[80]={0};
	char tmp2[80]={0};
	int i,j;
	time_t parsed = 0;

	if( sscanf(time,"%3c %3c%d%d:%d:%d %d",tmp1,tmp2,&ret.tm_mday,
			   &ret.tm_hour,&ret.tm_min,&ret.tm_sec,&ret.tm_year) == 7 ){
		ret.tm_year-=1900;
		for(i=0;i<7;i++) {
			if(strcmp(tmp1,days[i])==0) ret.tm_wday=i;
		}
		for(j=0;j<12;j++) {
			if(strcmp(tmp2,months[j])==0) ret.tm_mon=j;
		}
		ret.tm_isdst=-1;
		parsed = mktime(&ret);
	}
	return parsed;
}


static int migrate_messages_timestamp(void* data,int argc, char** argv, char** column_names) {
	time_t new_time = parse_time_from_db(argv[1]);
	if( new_time ){
		/* replace 'time' by -1 and set 'utc' to the timestamp */
		char *buf =	sqlite3_mprintf("UPDATE history SET utc=%lld,time='-1' WHERE id=%i;", (int64_t)new_time, atoi(argv[0]));
		if( buf) {
			linphone_sql_request((sqlite3*)data, buf);
			sqlite3_free(buf);
		}
	} else {
		ms_warning("Cannot parse time %s from id %s", argv[1], argv[0]);
	}
	return 0;
}

static void linphone_migrate_timestamps(sqlite3* db){
	int ret;
	char* errmsg = NULL;
	uint64_t begin=ortp_get_cur_time_ms();

	linphone_sql_request(db,"BEGIN TRANSACTION");

	ret = sqlite3_exec(db,"SELECT id,time,direction FROM history WHERE time != '-1';", migrate_messages_timestamp, db, &errmsg);
	if( ret != SQLITE_OK ){
		ms_warning("Error migrating outgoing messages: %s.\n", errmsg);
		sqlite3_free(errmsg);
		linphone_sql_request(db, "ROLLBACK");
	} else {
		uint64_t end;
		linphone_sql_request(db, "COMMIT");
		end=ortp_get_cur_time_ms();
		ms_message("Migrated message timestamps to UTC in %lu ms",(unsigned long)(end-begin));
	}
}

static void linphone_update_history_table(sqlite3* db) {
	char* errmsg=NULL;
	char *buf;
	int ret;

	// for image url storage
	ret=sqlite3_exec(db,"ALTER TABLE history ADD COLUMN url TEXT;",NULL,NULL,&errmsg);
	if(ret != SQLITE_OK) {
		ms_message("Table already up to date: %s.", errmsg);
		sqlite3_free(errmsg);
	} else {
		ms_debug("Table history updated successfully for URL.");
	}

	// for UTC timestamp storage
	ret = sqlite3_exec(db, "ALTER TABLE history ADD COLUMN utc INTEGER;", NULL,NULL,&errmsg);
	if( ret != SQLITE_OK ){
		ms_message("Table already up to date: %s.", errmsg);
		sqlite3_free(errmsg);
	} else {
		ms_debug("Table history updated successfully for UTC.");
		// migrate from old text-based timestamps to unix time-based timestamps
		linphone_migrate_timestamps(db);
	}

	// new field for app-specific storage
	ret=sqlite3_exec(db,"ALTER TABLE history ADD COLUMN appdata TEXT;",NULL,NULL,&errmsg);
	if(ret != SQLITE_OK) {
		ms_message("Table already up to date: %s.", errmsg);
		sqlite3_free(errmsg);
	} else {
		ms_debug("Table history updated successfully for app-specific data.");
	}

	// new field for linphone content storage
	ret=sqlite3_exec(db,"ALTER TABLE history ADD COLUMN content INTEGER;",NULL,NULL,&errmsg);
	if(ret != SQLITE_OK) {
		ms_message("Table already up to date: %s.", errmsg);
		sqlite3_free(errmsg);
	} else {
		ms_debug("Table history updated successfully for content data.");
		ret = sqlite3_exec(db,"CREATE TABLE IF NOT EXISTS content ("
							"id INTEGER PRIMARY KEY AUTOINCREMENT,"
							"type TEXT,"
							"subtype TEXT,"
							"name TEXT,"
							"encoding TEXT,"
							"size INTEGER,"
							"data BLOB"
						");",
			0,0,&errmsg);
		if(ret != SQLITE_OK) {
			ms_error("Error in creation: %s.\n", errmsg);
			sqlite3_free(errmsg);
		} else {
			ms_debug("Table content successfully created.");
		}
	}

	// new fields for content key storage when using lime
	ret=sqlite3_exec(db,"ALTER TABLE content ADD COLUMN key_size INTEGER;",NULL,NULL,&errmsg);
	if(ret != SQLITE_OK) {
		ms_message("Table already up to date: %s.", errmsg);
		sqlite3_free(errmsg);
	} else {
		ret=sqlite3_exec(db,"ALTER TABLE content ADD COLUMN key TEXT;",NULL,NULL,&errmsg);
		if(ret != SQLITE_OK) {
			ms_message("Table already up to date: %s.", errmsg);
			sqlite3_free(errmsg);
		} else {
			ms_debug("Table history content successfully for lime key storage data.");
		}
	}

	// new field for message_id
	ret = sqlite3_exec(db, "ALTER TABLE history ADD COLUMN messageId TEXT;", NULL, NULL, &errmsg);
	if (ret != SQLITE_OK) {
		ms_message("Table already up to date: %s", errmsg);
		sqlite3_free(errmsg);
	} else {
		ms_message("Table history updated successfully for messageId data.");
	}

	// Convert is_read to LinphoneChatMessageStateDisplayed
	buf = sqlite3_mprintf("UPDATE history SET status=%i WHERE read=1 AND direction=%i;", LinphoneChatMessageStateDisplayed, LinphoneChatMessageIncoming);
	linphone_sql_request(db, buf);
	sqlite3_free(buf);

	/* New field for content type */
	ret = sqlite3_exec(db, "ALTER TABLE history ADD COLUMN content_type TEXT;", NULL, NULL, &errmsg);
	if (ret != SQLITE_OK) {
		ms_message("Table already up to date: %s", errmsg);
		sqlite3_free(errmsg);
	} else {
		ms_message("Table history updated successfully for content_type data.");
	}

	// new field for secured flag
	ret = sqlite3_exec(db, "ALTER TABLE history ADD COLUMN is_secured INTEGER DEFAULT 0;", NULL, NULL, &errmsg);
	if (ret != SQLITE_OK) {
		ms_message("Table already up to date: %s", errmsg);
		sqlite3_free(errmsg);
	} else {
		ms_message("Table history updated successfully for is_secured data.");
	}
}

static void linphone_fix_outgoing_messages_state(sqlite3* db) {
	/* Convert Idle and InProgress states of outgoing messages to NotDelivered */
	char *buf = sqlite3_mprintf("UPDATE history SET status=%i WHERE direction=%i AND (status=%i OR status=%i);",
		LinphoneChatMessageStateNotDelivered, LinphoneChatMessageOutgoing, LinphoneChatMessageStateIdle, LinphoneChatMessageStateInProgress);
	linphone_sql_request(db, buf);
	sqlite3_free(buf);
}

void linphone_message_storage_init_chat_rooms(LinphoneCore *lc) {
	char *buf;

	if (lc->db==NULL) return;
	buf=sqlite3_mprintf("SELECT remoteContact FROM history GROUP BY remoteContact;");
	linphone_sql_request_all(lc->db,buf,lc);
	sqlite3_free(buf);
}

static void _linphone_message_storage_profile(void*data,const char*statement, sqlite3_uint64 duration){
	ms_warning("SQL statement '%s' took %llu microseconds", statement, (unsigned long long)(duration / 1000LL) );
}

static void linphone_message_storage_activate_debug(sqlite3* db, bool_t debug){
	if( debug  ){
		sqlite3_profile(db, _linphone_message_storage_profile, NULL );
	} else {
		sqlite3_profile(db, NULL, NULL );
	}
}

void linphone_core_message_storage_set_debug(LinphoneCore *lc, bool_t debug){

	lc->debug_storage = debug;

	if( lc->db ){
		linphone_message_storage_activate_debug(lc->db, debug);
	}
}

void linphone_core_message_storage_init(LinphoneCore *lc){
	int ret;
	const char *errmsg;
	sqlite3 *db = NULL;

	linphone_core_message_storage_close(lc);

	ret=_linphone_sqlite3_open(lc->chat_db_file,&db);
	if(ret != SQLITE_OK) {
		errmsg=sqlite3_errmsg(db);
		ms_error("Error in the opening: %s.\n", errmsg);
		sqlite3_close(db);
		return;
	}

	linphone_message_storage_activate_debug(db, lc->debug_storage);

	linphone_create_history_table(db);
	linphone_update_history_table(db);
	linphone_fix_outgoing_messages_state(db);
	lc->db=db;

	// Create a chatroom for each contact in the chat history
	linphone_message_storage_init_chat_rooms(lc);
}

void linphone_core_message_storage_close(LinphoneCore *lc){
	if (lc->db){
		sqlite3_close(lc->db);
		lc->db=NULL;
	}
}

#else

unsigned int linphone_chat_message_store(LinphoneChatMessage *cr){
	return 0;
}

void linphone_chat_message_store_state(LinphoneChatMessage *cr){
}

void linphone_chat_message_store_appdata(LinphoneChatMessage *msg){
}

void linphone_chat_room_mark_as_read(LinphoneChatRoom *cr){
}

bctbx_list_t *linphone_chat_room_get_history(LinphoneChatRoom *cr,int nb_message){
	return NULL;
}

bctbx_list_t *linphone_chat_room_get_history_range(LinphoneChatRoom *cr, int begin, int end){
	return NULL;
}

LinphoneChatMessage * linphone_chat_room_find_message(LinphoneChatRoom *cr, const char *message_id) {
	return NULL;
}

void linphone_chat_room_delete_message(LinphoneChatRoom *cr, LinphoneChatMessage *msg) {
}

void linphone_chat_room_delete_history(LinphoneChatRoom *cr){
}

void linphone_message_storage_init_chat_rooms(LinphoneCore *lc) {
}

void linphone_core_message_storage_init(LinphoneCore *lc){
}

void linphone_core_message_storage_close(LinphoneCore *lc){
}

int linphone_chat_room_get_unread_messages_count(LinphoneChatRoom *cr){
	return 0;
}

int linphone_chat_room_get_history_size(LinphoneChatRoom *cr){
	return 0;
}

void linphone_chat_message_store_update(LinphoneChatMessage *msg){
}

#endif

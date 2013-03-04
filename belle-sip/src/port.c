/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2010  Belledonne Communications SARL

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "belle_sip_internal.h"

#ifdef WIN32

#include <process.h>
#include <time.h>

static int sockets_initd=0;

int belle_sip_init_sockets(void){
	if (sockets_initd==0){
		WSADATA data;
		int err = WSAStartup(MAKEWORD(2,2), &data);
		if (err != 0) {
			belle_sip_error("WSAStartup failed with error: %d\n", err);
			return -1;
		}
	}
	sockets_initd++;
	return 0;
}

void belle_sip_uninit_sockets(void){
	sockets_initd--;
	if (sockets_initd==0) WSACleanup();
}

int belle_sip_socket_set_nonblocking (belle_sip_socket_t sock)
{
	unsigned long nonBlock = 1;
	return ioctlsocket(sock, FIONBIO , &nonBlock);
}

const char *belle_sip_get_socket_error_string(){
	return belle_sip_get_socket_error_string_from_code(WSAGetLastError());
}

const char *belle_sip_get_socket_error_string_from_code(int code){
	static TCHAR msgBuf[256];
	FormatMessage(
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			code,
			0, // Default language
			(LPTSTR) &msgBuf,
			sizeof(msgBuf),
			NULL);
	/*FIXME: should convert from TCHAR to UTF8 */
	return (const char *)msgBuf;
}


int belle_sip_thread_key_create(belle_sip_thread_key_t *key, void (*destructor)(void*) ){
	*key=TlsAlloc();
	if (*key==TLS_OUT_OF_INDEXES){
		belle_sip_error("TlsAlloc(): TLS_OUT_OF_INDEXES.");
		return -1;
	}
	return 0;
}

int belle_sip_thread_setspecific(belle_sip_thread_key_t key,const void *value){
	return TlsSetValue(key,(void*)value) ? 0 : -1;
}

const void* belle_sip_thread_getspecific(belle_sip_thread_key_t key){
	return TlsGetValue(key);
}

int belle_sip_thread_key_delete(belle_sip_thread_key_t key){
	return TlsFree(key) ? 0 : -1;
}

#ifdef WINAPI_FAMILY_PHONE_APP
void belle_sip_sleep(unsigned int ms) {
	HANDLE sleepEvent = CreateEventEx(NULL, NULL, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS);
	if (!sleepEvent)
		return;
	WaitForSingleObjectEx(sleepEvent, ms, FALSE);
}
#endif

#else

int belle_sip_init_sockets(){
	return 0;
}

void belle_sip_uninit_sockets(){
}

int belle_sip_socket_set_nonblocking(belle_sip_socket_t sock){
	return fcntl (sock, F_SETFL, fcntl(sock,F_GETFL) | O_NONBLOCK);
}

#endif






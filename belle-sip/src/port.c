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

#ifdef HAVE_COMPILER_TLS
static __declspec(thread) const void *current_thread_data = NULL;
#endif

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

int belle_sip_socket_set_dscp(belle_sip_socket_t sock, int ai_family, int dscp){
	belle_sip_warning("belle_sip_socket_set_dscp(): not implemented.");
	return -1;
}

const char *belle_sip_get_socket_error_string(){
	return belle_sip_get_socket_error_string_from_code(WSAGetLastError());
}

const char *belle_sip_get_socket_error_string_from_code(int code){
	static TCHAR msgBuf[256];
#ifdef _UNICODE
	static WCHAR wMsgBuf[256];
	int ret;
	FormatMessageW(
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			code,
			0, // Default language
			(LPWSTR) &wMsgBuf,
			sizeof(wMsgBuf),
			NULL);
	ret = wcstombs(msgBuf, wMsgBuf, sizeof(msgBuf));
	if (ret == sizeof(msgBuf)) msgBuf[sizeof(msgBuf) - 1] = '\0';
#else
	FormatMessageA(
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			code,
			0, // Default language
			(LPTSTR) &msgBuf,
			sizeof(msgBuf),
			NULL);
	/*FIXME: should convert from TCHAR to UTF8 */
#endif
	return (const char *)msgBuf;
}


int belle_sip_thread_key_create(belle_sip_thread_key_t *key, void (*destructor)(void*) ){
#ifdef HAVE_COMPILER_TLS
	*key = (belle_sip_thread_key_t)&current_thread_data;
#else
	*key=TlsAlloc();
	if (*key==TLS_OUT_OF_INDEXES){
		belle_sip_error("TlsAlloc(): TLS_OUT_OF_INDEXES.");
		return -1;
	}
#endif
	return 0;
}

int belle_sip_thread_setspecific(belle_sip_thread_key_t key,const void *value){
#ifdef HAVE_COMPILER_TLS
	current_thread_data = value;
	return 0;
#else
	return TlsSetValue(key,(void*)value) ? 0 : -1;
#endif
}

const void* belle_sip_thread_getspecific(belle_sip_thread_key_t key){
#ifdef HAVE_COMPILER_TLS
	return current_thread_data;
#else
	return TlsGetValue(key);
#endif
}

int belle_sip_thread_key_delete(belle_sip_thread_key_t key){
#ifdef HAVE_COMPILER_TLS
	current_thread_data = NULL;
	return 0;
#else
	return TlsFree(key) ? 0 : -1;
#endif
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

#include <signal.h>

int belle_sip_init_sockets(){
	signal(SIGPIPE,SIG_IGN);
	return 0;
}

void belle_sip_uninit_sockets(){
}

int belle_sip_socket_set_nonblocking(belle_sip_socket_t sock){
	return fcntl (sock, F_SETFL, fcntl(sock,F_GETFL) | O_NONBLOCK);
}

int belle_sip_socket_set_dscp(belle_sip_socket_t sock, int ai_family, int dscp){
	int tos;
	int proto;
	int value_type;
	int retval;
	
	tos = (dscp << 2) & 0xFC;
	switch (ai_family) {
		case AF_INET:
			proto=IPPROTO_IP;
			value_type=IP_TOS;
		break;
		case AF_INET6:
			proto=IPPROTO_IPV6;
#ifdef IPV6_TCLASS /*seems not defined by my libc*/
			value_type=IPV6_TCLASS;
#else
			value_type=IP_TOS;
#endif
		break;
		default:
			belle_sip_error("Cannot set DSCP because socket family is unspecified.");
			return -1;
	}
	retval = setsockopt(sock, proto, value_type, (const char*)&tos, sizeof(tos));
	if (retval==-1)
		belle_sip_error("Fail to set DSCP value on socket: %s",belle_sip_get_socket_error_string());
	return retval;
}

#endif






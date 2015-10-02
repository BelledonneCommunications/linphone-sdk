/*
	belle-sip - SIP (RFC3261) library.
    Copyright (C) 2010  Belledonne Communications SARL

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "belle_sip_internal.h"

#ifdef _WIN32

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

typedef struct thread_param {
	void * (*func)(void *);
	void * arg;
} thread_param_t;

static unsigned WINAPI thread_starter(void *data) {
	thread_param_t *params = (thread_param_t*)data;
	params->func(params->arg);
	belle_sip_free(data);
	return 0;
}

int belle_sip_thread_create(belle_sip_thread_t *thread, void *attr, void * (*func)(void *), void *data) {
	thread_param_t *params = belle_sip_new(thread_param_t);
	params->func = func;
	params->arg = data;
	*thread = (HANDLE)_beginthreadex(NULL, 0, thread_starter, params, 0, NULL);
	return 0;
}

int belle_sip_thread_join(belle_sip_thread_t thread, void **unused) {
	if (thread != NULL) {
		WaitForSingleObjectEx(thread, INFINITE, FALSE);
		CloseHandle(thread);
	}
	return 0;
}

int belle_sip_mutex_init(belle_sip_mutex_t *mutex, void *attr) {
#ifdef BELLE_SIP_WINDOWS_DESKTOP
	*mutex = CreateMutex(NULL, FALSE, NULL);
#else
	InitializeSRWLock(mutex);
#endif
	return 0;
}

int belle_sip_mutex_lock(belle_sip_mutex_t * hMutex) {
#ifdef BELLE_SIP_WINDOWS_DESKTOP
	WaitForSingleObject(*hMutex, INFINITE);
#else
	AcquireSRWLockExclusive(hMutex);
#endif
	return 0;
}

int belle_sip_mutex_unlock(belle_sip_mutex_t * hMutex) {
#ifdef BELLE_SIP_WINDOWS_DESKTOP
	ReleaseMutex(*hMutex);
#else
	ReleaseSRWLockExclusive(hMutex);
#endif
	return 0;
}

int belle_sip_mutex_destroy(belle_sip_mutex_t * hMutex) {
#ifdef BELLE_SIP_WINDOWS_DESKTOP
	CloseHandle(*hMutex);
#endif
	return 0;
}

int belle_sip_socket_set_nonblocking(belle_sip_socket_t sock)
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
	static CHAR msgBuf[256];
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
	*key = (belle_sip_thread_key_t)TlsAlloc();
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
	return TlsSetValue((DWORD)key,(void*)value) ? 0 : -1;
#endif
}

const void* belle_sip_thread_getspecific(belle_sip_thread_key_t key){
#ifdef HAVE_COMPILER_TLS
	return current_thread_data;
#else
	return TlsGetValue((DWORD)key);
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

#ifndef BELLE_SIP_WINDOWS_DESKTOP
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


int belle_sip_socket_enable_dual_stack(belle_sip_socket_t sock){
	int value=0;
	int err=setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&value, sizeof(value));
	if (err==-1){
		belle_sip_warning("belle_sip_socket_enable_dual_stack: setsockopt(IPV6_ONLY) failed: %s",belle_sip_get_socket_error_string());
	}
	return err;
}

#if defined(ANDROID) || defined(_WIN32)

/*
 * SHAME !!! bionic's getaddrinfo does not implement the AI_V4MAPPED flag !
 * It is declared in header file but rejected by the implementation.
 * The code below is to emulate a _compliant_ getaddrinfo for android.
**/

/**
 * SHAME AGAIN !!! Win32's implementation of getaddrinfo is bogus !
 * it is not able to return an IPv6 addrinfo from an IPv4 address when AI_V4MAPPED is set !
**/

struct addrinfo *_belle_sip_alloc_addrinfo(int ai_family, int socktype, int proto){
	struct addrinfo *ai=(struct addrinfo*)belle_sip_malloc0(sizeof(struct addrinfo));
	ai->ai_family=ai_family;
	ai->ai_socktype=socktype;
	ai->ai_protocol=proto;
	ai->ai_addrlen=AF_INET6 ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);
	ai->ai_addr=(struct sockaddr *) belle_sip_malloc0(ai->ai_addrlen);
	return ai;
}

struct addrinfo *convert_to_v4mapped(const struct addrinfo *ai){
	struct addrinfo *res=NULL;
	const struct addrinfo *it;
	struct addrinfo *v4m=NULL;
	struct addrinfo *last=NULL;
	
	for (it=ai;it!=NULL;it=it->ai_next){
		struct sockaddr_in6 *sin6;
		struct sockaddr_in *sin;
		v4m=_belle_sip_alloc_addrinfo(AF_INET6, it->ai_socktype, it->ai_protocol);
		v4m->ai_flags|=AI_V4MAPPED;
		sin6=(struct sockaddr_in6*)v4m->ai_addr;
		sin=(struct sockaddr_in*)it->ai_addr;
		sin6->sin6_family=AF_INET6;
		((uint8_t*)&sin6->sin6_addr)[10]=0xff;
		((uint8_t*)&sin6->sin6_addr)[11]=0xff;
		memcpy(((uint8_t*)&sin6->sin6_addr)+12,&sin->sin_addr,4);
		sin6->sin6_port=sin->sin_port;
		if (last){
			last->ai_next=v4m;
		}else{
			res=v4m;
		}
		last=v4m;
	}
	return res;
}

struct addrinfo *addrinfo_concat(struct addrinfo *a1, struct addrinfo *a2){
	struct addrinfo *it;
	struct addrinfo *last=NULL;
	for (it=a1;it!=NULL;it=it->ai_next){
		last=it;
	}
	if (last){
		last->ai_next=a2;
		return a1;
	}else
		return a2;
}

int belle_sip_getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res){
	if (hints && hints->ai_family!=AF_INET && hints->ai_flags & AI_V4MAPPED){
		struct addrinfo *res6=NULL;
		struct addrinfo *res4=NULL;
		struct addrinfo lhints={0};
		int err;
		
		if (hints) memcpy(&lhints,hints,sizeof(lhints));
		
		lhints.ai_flags &= ~(AI_ALL | AI_V4MAPPED); /*remove the unsupported flags*/
		if (hints->ai_flags & AI_ALL){
			lhints.ai_family=AF_INET6;
			err=getaddrinfo(node, service, &lhints, &res6);
		}
		lhints.ai_family=AF_INET;
		err=getaddrinfo(node, service, &lhints, &res4);
		if (err==0){
			struct addrinfo *v4m=convert_to_v4mapped(res4);
			freeaddrinfo(res4);
			res4=v4m;
		}
		*res=addrinfo_concat(res6,res4);
		if (*res) err=0;
		return err;
	}
	return getaddrinfo(node, service, hints, res);
}

void _belle_sip_freeaddrinfo(struct addrinfo *res){
	struct addrinfo *it,*next_it;
	for(it=res;it!=NULL;it=next_it){
		next_it=it->ai_next;
		belle_sip_free(it->ai_addr);
		belle_sip_free(it);
	}
}

void belle_sip_freeaddrinfo(struct addrinfo *res){
	struct addrinfo *it,*previt=NULL;
	struct addrinfo *allocated_by_belle_sip=NULL;
	for(it=res;it!=NULL;it=it->ai_next){
		if (it->ai_flags & AI_V4MAPPED){
			allocated_by_belle_sip=it;
			if (previt) previt->ai_next=NULL;
			break;
		}
		previt=it;
	}
	if (res!=allocated_by_belle_sip) freeaddrinfo(res);
	if (allocated_by_belle_sip) _belle_sip_freeaddrinfo(allocated_by_belle_sip);
}

#else

int belle_sip_getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res){
	return getaddrinfo(node, service, hints, res);
}

void belle_sip_freeaddrinfo(struct addrinfo *res){
	freeaddrinfo(res);
}

#endif

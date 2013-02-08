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

#ifndef belle_sip_port_h
#define belle_sip_port_h

#ifndef WIN32
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#else

#include <winsock2.h>
#include <ws2tcpip.h>

#ifdef _MSC_VER
typedef unsigned long long uint64_t;
typedef long long int64_t;
typedef unsigned char uint8_t;
#define strcasecmp(a,b) _stricmp(a,b)
#define snprintf _snprintf
#define strdup _strdup
#else
#include <stdint.h>
#endif

/*AI_NUMERICSERV is not defined for windows XP. Since it is not essential, we define it to 0 (does nothing)*/
#ifndef AI_NUMERICSERV
#define AI_NUMERICSERV 0
#endif

#endif

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) || defined(__WIN32__)
#ifdef BELLESIP_INTERNAL_EXPORTS
#define BELLESIP_INTERNAL_EXPORT __declspec(dllexport)
#else
#define BELLESIP_INTERNAL_EXPORT
#endif
#else
#define BELLESIP_INTERNAL_EXPORT extern
#endif

/*
 * Socket abstraction layer
 */

BELLESIP_INTERNAL_EXPORT int belle_sip_init_sockets(void);
BELLESIP_INTERNAL_EXPORT void belle_sip_uninit_sockets(void);
int belle_sip_socket_set_nonblocking (belle_sip_socket_t sock);
 
#if defined(WIN32)

static inline void close_socket(belle_sip_socket_t s){
	closesocket(s);
}

static inline int get_socket_error(void){
	return WSAGetLastError();
}

const char *belle_sip_get_socket_error_string();
const char *belle_sip_get_socket_error_string_from_code(int code);

#define usleep(us) Sleep((us)/1000)
static inline int inet_aton(const char *ip, struct in_addr *p){
	*(long*)p=inet_addr(ip);
	return 0;
}

#define BELLESIP_EWOULDBLOCK WSAEWOULDBLOCK
#define BELLESIP_EINPROGRESS WSAEINPROGRESS

#else

static inline void close_socket(belle_sip_socket_t s){
	close(s);
}

static inline int get_socket_error(void){
	return errno;
}
#define belle_sip_get_socket_error_string() strerror(errno)
#define belle_sip_get_socket_error_string_from_code(code) strerror(code)

#define BELLESIP_EWOULDBLOCK EWOULDBLOCK
#define BELLESIP_EINPROGRESS EINPROGRESS

#endif



#endif


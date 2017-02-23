/*
vconnect.h
Copyright (C) 2017 Belledonne Communications SARL

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

#ifndef BCTBX_VCONNECT
#define BCTBX_VCONNECT

#include <fcntl.h>

#include <bctoolbox/port.h>


#if !defined(_WIN32_WCE)
#include <sys/types.h>
#include <sys/stat.h>
#if _MSC_VER
#include <io.h>
#endif
#endif /*_WIN32_WCE*/


#ifndef _WIN32
#include <unistd.h>
#endif

#if !defined(_WIN32) && !defined(_WIN32_WCE)
#define getSocketError() strerror(errno)
#else
#define getSocketError()       __bctbx_getWinSocketError(WSAGetLastError())
#endif

#define BCTBX_VCONNECT_OK         0   /* Successful result */

#define BCTBX_VCONNECT_ERROR       -255   /* Some kind of socket error occurred */


#ifdef __cplusplus
extern "C"{
#endif


/**
 * Methods associated with the bctbx_vsocket_api_t.
 */
typedef struct bctbx_vsocket_methods_t bctbx_vsocket_methods_t;

/**
 * 
 */
struct bctbx_vsocket_methods_t {

	int (*pFuncSocket)(int socket_family, int socket_type, int protocol);
	int (*pFuncConnect)(bctbx_socket_t sock, const struct sockaddr *address, socklen_t address_len);
	int (*pFuncBind)(bctbx_socket_t sock, const struct sockaddr *address, socklen_t address_len);

	int (*pFuncGetSockName)(bctbx_socket_t sockfd, struct sockaddr *addr, socklen_t *addrlen);
	int (*pFuncGetSockOpt)(bctbx_socket_t sockfd, int level, int optname, 
							void *optval, socklen_t *optlen);
	int (*pFuncSetSockOpt)(bctbx_socket_t sockfd, int level, int optname, 
							const void *optval, socklen_t optlen);
	int (*pFuncClose)(bctbx_socket_t sock);
	char* (*pFuncGetError)(int err);
	int (*pFuncShutdown)(bctbx_socket_t sock, int how);



};


/**
 * vsocket definition
 */
typedef struct bctbx_vsocket_api_t bctbx_vsocket_api_t;
struct bctbx_vsocket_api_t {
	const char *vSockName;       /* Virtual file system name */
	const bctbx_vsocket_methods_t *pSocketMethods; 
};


/* API to use the vsocket */
/*
 * This function returns a pointer to the vsocket implemented in this file.
 */
BCTBX_PUBLIC bctbx_vsocket_api_t *bc_create_vsocket_api(void);

/**
 * [bctbx_socket description]
 * @param  socket_family [description]
 * @param  socket_type   [description]
 * @param  protocol      [description]
 * @return               [description]
 */
BCTBX_PUBLIC int bctbx_socket(int socket_family, int socket_type, int protocol);

/**
 * [bctbx_getsockname description]
 * @param  sockfd  [description]
 * @param  addr    [description]
 * @param  addrlen [description]
 * @return         [description]
 */
BCTBX_PUBLIC int bctbx_getsockname(bctbx_socket_t sockfd, struct sockaddr *addr, socklen_t *addrlen);
/**
 * [bctbx_getsockopt description]
 * @param  sockfd  [description]
 * @param  level   [description]
 * @param  optname [description]
 * @param  optval  [description]
 * @param  optlen  [description]
 * @return         [description]
 */
BCTBX_PUBLIC int bctbx_getsockopt(bctbx_socket_t sockfd, int level, int optname, 
							void *optval, socklen_t *optlen);
/**
 * [bctbx_setsockopt description]
 * @param  sockfd  [description]
 * @param  level   [description]
 * @param  optname [description]
 * @param  optval  [description]
 * @param  optlen  [description]
 * @return         [description]
 */
BCTBX_PUBLIC int bctbx_setsockopt(bctbx_socket_t sockfd, int level, int optname, 
							const void *optval, socklen_t optlen);

/**
 * [bctbx_shutdown description]
 * @param  sockfd [description]
 * @param  how    [description]
 * @return        [description]
 */
BCTBX_PUBLIC int bctbx_shutdown(bctbx_socket_t sockfd, int how);

/**
 * [bctbx_socket_close description]
 * @param  sockfd [description]
 * @return        [description]
 */
BCTBX_PUBLIC int bctbx_socket_close(bctbx_socket_t sockfd);

/**
 * [bctbx_bind description]
 * @param  sockfd      [description]
 * @param  address     [description]
 * @param  address_len [description]
 * @return             [description]
 */
BCTBX_PUBLIC int bctbx_bind(bctbx_socket_t sockfd, const struct sockaddr *address, socklen_t address_len);

/**
 * [bctbx_connect description]
 * @param  sockfd      [description]
 * @param  address     [description]
 * @param  address_len [description]
 * @return             [description]
 */
BCTBX_PUBLIC int bctbx_connect(bctbx_socket_t sockfd, const struct sockaddr *address, socklen_t address_len);


/**
 * Set default vsocket pointer pDefault to my_vsocket_api.
 * By default, the global pointer is set to use vsocket implemented in vconnect.c 
 * @param my_vsocket_api Pointer to a bctbx_vsocket_api_t structure. 
 */
BCTBX_PUBLIC void bctbx_vsocket_api_set_default(bctbx_vsocket_api_t *my_vsocket_api);


/**
 * Returns the value of the global variable pDefault,
 * pointing to the default bctbx_vsocket_api_t used.
 * @return Pointer to bctbx_vsocket_api_t set to operate as default vsocket.
 */
BCTBX_PUBLIC bctbx_vsocket_api_t* bctbx_vsocket_api_get_default(void);
	
/**
 * Return pointer to standard vsocket impletentation.
 * @return  pointer to bcSocketAPI
 */
BCTBX_PUBLIC bctbx_vsocket_api_t* bctbx_vsocket_api_get_standard(void);


#ifdef __cplusplus
}
#endif


#endif /* BCTBX_VCONNECT */


/*
vfs.h
Copyright (C) 2016 Belledonne Communications SARL

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



#define BCTBX_VCONNECT_OK         0   /* Successful result */

#define BCTBX_VCONNECT_ERROR       -255   /* Some kind of disk I/O error occurred */


#ifdef __cplusplus
extern "C"{
#endif


/**
 * Methods associated with the bctbx_vsocket_t.
 */
typedef struct bctbx_vsocket_methods_t bctbx_vsocket_methods_t;
/**
 */
struct bctbx_vsocket_methods_t {

	int (*pFuncSocket)(int socket_family, int socket_type, int protocol);
	int (*pFuncConnect)(int sockfd, const struct sockaddr *address, socklen_t address_len);
	int (*pFuncBind)(int sockfd, const struct sockaddr *address, socklen_t address_len);

	int (*pFuncGetSockName)(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
	int (*pFuncGetSockOpt)(int sockfd, int level, int optname, 
							void *optval, socklen_t *optlen);
	int (*pFuncSetSockOpt)(int sockfd, int level, int optname, 
							const void *optval, socklen_t optlen);
	int (*pFuncClose)(int fd);
	int (*pFuncShutdown)(int sockfd, int how);
};


/**
 * vsocket definition
 */
typedef struct bctbx_vsocket_t bctbx_vsocket_t;
struct bctbx_vsocket_t {
	const char *vSockName;       /* Virtual file system name */
	const bctbx_vsocket_methods_t *pSocketMethods; 
};


/* API to use the vsocket */
/*
 * This function returns a pointer to the vsocket implemented in this file.
 */
BCTBX_PUBLIC bctbx_vsocket_t *bc_create_vsocket_api(void);


BCTBX_PUBLIC int bctbx_vsocket(int socket_family, int socket_type, int protocol);


BCTBX_PUBLIC int bctbx_vgetsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
BCTBX_PUBLIC int bctbx_vgetsockopt(int sockfd, int level, int optname, 
							void *optval, socklen_t *optlen);
BCTBX_PUBLIC int bctbx_vsetsockopt(int sockfd, int level, int optname, 
							const void *optval, socklen_t optlen);

BCTBX_PUBLIC int bctbx_vshutdown(int sockfd, int how);

/* UNUSED : REPLACED BY FUNIONS FROM PORT.C
bctbx_socket_close, bctbx_bind, bctbx_connect */
BCTBX_PUBLIC int bctbx_vclose(int fd);
BCTBX_PUBLIC int bctbx_vbind(int sockfd, const struct sockaddr *address, socklen_t address_len);
BCTBX_PUBLIC int bctbx_vconnect(int sockfd, const struct sockaddr *address, socklen_t address_len);





/**
 * Set default vsocket pointer pDefault to my_vsocket_api.
 * By default, the global pointer is set to use vsocket implemnted in vfs.c 
 * @param my_vsocket_api Pointer to a bctbx_vsocket_t structure. 
 */
BCTBX_PUBLIC void bctbx_vsocket_api_set_default(bctbx_vsocket_t *my_vsocket_api);


/**
 * Returns the value of the global variable pDefault,
 * pointing to the default vfs used.
 * @return Pointer to bctbx_vsocket_t set to operate as default vsocket.
 */
BCTBX_PUBLIC bctbx_vsocket_t* bctbx_vsocket_api_get_default(void);
	
/**
 * Return pointer to standard vsocket impletentation.
 * @return  pointer to bcVfs
 */
BCTBX_PUBLIC bctbx_vsocket_t* bctbx_vsocket_api_get_standard(void);


#ifdef __cplusplus
}
#endif


#endif /* BCTBX_VCONNECT */


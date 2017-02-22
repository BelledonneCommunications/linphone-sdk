/*
vfs.c
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

#include "bctoolbox/vconnect.h"
#include "bctoolbox/port.h"
#include "bctoolbox/logging.h"
#include <sys/types.h>
#include <stdarg.h>
#include <errno.h>



/* Pointer to default VFS initialized to standard VFS implemented here.*/

static const bctbx_vsocket_methods_t bcSocketAPI = {
	socket,
	bind,  	  	/* from bctoolbox/port.c */
	connect,	  	/* from bctoolbox/port.c */
	getsockname,
	getsockopt,
	setsockopt,
	close,  /* from bctoolbox/port.c */
	shutdown,
};




static bctbx_vsocket_t bcvSocket = {
	"bctbx_vsocket",               /* vSockName */
	&bcSocketAPI,			/*pSocketMethods */
};



/* Pointer to default socket methods initialized to standard libc implementation here.*/
static  bctbx_vsocket_t *pDefaultvSocket = &bcvSocket;


int bctbx_vsocket(int socket_family, int socket_type, int protocol){
	return bcSocketAPI.pFuncSocket( socket_family, socket_type, protocol);
}


int bctbx_vgetsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen){
	int ret;
	ret = bcSocketAPI.pFuncGetSockName(sockfd, addr, addrlen);
	if (ret == 0){
		return BCTBX_VCONNECT_OK;
	}
	return -errno;
}


int bctbx_vgetsockopt(int sockfd, int level, int optname, 
						void *optval, socklen_t* optlen){
	int ret;
	ret = bcSocketAPI.pFuncGetSockOpt(sockfd, level, optname, optval, optlen);
	if (ret == 0){
		return BCTBX_VCONNECT_OK;
	}
	return -errno;
}

int bctbx_vsetsockopt(int sockfd, int level, int optname, 
							const void *optval, socklen_t optlen){
	int ret;
	ret = bcSocketAPI.pFuncSetSockOpt(sockfd, level, optname, optval, optlen);
	if (ret == 0){
		return BCTBX_VCONNECT_OK;
	}
		return -errno;
}


int bctbx_vshutdown(int sockfd, int how){
	return bcSocketAPI.pFuncShutdown(sockfd, how);
}


void bctbx_vsocket_api_set_default(bctbx_vsocket_t *my_vsocket_api) {
	pDefaultvSocket = my_vsocket_api ;
}

bctbx_vsocket_t* bctbx_vsocket_api_get_default(void) {
	return pDefaultvSocket;	
}

bctbx_vsocket_t* bctbx_vsocket_api_get_standard(void) {
	return &bcvSocket;
}



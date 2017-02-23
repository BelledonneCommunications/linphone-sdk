/*
vconnect.c
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include "bctoolbox/vconnect.h"
#include "bctoolbox/port.h"
#include "bctoolbox/logging.h"
#include <sys/types.h>
#include <stdarg.h>
#include <errno.h>


static int _bind (bctbx_socket_t sock, const struct sockaddr *address, socklen_t address_len){
	#ifdef _WIN32
		return bind(sock, address, (int)address_len);
	#else 
		return bind(sock, address, address_len);
	#endif
}

static const bctbx_vsocket_methods_t bcSocketAPI = {
	socket,
	connect,  	/* from bctoolbox/port.c */
	_bind,
	getsockname,
	getsockopt,
	setsockopt,
	#if	!defined(_WIN32) && !defined(_WIN32_WCE) 	  /* from bctoolbox/port.c */
	close,
	#else
	closesocket,
	#endif
	shutdown,
};


static bctbx_vsocket_api_t bcvSocket = {
	"bctbx_vsocket",               /* vSockName */
	&bcSocketAPI,			/*pSocketMethods */
};



/* Pointer to default socket methods initialized to standard libc implementation here.*/
static  bctbx_vsocket_api_t *pDefaultvSocket = &bcvSocket;


int bctbx_vsocket(int socket_family, int socket_type, int protocol){
	return pDefaultvSocket->pSocketMethods->pFuncSocket( socket_family, socket_type, protocol);
}


int bctbx_vclose(bctbx_socket_t sock){
	return pDefaultvSocket->pSocketMethods->pFuncClose(sock);
}


int bctbx_vbind(bctbx_socket_t sock, const struct sockaddr *address, socklen_t address_len){
	return pDefaultvSocket->pSocketMethods->pFuncBind(sock, address, address_len);

}


int bctbx_vconnect(bctbx_socket_t sock, const struct sockaddr *address, socklen_t address_len){
	return pDefaultvSocket->pSocketMethods->pFuncConnect(sock, address, address_len);
}

int bctbx_vgetsockname(bctbx_socket_t sockfd, struct sockaddr *addr, socklen_t *addrlen){
	return pDefaultvSocket->pSocketMethods->pFuncGetSockName(sockfd, addr, addrlen);
}


int bctbx_vgetsockopt(bctbx_socket_t sockfd, int level, int optname, 
						void *optval, socklen_t* optlen){
	return pDefaultvSocket->pSocketMethods->pFuncGetSockOpt(sockfd, level, optname, optval, optlen);

}

int bctbx_vsetsockopt(bctbx_socket_t sockfd, int level, int optname, 
							const void *optval, socklen_t optlen){
	return pDefaultvSocket->pSocketMethods->pFuncSetSockOpt(sockfd, level, optname, optval, optlen);

}


int bctbx_vshutdown(bctbx_socket_t sock, int how){
	return pDefaultvSocket->pSocketMethods->pFuncShutdown(sock, how);
}


void bctbx_vsocket_api_set_default(bctbx_vsocket_api_t *my_vsocket_api) {

	if (my_vsocket_api != NULL){
		pDefaultvSocket = my_vsocket_api ;
	}

}

bctbx_vsocket_api_t* bctbx_vsocket_api_get_default(void) {
	return pDefaultvSocket;	
}

bctbx_vsocket_api_t* bctbx_vsocket_api_get_standard(void) {
	return &bcvSocket;
}

/* Pointer to default VFS initialized to standard VFS implemented here.*/


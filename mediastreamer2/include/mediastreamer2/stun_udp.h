 /*
  The oRTP library is an RTP (Realtime Transport Protocol - rfc3550) stack.
  Copyright (C) 2001  Simon MORLAT simon.morlat@linphone.org

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* ====================================================================
 * The Vovida Software License, Version 1.0  *  * Copyright (c) 2000 Vovida Networks, Inc.  All rights reserved. *  * Redistribution and use in source and binary forms, with or without * modification, are permitted provided that the following conditions * are met: *  * 1. Redistributions of source code must retain the above copyright *    notice, this list of conditions and the following disclaimer. *  * 2. Redistributions in binary form must reproduce the above copyright *    notice, this list of conditions and the following disclaimer in *    the documentation and/or other materials provided with the *    distribution. *  * 3. The names "VOCAL", "Vovida Open Communication Application Library", *    and "Vovida Open Communication Application Library (VOCAL)" must *    not be used to endorse or promote products derived from this *    software without prior written permission. For written *    permission, please contact vocal@vovida.org. * * 4. Products derived from this software may not be called "VOCAL", nor *    may "VOCAL" appear in their name, without prior written *    permission of Vovida Networks, Inc. *  * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL VOVIDA * NETWORKS, INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT DAMAGES * IN EXCESS OF $1,000, NOR FOR ANY INDIRECT, INCIDENTAL, SPECIAL, * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH * DAMAGE. *  * ==================================================================== *  * This software consists of voluntary contributions made by Vovida * Networks, Inc. and many individuals on behalf of Vovida Networks, * Inc.  For more information on Vovida Networks, Inc., please see * <http://www.vovida.org/>.
 *
 */

#ifndef __STUN_UDP_H__
#define __STUN_UDP_H__

#ifndef __cplusplus
//#define bool int
//#define false 0
//#define true 1
#endif

#ifdef __MACH__
#include <sys/socket.h>
#ifndef _SOCKLEN_T
typedef int socklen_t;
#endif
#endif

#include <mediastreamer2/mscommon.h>
#include <ortp/port.h>

#if defined(_WIN32) || defined(_WIN32_WCE)

#if !defined(_WIN32_WCE)
#include <errno.h>
#endif



#include <winsock2.h>
/* #include <io.h> */

typedef int socklen_t;
typedef SOCKET Socket;

#ifndef EWOULDBLOCK
#define EWOULDBLOCK             WSAEWOULDBLOCK
#endif
#ifndef EINPROGRESS
#define EINPROGRESS             WSAEINPROGRESS
#endif
#ifndef EALREADY
#define EALREADY                WSAEALREADY
#endif
#ifndef ENOTSOCK
#define ENOTSOCK                WSAENOTSOCK
#endif
#ifndef EDESTADDRREQ
#define EDESTADDRREQ            WSAEDESTADDRREQ
#endif
#ifndef EMSGSIZE
#define EMSGSIZE                WSAEMSGSIZE
#endif
#ifndef EPROTOTYPE
#define EPROTOTYPE              WSAEPROTOTYPE
#endif
#ifndef ENOPROTOOPT
#define ENOPROTOOPT             WSAENOPROTOOPT
#endif
#ifndef EPROTONOSUPPORT
#define EPROTONOSUPPORT         WSAEPROTONOSUPPORT
#endif
#ifndef ESOCKTNOSUPPORT
#define ESOCKTNOSUPPORT         WSAESOCKTNOSUPPORT
#endif
#ifndef EOPNOTSUPP
#define EOPNOTSUPP              WSAEOPNOTSUPP
#endif
#ifndef EPFNOSUPPORT
#define EPFNOSUPPORT            WSAEPFNOSUPPORT
#endif
#ifndef EAFNOSUPPORT
#define EAFNOSUPPORT            WSAEAFNOSUPPORT
#endif
#ifndef EADDRINUSE
#define EADDRINUSE              WSAEADDRINUSE
#endif
#ifndef EADDRNOTAVAIL
#define EADDRNOTAVAIL           WSAEADDRNOTAVAIL
#endif
#ifndef ENETDOWN
#define ENETDOWN                WSAENETDOWN
#endif
#ifndef ENETUNREACH
#define ENETUNREACH             WSAENETUNREACH
#endif
#ifndef ENETRESET
#define ENETRESET               WSAENETRESET
#endif
#ifndef ECONNABORTED
#define ECONNABORTED            WSAECONNABORTED
#endif
#ifndef ECONNRESET
#define ECONNRESET              WSAECONNRESET
#endif
#ifndef ENOBUFS
#define ENOBUFS                 WSAENOBUFS
#endif
#ifndef EISCONN
#define EISCONN                 WSAEISCONN
#endif
#ifndef ENOTCONN
#define ENOTCONN                WSAENOTCONN
#endif
#ifndef ESHUTDOWN
#define ESHUTDOWN               WSAESHUTDOWN
#endif
#ifndef ETOOMANYREFS
#define ETOOMANYREFS            WSAETOOMANYREFS
#endif
#ifndef ETIMEDOUT
#define ETIMEDOUT               WSAETIMEDOUT
#endif
#ifndef ECONNREFUSED
#define ECONNREFUSED            WSAECONNREFUSED
#endif
#ifndef ELOOP
#define ELOOP                   WSAELOOP
#endif
#ifndef EHOSTDOWN
#define EHOSTDOWN               WSAEHOSTDOWN
#endif
#ifndef EHOSTUNREACH
#define EHOSTUNREACH            WSAEHOSTUNREACH
#endif
#ifndef EPROCLIM
#define EPROCLIM                WSAEPROCLIM
#endif
#ifndef EUSERS
#define EUSERS                  WSAEUSERS
#endif
#ifndef EDQUOT
#define EDQUOT                  WSAEDQUOT
#endif
#ifndef ESTALE
#define ESTALE                  WSAESTALE
#endif
#ifndef EREMOTE
#define EREMOTE                 WSAEREMOTE
#endif
typedef LONGLONG Int64; 

#else

typedef int Socket;
#ifndef INVALID_SOCKET 
#define INVALID_SOCKET -1
#endif
#ifndef SOCKET_ERROR 
#define SOCKET_ERROR -1
#endif

#define closesocket(fd) close(fd)

#define WSANOTINITIALISED  EPROTONOSUPPORT

#endif

#ifdef __cplusplus
extern "C"{
#endif

MS2_PUBLIC int getErrno(void);

/* Open a UDP socket to receive on the given port - if port is 0, pick a a
   port, if interfaceIp!=0 then use ONLY the interface specified instead of
   all of them  */
MS2_PUBLIC Socket
openPort( unsigned short port, unsigned int interfaceIp);


/* recive a UDP message */
MS2_PUBLIC bool_t
getMessage( Socket fd, char* buf, int* len,
            unsigned int* srcIp, unsigned short* srcPort);


/* send a UDP message */
MS2_PUBLIC bool_t
sendMessage( Socket fd, char* msg, int len, 
             unsigned int dstIp, unsigned short dstPort);


/* set up network - does nothing in unix but needed for windows */
MS2_PUBLIC void
initNetwork(void);

#ifdef __cplusplus
}
#endif

#endif

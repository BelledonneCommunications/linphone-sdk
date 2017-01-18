/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006  Simon MORLAT (simon.morlat@linphone.org)

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/


#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msudp.h"
#include "mediastreamer2/msfileplayer.h"
#if defined(__cplusplus)
#define B64_NO_NAMESPACE
#endif
#include "ortp/b64.h"

struct SenderData {
	// Contains both destination ip and port
	struct addrinfo *dst_info;
	ortp_socket_t sockfd;
};

typedef struct SenderData SenderData;

static void sender_init(MSFilter * f)
{
	f->data = ms_new0(SenderData, 1);
}


static void sender_uninit(MSFilter * f)
{
	SenderData *d = (SenderData *) f->data;

	if (d->sockfd != (ortp_socket_t)-1) {
		close_socket(d->sockfd);
	}
	if (d->dst_info != NULL)  {
		freeaddrinfo(d->dst_info);
	}
	ms_free(d);
}

static void sender_process(MSFilter * f)
{
	SenderData *d = (SenderData *) f->data;
	mblk_t *im;

	ms_filter_lock(f);

	while ((im = ms_queue_get(f->inputs[0])) != NULL) {
		int error;
		msgpullup(im, -1);

		error = bctbx_sendto(
			d->sockfd,
			im->b_rptr,
			(int) (im->b_wptr - im->b_rptr),
			0,
			d->dst_info->ai_addr,
			(socklen_t)d->dst_info->ai_addrlen
		);

		if (error == -1) {
			ms_error("Failed to send UDP packet: errno=%d", errno);
		}
	}

	ms_filter_unlock(f);
}

static int sender_set_destination(MSFilter *f, void *arg) {
	SenderData *d = (SenderData *) f->data;
	const MSIPPort * destination = (const MSIPPort*)arg;

	int err;
	char port[10];
	struct addrinfo hints;
	int family = PF_INET;

	/* Try to get the address family of the host (PF_INET or PF_INET6). */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_flags = AI_NUMERICHOST;
	err = getaddrinfo(destination->ip, NULL, &hints, &d->dst_info);
	memset(&hints,0,sizeof(hints));
	hints.ai_socktype = SOCK_DGRAM;
	if (err == 0) {
		hints.ai_family = d->dst_info->ai_family;
		freeaddrinfo(d->dst_info);
	}

	snprintf(port,sizeof(port),"%i",destination->port);
	err=getaddrinfo(destination->ip,port,&hints,&d->dst_info);
	if (err!=0){
		ms_error("getaddrinfo() failed: %s\n",gai_strerror(err));
		return -1;
	}

	d->sockfd = socket(family,SOCK_DGRAM,0);
	if (d->sockfd==-1){
		ms_error("socket() failed: %d\n",errno);
		return -1;
	}

	return 0;
}

static MSFilterMethod sender_methods[] = {
	{MS_UDP_SEND_SET_DESTINATION, sender_set_destination},

	{0, NULL}
};

MSFilterDesc ms_udp_send_desc = {
	MS_UDP_SEND_ID,
	"MSUdpSend",
	"UDP output filter",
	MS_FILTER_OTHER,
	NULL,
	1,
	0,
	sender_init,
	NULL,
	sender_process,
	NULL,
	sender_uninit,
	sender_methods,
	MS_FILTER_IS_PUMP
};

MS_FILTER_DESC_EXPORT(ms_udp_send_desc)

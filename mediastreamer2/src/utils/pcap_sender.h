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

#ifndef PCAP_SENDER_H
#define PCAP_SENDER_H

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msudp.h"

struct _MSPCAPSender;

/**
 * User callback which will be called when PCAP sender has
 * reached end of file or has been stopped
 */
typedef void (*MSPCAPFileEnded)(struct _MSPCAPSender*,void * user_data);

typedef struct _MSPCAPSender {
	MSFilter *file_player;
	MSFilter *udp_send;
	MSTicker *ticker;
	MSPCAPFileEnded pcap_ended_cb;
	void* pcap_ended_user_data;
} MSPCAPSender;

/**
 * @brief Send a PCAP file over UDP network to a given destination.
 * @details The given PCAP file must contain RTP packets which
 * will not be modified when sent. Only UDP/IP headers will be
 * recreated to send them to the given destination. It is user
 * role to ensure that the PCAP file given is valid.
 * To generate a valid pcap file, you should record some real traffic
 * and then filter it using something like: "rtp && !rtpevent && udp.dstport = some_port"
 * @param factory the MSFactory
 * @param filepath Path to a valid PCAP
 * @param from_port only send packets with the given source port value in the PCAP file. Set to 0 to send all packets.
 * @param dest IP/port destination of the RTP packets
 * @param sample_rate The rate to which file must be read. It should be the payload rate
 * @param cb User callback which will be called when the file has been played entirely or when stop method was called.
 * @return the created PCAP sender or NULL if parameters are invalid
 */
MSPCAPSender* ms_pcap_sendto(MSFactory *factory, const char* filepath, unsigned from_port, const MSIPPort* dest,
							int sample_rate, uint32_t ts_offset, MSPCAPFileEnded cb, void* user_data);


#endif

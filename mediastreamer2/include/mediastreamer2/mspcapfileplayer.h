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
#ifndef mspcapfileplayer_h
#define mspcapfileplayer_h

#include <mediastreamer2/msfilter.h>

/**
 * This enum provides two differents outputs of the filter depending on your use case.
 * If you want to plug this filter directly to a decoder, you may want to skip RTP header
 * and directly get payload data using @MSPCAPFilePlayerLayerPayload. If you want to
 * send these packet over the network, you may want to keep the RTP header using
 * @MSPCAPFilePlayerLayerRTP.
**/
typedef enum _MSPCAPFilePlayerLayer {
	MSPCAPFilePlayerLayerRTP, /* skip IP, UDP, but keeps RTP header + underlying layers */
	MSPCAPFilePlayerLayerPayload, /* skip IP, UDP, RTP, but keeps RTP content */
} MSPCAPFilePlayerLayer;

/**
 * This enum provides two differents way of incrementing time depending on your use case.
 * If you want to play packets at the rate they were encoded, you should use @MSPCAPFilePlayerTimeRefRTP
 * which contains timestamps value written by the encoder.
 * Instead, if you want to replay a receiver-based PCAP stream as it was heard by the receiver,
 * you should use @MSPCAPFilePlayerTimeRefCapture.
 */
typedef enum _MSPCAPFilePlayerTimeRef {
	MSPCAPFilePlayerTimeRefRTP, /* use timestamps contained in RTP header to replay packets, written by the encoder */
	MSPCAPFilePlayerTimeRefCapture, /* use time of packet capture to replay them, specially useful in case of receiver-based capture */
} MSPCAPFilePlayerTimeRef;

/*methods*/
#define MS_PCAP_FILE_PLAYER_SET_LAYER		MS_FILTER_METHOD(MS_PCAP_FILE_PLAYER_ID,0,MSPCAPFilePlayerLayer)
#define MS_PCAP_FILE_PLAYER_SET_TIMEREF		MS_FILTER_METHOD(MS_PCAP_FILE_PLAYER_ID,1,MSPCAPFilePlayerTimeRef)
#define MS_PCAP_FILE_PLAYER_SET_TO_PORT		MS_FILTER_METHOD(MS_PCAP_FILE_PLAYER_ID,2,unsigned)
#define MS_PCAP_FILE_PLAYER_SET_TS_OFFSET	MS_FILTER_METHOD(MS_PCAP_FILE_PLAYER_ID,3,uint32_t)

#endif


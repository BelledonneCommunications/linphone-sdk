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

#include "pcap_sender.h"

#include "mediastreamer2/msudp.h"
#include "mediastreamer2/mspcapfileplayer.h"
#include "mediastreamer2/msfileplayer.h"

static void ms_pcap_stop(MSPCAPSender *s) {
	MSConnectionHelper h;
	// notify user callback
	if (s->pcap_ended_cb != NULL) {
		s->pcap_ended_cb(s, s->pcap_ended_user_data);
	}
	//then delete and free the graph
	ms_ticker_detach(s->ticker, s->file_player);
	ms_connection_helper_unlink(&h,s->file_player,-1,0);
	ms_connection_helper_unlink(&h,s->udp_send,0,-1);
	ms_filter_destroy(s->file_player);
	ms_filter_destroy(s->udp_send);
	ms_ticker_destroy(s->ticker);
	ms_free(s);
}

static void reader_notify_cb(void *user_data, MSFilter *f, unsigned int event, void *eventdata)
{
	if (event == MS_FILE_PLAYER_EOF) {
		ms_message("Reached end of file, stopping PCAP Sender");
		ms_pcap_stop((MSPCAPSender*)user_data);
	}
}

MSPCAPSender* ms_pcap_sendto(MSFactory *factory, const char* filepath, unsigned to_port, const MSIPPort *dest,
							int sample_rate, uint32_t ts_offset, MSPCAPFileEnded cb, void* user_data) {
	MSTickerParams params;
	MSConnectionHelper h;
	MSPCAPSender * s;
	MSFilter *udp_sender;
	MSFilter *file_player;
	int pcap_filter_property = MSPCAPFilePlayerLayerRTP;

	if (sample_rate < 0 || dest == NULL ||  dest->ip == NULL || dest->port < 0) {
		return NULL;
	}

	/*First try to set the destination*/
	udp_sender = ms_factory_create_filter(factory, MS_UDP_SEND_ID);
	if (ms_filter_call_method(udp_sender, MS_UDP_SEND_SET_DESTINATION, (void*)dest) != 0) {
		ms_error("Failed to set destination, aborting");
		ms_filter_destroy(udp_sender);
		return NULL;
	}

	/*Secondly try to open the PCAP file*/
	file_player = ms_factory_create_filter(factory,MS_PCAP_FILE_PLAYER_ID);
	if (ms_filter_call_method(file_player, MS_PLAYER_OPEN, (void*)filepath) != 0) {
		ms_error("Failed to open file %s, aborting", filepath);
		ms_filter_destroy(file_player);
		ms_filter_destroy(udp_sender);
		return NULL;
	}
	if (ms_filter_call_method(file_player, MS_PCAP_FILE_PLAYER_SET_TO_PORT, (void*)&to_port) != 0) {
		ms_error("Failed to set to port, aborting");
		ms_filter_destroy(file_player);
		ms_filter_destroy(udp_sender);
		return NULL;
	}
	if (ms_filter_call_method(file_player, MS_PCAP_FILE_PLAYER_SET_TS_OFFSET, (void*)&ts_offset) != 0) {
		ms_error("Failed to set ts_offset, aborting");
		ms_filter_destroy(file_player);
		ms_filter_destroy(udp_sender);
		return NULL;
	}
	

	s = ms_new0(MSPCAPSender, 1);
	s->udp_send = udp_sender;
	s->file_player = file_player;
	s->pcap_ended_cb = cb;
	s->pcap_ended_user_data = user_data;

	ms_filter_call_method(s->file_player, MS_PCAP_FILE_PLAYER_SET_LAYER, &pcap_filter_property);
	pcap_filter_property = MSPCAPFilePlayerTimeRefCapture;
	ms_filter_call_method(s->file_player, MS_PCAP_FILE_PLAYER_SET_TIMEREF, &pcap_filter_property);
	ms_filter_call_method(s->file_player, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
	ms_filter_add_notify_callback(s->file_player, reader_notify_cb, s, FALSE);
	ms_filter_call_method_noarg(s->file_player, MS_PLAYER_START);

	params.name = "MSUDP ticker";
	params.prio  = MS_TICKER_PRIO_REALTIME;
	s->ticker = ms_ticker_new_with_params(&params);
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, s->file_player, -1, 0);
	ms_connection_helper_link(&h, s->udp_send, 0, -1);
	ms_ticker_attach(s->ticker, s->file_player);

	return s;
}

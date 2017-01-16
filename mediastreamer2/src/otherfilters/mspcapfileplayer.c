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

#if defined(HAVE_CONFIG_H)
#include "mediastreamer-config.h"
#endif


#include "mediastreamer2/mspcapfileplayer.h"
#include "mediastreamer2/msfileplayer.h"
#include "waveheader.h"
#include "mediastreamer2/msticker.h"

#include <pcap/pcap.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>



static int player_close(MSFilter *f, void *arg);

struct _PlayerData{
	int fd;
	MSPlayerState state;
	int rate;
	int hsize;
	int pause_time;
	int count;
	int samplesize;
	int datalink_size;
	unsigned to_port;
	uint32_t ts;
	bool_t swap;
	bool_t is_raw;
	pcap_t *pcap;
	struct pcap_pkthdr *pcap_hdr;
	const u_char *pcap_data;
	bool_t pcap_started;
	uint64_t pcap_initial_time;
	uint32_t pcap_initial_ts;
	uint16_t pcap_seq;
	struct timeval pcap_initial_timeval;
	MSPCAPFilePlayerTimeRef pcap_timeref;
	MSPCAPFilePlayerLayer pcap_layer;
	uint32_t ts_offset;
};

typedef struct _PlayerData PlayerData;

static void player_init(MSFilter *f){
	PlayerData *d=ms_new0(PlayerData,1);
	d->fd=-1;
	d->state=MSPlayerClosed;
	d->swap=FALSE;
	d->rate=8000;
	d->samplesize=2;
	d->datalink_size=0;
	d->to_port=0;
	d->hsize=0;
	d->pause_time=0;
	d->count=0;
	d->ts=0;
	d->is_raw=TRUE;
	d->pcap = NULL;
	d->pcap_hdr = NULL;
	d->pcap_data = NULL;
	d->pcap_started = FALSE;
	d->pcap_initial_ts = 0;
	d->pcap_seq = 0;
	d->pcap_timeref = MSPCAPFilePlayerTimeRefRTP;
	d->pcap_layer = MSPCAPFilePlayerLayerPayload;
	f->data=d;
}

static uint link_layer_size(int datalink) {
	switch (datalink) {
		//cf http://www.tcpdump.org/linktypes.html
		case 1: return 14; //LINKTYPE_ETHERNET: Ethernet
		case 113: return 16; //LINKTYPE_LINUX_SLL: Linux "cooked" capture encapsulation
		default:
			ms_error("Unsupported network type %d, assuming no link header...", datalink);
			return 0;
	}
}

static int player_open(MSFilter *f, void *arg){
	PlayerData *d=(PlayerData*)f->data;
	int fd;
	const char *file=(const char*)arg;

	if (d->fd!=-1){
		player_close(f,NULL);
	}
	if ((fd=open(file,O_RDONLY|O_BINARY))==-1){
		ms_warning("MSFilePlayer[%p]: failed to open %s: %s",f,file,strerror(errno));
		return -1;
	}

	d->state=MSPlayerPaused;
	d->fd=fd;
	d->ts=0;
	d->pcap = NULL;
	d->pcap_started = FALSE;
	if (strstr(file, ".pcap")) {
		char err[PCAP_ERRBUF_SIZE];
		d->pcap = pcap_open_offline(file, err);
		if (d->pcap == NULL) {
			ms_error("MSFilePlayer[%p]: failed to open pcap file: %s",f,err);
			d->fd=-1;
			close(fd);
			return -1;
		}
		d->datalink_size = link_layer_size(pcap_datalink(d->pcap));
		ms_filter_notify_no_arg(f,MS_FILTER_OUTPUT_FMT_CHANGED);
		ms_message("MSFilePlayer[%p]: %s opened: rate=%i",f,file,d->rate);
		return 0;
	} else {
		ms_error("Unsupported file extension: %s", file);
		return -1;
	}
}

static int player_start(MSFilter *f, void *arg){
	PlayerData *d=(PlayerData*)f->data;
	if (d->state==MSPlayerPaused)
		d->state=MSPlayerPlaying;
	return 0;
}

static int player_pause(MSFilter *f, void *arg){
	PlayerData *d=(PlayerData*)f->data;
	ms_filter_lock(f);
	if (d->state==MSPlayerPlaying){
		d->state=MSPlayerPaused;
	}
	ms_filter_unlock(f);
	return 0;
}

static int player_close(MSFilter *f, void *arg){
	PlayerData *d=(PlayerData*)f->data;
	if (d->pcap) pcap_close(d->pcap);
	if (d->fd!=-1)	close(d->fd);
	d->fd=-1;
	d->state=MSPlayerClosed;
	return 0;
}

static int player_get_state(MSFilter *f, void *arg){
	PlayerData *d=(PlayerData*)f->data;
	*(int*)arg=d->state;
	return 0;
}

static void player_uninit(MSFilter *f){
	PlayerData *d=(PlayerData*)f->data;
	if (d->fd!=-1) player_close(f,NULL);
	ms_free(d);
}

static void player_process(MSFilter *f){
	PlayerData *d=(PlayerData*)f->data;

	ms_filter_lock(f);

	if (d->state==MSPlayerPlaying) {
		if (d->pcap) {
			int res;
			bool_t cont = TRUE;
			do {
				if (d->pcap_data && d->pcap_hdr) {
					/* The PCAP data has already been read at previous iteration. */
					res = 1;
				} else {
					res = pcap_next_ex(d->pcap, &d->pcap_hdr, &d->pcap_data);
				}

				if (res == -2) {
					ms_filter_notify_no_arg(f,MS_PLAYER_EOF);
					ms_filter_notify_no_arg(f, MS_FILE_PLAYER_EOF);
				} else if (res == -1) {
					ms_error("Error while reading next pcap packet: %s", pcap_geterr(d->pcap));
				} else if (res > 0) {
					const u_char *link_layer_header = &d->pcap_data[0];
					const u_char *ip_header = link_layer_header + d->datalink_size; // sizeof(link_layer_header)
					const u_char *udp_header = ip_header + 20; // sizeof(ipv4_header)
					const u_char *rtp_header = udp_header + 8; // sizeof(udp_header)
					const u_char *payload = rtp_header + 12; // sizeof(rtp_header)

					int headers_size = payload - link_layer_header;
					unsigned to_port = ntohs(*(uint16_t*)(udp_header + 2));
					if (d->to_port != 0 && d->to_port != to_port) {
						// Skip packets with invalid from port
						ms_debug("Outputting wrong-from-port packet (got %u but expected %u), ignoring", to_port, d->to_port);
						d->pcap_hdr = NULL;
						d->pcap_data = NULL;
					} else if (((int)d->pcap_hdr->caplen >= headers_size) && ((rtp_header[0] >> 6) == 2)) {
						// Check headers size and RTP version
						mblk_t *om = NULL;
						uint16_t pcap_seq = ntohs(*((uint16_t*)(rtp_header + 2)));
						uint32_t ts = ntohl(*((uint32_t*)(rtp_header + 4)));
						uint32_t diff_ms;
						bool_t markbit=rtp_header[1]>>7;
						int headers_size = payload - link_layer_header;
						int bytes_pcap = d->pcap_hdr->caplen - headers_size;
						if (d->pcap_started == FALSE) {
							d->pcap_started = TRUE;
							d->pcap_initial_ts = ts;
							d->pcap_initial_time = f->ticker->time;

							d->pcap_seq = pcap_seq;
							ms_message("initial ts=%u, seq=%u",ts,pcap_seq);
							d->pcap_initial_timeval.tv_sec = d->pcap_hdr->ts.tv_sec;
							d->pcap_initial_timeval.tv_usec = d->pcap_hdr->ts.tv_usec;
						}
						diff_ms = (d->pcap_timeref == MSPCAPFilePlayerTimeRefRTP) ?
							 ((ts - d->pcap_initial_ts) * 1000) / d->rate
							: 1000 * (d->pcap_hdr->ts.tv_sec - d->pcap_initial_timeval.tv_sec)
							+ (d->pcap_hdr->ts.tv_usec - d->pcap_initial_timeval.tv_usec) / 1000;

						if ((f->ticker->time - d->pcap_initial_time) >= diff_ms) {
							if (d->pcap_layer == MSPCAPFilePlayerLayerRTP) {
								int headers_size = link_layer_header - rtp_header;
								int bytes_pcap = d->pcap_hdr->caplen + headers_size;
								
								*((uint32_t*)(rtp_header + 4)) = htonl(ts + d->ts_offset);

								om = allocb(bytes_pcap, 0);
								memcpy(om->b_wptr, rtp_header, bytes_pcap);
								om->b_wptr += bytes_pcap;
							} else {
								if (pcap_seq >= d->pcap_seq) {
									om = allocb(bytes_pcap, 0);
									memcpy(om->b_wptr, payload, bytes_pcap);
									om->b_wptr += bytes_pcap;
									mblk_set_cseq(om, pcap_seq);
									mblk_set_timestamp_info(om, f->ticker->time);
									mblk_set_marker_info(om,markbit);
								}
							}
							if (om != NULL) {
								ms_debug("Outputting RTP packet of size %i, seq=%u markbit=%i", bytes_pcap, pcap_seq, (int)markbit);
								ms_queue_put(f->outputs[0], om);
							}
							d->pcap_seq = pcap_seq;
							d->pcap_hdr = NULL;
							d->pcap_data = NULL;
						} else {
							ms_debug("Outputting non-RTP packet, stopping");
							cont = FALSE;
						}
					} else {
						// Ignore wrong RTP packet
						ms_debug("Outputting wrong-RTP packet, ignoring");
						d->pcap_hdr = NULL;
						d->pcap_data = NULL;
					}
				}
			} while ((res > 0) && cont);
		}
	}
	ms_filter_unlock(f);
}

static int player_get_sr(MSFilter *f, void*arg){
	PlayerData *d=(PlayerData*)f->data;
	*((int*)arg)=d->rate;
	return 0;
}

static int player_set_sr(MSFilter *f, void *arg) {
	PlayerData *d = (PlayerData *)f->data;
	if (d->is_raw) d->rate = *((int *)arg);
	else return -1;
	ms_message("MSFilePlayer[%p]: new rate=%i",f,d->rate);
	return 0;
}

static int player_set_layer(MSFilter *f, void *arg){
	PlayerData *d=(PlayerData*)f->data;
	d->pcap_layer = *(int*)arg;
	return 0;
}

static int player_set_timeref(MSFilter *f, void *arg){
	PlayerData *d=(PlayerData*)f->data;
	d->pcap_timeref = *(int*)arg;
	return 0;
}

static int player_set_to_port(MSFilter *f, void *arg){
	PlayerData *d=(PlayerData*)f->data;
	d->to_port = *(unsigned*)arg;
	return 0;
}

static int player_set_ts_offset(MSFilter *f, void *arg){
	PlayerData *d=(PlayerData*)f->data;
	d->ts_offset = *(uint32_t*)arg;
	return 0;
}

static MSFilterMethod player_methods[]={
	{ MS_FILTER_GET_SAMPLE_RATE, player_get_sr},
	{ MS_FILTER_SET_SAMPLE_RATE, player_set_sr},
	{ MS_PCAP_FILE_PLAYER_SET_LAYER, player_set_layer},
	{ MS_PCAP_FILE_PLAYER_SET_TIMEREF, player_set_timeref},
	{ MS_PCAP_FILE_PLAYER_SET_TO_PORT, player_set_to_port},
	{ MS_PCAP_FILE_PLAYER_SET_TS_OFFSET, player_set_ts_offset},

	/* this file player implements the MSFilterPlayerInterface*/
	{ MS_PLAYER_OPEN , player_open },
	{ MS_PLAYER_START , player_start },
	{ MS_PLAYER_PAUSE, player_pause },
	{ MS_PLAYER_CLOSE, player_close },
	{ MS_PLAYER_GET_STATE, player_get_state },
	{ 0, NULL }
};

#ifdef WIN32

MSFilterDesc ms_pcap_file_player_desc={
	MS_PCAP_FILE_PLAYER_ID,
	"MSPCAPFilePlayer",
	N_("PCAP file player"),
	MS_FILTER_OTHER,
	NULL,
	0,
	1,
	player_init,
	NULL,
	player_process,
	NULL,
	player_uninit,
	player_methods
};

#else

MSFilterDesc ms_pcap_file_player_desc={
	.id=MS_PCAP_FILE_PLAYER_ID,
	.name="MSPCAPFilePlayer",
	.text=N_("PCAP files player"),
	.category=MS_FILTER_OTHER,
	.ninputs=0,
	.noutputs=1,
	.init=player_init,
	.process=player_process,
	.uninit=player_uninit,
	.methods=player_methods
};

#endif

MS_FILTER_DESC_EXPORT(ms_pcap_file_player_desc)



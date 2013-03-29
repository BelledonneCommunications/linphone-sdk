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

#include "mediastreamer2/msfileplayer.h"
#include "waveheader.h"
#include "mediastreamer2/msticker.h"

#ifdef HAVE_PCAP
#include <pcap/pcap.h>
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>



static int player_close(MSFilter *f, void *arg);

struct _PlayerData{
	int fd;
	MSPlayerState state;
	int rate;
	int nchannels;
	int hsize;
	int loop_after;
	int pause_time;
	int count;
	int samplesize;
	uint32_t ts;
	bool_t swap;
	bool_t is_raw;
#ifdef HAVE_PCAP
	pcap_t *pcap;
	struct pcap_pkthdr *pcap_hdr;
	const u_char *pcap_data;
	bool_t pcap_started;
	uint64_t pcap_initial_time;
	uint32_t pcap_initial_ts;
	uint16_t pcap_seq;
#endif
};

typedef struct _PlayerData PlayerData;

static void player_init(MSFilter *f){
	PlayerData *d=ms_new0(PlayerData,1);
	d->fd=-1;
	d->state=MSPlayerClosed;
	d->swap=FALSE;
	d->rate=8000;
	d->nchannels=1;
	d->samplesize=2;
	d->hsize=0;
	d->loop_after=-1; /*by default, don't loop*/
	d->pause_time=0;
	d->count=0;
	d->ts=0;
	d->is_raw=TRUE;
#ifdef HAVE_PCAP
	d->pcap = NULL;
	d->pcap_hdr = NULL;
	d->pcap_data = NULL;
	d->pcap_started = FALSE;
	d->pcap_initial_ts = 0;
	d->pcap_seq = 0;
#endif
	f->data=d;	
}

int ms_read_wav_header_from_fd(wave_header_t *header,int fd){
	int count;
	int skip;
	int hsize=0;
	riff_t *riff_chunk=&header->riff_chunk;
	format_t *format_chunk=&header->format_chunk;
	data_t *data_chunk=&header->data_chunk;
	
	unsigned long len=0;
	
	len = read(fd, (char*)riff_chunk, sizeof(riff_t)) ;
	if (len != sizeof(riff_t)){
		goto not_a_wav;
	}
	
	if (0!=strncmp(riff_chunk->riff, "RIFF", 4) || 0!=strncmp(riff_chunk->wave, "WAVE", 4)){
		goto not_a_wav;
	}
	
	len = read(fd, (char*)format_chunk, sizeof(format_t)) ;            
	if (len != sizeof(format_t)){
		ms_warning("Wrong wav header: cannot read file");
		goto not_a_wav;
	}
	
	if ((skip=le_uint32(format_chunk->len)-0x10)>0)
	{
		lseek(fd,skip,SEEK_CUR);
	}
	hsize=sizeof(wave_header_t)-0x10+le_uint32(format_chunk->len);
	
	count=0;
	do{
		len = read(fd, data_chunk, sizeof(data_t)) ;
		if (len != sizeof(data_t)){
			ms_warning("Wrong wav header: cannot read file");
			goto not_a_wav;
		}
		if (strncmp(data_chunk->data, "data", 4)!=0){
			ms_warning("skipping chunk=%s len=%i", data_chunk->data, data_chunk->len);
			lseek(fd,le_uint32(data_chunk->len),SEEK_CUR);
			count++;
			hsize+=len+le_uint32(data_chunk->len);
		}else{
			hsize+=len;
			break;
		}
	}while(count<30);
	return hsize;

	not_a_wav:
		/*rewind*/
		lseek(fd,0,SEEK_SET);
		return -1;
}

static int read_wav_header(PlayerData *d){
	wave_header_t header;
	format_t *format_chunk=&header.format_chunk;
	int ret=ms_read_wav_header_from_fd(&header,d->fd);
	
	if (ret==-1) goto not_a_wav;
	
	d->rate=le_uint32(format_chunk->rate);
	d->nchannels=le_uint16(format_chunk->channel);
	if (d->nchannels==0) goto not_a_wav;
	d->samplesize=le_uint16(format_chunk->blockalign)/d->nchannels;
	d->hsize=ret;
	
	#ifdef WORDS_BIGENDIAN
	if (le_uint16(format_chunk->blockalign)==le_uint16(format_chunk->channel) * 2)
		d->swap=TRUE;
	#endif
	d->is_raw=FALSE;
	return 0;

	not_a_wav:
		/*rewind*/
		lseek(d->fd,0,SEEK_SET);
		d->hsize=0;
		d->is_raw=TRUE;
		return -1;
}

static int player_open(MSFilter *f, void *arg){
	PlayerData *d=(PlayerData*)f->data;
	int fd;
	const char *file=(const char*)arg;

	if (d->fd!=-1){
		player_close(f,NULL);
	}
	if ((fd=open(file,O_RDONLY|O_BINARY))==-1){
		ms_warning("Failed to open %s",file);
		return -1;
	}
	d->state=MSPlayerPaused;
	d->fd=fd;
	d->ts=0;
#ifdef HAVE_PCAP
	d->pcap = NULL;
	d->pcap_started = FALSE;
	if (strstr(file, ".pcap")) {
		char err[PCAP_ERRBUF_SIZE];
		d->pcap = pcap_open_offline(file, err);
		if (d->pcap == NULL) {
			ms_error("Failed to open pcap file: %s", err);
			d->fd=-1;
			close(fd);
			return -1;
		}
	} else
#endif
	if (read_wav_header(d)!=0 && strstr(file,".wav")){
		ms_warning("File %s has .wav extension but wav header could be found.",file);
	}
	ms_message("%s opened: rate=%i,channel=%i",file,d->rate,d->nchannels);
	return 0;
}

static int player_start(MSFilter *f, void *arg){
	PlayerData *d=(PlayerData*)f->data;
	if (d->state==MSPlayerPaused)
		d->state=MSPlayerPlaying;
	return 0;
}

static int player_stop(MSFilter *f, void *arg){
	PlayerData *d=(PlayerData*)f->data;
	ms_filter_lock(f);
	if (d->state!=MSPlayerClosed){
		d->state=MSPlayerPaused;
		lseek(d->fd,d->hsize,SEEK_SET);
	}
	ms_filter_unlock(f);
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
	player_stop(f,NULL);
#ifdef HAVE_PCAP
	if (d->pcap) pcap_close(d->pcap);
#endif
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

static void swap_bytes(unsigned char *bytes, int len){
	int i;
	unsigned char tmp;
	for(i=0;i<len;i+=2){
		tmp=bytes[i];
		bytes[i]=bytes[i+1];
		bytes[i+1]=tmp;
	}
}

static void player_process(MSFilter *f){
	PlayerData *d=(PlayerData*)f->data;
	int nsamples=(f->ticker->interval*d->rate*d->nchannels)/1000;
	int bytes;
	/*send an even number of samples each tick. At 22050Hz the number of samples per 10 ms chunk is odd.
	Odd size buffer of samples cause troubles to alsa. Fixing in alsa is difficult, so workaround here.
	*/
	if (nsamples & 0x1 ) { //odd number of samples
		if (d->count & 0x1 )
			nsamples++;
		else
			nsamples--;
	}
	bytes=nsamples*d->samplesize;
	d->count++;
	ms_filter_lock(f);
	if (d->state==MSPlayerPlaying){
#ifdef HAVE_PCAP
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
					ms_filter_notify_no_arg(f, MS_FILE_PLAYER_EOF);
				} else if (res > 0) {
					const u_char *ethernet_header = &d->pcap_data[0];
					const u_char *ip_header = ethernet_header + 14; // sizeof(ethernet_header)
					const u_char *udp_header = ip_header + 20; // sizeof(ipv4_header)
					const u_char *rtp_header = udp_header + 8; // sizeof(udp_header)
					const u_char *payload = rtp_header + 12; // sizeof(rtp_header)
					int headers_size = payload - ethernet_header;
					// Check headers size and RTP version
					if ((d->pcap_hdr->caplen >= headers_size) && ((rtp_header[0] >> 6) == 2)) {
						int bytes = d->pcap_hdr->caplen - headers_size;
						mblk_t *om;
						uint16_t pcap_seq = ntohs(*((uint16_t*)(rtp_header + 2)));
						uint32_t ts = ntohl(*((uint32_t*)(rtp_header + 4)));
						uint32_t diff_ms;
						if (d->pcap_started == FALSE) {
							d->pcap_started = TRUE;
							d->pcap_initial_ts = ts;
							d->pcap_initial_time = f->ticker->time;
							d->pcap_seq = pcap_seq;
						}
						diff_ms = ((ts - d->pcap_initial_ts) * 1000) / d->rate;
						if ((f->ticker->time - d->pcap_initial_time) >= diff_ms) {
							if (pcap_seq >= d->pcap_seq) {
								om = allocb(bytes, 0);
								memcpy(om->b_wptr, payload, bytes);
								om->b_wptr += bytes;
								mblk_set_timestamp_info(om, f->ticker->time);
								ms_queue_put(f->outputs[0], om);
							}
							d->pcap_seq = pcap_seq;
							d->pcap_hdr = NULL;
							d->pcap_data = NULL;
						} else {
							cont = FALSE;
						}
					} else {
						// Ignore wrong RTP packet
						d->pcap_hdr = NULL;
						d->pcap_data = NULL;
					}
				}
			} while ((res > 0) && cont);
		} else
#endif
		{
			int err;
			mblk_t *om=allocb(bytes,0);
			if (d->pause_time>0){
				err=bytes;
				memset(om->b_wptr,0,bytes);
				d->pause_time-=f->ticker->interval;
			}else{
				err=read(d->fd,om->b_wptr,bytes);
				if (d->swap) swap_bytes(om->b_wptr,bytes);
			}
			if (err>=0){
				if (err!=0){
					if (err<bytes)
						memset(om->b_wptr+err,0,bytes-err);
					om->b_wptr+=bytes;
					mblk_set_timestamp_info(om,d->ts);
					d->ts+=nsamples;
					ms_queue_put(f->outputs[0],om);
				}else freemsg(om);
				if (err<bytes){
					ms_filter_notify_no_arg(f,MS_FILE_PLAYER_EOF);
					lseek(d->fd,d->hsize,SEEK_SET);

					/* special value for playing file only once */
					if (d->loop_after<0)
					{
						d->state=MSPlayerPaused;
						ms_filter_unlock(f);
						return;
					}

					if (d->loop_after>=0){
						d->pause_time=d->loop_after;
					}
				}
			}else{
				ms_warning("Fail to read %i bytes: %s",bytes,strerror(errno));
			}
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
	/* This function should be used only when playing a PCAP or raw file */
	PlayerData *d = (PlayerData *)f->data;
	if (d->is_raw) d->rate = *((int *)arg);
	else return -1;
	return 0;
}

static int player_loop(MSFilter *f, void *arg){
	PlayerData *d=(PlayerData*)f->data;
	d->loop_after=*((int*)arg);
	return 0;
}

static int player_eof(MSFilter *f, void *arg){
	PlayerData *d=(PlayerData*)f->data;
	if (d->fd==-1 && d->state==MSPlayerClosed)
		*((int*)arg) = TRUE; /* 1 */
	else
		*((int*)arg) = FALSE; /* 0 */
	return 0;
}

static int player_get_nch(MSFilter *f, void *arg){
	PlayerData *d=(PlayerData*)f->data;
	*((int*)arg)=d->nchannels;
	return 0;
}

static MSFilterMethod player_methods[]={
	{	MS_FILE_PLAYER_OPEN,	player_open	},
	{	MS_FILE_PLAYER_START,	player_start	},
	{	MS_FILE_PLAYER_STOP,	player_stop	},
	{	MS_FILE_PLAYER_CLOSE,	player_close	},
	{	MS_FILTER_GET_SAMPLE_RATE, player_get_sr},
	{	MS_FILTER_SET_SAMPLE_RATE, player_set_sr},
	{	MS_FILTER_GET_NCHANNELS, player_get_nch	},
	{	MS_FILE_PLAYER_LOOP,	player_loop	},
	{	MS_FILE_PLAYER_DONE,	player_eof	},
	/* this wav file player implements the MSFilterPlayerInterface*/
	{ MS_PLAYER_OPEN , player_open },
	{ MS_PLAYER_START , player_start },
	{ MS_PLAYER_PAUSE, player_pause },
	{ MS_PLAYER_CLOSE, player_close },
	{ MS_PLAYER_GET_STATE, player_get_state },
	{	0,			NULL		}
};

#ifdef WIN32

MSFilterDesc ms_file_player_desc={
	MS_FILE_PLAYER_ID,
	"MSFilePlayer",
	N_("Raw files and wav reader"),
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

MSFilterDesc ms_file_player_desc={
	.id=MS_FILE_PLAYER_ID,
	.name="MSFilePlayer",
	.text=N_("Raw files and wav reader"),
	.category=MS_FILTER_OTHER,
	.ninputs=0,
	.noutputs=1,
	.init=player_init,
	.process=player_process,
	.uninit=player_uninit,
	.methods=player_methods
};

#endif

MS_FILTER_DESC_EXPORT(ms_file_player_desc)

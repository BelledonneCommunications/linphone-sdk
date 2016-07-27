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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#if defined(HAVE_CONFIG_H)
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/msfilerec.h"
#include "waveheader.h"
#include "asyncrw.h"


static int rec_close(MSFilter *f, void *arg);
static void write_wav_header(int fd, int rate, int nchannels, int size);

typedef struct RecState{
	int fd;
	int rate;
	int nchannels;
	int size;
	int max_size;
	char *mime;
	MSAsyncWriter *writer;
	MSRecorderState state;
	bool_t swap;
} RecState;

static void rec_init(MSFilter *f){
	RecState *s=ms_new0(RecState,1);
	s->fd=-1;
	s->rate=8000;
	s->nchannels = 1;
	s->size=0;
	s->max_size=0;
	s->state=MSRecorderClosed;
	s->mime = "pcm";
	s->swap = FALSE;
	f->data=s;
}

static void _rec_close(RecState *s);

static void swap_bytes(unsigned char *bytes, int len){
	int i;
	unsigned char tmp;
	for(i=0;i<len;i+=2){
		tmp=bytes[i];
		bytes[i]=bytes[i+1];
		bytes[i+1]=tmp;
	}
}

static void rec_process(MSFilter *f){
	RecState *s=(RecState*)f->data;
	mblk_t *m;
	
	ms_mutex_lock(&f->lock);
	while((m=ms_queue_get(f->inputs[0]))!=NULL){
		
		if (s->state==MSRecorderRunning){
			int len=(int)(m->b_wptr-m->b_rptr);
			int max_size_reached = 0;
			if (s->max_size!=0 && s->size+len > s->max_size) {
				len = s->max_size - s->size;
				max_size_reached = 1;
			}
			if (s->swap) swap_bytes(m->b_wptr,len);
			ms_async_reader_write(s->writer,m);
			s->size+=len;
			if (max_size_reached) {
				ms_warning("MSFileRec: Maximum size (%d) has been reached. closing file.",s->max_size);
				_rec_close(s);
				ms_filter_notify_no_arg(f,MS_RECORDER_MAX_SIZE_REACHED);
			}
		}else freemsg(m);
	}
	ms_mutex_unlock(&f->lock);
}

static int rec_get_length(const char *file, int *length){
	wave_header_t header;
	int fd=open(file,O_RDONLY|O_BINARY);
	int ret=ms_read_wav_header_from_fd(&header,fd);
	close(fd);
	if (ret>0){
		*length=le_uint32(header.data_chunk.len);
	}else{
		*length=0;
	}
	return ret;
}

static int rec_open(MSFilter *f, void *arg){
	RecState *s=(RecState*)f->data;
	const char *filename=(const char*)arg;
	int flags;
	
	if (s->fd!=-1) rec_close(f,NULL);
	
	if (access(filename,R_OK|W_OK)==0){
		flags=O_WRONLY|O_BINARY;
		if (rec_get_length(filename,&s->size)>0){
			ms_message("Opening wav file in append mode, current data size is %i",s->size);
		}
	}else{
		flags=O_WRONLY|O_CREAT|O_TRUNC|O_BINARY;
		s->size=0;
	}
	s->fd=open(filename,flags, S_IRUSR|S_IWUSR);
	if (s->fd==-1){
		ms_warning("Cannot open %s: %s",filename,strerror(errno));
		return -1;
	}
	if (s->size>0){
		struct stat statbuf;
		if (fstat(s->fd,&statbuf)==0){
			if (lseek(s->fd,statbuf.st_size,SEEK_SET) == -1){
				int err = errno;
				ms_error("Could not lseek to end of file: %s",strerror(err));
			}
		}else ms_error("fstat() failed: %s",strerror(errno));
	}
	ms_message("MSFileRec: recording into %s",filename);
	s->writer = ms_async_writer_new(s->fd);
	ms_mutex_lock(&f->lock);
	s->state=MSRecorderPaused;
	ms_mutex_unlock(&f->lock);
	return 0;
}

static int rec_start(MSFilter *f, void *arg){
	RecState *s=(RecState*)f->data;
	if (s->state!=MSRecorderPaused){
		ms_error("MSFileRec: cannot start, state=%i",s->state);
		return -1;
	}
	ms_mutex_lock(&f->lock);
	s->state=MSRecorderRunning;
	ms_mutex_unlock(&f->lock);
	return 0;
}

static int rec_stop(MSFilter *f, void *arg){
	RecState *s=(RecState*)f->data;
	ms_mutex_lock(&f->lock);
	s->state=MSRecorderPaused;
	ms_mutex_unlock(&f->lock);
	return 0;
}

static void write_wav_header(int fd, int rate, int nchannels, int size){
	wave_header_t header;
	memcpy(&header.riff_chunk.riff,"RIFF",4);
	header.riff_chunk.len=le_uint32(size+32);
	memcpy(&header.riff_chunk.wave,"WAVE",4);

	memcpy(&header.format_chunk.fmt,"fmt ",4);
	header.format_chunk.len=le_uint32(0x10);
	header.format_chunk.type=le_uint16(0x1);
	header.format_chunk.channel=le_uint16(nchannels);
	header.format_chunk.rate=le_uint32(rate);
	header.format_chunk.bps=le_uint32(rate*2*nchannels);
	header.format_chunk.blockalign=le_uint16(2*nchannels);
	header.format_chunk.bitpspl=le_uint16(16);

	memcpy(&header.data_chunk.data,"data",4);
	header.data_chunk.len=le_uint32(size);
	lseek(fd,0,SEEK_SET);
	if (write(fd,&header,sizeof(header))!=sizeof(header)){
		ms_warning("Fail to write wav header.");
	}
}

static void _rec_close(RecState *s){
	s->state=MSRecorderClosed;
	if (s->fd!=-1){
		ms_async_writer_destroy(s->writer);
		s->writer = NULL;
		write_wav_header(s->fd, s->rate, s->nchannels, s->size);
		close(s->fd);
		s->fd=-1;
	}
}

static int rec_close(MSFilter *f, void *arg){
	RecState *s=(RecState*)f->data;
	ms_mutex_lock(&f->lock);
	_rec_close(s);
	ms_mutex_unlock(&f->lock);
	return 0;
}

static int rec_get_state(MSFilter *f, void *arg){
	RecState *s=(RecState*)f->data;
	*(MSRecorderState*)arg=s->state;
	return 0;
}

static int rec_set_sr(MSFilter *f, void *arg){
	RecState *s=(RecState*)f->data;
	ms_mutex_lock(&f->lock);
	s->rate=*((int*)arg);
	ms_mutex_unlock(&f->lock);
	return 0;
}

static int rec_get_sr(MSFilter *f, void *arg){
	RecState *d=(RecState*)f->data;
	int *sample_rate = (int *)arg;
	*sample_rate = d->rate;
	return 0;
}

static int rec_set_nchannels(MSFilter *f, void *arg) {
	RecState *s = (RecState *)f->data;
	s->nchannels = *(int *)arg;
	return 0;
}

static int rec_get_nchannels(MSFilter *f, void *arg){
	RecState *d=(RecState*)f->data;
	int *nchannels = (int *)arg;
	*nchannels = d->nchannels;
	return 0;
}

static void rec_uninit(MSFilter *f){
	RecState *s=(RecState*)f->data;
	if (s->fd!=-1) rec_close(f,NULL);
	ms_free(s);
}

static int rec_get_fmtp(MSFilter *f, void *arg){
	RecState *d=(RecState*)f->data;
	MSPinFormat *pinfmt = (MSPinFormat*)arg;
	if (pinfmt->pin == 0) pinfmt->fmt = ms_factory_get_audio_format(f->factory, d->mime, d->rate, d->nchannels, NULL);
	return 0;
}

static int rec_set_fmtp(MSFilter *f, void *arg){
	RecState *d=(RecState*)f->data;
	MSPinFormat *pinfmt = (MSPinFormat*)arg;
	ms_filter_lock(f);
	d->rate = pinfmt->fmt->rate;
	d->nchannels = pinfmt->fmt->nchannels;
	d->mime = pinfmt->fmt->encoding;
	ms_filter_unlock(f);
	return 0;
}

static int rec_set_max_size(MSFilter *f, void *arg) {
	RecState *d=(RecState*)f->data;
	d->max_size = *((int *) arg);
	return 0;
}

static MSFilterMethod rec_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE,	rec_set_sr	},
	{	MS_FILTER_SET_NCHANNELS	,	rec_set_nchannels	},
	{	MS_FILTER_GET_SAMPLE_RATE,	rec_get_sr	},
	{	MS_FILTER_GET_NCHANNELS	,	rec_get_nchannels	},
	{	MS_FILE_REC_OPEN	,	rec_open	},
	{	MS_FILE_REC_START	,	rec_start	},
	{	MS_FILE_REC_STOP	,	rec_stop	},
	{	MS_FILE_REC_CLOSE	,	rec_close	},
	{	MS_RECORDER_OPEN	,	rec_open	},
	{	MS_RECORDER_START	,	rec_start	},
	{	MS_RECORDER_PAUSE	,	rec_stop	},
	{	MS_RECORDER_CLOSE	,	rec_close	},
	{	MS_RECORDER_GET_STATE	,	rec_get_state	},
	{ 	MS_FILTER_GET_OUTPUT_FMT, 	rec_get_fmtp },
	{ 	MS_FILTER_SET_OUTPUT_FMT, 	rec_set_fmtp },
	{	MS_RECORDER_SET_MAX_SIZE,	rec_set_max_size },
	{	0			,	NULL		}
};

#ifdef _WIN32

MSFilterDesc ms_file_rec_desc={
	MS_FILE_REC_ID,
	"MSFileRec",
	N_("Wav file recorder"),
	MS_FILTER_OTHER,
	NULL,
	1,
	0,
	rec_init,
	NULL,
	rec_process,
	NULL,
	rec_uninit,
	rec_methods
};

#else

MSFilterDesc ms_file_rec_desc={
	.id=MS_FILE_REC_ID,
	.name="MSFileRec",
	.text=N_("Wav file recorder"),
	.category=MS_FILTER_OTHER,
	.ninputs=1,
	.noutputs=0,
	.init=rec_init,
	.process=rec_process,
	.uninit=rec_uninit,
	.methods=rec_methods
};

#endif

MS_FILTER_DESC_EXPORT(ms_file_rec_desc)

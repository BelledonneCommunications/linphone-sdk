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

#ifdef WIN32
#include <malloc.h>  // for alloca
#endif

#include <stdint.h>
#include <mediastreamer2/msfilter.h>
#include <ortp/payloadtype.h>
#include "g722.h"

#define TYPE(val)				.type=(val)
#define CLOCK_RATE(val)			.clock_rate=(val)
#define BITS_PER_SAMPLE(val)	.bits_per_sample=(val)
#define ZERO_PATTERN(val)		.zero_pattern=(val)
#define PATTERN_LENGTH(val)		.pattern_length=(val)
#define NORMAL_BITRATE(val)		.normal_bitrate=(val)
#define MIME_TYPE(val)			.mime_type=(val)
#define CHANNELS(val)			.channels=(val)
#define FMTP(val)				.FMTP=(val)

struct EncState {
	struct g722_encode_state *state;
	uint32_t ts;
	int   ptime;
	MSBufferizer *bufferizer;
};

static void enc_init(MSFilter *f)
{
	struct EncState *s=(struct EncState*)ms_new(struct EncState,1);
	s->state = g722_encode_init(NULL, 64000, 0);
	s->ts=0;
	s->bufferizer=ms_bufferizer_new();
	s->ptime = 20;
	f->data=s;
};

static void enc_uninit(MSFilter *f)
{
	struct EncState *s=(struct EncState*)f->data;
	g722_encode_release(s->state);
	ms_bufferizer_destroy(s->bufferizer);
	ms_free(s);
	f->data = 0;
};

static void enc_process(MSFilter *f)
{
	struct EncState *s=(struct EncState*)f->data;
	mblk_t *im;
	int nbytes;
	uint8_t *buf;
	int frame_per_packet=1;
	int chunksize;
	
	if(s->ptime>=10)
		frame_per_packet = s->ptime/10;

	if(frame_per_packet<=0)
		frame_per_packet=1;

	nbytes = 160*2;  //  10 Msec at 16KHZ  = 320 bytes of data
	buf = (uint8_t*)alloca(nbytes*frame_per_packet);

	while((im=ms_queue_get(f->inputs[0])))
		ms_bufferizer_put(s->bufferizer,im);

	chunksize = nbytes*frame_per_packet;
	while(ms_bufferizer_read(s->bufferizer,buf, chunksize) == chunksize) {
		mblk_t *om=allocb(nbytes*frame_per_packet,0);//too large...
		int k;
		
		k = g722_encode(s->state, om->b_wptr, (int16_t *)buf, chunksize/2);		
		om->b_wptr += k;
		mblk_set_timestamp_info(om,s->ts);		
		ms_queue_put(f->outputs[0],om);
		s->ts += chunksize/4;  // Nr of samples is really chunksize/2 but for G722 we must 
		                       // pretend we have a 8KHZ sampling rate
	}
};

static int enc_add_attr(MSFilter *f, void *arg)
{
	const char *fmtp=(const char*)arg;
	struct EncState *s=(struct EncState*)f->data;
	if(strstr(fmtp,"ptime:"))
		s->ptime = atoi(fmtp+6);

	return 0;
};

static MSFilterMethod enc_methods[]={
	{	MS_FILTER_ADD_ATTR		,	enc_add_attr},
	{	0				,	NULL		}
};

#ifdef _MSC_VER

MSFilterDesc ms_g722_enc_desc={
	MS_G722_ENC_ID,
	"MSG722Enc",
	"The G.722 wideband codec",
	MS_FILTER_ENCODER,
	"g722",
	1,
	1,
	enc_init,
	NULL,
	enc_process,
	NULL,
	enc_uninit,
	enc_methods
};

#else

MSFilterDesc ms_g722_enc_desc={
	.id			= MS_G722_ENC_ID,
	.name		= "MSG722Enc",
	.text		= "The G.722 wideband codec",
	.category	= MS_FILTER_ENCODER,
	.enc_fmt	= "g722",
	.ninputs	= 1,
	.noutputs	= 1,
	.init		= enc_init,
	.process	= enc_process,
	.uninit		= enc_uninit,
	.methods	= enc_methods
};
#endif

struct DecState {
	struct g722_decode_state *state;
};

static void dec_init(MSFilter *f){
	struct DecState *s=(struct DecState*)ms_new(struct DecState,1);
	f->data=s;

	s->state = g722_decode_init(NULL, 64000, 0);
};

static void dec_uninit(MSFilter *f)
{
	struct DecState *s=(struct DecState*)f->data;
	g722_decode_release(s->state);
	ms_free(f->data);
	f->data = 0;
};

static void dec_process(MSFilter *f)
{
	struct DecState *s=(struct DecState*)f->data;
	mblk_t *im;
	mblk_t *om;

	while((im=ms_queue_get(f->inputs[0]))) {
		int payloadlen = im->b_wptr - im->b_rptr;
		int declen;

		om=allocb(payloadlen*4,0);
		if ((declen = g722_decode(s->state,(int16_t *)om->b_wptr, im->b_rptr, payloadlen))<0) {
			ms_warning("g722_decode error!");
			freemsg(om);
		} else {
			om->b_wptr  += declen*2;
			ms_queue_put(f->outputs[0],om);
		}
		freemsg(im);
	}
};

#ifdef _MSC_VER

MSFilterDesc ms_g722_dec_desc={
	MS_G722_DEC_ID,
	"MSG722Dec",
	"The G.722 wideband codec",
	MS_FILTER_DECODER,
	"g722",
	1,
	1,
	dec_init,
	NULL,
	dec_process,
	NULL,
	dec_uninit,
	NULL
};

#else

MSFilterDesc ms_g722_dec_desc={
	.id			= MS_G722_DEC_ID,
	.name		= "MSG722Dec",
	.text		= "The G.722 wideband codec",
	.category	= MS_FILTER_DECODER,
	.enc_fmt	= "g722",
	.ninputs	= 1,
	.noutputs	= 1,
	.init		= dec_init,
	.process	= dec_process,
	.uninit		= dec_uninit
};

#endif


MS_FILTER_DESC_EXPORT(ms_g722_dec_desc)
MS_FILTER_DESC_EXPORT(ms_g722_enc_desc)


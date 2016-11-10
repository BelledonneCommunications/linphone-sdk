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

#ifdef _WIN32
#include <malloc.h>  // for alloca
#endif
#include <stdint.h>

#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/msticker.h>
#include "genericplc.h"


#ifdef HAVE_SPANDSP
/*use the G722 version from spandsp, LGPL */
#include <spandsp.h>
#else
/*otherwise use built-in version, forked from spandsp a long time ago and that has a public license.*/
#include "g722.h"
#endif

#define ENABLE_PCM_RESCALING 1

struct EncState {
	g722_encode_state_t *state;
	uint32_t ts;
	int   ptime;
	MSBufferizer *bufferizer;
};

static void enc_init(MSFilter *f)
{
	struct EncState *s=ms_new0(struct EncState,1);
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

static void scale_down(int16_t *samples, int count){
#if ENABLE_PCM_RESCALING
	int i;
	for (i=0;i<count;++i)
		samples[i]=samples[i]>>2;
#endif
}

static void scale_up(int16_t *samples, int count){
#if ENABLE_PCM_RESCALING
	int i;
	for (i=0;i<count;++i) {
		if (samples[i]>INT16_MAX/4) {
			samples[i] = INT16_MAX;
		} else if (samples[i]<INT16_MIN/4) {
			samples[i] = INT16_MIN;
		} else {
			samples[i]=samples[i]<<2;
		}
	}
#endif
}

static void enc_process(MSFilter *f)
{
	struct EncState *s=(struct EncState*)f->data;
	mblk_t *im;
	int nbytes;
	uint8_t *buf;
	int frame_per_packet=1;
	size_t chunksize;
	
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
		
		scale_down((int16_t *)buf,(int)(chunksize/2));
		k = g722_encode(s->state, om->b_wptr, (int16_t *)buf, (int)(chunksize/2));
		om->b_wptr += k;
		ms_bufferizer_fill_current_metas(s->bufferizer, om);
		mblk_set_timestamp_info(om,s->ts);
		ms_queue_put(f->outputs[0],om);
		/* Nr of samples is really chunksize/2 but for G722 we must pretend we have a 8KHZ sampling rate */
		s->ts += (uint32_t)(chunksize/4);
	}
};

static void set_ptime(struct EncState *s, int value){
	if (value>0 && value<=100){
		s->ptime=value;
	}
}

static int enc_add_attr(MSFilter *f, void *arg)
{
	const char *fmtp=(const char*)arg;
	struct EncState *s=(struct EncState*)f->data;
	if(strstr(fmtp,"ptime:"))
		set_ptime(s,atoi(fmtp+6));

	return 0;
};

static int enc_add_fmtp(MSFilter *f, void *arg){
	const char *fmtp=(const char*)arg;
	struct EncState *s=(struct EncState*)f->data;
	char tmp[16]={0};
	if (fmtp_get_value(fmtp,"ptime",tmp,sizeof(tmp))){
		set_ptime(s,atoi(tmp));
	}
	return 0;
}

static int get_sr(MSFilter *f, void *arg){
	*(int*)arg=16000;
	return 0;
}

static MSFilterMethod enc_methods[]={
	{	MS_FILTER_ADD_ATTR		,	enc_add_attr},
	{	MS_FILTER_ADD_FMTP		,	enc_add_fmtp},
	{	MS_FILTER_GET_SAMPLE_RATE,	get_sr	},
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
	g722_decode_state_t *dec_state;
	g722_encode_state_t *enc_state;
	plc_context_t *plc_context;
	MSConcealerContext* concealer;
};

static void dec_init(MSFilter *f){
	struct DecState *s=ms_new0(struct DecState,1);
	f->data=s;

	s->dec_state = g722_decode_init(NULL, 64000, 0);
	s->enc_state = g722_encode_init(NULL, 64000, 0);
	s->concealer = ms_concealer_context_new(UINT32_MAX);
	s->plc_context = generic_plc_create_context(16000); /* g722 has a sample rate of 16kHz */
};

static void dec_uninit(MSFilter *f)
{
	struct DecState *s=(struct DecState*)f->data;
	g722_decode_release(s->dec_state);
	g722_encode_release(s->enc_state);
	generic_plc_destroy_context(s->plc_context);
	ms_concealer_context_destroy(s->concealer);
	ms_free(f->data);
	f->data = 0;
};

static void dec_process(MSFilter *f) {
	struct DecState *s=(struct DecState*)f->data;
	mblk_t *im;
	mblk_t *om;

	while((im=ms_queue_get(f->inputs[0]))) {
		size_t msg_size = msgdsize(im);
		int declen;
		uint8_t *reencoded_buffer = ms_malloc0(msg_size);


		om=allocb(msg_size*4,0);
		mblk_meta_copy(im, om);

		if ((declen = g722_decode(s->dec_state,(int16_t *)om->b_wptr, im->b_rptr, (int)msg_size))<0) {
			ms_warning("g722_decode error!");
			freemsg(om);
		} else {
			ms_concealer_inc_sample_time(s->concealer, f->ticker->time, declen/16, TRUE); /* increase concealer context by sample nbr decoded/sample rate (in ms) */
			/* update local encoder state with received data, re-encoded buffer is just discarded */
			g722_encode(s->enc_state, reencoded_buffer, (int16_t *)om->b_wptr, declen);

			scale_up((int16_t *)om->b_wptr,declen);

			/* Store decoded data in plc_buffer */
			generic_plc_update_plc_buffer(s->plc_context, om->b_wptr, declen*sizeof(int16_t));

			/* introduce delay (TRANSITION_DELAY ms) */
			generic_plc_update_continuity_buffer(s->plc_context, om->b_wptr, declen*sizeof(int16_t));

			if (s->plc_context->plc_samples_used!=0) {
				/* we were doing PLC, now resuming with normal audio, continuity buffer is twice the transition delay lengths,
				 * the second half is untouched by the update function and contains transition data generated by PLC */
				generic_plc_transition_mix(((int16_t *)(om->b_wptr))+16*TRANSITION_DELAY, ((int16_t *)(s->plc_context->continuity_buffer))+16*TRANSITION_DELAY, 16*TRANSITION_DELAY);
			}
		}
		s->plc_context->plc_index=0;
		s->plc_context->plc_samples_used=0;

		om->b_wptr  += declen*sizeof(int16_t);
		ms_queue_put(f->outputs[0],om);

		ms_free(reencoded_buffer);
		freemsg(im);
	}

	/* PLC needed ?*/
	if (ms_concealer_context_is_concealement_required(s->concealer, f->ticker->time)) {
		unsigned int buff_size = 16*sizeof(int16_t)*f->ticker->interval; /* size in bytes of signal to be generated */
		unsigned char *reencoded_buffer = ms_malloc0(buff_size);
		unsigned char *decoded_buffer = ms_malloc0(buff_size);
		int k;

		ms_concealer_inc_sample_time(s->concealer, f->ticker->time, f->ticker->interval, FALSE);

		om = allocb(buff_size, 0);

		mblk_set_plc_flag(om, 1);
		generic_plc_generate_samples(s->plc_context, (int16_t *)om->b_wptr, (uint16_t)(buff_size/sizeof(int16_t)));
		/* store the generated samples into plc_buffer */
		generic_plc_update_plc_buffer(s->plc_context, om->b_wptr, buff_size);

		memcpy(decoded_buffer, om->b_wptr+TRANSITION_DELAY*16*sizeof(int16_t), buff_size-TRANSITION_DELAY*16*sizeof(int16_t));
		memcpy(decoded_buffer+buff_size-TRANSITION_DELAY*16*sizeof(int16_t), s->plc_context->continuity_buffer, TRANSITION_DELAY*16*sizeof(int16_t));

		/* update local encoder/decoder state with plc generated data, re-encoded buffer is just discarded */
		scale_down((int16_t *)decoded_buffer, 16*f->ticker->interval);
		k = g722_encode(s->enc_state, reencoded_buffer, (int16_t *)decoded_buffer, 16*f->ticker->interval);
		k = g722_decode(s->dec_state,(int16_t *)decoded_buffer, reencoded_buffer, k);
		ms_free(reencoded_buffer);
		ms_free(decoded_buffer);

		om->b_wptr += buff_size;
		ms_queue_put(f->outputs[0], om);
	}
};

static int ms_g722_dec_have_plc(MSFilter *f, void *arg) {
	*((int *)arg) = 1;
	return 0;
}

static MSFilterMethod dec_methods[]={
	{	MS_FILTER_GET_SAMPLE_RATE,	get_sr	},
	{ 	MS_AUDIO_DECODER_HAVE_PLC,	ms_g722_dec_have_plc	},
	{	0				,	NULL		}
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
	dec_methods,
	MS_FILTER_IS_PUMP
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
	.uninit		= dec_uninit,
	.flags = MS_FILTER_IS_PUMP,
	.methods	= dec_methods
};

#endif


MS_FILTER_DESC_EXPORT(ms_g722_dec_desc)
MS_FILTER_DESC_EXPORT(ms_g722_enc_desc)


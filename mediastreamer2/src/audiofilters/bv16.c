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

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/mscodecutils.h"

#include <bv16-floatingpoint/bvcommon/typedef.h>
#include <bv16-floatingpoint/bv16/bv16cnst.h>
#include <bv16-floatingpoint/bvcommon/bvcommon.h>
#include <bv16-floatingpoint/bv16/bv16strct.h>
#include <bv16-floatingpoint/bv16/bitpack.h>
#include <bv16-floatingpoint/bv16/bv16.h>


#ifdef _MSC_VER
#include <malloc.h>
#define alloca _alloca
#endif

/* signal and bitstream frame size in byte */
#define SIGNAL_FRAME_SIZE 80
#define BITSTREAM_FRAME_SIZE 10

#define NO_OF_BYTES_PER_5MS     10

typedef struct EncState{
	struct BV16_Encoder_State state;
	uint32_t ts; // timestamp
	int ptime;
	int max_ptime;
	int nsamples;
	int frame_size;
	int size_samples;
	MSBufferizer *bufferizer;
} EncState;

static int enc_set_ptime(MSFilter *f, void* arg){
	EncState *s=(EncState*)f->data;
	s->ptime=*(int*)arg;
	if (s->ptime > s->max_ptime) {
			s->ptime=s->max_ptime;
	} 
	else if (s->ptime%5) {
		//if the ptime is not a mulptiple of 5, go to the next multiple
		s->ptime = s->ptime - s->ptime%5 + 5; 
	}
	 ms_message("MSBV16Enc: got ptime=%i ", s->ptime);
	return 0;
}

static int enc_add_fmtp(MSFilter *f, void *arg){
	EncState *s=(EncState*)f->data;
	const char *fmtp=(const char *)arg;
	char tmp[64];
	tmp[0] = '\0';
	if (fmtp_get_value(fmtp,"maxptime:",tmp,sizeof(tmp))){
		s->max_ptime=atoi(tmp);
		if (s->max_ptime <10 || s->max_ptime >100 ) {
			ms_warning("MSBV16Enc: unknown value [%i] for maxptime, use default value (100) instead",s->max_ptime);
			s->max_ptime=100;
		}
		ms_message("MSBV16Enc: got maxptime=%i",s->max_ptime);
	}
	else if (fmtp_get_value(fmtp,"ptime",tmp,sizeof(tmp))){
	 	return enc_set_ptime(f,atoi(tmp));
	 }
	return 0;
}

static int enc_add_attr(MSFilter *f, void *arg){
	 const char *attr=(const char *)arg;
	ms_message("MSBV16Enc: enc_add_attr %s", attr);

	 if (strstr(attr,"ptime:")!=NULL){
	 	int ptime = atoi(attr+6);
	 	return enc_set_ptime(f,ptime);
	 }
	return 0;
}

static int enc_get_sample_rate(MSFilter *f, void *arg) {
	MS_UNUSED(f);
	*((int *)arg) = 8000;
	return 0;
}

static int get_channels(MSFilter *f, void *arg) {
	*((int *)arg) = 1;
	return 0;
}
static void enc_init(MSFilter *f){
	EncState *s=(EncState *)ms_new0(EncState,1);
	Reset_BV16_Encoder(&(s->state));
	s->nsamples = FRSZ ;
	s->size_samples =  FRSZ * sizeof(short);
	s->ptime=10;
	s->max_ptime = 100;
	s->bufferizer=ms_bufferizer_new();
	f->data=s;
	
	ms_message("MSBV16Enc Init ");
}

static void enc_uninit(MSFilter *f){
	EncState *s=(EncState*)f->data;
	ms_bufferizer_destroy(s->bufferizer);
	ms_free(s);
	ms_message("MSBV16Enc Uninit ");
}

static void enc_process (MSFilter *f){
	EncState *s=(EncState*)f->data;
	struct	BV16_Bit_Stream bs;
	short *buf= NULL;
	mblk_t *inputMessage = NULL, *outputMessage = NULL;
	int frame_per_packet=s->ptime/5;
	int in_rcvd_bytes = 0;
	

	
	in_rcvd_bytes = SIGNAL_FRAME_SIZE * frame_per_packet;
	buf=(short*)alloca(in_rcvd_bytes);
	memset((void*)buf,0, in_rcvd_bytes );
	
	while((inputMessage=ms_queue_get(f->inputs[0]))!=NULL){
		ms_bufferizer_put(s->bufferizer,inputMessage);
		
	}
//	ms_message("MSBV16Enc in_rcvd_bytes  %d  frame_per_packet %d", in_rcvd_bytes, frame_per_packet );
	/* process ptimes ms of data : (ptime in ms)/1000->ptime is seconds * 8000(sample rate) * 2(byte per sample) */
	while(ms_bufferizer_get_avail(s->bufferizer)>= in_rcvd_bytes){
		outputMessage = allocb(BITSTREAM_FRAME_SIZE*frame_per_packet,0); /* output bitStream is 80 bits long * number of samples */
		/* process buffer in 5 ms frames but read everything first*/
		ms_bufferizer_read(s->bufferizer,(uint8_t*)buf,in_rcvd_bytes);
		
		for (int bufferIndex=0; bufferIndex<frame_per_packet; bufferIndex++) {
			
			BV16_Encode(&bs, &s->state, (short*)&buf[bufferIndex*FRSZ]);
			BV16_BitPack( (UWord8*)outputMessage->b_wptr, &bs );
			outputMessage->b_wptr+=BITSTREAM_FRAME_SIZE;

		}

		mblk_set_timestamp_info(outputMessage,s->ts);
		ms_bufferizer_fill_current_metas(s->bufferizer, outputMessage);
		ms_queue_put(f->outputs[0],outputMessage);
		s->ts +=  FRSZ * frame_per_packet;

		}

}

// /***
// Encodes 8 kHz-sampled narrowband speech at a bit rate of or 16 kbit/s,
//  uses 5 ms frames.
//  ***/
// static void enc_process(MSFilter *f){
// 	EncState *s=(EncState*)f->data;
// 	struct	BV16_Bit_Stream bs;
// 	//int in_single_frsz_bytes = (sizeof(short) ) * FRSZ ;
// 	int in_rcvd_bytes;
// 	int out_single_frsz_bytes = 10;
// 	int ret ;
// 	short *buf;
// 	mblk_t *inputMessage = NULL, *outputMessage;
// 	unsigned int offset = 0;
// 	int frame_per_packet=2;

// 	/*if (s->frame_size<=0)
// 		return;*/

// 	ms_filter_lock(f);

// 	if (s->ptime>=10)
// 	{
// 		frame_per_packet = s->ptime/5;
// 	}
// 	in_rcvd_bytes = SIGNAL_FRAME_SIZE * frame_per_packet;
// 	buf=(short*)alloca(in_rcvd_bytes);
// 	memset((void*)buf,0, in_rcvd_bytes );
// 	//ms_message("MSBV16Enc timestamp %d frame_per_packet %d ptime %d", s->ts,frame_per_packet, s->ptime );
// 	s->frame_size = 0;
// 	while((inputMessage=ms_queue_get(f->inputs[0]))!=NULL){
		
// 		ms_bufferizer_put(s->bufferizer,inputMessage);
// 	}
	
// 	while(ms_bufferizer_get_avail(s->bufferizer) >= in_rcvd_bytes ) {
// 		outputMessage=allocb(BITSTREAM_FRAME_SIZE*frame_per_packet,0);
// 		ret = ms_bufferizer_read(s->bufferizer,(uint8_t*)buf,in_rcvd_bytes);
// 		for (offset=0;offset<frame_per_packet;offset++) {
// 			//ms_message("MSBV16Enc offset %d in_rcvd_bytes %d  in_single_frsz_bytes %d ", offset, in_rcvd_bytes, in_single_frsz_bytes);

// 			BV16_Encode(&bs, &s->state, (short*)&buf[offset*FRSZ]);
// 			BV16_BitPack( (UWord8*)outputMessage->b_wptr, &bs );
// 			outputMessage->b_wptr+=out_single_frsz_bytes;
			
// 		}
// 		// //s->ts+=s->nsamples*frame_per_packet;
// 		// ms_bufferizer_fill_current_metas(s->bufferizer,outputMessage);
// 		// ms_message("MSBV16Enc timestamp %d frame_size %d", s->ts, s->frame_size );

// 		// mblk_set_timestamp_info(outputMessage,s->ts);
// 		// s->ts +=  FRSZ * frame_per_packet;
// 		// //s->ts += out_single_frsz_bytes * frame_per_packet/8000;
// 		// s->frame_size += out_single_frsz_bytes * frame_per_packet;
		
// 		// ms_queue_put(f->outputs[0],outputMessage);
		
// 		mblk_set_timestamp_info(outputMessage,s->ts);
// 		ms_bufferizer_fill_current_metas(s->bufferizer,outputMessage);
// 		s->ts +=  FRSZ * frame_per_packet;

// 		//s->ts += out_single_frsz_bytes * frame_per_packet/8000;
// //		s->frame_size += out_single_frsz_bytes * frame_per_packet;
// //		ms_message("MSBV16Enc timestamp %d frame_size %d", s->ts, s->frame_size );
// 		ms_queue_put(f->outputs[0],outputMessage);
// 	}

		
// 	ms_filter_unlock(f);


// }



static MSFilterMethod enc_methods[]={
	{MS_FILTER_ADD_FMTP			,enc_add_fmtp},
	{MS_FILTER_ADD_ATTR        	,enc_add_attr},
	{MS_FILTER_GET_SAMPLE_RATE	,enc_get_sample_rate },
	{MS_FILTER_GET_NCHANNELS		,	get_channels},
	{MS_AUDIO_ENCODER_SET_PTIME, enc_set_ptime},
	{	0				,	NULL		}
};

#ifdef _MSC_VER

MSFilterDesc ms_bv16_enc_desc={
	MS_BV16_ENC_ID,
	"MSBv16Enc",
	N_("The BV16 full-rate codec"),
	MS_FILTER_ENCODER,
	"bv16",
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

MSFilterDesc ms_bv16_enc_desc={
	.id=MS_BV16_ENC_ID,
	.name="MSBv16Enc",
	.text=N_("The BV16 full-rate codec"),
	.category=MS_FILTER_ENCODER,
	.enc_fmt="bv16",
	.ninputs=1,
	.noutputs=1,
	.init=enc_init,
	.process=enc_process,
	.uninit=enc_uninit,
	.methods = enc_methods
};

#endif

typedef struct DecState{
	struct BV16_Decoder_State state;
	uint32_t ts; // timestamp
	uint64_t last_sample_size;
	bool_t plc;
	int plc_count;
	MSConcealerContext *concealer;
} DecState;


//static const int plc_max=40;/*40 frames of plc, no more*/
static void dec_init(MSFilter *f){
	DecState *s=ms_new0(DecState,1);
	Reset_BV16_Decoder(&s->state);
	f->data = s;
	s->last_sample_size = 0;
	s->plc=1;
	s->ts = 0;
	s->plc_count=0;
	
	ms_message("MSBV16Dec Init ");
	
}
static void dec_preprocess(MSFilter* f){
	DecState *s = (DecState*)f->data;
	s->concealer = ms_concealer_context_new(UINT32_MAX);

}

static void dec_postprocess(MSFilter* f ){
	DecState *s = (DecState*)f->data;
	ms_concealer_context_destroy(s->concealer);
	

}

static void dec_uninit(MSFilter *f){
	DecState *s = (DecState*)f->data;
	Reset_BV16_Decoder((struct BV16_Decoder_State *)f->data);

	ms_free(s);

	ms_message("MSBV16Dec Uninit ");
}



static void dec_process(MSFilter *f){

	DecState *s=(DecState*)f->data;

	mblk_t *inputMessage, *outputMessage;
	// int input_bytes = 0;
	// int frames_rcvd = 0;
	int nb_frames_lost = 0;
	struct	BV16_Bit_Stream bs;
	// int k ;
	
	while((inputMessage=ms_queue_get(f->inputs[0]))!=NULL){
		
		s->last_sample_size = msgdsize(inputMessage);
//		ms_message("MSBV16Dec : input msg size %llu time %d" , s->last_sample_size, 5);
		while(inputMessage->b_rptr<inputMessage->b_wptr) {
			outputMessage = allocb(SIGNAL_FRAME_SIZE,0);
			mblk_meta_copy(inputMessage, outputMessage);
			BV16_BitUnPack((UWord8*)inputMessage->b_rptr, &bs);
			BV16_Decode(&bs, &s->state, (short*)(outputMessage->b_wptr));
			outputMessage->b_wptr+=SIGNAL_FRAME_SIZE;
			inputMessage->b_rptr+=BITSTREAM_FRAME_SIZE;
			ms_queue_put(f->outputs[0],outputMessage);
			
			ms_concealer_inc_sample_time(s->concealer,f->ticker->time, 5, 1);
			
		}
		freemsg(inputMessage);
	//	ms_message("MSBV16Dec : out \r\n");
	}
	if (ms_concealer_context_is_concealement_required(s->concealer, f->ticker->time)) {

		nb_frames_lost = s->last_sample_size / BITSTREAM_FRAME_SIZE;
		ms_message("MSBV16Dec : CONCEAL %d frames" ,nb_frames_lost);

		for (int i=0; i<2; i++){
			outputMessage = allocb(SIGNAL_FRAME_SIZE,0);
			BV16_PLC(&s->state,(short*)outputMessage->b_wptr);
			outputMessage->b_wptr+=SIGNAL_FRAME_SIZE;
			mblk_set_plc_flag(outputMessage, 1);
			ms_queue_put(f->outputs[0],outputMessage);
		}
		ms_concealer_inc_sample_time(s->concealer,f->ticker->time,10, 0);
	}
}

		

static int dec_have_plc(MSFilter *f, void *arg)
{
	*((int *)arg) = 1;
	return 0;
}

static int dec_get_sample_rate(MSFilter *f, void *arg) {
	MS_UNUSED(f);
	*((int *)arg) = 8000;
	return 0;
}

static int dec_add_fmtp(MSFilter *f, void *arg) {
	DecState *s=(DecState*)f->data;
	const char *fmtp = (const char *)arg;
	char buf[32];
	
	memset(buf, '\0', sizeof(buf));
	if (fmtp_get_value(fmtp, "plc", buf, sizeof(buf))) {
		s->plc = atoi(buf);
	}
	return 0;
}

static MSFilterMethod dec_methods[] = {
	{ MS_FILTER_GET_SAMPLE_RATE, 	dec_get_sample_rate },
	{ MS_FILTER_GET_NCHANNELS, 		get_channels },
	{	MS_FILTER_ADD_FMTP		,	dec_add_fmtp	},
	{ MS_DECODER_HAVE_PLC		, 	dec_have_plc	},
	{ 0,                         NULL                }
};

#ifdef _MSC_VER

MSFilterDesc ms_bv16_dec_desc={
	MS_BV16_DEC_ID,
	"MSBv16Dec",
	N_("The BV16 codec"),
	MS_FILTER_DECODER,
	"bv16",
	1,
	1,
	dec_init,
	dec_preprocess,
	dec_process,
	dec_postprocess,
	dec_uninit,
	dec_methods
};

#else

MSFilterDesc ms_bv16_dec_desc={
	.id=MS_BV16_DEC_ID,
	.name="MSBv16Dec",
	.text=N_("The BV16 codec"),
	.category=MS_FILTER_DECODER,
	.enc_fmt="bv16",
	.ninputs=1,
	.noutputs=1,
	.init=dec_init,
	.preprocess=dec_preprocess,
	.process=dec_process,
	.postprocess =dec_postprocess,
	.uninit=dec_uninit,
	.methods=dec_methods
};

#endif

MS_FILTER_DESC_EXPORT(ms_bv16_dec_desc)
MS_FILTER_DESC_EXPORT(ms_bv16_enc_desc)

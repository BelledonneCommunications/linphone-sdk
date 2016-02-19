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

#define NO_OF_BYTES_PER_5MS     10

typedef struct EncState{
	struct BV16_Encoder_State state;
	uint32_t ts; // timestamp
	int ptime;
	int maxptime;
	int nsamples;
	int frame_size;
	int size_samples;
	MSBufferizer *bufferizer;
} EncState;

static int set_ptime(MSFilter *f, int ptime){
	 EncState *s=(EncState*)f->data;
	 if (ptime<5 || ptime>140) return -1;
	 s->ptime = ptime;
	 ms_message("MSBV16Enc: got ptime=%i using [%i]",ptime,s->ptime);
	return 0;
}

static int enc_add_fmtp(MSFilter *f, void *arg){
	 const char *fmtp=(const char *)arg;
	 char tmp[30];
	 if (fmtp_get_value(fmtp,"ptime",tmp,sizeof(tmp))){
	 	return set_ptime(f,atoi(tmp));
	 }
	return 0;
}

static int enc_add_attr(MSFilter *f, void *arg){
	 const char *attr=(const char *)arg;
	ms_message("MSBV16Enc: enc_add_attr %s", attr);

	 if (strstr(attr,"ptime:")!=NULL){
	 	int ptime = atoi(attr+6);
	 	return set_ptime(f,ptime);
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
	EncState *s=(EncState *)ms_new(EncState,1);
	Reset_BV16_Encoder(&(s->state));
	s->ts=0;
	s->nsamples = FRSZ ;
	s->size_samples =  FRSZ * sizeof(short);
	s->ptime=0;
	s->maxptime = 0;
	s->bufferizer=ms_bufferizer_new();
	s->frame_size = 0;
	f->data=s;
	
	ms_message("MSBV16Enc Init ");
}

static void enc_uninit(MSFilter *f){
	EncState *s=(EncState*)f->data;
	ms_bufferizer_destroy(s->bufferizer);
	ms_free(s);
	ms_message("MSBV16Enc Uninit ");
}

static void enc_process(MSFilter *f){
	//const int 	in_max_sample_size = FRSZ*28;
//	short  	in_full_buff[80]={0};//160
	uint8_t  in_full_buff[160]={0};//160
	//short* p_in_full_buff = (short*)&in_full_buff[0];
//		short   	in_full_buff[in_max_sample_size] = {0};
//	short 		in_unitary_buff[FRSZ];
	
	EncState *s=(EncState*)f->data;
	struct	BV16_Bit_Stream bs;
//	int size=s->nsamples;
	int in_single_frsz_bytes = (sizeof(short) ) * FRSZ ;
//	int in_single_frsz_bytes = (sizeof(short) ) * FRSZ ;

	int in_rcvd_bytes;
	int out_single_frsz_bytes = 10;
	int ret ;


	mblk_t *im = NULL, *om;
	unsigned int offset = 0;
	int frame_per_packet=2;

	//for (i=0;i<max_size_for_ptime;i++) x[i] = 0;

	/*if (s->frame_size<=0)
		return;*/

	ms_filter_lock(f);

	if (s->ptime>=5)
	{
		frame_per_packet = s->ptime/5;
	}

	if (frame_per_packet<=0)
		frame_per_packet=1;
	if (frame_per_packet>28) /* 28*5 == 140 ms max */
		frame_per_packet=28;
	
	in_rcvd_bytes = in_single_frsz_bytes * frame_per_packet;
	
	

	while((im=ms_queue_get(f->inputs[0]))!=NULL){
	//	ms_message("MSBV16Enc size im %d ", (int)msgdsize(im));
		ms_bufferizer_put(s->bufferizer,im);
	}
	
	while(ms_bufferizer_get_avail(s->bufferizer) >=  in_rcvd_bytes) {
	
		//ms_message("MSBV16Enc frame_per_packet %d ptime %d in_rcvd_bytes %d ", frame_per_packet, s->ptime, in_rcvd_bytes);


		//in_full_buff = (short *)alloca(in_rcvd_bytes);

		om=allocb(out_single_frsz_bytes*frame_per_packet,0);
		
		ret = ms_bufferizer_read(s->bufferizer,in_full_buff,in_rcvd_bytes);
		
		for (offset=0;offset<=in_rcvd_bytes/2;offset+=in_single_frsz_bytes) {
			//ms_message("MSBV16Enc offset %d in_rcvd_bytes %d  in_single_frsz_bytes %d ", offset, in_rcvd_bytes, in_single_frsz_bytes);
			BV16_Encode(&bs, &s->state, (short*)&in_full_buff[offset/sizeof(short)]);
			//BV16_Encode(&bs, &s->state, &in_full_buff[offset]);
			BV16_BitPack( (UWord8*)om->b_wptr, &bs );
			om->b_wptr+=out_single_frsz_bytes;
			
		}
		//s->ts+=s->nsamples*frame_per_packet;
		ms_bufferizer_fill_current_metas(s->bufferizer,om);
		//ms_message("MSBV16Enc timestamp %d frame_size %d", s->ts, s->frame_size );

		mblk_set_timestamp_info(om,s->ts);
		//s->ts += 5 * frame_per_packet;
		s->ts += out_single_frsz_bytes * frame_per_packet;
		s->frame_size += out_single_frsz_bytes * frame_per_packet;
		
		ms_queue_put(f->outputs[0],om);
		
	}

		
	ms_filter_unlock(f);


}

// static void enc_process(MSFilter *f){
// 	//const int max_size_for_ptime = FRSZ*28;
// //	short    full_buff[max_size_for_ptime];
// 	short unitary_buff[FRSZ];
// 	EncState *s=(EncState*)f->data;
// 	struct	BV16_Bit_Stream bs;
// //	int size=s->nsamples;
// 	int size = (sizeof(short) ) * FRSZ / 8;
// 	int ret ; 
// 	//UWord8 PackedStream[10];

// 	mblk_t *im, *om;

// 	int nbytes;
// //	int i;
// 	int frame_per_packet=1;

// 	//for (i=0;i<max_size_for_ptime;i++) x[i] = 0;

// 	/*if (s->frame_size<=0)
// 		return;*/

// 	ms_filter_lock(f);

// 	if (s->ptime>=5)
// 	{
// 		frame_per_packet = s->ptime/5;
// 	}

// 	if (frame_per_packet<=0)
// 		frame_per_packet=1;
// 	if (frame_per_packet>28) /* 28*5 == 140 ms max */
// 		frame_per_packet=28;

// 	/* 10 bytes per frame */
// 	nbytes=10;

// 	while((im=ms_queue_get(f->inputs[0]))!=NULL){
// 		ms_bufferizer_put(s->bufferizer,im);
// 	}
// 	while(ms_bufferizer_get_avail(s->bufferizer) >= size) {
// 	//while(ms_bufferizer_read(s->bufferizer,(uint8_t*)x,size*frame_per_packet)==(size*frame_per_packet)){
// 	//	int k;
// 		om=allocb(nbytes,0);
// 		ret = ms_bufferizer_read(s->bufferizer,(uint8_t*)unitary_buff,size);
// 	//	om=allocb(nbytes*frame_per_packet,0);
// 		// for (k=0;k<frame_per_packet;k++)
// 		// {
// 		// 	for (i=0;i<s->nsamples;i++){
// 		// 		full_buff[i+(s->nsamples*k)]=unitary_buff[i];
// 		// 	}
// 			BV16_Encode(&bs, &s->state, unitary_buff);
// 			BV16_BitPack( (UWord8*)om->b_wptr, &bs );
// 			om->b_wptr+=nbytes;
// 		// }
// 		//s->ts+=s->nsamples*frame_per_packet;
// 		ms_bufferizer_fill_current_metas(s->bufferizer,om);

// 		mblk_set_timestamp_info(om,s->ts);
// 		s->ts += 5; //timestamp + 5ms
// 		s->frame_size += nbytes;
		
// 		ms_queue_put(f->outputs[0],om);
// 	}

		
// 	ms_filter_unlock(f);


// }


static MSFilterMethod enc_methods[]={
	{MS_FILTER_ADD_FMTP			,enc_add_fmtp},
	{MS_FILTER_ADD_ATTR        	,enc_add_attr},
	{MS_FILTER_GET_SAMPLE_RATE	,enc_get_sample_rate },
	{MS_FILTER_GET_NCHANNELS		,	get_channels},
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
	uint64_t sample_time;
	bool_t plc;
	int plc_count;
	// MSBufferizer *bufferizer;
} DecState;


//static const int plc_max=10;/*10 frames of plc, no more*/
static void dec_init(MSFilter *f){
	DecState *s=ms_new0(DecState,1);
	Reset_BV16_Decoder(&s->state);
	f->data = s;
	s->sample_time = 0;
	s->ts = 0;
	s->plc=1;
	s->plc_count=0;
	ms_message("MSBV16Dec Init ");
	
}

static void dec_uninit(MSFilter *f){
	DecState *s = (DecState*)f->data;
	Reset_BV16_Decoder((struct BV16_Decoder_State *)f->data);
	ms_free(s);
	ms_message("MSBV16Dec Uninit ");
}

//static void dec_process(MSFilter *f){
//	
//	DecState *s=(DecState*)f->data;
//	struct	BV16_Bit_Stream bs;
//	int frsz = ((sizeof(short)) * FRSZ )  ; //size in bytes
//
//	mblk_t *m;
//	while((m=ms_queue_get(f->inputs[0]))!=NULL){
//	
//		mblk_t *o;
//		msgpullup(m,-1);
//		ms_message("MSBV16Dec  %d" , (int)msgdsize(m));
//		for(;m->b_rptr<m->b_wptr;m->b_rptr++,o->b_wptr+=10){
//			o=allocb((sizeof(UWord8))*10,0);
//			mblk_meta_copy(m, o);
//
//			BV16_BitUnPack(m->b_rptr, &bs);
//			BV16_Decode(&bs, &s->state, (short*)o->b_wptr);
//			o->b_wptr += frsz ;
//
//		}
//	freemsg(m);
//	ms_queue_put(f->outputs[0],o);
//
//	}
//}

static void dec_process(MSFilter *f){

	DecState *s=(DecState*)f->data;

	int k;
	mblk_t *im, *om;
	//int nbytes;
	int frsz;
	struct	BV16_Bit_Stream bs;
	int rem_bytes, nbytes;
	frsz = ((sizeof(short)) * FRSZ ) ; //size in bytes

	int frame_per_packet = 0  ;
	
	while((im=ms_queue_get(f->inputs[0]))!=NULL){
		nbytes=msgdsize(im);
		//ms_message("MSBV16Dec size recvd %d  ", nbytes);
		rem_bytes = ((im->b_wptr-im->b_rptr));

		if (nbytes==0  || nbytes%NO_OF_BYTES_PER_5MS!=0 ){
//			om=allocb(frsz ,0);
//			mblk_meta_copy(im,om);
//			om->b_wptr += frsz ;
			ms_message("bv16 packet loss ? rem_bytes %d  ", rem_bytes);
//			BV16_PLC(&s->state,(short*)om->b_wptr);
//			mblk_set_plc_flag(om, 1);
//			ms_queue_put(f->outputs[0],om);
//			
//			s->sample_time+=5;
//			s->plc_count++;
//			if (s->plc_count>=plc_max){
//				s->sample_time=0;
//			}
			freemsg(im);
			continue;
		}
		if (nbytes%NO_OF_BYTES_PER_5MS ==0){
			frame_per_packet = nbytes/NO_OF_BYTES_PER_5MS;
		//	ms_message("MSBV16Dec  frame_per_packet %d frsz %d ", frame_per_packet,frsz);
			for (k=0;k<frame_per_packet;k++,im->b_rptr+=k*frsz ) {
				om=allocb(frsz ,0);
				mblk_meta_copy(im,om);

				BV16_BitUnPack(im->b_rptr, &bs);
				BV16_Decode(&bs, &s->state, (short*)om->b_wptr);   
				om->b_wptr += frsz ;
				
				ms_queue_put(f->outputs[0],om);
			}
		
			
		}//while(rem_bytes >= NO_OF_BYTES_PER_5MS);
		freemsg(im);
	}

// static void dec_process(MSFilter *f){

// 	DecState *s=(DecState*)f->data;


// 	mblk_t *im, *om;
// 	//int nbytes;
// 	int frsz;
// 	struct	BV16_Bit_Stream bs;
// 	int rem_bytes, nbytes;
// 	frsz = ((sizeof(short)) * FRSZ ) / 8 ; //size in bytes
// 	int frame_per_packet = 0  ;
// 	while((im=ms_queue_get(f->inputs[0]))!=NULL){
// 		//do{
// 		nbytes=msgdsize(im);,
// 		rem_bytes = ((im->b_wptr-im->b_rptr));
// 			// nbytes=msgdsize(im);
// 		// if (nbytes==0  || nbytes%NO_OF_BYTES_PER_5MS!=0 ){
// 		// 	freemsg(im);
// 		// 	continue;
// 		// }
// 		// if (nbytes%NO_OF_BYTES_PER_5MS ==0){
// 		// 	frame_per_packet = nbytes/NO_OF_BYTES_PER_5MS;
		
// 		// 	for (k=0;k<frame_per_packet;k++) {
// 		// 		om=allocb(frsz ,0);
// 		// 		BV16_BitUnPack(im->b_rptr+k*frsz, &bs);
// 		// 		BV16_Decode(&bs, &s->state, (short*)om->b_wptr);   
// 		// 		om->b_wptr += frsz * 8;
// 		// 		mblk_meta_copy(im,om);
// 		// 		ms_queue_put(f->outputs[0],om);
// 		// 	}
// 		while(rem_bytes >= NO_OF_BYTES_PER_5MS){
// 			om=allocb(frsz ,0);			
// 			mblk_meta_copy(im,om);
// 			om->b_wptr += frsz ;
// 			if ( rem_bytes  < NO_OF_BYTES_PER_5MS){
// 				ms_message("bv16 packet loss ? rem_bytes %d  ", rem_bytes);
// 				BV16_PLC(&s->state,(short*)om->b_wptr);
// 				mblk_set_plc_flag(om, 1);
// 				ms_queue_put(f->outputs[0],om);
				
// 				s->sample_time+=5;
// 				s->plc_count++;
// 				if (s->plc_count>=plc_max){
// 					s->sample_time=0;
// 				}

// 			//	freemsg(om);
// 				// BV16_PLC(&s->state,(short*)om->b_wptr);
// 				// mblk_set_plc_flag(om, 1);
// 			}
			
// 			else{
// 				BV16_BitUnPack(im->b_rptr+frame_per_packet*frsz, &bs);
// 				BV16_Decode(&bs, &s->state, (short*)om->b_wptr); 
// 				ms_queue_put(f->outputs[0],om); 
// 				im->b_rptr += NO_OF_BYTES_PER_5MS;
// 				if (s->sample_time==0) s->sample_time=f->ticker->time;
// 				s->sample_time+=5;
// 				frame_per_packet ++;
// 				if (s->plc_count>0){
// 					s->plc_count=0;
// 				}
// 			}
// 			rem_bytes = ((im->b_wptr-im->b_rptr));
// 		}//while(rem_bytes >= NO_OF_BYTES_PER_5MS);
// 		freemsg(im);
// 	}
//	if (s->plc && s->sample_time!=0 && f->ticker->time>=s->sample_time){
//		/* we should output a frame but no packet were decoded
//		 thus do packet loss concealment*/
//		om=allocb(frsz,0);
//		BV16_PLC(&s->state,(short*)om->b_wptr);
//		om->b_wptr+=frsz;
//		mblk_set_plc_flag(om, 1);
//		ms_queue_put(f->outputs[0],om);
//
//		s->sample_time+=5;
//		s->plc_count++;
//		if (s->plc_count>=plc_max){
//			s->sample_time=0;
//		}
	//}

		// nbytes=msgdsize(im);
		// if (nbytes==0  || nbytes%NO_OF_BYTES_PER_5MS!=0 ){
		// 	freemsg(im);
		// 	continue;
		// }
		// if (nbytes%NO_OF_BYTES_PER_5MS ==0){
		// 	frame_per_packet = nbytes/NO_OF_BYTES_PER_5MS;
		
		// 	for (k=0;k<frame_per_packet;k++) {
		// 		om=allocb(frsz ,0);
		// 		BV16_BitUnPack(im->b_rptr+k*frsz, &bs);
		// 		BV16_Decode(&bs, &s->state, (short*)om->b_wptr);   
		// 		om->b_wptr += frsz * 8;
		// 		mblk_meta_copy(im,om);
		// 		ms_queue_put(f->outputs[0],om);
		// 	}
		// 	if 
			
		// }
		// freemsg(im);
	//}
	
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

static MSFilterMethod dec_methods[] = {
	{ MS_FILTER_GET_SAMPLE_RATE, dec_get_sample_rate },
	{ 	MS_DECODER_HAVE_PLC		, 	dec_have_plc	},
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
	NULL,
	dec_process,
	NULL,
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
	.process=dec_process,
	.uninit=dec_uninit,
	.methods=dec_methods
};

#endif

MS_FILTER_DESC_EXPORT(ms_bv16_dec_desc)
MS_FILTER_DESC_EXPORT(ms_bv16_enc_desc)

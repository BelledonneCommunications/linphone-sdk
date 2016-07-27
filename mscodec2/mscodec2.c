/*
 codec2_enc.c
 Copyright (C) 2015 Belledonne Communications, Grenoble, France

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

 Authors : Dragos Oancea @ Orange Vallee
           Johan Pascal
 */

#include "codec2/codec2.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/mscodecutils.h"

#define CODEC2_MAX_BYTES_PER_FRAME 250 // soundcard 
#define CODEC2_MAX_INPUT_FRAMES 5  // soundcard

#define MSCODEC2_VERSION "0.1"

//#define CODEC2_DEBUG 1
#undef CODEC2_DEBUG

/******************************************************************************/
/****                           Encoder part                               ****/
/******************************************************************************/
/*filter common method*/
struct codec2_enc_struct {
/* codec2 related */
	unsigned char ptime;
	unsigned int max_network_bitrate;
	int network_bitrate;
	int            mode;
	void          *codec2;
	int            nsam;
	int nbit;
	int nbyte;
	int gray;
	int sample_rate; 
	int packet_size; /* in samples */
	int bit_rate ;
/* mediastreamer related */
	uint32_t ts;
	MSBufferizer *bufferizer;
};



static void enc_filter_init ( MSFilter *f ) {
	struct codec2_enc_struct* obj;
	int setmode=3200;
	f->data = ms_new0 ( struct codec2_enc_struct,1 );
	obj = ( struct codec2_enc_struct* ) f->data;
	if (setmode==3200) 
		obj->mode = CODEC2_MODE_3200;
	else if (setmode==2400) 
		obj->mode = CODEC2_MODE_2400;
	else if (setmode==1600)
		obj->mode = CODEC2_MODE_3200; /* 1600 not supported*/
	else if (setmode==1400) 
		obj->mode = CODEC2_MODE_3200; /* 1400 not supported*/
	else if (setmode==1300)
		obj->mode = CODEC2_MODE_3200; /* 1300 not supported*/
	else if (setmode==1200) 
		obj->mode = CODEC2_MODE_3200; /* 1200 not supported*/
	else if (setmode==450)
		obj->mode = CODEC2_MODE_3200; /* 450 not supported*/
	else {
		ms_warning("codec2: Error in mode: %d.  Must be 3200, 2400, 1600, 1400, 1300, 1200, or 450, forcing CODEC2_MODE_3200\n", setmode);
	}
	obj->ptime=20;  // CODEC2_MODE_3200 has 20 ms ptime
	obj->bufferizer=ms_bufferizer_new();
	obj->codec2 = codec2_create(obj->mode);
	obj->nsam = codec2_samples_per_frame(obj->codec2);
	obj->nbit = codec2_bits_per_frame(obj->codec2);
	obj->nbyte = (obj->nbit + 7) / 8;
	obj->gray=1;
	obj->sample_rate=8000; /*always 8000 for CODEC2 */
#ifdef CODEC2_DEBUG 
	ms_message("codec2_encoder: filter initialized"); 
	ms_message("codec2_encoder: enc obj->nsam: %d",obj->nsam);
	ms_message("codec2_encoder: enc obj->nbit: %d",obj->nbit);
	ms_message("codec2_encoder: enc obj->nbyte: %d",obj->nbyte);
#endif
}

static void enc_filter_preprocess ( MSFilter *f ) {

}

static void enc_filter_process ( MSFilter *f ) {
	mblk_t *im;
	mblk_t *om=NULL;
	uint8_t * buff=NULL;
	struct codec2_enc_struct* obj= ( struct codec2_enc_struct* ) f->data;
	obj->packet_size = obj->sample_rate*obj->ptime/1000; /*in sample*/

	while ( ( im=ms_queue_get ( f->inputs[0] ) ) !=NULL ) {
		ms_bufferizer_put ( obj->bufferizer,im );
	}

	while ( ms_bufferizer_get_avail ( obj->bufferizer ) >=obj->packet_size*2 ) {
		om = allocb ( obj->nbyte,0 );
		if ( !buff ) {
			buff=ms_malloc ( obj->packet_size*2 );
		}

		ms_bufferizer_read ( obj->bufferizer,buff,obj->packet_size*2 );
		codec2_encode(obj->codec2,om->b_wptr,(short int *)buff);
		obj->ts+=obj->packet_size;
		om->b_wptr+=obj->nbyte;
		mblk_set_timestamp_info ( om,obj->ts );
		ms_queue_put ( f->outputs[0],om );
		om=NULL;
	}

	if ( buff!=NULL ) {
		ms_free(buff);
	}
}

static void enc_filter_postprocess ( MSFilter *f ) {
}

static void enc_filter_uninit ( MSFilter *f ) {
	struct codec2_enc_struct* obj= ( struct codec2_enc_struct* ) f->data;
	ms_bufferizer_destroy ( obj->bufferizer );
	codec2_destroy(obj->codec2);
	ms_free ( f->data );
}

static int enc_filter_get_sample_rate ( MSFilter *f, void *arg ) {
	struct codec2_enc_struct* obj= ( struct codec2_enc_struct* ) f->data;
	* ( int* ) arg = obj->sample_rate;
	return 0;
}

static int enc_filter_set_bitrate ( MSFilter *f, void *arg ) {
	struct codec2_enc_struct* obj= ( struct codec2_enc_struct* ) f->data;
	int inital_cbr=0;
	int normalized_cbr=0;
	float pps=1000.0f/obj->ptime;
	unsigned int network_bitrate=* ( int* ) arg;
	normalized_cbr=inital_cbr= ( int ) ( ( ( ( ( float ) network_bitrate ) / ( pps*8 ) )-20-12-8 ) *pps*8 );
	if ( normalized_cbr!=inital_cbr ) {
		ms_warning ( "Codec2 enc unsupported codec bitrate [%i], normalizing",inital_cbr );
	}
	obj->bit_rate=normalized_cbr;
	obj->max_network_bitrate=(unsigned int)(((float)normalized_cbr/(pps*8) +20+12+8)*pps*8);
	ms_message("codec2 : set bit rate to %d : bit_rate %d max_network %d\n", network_bitrate, obj->bit_rate, obj->max_network_bitrate);
	return 0;
}

static int enc_filter_get_bitrate ( MSFilter *f, void *arg ) {
	struct codec2_enc_struct* obj= ( struct codec2_enc_struct* ) f->data;
	* ( int* ) arg=obj->max_network_bitrate;
	ms_message("codec2 : get bit rate : %d\n", obj->max_network_bitrate);
	return 0;
}


static MSFilterMethod enc_filter_methods[]= {
	{	MS_FILTER_GET_SAMPLE_RATE , enc_filter_get_sample_rate },
	{	MS_FILTER_SET_BITRATE		,	enc_filter_set_bitrate	},
	{	MS_FILTER_GET_BITRATE		,	enc_filter_get_bitrate	},
	{	0, NULL}
};


#ifdef _MSC_VER

MSFilterDesc ms_codec2_enc_desc= {
	MS_FILTER_PLUGIN_ID, /* from Allfilters.h*/
	"MSCodec2Enc",
	"CODEC2 audio encoder filter.",
	MS_FILTER_ENCODER,
	"CODEC2",
	1, /*number of inputs*/
	1, /*number of outputs*/
	enc_filter_init,
	enc_filter_preprocess,
	enc_filter_process,
	enc_filter_postprocess,
	enc_filter_uninit,
	enc_filter_methods,
	0
};

#else

MSFilterDesc ms_codec2_enc_desc= {
	.id=MS_FILTER_PLUGIN_ID, /* from Allfilters.h*/
	.name="MSCodec2Enc",
	.text="Codec2 audio encoder filter.",
	.category=MS_FILTER_ENCODER,
	.enc_fmt="CODEC2",
	.ninputs=1, /*number of inputs*/
	.noutputs=1, /*number of outputs*/
	.init=enc_filter_init,
	.preprocess=enc_filter_preprocess,
	.process=enc_filter_process,
	.postprocess=enc_filter_postprocess,
	.uninit=enc_filter_uninit,
	.methods=enc_filter_methods
};

#endif

MS_FILTER_DESC_EXPORT ( ms_codec2_enc_desc )


/******************************************************************************/
/****                           Decoder part                               ****/
/******************************************************************************/


/*filter common method*/
struct codec2_dec_struct {
/*codec2 */
	void *codec2 ;
	int nsam ; /* number of samples */
	int nbit ; /*number of bits per sample*/
	int nbyte;  
	int byte; 
	int bits_proc;
	int nstart_bit;
	int nend_bit; 
	int bit_rate;
	int state; 
	int next_state;
	float ber;
	float r;  
	float ber_est; // dr: BER mode not supported yet
	unsigned char  mask;
	int natural;
	int dump;
	unsigned int frames ; 
	int mode ;
	int sample_rate;
/*mediastreamer*/
	unsigned  short int sequence_number;
};

static void dec_filter_init(MSFilter *f){
	f->data = ms_new0(struct codec2_dec_struct,1);
}

static void dec_filter_preprocess(MSFilter *f){
	struct codec2_dec_struct* obj= (struct codec2_dec_struct*) f->data;
	int setmode=3200; /* wikipedia:  Mode 3200, has 20 ms of audio converted to 64 Bits. 
						 So 64 Bits will be output every 20 ms (50 times a second), for a minimum data rate of 3200 bits/sec. 
						 These 64 bits are sent as 8 bytes to the application, which has to unwrap the bit-fields, 
						 or send the bytes on a data channel.  */
	if (setmode==3200) 
		obj->mode = CODEC2_MODE_3200;  /* CODEC2_MODE_3200 has 20 ms ptime */
	else if (setmode==2400) 
		obj->mode = CODEC2_MODE_2400;
	else if (setmode==1600)   /*not supported */
		obj->mode = CODEC2_MODE_3200;
	else if (setmode==1400) 
		obj->mode = CODEC2_MODE_3200;
	else if (setmode==1300)
		obj->mode = CODEC2_MODE_3200;
	else if (setmode==1200) 
		obj->mode = CODEC2_MODE_3200;
	else if (setmode==450)
		obj->mode = CODEC2_MODE_3200;
	else {
		ms_error("codec2_decode: Error in mode: %d.  Must be 3200, 2400, 1600, 1400, 1300, 1200, or 450, forcing CODEC2_MODE_3200\n", setmode);
	}
	obj->codec2 = codec2_create(obj->mode);
	obj->nsam = codec2_samples_per_frame(obj->codec2);
	obj->nbit = codec2_bits_per_frame(obj->codec2);
	obj->nbyte = (obj->nbit + 7) / 8;
	obj->frames = obj->bits_proc = 0;
	obj->nstart_bit = 0;
	obj->nend_bit = obj->nbit-1;
	obj->sample_rate=8000;
	obj->sequence_number=0;
#ifdef  CODEC2_DEBUG 
	ms_message("codec2_decode ( filter_preprocess ) obj->nbyte: %d",obj->nbyte);
#endif 
}

static void c2_decode(MSFilter *f, mblk_t *im) {
	struct codec2_dec_struct* obj= (struct codec2_dec_struct*) f->data;
	mblk_t *om; 
	int len=obj->sample_rate*0.02*2 ; // (160)
	obj->frames++;
#ifdef  CODEC2_DEBUG
	ms_message("codec2_decode: calling decoder . received frames %d",obj->frames);
#endif
	while(im->b_rptr<im->b_wptr) {
		om=allocb(len,0);
		mblk_meta_copy(im, om);
#ifdef  CODEC2_DEBUG
		ms_message("codec2_decode: im->b_rptr: %p , im->b_wptr: %p , om->b_wptr: %p",im->b_rptr,im->b_wptr,om->b_wptr);
#endif
		codec2_decode(obj->codec2,(short int *)(om->b_wptr), im->b_rptr);
		om->b_wptr+=len;
		im->b_rptr+=obj->nbyte;
		ms_queue_put(f->outputs[0],om);
#ifdef CODEC2_DEBUG
		ms_message("codec2_decode: om->b_wptr: %p, im->b_rptr: %p ",om->b_wptr,im->b_rptr);
#endif 
	}
}

static void dec_filter_process(MSFilter *f){
	mblk_t* im;
	while((im=ms_queue_get(f->inputs[0]))!=NULL){
		msgpullup(im,-1);
		c2_decode(f,im);
		freemsg(im);
	}
}

static void dec_filter_postprocess(MSFilter *f){
	struct codec2_dec_struct* obj= (struct codec2_dec_struct*) f->data;
	codec2_destroy(obj->codec2);
}

static void dec_filter_uninit(MSFilter *f){
	ms_free(f->data);
}

/*filter specific method*/
static int dec_filter_get_sample_rate(MSFilter *f, void *arg) {
	struct codec2_dec_struct* obj= (struct codec2_dec_struct*) f->data;
	*(int*)arg = obj->sample_rate;
	return 0;
}

static int dec_filter_have_plc(MSFilter *f, void *arg){
	*((int *)arg) = 0;
	return 0;
}

static MSFilterMethod dec_filter_methods[]={
	{	MS_FILTER_GET_SAMPLE_RATE 	, 	dec_filter_get_sample_rate 	},
	{ 	MS_DECODER_HAVE_PLC		, 	dec_filter_have_plc		},
	{	0				, 	NULL			}
};


#ifdef _MSC_VER

MSFilterDesc ms_codec2_dec_desc={
	MS_FILTER_PLUGIN_ID, /* from Allfilters.h*/
	"MSCodec2Dec",
	"Codec2 decoder filter.",
	MS_FILTER_DECODER,
	"CODEC2",
	1, /*number of inputs*/
	1, /*number of outputs*/
	dec_filter_init,
	dec_filter_preprocess,
	dec_filter_process,
	dec_filter_postprocess,
	dec_filter_uninit,
	dec_filter_methods,
	MS_FILTER_IS_PUMP
};

#else

MSFilterDesc ms_codec2_dec_desc={
	.id=MS_FILTER_PLUGIN_ID, /* from Allfilters.h*/
	.name="MSCodec2Dec",
	.text="Codec2 decoder filter.",
	.category=MS_FILTER_DECODER,
	.enc_fmt="CODEC2",
	.ninputs=1, /*number of inputs*/
	.noutputs=1, /*number of outputs*/
	.init=dec_filter_init,
	.preprocess=dec_filter_preprocess,
	.process=dec_filter_process,
	.postprocess=dec_filter_postprocess,
	.uninit=dec_filter_uninit,
	.methods=dec_filter_methods,
	.flags=MS_FILTER_IS_PUMP
};

#endif

MS_FILTER_DESC_EXPORT(ms_codec2_dec_desc)


#ifdef _MSC_VER
#define MS_PLUGIN_DECLARE(type) __declspec(dllexport) type
#else
#define MS_PLUGIN_DECLARE(type) type
#endif

MS_PLUGIN_DECLARE ( void ) libmscodec2_init(MSFactory *factory) {
	ms_factory_register_filter(factory, &ms_codec2_enc_desc );
	ms_factory_register_filter(factory, &ms_codec2_dec_desc );
	ms_message( "libmscodec2 v%s plugin loaded", MSCODEC2_VERSION );
}

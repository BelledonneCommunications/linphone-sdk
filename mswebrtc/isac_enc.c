/*
 isac_enc.c
 Copyright (C) 2013 Belledonne Communications, Grenoble, France

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

#include "isacfix.h"
#include "signal_processing_library.h"

#include "isac_constants.h"

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/mscodecutils.h"


/*filter common method*/
struct _isac_encoder_struct_t {
	ISACFIX_MainStruct* isac;
	MSBufferizer *bufferizer;
	unsigned int ptime;
	unsigned int bitrate;
	unsigned int ts;
};

typedef struct _isac_encoder_struct_t isac_encoder_struct_t;


static void filter_init ( MSFilter *f ) {
	ISACFIX_MainStruct* isac_mainstruct = NULL;
	struct _isac_encoder_struct_t *obj = NULL;
	int instance_size;
	int16_t ret;

	f->data = ms_new0(isac_encoder_struct_t, 1);
	obj = (isac_encoder_struct_t*)f->data;


	ret = WebRtcIsacfix_AssignSize( &instance_size );
	if( ret ) {
		ms_error("WebRtcIsacfix_AssignSize returned size %d", instance_size);
	}
	isac_mainstruct = ms_malloc0(instance_size);

	ret = WebRtcIsacfix_Assign(&obj->isac, isac_mainstruct);
	if( ret ) {
		ms_error("WebRtcIsacfix_Create failed (%d)", ret);
	}

	// TODO: AUTO or USER coding mode?
	ret = WebRtcIsacfix_EncoderInit(obj->isac, CODING_USERDEFINED);
	if( ret ) {
		ms_error("WebRtcIsacfix_EncoderInit failed (%d)",  WebRtcIsacfix_GetErrorCode(obj->isac));
	}

	obj->ptime   = 30; // iSAC allows 30 or 60ms per packet
	obj->bitrate = ISAC_BITRATE_MAX;


	ret = WebRtcIsacfix_Control(obj->isac, obj->bitrate, obj->ptime);
	if( ret ) {
		ms_error("WebRtcIsacfix_Control failed: %d", WebRtcIsacfix_GetErrorCode(obj->isac));
	}

	obj->bufferizer = ms_bufferizer_new();
	obj->ts = 0;
}

static void filter_preprocess ( MSFilter *f ) {
}

static void filter_process ( MSFilter *f ) {
	isac_encoder_struct_t* obj = (isac_encoder_struct_t*)f->data;

	mblk_t *im;
	mblk_t *om=NULL;
#if defined (_MSC_VER)
	uint8_t* input_buf = NULL;
#else
	u_int8_t* input_buf = NULL;
#endif
	int16_t ret;
	static int out_count = 0;

	// get the input data and put it into our buffered input
	while( (im = ms_queue_get( f->inputs[0] ) ) != NULL ) {
		ms_bufferizer_put( obj->bufferizer, im );
	}

	// feed the encoder with 160 16bit samples, until it has reached enough data
	// to produce a packet
	while( ms_bufferizer_get_avail(obj->bufferizer) > ISAC_SAMPLES_PER_ENCODE*2 ){

		om = allocb( WebRtcIsacfix_GetNewFrameLen(obj->isac), 0 );
		if(!input_buf) input_buf = ms_malloc( ISAC_SAMPLES_PER_ENCODE*2 );
		ms_bufferizer_read(obj->bufferizer, input_buf, ISAC_SAMPLES_PER_ENCODE*2);

		ret = WebRtcIsacfix_Encode(obj->isac,
								   (const int16_t*)input_buf,
								   (uint8_t*)om->b_wptr);

		if( ret < 0) {

			ms_error( "WebRtcIsacfix_Encode error: %d", WebRtcIsacfix_GetErrorCode(obj->isac) );
			freeb(om);

		} else if( ret == 0 ) {
			// Encode() buffered the input, not yet able to produce a packet, continue feeding it
			// 160 samples per-call
			obj->ts += ISAC_SAMPLES_PER_ENCODE;
			freeb(om);

		} else {

			// a new packet has been encoded, send it
			obj->ts += ISAC_SAMPLES_PER_ENCODE;
			om->b_wptr += ret;
			out_count++;
			//            ms_message("packet %d out, samples %d", out_count, obj->ts);

			mblk_set_timestamp_info( om, obj->ts );
			ms_queue_put(f->outputs[0], om);

			om = NULL;
		}

	}

	if( input_buf ){
		ms_free(input_buf);
	}
}

static void filter_postprocess ( MSFilter *f ) {

}

static void filter_uninit ( MSFilter *f ) {
	struct _isac_encoder_struct_t *encoder = (struct _isac_encoder_struct_t*)f->data;
	ms_free(encoder->isac);
	ms_bufferizer_destroy(encoder->bufferizer);
	ms_free(encoder);
	f->data = 0;
}


/*filter specific method*/

static int filter_get_sample_rate ( MSFilter *f, void *arg ) {
	*(int*)arg = ISAC_SAMPLE_RATE;
	return 0;
}

#ifdef MS_AUDIO_ENCODER_SET_PTIME
static int filter_get_ptime(MSFilter *f, void *arg){
	isac_encoder_struct_t* obj= ( isac_encoder_struct_t* ) f->data;
	*(int*)arg=obj->ptime;
	return 0;
}

static int filter_set_ptime(MSFilter *f, void *arg){
	int asked = *(int*)arg;
	isac_encoder_struct_t* obj = (isac_encoder_struct_t*)f->data;

	// iSAC handles only 30 or 60ms ptime
	if( asked != 30 && asked != 60 ){
		// use the closest
		asked = (asked > 45)? 60 : 30;
		ms_warning("iSAC doesn't handle %dms ptime, choosing closest: %dms", *(int*)arg, asked);
	}

	obj->ptime = asked;

	return 0;
}
#endif

static int filter_set_bitrate ( MSFilter *f, void *arg ) {
	isac_encoder_struct_t *obj = (isac_encoder_struct_t*)f->data;
	int wanted_bitrate = *(int*)arg;

	if( wanted_bitrate > ISAC_BITRATE_MAX ) {
		ms_warning("iSAC doesn't handle bitrate > %d (wanted: %d)",
				   ISAC_BITRATE_MAX, wanted_bitrate );

		wanted_bitrate = ISAC_BITRATE_MAX;

	} else if( wanted_bitrate < ISAC_BITRATE_MIN) {
		ms_warning("iSAC doesn't handle bitrate < %d (wanted: %d)",
				   ISAC_BITRATE_MIN, wanted_bitrate );

		wanted_bitrate = ISAC_BITRATE_MIN;

	}

	return WebRtcIsacfix_SetMaxRate(obj->isac, wanted_bitrate);
}

static int filter_get_bitrate ( MSFilter *f, void *arg ) {
	isac_encoder_struct_t *obj = (isac_encoder_struct_t*)f->data;
	*(int*)arg = (int)WebRtcIsacfix_GetUplinkBw(obj->isac);
	return 0;
}

static MSFilterMethod filter_methods[]= {
	{ MS_FILTER_GET_SAMPLE_RATE,  filter_get_sample_rate },
	{ MS_FILTER_SET_BITRATE,      filter_set_bitrate     },
	{ MS_FILTER_GET_BITRATE,      filter_get_bitrate     },
#ifdef MS_AUDIO_ENCODER_SET_PTIME
	{ MS_AUDIO_ENCODER_SET_PTIME, filter_set_ptime       },
	{ MS_AUDIO_ENCODER_GET_PTIME, filter_get_ptime       },
#endif
	{ 0,                          NULL                   }
};



#define MS_ISAC_ENC_NAME        "MSiSACEnc"
#define MS_ISAC_ENC_DESCRIPTION "iSAC audio encoder filter."
#define MS_ISAC_ENC_CATEGORY    MS_FILTER_ENCODER
#define MS_ISAC_ENC_ENC_FMT     "iSAC"
#define MS_ISAC_ENC_NINPUTS     1
#define MS_ISAC_ENC_NOUTPUTS    1
#define MS_ISAC_ENC_FLAGS       0

#ifdef _MSC_VER

MSFilterDesc ms_isac_enc_desc = {
	MS_FILTER_PLUGIN_ID,
	MS_ISAC_ENC_NAME,
	MS_ISAC_ENC_DESCRIPTION,
	MS_ISAC_ENC_CATEGORY,
	MS_ISAC_ENC_ENC_FMT,
	MS_ISAC_ENC_NINPUTS,
	MS_ISAC_ENC_NOUTPUTS,
	filter_init,
	filter_preprocess,
	filter_process,
	filter_postprocess,
	filter_uninit,
	filter_methods,
	MS_ISAC_ENC_FLAGS
};

#else

MSFilterDesc ms_isac_enc_desc = {
	.id = MS_FILTER_PLUGIN_ID,
	.name = MS_ISAC_ENC_NAME,
	.text = MS_ISAC_ENC_DESCRIPTION,
	.category = MS_ISAC_ENC_CATEGORY,
	.enc_fmt = MS_ISAC_ENC_ENC_FMT,
	.ninputs = MS_ISAC_ENC_NINPUTS,
	.noutputs = MS_ISAC_ENC_NOUTPUTS,
	.init = filter_init,
	.preprocess = filter_preprocess,
	.process = filter_process,
	.postprocess = filter_postprocess,
	.uninit = filter_uninit,
	.methods = filter_methods,
	.flags = MS_ISAC_ENC_FLAGS
};

#endif

MS_FILTER_DESC_EXPORT ( ms_isac_enc_desc )

/*
 isac_dec.c
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

#include "mediastreamer2/mscodecutils.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "ortp/rtp.h"

#include <stdint.h>

#include "isac_constants.h"

/*filter common method*/
struct _isac_decoder_struct_t {
	ISACFIX_MainStruct* isac;

	MSConcealerContext* plc_ctx;
	unsigned short int  seq_nb;
	unsigned int        ptime;
};

typedef struct _isac_decoder_struct_t isac_decoder_t;

static void filter_init(MSFilter *f){
	ISACFIX_MainStruct* isac_mainstruct = NULL;
	isac_decoder_t *obj = NULL;
	int instance_size;
	int16_t ret;

	f->data = ms_new0(isac_decoder_t, 1);
	obj = (isac_decoder_t*)f->data;

	ret = WebRtcIsacfix_AssignSize( &instance_size );
	if( ret ) {
		ms_error("WebRtcIsacfix_AssignSize returned size %d", instance_size);
	}
	isac_mainstruct = ms_malloc(instance_size);

	ret = WebRtcIsacfix_Assign(&obj->isac, isac_mainstruct);
	if( ret ) {
		ms_error("WebRtcIsacfix_Create failed (%d)", ret);
	}

	WebRtcIsacfix_DecoderInit(obj->isac);

	obj->ptime = 30; // default ptime is 30ms per packet
}

static void filter_preprocess(MSFilter *f){
	isac_decoder_t* obj = (isac_decoder_t*)f->data;
	obj->plc_ctx = ms_concealer_context_new(UINT32_MAX);
}


static void decode(MSFilter *f, mblk_t *im) {
	isac_decoder_t* obj = (isac_decoder_t*)f->data;
	int16_t ret;
	int16_t speech_type; // needed but not used..
	size_t samples_nb;

	// im is one packet from the encoder, so it's either 30 or 60 ms of audio
	ret = WebRtcIsacfix_ReadFrameLen( (const uint8_t*)im->b_rptr, msgdsize(im), &samples_nb);
	//     ms_message("WebRtcIsacfix_ReadFrameLen -> %d", samples_nb);

	if( ret == 0 ) {
		mblk_t *om = allocb(samples_nb*2, 0);
		mblk_meta_copy(im, om);

		obj->ptime = (samples_nb == ISAC_30MS_SAMPLE_COUNT) ? 30 : 60; // update ptime
		//         ms_message("DECODED om datap @%p", om->b_datap);

		ret = WebRtcIsacfix_Decode(obj->isac,
								(const uint8_t*)im->b_rptr,
								(im->b_wptr - im->b_rptr),
								(int16_t*)om->b_wptr,
								&speech_type );
		if( ret < 0 ) {
			ms_error( "WebRtcIsacfix_Decode error: %d", WebRtcIsacfix_GetErrorCode(obj->isac) );
			freeb(om);
		} else {
			//             ms_message("Decoded %d samples", ret);
			om->b_wptr+= ret*2;
			mblk_set_plc_flag(om, 0);
			ms_queue_put(f->outputs[0], om);
		}

	} else {
		ms_error( "WebRtcIsacfix_ReadFrameLen failed: %d", WebRtcIsacfix_GetErrorCode(obj->isac) );
	}

	obj->seq_nb = mblk_get_cseq(im);

	ms_concealer_inc_sample_time(obj->plc_ctx, f->ticker->time, obj->ptime, TRUE);

	return;
}

static void filter_process(MSFilter *f){
	isac_decoder_t* obj = (isac_decoder_t*)f->data;
	mblk_t* im;
	int count = 0;

	im = ms_queue_get( f->inputs[0] );
	while( im != NULL ){
		decode(f, im);
		freemsg(im);
		count++;
		im = ms_queue_get( f->inputs[0] );
	}

	if( ms_concealer_context_is_concealement_required(obj->plc_ctx, f->ticker->time) ) {

		int16_t flen =  (obj->ptime == 30) ? ISAC_30MS_SAMPLE_COUNT
												 : ISAC_60MS_SAMPLE_COUNT;
		mblk_t* plc_blk = allocb(flen*2, 0 );
		//        ms_message("PLC for %d ms", obj->ptime);

		// interpolate 1 frame for 30ms ptime, 2 frames for 60ms
		int16_t ret = WebRtcIsacfix_DecodePlc(obj->isac,
													(int16_t*)plc_blk->b_wptr,
													(obj->ptime == 30) ? 1 : 2);

		if( ret < 0 ) {

			ms_error("WebRtcIsacfix_DecodePlc error: %d", WebRtcIsacfix_GetErrorCode(obj->isac) );
			freeb(plc_blk);

		} else {

			plc_blk->b_wptr += ret*2;
			obj->seq_nb++;

			// insert this interpolated block into the output, with correct args:
			mblk_set_cseq(plc_blk, obj->seq_nb );
			mblk_set_plc_flag(plc_blk, 1); // this one's a PLC packet

			ms_queue_put(f->outputs[0], plc_blk);

			ms_concealer_inc_sample_time(obj->plc_ctx, f->ticker->time, obj->ptime, FALSE);

		}
	}
}

static void filter_postprocess(MSFilter *f){
	isac_decoder_t* obj = (isac_decoder_t*)f->data;
	ms_concealer_context_destroy(obj->plc_ctx);
	obj->plc_ctx = NULL;
}

static void filter_uninit(MSFilter *f){
	isac_decoder_t* obj = (isac_decoder_t*)f->data;
	ms_free(obj->isac);
	ms_free(f->data);
	f->data = NULL;
}


/*filter specific method*/

static int filter_set_sample_rate(MSFilter *f, void *arg) {
	if( *(int*)arg != ISAC_SAMPLE_RATE) {
		ms_error("iSAC doesn't support sampling rate %d, only %d",
				 *(int*)arg, ISAC_SAMPLE_RATE);
	}
	return 0;
}

static int filter_get_sample_rate(MSFilter *f, void *arg) {
	*(int*)arg = ISAC_SAMPLE_RATE;
	return 0;
}

static int filter_have_plc(MSFilter *f, void *arg)
{
	*(int*)arg = 1;
	return 0;
}

static MSFilterMethod filter_methods[] = {
	{ MS_FILTER_SET_SAMPLE_RATE, filter_set_sample_rate },
	{ MS_FILTER_GET_SAMPLE_RATE, filter_get_sample_rate },
	{ MS_DECODER_HAVE_PLC,       filter_have_plc        },
	{ 0,                         NULL                   }
};



#define MS_ISAC_DEC_NAME        "MSiSACDec"
#define MS_ISAC_DEC_DESCRIPTION "iSAC audio decoder filter."
#define MS_ISAC_DEC_CATEGORY    MS_FILTER_DECODER
#define MS_ISAC_DEC_ENC_FMT     "iSAC"
#define MS_ISAC_DEC_NINPUTS     1
#define MS_ISAC_DEC_NOUTPUTS    1
#define MS_ISAC_DEC_FLAGS       MS_FILTER_IS_PUMP

#ifdef _MSC_VER

MSFilterDesc ms_isac_dec_desc = {
	MS_FILTER_PLUGIN_ID,
	MS_ISAC_DEC_NAME,
	MS_ISAC_DEC_DESCRIPTION,
	MS_ISAC_DEC_CATEGORY,
	MS_ISAC_DEC_ENC_FMT,
	MS_ISAC_DEC_NINPUTS,
	MS_ISAC_DEC_NOUTPUTS,
	filter_init,
	filter_preprocess,
	filter_process,
	filter_postprocess,
	filter_uninit,
	filter_methods,
	MS_ISAC_DEC_FLAGS
};

#else

MSFilterDesc ms_isac_dec_desc = {
	.id = MS_FILTER_PLUGIN_ID,
	.name = MS_ISAC_DEC_NAME,
	.text = MS_ISAC_DEC_DESCRIPTION,
	.category = MS_ISAC_DEC_CATEGORY,
	.enc_fmt = MS_ISAC_DEC_ENC_FMT,
	.ninputs = MS_ISAC_DEC_NINPUTS,
	.noutputs = MS_ISAC_DEC_NOUTPUTS,
	.init = filter_init,
	.preprocess = filter_preprocess,
	.process = filter_process,
	.postprocess = filter_postprocess,
	.uninit = filter_uninit,
	.methods = filter_methods,
	.flags = MS_ISAC_DEC_FLAGS
};

#endif

MS_FILTER_DESC_EXPORT(ms_isac_dec_desc)

/*
msvaddtx.c - Generic VAD/DTX (voice activity detector, discontinuous transmission)
	for CN payload type (RFC3389)

mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2011-2012 Belledonne Communications, Grenoble, France

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


#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msutils.h"
#include "mediastreamer2/msvaddtx.h"

#include "ortp/utils.h"

#ifdef HAVE_G729B
#include "bcg729/encoder.h"
#endif

#include <math.h>

static const float max_e = (32768* 0.7f);   /* 0.7 - is RMS factor */
static const float coef = 0.2f; /* floating averaging coeff. for energy */
static const float silence_threshold=0.01f;

typedef struct _VadDtxContext{
	int silence_mode;/*set to 1 if a silence period is running*/
#ifndef HAVE_G729B
	float energy;
	OrtpExtremum max;
#else
	bcg729EncoderChannelContextStruct *encoderChannelContext;
	MSBufferizer *bufferizer;
#endif
}VadDtxContext;


static void vad_dtx_init(MSFilter *f){
	VadDtxContext *ctx=ms_new0(VadDtxContext,1);
	f->data=ctx;

#ifdef HAVE_G729B
	ctx->encoderChannelContext = initBcg729EncoderChannel(1); /* init G729 encoder with VAD enabled */
	ctx->bufferizer=ms_bufferizer_new();
#else
	ortp_extremum_init(&ctx->max,2000);
#endif
}

static void vad_dtx_preprocess(MSFilter *f){
#ifndef HAVE_G729B
	VadDtxContext *ctx=(VadDtxContext*)f->data;
	ortp_extremum_reset(&ctx->max);
#endif

}

#ifndef HAVE_G729B
static void update_energy(VadDtxContext *v, int16_t *signal, int numsamples, uint64_t curtime) {
	int i;
	float acc = 0;
	float en;

	for (i=0;i<numsamples;++i){
		int s=signal[i];
		acc += s * s;
	}
	en = (float)((sqrt(acc / numsamples)+1) / max_e);
	v->energy = (en * coef) + v->energy * (1.0f - coef);
	ortp_extremum_record_max(&v->max,curtime,v->energy);
	//ms_message("Energy=%f, current max=%f",v->energy, ortp_extremum_get_current(&v->max));
}
#endif

static void vad_dtx_process(MSFilter *f){
	VadDtxContext *ctx=(VadDtxContext*)f->data;
	mblk_t *m;

#ifdef HAVE_G729B
	uint8_t inputBuffer[160]; /* 2bytes per sample at 8000Hz -> 16 bytes per ms */
	/* get all input data into a buffer */
	while((m=ms_queue_get(f->inputs[0]))!=NULL){
		ms_queue_put(f->outputs[0],dupmsg(m)); /* forward the message to the next filter, duplicate it because putting it in the bufferizer destroys it */
		ms_bufferizer_put(ctx->bufferizer,m);
	}
	/* process frames of 10 ms (160 bytes) at 8000kHz and 2 bytes per sample */
	while(ms_bufferizer_get_avail(ctx->bufferizer)>=160){
		uint8_t bitStreamLength = 0;
		uint8_t bitStream[10]; /* store the g729 encoder output : 10 bytes(voice frame), 2 bytes (noise frame) or 0 byte (untransmitted frame) */
		memset(bitStream, 0, 10*sizeof(uint8_t));
		/* process buffer in 10 ms frames */
		/* RFC3551 section 4.5.6 we must end the RTP payload of G729 frames when transmitting a SID frame : bitStreamLength == 2 */
		ms_bufferizer_read(ctx->bufferizer,inputBuffer,160);
		bcg729Encoder(ctx->encoderChannelContext, (int16_t *)inputBuffer, bitStream, &bitStreamLength);
		if (bitStreamLength != 10) { /* this is a noise frame */
			if (bitStreamLength == 2){ /* there is a NOISE frame to send */
				MSCngData cngdata;
				memset (&cngdata, 0, sizeof(MSCngData));
				/* extract the rfc3389 parameters */
				bcg729GetRFC3389Payload(ctx->encoderChannelContext, cngdata.data);
				cngdata.datasize=11; /* noise parameters on 11 bytes */
				ms_message("vad_dtx_process(): send SID frame. Notify filter [%p]", f);
				ctx->silence_mode=1;
				ms_filter_notify(f, MS_VAD_DTX_NO_VOICE, &cngdata);
			}
		} else { /* voice frame */
			if (ctx->silence_mode){
				/*ms_message("vad_dtx_process(): silence period finished.");*/
				ctx->silence_mode=0;
				ms_filter_notify(f, MS_VAD_DTX_VOICE, NULL);
			}
		}
	}

#else
	while((m=ms_queue_get(f->inputs[0]))!=NULL){
		update_energy(ctx,(int16_t*)m->b_rptr, (int)((m->b_wptr - m->b_rptr) / 2), f->ticker->time);

		if (ortp_extremum_get_current(&ctx->max)<silence_threshold){
			if (!ctx->silence_mode){
				MSCngData cngdata={0};
				cngdata.datasize=1; /*only noise level*/
				cngdata.data[0]=0; /*noise level set to zero for the moment*/
				ms_message("vad_dtx_process(): silence period detected.");
				ctx->silence_mode=1;
				ms_filter_notify(f, MS_VAD_DTX_NO_VOICE, &cngdata);
			}
		}else{
			if (ctx->silence_mode){
				ms_message("vad_dtx_process(): silence period finished.");
				ctx->silence_mode=0;
				ms_filter_notify(f, MS_VAD_DTX_VOICE, NULL);
			}
		}

		ms_queue_put(f->outputs[0],m);
	}
#endif

}

static void vad_dtx_postprocess(MSFilter *f){
#ifdef HAVE_G729B
	VadDtxContext *ctx=(VadDtxContext*)f->data;
	ms_bufferizer_destroy(ctx->bufferizer);
	closeBcg729EncoderChannel(ctx->encoderChannelContext);
#endif

}

static void vad_dtx_uninit(MSFilter *f){
	VadDtxContext *ctx=(VadDtxContext*)f->data;
	ms_free(ctx);
}

#ifndef _MSC_VER

MSFilterDesc ms_vad_dtx_desc = {
	.id = MS_VAD_DTX_ID,
	.name = "MSVadDtx",
	.text = "A filter detecting silence period and encoding residual noise",
	.category = MS_FILTER_OTHER,
	.ninputs = 1,
	.noutputs = 1,
	.init = vad_dtx_init,
	.preprocess = vad_dtx_preprocess,
	.process = vad_dtx_process,
	.postprocess = vad_dtx_postprocess,
	.uninit = vad_dtx_uninit,
};

#else

MSFilterDesc ms_vad_dtx_desc = {
	MS_VAD_DTX_ID,
	"MSVadDtx",
	"A filter detecting silence period and encoding residual noise",
	MS_FILTER_OTHER,
	NULL,
	1,
	1,
	vad_dtx_init,
	vad_dtx_preprocess,
	vad_dtx_process,
	vad_dtx_postprocess,
	vad_dtx_uninit
};

#endif

MS_FILTER_DESC_EXPORT(ms_vad_dtx_desc)


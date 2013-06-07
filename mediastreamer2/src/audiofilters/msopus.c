/*
msopus.c - Opus encoder/decoder for Linphone

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/


#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/mscodecutils.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "ortp/rtp.h"


#include <opus/opus.h>

#define SIGNAL_SAMPLE_SIZE  2 // 2 bytes per sample

/* Define codec specific settings */
#define FRAME_LENGTH			20 // ptime may be 20, 40, 60, 80, 100 or 120, packets composed of multiples 20ms frames 
#define MAX_BYTES_PER_FRAME     500 // Equals peak bitrate of 200 kbps
#define MAX_INPUT_FRAMES        6


/**
 * Definition of the private data structure of the opus encoder.
 */
typedef struct _OpusEncData {
	OpusEncoder *state;
	MSBufferizer *bufferizer;
	uint32_t ts;
	int samplerate;
	int channels;
	int application;
	int max_network_bitrate;
	int bitrate;

	int maxplaybackrate;
	int maxptime;
	int ptime;
	int minptime;
	int maxaveragebitrate;
	int stereo;
	int vbr;
	int useinbandfec;
	int usedtx;

} OpusEncData;

/* Local functions headers */
static void apply_max_bitrate(OpusEncData *d);
static void compute_max_bitrate(OpusEncData *d, int ptimeStep);
static int ms_opus_enc_set_vbr(MSFilter *f);
static int ms_opus_enc_set_inbandfec(MSFilter *f);
static int ms_opus_enc_set_dtx(MSFilter *f);

/******************************************************************************
 * Methods to (de)initialize and run the opus encoder                         *
 *****************************************************************************/

static void ms_opus_enc_init(MSFilter *f) {
	OpusEncData *d = (OpusEncData *)ms_new(OpusEncData, 1);
	d->bufferizer = ms_bufferizer_new();
	d->state = NULL;
	d->ts = 0;
	d->samplerate = 48000;
	d->application = OPUS_APPLICATION_VOIP; // property not really needed as we are always in this application mode
	d->bitrate = -1;
	d->max_network_bitrate = 46000;

	/* set default parameters according to draft RFC RTP Payload Format for Opus codec section 6.1 */
	d->maxplaybackrate = 48000;
	d->maxptime = 120;
	d->ptime = -1; // ptime will be set once by the set ptime function and then managed by set bitrate/apply_max_bitrate
	d->minptime = 20; // defaut shall be 3, but we never use less than 20.
	d->maxaveragebitrate = -1;
	d->stereo = 1;
	d->channels = 1;
	d->vbr = 1;
	d->useinbandfec = 0;
	d->usedtx = 0;

	f->data = d;
}

static void ms_opus_enc_preprocess(MSFilter *f) {
	int error;

	OpusEncData *d = (OpusEncData *)f->data;
	/* create the encoder */
	d->state = opus_encoder_create(d->samplerate, d->channels, d->application, &error);
	if (error != OPUS_OK) {
		ms_error("Opus encoder creation failed: %s", opus_strerror(error));
		return;
	}

	/* set complexity to 0 for arm devices */
#ifdef __arm__
	opus_encoder_ctl(d->state, OPUS_SET_COMPLEXITY(0));
#endif
	error = opus_encoder_ctl(d->state, OPUS_SET_PACKET_LOSS_PERC(10));
	if (error != OPUS_OK) {
		ms_error("Could not set default loss percentage to opus encoder: %s", opus_strerror(error));
	}

	/* set the encoder parameters: VBR, IN_BAND_FEC, DTX and bitrate settings */
	ms_opus_enc_set_vbr(f);
	ms_opus_enc_set_inbandfec(f);
	ms_opus_enc_set_dtx(f);
	/* if decoder prefers mono signal, force encoder to output mono signal */
	if (d->stereo == 0) {
		error = opus_encoder_ctl(d->state, OPUS_SET_FORCE_CHANNELS(1));
		if (error != OPUS_OK) {
			ms_error("could not force mono channel to opus encoder: %s", opus_strerror(error));
		}
	}

	ms_filter_lock(f);
	if (d->ptime==-1) { // set ptime wasn't call, set default:20ms
		d->ptime = 20;
	}
	if (d->bitrate==-1) { // set bitrate wasn't call, compute it with the default network bitrate (36000)
		compute_max_bitrate(d, 0);
	}
	apply_max_bitrate(d);
	ms_filter_unlock(f);
}

static void ms_opus_enc_process(MSFilter *f) {
	OpusEncData *d = (OpusEncData *)f->data;
	mblk_t *im;
	mblk_t *om = NULL;
	int i;
	int frameNumber, packet_size;
	uint8_t *signalFrameBuffer = NULL;
	uint8_t *codedFrameBuffer[MAX_INPUT_FRAMES];
	OpusRepacketizer *rp = opus_repacketizer_create();
	opus_int32 ret = 0;
	opus_int32 totalLength = 0;
	int frame_size = d->samplerate * FRAME_LENGTH / 1000; /* in samples */

	// lock the access while getting ptime
	ms_filter_lock(f);
	frameNumber = d->ptime/FRAME_LENGTH; /* encode 20ms frames, ptime is a multiple of 20ms */
	packet_size = d->samplerate * d->ptime / 1000; /* in samples */
	ms_filter_unlock(f);


	while ((im = ms_queue_get(f->inputs[0])) != NULL) {
		ms_bufferizer_put(d->bufferizer, im);
	}

	for (i=0; i<MAX_INPUT_FRAMES; i++) {
		codedFrameBuffer[i]=NULL;
	}
	while (ms_bufferizer_get_avail(d->bufferizer) >= (d->channels * packet_size * SIGNAL_SAMPLE_SIZE)) {
		totalLength = 0;
		opus_repacketizer_init(rp);
		for (i=0; i<frameNumber; i++) { /* encode 20ms by 20ms and repacketize all of them together */
			if (!codedFrameBuffer[i]) codedFrameBuffer[i] = ms_malloc(MAX_BYTES_PER_FRAME); /* the repacketizer need the pointer to packet to remain valid, so we shall have a buffer for each coded frame */
			if (!signalFrameBuffer) signalFrameBuffer = ms_malloc(frame_size * SIGNAL_SAMPLE_SIZE * d->channels);

			ms_bufferizer_read(d->bufferizer, signalFrameBuffer, frame_size * SIGNAL_SAMPLE_SIZE * d->channels);
			ret = opus_encode(d->state, (opus_int16 *)signalFrameBuffer, frame_size, codedFrameBuffer[i], MAX_BYTES_PER_FRAME);
			if (ret < 0) {
				ms_error("Opus encoder error: %s", opus_strerror(ret));
				break;
			}
			if (ret > 0) {
				int err = opus_repacketizer_cat(rp, codedFrameBuffer[i], ret); /* add the encoded frame into the current packet */
				if (err != OPUS_OK) {
					ms_error("Opus repacketizer error: %s", opus_strerror(err));
					break;
				}
				totalLength += ret;
			}
		}

		if (ret > 0) {
			om = allocb(totalLength+frameNumber + 1, 0); /* opus repacktizer API: allocate at leat number of frame + size of all data added before */ 
			ret = opus_repacketizer_out(rp, om->b_wptr, totalLength+frameNumber);

			om->b_wptr += ret;
			mblk_set_timestamp_info(om, d->ts);
			ms_queue_put(f->outputs[0], om);
			d->ts += packet_size*48000/d->samplerate; /* RFC payload RTP opus 03 - section 4: RTP timestamp multiplier : WARNING works only with sr at 48000 */
			ret = 0;
		}
	}

	opus_repacketizer_destroy(rp);

	if (signalFrameBuffer != NULL) {
		ms_free(signalFrameBuffer);
	}
	for (i=0; i<frameNumber; i++) {
		if (codedFrameBuffer[i] != NULL) {
			ms_free(codedFrameBuffer[i]);
		}
	}
}

static void ms_opus_enc_postprocess(MSFilter *f) {
	OpusEncData *d = (OpusEncData *)f->data;
	opus_encoder_destroy(d->state);
	d->state = NULL;
}

static void ms_opus_enc_uninit(MSFilter *f) {
	OpusEncData *d = (OpusEncData *)f->data;
	if (d == NULL) return;
	if (d->state) {
		opus_encoder_destroy(d->state);
		d->state = NULL;
	}
	ms_bufferizer_destroy(d->bufferizer);
	d->bufferizer = NULL;
	ms_free(d);
}


/******************************************************************************
 * Methods to configure the opus encoder                                      *
 *****************************************************************************/
/* the compute max_bitrate function will also modify the ptime according to requested bitrate */
static void compute_max_bitrate(OpusEncData *d, int ptimeStep) {
	int normalized_cbr = 0;
	int pps;

	/* check maxaverage bit rate parameter */
	if (d->maxaveragebitrate>0 && d->maxaveragebitrate<d->max_network_bitrate) {
		ms_warning("Opus encoder can't apply network bitrate [%i] because of maxaveragebitrate [%i] requested by fmtp line. Fall back to fmtp bitrate setting.", d->max_network_bitrate, d->maxaveragebitrate);
		d->max_network_bitrate = d->maxaveragebitrate;
	}

	if (d->ptime==-1) {
		pps = 50; // if this function is called before ptime being set, use default ptime 20ms
	} else {
		pps = 1000 / d->ptime;
	}

	/* if have a potentiel ptime modification suggested by caller, check ptime and bitrate for adjustment */
	if (ptimeStep != 0) {
		normalized_cbr = (int)(((((float)d->max_network_bitrate) / (pps * 8)) - 20 - 12 -8) * pps * 8);
		if (normalized_cbr<12000) {
			if (d->ptime<120 || (ptimeStep<0 && d->ptime>40)) {
				d->ptime += ptimeStep;
			}
		} else if (normalized_cbr<20000) {
			if (d->ptime<60 || ptimeStep<0) {
				d->ptime += ptimeStep;
			}
		} else if (normalized_cbr<40000) {
			if (d->ptime<40 || ptimeStep<0) {
				d->ptime += ptimeStep;
			}
		}
		if (d->ptime<20) {
			d->ptime = 20;
		}

		pps = 1000 / d->ptime;
	}

	normalized_cbr = (int)(((((float)d->max_network_bitrate) / (pps * 8)) - 20 - 12 -8) * pps * 8);
	/* check if bitrate is in range [6,510kbps] */
	if (normalized_cbr<6000) {
		normalized_cbr = 6000;
		d->max_network_bitrate = ((normalized_cbr*d->ptime/8000) + 12 + 8 + 20) *8000/d->ptime;
		ms_warning("Opus encoder doesn't support bitrate [%i] set to 6kbps network bitrate [%d]", normalized_cbr, d->max_network_bitrate);
	}

	if (normalized_cbr>510000) {
		normalized_cbr = 510000;
		d->max_network_bitrate = ((normalized_cbr*d->ptime/8000) + 12 + 8 + 20) *8000/d->ptime;
		ms_warning("Opus encoder doesn't support bitrate [%i] set to 510kbps network bitrate [%d]", normalized_cbr, d->max_network_bitrate);
	}

	d->bitrate = normalized_cbr;


}


static void apply_max_bitrate(OpusEncData *d) {
	ms_message("Setting opus codec birate to [%i] from network bitrate [%i] with ptime [%i]", d->bitrate, d->max_network_bitrate, d->ptime);
	/* give the bitrate to the encoder if exists*/
	if (d->state) {
		opus_int32 maxBandwidth;

		int error = opus_encoder_ctl(d->state, OPUS_SET_BITRATE(d->bitrate));
		if (error != OPUS_OK) {
			ms_error("could not set bit rate to opus encoder: %s", opus_strerror(error));
		}

		/* set output sampling rate according to bitrate and RFC section 3.1.1 */
		if (d->bitrate<12000) {
			maxBandwidth = OPUS_BANDWIDTH_NARROWBAND;
		} else if (d->bitrate<20000) {
			maxBandwidth = OPUS_BANDWIDTH_WIDEBAND;
		} else if (d->bitrate<40000) {
			maxBandwidth = OPUS_BANDWIDTH_FULLBAND;
		} else if (d->bitrate<64000) {
			maxBandwidth = OPUS_BANDWIDTH_FULLBAND;
		} else {
			maxBandwidth = OPUS_BANDWIDTH_FULLBAND;
		}

		/* check if selected maxBandwidth is compatible with the maxplaybackrate parameter */
		if (d->maxplaybackrate < 12000) {
			maxBandwidth = OPUS_BANDWIDTH_NARROWBAND;
		} else if (d->maxplaybackrate < 16000) {
			if (maxBandwidth != OPUS_BANDWIDTH_NARROWBAND) {
				maxBandwidth = OPUS_BANDWIDTH_MEDIUMBAND;
			}
		} else if (d->maxplaybackrate < 24000) {
			if (maxBandwidth != OPUS_BANDWIDTH_NARROWBAND) {
				maxBandwidth = OPUS_BANDWIDTH_WIDEBAND;
			}
		} else if (d->maxplaybackrate < 48000) {
			if (maxBandwidth == OPUS_BANDWIDTH_FULLBAND) {
				maxBandwidth = OPUS_BANDWIDTH_SUPERWIDEBAND;
			}
		}

		error = opus_encoder_ctl(d->state, OPUS_SET_MAX_BANDWIDTH(maxBandwidth));
		if (error != OPUS_OK) {
			ms_error("could not set max bandwidth to opus encoder: %s", opus_strerror(error));
		}
	}

}

static int ms_opus_enc_set_sample_rate(MSFilter *f, void *arg) {
	OpusEncData *d = (OpusEncData *)f->data;
	int samplerate = *((int *)arg);
	/* check values: supported are 8, 12, 16, 24 and 48 kHz */
	switch (samplerate) {
		case 8000:case 12000:case 16000:case 24000:case 48000:
			d->samplerate=samplerate;
			break;
		default:
			ms_error("Opus encoder got unsupported sample rate of %d, switch to default 48kHz",samplerate);
			d->samplerate=48000;
	}
	return 0;
}

static int ms_opus_enc_get_sample_rate(MSFilter *f, void *arg) {
	OpusEncData *d = (OpusEncData *)f->data;
	*((int *)arg) = d->samplerate;
	return 0;
}

static int ms_opus_enc_set_bitrate(MSFilter *f, void *arg) {
	OpusEncData *d = (OpusEncData *)f->data;
	int ptimeStepValue = 0;
	int ptimeStepSign = 1;
	int ptimeTarget = d->ptime;
	int bitrate = *((int *)arg); // the argument is the network bitrate requested


	/* this function also manage the ptime, check if we are increasing or decreasing the bitrate in order to possibly decrease or increase ptime */
	if (d->bitrate>0 && d->ptime>0) { /* at first call to set_bitrate(bitrate is initialised at -1), do not modify ptime, neither if it wasn't initialised too */
		if (bitrate > d->max_network_bitrate ) {
			ptimeStepSign = -1;
		}

		/* if the change requested is higher than the expected modification gained by changing ptime, do it */
		/* target ptime is 20 more or less than current one, but in range [20,120] */
		ptimeTarget = d->ptime+ptimeStepSign*20;
		if (ptimeTarget<20) {
			ptimeTarget = 20;
		}
		if (ptimeTarget>120) {
			ptimeTarget = 120;
		}

		// ABS(difference of current and requested bitrate)   ABS(current network bandwidth overhead - new ptime target overhead)
		if ( (d->max_network_bitrate - bitrate)*ptimeStepSign > (((40*8*1000)/d->ptime - (40*8*1000)/ptimeTarget) * ptimeStepSign)) {
			ptimeStepValue = 20;
		}
	}


	d->max_network_bitrate = bitrate;
	ms_message("opus setbitrate to %d",d->max_network_bitrate);
	ms_filter_lock(f);
	compute_max_bitrate(d, ptimeStepValue*ptimeStepSign);
	apply_max_bitrate(d);
	ms_filter_unlock(f);
	return 0;
}

static int ms_opus_enc_get_bitrate(MSFilter *f, void *arg) {
	OpusEncData *d = (OpusEncData *)f->data;
	*((int *)arg) = d->max_network_bitrate;
	return 0;
}

/* ptime can be set using this function only at first call (between init and preprocess) */
/* other calls will return -1 to force the adaptative bit rate to use the set bitrate function */
/* the ptime is managed by the set bitrate function which increase ptime only at low bitrate */
static int ms_opus_enc_set_ptime(MSFilter *f, void *arg) {
	OpusEncData *d = (OpusEncData *)f->data;
	int retval = -1;
	int ptime = *(int*)arg;

	ms_filter_lock(f);
	if (d->ptime==-1) {
		/* ptime value can be 20, 40, 60, 80, 100 or 120 */
		if ((ptime%20 != 0) || (ptime>120) || (ptime<20)) {
			d->ptime = ptime-ptime%20;
			if (d->ptime<20) {
				d->ptime=20;
			}
			if (d->ptime>120) {
				d->ptime=120;
			}
			ms_warning("Opus encoder doesn't support ptime [%i]( 20 multiple in range [20,120] only) set to %d", ptime, d->ptime);
		} else {
			d->ptime=ptime;
			ms_message ( "Opus enc: got ptime=%i",d->ptime );
		}
		/* network bitrate is affected by ptime, only if bitrate was already set */
		if (d->bitrate!=-1) {
			d->max_network_bitrate = ((d->bitrate*d->ptime/8000) + 12 + 8 + 20) *8000/d->ptime;
		}
		retval=0;
	}
	ms_filter_unlock(f);
	return retval;
}

static int ms_opus_enc_get_ptime(MSFilter *f, void *arg) {
	OpusEncData *d = (OpusEncData *)f->data;
	*((int *)arg) = d->ptime;
	return 0;
}

static int ms_opus_enc_set_nchannels(MSFilter *f, void *arg) {
	OpusEncData *d = (OpusEncData *)f->data;
	d->channels = *(int*)arg;
	return 0;
}

static int ms_opus_enc_set_vbr(MSFilter *f) {
	OpusEncData *d = (OpusEncData *)f->data;
	int error;

	if (d->state) {
		error = opus_encoder_ctl(d->state, OPUS_SET_VBR(d->vbr));
		if (error != OPUS_OK) {
			ms_error("could not set VBR to opus encoder: %s", opus_strerror(error));
		}
	}

	return 0;
}

static int ms_opus_enc_set_inbandfec(MSFilter *f) {
	OpusEncData *d = (OpusEncData *)f->data;
	int error;

	if (d->state) {
		error = opus_encoder_ctl(d->state, OPUS_SET_INBAND_FEC(d->useinbandfec));
		if (error != OPUS_OK) {
			ms_error("could not set inband FEC to opus encoder: %s", opus_strerror(error));
		}
	}

	return 0;
}

static int ms_opus_enc_set_dtx(MSFilter *f) {
	OpusEncData *d = (OpusEncData *)f->data;
	int error;

	if (d->state) {
		error = opus_encoder_ctl(d->state, OPUS_SET_DTX(d->usedtx));
		if (error != OPUS_OK) {
			ms_error("could not set use DTX to opus encoder: %s", opus_strerror(error));
		}
	}

	return 0;
}

static int ms_opus_enc_add_fmtp(MSFilter *f, void *arg) {
	OpusEncData *d = (OpusEncData *)f->data;
	const char *fmtp = (const char *)arg;
	char buf[64]= {0};

	if ( fmtp_get_value ( fmtp,"maxplaybackrate",buf,sizeof ( buf ) ) ) {
		d->maxplaybackrate=atoi(buf);
	} else if ( fmtp_get_value ( fmtp,"maxptime",buf,sizeof ( buf ) ) ) {
		d->maxptime=MIN(atoi(buf),120);
	} else if ( fmtp_get_value ( fmtp,"ptime",buf,sizeof ( buf ) ) ) {
		int val=atoi(buf);
		ms_opus_enc_set_ptime(f,&val);
	} else if ( fmtp_get_value ( fmtp,"minptime",buf,sizeof ( buf ) ) ) {
		d->minptime=MAX(atoi(buf),20); // minimum shall be 3 but we do not provide less than 20ms ptime.
	} else if ( fmtp_get_value ( fmtp,"maxaveragebitrate",buf,sizeof ( buf ) ) ) {
		d->maxaveragebitrate = atoi(buf);
	} else if ( fmtp_get_value ( fmtp,"stereo",buf,sizeof ( buf ) ) ) {
		d->stereo = atoi(buf);
	} else if ( fmtp_get_value ( fmtp,"cbr",buf,sizeof ( buf ) ) ) {
		if (atoi(buf) == 1 ) {
			d->vbr = 0;
		} else {
			d->vbr = 1;
		}
		ms_opus_enc_set_vbr(f);
	} else if ( fmtp_get_value ( fmtp,"useinbandfec",buf,sizeof ( buf ) ) ) {
		d->useinbandfec = atoi(buf);
	} else if ( fmtp_get_value ( fmtp,"usedtx",buf,sizeof ( buf ) ) ) {
		d->usedtx = atoi(buf);
	}

	return 0;
}

static MSFilterMethod ms_opus_enc_methods[] = {
	{	MS_FILTER_SET_SAMPLE_RATE,	ms_opus_enc_set_sample_rate	},
	{	MS_FILTER_GET_SAMPLE_RATE,	ms_opus_enc_get_sample_rate	},
	{	MS_FILTER_SET_BITRATE,		ms_opus_enc_set_bitrate		},
	{	MS_FILTER_GET_BITRATE,		ms_opus_enc_get_bitrate		},
	{	MS_FILTER_ADD_FMTP,			ms_opus_enc_add_fmtp		},
	{	MS_AUDIO_ENCODER_SET_PTIME,	ms_opus_enc_set_ptime		},
	{	MS_AUDIO_ENCODER_GET_PTIME,	ms_opus_enc_get_ptime		},
	{	MS_FILTER_SET_NCHANNELS		,	ms_opus_enc_set_nchannels},
	{	0,				NULL				}
};


/******************************************************************************
 * Definition of the opus encoder                                             *
 *****************************************************************************/

#define MS_OPUS_ENC_NAME	"MSOpusEnc"
#define MS_OPUS_ENC_DESCRIPTION	"An opus encoder."
#define MS_OPUS_ENC_CATEGORY	MS_FILTER_ENCODER
#define MS_OPUS_ENC_ENC_FMT	"opus"
#define MS_OPUS_ENC_NINPUTS	1
#define MS_OPUS_ENC_NOUTPUTS	1
#define MS_OPUS_ENC_FLAGS	0

#ifndef _MSC_VER

MSFilterDesc ms_opus_enc_desc = {
	.id = MS_OPUS_ENC_ID,
	.name = MS_OPUS_ENC_NAME,
	.text = MS_OPUS_ENC_DESCRIPTION,
	.category = MS_OPUS_ENC_CATEGORY,
	.enc_fmt = MS_OPUS_ENC_ENC_FMT,
	.ninputs = MS_OPUS_ENC_NINPUTS,
	.noutputs = MS_OPUS_ENC_NOUTPUTS,
	.init = ms_opus_enc_init,
	.preprocess = ms_opus_enc_preprocess,
	.process = ms_opus_enc_process,
	.postprocess = ms_opus_enc_postprocess,
	.uninit = ms_opus_enc_uninit,
	.methods = ms_opus_enc_methods,
	.flags = MS_OPUS_ENC_FLAGS
};

#else

MSFilterDesc ms_opus_enc_desc = {
	MS_OPUS_ENC_ID,
	MS_OPUS_ENC_NAME,
	MS_OPUS_ENC_DESCRIPTION,
	MS_OPUS_ENC_CATEGORY,
	MS_OPUS_ENC_ENC_FMT,
	MS_OPUS_ENC_NINPUTS,
	MS_OPUS_ENC_NOUTPUTS,
	ms_opus_enc_init,
	ms_opus_enc_preprocess,
	ms_opus_enc_process,
	ms_opus_enc_postprocess,
	ms_opus_enc_uninit,
	ms_opus_enc_methods,
	MS_OPUS_ENC_FLAGS
};

#endif

MS_FILTER_DESC_EXPORT(ms_opus_enc_desc)




/**
 * Definition of the private data structure of the opus decoder.
 */
typedef struct _OpusDecData {
	OpusDecoder *state;
	int samplerate;
	int channels;

	/* concealment properties */
	MSConcealerContext *concealer;
	MSRtpPayloadPickerContext rtp_picker_context;
	int sequence_number;
	int lastPacketLength;
	bool_t plc;
	int statsfec;
	int statsplc;

} OpusDecData;


/******************************************************************************
 * Methods to (de)initialize and run the opus decoder                         *
 *****************************************************************************/

static void ms_opus_dec_init(MSFilter *f) {
	OpusDecData *d = (OpusDecData *)ms_new(OpusDecData, 1);
	d->state = NULL;
	d->samplerate = 48000;
	d->channels = 1;
	d->lastPacketLength = 20;
	d->statsfec = 0;
	d->statsplc = 0;
	f->data = d;
}

static void ms_opus_dec_preprocess(MSFilter *f) {
	int error;
	OpusDecData *d = (OpusDecData *)f->data;
	d->state = opus_decoder_create(d->samplerate, d->channels, &error);
	if (error != OPUS_OK) {
		ms_error("Opus decoder creation failed: %s", opus_strerror(error));
	}
	/* initialise the concealer context */
	d->concealer = ms_concealer_context_new(UINT32_MAX);
}

static void ms_opus_dec_process(MSFilter *f) {
	OpusDecData *d = (OpusDecData *)f->data;
	mblk_t *im;
	mblk_t *om;
	int frames;

	/* decode available packets */
	while ((im = ms_queue_get(f->inputs[0])) != NULL) {
		om = allocb(5760 * d->channels * SIGNAL_SAMPLE_SIZE, 0); /* 5760 is the maximum number of sample in a packet (120ms at 48KHz) */

		frames = opus_decode(d->state, (const unsigned char *)im->b_rptr, im->b_wptr - im->b_rptr, (opus_int16 *)om->b_wptr, 5760, 0);

		if (frames < 0) {
			ms_warning("Opus decoder error: %s", opus_strerror(frames));
			freemsg(om);
		} else {
			d->lastPacketLength = frames; // store the packet length for eventual PLC if next two packets are missing
			om->b_wptr += frames * d->channels * SIGNAL_SAMPLE_SIZE;
			ms_queue_put(f->outputs[0], om);
			d->sequence_number = mblk_get_cseq(im); // used to get eventual FEC information if next packet is missing
			ms_concealer_inc_sample_time(d->concealer,f->ticker->time, frames*1000/d->samplerate, 1);
		}
		freemsg(im);
	}

	/* Concealment if needed */
	if (ms_concealer_context_is_concealement_required(d->concealer, f->ticker->time)) {
		int imLength = 0;
		im = NULL;
		uint8_t *payload = NULL;

		// try fec : info are stored in the next packet, do we have it?
		if (d->rtp_picker_context.picker) {
			im = d->rtp_picker_context.picker(&d->rtp_picker_context,d->sequence_number+1);
			if (im) {
				imLength=rtp_get_payload(im,&payload);
				d->statsfec++;
			} else {
				d->statsplc++;
			}
		}
		om = allocb(5760 * d->channels * SIGNAL_SAMPLE_SIZE, 0); /* 5760 is the maximum number of sample in a packet (120ms at 48KHz) */
		/* call to the decoder, we'll have either FEC or PLC, do it on the same length that last received packet */
		if (payload) { // found frame to try FEC
			frames = opus_decode(d->state, payload, imLength, (opus_int16 *)om->b_wptr, d->lastPacketLength, 1);
		} else { // do PLC: PLC doesn't seem to be able to generate more than 960 samples (20 ms at 48000 Hz), get PLC until we have the correct number of sample
			//frames = opus_decode(d->state, NULL, 0, (opus_int16 *)om->b_wptr, d->lastPacketLength, 0); // this should have work if opus_decode returns the requested number of samples
			frames = 0;
			while (frames < d->lastPacketLength) {
				frames += opus_decode(d->state, NULL, 0, (opus_int16 *)(om->b_wptr + (frames*d->channels*SIGNAL_SAMPLE_SIZE)), d->lastPacketLength-frames, 0);
			}
		}
		if (frames < 0) {
			ms_warning("Opus decoder error in concealment: %s", opus_strerror(frames));
			freemsg(om);
		} else {
			om->b_wptr += frames * d->channels * SIGNAL_SAMPLE_SIZE;
			ms_queue_put(f->outputs[0], om);
			d->sequence_number++;
			ms_concealer_inc_sample_time(d->concealer,f->ticker->time, frames*1000/d->samplerate, 0);
		}
	}

}

static void ms_opus_dec_postprocess(MSFilter *f) {
	OpusDecData *d = (OpusDecData *)f->data;
	ms_message("opus decoder stats: fec %d packets - plc %d packets.", d->statsfec, d->statsplc);
	opus_decoder_destroy(d->state);
	d->state = NULL;
}

static void ms_opus_dec_uninit(MSFilter *f) {
	OpusDecData *d = (OpusDecData *)f->data;
	if (d == NULL) return;
	if (d->state) {
		opus_decoder_destroy(d->state);
		d->state = NULL;
	}
	ms_free(d);
}


/******************************************************************************
 * Methods to configure the opus decoder                                      *
 *****************************************************************************/

static int ms_opus_dec_set_sample_rate(MSFilter *f, void *arg) {
	OpusDecData *d = (OpusDecData *)f->data;
	d->samplerate = *((int *)arg);
	return 0;
}

static int ms_opus_dec_get_sample_rate(MSFilter *f, void *arg) {
	OpusDecData *d = (OpusDecData *)f->data;
	*((int *)arg) = d->samplerate;
	return 0;
}

static int ms_opus_dec_add_fmtp(MSFilter *f, void *arg) {
	OpusDecData *d = (OpusDecData *)f->data;
	const char *fmtp = (const char *)arg;
	char buf[32];

	memset(buf, '\0', sizeof(buf));
	if (fmtp_get_value(fmtp, "plc", buf, sizeof(buf))) {
		d->plc = atoi(buf);
	}
	return 0;
}

static int ms_opus_set_rtp_picker(MSFilter *f, void *arg) {
	OpusDecData *d = (OpusDecData *)f->data;
	d->rtp_picker_context=*(MSRtpPayloadPickerContext*)arg;
	return 0;
}

static int ms_opus_dec_have_plc(MSFilter *f, void *arg) {
	*((int *)arg) = 1;
	return 0;
}

static int ms_opus_dec_set_nchannels(MSFilter *f, void *arg) {
	OpusDecData *d = (OpusDecData *)f->data;
	d->channels=*(int*)arg;
	return 0;
}

static MSFilterMethod ms_opus_dec_methods[] = {
	{	MS_FILTER_SET_SAMPLE_RATE,	ms_opus_dec_set_sample_rate	},
	{	MS_FILTER_GET_SAMPLE_RATE,	ms_opus_dec_get_sample_rate	},
	{	MS_FILTER_ADD_FMTP,		ms_opus_dec_add_fmtp		},
	{	MS_FILTER_SET_RTP_PAYLOAD_PICKER,	ms_opus_set_rtp_picker	},
	{ 	MS_DECODER_HAVE_PLC,		ms_opus_dec_have_plc		},
	{	MS_FILTER_SET_NCHANNELS		,	ms_opus_dec_set_nchannels},
	{	0,				NULL				}
};


/******************************************************************************
 * Definition of the opus decoder                                             *
 *****************************************************************************/

#define MS_OPUS_DEC_NAME	"MSOpusDec"
#define MS_OPUS_DEC_DESCRIPTION	"An opus decoder."
#define MS_OPUS_DEC_CATEGORY	MS_FILTER_DECODER
#define MS_OPUS_DEC_ENC_FMT	"opus"
#define MS_OPUS_DEC_NINPUTS	1
#define MS_OPUS_DEC_NOUTPUTS	1
#define MS_OPUS_DEC_FLAGS	0

#ifndef _MSC_VER

MSFilterDesc ms_opus_dec_desc = {
	.id = MS_OPUS_DEC_ID,
	.name = MS_OPUS_DEC_NAME,
	.text = MS_OPUS_DEC_DESCRIPTION,
	.category = MS_OPUS_DEC_CATEGORY,
	.enc_fmt = MS_OPUS_DEC_ENC_FMT,
	.ninputs = MS_OPUS_DEC_NINPUTS,
	.noutputs = MS_OPUS_DEC_NOUTPUTS,
	.init = ms_opus_dec_init,
	.preprocess = ms_opus_dec_preprocess,
	.process = ms_opus_dec_process,
	.postprocess = ms_opus_dec_postprocess,
	.uninit = ms_opus_dec_uninit,
	.methods = ms_opus_dec_methods,
	.flags = MS_OPUS_DEC_FLAGS
};

#else

MSFilterDesc ms_opus_dec_desc = {
	MS_OPUS_DEC_ID,
	MS_OPUS_DEC_NAME,
	MS_OPUS_DEC_DESCRIPTION,
	MS_OPUS_DEC_CATEGORY,
	MS_OPUS_DEC_ENC_FMT,
	MS_OPUS_DEC_NINPUTS,
	MS_OPUS_DEC_NOUTPUTS,
	ms_opus_dec_init,
	ms_opus_dec_preprocess,
	ms_opus_dec_process,
	ms_opus_dec_postprocess,
	ms_opus_dec_uninit,
	ms_opus_dec_methods,
	MS_OPUS_DEC_FLAGS
};

#endif

MS_FILTER_DESC_EXPORT(ms_opus_dec_desc)

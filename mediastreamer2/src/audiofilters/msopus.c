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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/


#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/mscodecutils.h"
#include "mediastreamer2/msticker.h"
#include "ortp/rtp.h"

#include <stdint.h>
#if defined(MS2_WINDOWS_PHONE)
#include <opus.h>
#else
#include <opus/opus.h>
#endif

#define SIGNAL_SAMPLE_SIZE  2 // 2 bytes per sample

/* Define codec specific settings */
#define MAX_BYTES_PER_MS	25  // Equals peak bitrate of 200 kbps
#define MAX_INPUT_FRAMES	5   // The maximum amount of Opus frames in a packet we are using


/**
 * Definition of the private data structure of the opus encoder.
 */
typedef struct _OpusEncData {
	OpusEncoder *state;
	MSBufferizer *bufferizer;
	uint32_t ts;
	uint8_t *pcmbuffer;
	int pcmbufsize;
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
	bool_t ptime_set;

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
	OpusEncData *d = (OpusEncData *)ms_new0(OpusEncData, 1);
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
	d->ptime = 20;
	d->minptime = 20; // defaut shall be 3, but we never use less than 20.
	d->maxaveragebitrate = -1;
	d->stereo = 0; /*stereo=0 is the default mode in opus rtp draft*/
	d->channels = 1;
	d->vbr = 1;
	d->useinbandfec = 0;
	d->usedtx = 0;

	f->data = d;
}

static void ms_opus_enc_preprocess(MSFilter *f) {
	int error;
	int opusComplexity = -1;
	const char *env = NULL;

	OpusEncData *d = (OpusEncData *)f->data;
	/* create the encoder */
	d->state = opus_encoder_create(d->samplerate, d->channels, d->application, &error);
	if (error != OPUS_OK) {
		ms_error("Opus encoder creation failed: %s", opus_strerror(error));
		return;
	}

	
#ifndef MS2_WINDOWS_UNIVERSAL
	env = getenv("MS2_OPUS_COMPLEXITY");
#endif
	if (env != NULL) {
		opusComplexity = atoi(env);
		if (opusComplexity < -1)
			opusComplexity = -1; /*our default value*/
		if (opusComplexity > 10)
			opusComplexity = 10;
	}
	if (opusComplexity == -1){
#if defined(__arm__) || defined(_M_ARM)
		int cpucount = ms_factory_get_cpu_count(f->factory);
		if (cpucount == 1){
			opusComplexity = 0; /* set complexity to 0 for single processor arm devices */ 
		}else if (cpucount == 2) {
			opusComplexity = 5; 
		}
#endif
	}
	if (opusComplexity != -1){
		ms_message("Set Opus complexity to %d", opusComplexity);
		opus_encoder_ctl(d->state, OPUS_SET_COMPLEXITY(opusComplexity));
	} /*otherwise we let opus with its default value, which is 9*/
	
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
		if (d->channels == 2) ms_message("Opus encoder configured to encode mono despite it is feed with stereo.");
	}else if (d->channels == 2){
		ms_message("Opus encoder configured to encode stereo.");
	}

	ms_filter_lock(f);
	// set bitrate wasn't call, compute it with the default network bitrate (36000)
	if (d->bitrate==-1) {
		compute_max_bitrate(d, 0);
	}
	apply_max_bitrate(d);
	ms_filter_unlock(f);
}

static void ms_opus_enc_process(MSFilter *f) {
	OpusEncData *d = (OpusEncData *)f->data;
	OpusRepacketizer *repacketizer = NULL;
	mblk_t *om = NULL;
	int packet_size, pcm_buffer_size;
	int max_frame_byte_size, ptime = 20;
	int frame_count = 0, frame_size = 0;
	opus_int32 total_length = 0;
	uint8_t *repacketizer_frame_buffer[MAX_INPUT_FRAMES] = {NULL};
	int i;
	ms_filter_lock(f);
	ptime = d->ptime;
	packet_size = d->samplerate * ptime / 1000; /* in samples */
	ms_filter_unlock(f);
	
	switch (ptime) {
		case 10:
			frame_size = d->samplerate * 10 / 1000;
			frame_count = 1;
			break;
		case 20:
			frame_size = d->samplerate * 20 / 1000;
			frame_count = 1;
			break;
		case 40:
			frame_size = d->samplerate * 40 / 1000;
			frame_count = 1;
			break;
		case 60:
			frame_size = d->samplerate * 60 / 1000;
			frame_count = 1;
			break;
		case 80:
			frame_size = d->samplerate * 40 / 1000;
			frame_count = 2;
			break;
		case 100:
			frame_size = d->samplerate * 20 / 1000;
			frame_count = 5;
			break;
		case 120:
			frame_size = d->samplerate * 60 / 1000;
			frame_count = 2;
			break;
		default:
			frame_size = d->samplerate * 20 / 1000;
			frame_count = 1;
	}

	max_frame_byte_size = MAX_BYTES_PER_MS * ptime/frame_count;

	pcm_buffer_size = d->channels * frame_size * SIGNAL_SAMPLE_SIZE;
	if (pcm_buffer_size > d->pcmbufsize){
		if (d->pcmbuffer) ms_free(d->pcmbuffer);
		d->pcmbuffer = ms_malloc(pcm_buffer_size);
		d->pcmbufsize = pcm_buffer_size;
	}

	ms_bufferizer_put_from_queue(d->bufferizer, f->inputs[0]);
	while (ms_bufferizer_get_avail(d->bufferizer) >= (size_t)(d->channels * packet_size * SIGNAL_SAMPLE_SIZE)) {
		opus_int32 ret = 0;

		if (frame_count == 1) { /* One Opus frame, not using the repacketizer */
			om = allocb(max_frame_byte_size, 0);
			ms_bufferizer_read(d->bufferizer, d->pcmbuffer, frame_size * SIGNAL_SAMPLE_SIZE * d->channels);
			ret = opus_encode(d->state, (opus_int16 *)d->pcmbuffer, frame_size, om->b_wptr, max_frame_byte_size);
			if (ret < 0) {
				freemsg(om);
				om=NULL;
				ms_error("Opus encoder error: %s", opus_strerror(ret));
				break;
			} else {
				total_length = ret;
				om->b_wptr += total_length;
			}
		} else if(frame_count > 1) { /* We have multiple Opus frames we will use the opus repacketizer */

			repacketizer = opus_repacketizer_create();
			opus_repacketizer_init(repacketizer);

			/* Do not include FEC/LBRR in any frame after the first one since it will be sent with the previous one */
			ret = opus_encoder_ctl(d->state, OPUS_SET_INBAND_FEC(0));
			if (ret != OPUS_OK) {
				ms_error("could not set inband FEC to opus encoder: %s", opus_strerror(ret));
			}
			for (i=0; i<frame_count; i++) {
				if(frame_count == i+1){ /* if configured, reactivate FEC on the last frame to tell the encoder he should restart saving LBRR frames */
					ret = opus_encoder_ctl(d->state, OPUS_SET_INBAND_FEC(d->useinbandfec));
					if (ret != OPUS_OK) {
						ms_error("could not set inband FEC to opus encoder: %s", opus_strerror(ret));
					}
				}
				if (!repacketizer_frame_buffer[i]) repacketizer_frame_buffer[i] = ms_malloc(max_frame_byte_size); /* the repacketizer need the pointer to packet to remain valid, so we shall have a buffer for each coded frame */
				ms_bufferizer_read(d->bufferizer, d->pcmbuffer, frame_size * SIGNAL_SAMPLE_SIZE * d->channels);
				ret = opus_encode(d->state, (opus_int16 *)d->pcmbuffer, frame_size, repacketizer_frame_buffer[i], max_frame_byte_size);
				if (ret < 0) {
					ms_error("Opus encoder error: %s", opus_strerror(ret));
					break;
				} else if (ret > 0) {
					int err = opus_repacketizer_cat(repacketizer, repacketizer_frame_buffer[i], ret); /* add the encoded frame into the current packet */
					if (err != OPUS_OK) {
						ms_error("Opus repacketizer error: %s", opus_strerror(err));
						break;
					}
					total_length += ret;
				}
			}

			om = allocb(total_length + frame_count + 1, 0); /* opus repacketizer API: allocate at least number of frame + size of all data added before */
			ret = opus_repacketizer_out(repacketizer, om->b_wptr, total_length+frame_count);
			if(ret < 0){
				freemsg(om);
				om=NULL;
				ms_error("Opus repacketizer out error: %s", opus_strerror(ret));
			} else {
				om->b_wptr += ret;
			}
			opus_repacketizer_destroy(repacketizer);
		}

		if(om) { /* we have an encoded output message */
			mblk_set_timestamp_info(om, d->ts);
			ms_bufferizer_fill_current_metas(d->bufferizer, om);
			ms_queue_put(f->outputs[0], om);
			d->ts += packet_size*48000/d->samplerate; /* RFC payload RTP opus 03 - section 4: RTP timestamp multiplier : WARNING works only with sr at 48000 */
			total_length = 0;
		}
	}
	
	for (i=0; i<frame_count; i++) {
		if (repacketizer_frame_buffer[i] != NULL) {
			ms_free(repacketizer_frame_buffer[i]);
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
	if(d->pcmbuffer) ms_free(d->pcmbuffer);
	ms_free(d);
}


/******************************************************************************
 * Methods to configure the opus encoder                                      *
 *****************************************************************************/
/* the compute max_bitrate function will also modify the ptime according to requested bitrate */
static void compute_max_bitrate(OpusEncData *d, int ptimeStep) {
	int normalized_cbr = 0;
	float pps;
	int maxaveragebitrate=510000;

	pps = 1000.0f / d->ptime;

	/* if have a potentiel ptime modification suggested by caller, check ptime and bitrate for adjustment */
	if (ptimeStep != 0) {
		normalized_cbr = (int)(((((float)d->max_network_bitrate) / (pps * 8)) - 20 - 12 -8) * pps * 8);
		if (normalized_cbr<12000) {
			if (d->ptime<d->maxptime || (ptimeStep<0 && d->ptime>40)) {
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

		pps = 1000.0f / d->ptime;
	}

	normalized_cbr = (int)(((((float)d->max_network_bitrate) / (pps * 8)) - 20 - 12 -8) * pps * 8);
	/* check if bitrate is in range [6,510kbps] */
	if (normalized_cbr<6000) {
		int initial_value = normalized_cbr;
		normalized_cbr = 6000;
		d->max_network_bitrate = (int)((normalized_cbr/(pps*8) + 12 + 8 + 20) *8*pps);
		ms_warning("Opus encoder does not support bitrate [%i]. Instead set to 6kbps, network bitrate [%d]", initial_value, d->max_network_bitrate);
	}
	if (d->maxaveragebitrate>0)
		maxaveragebitrate=d->maxaveragebitrate;

	if (normalized_cbr>maxaveragebitrate) {
		int initial_value = normalized_cbr;
		normalized_cbr = maxaveragebitrate;
		d->max_network_bitrate = (int)((normalized_cbr/(pps*8) + 12 + 8 + 20) *8*pps);
		ms_warning("Opus encoder cannot set codec bitrate to [%i] because of maxaveragebitrate constraint or absolute maximum bitrate value. New network bitrate is [%i]", initial_value, d->max_network_bitrate);
	}
	ms_message("MSOpusEnc: codec bitrate set to [%i] with ptime [%i]", normalized_cbr, d->ptime);
	d->bitrate = normalized_cbr;
}


static void apply_max_bitrate(OpusEncData *d) {
	ms_message("Setting opus codec bitrate to [%i] from network bitrate [%i] with ptime [%i]", d->bitrate, d->max_network_bitrate, d->ptime);
	/* give the bitrate to the encoder if exists*/
	if (d->state) {
		opus_int32 maxBandwidth=0;

		/*tell the target bitrate, opus will choose internally the bandwidth to use*/
		int error = opus_encoder_ctl(d->state, OPUS_SET_BITRATE(d->bitrate));
		if (error != OPUS_OK) {
			ms_error("could not set bit rate to opus encoder: %s", opus_strerror(error));
		}

		/* implement maxplaybackrate parameter, which is constraint on top of bitrate */
		if (d->maxplaybackrate <= 8000) {
			maxBandwidth = OPUS_BANDWIDTH_NARROWBAND;
		} else if (d->maxplaybackrate <= 12000) {
			maxBandwidth = OPUS_BANDWIDTH_MEDIUMBAND;
		} else if (d->maxplaybackrate <= 16000) {
			maxBandwidth = OPUS_BANDWIDTH_WIDEBAND;
		} else if (d->maxplaybackrate <= 24000) {
			maxBandwidth = OPUS_BANDWIDTH_SUPERWIDEBAND;
		} else {
			maxBandwidth = OPUS_BANDWIDTH_FULLBAND;
		}

		if (maxBandwidth!=0){
			error = opus_encoder_ctl(d->state, OPUS_SET_MAX_BANDWIDTH(maxBandwidth));
			if (error != OPUS_OK) {
				ms_error("could not set max bandwidth to opus encoder: %s", opus_strerror(error));
			}
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

	/* If ptime isn't set explicitely,
	 * this function also manage the ptime, check if we are increasing or decreasing the bitrate in order to possibly decrease or increase ptime */
	if (d->bitrate>0 && !d->ptime_set) {
		if (bitrate > d->max_network_bitrate ) {
			ptimeStepSign = -1;
		}

		/* if the change requested is higher than the expected modification gained by changing ptime, do it */
		/* target ptime is 20 more or less than current one, but in range [20,120] */
		ptimeTarget = d->ptime+ptimeStepSign*20;
		if (ptimeTarget<20) {
			ptimeTarget = 20;
		}
		if (ptimeTarget>d->maxptime) {
			ptimeTarget = d->maxptime;
		}

		// ABS(difference of current and requested bitrate)   ABS(current network bandwidth overhead - new ptime target overhead)
		if ( (d->max_network_bitrate - bitrate)*ptimeStepSign > (((40*8*1000)/d->ptime - (40*8*1000)/ptimeTarget) * ptimeStepSign)) {
			ptimeStepValue = 20;
		}
	}
	d->max_network_bitrate = bitrate;
	ms_message("opus setbitrate to %d",d->max_network_bitrate);

	if (d->bitrate>0) { /*don't apply bitrate before prepocess*/
		ms_filter_lock(f);
		compute_max_bitrate(d, ptimeStepValue*ptimeStepSign);
		apply_max_bitrate(d);
		ms_filter_unlock(f);
	}

	return 0;
}

static int ms_opus_enc_get_bitrate(MSFilter *f, void *arg) {
	OpusEncData *d = (OpusEncData *)f->data;
	*((int *)arg) = d->max_network_bitrate;
	return 0;
}

/* ptime can be set using this function only at first call (between init and preprocess) */
/* other calls will return -1 to force the adaptive bit rate to use the set bitrate function */
/* the ptime is managed by the set bitrate function which increase ptime only at low bitrate */
static int ms_opus_enc_set_ptime(MSFilter *f, void *arg) {
	OpusEncData *d = (OpusEncData *)f->data;
	int retval = -1;
	int ptime = *(int*)arg;

	ms_filter_lock(f);

	/* ptime value can be 20, 40, 60, 80, 100 or 120 */
	if ((ptime%20 != 0) || (ptime>d->maxptime) || (ptime<20)) {
		d->ptime = ptime-ptime%20;
		if (d->ptime<20) {
			d->ptime=20;
		}
		if (d->ptime>d->maxptime) {
			d->ptime=d->maxptime;
		}
		ms_warning("Opus encoder doesn't support ptime [%i]( 20 multiple in range [20,%i] only) set to %d", ptime, d->maxptime, d->ptime);
	} else {
		d->ptime=ptime;
		ms_message ( "Opus enc: got ptime=%i",d->ptime );
	}
	if (d->bitrate!=-1) {
		d->max_network_bitrate = ((d->bitrate*d->ptime/8000) + 12 + 8 + 20) *8000/d->ptime;
	}
	d->ptime_set=TRUE;
	retval=0;
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

static int ms_opus_enc_get_nchannels(MSFilter *f, void *arg) {
	OpusEncData *d = (OpusEncData *)f->data;
	*(int*)arg = d->channels;
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
	}
	if ( fmtp_get_value ( fmtp,"maxptime",buf,sizeof ( buf ) ) ) {
		d->maxptime=MIN(atoi(buf),120);
	}
	if ( fmtp_get_value ( fmtp,"ptime",buf,sizeof ( buf ) ) ) {
		int val=atoi(buf);
		ms_opus_enc_set_ptime(f,&val);
	}
	if ( fmtp_get_value ( fmtp,"minptime",buf,sizeof ( buf ) ) ) {
		d->minptime=MAX(atoi(buf),20); // minimum shall be 3 but we do not provide less than 20ms ptime.
	}
	if ( fmtp_get_value ( fmtp,"maxaveragebitrate",buf,sizeof ( buf ) ) ) {
		d->maxaveragebitrate = atoi(buf);
	}
	if ( fmtp_get_value ( fmtp,"stereo",buf,sizeof ( buf ) ) ) {
		d->stereo = atoi(buf);
	}
	if ( fmtp_get_value ( fmtp,"cbr",buf,sizeof ( buf ) ) ) {
		if (atoi(buf) == 1 ) {
			d->vbr = 0;
		} else {
			d->vbr = 1;
		}
		ms_opus_enc_set_vbr(f);
	}
	if ( fmtp_get_value ( fmtp,"useinbandfec",buf,sizeof ( buf ) ) ) {
		d->useinbandfec = atoi(buf);
	}
	if ( fmtp_get_value ( fmtp,"usedtx",buf,sizeof ( buf ) ) ) {
		d->usedtx = atoi(buf);
	}

	return 0;
}

static int ms_opus_enc_get_capabilities(MSFilter *f, void *arg){
	*(int*)arg=MS_AUDIO_ENCODER_CAP_AUTO_PTIME;
	return 0;
}

static MSFilterMethod ms_opus_enc_methods[] = {
	{	MS_FILTER_SET_SAMPLE_RATE,	ms_opus_enc_set_sample_rate	},
	{	MS_FILTER_GET_SAMPLE_RATE,	ms_opus_enc_get_sample_rate	},
	{	MS_FILTER_SET_BITRATE,		ms_opus_enc_set_bitrate		},
	{	MS_FILTER_GET_BITRATE,		ms_opus_enc_get_bitrate		},
	{	MS_FILTER_ADD_FMTP,		ms_opus_enc_add_fmtp		},
	{	MS_AUDIO_ENCODER_SET_PTIME,	ms_opus_enc_set_ptime		},
	{	MS_AUDIO_ENCODER_GET_PTIME,	ms_opus_enc_get_ptime		},
	{	MS_FILTER_SET_NCHANNELS	,	ms_opus_enc_set_nchannels	},
	{	MS_FILTER_GET_NCHANNELS	,	ms_opus_enc_get_nchannels	},
	{	MS_AUDIO_ENCODER_GET_CAPABILITIES,	ms_opus_enc_get_capabilities },
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
	OpusDecData *d = (OpusDecData *)ms_new0(OpusDecData, 1);
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

	if (!d->state) ms_queue_flush(f->inputs[0]);

	/* decode available packets */
	while ((im = ms_queue_get(f->inputs[0])) != NULL) {
		om = allocb(5760 * d->channels * SIGNAL_SAMPLE_SIZE, 0); /* 5760 is the maximum number of sample in a packet (120ms at 48KHz) */

		frames = opus_decode(d->state, (const unsigned char *)im->b_rptr, (opus_int32)(im->b_wptr - im->b_rptr), (opus_int16 *)om->b_wptr, 5760, 0);

		if (frames < 0) {
			ms_warning("Opus decoder error: %s", opus_strerror(frames));
			freemsg(om);
		} else {
			d->lastPacketLength = frames; // store the packet length for eventual PLC if next two packets are missing
			om->b_wptr += frames * d->channels * SIGNAL_SAMPLE_SIZE;
			mblk_meta_copy(im,om);
			ms_queue_put(f->outputs[0], om);
			/*ms_message("Opus: outputing a normal frame of %i bytes (%i samples,%i ms)",(int)(om->b_wptr-om->b_rptr),frames,frames*1000/d->samplerate);*/
			d->sequence_number = mblk_get_cseq(im); // used to get eventual FEC information if next packet is missing
			ms_concealer_inc_sample_time(d->concealer,f->ticker->time, frames*1000/d->samplerate, 1);
		}
		freemsg(im);
	}

	/* Concealment if needed */
	if (ms_concealer_context_is_concealement_required(d->concealer, f->ticker->time)) {
		int imLength = 0;
		uint8_t *payload = NULL;
		im = NULL;

		// try fec : info are stored in the next packet, do we have it?
		if (d->rtp_picker_context.picker) {
			/* FEC information is in the next packet, last valid packet was d->sequence_number, the missing one shall then be d->sequence_number+1, so check jitter buffer for d->sequence_number+2 */
			/* but we may have the n+1 packet in the buffer and adaptative jitter control keeping it for later, in that case, just go for PLC */
			if (d->rtp_picker_context.picker(&d->rtp_picker_context,d->sequence_number+1) == NULL) { /* missing packet is really missing */
				im = d->rtp_picker_context.picker(&d->rtp_picker_context,d->sequence_number+2); /* try to get n+2 */
				if (im) {
					imLength=rtp_get_payload(im,&payload);
				}
			}
		}
		om = allocb(5760 * d->channels * SIGNAL_SAMPLE_SIZE, 0); /* 5760 is the maximum number of sample in a packet (120ms at 48KHz) */
		/* call to the decoder, we'll have either FEC or PLC, do it on the same length that last received packet */
		if (payload) { // found frame to try FEC
			d->statsfec++;
			frames = opus_decode(d->state, payload, imLength, (opus_int16 *)om->b_wptr, d->lastPacketLength, 1);
		} else { // do PLC: PLC doesn't seem to be able to generate more than 960 samples (20 ms at 48000 Hz), get PLC until we have the correct number of sample
			//frames = opus_decode(d->state, NULL, 0, (opus_int16 *)om->b_wptr, d->lastPacketLength, 0); // this should have work if opus_decode returns the requested number of samples
			d->statsplc++;
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
			/*ms_message("Opus: outputing a PLC frame of %i bytes (%i samples,%i ms)",(int)(om->b_wptr-om->b_rptr),frames,frames*1000/d->samplerate);*/
			mblk_set_plc_flag(om,TRUE);
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
	ms_concealer_context_destroy(d->concealer);
	d->concealer=NULL;
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

static int ms_opus_dec_get_nchannels(MSFilter *f, void *arg) {
	OpusDecData *d = (OpusDecData *)f->data;
	*(int*)arg=d->channels;
	return 0;
}

static MSFilterMethod ms_opus_dec_methods[] = {
	{	MS_FILTER_SET_SAMPLE_RATE,	ms_opus_dec_set_sample_rate	},
	{	MS_FILTER_GET_SAMPLE_RATE,	ms_opus_dec_get_sample_rate	},
	{	MS_FILTER_ADD_FMTP,		ms_opus_dec_add_fmtp		},
	{	MS_FILTER_SET_RTP_PAYLOAD_PICKER,	ms_opus_set_rtp_picker	},
	{	MS_AUDIO_DECODER_SET_RTP_PAYLOAD_PICKER, ms_opus_set_rtp_picker },
	{ 	MS_AUDIO_DECODER_HAVE_PLC,		ms_opus_dec_have_plc	},
	{	MS_FILTER_SET_NCHANNELS,	ms_opus_dec_set_nchannels	},
	{	MS_FILTER_GET_NCHANNELS,	ms_opus_dec_get_nchannels	},
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
#define MS_OPUS_DEC_FLAGS	MS_FILTER_IS_PUMP

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

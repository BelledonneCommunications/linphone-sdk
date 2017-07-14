/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2012  Belledonne Communications, Grenoble, France

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#ifdef BUILD_AEC
#include "echo_cancellation.h"
#include "aec_splitting_filter.h"
#endif
#ifdef BUILD_AECM
#include "echo_control_mobile.h"
#endif
#include "ortp/b64.h"

#ifdef _WIN32
#include <malloc.h> /* for alloca */
#endif

//#define EC_DUMP 1
#ifdef ANDROID
#define EC_DUMP_PREFIX "/sdcard"
#else
#define EC_DUMP_PREFIX "/dynamic/tests"
#endif

#include "mediastreamer2/flowcontrol.h"

static const float smooth_factor = 0.05f;
static const int framesize = 80;


typedef enum _WebRTCAECType {
	WebRTCAECTypeNormal,
	WebRTCAECTypeMobile
} WebRTCAECType;

typedef struct WebRTCAECState {
	void *aecInst;
	MSBufferizer delayed_ref;
	MSFlowControlledBufferizer ref;
	MSBufferizer echo;
	int framesize;
	int samplerate;
	int delay_ms;
	int nominal_ref_samples;
	char *state_str;
#ifdef EC_DUMP
	FILE *echofile;
	FILE *reffile;
	FILE *cleanfile;
#endif
	bool_t echostarted;
	bool_t bypass_mode;
	bool_t using_zeroes;
	WebRTCAECType aec_type;
#ifdef BUILD_AEC
	MSWebRtcAecSplittingFilter *splitting_filter;
#endif
} WebRTCAECState;

static void webrtc_aecgeneric_init(MSFilter *f, WebRTCAECType aec_type) {
	WebRTCAECState *s = (WebRTCAECState *) ms_new0(WebRTCAECState, 1);

	s->samplerate = 8000;
	ms_bufferizer_init(&s->delayed_ref);
	ms_bufferizer_init(&s->echo);
	ms_flow_controlled_bufferizer_init(&s->ref, f, s->samplerate, 1);
	s->delay_ms = 0;
	s->aecInst = NULL;
	s->framesize = framesize;
	s->state_str = NULL;
	s->using_zeroes = FALSE;
	s->echostarted = FALSE;
	s->bypass_mode = FALSE;
	s->aec_type = aec_type;

#ifdef EC_DUMP
	{
		char *fname = ms_strdup_printf("%s/mswebrtcaec-%p-echo.raw", EC_DUMP_PREFIX, f);
		s->echofile = fopen(fname, "w");
		ms_free(fname);
		fname = ms_strdup_printf("%s/mswebrtcaec-%p-ref.raw", EC_DUMP_PREFIX, f);
		s->reffile = fopen(fname, "w");
		ms_free(fname);
		fname = ms_strdup_printf("%s/mswebrtcaec-%p-clean.raw", EC_DUMP_PREFIX, f);
		s->cleanfile = fopen(fname, "w");
		ms_free(fname);
	}
#endif

	f->data = s;
}

#ifdef BUILD_AEC
static void webrtc_aec_init(MSFilter *f) {
	webrtc_aecgeneric_init(f, WebRTCAECTypeNormal);
}
#endif

#ifdef BUILD_AECM
static void webrtc_aecm_init(MSFilter *f) {
	webrtc_aecgeneric_init(f, WebRTCAECTypeMobile);
}
#endif

static void webrtc_aec_uninit(MSFilter *f) {
	WebRTCAECState *s = (WebRTCAECState *) f->data;
	if (s->state_str) ms_free(s->state_str);
	ms_bufferizer_uninit(&s->delayed_ref);
#ifdef EC_DUMP
	if (s->echofile)
		fclose(s->echofile);
	if (s->reffile)
		fclose(s->reffile);
#endif
	ms_free(s);
}

static void configure_flow_controlled_bufferizer(WebRTCAECState *s) {
	ms_flow_controlled_bufferizer_set_samplerate(&s->ref, s->samplerate);
	ms_flow_controlled_bufferizer_set_max_size_ms(&s->ref, s->delay_ms);
	ms_flow_controlled_bufferizer_set_granularity_ms(&s->ref, (s->framesize * 1000) / s->samplerate);
}

static void webrtc_aec_preprocess(MSFilter *f) {
	WebRTCAECState *s = (WebRTCAECState *) f->data;
#ifdef BUILD_AEC
	AecConfig aec_config;
#endif
#ifdef BUILD_AECM
	AecmConfig aecm_config;
	int error_code;
#endif
	int delay_samples = 0;
	mblk_t *m;

	s->echostarted = FALSE;
	delay_samples = s->delay_ms * s->samplerate / 1000;
	s->framesize=(framesize*s->samplerate)/8000;
	ms_message("Initializing WebRTC echo canceler with framesize=%i, delay_ms=%i, delay_samples=%i", s->framesize, s->delay_ms, delay_samples);
	configure_flow_controlled_bufferizer(s);

#ifdef BUILD_AEC
	if (s->aec_type == WebRTCAECTypeNormal) {
		if ((s->aecInst = WebRtcAec_Create()) == NULL) {
			s->bypass_mode = TRUE;
			ms_error("WebRtcAec_Create(): error, entering bypass mode");
			return;
		}
		if ((WebRtcAec_Init(s->aecInst, MIN(48000, s->samplerate), s->samplerate)) < 0) {
			ms_error("WebRtcAec_Init(): WebRTC echo canceller does not support %d samplerate", s->samplerate);
			s->bypass_mode = TRUE;
			ms_error("Entering bypass mode");
			return;
		}
		aec_config.nlpMode = kAecNlpAggressive;
		aec_config.skewMode = kAecFalse;
		aec_config.metricsMode = kAecFalse;
		aec_config.delay_logging = kAecFalse;
		if (WebRtcAec_set_config(s->aecInst, aec_config) != 0) {
			ms_error("WebRtcAec_set_config(): failed.");
		}
	}
#endif
#ifdef BUILD_AECM
	if (s->aec_type == WebRTCAECTypeMobile) {
		if ((s->aecInst = WebRtcAecm_Create()) == NULL) {
			s->bypass_mode = TRUE;
			ms_error("WebRtcAecm_Create(): error, entering bypass mode");
			return;
		}
		if ((error_code = WebRtcAecm_Init(s->aecInst, s->samplerate)) < 0) {
			if (error_code == AECM_BAD_PARAMETER_ERROR) {
				ms_error("WebRtcAecm_Init(): WebRTC echo canceller does not support %d samplerate", s->samplerate);
			}
			s->bypass_mode = TRUE;
			ms_error("Entering bypass mode");
			return;
		}
		aecm_config.cngMode = TRUE;
		aecm_config.echoMode = 3;
		if (WebRtcAecm_set_config(s->aecInst, aecm_config)!=0){
			ms_error("WebRtcAecm_set_config(): failed.");
		}
	}
#endif

	/* fill with zeroes for the time of the delay*/
	m = allocb(delay_samples * 2, 0);
	m->b_wptr += delay_samples * 2;
	ms_bufferizer_put(&s->delayed_ref, m);
	s->nominal_ref_samples = delay_samples;
}

/*	inputs[0]= reference signal from far end (sent to soundcard)
 *	inputs[1]= near speech & echo signal (read from soundcard)
 *	outputs[0]=  is a copy of inputs[0] to be sent to soundcard
 *	outputs[1]=  near end speech, echo removed - towards far end
*/
static void webrtc_aec_process(MSFilter *f) {
	WebRTCAECState *s = (WebRTCAECState *) f->data;
	int nbytes = s->framesize * sizeof(int16_t);
	mblk_t *refm;
	int16_t *ref, *echo;
	int nbands = 1;
	int bandsize = s->framesize;

	if (s->bypass_mode) {
		while ((refm = ms_queue_get(f->inputs[0])) != NULL) {
			ms_queue_put(f->outputs[0], refm);
		}
		while ((refm = ms_queue_get(f->inputs[1])) != NULL) {
			ms_queue_put(f->outputs[1], refm);
		}
		return;
	}

	if (f->inputs[0] != NULL) {
		if (s->echostarted) {
			while ((refm = ms_queue_get(f->inputs[0])) != NULL) {
				mblk_t *cp=dupmsg(refm);
				ms_bufferizer_put(&s->delayed_ref,cp);
				ms_flow_controlled_bufferizer_put(&s->ref,refm);
			}
		} else {
			ms_warning("Getting reference signal but no echo to synchronize on.");
			ms_queue_flush(f->inputs[0]);
		}
	}

	ms_bufferizer_put_from_queue(&s->echo, f->inputs[1]);

	ref = (int16_t *) alloca(nbytes);
	echo = (int16_t *) alloca(nbytes);
#ifdef BUILD_AEC
	if (s->aec_type == WebRTCAECTypeNormal) {
		if (s->samplerate > 16000) {
			nbands = s->samplerate / 16000;
			bandsize = 160;
		}
		if (!s->splitting_filter) {
			s->splitting_filter = mswebrtc_aec_splitting_filter_create(nbands, bandsize);
		}
	}
#endif
	while (ms_bufferizer_read(&s->echo, (uint8_t *)echo, (size_t)nbytes) >= (size_t)nbytes) {
		mblk_t *oecho = allocb(nbytes, 0);
		int avail;
		int avail_samples;

		if (!s->echostarted) s->echostarted = TRUE;
		if ((avail = ms_bufferizer_get_avail(&s->delayed_ref)) < ((s->nominal_ref_samples * 2) + nbytes)) {
			/*we don't have enough to read in a reference signal buffer, inject silence instead*/
			refm = allocb(nbytes, 0);
			memset(refm->b_wptr, 0, nbytes);
			refm->b_wptr += nbytes;
			ms_bufferizer_put(&s->delayed_ref, refm);
			ms_queue_put(f->outputs[0], dupmsg(refm));
			if (!s->using_zeroes) {
				ms_warning("Not enough ref samples, using zeroes");
				s->using_zeroes = TRUE;
			}
		} else {
			if (s->using_zeroes) {
				ms_message("Samples are back.");
				s->using_zeroes = FALSE;
			}
			/* read from our no-delay buffer and output */
			refm = allocb(nbytes, 0);
			if (ms_flow_controlled_bufferizer_read(&s->ref, refm->b_wptr, nbytes) == 0) {
				ms_fatal("Should never happen");
			}
			refm->b_wptr += nbytes;
			ms_queue_put(f->outputs[0], refm);
		}

		/*now read a valid buffer of delayed ref samples*/
		if (ms_bufferizer_read(&s->delayed_ref, (uint8_t *)ref, nbytes) == 0) {
			ms_fatal("Should never happen");
		}
		avail -= nbytes;
		avail_samples = avail / 2;

#ifdef EC_DUMP
		if (s->reffile)
			fwrite(ref, nbytes, 1, s->reffile);
		if (s->echofile)
			fwrite(echo, nbytes, 1, s->echofile);
#endif
#ifdef BUILD_AEC
		if (s->aec_type == WebRTCAECTypeNormal) {
			mswebrtc_aec_splitting_filter_analysis(s->splitting_filter, ref, echo);
			if (WebRtcAec_BufferFarend(s->aecInst,
					mswebrtc_aec_splitting_filter_get_ref(s->splitting_filter),
					(size_t)mswebrtc_aec_splitting_filter_get_bandsize(s->splitting_filter)) != 0)
				ms_error("WebRtcAec_BufferFarend() failed.");
			if (WebRtcAec_Process(s->aecInst,
					mswebrtc_aec_splitting_filter_get_echo_bands(s->splitting_filter),
					mswebrtc_aec_splitting_filter_get_number_of_bands(s->splitting_filter),
					mswebrtc_aec_splitting_filter_get_output_bands(s->splitting_filter),
					(size_t)mswebrtc_aec_splitting_filter_get_bandsize(s->splitting_filter), 0, 0) != 0)
				ms_error("WebRtcAec_Process() failed.");
			mswebrtc_aec_splitting_filter_synthesis(s->splitting_filter, (int16_t *)oecho->b_wptr);
		}
#endif
#ifdef BUILD_AECM
		if (s->aec_type == WebRTCAECTypeMobile) {
			if (WebRtcAecm_BufferFarend(s->aecInst, ref, (size_t)s->framesize) != 0)
				ms_error("WebRtcAecm_BufferFarend() failed.");
			if (WebRtcAecm_Process(s->aecInst, echo, NULL, (int16_t *)oecho->b_wptr, (size_t)s->framesize, 0) != 0)
				ms_error("WebRtcAecm_Process() failed.");
		}
#endif
#ifdef EC_DUMP
		if (s->cleanfile)
			fwrite(oecho->b_wptr, nbytes, 1, s->cleanfile);
#endif
		oecho->b_wptr += nbytes;
		ms_queue_put(f->outputs[1], oecho);
	}
}

static void webrtc_aec_postprocess(MSFilter *f) {
	WebRTCAECState *s = (WebRTCAECState *) f->data;

	ms_bufferizer_flush(&s->delayed_ref);
	ms_bufferizer_flush(&s->echo);
	ms_flow_controlled_bufferizer_flush(&s->ref);
#ifdef BUILD_AEC
	if (s->splitting_filter) {
		mswebrtc_aec_splitting_filter_destroy(s->splitting_filter);
		s->splitting_filter = NULL;
	}
#endif
	if (s->aecInst != NULL) {
#ifdef BUILD_AEC
		if (s->aec_type == WebRTCAECTypeNormal) {
			WebRtcAec_Free(s->aecInst);
		}
#endif
#ifdef BUILD_AECM
		if (s->aec_type == WebRTCAECTypeMobile) {
			WebRtcAecm_Free(s->aecInst);
		}
#endif
		s->aecInst = NULL;
	}
}

static int webrtc_aec_set_sr(MSFilter *f, void *arg) {
	WebRTCAECState *s = (WebRTCAECState *) f->data;
	int requested_sr = *(int *) arg;
	int sr = requested_sr;

	if ((requested_sr != 8000) && (requested_sr != 16000) && (requested_sr != 32000) && (requested_sr != 48000)) {
		if ((s->aec_type == WebRTCAECTypeNormal) && (requested_sr > 48000)) {
			sr = 48000;
		} else if ((s->aec_type == WebRTCAECTypeNormal) && (requested_sr > 32000)) {
			sr = 32000;
		} else if (requested_sr > 16000) {
			sr = 16000;
		} else {
			sr = 8000;
		}
		ms_message("Webrtc aec does not support sampling rate %i, using %i instead", requested_sr, sr);
	}
	s->samplerate = sr;
	configure_flow_controlled_bufferizer(s);
	return 0;
}

static int webrtc_aec_get_sr(MSFilter *f, void *arg) {
	WebRTCAECState *s = (WebRTCAECState *) f->data;
	*(int *) arg=s->samplerate;
	return 0;
}

static int webrtc_aec_set_framesize(MSFilter *f, void *arg) {
	/* Do nothing because the WebRTC echo canceller only accept specific values: 80 and 160. We use 80 at 8khz, and 160 at 16khz */
	return 0;
}

static int webrtc_aec_set_delay(MSFilter *f, void *arg) {
	WebRTCAECState *s = (WebRTCAECState *) f->data;
	s->delay_ms = *(int *) arg;
	configure_flow_controlled_bufferizer(s);
	return 0;
}

static int webrtc_aec_set_tail_length(MSFilter *f, void *arg) {
	/* Do nothing because this is not needed by the WebRTC echo canceller. */
	return 0;
}
static int webrtc_aec_set_bypass_mode(MSFilter *f, void *arg) {
	WebRTCAECState *s = (WebRTCAECState *) f->data;
	s->bypass_mode = *(bool_t *) arg;
	ms_message("set EC bypass mode to [%i]", s->bypass_mode);
	return 0;
}
static int webrtc_aec_get_bypass_mode(MSFilter *f, void *arg) {
	WebRTCAECState *s = (WebRTCAECState *) f->data;
	*(bool_t *) arg = s->bypass_mode;
	return 0;
}

static int webrtc_aec_set_state(MSFilter *f, void *arg) {
	WebRTCAECState *s = (WebRTCAECState *) f->data;
	s->state_str = ms_strdup((const char *) arg);
	return 0;
}

static int webrtc_aec_get_state(MSFilter *f, void *arg) {
	WebRTCAECState *s = (WebRTCAECState *) f->data;
	*(char **) arg = s->state_str;
	return 0;
}

static MSFilterMethod webrtc_aec_methods[] = {
	{	MS_FILTER_SET_SAMPLE_RATE		,	webrtc_aec_set_sr 		},
	{	MS_FILTER_GET_SAMPLE_RATE		,	webrtc_aec_get_sr 		},
	{	MS_ECHO_CANCELLER_SET_TAIL_LENGTH	,	webrtc_aec_set_tail_length	},
	{	MS_ECHO_CANCELLER_SET_DELAY		,	webrtc_aec_set_delay		},
	{	MS_ECHO_CANCELLER_SET_FRAMESIZE		,	webrtc_aec_set_framesize	},
	{	MS_ECHO_CANCELLER_SET_BYPASS_MODE	,	webrtc_aec_set_bypass_mode	},
	{	MS_ECHO_CANCELLER_GET_BYPASS_MODE	,	webrtc_aec_get_bypass_mode	},
	{	MS_ECHO_CANCELLER_GET_STATE_STRING	,	webrtc_aec_get_state		},
	{	MS_ECHO_CANCELLER_SET_STATE_STRING	,	webrtc_aec_set_state		}
};


#ifdef BUILD_AEC

#define MS_WEBRTC_AEC_NAME        "MSWebRTCAEC"
#define MS_WEBRTC_AEC_DESCRIPTION "Echo canceller using WebRTC library."
#define MS_WEBRTC_AEC_CATEGORY    MS_FILTER_OTHER
#define MS_WEBRTC_AEC_ENC_FMT     NULL
#define MS_WEBRTC_AEC_NINPUTS     2
#define MS_WEBRTC_AEC_NOUTPUTS    2
#define MS_WEBRTC_AEC_FLAGS       0

#ifdef _MSC_VER

MSFilterDesc ms_webrtc_aec_desc = {
	MS_FILTER_PLUGIN_ID,
	MS_WEBRTC_AEC_NAME,
	MS_WEBRTC_AEC_DESCRIPTION,
	MS_WEBRTC_AEC_CATEGORY,
	MS_WEBRTC_AEC_ENC_FMT,
	MS_WEBRTC_AEC_NINPUTS,
	MS_WEBRTC_AEC_NOUTPUTS,
	webrtc_aec_init,
	webrtc_aec_preprocess,
	webrtc_aec_process,
	webrtc_aec_postprocess,
	webrtc_aec_uninit,
	webrtc_aec_methods,
	MS_WEBRTC_AEC_FLAGS
};

#else

MSFilterDesc ms_webrtc_aec_desc = {
	.id = MS_FILTER_PLUGIN_ID,
	.name = MS_WEBRTC_AEC_NAME,
	.text = MS_WEBRTC_AEC_DESCRIPTION,
	.category = MS_WEBRTC_AEC_CATEGORY,
	.enc_fmt = MS_WEBRTC_AEC_ENC_FMT,
	.ninputs = MS_WEBRTC_AEC_NINPUTS,
	.noutputs = MS_WEBRTC_AEC_NOUTPUTS,
	.init = webrtc_aec_init,
	.preprocess = webrtc_aec_preprocess,
	.process = webrtc_aec_process,
	.postprocess = webrtc_aec_postprocess,
	.uninit = webrtc_aec_uninit,
	.methods = webrtc_aec_methods,
	.flags = MS_WEBRTC_AEC_FLAGS
};

#endif

MS_FILTER_DESC_EXPORT(ms_webrtc_aec_desc)

#endif /* BUILD_AEC */

#ifdef BUILD_AECM

#define MS_WEBRTC_AECM_NAME        "MSWebRTCAECM"
#define MS_WEBRTC_AECM_DESCRIPTION "Echo canceller for mobile using WebRTC library."
#define MS_WEBRTC_AECM_CATEGORY    MS_FILTER_OTHER
#define MS_WEBRTC_AECM_ENC_FMT     NULL
#define MS_WEBRTC_AECM_NINPUTS     2
#define MS_WEBRTC_AECM_NOUTPUTS    2
#define MS_WEBRTC_AECM_FLAGS       0

#ifdef _MSC_VER

MSFilterDesc ms_webrtc_aecm_desc = {
	MS_FILTER_PLUGIN_ID,
	MS_WEBRTC_AECM_NAME,
	MS_WEBRTC_AECM_DESCRIPTION,
	MS_WEBRTC_AECM_CATEGORY,
	MS_WEBRTC_AECM_ENC_FMT,
	MS_WEBRTC_AECM_NINPUTS,
	MS_WEBRTC_AECM_NOUTPUTS,
	webrtc_aecm_init,
	webrtc_aec_preprocess,
	webrtc_aec_process,
	webrtc_aec_postprocess,
	webrtc_aec_uninit,
	webrtc_aec_methods,
	MS_WEBRTC_AECM_FLAGS
};

#else

MSFilterDesc ms_webrtc_aecm_desc = {
	.id = MS_FILTER_PLUGIN_ID,
	.name = MS_WEBRTC_AECM_NAME,
	.text = MS_WEBRTC_AECM_DESCRIPTION,
	.category = MS_WEBRTC_AECM_CATEGORY,
	.enc_fmt = MS_WEBRTC_AECM_ENC_FMT,
	.ninputs = MS_WEBRTC_AECM_NINPUTS,
	.noutputs = MS_WEBRTC_AECM_NOUTPUTS,
	.init = webrtc_aecm_init,
	.preprocess = webrtc_aec_preprocess,
	.process = webrtc_aec_process,
	.postprocess = webrtc_aec_postprocess,
	.uninit = webrtc_aec_uninit,
	.methods = webrtc_aec_methods,
	.flags = MS_WEBRTC_AECM_FLAGS
};

#endif

MS_FILTER_DESC_EXPORT(ms_webrtc_aecm_desc)

#endif /* BUILD_AECM */

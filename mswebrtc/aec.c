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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include <echo_control_mobile.h>
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

static const float smooth_factor = 0.05;
static const int framesize = 80;
static const int flow_control_interval_ms = 5000;


typedef struct WebRTCAECState {
	void *aecmInst;
	MSBufferizer delayed_ref;
	MSBufferizer ref;
	MSBufferizer echo;
	int framesize;
	int samplerate;
	int delay_ms;
	int nominal_ref_samples;
	int min_ref_samples;
	MSAudioFlowController afc;
	uint64_t flow_control_time;
	char *state_str;
#ifdef EC_DUMP
	FILE *echofile;
	FILE *reffile;
	FILE *cleanfile;
#endif
	bool_t echostarted;
	bool_t bypass_mode;
	bool_t using_zeroes;
} WebRTCAECState;

static void webrtc_aec_init(MSFilter *f)
{
	WebRTCAECState *s = (WebRTCAECState *) ms_new(WebRTCAECState, 1);

	s->samplerate = 8000;
	ms_bufferizer_init(&s->delayed_ref);
	ms_bufferizer_init(&s->echo);
	ms_bufferizer_init(&s->ref);
	s->delay_ms = 0;
	s->aecmInst = NULL;
	s->framesize = framesize;
	s->state_str = NULL;
	s->using_zeroes = FALSE;
	s->echostarted = FALSE;
	s->bypass_mode = FALSE;

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

static void webrtc_aec_uninit(MSFilter *f)
{
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

static void webrtc_aec_preprocess(MSFilter *f)
{
	WebRTCAECState *s = (WebRTCAECState *) f->data;
	AecmConfig config;
	int delay_samples = 0;
	mblk_t *m;
	int error_code;

	s->echostarted = FALSE;
	delay_samples = s->delay_ms * s->samplerate / 1000;
	s->framesize=(framesize*s->samplerate)/8000;
	ms_message("Initializing WebRTC echo canceler with framesize=%i, delay_ms=%i, delay_samples=%i", s->framesize, s->delay_ms, delay_samples);

	if ((s->aecmInst = WebRtcAecm_Create()) == NULL) {
		s->bypass_mode = TRUE;
		ms_error("WebRtcAecm_Create(): error, entering bypass mode");
		return;
	}
	if ((error_code = WebRtcAecm_Init(s->aecmInst, s->samplerate)) < 0) {
		if (error_code == AECM_BAD_PARAMETER_ERROR) {
			ms_error("WebRtcAecm_Init(): WebRTC echo canceller does not support %d samplerate", s->samplerate);
		}
		s->bypass_mode = TRUE;
		ms_error("Entering bypass mode");
		return;
	}
	config.cngMode = TRUE;
	config.echoMode = 3;
	if (WebRtcAecm_set_config(s->aecmInst, config)!=0){
		ms_error("WebRtcAecm_set_config(): failed.");
	}

	/* fill with zeroes for the time of the delay*/
	m = allocb(delay_samples * 2, 0);
	m->b_wptr += delay_samples * 2;
	ms_bufferizer_put(&s->delayed_ref, m);
	s->min_ref_samples = -1;
	s->nominal_ref_samples = delay_samples;
	ms_audio_flow_controller_init(&s->afc);
	s->flow_control_time = f->ticker->time;
}

/*	inputs[0]= reference signal from far end (sent to soundcard)
 *	inputs[1]= near speech & echo signal (read from soundcard)
 *	outputs[0]=  is a copy of inputs[0] to be sent to soundcard
 *	outputs[1]=  near end speech, echo removed - towards far end
*/
static void webrtc_aec_process(MSFilter *f)
{
	WebRTCAECState *s = (WebRTCAECState *) f->data;
	int nbytes = s->framesize * 2;
	mblk_t *refm;
	uint8_t *ref, *echo;

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
				refm=ms_audio_flow_controller_process(&s->afc,refm);
				if (refm){
					mblk_t *cp=dupmsg(refm);
					ms_bufferizer_put(&s->delayed_ref,cp);
					ms_bufferizer_put(&s->ref,refm);
				}
			}
		} else {
			ms_warning("Getting reference signal but no echo to synchronize on.");
			ms_queue_flush(f->inputs[0]);
		}
	}

	ms_bufferizer_put_from_queue(&s->echo, f->inputs[1]);

	ref = (uint8_t *) alloca(nbytes);
	echo = (uint8_t *) alloca(nbytes);
	while (ms_bufferizer_read(&s->echo, echo, nbytes) >= nbytes) {
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
			if (ms_bufferizer_read(&s->ref, refm->b_wptr, nbytes) == 0) {
				ms_fatal("Should never happen");
			}
			refm->b_wptr += nbytes;
			ms_queue_put(f->outputs[0], refm);
		}

		/*now read a valid buffer of delayed ref samples*/
		if (ms_bufferizer_read(&s->delayed_ref, ref, nbytes) == 0) {
			ms_fatal("Should never happen");
		}
		avail -= nbytes;
		avail_samples = avail / 2;
		if (avail_samples < s->min_ref_samples || s->min_ref_samples == -1) {
			s->min_ref_samples = avail_samples;
		}

#ifdef EC_DUMP
		if (s->reffile)
			fwrite(ref, nbytes, 1, s->reffile);
		if (s->echofile)
			fwrite(echo, nbytes, 1, s->echofile);
#endif
		if (WebRtcAecm_BufferFarend(s->aecmInst, (const int16_t *) ref, s->framesize)!=0)
			ms_error("WebRtcAecm_BufferFarend() failed.");
		if (WebRtcAecm_Process(s->aecmInst, (const int16_t *) echo, NULL, (int16_t *) oecho->b_wptr, s->framesize, 0)!=0)
			ms_error("WebRtcAecm_Process() failed.");
#ifdef EC_DUMP
		if (s->cleanfile)
			fwrite(oecho->b_wptr, nbytes, 1, s->cleanfile);
#endif
		oecho->b_wptr += nbytes;
		ms_queue_put(f->outputs[1], oecho);
	}

	/*verify our ref buffer does not become too big, meaning that we are receiving more samples than we are sending*/
	if ((((uint32_t) (f->ticker->time - s->flow_control_time)) >= flow_control_interval_ms) && (s->min_ref_samples != -1)) {
		int diff = s->min_ref_samples - s->nominal_ref_samples;
		if (diff > (nbytes / 2)) {
			int purge = diff - (nbytes / 2);
			ms_warning("echo canceller: we are accumulating too much reference signal, need to throw out %i samples", purge);
			ms_audio_flow_controller_set_target(&s->afc, purge, (flow_control_interval_ms * s->samplerate) / 1000);
		}
		s->min_ref_samples = -1;
		s->flow_control_time = f->ticker->time;
	}
}

static void webrtc_aec_postprocess(MSFilter *f)
{
	WebRTCAECState *s = (WebRTCAECState *) f->data;

	ms_bufferizer_flush(&s->delayed_ref);
	ms_bufferizer_flush(&s->echo);
	ms_bufferizer_flush(&s->ref);
	if (s->aecmInst != NULL) {
		WebRtcAecm_Free(s->aecmInst);
		s->aecmInst = NULL;
	}
}

static int webrtc_aec_set_sr(MSFilter *f, void *arg)
{
	WebRTCAECState *s = (WebRTCAECState *) f->data;
	int requested_sr = *(int *) arg;
	int sr = requested_sr;

	if (requested_sr != 8000 && requested_sr != 16000) {
		if (requested_sr > 16000) sr = 16000;
		else sr = 8000;
		ms_message("Webrtc aec does not support sampling rate %i, using %i instead", requested_sr, sr);
	}
	s->samplerate = sr;
	return 0;
}

static int webrtc_aec_get_sr(MSFilter *f, void *arg)
{
	WebRTCAECState *s = (WebRTCAECState *) f->data;
	*(int *) arg=s->samplerate;
	return 0;
}

static int webrtc_aec_set_framesize(MSFilter *f, void *arg)
{
	/* Do nothing because the WebRTC echo canceller only accept specific values: 80 and 160. We use 80 at 8khz, and 160 at 16khz */
	return 0;
}

static int webrtc_aec_set_delay(MSFilter *f, void *arg)
{
	WebRTCAECState *s = (WebRTCAECState *) f->data;
	s->delay_ms = *(int *) arg;
	return 0;
}

static int webrtc_aec_set_tail_length(MSFilter *f, void *arg)
{
	/* Do nothing because this is not needed by the WebRTC echo canceller. */
	return 0;
}
static int webrtc_aec_set_bypass_mode(MSFilter *f, void *arg)
{
	WebRTCAECState *s = (WebRTCAECState *) f->data;
	s->bypass_mode = *(bool_t *) arg;
	ms_message("set EC bypass mode to [%i]", s->bypass_mode);
	return 0;
}
static int webrtc_aec_get_bypass_mode(MSFilter *f, void *arg)
{
	WebRTCAECState *s = (WebRTCAECState *) f->data;
	*(bool_t *) arg = s->bypass_mode;
	return 0;
}

static int webrtc_aec_set_state(MSFilter *f, void *arg)
{
	WebRTCAECState *s = (WebRTCAECState *) f->data;
	s->state_str = ms_strdup((const char *) arg);
	return 0;
}

static int webrtc_aec_get_state(MSFilter *f, void *arg)
{
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

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

#if defined(HAVE_CONFIG_H)
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include <echo_control_mobile.h>
#include "ortp/b64.h"

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#ifdef WIN32
#include <malloc.h> /* for alloca */
#endif

typedef struct _AudioFlowController {
	int target_samples;
	int total_samples;
	int current_pos;
	int current_dropped;
} AudioFlowController;

static void audio_flow_controller_init(AudioFlowController *ctl)
{
	ctl->target_samples = 0;
	ctl->total_samples = 0;
	ctl->current_pos = 0;
	ctl->current_dropped = 0;
}

static void audio_flow_controller_set_target(AudioFlowController *ctl, int samples_to_drop, int total_samples)
{
	ctl->target_samples = samples_to_drop;
	ctl->total_samples = total_samples;
	ctl->current_pos = 0;
	ctl->current_dropped = 0;
}

static void discard_well_choosed_samples(mblk_t *m, int nsamples, int todrop)
{
	int i;
	int16_t *samples = (int16_t *) m->b_rptr;
	int min_diff = 32768;
	int pos = 0;


#ifdef TWO_SAMPLES_CRITERIA
	for (i = 0; i < nsamples - 1; ++i) {
		int tmp = abs((int) samples[i] - (int) samples[i + 1]);
#else
	for (i = 0; i < nsamples - 2; ++i) {
		int tmp = abs((int) samples[i] - (int) samples[i + 1]) + abs((int) samples[i + 1] - (int) samples[i + 2]);
#endif
		if (tmp <= min_diff) {
			pos = i;
			min_diff = tmp;
		}
	}
	/*ms_message("min_diff=%i at pos %i",min_diff, pos);*/
#ifdef TWO_SAMPLES_CRITERIA
	memmove(samples + pos, samples + pos + 1, (nsamples - pos - 1) * 2);
#else
	memmove(samples + pos + 1, samples + pos + 2, (nsamples - pos - 2) * 2);
#endif

	todrop--;
	m->b_wptr -= 2;
	nsamples--;
	if (todrop > 0) {
		/*repeat the same process again*/
		discard_well_choosed_samples(m, nsamples, todrop);
	}
}

static mblk_t *audio_flow_controller_process(AudioFlowController *ctl, mblk_t *m)
{
	if (ctl->total_samples > 0 && ctl->target_samples > 0) {
		int nsamples = (m->b_wptr - m->b_rptr) / 2;
		if (ctl->target_samples * 16 > ctl->total_samples) {
			ms_warning("Too many samples to drop, dropping entire frames");
			m->b_wptr = m->b_rptr;
			ctl->current_pos += nsamples;
		} else {
			int th_dropped;
			int todrop;

			ctl->current_pos += nsamples;
			th_dropped = (ctl->target_samples * ctl->current_pos) / ctl->total_samples;
			todrop = th_dropped - ctl->current_dropped;
			if (todrop > 0) {
				if (todrop > nsamples) todrop = nsamples;
				discard_well_choosed_samples(m, nsamples, todrop);
				/*ms_message("th_dropped=%i, current_dropped=%i, %i samples dropped.",th_dropped,ctl->current_dropped,todrop);*/
				ctl->current_dropped += todrop;
			}
		}
		if (ctl->current_pos >= ctl->total_samples) ctl->target_samples = 0; /*stop discarding*/
	}
	return m;
}


//#define EC_DUMP 1
#ifdef ANDROID
#define EC_DUMP_PREFIX "/sdcard"
#else
#define EC_DUMP_PREFIX "/dynamic/tests"
#endif

static const float smooth_factor = 0.05;
static const int framesize = 160;
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
	AudioFlowController afc;
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

	s->echostarted = FALSE;
	delay_samples = s->delay_ms * s->samplerate / 1000;
	ms_message("Initializing WebRTC echo canceler with framesize=%i, delay_ms=%i, delay_samples=%i", s->framesize, s->delay_ms, delay_samples);

	if (WebRtcAecm_Create(&s->aecmInst) < 0) {
		s->bypass_mode = TRUE;
		return;
	}
	if (WebRtcAecm_Init(s->aecmInst, s->samplerate) < 0) {
		if (WebRtcAecm_get_error_code(s->aecmInst) == AECM_BAD_PARAMETER_ERROR) {
			ms_error("WebRTC echo canceller does not support %d samplerate", s->samplerate);
		}
		s->bypass_mode = TRUE;
		return;
	}
	config.cngMode = TRUE;
	config.echoMode = 3;
	WebRtcAecm_set_config(s->aecmInst, config);

	/* fill with zeroes for the time of the delay*/
	m = allocb(delay_samples * 2, 0);
	m->b_wptr += delay_samples * 2;
	ms_bufferizer_put(&s->delayed_ref, m);
	s->min_ref_samples = -1;
	s->nominal_ref_samples = delay_samples;
	audio_flow_controller_init(&s->afc);
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
				mblk_t *cp = dupmsg(audio_flow_controller_process(&s->afc, refm));
				ms_bufferizer_put(&s->delayed_ref, cp);
				ms_bufferizer_put(&s->ref, refm);
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
		WebRtcAecm_BufferFarend(s->aecmInst, (const WebRtc_Word16 *) ref, s->framesize);
		WebRtcAecm_Process(s->aecmInst, (const WebRtc_Word16 *) echo, NULL, (WebRtc_Word16 *) oecho->b_wptr, s->framesize, 0);
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
			audio_flow_controller_set_target(&s->afc, purge, (flow_control_interval_ms * s->samplerate) / 1000);
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
	s->samplerate = *(int *) arg;
	return 0;
}

static int webrtc_aec_set_framesize(MSFilter *f, void *arg)
{
	/* Do nothing because the WebRTC echo canceller only accept specific values: 80 and 160. We use 160. */
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
	{	MS_ECHO_CANCELLER_SET_TAIL_LENGTH	,	webrtc_aec_set_tail_length	},
	{	MS_ECHO_CANCELLER_SET_DELAY		,	webrtc_aec_set_delay		},
	{	MS_ECHO_CANCELLER_SET_FRAMESIZE		,	webrtc_aec_set_framesize	},
	{	MS_ECHO_CANCELLER_SET_BYPASS_MODE	,	webrtc_aec_set_bypass_mode	},
	{	MS_ECHO_CANCELLER_GET_BYPASS_MODE	,	webrtc_aec_get_bypass_mode	},
	{	MS_ECHO_CANCELLER_GET_STATE_STRING	,	webrtc_aec_get_state		},
	{	MS_ECHO_CANCELLER_SET_STATE_STRING	,	webrtc_aec_set_state		}
};

#ifdef _MSC_VER

MSFilterDesc ms_webrtc_aec_desc = {
	MS_WEBRTC_AEC_ID,
	"MSWebRTCAEC",
	N_("Echo canceller using WebRTC library"),
	MS_FILTER_OTHER,
	NULL,
	2,
	2,
	webrtc_aec_init,
	webrtc_aec_preprocess,
	webrtc_aec_process,
	webrtc_aec_postprocess,
	webrtc_aec_uninit,
	webrtc_aec_methods
};

#else

MSFilterDesc ms_webrtc_aec_desc = {
	.id = MS_WEBRTC_AEC_ID,
	.name = "MSWebRTCAEC",
	.text = N_("Echo canceller using WebRTC library"),
	.category = MS_FILTER_OTHER,
	.ninputs = 2,
	.noutputs = 2,
	.init = webrtc_aec_init,
	.preprocess = webrtc_aec_preprocess,
	.process = webrtc_aec_process,
	.postprocess = webrtc_aec_postprocess,
	.uninit = webrtc_aec_uninit,
	.methods = webrtc_aec_methods
};

#endif

MS_FILTER_DESC_EXPORT(ms_webrtc_aec_desc)

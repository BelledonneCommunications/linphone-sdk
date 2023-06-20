/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <bctoolbox/defs.h>

#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msvideorouter.h"
#include "vp8rtpfmt.h"

static bool_t is_vp8_key_frame(mblk_t *m) {
	uint8_t *p;

	if (m->b_cont) {
		/* When data comes directly from the VP8 encoder, the VP8 payload is the second of the mblk_t chain.*/
		return !(m->b_cont->b_rptr[0] & 1);
	}
	p = vp8rtpfmt_skip_payload_descriptor(m);

	if (!p) {
		ms_warning("MSVideoRouter: invalid vp8 payload descriptor.");
		return FALSE;
	}
	return !(p[0] & 1);
}

static bool_t is_h264_key_frame(BCTBX_UNUSED(mblk_t *frame)) {
	/*TODO - return TRUE for now so that transfer can start at any point of time.*/
	return TRUE;
}

static bool_t is_key_frame_dummy(BCTBX_UNUSED(mblk_t *frame)) {
	return TRUE;
}

typedef bool_t (*is_key_frame_func_t)(mblk_t *frame);

enum _IOState { STOPPED, RUNNING };

typedef enum _IOState IOState;

typedef struct _InputContext {
	IOState state;
	bool_t disabled;
	uint8_t ignore_cseq;
	uint16_t cur_seq; /* to detect discontinuties*/
	uint32_t cur_ts;  /* current timestamp, to detect beginning of frames */
	int seq_set;
	int key_frame_requested;
	mblk_t *key_frame_start;
} InputContext;

typedef struct _OutputContext {
	IOState state;
	uint32_t out_ts;
	uint32_t adjusted_out_ts;
	uint16_t out_seq;
	int next_source;
	int current_source;
	int link_source; // for active speaker. If link_source != -1, the output should be switched to the focus pin.
} OutputContext;

typedef struct RouterState {
	InputContext input_contexts[ROUTER_MAX_INPUT_CHANNELS];
	OutputContext output_contexts[ROUTER_MAX_OUTPUT_CHANNELS];
	int focus_pin;
	is_key_frame_func_t is_key_frame;
	int placeholder_pin;
} RouterState;

static void router_init(MSFilter *f) {
	RouterState *s = ms_new0(RouterState, 1);
	s->is_key_frame = is_key_frame_dummy;
	s->focus_pin = -1;
	s->placeholder_pin = -1;
	/* FIXME:
	 * Disable N-2 pin because it is linked to a VoidSource.
	 * Remove this assumption. Should be set by upper layer instead.
	 */
	s->input_contexts[ROUTER_MAX_INPUT_CHANNELS - 2].disabled = TRUE;

	for (int i = 0; i < f->desc->noutputs; i++) {
		/* All outputs must be unconfigured */
		s->output_contexts[i].next_source = -1;
		s->output_contexts[i].current_source = -1;
		s->output_contexts[i].link_source = -1;
	}
	f->data = s;
}

static void router_uninit(MSFilter *f) {
	RouterState *s = (RouterState *)f->data;
	ms_free(s);
}

/* Only called in the active speaker case */
static int elect_new_source(MSFilter *f, OutputContext *output_context) {
	RouterState *s = (RouterState *)f->data;
	int k;
	int current;

	if (output_context->link_source == -1) {
		ms_error("elect_new_source(): should be called only for active speaker case.");
		return output_context->next_source;
	}
	if (output_context->link_source != s->focus_pin && s->focus_pin != -1 && f->inputs[s->focus_pin] != NULL) {
		/* show the active speaker */
		output_context->next_source = s->focus_pin;
		return output_context->next_source;
	}
	if (output_context->next_source != -1) {
		current = output_context->next_source;
	} else if (output_context->current_source != -1) {
		current = output_context->current_source;
	} else current = output_context->link_source;

	/* show somebody else, but not us */
	for (k = current + 1; k < current + 1 + f->desc->ninputs; k++) {
		int next_pin = k % f->desc->ninputs;
		if (f->inputs[next_pin] && next_pin != output_context->link_source && next_pin != s->placeholder_pin &&
		    !s->input_contexts[next_pin].disabled) {
			output_context->next_source = next_pin;
			return output_context->next_source;
		}
	}
	/* Otherwise, show nothing. */
	output_context->next_source = -1;
	return output_context->next_source;
}

static int router_configure_output(MSFilter *f, void *data) {
	RouterState *s = (RouterState *)f->data;
	MSVideoRouterPinData *pd = (MSVideoRouterPinData *)data;
	ms_filter_lock(f);
	s->output_contexts[pd->output].current_source = pd->input;
	s->output_contexts[pd->output].link_source = pd->link_source;

	if (s->output_contexts[pd->output].link_source != -1) {
		/* Active speaker mode */
		s->output_contexts[pd->output].current_source = -1;
		if (s->focus_pin != s->output_contexts[pd->output].link_source) {
			s->output_contexts[pd->output].next_source = s->focus_pin;
		} else {
			/* we will elect another source in process() function */
			s->output_contexts[pd->output].next_source = -1;
		}
	}

	ms_filter_unlock(f);
	ms_message("%s: router configure link_source[%d] pin output %d with input %d, next_source %d", f->desc->name,
	           pd->link_source, pd->output, pd->input, s->output_contexts[pd->output].next_source);
	return 0;
}

static int router_unconfigure_output(MSFilter *f, void *data) {
	RouterState *s = (RouterState *)f->data;
	int pin = *(int *)data;
	ms_filter_lock(f);
	s->output_contexts[pin].current_source = -1;
	s->output_contexts[pin].link_source = -1;
	ms_filter_unlock(f);
	ms_message("%s: router unconfigure output pin %i ", f->desc->name, pin);
	return 0;
}

static void router_channel_update_input(RouterState *s, int pin, MSQueue *q) {
	InputContext *input_context = &s->input_contexts[pin];
	mblk_t *m;
	input_context->key_frame_start = NULL;

	for (m = ms_queue_peek_first(q); !ms_queue_end(q, m); m = ms_queue_peek_next(q, m)) {
		uint32_t new_ts = mblk_get_timestamp_info(m);
		uint16_t new_seq = mblk_get_cseq(m);
		// uint8_t marker = mblk_get_marker_info(m);

		if (!input_context->seq_set) {
			input_context->state = STOPPED;
			input_context->key_frame_requested = TRUE;
		} else if (!input_context->ignore_cseq && (new_seq != input_context->cur_seq + 1)) {
			ms_warning("MSVideoRouter: Sequence discontinuity detected on pin %i, key-frame requested", pin);
			input_context->state = STOPPED;
			input_context->key_frame_requested = TRUE;
		}
		if (input_context->key_frame_requested) {
			if (!input_context->seq_set || input_context->cur_ts != new_ts) {
				// Possibly a beginning of frame !
				if (s->is_key_frame(m)) {
					ms_message("MSVideoRouter: key frame detected on pin %i", pin);
					input_context->state = RUNNING;
					input_context->key_frame_start = m;
					input_context->key_frame_requested = FALSE;
				}
			}
		}
		input_context->cur_ts = new_ts;
		input_context->cur_seq = new_seq;
		input_context->seq_set = 1;
	}
}

static void
router_transfer(MSFilter *f, MSQueue *input, MSQueue *output, OutputContext *output_context, mblk_t *start) {
	mblk_t *m;
	if (ms_queue_empty(input)) return;

	if (start == NULL) start = ms_queue_peek_first(input);

	for (m = start; !ms_queue_end(input, m); m = ms_queue_peek_next(input, m)) {
		mblk_t *o = dupmsg(m);

		if (mblk_get_timestamp_info(m) != output_context->out_ts) {
			/* Each time we observe a new input timestamp, we must select a new output timestamp */
			output_context->out_ts = mblk_get_timestamp_info(m);
			output_context->adjusted_out_ts = (uint32_t)(f->ticker->time * 90LL);
		}

		/* We need to set sequence number for what we send out, otherwise the VP8 decoder won't be able
		 * to verify the integrity of the stream*/

		mblk_set_timestamp_info(o, output_context->adjusted_out_ts);
		mblk_set_cseq(o, output_context->out_seq++);
		mblk_set_marker_info(o, mblk_get_marker_info(m));

		ms_queue_put(output, o);
	}
}

static void _router_set_focus(MSFilter *f, RouterState *s, int pin) {
	int i;
	int requested_pin = pin;

	ms_message("%s: set focus %d", f->desc->name, pin);

	if (pin != -1 && f->inputs[pin] == NULL && s->placeholder_pin != -1) {
		ms_message("%s: Focus requested on an unconnected pin, will use placeholder on input pin [%i]", f->desc->name,
		           s->placeholder_pin);
		pin = s->placeholder_pin;
	}

	for (i = 0; i < f->desc->noutputs - 1; ++i) {
		// When link_source is defined, it means that the output has to be feed with active speaker stream.
		if (f->outputs[i] && s->output_contexts[i].link_source != -1) {
			// Never send back self contribution.
			if (requested_pin != s->output_contexts[i].link_source) {
				s->output_contexts[i].next_source = pin;
			} else if (s->output_contexts[i].next_source == s->placeholder_pin) {
				/* Do not leave the new focus on the place holder :
				 * - looking at the placeholder is not so pleasant, even more if it is no longer speaking
				 * - this solves the case where a member leaves, where a new focus is arbitrary selected.
				 */
				s->output_contexts[i].next_source = -1;
			}
			ms_message("%s: this pin %d link_source[%d], current_source %d and next_source %d", f->desc->name, i,
			           s->output_contexts[i].link_source, s->output_contexts[i].current_source,
			           s->output_contexts[i].next_source);
		}
	}
	s->focus_pin = requested_pin;
}

static void router_preprocess(MSFilter *f) {
	RouterState *s = (RouterState *)f->data;
	int i;

	if (s->focus_pin != -1) {
		/* Re-apply the focus selection in case of reconnection.
		 * Indeed, some new contributors may appear, in which case the placeholder is no longer needed */
		for (i = 0; i < f->desc->noutputs - 1; ++i) {
			MSQueue *q = f->outputs[i];
			if (q) {
				_router_set_focus(f, s, s->focus_pin);
				break;
			}
		}
	}
}

static void router_postprocess(BCTBX_UNUSED(MSFilter *f)) {
}

static void router_process(MSFilter *f) {
	RouterState *s = (RouterState *)f->data;
	int i;

	ms_filter_lock(f);
	/* First update channel states according to their received packets */
	for (i = 0; i < f->desc->ninputs; ++i) {
		MSQueue *q = f->inputs[i];
		InputContext *input_context = &s->input_contexts[i];
		if (q) {
			router_channel_update_input(s, i, q);
			if (!ms_queue_empty(q) && input_context->key_frame_requested) {
				if (input_context->state == STOPPED) {
					ms_filter_notify(f, MS_VIDEO_ROUTER_SEND_PLI, &i);
				} else {
					ms_filter_notify(f, MS_VIDEO_ROUTER_SEND_FIR, &i);
				}
			}
		}
	}

	/*fill outputs from inputs according to rules below*/
	for (i = 0; i < f->desc->noutputs; ++i) {
		MSQueue *q = f->outputs[i];
		OutputContext *output_context = &s->output_contexts[i];
		InputContext *input_context;

		if (q) {
			mblk_t *key_frame_start = NULL;

			if (output_context->link_source != -1) {
				/* Active speaker mode */
				if (output_context->next_source != -1 && f->inputs[output_context->next_source] == NULL) {
					ms_warning("%s: next source %i disapeared, choosing another one.", f->desc->name,
					           output_context->next_source);
					output_context->next_source = -1;
				}
				if (output_context->current_source != -1 && f->inputs[output_context->current_source] == NULL) {
					output_context->current_source = -1; /* Invalidate the current source until the next switch. */
					ms_warning("%s: current source %i disapeared.", f->desc->name, output_context->current_source);
				}
				if (output_context->next_source == -1) {
					if (elect_new_source(f, output_context) != -1) {
						ms_message("%s: new source automatically selected for output pin [%i]: next_source=[%i]",
						           f->desc->name, i, output_context->next_source);
					}
				}

				if (output_context->current_source != output_context->next_source &&
				    output_context->next_source != -1) {
					/* This output is waiting a key-frame to start */
					input_context = &s->input_contexts[output_context->next_source];
					if (input_context->key_frame_start != NULL) {
						MSVideoRouterSwitchedEventData event_data;
						event_data.output = i;
						event_data.input = output_context->next_source;
						/* The input just got a key frame, we can switch ! */
						output_context->current_source = output_context->next_source;
						key_frame_start = input_context->key_frame_start;
						ms_filter_notify(f, MS_VIDEO_ROUTER_OUTPUT_SWITCHED, &event_data);
					} else {
						/* else request a key frame */
						if (input_context->key_frame_requested == FALSE) {
							ms_message("%s: need key-frame for pin %i", f->desc->name, output_context->next_source);
							input_context->key_frame_requested = TRUE;
						}
					}
				}
			}
			if (output_context->current_source != -1 && f->inputs[output_context->current_source]) {
				input_context = &s->input_contexts[output_context->current_source];
				if (input_context->state == RUNNING) {
					router_transfer(f, f->inputs[output_context->current_source], q, output_context, key_frame_start);
				}
			}
		}
	}
	/*purge all inputs, we are done*/
	for (i = 0; i < f->desc->ninputs; ++i) {
		MSQueue *q = f->inputs[i];
		if (q) ms_queue_flush(q);
	}
	ms_filter_unlock(f);
}

static int router_set_focus(MSFilter *f, void *data) {
	RouterState *s = (RouterState *)f->data;
	int pin = *(int *)data;
	if (pin < 0 || pin >= f->desc->ninputs) {
		ms_warning("router_set_focus: invalid pin number %i", pin);
		return -1;
	}
	if (s->focus_pin != pin) {
		ms_message("%s: focus requested on pin %i", f->desc->name, pin);
		ms_filter_lock(f);
		_router_set_focus(f, s, pin);
		ms_filter_unlock(f);
	}
	return 0;
}

static int router_set_fmt(MSFilter *f, void *data) {
	const MSFmtDescriptor *fmt = (MSFmtDescriptor *)data;
	RouterState *s = (RouterState *)f->data;
	if (fmt) {
		if (strcasecmp(fmt->encoding, "VP8") == 0) {
			s->is_key_frame = is_vp8_key_frame;
		} else if (strcasecmp(fmt->encoding, "H264") == 0) {
			s->is_key_frame = is_h264_key_frame;
		} else {
			ms_error("%s: unsupported format %s", f->desc->name, fmt->encoding);
			return -1;
		}
	}
	return 0;
}

static int router_set_local_member_pin(MSFilter *f, void *data) {
	RouterState *s = (RouterState *)f->data;
	MSVideoConferenceFilterPinControl *pc = (MSVideoConferenceFilterPinControl *)data;
	if (pc->pin >= 0 && pc->pin < f->desc->ninputs) {
		s->input_contexts[pc->pin].ignore_cseq = (unsigned char)pc->enabled;
		ms_message("MSVideoRouter: Pin #%i local member attribute: %i", pc->pin, pc->enabled);
		return 0;
	}
	ms_error("%s: invalid argument to MS_VIDEO_ROUTER_SET_AS_LOCAL_MEMBER", f->desc->name);
	return -1;
}

static int router_set_placeholder(MSFilter *f, void *data) {
	RouterState *s = (RouterState *)f->data;
	s->placeholder_pin = *(int *)data;
	return 0;
}

static int router_notify_pli(MSFilter *f, void *data) {
	RouterState *s = (RouterState *)f->data;
	int pin = *(int *)data;
	int source_pin;
	if (pin < 0 || pin >= f->desc->noutputs) {
		ms_error("%s: invalid argument to MS_VIDEO_ROUTER_NOTIFY_PLI", f->desc->name);
		return -1;
	}
	/* Propagate the PLI to the current input source. */
	source_pin = s->output_contexts[pin].current_source;
	if (source_pin != -1) {
		ms_filter_notify(f, MS_VIDEO_ROUTER_SEND_PLI, &source_pin);
	}
	return 0;
}

static int router_notify_fir(MSFilter *f, void *data) {
	RouterState *s = (RouterState *)f->data;
	int pin = *(int *)data;
	int source_pin;
	if (pin < 0 || pin >= f->desc->noutputs) {
		ms_error("%s: invalid argument to MS_VIDEO_ROUTER_NOTIFY_PLI", f->desc->name);
		return -1;
	}
	/* Propagate the FIR to the current input source. */
	source_pin = s->output_contexts[pin].current_source;
	if (source_pin != -1) {
		ms_filter_notify(f, MS_VIDEO_ROUTER_SEND_FIR, &source_pin);
	}
	return 0;
}

static MSFilterMethod methods[] = {{MS_VIDEO_ROUTER_CONFIGURE_OUTPUT, router_configure_output},
                                   {MS_VIDEO_ROUTER_UNCONFIGURE_OUTPUT, router_unconfigure_output},
                                   {MS_VIDEO_ROUTER_SET_FOCUS, router_set_focus},
                                   {MS_VIDEO_ROUTER_SET_AS_LOCAL_MEMBER, router_set_local_member_pin},
                                   {MS_VIDEO_ROUTER_SET_PLACEHOLDER, router_set_placeholder},
                                   {MS_VIDEO_ROUTER_NOTIFY_PLI, router_notify_pli},
                                   {MS_VIDEO_ROUTER_NOTIFY_FIR, router_notify_fir},
                                   {MS_FILTER_SET_INPUT_FMT, router_set_fmt},
                                   {0, NULL}};

#ifdef _MSC_VER

MSFilterDesc ms_video_router_desc = {
    MS_VIDEO_ROUTER_ID,
    "MSVideoRouter",
    N_("A filter that router video streams from several inputs, used for video conferencing."),
    MS_FILTER_OTHER,
    NULL,
    ROUTER_MAX_INPUT_CHANNELS,
    ROUTER_MAX_OUTPUT_CHANNELS,
    router_init,
    router_preprocess,
    router_process,
    router_postprocess,
    router_uninit,
    methods,
};

#else

MSFilterDesc ms_video_router_desc = {
    .id = MS_VIDEO_ROUTER_ID,
    .name = "MSVideoRouter",
    .text = N_("A filter that router video streams from several inputs, used for video conferencing."),
    .category = MS_FILTER_OTHER,
    .ninputs = ROUTER_MAX_INPUT_CHANNELS,
    .noutputs = ROUTER_MAX_OUTPUT_CHANNELS,
    .init = router_init,
    .preprocess = router_preprocess,
    .process = router_process,
    .postprocess = router_postprocess,
    .uninit = router_uninit,
    .methods = methods,
};

#endif

MS_FILTER_DESC_EXPORT(ms_video_router_desc)

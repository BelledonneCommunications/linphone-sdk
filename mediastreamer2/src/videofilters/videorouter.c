/*
 * Copyright (c) 2010-2021 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include "mediastreamer2/msvideorouter.h"
#include "mediastreamer2/msticker.h"
#include "vp8rtpfmt.h"


static bool_t is_vp8_key_frame(mblk_t *m){
	uint8_t *p;
	
	if (m->b_cont){
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

static bool_t is_h264_key_frame(mblk_t *frame){
	/*TODO*/
	return FALSE;
}

static bool_t is_key_frame_dummy(mblk_t *frame){
	return TRUE;
}

typedef bool_t (*is_key_frame_func_t)(mblk_t *frame);


enum _IOState{
	STOPPED,
	RUNNING
};


typedef enum _IOState IOState;

typedef struct _InputContext{
	IOState state;
	uint8_t ignore_cseq;
	uint16_t cur_seq; /* to detect discontinuties*/
	uint32_t cur_ts; /* current timestamp, to detect beginning of frames */
	int seq_set;
	int key_frame_requested;
	mblk_t *key_frame_start;
}InputContext;

typedef struct _OutputContext{
	IOState state;
	uint32_t out_ts;
	uint32_t adjusted_out_ts;
	uint16_t out_seq;
	int next_source;
	int current_source;
	int switched;
}OutputContext;

typedef struct RouterState{
	InputContext input_contexts[ROUTER_MAX_INPUT_CHANNELS];
	OutputContext output_contexts[ROUTER_MAX_OUTPUT_CHANNELS];
	int focus_pin;
	is_key_frame_func_t is_key_frame;
} RouterState;

static void router_init(MSFilter *f){
	RouterState *s=ms_new0(RouterState,1);
	s->is_key_frame=is_key_frame_dummy;
	s->focus_pin = -1;
	f->data=s;
}

static void router_uninit(MSFilter *f){
	RouterState *s=(RouterState *)f->data;
	ms_free(s);
}

static int router_configure_output(MSFilter *f, void *data){
	RouterState *s=(RouterState *)f->data;
	MSVideoRouterPinData *pd = (MSVideoRouterPinData *)data;
	ms_filter_lock(f);
	s->output_contexts[pd->output].current_source = pd->input;
	s->output_contexts[pd->output].switched =pd->switched;
	ms_filter_unlock(f);
	ms_message("%s: router configure switched[%d] pin output %d with input %d", f->desc->name, pd->switched, pd->output, pd->input);
	return 0;
}

static int router_unconfigure_output(MSFilter *f, void *data){
	RouterState *s=(RouterState *)f->data;
	int pin = *(int*)data;
	ms_filter_lock(f);
	s->output_contexts[pin].current_source = -1;
	ms_filter_unlock(f);
	ms_message("%s: router unconfigure output pin %i ", f->desc->name, pin);
	return 0;
}

static void router_channel_update_input(RouterState *s, int pin, MSQueue *q){
	InputContext *input_context = &s->input_contexts[pin];
	mblk_t *m;
	input_context->key_frame_start = NULL;

	for(m = ms_queue_peek_first(q); !ms_queue_end(q, m); m = ms_queue_peek_next(q,m)){
		uint32_t new_ts = mblk_get_timestamp_info(m);
		uint16_t new_seq = mblk_get_cseq(m);
		//uint8_t marker = mblk_get_marker_info(m);

		if (!input_context->seq_set){
			input_context->state = STOPPED;
			input_context->key_frame_requested = TRUE;
		}else if (!input_context->ignore_cseq && (new_seq != input_context->cur_seq + 1)){
			ms_warning("MSVideoRouter: Sequence discontinuity detected on pin %i, key-frame requested", pin);
			input_context->state = STOPPED;
			input_context->key_frame_requested = TRUE;
		}
		if (input_context->key_frame_requested){
			if (!input_context->seq_set || input_context->cur_ts != new_ts){
				// Possibly a beginning of frame !
				if (s->is_key_frame(m)){
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

static int next_input_pin(MSFilter *f, int i){
	int k;
	for(k=i+1;k<i+1+f->desc->ninputs-2;k++){
		int next_pin=k % (f->desc->ninputs-2);
		if (f->inputs[next_pin]) return next_pin;
	}
	ms_error("next_input_pin: should not happen");
	return 0;
}

static void router_transfer(MSFilter *f, MSQueue *input,  MSQueue *output, OutputContext *output_context, mblk_t *start){
	mblk_t *m;
	if (ms_queue_empty(input)) return;

	if (start == NULL) start = ms_queue_peek_first(input);

	for(m = start; !ms_queue_end(input, m) ; m = ms_queue_peek_next(input,m)){
		mblk_t *o = dupmsg(m);

		if (mblk_get_timestamp_info(m) != output_context->out_ts){
			/* Each time we observe a new input timestamp, we must select a new output timestamp */
			output_context->out_ts = mblk_get_timestamp_info(m);
			output_context->adjusted_out_ts = (uint32_t) (f->ticker->time * 90LL);
		}

		/* We need to set sequence number for what we send out, otherwise the VP8 decoder won't be able
		 * to verify the integrity of the stream*/

		mblk_set_timestamp_info(o, output_context->adjusted_out_ts);
		mblk_set_cseq(o, output_context->out_seq++);
		mblk_set_marker_info(o, mblk_get_marker_info(m));
		
		ms_queue_put(output, o);
	}
}

static void _router_set_focus(MSFilter *f, RouterState *s , int pin){
	int i;
	ms_message("%s: set focus %d", f->desc->name, pin);
	for (i = 0 ; i < f->desc->noutputs-1; ++i){ // pin is reserved
		if (i != pin){
			s->output_contexts[i].next_source = pin;
		} else {
			ms_message("%s: this pin %d keeps current source %d and next source %d", f->desc->name, i,s->output_contexts[i].current_source, s->output_contexts[i].next_source);
		}
	}
	s->focus_pin=pin;
}

static void router_preprocess(MSFilter *f){
	RouterState *s=(RouterState *)f->data;
	int i;
	
	if (s->focus_pin == -1){
		for(i=0;i<f->desc->noutputs-1;++i){ // pin is reserved
			MSQueue *q = f->outputs[i];
			if (q){
				_router_set_focus(f, s, i);
				break;
			}
		}
	}
}

static void router_postprocess(MSFilter *f){
}

static void router_process(MSFilter *f){
	RouterState *s=(RouterState *)f->data;
	int i;

	ms_filter_lock(f);
	/* First update channel states according to their received packets */
	for(i=0;i<f->desc->ninputs;++i){
		MSQueue *q=f->inputs[i];
		InputContext *input_context=&s->input_contexts[i];
		if (q) {
			router_channel_update_input(s, i, q);
			if (!ms_queue_empty(q) && input_context->key_frame_requested){
				if (input_context->state == STOPPED){
					ms_filter_notify(f, MS_VIDEO_ROUTER_SEND_PLI, &i);
				}else{
					ms_filter_notify(f,MS_VIDEO_ROUTER_SEND_FIR, &i);
				}
			}
		}
	}

	/*fill outputs from inputs according to rules below*/
	for(i=0;i<f->desc->noutputs;++i){
		MSQueue * q = f->outputs[i];
		OutputContext *output_context = &s->output_contexts[i];
		InputContext *input_context;

		if (q){
			mblk_t *key_frame_start = NULL;

			if (output_context->switched) {
				if (f->inputs[output_context->next_source] == NULL){
					ms_warning("%s: next source %i disapeared, choosing another one.", f->desc->name, output_context->next_source);
					output_context->next_source = next_input_pin(f, output_context->next_source);
				}
				if (output_context->current_source != -1 && f->inputs[output_context->current_source] == NULL){
					ms_warning("%s: current source %i disapeared, choosing another one to switch to.", f->desc->name, output_context->current_source);
					output_context->next_source = next_input_pin(f, output_context->current_source);
					output_context->current_source = -1; /* Invalidate the current source until the switch.*/
				}

				if (output_context->current_source != output_context->next_source){
					/* This output is waiting a key-frame to start */
					input_context = &s->input_contexts[output_context->next_source];
					if (input_context->key_frame_start != NULL){
						/* The input just got a key frame, we can switch ! */
						output_context->current_source = output_context->next_source;
						key_frame_start = input_context->key_frame_start;
					}else{
						/* else request a key frame */
						if (input_context->key_frame_requested == FALSE){
							ms_message("%s: need key-frame for pin %i", f->desc->name, output_context->next_source);
							input_context->key_frame_requested = TRUE;
						}
					}
				}

				if (output_context->current_source != -1){
					input_context = &s->input_contexts[output_context->current_source];
					if (input_context->state == RUNNING){
						router_transfer(f, f->inputs[output_context->current_source], q, output_context, key_frame_start);
					}
				}
			} else if (output_context->current_source != -1 && f->inputs[output_context->current_source]){
				input_context = &s->input_contexts[output_context->current_source];
				if (input_context->state == RUNNING){
					router_transfer(f, f->inputs[output_context->current_source], q, output_context, input_context->key_frame_start);
				}
			}
		}
	}
	/*purge all inputs, we are done*/
	for(i=0;i<f->desc->ninputs;++i){
		MSQueue *q=f->inputs[i];
		if (q) ms_queue_flush(q);
	}
	ms_filter_unlock(f);
}

static int router_set_focus(MSFilter *f, void *data){
	RouterState *s=(RouterState *)f->data;
	int pin=*(int*)data;
	if (pin<0 || pin>=f->desc->ninputs){
		ms_warning("router_set_focus: invalid pin number %i",pin);
		return -1;
	}
	if (s->focus_pin!=pin){
		ms_filter_lock(f);
		_router_set_focus(f, s, pin);
		ms_filter_unlock(f);
		ms_message("%s: focus requested on pin %i", f->desc->name, pin);
	}
	return 0;
}

static int router_set_fmt(MSFilter *f, void *data){
	const MSFmtDescriptor *fmt=(MSFmtDescriptor*)data;
	RouterState *s=(RouterState *)f->data;
	if (fmt){
		if (strcasecmp(fmt->encoding,"VP8")==0){
			s->is_key_frame=is_vp8_key_frame;
		}else if (strcasecmp(fmt->encoding,"H264")==0){
			s->is_key_frame=is_h264_key_frame;
		}else{
			ms_error("%s: unsupported format %s", f->desc->name, fmt->encoding);
			return -1;
		}
	}
	return 0;
}

static int router_set_local_member_pin(MSFilter *f, void *data) {
	RouterState *s=(RouterState *)f->data;
	MSVideoConferenceFilterPinControl *pc = (MSVideoConferenceFilterPinControl*)data;
	if (pc->pin >= 0 && pc->pin < f->desc->ninputs){
		s->input_contexts[pc->pin].ignore_cseq = (unsigned char)pc->enabled;
		ms_message("MSVideoRouter: Pin #%i local member attribute: %i", pc->pin, pc->enabled);
		return 0;
	}
	ms_error("%s: invalid argument to MS_VIDEO_ROUTER_SET_AS_LOCAL_MEMBER", f->desc->name);
	return -1;
}

static int router_notify_pli(MSFilter *f, void *data){
	RouterState *s=(RouterState *)f->data;
	int pin = *(int*)data;
	int source_pin;
	if (pin < 0 || pin >= f->desc->noutputs){
		ms_error("%s: invalid argument to MS_VIDEO_ROUTER_NOTIFY_PLI", f->desc->name);
		return -1;
	}
	/* Propagate the PLI to the current input source. */
	source_pin = s->output_contexts[pin].current_source;
	if (source_pin != -1){
		ms_filter_notify(f, MS_VIDEO_ROUTER_SEND_PLI, &source_pin);
	}
	return 0;
}

static int router_notify_fir(MSFilter *f, void *data){
	RouterState *s=(RouterState *)f->data;
	int pin = *(int*)data;
	int source_pin;
	if (pin < 0 || pin >= f->desc->noutputs){
		ms_error("%s: invalid argument to MS_VIDEO_ROUTER_NOTIFY_PLI", f->desc->name);
		return -1;
	}
	/* Propagate the FIR to the current input source. */
	source_pin = s->output_contexts[pin].current_source;
	if (source_pin != -1){
		ms_filter_notify(f, MS_VIDEO_ROUTER_SEND_FIR, &source_pin);
	}
	return 0;
}

static MSFilterMethod methods[]={
	{	MS_VIDEO_ROUTER_CONFIGURE_OUTPUT , router_configure_output },
	{	MS_VIDEO_ROUTER_UNCONFIGURE_OUTPUT , router_unconfigure_output },
	{	MS_VIDEO_ROUTER_SET_FOCUS , router_set_focus},
	{	MS_VIDEO_ROUTER_SET_AS_LOCAL_MEMBER , router_set_local_member_pin },
	{	MS_VIDEO_ROUTER_NOTIFY_PLI, router_notify_pli },
	{	MS_VIDEO_ROUTER_NOTIFY_FIR, router_notify_fir },
	{	MS_FILTER_SET_INPUT_FMT, router_set_fmt },
	{0,NULL}
};


#ifdef _MSC_VER


MSFilterDesc ms_video_router_desc={
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


MSFilterDesc ms_video_router_desc={
	.id=MS_VIDEO_ROUTER_ID,
	.name="MSVideoRouter",
	.text=N_("A filter that router video streams from several inputs, used for video conferencing."),
	.category=MS_FILTER_OTHER,
	.ninputs=ROUTER_MAX_INPUT_CHANNELS,
	.noutputs=ROUTER_MAX_OUTPUT_CHANNELS,
	.init=router_init,
	.preprocess=router_preprocess,
	.process=router_process,
	.postprocess=router_postprocess,
	.uninit=router_uninit,
	.methods=methods,
};


#endif

MS_FILTER_DESC_EXPORT(ms_video_router_desc)

/*
 * Copyright (c) 2020 Belledonne Communications SARL.
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



#include "mediastreamer2/msvideoswitcher.h"
#include "mediastreamer2/msticker.h"
#include "vp8rtpfmt.h"

#define SWITCHER_MAX_CHANNELS 20
#define SWITCHER_MAX_INPUT_CHANNELS SWITCHER_MAX_CHANNELS+1 //one more pin for static image

static bool_t is_vp8_key_frame(mblk_t *m){
	uint8_t *p;
	
	if (m->b_cont){
		/* When data comes directly from the VP8 encoder, the VP8 payload is the second of the mblk_t chain.*/
		return !(m->b_cont->b_rptr[0] & 1);
	}
	p = vp8rtpfmt_skip_payload_descriptor(m);
	
	if (!p) {
		ms_warning("MSVideoSwitcher: invalid vp8 payload descriptor.");
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
	int current_source;
	int next_source;
}OutputContext;

typedef struct SwitcherState{
	InputContext input_contexts[SWITCHER_MAX_INPUT_CHANNELS];
	OutputContext output_contexts[SWITCHER_MAX_CHANNELS];
	int focus_pin;
	is_key_frame_func_t is_key_frame;
} SwitcherState;



static void switcher_init(MSFilter *f){
	SwitcherState *s=ms_new0(SwitcherState,1);
	s->focus_pin = -1;
	s->is_key_frame=is_key_frame_dummy;
	f->data=s;
}

static void switcher_uninit(MSFilter *f){
	SwitcherState *s=(SwitcherState *)f->data;
	ms_free(s);
}

static int next_input_pin(MSFilter *f, int i){
	int k;
	for(k=i+1;k<i+1+f->desc->ninputs;k++){
		int next_pin=k % f->desc->ninputs;
		if (f->inputs[next_pin]) return next_pin;
	}
	ms_error("next_input_pin: should not happen");
	return 0;
}

static void _switcher_set_focus(MSFilter *f, SwitcherState *s , int pin){
	int i;
	for (i = 0 ; i < f->desc->noutputs ; ++i){
		if (i != pin){
			s->output_contexts[i].next_source = pin;
		}
	}
	s->focus_pin=pin;
}

static void switcher_preprocess(MSFilter *f){
	SwitcherState *s=(SwitcherState *)f->data;
	int i;
	
	if (s->focus_pin == -1){
		for(i=0;i<f->desc->noutputs;++i){
			MSQueue *q = f->outputs[i];
			//Channel *chan=&s->channels[i];
			if (q){
				_switcher_set_focus(f, s, i);
				break;
			}
		}
	}
}

static void switcher_postprocess(MSFilter *f){
}

static void switcher_channel_update_input(SwitcherState *s, int pin, MSQueue *q){
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
			ms_warning("MSVideoSwitcher: Sequence discontinuity detected on pin %i, key-frame requested", pin);
			input_context->state = STOPPED;
			input_context->key_frame_requested = TRUE;
		}
		if (input_context->key_frame_requested){
			if (!input_context->seq_set || input_context->cur_ts != new_ts){
				/* Possibly a beginning of frame ! */
				if (s->is_key_frame(m)){
					ms_message("MSVideoSwitcher: key frame detected on pin %i", pin);
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

static void switcher_transfer(MSFilter *f, MSQueue *input,  MSQueue *output, OutputContext *output_context, mblk_t *start){
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

static void switcher_process(MSFilter *f){
	SwitcherState *s=(SwitcherState *)f->data;
	int i;
	
	ms_filter_lock(f);
	
	/* First update channel states according to their received packets */
	for(i=0;i<f->desc->ninputs;++i){
		MSQueue *q=f->inputs[i];
		InputContext *input_context=&s->input_contexts[i];
		if (q) {
			switcher_channel_update_input(s, i, q);
			if (!ms_queue_empty(q) && input_context->key_frame_requested){
				if (input_context->state == STOPPED){
					ms_filter_notify(f, MS_VIDEO_SWITCHER_SEND_PLI, &i);
				}else{
					ms_filter_notify(f,MS_VIDEO_SWITCHER_SEND_FIR, &i);
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
					switcher_transfer(f, f->inputs[output_context->current_source], q, output_context, key_frame_start);
					//ms_message("%s: transfered packets from pin %i to pin %i", f->desc->name, output_context->current_source, i);
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

static int switcher_set_focus(MSFilter *f, void *data){
	SwitcherState *s=(SwitcherState *)f->data;
	int pin=*(int*)data;
	if (pin<0 || pin>=f->desc->ninputs){
		ms_warning("switcher_set_focus: invalid pin number %i",pin);
		return -1;
	}
	if (s->focus_pin!=pin){
		ms_filter_lock(f);
		_switcher_set_focus(f, s, pin);
		ms_filter_unlock(f);
		ms_message("%s: focus requested on pin %i", f->desc->name, pin);
	}
	return 0;
}

static int switcher_set_fmt(MSFilter *f, void *data){
	const MSFmtDescriptor *fmt=(MSFmtDescriptor*)data;
	SwitcherState *s=(SwitcherState *)f->data;
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

static int switcher_set_local_member_pin(MSFilter *f, void *data){
	SwitcherState *s=(SwitcherState *)f->data;
	MSVideoSwitcherPinControl *pc = (MSVideoSwitcherPinControl*)data;
	if (pc->pin >= 0 && pc->pin < f->desc->ninputs){
		s->input_contexts[pc->pin].ignore_cseq = (unsigned char)pc->enabled;
		ms_message("MSVideoSwitcher: Pin #%i local member attribute: %i", pc->pin, pc->enabled);
		return 0;
	}
	ms_error("%s: invalid argument to MS_VIDEO_SWITCHER_SET_AS_LOCAL_MEMBER", f->desc->name);
	return -1;
}

static int switcher_notify_pli(MSFilter *f, void *data){
	SwitcherState *s=(SwitcherState *)f->data;
	int pin = *(int*)data;
	int source_pin;
	if (pin < 0 || pin >= f->desc->noutputs){
		ms_error("%s: invalid argument to MS_VIDEO_SWITCHER_NOTIFY_PLI", f->desc->name);
		return -1;
	}
	/* Propagate the PLI to the current input source. */
	source_pin = s->output_contexts[pin].current_source;
	if (source_pin != -1){
		ms_filter_notify(f, MS_VIDEO_SWITCHER_SEND_PLI, &source_pin);
	}
	return 0;
}

static int switcher_notify_fir(MSFilter *f, void *data){
	SwitcherState *s=(SwitcherState *)f->data;
	int pin = *(int*)data;
	int source_pin;
	if (pin < 0 || pin >= f->desc->noutputs){
		ms_error("%s: invalid argument to MS_VIDEO_SWITCHER_NOTIFY_PLI", f->desc->name);
		return -1;
	}
	/* Propagate the FIR to the current input source. */
	source_pin = s->output_contexts[pin].current_source;
	if (source_pin != -1){
		ms_filter_notify(f, MS_VIDEO_SWITCHER_SEND_FIR, &source_pin);
	}
	return 0;
}

static MSFilterMethod methods[]={
	{	MS_VIDEO_SWITCHER_SET_FOCUS , switcher_set_focus },
	{	MS_VIDEO_SWITCHER_SET_AS_LOCAL_MEMBER , switcher_set_local_member_pin },
	{	MS_VIDEO_SWITCHER_NOTIFY_PLI, switcher_notify_pli },
	{	MS_VIDEO_SWITCHER_NOTIFY_FIR, switcher_notify_fir },
	{	MS_FILTER_SET_INPUT_FMT, switcher_set_fmt },
	{0,NULL}
};

#ifdef _MSC_VER

MSFilterDesc ms_video_switcher_desc={
	MS_VIDEO_SWITCHER_ID,
	"MSVideoSwitcher",
	N_("A filter that switches video streams from several inputs, used for video conferencing."),
	MS_FILTER_OTHER,
	NULL,
	SWITCHER_MAX_INPUT_CHANNELS,
	SWITCHER_MAX_CHANNELS,
	switcher_init,
	switcher_preprocess,
	switcher_process,
	switcher_postprocess,
	switcher_uninit,
	methods,
};

#else

MSFilterDesc ms_video_switcher_desc={
	.id=MS_VIDEO_SWITCHER_ID,
	.name="MSVideoSwitcher",
	.text=N_("A filter that switches video streams from several inputs, used for video conferencing."),
	.category=MS_FILTER_OTHER,
	.ninputs=SWITCHER_MAX_INPUT_CHANNELS,
	.noutputs=SWITCHER_MAX_CHANNELS,
	.init=switcher_init,
	.preprocess=switcher_preprocess,
	.process=switcher_process,
	.postprocess=switcher_postprocess,
	.uninit=switcher_uninit,
	.methods=methods,
};

#endif

MS_FILTER_DESC_EXPORT(ms_video_switcher_desc)

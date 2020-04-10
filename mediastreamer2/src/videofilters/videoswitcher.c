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

static bool_t is_vp8_key_frame(mblk_t *m){
	uint8_t *p;
	bool_t duplicated = FALSE;
	if (m->b_cont){
		m = dupmsg(m);
		msgpullup(m, 50);
		duplicated = TRUE;
	}
	p = vp8rtpfmt_skip_payload_descriptor(m);
	if (duplicated) freeb(m);
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


enum _ChannelState{
	WAITING_KEYFRAME,
	STARTING_WITH_KEYFRAME,
	RUNNING
};
typedef enum _ChannelState ChannelState;

typedef struct Channel{
	int current_source;
	uint8_t cur_marker_set;
	uint8_t ignore_cseq;
	uint16_t cur_seq; /* to detect discontinuties*/
	uint32_t cur_ts; /* current timestamp, to detect beginning of frames */
	int seq_set;
	int has_focus_pending;
	mblk_t *key_frame_start;
	ChannelState state;
} Channel;


typedef struct SwitcherState{
	Channel channels[SWITCHER_MAX_CHANNELS];
	int focus_pin;
	is_key_frame_func_t is_key_frame;
} SwitcherState;

int switcher_channel_get_pin(SwitcherState *s, Channel *chan){
	return chan - &s->channels[0];
}

static void switcher_channel_update(SwitcherState *s, Channel *chan, MSQueue *q){
	mblk_t *m;
	int pin = switcher_channel_get_pin(s, chan);
	
	if (chan->state == STARTING_WITH_KEYFRAME){
		chan->state = RUNNING;
	}
	chan->key_frame_start = NULL;
	
	for(m = ms_queue_peek_first(q); !ms_queue_end(q, m); m = ms_queue_peek_next(q,m)){
		uint32_t new_ts = mblk_get_timestamp_info(m);
		uint16_t new_seq = mblk_get_cseq(m);
		//uint8_t marker = mblk_get_marker_info(m);
		
		if (!chan->seq_set){
			chan->state = WAITING_KEYFRAME;
		}else if (!chan->ignore_cseq && (new_seq != chan->cur_seq + 1)){
			ms_warning("MSVideoSwitcher: Sequence discontinuity detected on pin %i", pin);
			chan->state = WAITING_KEYFRAME;
		}
		if (chan->state == WAITING_KEYFRAME || chan->has_focus_pending){
			if (!chan->seq_set || chan->cur_ts != new_ts){
				/* Possibly a beginning of frame ! */
				if (s->is_key_frame(m)){
					ms_message("MSVideoSwitcher: key frame detected on pin %i", pin);
					chan->state = STARTING_WITH_KEYFRAME;
					chan->key_frame_start = m;
					if (chan->has_focus_pending) chan->has_focus_pending = FALSE;
				}
			}
		}
		chan->cur_ts = new_ts;
		chan->cur_seq = new_seq;
		chan->seq_set = 1;
	}
}


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

static void switcher_preprocess(MSFilter *f){
	SwitcherState *s=(SwitcherState *)f->data;
	int i;
	/*define routing tables according to the connected inputs*/
	for(i=0;i<f->desc->noutputs;++i){
		MSQueue *q=f->outputs[i];
		Channel *chan=&s->channels[i];
		if (q){
			if (s->focus_pin == -1) s->focus_pin = i;
			chan->current_source=next_input_pin(f,i);
		}
	}
}

static void switcher_postprocess(MSFilter *f){
}

static void switcher_transfer(MSQueue *input, MSQueue *output, mblk_t *start){
	mblk_t *m;
	if (ms_queue_empty(input)) return;
	for(m = ms_queue_peek_first(input); !ms_queue_end(input, m) ;m = ms_queue_peek_next(input,m)){
		if (start != NULL && m != start) continue; /* Skip packets until the key-frame start packet is found */
		
		ms_queue_put(output,dupmsg(m));
	}
}

static void switcher_process(MSFilter *f){
	SwitcherState *s=(SwitcherState *)f->data;
	int i;
	MSQueue *focus_q=f->inputs[s->focus_pin];
	Channel *focus_chan = &s->channels[s->focus_pin];
	
	ms_filter_lock(f);
	
	/* First update channel states according to their received packets */
	for(i=0;i<f->desc->ninputs;++i){
		MSQueue *q=f->inputs[i];
		Channel *chan=&s->channels[i];
		if (q && !ms_queue_empty(q)) {
			switcher_channel_update(s, chan, q);
			if (chan->state == WAITING_KEYFRAME || chan->has_focus_pending){
				ms_message("%s: need key-frame for pin %i", f->desc->name, i);
				ms_filter_notify(f,MS_VIDEO_SWITCHER_NEEDS_KEYFRAME, &i);
			}
		}
	}
	
	if (focus_chan->state == STARTING_WITH_KEYFRAME){
		ms_message("%s: got key-frame, focus definitely switched to pin %i", f->desc->name, s->focus_pin);
	}
	
	/*fill outputs from inputs according to rules below*/
	for(i=0;i<f->desc->noutputs;++i){
		MSQueue * q = f->outputs[i];
		Channel *chan = &s->channels[i];
		if (q){
			if (i != s->focus_pin && !focus_chan->has_focus_pending && focus_chan->state != WAITING_KEYFRAME){
				switcher_transfer(focus_q, q, focus_chan->key_frame_start);
				chan->current_source = s->focus_pin;
				ms_message("%s: transfered packets from focus pin %i to pin %i", f->desc->name, s->focus_pin, i);
			}else{
				/*selfviewing is not allowed, sending something that doesn't start with a keyframe also*/
				MSQueue *source_queue = f->inputs[chan->current_source]; /* Get source for output channel i*/
				Channel *source_chan =  &s->channels[chan->current_source]; /* Source channel */
				if (source_queue && !ms_queue_empty(source_queue) && source_chan->state != WAITING_KEYFRAME) {
					ms_message("%s: transfered packets from pin %i to pin %i", f->desc->name, chan->current_source, i);
					switcher_transfer(source_queue, q, chan->key_frame_start);
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
		s->focus_pin=pin;
		s->channels[s->focus_pin].has_focus_pending = TRUE;
		ms_filter_unlock(f);
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
		s->channels[pc->pin].ignore_cseq = (unsigned char)pc->enabled;
		ms_message("MSVideoSwitcher: Pin #%i local member attribute: %i", pc->pin, pc->enabled);
		return 0;
	}
	ms_error("%s: invalid argument to MS_VIDEO_SWITCHER_SET_AS_LOCAL_MEMBER", f->desc->name);
	return -1;
}

static MSFilterMethod methods[]={
	{	MS_VIDEO_SWITCHER_SET_FOCUS , switcher_set_focus },
	{	MS_VIDEO_SWITCHER_SET_AS_LOCAL_MEMBER , switcher_set_local_member_pin },
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
	SWITCHER_MAX_CHANNELS,
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
	.ninputs=SWITCHER_MAX_CHANNELS,
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

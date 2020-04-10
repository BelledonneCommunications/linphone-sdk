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

#define SWITCHER_MAX_CHANNELS 20

static bool_t is_vp8_key_frame(mblk_t *m){
	return !(m->b_rptr[0] & 1);
}

static bool_t is_h264_key_frame(mblk_t *m){
	/*TODO*/
	return FALSE;
}

static bool_t is_key_frame_dummy(mblk_t *m){
	return TRUE;
}

typedef bool_t (*is_key_frame_func_t)(mblk_t *m);

typedef struct Channel{
	int current_source;
} Channel;

typedef struct SwitcherState{
	Channel channels[SWITCHER_MAX_CHANNELS];
	int focus_pin;
	is_key_frame_func_t is_key_frame;
	mblk_t *found_keyframe;
	bool_t focus_got_keyframe;
	bool_t keyframe_requested;
} SwitcherState;


static void switcher_init(MSFilter *f){
	SwitcherState *s=ms_new0(SwitcherState,1);
	s->focus_pin=0;
	s->focus_got_keyframe=FALSE;
	s->is_key_frame=is_key_frame_dummy;
	f->data=s;
}

static void switcher_uninit(MSFilter *f){
	SwitcherState *s=(SwitcherState *)f->data;
	ms_free(s);
}

static int next_input_pin(MSFilter *f, int i){
	int k;
	for(k=i+1;k<i+f->desc->ninputs;k++){
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
			chan->current_source=next_input_pin(f,i);
		}
	}
}

static void switcher_postprocess(MSFilter *f){
}

static void switcher_transfer(MSQueue *input, MSQueue *output, mblk_t *start){
	mblk_t *m;
	if (ms_queue_empty(input)) return;
	for(m=ms_queue_peek_first(input);m!=NULL;m=ms_queue_peek_next(input,m)){
		if (m==start){
			start=NULL;
		}
		if (start==NULL){
			ms_queue_put(output,dupmsg(m));
		}
	}
}

static void switcher_process(MSFilter *f){
	SwitcherState *s=(SwitcherState *)f->data;
	int i;
	MSQueue *focus_q=f->inputs[s->focus_pin];
	
	ms_filter_lock(f);
	if (focus_q && !s->focus_got_keyframe){
		if (!s->keyframe_requested){
			ms_filter_notify(f,MS_VIDEO_SWITCHER_NEEDS_KEYFRAME,&s->focus_pin);
			s->keyframe_requested=TRUE;
		}
		if (!ms_queue_empty(focus_q)){
			mblk_t *m;
			s->found_keyframe=NULL;
			for(m=ms_queue_peek_first(focus_q);m!=NULL;m=ms_queue_peek_next(focus_q,m)){
				if (s->is_key_frame(m)){
					s->focus_got_keyframe=TRUE;
					s->found_keyframe=m;
					break;
				}
			}
		}
	}
	/*fill outputs from inputs according to rules below*/
	for(i=0;i<f->desc->noutputs;++i){
		MSQueue *q=f->outputs[i];
		Channel *chan=&s->channels[i];
		if (q){
			if (i!=s->focus_pin && s->focus_got_keyframe){
				switcher_transfer(focus_q,q,s->found_keyframe);
				if (s->found_keyframe){
					chan->current_source=s->focus_pin;
				}
			}else{/*selfviewing is not allowed, sending something that doesn't start with a keyframe also*/
				MSQueue *prev=f->inputs[chan->current_source];
				if (prev) switcher_transfer(prev,q,NULL);
			}
		}
	}
	/*purge all inputs, we are done*/
	for(i=0;i<f->desc->ninputs;++i){
		MSQueue *q=f->inputs[i];
		if (q) ms_queue_flush(q);
	}
	if (s->found_keyframe)
		s->found_keyframe=NULL;
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
		s->focus_got_keyframe=FALSE;
		s->keyframe_requested=FALSE;
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

static MSFilterMethod methods[]={
	{	MS_VIDEO_SWITCHER_SET_FOCUS , switcher_set_focus },
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

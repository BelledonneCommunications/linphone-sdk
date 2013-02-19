/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2011  Belledonne Communications SARL.
Author: Simon Morlat (simon.morlat@linphone.org)

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

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/mstonedetector.h"
#include "mediastreamer2/msticker.h"

#include <math.h>

#ifndef M_PI
#define M_PI       3.14159265358979323846
#endif

#define MAX_SCANS 10

static const float energy_min_threshold=0.01;

typedef struct _GoertzelState{
	uint64_t starttime;
	int dur;
	float coef;
	bool_t event_sent;
	bool_t pad[3];
}GoertzelState;

static void goertzel_state_init(GoertzelState *gs, int frequency, int sampling_frequency){
	gs->coef=(float)2*(float)cos(2*M_PI*((float)frequency/(float)sampling_frequency));
	gs->starttime=0;
	gs->dur=0;
}

static float goertzel_state_run(GoertzelState *gs,int16_t  *samples, int nsamples, float total_energy){
	int i;
	float tmp;
	float q1=0;
	float q2=0;
	float freq_en;

	for(i=0;i<nsamples;++i){
		tmp=q1;
		q1=(gs->coef*q1) - q2 + (float)samples[i];
		q2=tmp;
	}
	
	freq_en= (q1*q1) + (q2*q2) - (q1*q2*gs->coef);
	/*return a relative frequency energy compared over the total signal energy */
	return freq_en/(total_energy*(float)nsamples*0.5);
}

static float compute_energy(int16_t *samples, int nsamples){
	float en=0;
	int i;
	for(i=0;i<nsamples;++i){
		float s=(float)samples[i];
		en+=s*s;
	}
	return en;
}

typedef struct _DetectorState{
	MSToneDetectorDef tone_def[MAX_SCANS];
	GoertzelState tone_gs[MAX_SCANS];
	int nscans;
	MSBufferizer *buf;
	int rate;
	int framesize;
	int frame_ms;
}DetectorState;

static void detector_init(MSFilter *f){
	DetectorState *s=ms_new0(DetectorState,1);
	s->buf=ms_bufferizer_new();
	s->rate=8000;
	s->frame_ms=20;
	s->framesize=2*(s->frame_ms*s->rate)/1000;
	f->data=s;
}

static void detector_uninit(MSFilter *f){
	DetectorState *s=(DetectorState *)f->data;
	ms_bufferizer_destroy (s->buf);
	ms_free(f->data);
}

static int find_free_slot(DetectorState *s){
	int i;
	for(i=0;i<MAX_SCANS;++i){
		if (s->tone_def[i].frequency==0) return i;
	}
	ms_error("No more free tone detector scans allowed, maximum reached.");
	return -1;
}

static int detector_add_scan(MSFilter *f, void *arg){
	DetectorState *s=(DetectorState *)f->data;
	MSToneDetectorDef *def=(MSToneDetectorDef*)arg;
	int i=find_free_slot(s);
	if (i!=-1){
		s->tone_def[i]=*def;
		s->nscans++;
		goertzel_state_init(&s->tone_gs[i],def->frequency,s->rate);
		return 0;
	}
	return -1;
}

static int detector_clear_scans(MSFilter *f, void *arg){
	DetectorState *s=(DetectorState *)f->data;
	memset(&s->tone_def,0,sizeof(s->tone_def));
	s->nscans=0;
	return 0;
}

static int detector_set_rate(MSFilter *f, void *arg){
	DetectorState *s=(DetectorState *)f->data;
	s->rate = *((int*) arg);
	return 0;
}

static void end_all_tones(DetectorState *s){
	int i;
	for(i=0;i<s->nscans;++i){
		GoertzelState *gs=&s->tone_gs[i];
		gs->dur=0;
		gs->event_sent=FALSE;
	}
}

static void detector_process(MSFilter *f){
	DetectorState *s=(DetectorState *)f->data;
	mblk_t *m;
	
	while ((m=ms_queue_get(f->inputs[0]))!=NULL){
		ms_queue_put(f->outputs[0],m);
		if (s->nscans>0){
			ms_bufferizer_put(s->buf,dupmsg(m));
		}
	}
	if (s->nscans>0){
		uint8_t *buf=alloca(s->framesize);

		while(ms_bufferizer_read(s->buf,buf,s->framesize)!=0){
			float en=compute_energy((int16_t*)buf,s->framesize/2);
			if (en>energy_min_threshold*(32767.0*32767.0*0.7)){
				int i;
				for(i=0;i<s->nscans;++i){
					GoertzelState *gs=&s->tone_gs[i];
					MSToneDetectorDef *tone_def=&s->tone_def[i];
					float freq_en=goertzel_state_run(gs,(int16_t*)buf,s->framesize/2,en);
					if (freq_en>=tone_def->min_amplitude){
						if (gs->dur==0) gs->starttime=f->ticker->time;
						gs->dur+=s->frame_ms;
						if (gs->dur>=tone_def->min_duration && !gs->event_sent){
							MSToneDetectorEvent event;
						
							strncpy(event.tone_name,tone_def->tone_name,sizeof(event.tone_name));
							event.tone_start_time=gs->starttime;
							ms_filter_notify(f,MS_TONE_DETECTOR_EVENT,&event);
							gs->event_sent=TRUE;
						}
					}else{
						gs->event_sent=FALSE;
						gs->dur=0;
						gs->starttime=0;
					}
				}
			}else end_all_tones(s);
		}
	}
}

static MSFilterMethod detector_methods[]={
	{	MS_TONE_DETECTOR_ADD_SCAN, 		detector_add_scan	},
	{	MS_TONE_DETECTOR_CLEAR_SCANS,	detector_clear_scans	},
	{	MS_FILTER_SET_SAMPLE_RATE,	detector_set_rate	},
	{	0	,	NULL}
};

#ifndef _MSC_VER

MSFilterDesc ms_tone_detector_desc={
	.id=MS_TONE_DETECTOR_ID,
	.name="MSToneDetector",
	.text="Custom tone detection filter.",
	.category=MS_FILTER_OTHER,
	.ninputs=1,
	.noutputs=1,
	.init=detector_init,
	.process=detector_process,
	.uninit=detector_uninit,
	.methods=detector_methods
};

#else

MSFilterDesc ms_tone_detector_desc={
	MS_TONE_DETECTOR_ID,
	"MSToneDetector",
	"Custom tone detection filter.",
	MS_FILTER_OTHER,
	NULL,
	1,
	1,
	detector_init,
	NULL,
	detector_process,
	NULL,
	detector_uninit,
	detector_methods,
};

#endif


MS_FILTER_DESC_EXPORT(ms_tone_detector_desc)




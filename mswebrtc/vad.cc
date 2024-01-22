/*
 vad.cc
 Copyright (C) 2018 Belledonne Communications, Grenoble, France

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
#include "mediastreamer2/msticker.h"
#include "webrtc_vad.h"

#define NUMBER_SAMPLE_RATES 4

static const int _sample_rates[NUMBER_SAMPLE_RATES] = {8000, 16000, 32000, 48000};

typedef struct MSWebRTCVAD {
	VadInst *instance = nullptr;
	int sample_rate = 8000;
	int enable = 1;                    // Disable if the sample rate doesn't match webrtc requirement
	int silence_detection_enabled = 0; // silence detection enabled information
	uint64_t silence_duration = 0;     // silence threshold duration in ms
	uint64_t last_voice_detection = 0; // last time voice was detected
	int silence_event_send = 0;
	int voice_event_send = 0;
} MSWebRTCVAD;

static int check_webrtc_sample_rate(MSFilter *f) {
	MSWebRTCVAD *vad = static_cast<MSWebRTCVAD *>(f->data);
	for (unsigned int i = 0; i < NUMBER_SAMPLE_RATES; i++) {
		if (_sample_rates[i] == vad->sample_rate) return 1;
	}
	ms_warning("MSWebRTC VAD: VAD disabled, sample rate(%d) doesn't match requirement", vad->sample_rate);
	return 0;
}

static int vad_enable_silence_detection(MSFilter *f, void *arg) {
	MSWebRTCVAD *vad = static_cast<MSWebRTCVAD *>(f->data);
	vad->silence_detection_enabled = *(int *)arg;
	return 0;
}

static int vad_set_silence_duration_threshold(MSFilter *f, void *arg) {
	MSWebRTCVAD *vad = static_cast<MSWebRTCVAD *>(f->data);
	vad->silence_duration = *(unsigned int *)arg;
	return 0;
}

static int vad_set_mode(MSFilter *f, void *arg) {
	MSWebRTCVAD *vad = static_cast<MSWebRTCVAD *>(f->data);
	int mode = *(int *)arg;
	if (WebRtcVad_set_mode(vad->instance, mode) == -1) {
		ms_warning("MSWebRTC VAD: can't set mode %d", mode);
	}
	return 0;
}

static int vad_set_sample_rate(MSFilter *f, void *arg) {
	MSWebRTCVAD *vad = static_cast<MSWebRTCVAD *>(f->data);
	vad->sample_rate = *(int *)arg;
	vad->enable = check_webrtc_sample_rate(f);
	return 0;
}

static void vad_init(MSFilter *f) {
	MSWebRTCVAD *vad = new MSWebRTCVAD();
	vad->instance = WebRtcVad_Create();
	if (WebRtcVad_Init(vad->instance) == -1) {
		vad->instance = nullptr;
	}
	f->data = vad;
}

static void vad_process(MSFilter *f) {
	mblk_t *im;
	MSWebRTCVAD *vad = static_cast<MSWebRTCVAD *>(f->data);
	while ((im = ms_queue_get(f->inputs[0]))) {
		if (vad->enable && vad->instance) {
			if (vad->silence_detection_enabled && vad->silence_duration > 0) {
				// Silence detection mode
				if (WebRtcVad_Process(vad->instance, vad->sample_rate, (int16_t *)im->b_rptr, msgdsize(im) / 2)) {
					if (vad->silence_event_send) {
						uint64_t silence_time = f->ticker->time - vad->last_voice_detection;
						ms_filter_notify(f, MS_VAD_EVENT_SILENCE_ENDED, (void *)&silence_time);
						ms_message("Silence end at %llu with %llu ms of silence", (unsigned long long)f->ticker->time,
						           (unsigned long long)silence_time);
					}
					vad->last_voice_detection = f->ticker->time;
					vad->silence_event_send = 0;
				} else if ((vad->last_voice_detection + vad->silence_duration) <= f->ticker->time) {
					if (!vad->silence_event_send) {
						ms_filter_notify_no_arg(f, MS_VAD_EVENT_SILENCE_DETECTED);
						ms_message("Silence begin at %llu", (unsigned long long)f->ticker->time);
					}
					vad->silence_event_send = 1;
				}
			} else {
				// Voice detection mode
				if (WebRtcVad_Process(vad->instance, vad->sample_rate, (int16_t *)im->b_rptr, msgdsize(im) / 2)) {
					if (!vad->voice_event_send) {
						ms_filter_notify_no_arg(f, MS_VAD_EVENT_VOICE_DETECTED);
						vad->voice_event_send = 1;
					}
					vad->last_voice_detection = f->ticker->time;
				} else {
					// Wait a little before sending the event in case of a small pause in the voice
					if (vad->voice_event_send && (vad->last_voice_detection + 1000 <= f->ticker->time)) {
						ms_filter_notify_no_arg(f, MS_VAD_EVENT_VOICE_ENDED);
						vad->voice_event_send = 0;
					}
				}
			}
		}
		ms_queue_put(f->outputs[0], im);
	}
}

static void vad_uninit(MSFilter *f) {
	MSWebRTCVAD *vad = static_cast<MSWebRTCVAD *>(f->data);
	if (vad->instance) WebRtcVad_Free(vad->instance);
	vad->instance = nullptr;
	delete vad;
	f->data = nullptr;
}

static MSFilterMethod vad_methods[] = {{MS_VAD_ENABLE_SILENCE_DETECTION, vad_enable_silence_detection},
                                       {MS_VAD_SET_SILENCE_DURATION_THRESHOLD, vad_set_silence_duration_threshold},
                                       {MS_VAD_SET_MODE, vad_set_mode},
                                       {MS_FILTER_SET_SAMPLE_RATE, vad_set_sample_rate},
                                       {0, nullptr}};

extern "C" MSFilterDesc ms_webrtc_vad_desc = {MS_FILTER_PLUGIN_ID,
                                              "MSWebRtcVADDec",
                                              "WebRtc's VAD",
                                              MS_FILTER_OTHER,
                                              "VAD",
                                              1, // Inputs
                                              1, // Outputs
                                              vad_init,
                                              nullptr,
                                              vad_process,
                                              nullptr,
                                              vad_uninit,
                                              vad_methods,
                                              0};

/*
 * Copyright (c) 2010-2024 Belledonne Communications SARL.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mediastreamer2/msfilter.h"
#include "mswebrtc_aec3.h"

static void webrtc_aec3_init(MSFilter *f) {
	mswebrtc_aec3::mswebrtc_aec3 *aec3Inst = new mswebrtc_aec3::mswebrtc_aec3(f);
	f->data = aec3Inst;
}

static void webrtc_aec3_uninit(MSFilter *f) {
	mswebrtc_aec3::mswebrtc_aec3 *aec3Inst = static_cast<mswebrtc_aec3::mswebrtc_aec3 *>(f->data);
	aec3Inst->uninit();
	delete aec3Inst;
	f->data = nullptr;
}

static void webrtc_aec3_preprocess(MSFilter *f) {
	mswebrtc_aec3::mswebrtc_aec3 *aecInst = static_cast<mswebrtc_aec3::mswebrtc_aec3 *>(f->data);
	aecInst->preprocess();
}

static void webrtc_aec3_process(MSFilter *f) {
	mswebrtc_aec3::mswebrtc_aec3 *aecInst = static_cast<mswebrtc_aec3::mswebrtc_aec3 *>(f->data);
	aecInst->process(f);
}

static void webrtc_aec3_postprocess(MSFilter *f) {
	mswebrtc_aec3::mswebrtc_aec3 *aecInst = static_cast<mswebrtc_aec3::mswebrtc_aec3 *>(f->data);
	aecInst->postprocess();
}

static int webrtc_aec3_set_sample_rate(MSFilter *f, void *arg) {
	mswebrtc_aec3::mswebrtc_aec3 *aecInst = static_cast<mswebrtc_aec3::mswebrtc_aec3 *>(f->data);
	return aecInst->set_sample_rate(*static_cast<int *>(arg));
}

static int webrtc_aec3_get_sample_rate(MSFilter *f, void *arg) {
	mswebrtc_aec3::mswebrtc_aec3 *aecInst = static_cast<mswebrtc_aec3::mswebrtc_aec3 *>(f->data);
	*static_cast<int *>(arg) = aecInst->get_sample_rate();
	return 0;
}

static int webrtc_aec3_set_framesize(MSFilter *f, void *arg) {
	/* Do nothing because the WebRTC echo canceller 3 works at a given frame
	 * duration: 100 ms */
	return 0;
}

static int webrtc_aec3_set_delay(MSFilter *f, void *arg) {
	mswebrtc_aec3::mswebrtc_aec3 *aecInst = static_cast<mswebrtc_aec3::mswebrtc_aec3 *>(f->data);
	aecInst->set_delay(*static_cast<int *>(arg));
	return 0;
}

static int webrtc_aec3_get_delay(MSFilter *f, void *arg) {
	mswebrtc_aec3::mswebrtc_aec3 *aecInst = static_cast<mswebrtc_aec3::mswebrtc_aec3 *>(f->data);
	*static_cast<int *>(arg) = aecInst->get_delay();
	return 0;
}

static int webrtc_aec3_set_tail_length(MSFilter *f, void *arg) {
	/* Do nothing because this is not needed by the WebRTC echo canceller. */
	return 0;
}

static int webrtc_aec3_set_bypass_mode(MSFilter *f, void *arg) {
	mswebrtc_aec3::mswebrtc_aec3 *aecInst = static_cast<mswebrtc_aec3::mswebrtc_aec3 *>(f->data);
	bool_t *bypass = static_cast<bool_t *>(arg);
	aecInst->set_bypass_mode(*bypass);
	return 0;
}

static int webrtc_aec3_get_bypass_mode(MSFilter *f, void *arg) {
	mswebrtc_aec3::mswebrtc_aec3 *aecInst = static_cast<mswebrtc_aec3::mswebrtc_aec3 *>(f->data);
	*static_cast<bool_t *>(arg) = aecInst->get_bypass_mode();
	return 0;
}

static int webrtc_aec3_set_state(MSFilter *f, void *arg) {
	mswebrtc_aec3::mswebrtc_aec3 *aecInst = static_cast<mswebrtc_aec3::mswebrtc_aec3 *>(f->data);
	aecInst->set_state(ms_strdup((const char *)arg));
	return 0;
}

static int webrtc_aec3_get_state(MSFilter *f, void *arg) {
	mswebrtc_aec3::mswebrtc_aec3 *aecInst = static_cast<mswebrtc_aec3::mswebrtc_aec3 *>(f->data);
	*(char **)arg = aecInst->get_state();
	return 0;
}

static MSFilterMethod webrtc_aec3_methods[] = {{MS_FILTER_SET_SAMPLE_RATE, webrtc_aec3_set_sample_rate},
                                               {MS_FILTER_GET_SAMPLE_RATE, webrtc_aec3_get_sample_rate},
                                               {MS_ECHO_CANCELLER_SET_TAIL_LENGTH, webrtc_aec3_set_tail_length},
                                               {MS_ECHO_CANCELLER_SET_DELAY, webrtc_aec3_set_delay},
                                               {MS_ECHO_CANCELLER_SET_FRAMESIZE, webrtc_aec3_set_framesize},
                                               {MS_ECHO_CANCELLER_SET_BYPASS_MODE, webrtc_aec3_set_bypass_mode},
                                               {MS_ECHO_CANCELLER_GET_BYPASS_MODE, webrtc_aec3_get_bypass_mode},
                                               {MS_ECHO_CANCELLER_GET_STATE_STRING, webrtc_aec3_get_state},
                                               {MS_ECHO_CANCELLER_SET_STATE_STRING, webrtc_aec3_set_state},
                                               {MS_ECHO_CANCELLER_GET_DELAY, webrtc_aec3_get_delay},
                                               {0, NULL}};

MSFilterDesc ms_webrtc_aec3_desc = {MS_FILTER_PLUGIN_ID,
                                    "MSWebRTCAEC3",
                                    "Echo canceller using AEC3 in WebRTC library.",
                                    MS_FILTER_OTHER,
                                    NULL,
                                    2,
                                    2,
                                    webrtc_aec3_init,
                                    webrtc_aec3_preprocess,
                                    webrtc_aec3_process,
                                    webrtc_aec3_postprocess,
                                    webrtc_aec3_uninit,
                                    webrtc_aec3_methods,
                                    0};

#ifdef _MSC_VER
#define MS_PLUGIN_DECLARE(type) extern "C" __declspec(dllexport) type
#else
#define MS_PLUGIN_DECLARE(type) extern "C" type
#endif

MS_PLUGIN_DECLARE(void) libmswebrtcaec3_init(MSFactory *factory) {
	ms_factory_register_filter(factory, &ms_webrtc_aec3_desc);
	ms_message("libmswebrtcaec3 plugin registered.");
}

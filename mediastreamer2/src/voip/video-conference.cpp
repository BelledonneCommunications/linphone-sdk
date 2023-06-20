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

#ifdef VIDEO_ENABLED
#include <bctoolbox/defs.h>

#include "mediastreamer2/msconference.h"
#include "mediastreamer2/msrtp.h"
#include "video-conference.h"

using namespace ms2;

//------------------------------------------------------------------------------

extern "C" int ms_video_conference_get_size(MSVideoConference *obj) {
	return ((VideoConferenceGeneric *)obj)->getSize();
}

extern "C" const bctbx_list_t *ms_video_conference_get_members(const MSVideoConference *obj) {
	return ((VideoConferenceGeneric *)obj)->getMembers();
}

extern "C" MSVideoEndpoint *ms_video_conference_get_video_placeholder_member(const MSVideoConference *obj) {
	return (MSVideoEndpoint *)(((VideoConferenceGeneric *)obj)->getVideoPlaceholderMember());
}

extern "C" void ms_video_conference_add_member(MSVideoConference *obj, MSVideoEndpoint *ep) {
	((VideoConferenceGeneric *)obj)->addMember((VideoEndpoint *)ep);
}

extern "C" void ms_video_conference_remove_member(MSVideoConference *obj, MSVideoEndpoint *ep) {
	((VideoConferenceGeneric *)obj)->removeMember((VideoEndpoint *)ep);
}

extern "C" void ms_video_conference_set_focus(MSVideoConference *obj, MSVideoEndpoint *ep) {
	((VideoConferenceGeneric *)obj)->setFocus((VideoEndpoint *)ep);
}

extern "C" void ms_video_conference_destroy(MSVideoConference *obj) {
	delete ((VideoConferenceGeneric *)obj);
}

extern "C" MSVideoConference *ms_video_conference_new(MSFactory *f, const MSVideoConferenceParams *params) {
	return (MSVideoConference *)(new VideoConferenceAllToAll(f, params));
}

namespace ms2 {

//-----------------------------------------------
int VideoConferenceGeneric::getSize() const {
	return (int)bctbx_list_size(mMembers);
}

const bctbx_list_t *VideoConferenceGeneric::getMembers() const {
	return mMembers;
}

VideoEndpoint *VideoConferenceGeneric::getVideoPlaceholderMember() const {
	return mVideoPlaceholderMember;
}

VideoEndpoint *VideoConferenceGeneric::getMemberAtInputPin(int pin) const {
	const MSList *it;
	for (it = mMembers; it != NULL; it = it->next) {
		VideoEndpoint *ep = (VideoEndpoint *)it->data;
		if (ep->mPin == pin) return ep;
	}
	if (mVideoPlaceholderMember && (mVideoPlaceholderMember->mPin == pin)) return mVideoPlaceholderMember;
	return NULL;
}

VideoEndpoint *VideoConferenceGeneric::getMemberAtOutputPin(int pin) const {
	const MSList *it;
	for (it = mMembers; it != NULL; it = it->next) {
		VideoEndpoint *ep = (VideoEndpoint *)it->data;
		if (ep->mOutPin == pin) return ep;
	}
	if (mVideoPlaceholderMember && (mVideoPlaceholderMember->mOutPin == pin)) return mVideoPlaceholderMember;
	return NULL;
}

MSFilter *VideoConferenceGeneric::getMixer() const {
	return mMixer;
}

void VideoConferenceGeneric::applyNewBitrateRequest() {
	const bctbx_list_t *elem;
	for (elem = mMembers; elem != NULL; elem = elem->next) {
		VideoEndpoint *ep = (VideoEndpoint *)elem->data;
		if (ep->mIsRemote) {
			if (ep->mSt->ms.bandwidth_controller) {
				ms_bandwidth_controller_set_maximum_bandwidth_usage(ep->mSt->ms.bandwidth_controller, mBitrate);
			}
		} else {
			media_stream_process_tmmbr((MediaStream *)ep->mSt, mBitrate);
		}
	}
}

//------------------------------------------------------------------------
void plumb_to_conf(VideoEndpoint *ep) {
	VideoConferenceGeneric *conf = (VideoConferenceGeneric *)ep->mConference;
	MSVideoConferenceFilterPinControl pc;

	if (ep->mMixerOut.filter && ep->mOutPin != -1) {
		ms_filter_link(conf->getMixer(), ep->mOutPin, ep->mMixerOut.filter, ep->mMixerOut.pin);
	}

	if (ep->mMixerIn.filter) {
		ms_filter_link(ep->mMixerIn.filter, ep->mMixerIn.pin, conf->getMixer(), ep->mPin);
	}

	if (ep->mPin > -1) {
		pc.pin = ep->mPin;
		pc.enabled = !ep->mIsRemote;
		conf->setLocalMember(pc);
	}
}

void unplumb_from_conf(VideoEndpoint *ep) {
	VideoConferenceGeneric *conf = (VideoConferenceGeneric *)ep->mConference;

	if (ep->mMixerIn.filter) {
		ms_filter_unlink(ep->mMixerIn.filter, ep->mMixerIn.pin, conf->getMixer(), ep->mPin);
	}
	if (ep->mMixerOut.filter && ep->mOutPin != -1) {
		ms_filter_unlink(conf->getMixer(), ep->mOutPin, ep->mMixerOut.filter, ep->mMixerOut.pin);
	}
}

void on_filter_event(void *data, BCTBX_UNUSED(MSFilter *f), unsigned int event_id, void *event_data) {
	VideoConferenceGeneric *obj = (VideoConferenceGeneric *)data;
	int pin;
	VideoEndpoint *ep;

	switch (event_id) {
		case MS_VIDEO_ROUTER_SEND_FIR:
			pin = *(int *)event_data;
			ep = obj->getMemberAtInputPin(pin);
			if (ep) {
				ms_message("Filter needs a refresh frame (FIR) for [%s] endpoint created from VideoStream [%p]",
				           ep->mIsRemote ? "remote" : "local", ep->mSt);
				if (ep->mIsRemote) {
					video_stream_send_fir(ep->mSt);
				} else {
					video_stream_send_vfu(ep->mSt);
				}
			} else {
				ms_error("Filter generated an event for an unknown pin [%i]", pin);
			}
			break;
		case MS_VIDEO_ROUTER_SEND_PLI:
			pin = *(int *)event_data;
			ep = obj->getMemberAtInputPin(pin);
			if (ep) {
				ms_message("Filter needs a refresh frame (PLI) for [%s] endpoint created from VideoStream [%p]",
				           ep->mIsRemote ? "remote" : "local", ep->mSt);
				if (ep->mIsRemote) {
					video_stream_send_pli(ep->mSt);
				} else {
					ms_filter_call_method_noarg(ep->mSt->ms.encoder, MS_VIDEO_ENCODER_NOTIFY_PLI);
				}
			} else {
				ms_error("Filter generated an event for an unknown pin [%i]", pin);
			}
			break;
		case MS_VIDEO_ROUTER_OUTPUT_SWITCHED:
			MSVideoRouterSwitchedEventData *data = (MSVideoRouterSwitchedEventData *)event_data;
			VideoEndpoint *in = obj->getMemberAtInputPin(data->input);
			VideoEndpoint *out = obj->getMemberAtOutputPin(data->output);

			if (in != NULL && out != NULL) {
				uint32_t ssrc = media_stream_get_recv_ssrc(&in->mSt->ms);

				ms_filter_call_method(out->mSt->ms.rtpsend, MS_RTP_SEND_SET_ACTIVE_SPEAKER_SSRC, &ssrc);
			}
			break;
	}
}

void ms_video_conference_process_encoder_control(BCTBX_UNUSED(VideoStream *vs),
                                                 unsigned int method_id,
                                                 void *,
                                                 void *user_data) {
	VideoEndpoint *ep = (VideoEndpoint *)user_data;
	VideoConferenceGeneric *conf = (VideoConferenceGeneric *)ep->mConference;
	switch (method_id) {
		case MS_VIDEO_ENCODER_NOTIFY_FIR:
			conf->notifyFir(ep->mOutPin);
			break;
		case MS_VIDEO_ENCODER_NOTIFY_PLI:
		case MS_VIDEO_ENCODER_NOTIFY_SLI:
			/* SLI and PLI are processed in the same way.*/
			conf->notifySli(ep->mOutPin);
			break;
		case MS_VIDEO_ENCODER_NOTIFY_RPSI:
			/* Ignored. We can't do anything with RPSI in a case where there are multiple receivers of a given encoder
			 * stream.*/
			break;
	}
}

} // namespace ms2
#endif

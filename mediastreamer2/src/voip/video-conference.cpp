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
	return ((VideoConferenceAllToAll *)obj)->getSize();
}

extern "C" const bctbx_list_t *ms_video_conference_get_members(const MSVideoConference *obj) {
	return ((VideoConferenceAllToAll *)obj)->getMembers();
}

extern "C" void ms_video_conference_add_member(MSVideoConference *obj, MSVideoEndpoint *ep) {
	((VideoConferenceAllToAll *)obj)->addMember((VideoEndpoint *)ep);
}

extern "C" void ms_video_conference_remove_member(MSVideoConference *obj, MSVideoEndpoint *ep) {
	((VideoConferenceAllToAll *)obj)->removeMember((VideoEndpoint *)ep);
}

extern "C" void ms_video_conference_set_focus(MSVideoConference *obj, MSVideoEndpoint *ep) {
	((VideoConferenceAllToAll *)obj)->setFocus((VideoEndpoint *)ep);
}

extern "C" void ms_video_conference_destroy(MSVideoConference *obj) {
	delete ((VideoConferenceAllToAll *)obj);
}

extern "C" MSVideoConference *ms_video_conference_new(MSFactory *f, const MSVideoConferenceParams *params) {
	return (MSVideoConference *)(new VideoConferenceAllToAll(f, params));
}

extern "C" const MSVideoConferenceParams *ms_video_conference_get_params(MSVideoConference *obj) {
	return &((VideoConferenceAllToAll *)obj)->getConferenceParams();
}

namespace ms2 {

//-----------------------------------------------
int VideoConferenceAllToAll::getSize() const {
	return (int)bctbx_list_size(mMembers);
}

const bctbx_list_t *VideoConferenceAllToAll::getMembers() const {
	return mMembers;
}

VideoEndpoint *VideoConferenceAllToAll::getMemberAtInputPin(int pin) const {
	const MSList *it;
	for (it = mMembers; it != NULL; it = it->next) {
		VideoEndpoint *ep = (VideoEndpoint *)it->data;
		if (ep->mPin == pin) return ep;
	}
	return NULL;
}

VideoEndpoint *VideoConferenceAllToAll::getMemberAtOutputPin(int pin) const {
	const MSList *it;
	for (it = mMembers; it != NULL; it = it->next) {
		VideoEndpoint *ep = (VideoEndpoint *)it->data;
		if (ep->mOutPin == pin) return ep;
	}
	return NULL;
}

MSFilter *VideoConferenceAllToAll::getMixer() const {
	return mMixer;
}

void VideoConferenceAllToAll::applyNewBitrateRequest() {
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
	VideoConferenceAllToAll *conf = (VideoConferenceAllToAll *)ep->mConference;
	MSPacketRouterPinControl pc;

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
	VideoConferenceAllToAll *conf = (VideoConferenceAllToAll *)ep->mConference;

	if (ep->mMixerIn.filter) {
		ms_filter_unlink(ep->mMixerIn.filter, ep->mMixerIn.pin, conf->getMixer(), ep->mPin);
	}
	if (ep->mMixerOut.filter && ep->mOutPin != -1) {
		ms_filter_unlink(conf->getMixer(), ep->mOutPin, ep->mMixerOut.filter, ep->mMixerOut.pin);
	}
}

void on_filter_event(void *data, BCTBX_UNUSED(MSFilter *f), unsigned int event_id, void *event_data) {
	VideoConferenceAllToAll *obj = (VideoConferenceAllToAll *)data;
	int pin;
	VideoEndpoint *ep;

	switch (event_id) {
		case MS_PACKET_ROUTER_SEND_FIR:
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
		case MS_PACKET_ROUTER_SEND_PLI:
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
		case MS_PACKET_ROUTER_OUTPUT_SWITCHED:
			MSPacketRouterSwitchedEventData *eventData = (MSPacketRouterSwitchedEventData *)event_data;
			VideoEndpoint *in = obj->getMemberAtInputPin(eventData->input);
			VideoEndpoint *out = obj->getMemberAtOutputPin(eventData->output);

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
	VideoConferenceAllToAll *conf = (VideoConferenceAllToAll *)ep->mConference;
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

VideoConferenceAllToAll::VideoConferenceAllToAll(MSFactory *f, const MSVideoConferenceParams *params) {
	const MSFmtDescriptor *fmt;
	MSVideoSize vsize = {0};
	MSTickerParams tickerParams;
	memset(&tickerParams, 0, sizeof(tickerParams));
	tickerParams.name = "Video conference(all to all)";
	tickerParams.prio = __ms_get_default_prio(TRUE);

	mTicker = ms_ticker_new_with_params(&tickerParams);
	mMixer = ms_factory_create_filter(f, MS_PACKET_ROUTER_ID);
	mVoidSource = ms_factory_create_filter(f, MS_VOID_SOURCE_ID);
	mVoidOutput = ms_factory_create_filter(f, MS_VOID_SINK_ID);

	MSPacketRouterMode mode = MS_PACKET_ROUTER_MODE_VIDEO;
	ms_filter_call_method(mMixer, MS_PACKET_ROUTER_SET_ROUTING_MODE, &mode);

	fmt = ms_factory_get_video_format(f, params->codec_mime_type ? params->codec_mime_type : "VP8", vsize, 0, NULL);
	ms_filter_call_method(mMixer, MS_FILTER_SET_INPUT_FMT, (void *)fmt);

	bool_t full_packet = params->mode == MSConferenceModeRouterFullPacket ? TRUE : FALSE;
	ms_filter_call_method(mMixer, MS_PACKET_ROUTER_SET_FULL_PACKET_MODE_ENABLED, &full_packet);

	ms_filter_add_notify_callback(mMixer, on_filter_event, this, TRUE);
	mCfparams = *params;

	std::fill_n(mOutputs, ROUTER_MAX_OUTPUT_CHANNELS, -1);
	std::fill_n(mInputs, ROUTER_MAX_INPUT_CHANNELS, -1);

	ms_filter_link(mVoidSource, 0, mMixer, 0);
	ms_filter_link(mMixer, 0, mVoidOutput, 0);
	ms_ticker_attach(mTicker, mMixer);
}

const MSVideoConferenceParams &VideoConferenceAllToAll::getConferenceParams() const {
	return mCfparams;
}

int VideoConferenceAllToAll::findFreeOutputPin() {
	int i;
	// pin 0 is reserved for voidsink
	for (i = 1; i < mMixer->desc->noutputs; ++i) {
		if (mOutputs[i] == -1) {
			mOutputs[i] = 0;
			return i;
		}
	}
	ms_fatal("No more free output pin in video router filter");
	return -1;
}

int VideoConferenceAllToAll::findFreeInputPin() {
	int i;
	// pin 0 is reserved for voidsource
	for (i = 1; i < mMixer->desc->ninputs; ++i) {
		if (mInputs[i] == -1) {
			mInputs[i] = 0;
			return i;
		}
	}
	ms_fatal("No more free input pin in video router filter");
	return -1;
}

int VideoConferenceAllToAll::findSourcePin(const std::string &participant) {
	VideoEndpoint *ret = nullptr;
	for (const bctbx_list_t *elem = getMembers(); elem != nullptr; elem = elem->next) {
		VideoEndpoint *ep_it = (VideoEndpoint *)elem->data;
		if (ep_it->mName.compare(participant) == 0) {
			if (!ret) {
				ms_message("Found source pin %d for %s", ep_it->mPin, participant.c_str());
				ret = ep_it;
			} else {
				ms_error("There are more than one endpoint with label '%s' !", participant.c_str());
			}
		}
	}
	if (!ret) ms_message("Can not find source pin for '%s'", participant.c_str());
	return ret ? ret->mPin : -1;
}

static void configureEndpoint(VideoEndpoint *ep) {
	VideoConferenceAllToAll *conf = (VideoConferenceAllToAll *)ep->mConference;
	conf->connectEndpoint(ep);
}

static void unconfigureEndpoint(VideoEndpoint *ep, int *pin) {
	int source = *pin;
	if (ep->mSource == source) {
		ms_message("[all to all] unconfigure endpoint at output pin %d with source %d", ep->mOutPin, ep->mSource);
		ep->mSource = -1;
		VideoConferenceAllToAll *conf = (VideoConferenceAllToAll *)ep->mConference;
		conf->unconfigureOutput(ep->mOutPin);
	}
}

void VideoConferenceAllToAll::addMember(VideoEndpoint *ep) {
	MSVideoContent content = video_stream_get_content(ep->mSt);
	MediaStreamDir dir = media_stream_get_direction(&ep->mSt->ms);
	/* now connect to the filter */
	if (dir != MediaStreamSendRecv && ep->mName.empty()) {
		ms_error("[all to all] conference %p add member %p failed because the label is empty.", this, ep);
		return;
	}

	ep->mConference = (MSVideoConference *)this;
	if (ep->mIsRemote && dir == MediaStreamSendOnly && content != MSVideoContentSpeaker) {
		/* Case of a remote participant that is requesting to receive a specific video stream pointed by its label */
		ep->mOutPin = findFreeOutputPin();
		ms_message("[all to all] conference %p add endpoint %s with output pin %d", this, ep->mName.c_str(),
		           ep->mOutPin);
		ms_ticker_detach(mTicker, mMixer);
		plumb_to_conf(ep);
		video_stream_set_encoder_control_callback(ep->mSt, ms_video_conference_process_encoder_control, ep);
		ms_ticker_attach(mTicker, mMixer);
		connectEndpoint(ep);
		mEndpoints = bctbx_list_append(mEndpoints, ep);
		return;
	}

	if (dir != MediaStreamSendRecv && findSourcePin(ep->mName) > -1) return;

	ep->mPin = findFreeInputPin();
	ms_ticker_detach(mTicker, mMixer);
	if (content == MSVideoContentSpeaker) {
		/* case of participant that is willing to receive the active speaker stream */
		ep->mOutPin = findFreeOutputPin();
		ep->mLinkSource = ep->mPin;
		video_stream_set_encoder_control_callback(ep->mSt, ms_video_conference_process_encoder_control, ep);
	}
	ms_message("[all to all] conference %p add remote[%d] member %s to pin input %d output %d", this, ep->mIsRemote,
	           ep->mName.c_str(), ep->mPin, ep->mOutPin);
	plumb_to_conf(ep);
	ms_ticker_attach(mTicker, mMixer);
	mMembers = bctbx_list_append(mMembers, ep);
	if (dir == MediaStreamSendRecv || dir == MediaStreamSendOnly) configureOutput(ep);
	bctbx_list_for_each(mEndpoints, (void (*)(void *))configureEndpoint);
}

void VideoConferenceAllToAll::removeMember(VideoEndpoint *ep) {
	if (bctbx_list_find(mMembers, ep) != NULL) {
		ms_message("[VideoConferenceAllToAll]: conference %p remove member %s with input pin %d output pin %d", this,
		           ep->mName.c_str(), ep->mPin, ep->mOutPin);
		mMembers = bctbx_list_remove(mMembers, ep);

		mInputs[ep->mPin] = -1;
		if (ep->mOutPin > -1) {
			mOutputs[ep->mOutPin] = -1;

			MediaStreamDir dir = media_stream_get_direction(&ep->mSt->ms);
			if (dir == MediaStreamSendRecv || dir == MediaStreamSendOnly) unconfigureOutput(ep->mOutPin);
		}
		bctbx_list_for_each2(mEndpoints, (void (*)(void *, void *))unconfigureEndpoint, (void *)&ep->mPin);
	} else if (bctbx_list_find(mEndpoints, ep) != NULL) {
		ms_message("[VideoConferenceAllToAll] conference %p remove endpoint %s with output pin %d", this,
		           ep->mName.c_str(), ep->mOutPin);
		mEndpoints = bctbx_list_remove(mEndpoints, ep);
		unconfigureOutput(ep->mOutPin);
		mOutputs[ep->mOutPin] = -1;
	} else {
		return;
	}

	video_stream_set_encoder_control_callback(ep->mSt, NULL, NULL);
	ms_ticker_detach(mTicker, mMixer);

	unplumb_from_conf(ep);
	ep->mConference = NULL;
	if (mMembers || mEndpoints) {
		ms_ticker_attach(mTicker, mMixer);
	}
}

void VideoConferenceAllToAll::chooseNewFocus() {
	if (mMembers == nullptr) return;
	size_t size = bctbx_list_size(mMembers);
	int newFocusIndex = bctbx_random() % (int)size;
	VideoEndpoint *ep = (VideoEndpoint *)bctbx_list_nth_data(mMembers, newFocusIndex);
	if (ep) {
		setFocus(ep);
	} else {
		ms_error("VideoConferenceAllToAll::chooseNewFocus(): bug here.");
	}
}

void VideoConferenceAllToAll::connectEndpoint(VideoEndpoint *ep) {
	if (ep->mSource > -1) return;
	ep->mSource = findSourcePin(ep->mName);
	if (ep->mSource > -1) {
		ms_message("[all to all] configure endpoint output pin %d with source pin %d", ep->mOutPin, ep->mSource);
		configureOutput(ep);
	} else {
		ms_warning("There is no source connected for stream labeled '%s'", ep->mName.c_str());
	}
}

void VideoConferenceAllToAll::updateBitrateRequest() {
	const bctbx_list_t *elem;
	int min_of_tmmbr = -1;
	for (elem = mEndpoints; elem != NULL; elem = elem->next) {
		VideoEndpoint *ep = (VideoEndpoint *)elem->data;
		if (ep->mSt->content != MSVideoContentThumbnail && ep->mLastTmmbrReceived != 0) {
			if (min_of_tmmbr == -1) {
				min_of_tmmbr = ep->mLastTmmbrReceived;
			} else {
				if (ep->mLastTmmbrReceived < min_of_tmmbr) {
					min_of_tmmbr = ep->mLastTmmbrReceived;
				}
			}
		}
	}
	for (elem = mMembers; elem != NULL; elem = elem->next) {
		VideoEndpoint *ep = (VideoEndpoint *)elem->data;
		if ((ep->mOutPin > -1) && ep->mLastTmmbrReceived != 0) {
			if (min_of_tmmbr == -1) {
				min_of_tmmbr = ep->mLastTmmbrReceived;
			} else {
				if (ep->mLastTmmbrReceived < min_of_tmmbr) {
					min_of_tmmbr = ep->mLastTmmbrReceived;
				}
			}
		}
	}
	if (min_of_tmmbr != -1) {
		if (mBitrate != min_of_tmmbr) {
			mBitrate = min_of_tmmbr;
			ms_message("MSVideoConference [%p]: new bitrate requested: %i kbits/s.", this, mBitrate / 1000);
			applyNewBitrateRequest();
		}
	}
}

void VideoConferenceAllToAll::configureOutput(VideoEndpoint *ep) {
	MSPacketRouterPinData pd;
	pd.input = ep->mSource;
	pd.output = ep->mOutPin;
	pd.self = ep->mPin;
	pd.active_speaker_enabled = ep->mLinkSource > -1;
	ms_filter_call_method(mMixer, MS_PACKET_ROUTER_CONFIGURE_OUTPUT, &pd);
}

void VideoConferenceAllToAll::unconfigureOutput(int pin) {
	ms_filter_call_method(mMixer, MS_PACKET_ROUTER_UNCONFIGURE_OUTPUT, &pin);
}

void VideoConferenceAllToAll::setLocalMember(MSPacketRouterPinControl pc) {
	ms_filter_call_method(mMixer, MS_PACKET_ROUTER_SET_AS_LOCAL_MEMBER, &pc);
}

void VideoConferenceAllToAll::notifyFir(int pin) {
	ms_filter_call_method(mMixer, MS_PACKET_ROUTER_NOTIFY_FIR, &pin);
}

void VideoConferenceAllToAll::notifySli(int pin) {
	ms_filter_call_method(mMixer, MS_PACKET_ROUTER_NOTIFY_PLI, &pin);
}

void VideoConferenceAllToAll::setFocus(VideoEndpoint *ep) {
	ms_filter_call_method(mMixer, MS_PACKET_ROUTER_SET_FOCUS, &ep->mPin);
	mLastFocusPin = ep->mPin;
}

VideoConferenceAllToAll::~VideoConferenceAllToAll() {
	ms_ticker_detach(mTicker, mMixer);
	ms_filter_unlink(mVoidSource, 0, mMixer, 0);
	ms_filter_unlink(mMixer, 0, mVoidOutput, 0);
	ms_filter_destroy(mVoidOutput);
	ms_filter_destroy(mVoidSource);
	ms_ticker_destroy(mTicker);
	ms_filter_destroy(mMixer);
}

} // namespace ms2
#endif

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

#ifdef VIDEO_ENABLED
#include "video-conference.h"
#include "mediastreamer2/msconference.h"


using namespace ms2;
namespace ms2 {

VideoConferenceAllToAll::VideoConferenceAllToAll(MSFactory *f, const MSVideoConferenceParams *params) {
	const MSFmtDescriptor *fmt;
	MSVideoSize vsize = {0};

	mTicker=ms_ticker_new();
	ms_ticker_set_name(mTicker,"Video conference(all to all) MSTicker");
	ms_ticker_set_priority(mTicker,__ms_get_default_prio(FALSE));
	mMixer = ms_factory_create_filter(f, MS_VIDEO_ROUTER_ID);
	mVoidSource = ms_factory_create_filter(f, MS_VOID_SOURCE_ID);
	mVoidOutput = ms_factory_create_filter(f, MS_VOID_SINK_ID);

	fmt = ms_factory_get_video_format(f, params->codec_mime_type ? params->codec_mime_type : "VP8" ,vsize,0,NULL);
	ms_filter_call_method(mMixer, MS_FILTER_SET_INPUT_FMT, (void*)fmt);
	ms_filter_add_notify_callback(mMixer,on_filter_event,this,TRUE);
	mCfparams=*params;

	std::fill_n(mOutputs, ROUTER_MAX_OUTPUT_CHANNELS, -1);
	std::fill_n(mInputs, ROUTER_MAX_INPUT_CHANNELS, -1);

	ms_filter_link(mVoidSource, 0, mMixer, 0);
	ms_filter_link(mMixer, 0, mVoidOutput, 0);
	ms_ticker_attach(mTicker,mMixer);
}

int VideoConferenceAllToAll::findFreeOutputPin() {
	int i;
	// pin 0 is reserved
	for(i=1;i<mMixer->desc->noutputs;++i){
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
	// pin 0 is reserved
	for(i=1;i<mMixer->desc->ninputs-1;++i){
		if (mInputs[i] == -1) {
			mInputs[i] =0;
			return i;
		}
	}
	ms_fatal("No more free input pin in video router filter");
	return -1;
}

int VideoConferenceAllToAll::findSinkPin(std::string participant) {
	for (const bctbx_list_t *elem = mEndpoints; elem != nullptr; elem = elem->next){
		VideoEndpoint *ep_it = (VideoEndpoint *)elem->data;
		if (ep_it->mName.compare(participant) == 0) {
			ms_message("Found sink pin %d for %s", ep_it->mPin, participant.c_str());
			return ep_it->mPin;
		}
	}
	ms_message("Can not find sink pin for %s",participant.c_str());
	return -1;
}

int VideoConferenceAllToAll::findSourcePin(std::string participant) {
	for (const bctbx_list_t *elem = getMembers(); elem != nullptr; elem = elem->next){
		VideoEndpoint *ep_it = (VideoEndpoint *)elem->data;
		if (ep_it->mName.compare(participant) == 0) {
			ms_message("Found source pin %d for %s", ep_it->mPin, participant.c_str());
			return ep_it->mPin;
		}
	}
	ms_message("Can not find source pin for %s",participant.c_str());
	return -1;
}

static void configureEndpoint(VideoEndpoint *ep){
	VideoConferenceAllToAll *conf = (VideoConferenceAllToAll *)ep->mConference;
	conf->connectEndpoint(ep);
}

static void unconfigureEndpoint(VideoEndpoint *ep, int *pin){
	int source = *pin;
	if (ep->mSource == source) {
		ms_message("[all to all] unconfigure endpoint at output pin %d with source %d", ep->mOutPin, ep->mSource);
		ep->mSource = -1;
		VideoConferenceAllToAll *conf = (VideoConferenceAllToAll *)ep->mConference;
		conf->unconfigureOutput(ep->mOutPin);
	}
}

static MSVideoEndpoint *create_video_placeholder_member(VideoConferenceAllToAll *obj) {
	// create an endpoint for static image
	VideoStream *stream = video_stream_new(obj->getMixer()->factory, 65004, 65005, FALSE);
	media_stream_set_direction(&stream->ms, MediaStreamSendOnly);
	MSMediaStreamIO io = MS_MEDIA_STREAM_IO_INITIALIZER;
	io.input.type = MSResourceCamera;
	io.output.type = MSResourceVoid;
	MSWebCam *nowebcam = ms_web_cam_manager_get_cam(obj->getMixer()->factory->wbcmanager, "StaticImage: Static picture");
	io.input.camera = nowebcam;
	RtpProfile *prof = rtp_profile_new("dummy video");
	PayloadType *pt = payload_type_clone(&payload_type_vp8);
	pt->clock_rate = 90000;
	rtp_profile_set_payload(prof, 95, pt);
	video_stream_start_from_io(stream, prof, "127.0.0.1",65004, "127.0.0.1", 65005, 95, &io);
	obj->setProfile(prof);
	return ms_video_endpoint_get_from_stream(stream, FALSE);
}

void VideoConferenceAllToAll::addVideoPlaceholderMember() {
	mVideoPlaceholderMember = (VideoEndpoint *)(create_video_placeholder_member(this));

	mVideoPlaceholderMember->mConference = (MSVideoConference *)this;
	mVideoPlaceholderMember->mPin = mMixer->desc->ninputs-1;
	mVideoPlaceholderMember->switched = TRUE;
	ms_message("[all to all] conference %p add video placeholder %p to pin input %d", this, mVideoPlaceholderMember, mVideoPlaceholderMember->mPin);
	plumb_to_conf(mVideoPlaceholderMember);
}

void VideoConferenceAllToAll::addMember(VideoEndpoint *ep) {
	/* now connect to the filter */
	if (media_stream_get_direction(&ep->mSt->ms) != MediaStreamSendRecv && ep->mName.empty()) {
		ms_error("[all to all] conference %p add member %p failed because the label is empty.", this, ep);
		return;
	}

	ep->mConference = (MSVideoConference *)this;
	if (ep->mIsRemote && media_stream_get_direction(&ep->mSt->ms) == MediaStreamSendOnly) {
		if (findSinkPin(ep->mName) == -1) {
			ep->mOutPin = findFreeOutputPin();
			ms_message("[all to all] conference %p add endpoint %s with output pin %d", this, ep->mName.c_str(), ep->mOutPin);
			ms_ticker_detach(mTicker,mMixer);
			plumb_to_conf(ep);
			video_stream_set_encoder_control_callback(ep->mSt, ms_video_conference_process_encoder_control, ep);
			ms_ticker_attach(mTicker,mMixer);
			connectEndpoint(ep);
			mEndpoints = bctbx_list_append(mEndpoints, ep);
		}
		return;
	}

	if (media_stream_get_direction(&ep->mSt->ms) != MediaStreamSendRecv && findSourcePin(ep->mName) > -1) return;

	ep->mPin = findFreeInputPin();
	ms_ticker_detach(mTicker,mMixer);
	if (media_stream_get_direction(&ep->mSt->ms) == MediaStreamSendRecv) {
		if (mVideoPlaceholderMember == NULL) {
			addVideoPlaceholderMember();
		}
		ep->mOutPin = findFreeOutputPin();
		ep->switched = TRUE;
		video_stream_set_encoder_control_callback(ep->mSt, ms_video_conference_process_encoder_control, ep);
	}
	ms_message("[all to all] conference %p add remote[%d] member %s to pin input %d output %d", this, ep->mIsRemote, ep->mName.c_str(), ep->mPin, ep->mOutPin);
	plumb_to_conf(ep);
	ms_ticker_attach(mTicker,mMixer);
	mMembers=bctbx_list_append(mMembers,ep);
	if (media_stream_get_direction(&ep->mSt->ms) == MediaStreamSendRecv) configureOutput(ep);
	bctbx_list_for_each(mEndpoints, (void (*)(void*))configureEndpoint);
}

void VideoConferenceAllToAll::removeMember(VideoEndpoint *ep) {
	if (bctbx_list_find(mMembers,ep) != NULL) {
		ms_message("[all to all] conference %p remove member %s with input pin %d output pin %d", this, ep->mName.c_str(), ep->mPin, ep->mOutPin);
		mMembers=bctbx_list_remove(mMembers,ep);
		mInputs[ep->mPin] = -1;
		if (ep->mOutPin > -1) mOutputs[ep->mOutPin] = -1;
		bctbx_list_for_each2(mEndpoints, (void (*)(void*,void*))unconfigureEndpoint, (void*)&ep->mPin);
	} else if (bctbx_list_find(mEndpoints,ep) != NULL) {
		ms_message("[all to all] conference %p remove endpoint %s with output pin %d", this, ep->mName.c_str(), ep->mOutPin);
		mEndpoints=bctbx_list_remove(mEndpoints,ep);
		mOutputs[ep->mOutPin] = -1;
	} else {
		return;
	}

	video_stream_set_encoder_control_callback(ep->mSt, NULL, NULL);
	ms_ticker_detach(mTicker,mMixer);

	if (mMembers == NULL && mVideoPlaceholderMember) {
		ms_message("[all to all] conference %p remove video placeholder member %p at pin %d", this, mVideoPlaceholderMember, mVideoPlaceholderMember->mPin);
		video_stream_set_encoder_control_callback(mVideoPlaceholderMember->mSt, NULL, NULL);
		unplumb_from_conf(mVideoPlaceholderMember);
		mVideoPlaceholderMember->redoVideoStreamGraph();
		video_stream_stop(mVideoPlaceholderMember->mSt);
		delete mVideoPlaceholderMember;
		mVideoPlaceholderMember =  NULL;
		rtp_profile_destroy(mLocalDummyProfile);
		mLocalDummyProfile = NULL;
	}

	unplumb_from_conf(ep);
	ep->mConference=NULL;
	if (mMembers || mEndpoints) {
		ms_ticker_attach(mTicker,mMixer);
	}
}

void VideoConferenceAllToAll::connectEndpoint(VideoEndpoint *ep) {
	if (ep->mSource > -1) return;
	ep->mSource = findSourcePin(ep->mName);
	if (ep->mSource > -1) {
		ms_message("[all to all] configure endpoint output pin %d with source pin %d", ep->mOutPin, ep->mSource);
		configureOutput(ep);
	}
}

void VideoConferenceAllToAll::updateBitrateRequest() {
	const bctbx_list_t *elem;
	int min_of_tmmbr = -1;
	for (elem = mEndpoints; elem != NULL; elem = elem->next){
		VideoEndpoint *ep = (VideoEndpoint*) elem->data;
		if (!video_stream_thumbnail_enabled(ep->mSt) && ep->mLastTmmbrReceived != 0){
			if (min_of_tmmbr == -1){
				min_of_tmmbr = ep->mLastTmmbrReceived;
			}else{
				if (ep->mLastTmmbrReceived < min_of_tmmbr){
					min_of_tmmbr = ep->mLastTmmbrReceived;
				}
			}
		}
	}
	for (elem = mMembers; elem != NULL; elem = elem->next){
		VideoEndpoint *ep = (VideoEndpoint*) elem->data;
		if ((ep->mOutPin > -1) && ep->mLastTmmbrReceived != 0){
			if (min_of_tmmbr == -1){
				min_of_tmmbr = ep->mLastTmmbrReceived;
			}else{
				if (ep->mLastTmmbrReceived < min_of_tmmbr){
					min_of_tmmbr = ep->mLastTmmbrReceived;
				}
			}
		}
	}
	if (min_of_tmmbr != -1){
		if (mBitrate != min_of_tmmbr){
			mBitrate = min_of_tmmbr;
			ms_message("MSVideoConference [%p]: new bitrate requested: %i kbits/s.", this, mBitrate/1000);
			applyNewBitrateRequest();
		}
	}
}

void VideoConferenceAllToAll::configureOutput(VideoEndpoint *ep) {
	MSVideoRouterPinData pd;
	pd.input = ep->mSource;
	pd.output = ep->mOutPin;
	pd.switched = ep->switched;
	ms_filter_call_method(mMixer, MS_VIDEO_ROUTER_CONFIGURE_OUTPUT, &pd);
}

void VideoConferenceAllToAll::unconfigureOutput(int pin) {
	ms_filter_call_method(mMixer, MS_VIDEO_ROUTER_UNCONFIGURE_OUTPUT, &pin);
}

void VideoConferenceAllToAll::setLocalMember(MSVideoConferenceFilterPinControl pc) {
	ms_filter_call_method(mMixer, MS_VIDEO_ROUTER_SET_AS_LOCAL_MEMBER, &pc);
}

void VideoConferenceAllToAll::notifyFir(int pin) {
	ms_filter_call_method(mMixer, MS_VIDEO_ROUTER_NOTIFY_FIR, &pin);
}

void VideoConferenceAllToAll::notifySli(int pin) {
	ms_filter_call_method(mMixer, MS_VIDEO_ROUTER_NOTIFY_PLI, &pin);
}

void VideoConferenceAllToAll::setFocus(VideoEndpoint *ep) {
	ms_filter_call_method(mMixer, MS_VIDEO_ROUTER_SET_FOCUS, &ep->mPin);
}

VideoConferenceAllToAll::~VideoConferenceAllToAll() {
	ms_ticker_detach(mTicker,mMixer);
	ms_filter_unlink(mVoidSource, 0, mMixer, 0);
	ms_filter_unlink(mMixer, 0, mVoidOutput, 0);
	ms_filter_destroy(mVoidOutput);
	ms_filter_destroy(mVoidSource);
	ms_ticker_destroy(mTicker);
	ms_filter_destroy(mMixer);
}

}// namespace ms2
#endif

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
#include "mediastreamer2/msconference.h"
#include "video-conference.h"

using namespace ms2;

//------------------------------------------------------------------------------
MSVideoEndpoint *ms_video_endpoint_get_from_stream(VideoStream *st, bool_t is_remote) {
	VideoEndpoint *ep = new VideoEndpoint();
	ep->cutVideoStreamGraph(is_remote, st);
	return (MSVideoEndpoint *)ep;
}

void ms_video_endpoint_set_user_data(MSVideoEndpoint *ep, void *user_data) {
	((VideoEndpoint *)ep)->setUserData(user_data);
}

void *ms_video_endpoint_get_user_data(const MSVideoEndpoint *ep) {
	return ((VideoEndpoint *)ep)->getUserData();
}

MediaStreamDir ms_video_endpoint_get_direction(const MSVideoEndpoint *ep) {
	return ((VideoEndpoint *)ep)->getDirection();
}

void ms_video_endpoint_release_from_stream(MSVideoEndpoint *obj) {
	((VideoEndpoint *)obj)->redoVideoStreamGraph();
	delete ((VideoEndpoint *)obj);
}

namespace ms2 {

static MSCPoint just_before(MSFilter *f) {
	MSQueue *q;
	MSCPoint pnull = {0};
	if ((q = f->inputs[0]) != NULL) {
		return q->prev;
	}
	ms_fatal("No filter before %s", f->desc->name);
	return pnull;
}

static MSCPoint just_after(MSFilter *f) {
	MSQueue *q;
	MSCPoint pnull = {0};
	if ((q = f->outputs[0]) != NULL) {
		return q->next;
	}
	ms_fatal("No filter after %s", f->desc->name);
	return pnull;
}

static void ms_video_endpoint_tmmbr_received(const OrtpEventData *evd, void *user_pointer) {
	VideoEndpoint *ep = (VideoEndpoint *)user_pointer;

	switch (rtcp_RTPFB_get_type(evd->packet)) {
		case RTCP_RTPFB_TMMBR: {
			int tmmbr_mxtbr = (int)rtcp_RTPFB_tmmbr_get_max_bitrate(evd->packet);

			ms_message("MSVideoConference [%p]: received a TMMBR for bitrate %i kbits/s on pin %i.", ep->mConference,
			           (int)(tmmbr_mxtbr / 1000), ep->mPin);
			ep->mLastTmmbrReceived = tmmbr_mxtbr;
			VideoConferenceAllToAll *conf = (VideoConferenceAllToAll *)ep->mConference;
			conf->updateBitrateRequest();
		} break;
		default:
			break;
	}
}

//-------------------------------------------------------------------------

MediaStreamDir VideoEndpoint::getDirection() const {
	if (!mSt) {
		ms_error("Endpoint direction is undefined when the endpoint is not constructed.");
		return MediaStreamSendRecv;
	}
	return media_stream_get_direction(&mSt->ms);
}

void VideoEndpoint::cutVideoStreamGraph(bool isRemote, VideoStream *st) {
	mSt = st;
	if (st->label) {
		mName = st->label;
	}
	/*stop the video graph*/
	if (mSt->source) ms_ticker_detach(mSt->ms.sessions.ticker, mSt->source);
	if (mSt->ms.rtprecv && media_stream_get_direction(&mSt->ms) != MediaStreamSendOnly)
		ms_ticker_detach(mSt->ms.sessions.ticker, mSt->ms.rtprecv);
	mIsRemote = isRemote;
	mInCutPointPrev.pin = 0;
	if (isRemote && media_stream_get_direction(&mSt->ms) != MediaStreamSendOnly) {
		/*we need to cut just after the rtp recveiver*/
		mInCutPointPrev.filter = mSt->ms.rtprecv;
	} else if (!isRemote && media_stream_get_direction(&mSt->ms) != MediaStreamRecvOnly) {
		/*we need to cut just after the encoder*/
		mInCutPointPrev.filter = mSt->ms.encoder;
	}

	if (mInCutPointPrev.filter) {
		mInCutPoint = just_after(mInCutPointPrev.filter);
		ms_filter_unlink(mInCutPointPrev.filter, mInCutPointPrev.pin, mInCutPoint.filter, mInCutPoint.pin);
	}

	mOutCutPoint.pin = 0;
	if (isRemote && media_stream_get_direction(&mSt->ms) != MediaStreamRecvOnly) {
		mOutCutPoint.filter = mSt->ms.rtpsend;
	} else if (!isRemote && media_stream_get_direction(&mSt->ms) != MediaStreamSendOnly) {
		mOutCutPoint.filter = mSt->ms.decoder;
	}
	if (mOutCutPoint.filter) {
		mOutCutPointPrev = just_before(mOutCutPoint.filter);
		ms_filter_unlink(mOutCutPointPrev.filter, mOutCutPointPrev.pin, mOutCutPoint.filter, mOutCutPoint.pin);
	}

	mMixerIn = mInCutPointPrev;
	mMixerOut = mOutCutPoint;

	/* Replaces own's MediaStream tmmbr handler by the video conference implementation.*/
	media_stream_remove_tmmbr_handler((MediaStream *)mSt, media_stream_tmmbr_received, mSt);
	media_stream_add_tmmbr_handler((MediaStream *)mSt, ms_video_endpoint_tmmbr_received, this);
}

void VideoEndpoint::redoVideoStreamGraph() {
	media_stream_remove_tmmbr_handler((MediaStream *)mSt, ms_video_endpoint_tmmbr_received, this);
	media_stream_add_tmmbr_handler((MediaStream *)mSt, media_stream_tmmbr_received, mSt);
	if (mInCutPointPrev.filter)
		ms_filter_link(mInCutPointPrev.filter, mInCutPointPrev.pin, mInCutPoint.filter, mInCutPoint.pin);
	if (mOutCutPointPrev.filter)
		ms_filter_link(mOutCutPointPrev.filter, mOutCutPointPrev.pin, mOutCutPoint.filter, mOutCutPoint.pin);

	if (mSt->source) ms_ticker_attach(mSt->ms.sessions.ticker, mSt->source);
	if (mSt->ms.rtprecv && media_stream_get_direction(&mSt->ms) != MediaStreamSendOnly)
		ms_ticker_attach(mSt->ms.sessions.ticker, mSt->ms.rtprecv);
}

} // namespace ms2
#endif

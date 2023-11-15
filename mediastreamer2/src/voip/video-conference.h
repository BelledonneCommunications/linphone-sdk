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
#ifndef MS_VIDEO_CONFERENCE_H
#define MS_VIDEO_CONFERENCE_H

#include <bctoolbox/defs.h>

#include "mediastreamer2/msconference.h"
#include "mediastreamer2/mspacketrouter.h"
#include "private.h"

namespace ms2 {

class VideoEndpoint {

public:
	void cutVideoStreamGraph(bool isRemote, VideoStream *st);
	void setUserData(void *userData) {
		mUserData = userData;
	};
	void *getUserData() const {
		return mUserData;
	};
	void redoVideoStreamGraph();
	MediaStreamDir getDirection() const;

	VideoStream *mSt = NULL;
	void *mUserData = NULL;
	MSCPoint mOutCutPoint;
	MSCPoint mOutCutPointPrev;
	MSCPoint mInCutPoint;
	MSCPoint mInCutPointPrev;
	MSCPoint mMixerIn;
	MSCPoint mMixerOut;
	MSVideoConference *mConference = NULL;
	int mPin = -1;
	int mOutPin = -1;
	int mSource = -1;
	bool connected = false;
	std::string mName = ""; /*Participant*/
	int mIsRemote = 0;
	int mLastTmmbrReceived = 0; /*Value in bits/s */
	int mLinkSource = -1;
};

class VideoConferenceAllToAll {

public:
	VideoConferenceAllToAll(MSFactory *f, const MSVideoConferenceParams *params);
	~VideoConferenceAllToAll();

	int getSize() const;
	const bctbx_list_t *getMembers() const;
	void removeMember(VideoEndpoint *ep);
	void addMember(VideoEndpoint *ep);
	void setFocus(VideoEndpoint *ep);
	void setProfile(RtpProfile *prof) {
		mLocalDummyProfile = prof;
	};

	MSFilter *getMixer() const;
	void updateBitrateRequest();
	void setLocalMember(MSPacketRouterPinControl pc);
	void notifyFir(int pin);
	void notifySli(int pin);
	VideoEndpoint *getMemberAtInputPin(int pin) const;
	VideoEndpoint *getMemberAtOutputPin(int pin) const;
	void unconfigureOutput(int pin);

	void connectEndpoint(VideoEndpoint *ep);
	int findFreeOutputPin();
	int findFreeInputPin();

protected:
	void chooseNewFocus();
	int findSourcePin(const std::string &participant);
	void configureOutput(VideoEndpoint *ep);
	void applyNewBitrateRequest();

	MSVideoConferenceParams mCfparams{};
	MSTicker *mTicker = nullptr;
	MSFilter *mMixer = nullptr;
	bctbx_list_t *mMembers = nullptr;
	int mBitrate = 0;
	bctbx_list_t *mEndpoints = nullptr;
	RtpProfile *mLocalDummyProfile = nullptr;

	MSFilter *mVoidSource = nullptr;
	MSFilter *mVoidOutput = nullptr;
	int mOutputs[ROUTER_MAX_OUTPUT_CHANNELS];
	int mInputs[ROUTER_MAX_INPUT_CHANNELS];
	int mLastFocusPin = -1;
};

void plumb_to_conf(VideoEndpoint *ep);
void unplumb_from_conf(VideoEndpoint *ep);
void on_filter_event(void *data, MSFilter *f, unsigned int event_id, void *event_data);
void ms_video_conference_process_encoder_control(VideoStream *vs, unsigned int method_id, void *arg, void *user_data);

} // namespace ms2
#endif /* MS_VIDEO_CONFERENCE_H */
#endif

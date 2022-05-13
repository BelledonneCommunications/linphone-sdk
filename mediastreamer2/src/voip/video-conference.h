/*
* Copyright (c) 2010-2020 Belledonne Communications SARL.
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
#ifndef MS_VIDEO_CONFERENCE_H
#define MS_VIDEO_CONFERENCE_H

#include "mediastreamer2/msconference.h"
#include "mediastreamer2/msvideoswitcher.h"
#include "mediastreamer2/msvideorouter.h"
#include "private.h"


namespace ms2 {

class VideoEndpoint {

public:
	void cutVideoStreamGraph(bool isRemote, VideoStream *st);
	void setUserData(void *userData) {mUserData = userData;};
	void *getUserData() const {return mUserData;};
	void redoVideoStreamGraph();

	VideoStream *mSt=NULL;
	void *mUserData=NULL;
	MSCPoint mOutCutPoint;
	MSCPoint mOutCutPointPrev;
	MSCPoint mInCutPoint;
	MSCPoint mInCutPointPrev;
	MSCPoint mMixerIn;
	MSCPoint mMixerOut;
	MSVideoConference *mConference=NULL;
	int mPin=-1;
	int mOutPin=-1;
	int mSource = -1;
	bool connected = false;
	std::string mName=""; /*Particapant*/
	int mIsRemote=0;
	int mLastTmmbrReceived=0; /*Value in bits/s */
	int mLinkSource = -1;
};

class VideoConferenceGeneric {
	
public:
	VideoConferenceGeneric()=default;
	virtual ~VideoConferenceGeneric()=default;

	virtual int getSize() const;
	virtual const bctbx_list_t* getMembers() const;
	virtual void removeMember(VideoEndpoint *ep) = 0;
	virtual void addMember(VideoEndpoint *ep) = 0;
	virtual VideoEndpoint *getVideoPlaceholderMember() const;
	virtual void setFocus(VideoEndpoint *ep) {};
	virtual void setProfile(RtpProfile *prof) {mLocalDummyProfile=prof;};

	virtual MSFilter *getMixer() const;
	virtual void updateBitrateRequest() = 0;
	virtual void setLocalMember(MSVideoConferenceFilterPinControl pc) = 0;
	virtual void notifyFir(int pin) = 0;
	virtual void notifySli(int pin) = 0;
	virtual VideoEndpoint *getMemberAtPin(int pin) const;
	virtual void unconfigureOutput(int pin) {};
	virtual bool allToAllEnabled() const = 0;

protected:
	virtual void addVideoPlaceholderMember() {};
	virtual void setPin(VideoEndpoint *ep) {};
	virtual void configureOutput(VideoEndpoint *ep) {};
	virtual void applyNewBitrateRequest();

	MSVideoConferenceParams mCfparams;
	MSTicker *mTicker = NULL;
	MSFilter *mMixer = NULL;
	bctbx_list_t *mMembers = NULL;
	int mBitrate = 0;
	VideoEndpoint *mVideoPlaceholderMember = NULL;
	bctbx_list_t *mEndpoints = NULL;
	bool allToAll = false;
	RtpProfile *mLocalDummyProfile = NULL;
	
	MSFilter *mVoidSource = NULL;
	MSFilter *mVoidOutput = NULL;
};

class VideoConferenceAllToAll: public VideoConferenceGeneric {

public:
	VideoConferenceAllToAll(MSFactory *f, const MSVideoConferenceParams *params);
	~VideoConferenceAllToAll();
	
	void removeMember(VideoEndpoint *ep) override;
	void addMember(VideoEndpoint *ep) override;
	void setLocalMember(MSVideoConferenceFilterPinControl pc) override;
	void notifyFir(int pin) override;
	void notifySli(int pin) override;
	void connectEndpoint(VideoEndpoint *ep);
	int findFreeOutputPin();
	int findFreeInputPin ();
	void unconfigureOutput(int pin) override;
	bool allToAllEnabled() const override { return true; }
	void setFocus(VideoEndpoint *ep) override;
	void updateBitrateRequest() override;
	

protected:
	void addVideoPlaceholderMember() override;
	int findSourcePin(std::string participant);
	void configureOutput(VideoEndpoint *ep) override;
	int mOutputs[ROUTER_MAX_OUTPUT_CHANNELS] ;
	int mInputs[ROUTER_MAX_INPUT_CHANNELS];
};


void plumb_to_conf(VideoEndpoint *ep);
void unplumb_from_conf(VideoEndpoint *ep);
void on_filter_event(void *data, MSFilter *f, unsigned int event_id, void *event_data);
void ms_video_conference_process_encoder_control(VideoStream *vs, unsigned int method_id, void *arg, void *user_data);


} // namespace ms2
#endif /* MS_VIDEO_CONFERENCE_H */
#endif

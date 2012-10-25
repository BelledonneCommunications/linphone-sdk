/*
 * AudioTrack.cpp
 *
 * Copyright (C) 2009-2012  Belledonne Communications, Grenoble, France
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#include "mediastreamer2/mscommon.h"
#include "AudioTrack.h"


namespace fake_android{
	
	AudioTrack::AudioTrack( audio_stream_type_t streamType,
                                    uint32_t sampleRate  ,
                                    audio_format_t format,
                                    int channelMask,
                                    int frameCount,
                                    audio_output_flags_t flags,
                                    callback_t cbf,
                                    void* user,
                                    int notificationFrames,
                                    int sessionId ){
		mThis=new uint8_t[512];
		mImpl=AudioTrackImpl::get();
		
		/* HACK for Froyo and Gingerbread */
		/* This is needed because the enum values have changed between Gingerbread and ICS */
		if (mImpl->mBeforeICS) {
			ms_message("Android version older than ICS, apply audio channel hack for AudioTrack");
			if ((channelMask & AUDIO_CHANNEL_OUT_MONO) == AUDIO_CHANNEL_OUT_MONO) {
				channelMask = 0x4;
			} else if ((channelMask & AUDIO_CHANNEL_OUT_STEREO) == AUDIO_CHANNEL_OUT_STEREO) {
				channelMask = 0x4|0x8;
			}
		}
		
		mImpl->mCtor.invoke(mThis,streamType,sampleRate,format,channelMask,frameCount,flags,cbf,user,notificationFrames,sessionId);
	}
	
	AudioTrack::~AudioTrack(){
		mImpl->mDtor.invoke(mThis);
		delete mThis;
	}
	
	void AudioTrack::start(){
		mImpl->mStart.invoke(mThis);
	}
	
	void AudioTrack::stop(){
		mImpl->mStop.invoke(mThis);
	}
	
	status_t AudioTrack::initCheck()const{
		return mImpl->mInitCheck.invoke(mThis);
	}
	
	bool AudioTrack::stopped()const{
		return mImpl->mStopped.invoke(mThis);
	}
	
	void AudioTrack::flush(){
		return mImpl->mFlush.invoke(mThis);
	}
	
	status_t AudioTrack::getMinFrameCount(int* frameCount,
                                      audio_stream_type_t streamType,
                                      uint32_t sampleRate){
		if (AudioTrackImpl::get()->mGetMinFrameCount.isFound()){
			return AudioTrackImpl::get()->mGetMinFrameCount.invoke(frameCount,streamType,sampleRate);
		}else{
			//this method didn't existed in 2.2
			//Use hardcoded values instead (1024 frames at 8khz) 
			*frameCount=(1024*sampleRate)/8000;
			return 0;
		}
	}
	
	uint32_t AudioTrack::latency()const{
		return mImpl->mLatency.invoke(mThis);
	}
	
	status_t AudioTrack::getPosition(uint32_t *frames){
		return mImpl->mGetPosition.invoke(mThis,frames);
	}
	
	AudioTrackImpl::AudioTrackImpl(Library *lib) :
		// By default, try to load Android 2.3 symbols
		mCtor(lib,"_ZN7android10AudioTrackC1EijiiijPFviPvS1_ES1_ii"),
		mDtor(lib,"_ZN7android10AudioTrackD1Ev"),
		mInitCheck(lib,"_ZNK7android10AudioTrack9initCheckEv"),
		mStop(lib,"_ZN7android10AudioTrack4stopEv"),
		mStart(lib,"_ZN7android10AudioTrack5startEv"),
		mStopped(lib,"_ZNK7android10AudioTrack7stoppedEv"),
		mFlush(lib,"_ZN7android10AudioTrack5flushEv"),
		mGetMinFrameCount(lib,"_ZN7android10AudioTrack16getMinFrameCountEPiij"),
		mLatency(lib,"_ZNK7android10AudioTrack7latencyEv"),
		mGetPosition(lib,"_ZN7android10AudioTrack11getPositionEPj"),
		mBeforeICS(false)
	{
		// Try some Android 2.2 symbols if not found
		if (!mCtor.isFound()) {
			mCtor.load(lib,"_ZN7android10AudioTrackC1EijiiijPFviPvS1_ES1_i");
		}

		// Then try some Android 4.1 symbols if still not found
		if (!mGetMinFrameCount.isFound()) {
			mGetMinFrameCount.load(lib,"_ZN7android10AudioTrack16getMinFrameCountEPi19audio_stream_type_tj");
		}

		// Dummy symbol loading to detect the Android version, this function did not exist before ICS,
		Function0<uint32_t> getFrameRate(lib,"_ZN7android10AudioTrack9setLoop_lEjji");
		if (!getFrameRate.isFound()) {
			mBeforeICS = true;
		}
	}
	
	bool AudioTrackImpl::init(Library *lib){
		AudioTrackImpl *impl=new AudioTrackImpl(lib);
		if (!impl->mCtor.isFound()) goto fail;
		if (!impl->mDtor.isFound()) goto fail;
		if (!impl->mStart.isFound()) goto fail;
		if (!impl->mStop.isFound()) goto fail;
		if (!impl->mInitCheck.isFound()) goto fail;
		if (!impl->mFlush.isFound()) goto fail;
		if (!impl->mLatency.isFound()) goto fail;
		if (!impl->mGetPosition.isFound()) goto fail;
		sImpl=impl;
		return true;
		fail:
			delete impl;
			return false;
	}
	
	AudioTrackImpl * AudioTrackImpl::sImpl=NULL;
	
	
	
}//end of namespace
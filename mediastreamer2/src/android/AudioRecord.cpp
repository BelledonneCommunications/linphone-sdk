/*
 * AudioRecord.cpp
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
#include "AudioRecord.h"

namespace fake_android{


AudioRecord::AudioRecord(audio_source_t inputSource,
                                    uint32_t sampleRate,
                                    audio_format_t format,
                                    uint32_t channelMask,
                                    int frameCount,
                                    callback_t cbf,
                                    void* user ,
                                    int notificationFrames,
                                    int sessionId, 
									transfer_type transferType,
                                    audio_input_flags_t flags){
	mThis=new uint8_t[512];
	mImpl=AudioRecordImpl::get();

	if (mImpl->mCtorBeforeAPI17.isFound()) {
		mImpl->mCtorBeforeAPI17.invoke(mThis,inputSource,sampleRate,format,channelMask,frameCount,(record_flags)0,cbf,user,notificationFrames,sessionId);
	} else {
		/* The flags parameter was removed in Android 4.2 (API level 17). */
		mImpl->mCtor.invoke(mThis,inputSource,sampleRate,format,channelMask,frameCount,cbf,user,notificationFrames,sessionId,transferType,flags);
	}
}


    /* Terminates the AudioRecord and unregisters it from AudioFlinger.
     * Also destroys all resources assotiated with the AudioRecord.
     */
AudioRecord::~AudioRecord(){
	mImpl->mDtor.invoke(mThis);
	delete mThis;
}


status_t AudioRecord::start(AudioSystem::sync_event_t event, int triggerSession){
	ms_message("starting audio record on [%p]",mThis);
	return mImpl->mStart.invoke(mThis,event,triggerSession);
}

status_t AudioRecord::stop(){
	return mImpl->mStop.invoke(mThis);
}

status_t AudioRecord::initCheck()const{
	return mImpl->mInitCheck.invoke(mThis);
}

status_t AudioRecord::getMinFrameCount(int* frameCount,
                                      uint32_t sampleRate,
                                      audio_format_t format,
                                      int channelCount)
{
	if (AudioRecordImpl::get()->mGetMinFrameCount.isFound()){
		return AudioRecordImpl::get()->mGetMinFrameCount.invoke(frameCount,sampleRate,format,channelCount);
	}else{
		//this method didn't existed in 2.2
		//Use hardcoded values instead (1024 frames at 8khz) 
		*frameCount=(1024*channelCount*sampleRate)/8000;
		return 0;
	}
}

audio_io_handle_t AudioRecord::getInput() const{
	//return mImpl->mGetInput.invoke(mThis);
	ms_error("AudioRecord::getInput() not implemented.");
	return 0;
}

int AudioRecord::getSessionId() const{
	if (mImpl->mGetSessionId.isFound()){
		return mImpl->mGetSessionId.invoke(mThis);
	}
	ms_warning("AudioRecord::getSessionId() not available");
	return -1;
}

void AudioRecord::readBuffer(const void *p_info, Buffer *buffer){
	if (AudioSystemImpl::get()->mApi18){
		*buffer=*(const Buffer*)p_info;
	}else{
		const OldBuffer *oldbuf=(const OldBuffer*)p_info;
		buffer->frameCount=oldbuf->frameCount;
		buffer->size=oldbuf->size;
		buffer->raw=oldbuf->raw;
	}
}

bool AudioRecordImpl::init(Library *lib){
	bool fail=false;
	AudioRecordImpl *impl=new AudioRecordImpl(lib);
	if (!impl->mCtorBeforeAPI17.isFound() && !impl->mCtor.isFound()) {
		fail=true;
		ms_error("AudioRecord::AudioRecord() not found.");
	}
	if (!impl->mDtor.isFound()) {
		fail=true;
		ms_error("AudioRecord::~AudioRecord() dtor not found.");
	}
	if (!impl->mInitCheck.isFound()) {
		ms_warning("AudioRecord::initCheck() not found (normal on Android 4.4)");
	}
	if (!impl->mStop.isFound()) {
		fail=true;
		ms_error("AudioRecord::stop() not found.");
	}
	if (!impl->mStart.isFound()) {
		fail=true;
		ms_error("AudioRecord::start() not found.");
	}
	if (fail){
		delete impl;
		return false;
	}
	sImpl=impl;
	return true;
}

AudioRecordImpl *AudioRecordImpl::sImpl=NULL;

AudioRecordImpl::AudioRecordImpl(Library *lib) :
	// By default, try to load Android 2.3 symbols
	mCtorBeforeAPI17(lib,"_ZN7android11AudioRecordC1EijijijPFviPvS1_ES1_ii"),
	mCtor(lib, "_ZN7android11AudioRecordC1E14audio_source_tj14audio_format_tjiPFviPvS3_ES3_iiNS0_13transfer_typeE19audio_input_flags_t"),	// 4.4 symbol
	mDtor(lib,"_ZN7android11AudioRecordD1Ev"),
	mInitCheck(lib,"_ZNK7android11AudioRecord9initCheckEv"),
	mStop(lib,"_ZN7android11AudioRecord4stopEv"),
	mStart(lib,"_ZN7android11AudioRecord5startEv"),
	mGetMinFrameCount(lib,"_ZN7android11AudioRecord16getMinFrameCountEPijii"),
	mGetSessionId(lib,"_ZNK7android11AudioRecord12getSessionIdEv")
{
	// Try some Android 2.2 symbols if not found
	if (!mCtorBeforeAPI17.isFound()) {
		mCtorBeforeAPI17.load(lib,"_ZN7android11AudioRecordC1EijijijPFviPvS1_ES1_i");
	}
	// Then try some Android 4.1 symbols if still not found
	if (!mCtorBeforeAPI17.isFound()) {
		mCtorBeforeAPI17.load(lib,"_ZN7android11AudioRecordC1E14audio_source_tj14audio_format_tjiNS0_12record_flagsEPFviPvS4_ES4_ii");
	}
	// Try to load Android 4.2 constructor
	if (!mCtorBeforeAPI17.isFound() && !mCtor.isFound()) {
		mCtor.load(lib,"_ZN7android11AudioRecordC1E14audio_source_tj14audio_format_tjiPFviPvS3_ES3_ii");
	}
	
	if (!mStart.isFound()) {
		mStart.load(lib,"_ZN7android11AudioRecord5startENS_11AudioSystem12sync_event_tEi");
	}
	if (!mGetMinFrameCount.isFound()) {
		mGetMinFrameCount.load(lib, "_ZN7android11AudioRecord16getMinFrameCountEPij14audio_format_ti");
	}

	// Then try some Android 4.2 symbols if still not found
	if (!mGetMinFrameCount.isFound()) {
		mGetMinFrameCount.load(lib, "_ZN7android11AudioRecord16getMinFrameCountEPij14audio_format_tj");
	}
}


}//end of namespace

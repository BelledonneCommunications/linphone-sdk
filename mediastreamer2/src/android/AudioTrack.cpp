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
#include "AudioSystem.h"


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
                                    int sessionId, 
									transfer_type transferType,
                                    const audio_offload_info_t *offloadInfo,
                                    int uid, pid_t pid,
									const audio_attributes_t* pAttributes){
		mThis=new uint8_t[AudioTrackImpl::sObjSize];
		memset(mThis,0,AudioTrackImpl::sObjSize);
		mImpl=AudioTrackImpl::get();
		mImpl->mCtor.invoke(mThis,streamType,sampleRate,format,channelMask,frameCount,flags,cbf,user,notificationFrames,sessionId,transferType,(void*)offloadInfo,uid,pid,pAttributes);
		//dumpMemory(mThis,AudioTrackImpl::sObjSize);
	}
	
	AudioTrack::AudioTrack(){
		mThis=new uint8_t[AudioTrackImpl::sObjSize];
		memset(mThis,0,AudioTrackImpl::sObjSize);
		mImpl=AudioTrackImpl::get();
		mImpl->mDefaultCtor.invoke(mThis);
	}
	
	AudioTrack::~AudioTrack(){
	}
	
	void AudioTrack::start(){
		mImpl->mStart.invoke(mThis);
	}
	
	void AudioTrack::stop(){
		mImpl->mStop.invoke(mThis);
	}
	
	status_t AudioTrack::initCheck()const{
		if (mImpl->mInitCheck.isFound()) return mImpl->mInitCheck.invoke(mThis);
		ms_warning("AudioTrack::initCheck() not available assuming OK");
		return 0;
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
		// Initialize frameCount to a magic value
		*frameCount = 54321;
		if (AudioTrackImpl::get()->mGetMinFrameCount.isFound()){
			status_t ret = AudioTrackImpl::get()->mGetMinFrameCount.invoke(frameCount,streamType,sampleRate);
			if ((ret == 0) && (*frameCount == 54321)) {
				// If no error and the magic value has not been erased then the getMinFrameCount implementation
				// is a dummy one. So perform the calculation manually as it is supposed to be implemented...
				int afSampleRate;
				if (AudioSystem::getOutputSamplingRate(&afSampleRate, streamType) != 0) return -1;
				int afFrameCount;
				if (AudioSystem::getOutputFrameCount(&afFrameCount, streamType) != 0) return -1;
				uint32_t afLatency;
				if (AudioSystem::getOutputLatency(&afLatency, streamType) != 0) return -1;

				// Ensure that buffer depth covers at least audio hardware latency
				uint32_t minBufCount = afLatency / ((1000 * afFrameCount) / afSampleRate);
				if (minBufCount < 2) minBufCount = 2;

				*frameCount = (sampleRate == 0) ? afFrameCount * minBufCount :
					afFrameCount * minBufCount * sampleRate / afSampleRate;
			}
			return ret;
		}else{
			//this method didn't existed in 2.2
			//Use hardcoded values instead (1024 frames at 8khz) 
			*frameCount=(1024*sampleRate)/8000;
			return 0;
		}
	}
	
	uint32_t AudioTrack::latency()const{
		if (mImpl->mLatency.isFound())
			return mImpl->mLatency.invoke(mThis);
		else return (uint32_t)-1;
	}
	
	status_t AudioTrack::getPosition(uint32_t *frames)const{
		return mImpl->mGetPosition.invoke(mThis,frames);
	}
	
	void AudioTrack::readBuffer(const void *p_info, Buffer *buffer){
		if (AudioSystemImpl::get()->mApi18){
			*buffer=*(const Buffer*)p_info;
		}else{
			const OldBuffer *oldbuf=(const OldBuffer*)p_info;
			buffer->frameCount=oldbuf->frameCount;
			buffer->size=oldbuf->size;
			buffer->raw=oldbuf->raw;
		}
	}
	
	void AudioTrack::writeBuffer(void *p_info, const Buffer *buffer){
		if (AudioSystemImpl::get()->mApi18){
			*(Buffer*)p_info=*buffer;
		}else{
			OldBuffer *oldbuf=(OldBuffer*)p_info;
			oldbuf->frameCount=buffer->frameCount;
			oldbuf->raw=buffer->raw;
			oldbuf->size=buffer->size;
		}
	}

	void *AudioTrack::getRealThis()const{
		return mThis + mImpl->mRefBaseOffset;
	}
	
	bool AudioTrack::isRefCounted()const{
		return mImpl->mUseRefCount;
	}
	
	void AudioTrack::destroy()const{
		mImpl->mDtor.invoke(mThis);
		delete []mThis;
	}
	
	
	AudioTrackImpl::AudioTrackImpl(Library *lib) :
		// By default, try to load Android 2.3 symbols
		mCtor(lib,"_ZN7android10AudioTrackC1EijiiijPFviPvS1_ES1_ii"),
		mDtor(lib,"_ZN7android10AudioTrackD1Ev"),
		mDefaultCtor(lib,"_ZN7android10AudioTrackC1Ev"),
		mInitCheck(lib,"_ZNK7android10AudioTrack9initCheckEv"),
		mStop(lib,"_ZN7android10AudioTrack4stopEv"),
		mStart(lib,"_ZN7android10AudioTrack5startEv"),
		mStopped(lib,"_ZNK7android10AudioTrack7stoppedEv"),
		mFlush(lib,"_ZN7android10AudioTrack5flushEv"),
		mGetMinFrameCount(lib,"_ZN7android10AudioTrack16getMinFrameCountEPiij"),
		mLatency(lib,"_ZNK7android10AudioTrack7latencyEv"),
		mGetPosition(lib,"_ZNK7android10AudioTrack11getPositionEPj") //4.4 symbol
	{
		mRefBaseOffset=0;
		mSdkVersion=0;
		mUseRefCount=false;
		// Try some Android 2.2 symbols if not found
		if (!mCtor.isFound()) {
			mCtor.load(lib,"_ZN7android10AudioTrackC1EijiiijPFviPvS1_ES1_i");
			if (!mCtor.isFound())
				mCtor.load(lib,"_ZN7android10AudioTrackC1E19audio_stream_type_tj14audio_format_tji20audio_output_flags_tPFviPvS4_ES4_ii");
			//4.4 symbol
			if (!mCtor.isFound()){
				mCtor.load(lib,"_ZN7android10AudioTrackC1E19audio_stream_type_tj14audio_format_t"
				"ji20audio_output_flags_tPFviPvS4_ES4_iiNS0_13transfer_typeEPK20audio_offload_info_ti");
				if (mCtor.isFound()){
					mSdkVersion=19;
				}
			}
			//5.0 symbol
			if (!mCtor.isFound()){
				mCtor.load(lib,"_ZN7android10AudioTrackC1E19audio_stream_type_tj14audio_format_tjj20"
				"audio_output_flags_tPFviPvS4_ES4_jiNS0_13transfer_typeEPK20audio_offload_info_tiiPK18audio_attributes_t"
				);
				if (mCtor.isFound()){
					mSdkVersion=21;
				}
			}
		}

		// Then try some Android 4.1 symbols if still not found
		if (!mGetMinFrameCount.isFound()) {
			mGetMinFrameCount.load(lib,"_ZN7android10AudioTrack16getMinFrameCountEPi19audio_stream_type_tj");
		}
		if (!mGetPosition.isFound()){
			mGetPosition.load(lib,"_ZN7android10AudioTrack11getPositionEPj"); //until 4.3 included
		}
	}
	
	bool AudioTrackImpl::init(Library *lib){
		bool fail=false;
		
		if (sImpl==NULL){
			AudioTrackImpl *impl=new AudioTrackImpl(lib);
			
			if (!impl->mCtor.isFound()) {
				ms_error("AudioTrack::AudioTrack(...) not found");
				fail=true;
			}
			if (!impl->mDtor.isFound()) {
				ms_error("AudioTrack::~AudioTrack() not found");
				fail=true;
			}
			if (!impl->mStart.isFound()) {
				ms_error("AudioTrack::start() not found");
				fail=true;
			}
			if (!impl->mStop.isFound()) {
				ms_error("AudioTrack::stop() not found");
				fail=true;
			}
			if (!impl->mInitCheck.isFound()) {
				ms_warning("AudioTrack::initCheck() not found (normal in android 4.3)");
			}
			if (!impl->mFlush.isFound()) {
				ms_error("AudioTrack::flush() not found");
				fail=true;
			}
			if (!impl->mLatency.isFound()) {
				ms_warning("AudioTrack::latency() not found (normal in android 4.3)");
			}
			if (!impl->mGetPosition.isFound()) {
				ms_error("AudioTrack::getPosition() not found");
				fail=true;
			}
			if (impl->mSdkVersion>=19 && !impl->mDefaultCtor.isFound()) {
				ms_error("AudioTrack::AudioTrack() not found");
				fail=true;
			}
			if (!fail){
				sImpl=impl;
				if (impl->mSdkVersion>=19){
					impl->mUseRefCount=true;
					
					AudioTrack *test=new AudioTrack();
					//dumpMemory(test->getRealThis(),AudioTrackImpl::sObjSize);
					ptrdiff_t offset=findRefbaseOffset(test->getRealThis(),AudioTrackImpl::sObjSize);
					
					if (offset>(ptrdiff_t)sizeof(void*)){
						ms_message("AudioTrack uses virtual RefBase despite it is 4.4");
						impl->mRefBaseOffset=offset;
					}else{
						ms_message("AudioTrack needs refcounting.");
					}
					sp<AudioTrack> st(test);
				}
				return true;
			}else{
				delete impl;
				return false;
			}
		}
		return true;
	}
	
	AudioTrackImpl * AudioTrackImpl::sImpl=NULL;
}//end of namespace

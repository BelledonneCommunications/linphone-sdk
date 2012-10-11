

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
		
		/*HACK for gingerbread */
		if ((channelMask & AUDIO_CHANNEL_OUT_MONO) == AUDIO_CHANNEL_OUT_MONO){
			channelMask=0x4;
		}else if ((channelMask & AUDIO_CHANNEL_OUT_STEREO) == AUDIO_CHANNEL_OUT_STEREO){
			channelMask=0x4|0x8;
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
		mCtor(lib,"_ZN7android10AudioTrackC1EijiiijPFviPvS1_ES1_ii"),
		mDtor(lib,"_ZN7android10AudioTrackD1Ev"),
		mInitCheck(lib,"_ZNK7android10AudioTrack9initCheckEv"),
		mStop(lib,"_ZN7android10AudioTrack4stopEv"),
		mStart(lib,"_ZN7android10AudioTrack5startEv"),
		mStopped(lib,"_ZNK7android10AudioTrack7stoppedEv"),
		mFlush(lib,"_ZN7android10AudioTrack5flushEv"),
		mGetMinFrameCount(lib,"_ZN7android10AudioTrack16getMinFrameCountEPiij"),
		mLatency(lib,"_ZNK7android10AudioTrack7latencyEv"),
		mGetPosition(lib,"_ZN7android10AudioTrack11getPositionEPj")
		{
	
	}
	
	bool AudioTrackImpl::init(Library *lib){
		AudioTrackImpl *impl=new AudioTrackImpl(lib);
		if (!impl->mCtor.isFound()) goto fail;
		if (!impl->mDtor.isFound()) goto fail;
		if (!impl->mStart.isFound()) goto fail;
		if (!impl->mStop.isFound()) goto fail;
		if (!impl->mInitCheck.isFound()) goto fail;
		if (!impl->mFlush.isFound()) goto fail;
		if (!impl->mGetMinFrameCount.isFound()) goto fail;
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

#include "mediastreamer2/mscommon.h"
#include "AudioRecord.h"

namespace fake_android{


AudioRecord::AudioRecord(audio_source_t inputSource,
                                    uint32_t sampleRate,
                                    audio_format_t format,
                                    uint32_t channelMask,
                                    int frameCount,
                                    record_flags flags,
                                    callback_t cbf,
                                    void* user ,
                                    int notificationFrames,
                                    int sessionId){
	mThis=new uint8_t[512];
	mImpl=AudioRecordImpl::get();
	
	/*HACK for gingerbread */
	/*
	if ((channelMask & AUDIO_CHANNEL_IN_MONO) == AUDIO_CHANNEL_IN_MONO){
		channelMask=0x4;
	}else if ((channelMask & AUDIO_CHANNEL_IN_STEREO) == AUDIO_CHANNEL_IN_STEREO){
		channelMask=0x4|0x8;
	}
	*/
	
	mImpl->mCtor.invoke(mThis,inputSource,sampleRate,format,channelMask,frameCount,flags,cbf,user,notificationFrames,sessionId);
}


    /* Terminates the AudioRecord and unregisters it from AudioFlinger.
     * Also destroys all resources assotiated with the AudioRecord.
     */
AudioRecord::~AudioRecord(){
	mImpl->mDtor.invoke(mThis);
	delete mThis;
}


status_t AudioRecord::start(AudioSystem::sync_event_t event, int triggerSession){
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
	return AudioRecordImpl::get()->mGetMinFrameCount.invoke(frameCount,sampleRate,format,channelCount);
}

audio_io_handle_t AudioRecord::getInput() const{
	//return mImpl->mGetInput.invoke(mThis);
	ms_error("AudioRecord::getInput() not implemented.");
	return 0;
}

bool AudioRecordImpl::init(Library *lib){
	AudioRecordImpl *impl=new AudioRecordImpl(lib);
	if (!impl->mCtor.isFound()) goto fail;
	if (!impl->mDtor.isFound()) goto fail;
	if (!impl->mInitCheck.isFound()) goto fail;
	if (!impl->mStop.isFound()) goto fail;
	if (!impl->mStart.isFound()) goto fail;
	if (!impl->mGetMinFrameCount.isFound()) goto fail;
	//if (!impl->mGetInput.isFound()) goto fail;
	sImpl=impl;
	return true;
	
fail:
	delete impl;
	return false;
}

AudioRecordImpl *AudioRecordImpl::sImpl=NULL;

AudioRecordImpl::AudioRecordImpl(Library *lib) :
	mCtor(lib,"_ZN7android11AudioRecordC1EijijijPFviPvS1_ES1_ii"),
	mDtor(lib,"_ZN7android11AudioRecordD1Ev"),
	mInitCheck(lib,"_ZNK7android11AudioRecord9initCheckEv"),
	mStop(lib,"_ZN7android11AudioRecord4stopEv"),
	mStart(lib,"_ZN7android11AudioRecord5startEv"),
	mGetMinFrameCount(lib,"_ZN7android11AudioRecord16getMinFrameCountEPijii")
	//mGetInput(lib,"_ZN7android11AudioRecord8getInputEv")
{
}


}//end of namespace



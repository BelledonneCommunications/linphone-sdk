/*
 * AudioSystem.cpp
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

#include "AudioSystem.h"
#include "mediastreamer2/mscommon.h"

namespace fake_android{

status_t AudioSystem::getOutputSamplingRate(int *rate, audio_stream_type_t streamType){
	return AudioSystemImpl::get()->mGetOutputSamplingRate.invoke(rate,streamType);
}

status_t AudioSystem::getOutputSamplingRate(int *rate, int streamType){
	return AudioSystemImpl::get()->mGetOutputSamplingRate.invoke(rate,streamType);
}

status_t AudioSystem::getOutputFrameCount(int *frameCount, audio_stream_type_t streamType) {
	return AudioSystemImpl::get()->mGetOutputFrameCount.invoke(frameCount, streamType);
}

status_t AudioSystem::getOutputFrameCount(int *frameCount, int streamType) {
	return AudioSystemImpl::get()->mGetOutputFrameCount.invoke(frameCount, streamType);
}

status_t AudioSystem::getOutputLatency(uint32_t *latency, audio_stream_type_t streamType) {
	return AudioSystemImpl::get()->mGetOutputLatency.invoke(latency, streamType);
}

status_t AudioSystem::getOutputLatency(uint32_t *latency, int streamType) {
	return AudioSystemImpl::get()->mGetOutputLatency.invoke(latency, streamType);
}

status_t AudioSystem::setParameters(audio_io_handle_t ioHandle, const String8& keyValuePairs){
	return AudioSystemImpl::get()->mSetParameters.invoke(ioHandle,keyValuePairs);
}

status_t AudioSystem::setPhoneState(audio_mode_t state) {
	return AudioSystemImpl::get()->mSetPhoneState.invoke(state);
}

status_t AudioSystem::setForceUse(audio_policy_force_use_t usage, audio_policy_forced_cfg_t config) {
	return AudioSystemImpl::get()->mSetForceUse.invoke(usage, config);
}

int AudioSystem::newAudioSessionId(){
	if (AudioSystemImpl::get()->mNewAudioSessionId.isFound())
		return AudioSystemImpl::get()->mNewAudioSessionId.invoke();
	else{
		ms_warning("AudioSystem::newAudioSessionId() not found.");
		return -1;
	}
}

audio_io_handle_t AudioSystem::getInput(audio_source_t inputSource,
                                    uint32_t samplingRate,
                                    audio_format_t format,
                                    uint32_t channels,
                                    audio_in_acoustics_t acoustics,
                                    int sessionId){
	ms_error("AudioSystem::getInput() not implemented.");
	return 0;
	//return AudioSystemImpl::get()->mGetInput.invoke(inputSource,samplingRate,format,channels,acoustics,sessionId);
}


AudioSystemImpl::AudioSystemImpl(Library *lib) :
	// By default, try to load Android 2.3 symbols
	mGetOutputSamplingRate(lib,"_ZN7android11AudioSystem21getOutputSamplingRateEPii"),
	mGetOutputFrameCount(lib, "_ZN7android11AudioSystem19getOutputFrameCountEPii"),
	mGetOutputLatency(lib, "_ZN7android11AudioSystem16getOutputLatencyEPji"),
	mSetParameters(lib,"_ZN7android11AudioSystem13setParametersEiRKNS_7String8E"),
	mSetPhoneState(lib, "_ZN7android11AudioSystem13setPhoneStateEi"),
	mSetForceUse(lib, "_ZN7android11AudioSystem11setForceUseENS0_9force_useENS0_13forced_configE"),
	mNewAudioSessionId(lib,"_ZN7android11AudioSystem17newAudioSessionIdEv"){
	//mGetInput(lib,"_ZN7android11AudioSystem8getInputEijjjNS0_18audio_in_acousticsE"){
	mApi18=false;
	// Try some Android 4.0 symbols if not found
	if (!mSetForceUse.isFound()) {
		mSetForceUse.load(lib, "_ZN7android11AudioSystem11setForceUseE24audio_policy_force_use_t25audio_policy_forced_cfg_t");
	}

	// Then try some Android 4.1 symbols if still not found
	if (!mGetOutputSamplingRate.isFound()) {
		mGetOutputSamplingRate.load(lib, "_ZN7android11AudioSystem21getOutputSamplingRateEPi19audio_stream_type_t");
		if (!mGetOutputSamplingRate.isFound()){
			mGetOutputSamplingRate.load(lib,"_ZN7android11AudioSystem21getOutputSamplingRateEPj19audio_stream_type_t");
			mApi18=true;
		}
	}
	if (!mGetOutputFrameCount.isFound()) {
		mGetOutputFrameCount.load(lib, "_ZN7android11AudioSystem19getOutputFrameCountEPi19audio_stream_type_t");
		if (!mGetOutputFrameCount.isFound())
			mGetOutputFrameCount.load(lib,"_ZN7android11AudioSystem19getOutputFrameCountEPj19audio_stream_type_t");
	}
	if (!mGetOutputLatency.isFound()) {
		mGetOutputLatency.load(lib, "_ZN7android11AudioSystem16getOutputLatencyEPj19audio_stream_type_t");
	}
	if (!mSetPhoneState.isFound()) {
		mSetPhoneState.load(lib, "_ZN7android11AudioSystem13setPhoneStateE12audio_mode_t");
	}
	if (!mNewAudioSessionId.isFound()){
		//android 5.0 symbol
		mNewAudioSessionId.load(lib, "_ZN7android11AudioSystem16newAudioUniqueIdEv");
	}
}

bool AudioSystemImpl::init(Library *lib){
	bool fail=false;
	AudioSystemImpl *impl=new AudioSystemImpl(lib);

	if (!impl->mGetOutputSamplingRate.isFound()){
		ms_error("AudioSystem::getOutputSamplingRate() not found.");
		fail=true;
	}
	if (!impl->mGetOutputFrameCount.isFound()) {
		ms_error("AudioSystem::getOutputFrameCount() not found.");
		fail=true;
	}
	if (!impl->mGetOutputLatency.isFound()){
		ms_error("AudioSystem::getOutputLatency() not found.");
		fail=true;
	}
	if (!impl->mSetParameters.isFound()) {
		ms_error("AudioSystem::setParameters() not found.");
		fail=true;
	}
	if (!impl->mSetPhoneState.isFound()){
		ms_error("AudioSystem::setPhoneState() not found.");
		fail=true;
	}
	if (!impl->mSetForceUse.isFound()) {
		ms_error("AudioSystem::setForceUse() not found.");
		fail=true;
	}
	//if (!impl->mGetInput.isFound()) goto fail;

	if (!fail){
		sImpl=impl;
		return true;
	}else{
		delete impl;
		return false;
	}
}

AudioSystemImpl *AudioSystemImpl::sImpl=NULL;

bool RefBaseImpl::init(Library *lib){
	RefBaseImpl *impl=new RefBaseImpl(lib);
	bool fail=false;
	
	if (!impl->mIncStrong.isFound()){
		ms_error("RefBase::incStrong() not found");
		fail=true;
	}
	if (!impl->mDecStrong.isFound()){
		ms_error("RefBase::decStrong() not found");
		fail=true;
	}
	if (fail){
		delete impl;
		return false;
	}else{
		sImpl=impl;
		return true;
	}
}

RefBaseImpl * RefBaseImpl::sImpl=0;


RefBaseImpl::RefBaseImpl(Library *lib) :
	mCtor(lib,"_ZN7android7RefBaseC2Ev"),
	mIncStrong(lib,"_ZNK7android7RefBase9incStrongEPKv"),
	mDecStrong(lib,"_ZNK7android7RefBase9decStrongEPKv"),
	mGetStrongCount(lib,"_ZNK7android7RefBase14getStrongCountEv")
{
	
}

RefBase::RefBase(){
	mImpl=RefBaseImpl::get();
	mCnt=0;
}

RefBase::~RefBase(){
}

void RefBase::incStrong(const void* id) const{
	mCnt++;
	if (isRefCounted()) {
		ms_message("incStrong(%p)",getRealThis());
		mImpl->mIncStrong.invoke(getRealThis(),this);
	}
}

void RefBase::decStrong(const void* id) const{
	if (isRefCounted()) {
		ms_message("decStrong(%p)",getRealThis());
		mImpl->mDecStrong.invoke(getRealThis(),this);
	}
	mCnt--;
	if (mCnt==0){
		if (!isRefCounted()){
			destroy();
		}
		delete this;
	}
}

int32_t RefBase::getStrongCount() const{
	return mImpl->mGetStrongCount.invoke(getRealThis());
}

ptrdiff_t findRefbaseOffset(void *obj, size_t size){
	uint8_t *base_vptr=(uint8_t*)*(void **)obj;
	const long vptrMemRange=0x1000000;
	size_t i;
	int ret=-1;
	
	if (base_vptr==NULL){
		ms_warning("findRefbaseOffset(): no base vptr");
	}
	ms_message("base_vptr is %p for obj %p",base_vptr, obj);
	for (i=((size/sizeof(void*))-1)*sizeof(void*);i>0;i-=sizeof(void*)){
		uint8_t *ptr= ((uint8_t*)obj) + i;
		uint8_t *candidate=(uint8_t*)*(void**)ptr;
		if (i!=0 && labs((ptrdiff_t)(candidate-base_vptr))<vptrMemRange){
			ret=i;
			break;
		}
	}
	if (ret==-1) ms_message("findRefbaseOffset(): no refbase vptr found");
	return ret;
}

void dumpMemory(void *obj, size_t size){
	size_t i;
	ms_message("Dumping memory at %p",obj);
	for (i=0;i<size;i+=sizeof(long)){
		ms_message("%4i\t%lx",(int)i,*(long*)(((uint8_t*)obj)+i));
	}
}

}


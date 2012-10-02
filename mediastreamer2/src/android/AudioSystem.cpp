
#include "AudioSystem.h"


namespace fake_android{

status_t AudioSystem::getOutputSamplingRate(int *rate, audio_stream_type_t streamType){
	return AudioSystemImpl::get()->mGetOutputSamplingRate.invoke(rate,streamType);
}

status_t AudioSystem::getOutputSamplingRate(int *rate, int streamType){
	return AudioSystemImpl::get()->mGetOutputSamplingRate.invoke(rate,streamType);
}


AudioSystemImpl::AudioSystemImpl(Library *lib) :
	mGetOutputSamplingRate(lib,"_ZN7android11AudioSystem21getOutputSamplingRateEPii"){
}

bool AudioSystemImpl::init(Library *lib){
	AudioSystemImpl *impl=new AudioSystemImpl(lib);
	if (!impl->mGetOutputSamplingRate.isFound()) goto fail;
	sImpl=impl;
	return true;
	
	fail:
		delete impl;
		return false;
}

AudioSystemImpl *AudioSystemImpl::sImpl=NULL;

}


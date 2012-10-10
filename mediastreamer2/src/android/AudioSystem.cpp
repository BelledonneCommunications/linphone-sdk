
#include "AudioSystem.h"


namespace fake_android{

status_t AudioSystem::getOutputSamplingRate(int *rate, audio_stream_type_t streamType){
	return AudioSystemImpl::get()->mGetOutputSamplingRate.invoke(rate,streamType);
}

status_t AudioSystem::getOutputSamplingRate(int *rate, int streamType){
	return AudioSystemImpl::get()->mGetOutputSamplingRate.invoke(rate,streamType);
}

status_t AudioSystem::setParameters(audio_io_handle_t ioHandle, const String8& keyValuePairs){
	return AudioSystemImpl::get()->mSetParameters.invoke(ioHandle,keyValuePairs);
}

audio_io_handle_t AudioSystem::getInput(audio_source_t inputSource,
                                    uint32_t samplingRate,
                                    audio_format_t format,
                                    uint32_t channels,
                                    audio_in_acoustics_t acoustics,
                                    int sessionId){
	return AudioSystemImpl::get()->mGetInput.invoke(inputSource,samplingRate,format,channels,acoustics,sessionId);
}


AudioSystemImpl::AudioSystemImpl(Library *lib) :
	mGetOutputSamplingRate(lib,"_ZN7android11AudioSystem21getOutputSamplingRateEPii"),
	mSetParameters(lib,"_ZN7android11AudioSystem13setParametersEiRKNS_7String8E"),
	mGetInput(lib,"_ZN7android11AudioSystem8getInputEijjjNS0_18audio_in_acousticsE"){
}

bool AudioSystemImpl::init(Library *lib){
	AudioSystemImpl *impl=new AudioSystemImpl(lib);
	if (!impl->mGetOutputSamplingRate.isFound()) goto fail;
	if (!impl->mSetParameters.isFound()) goto fail;
	if (!impl->mGetInput.isFound()) goto fail;
	sImpl=impl;
	return true;
	
	fail:
		delete impl;
		return false;
}

AudioSystemImpl *AudioSystemImpl::sImpl=NULL;

}


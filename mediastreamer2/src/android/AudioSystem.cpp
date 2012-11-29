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
	mSetParameters(lib,"_ZN7android11AudioSystem13setParametersEiRKNS_7String8E"){
	//mGetInput(lib,"_ZN7android11AudioSystem8getInputEijjjNS0_18audio_in_acousticsE"){

	// Try some Android 4.1 symbols if not found
	if (!mGetOutputSamplingRate.isFound()) {
		mGetOutputSamplingRate.load(lib, "_ZN7android11AudioSystem21getOutputSamplingRateEPi19audio_stream_type_t");
	}
	if (!mGetOutputFrameCount.isFound()) {
		mGetOutputFrameCount.load(lib, "_ZN7android11AudioSystem19getOutputFrameCountEPi19audio_stream_type_t");
	}
	if (!mGetOutputLatency.isFound()) {
		mGetOutputLatency.load(lib, "_ZN7android11AudioSystem16getOutputLatencyEPj19audio_stream_type_t");
	}
}

bool AudioSystemImpl::init(Library *lib){
	AudioSystemImpl *impl=new AudioSystemImpl(lib);
	int samplingRate;
	int frameCount;
	uint32_t latency;
	status_t err;
	bool buggyAndroid = false;

	if (!impl->mGetOutputSamplingRate.isFound()) goto fail;
	if (!impl->mGetOutputFrameCount.isFound()) goto fail;
	if (!impl->mGetOutputLatency.isFound()) goto fail;
	if (!impl->mSetParameters.isFound()) goto fail;
	//if (!impl->mGetInput.isFound()) goto fail;

	err = impl->mGetOutputSamplingRate.invoke(&samplingRate, AUDIO_STREAM_VOICE_CALL);
	if (err == 0) err = impl->mGetOutputFrameCount.invoke(&frameCount, AUDIO_STREAM_VOICE_CALL);
	if (err == 0) err = impl->mGetOutputLatency.invoke(&latency, AUDIO_STREAM_VOICE_CALL);
	if (err == 0) {
		ms_message("AUDIO_STREAM_VOICE_CALL characteristics: SamplingRate=%d, FrameCount=%d, Latency=%u", samplingRate, frameCount, latency);
	} else {
		ms_error("Unable to get AUDIO_STREAM_VOICE_CALL characteristics");
	}
	if ((frameCount > 8192) || (latency > 200)) buggyAndroid = true;
	err = impl->mGetOutputSamplingRate.invoke(&samplingRate, AUDIO_STREAM_DEFAULT);
	if (err == 0) err = impl->mGetOutputFrameCount.invoke(&frameCount, AUDIO_STREAM_DEFAULT);
	if (err == 0) err = impl->mGetOutputLatency.invoke(&latency, AUDIO_STREAM_DEFAULT);
	if (err == 0) {
		ms_message("AUDIO_STREAM_DEFAULT characteristics: SamplingRate=%d, FrameCount=%d, Latency=%u", samplingRate, frameCount, latency);
	} else {
		ms_error("Unable to get AUDIO_STREAM_DEFAULT characteristics");
	}
	if (buggyAndroid) goto fail;

	sImpl=impl;
	return true;

	fail:
		delete impl;
		return false;
}

AudioSystemImpl *AudioSystemImpl::sImpl=NULL;

}


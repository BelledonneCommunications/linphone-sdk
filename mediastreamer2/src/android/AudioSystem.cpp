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
	mSetForceUse(lib, "_ZN7android11AudioSystem11setForceUseENS0_9force_useENS0_13forced_configE") {
	//mGetInput(lib,"_ZN7android11AudioSystem8getInputEijjjNS0_18audio_in_acousticsE"){

	// Try some Android 4.0 symbols if not found
	if (!mSetForceUse.isFound()) {
		mSetForceUse.load(lib, "_ZN7android11AudioSystem11setForceUseE24audio_policy_force_use_t25audio_policy_forced_cfg_t");
	}

	// Then try some Android 4.1 symbols if still not found
	if (!mGetOutputSamplingRate.isFound()) {
		mGetOutputSamplingRate.load(lib, "_ZN7android11AudioSystem21getOutputSamplingRateEPi19audio_stream_type_t");
	}
	if (!mGetOutputFrameCount.isFound()) {
		mGetOutputFrameCount.load(lib, "_ZN7android11AudioSystem19getOutputFrameCountEPi19audio_stream_type_t");
	}
	if (!mGetOutputLatency.isFound()) {
		mGetOutputLatency.load(lib, "_ZN7android11AudioSystem16getOutputLatencyEPj19audio_stream_type_t");
	}
	if (!mSetPhoneState.isFound()) {
		mSetPhoneState.load(lib, "_ZN7android11AudioSystem13setPhoneStateE12audio_mode_t");
	}
}

bool AudioSystemImpl::init(Library *lib){
	AudioSystemImpl *impl=new AudioSystemImpl(lib);

	if (!impl->mGetOutputSamplingRate.isFound()) goto fail;
	if (!impl->mGetOutputFrameCount.isFound()) goto fail;
	if (!impl->mGetOutputLatency.isFound()) goto fail;
	if (!impl->mSetParameters.isFound()) goto fail;
	if (!impl->mSetPhoneState.isFound()) goto fail;
	if (!impl->mSetForceUse.isFound()) goto fail;
	//if (!impl->mGetInput.isFound()) goto fail;

	sImpl=impl;
	return true;

	fail:
		delete impl;
		return false;
}

AudioSystemImpl *AudioSystemImpl::sImpl=NULL;

}


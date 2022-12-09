/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2 
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
package org.linphone.mediastream;

public class Factory {
	public static final int DEVICE_HAS_BUILTIN_AEC  = 1; // Says the device has a builtin AEC because the API that would tell us that isn't available
	public static final int DEVICE_HAS_BUILTIN_AEC_CRAPPY = 2; // The device has the API to tell us it has a builtin AEC but we shouldn't trust it (so we'll use software AEC)
	public static final int DEVICE_USE_ANDROID_MIC = 4; // The device needs to capture using MIC instead of Voice communication (I.E kindle fire)
	public static final int DEVICE_HAS_BUILTIN_OPENSLES_AEC = 8; // The device has a builtin AEC and it is working with OpenSLES (which is uncommon)
	public static final int DEVICE_HAS_CRAPPY_ANDROID_FASTTRACK = 16; // AUDIO_OUTPUT_FLAG_FAST flag of android AudioTrack doesn't work
	public static final int DEVICE_HAS_CRAPPY_ANDROID_FASTRECORD = 32; // AUDIO_INPUT_FLAG_FAST flag of android AudioRecord doesn't work
	public static final int DEVICE_HAS_UNSTANDARD_LIBMEDIA = 64; // libmedia backend shall not be used because of proprietary modifications made into it by the manufacturer
	public static final int DEVICE_HAS_CRAPPY_OPENGL = 128; // opengl is crappy and our opengl surfaceview will crash
	public static final int DEVICE_HAS_CRAPPY_OPENSLES = 256; // opensles latency is crappy
	public static final int DEVICE_USE_ANDROID_CAMCORDER = 512; // the device needs to capture using CAMCORDER instead of Voice communication (I.E kindle fire)
	public static final int DEVICE_MCH264ENC_NO_PIX_FMT_CONV = 1024; // avoid pixel format convervion before MediaCodec H264 encoder input
	public static final int DEVICE_MCH265_LIMIT_DEQUEUE_OF_OUTPUT_BUFFERS = 2048; // avoid calling dequeueOutputBuffers() too often.
	public static final int DEVICE_HAS_CRAPPY_AAUDIO = 4096; // set to prevent aaudio plugin to create a soundcard

	private long mNativePtr;
	private Factory(long nativePtr){
		mNativePtr = nativePtr;
	}
	
	private native void enableFilterFromName(long nativePtr, String name, boolean enabled);
	public void enableFilterFromName(String name, boolean enabled){
		enableFilterFromName(mNativePtr, name, enabled);
	}

	private native boolean filterFromNameEnabled(long nativePtr, String name);
	public boolean filterFromNameEnabled(String name){
		return filterFromNameEnabled(mNativePtr, name);
	}

	private native void setDeviceInfo(long nativePtr, String manufacturer, String model, String platform, int flags, int delay, int recommended_rate);
	public void setDeviceInfo(String manufacturer, String model, String platform, int flags, int delay, int recommended_rate) {
		setDeviceInfo(mNativePtr, manufacturer, model, platform, flags, delay, recommended_rate);
	}

	private native int getDeviceFlags(long nativePtr);
	public int getDeviceFlags() {
		return getDeviceFlags(mNativePtr);
	}

	private native String getEncoderText(long nativePtr, String mime);
	public String getEncoderText(String mime){
		return getEncoderText(mNativePtr, mime);
	}

	private native String getDecoderText(long nativePtr, String mime);
	public String getDecoderText(String mime){
		return getDecoderText(mNativePtr, mime);
	}
};

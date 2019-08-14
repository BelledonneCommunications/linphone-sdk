/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
package org.linphone.mediastream;

public class Factory{
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

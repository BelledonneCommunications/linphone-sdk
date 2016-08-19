/*
AndroidContext.java
Copyright (C) 2014  Belledonne Communications, Grenoble, France

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
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
};

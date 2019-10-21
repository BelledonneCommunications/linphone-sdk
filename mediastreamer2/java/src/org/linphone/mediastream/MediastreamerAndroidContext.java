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

import android.annotation.TargetApi;
import android.content.Context;
import android.content.pm.PackageManager;
import java.io.File;
import android.media.AudioManager;
import android.os.Build;

public class MediastreamerAndroidContext {
	private static final int DEVICE_CHOICE = 0; // The device has the API to tell us it as or not a builtin AEC and we can trust it
	public static final int DEVICE_HAS_BUILTIN_AEC  = 1; // Says the device has a builtin AEC because the API that would tell us that isn't available
	public static final int DEVICE_HAS_BUILTIN_AEC_CRAPPY = 2; // The device has the API to tell us it has a builtin AEC but we shouldn't trust it (so we'll use software AEC)
	public static final int DEVICE_USE_ANDROID_MIC = 4;
	public static final int DEVICE_HAS_BUILTIN_OPENSLES_AEC = 8; // The device has a builtin AEC and it is working with OpenSLES (which is uncommon)
	public static final int DEVICE_USE_ANDROID_CAMCORDER = 512;

	private static Context mContext;
	private static int mDeviceFavoriteSampleRate = 44100;
	private static int mDeviceFavoriteBufferSize = 256;

	private MediastreamerAndroidContext() {
	}

	private static MediastreamerAndroidContext instance;

	private static MediastreamerAndroidContext getInstance() {
		if (instance == null)
			instance = new MediastreamerAndroidContext();
		return instance;
	}

	public static Context getContext() {
		return mContext;
	}

	public static int getDeviceFavoriteSampleRate() {
		return mDeviceFavoriteSampleRate;
	}

	public static int getDeviceFavoriteBufferSize() {
		return mDeviceFavoriteBufferSize;
	}

	public static String getNativeLibrariesDirectory() {
		String nativeLibDir = getContext().getApplicationInfo().nativeLibraryDir;

		File directory = new File(nativeLibDir);
		File[] nativeLibs = directory.listFiles();

		if (nativeLibs == null || nativeLibs.length == 0) {
			// This scenario is for app bundle mode packaging, expected path to be like
			// /data/app/org.linphone.debug-2/split_config.armeabi_v7a.apk!/lib/armeabi-v7a
			Log.w("Native library directory is empty, using path to native libs for app bundle mode");
			nativeLibDir = nativeLibDir.substring(0, nativeLibDir.indexOf("/lib"));
			nativeLibDir += "/split_config." + Build.SUPPORTED_ABIS[0].replace("-", "_") + ".apk!/lib/" + Build.SUPPORTED_ABIS[0];
		}

		return nativeLibDir;
	}

	@TargetApi(Build.VERSION_CODES.KITKAT)
	public static void setContext(Object c) {
		if (c == null)
			return;

		mContext = (Context)c;

		boolean hasLowLatencyFeature = mContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_AUDIO_LOW_LATENCY);
		boolean hasProFeature = mContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_AUDIO_PRO);
		Log.i("[Device] hasLowLatencyFeature: " + hasLowLatencyFeature + ", hasProFeature: " + hasProFeature);

		int bufferSize = 256;
		int sampleRate = 44100;
		MediastreamerAndroidContext mac = getInstance();
		// When using the OpenSLES sound card, the system is capable of giving us the best values to use for the buffer size and the sample rate
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT)
		{
			AudioManager audiomanager = (AudioManager)mContext.getSystemService(Context.AUDIO_SERVICE);

			String bufferProperty = audiomanager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER);
			bufferSize = parseInt(bufferProperty, bufferSize);
			String sampleRateProperty = audiomanager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE);
			sampleRate = parseInt(sampleRateProperty, sampleRate);
			Log.i("[Device] Output frames per buffer: " + bufferSize + ", output sample rate: " + sampleRate + ".");
			mDeviceFavoriteSampleRate = sampleRate;
			mDeviceFavoriteBufferSize = bufferSize;
		} else {
			Log.i("Android < 4.4 detected, android context not used.");
		}
	}

	public static boolean getSpeakerphoneAlwaysOn(org.linphone.mediastream.Factory factory) {
		//For special devices, speakerphone always on
		return ((factory.getDeviceFlags() & DEVICE_USE_ANDROID_CAMCORDER)!=0);
	}

	private static int parseInt(String value, int defaultValue) {
		int returnedValue = defaultValue;
		try {
			returnedValue = Integer.parseInt(value);
		} catch (NumberFormatException nfe) {
			Log.e("Can't parse " + value + " to integer ; using default value " + defaultValue);
		}
		return returnedValue;
	}
	native private int enableFilterFromNameImpl(String name,boolean enable);
	native private boolean filterFromNameEnabledImpl(String name);
	/**
	 * Specifies if a filter is enabled or not.
	 * @param name   A name for the filter. refer to ms2 internals to get list of filters
	 * @param enable, true/false
	 * @throws MediastreamException if filter name is unknown
	 * @deprecated
	 * */
	public static void enableFilterFromName(String name,boolean enable) throws MediastreamException {
		if (getInstance().enableFilterFromNameImpl(name,enable) != 0)
			throw new MediastreamException("Cannot "+(enable?"enable":"disable") + " filter  name ["+name+"]");
	}
	/**
	 * Specifies if a filter is enabled or not.
	 * @param name   A name for the filter. refer to ms2 internals to get list of filters
	 * @return enable, true/false
	 * @deprecated
	 * */
	public static boolean filterFromNameEnabled(String name) {
		return getInstance().filterFromNameEnabledImpl(name);
	}
}

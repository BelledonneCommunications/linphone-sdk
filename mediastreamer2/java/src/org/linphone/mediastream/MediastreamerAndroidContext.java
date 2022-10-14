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
import android.Manifest;
import java.io.File;
import android.content.pm.PackageManager;
import android.media.AudioManager;
import android.media.AudioDeviceInfo;
import android.os.Build;

public class MediastreamerAndroidContext {
	private static final int DEVICE_CHOICE = 0; // The device has the API to tell us it as or not a builtin AEC and we can trust it

	private static Context mContext;
	private static int mDeviceFavoriteSampleRate = 44100;
	private static int mDeviceFavoriteBufferSize = 256;
	private static boolean mDisableAudioRouteChanges = false;

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

	public static AudioDeviceInfo[] getAudioDevices(final String device_dir) {
		int flag = -1;
		switch(device_dir) {
			case "output":
				flag = AudioManager.GET_DEVICES_OUTPUTS;
				break;
			case "input":
				flag = AudioManager.GET_DEVICES_INPUTS;
				break;
			case "all":
				flag = AudioManager.GET_DEVICES_ALL;
				break;
			default:
				Log.e("Unknown device direction - Provided is " + device_dir + " Valid values are output input all");
				break;
		}

		AudioManager audiomanager = (AudioManager)getContext().getSystemService(Context.AUDIO_SERVICE);
		final AudioDeviceInfo[] devices = audiomanager.getDevices(flag);
		for (AudioDeviceInfo device : devices) {
			int type = device.getType();
			String stringType = getHumanReadableAudioDeviceType(type);
			Log.i("[Audio Manager] Found device: name [" + device.getProductName() + "], ID [" + device.getId() + "], type [" + stringType + " (" + type + ")], isSource [" + device.isSource() + "], isSink [" + device.isSink() + "]");
		}
		return devices;
	}

	public static boolean getSpeakerphoneAlwaysOn(org.linphone.mediastream.Factory factory) {
		//For special devices, speakerphone always on
		return ((factory.getDeviceFlags() & Factory.DEVICE_USE_ANDROID_CAMCORDER)!=0);
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
	 * @param enable Whether to enable or not the filter
	 * @throws MediastreamException if filter name is unknown
	 * @deprecated
	 * */
	public static void enableFilterFromName(String name, boolean enable) throws MediastreamException {
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

	public synchronized static boolean isAudioRouteChangesDisabled() {
		return mDisableAudioRouteChanges;
	}

	public synchronized static void disableAudioRouteChanges(boolean disable) {
		if (disable) {
			Log.i("[Audio Manager] Disabling audio route changes in sound cards");
		} else {
			Log.i("[Audio Manager] Enabling audio route changes in sound cards");
		}
		mDisableAudioRouteChanges = disable;
	}

	public synchronized static void enableSpeaker() { 
		AudioManager audioManager = (AudioManager)getContext().getSystemService(Context.AUDIO_SERVICE);
		if (audioManager.isBluetoothScoOn()) {
			stopBluetooth();
		}

		Log.i("[Audio Manager] Turning on speakerphone");
		audioManager.setSpeakerphoneOn(true);
	}

	public synchronized static void enableEarpiece() {
		AudioManager audioManager = (AudioManager)getContext().getSystemService(Context.AUDIO_SERVICE);
		if (audioManager.isBluetoothScoOn()) {
			stopBluetooth();
		}

		Log.i("[Audio Manager] Turning off speakerphone");
		audioManager.setSpeakerphoneOn(false);
	}

	public synchronized static void startBluetooth() {
		AudioManager audioManager = (AudioManager)getContext().getSystemService(Context.AUDIO_SERVICE);
		if (audioManager.isBluetoothScoOn()) {
			Log.i("[Audio Manager] Bluetooth SCO is already started");
		} else {
			Log.i("[Audio Manager] Starting bluetooth SCO");
			audioManager.setBluetoothScoOn(true);
			audioManager.startBluetoothSco();
		}
	}

	public synchronized static void stopBluetooth() {
		AudioManager audioManager = (AudioManager)getContext().getSystemService(Context.AUDIO_SERVICE);
		if (!audioManager.isBluetoothScoOn()) {
			Log.i("[Audio Manager] Bluetooth SCO is not started");
		} else {
			Log.i("[Audio Manager] Stopping bluetooth SCO");
			audioManager.stopBluetoothSco();
			audioManager.setBluetoothScoOn(false);
		}
	}

	public synchronized static void hackVolume() {
		Log.i("[Audio Manager] Lower & raise audio volume to workaround no sound issue until volume has changed...");
		AudioManager audioManager = (AudioManager)getContext().getSystemService(Context.AUDIO_SERVICE);

		int maxVolume = audioManager.getStreamMaxVolume(AudioManager.STREAM_VOICE_CALL);
		int currentVolume = audioManager.getStreamVolume(AudioManager.STREAM_VOICE_CALL);
		Log.i("[Audio Manager] Max volume for voice call stream is " + maxVolume + ", current volume is " + currentVolume);
		if (maxVolume == currentVolume) {
			audioManager.adjustSuggestedStreamVolume(AudioManager.ADJUST_LOWER, AudioManager.STREAM_VOICE_CALL, 0);
			audioManager.adjustSuggestedStreamVolume(AudioManager.ADJUST_RAISE, AudioManager.STREAM_VOICE_CALL, 0);
		} else {
			audioManager.adjustSuggestedStreamVolume(AudioManager.ADJUST_RAISE, AudioManager.STREAM_VOICE_CALL, 0);
			audioManager.adjustSuggestedStreamVolume(AudioManager.ADJUST_LOWER, AudioManager.STREAM_VOICE_CALL, 0);
		}
	}

	public synchronized static boolean isRecordAudioPermissionGranted() {
		boolean granted = mContext.checkSelfPermission(Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED;
		if (!granted) {
			Log.w("[Audio Manager] RECORD_AUDIO permission is not granted!");
		}
		return granted;
	}

	@TargetApi(31)
	public synchronized static boolean setCommunicationDevice(int id) {
		AudioManager audioManager = (AudioManager)getContext().getSystemService(Context.AUDIO_SERVICE);
		for (AudioDeviceInfo audioDeviceInfo : audioManager.getAvailableCommunicationDevices()) {
			if (audioDeviceInfo.getId() == id) {
				Log.i("[Audio Manager] Found available communication device matching ID [" + id + "]: " + audioDeviceInfo);
				return audioManager.setCommunicationDevice(audioDeviceInfo);
			}
		}
		return false;
	}

	@TargetApi(31)
	public synchronized static void clearCommunicationDevice() {
		AudioManager audioManager = (AudioManager)getContext().getSystemService(Context.AUDIO_SERVICE);
		audioManager.clearCommunicationDevice();
		Log.i("[Audio Manager] Cleared communication device");
	}

	private static String getHumanReadableAudioDeviceType(int type) {
		if (type == 19/*AudioDeviceInfo.TYPE_AUX_LINE*/) {
			return "Auxiliary line";
		} else if (type == 8/*AudioDeviceInfo.TYPE_BLUETOOTH_A2DP*/) {
			return "Bluetooth A2DP";
		} else if (type == 7/*AudioDeviceInfo.TYPE_BLUETOOTH_SCO*/) {
			return "Bluetooth SCO";
		} else if (type == 1/*AudioDeviceInfo.TYPE_BUILTIN_EARPIECE*/) {
			return "Built-in earpiece";
		} else if (type == 15/*AudioDeviceInfo.TYPE_BUILTIN_MIC*/) {
			return "Built-in microphone";
		} else if (type == 2/*AudioDeviceInfo.TYPE_BUILTIN_SPEAKER*/) {
			return "Built-in speaker";
		} else if (type == 24/*AudioDeviceInfo.TYPE_BUILTIN_SPEAKER_SAFE*/) {
			return "Built-in speaker (safe)";
		} else if (type == 21/*AudioDeviceInfo.TYPE_BUS*/) {
			return "Type agnostic bus";
		} else if (type == 13/*AudioDeviceInfo.TYPE_DOCK*/) {
			return "Dock";
		} else if (type == 14/*AudioDeviceInfo.TYPE_FM*/) {
			return "FM";
		} else if (type == 16/*AudioDeviceInfo.TYPE_FM_TUNER*/) {
			return "FM tuner";
		} else if (type == 9/*AudioDeviceInfo.TYPE_HDMI*/) {
			return "HDMI";
		} else if (type == 10/*AudioDeviceInfo.TYPE_HDMI_ARC*/) {
			return "HDMI Audio Return Channel";
		} else if (type == 23/*AudioDeviceInfo.TYPE_HEARING_AID*/) {
			return "Hearing aid";
		} else if (type == 20/*AudioDeviceInfo.TYPE_IP*/) {
			return "IP";
		} else if (type == 5/*AudioDeviceInfo.TYPE_LINE_ANALOG*/) {
			return "Analog";
		} else if (type == 6/*AudioDeviceInfo.TYPE_LINE_DIGITAL*/) {
			return "Digital";
		} else if (type == 18/*AudioDeviceInfo.TYPE_TELEPHONY*/) {
			return "Telephony";
		} else if (type == 17/*AudioDeviceInfo.TYPE_TV_TUNER*/) {
			return "TV tuner";
		} else if (type == 0/*AudioDeviceInfo.TYPE_UNKNOWN*/) {
			return "Unknown";
		} else if (type == 12/*AudioDeviceInfo.TYPE_USB_ACCESSORY*/) {
			return "USB accessory";
		} else if (type == 11/*AudioDeviceInfo.TYPE_USB_DEVICE*/) {
			return "USB";
		} else if (type == 22/*AudioDeviceInfo.TYPE_USB_HEADSET*/) {
			return "USB headset";
		} else if (type == 4/*AudioDeviceInfo.TYPE_WIRED_HEADPHONES*/) {
			return "Headphones";
		} else if (type == 3 /*AudioDeviceInfo.TYPE_WIRED_HEADSET*/) {
			return "Headset";
		} else if (type == 25/*TYPE_REMOTE_SUBMIX*/) {
			return "Remote Submix";
		} else if (type == 26/*TYPE_BLE_HEADSET*/) {
			return "BLE Headset";
		} else if (type == 27/*TYPE_BLE_SPEAKER*/) {
			return "BLE Speaker";
		} else if (type == 29/*TYPE_HDMI_EARC*/) {
			return "HDMI Enhanced Audio Return Channel";
		} else if (type == 30/*TYPE_BLE_BROADCAST*/) {
			return "BLE Broadcast";
		}
		return "UNEXPECTED";
	}
}

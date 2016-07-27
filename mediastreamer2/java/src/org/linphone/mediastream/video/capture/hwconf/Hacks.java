/*
Hacks.java
Copyright (C) 2010  Belledonne Communications, Grenoble, France

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
package org.linphone.mediastream.video.capture.hwconf;

import org.linphone.mediastream.Log;
import org.linphone.mediastream.Version;

import android.hardware.Camera;
import android.os.Build;

public final class Hacks {

	private static class BuiltInEchoCancellerModel {
		public String manufacturer;
		public String model;

		public BuiltInEchoCancellerModel(String manufacturer, String model) {
			this.manufacturer = manufacturer;
			this.model = model;
		}
	}

	private Hacks() {}

	private static BuiltInEchoCancellerModel[] mBuiltInEchoCancellerModels = new BuiltInEchoCancellerModel[] {
		//Deprecated: fill table in mediastreamer2/src/android/android_echo.cpp instead.
		new BuiltInEchoCancellerModel("samsung", "GT-I9100"),	// Samsung Galaxy SII
		new BuiltInEchoCancellerModel("samsung", "GT-I9300"),	// Samsung Galaxy SIII
	};


	public static boolean isGalaxySOrTabWithFrontCamera() {
		return isGalaxySOrTab() && !isGalaxySOrTabWithoutFrontCamera();
	}
	private static boolean isGalaxySOrTabWithoutFrontCamera() {
		return isSC02B() || isSGHI896();
	}


	public static boolean isGalaxySOrTab() {
		return isGalaxyS() || isGalaxyTab();
	}

	public static boolean isGalaxyTab() {
		return isGTP1000();
	}
	private static boolean isGalaxyS() {
		return isGT9000() || isSC02B() || isSGHI896() || isSPHD700();
	}
	
	public static final boolean hasTwoCamerasRear0Front1() {
		return isLGP970() || isSPHD700() || isADR6400();
	}
	
	// HTC
	private static final boolean isADR6400() {
		return Build.MODEL.startsWith("ADR6400") || Build.DEVICE.startsWith("ADR6400");
	} // HTC Thunderbolt

	// Galaxy S variants
	private static final boolean isSPHD700() {return Build.DEVICE.startsWith("SPH-D700");} // Epic 
	private static boolean isSGHI896() {return Build.DEVICE.startsWith("SGH-I896");} // Captivate
	private static boolean isGT9000() {return Build.DEVICE.startsWith("GT-I9000");} // Galaxy S
	private static boolean isSC02B() {return Build.DEVICE.startsWith("SC-02B");} // Docomo
	private static boolean isGTP1000() {return Build.DEVICE.startsWith("GT-P1000");} // Tab

	// LG with two cameras
	private static final boolean isLGP970() {return Build.DEVICE.startsWith("LG-P970");}

	public static final void sleep(int time) {
		try  {
			Thread.sleep(time);
		} catch(InterruptedException ie){}
	}

	public static boolean needSoftvolume() {
		return isGalaxySOrTab() && Version.sdkStrictlyBelow(Version.API09_GINGERBREAD_23);
	}

	public static boolean needGalaxySAudioHack() {
		return isGalaxySOrTab() && !isSC02B();
	}

	public static boolean needPausingCallForSpeakers() {
		return needGalaxySAudioHack();
	}

	public static boolean hasCamera() {
		if (Version.sdkAboveOrEqual(9)) {
			int nb = 0;
			try {
				nb = (Integer) Camera.class.getMethod("getNumberOfCameras", (Class[])null).invoke(null);
			} catch (Exception e) {
				Log.e("Error getting number of cameras");
			}
			return nb > 0;
		}

		Log.i("Hack: considering there IS a camera.\n"
				+ "If it is not the case, report DEVICE and MODEL to linphone-users@nongnu.org");
		return true;
	}

	public static boolean hasBuiltInEchoCanceller() {
		for (BuiltInEchoCancellerModel model: mBuiltInEchoCancellerModels) {
			if (Build.MANUFACTURER.equals(model.manufacturer) && Build.MODEL.startsWith(model.model)) {
				Log.i(Build.MANUFACTURER + " " + Build.MODEL + " has a built-in echo canceller");
				return true;
			}
		}
		Log.i(Build.MANUFACTURER + " " + Build.MODEL + " doesn't have a built-in echo canceller");
		return false;
	}
}

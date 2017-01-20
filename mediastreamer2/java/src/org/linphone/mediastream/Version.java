/*
Version.java
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
package org.linphone.mediastream;

import java.util.ArrayList;
import java.util.List;

import org.linphone.mediastream.video.capture.hwconf.Hacks;

import android.content.Context;
import android.content.res.Configuration;
import android.os.Build;

/**
 * Centralize version access and allow simulation of lower versions.
 * @author Guillaume Beraudo
 */
public class Version {

	public static final int API03_CUPCAKE_15 = 3;
	public static final int API04_DONUT_16 = 4;
	public static final int API05_ECLAIR_20 = 5;
	public static final int API06_ECLAIR_201 = 6;
	public static final int API07_ECLAIR_21 = 7;
	public static final int API08_FROYO_22 = 8;
	public static final int API09_GINGERBREAD_23 = 9;
	public static final int API10_GINGERBREAD_MR1_233 = 10;
	public static final int API11_HONEYCOMB_30 = 11;
	public static final int API12_HONEYCOMB_MR1_31X = 12;
	public static final int API13_HONEYCOMB_MR2_32  = 13;
	public static final int API14_ICE_CREAM_SANDWICH_40 = 14;
	public static final int API15_ICE_CREAM_SANDWICH_403 = 15;
	public static final int API16_JELLY_BEAN_41 = 16;
	public static final int API17_JELLY_BEAN_42 = 17;
	public static final int API18_JELLY_BEAN_43 = 18;
	public static final int API19_KITKAT_44 = 19;
	public static final int API21_LOLLIPOP_50 = 21;
	public static final int API22_LOLLIPOP_51 = 22;
	public static final int API23_MARSHMALLOW_60 = 23;
	public static final int API24_NOUGAT_70 = 24;

	private static native boolean nativeHasZrtp();
	private static native boolean nativeHasNeon();
	private static Boolean hasNeon;

	private static final int buildVersion = Build.VERSION.SDK_INT;
//		API03_CUPCAKE_15;
//		8; // 2.2
//		7; // 2.1

	public static boolean isXLargeScreen(Context context) 
	{
		return (context.getResources().getConfiguration().screenLayout & Configuration.SCREENLAYOUT_SIZE_MASK) >= Configuration.SCREENLAYOUT_SIZE_XLARGE;
	}
	
	public static final boolean sdkAboveOrEqual(int value) {
		return buildVersion >= value;
	}

	public static final boolean sdkStrictlyBelow(int value) {
		return buildVersion < value;
	}

	public static int sdk() {
		return buildVersion;
	}

	public static List<String> getCpuAbis(){
		List<String> cpuabis=new ArrayList<String>();
		if (sdkAboveOrEqual(API21_LOLLIPOP_50)){
			try {
				String abis[]=(String[])Build.class.getField("SUPPORTED_ABIS").get(null);
				for (String abi: abis){
					cpuabis.add(abi);
				}
			} catch (Throwable e) {
				Log.e(e);
			}
		}else{
			cpuabis.add(Build.CPU_ABI);
			cpuabis.add(Build.CPU_ABI2);
		}
		return cpuabis;
	}
	private static boolean isArm64() {
		try {
			return getCpuAbis().get(0).startsWith("arm64-v8a");
		} catch (Throwable e) {
			Log.e(e);
		}
		return false;
	}
	private static boolean isArmv7() {
		try {
			return getCpuAbis().get(0).startsWith("armeabi-v7");
		} catch (Throwable e) {
			Log.e(e);
		}
		return false;
	}
	private static boolean isX86() {
		try {
			return getCpuAbis().get(0).startsWith("x86");
		} catch (Throwable e) {
			Log.e(e);
		}
		return false;
	}
	private static boolean isArmv5() {
		try {
			return getCpuAbis().get(0).equals("armeabi");
		} catch (Throwable e) {
			Log.e(e);
		}
		return false;
	}
	public static boolean hasNeon(){
		if (hasNeon == null) hasNeon = nativeHasNeon();
		return hasNeon;
	}
	public static boolean hasFastCpu() {
		return !isArmv5();
	}
	public static boolean hasFastCpuWithAsmOptim() {
		return isX86() || isArm64() || (!isArmv5() && hasNeon());
	}
	public static boolean isVideoCapable() {
		return !Version.sdkStrictlyBelow(5) && Version.hasFastCpu();
	}
	public static boolean isHDVideoCapable() {
		int availableCores = Runtime.getRuntime().availableProcessors();
		return isVideoCapable() && hasFastCpuWithAsmOptim() && (availableCores > 1);
	}

	private static Boolean sCacheHasZrtp;
	public static boolean hasZrtp(){
		if (sCacheHasZrtp == null) {
			sCacheHasZrtp = nativeHasZrtp();
		}
		return sCacheHasZrtp;
	}

	public static void dumpCapabilities(){
		StringBuilder sb = new StringBuilder(" ==== Capabilities dump ====\n");
		if (isArmv7()) sb.append("Has neon: ").append(Boolean.toString(hasNeon())).append("\n");
		sb.append("Has ZRTP: ").append(Boolean.toString(hasZrtp())).append("\n");
		Log.i(sb.toString());
	}
}

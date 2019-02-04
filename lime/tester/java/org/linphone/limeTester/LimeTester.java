/*
	LimeTester.java
	@author Johan Pascal
	@copyright 	Copyright (C) 2019  Belledonne Communications SARL

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

package org.linphone.limeTester;

import org.linphone.lime.*;

import javax.net.ssl.*;
import java.security.cert.X509Certificate;

import javax.net.ssl.HttpsURLConnection;

public class LimeTester {
	public static void main(String[] args) {
		// Load lime library
		try {
			System.loadLibrary("lime");
		} catch (Exception e) {
			System.out.println("Exception while loading peer library" + e.getMessage());
			System.exit(-1);
		}

		// INSECURE CODE - USED FOR TESTING ONLY - DO NOT USE THIS IN REAL SITUATION
		// Disable SSL certificate verification
		TrustManager[] trustAllCerts = new TrustManager[] {
			new X509TrustManager() {
				public java.security.cert.X509Certificate[] getAcceptedIssuers() {
					return new X509Certificate[0];
				}
				public void checkClientTrusted(java.security.cert.X509Certificate[] certs, String authType) { }
				public void checkServerTrusted(java.security.cert.X509Certificate[] certs, String authType) { }
			}
		};

		try {
			SSLContext sc = SSLContext.getInstance("SSL");
			sc.init(null, trustAllCerts, new java.security.SecureRandom());
			HttpsURLConnection.setDefaultSSLSocketFactory(sc.getSocketFactory());
		} catch (Exception e) {
			System.out.println("Exception while initializing the trustAll certificates" + e.getMessage());
			System.exit(-1);
		}

		// Get system properties to check if we enabled curves 25519 and/or 448
		// default is disabling the tests
		String SPenableC25519 = System.getProperty("enableC25519");
		String SPenableC448 = System.getProperty("enableC448");
		boolean enableC25519 = false;
		boolean enableC448 = false;
		if (SPenableC25519 != null) {
			enableC25519 = SPenableC25519.equals("true");
		}
		if (SPenableC448 != null) {
			enableC448 = SPenableC448.equals("true");
		}
		System.out.println("Tests enabled on curve 25519 "+enableC25519);
		System.out.println("Tests enabled on curve 448 "+enableC448);
		// End of INSECURE CODE - USED FOR TESTING ONLY - DO NOT USE THIS IN REAL SITUATION

		/*
		 * Hello World test:
		 * Mostly a code demo, see HelloWorld.java for details.
		 * Run it with synchronous and asynchronous X3DH server access(leading to synchronous or asynchronous lime operations).
		 * Synchronous is just for simple code example, real situation is much more likely to be asynchronous
		 * HelloWorld code is meant to be run with asynchronous X3DH server access but is anyway able to run synchronous.
		 */
		LimePostToX3DH_Sync sync_postObj = new LimePostToX3DH_Sync();
		if (enableC25519) HelloWorld.hello_world(LimeCurveId.C25519, "hello_world", "https://localhost:25519", sync_postObj);
		if (enableC448) HelloWorld.hello_world(LimeCurveId.C448, "hello_world", "https://localhost:25520", sync_postObj);

		LimePostToX3DH_Async async_postObj = new LimePostToX3DH_Async();
		if (enableC25519) HelloWorld.hello_world(LimeCurveId.C25519, "hello_world", "https://localhost:25519", async_postObj);
		if (enableC448) HelloWorld.hello_world(LimeCurveId.C448, "hello_world", "https://localhost:25520", async_postObj);

		/*
		 * Lime user management test
		 */
		if (enableC25519) LimeLimeTester.user_management(LimeCurveId.C25519, "lime_user_management", "https://localhost:25519", async_postObj);
		if (enableC448) LimeLimeTester.user_management(LimeCurveId.C448, "lime_user_management", "https://localhost:25520", async_postObj);

		/*
		 * Lime Basic test
		 */
		if (enableC25519) LimeLimeTester.basic(LimeCurveId.C25519, "lime_basic", "https://localhost:25519", async_postObj);
		if (enableC448) LimeLimeTester.basic(LimeCurveId.C448, "lime_basic", "https://localhost:25520", async_postObj);

		/*
		 * Lime User not found test
		 */
		if (enableC25519) LimeLimeTester.user_not_found(LimeCurveId.C25519, "lime_user_not_found", "https://localhost:25519", async_postObj);
		if (enableC448) LimeLimeTester.user_not_found(LimeCurveId.C448, "lime_user_not_found", "https://localhost:25520", async_postObj);

		/*
		 * Lime multidevice operation queue
		 */
		if (enableC25519) LimeLimeTester.x3dh_multidev_operation_queue(LimeCurveId.C25519, "lime_multidev_operation_queue", "https://localhost:25519", async_postObj);
		if (enableC448) LimeLimeTester.x3dh_multidev_operation_queue(LimeCurveId.C448, "lime_multidev_operation_queue", "https://localhost:25520", async_postObj);


		/*
		 * Lime getSelf Ik
		 */
		byte[] pattern_selfIk_C25519 = {(byte)0x55, (byte)0x6b, (byte)0x4a, (byte)0xc2, (byte)0x24, (byte)0xc1, (byte)0xd4, (byte)0xff, (byte)0xb7, (byte)0x44, (byte)0x82, (byte)0xe2, (byte)0x3c, (byte)0x75, (byte)0x1c, (byte)0x2b, (byte)0x1c, (byte)0xcb, (byte)0xf6, (byte)0xe2, (byte)0x96, (byte)0xcb, (byte)0x18, (byte)0x01, (byte)0xc6, (byte)0x76, (byte)0x2d, (byte)0x30, (byte)0xa0, (byte)0xa2, (byte)0xbb, (byte)0x27};
		if (enableC25519) LimeLimeTester.getSelfIk(LimeCurveId.C25519, "pattern_getSelfIk.C25519.sqlite3", async_postObj, pattern_selfIk_C25519);
		byte[] pattern_selfIk_C448 = {(byte)0xe7, (byte)0x96, (byte)0x9e, (byte)0x53, (byte)0xd3, (byte)0xbf, (byte)0xfb, (byte)0x4c, (byte)0x6d, (byte)0xdb, (byte)0x79, (byte)0xd2, (byte)0xd7, (byte)0x24, (byte)0x91, (byte)0x7b, (byte)0xa8, (byte)0x99, (byte)0x87, (byte)0x20, (byte)0x23, (byte)0xe1, (byte)0xec, (byte)0xd4, (byte)0xb5, (byte)0x76, (byte)0x0f, (byte)0xc2, (byte)0x83, (byte)0xae, (byte)0x5a, (byte)0xf9, (byte)0x1d, (byte)0x25, (byte)0x47, (byte)0xda, (byte)0x0e, (byte)0x71, (byte)0x50, (byte)0xd5, (byte)0xaf, (byte)0x79, (byte)0x92, (byte)0x48, (byte)0xb0, (byte)0xb6, (byte)0x0f, (byte)0xdc, (byte)0x6f, (byte)0x73, (byte)0x3f, (byte)0xd9, (byte)0x9c, (byte)0x2c, (byte)0x95, (byte)0xe3, (byte)0x00};
		if (enableC448) LimeLimeTester.getSelfIk(LimeCurveId.C448, "pattern_getSelfIk.C448.sqlite3", async_postObj, pattern_selfIk_C448);

		/*
		 * Lime peer device status
		 */
		if (enableC25519) LimeLimeTester.peerDeviceStatus(LimeCurveId.C25519, "lime_peerDeviceStatus", "https://localhost:25519", async_postObj);
		if (enableC448) LimeLimeTester.peerDeviceStatus(LimeCurveId.C448, "lime_peerDeviceStatus", "https://localhost:25520", async_postObj);

		/*
		 * Lime encryption policy
		 */
		if (enableC25519) LimeLimeTester.encryptionPolicy_suite(LimeCurveId.C25519, "lime_encryptioPolicy", "https://localhost:25519", async_postObj);
		if (enableC448) LimeLimeTester.encryptionPolicy_suite(LimeCurveId.C448, "lime_encryptioPolicy", "https://localhost:25520", async_postObj);

		/*
		 * Lime identity verified status
		 */
		if (enableC25519) LimeLimeTester.identityVerifiedStatus(LimeCurveId.C25519, "lime_identityVerifiedStatus", "https://localhost:25519", async_postObj);
		if (enableC448) LimeLimeTester.identityVerifiedStatus(LimeCurveId.C448, "lime_identityVerifiedStatus", "https://localhost:25520", async_postObj);

		System.exit(0);
	}
}

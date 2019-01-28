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
		// End of INSECURE CODE - USED FOR TESTING ONLY - DO NOT USE THIS IN REAL SITUATION

		/*
		 * Hello World test:
		 * Mostly a code demo, see HelloWorld.java for details.
		 * Run it with synchronous and asynchronous X3DH server access(leading to synchronous or asynchronous lime operations).
		 * Synchronous is just for simple code example, real situation is much more likely to be asynchronous
		 * HelloWorld code is meant to be run with asynchronous X3DH server access but is anyway able to run synchronous.
		 */
		LimePostToX3DH_Sync sync_postObj = new LimePostToX3DH_Sync();
		HelloWorld.hello_world(LimeCurveId.C25519, "hello_world", "https://localhost:25519", sync_postObj);
		HelloWorld.hello_world(LimeCurveId.C448, "hello_world", "https://localhost:25520", sync_postObj);

		LimePostToX3DH_Async async_postObj = new LimePostToX3DH_Async();
		HelloWorld.hello_world(LimeCurveId.C25519, "hello_world", "https://localhost:25519", async_postObj);
		HelloWorld.hello_world(LimeCurveId.C448, "hello_world", "https://localhost:25520", async_postObj);

		/*
		 * Lime Basic test
		 */
		LimeLimeTester.basic(LimeCurveId.C25519, "lime_basic", "https://localhost:25519", async_postObj);
		LimeLimeTester.basic(LimeCurveId.C448, "lime_basic", "https://localhost:25520", async_postObj);
		System.exit(0);
	}
}

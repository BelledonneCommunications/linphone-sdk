/*
	PostToX3DH.java
	@author Johan Pascal
	@copyright	Copyright (C) 2019  Belledonne Communications SARL

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
package org.linphone.lime;

import java.net.URL;
import javax.net.ssl.HttpsURLConnection;
import java.io.DataOutputStream;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;

/** @brief Class to communicate with X3DH server
 * an object is instanciated for each POST to the server as it hold a pointer to a encapsulated c++ closure
 * the object is instanciated from the native code so do not modify class or method name
 */
public class PostToX3DH {
	public long m_responseFunctionPtr; // store a native pointer used to send the response to the lime library via a stateful callback

	/** @brief Function call by native side to post a message to an X3DH server.
	 * The connection is synchronous so this function also collect the answer
	 * and send it back to the native library using the process_response native method
	 */
	public void postToX3DHServer(long ptr, String url, String from, byte[] message) {
		try {
			m_responseFunctionPtr = ptr;

			// connect to the given URL
			String local_url = new String(url.toCharArray());
			URL obj = new URL(local_url);
			HttpsURLConnection con = (HttpsURLConnection) obj.openConnection();

			// set request header
			con.setRequestMethod("POST");
			con.setRequestProperty("User-Agent", "lime");
			con.setRequestProperty("Content-type", "x3dh/octet-stream");
			con.setRequestProperty("From", from);

			// Send post request
			con.setDoOutput(true);
			DataOutputStream wr = new DataOutputStream(con.getOutputStream());
			wr.write(message, 0, message.length);
			wr.flush();
			wr.close();

			// wait for a response
			int responseCode = con.getResponseCode();
			InputStream in = con.getInputStream();
			ByteArrayOutputStream response = new ByteArrayOutputStream( );

			byte[] buffer = new byte[256];
			int bufferLength;

			while ((bufferLength = in.read(buffer)) != -1){
				response.write(buffer, 0, bufferLength);
			}
			in.close();

			// call response process native function
			process_response(m_responseFunctionPtr, responseCode, response.toByteArray());
			response.close();
		}
		catch (Exception e) {
			System.out.println("Exception during HTTPS connection" + e.getMessage());
		}
	}

	// native function to process response
	public static native void process_response(long ptr, int responseCode, byte[] response);

	// at construction(fired by native code only), store the native pointer(casted to a jlong)
	// to be able to give it back to the native process_response method
	public PostToX3DH() {
		m_responseFunctionPtr = 0;
	}
	public PostToX3DH(long response) {
		m_responseFunctionPtr = response;
	}
}

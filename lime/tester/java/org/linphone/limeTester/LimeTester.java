package org.linphone.limeTester;

import org.linphone.lime.*;

import javax.net.ssl.*;
import java.util.UUID;
import java.security.cert.X509Certificate;

import java.io.File;

import java.net.URL;
import javax.net.ssl.HttpsURLConnection;
import java.io.DataOutputStream;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;

import java.util.concurrent.*;

/**
 * @brief For testing purpose, we just count the success and fail received
 * This only mandatory method to implement is callback
 * Note: in real situation you may want to implements several variants of
 * this interface :
 *  - dedicated to encrypt call would store reference to in/out buffers
 *  - used on create_user, delete_user, update would only need to process the
 *  status.
 */
class LimeStatusCallbackImpl implements LimeStatusCallback {
	public int success;
	public int fail;
	public int timeout;

	/**
	 * @brief Function called by native code when asynchronous processing is completed
	 *
	 * @param[in]	status	an integer mapped lime:CallbackReturn, use LimeCallbackReturn.fromNative to turn it into a java enumeration
	 * @param[in]	message	a string message giving some details in case of failure
	 */
	public void callback(int cstatus, String message) {
		LimeCallbackReturn status = LimeCallbackReturn.fromNative(cstatus);
		if (status == LimeCallbackReturn.SUCCESS) {
			success++;
		} else { // status is LimeCallbackReturn.FAIL
			fail++;
		}
	}

	public void resetStatus() {
		success = 0;
		fail = 0;
		timeout = 4000;
	}

	public LimeStatusCallbackImpl() {
		resetStatus();
	}

	public boolean wait_for_success(int expected_success) {
		try {
			int time = 0;
			while (time<timeout && success<expected_success) {
				time += 25;
				Thread.sleep(25);
			}
		}
		catch (InterruptedException e) {
			System.out.println("Interrupt exception in wait for success");
			return false;
		}

		if (expected_success == success) {
			return true;
		} else {
			return false;
		}
	}
}

class LimePostToX3DH_Sync implements LimePostToX3DH {
	/** @brief Function call by native side to post a message to an X3DH server.
	 * The connection is synchronous so this function also collect the answer
	 * and send it back to the native library using the LimeManager.process_response native method
	 */
	public void postToX3DHServer(long ptr, String url, String from, byte[] message) {
		try {
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
			LimeManager.process_X3DHresponse(ptr, responseCode, response.toByteArray());
			response.close();
		}
		catch (Exception e) {
			System.out.println("Exception during HTTPS connection" + e.getMessage());
		}
	}
}

class RunnablePostToHttp implements Runnable {
	private Thread t;
	private long m_ptr;
	private String m_url;
	private String m_from;
	private byte[] m_message;

	RunnablePostToHttp( long ptr, String url, String from, byte[] message) {
		m_ptr = ptr;
		m_url = url;
		m_from = from;
		m_message = message;
	}


	public void run() {
		try {
			// connect to the given URL
			String local_url = new String(m_url.toCharArray());
			URL obj = new URL(local_url);
			HttpsURLConnection con = (HttpsURLConnection) obj.openConnection();

			// set request header
			con.setRequestMethod("POST");
			con.setRequestProperty("User-Agent", "lime");
			con.setRequestProperty("Content-type", "x3dh/octet-stream");
			con.setRequestProperty("From", m_from);

			// Send post request
			con.setDoOutput(true);
			DataOutputStream wr = new DataOutputStream(con.getOutputStream());
			wr.write(m_message, 0, m_message.length);
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
			LimeManager.process_X3DHresponse(m_ptr, responseCode, response.toByteArray());
			response.close();
		}
		catch (Exception e) {
			System.out.println("Thread interrupted : "+ e.getMessage());
		}
	}
}

class LimePostToX3DH_Async implements LimePostToX3DH {
	/** @brief Function call by native side to post a message to an X3DH server.
	 * The connection is asynchronous : when called the post function starts a new thread
	 * and makes it request in it, then collect the answer
	 * and send it back to the native library using the LimeManager.process_response native method
	 */
	public void postToX3DHServer(long ptr, String url, String from, byte[] message) {
		RunnablePostToHttp poster = new RunnablePostToHttp(ptr, url, from, message);
		new Thread(poster).start();
	}
}

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

		/**
		 * Hello World test:
		 * - mostly a code demo. More details in the c++ hello world test, but it is a good start
		 * Run it synchronous and asynchronous on both curves.
		 */
		LimePostToX3DH_Sync sync_postObj = new LimePostToX3DH_Sync();
		HelloWorld.hello_world(LimeCurveId.C25519, "hello_world", "https://localhost:25519", sync_postObj);
		HelloWorld.hello_world(LimeCurveId.C448, "hello_world", "https://localhost:25520", sync_postObj);

		LimePostToX3DH_Async async_postObj = new LimePostToX3DH_Async();
		HelloWorld.hello_world(LimeCurveId.C25519, "hello_world", "https://localhost:25519", async_postObj);
		HelloWorld.hello_world(LimeCurveId.C448, "hello_world", "https://localhost:25520", async_postObj);

		System.exit(0);
	}
}

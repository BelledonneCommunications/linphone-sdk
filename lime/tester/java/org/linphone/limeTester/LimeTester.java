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
	/**
	 * Test Scenario:
	 * - Create Alice and Bob users
	 * - Alice encrypts to Bob who decrypts and check it matches the original
	 * - Bob encryps to Alice who decrypts and check it matches the original
	 * - Alice and Bob call the update function
	 */
	public static void hello_world(LimeCurveId curveId, String dbBasename, String x3dhServerUrl, LimePostToX3DH postObj) {
		int expected_success = 0;
		int expected_fail = 0;

		// Create a callback
		LimeStatusCallbackImpl statusCallback = new LimeStatusCallbackImpl();

		// Create random device id for alice and bob
		String AliceDeviceId = "alice"+UUID.randomUUID().toString();
		String BobDeviceId = "bob"+UUID.randomUUID().toString();

		// Create db filenames and delete potential existing ones
		String curveIdString;
		if (curveId == LimeCurveId.C25519) {
			curveIdString = ".C25519.";
		} else {
			curveIdString = ".C448.";
		}
		String aliceDbFilename = "alice."+dbBasename+curveIdString+".sqlite3";
		String bobDbFilename = "bob."+dbBasename+curveIdString+".sqlite3";
		File file = new File(aliceDbFilename);
		file.delete();
		file = new File(bobDbFilename);
		file.delete();

		// Create bob and alice lime managers
		LimeManager aliceManager = new LimeManager(aliceDbFilename, postObj);
		LimeManager bobManager = new LimeManager(bobDbFilename, postObj);

		// Use this managers to create users
		expected_success+= 2;
		aliceManager.create_user(AliceDeviceId, x3dhServerUrl, curveId, statusCallback);
		bobManager.create_user(BobDeviceId, x3dhServerUrl, curveId, 10, statusCallback);
		assert (statusCallback.wait_for_success(expected_success));
		assert (statusCallback.fail == expected_fail);

		// prepare for encrypt: An array of RecipientData, one element for each recipient device
		// encrypted output will be stored in it
		RecipientData[] recipients = new RecipientData[1]; // 1 target for the encrypt
		String plainMessage = "Moi je connais un ami il s'appelle Alceste";
		recipients[0] = new RecipientData(BobDeviceId);
		LimeOutputBuffer cipherMessage = new LimeOutputBuffer();

		// encrypts, the plain message is a byte array so we can encrypt anything
		// this call is asynchronous (unless you use an synchronous connection to the X3DH server)
		// and the statusCallback.callback function will be called when the encryption is done
		expected_success+= 1;
		aliceManager.encrypt(AliceDeviceId, "bob", recipients, plainMessage.getBytes(), cipherMessage, statusCallback);
		// here we wait for the status callback to be called but a full asynchronous process would
		// store references to recipients and cipherMessage in the statusCallback object and process
		// them from the callback method.
		assert (statusCallback.wait_for_success(expected_success));
		assert (recipients[0].getPeerStatus() == LimePeerDeviceStatus.UNKNOWN):"Encryption status for Bob is expected to be UNKNOWN"; // Bob is unknown in Alice's base
		assert (cipherMessage.buffer.length==0):"Default encryption policy implies no cipherMessage for one recipient only";
		assert (statusCallback.fail == expected_fail);

		LimeOutputBuffer decodedMessage = new LimeOutputBuffer();
		// decrypt and check we get back to the original
		// decryption is synchronous, no callback on this one
		LimePeerDeviceStatus status = bobManager.decrypt(BobDeviceId, "bob", AliceDeviceId, recipients[0].DRmessage, decodedMessage);
		String s = new String(decodedMessage.buffer);
		assert (s.equals(plainMessage)):"Decoded message is not the encoded one";
		assert (status == LimePeerDeviceStatus.UNKNOWN):"decrypt status was expected to be unknown but is not";

		// encrypt another message
		recipients = new RecipientData[1]; // 1 target for the encrypt
		plainMessage = "Nous, on l'appelle Zantrop c'est note ami Zantrop";
		recipients[0] = new RecipientData(AliceDeviceId);
		cipherMessage = new LimeOutputBuffer();

		expected_success+= 1;
		bobManager.encrypt(BobDeviceId, "alice", recipients, plainMessage.getBytes(), cipherMessage, statusCallback, LimeEncryptionPolicy.CIPHERMESSAGE);
		assert (statusCallback.wait_for_success(expected_success));
		assert (recipients[0].getPeerStatus() == LimePeerDeviceStatus.UNTRUSTED):"Encryption status for Alice is expected to be UNTRUSTED"; // Alice status is untrusted in Bob's base
		assert (cipherMessage.buffer.length>0):"Forced cipherMessage encryption policy must produce a cipherMessage even for one recipient only";
		assert (statusCallback.fail == expected_fail);


		decodedMessage = new LimeOutputBuffer();
		// decrypt and check we get back to the original
		status = aliceManager.decrypt(AliceDeviceId, "alice", BobDeviceId, recipients[0].DRmessage, cipherMessage.buffer, decodedMessage);
		s = new String(decodedMessage.buffer);
		assert (s.equals(plainMessage)):"Decoded message is not the encoded one";
		assert (status == LimePeerDeviceStatus.UNTRUSTED):"decrypt status was expected to be untrusted but is not";

		expected_success+= 2;
		aliceManager.update(statusCallback);
		bobManager.update(statusCallback);
		assert (statusCallback.wait_for_success(expected_success));
		assert (statusCallback.fail == expected_fail);

		// clean db
		expected_success+= 2;
		aliceManager.delete_user(AliceDeviceId, statusCallback);
		bobManager.delete_user(BobDeviceId, statusCallback);
		assert (statusCallback.wait_for_success(expected_success));
		assert (statusCallback.fail == expected_fail);

		// Force garbage collection to be sure the native object destructor is called
		aliceManager = null;
		bobManager = null;
		System.gc();
	}

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
		hello_world(LimeCurveId.C25519, "hello_world", "https://localhost:25519", sync_postObj);
		hello_world(LimeCurveId.C448, "hello_world", "https://localhost:25520", sync_postObj);

		LimePostToX3DH_Async async_postObj = new LimePostToX3DH_Async();
		hello_world(LimeCurveId.C25519, "hello_world", "https://localhost:25519", async_postObj);
		hello_world(LimeCurveId.C448, "hello_world", "https://localhost:25520", async_postObj);

		System.exit(0);
	}
}

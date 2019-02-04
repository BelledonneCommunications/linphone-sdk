/*
	LimeTesterUtils.java
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

import java.net.URL;
import javax.net.ssl.HttpsURLConnection;
import java.io.DataOutputStream;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.util.concurrent.*;
import java.util.Arrays;

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

	public boolean wait_for_fail(int expected_fail) {
		try {
			int time = 0;
			while (time<timeout && fail<expected_fail) {
				time += 25;
				Thread.sleep(25);
			}
		}
		catch (InterruptedException e) {
			System.out.println("Interrupt exception in wait for fail");
			return false;
		}

		if (expected_fail == fail) {
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

public class LimeTesterUtils {
	// Protocol version mapped to a unsigned byte (this must be aligned with definition in the cpp code)
	public final static byte DR_v01 = 1;
	public final static byte X3DHInitFlagbit = 0x01;
	public final static byte PayloadEncryptionFlagbit = 0x02;
	public final static byte C25519 = 1;
	public final static byte C448 = 2;

	public final static String shortMessage = "Short Message";
	// with a long one(>80 <176) optimizeUploadSzie policy shall go for the cipherMessage encryption, but the optimizeGlobalBandwith stick to DRmessage (with 2 recipients)
	public final static String longMessage = "This message is long enough to automatically switch to cipher Message mode when at least two recipients are involved.";
	// with a very long one(>176) all optimize policies shall go for the cipherMessage encryption(with 2 recipients)
	public final static String veryLongMessage = "This message is long enough to automatically switch to cipher Message mode when at least two recipients are involved. This message is long enough to automatically switch to cipher Message mode when at least two recipients are involved.";

	public final static String[] patterns = {
		"Frankly, my dear, I don't give a damn.",
		"I'm gonna make him an offer he can't refuse.",
		"You don't understand! I coulda had class. I coulda been a contender. I could've been somebody, instead of a bum, which is what I am.",
		"Toto, I've a feeling we're not in Kansas anymore.",
		"Here's looking at you, kid.",
		"Go ahead, make my day.",
		"All right, Mr. DeMille, I'm ready for my close-up.",
		"May the Force be with you.",
		"Fasten your seatbelts. It's going to be a bumpy night.",
		"You talking to me?",
		"What we've got here is failure to communicate.",
		"I love the smell of napalm in the morning. ",
		"Love means never having to say you're sorry.",
		"The stuff that dreams are made of.",
		"E.T. phone home.",
		"They call me Mister Tibbs!",
		"Rosebud.",
		"Made it, Ma! Top of the world!",
		"I'm as mad as hell, and I'm not going to take this anymore!",
		"Louis, I think this is the beginning of a beautiful friendship.",
		"A census taker once tried to test me. I ate his liver with some fava beans and a nice Chianti.",
		"Bond. James Bond.",
		"There's no place like home. ",
		"I am big! It's the pictures that got small.",
		"Show me the money!",
		"Why don't you come up sometime and see me?",
		"I'm walking here! I'm walking here!",
		"Play it, Sam. Play 'As Time Goes By.'",
		"You can't handle the truth!",
		"I want to be alone.",
		"After all, tomorrow is another day!",
		"Round up the usual suspects.",
		"I'll have what she's having.",
		"You know how to whistle, don't you, Steve? You just put your lips together and blow.",
		"You're gonna need a bigger boat.",
		"Badges? We ain't got no badges! We don't need no badges! I don't have to show you any stinking badges!",
		"I'll be back.",
		"Today, I consider myself the luckiest man on the face of the earth.",
		"If you build it, he will come.",
		"My mama always said life was like a box of chocolates. You never know what you're gonna get.",
		"We rob banks.",
		"Plastics.",
		"We'll always have Paris.",
		"I see dead people.",
		"Stella! Hey, Stella!",
		"Oh, Jerry, don't let's ask for the moon. We have the stars.",
		"Shane. Shane. Come back!",
		"Well, nobody's perfect.",
		"It's alive! It's alive!",
		"Houston, we have a problem.",
		"You've got to ask yourself one question: 'Do I feel lucky?' Well, do ya, punk?",
		"You had me at 'hello.'",
		"One morning I shot an elephant in my pajamas. How he got in my pajamas, I don't know.",
		"There's no crying in baseball!",
		"La-dee-da, la-dee-da.",
		"A boy's best friend is his mother.",
		"Greed, for lack of a better word, is good.",
		"Keep your friends close, but your enemies closer.",
		"As God is my witness, I'll never be hungry again.",
		"Well, here's another nice mess you've gotten me into!",
		"Say 'hello' to my little friend!",
		"What a dump.",
		"Mrs. Robinson, you're trying to seduce me. Aren't you?",
		"Gentlemen, you can't fight in here! This is the War Room!",
		"Elementary, my dear Watson.",
		"Take your stinking paws off me, you damned dirty ape.",
		"Of all the gin joints in all the towns in all the world, she walks into mine.",
		"Here's Johnny!",
		"They're here!",
		"Is it safe?",
		"Wait a minute, wait a minute. You ain't heard nothin' yet!",
		"No wire hangers, ever!",
		"Mother of mercy, is this the end of Rico?",
		"Forget it, Jake, it's Chinatown.",
		"I have always depended on the kindness of strangers.",
		"Hasta la vista, baby.",
		"Soylent Green is people!",
		"Open the pod bay doors, please, HAL.",
		"Striker: Surely you can't be serious. ",
		"Rumack: I am serious...and don't call me Shirley.",
		"Yo, Adrian!",
		"Hello, gorgeous.",
		"Toga! Toga!",
		"Listen to them. Children of the night. What music they make.",
		"Oh, no, it wasn't the airplanes. It was Beauty killed the Beast.",
		"My precious.",
		"Attica! Attica!",
		"Sawyer, you're going out a youngster, but you've got to come back a star!",
		"Listen to me, mister. You're my knight in shining armor. Don't you forget it. You're going to get back on that horse, and I'm going to be right behind you, holding on tight, and away we're gonna go, go, go!",
		"Tell 'em to go out there with all they got and win just one for the Gipper.",
		"A martini. Shaken, not stirred.",
		"Who's on first.",
		"Cinderella story. Outta nowhere. A former greenskeeper, now, about to become the Masters champion. It looks like a mirac...It's in the hole! It's in the hole! It's in the hole!",
		"Life is a banquet, and most poor suckers are starving to death!",
		"I feel the need - the need for speed!",
		"Carpe diem. Seize the day, boys. Make your lives extraordinary.",
		"Snap out of it!",
		"My mother thanks you. My father thanks you. My sister thanks you. And I thank you.",
		"Nobody puts Baby in a corner.",
		"I'll get you, my pretty, and your little dog, too!",
		"I'm the king of the world!",
		"I have come here to chew bubble gum and kick ass, and I'm all out of bubble gum."
	};


	class X3DHStatus {
		public boolean holdsX3DHInit;
		public boolean haveOPk;

		public X3DHStatus() {
			holdsX3DHInit = false;
			haveOPk = false;
		}
	};

	X3DHStatus DR_message_holdsX3DHInit(byte[] message) {
		// all set to false by default
		X3DHStatus retObj = new X3DHStatus();

		// checks on length
		if (message.length<4) return retObj;

		// check protocol version
		if (message[0] != DR_v01) return retObj;

		// check message type: we must have a X3DH init message
		if (!((message[1]&X3DHInitFlagbit)==X3DHInitFlagbit)) return retObj;
		boolean payload_direct_encryption = ((message[1]&PayloadEncryptionFlagbit)==PayloadEncryptionFlagbit);

		/* check message length :
		 * message with payload not included (DR payload is a fixed 32 byte random seed)
		 * - header<3 bytes>, X3DH init packet, Ns+PN<4 bytes>, DHs<X<Curve>::keyLength>, Cipher message RandomSeed<32 bytes>, key auth tag<16 bytes> = <55 + X<Curve>::keyLengh + X3DH init size>
		 * message with payload included
		 * - header<3 bytes>, X3DH init packet, Ns+PN<4 bytes>, DHs<X<Curve>::keyLength>,  Payload<variable size>, key auth tag<16 bytes> = <23 + X<Curve>::keyLengh + X3DH init size>
		 */
		// X3DH init size = OPk_flag<1 byte> + selfIK<DSA<Curve>::keyLength> + EK<X<Curve>::keyLenght> + SPk id<4 bytes> + OPk id (if flag is 1)<4 bytes>
		switch (message[2]) {
			case C25519:
				if (message[3] == 0x00) { // no OPk in the X3DH init message
					// check size
					if (payload_direct_encryption) {
						if (message.length <= (23 + 32 + 5 + 32 + 32)) return retObj;
					} else {
						if (message.length != (55 + 32 + 5 + 32 + 32)) return retObj;
					}
				} else { // OPk present in the X3DH init message
					// check size
					if (payload_direct_encryption) {
						if (message.length <= (23 + 32 + 9 + 32 + 32)) return retObj;
					} else {
						if (message.length != (55 + 32 + 9 + 32 + 32)) return retObj;
					}
					retObj.haveOPk=true;
				}
				retObj.holdsX3DHInit = true;
				return retObj;
			case C448:
				if (message[3] == 0x00) { // no OPk in the X3DH init message
					// check size
					if (payload_direct_encryption) {
						if (message.length <= (23 + 56 + 5 + 57 + 56)) return retObj;
					} else {
						if (message.length != (55 + 56 + 5 + 57 + 56)) return retObj;
					}
				} else { // OPk present in the X3DH init message
					// check size
					if (payload_direct_encryption) {
						if (message.length <= (23 + 56 + 9 + 57 + 56)) return retObj;
					} else {
						if (message.length != (55 + 56 + 9 + 57 + 56)) return retObj;
					}
					retObj.haveOPk=true;
				}
				retObj.holdsX3DHInit = true;
				return retObj;

			default:
				return retObj;
		}
	}

	byte[] DR_message_extractX3DHInit(byte[] message) {
		// check if we actually have an X3DH init message to return
		if (DR_message_holdsX3DHInit(message).holdsX3DHInit == false) {
			return new byte[0];
		}

		// compute size
		int X3DH_length = 5;
		if (message[2] == C25519) {
			X3DH_length += 32 + 32;
		} else { // curve 448
			X3DH_length += 57 + 56;
		}

		if (message[3] == 0x01) { // there is an OPk id
			X3DH_length += 4;
		}

		// copy it in buffer
		return Arrays.copyOfRange(message, 3, 3+X3DH_length);
	}

	private final static char[] hexArray = "0123456789ABCDEF".toCharArray();
	public static String bytesToHex(byte[] bytes) {
		char[] hexChars = new char[bytes.length * 2];
		for ( int j = 0; j < bytes.length; j++ ) {
			int v = bytes[j] & 0xFF;
			hexChars[j * 2] = hexArray[v >>> 4];
			hexChars[j * 2 + 1] = hexArray[v & 0x0F];
		}
		return new String(hexChars);
	}

	boolean DR_message_payloadDirectEncrypt(byte[] message) {
		// checks on length to at least perform more checks
		if (message.length<4) return false;

		// check protocol version
		if (message[0] != DR_v01) return false;

		return ((message[1]&PayloadEncryptionFlagbit)==PayloadEncryptionFlagbit);
	}

}

/*
	LimeLimeTester.java
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
import java.util.UUID;
import java.io.File;
import java.util.Arrays;

public class LimeLimeTester {
	/* Test Scenario:
	 * - Create alice using LimeManager
	 * - Fail at creating alice again in the same DB
	 * - Delete Alice
	 * - Fail at deleting an unknown user
	 * - Create Alice again
	 * - using an other DB, create alice, success on local Storage but shall get a callback error
	 * - clean DB by deleting alice
	 */
	public static void user_management(LimeCurveId curveId, String dbBasename, String x3dhServerUrl, LimePostToX3DH postObj) {
		int expected_success = 0;
		int expected_fail = 0;

		LimeTesterUtils TesterUtils = new LimeTesterUtils();

		// Create a callback, this one will be used for all operations
		LimeStatusCallbackImpl statusCallback = new LimeStatusCallbackImpl();

		// Create db filenames and delete potential existing ones
		String curveIdString;
		if (curveId == LimeCurveId.C25519) {
			curveIdString = ".C25519";
		} else {
			curveIdString = ".C448";
		}
		String aliceDbFilename = "alice."+dbBasename+curveIdString+".sqlite3";
		String aliceDb2Filename = "alice."+dbBasename+curveIdString+".db2.sqlite3"; // this one will be used to generate another base
		File file = new File(aliceDbFilename);
		file.delete();

		// Create alice lime managers
		LimeManager aliceManager = new LimeManager(aliceDbFilename, postObj);

		// Create random device id for alice
		String AliceDeviceId = "alice."+UUID.randomUUID().toString();

		try {
			// Check alice is not already there
			assert(aliceManager.is_user(AliceDeviceId) == false);

			aliceManager.create_user(AliceDeviceId, x3dhServerUrl, curveId, 10, statusCallback);
			expected_success+= 1;
			assert (statusCallback.wait_for_success(expected_success));

			// Check alice is there
			assert(aliceManager.is_user(AliceDeviceId) == true);

			// Get alice x3dh server url
			assert(aliceManager.get_x3dhServerUrl(AliceDeviceId).equals(x3dhServerUrl));

			// Set the X3DH URL server to something else and check it worked
			aliceManager.set_x3dhServerUrl(AliceDeviceId, "https://testing.testing:12345");
			assert(aliceManager.get_x3dhServerUrl(AliceDeviceId).equals("https://testing.testing:12345"));
			// Force a reload of data from local storage just to be sure the modification was perform correctly
			aliceManager.nativeDestructor();
			aliceManager = null;
			aliceManager = new LimeManager(aliceDbFilename, postObj);
			assert(aliceManager.is_user(AliceDeviceId) == true); // Check again after LimeManager reload that Alice is in local storage
			assert(aliceManager.get_x3dhServerUrl(AliceDeviceId).equals("https://testing.testing:12345"));
			// Set it back to the regular one to be able to complete the test
			aliceManager.set_x3dhServerUrl(AliceDeviceId, x3dhServerUrl);
		}
		catch (LimeException e) {
			assert(false):"Got an unexpected exception during Lime user management test: "+e.getMessage();
		}

		boolean gotException = false;
		// Create again alice device, it shall fail with an exception
		try {
			aliceManager.create_user(AliceDeviceId, x3dhServerUrl, curveId, 10, statusCallback);
		}
		catch (LimeException e) {
			gotException = true;
		}

		assert (gotException == true);

		// Delete alice device, it shall work
		try {
			expected_success+= 1;
			aliceManager.delete_user(AliceDeviceId, statusCallback);
			assert (statusCallback.wait_for_success(expected_success));
		}
		catch (LimeException e) {
			assert(false):"Got an unexpected exception during Lime user management test: "+e.getMessage();
		}

		// Delete bob device, it is not in DB, it shall raise an exception
		gotException = false;
		try {
			aliceManager.delete_user("bob", statusCallback);
		}
		catch (LimeException e) {
			gotException = true;
		}
		assert (gotException == true):"Expected an exception when deleting an unknown device";

		try {
			// Create alice again
			aliceManager.create_user(AliceDeviceId, x3dhServerUrl, curveId, 10, statusCallback);
			expected_success+= 1;
			assert (statusCallback.wait_for_success(expected_success));

			// create a new db file and an other manager
			file = new File(aliceDb2Filename);
			file.delete();

			// Create alice lime managers
			LimeManager alice2Manager = new LimeManager(aliceDb2Filename, postObj);
			// Create alice again, it shall pass the local creation but fail on X3DH server.
			// So we will get a fail in callback but no exception
			alice2Manager.create_user(AliceDeviceId, x3dhServerUrl, curveId, 10, statusCallback);
			expected_fail+= 1;
			assert (statusCallback.wait_for_fail(expected_fail));

			// clean db: delete users
			expected_success+= 1;
			aliceManager.delete_user(AliceDeviceId, statusCallback);
			assert (statusCallback.wait_for_success(expected_success));
			assert (statusCallback.fail == expected_fail);

			// Do not forget do deallocate the native ressources
			aliceManager.nativeDestructor();
			alice2Manager.nativeDestructor();
			aliceManager = null;
		}
		catch (LimeException e) {
			assert(false):"Got an unexpected exception during Lime user management test: "+e.getMessage();
		}

		// Remove database files
		file = new File(aliceDbFilename);
		file.delete();
		file = new File(aliceDb2Filename);
		file.delete();
	}

	/* Test Scenario:
	 * - Alice and Bob(d1 and d2) register themselves on X3DH server
	 * - Alice send message to Bob (d1 and d2)
	 * - Alice send another message to Bob (d1 and d2)
	 * - Bob d1 respond to Alice(with bob d2 in copy)
	 * - Bob d2 respond to Alice(with bob d1 in copy)
	 * - Alice send another message to Bob(d1 and d2)
	 * - Delete Alice and Bob devices to leave distant server base clean
	 *
	 * At each message check that the X3DH init is present or not in the DR header
	 * Note: no asynchronous operation will start before the previous is over(callback returns)
	 */
	public static void basic(LimeCurveId curveId, String dbBasename, String x3dhServerUrl, LimePostToX3DH postObj) {
		int expected_success = 0;
		int expected_fail = 0;

		LimeTesterUtils TesterUtils = new LimeTesterUtils();

		// Create a callback, this one will be used for all operations
		LimeStatusCallbackImpl statusCallback = new LimeStatusCallbackImpl();

		// Create db filenames and delete potential existing ones
		String curveIdString;
		if (curveId == LimeCurveId.C25519) {
			curveIdString = ".C25519";
		} else {
			curveIdString = ".C448";
		}
		String aliceDbFilename = "alice."+dbBasename+curveIdString+".sqlite3";
		String bobDbFilename = "bob."+dbBasename+curveIdString+".sqlite3";
		File file = new File(aliceDbFilename);
		file.delete();
		file = new File(bobDbFilename);
		file.delete();

		try {
			// Create bob and alice lime managers
			LimeManager aliceManager = new LimeManager(aliceDbFilename, postObj);
			LimeManager bobManager = new LimeManager(bobDbFilename, postObj);

			// Create random device id for alice and bob
			String AliceDeviceId = "alice."+UUID.randomUUID().toString();
			String BobDeviceId1 = "bob.d1."+UUID.randomUUID().toString();
			String BobDeviceId2 = "bob.d2."+UUID.randomUUID().toString();

			aliceManager.create_user(AliceDeviceId, x3dhServerUrl, curveId, 10, statusCallback);
			bobManager.create_user(BobDeviceId1, x3dhServerUrl, curveId, 10, statusCallback);
			bobManager.create_user(BobDeviceId2, x3dhServerUrl, curveId, 10, statusCallback);
			expected_success+= 3;
			assert (statusCallback.wait_for_success(expected_success));
			assert (statusCallback.fail == expected_fail);

			// Alice sends a message to bob
			RecipientData[] recipients = new RecipientData[2]; // Bob has 2 devices
			recipients[0] = new RecipientData(BobDeviceId1);
			recipients[1] = new RecipientData(BobDeviceId2);

			LimeOutputBuffer cipherMessage = new LimeOutputBuffer();
			aliceManager.encrypt(AliceDeviceId, "bob", recipients, LimeTesterUtils.patterns[0].getBytes(), cipherMessage, statusCallback);
			expected_success+= 1;
			assert (statusCallback.wait_for_success(expected_success));

			// loop on outputs and decrypt with bob Manager
			for (RecipientData recipient : recipients) {
				LimeOutputBuffer decodedMessage = new LimeOutputBuffer();
				assert(TesterUtils.DR_message_holdsX3DHInit(recipient.DRmessage).holdsX3DHInit);  // new sessions created, they must convey X3DH init message
				assert(bobManager.decrypt(recipient.deviceId, "bob", AliceDeviceId, recipient.DRmessage, cipherMessage.buffer, decodedMessage) != LimePeerDeviceStatus.FAIL);
				String s = new String(decodedMessage.buffer);
				assert (s.equals(LimeTesterUtils.patterns[0])):"Decoded message is not the encoded one";
			}

			// encrypt another one, same recipients(we shall have no new X3DH session but still the X3DH init message)
			recipients = new RecipientData[2]; // Bob has 2 devices
			recipients[0] = new RecipientData(BobDeviceId1);
			recipients[1] = new RecipientData(BobDeviceId2);

			cipherMessage = new LimeOutputBuffer();
			aliceManager.encrypt(AliceDeviceId, "bob", recipients, LimeTesterUtils.patterns[1].getBytes(), cipherMessage, statusCallback);
			expected_success+= 1;
			assert (statusCallback.wait_for_success(expected_success));

			// loop on outputs and decrypt with bob Manager
			for (RecipientData recipient : recipients) {
				LimeOutputBuffer decodedMessage = new LimeOutputBuffer();
				assert(TesterUtils.DR_message_holdsX3DHInit(recipient.DRmessage).holdsX3DHInit);  // they must again convey X3DH init message
				assert(bobManager.decrypt(recipient.deviceId, "bob", AliceDeviceId, recipient.DRmessage, cipherMessage.buffer, decodedMessage) == LimePeerDeviceStatus.UNTRUSTED); // now we know alice
				String s = new String(decodedMessage.buffer);
				assert (s.equals(LimeTesterUtils.patterns[1])):"Decoded message is not the encoded one";
			}

			// bob.d1 reply to alice and copy bob.d2
			recipients = new RecipientData[2];
			recipients[0] = new RecipientData(AliceDeviceId);
			recipients[1] = new RecipientData(BobDeviceId2);

			cipherMessage = new LimeOutputBuffer();
			bobManager.encrypt(BobDeviceId1, "alice", recipients, LimeTesterUtils.patterns[2].getBytes(), cipherMessage, statusCallback);
			expected_success+= 1;
			assert (statusCallback.wait_for_success(expected_success));

			// decrypt it
			LimeOutputBuffer decodedMessage = new LimeOutputBuffer();
			assert(TesterUtils.DR_message_holdsX3DHInit(recipients[0].DRmessage).holdsX3DHInit == false);  // alice.d1 to bob.d1 already set up the DR Session, we shall not have any  X3DH message here
			assert(aliceManager.decrypt(recipients[0].deviceId, "alice", BobDeviceId1, recipients[0].DRmessage, cipherMessage.buffer, decodedMessage) == LimePeerDeviceStatus.UNTRUSTED); // we know alice
			String s = new String(decodedMessage.buffer);
			assert (s.equals(LimeTesterUtils.patterns[2])):"Decoded message is not the encoded one";

			decodedMessage = new LimeOutputBuffer();
			assert(TesterUtils.DR_message_holdsX3DHInit(recipients[1].DRmessage).holdsX3DHInit);   // bob.d1 to bob.d2 is a new session, we must have a X3DH message here
			assert(bobManager.decrypt(recipients[1].deviceId, "alice", BobDeviceId1, recipients[1].DRmessage, cipherMessage.buffer, decodedMessage) != LimePeerDeviceStatus.FAIL); // as we use the same manager for bob.d1 and bob.d2, the are untrusted but it shall be unknown, just check it does not fail
			s = new String(decodedMessage.buffer);
			assert (s.equals(LimeTesterUtils.patterns[2])):"Decoded message is not the encoded one";

			// Now do bob.d2 to alice and bob.d1 every one has an open session towards everyone
			recipients = new RecipientData[2];
			recipients[0] = new RecipientData(AliceDeviceId);
			recipients[1] = new RecipientData(BobDeviceId1);

			cipherMessage = new LimeOutputBuffer();
			bobManager.encrypt(BobDeviceId2, "alice", recipients, LimeTesterUtils.patterns[3].getBytes(), cipherMessage, statusCallback);
			expected_success+= 1;
			assert (statusCallback.wait_for_success(expected_success));

			// decrypt it
			decodedMessage = new LimeOutputBuffer();
			assert(TesterUtils.DR_message_holdsX3DHInit(recipients[0].DRmessage).holdsX3DHInit == false);  // alice.d1 to bob.d1 already set up the DR Session, we shall not have any  X3DH message here
			assert(aliceManager.decrypt(recipients[0].deviceId, "alice", BobDeviceId2, recipients[0].DRmessage, cipherMessage.buffer, decodedMessage) == LimePeerDeviceStatus.UNTRUSTED); // we know alice
			s = new String(decodedMessage.buffer);
			assert (s.equals(LimeTesterUtils.patterns[3])):"Decoded message is not the encoded one";

			decodedMessage = new LimeOutputBuffer();
			assert(TesterUtils.DR_message_holdsX3DHInit(recipients[1].DRmessage).holdsX3DHInit == false);   // bob.d2 to bob.d1 is already open so no X3DH message here
			assert(bobManager.decrypt(recipients[1].deviceId, "alice", BobDeviceId2, recipients[1].DRmessage, cipherMessage.buffer, decodedMessage) == LimePeerDeviceStatus.UNTRUSTED); // Bob's d1 knows bob's d2
			s = new String(decodedMessage.buffer);
			assert (s.equals(LimeTesterUtils.patterns[3])):"Decoded message is not the encoded one";

			// encrypt another one from alice to bob.d1 and .d2, it must not send X3DH init anymore
			recipients = new RecipientData[2]; // Bob has 2 devices
			recipients[0] = new RecipientData(BobDeviceId1);
			recipients[1] = new RecipientData(BobDeviceId2);

			cipherMessage = new LimeOutputBuffer();
			aliceManager.encrypt(AliceDeviceId, "bob", recipients, LimeTesterUtils.patterns[4].getBytes(), cipherMessage, statusCallback);
			expected_success+= 1;
			assert (statusCallback.wait_for_success(expected_success));

			// loop on outputs and decrypt with bob Manager
			for (RecipientData recipient : recipients) {
				decodedMessage = new LimeOutputBuffer();
				assert(TesterUtils.DR_message_holdsX3DHInit(recipient.DRmessage).holdsX3DHInit == false);
				assert(bobManager.decrypt(recipient.deviceId, "bob", AliceDeviceId, recipient.DRmessage, cipherMessage.buffer, decodedMessage) == LimePeerDeviceStatus.UNTRUSTED); // now we know alice
				s = new String(decodedMessage.buffer);
				assert (s.equals(LimeTesterUtils.patterns[4])):"Decoded message is not the encoded one";
			}

			// clean db: delete users
			expected_success+= 3;
			aliceManager.delete_user(AliceDeviceId, statusCallback);
			bobManager.delete_user(BobDeviceId1, statusCallback);
			bobManager.delete_user(BobDeviceId2, statusCallback);
			assert (statusCallback.wait_for_success(expected_success));
			assert (statusCallback.fail == expected_fail);

			// Do not forget do deallocate the native ressources
			aliceManager.nativeDestructor();
			bobManager.nativeDestructor();
			aliceManager = null;
			bobManager = null;
		}
		catch (LimeException e) {
			assert(false):"Got an unexpected exception during Lime Basic test : "+e.getMessage();
		}

		// Remove database files
		file = new File(aliceDbFilename);
		file.delete();
		file = new File(bobDbFilename);
		file.delete();
	}

	 /* A simple test with alice having 1 device and bob 2
	 * - Alice and Bob(d1 and d2) register themselves on X3DH server
	 * - Alice send another message to Bob non existing device d3, it shall fail without exception
	 * - Alice send another message to Bob (d1 and d2) and try to send to an non existing device d3, d1 and d2 shall work
	 * - Delete Alice and Bob devices to leave distant server base clean
	 *
	 * Note: no asynchronous operation will start before the previous is over(callback returns)
	 */
	public static void user_not_found(LimeCurveId curveId, String dbBasename, String x3dhServerUrl, LimePostToX3DH postObj) {
		int expected_success = 0;
		int expected_fail = 0;

		LimeTesterUtils TesterUtils = new LimeTesterUtils();

		// Create a callback, this one will be used for all operations
		LimeStatusCallbackImpl statusCallback = new LimeStatusCallbackImpl();

		// Create db filenames and delete potential existing ones
		String curveIdString;
		if (curveId == LimeCurveId.C25519) {
			curveIdString = ".C25519";
		} else {
			curveIdString = ".C448";
		}
		String aliceDbFilename = "alice."+dbBasename+curveIdString+".sqlite3";
		String bobDbFilename = "bob."+dbBasename+curveIdString+".sqlite3";
		File file = new File(aliceDbFilename);
		file.delete();
		file = new File(bobDbFilename);
		file.delete();

		try {
			// Create bob and alice lime managers
			LimeManager aliceManager = new LimeManager(aliceDbFilename, postObj);
			LimeManager bobManager = new LimeManager(bobDbFilename, postObj);

			// Create random device id for alice and bob
			String AliceDeviceId = "alice."+UUID.randomUUID().toString();
			String BobDeviceId1 = "bob.d1."+UUID.randomUUID().toString();
			String BobDeviceId2 = "bob.d2."+UUID.randomUUID().toString();
			String BobDeviceId3 = "bob.d3."+UUID.randomUUID().toString(); // we just get a random device name but we will not create it

			aliceManager.create_user(AliceDeviceId, x3dhServerUrl, curveId, 10, statusCallback);
			bobManager.create_user(BobDeviceId1, x3dhServerUrl, curveId, 10, statusCallback);
			bobManager.create_user(BobDeviceId2, x3dhServerUrl, curveId, 10, statusCallback);
			expected_success+= 3;
			assert (statusCallback.wait_for_success(expected_success));
			assert (statusCallback.fail == expected_fail);

			// Alice sends a message to bob.d3 devices, non existent
			RecipientData[] recipients = new RecipientData[1];
			recipients[0] = new RecipientData(BobDeviceId3);

			LimeOutputBuffer cipherMessage = new LimeOutputBuffer();
			aliceManager.encrypt(AliceDeviceId, "bob", recipients, LimeTesterUtils.patterns[0].getBytes(), cipherMessage, statusCallback);
			assert (statusCallback.wait_for_fail(++expected_fail)); // no recipients got a message, callback will return a fail
			assert (recipients[0].getPeerStatus() == LimePeerDeviceStatus.FAIL); // the device is unknown, so it shall fail

			// Alice sends a message to bob 3 devices, one is non existent
			recipients = new RecipientData[3];
			recipients[0] = new RecipientData(BobDeviceId1);
			recipients[1] = new RecipientData(BobDeviceId2);
			recipients[2] = new RecipientData(BobDeviceId3);

			cipherMessage = new LimeOutputBuffer();
			aliceManager.encrypt(AliceDeviceId, "bob", recipients, LimeTesterUtils.patterns[0].getBytes(), cipherMessage, statusCallback);
			expected_success+= 1;
			assert (statusCallback.wait_for_success(expected_success));

			// decrypt with bob Manager
			// bob.d1
			LimeOutputBuffer decodedMessage = new LimeOutputBuffer();
			assert(TesterUtils.DR_message_holdsX3DHInit(recipients[0].DRmessage).holdsX3DHInit);  // new sessions created, they must convey X3DH init message
			assert(bobManager.decrypt(recipients[0].deviceId, "bob", AliceDeviceId, recipients[0].DRmessage, cipherMessage.buffer, decodedMessage) != LimePeerDeviceStatus.FAIL);
			String s = new String(decodedMessage.buffer);
			assert (s.equals(LimeTesterUtils.patterns[0])):"Decoded message is not the encoded one";

			// bob.d2
			decodedMessage = new LimeOutputBuffer();
			assert(TesterUtils.DR_message_holdsX3DHInit(recipients[1].DRmessage).holdsX3DHInit);  // new sessions created, they must convey X3DH init message
			assert(bobManager.decrypt(recipients[1].deviceId, "bob", AliceDeviceId, recipients[1].DRmessage, cipherMessage.buffer, decodedMessage) != LimePeerDeviceStatus.FAIL);
			s = new String(decodedMessage.buffer);
			assert (s.equals(LimeTesterUtils.patterns[0])):"Decoded message is not the encoded one";

			// bob.d3
			assert (recipients[2].getPeerStatus() == LimePeerDeviceStatus.FAIL); // the device is unknown, so it shall fail
			assert (recipients[2].DRmessage.length == 0); // encrypt produced no DRmessage for non existent device

			// clean db: delete users
			expected_success+= 3;
			aliceManager.delete_user(AliceDeviceId, statusCallback);
			bobManager.delete_user(BobDeviceId1, statusCallback);
			bobManager.delete_user(BobDeviceId2, statusCallback);
			assert (statusCallback.wait_for_success(expected_success));
			assert (statusCallback.fail == expected_fail);

			// Do not forget do deallocate the native ressources
			aliceManager.nativeDestructor();
			bobManager.nativeDestructor();
			aliceManager = null;
			bobManager = null;
		}
		catch (LimeException e) {
			assert(false):"Got an unexpected exception during Lime user not found test : "+e.getMessage();
		}

		// Remove database files
		file = new File(aliceDbFilename);
		file.delete();
		file = new File(bobDbFilename);
		file.delete();
	}

	/* Test Scenario :
	 * alice.d1 will encrypt to bob.d1, bob.d2, bob.d3, bob.d4
	 * - message burst from alice.d1 -> bob.d1
	 * - wait for callbacks. alice.d1 hold session toward d1 only
	 * then burst encrypt to:
	 * - bob.d1, bob.d2 : test enqueing if a part of recipients are not available
	 * - bob.d1 : test going through if we can process it without calling X3DH server
	 * - bob.d2 : test enqueue and have session ready when processed
	 * - bob.d3 : test enqueue and must start an asynchronous X3DH request when back
	 * - bob.d4 : test enqueue and must start an asynchronous X3DH request when back
	 */
	public static void x3dh_multidev_operation_queue(LimeCurveId curveId, String dbBasename, String x3dhServerUrl, LimePostToX3DH postObj) {
		int expected_success = 0;
		int expected_fail = 0;

		LimeTesterUtils TesterUtils = new LimeTesterUtils();

		// Create a callback, this one will be used for all operations
		LimeStatusCallbackImpl statusCallback = new LimeStatusCallbackImpl();

		// Create db filenames and delete potential existing ones
		String curveIdString;
		if (curveId == LimeCurveId.C25519) {
			curveIdString = ".C25519";
		} else {
			curveIdString = ".C448";
		}
		String aliceDbFilename = "alice."+dbBasename+curveIdString+".sqlite3";
		String bobDbFilename = "bob."+dbBasename+curveIdString+".sqlite3";
		File file = new File(aliceDbFilename);
		file.delete();
		file = new File(bobDbFilename);
		file.delete();

		try {
			// Create bob and alice lime managers
			LimeManager aliceManager = new LimeManager(aliceDbFilename, postObj);
			LimeManager bobManager = new LimeManager(bobDbFilename, postObj);

			// Create random device id for alice and bob
			String AliceDeviceId = "alice."+UUID.randomUUID().toString();
			String BobDeviceId1 = "bob.d1."+UUID.randomUUID().toString();
			String BobDeviceId2 = "bob.d2."+UUID.randomUUID().toString();
			String BobDeviceId3 = "bob.d3."+UUID.randomUUID().toString();
			String BobDeviceId4 = "bob.d4."+UUID.randomUUID().toString();

			aliceManager.create_user(AliceDeviceId, x3dhServerUrl, curveId, 10, statusCallback);
			bobManager.create_user(BobDeviceId1, x3dhServerUrl, curveId, 10, statusCallback);
			bobManager.create_user(BobDeviceId2, x3dhServerUrl, curveId, 10, statusCallback);
			bobManager.create_user(BobDeviceId3, x3dhServerUrl, curveId, 10, statusCallback);
			bobManager.create_user(BobDeviceId4, x3dhServerUrl, curveId, 10, statusCallback);
			expected_success+= 5;
			assert (statusCallback.wait_for_success(expected_success));
			assert (statusCallback.fail == expected_fail);

			// Alice encrypts a burst of messages to bob.d1
			int messageBurstSize = 10;
			RecipientData[][] recipients = new RecipientData[messageBurstSize][1];
			LimeOutputBuffer[] cipherMessage = new LimeOutputBuffer[messageBurstSize];

			for (int i=0; i<messageBurstSize; i++) {
				recipients[i][0] = new RecipientData(BobDeviceId1);
				cipherMessage[i] = new LimeOutputBuffer();
			}

			for (int i=0; i<messageBurstSize; i++) {
				aliceManager.encrypt(AliceDeviceId, "bob", recipients[i], LimeTesterUtils.patterns[i].getBytes(), cipherMessage[i], statusCallback);
				expected_success+= 1;
			}
			assert (statusCallback.wait_for_success(expected_success));

			byte[] X3DH_initMessageBuffer = new byte[0];

			// loop on cipher message and decrypt them bob Manager
			for (int i=0; i<messageBurstSize; i++) {
				LimeOutputBuffer decodedMessage = new LimeOutputBuffer();
				assert(TesterUtils.DR_message_holdsX3DHInit(recipients[i][0].DRmessage).holdsX3DHInit);  // new sessions created, they must convey X3DH init message
				if (i==0) { // first message of the burst, extract and store the X3DH init message
					X3DH_initMessageBuffer = TesterUtils.DR_message_extractX3DHInit(recipients[i][0].DRmessage);
				} else { // following message of the burst, extract X3DH init message and compare it to the first one, they must be the same, we shall not create new sessions
					byte[] X3DH_initMessageBuffer_next = TesterUtils.DR_message_extractX3DHInit(recipients[i][0].DRmessage);
					assert(Arrays.equals(X3DH_initMessageBuffer, X3DH_initMessageBuffer_next));
				}
				assert(bobManager.decrypt(recipients[i][0].deviceId, "bob", AliceDeviceId, recipients[i][0].DRmessage, cipherMessage[i].buffer, decodedMessage) != LimePeerDeviceStatus.FAIL);
				String s = new String(decodedMessage.buffer);
				assert (s.equals(LimeTesterUtils.patterns[i])):"Decoded message is not the encoded one";
			}

			recipients = new RecipientData[5][];
			cipherMessage = new LimeOutputBuffer[5];
			for (int i=0; i<5; i++) {
				cipherMessage[i] = new LimeOutputBuffer();
			}
			// now alice will request encryption of message without waiting for callback to:
			//  bob.d1,bob.d2 -> this one shall trigger a X3DH request to acquire bob.d2 key bundle
			recipients[0] = new RecipientData[2];
			recipients[0][0] = new RecipientData(BobDeviceId1);
			recipients[0][1] = new RecipientData(BobDeviceId2);
			//  bob.d1 -> this one shall be just be processed so callback will be called before even returning from encrypt call
			recipients[1] = new RecipientData[1];
			recipients[1][0] = new RecipientData(BobDeviceId1);
			//  bob.d2 -> this one shall be queued and processed when d1,d2 is done but it won't trigger an X3DH request
			recipients[2] = new RecipientData[1];
			recipients[2][0] = new RecipientData(BobDeviceId2);
			//  bob.d3 -> this one shall be queued and processed when previous one is done, it will trigger an X3DH request to get d3 key bundle
			recipients[3] = new RecipientData[1];
			recipients[3][0] = new RecipientData(BobDeviceId3);
			//  bob.d4 -> this one shall be queued and processed when previous one is done, it will trigger an X3DH request to get d4 key bundle
			recipients[4] = new RecipientData[1];
			recipients[4][0] = new RecipientData(BobDeviceId4);

			for (int i=0; i<5; i++) {
				aliceManager.encrypt(AliceDeviceId, "bob", recipients[i], LimeTesterUtils.patterns[messageBurstSize+i].getBytes(), cipherMessage[i], statusCallback);
				expected_success+= 1;
			}
			assert (statusCallback.wait_for_success(expected_success));

			// Check the encryption queue is ok
			// recipients holds:
			// recipients[0] -> bob.d1, bob.d2
			// recipents[1] -> bob.d1
			// recipients[2] -> bob.d2
			// Check on these that the X3DH init message are matching (we didn't create a second session an encryption was queued correctly)
			// recipients[0][0] and recipients[1][0]
			byte[] X3DH_initMessageBuffer1 = TesterUtils.DR_message_extractX3DHInit(recipients[0][0].DRmessage);
			byte[] X3DH_initMessageBuffer2 = TesterUtils.DR_message_extractX3DHInit(recipients[1][0].DRmessage);
			assert(X3DH_initMessageBuffer1.length>0); // check we actually found a X3DH buffer
			assert(Arrays.equals(X3DH_initMessageBuffer1, X3DH_initMessageBuffer2));
			// recipients[0][1] and recipients[2][0]
			X3DH_initMessageBuffer1 = TesterUtils.DR_message_extractX3DHInit(recipients[0][1].DRmessage);
			X3DH_initMessageBuffer2 = TesterUtils.DR_message_extractX3DHInit(recipients[2][0].DRmessage);
			assert(X3DH_initMessageBuffer1.length>0); // check we actually found a X3DH buffer
			assert(Arrays.equals(X3DH_initMessageBuffer1, X3DH_initMessageBuffer2));

			// decrypt and match original message
			// in recipient[0] we have a message encrypted for bob.d1 and bob.d2
			LimeOutputBuffer decodedMessage = new LimeOutputBuffer();
			assert(bobManager.decrypt(BobDeviceId1, "bob", AliceDeviceId, recipients[0][0].DRmessage, cipherMessage[0].buffer, decodedMessage) != LimePeerDeviceStatus.FAIL);
			String s = new String(decodedMessage.buffer);
			assert (s.equals(LimeTesterUtils.patterns[messageBurstSize])):"Decoded message is not the encoded one";
			decodedMessage = new LimeOutputBuffer();
			assert(bobManager.decrypt(BobDeviceId2, "bob", AliceDeviceId, recipients[0][1].DRmessage, cipherMessage[0].buffer, decodedMessage) != LimePeerDeviceStatus.FAIL);
			s = new String(decodedMessage.buffer);
			assert (s.equals(LimeTesterUtils.patterns[messageBurstSize])):"Decoded message is not the encoded one";

			// in recipient[1] we have a message encrypted to bob.d1
			decodedMessage = new LimeOutputBuffer();
			assert(bobManager.decrypt(BobDeviceId1, "bob", AliceDeviceId, recipients[1][0].DRmessage, cipherMessage[1].buffer, decodedMessage) != LimePeerDeviceStatus.FAIL);
			s = new String(decodedMessage.buffer);
			assert (s.equals(LimeTesterUtils.patterns[messageBurstSize+1])):"Decoded message is not the encoded one";

			// in recipient[2] we have a message encrypted to bob.d2
			decodedMessage = new LimeOutputBuffer();
			assert(bobManager.decrypt(BobDeviceId2, "bob", AliceDeviceId, recipients[2][0].DRmessage, cipherMessage[2].buffer, decodedMessage) != LimePeerDeviceStatus.FAIL);
			s = new String(decodedMessage.buffer);
			assert (s.equals(LimeTesterUtils.patterns[messageBurstSize+2])):"Decoded message is not the encoded one";

			// in recipient[3] we have a message encrypted to bob.d3
			decodedMessage = new LimeOutputBuffer();
			assert(bobManager.decrypt(BobDeviceId3, "bob", AliceDeviceId, recipients[3][0].DRmessage, cipherMessage[3].buffer, decodedMessage) != LimePeerDeviceStatus.FAIL);
			s = new String(decodedMessage.buffer);
			assert (s.equals(LimeTesterUtils.patterns[messageBurstSize+3])):"Decoded message is not the encoded one";

			// in recipient[4] we have a message encrypted to bob.d4
			decodedMessage = new LimeOutputBuffer();
			assert(bobManager.decrypt(BobDeviceId4, "bob", AliceDeviceId, recipients[4][0].DRmessage, cipherMessage[4].buffer, decodedMessage) != LimePeerDeviceStatus.FAIL);
			s = new String(decodedMessage.buffer);
			assert (s.equals(LimeTesterUtils.patterns[messageBurstSize+4])):"Decoded message is not the encoded one";

			// clean db: delete users
			expected_success+= 5;
			aliceManager.delete_user(AliceDeviceId, statusCallback);
			bobManager.delete_user(BobDeviceId1, statusCallback);
			bobManager.delete_user(BobDeviceId2, statusCallback);
			bobManager.delete_user(BobDeviceId3, statusCallback);
			bobManager.delete_user(BobDeviceId4, statusCallback);
			assert (statusCallback.wait_for_success(expected_success));
			assert (statusCallback.fail == expected_fail);

			// Do not forget do deallocate the native ressources
			aliceManager.nativeDestructor();
			bobManager.nativeDestructor();
			aliceManager = null;
			bobManager = null;
		}
		catch (LimeException e) {
			assert(false):"Got an unexpected exception during Multidev operation queue test : "+e.getMessage();
		}

		// Remove database files
		file = new File(aliceDbFilename);
		file.delete();
		file = new File(bobDbFilename);
		file.delete();
	}

	/*
	 * Test scenario :
	 * - check the Ik in pattern_db is retrieved as expected
	 * - try asking for an unknown user, we shall get an exception
	 */
	public static void getSelfIk(LimeCurveId curveId, String dbFilename, LimePostToX3DH postObj, byte[] patternIk) {
		LimeManager aliceManager = new LimeManager(dbFilename, postObj);
		try  {
			// retrieve alice identity key
			LimeOutputBuffer Ik = new LimeOutputBuffer();
			aliceManager.get_selfIdentityKey("alice", Ik);

			assert(Arrays.equals(Ik.buffer, patternIk));
		} catch (LimeException e) {
			aliceManager.nativeDestructor();
			assert(false):"Got an unexpected exception during getSelfIk test : "+e.getMessage();
			return;
		}

		// try to get the Ik of a user not in there, we shall get an exception
		try {
			LimeOutputBuffer Ik = new LimeOutputBuffer();
			aliceManager.get_selfIdentityKey("bob", Ik);
		} catch (LimeException e) {
			aliceManager.nativeDestructor();
			// just swallow it
			return;
		}
		aliceManager.nativeDestructor();
		assert(false):"Get the Ik of a user not in local Storage didn't throw an exception";
	}

	/* Test scenario
	 * - Create managers and DB for alice, bob, carol and dave
	 * - Get their Identity key and gives alice (apply mutual action with alice's identity key on bob's, carol's and dave's manager)
	 *     - bob's keys as trusted
	 *     - set carol's key as trusted and then untrusted so it is in Alice local storage as untrusted
	 *     - set dave's key as untrusted, as it was not konwn before, it shall not be registered at all in Alice local storage
	 * - Alice encrypts a message to bob, carol and dave. Check that the peerDevice status given after encryption are respectively: trusted, untrusted, unknown
	 * - Recipients decrypt Alice message and check we have the expected return values:
	 *   - Bob: trusted
	 *   - Carol: untrusted
	 *   - Dave: unknown
	 * - Alice encrypts a second message to bob, carol and dave. Check that the peerDevice status given after encryption are respectively: trusted, untrusted, untrusted
	 * - Recipients decrypt Alice message and check we have the expected return values:
	 *   - Bob: trusted
	 *   - Carol: untrusted
	 *   - Dave: untrusted
	 */
	public static void peerDeviceStatus(LimeCurveId curveId, String dbBasename,  String x3dhServerUrl, LimePostToX3DH postObj) {
		int expected_success = 0;
		int expected_fail = 0;

		LimeTesterUtils TesterUtils = new LimeTesterUtils();

		// Create a callback, this one will be used for all operations
		LimeStatusCallbackImpl statusCallback = new LimeStatusCallbackImpl();

		// Create db filenames and delete potential existing ones
		String curveIdString;
		if (curveId == LimeCurveId.C25519) {
			curveIdString = ".C25519";
		} else {
			curveIdString = ".C448";
		}
		String aliceDbFilename = "alice."+dbBasename+curveIdString+".sqlite3";
		String bobDbFilename = "bob."+dbBasename+curveIdString+".sqlite3";
		String carolDbFilename = "carol."+dbBasename+curveIdString+".sqlite3";
		String daveDbFilename = "dave."+dbBasename+curveIdString+".sqlite3";
		File file = new File(aliceDbFilename);
		file.delete();
		file = new File(bobDbFilename);
		file.delete();
		file = new File(carolDbFilename);
		file.delete();
		file = new File(daveDbFilename);
		file.delete();

		try {
			// Create random device ids
			String aliceDeviceId = "alice."+UUID.randomUUID().toString();
			String bobDeviceId = "bob."+UUID.randomUUID().toString();
			String carolDeviceId = "carol."+UUID.randomUUID().toString();
			String daveDeviceId = "dave."+UUID.randomUUID().toString();

			// create Manager and devices
			LimeManager aliceManager = new LimeManager(aliceDbFilename, postObj);
			aliceManager.create_user(aliceDeviceId, x3dhServerUrl, curveId, 10, statusCallback);

			LimeManager bobManager = new LimeManager(bobDbFilename, postObj);
			bobManager.create_user(bobDeviceId, x3dhServerUrl, curveId, 10, statusCallback);

			LimeManager carolManager = new LimeManager(carolDbFilename, postObj);
			carolManager.create_user(carolDeviceId, x3dhServerUrl, curveId, 10, statusCallback);

			LimeManager daveManager = new LimeManager(daveDbFilename, postObj);
			daveManager.create_user(daveDeviceId, x3dhServerUrl, curveId, 10, statusCallback);

			expected_success += 4;
			assert (statusCallback.wait_for_success(expected_success));

			// retrieve their respective Ik
			LimeOutputBuffer aliceIk = new LimeOutputBuffer();
			LimeOutputBuffer bobIk = new LimeOutputBuffer();
			LimeOutputBuffer carolIk = new LimeOutputBuffer();
			LimeOutputBuffer daveIk = new LimeOutputBuffer();
			aliceManager.get_selfIdentityKey(aliceDeviceId, aliceIk);
			bobManager.get_selfIdentityKey(bobDeviceId, bobIk);
			carolManager.get_selfIdentityKey(carolDeviceId, carolIk);
			daveManager.get_selfIdentityKey(daveDeviceId, daveIk);

			// exchange trust between alice and bob
			bobManager.set_peerDeviceStatus(aliceDeviceId, aliceIk.buffer, LimePeerDeviceStatus.TRUSTED);
			aliceManager.set_peerDeviceStatus(bobDeviceId, bobIk.buffer, LimePeerDeviceStatus.TRUSTED);
			assert(bobManager.get_peerDeviceStatus(aliceDeviceId) == LimePeerDeviceStatus.TRUSTED);
			assert(aliceManager.get_peerDeviceStatus(bobDeviceId) == LimePeerDeviceStatus.TRUSTED);

			// alice and carol gets trust and back to not trust so the Ik gets registered in their local storage
			carolManager.set_peerDeviceStatus(aliceDeviceId, aliceIk.buffer, LimePeerDeviceStatus.TRUSTED);
			aliceManager.set_peerDeviceStatus(carolDeviceId, carolIk.buffer, LimePeerDeviceStatus.TRUSTED);
			carolManager.set_peerDeviceStatus(aliceDeviceId, aliceIk.buffer, LimePeerDeviceStatus.UNTRUSTED);
			aliceManager.set_peerDeviceStatus(carolDeviceId, carolIk.buffer, LimePeerDeviceStatus.UNTRUSTED);
			assert(carolManager.get_peerDeviceStatus(aliceDeviceId) == LimePeerDeviceStatus.UNTRUSTED);
			assert(aliceManager.get_peerDeviceStatus(carolDeviceId) == LimePeerDeviceStatus.UNTRUSTED);

			// alice and dave gets just an untrusted setting, as they do not know each other, it shall not affect their respective local storage and they would remain unknown
			daveManager.set_peerDeviceStatus(aliceDeviceId, aliceIk.buffer, LimePeerDeviceStatus.UNTRUSTED);
			aliceManager.set_peerDeviceStatus(daveDeviceId, daveIk.buffer, LimePeerDeviceStatus.UNTRUSTED);
			assert(daveManager.get_peerDeviceStatus(aliceDeviceId) == LimePeerDeviceStatus.UNKNOWN);
			assert(aliceManager.get_peerDeviceStatus(daveDeviceId) == LimePeerDeviceStatus.UNKNOWN);


			// Alice encrypts a message for Bob, Carol and Dave
			RecipientData[] recipients = new RecipientData[3];
			recipients[0] = new RecipientData(bobDeviceId);
			recipients[1] = new RecipientData(carolDeviceId);
			recipients[2] = new RecipientData(daveDeviceId);

			LimeOutputBuffer cipherMessage = new LimeOutputBuffer();
			aliceManager.encrypt(aliceDeviceId, "my friends group", recipients, LimeTesterUtils.patterns[0].getBytes(), cipherMessage, statusCallback);
			expected_success+= 1;
			assert (statusCallback.wait_for_success(expected_success));

			assert(recipients[0].getPeerStatus() == LimePeerDeviceStatus.TRUSTED); // recipient 0 is Bob: trusted
			assert(recipients[1].getPeerStatus() == LimePeerDeviceStatus.UNTRUSTED); // recipient 1 is Carol: untrusted
			assert(recipients[2].getPeerStatus() == LimePeerDeviceStatus.UNKNOWN); // recipient 2 is Dave: unknown

			// recipients decrypt
			LimeOutputBuffer decodedMessage = new LimeOutputBuffer();
			// bob shall return trusted
			assert(bobManager.decrypt(bobDeviceId, "my friends group", aliceDeviceId, recipients[0].DRmessage, cipherMessage.buffer, decodedMessage) == LimePeerDeviceStatus.TRUSTED);
			String s = new String(decodedMessage.buffer);
			assert (s.equals(LimeTesterUtils.patterns[0])):"Decoded message is not the encoded one";

			decodedMessage = new LimeOutputBuffer();
			// carol shall return untrusted
			assert(carolManager.decrypt(carolDeviceId, "my friends group", aliceDeviceId, recipients[1].DRmessage, cipherMessage.buffer, decodedMessage) == LimePeerDeviceStatus.UNTRUSTED);
			s = new String(decodedMessage.buffer);
			assert (s.equals(LimeTesterUtils.patterns[0])):"Decoded message is not the encoded one";

			decodedMessage = new LimeOutputBuffer();
			// dave shall return unknown
			assert(daveManager.decrypt(daveDeviceId, "my friends group", aliceDeviceId, recipients[2].DRmessage, cipherMessage.buffer, decodedMessage) == LimePeerDeviceStatus.UNKNOWN);
			s = new String(decodedMessage.buffer);
			assert (s.equals(LimeTesterUtils.patterns[0])):"Decoded message is not the encoded one";

			// Alice encrypts a second message for Bob, Carol and Dave
			recipients = new RecipientData[3];
			recipients[0] = new RecipientData(bobDeviceId);
			recipients[1] = new RecipientData(carolDeviceId);
			recipients[2] = new RecipientData(daveDeviceId);

			cipherMessage = new LimeOutputBuffer();
			aliceManager.encrypt(aliceDeviceId, "my friends group", recipients, LimeTesterUtils.patterns[1].getBytes(), cipherMessage, statusCallback);
			expected_success+= 1;
			assert (statusCallback.wait_for_success(expected_success));

			assert(recipients[0].getPeerStatus() == LimePeerDeviceStatus.TRUSTED); // recipient 0 is Bob: trusted
			assert(recipients[1].getPeerStatus() == LimePeerDeviceStatus.UNTRUSTED); // recipient 1 is Carol: untrusted
			assert(recipients[2].getPeerStatus() == LimePeerDeviceStatus.UNTRUSTED); // recipient 2 is Dave: untrusted

			// recipients decrypt
			decodedMessage = new LimeOutputBuffer();
			// bob shall return trusted
			assert(bobManager.decrypt(bobDeviceId, "my friends group", aliceDeviceId, recipients[0].DRmessage, cipherMessage.buffer, decodedMessage) == LimePeerDeviceStatus.TRUSTED);
			s = new String(decodedMessage.buffer);
			assert (s.equals(LimeTesterUtils.patterns[1])):"Decoded message is not the encoded one";

			decodedMessage = new LimeOutputBuffer();
			// carol shall return untrusted
			assert(carolManager.decrypt(carolDeviceId, "my friends group", aliceDeviceId, recipients[1].DRmessage, cipherMessage.buffer, decodedMessage) == LimePeerDeviceStatus.UNTRUSTED);
			s = new String(decodedMessage.buffer);
			assert (s.equals(LimeTesterUtils.patterns[1])):"Decoded message is not the encoded one";

			decodedMessage = new LimeOutputBuffer();
			// dave shall return untrusted
			assert(daveManager.decrypt(daveDeviceId, "my friends group", aliceDeviceId, recipients[2].DRmessage, cipherMessage.buffer, decodedMessage) == LimePeerDeviceStatus.UNTRUSTED);
			s = new String(decodedMessage.buffer);
			assert (s.equals(LimeTesterUtils.patterns[1])):"Decoded message is not the encoded one";

			// Cleaning
			expected_success+= 4;
			aliceManager.delete_user(aliceDeviceId, statusCallback);
			bobManager.delete_user(bobDeviceId, statusCallback);
			carolManager.delete_user(carolDeviceId, statusCallback);
			daveManager.delete_user(daveDeviceId, statusCallback);
			assert (statusCallback.wait_for_success(expected_success));
			assert (statusCallback.fail == expected_fail);

			// Do not forget do deallocate the native ressources
			aliceManager.nativeDestructor();
			bobManager.nativeDestructor();
			carolManager.nativeDestructor();
			daveManager.nativeDestructor();
			aliceManager = null;
			bobManager = null;
			carolManager = null;
			daveManager = null;
		}
		catch (LimeException e) {
			assert(false):"Got an unexpected exception during Multidev operation queue test : "+e.getMessage();
		}

		// Remove database files
		file = new File(aliceDbFilename);
		file.delete();
		file = new File(bobDbFilename);
		file.delete();
		file = new File(carolDbFilename);
		file.delete();
		file = new File(daveDbFilename);
		file.delete();

	}

	/*
	 * Scenario: Bob encrypt a message to Alice device 1 and 2 using given encryptionPolicy
	 *
	 * parameters allow to control:
	 *  - plaintext message
	 *  - number of recipients (1 or 2)
	 *  - forced encryption policy(with a bool switch)
	 *  - expected message type (must be DRMessage or cipherMessage)
	 */
	private static void encryptionPolicy(LimeManager aliceManager, String aliceDevice1Id, String aliceDevice2Id,
			LimeManager bobManager, String bobDeviceId,
			String plainMessage, boolean multipleRecipients,
			LimeEncryptionPolicy setEncryptionPolicy, boolean forceEncryptionPolicy,
			LimeEncryptionPolicy getEncryptionPolicy) {

		int expected_success = 0;
		int expected_fail = 0;

		LimeTesterUtils TesterUtils = new LimeTesterUtils();

		// Create a callback, this one will be used for all operations
		LimeStatusCallbackImpl statusCallback = new LimeStatusCallbackImpl();


		// bob encrypt a message to Alice devices 1 (and 2 if setting says so)
		RecipientData[] recipients;
		if (multipleRecipients) {
			recipients = new RecipientData[2];
			recipients[0] = new RecipientData(aliceDevice1Id);
			recipients[1] = new RecipientData(aliceDevice2Id);
		} else {
			recipients = new RecipientData[1];
			recipients[0] = new RecipientData(aliceDevice1Id);
		}

		LimeOutputBuffer cipherMessage = new LimeOutputBuffer();

		if (forceEncryptionPolicy) {
			bobManager.encrypt(bobDeviceId, "alice", recipients, plainMessage.getBytes(), cipherMessage, statusCallback, setEncryptionPolicy);
		} else {
			bobManager.encrypt(bobDeviceId, "alice", recipients, plainMessage.getBytes(), cipherMessage, statusCallback);
		}
		expected_success+= 1;
		assert (statusCallback.wait_for_success(expected_success));

		boolean is_directEncryptionType = TesterUtils.DR_message_payloadDirectEncrypt(recipients[0].DRmessage);
		if (multipleRecipients) {
			// all cipher header must have the same message type
			assert(is_directEncryptionType == TesterUtils.DR_message_payloadDirectEncrypt(recipients[1].DRmessage));
		}

		if (getEncryptionPolicy == LimeEncryptionPolicy.DRMESSAGE) {
			assert(is_directEncryptionType);
			assert(cipherMessage.buffer.length == 0); // in direct Encryption mode, cipherMessage is empty
		} else {
			assert(is_directEncryptionType == false);
			assert(cipherMessage.buffer.length > 0); // in direct cipher message mode, cipherMessage is not empty
		}

		// alice1 decrypt
		LimeOutputBuffer decodedMessage = new LimeOutputBuffer();
		if (is_directEncryptionType) { // when having the message in DR message only, use the decrypt interface without cipherMessage
			assert(aliceManager.decrypt(aliceDevice1Id, "alice", bobDeviceId, recipients[0].DRmessage, decodedMessage) != LimePeerDeviceStatus.FAIL);
		} else {
			assert(aliceManager.decrypt(aliceDevice1Id, "alice", bobDeviceId, recipients[0].DRmessage, cipherMessage.buffer, decodedMessage) != LimePeerDeviceStatus.FAIL);
		}

		String s = new String(decodedMessage.buffer);
		assert (s.equals(plainMessage)):"Decoded message is not the encoded one";

		if (multipleRecipients) {
			// alice2 decrypt
			decodedMessage = new LimeOutputBuffer();
			assert(aliceManager.decrypt(aliceDevice2Id, "alice", bobDeviceId, recipients[1].DRmessage, cipherMessage.buffer, decodedMessage) != LimePeerDeviceStatus.FAIL);
			s = new String(decodedMessage.buffer);
			assert (s.equals(plainMessage)):"Decoded message is not the encoded one";
		}
	}

	public static void encryptionPolicy_suite(LimeCurveId curveId, String dbBasename, String x3dhServerUrl,  LimePostToX3DH postObj) {
		int expected_success = 0;
		int expected_fail = 0;

		LimeTesterUtils TesterUtils = new LimeTesterUtils();

		// Create a callback, this one will be used for all operations
		LimeStatusCallbackImpl statusCallback = new LimeStatusCallbackImpl();

		// Create db filenames and delete potential existing ones
		String curveIdString;
		if (curveId == LimeCurveId.C25519) {
			curveIdString = ".C25519";
		} else {
			curveIdString = ".C448";
		}
		String aliceDbFilename = "alice."+dbBasename+curveIdString+".sqlite3";
		String bobDbFilename = "bob."+dbBasename+curveIdString+".sqlite3";
		File file = new File(aliceDbFilename);
		file.delete();
		file = new File(bobDbFilename);
		file.delete();

		try {
			// Create random device ids
			String aliceDevice1Id = "alice.d1."+UUID.randomUUID().toString();
			String aliceDevice2Id = "alice.d2."+UUID.randomUUID().toString();
			String bobDeviceId = "bob."+UUID.randomUUID().toString();

			// create Manager and devices
			LimeManager aliceManager = new LimeManager(aliceDbFilename, postObj);
			aliceManager.create_user(aliceDevice1Id, x3dhServerUrl, curveId, 10, statusCallback);
			aliceManager.create_user(aliceDevice2Id, x3dhServerUrl, curveId, 10, statusCallback);
			LimeManager bobManager = new LimeManager(bobDbFilename, postObj);
			bobManager.create_user(bobDeviceId, x3dhServerUrl, curveId, 10, statusCallback);
			expected_success+=3;
			assert (statusCallback.wait_for_success(expected_success));

			// Bob encrypts to alice and we check result
			// Short messages
			encryptionPolicy(aliceManager, aliceDevice1Id, aliceDevice2Id,
					bobManager, bobDeviceId,
					LimeTesterUtils.shortMessage, false, // single recipient
					LimeEncryptionPolicy.OPTIMIZEUPLOADSIZE, false, // default policy(->optimizeUploadSize)
					LimeEncryptionPolicy.DRMESSAGE); // -> DRMessage

			encryptionPolicy(aliceManager, aliceDevice1Id, aliceDevice2Id,
					bobManager, bobDeviceId,
					LimeTesterUtils.shortMessage, false, // single recipient
					LimeEncryptionPolicy.OPTIMIZEUPLOADSIZE, true, // force to optimizeUploadSize
					LimeEncryptionPolicy.DRMESSAGE); // -> DRMessage

			encryptionPolicy(aliceManager, aliceDevice1Id, aliceDevice2Id,
					bobManager, bobDeviceId,
					LimeTesterUtils.shortMessage, false, // single recipient
					LimeEncryptionPolicy.OPTIMIZEGLOBALBANDWIDTH, true, // force to optimizeGlobalBandwidth
					LimeEncryptionPolicy.DRMESSAGE); // -> DRMessage

			encryptionPolicy(aliceManager, aliceDevice1Id, aliceDevice2Id,
					bobManager, bobDeviceId,
					LimeTesterUtils.shortMessage, false, // single recipient
					LimeEncryptionPolicy.DRMESSAGE, true, // force to DRmessage
					LimeEncryptionPolicy.DRMESSAGE); // -> DRMessage

			encryptionPolicy(aliceManager, aliceDevice1Id, aliceDevice2Id,
					bobManager, bobDeviceId,
					LimeTesterUtils.shortMessage, false, // single recipient
					LimeEncryptionPolicy.CIPHERMESSAGE, true, // force to cipher message
					LimeEncryptionPolicy.CIPHERMESSAGE); // -> cipher message

			encryptionPolicy(aliceManager, aliceDevice1Id, aliceDevice2Id,
					bobManager, bobDeviceId,
					LimeTesterUtils.shortMessage, true, // multiple recipients
					LimeEncryptionPolicy.OPTIMIZEUPLOADSIZE, false, // default policy(->optimizeUploadSize)
					LimeEncryptionPolicy.DRMESSAGE); // -> DRMessage

			encryptionPolicy(aliceManager, aliceDevice1Id, aliceDevice2Id,
					bobManager, bobDeviceId,
					LimeTesterUtils.shortMessage, true, // multiple recipient
					LimeEncryptionPolicy.OPTIMIZEUPLOADSIZE, true, // force to optimizeUploadSize
					LimeEncryptionPolicy.DRMESSAGE); // -> DRMessage

			encryptionPolicy(aliceManager, aliceDevice1Id, aliceDevice2Id,
					bobManager, bobDeviceId,
					LimeTesterUtils.shortMessage, true, // multiple recipient
					LimeEncryptionPolicy.OPTIMIZEGLOBALBANDWIDTH, true, // force to optimizeGlobalBandwidth
					LimeEncryptionPolicy.DRMESSAGE); // -> DRMessage

			encryptionPolicy(aliceManager, aliceDevice1Id, aliceDevice2Id,
					bobManager, bobDeviceId,
					LimeTesterUtils.shortMessage, true, // multiple recipient
					LimeEncryptionPolicy.DRMESSAGE, true, // force to DRmessage
					LimeEncryptionPolicy.DRMESSAGE); // -> DRMessage

			encryptionPolicy(aliceManager, aliceDevice1Id, aliceDevice2Id,
					bobManager, bobDeviceId,
					LimeTesterUtils.shortMessage, true, // multiple recipient
					LimeEncryptionPolicy.CIPHERMESSAGE, true, // force to cipher message
					LimeEncryptionPolicy.CIPHERMESSAGE); // -> cipher message

			// Long or veryLong messages
			encryptionPolicy(aliceManager, aliceDevice1Id, aliceDevice2Id,
					bobManager, bobDeviceId,
					LimeTesterUtils.longMessage, false, // single recipient
					LimeEncryptionPolicy.OPTIMIZEUPLOADSIZE, false, // default policy(->optimizeUploadSize)
					LimeEncryptionPolicy.DRMESSAGE); // -> DRMessage

			encryptionPolicy(aliceManager, aliceDevice1Id, aliceDevice2Id,
					bobManager, bobDeviceId,
					LimeTesterUtils.longMessage, false, // single recipient
					LimeEncryptionPolicy.OPTIMIZEUPLOADSIZE, true, // force optimizeUploadSize
					LimeEncryptionPolicy.DRMESSAGE); // -> DRMessage

			encryptionPolicy(aliceManager, aliceDevice1Id, aliceDevice2Id,
					bobManager, bobDeviceId,
					LimeTesterUtils.longMessage, false, // single recipient
					LimeEncryptionPolicy.OPTIMIZEGLOBALBANDWIDTH, true, // force optimizeGlobalBandwidth
					LimeEncryptionPolicy.DRMESSAGE); // -> DRMessage

			encryptionPolicy(aliceManager, aliceDevice1Id, aliceDevice2Id,
					bobManager, bobDeviceId,
					LimeTesterUtils.longMessage, false, // single recipient
					LimeEncryptionPolicy.DRMESSAGE, true, // force to DRmessage
					LimeEncryptionPolicy.DRMESSAGE); // -> DRMessage

			encryptionPolicy(aliceManager, aliceDevice1Id, aliceDevice2Id,
					bobManager, bobDeviceId,
					LimeTesterUtils.longMessage, false, // single recipient
					LimeEncryptionPolicy.CIPHERMESSAGE, true, // force to cipher message
					LimeEncryptionPolicy.CIPHERMESSAGE); // -> cipher message

			encryptionPolicy(aliceManager, aliceDevice1Id, aliceDevice2Id,
					bobManager, bobDeviceId,
					LimeTesterUtils.longMessage, true, // multiple recipient
					LimeEncryptionPolicy.OPTIMIZEUPLOADSIZE, false, // default policy(->optimizeUploadSize)
					LimeEncryptionPolicy.CIPHERMESSAGE); // -> cipher message

			encryptionPolicy(aliceManager, aliceDevice1Id, aliceDevice2Id,
					bobManager, bobDeviceId,
					LimeTesterUtils.longMessage, true, // multiple recipient
					LimeEncryptionPolicy.OPTIMIZEUPLOADSIZE, true, // force optimizeUploadSize
					LimeEncryptionPolicy.CIPHERMESSAGE); // -> cipher message

			encryptionPolicy(aliceManager, aliceDevice1Id, aliceDevice2Id,
					bobManager, bobDeviceId,
					LimeTesterUtils.longMessage, true, // multiple recipient
					LimeEncryptionPolicy.OPTIMIZEGLOBALBANDWIDTH, true, // force optimizeGlobalBandwidth
					LimeEncryptionPolicy.DRMESSAGE); // -> DRMessage (we need a very long message to switch to cipherMessage with that setting)

			encryptionPolicy(aliceManager, aliceDevice1Id, aliceDevice2Id,
					bobManager, bobDeviceId,
					LimeTesterUtils.veryLongMessage, true, // multiple recipient
					LimeEncryptionPolicy.OPTIMIZEGLOBALBANDWIDTH, true, // force optimizeGlobalBandwidth
					LimeEncryptionPolicy.CIPHERMESSAGE); // -> cipherMessage (we need a very long message to switch to cipherMessage with that setting)

			encryptionPolicy(aliceManager, aliceDevice1Id, aliceDevice2Id,
					bobManager, bobDeviceId,
					LimeTesterUtils.longMessage, true, // multiple recipient
					LimeEncryptionPolicy.DRMESSAGE, true, // force to DRmessage
					LimeEncryptionPolicy.DRMESSAGE); // -> DRMessage

			encryptionPolicy(aliceManager, aliceDevice1Id, aliceDevice2Id,
					bobManager, bobDeviceId,
					LimeTesterUtils.longMessage, true, // multiple recipient
					LimeEncryptionPolicy.CIPHERMESSAGE, true, // force to cipher message
					LimeEncryptionPolicy.CIPHERMESSAGE); // -> cipher message

			// Cleaning
			expected_success+= 3;
			aliceManager.delete_user(aliceDevice1Id, statusCallback);
			aliceManager.delete_user(aliceDevice2Id, statusCallback);
			bobManager.delete_user(bobDeviceId, statusCallback);
			assert (statusCallback.wait_for_success(expected_success));
			assert (statusCallback.fail == expected_fail);

			// Do not forget do deallocate the native ressources
			aliceManager.nativeDestructor();
			bobManager.nativeDestructor();
			aliceManager = null;
			bobManager = null;
		}
		catch (LimeException e) {
			assert(false):"Got an unexpected exception during Multidev operation queue test : "+e.getMessage();
		}

		// Remove database files
		file = new File(aliceDbFilename);
		file.delete();
		file = new File(bobDbFilename);
		file.delete();
	}
	/*Test Scenario:
	 * - create Bob and Alice devices
	 * - retrieve their respective Identity keys
	 * - check if they are verified -> they shall not be
	 * - set alice key as verified in bob's context
	 * - check it is now verified
	 * - set it to unsafe and check
	 * - set it as non verified and check
	 * - set it to unsafe and then untrusted using the alternative API without giving the Ik
	 * - try to set it to trusted using the API without Ik, we shall have and exception
	 * - try to set it to unknown, we shall have and exception
	 * - try to set it to fail, we shall have and exception
	 * - try to set a different alice identity key in bob's context, we shall have an exception
	 * - bob encrypts a message to alice -> check return status give NOT all recipients trusted
	 * - set alice key as verified in bob's context
	 * - bob encrypts a message to alice -> check return status give all recipients trusted
	 * - set a fake bob key in alice context
	 * - try to decrypt bob's message, it shall fail
	 * - alice try to encrypt a message to bob, it shall fail
	 */
	public static void identityVerifiedStatus(LimeCurveId curveId, String dbBasename, String x3dhServerUrl,  LimePostToX3DH postObj) {
		int expected_success = 0;
		int expected_fail = 0;

		LimeTesterUtils TesterUtils = new LimeTesterUtils();

		// Create a callback, this one will be used for all operations
		LimeStatusCallbackImpl statusCallback = new LimeStatusCallbackImpl();

		// Create db filenames and delete potential existing ones
		String curveIdString;
		if (curveId == LimeCurveId.C25519) {
			curveIdString = ".C25519";
		} else {
			curveIdString = ".C448";
		}
		String aliceDbFilename = "alice."+dbBasename+curveIdString+".sqlite3";
		String bobDbFilename = "bob."+dbBasename+curveIdString+".sqlite3";
		File file = new File(aliceDbFilename);
		file.delete();
		file = new File(bobDbFilename);
		file.delete();

		// Create random device ids
		String aliceDeviceId = "alice."+UUID.randomUUID().toString();
		String bobDeviceId = "bob."+UUID.randomUUID().toString();

		LimeManager aliceManager = new LimeManager(aliceDbFilename, postObj);
		LimeManager bobManager = new LimeManager(bobDbFilename, postObj);

		LimeOutputBuffer aliceIk = new LimeOutputBuffer();
		LimeOutputBuffer bobIk = new LimeOutputBuffer();
		LimeOutputBuffer fakeIk = new LimeOutputBuffer();

		try {
			// create devices
			aliceManager.create_user(aliceDeviceId, x3dhServerUrl, curveId, 10, statusCallback);
			bobManager.create_user(bobDeviceId, x3dhServerUrl, curveId, 10, statusCallback);
			expected_success+=2;
			assert (statusCallback.wait_for_success(expected_success));


			// retrieve their respective Ik
			aliceManager.get_selfIdentityKey(aliceDeviceId, aliceIk);
			bobManager.get_selfIdentityKey(bobDeviceId, bobIk);

			// build the fake alice Ik
			aliceManager.get_selfIdentityKey(aliceDeviceId, fakeIk);
			fakeIk.buffer[0] ^= 0xFF;

			// check their status: they don't know each other
			assert(aliceManager.get_peerDeviceStatus(bobDeviceId) == LimePeerDeviceStatus.UNKNOWN);
			assert(bobManager.get_peerDeviceStatus(aliceDeviceId) == LimePeerDeviceStatus.UNKNOWN);

			// set alice Id key as verified in Bob's Manager and check it worked
			bobManager.set_peerDeviceStatus(aliceDeviceId, aliceIk.buffer, LimePeerDeviceStatus.TRUSTED);
			assert(bobManager.get_peerDeviceStatus(aliceDeviceId) == LimePeerDeviceStatus.TRUSTED);
			// set it to unsafe and check it worked
			bobManager.set_peerDeviceStatus(aliceDeviceId, aliceIk.buffer, LimePeerDeviceStatus.UNSAFE);
			assert(bobManager.get_peerDeviceStatus(aliceDeviceId) == LimePeerDeviceStatus.UNSAFE);

			// reset it to untrusted and check it is still unsafe : we can escape unsafe only by setting to safe
			bobManager.set_peerDeviceStatus(aliceDeviceId, aliceIk.buffer, LimePeerDeviceStatus.UNTRUSTED);
			assert(bobManager.get_peerDeviceStatus(aliceDeviceId) == LimePeerDeviceStatus.UNSAFE);

			// set alice Id key as verified and check it this time it worked
			bobManager.set_peerDeviceStatus(aliceDeviceId, aliceIk.buffer, LimePeerDeviceStatus.TRUSTED);
			assert(bobManager.get_peerDeviceStatus(aliceDeviceId) == LimePeerDeviceStatus.TRUSTED);

			// set to untrusted without using alice Ik
			bobManager.set_peerDeviceStatus(aliceDeviceId, LimePeerDeviceStatus.UNTRUSTED);
			assert(bobManager.get_peerDeviceStatus(aliceDeviceId) == LimePeerDeviceStatus.UNTRUSTED);

			// set to unsafe without using alice Ik
			bobManager.set_peerDeviceStatus(aliceDeviceId, LimePeerDeviceStatus.UNSAFE);
			assert(bobManager.get_peerDeviceStatus(aliceDeviceId) == LimePeerDeviceStatus.UNSAFE);

			// try to set it to trusted without giving the Ik, it shall be ignored
			bobManager.set_peerDeviceStatus(aliceDeviceId, LimePeerDeviceStatus.TRUSTED);
			assert(bobManager.get_peerDeviceStatus(aliceDeviceId) == LimePeerDeviceStatus.UNSAFE);

			// set it back to trusted and check it this time it worked
			bobManager.set_peerDeviceStatus(aliceDeviceId, aliceIk.buffer, LimePeerDeviceStatus.TRUSTED);
			assert(bobManager.get_peerDeviceStatus(aliceDeviceId) == LimePeerDeviceStatus.TRUSTED);

			// try to set it to unknown, it shall be ignored
			bobManager.set_peerDeviceStatus(aliceDeviceId, aliceIk.buffer, LimePeerDeviceStatus.UNKNOWN);
			assert(bobManager.get_peerDeviceStatus(aliceDeviceId) == LimePeerDeviceStatus.TRUSTED);

			// try to set it to fail, it shall be ignored
			bobManager.set_peerDeviceStatus(aliceDeviceId, aliceIk.buffer, LimePeerDeviceStatus.FAIL);
			assert(bobManager.get_peerDeviceStatus(aliceDeviceId) == LimePeerDeviceStatus.TRUSTED);

			// try to set another key for alice in bob's context, setting it to untrusted, it shall be ok as the Ik is ignored when setting to untrusted
			bobManager.set_peerDeviceStatus(aliceDeviceId, fakeIk.buffer, LimePeerDeviceStatus.UNTRUSTED);
			assert(bobManager.get_peerDeviceStatus(aliceDeviceId) == LimePeerDeviceStatus.UNTRUSTED);

			// same goes for unsafe
			bobManager.set_peerDeviceStatus(aliceDeviceId, fakeIk.buffer, LimePeerDeviceStatus.UNSAFE);
			assert(bobManager.get_peerDeviceStatus(aliceDeviceId) == LimePeerDeviceStatus.UNSAFE);

			// set it back to trusted with the real key, it shall be Ok as it is still the one present in local storage
			bobManager.set_peerDeviceStatus(aliceDeviceId, aliceIk.buffer, LimePeerDeviceStatus.TRUSTED);
			assert(bobManager.get_peerDeviceStatus(aliceDeviceId) == LimePeerDeviceStatus.TRUSTED);
		}
		catch (LimeException e) {
			assert(false):"Got an unexpected exception during Multidev operation queue test : "+e.getMessage();
		}

		boolean gotException = false;

		try {
			// try to set another key for alice in bob's context, it shall generate an exception
			bobManager.set_peerDeviceStatus(aliceDeviceId, fakeIk.buffer, LimePeerDeviceStatus.TRUSTED);
		} catch (LimeException e) {
			gotException = true;
		}

		assert(gotException);
		gotException = false;

		try {
			// Now delete the alice device from Bob's cache and check its status is now back to unknown
			bobManager.delete_peerDevice(aliceDeviceId);
			assert(bobManager.get_peerDeviceStatus(aliceDeviceId) == LimePeerDeviceStatus.UNKNOWN);

			// set the device with the fake Ik but to untrusted so it won't be actually stored and shall still be unknown
			bobManager.set_peerDeviceStatus(aliceDeviceId, fakeIk.buffer, LimePeerDeviceStatus.UNTRUSTED);
			assert(bobManager.get_peerDeviceStatus(aliceDeviceId) == LimePeerDeviceStatus.UNKNOWN);

			// set the device with the fake Ik but to unsafe so the key shall not be registered in base but the user will
			bobManager.set_peerDeviceStatus(aliceDeviceId, fakeIk.buffer, LimePeerDeviceStatus.UNSAFE);
			assert(bobManager.get_peerDeviceStatus(aliceDeviceId) == LimePeerDeviceStatus.UNSAFE);

			// Set it to trusted, still using the fake Ik, it shall replace the invalid/empty Ik replacing it with the fake one
			bobManager.set_peerDeviceStatus(aliceDeviceId, fakeIk.buffer, LimePeerDeviceStatus.TRUSTED);
			assert(bobManager.get_peerDeviceStatus(aliceDeviceId) == LimePeerDeviceStatus.TRUSTED);
			bobManager.set_peerDeviceStatus(aliceDeviceId, fakeIk.buffer, LimePeerDeviceStatus.TRUSTED); // do it twice so we're sure the store Ik is the fake one
			assert(bobManager.get_peerDeviceStatus(aliceDeviceId) == LimePeerDeviceStatus.TRUSTED);

		}
		catch (LimeException e) {
			assert(false):"Got an unexpected exception during Multidev operation queue test : "+e.getMessage();
		}

		try {
			// same than above but using the actual key : try to set it to trusted, still using the fake Ik, it shall generate an exception as the Ik is invalid in storage
			bobManager.set_peerDeviceStatus(aliceDeviceId, aliceIk.buffer, LimePeerDeviceStatus.TRUSTED);
		} catch (LimeException e) {
			gotException = true;

			// Now delete the alice device from Bob's cache and check its status is now back to unknown
			bobManager.delete_peerDevice(aliceDeviceId);
			assert(bobManager.get_peerDeviceStatus(aliceDeviceId) == LimePeerDeviceStatus.UNKNOWN);
		}

		assert(gotException);
		gotException = false;






		try {
			// Bob encrypts a message for Alice, alice device status shall be : unknown(it is the first message bob sends and alice is not in cache)
			RecipientData[] recipients = new RecipientData[1];
			recipients[0] = new RecipientData(aliceDeviceId);
			LimeOutputBuffer cipherMessage = new LimeOutputBuffer();
			bobManager.encrypt(bobDeviceId, "alice", recipients, LimeTesterUtils.patterns[0].getBytes(), cipherMessage, statusCallback);
			assert (statusCallback.wait_for_success(++expected_success));
			assert(recipients[0].getPeerStatus() == LimePeerDeviceStatus.UNKNOWN);

			// Bob encrypts a second message for Alice, alice device status shall now be : untrusted(we know that device but didn't share the trust yet)
			recipients = new RecipientData[1];
			recipients[0] = new RecipientData(aliceDeviceId);
			cipherMessage = new LimeOutputBuffer();
			bobManager.encrypt(bobDeviceId, "alice", recipients, LimeTesterUtils.patterns[1].getBytes(), cipherMessage, statusCallback);
			assert (statusCallback.wait_for_success(++expected_success));
			assert(recipients[0].getPeerStatus() == LimePeerDeviceStatus.UNTRUSTED);

			// set again the key as verified in bob's context
			bobManager.set_peerDeviceStatus(aliceDeviceId, aliceIk.buffer, LimePeerDeviceStatus.TRUSTED);
			assert(bobManager.get_peerDeviceStatus(aliceDeviceId) == LimePeerDeviceStatus.TRUSTED);

			// Bob encrypts a message for Alice, alice device status shall now be : trusted
			recipients = new RecipientData[1];
			recipients[0] = new RecipientData(aliceDeviceId);
			cipherMessage = new LimeOutputBuffer();
			bobManager.encrypt(bobDeviceId, "alice", recipients, LimeTesterUtils.patterns[2].getBytes(), cipherMessage, statusCallback);
			assert (statusCallback.wait_for_success(++expected_success));
			assert(recipients[0].getPeerStatus() == LimePeerDeviceStatus.TRUSTED);

			// set a fake bob key in alice context(set is as verified otherwise the request is just ignored)
			bobManager.get_selfIdentityKey(bobDeviceId, fakeIk);
			fakeIk.buffer[0] ^= 0xFF;
			aliceManager.set_peerDeviceStatus(bobDeviceId, fakeIk.buffer, LimePeerDeviceStatus.TRUSTED);

			// alice decrypt but it will fail as the identity key in X3DH init packet is not matching the one we assert as verified
			LimeOutputBuffer decodedMessage = new LimeOutputBuffer();
			assert(aliceManager.decrypt(aliceDeviceId, "alice", bobDeviceId, recipients[0].DRmessage, cipherMessage.buffer, decodedMessage) == LimePeerDeviceStatus.FAIL);

			// alice now try to encrypt to Bob but it will fail as key fetched from X3DH server won't match the one we assert as verified
			recipients = new RecipientData[1];
			recipients[0] = new RecipientData(bobDeviceId);
			cipherMessage = new LimeOutputBuffer();
			aliceManager.encrypt(aliceDeviceId, "bob", recipients, LimeTesterUtils.patterns[3].getBytes(), cipherMessage, statusCallback);
			assert (statusCallback.wait_for_fail(++expected_fail));

			// delete bob's key from alice context and just set it to unsafe, he will get then no Ik in local storage
			aliceManager.delete_peerDevice(bobDeviceId);
			assert(aliceManager.get_peerDeviceStatus(bobDeviceId) == LimePeerDeviceStatus.UNKNOWN);
			aliceManager.set_peerDeviceStatus(bobDeviceId, LimePeerDeviceStatus.UNSAFE);
			assert(aliceManager.get_peerDeviceStatus(bobDeviceId) == LimePeerDeviceStatus.UNSAFE);

			// delete alice's key from Bob context, it will delete all session associated to Alice so when we encrypt a new message, it will fetch a new OPk as the previous one was deleted by alice
			bobManager.delete_peerDevice(aliceDeviceId);
			assert(bobManager.get_peerDeviceStatus(aliceDeviceId) == LimePeerDeviceStatus.UNKNOWN);

			// Bob encrypts a message for Alice, alice device status shall be : unknown(it is the first message bob sends and alice is not in cache)
			recipients = new RecipientData[1];
			recipients[0] = new RecipientData(aliceDeviceId);
			cipherMessage = new LimeOutputBuffer();
			bobManager.encrypt(bobDeviceId, "alice", recipients, LimeTesterUtils.patterns[4].getBytes(), cipherMessage, statusCallback);
			assert (statusCallback.wait_for_success(++expected_success));
			assert(recipients[0].getPeerStatus() == LimePeerDeviceStatus.UNKNOWN);

			// alice decrypts, this will update the empty Bob's Ik in storage using the X3DH init packet but shall give an unsafe status
			decodedMessage = new LimeOutputBuffer();
			assert(aliceManager.decrypt(aliceDeviceId, "alice", bobDeviceId, recipients[0].DRmessage, cipherMessage.buffer, decodedMessage) == LimePeerDeviceStatus.UNSAFE);

			// delete bob's key from alice context
			aliceManager.delete_peerDevice(bobDeviceId);
			assert(aliceManager.get_peerDeviceStatus(bobDeviceId) == LimePeerDeviceStatus.UNKNOWN);

			// delete alice's key from Bob context, it will delete all session associated to Alice so when we encrypt a new message, it will fetch a new OPk as the previous one was deleted by alice
			bobManager.delete_peerDevice(aliceDeviceId);
			assert(bobManager.get_peerDeviceStatus(aliceDeviceId) == LimePeerDeviceStatus.UNKNOWN);

			// Bob encrypts a message for Alice, alice device status shall be : unknown(it is the first message bob sends and alice is not in cache)
			recipients = new RecipientData[1];
			recipients[0] = new RecipientData(aliceDeviceId);
			cipherMessage = new LimeOutputBuffer();
			bobManager.encrypt(bobDeviceId, "alice", recipients, LimeTesterUtils.patterns[6].getBytes(), cipherMessage, statusCallback);
			assert (statusCallback.wait_for_success(++expected_success));
			assert(recipients[0].getPeerStatus() == LimePeerDeviceStatus.UNKNOWN);

			// alice tries again to decrypt but it shall work and return status unknown as we just deleted bob's device
			decodedMessage = new LimeOutputBuffer();
			assert(aliceManager.decrypt(aliceDeviceId, "alice", bobDeviceId, recipients[0].DRmessage, cipherMessage.buffer, decodedMessage) == LimePeerDeviceStatus.UNKNOWN);
			String s = new String(decodedMessage.buffer);
			assert (s.equals(LimeTesterUtils.patterns[6])):"Decoded message is not the encoded one";

			// now set bob's to trusted in alice cache, it shall work as key retrieved from X3DH init message during decryption match the one we're giving
			aliceManager.set_peerDeviceStatus(bobDeviceId, bobIk.buffer, LimePeerDeviceStatus.TRUSTED);
			assert(aliceManager.get_peerDeviceStatus(bobDeviceId) == LimePeerDeviceStatus.TRUSTED);

			// Cleaning
			expected_success+= 2;
			aliceManager.delete_user(aliceDeviceId, statusCallback);
			bobManager.delete_user(bobDeviceId, statusCallback);
			assert (statusCallback.wait_for_success(expected_success));
			assert (statusCallback.fail == expected_fail);

			// Do not forget do deallocate the native ressources
			aliceManager.nativeDestructor();
			bobManager.nativeDestructor();
			aliceManager = null;
			bobManager = null;
		}
		catch (LimeException e) {
			assert(false):"Got an unexpected exception during Multidev operation queue test : "+e.getMessage();
		}

		// Remove database files
		file = new File(aliceDbFilename);
		file.delete();
		file = new File(bobDbFilename);
		file.delete();
	}
}

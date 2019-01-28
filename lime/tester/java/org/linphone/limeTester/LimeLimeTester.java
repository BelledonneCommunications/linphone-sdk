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

public class LimeLimeTester {
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
		// which does not produce encrypted output(all except encryptions)
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

		// Create bob and alice lime managers
		LimeManager aliceManager = new LimeManager(aliceDbFilename, postObj);
		LimeManager bobManager = new LimeManager(bobDbFilename, postObj);

		// Create random device id for alice and bob
		String AliceDeviceId = "alice."+UUID.randomUUID().toString();
		String BobDeviceId1 = "bob.d1."+UUID.randomUUID().toString();
		String BobDeviceId2 = "bob.d2."+UUID.randomUUID().toString();

		aliceManager.create_user(AliceDeviceId, x3dhServerUrl, curveId, 10, statusCallback);
		expected_success+= 1;
		assert (statusCallback.wait_for_success(expected_success));
		bobManager.create_user(BobDeviceId1, x3dhServerUrl, curveId, 10, statusCallback);
		expected_success+= 1;
		assert (statusCallback.wait_for_success(expected_success)); // We cannot create both bob's device at the same time as they use the same database
		bobManager.create_user(BobDeviceId2, x3dhServerUrl, curveId, 10, statusCallback);
		expected_success+= 1;
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

		// Remove database files
		file = new File(aliceDbFilename);
		file.delete();
		file = new File(bobDbFilename);
		file.delete();
	}
}

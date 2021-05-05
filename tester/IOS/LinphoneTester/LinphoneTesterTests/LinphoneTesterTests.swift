
//
//  LinphoneTesterTests.swift
//  LinphoneTester
//
//  Created by QuentinArguillere on 25/02/2021.
//  Copyright © 2021 belledonne. All rights reserved.
//

import Foundation
import XCTest
import linphonesw

class IncomingPushTest: XCTestCase {
	
	var tokenReceivedExpect : XCTestExpectation!
	var pushReceivedExpect : XCTestExpectation!
	
	/*
	func testTokenReception() {
		NotificationCenter.default.addObserver(self,
											   selector:#selector(self.receivedPushCallback),
											   name: NSNotification.Name(rawValue: kPushReceivedEvent),
											   object:nil);
		
		
		tokenReceivedExpect = expectation(description: "Token received by push")
		var creatorCallbackExpect = expectation(description: "OnSendToken callback")
		creatorCallbacks.expect = creatorCallbackExpect
		
		creator.pnPrid =  (UIApplication.shared.delegate as! AppDelegate).pushToken! as String
		creator.pnParam = "testteam.\(Bundle.main.bundleIdentifier! as String)"
		creator.pnProvider = "apns.dev"
		
		creator.addDelegate(delegate: creatorCallbacks)
		
		
		//_ = creator.sendTokenFlexiapi()
		
		waitForExpectations(timeout: 20)
		
	}
	
	*/
	func testUpdateContactUriWhenPushConfigurationChanges() {
		let pauline = linphone_core_manager_new("pauline_rc")
		let paulineCore = Core.getSwiftObject(cObject: pauline!.pointee.lc)
		let paulineAccount = paulineCore.defaultAccount!
		paulineCore.autoIterateEnabled = true
		
		var newPaulineParams = paulineAccount.params?.clone()
		paulineAccount.params = newPaulineParams
		
		let voipRegisteredExpect = expectation(description: "Registered with voip token")
		voipRegisteredExpect.assertForOverFulfill = false
		let paulineRegisterDelegate = AccountDelegateStub(onRegistrationStateChanged: { (account: Account, state: RegistrationState, message: String) in
			if (!account.params!.pushNotificationConfig!.voipToken.isEmpty) {
				debugPrint("Current token : \(account.params!.pushNotificationConfig!.voipToken)")
				voipRegisteredExpect.fulfill()
			}
		})
		paulineAccount.addDelegate(delegate: paulineRegisterDelegate)
		waitForExpectations(timeout: 40)
		
		newPaulineParams = paulineAccount.params?.clone()
		newPaulineParams?.pushNotificationConfig?.provider = "testprovider"
		paulineAccount.params = newPaulineParams
		XCTAssertTrue(paulineAccount.params!.contactUriParameters.contains("pn-provider=testprovider;"))
		
		newPaulineParams = paulineAccount.params?.clone()
		newPaulineParams?.pushNotificationConfig?.param = "testparams"
		paulineAccount.params = newPaulineParams
		XCTAssertTrue(paulineAccount.params!.contactUriParameters.contains("pn-param=testparams;"))
		
		newPaulineParams = paulineAccount.params?.clone()
		newPaulineParams?.pushNotificationConfig?.voipToken = "testvoiptoken"
		paulineAccount.params = newPaulineParams
		XCTAssertTrue(paulineAccount.params!.contactUriParameters.contains("pn-prid=testvoiptoken"))
	}
	
	func testVoipPushCall() {
		let marie = linphone_core_manager_new("marie_rc")
		let marieCore = Core.getSwiftObject(cObject: marie!.pointee.lc)
		let pauline = linphone_core_manager_new("pauline_rc")
		var paulineCore : Core? = Core.getSwiftObject(cObject: pauline!.pointee.lc)
		
		var paulineAccount : Account! = paulineCore!.defaultAccount
		let marieAccount = marieCore.defaultAccount!
		marieCore.pushNotificationEnabled = false
		marieCore.autoIterateEnabled = true
		//try! marieCore.start()
		
		let newPaulineParams = paulineAccount.params?.clone()
		newPaulineParams?.pushNotificationConfig?.provider = "apns.dev"
		paulineAccount.params = newPaulineParams
		paulineCore!.callkitEnabled = true
		paulineCore!.autoIterateEnabled = true
		
		// Marie has push notification disabled, so we should never register get a voip token for her
		let marieShouldNotHavePushDelegate = AccountDelegateStub(onRegistrationStateChanged: { (account: Account, state: RegistrationState, message: String) in
			XCTAssertTrue(account.params!.pushNotificationConfig?.voipToken == nil)
		})
		marieAccount.addDelegate(delegate: marieShouldNotHavePushDelegate)
		
		
		let voipRegisteredExpect = expectation(description: "Registered with voip token")
		voipRegisteredExpect.assertForOverFulfill = false
		let paulineRegisterDelegate = AccountDelegateStub(onRegistrationStateChanged: { (account: Account, state: RegistrationState, message: String) in
			if (!account.params!.pushNotificationConfig!.voipToken.isEmpty) {
				debugPrint("Current token : \(account.params!.pushNotificationConfig!.voipToken)")
				voipRegisteredExpect.fulfill()
			}
		})
		paulineAccount.addDelegate(delegate: paulineRegisterDelegate)
		waitForExpectations(timeout: 40)
		
		func stopCoreAndReceivePushRoutine(iterationNumber: Int) {
			let expectCoreStopped = self.expectation(description: "Pauline Core Stopped - iteration \(iterationNumber)")
			let paulineCoreStoppedDelegate = CoreDelegateStub(onGlobalStateChanged: { (lc: Core, gstate: GlobalState, message: String) in
				if (gstate == GlobalState.Off){
					expectCoreStopped.fulfill()
				}
			})
			paulineCore!.addDelegate(delegate: paulineCoreStoppedDelegate)
			paulineCore!.stopAsync()
			self.waitForExpectations(timeout: 30)
			paulineCore?.removeDelegate(delegate: paulineCoreStoppedDelegate)
			
			let expectPushIncoming = self.expectation(description: "Incoming Push Received - iteration \(iterationNumber)")
			let paulineCallDelegate = CoreDelegateStub(onCallStateChanged: { (lc: Core, call: Call, cstate: Call.State, message: String) in
				if (cstate == .PushIncomingReceived){
					expectPushIncoming.fulfill()
				}
			})
			paulineCore!.addDelegate(delegate: paulineCallDelegate)
			let call = marieCore.inviteAddress(addr: paulineAccount.contactAddress!)
			self.waitForExpectations(timeout: 100)
			paulineCore!.removeDelegate(delegate: paulineCallDelegate)
			
			let expectCallTerminated = self.expectation(description: "Call terminated expectation - iteration \(iterationNumber)")
			let callTerminatedDelegate = CallDelegateStub(onStateChanged: { (thisCall: Call, state: Call.State, message : String) in
				if (state == Call.State.Released) {
					usleep(5000000)
					expectCallTerminated.fulfill()
				}
			})
			call?.addDelegate(delegate: callTerminatedDelegate)
			try! call!.terminate()
			self.waitForExpectations(timeout: 100)
			usleep(5000000)
		}
		stopCoreAndReceivePushRoutine(iterationNumber: 1)
		stopCoreAndReceivePushRoutine(iterationNumber: 2)
		stopCoreAndReceivePushRoutine(iterationNumber: 3)
		stopCoreAndReceivePushRoutine(iterationNumber: 4)
		stopCoreAndReceivePushRoutine(iterationNumber: 5)
		stopCoreAndReceivePushRoutine(iterationNumber: 6)
		stopCoreAndReceivePushRoutine(iterationNumber: 7)
		
		
		paulineCore = nil
		paulineAccount = nil
		linphone_core_manager_restart(pauline, 1)
		paulineCore = Core.getSwiftObject(cObject: pauline!.pointee.lc)
		paulineAccount = paulineCore!.defaultAccount!
		
		let voipRegisteredExpect2 = expectation(description: "Registered with voip token")
		voipRegisteredExpect2.assertForOverFulfill = false
		let paulineRegisterDelegate2 = AccountDelegateStub(onRegistrationStateChanged: { (account: Account, state: RegistrationState, message: String) in
			if (!account.params!.pushNotificationConfig!.voipToken.isEmpty) {
				debugPrint("Current token : \(account.params!.pushNotificationConfig!.voipToken)")
				voipRegisteredExpect2.fulfill()
			}
		})
		paulineAccount.addDelegate(delegate: paulineRegisterDelegate2)
		waitForExpectations(timeout: 40)
		
		let expectSecondPushIncoming = expectation(description: "Second Incoming Push Received")
		let paulineSecondCallDelegate = CoreDelegateStub(onCallStateChanged: { (lc: Core, call: Call, cstate: Call.State, message: String) in
			if (cstate == .PushIncomingReceived){
				expectSecondPushIncoming.fulfill()
			}
		})
		paulineCore!.addDelegate(delegate: paulineSecondCallDelegate)
		
		_ = marieCore.inviteAddress(addr: paulineAccount.contactAddress!)
		waitForExpectations(timeout: 100)
		
	}
	
	func receivedPushTokenCallback() {
		tokenReceivedExpect.fulfill()
	}
	func receivedPushNotificationCallback() {
		pushReceivedExpect.fulfill()
	}
	
	func testChatroom() {
		tokenReceivedExpect = expectation(description: "Push Token received")
		tokenReceivedExpect.assertForOverFulfill = false
		NotificationCenter.default.addObserver(self,
											   selector:#selector(self.receivedPushTokenCallback),
											   name: NSNotification.Name(rawValue: kPushTokenReceived),
											   object:nil);
		
		UIApplication.shared.registerForRemoteNotifications()
		waitForExpectations(timeout: 15)

		let marie = linphone_core_manager_new("marie_rc")
		let marieCore = Core.getSwiftObject(cObject: marie!.pointee.lc)
		let pauline = linphone_core_manager_new("pauline_rc")
		let paulineCore = Core.getSwiftObject(cObject: pauline!.pointee.lc)
		
		let marieAccount = marieCore.defaultAccount!
		let marieShouldNotHavePushDelegate = AccountDelegateStub(onRegistrationStateChanged: { (account: Account, state: RegistrationState, message: String) in
			XCTAssertTrue(account.params!.pushNotificationConfig?.voipToken == nil)
		})
		marieAccount.addDelegate(delegate: marieShouldNotHavePushDelegate)
		marieCore.pushNotificationEnabled = false
		marieCore.autoIterateEnabled = true
		
		let paulineAccount = paulineCore.defaultAccount!
		let enablePushParams = paulineAccount.params!.clone()
		enablePushParams?.remotePushNotificationAllowed = true
		paulineAccount.params = enablePushParams
		
		//let testToken = (UIApplication.shared.delegate as! AppDelegate).pushDeviceToken
		//paulineCore.didRegisterForRemotePush(deviceToken: &(UIApplication.shared.delegate as! AppDelegate).pushDeviceToken)
		
		paulineCore.didRegisterForRemotePush(deviceToken: Factory.Instance.userData)
		paulineCore.autoIterateEnabled = true
		
		let remoteTokenAdded = expectation(description: "Test")
		remoteTokenAdded.assertForOverFulfill = false
		let paulineRegisterDelegate = AccountDelegateStub(onRegistrationStateChanged: { (account: Account, state: RegistrationState, message: String) in
			let token = account.params!.pushNotificationConfig!.remoteToken
			if (!token.isEmpty) {
				remoteTokenAdded.fulfill()
			}
		})
		paulineAccount.addDelegate(delegate: paulineRegisterDelegate)
		waitForExpectations(timeout: 15)

		
		
		pushReceivedExpect = expectation(description: "Push Notification received")
		pushReceivedExpect.assertForOverFulfill = false
		NotificationCenter.default.addObserver(self,
											   selector:#selector(self.receivedPushNotificationCallback),
											   name: NSNotification.Name(rawValue: kPushNotificationReceived),
											   object:nil);
		paulineCore.autoIterateEnabled = false
		let chatParams = try! marieCore.createDefaultChatRoomParams()
		chatParams.backend = ChatRoomBackend.Basic
		let marieChatroom = try! marieCore.createChatRoom(params: chatParams, localAddr: marieAccount.contactAddress, participants: [paulineAccount.contactAddress!])
		let chatMsg = try! marieChatroom.createMessageFromUtf8(message: "TestMessage")
		chatMsg.send()
		
		waitForExpectations(timeout: 100)
		paulineCore.autoIterateEnabled = true
		let expectMessageIncoming = expectation(description: "Incoming Push Received")
		let paulineIncomingMessageDelegate = CoreDelegateStub(onMessageReceived: { (lc: Core, chatroom: ChatRoom, message: ChatMessage) in
			expectMessageIncoming.fulfill()
		})
		paulineCore.addDelegate(delegate: paulineIncomingMessageDelegate)
	}
 }

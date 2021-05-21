
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
	class LinphoneTestUser {
		let core : Core
		let manager : UnsafeMutablePointer<LinphoneCoreManager>
		
		var coreDelegates : [CoreDelegate] = []
		var accountDelegates : [AccountDelegate] = []
		
		init(rcFile : String) {
			manager = linphone_core_manager_new(rcFile)
			core = Core.getSwiftObject(cObject: manager.pointee.lc)
			core.autoIterateEnabled = true
		}
		
		func cleanupDelegates() {
			for coreDel in coreDelegates {
				core.removeDelegate(delegate: coreDel)
			}
			coreDelegates.removeAll()
			for accountDel in accountDelegates {
				core.defaultAccount!.removeDelegate(delegate: accountDel)
			}
			accountDelegates.removeAll()
		}
		
		func stopCore(stoppedCoreExpectation: XCTestExpectation, cleanUpAfterWait : Bool = true, waitFn : @escaping()->Void) {
			let coreStoppedDelegate = CoreDelegateStub(onGlobalStateChanged: { (lc: Core, gstate: GlobalState, message: String) in
				if (gstate == GlobalState.Off){
					stoppedCoreExpectation.fulfill()
				}
			})
			core.addDelegate(delegate: coreStoppedDelegate)
			core.stopAsync()
			waitFn()
			if (cleanUpAfterWait) {
				core.removeDelegate(delegate: coreStoppedDelegate)
			} else {
				coreDelegates.append(coreStoppedDelegate)
			}
		}
		
		func waitForVoipTokenRegistration(voipTokenRegisteredExpectation: XCTestExpectation, cleanUpAfterWait : Bool = true, waitFn : @escaping()->Void) {
			let voipTokenDelegate = AccountDelegateStub(onRegistrationStateChanged: { (account: Account, state: RegistrationState, message: String) in
				if (!account.params!.pushNotificationConfig!.voipToken.isEmpty) {
					voipTokenRegisteredExpectation.fulfill()
				}
			})
			core.defaultAccount!.addDelegate(delegate: voipTokenDelegate)
			waitFn()
			if (cleanUpAfterWait) {
				core.defaultAccount!.removeDelegate(delegate: voipTokenDelegate)
			} else {
				accountDelegates.append(voipTokenDelegate)
			}
		}
		
		func waitForVoipPushIncoming(voipPushIncomingExpectation: XCTestExpectation, cleanUpAfterWait : Bool = true, callAndWaitFn : @escaping() -> Void) {
			let receivedPushDelegate = CoreDelegateStub(onCallStateChanged: { (lc: Core, call: Call, cstate: Call.State, message: String) in
				if (cstate == .PushIncomingReceived){
					voipPushIncomingExpectation.fulfill()
				}
			})
			core.addDelegate(delegate: receivedPushDelegate)
			callAndWaitFn()
			if (cleanUpAfterWait) {
				core.removeDelegate(delegate: receivedPushDelegate)
			} else {
				coreDelegates.append(receivedPushDelegate)
			}
		}
	}
	
	
	func testUpdateContactUriWhenPushConfigurationChanges() {
		let pauline = LinphoneTestUser(rcFile: "pauline_rc")
		pauline.core.autoIterateEnabled = true
		
		pauline.waitForVoipTokenRegistration(voipTokenRegisteredExpectation: expectation(description: "Registered with voip token")) {
			self.waitForExpectations(timeout: 20)
		}
		
		let paulineAccount = pauline.core.defaultAccount!
		var newPaulineParams = paulineAccount.params?.clone()
		paulineAccount.params = newPaulineParams
		
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
		let marie = LinphoneTestUser(rcFile: "marie_rc")
		marie.core.pushNotificationEnabled = false
		let ensureNotRegisteredAgainExp = expectation(description: "Registered with voip token")
		ensureNotRegisteredAgainExp.isInverted = true
		marie.waitForVoipTokenRegistration(voipTokenRegisteredExpectation: ensureNotRegisteredAgainExp) {
			self.waitForExpectations(timeout: 5) // Inverted expectation, this ensure that Marie does not register with a voip token
		}

		// ONLY A SINGLE VOIP PUSH REGISTRY EXIST PER APP.
		// IF YOU INSTANCIATE SEVERAL CORES, MAKE SURE THAT THE ONE THAT WILL PROCESS PUSH NOTIFICATION IS CREATE LAST
		let pauline = LinphoneTestUser(rcFile: "pauline_rc")
		pauline.waitForVoipTokenRegistration(voipTokenRegisteredExpectation: expectation(description: "Registered with voip token")) {
			self.waitForExpectations(timeout: 20)
		}
		
		let paulineAddress = pauline.core.defaultAccount!.contactAddress!
		pauline.stopCore(stoppedCoreExpectation: self.expectation(description: "Pauline Core Stopped")) {
			self.waitForExpectations(timeout: 10)
		}
		
		var call : Call?
		pauline.waitForVoipPushIncoming(voipPushIncomingExpectation: self.expectation(description: "Incoming Push Received")) {
			call = marie.core.inviteAddress(addr: paulineAddress)
			self.waitForExpectations(timeout: 10)
		}
		
		let expectCallTerminated = self.expectation(description: "Call terminated expectation - iteration")
		let callTerminatedDelegate = CallDelegateStub(onStateChanged: { (thisCall: Call, state: Call.State, message : String) in
			if (state == Call.State.Released) {
				expectCallTerminated.fulfill()
			}
		})
		call?.addDelegate(delegate: callTerminatedDelegate)
		try! call!.terminate()
		self.waitForExpectations(timeout: 10)
		
		/*
		func stopCoreAndReceivePushRoutine(iterationNumber: Int) {
			pauline.stopCore(stoppedCoreExpectation: self.expectation(description: "Pauline Core Stopped - iteration \(iterationNumber)")) {
				self.waitForExpectations(timeout: 10)
			}
			
			var call : Call?
			pauline.waitForVoipPushIncoming(voipPushIncomingExpectation: self.expectation(description: "Incoming Push Received - iteration \(iterationNumber)")) {
				call = marie.core.inviteAddress(addr: pauline.account.contactAddress!)
				self.waitForExpectations(timeout: 20)
			}
			
			let expectCallTerminated = self.expectation(description: "Call terminated expectation - iteration \(iterationNumber)")
			let callTerminatedDelegate = CallDelegateStub(onStateChanged: { (thisCall: Call, state: Call.State, message : String) in
				if (state == Call.State.Released) {
					expectCallTerminated.fulfill()
				}
			})
			call?.addDelegate(delegate: callTerminatedDelegate)
			try! call!.terminate()
			self.waitForExpectations(timeout: 30)
		}
		stopCoreAndReceivePushRoutine(iterationNumber: 1)
		stopCoreAndReceivePushRoutine(iterationNumber: 2)
		stopCoreAndReceivePushRoutine(iterationNumber: 3)
		stopCoreAndReceivePushRoutine(iterationNumber: 4)
		stopCoreAndReceivePushRoutine(iterationNumber: 5) */
		
		/*
		let newPaulineParams = pauline.account.params?.clone()
		paulineCore = nil
		paulineAccount = nil
		linphone_core_manager_restart(pauline, 1)
		paulineCore = Core.getSwiftObject(cObject: pauline!.pointee.lc)
		paulineCore!.autoIterateEnabled = true
		paulineAccount = paulineCore!.defaultAccount!
		
		let voipRegisteredExpect2 = expectation(description: "Registered with voip token")
		voipRegisteredExpect2.assertForOverFulfill = false
		let paulineRegisterDelegate2 = AccountDelegateStub(onRegistrationStateChanged: { (account: Account, state: RegistrationState, message: String) in
			if (!account.params!.pushNotificationConfig!.voipToken.isEmpty) {
				voipRegisteredExpect2.fulfill()
			}
		})
		paulineAccount.addDelegate(delegate: paulineRegisterDelegate2)
		waitForExpectations(timeout: 40)
		
		paulineCore!.autoIterateEnabled = false
		let expectSecondPushIncoming = expectation(description: "Second Incoming Push Received")
		let paulineSecondCallDelegate = CoreDelegateStub(onCallStateChanged: { (lc: Core, call: Call, cstate: Call.State, message: String) in
			if (cstate == .PushIncomingReceived){
				expectSecondPushIncoming.fulfill()
			}
		})
		paulineCore!.addDelegate(delegate: paulineSecondCallDelegate)
		
		_ = marieCore.inviteAddress(addr: paulineAccount.contactAddress!)
		waitForExpectations(timeout: 100)
*/
		
	}
	
	func testAnswerCallBeforePushIsReceivedOnSecondDevice() {
		let marie = LinphoneTestUser(rcFile: "marie_rc")
		marie.core.pushNotificationEnabled = false
		
		let basicPauline = LinphoneTestUser(rcFile: "pauline_rc")
		basicPauline.core.pushNotificationEnabled = false
		
		// ONLY A SINGLE VOIP PUSH REGISTRY EXIST PER APP. IF YOU INSTANCIATE SEVERAL CORES, MAKE SURE THAT THE ONE THAT WILL PROCESS PUSH NOTIFICATION IS CREATE LAST
		let pushPauline = LinphoneTestUser(rcFile: "pauline_rc")
		pushPauline.core.pushIncomingCallTimeout = 5
		pushPauline.core.defaultAccount?.params?.conferenceFactoryUri = "sip:conference@fakeserver.com"
		pushPauline.waitForVoipTokenRegistration(voipTokenRegisteredExpectation: expectation(description: "Registered with voip token")) {
			self.waitForExpectations(timeout: 20)
		}
		
		pushPauline.stopCore(stoppedCoreExpectation: expectation(description: "Pauline Core Stopped")) {
			self.waitForExpectations(timeout: 10)
		}
		

		let expectSipInviteAccepted = self.expectation(description: "Sip invite received")
		let basicPaulineIncomingCallDelegate = CoreDelegateStub(onCallStateChanged: { (lc: Core, call: Call, cstate: Call.State, message: String) in
			if (cstate == .IncomingReceived){
				try! call.accept()
			} else if (cstate == .PushIncomingReceived) {
				XCTAssertFalse(false, "Should never receive push on this user")
			} else if (cstate == .StreamsRunning) {
				expectSipInviteAccepted.fulfill()
			}
		})
		basicPauline.core.addDelegate(delegate: basicPaulineIncomingCallDelegate)
		
		let expectPushIncoming = self.expectation(description: "Incoming Push Received")
		let expectPushCallTimedOutTooSoon = self.expectation(description: "Push Call timed out too soon")
		expectPushCallTimedOutTooSoon.isInverted = true
		let pushPaulineIncomingCallDelegate = CoreDelegateStub(onCallStateChanged: { (lc: Core, call: Call, cstate: Call.State, message: String) in
			if (cstate == .PushIncomingReceived){
				expectPushIncoming.fulfill()
			} else if (cstate == .End) {
				expectPushCallTimedOutTooSoon.fulfill()
			}
		})
		pushPauline.core.addDelegate(delegate: pushPaulineIncomingCallDelegate)
		
		let marieCall = marie.core.invite(url: basicPauline.core.defaultAccount!.params!.identityAddress!.asString())
		self.waitForExpectations(timeout: TimeInterval(pushPauline.core.pushIncomingCallTimeout - 1))
		basicPauline.core.removeDelegate(delegate: basicPaulineIncomingCallDelegate)
		pushPauline.core.removeDelegate(delegate: pushPaulineIncomingCallDelegate)
		
		let expectPushCallTimedOut = self.expectation(description: "Push Call timed out when expected")
		let pushPaulineTimedOutDelegate = CoreDelegateStub(onCallStateChanged: { (lc: Core, call: Call, cstate: Call.State, message: String) in
			if (cstate == .End) {
				expectPushCallTimedOut.fulfill()
			}
		})
		pushPauline.core.addDelegate(delegate: pushPaulineTimedOutDelegate)
		self.waitForExpectations(timeout: 5)
		pushPauline.core.removeDelegate(delegate: pushPaulineTimedOutDelegate)
		
		
		let expectCallTerminated = self.expectation(description: "Call terminated expectation")
		let callTerminatedDelegate = CallDelegateStub(onStateChanged: { (thisCall: Call, state: Call.State, message : String) in
			if (state == Call.State.Released) {
				expectCallTerminated.fulfill()
			}
		})
		marieCall?.addDelegate(delegate: callTerminatedDelegate)
		
		try! marieCall!.terminate()
		self.waitForExpectations(timeout: 100)
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
		
		waitForExpectations(timeout: 15)
	}
 }

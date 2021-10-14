
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
		var setupDNSDelegate : CoreDelegate!
		var core : Core!
		let manager : UnsafeMutablePointer<LinphoneCoreManager>
		
		
		init(rcFile : String) {
			manager = linphone_core_manager_new(rcFile)
			core = Core.getSwiftObject(cObject: manager.pointee.lc)
			core.autoIterateEnabled = true
			
			setupDNSDelegate = CoreDelegateStub(onGlobalStateChanged: { (lc: Core, gstate: GlobalState, message: String) in
				if (gstate == .Configuring) {
					liblinphone_tester_set_dns_engine_by_default(lc.getCobject)
					linphone_core_set_dns_servers(lc.getCobject, flexisip_tester_dns_ip_addresses)
				} else if (gstate == .Startup) {
					let transport = try! Factory.Instance.createTransports()
					transport.tcpPort = -1
					transport.tlsPort = -1
					try! lc.setTransports(newValue: transport)
				}
			})
			core.addDelegate(delegate: setupDNSDelegate)
		}
		
		deinit {
			core = nil
			setupDNSDelegate = nil
			linphone_core_manager_destroy(manager)
		}
		
		func stopCore(stoppedCoreExpectation: XCTestExpectation, waitFn : @escaping()->Void) {
			let coreStoppedDelegate = CoreDelegateStub(onGlobalStateChanged: { (lc: Core, gstate: GlobalState, message: String) in
				if (gstate == GlobalState.Off){
					stoppedCoreExpectation.fulfill()
				}
			})
			core.addDelegate(delegate: coreStoppedDelegate)
			core.stopAsync()
			waitFn()
			core.removeDelegate(delegate: coreStoppedDelegate)
		}
		
		func waitForRegistration(registeredExpectation: XCTestExpectation, requireVoipToken : Bool = true, waitFn : @escaping()->Void) {
			registeredExpectation.assertForOverFulfill = false
			let registeredDelegate = AccountDelegateStub(onRegistrationStateChanged: { (account: Account, state: RegistrationState, message: String) in
				if (state == .Ok) {
					if (!requireVoipToken || !account.params!.pushNotificationConfig!.voipToken.isEmpty) {
						registeredExpectation.fulfill()
					}
				}
			})
			core.defaultAccount!.addDelegate(delegate: registeredDelegate)
			waitFn()
		}
		
		func waitForVoipPushIncoming(voipPushIncomingExpectation: XCTestExpectation, callAndWaitFn : @escaping() -> Void) {
			let receivedPushDelegate = CoreDelegateStub(onCallStateChanged: { (lc: Core, call: Call, cstate: Call.State, message: String) in
				if (cstate == .PushIncomingReceived){
					voipPushIncomingExpectation.fulfill()
				}
			})
			core.addDelegate(delegate: receivedPushDelegate)
			callAndWaitFn()
			core.removeDelegate(delegate: receivedPushDelegate)
		}
	}
	
	func testCallToSRTPMandatoryEncryptionWithNoEncryptionEnabled() {
		let marie = LinphoneTestUser(rcFile: "marie_rc")
		try! marie.core.setMediaencryption(newValue: .None)
		marie.core.avpfMode = .Enabled
		
		let pauline = LinphoneTestUser(rcFile: "pauline_push_enabled_rc")
		try! pauline.core.setMediaencryption(newValue: .SRTP)
		pauline.core.mediaEncryptionMandatory = true
		
		pauline.waitForRegistration(registeredExpectation: expectation(description: "Pauline voip registered")) {
			self.waitForExpectations(timeout: 10)
		}
		
		let paulineAddress = pauline.core.defaultAccount!.params!.identityAddress!.asString()
		pauline.stopCore(stoppedCoreExpectation: self.expectation(description: "Pauline Core Stopped")) {
			self.waitForExpectations(timeout: 10)
		}
		
		var call1, call2 : Call?
		
		let expectPush1Incoming = expectation(description: "PushIncoming 1 received")
		let expectPush2Incoming = expectation(description: "PushIncoming 2 received")
		// We expect 2 calls because once the first call fails, a second one will start to check if it was due to AVPF and not SRTP
		let receivedPushDelegate = CoreDelegateStub(onCallStateChanged: { (lc: Core, call: Call, cstate: Call.State, message: String) in
			if (cstate == .PushIncomingReceived){
				if (call1 == nil) {
					expectPush1Incoming.fulfill()
					call1 = call
				} else if (call2 == nil) {
					expectPush2Incoming.fulfill()
					call2 = call
				}
			}
		})
		pauline.core.addDelegate(delegate: receivedPushDelegate)
		_ = marie.core.invite(url: paulineAddress)
		self.waitForExpectations(timeout: 30)
		call1 = nil // this will crash if the extra unref is still there. If not, it means we fixed it.
		XCTAssertNotNil(call2?.callLog?.callId)
		call2 = nil
	}
	
	/*
	func testReproduceHeapCorruption() {
		let marie = LinphoneTestUser(rcFile: "marie_rc")
		
		let pauline = LinphoneTestUser(rcFile: "pauline_rc")
	
		let paulineAddress = pauline.core.defaultAccount!.params!.identityAddress!.asString()
		let expectCall = expectation(description: "Sip call received")
		let ensureSipInviteDelegate = CoreDelegateStub(onCallStateChanged: { (lc: Core, call: Call, cstate: Call.State, message: String) in
			if (cstate == .IncomingReceived) {
				expectCall.fulfill()
			}
		})
		pauline.core.addDelegate(delegate: ensureSipInviteDelegate)
		
		let call = marie.core.invite(url: paulineAddress)
		waitForExpectations(timeout: 3)
		let expectCallTerminated = self.expectation(description: "Call terminated expectation - iteration")
		let callTerminatedDelegate = CallDelegateStub(onStateChanged: { (thisCall: Call, state: Call.State, message : String) in
			if (state == Call.State.Released) {
				expectCallTerminated.fulfill()
			}
		})
		call?.addDelegate(delegate: callTerminatedDelegate)
		try! call!.terminate()
		self.waitForExpectations(timeout: 10)
	}
*/
	
	func testNoVoipTokenInRegistrationWhenPushAreDisabled() {
		let marie = LinphoneTestUser(rcFile: "marie_rc")
		let ensureNotRegisteredAgainExp = expectation(description: "Check that Marie does not register with voip token")
		ensureNotRegisteredAgainExp.isInverted = true // Inverted expectation, this test ensures that Marie does not register with a voip token
	
		marie.waitForRegistration(registeredExpectation: ensureNotRegisteredAgainExp) {
			self.waitForExpectations(timeout: 5)
		}
	}
	
	func testUpdateRegisterWhenPushConfigurationChanges() {
		let pauline = LinphoneTestUser(rcFile: "pauline_push_enabled_rc")
		
		pauline.waitForRegistration(registeredExpectation: expectation(description: "testUpdateRegisterWhenPushConfigurationChanges -- Registered with voip token")) {
			self.waitForExpectations(timeout: 10)
		}
		
		let paulineAccount = pauline.core.defaultAccount!
		var newPaulineParams = paulineAccount.params?.clone()
		paulineAccount.params = newPaulineParams
		
		newPaulineParams = paulineAccount.params?.clone()
		newPaulineParams?.pushNotificationConfig?.provider = "testprovider"
		paulineAccount.params = newPaulineParams
		pauline.waitForRegistration(registeredExpectation: expectation(description: "testUpdateRegisterWhenPushConfigurationChanges -- register again when changing provider")) {
			self.waitForExpectations(timeout: 10)
		}
		
		newPaulineParams = paulineAccount.params?.clone()
		newPaulineParams?.pushNotificationConfig?.param = "testparams"
		paulineAccount.params = newPaulineParams
		pauline.waitForRegistration(registeredExpectation: expectation(description: "testUpdateRegisterWhenPushConfigurationChanges -- register again when changing params")) {
			self.waitForExpectations(timeout: 10)
		}
		
		newPaulineParams = paulineAccount.params?.clone()
		newPaulineParams?.pushNotificationConfig?.prid = "testprid"
		paulineAccount.params = newPaulineParams
		pauline.waitForRegistration(registeredExpectation: expectation(description: "testUpdateRegisterWhenPushConfigurationChanges -- register again when changing prid")) {
			self.waitForExpectations(timeout: 10)
		}
	}
	
	func voipPushStopWhenDisablingPush(willDisableCorePush: Bool) { // if false, will disable account push allowed instead
		let marie = LinphoneTestUser(rcFile: "marie_rc")
		
		// ONLY A SINGLE VOIP PUSH REGISTRY EXIST PER APP.
		// IF YOU INSTANCIATE SEVERAL CORES, MAKE SURE THAT THE ONE THAT WILL PROCESS PUSH NOTIFICATION IS CREATE LAST
		let pauline = LinphoneTestUser(rcFile: "pauline_push_enabled_rc")
		
		pauline.waitForRegistration(registeredExpectation: expectation(description: "TestVoipPushCall - Registered with voip token")) {
			self.waitForExpectations(timeout: 20)
		}
		
		let paulineAddress = pauline.core.defaultAccount!.params!.identityAddress!.asString()
		pauline.stopCore(stoppedCoreExpectation: self.expectation(description: "Pauline Core Stopped")) { self.waitForExpectations(timeout: 10)	}
		
		// First we receive the push, then the sip invite, since the core is stopped
		let expectPushIncomingState = expectation(description: "Incoming Push Received")
		var receivedPushFirst = false
		let expectIncomingReceivedState = expectation(description: "Sip invite received")
		let basicPaulineIncomingCallDelegate = CoreDelegateStub(onCallStateChanged: { (lc: Core, call: Call, cstate: Call.State, message: String) in
			if (cstate == .IncomingReceived){
				XCTAssertTrue(receivedPushFirst)
				expectIncomingReceivedState.fulfill()
			} else if (cstate == .PushIncomingReceived) {
				receivedPushFirst = true
				expectPushIncomingState.fulfill()
			}
		})
		pauline.core.addDelegate(delegate: basicPaulineIncomingCallDelegate)
		
		var call = marie.core.invite(url: paulineAddress)
		self.waitForExpectations(timeout: 10)
		pauline.core.removeDelegate(delegate: basicPaulineIncomingCallDelegate)
		
		let expectCallTerminated = self.expectation(description: "Call terminated expectation - iteration")
		let callTerminatedDelegate = CallDelegateStub(onStateChanged: { (thisCall: Call, state: Call.State, message : String) in
			if (state == Call.State.Released) {
				expectCallTerminated.fulfill()
			}
		})
		call?.addDelegate(delegate: callTerminatedDelegate)
		try! call!.terminate()
		self.waitForExpectations(timeout: 10)
		
		// Now, check that we do not receive it anymore when we disable push
		if (willDisableCorePush) {
			pauline.core.pushNotificationEnabled = false
		} else {
			let newParams = pauline.core.defaultAccount!.params!.clone()!
			newParams.pushNotificationAllowed = false
			pauline.core.defaultAccount!.params = newParams
		}
		pauline.waitForRegistration(registeredExpectation: expectation(description: "TestVoipPushCall - Registered after disabling core push"), requireVoipToken: false) {
			self.waitForExpectations(timeout: 10)
		}
		pauline.stopCore(stoppedCoreExpectation: self.expectation(description: "Pauline Core Stopped")) { self.waitForExpectations(timeout: 10)	}
		
		let expectNoCall = expectation(description: "Do not receive call when push is disabled")
		expectNoCall.isInverted = true
		let ensureNoIncomingCallDelegate = CoreDelegateStub(onCallStateChanged: { (lc: Core, call: Call, cstate: Call.State, message: String) in
			expectNoCall.fulfill()
		})
		pauline.core.addDelegate(delegate: basicPaulineIncomingCallDelegate)
		call = marie.core.invite(url: paulineAddress)
		self.waitForExpectations(timeout: 5)
	}
	func testVoipPushStopWhenDisablingCorePush() {
		voipPushStopWhenDisablingPush(willDisableCorePush: true)
	}
	func testVoipPushStopWhenDisablingAccountPush() {
		voipPushStopWhenDisablingPush(willDisableCorePush: false)
	}
	
	
	var tokenReceivedExpect : XCTestExpectation!
	var pushReceivedExpect : XCTestExpectation!
	func receivedPushTokenCallback() {
		tokenReceivedExpect.fulfill()
	}
	func receivedPushNotificationCallback() {
		pushReceivedExpect.fulfill()
	}
	func testVoipPushCallWithManualManagement() {
		tokenReceivedExpect = expectation(description: "VOIP Push Token received")
		NotificationCenter.default.addObserver(self,
											   selector:#selector(self.receivedPushTokenCallback),
											   name: NSNotification.Name(rawValue: kPushTokenReceived),
											   object:nil);
		
		(UIApplication.shared.delegate as! AppDelegate).enableVoipPush()
		waitForExpectations(timeout: 5)
		let voipToken = (UIApplication.shared.delegate as! AppDelegate).voipToken!
		
		let marie = LinphoneTestUser(rcFile: "marie_rc")
		let pauline = LinphoneTestUser(rcFile: "pauline_rc")
		
		let paulineAccount = pauline.core.defaultAccount!
		let newParams = paulineAccount.params!.clone()
		newParams?.pushNotificationConfig?.voipToken = voipToken
		newParams!.contactUriParameters = "pn-prid=" + voipToken + ";pn-provider=apns.dev;pn-param=ABCD1234.belledonne.LinphoneTester.voip;pn-silent=1;pn-timeout=0"
		paulineAccount.params = newParams
		
		pauline.waitForRegistration(registeredExpectation: expectation(description: "TestVoipPushCall - Registered with voip token")) {
			self.waitForExpectations(timeout: 20)
		}
		
		NotificationCenter.default.addObserver(self,
											   selector:#selector(self.receivedPushNotificationCallback),
											   name: NSNotification.Name(rawValue: kPushNotificationReceived),
											   object:nil);
		
		let paulineAddress = pauline.core.defaultAccount!.params!.identityAddress!.asString()
		pauline.stopCore(stoppedCoreExpectation: self.expectation(description: "Pauline Core Stopped")) { self.waitForExpectations(timeout: 10)	}
		
		pushReceivedExpect = expectation(description: "VOIP Push notification received")
		var call = marie.core.invite(url: paulineAddress)
		self.waitForExpectations(timeout: 10)
	}
	
	func testAnswerCallBeforePushIsReceivedOnSecondDevice() {
		let marie = LinphoneTestUser(rcFile: "marie_rc")
		
		let basicPauline = LinphoneTestUser(rcFile: "pauline_rc")
		
		// ONLY A SINGLE VOIP PUSH REGISTRY EXIST PER APP. IF YOU INSTANCIATE SEVERAL CORES, MAKE SURE THAT THE ONE THAT WILL PROCESS PUSH NOTIFICATION IS CREATE LAST
		let pushPauline = LinphoneTestUser(rcFile: "pauline_push_enabled_rc")
		let pushTimeoutInSecond = 5
		pushPauline.core.pushIncomingCallTimeout = pushTimeoutInSecond
		pushPauline.core.defaultAccount?.params?.conferenceFactoryUri = "sip:conference@fakeserver.com"
		pushPauline.waitForRegistration(registeredExpectation: expectation(description: "testAnswerCallBeforePushIsReceivedOnSecondDevice - Registered with voip token")) {
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
		self.waitForExpectations(timeout: TimeInterval(pushTimeoutInSecond - 1))
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
		self.waitForExpectations(timeout: 10)
	}
	
	func testDeclineCallBeforeReceivingSipInvite() {
		let marie = LinphoneTestUser(rcFile: "marie_rc")
		
		// ONLY A SINGLE VOIP PUSH REGISTRY EXIST PER APP. IF YOU INSTANCIATE SEVERAL CORES, MAKE SURE THAT THE ONE THAT WILL PROCESS PUSH NOTIFICATION IS CREATE LAST
		let pauline = LinphoneTestUser(rcFile: "pauline_push_enabled_rc")
		pauline.waitForRegistration(registeredExpectation: expectation(description: "Registered with voip token")) {
			self.waitForExpectations(timeout: 20)
		}
		let paulineAddress = pauline.core.defaultAccount!.params!.identityAddress!.asString()
		
		pauline.stopCore(stoppedCoreExpectation: expectation(description: "Pauline Core Stopped")) {
			self.waitForExpectations(timeout: 10)
		}
		
		pauline.core.autoIterateEnabled = false // Disable auto iterate to ensure that we do not receive SIP invite before pauline declines the call
		
		let ensureSipInviteDelegate = CoreDelegateStub(onCallStateChanged: { (lc: Core, call: Call, cstate: Call.State, message: String) in
			if (cstate == .IncomingReceived) {
				XCTAssertFalse(true, "Should never receive sip invite before pauline declines the call")
			}
		})
		pauline.core.addDelegate(delegate: ensureSipInviteDelegate)
		
		pauline.waitForVoipPushIncoming(voipPushIncomingExpectation: expectation(description: "Incoming Push Received")) {
			marie.core.invite(url: paulineAddress)
			self.waitForExpectations(timeout: 10)
		}
		
		let paulineCall = pauline.core.currentCall
		
		let expectCallTerminated = self.expectation(description: "Call terminated expectation")
		let callTerminatedDelegate = CallDelegateStub(onStateChanged: { (thisCall: Call, state: Call.State, message : String) in
			if (state == Call.State.Released) {
				expectCallTerminated.fulfill()
			}
		})
		paulineCall?.addDelegate(delegate: callTerminatedDelegate)
		
		try! paulineCall!.decline(reason: Reason.Declined)
		pauline.core.removeDelegate(delegate: ensureSipInviteDelegate)
		pauline.core.autoIterateEnabled = true
		self.waitForExpectations(timeout: 5)
	}
	
	func testAcceptCallBeforeReceivingSipInvite() {
		let marie = LinphoneTestUser(rcFile: "marie_rc")
		marie.core.pushNotificationEnabled = false
		
		// ONLY A SINGLE VOIP PUSH REGISTRY EXIST PER APP. IF YOU INSTANCIATE SEVERAL CORES, MAKE SURE THAT THE ONE THAT WILL PROCESS PUSH NOTIFICATION IS CREATE LAST
		let pauline = LinphoneTestUser(rcFile: "pauline_push_enabled_rc")
		pauline.waitForRegistration(registeredExpectation: expectation(description: "Registered with voip token")) {
			self.waitForExpectations(timeout: 10)
		}
		let paulineAddress = pauline.core.defaultAccount!.params!.identityAddress!.asString()
		
		pauline.stopCore(stoppedCoreExpectation: expectation(description: "Pauline Core Stopped")) {
			self.waitForExpectations(timeout: 10)
		}
	
		pauline.core.autoIterateEnabled = false // Disable auto iterate to ensure that we do not receive SIP invite before pauline declines the call
		
		let ensureSipInviteDelegate = CoreDelegateStub(onCallStateChanged: { (lc: Core, call: Call, cstate: Call.State, message: String) in
			if (cstate == .IncomingReceived) {
				XCTAssertFalse(true, "Should never receive sip invite before pauline accepts the call")
			}
		})
		pauline.core.addDelegate(delegate: ensureSipInviteDelegate)
		
		pauline.waitForVoipPushIncoming(voipPushIncomingExpectation: expectation(description: "Incoming Push Received")) {
			marie.core.invite(url: paulineAddress)
			self.waitForExpectations(timeout: 10)
		}
		
		let paulineCall = pauline.core.currentCall
		
		let expectCallRunning = self.expectation(description: "Call running expectation")
		let callRunningDelegate = CallDelegateStub(onStateChanged: { (thisCall: Call, state: Call.State, message : String) in
			if (state == Call.State.StreamsRunning) {
				expectCallRunning.fulfill()
			}
		})
		paulineCall?.addDelegate(delegate: callRunningDelegate)
		
		try! paulineCall!.accept()
		pauline.core.removeDelegate(delegate: ensureSipInviteDelegate)
		pauline.core.autoIterateEnabled = true
		self.waitForExpectations(timeout: 5)
		paulineCall?.removeDelegate(delegate: callRunningDelegate)
		
		
		let expectCallTerminated = self.expectation(description: "Call terminated expectation")
		let callTerminatedDelegate = CallDelegateStub(onStateChanged: { (thisCall: Call, state: Call.State, message : String) in
			if (state == Call.State.Released) {
				expectCallTerminated.fulfill()
			}
		})
		paulineCall?.addDelegate(delegate: callTerminatedDelegate)
		
		try! paulineCall!.terminate()
		self.waitForExpectations(timeout: 5)
	}
	
	// Class wide expectations and functions to use for chatroom tests, since it requires intervention of the application delegate to receive the device token

	func testChatroom() {
		tokenReceivedExpect = expectation(description: "Push Token received")
		tokenReceivedExpect.assertForOverFulfill = false
		NotificationCenter.default.addObserver(self,
											   selector:#selector(self.receivedPushTokenCallback),
											   name: NSNotification.Name(rawValue: kRemotePushTokenReceived),
											   object:nil);
		
		UIApplication.shared.registerForRemoteNotifications()
		waitForExpectations(timeout: 15)

		let marie = linphone_core_manager_new("marie_rc")
		let marieCore = Core.getSwiftObject(cObject: marie!.pointee.lc)
		let pauline = linphone_core_manager_new("pauline_push_enabled_rc")
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
		
		waitForExpectations(timeout: 10)
	}
 }

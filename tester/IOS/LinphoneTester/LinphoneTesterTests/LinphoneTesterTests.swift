
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
import CallKit

class IncomingPushTest: XCTestCase {
	
	var tokenReceivedExpect : XCTestExpectation!
	//var core : Core!
	//var creator : AccountCreator!
	//var creatorCallbacks = AccountCreatorCallbacks()
	
	/*
	override func setUp() {
		super.setUp()
		let factory = Factory.Instance // Instanciate
		let configDir = factory.getConfigDir(context: nil)
		core = try! factory.createCore(configPath: "\(configDir)/MyConfig", factoryConfigPath: "", systemContext: nil)
		core.autoIterateEnabled = true
		core.pushNotificationEnabled = true
		core.config!.setString(section: "sip", key: "flexiapi_url", value: "http://centos7-quentin.local/flexiapi/api/")
		try! core.start()
		
		creator = try! core.createAccountCreator(xmlrpcUrl: "http://centos7-quentin.local/flexisip-account-manager/xmlrpc.php")
		
	}
	
	func receivedPushCallback(_ notification: Notification) {
		let payload = notification.userInfo!["customPayload"] as! [String : String]
		creator.token = payload["token"]! as String
		tokenReceivedExpect.fulfill()
	}
	
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
	
	class AccountCreatorCallbacks : AccountCreatorDelegate {
		var expect : XCTestExpectation!
		
		func onSendToken(creator: AccountCreator, status: AccountCreator.Status, response: String) {
			XCTAssert(status == AccountCreator.Status.RequestOk)
			expect.fulfill()
		}
		func onCreateAccount(creator: AccountCreator, status: AccountCreator.Status, response: String) {
			XCTAssert(status == AccountCreator.Status.RequestOk)
			expect.fulfill()
		}
	}
	*/
	
	func testIncomingCall() {
		let callKitProviderDelegate = CallKitProviderDelegate()
		callKitProviderDelegate.expectIncomingCall = expectation(description: "Incoming Callkit Call Received")
		
		let factory = Factory.Instance // Instanciate
		let configDir = factory.getConfigDir(context: nil)
		let core = try! factory.createCore(configPath: "\(configDir)/MyConfig", factoryConfigPath: "", systemContext: nil)

		// main loop for receiving notifications and doing background linphonecore work:
		core.autoIterateEnabled = true
		core.pushNotificationEnabled = true
		
		// This is necessary to register to the server and handle push Notifications. Make sure you have a certificate to match your app's bundle ID.
		//let pushConfig = mCore.pushNotificationConfig!
		//pushConfig.provider = "apns.dev"
		
		let proxy_cfg = try! core.createProxyConfig()
		let address = try! factory.createAddress(addr: "sip:quentindev@sip.linphone.org")
		let info = try! factory.createAuthInfo(username: address.username, userid: "", passwd: "dev", ha1: "", realm: "", domain: address.domain)
		core.addAuthInfo(info: info)

		try! proxy_cfg.setIdentityaddress(newValue: address)
		let server_addr = "sip:" + address.domain + ";transport=tls"
		try! proxy_cfg.setServeraddr(newValue: server_addr)
		proxy_cfg.registerEnabled = true
		proxy_cfg.pushNotificationAllowed = true
		try! core.addProxyConfig(config: proxy_cfg)
		if ( core.defaultProxyConfig == nil)
		{
			// IMPORTANT : default proxy config setting MUST be done AFTER adding the config to the core !
			core.defaultProxyConfig = proxy_cfg
		}
		try? core.start()
		
		waitForExpectations(timeout: 1000)
	}
	
	func testCall() {
		let marie = linphone_core_manager_create("marie_rc")
		let marieCore = Core.getSwiftObject(cObject: marie!.pointee.lc)
		let pauline = linphone_core_manager_create("pauline_rc")
		let paulineCore = Core.getSwiftObject(cObject: pauline!.pointee.lc)
		
		let paulineAccount = paulineCore.defaultAccount!
		let newParams = paulineAccount.params?.clone()
		newParams!.pushNotificationAllowed = true
		paulineAccount.params = newParams
		
		//paulineCore.pushNotificationEnabled = true
		//paulineCore.callkitEnabled = true
		marieCore.config!.setInt(section: "tester", key: "test_env", value: 0)
		paulineCore.config!.setInt(section: "tester", key: "test_env", value: 0)
		try! marieCore.start()
		try! paulineCore.start()
		
		DispatchQueue.global().async {
			while (true) {
				usleep(20000)
				marieCore.iterate()
			}
		}
		DispatchQueue.global().async {
			while (true) {
				usleep(20000)
				paulineCore.iterate()
			}
		}
		
		let expMarieRegistered = expectation(description: "Marie Registered")
		let expPaulineRegistered = expectation(description: "Pauline Registered")

		let marieDelegate = AccountDelegateStub(onRegistrationStateChanged: { (account: Account, state: RegistrationState, message: String) in
			if (state == RegistrationState.Ok) {
				expMarieRegistered.fulfill()
			}
		})
		let marieAccount = marieCore.defaultAccount!
		marieAccount.addDelegate(delegate: marieDelegate)
		
		let paulineDelegate = AccountDelegateStub(onRegistrationStateChanged: { (account: Account, state: RegistrationState, message: String) in
			if (state == RegistrationState.Ok) {
				expPaulineRegistered.fulfill()
			}
		})
		paulineAccount.addDelegate(delegate: paulineDelegate)
		
		waitForExpectations(timeout: 1000)
		
		let paulineAddress = Address.getSwiftObject(cObject: pauline!.pointee.identity)
		
		let callKitProviderDelegate = CallKitProviderDelegate()
		callKitProviderDelegate.expectIncomingCall = expectation(description: "Incoming Callkit Call Received")
		
		var call = marieCore.inviteAddress(addr: paulineAddress)
		waitForExpectations(timeout: 1000)
	}
	
 }

class CallKitProviderDelegate : NSObject
{
	private let provider: CXProvider
	let mCallController = CXCallController()
	var incomingCallUUID : UUID!
	var expectIncomingCall : XCTestExpectation!
	override init()
	{
		let providerConfiguration = CXProviderConfiguration(localizedName: Bundle.main.infoDictionary!["CFBundleName"] as! String)
		providerConfiguration.supportsVideo = true
		providerConfiguration.supportedHandleTypes = [.generic]
		providerConfiguration.maximumCallsPerCallGroup = 1
		providerConfiguration.maximumCallGroups = 1
		
		provider = CXProvider(configuration: providerConfiguration)
		super.init()
		provider.setDelegate(self, queue: nil) // The CXProvider delegate will trigger CallKit related callbacks
	}
	/*
	func incomingCall()
	{
		incomingCallUUID = UUID()
		let update = CXCallUpdate()
		update.remoteHandle = CXHandle(type:.generic, value: "CallInc")
		provider.reportNewIncomingCall(with: incomingCallUUID, update: update, completion: { error in }) // Report to CallKit a call is incoming
	}
	*/
}


// In this extension, we implement the action we want to be done when CallKit is notified of something.
// This can happen through the CallKit GUI in the app, or directly in the code (see outgoingCall(), incomingCall(), stopCall() functions above)
extension CallKitProviderDelegate: CXProviderDelegate {
	
	func provider(_ provider: CXProvider, perform action: CXAnswerCallAction) {
		action.fulfill()
	}
	
	func provider(_ provider: CXProvider, perform action: CXEndCallAction) {}
	func provider(_ provider: CXProvider, perform action: CXSetHeldCallAction) {}
	func provider(_ provider: CXProvider, perform action: CXStartCallAction) {}
	func provider(_ provider: CXProvider, perform action: CXSetMutedCallAction) {}
	func provider(_ provider: CXProvider, perform action: CXPlayDTMFCallAction) {}
	func provider(_ provider: CXProvider, timedOutPerforming action: CXAction) {}
	func providerDidReset(_ provider: CXProvider) {}
}


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
		let factory = Factory.Instance // Instanciate
		let configDir = factory.getConfigDir(context: nil)
		let core = try! factory.createCore(configPath: "\(configDir)/MyConfig", factoryConfigPath: "", systemContext: nil)

		// main loop for receiving notifications and doing background linphonecore work:
		core.autoIterateEnabled = true
		core.pushNotificationEnabled = true
		
		try? core.start()
		
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
		
		let expectIncomingCall = expectation(description: "Incoming Call Received")
		let expectPushIncoming = expectation(description: "Incoming Push Received")
		let coreDelegate = CoreDelegateStub(onCallStateChanged: { (lc: Core, call: Call, cstate: Call.State, message: String) in
			print("CallTrace - \(cstate)")
			if (cstate == .PushIncomingReceived){
				expectPushIncoming.fulfill()
			}
			else if (cstate == .IncomingReceived) {
				expectIncomingCall.fulfill()
			}
		})
		core.addDelegate(delegate: coreDelegate)
		waitForExpectations(timeout: 1000)
	}
	
	func testCall() {
		let marie = linphone_core_manager_new("marie_rc")
		let marieCore = Core.getSwiftObject(cObject: marie!.pointee.lc)
		let pauline = linphone_core_manager_new("pauline_rc")
		let paulineCore = Core.getSwiftObject(cObject: pauline!.pointee.lc)
		
		let paulineAccount = paulineCore.defaultAccount!
		let newParams = paulineAccount.params?.clone()
		newParams!.pushNotificationAllowed = true
		paulineAccount.params = newParams
		
		marieCore.autoIterateEnabled = true
		paulineCore.autoIterateEnabled = true
		
		
		let paulineAddress = Address.getSwiftObject(cObject: pauline!.pointee.identity)
		
		let unlockExpec = expectation(description: "Test")
		unlockExpec.assertForOverFulfill = false
		let paulineRegisterDelegate = AccountDelegateStub(onRegistrationStateChanged: { (account: Account, state: RegistrationState, message: String) in
			if (account.params!.pushNotificationConfig?.voipToken != nil) {
				unlockExpec.fulfill()
			}
		})
		paulineAccount.addDelegate(delegate: paulineRegisterDelegate)
		waitForExpectations(timeout: 5000)
		//let callKitProviderDelegate = CallKitProviderDelegate()
		
		let expectPushIncoming = expectation(description: "Incoming Push Received")
		let paulineCallDelegate = CoreDelegateStub(onCallStateChanged: { (lc: Core, call: Call, cstate: Call.State, message: String) in
			if (cstate == .PushIncomingReceived){
				expectPushIncoming.fulfill()
			}
		})
		paulineCore.addDelegate(delegate: paulineCallDelegate)
		var call = marieCore.inviteAddress(addr: paulineAddress)
		waitForExpectations(timeout: 5000)
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

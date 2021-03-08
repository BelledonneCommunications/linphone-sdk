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

/*
@interface IncomingPushTest : LinphoneTesterBase
@end

@implementation IncomingPushTest
+ (void)initialize {
}

- (void)testIncomingPush {
	NSData *pushToken = ((AppDelegate*) UIApplication.sharedApplication.delegate).pushToken;
	
	const char *tokenData = [pushToken bytes];
	NSMutableString *stringDeviceToken = [NSMutableString string];
	for (NSUInteger i = 0; i < [pushToken length]; i++) {
		[stringDeviceToken appendFormat:@"%02.2hhX", tokenData[i]];
	}
	NSString *appId = [[NSBundle mainBundle] bundleIdentifier];
	
	LinphoneCoreManager * marie = linphone_core_manager_new("account_creator_rc");
	LinphoneAccountCreator *creator = linphone_account_creator_new(marie->lc, "http://subscribe.example.org:8082/flexisip-account-manager/xmlrpc.php");
	
	linphone_account_creator_set_pn_provider(creator, "apns.dev");
	linphone_account_creator_set_pn_param(creator, appId.UTF8String);
	linphone_account_creator_set_pn_prid(creator, tokenData);
	
	char *provider = linphone_account_creator_get_pn_provider(creator);
	char *param = linphone_account_creator_get_pn_param(creator);
	char *prid = linphone_account_creator_get_pn_prid(creator);
	
}
@end
*/


class IncomingPushTest: XCTestCase {
	
	var expect : XCTestExpectation!
	
	func receivedPushCallback(_ notification: Notification) {
		debugPrint(notification.name)
		expect.fulfill()
		
	}
	
	func testIncomingPush() {
		
		NotificationCenter.default.addObserver(self,
											   selector:#selector(self.receivedPushCallback),
											   name: NSNotification.Name(rawValue: kPushReceivedEvent),
											   object:nil);
		
		let pnPrid = (UIApplication.shared.delegate as! AppDelegate).pushToken! as String
		let pnParam = Bundle.main.bundleIdentifier! as String
		
		expect = expectation(description: "Test")
		
		let factory = Factory.Instance // Instanciate
		let configDir = factory.getConfigDir(context: nil)
		let core = try! factory.createCore(configPath: "\(configDir)/MyConfig", factoryConfigPath: "", systemContext: nil)

		// main loop for receiving notifications and doing background linphonecore work:
		core.autoIterateEnabled = true
		core.pushNotificationEnabled = true
		
		/*
		let creator = try! core.createAccountCreator(xmlrpcUrl: "http://centos7-quentin.local:8082/flexisip-account-manager/xmlrpc.php")
		creator.pnParam = pnParam
		creator.pnPrid = pnPrid
		creator.pnProvider = "apns.dev"
		
		let accountDelegate = AccountCallbacks()
		accountDelegate.expect = expect
		
		creator.addDelegate(delegate: accountDelegate)
		try! creator.createValidationAccount()
		try! core.start()
		*/
		waitForExpectations(timeout: 60)
	}
	
}

class AccountCallbacks : AccountCreatorDelegate {
	
	func onCreateValidationAccount(creator: AccountCreator, status: AccountCreator.Status, response: String) {
		if (status == AccountCreator.Status.ValidationAccountCreated) {
			debugPrint("pouet");
		}
	}
}

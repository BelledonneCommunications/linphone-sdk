//
//  LinphoneTesterTests.m
//  LinphoneTesterTests
//
//  Created by Danmei Chen on 13/02/2019.
//  Copyright © 2019 belledonne. All rights reserved.
//

#import <XCTest/XCTest.h>
#include "linphone/linphonecore.h"
#include "linphonetester/liblinphone_tester.h"
#import "NSObject+DTRuntime.h"
//#import "Utils.h"
#import "Log.h"

@interface LinphoneTesterBase : XCTestCase
@end

@implementation LinphoneTesterBase

+ (NSString *)safetyTestString:(NSString *)testString {
    NSCharacterSet *charactersToRemove = [[NSCharacterSet alphanumericCharacterSet] invertedSet];
    return [[testString componentsSeparatedByCharactersInSet:charactersToRemove] componentsJoinedByString:@"_"];
}

void dummy_logger(const char *domain, OrtpLogLevel lev, const char *fmt, va_list args) {
}

+ (void)initialize {
}

+ (void)testForSuite:(NSString *)sSuite forAsync:(BOOL)async{
    LOGI(@"[message] Launching tests from suite %@", sSuite);
    const char *suite = sSuite.UTF8String;
    bc_tester_register_suite_by_name(suite);
    int test_count = bc_tester_nb_tests(suite);
    for (int k = 0; k < test_count; k++) {
        const char *test = bc_tester_test_name(suite, k);
        if (test) {
            NSString *sTest = [NSString stringWithUTF8String:test];
            
            // prepend "test_" so that it gets found by introspection
            NSString *safesTest = [LinphoneTesterBase safetyTestString:sTest];
            NSString *safesSuite = [LinphoneTesterBase safetyTestString:sSuite];
            // ordering tests
            NSString *safesIndex = nil;
            if (k < 10) {
                safesIndex = [NSString stringWithFormat:@"00%d",k];
            } else if (k <100) {
                safesIndex = [NSString stringWithFormat:@"0%d",k];
            } else if (k <1000) {
                safesIndex = [NSString stringWithFormat:@"%d",k];
            }
            NSString *selectorName = [NSString stringWithFormat:@"test%@_%@__%@", safesIndex, safesSuite, safesTest];
            
			[self addInstanceMethodWithSelectorName:selectorName
			block:^(LinphoneTesterBase *myself) {
				[myself testForSuiteTest:sSuite andTest:sTest forAsync:async];
			}];
        }
    }
}

- (void)testForSuiteTest:(NSString *)suite andTest:(NSString *)test forAsync:(BOOL)async {
    LOGI(@"[message] Launching test %@ from suite %@", test, suite);
	if (async) {
		XCTestExpectation *exp = [self expectationWithDescription:test];
		dispatch_async(dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0), ^{
			XCTAssertFalse(bc_tester_run_tests(suite.UTF8String, test.UTF8String, NULL), @"Suite '%@' / Test '%@' failed", suite, test);
			[exp fulfill];
		});

		[self waitForExpectationsWithTimeout:120 handler:^(NSError *error) {
			// handle failure
			LOGI(@"[message] Suite '%@' / Test '%@' failed", test, suite);
		}];
	} else {
		XCTAssertFalse(bc_tester_run_tests(suite.UTF8String, test.UTF8String, NULL), @"Suite '%@' / Test '%@' failed",
		suite, test);
	}
}

@end

@interface SetupTests : LinphoneTesterBase
@end

@implementation SetupTests
+ (void)initialize {
    [self testForSuite:@"Setup" forAsync:FALSE];
}
@end

@interface TunnelTests : LinphoneTesterBase
@end

@implementation TunnelTests
+ (void)initialize {
    [self testForSuite:@"Tunnel" forAsync:FALSE];
}
@end

@interface OfferAnswerTests : LinphoneTesterBase
@end

@implementation OfferAnswerTests
+ (void)initialize {
    [self testForSuite:@"Offer-answer" forAsync:FALSE];
}
@end

@interface PresenceTests : LinphoneTesterBase
@end

@implementation PresenceTests
+ (void)initialize {
    [self testForSuite:@"Presence" forAsync:FALSE];
}
@end

@interface AccountCreatorTests : LinphoneTesterBase
@end

@implementation AccountCreatorTests
+ (void)initialize {
    [self testForSuite:@"Account creator" forAsync:FALSE];
}
@end

@interface ConferenceEventTests : LinphoneTesterBase
@end

@implementation ConferenceEventTests
+ (void)initialize {
    [self testForSuite:@"Conference event" forAsync:FALSE];
}
@end

// TODO enable LogCollection in IOS
/*@interface LogCollectionTests : LinphoneTesterBase
@end

@implementation LogCollectionTests
+ (void)initialize {
    [self testForSuite:@"LogCollection"];
}
@end*/

@interface PlayerTests : LinphoneTesterBase
@end

@implementation PlayerTests
+ (void)initialize {
    [self testForSuite:@"Player" forAsync:FALSE];
}
@end

@interface DTMFTests : LinphoneTesterBase
@end

@implementation DTMFTests
+ (void)initialize {
    [self testForSuite:@"DTMF" forAsync:FALSE];
}
@end

/*@interface CpimTests : LinphoneTesterBase
 @end
 
 @implementation CpimTests
 + (void)initialize {
 [self testForSuite:@"Cpim"];
 }
 @end*/

@interface MultipartTests : LinphoneTesterBase
@end

@implementation MultipartTests
+ (void)initialize {
    [self testForSuite:@"Multipart" forAsync:FALSE];
}
@end

@interface ClonableObjectTests : LinphoneTesterBase
@end

@implementation ClonableObjectTests
+ (void)initialize {
    [self testForSuite:@"ClonableObject" forAsync:FALSE];
}
@end

@interface MainDbTests : LinphoneTesterBase
@end

@implementation MainDbTests
+ (void)initialize {
    [self testForSuite:@"MainDb" forAsync:FALSE];
}
@end

@interface PropertyContainerTests : LinphoneTesterBase
@end

@implementation PropertyContainerTests
+ (void)initialize {
    [self testForSuite:@"PropertyContainer" forAsync:FALSE];
}
@end

@interface ProxyConfigTests : LinphoneTesterBase
@end

@implementation ProxyConfigTests
+ (void)initialize {
    [self testForSuite:@"Proxy config" forAsync:FALSE];
}
@end

@interface VCardTests : LinphoneTesterBase
@end

@implementation VCardTests
+ (void)initialize {
    [self testForSuite:@"VCard" forAsync:FALSE];
}
@end

@interface VideoCallTests : LinphoneTesterBase
@end

@implementation VideoCallTests
+ (void)initialize {
    [self testForSuite:@"Video Call" forAsync:FALSE];
}
@end

@interface AudioBypassTests : LinphoneTesterBase
@end

@implementation AudioBypassTests
+ (void)initialize {
    [self testForSuite:@"Audio Bypass" forAsync:FALSE];
}
@end

@interface ContentsTests : LinphoneTesterBase
@end

@implementation ContentsTests
+ (void)initialize {
    [self testForSuite:@"Contents" forAsync:FALSE];
}
@end

@interface VideoTests : LinphoneTesterBase
@end

@implementation VideoTests
+ (void)initialize {
    [self testForSuite:@"Video" forAsync:FALSE];
}
@end

@interface MulticastCallTests : LinphoneTesterBase
@end

@implementation MulticastCallTests
+ (void)initialize {
    [self testForSuite:@"Multicast Call" forAsync:FALSE];
}
@end

@interface StunTests : LinphoneTesterBase
@end

@implementation StunTests
+ (void)initialize {
    [self testForSuite:@"Stun" forAsync:FALSE];
}
@end

@interface EventTests : LinphoneTesterBase
@end

@implementation EventTests
+ (void)initialize {
    [self testForSuite:@"Event" forAsync:FALSE];
}
@end

@interface MessageTests : LinphoneTesterBase
@end

@implementation MessageTests
+ (void)initialize {
    [self testForSuite:@"Message" forAsync:FALSE];
}
@end

@interface RemoteProvisioningTests : LinphoneTesterBase
@end

@implementation RemoteProvisioningTests
+ (void)initialize {
    [self testForSuite:@"RemoteProvisioning" forAsync:FALSE];
}
@end

@interface QualityReportingTests : LinphoneTesterBase
@end

@implementation QualityReportingTests
+ (void)initialize {
    [self testForSuite:@"QualityReporting" forAsync:FALSE];
}
@end

@interface RegisterTests : LinphoneTesterBase
@end

@implementation RegisterTests
+ (void)initialize {
    [self testForSuite:@"Register"forAsync:TRUE];
}
@end

@interface GroupChatTests : LinphoneTesterBase
@end

@implementation GroupChatTests
+ (void)initialize {
    [self testForSuite:@"Group Chat" forAsync:FALSE];
}
@end

@interface SecureGroupChatTests : LinphoneTesterBase
@end

@implementation SecureGroupChatTests
+ (void)initialize {
	[self testForSuite:@"Secure group chat" forAsync:FALSE];
}
@end

@interface FlexisipTests : LinphoneTesterBase
@end

@implementation FlexisipTests
+ (void)initialize {
    [self testForSuite:@"Flexisip" forAsync:FALSE];
}
@end

@interface MultiCallTests : LinphoneTesterBase
@end

@implementation MultiCallTests
+ (void)initialize {
    [self testForSuite:@"Multi call" forAsync:FALSE];
}
@end

@interface SecureCallTests : LinphoneTesterBase
@end

@implementation SecureCallTests
+ (void)initialize {
	[self testForSuite:@"Secure Call" forAsync:FALSE];
}
@end

@interface SingleCallTests : LinphoneTesterBase
@end

@implementation SingleCallTests
+ (void)initialize {
    [self testForSuite:@"Single Call" forAsync:FALSE];
}
@end

@interface VideoCallQualityTests : LinphoneTesterBase
@end

@implementation VideoCallQualityTests
+ (void)initialize {
	[self testForSuite:@"Video Call quality" forAsync:FALSE];
}
@end

@interface PresenceUsingServerTests : LinphoneTesterBase
@end

@implementation PresenceUsingServerTests
+ (void)initialize {
    [self testForSuite:@"Presence using server" forAsync:FALSE];
}
@end

@interface CallRecoveryTests : LinphoneTesterBase
@end

@implementation CallRecoveryTests
+ (void)initialize {
    [self testForSuite:@"Call recovery" forAsync:FALSE];
}
@end

@interface CallWithICETests : LinphoneTesterBase
@end

@implementation CallWithICETests
+ (void)initialize {
    [self testForSuite:@"Call with ICE" forAsync:FALSE];
}
@end

@interface UtilsTests : LinphoneTesterBase
@end

@implementation UtilsTests
+ (void)initialize {
	[self testForSuite:@"Utils" forAsync:FALSE];
}
@end




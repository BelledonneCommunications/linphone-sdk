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

+ (void)testForSuite:(NSString *)sSuite {
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
                                                  [myself testForSuiteTest:sSuite andTest:sTest];
                                              }];
        }
    }
}

- (void)testForSuiteTest:(NSString *)suite andTest:(NSString *)test {
    LOGI(@"[message] Launching test %@ from suite %@", test, suite);
    XCTAssertFalse(bc_tester_run_tests(suite.UTF8String, test.UTF8String, NULL), @"Suite '%@' / Test '%@' failed",
                   suite, test);
}

+ (void)testForSuiteAsync:(NSString *)sSuite {
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
                                                  [myself testForSuiteTestAsync:sSuite andTest:sTest];
                                              }];
        }
    }
}

- (void)testForSuiteTestAsync:(NSString *)suite andTest:(NSString *)test {
    LOGI(@"[message] Launching test %@ from suite %@", test, suite);
	XCTestExpectation *exp = [self expectationWithDescription:test];

	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0), dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0), ^{
		 XCTAssertFalse(bc_tester_run_tests(suite.UTF8String, test.UTF8String, NULL), @"Suite '%@' / Test '%@' failed",
						  suite, test);
		[exp fulfill];
	});

	[self waitForExpectationsWithTimeout:120 handler:^(NSError *error) {
		// handle failure
		NSLog( @"Suite '%@' / Test '%@' failed", suite, test);
	}];
}

@end

@interface SetupTests : LinphoneTesterBase
@end

@implementation SetupTests
+ (void)initialize {
    [self testForSuite:@"Setup"];
}
@end

@interface TunnelTests : LinphoneTesterBase
@end

@implementation TunnelTests
+ (void)initialize {
    [self testForSuite:@"Tunnel"];
}
@end

@interface OfferAnswerTests : LinphoneTesterBase
@end

@implementation OfferAnswerTests
+ (void)initialize {
    [self testForSuite:@"Offer-answer"];
}
@end

@interface PresenceTests : LinphoneTesterBase
@end

@implementation PresenceTests
+ (void)initialize {
    [self testForSuite:@"Presence"];
}
@end

@interface AccountCreatorTests : LinphoneTesterBase
@end

@implementation AccountCreatorTests
+ (void)initialize {
    [self testForSuite:@"Account creator"];
}
@end

@interface ConferenceEventTests : LinphoneTesterBase
@end

@implementation ConferenceEventTests
+ (void)initialize {
    [self testForSuite:@"Conference event"];
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
    [self testForSuite:@"Player"];
}
@end

@interface DTMFTests : LinphoneTesterBase
@end

@implementation DTMFTests
+ (void)initialize {
    [self testForSuite:@"DTMF"];
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
    [self testForSuite:@"Multipart"];
}
@end

@interface ClonableObjectTests : LinphoneTesterBase
@end

@implementation ClonableObjectTests
+ (void)initialize {
    [self testForSuite:@"ClonableObject"];
}
@end

@interface MainDbTests : LinphoneTesterBase
@end

@implementation MainDbTests
+ (void)initialize {
    [self testForSuite:@"MainDb"];
}
@end

@interface PropertyContainerTests : LinphoneTesterBase
@end

@implementation PropertyContainerTests
+ (void)initialize {
    [self testForSuite:@"PropertyContainer"];
}
@end

@interface ProxyConfigTests : LinphoneTesterBase
@end

@implementation ProxyConfigTests
+ (void)initialize {
    [self testForSuite:@"Proxy config"];
}
@end

@interface VCardTests : LinphoneTesterBase
@end

@implementation VCardTests
+ (void)initialize {
    [self testForSuite:@"VCard"];
}
@end

@interface VideoCallTests : LinphoneTesterBase
@end

@implementation VideoCallTests
+ (void)initialize {
    [self testForSuite:@"Video Call"];
}
@end

@interface AudioBypassTests : LinphoneTesterBase
@end

@implementation AudioBypassTests
+ (void)initialize {
    [self testForSuite:@"Audio Bypass"];
}
@end

@interface AudioQualityTests : LinphoneTesterBase
@end

@implementation AudioQualityTests
+ (void)initialize {
	[self testForSuite:@"Audio Call quality"];
}
@end

@interface AudioRoutesTests : LinphoneTesterBase
@end

@implementation AudioRoutesTests
+ (void)initialize {
	[self testForSuite:@"Audio Routes"];
}
@end

@interface AudioVideoConferenceTests : LinphoneTesterBase
@end

@implementation AudioVideoConferenceTests
+ (void)initialize {
	[self testForSuite:@"Audio video conference"];
}
@end

@interface ContentsTests : LinphoneTesterBase
@end

@implementation ContentsTests
+ (void)initialize {
    [self testForSuite:@"Contents"];
}
@end

@interface VideoTests : LinphoneTesterBase
@end

@implementation VideoTests
+ (void)initialize {
    [self testForSuite:@"Video"];
}
@end

@interface MulticastCallTests : LinphoneTesterBase
@end

@implementation MulticastCallTests
+ (void)initialize {
    [self testForSuite:@"Multicast Call"];
}
@end

@interface StunTests : LinphoneTesterBase
@end

@implementation StunTests
+ (void)initialize {
    [self testForSuite:@"Stun"];
}
@end

@interface EventTests : LinphoneTesterBase
@end

@implementation EventTests
+ (void)initialize {
    [self testForSuite:@"Event"];
}
@end

@interface MessageTests : LinphoneTesterBase
@end

@implementation MessageTests
+ (void)initialize {
    [self testForSuite:@"Message"];
}
@end

@interface RemoteProvisioningTests : LinphoneTesterBase
@end

@implementation RemoteProvisioningTests
+ (void)initialize {
    [self testForSuite:@"RemoteProvisioning"];
}
@end

@interface QualityReportingTests : LinphoneTesterBase
@end

@implementation QualityReportingTests
+ (void)initialize {
    [self testForSuite:@"QualityReporting"];
}
@end

@interface RegisterTests : LinphoneTesterBase
@end

@implementation RegisterTests
+ (void)initialize {
    [self testForSuite:@"Register"];
}
@end

@interface GroupChatTests : LinphoneTesterBase
@end

@implementation GroupChatTests
+ (void)initialize {
    [self testForSuite:@"Group Chat"];
}
@end

@interface SecureGroupChatTests : LinphoneTesterBase
@end

@implementation SecureGroupChatTests
+ (void)initialize {
	[self testForSuite:@"Secure group chat"];
}
@end

@interface FlexisipTests : LinphoneTesterBase
@end

@implementation FlexisipTests
+ (void)initialize {
    [self testForSuite:@"Flexisip"];
}
@end

@interface MultiCallTests : LinphoneTesterBase
@end

@implementation MultiCallTests
+ (void)initialize {
    [self testForSuite:@"Multi call"];
}
@end

@interface SecureCallTests : LinphoneTesterBase
@end

@implementation SecureCallTests
+ (void)initialize {
	[self testForSuite:@"Secure Call"];
}
@end

@interface SingleCallTests : LinphoneTesterBase
@end

@implementation SingleCallTests
+ (void)initialize {
    [self testForSuite:@"Single Call"];
}
@end

@interface VideoCallQualityTests : LinphoneTesterBase
@end

@implementation VideoCallQualityTests
+ (void)initialize {
	[self testForSuite:@"Video Call quality"];
}
@end

@interface PresenceUsingServerTests : LinphoneTesterBase
@end

@implementation PresenceUsingServerTests
+ (void)initialize {
    [self testForSuite:@"Presence using server"];
}
@end

@interface CallRecoveryTests : LinphoneTesterBase
@end

@implementation CallRecoveryTests
+ (void)initialize {
    [self testForSuite:@"Call recovery"];
}
@end

@interface CallWithICETests : LinphoneTesterBase
@end

@implementation CallWithICETests
+ (void)initialize {
    [self testForSuite:@"Call with ICE"];
}
@end

@interface UtilsTests : LinphoneTesterBase
@end

@implementation UtilsTests
+ (void)initialize {
	[self testForSuite:@"Utils"];
}
@end

@interface SharedCoreTests : LinphoneTesterBase
@end

@implementation SharedCoreTests
+ (void)initialize {
	[self testForSuiteAsync:@"Shared Core"];
}
@end

@interface PushIncomingCallTests : LinphoneTesterBase
@end

@implementation PushIncomingCallTests
+ (void)initialize {
	[self testForSuiteAsync:@"Push Incoming Call"];
}
@end


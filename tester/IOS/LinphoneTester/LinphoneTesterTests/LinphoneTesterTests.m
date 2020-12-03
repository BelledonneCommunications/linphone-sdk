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


//
//  AppDelegate.m
//  LinphoneTester
//
//  Created by Danmei Chen on 13/02/2019.
//  Copyright © 2019 belledonne. All rights reserved.
//

#import "AppDelegate.h"
//#import <XCTest/XCTest.h>
#include "linphone/linphonecore.h"
#include "linphonetester/liblinphone_tester.h"
#import "NSObject+DTRuntime.h"
//#import "Utils.h"
#import "Log.h"

@interface AppDelegate ()

@end

@implementation AppDelegate

//+ (NSString *)safetyTestString:(NSString *)testString {
//    NSCharacterSet *charactersToRemove = [[NSCharacterSet alphanumericCharacterSet] invertedSet];
//    return [[testString componentsSeparatedByCharactersInSet:charactersToRemove] componentsJoinedByString:@"_"];
//}
//
//void dummy_logger(const char *domain, OrtpLogLevel lev, const char *fmt, va_list args) {
//}
//
//+ (void)initialize {
//}
//
//+ (void)testForSuite:(NSString *)sSuite {
//    LOGI(@"[message] Launching tests from suite %@", sSuite);
//    const char *suite = sSuite.UTF8String;
//    bc_tester_register_suite_by_name(suite);
//    int test_count = bc_tester_nb_tests(suite);
//    for (int k = 0; k < test_count; k++) {
//        const char *test = bc_tester_test_name(suite, k);
//        if (test) {
//            NSString *sTest = [NSString stringWithUTF8String:test];
//
//            // prepend "test_" so that it gets found by introspection
//            NSString *safesTest = [self safetyTestString:sTest];
//            NSString *safesSuite = [self safetyTestString:sSuite];
//            // ordering tests
//            NSString *safesIndex = nil;
//            if (k < 10) {
//                safesIndex = [NSString stringWithFormat:@"00%d",k];
//            } else if (k <100) {
//                safesIndex = [NSString stringWithFormat:@"0%d",k];
//            } else if (k <1000) {
//                safesIndex = [NSString stringWithFormat:@"%d",k];
//            }
//            NSString *selectorName = [NSString stringWithFormat:@"test%@_%@__%@", safesIndex, safesSuite, safesTest];
//
//            [self addInstanceMethodWithSelectorName:selectorName
//                                              block:^(AppDelegate *myself) {
//                                                  [myself testForSuiteTest:sSuite andTest:sTest];
//                                              }];
//        }
//    }
//}

- (void)testForSuiteTest:(NSString *)suite andTest:(NSString *)test {
    LOGI(@"[message] Launching test %@ from suite %@", test, suite);
	bc_tester_register_suite_by_name(suite.UTF8String);
	bc_tester_run_tests(suite.UTF8String, test.UTF8String, NULL);
}

//int main(int argc, char *argv[]) {
//	liblinphone_tester_init(NULL);
//	[Log enableLogs:0];
//#ifdef DEBUG
//    NSSetUncaughtExceptionHandler(&uncaughtExceptionHandler);
//#endif
//    int i;
//    for(i = 1; i < argc; ++i) {
//        if (strcmp(argv[i], "--verbose") == 0) {
//            [Log enableLogs:ORTP_MESSAGE];
//        } else if (strcmp(argv[i],"--log-file")==0){
//#if TARGET_OS_SIMULATOR
//            char *xmlFile = bc_tester_file("LibLinphoneIOS.xml");
//            char *args[] = {"--xml-file", xmlFile};
//            bc_tester_parse_args(2, args, 0);
//
//            char *logFile = bc_tester_file("LibLinphoneIOS.txt");
//            liblinphone_tester_set_log_file(logFile);
//#endif
//        } else if (strcmp(argv[i],"--no-ipv6")==0){
//            liblinphonetester_ipv6 = FALSE;
//        }
//    }
//    @autoreleasepool {
//        return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
//    }
//}


- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    // Override point for customization after application launch.
//	[AppDelegate testForSuite:@"Shared Core"];
	
	
	
//
//
//	liblinphone_tester_init(NULL);
//
//
//	liblinphone_tester_keep_accounts(TRUE);
//
//	   NSString *bundlePath = [NSString stringWithFormat:@"%@/liblinphone_tester/", [[NSBundle mainBundle] bundlePath]] ;
//	   NSArray *paths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
//	   NSString *writablePath = [paths objectAtIndex:0];
//
//	   bc_tester_set_resource_dir_prefix([bundlePath UTF8String]);
//	   bc_tester_set_writable_dir_prefix([writablePath UTF8String]);
//
//
//
////	[Log enableLogs:0];
////	char *xmlFile = bc_tester_file("LibLinphoneIOS.xml");
////	char *args[] = {"--xml-file", xmlFile};
////	bc_tester_parse_args(2, args, 0);
////	char *logFile = bc_tester_file("LibLinphoneIOS.txt");
////	liblinphone_tester_set_log_file(logFile);
//
////	[AppDelegate addInstanceMethodWithSelectorName:@"testSharedCore"
////	block:^(AppDelegate *myself) {
////		[myself testForSuiteTest:@"Shared Core" andTest:@"Executor Shared Core can't start because Main Shared Core runs"];
////	}];
//	[self testForSuiteTest:@"Shared Core" andTest:@"Executor Shared Core can't start because Main Shared Core runs"];
////	bc_tester_register_suite_by_name(@"Shared Core".UTF8String);
////	bc_tester_run_tests(@"Shared Core".UTF8String, @"Executor Shared Core can't start because Main Shared Core runs".UTF8String, NULL);
    return YES;
}


- (void)applicationWillResignActive:(UIApplication *)application {
    // Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
    // Use this method to pause ongoing tasks, disable timers, and invalidate graphics rendering callbacks. Games should use this method to pause the game.
}


- (void)applicationDidEnterBackground:(UIApplication *)application {
    // Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later.
    // If your application supports background execution, this method is called instead of applicationWillTerminate: when the user quits.
}


- (void)applicationWillEnterForeground:(UIApplication *)application {
    // Called as part of the transition from the background to the active state; here you can undo many of the changes made on entering the background.
	
}


- (void)applicationDidBecomeActive:(UIApplication *)application {
    // Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
}


- (void)applicationWillTerminate:(UIApplication *)application {
    // Called when the application is about to terminate. Save data if appropriate. See also applicationDidEnterBackground:.
}




@end

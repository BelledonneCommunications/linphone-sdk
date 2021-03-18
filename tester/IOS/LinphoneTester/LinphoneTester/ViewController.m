//
//  ViewController.m
//  LinphoneTester
//
//  Created by Danmei Chen on 13/02/2019.
//  Copyright © 2019 belledonne. All rights reserved.
//

#import "ViewController.h"
#import "Log.h"
#include "TargetConditionals.h"
#include "linphonetester/liblinphone_tester.h"


@interface ViewController ()
@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view, typically from a nib.
    
    liblinphone_tester_keep_accounts(TRUE);

	NSString *bundlePath = [self getResourceDirPath:@"com.belledonne-communications.linphonetester" resource:@"images"];
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
    NSString *writablePath = [paths objectAtIndex:0];
    
    bc_tester_set_resource_dir_prefix([bundlePath UTF8String]);
    bc_tester_set_writable_dir_prefix([writablePath UTF8String]);
}

- (void)testForSuiteTest:(NSString *)suite andTest:(NSString *)test {
    LOGI(@"[message] Launching test %@ from suite %@", test, suite);
	bc_tester_register_suite_by_name(suite.UTF8String);
	bc_tester_run_tests(suite.UTF8String, test.UTF8String, NULL);
}

-(NSString *)getResourceDirPath: (NSString *)framework resource: (NSString *)resource {
	CFStringEncoding encodingMethod = CFStringGetSystemEncoding();
	CFStringRef cfFramework = CFStringCreateWithCString(NULL, framework.UTF8String, encodingMethod);
	CFStringRef cfResource = CFStringCreateWithCString(NULL, resource.UTF8String, encodingMethod);
	CFBundleRef bundle = CFBundleGetBundleWithIdentifier(cfFramework);
	CFURLRef resourceUrl = CFBundleCopyResourceURL(bundle, cfResource, NULL, NULL);
	NSString *path = @"";
	if (resourceUrl) {
		CFURLRef resourceUrlDirectory = CFURLCreateCopyDeletingLastPathComponent(NULL, resourceUrl);
		CFStringRef resourcePath = CFURLCopyFileSystemPath(resourceUrlDirectory, kCFURLPOSIXPathStyle);
		path =  [NSString stringWithCString:CFStringGetCStringPtr(resourcePath, encodingMethod) encoding:encodingMethod];
		CFRelease(resourcePath);
		CFRelease(resourceUrlDirectory);
		CFRelease(resourceUrl);
	}

	CFRelease(cfResource);
	CFRelease(cfFramework);
	return path;
}

- (IBAction)onClick:(id)sender {
	liblinphone_tester_init(NULL);
	[self testForSuiteTest:@"Shared Core" andTest:@"Executor Shared Core get new chat room from invite"];
}
@end

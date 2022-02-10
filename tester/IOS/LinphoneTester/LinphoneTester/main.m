//
//  main.m
//  LinphoneTester
//
//  Created by guillaume on 28/05/2014.
//
//

#import <UIKit/UIKit.h>
#import "AppDelegate.h"
#import "Log.h"
#import <os/log.h>
#include "linphonetester/liblinphone_tester.h"

#ifdef DEBUG

// Dump exception
void uncaughtExceptionHandler(NSException *exception) {
    NSLog(@"Crash: %@", exception);
    NSLog(@"Stack Trace: %@", [exception callStackSymbols]);
    // Internal error reporting
};

#endif

int main(int argc, char *argv[]) {
	liblinphone_tester_init(NULL);
	[Log enableLogs:0];
#ifdef DEBUG
    NSSetUncaughtExceptionHandler(&uncaughtExceptionHandler);
#endif
    int i;
    for(i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--verbose") == 0) {
            [Log enableLogs:ORTP_MESSAGE];
        } else if (strcmp(argv[i],"--log-file")==0){
#if TARGET_OS_SIMULATOR
            char *xmlFile = bc_tester_file("LibLinphoneIOS.xml");
            char *args[] = {"--xml-file", xmlFile};
            bc_tester_parse_args(2, args, 0);
            
            char *logFile = bc_tester_file("LibLinphoneIOS.txt");
            liblinphone_tester_set_log_file(logFile);
#endif
        } else if (strcmp(argv[i],"--no-ipv6")==0){
            liblinphonetester_ipv6 = FALSE;
        } else if (strcmp(argv[i],"--show-account-manager-logs")==0){
			liblinphonetester_show_account_manager_logs=TRUE;
        }
    }
	flexisip_tester_dns_ip_addresses = bctbx_list_new((void *)"5.135.31.162");
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
    }
}

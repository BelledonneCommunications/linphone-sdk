//
//  AppDelegate.m
//  LinphoneTester
//
//  Created by Danmei Chen on 13/02/2019.
//  Copyright © 2019 belledonne. All rights reserved.
//

#import "AppDelegate.h"
#include "linphonetester/liblinphone_tester.h"

NSString *const kPushTokenReceived = @"PushTokenReceived";
NSString *const kRemotePushTokenReceived = @"RemotePushTokenReceived";
NSString *const kPushNotificationReceived = @"PushNotificationReceived";
@interface AppDelegate ()

@end

@implementation AppDelegate


- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    // Override point for customization after application launch.
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

- (void)application:(UIApplication *)application
didRegisterForRemoteNotificationsWithDeviceToken:(NSData *)deviceToken {
	const char *tokenData = [deviceToken bytes];
	NSMutableString *stringDeviceToken = [NSMutableString string];
	for (NSUInteger i = 0; i < [deviceToken length]; i++) {
		[stringDeviceToken appendFormat:@"%02.2hhX", tokenData[i]];
	}
	
	linphone_factory_set_user_data(linphone_factory_get(), (__bridge void*)deviceToken);
	[NSNotificationCenter.defaultCenter postNotificationName:kRemotePushTokenReceived object:self];
}
- (void)application:(UIApplication *)application
didFailToRegisterForRemoteNotificationsWithError:(NSError *)error {
}

- (void)application:(UIApplication *)application
didReceiveRemoteNotification:(NSDictionary *)userInfo
fetchCompletionHandler:(void (^)(UIBackgroundFetchResult result))completionHandler {
	[NSNotificationCenter.defaultCenter postNotificationName:kPushNotificationReceived object:self];
}

- (void)pushRegistry:(PKPushRegistry *)registry didInvalidatePushTokenForType:(NSString *)type
{
}
 
- (void)pushRegistry:(PKPushRegistry *)registry didReceiveIncomingPushWithPayload:(PKPushPayload *)payload forType:(NSString *)type
{
	[NSNotificationCenter.defaultCenter postNotificationName:kPushNotificationReceived object:self];
}
 
- (void)pushRegistry:(PKPushRegistry *)registry didUpdatePushCredentials:(PKPushCredentials *)credentials forType:(NSString *)type
{
	NSMutableString *tokenString = nil;
	const unsigned char *tokenBuffer = (const unsigned char *)[credentials.token bytes];
	tokenString = [NSMutableString stringWithCapacity:[credentials.token length] * 2];
	for (unsigned long i = 0; i < [credentials.token length]; ++i) {
		[tokenString appendFormat:@"%02X", (unsigned int)tokenBuffer[i]];
	}
	[tokenString appendFormat:@":%@",@"voip"];
	_voipToken = tokenString;
	[NSNotificationCenter.defaultCenter postNotificationName:kPushTokenReceived object:self];
}

- (void) enableVoipPush {
	_voipRegistry = [[PKPushRegistry alloc] initWithQueue:dispatch_get_main_queue()];
	_voipRegistry.delegate = self;
	_voipRegistry.desiredPushTypes = [NSSet setWithObject:PKPushTypeVoIP];
}
@end

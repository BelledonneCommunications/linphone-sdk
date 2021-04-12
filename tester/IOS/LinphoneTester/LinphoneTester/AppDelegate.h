//
//  AppDelegate.h
//  LinphoneTester
//
//  Created by Danmei Chen on 13/02/2019.
//  Copyright © 2019 belledonne. All rights reserved.
//

#import <UIKit/UIKit.h>

extern NSString *const kPushTokenReceived;
extern NSString *const kPushNotificationReceived;
static void *rawDeviceToken = NULL;
@interface AppDelegate : UIResponder <UIApplicationDelegate>

@property (strong, nonatomic) UIWindow *window;
@property NSData* pushDeviceToken;

@end


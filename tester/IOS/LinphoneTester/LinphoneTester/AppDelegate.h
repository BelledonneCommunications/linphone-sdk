//
//  AppDelegate.h
//  LinphoneTester
//
//  Created by Danmei Chen on 13/02/2019.
//  Copyright © 2019 belledonne. All rights reserved.
//

#import <UIKit/UIKit.h>
#import <PushKit/PushKit.h>

extern NSString *const kPushTokenReceived;
extern NSString *const kPushNotificationReceived;
extern NSString *const kRemotePushTokenReceived;
@interface AppDelegate : UIResponder <UIApplicationDelegate, PKPushRegistryDelegate>

@property (nonatomic, strong) PKPushRegistry* voipRegistry;
@property (strong, nonatomic) UIWindow *window;
@property NSString* voipToken;

//
-(void) enableVoipPush;

@end


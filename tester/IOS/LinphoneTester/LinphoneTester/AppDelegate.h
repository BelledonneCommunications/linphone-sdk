//
//  AppDelegate.h
//  LinphoneTester
//
//  Created by Danmei Chen on 13/02/2019.
//  Copyright © 2019 belledonne. All rights reserved.
//

#import <UIKit/UIKit.h>

extern NSString *const kPushReceivedEvent;

@interface AppDelegate : UIResponder <UIApplicationDelegate>

@property (strong, nonatomic) UIWindow *window;
@property NSMutableString *pushToken;

@end

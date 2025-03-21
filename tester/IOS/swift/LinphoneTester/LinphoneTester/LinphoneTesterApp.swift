//
//  LinphoneTesterApp.swift
//  LinphoneTester
//
//  Created by QuentinArguillere on 24/02/2025.
//

import SwiftUI
import Foundation
import linphonesw

let kPushTokenReceived = Notification.Name("PushTokenReceived")
let kRemotePushTokenReceived = Notification.Name("RemotePushTokenReceived")
let kPushNotificationReceived = Notification.Name("PushNotificationReceived")

class AppDelegate: NSObject, UIApplicationDelegate, UNUserNotificationCenterDelegate {
    func application(_ application: UIApplication, didRegisterForRemoteNotificationsWithDeviceToken deviceToken: Data) {
        print("Received remote push token")
        let tokenStr = deviceToken.map { String(format: "%02.2hhx", $0) }.joined()
        NotificationCenter.default.post(name: kRemotePushTokenReceived, object: nil, userInfo: ["token": "\(tokenStr):remote"])
    }
    
    func application(_ application: UIApplication, didReceiveRemoteNotification userInfo: [AnyHashable: Any], fetchCompletionHandler completionHandler: @escaping (UIBackgroundFetchResult) -> Void) {
        print("Received remote push notification")
        NotificationCenter.default.post(name: kPushNotificationReceived, object: nil)
        completionHandler(UIBackgroundFetchResult.newData)
    }
    
    func application(_ application: UIApplication, didFailToRegisterForRemoteNotificationsWithError error: Error) {
        print("Failed to register for remote notifications: \(error)")
    }
}


@main
struct LinphoneTesterApp: App {
    @UIApplicationDelegateAdaptor(AppDelegate.self) var delegate
    var body: some Scene {
        WindowGroup {
            EmptyView() // ContentView()
        }
    }
}

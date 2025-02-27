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

class LinphoneTesterInitializer {
    @UIApplicationDelegateAdaptor(AppDelegate.self) var delegate
    var rawFlexisipTesterdnsIpAddresses: UnsafeMutableRawPointer!
    
    init() {
        liblinphone_tester_init(nil)
        
        
        // fs-test-9.linphone.org: 178.32.112.28
        let dnsData = "178.32.112.28".data(using: .utf8)!
        let rawFlexisipTesterdnsIpAddresses = UnsafeMutableRawPointer.allocate(byteCount: dnsData.count, alignment: MemoryLayout<UInt8>.alignment)
        dnsData.copyBytes(to: rawFlexisipTesterdnsIpAddresses.assumingMemoryBound(to: UInt8.self), count: dnsData.count)
        flexisip_tester_dns_ip_addresses = bctbx_list_new(rawFlexisipTesterdnsIpAddresses)
        
        liblinphonetester_show_account_manager_logs = 1
        liblinphone_tester_keep_accounts(1);
        
        
        let testerUrl = Bundle.main.url(forResource: "Frameworks/linphonetester.framework", withExtension: nil)!
        bc_tester_set_resource_dir_prefix(testerUrl.relativePath)
        let cacheUrl = FileManager.default.urls(for: .cachesDirectory, in: .userDomainMask).first!.relativePath
        bc_tester_set_writable_dir_prefix(cacheUrl)

        UNUserNotificationCenter.current().requestAuthorization(options: [.alert, .sound, .badge]) { granted, error in
            if let error = error {
                print("Error requesting notification permissions: \(error)")
            }
            print("Notification permissions granted: \(granted)")
            /*
            if granted {
                DispatchQueue.main.async {
                    UIApplication.shared.registerForRemoteNotifications()
                }
            } */
        }
    }
    deinit {
        rawFlexisipTesterdnsIpAddresses.deallocate()
    }
}

@main
struct LinphoneTesterApp: App {
    let testerInitializer = LinphoneTesterInitializer()
    
    var body: some Scene {
        WindowGroup {
            EmptyView() // ContentView()
        }
    }
}

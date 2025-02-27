//
//  ManualPushkitManagement.swift
//  LinphoneTester
//
//  Created by QuentinArguillere on 07/03/2025.
//

import UIKit
import PushKit

class ManualPushRegistry: NSObject, PKPushRegistryDelegate {
    
    let pushRegistry = PKPushRegistry(queue: .main)
    var voipToken: String?
    
    func enableVoipPush() {
        pushRegistry.delegate = self
        pushRegistry.desiredPushTypes = [.voIP]
        UIApplication.shared.registerForRemoteNotifications()
    }
    
    func pushRegistry(_ registry: PKPushRegistry, didUpdate pushCredentials: PKPushCredentials, for type: PKPushType) {
        voipToken = pushCredentials.token.map { String(format: "%02.2hhx", $0) }.joined() + ":voip"
        NotificationCenter.default.post(name: kPushTokenReceived, object: nil)
        Log.info("Manual push registry properly received VOIP token")
    }
    
    func pushRegistry(_ registry: PKPushRegistry, didReceiveIncomingPushWith payload: PKPushPayload, for type: PKPushType) async {
        Log.info("Manual push registry properly received push notification")
        NotificationCenter.default.post(name: kPushNotificationReceived, object: nil)
    }
    
    func pushRegistry(_ registry: PKPushRegistry, didReceiveError error: Error, for type: PKPushType) {
        Log.error("Push registry error: \(error.localizedDescription)")
    }
}


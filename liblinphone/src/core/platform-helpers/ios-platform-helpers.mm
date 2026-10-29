/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of Liblinphone
 * (see https://gitlab.linphone.org/BC/public/liblinphone).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include <bctoolbox/defs.h>

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif
#if TARGET_OS_IPHONE

#include <AVFoundation/AVAudioSession.h>
#include <CoreLocation/CoreLocation.h>
#include <Foundation/Foundation.h>
#include <Network/Network.h>
#include <SystemConfiguration/CaptiveNetwork.h>
#include <belr/grammarbuilder.h>
#include <future>
#include <mutex>
#include <optional>

#include "c-wrapper/c-wrapper.h"
#include "core/core-p.h"
#include "core/core.h"
#include "linphone/core.h"
#include "linphone/utils/general.h"
#include "linphone/utils/utils.h"

#include "logger/logger.h"
#include "mac-platform-helpers.h"

// TODO: Remove me
#include "private.h"

#include "core/app/ios-app-delegate.h"

#import <AVFoundation/AVFoundation.h>
// =============================================================================

using namespace std;

LINPHONE_BEGIN_NAMESPACE

class IosPlatformHelpers : public MacPlatformHelpers, public CoreListener {
public:
  IosPlatformHelpers(std::shared_ptr<LinphonePrivate::Core> core,
                     void *systemContext);
  ~IosPlatformHelpers() {
    stopPathMonitor();
    [mAppDelegate dealloc];
  }

  void acquireCpuLock() override;
  void releaseCpuLock() override;

  void *getPathContext() override;
  string getPhysicalDeviceIdentifier() const override;

  NetworkType getNetworkType() const override;
  string getWifiSSID() override;
  void setWifiSSID(const string &ssid) override;

  void setVideoPreviewWindow(BCTBX_UNUSED(void *windowId)) override {}
  string getDownloadPath() override {
    return Utils::getEmptyConstRefObject<string>();
  }
  void setVideoWindow(BCTBX_UNUSED(void *windowId)) override {}
  void setParticipantDeviceVideoWindow(
      BCTBX_UNUSED(LinphoneParticipantDevice *participantDevice),
      BCTBX_UNUSED(void *windowId)) override{};
  void resizeVideoPreview(BCTBX_UNUSED(int width),
                          BCTBX_UNUSED(int height)) override {}

  void onWifiOnlyEnabled(bool enabled) override;
  bool isActiveNetworkWifiOnlyCompliant() const override;
  void setDnsServers() override;
  bool startNetworkMonitoring() override;
  void stopNetworkMonitoring() override;

  void onLinphoneCoreStart(bool monitoringEnabled) override;
  void onLinphoneCoreStop() override;

  void startAudioForEchoTestOrCalibration() override;
  void stopAudioForEchoTestOrCalibration() override;

  void start(std::shared_ptr<LinphonePrivate::Core> core) override;
  void stop(void) override;

  // IosHelper specific
  void onNetworkChanged(bool reachable, bool force);

  void didRegisterForRemotePush(void *token) override;
  void didRegisterForRemotePushWithStringifiedToken(const char *token) override;
  void setPushAndAppDelegateDispatchQueue(void *dispatchQueue) override;
  void enableAutoIterate(bool autoIterateEnabled) override;

  void onRecordingStarted() const override;
  void onRecordingPaused() const override;
  void stopRinging() const override;

  void setDeviceRotation(int orientation) const override;

private:
  // Snapshot of the default network path, as delivered by the path monitor.
  struct NetworkPath {
    nw_path_status_t status;
    bool cellular;

    bool operator!=(const NetworkPath &other) const {
      return status != other.status || cellular != other.cellular;
    }
  };

  void kickOffConnectivity();
  void bgTaskTimeout();
  static void sBgTaskTimeout(void *data);
  void onGlobalStateChanged(LinphoneGlobalState state) override;
  void startPathMonitor();
  void stopPathMonitor();
  void onPathUpdate(nw_path_t nwPath);
  void networkChangeCallback(const NetworkPath &path);
  bool isReachable(const NetworkPath &path);

  long int mCpuLockTaskId;
  int mCpuLockCount;
  bool mNetworkMonitoringEnabled = false;
  nw_path_monitor_t mPathMonitor = nullptr;
  dispatch_queue_t mPathMonitorQueue = nullptr;
  std::future<void> mPathMonitorCancelled;
  mutable std::mutex mPathMutex;
  std::optional<NetworkPath> mLastPath; // Guarded by mPathMutex, written on mPathMonitorQueue.
  bool mNotifyPathChanges = false; // Guarded by mPathMutex, set while network monitoring is started.
  // Last path handled by networkChangeCallback(), on the core thread. Starts
  // as "no network", so that the first usable path forces a reinit.
  NetworkPath mCurrentPath{nw_path_status_unsatisfied, false};

  IosAppDelegate *mAppDelegate =
      NULL; /* auto didEnterBackground/didEnterForeground and other callbacks */
  bool mStart =
      false; /* generic platformhelper's funcs only work when mStart is true */
  bool mUseAppDelgate = false; /* app delegate is only used by main core*/
  dispatch_source_t mIterateTimer = NULL;
};

// =============================================================================

IosPlatformHelpers::IosPlatformHelpers(
    std::shared_ptr<LinphonePrivate::Core> core,
    BCTBX_UNUSED(void *systemContext))
    : MacPlatformHelpers(core) {
  mUseAppDelgate = core->getCCore()->is_main_core;
  if (mUseAppDelgate) {
    mAppDelegate = [[IosAppDelegate alloc] initWithCore:core];
  }
  ms_message("IosPlatformHelpers is fully initialised");
}

void IosPlatformHelpers::start(std::shared_ptr<LinphonePrivate::Core> core) {
  mCpuLockCount = 0;
  mCpuLockTaskId = 0;
  mNetworkReachable = 0; // wait until monitor to give a status;
  mSharedCoreHelpers = createIosSharedCoreHelpers(core);
  startPathMonitor();

  ms_message("IosPlatformHelpers is fully started");
  mStart = true;
  [mAppDelegate onStopAsyncEnd:false];
}

void IosPlatformHelpers::stop() {
  mStart = false;
  ms_message("IosPlatformHelpers is fully stopped");
}

// Make sure that we register for push notification token after the core is
// GlobalOn in order to avoid push token being erased by remote provisioning
void IosPlatformHelpers::onGlobalStateChanged(LinphoneGlobalState state) {
  if (state == LinphoneGlobalOn) {
    if (mUseAppDelgate &&
        linphone_core_is_push_notification_enabled(getCore()->getCCore())) {
      [mAppDelegate registerForPush];
    }
  }
}

void IosPlatformHelpers::didRegisterForRemotePush(void *token) {
  [mAppDelegate didRegisterForRemotePush:(NSData *)token];
}

void IosPlatformHelpers::didRegisterForRemotePushWithStringifiedToken(
    const char *tokenStr) {
  [mAppDelegate didRegisterForRemotePushWithStringifiedToken:tokenStr];
}

void IosPlatformHelpers::setPushAndAppDelegateDispatchQueue(
    void *dispatchQueue) {
  [mAppDelegate setCoreQueue:dispatchQueue];
}

void IosPlatformHelpers::enableAutoIterate(bool autoIterateEnabled) {
  if (mUseAppDelgate && mStart) {
    if (autoIterateEnabled) {
      if (mIterateTimer) {
        ms_message("[IosPlatformHelpers] core.iterate() is already scheduled");
        return;
      }

      mIterateTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0,
                                             [mAppDelegate getCoreQueue]);
      if (!mIterateTimer) {
        ms_error("[IosPlatformHelpers] Failed to create dispatch source timer "
                 "for auto iterate. This is a critical error, as nothing will "
                 "proceed without calls to core iterate.");
        return;
      }

      dispatch_source_set_timer(mIterateTimer,
                                dispatch_time(DISPATCH_TIME_NOW, 0),
                                20 * NSEC_PER_MSEC, // 20ms interval
                                1 * NSEC_PER_MSEC); // 1ms leeway

      dispatch_source_set_event_handler(mIterateTimer, ^{
        if (mAppDelegate) {
          [mAppDelegate iterate];
        }
      });

      ms_message(
          "[IosPlatformHelpers] Call to core.iterate() scheduled every 20ms");

      // Start the timer
      dispatch_resume(mIterateTimer);

    } else {
      if (mIterateTimer) {
        dispatch_source_cancel(mIterateTimer);
        mIterateTimer = NULL;
        ms_message("[IosPlatformHelpers] Auto core.iterate() stopped");
      }
    }
  }
}

void IosPlatformHelpers::onRecordingStarted() const {}

void IosPlatformHelpers::onRecordingPaused() const {}

void IosPlatformHelpers::stopRinging() const {}

void IosPlatformHelpers::setDeviceRotation(
    BCTBX_UNUSED(int orientation)) const {}

// -----------------------------------------------------------------------------

void IosPlatformHelpers::bgTaskTimeout() {
  ms_error("IosPlatformHelpers: the system requests that the cpu lock is "
           "released now.");
  if (mCpuLockTaskId != 0) {
    belle_sip_end_background_task(static_cast<unsigned long>(mCpuLockTaskId));
    mCpuLockTaskId = 0;
  }
}

void IosPlatformHelpers::sBgTaskTimeout(void *data) {
  IosPlatformHelpers *zis = static_cast<IosPlatformHelpers *>(data);
  zis->bgTaskTimeout();
}

// -----------------------------------------------------------------------------

void IosPlatformHelpers::acquireCpuLock() {
  if (!mStart)
    return;

  // on iOS, cpu lock is implemented by a long running task and it is abstracted
  // by belle-sip, so let's use belle-sip directly.
  if (mCpuLockCount == 0)
    mCpuLockTaskId = static_cast<long>(belle_sip_begin_background_task(
        "Liblinphone cpu lock", sBgTaskTimeout, this));

  mCpuLockCount++;
}

void IosPlatformHelpers::releaseCpuLock() {
  if (!mStart)
    return;

  mCpuLockCount--;
  if (mCpuLockCount != 0)
    return;

  if (mCpuLockTaskId == 0) {
    ms_error("IosPlatformHelpers::releaseCpuLock(): too late, the lock has "
             "been released already by the system.");
    return;
  }

  belle_sip_end_background_task(static_cast<unsigned long>(mCpuLockTaskId));
  mCpuLockTaskId = 0;
}

void *IosPlatformHelpers::getPathContext() {
  return getSharedCoreHelpers()->getPathContext();
}

string IosPlatformHelpers::getPhysicalDeviceIdentifier() const {
  NSUUID *identifierForVendor = [[UIDevice currentDevice] identifierForVendor];
  if (identifierForVendor) {
    NSString *identifierString = [identifierForVendor UUIDString];
    return std::string([identifierString UTF8String]);
  }
  ms_warning("[IosPlatformHelpers] Did not manage to retrieve this device's "
             "identifierForVendor. This could prevent the proper detection of "
             "a device change in case of configuration cloning");
  return "unavailable_identifier";
}

void IosPlatformHelpers::onLinphoneCoreStart(bool monitoringEnabled) {
  if (!mStart)
    return;

  mNetworkMonitoringEnabled = monitoringEnabled;
  if (monitoringEnabled) {
    startNetworkMonitoring();
  }
  if (mUseAppDelgate &&
      linphone_core_is_auto_iterate_enabled(getCore()->getCCore())) {
    enableAutoIterate(TRUE);
  } else {
    ms_warning("[IosPlatformHelpers] Auto core.iterate() isn't enabled, ensure "
               "you do it in your application!");
  }

  ms_message("[IosPlatformHelpers] Register onGlobalStateChanged listener to "
             "request push notification when finished starting");
  L_GET_PRIVATE(getCore())->addListener(this);
}

void IosPlatformHelpers::onLinphoneCoreStop() {
  if (!mStart)
    return;

  if (mNetworkMonitoringEnabled) {
    stopNetworkMonitoring();
  }

  if (mUseAppDelgate &&
      linphone_core_is_auto_iterate_enabled(getCore()->getCCore())) {
    enableAutoIterate(FALSE);
  }

  try {
    L_GET_PRIVATE(getCore())->removeListener(this);
  } catch (...) {
  }
  // To avoid trigger callbacks of mHandler after linphone core stop
  [mAppDelegate onStopAsyncEnd:true];
  getSharedCoreHelpers()->onLinphoneCoreStop();
}

void IosPlatformHelpers::startAudioForEchoTestOrCalibration() {}

void IosPlatformHelpers::stopAudioForEchoTestOrCalibration() {}

void IosPlatformHelpers::onWifiOnlyEnabled(bool enabled) {
  if (!mStart)
    return;

  mWifiOnly = enabled;
  if (isNetworkReachable()) {
    // Nothing to do if we have no connection
    if (enabled && mCurrentPath.cellular) {
      onNetworkChanged(false, true);
    }
  }
}

void IosPlatformHelpers::setDnsServers() {
  // Nothing to do here, already handled by core for IOS platforms
}

// The path monitor runs all along: starting the monitoring only makes its updates reach networkChangeCallback().
bool IosPlatformHelpers::startNetworkMonitoring(void) {
	if (!mStart || !mPathMonitor) return false;

	std::optional<NetworkPath> lastPath;
	{
		std::lock_guard<std::mutex> lock(mPathMutex);
		mNotifyPathChanges = true;
		lastPath = mLastPath;
	}
	// Load and trigger initial state. If no path has been delivered yet, the first update of the monitor will.
	if (lastPath) networkChangeCallback(*lastPath);
	return true;
}

void IosPlatformHelpers::stopNetworkMonitoring(void) {
	if (!mStart) return;

	std::lock_guard<std::mutex> lock(mPathMutex);
	mNotifyPathChanges = false;
}

// Runs on the core thread.
void IosPlatformHelpers::networkChangeCallback(const NetworkPath &path) {
  if (!mStart)
    return;
  {
    // Drop an update posted just before stopNetworkMonitoring()
    std::lock_guard<std::mutex> lock(mPathMutex);
    if (!mNotifyPathChanges)
      return;
  }

  // The path monitor also delivers updates for changes not relevant here
  // (IPv6, DNS, expensive...). So take actions only if state changed of our
  // internal network-related properties

  bool reachable = isReachable(path), changed = false, force = false;
  if (mCurrentPath != path || reachable != isNetworkReachable()) {
    changed = true;
    if (mCurrentPath.status == nw_path_status_unsatisfied ||
        /*check if moving from Wifi to cellular*/
        mCurrentPath.cellular != path.cellular) {
      // Force reinit after network down
      force = true;
    }
  }
  mCurrentPath = path;
  if (!path.cellular) {
/*
 * CHECK_SSID is undefined by default. Previously we are monitoring SSID changes
 * to notify liblinphone of possible network changes. Since iOS 12, it requires
 * an annoying location permission. For that reason, the default algorithm just
 * monitors the default local ip addresses for changes. A bit less reliable
 * since we can change from wifi network but by change obtain the same ip
 * address. But much less annoying because it doesn't require to ask a
 * permission to the user.
 */
#ifdef CHECK_SSID
    // Only check for wifi changes if current connection type is Wifi
    string newSSID = getWifiSSID();
    if (newSSID.empty() || newSSID.compare(mCurrentSSID) != 0) {
      setWifiSSID(newSSID);
      changed = true;
      // We possibly changed network, force reset of transports
      force = true;
      ms_message("New Wifi SSID detected: %s",
                 mCurrentSSID.empty() ? "[none]" : mCurrentSSID.c_str());
    }
#else
    if (reachable && !force) {
      force = checkIpAddressChanged();
    }
#endif
  }
  if (!!linphone_core_automatic_http_proxy_detection_enabled(
          getCore()->getCCore())) {
    ms_message(
        "Do not try to get HTTP proxy as automatic proxy detection is enabled");
  } else {
    getHttpProxySettings();
  }
  if (mHttpProxyEnabled) {
    ms_message("Update HTTP proxy settings: using [%s:%d]",
               mHttpProxyHost.c_str(), mHttpProxyPort);
  } else {
    ms_message("Updated HTTP proxy settings: no proxy");
  }
  const char *currentProxyHostCstr =
      linphone_core_get_http_proxy_host(getCore()->getCCore());
  string currentProxyHost;
  if (currentProxyHostCstr) {
    currentProxyHost = currentProxyHostCstr;
  }
  int currentProxyPort =
      linphone_core_get_http_proxy_port(getCore()->getCCore());
  if (mHttpProxyEnabled == currentProxyHost.empty() ||
      currentProxyPort != mHttpProxyPort ||
      currentProxyHost.compare(mHttpProxyHost)) {
    // Empty host is considered as no proxy
    changed = true;
    force = true;
  }
  if (changed) {
    onNetworkChanged(reachable, force);
  }
}

// Get reachability state from given path
bool IosPlatformHelpers::isReachable(const NetworkPath &path) {
	switch (path.status) {
		case nw_path_status_satisfied:
			break;
		case nw_path_status_satisfiable:
			// No usable route yet, but a connection attempt will bring one up (VPN On Demand, for instance). Assume
			// unreachable for now. Wait for kickoff and for later network change events
			kickOffConnectivity();
			return false;
		case nw_path_status_unsatisfied:
		case nw_path_status_invalid:
			return false;
	}
	bool isWifiOnly = linphone_core_wifi_only_enabled(getCore()->getCCore());
	return !(isWifiOnly && path.cellular);
}

bool IosPlatformHelpers::isActiveNetworkWifiOnlyCompliant() const {
  return false;
}

// Method called when we detected actual network changes in callbacks
void IosPlatformHelpers::onNetworkChanged(bool reachable, bool force) {
  if (reachable != isNetworkReachable() || force) {
    ms_message("Global network status changed: reachable: [%d].",
               (int)reachable);
    setHttpProxy(mHttpProxyHost, mHttpProxyPort);
    if (force && reachable) {
      // mandatory to  trigger action from the core in case of switch from 3G to
      // wifi (both up)
      setNetworkReachable(FALSE);
    }
    setNetworkReachable(reachable);
  }
}

// In case the network path is only satisfiable (no usable route until a
// connection attempt, as with VPN On Demand), we have to use CFStream APIs for
// the connection to be opened Start
// asynchronously the connection kickoff and do not change reachability status.
// We assume we will be notified via callbacks if connection changes state
// afterwards 192.168.0.200	is just an arbitrary address. It should	still
// activate the on-demand connection even if not routable
void IosPlatformHelpers::kickOffConnectivity() {
  static bool in_progress = false;
  if (in_progress) {
    return;
  }
  in_progress = true;
  /* start a new thread to avoid blocking the main ui in case of peer host
   * failure */
  dispatch_async(
      dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        static useconds_t sleep_us = 10000;
        static int timeout_s = 5;
        bool timeout_reached = FALSE;
        CFWriteStreamRef writeStream;
        CFStreamCreatePairWithSocketToHost(
            NULL, (CFStringRef) @"192.168.0.200" /*"linphone.org"*/, 15000, nil,
            &writeStream);
        bool res = CFWriteStreamOpen(writeStream);
        const char *buff = "hello";
        time_t start = time(NULL);
        time_t loop_time;

        if (res == false) {
          ms_error("Could not open write stream, backing off");
          CFRelease(writeStream);
          in_progress = FALSE;
          return;
        }

        // check stream status and handle timeout
        CFStreamStatus status = CFWriteStreamGetStatus(writeStream);
        while (status != kCFStreamStatusOpen &&
               status != kCFStreamStatusError) {
          usleep(sleep_us);
          status = CFWriteStreamGetStatus(writeStream);
          loop_time = time(NULL);
          if (loop_time - start >= timeout_s) {
            timeout_reached = false;
            break;
          }
        }

        if (status == kCFStreamStatusOpen) {
          CFWriteStreamWrite(writeStream, (const UInt8 *)buff,
                             (CFIndex)strlen(buff));
        } else if (!timeout_reached) {
          CFErrorRef cfError = CFWriteStreamCopyError(writeStream);
          if (cfError) {
            NSError *error = (__bridge NSError *)cfError;
            lError() << "CFStream error " << error.localizedDescription;
            CFRelease(cfError);
          }
        } else if (timeout_reached) {
          ms_message("CFStream timeout reached");
        }
        CFWriteStreamClose(writeStream);
        CFRelease(writeStream);
        in_progress = false;
      });
}

static const char *pathStatusToString(nw_path_status_t status) {
	switch (status) {
		case nw_path_status_satisfied:
			return "satisfied";
		case nw_path_status_unsatisfied:
			return "unsatisfied";
		case nw_path_status_satisfiable:
			return "satisfiable";
		case nw_path_status_invalid:
			break;
	}
	return "invalid";
}

static const char *interfaceTypeToString(nw_interface_type_t type) {
	switch (type) {
		case nw_interface_type_wifi:
			return "wifi";
		case nw_interface_type_cellular:
			return "cellular";
		case nw_interface_type_wired:
			return "wired";
		case nw_interface_type_loopback:
			return "loopback";
		case nw_interface_type_other:
			break;
	}
	return "other";
}

static void logNetworkPath(nw_path_t path) {
	string interfaces;
	// In Objective-C++, the compiler converts a lambda to the block type expected by the Network framework.
	nw_path_enumerate_interfaces(path, [&interfaces](nw_interface_t interface) -> bool {
		if (!interfaces.empty()) interfaces += ", ";
		interfaces += string(nw_interface_get_name(interface)) + " (" +
		              interfaceTypeToString(nw_interface_get_type(interface)) + ")";
		return true;
	});
	nw_path_status_t status = nw_path_get_status(path);
	ms_message(
	    "[IosPlatformHelpers] Network path: status [%s], unsatisfied reason [%d], interfaces [%s], expensive [%d], "
	    "constrained [%d], IPv4 [%d], IPv6 [%d]",
	    pathStatusToString(status),
	    status == nw_path_status_unsatisfied ? (int)nw_path_get_unsatisfied_reason(path) : 0, interfaces.c_str(),
	    (int)nw_path_is_expensive(path), (int)nw_path_is_constrained(path), (int)nw_path_has_ipv4(path),
	    (int)nw_path_has_ipv6(path));
}

// The path monitor lives as long as this object, whatever auto_net_state_mon: getNetworkType() relies on it. Its
// handlers run on a private serial queue.
void IosPlatformHelpers::startPathMonitor() {
	if (mPathMonitor) return;

	mPathMonitorQueue = dispatch_queue_create("org.linphone.liblinphone.path-monitor", DISPATCH_QUEUE_SERIAL);
	mPathMonitor = nw_path_monitor_create();
	nw_path_monitor_set_queue(mPathMonitor, mPathMonitorQueue);
	nw_path_monitor_set_update_handler(mPathMonitor, [this](nw_path_t nwPath) { onPathUpdate(nwPath); });
	// The handler is copied when it becomes a block, and a promise cannot be copied: hence the shared_ptr.
	auto cancelled = std::make_shared<std::promise<void>>();
	mPathMonitorCancelled = cancelled->get_future();
	nw_path_monitor_set_cancel_handler(mPathMonitor, [cancelled]() { cancelled->set_value(); });
	nw_path_monitor_start(mPathMonitor);
	ms_message("[IosPlatformHelpers] Network path monitor started");
}

void IosPlatformHelpers::stopPathMonitor() {
	if (!mPathMonitor) return;

	nw_path_monitor_cancel(mPathMonitor);
	// The update handler uses this object. Once the cancel handler has run, the update handler cannot fire again.
	mPathMonitorCancelled.wait();
	nw_release(mPathMonitor);
	mPathMonitor = nullptr;
	dispatch_release(mPathMonitorQueue);
	mPathMonitorQueue = nullptr;
	ms_message("[IosPlatformHelpers] Network path monitor stopped");
}

// Runs on mPathMonitorQueue.
void IosPlatformHelpers::onPathUpdate(nw_path_t nwPath) {
	logNetworkPath(nwPath);
	NetworkPath path{nw_path_get_status(nwPath), nw_path_uses_interface_type(nwPath, nw_interface_type_cellular)};
	std::lock_guard<std::mutex> lock(mPathMutex);
	mLastPath = path;
	// Post under the lock: once stopNetworkMonitoring() has returned, no update can reach the core until restarted.
	if (!mNotifyPathChanges) return;
	try {
		getCore()->doLater([this, path]() { networkChangeCallback(path); });
	} catch (const std::bad_weak_ptr &) {
		// Not on the core thread: an exception must not escape the dispatch queue.
		ms_warning("[IosPlatformHelpers] Network path update ignored, the core is gone");
	}
}

PlatformHelpers::NetworkType IosPlatformHelpers::getNetworkType() const {
	// Read the last path delivered by the path monitor. It runs even with auto_net_state_mon=0, so the type matches the
	// network whose addresses the local network permission check probes. Its first update arrives a few milliseconds
	// after start(); until then, assume Wifi as before.
	std::optional<NetworkPath> path;
	{
		std::lock_guard<std::mutex> lock(mPathMutex);
		path = mLastPath;
	}
	if (!path) {
		ms_warning("[IosPlatformHelpers] No network path received yet, assuming the network type is Wifi");
		return NetworkType::Wifi;
	}
	NetworkType type = path->cellular ? NetworkType::MobileData : NetworkType::Wifi;
	ms_message("[IosPlatformHelpers] Network type is [%s] (path status [%s])",
	           type == NetworkType::MobileData ? "MobileData" : "Wifi", pathStatusToString(path->status));
	return type;
}

void IosPlatformHelpers::setWifiSSID(const string &ssid) {
  mCurrentSSID = ssid;
}

string IosPlatformHelpers::getWifiSSID(void) {
#if TARGET_IPHONE_SIMULATOR
  return "Sim_err_SSID_NotSupported";
#else
  string ssid;
  bool shallGetWifiInfo = true;

  if (@available(iOS 13.0, *)) {
    // Starting from IOS13 we need to check for authorization to get wifi
    // information. User permission is asked in the main app
    CLAuthorizationStatus status = [CLLocationManager authorizationStatus];
    if (status != kCLAuthorizationStatusAuthorizedAlways &&
        status != kCLAuthorizationStatusAuthorizedWhenInUse) {
      shallGetWifiInfo = false;
      ms_warning("User has not given authorization to access Wifi information "
                 "(Authorization: [%d])",
                 (int)status);
    }
  }
  if (shallGetWifiInfo) {
    CFArrayRef ifaceNames = CNCopySupportedInterfaces();
    if (ifaceNames) {
      CFIndex i;
      for (i = 0; i < CFArrayGetCount(ifaceNames); ++i) {
        CFStringRef iface = (CFStringRef)CFArrayGetValueAtIndex(ifaceNames, i);
        CFDictionaryRef ifaceInfo = CNCopyCurrentNetworkInfo(iface);

        if (ifaceInfo != NULL && CFDictionaryGetCount(ifaceInfo) > 0) {
          CFStringRef ifaceSSID = (CFStringRef)CFDictionaryGetValue(
              ifaceInfo, kCNNetworkInfoKeySSID);
          if (ifaceSSID != NULL) {
            ssid = toUTF8String(ifaceSSID);
            if (!ssid.empty()) {
              CFRelease(ifaceInfo);
              break;
            }
          }
          CFRelease(ifaceInfo);
        }
      }
    }
    CFRelease(ifaceNames);
  }
  return ssid;
#endif
}

// -----------------------------------------------------------------------------

PlatformHelpers *
createIosPlatformHelpers(std::shared_ptr<LinphonePrivate::Core> core,
                         void *systemContext) {
  return new IosPlatformHelpers(core, systemContext);
}

LINPHONE_END_NAMESPACE

#endif

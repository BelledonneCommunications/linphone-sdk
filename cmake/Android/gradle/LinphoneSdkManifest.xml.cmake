<?xml version="1.0" encoding="utf-8"?>
<manifest package="org.linphone.core"
    xmlns:android="http://schemas.android.com/apk/res/android"
    android:installLocation="auto">

    <!-- SDK required permissions -->
    <uses-permission android:name="android.permission.INTERNET" />
    <uses-permission android:name="android.permission.RECORD_AUDIO" />
    <uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
	<uses-permission android:name="android.permission.FOREGROUND_SERVICE" />
    <uses-permission android:name="android.permission.WAKE_LOCK" />
    <!-- Needed to be able to use WifiManager.MulticastLock -->
    <uses-permission android:name="android.permission.CHANGE_WIFI_MULTICAST_STATE" />
    <uses-permission android:name="android.permission.CAMERA" />
    <!-- Needed to allow Linphone to install on tablets, since android.permission.CAMERA implies android.hardware.camera and android.hardware.camera.autofocus are required -->
    <uses-feature android:name="android.hardware.camera" android:required="false" />
    <!-- Needed for bluetooth -->
    <uses-permission android:name="android.permission.BLUETOOTH" />
    <!-- Needed for bluetooth headset -->
    <uses-permission android:name="android.permission.MODIFY_AUDIO_SETTINGS" />
    <!-- To vibrate while incoming call -->
    <uses-permission android:name="android.permission.VIBRATE" />
    <!-- Needed to check current do not disturb policy -->
    <uses-permission android:name="android.permission.ACCESS_NOTIFICATION_POLICY" />
    <!-- Needed to gain access to network temporarily when in background on a metered network if data saver is on when a push is being received -->
    <!-- Needed for Android 14
    https://developer.android.com/about/versions/14/behavior-changes-14#fgs-types -->
    <uses-permission android:name="android.permission.FOREGROUND_SERVICE"/>
    <uses-permission android:name="android.permission.FOREGROUND_SERVICE_CAMERA" />
    <uses-permission android:name="android.permission.FOREGROUND_SERVICE_MICROPHONE" />
    <uses-permission android:name="android.permission.FOREGROUND_SERVICE_PHONE_CALL" />
    <uses-permission android:name="android.permission.FOREGROUND_SERVICE_DATA_SYNC" />
    <!-- End of SDK required permissions -->
	
</manifest>

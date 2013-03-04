APP_PROJECT_PATH := $(call my-dir)/../
APP_MODULES      :=libneon libspeex libgsm libortp libavutil libavcore libavcodec libswscale libvpx libmediastreamer2

APP_STL := stlport_static

# define linphone-root-dir to linphone base dir
BUILD_MS2 := 1
BUILD_SRTP := 1
linphone-root-dir:=$(APP_PROJECT_PATH)/../../../

APP_BUILD_SCRIPT:=$(linphone-root-dir)/jni/Android.mk
APP_PLATFORM := android-8
APP_ABI := armeabi-v7a x86
APP_CFLAGS:=-DDISABLE_NEON

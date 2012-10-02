##
## Android.mk -Android build script-
##
##
## Copyright (C) 2010  Belledonne Communications, Grenoble, France
##
##  This program is free software; you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation; either version 2 of the License, or
##  (at your option) any later version.
##
##  This program is distributed in the hope that it will be useful,
##  but WITHOUT ANY WARRANTY; without even the implied warranty of
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
##  GNU Library General Public License for more details.
##
##  You should have received a copy of the GNU General Public License
##  along with this program; if not, write to the Free Software
##  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
##

LOCAL_PATH:= $(call my-dir)/../../src
include $(CLEAR_VARS)


LOCAL_ARM_MODE := arm

MEDIASTREAMER2_INCLUDES := \
	$(LOCAL_PATH)/../build/android \
	$(LOCAL_PATH)/../include \
	$(LOCAL_PATH)/../../oRTP \
	$(LOCAL_PATH)/../../oRTP/include \
	$(LOCAL_PATH)/../../../externals/speex/include \
	$(LOCAL_PATH)/../../../externals/build/speex \
	$(LOCAL_PATH)/../../../externals/gsm/inc \
	$(LOCAL_PATH)/../../../externals/ffmpeg \
	$(LOCAL_PATH)/../../../externals/ \
	$(LOCAL_PATH)/../../../externals/build/ffmpeg \
	$(LOCAL_PATH)/../../../externals/libvpx/


LOCAL_MODULE := libmediastreamer2

LOCAL_SRC_FILES = \
	audiomixer.c \
	audioconference.c \
	mscommon.c \
	msfilter.c \
	msqueue.c \
	msticker.c \
	msvoip.c \
	alaw.c \
	ulaw.c \
	mssndcard.c \
	msfileplayer.c \
	msrtp.c \
	dtmfgen.c \
	msfilerec.c \
	ice.c \
	tee.c \
	msconf.c \
	msjoin.c \
	msvolume.c \
	mtu.c \
	mswebcam.c \
	equalizer.c \
	dsptools.c \
	kiss_fft.c \
	kiss_fftr.c \
	void.c \
	msandroid.cpp \
	eventqueue.c \
	msjava.c \
	tonedetector.c \
	audiostream.c \
	qualityindicator.c \
	bitratecontrol.c \
	bitratedriver.c \
	qosanalyzer.c \
	msg722.c \
	g722_decode.c \
	g722_encode.c \
	l16.c \
	msresample.c \
	android/loader.cpp \
	android/androidsound.cpp \
	android/AudioRecord.cpp \
	android/AudioTrack.cpp \
	android/AudioSystem.cpp

##if BUILD_ALSA
ifeq ($(strip $(BOARD_USES_ALSA_AUDIO)),true)
LOCAL_SRC_FILES += alsa.c
LOCAL_CFLAGS += -D__ALSA_ENABLED__
endif

ifeq ($(BUILD_SRTP), 1)
	LOCAL_C_INCLUDES += $(SRTP_C_INCLUDE)
else

endif

ifeq ($(LINPHONE_VIDEO),1)
LOCAL_SRC_FILES += \
	videostream.c \
	videoenc.c \
	videodec.c \
	pixconv.c  \
	sizeconv.c \
	nowebcam.c \
	h264dec.c \
	rfc3984.c \
	mire.c \
	layouts.c \
	android-display.c \
	android-display-bad.cpp \
	msandroidvideo.cpp \
	vp8.c \
	shaders.c \
	opengles_display.c \
	android-opengl-display.c \
	jpegwriter.c

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
	LOCAL_CFLAGS += -DVIDEO_ENABLED
LOCAL_SRC_FILES+= \
	scaler.c.neon \
	scaler_arm.S.neon \
	msvideo.c \
	msvideo_neon.c.neon
else
LOCAL_SRC_FILES+= 	scaler.c \
					msvideo.c 
endif
endif

#LOCAL_SRC_FILES += videostream.c
#
##if BUILD_THEORA
#LOCAL_SRC_FILES += theora.c

#if BUILD_SPEEX
LOCAL_SRC_FILES += \
	msspeex.c \
	speexec.c

##if BUILD_GSM
LOCAL_SRC_FILES += gsm.c

LOCAL_CFLAGS += \
	-UHAVE_CONFIG_H \
	-include $(LOCAL_PATH)/../build/android/libmediastreamer2_AndroidConfig.h \
	-DMS2_INTERNAL \
	-DMS2_FILTERS \
	-DINET6 \
        -DORTP_INET6 \
	-D_POSIX_SOURCE -Wall


ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
	LOCAL_CFLAGS += -DUSE_HARDWARE_RATE=1
endif


#LOCAL_CFLAGS += -DDEBUG

LOCAL_C_INCLUDES += \
	$(MEDIASTREAMER2_INCLUDES)

LOCAL_STATIC_LIBRARIES := \
	libortp \
	libspeex \
	libspeexdsp


ifeq ($(strip $(BOARD_USES_ALSA_AUDIO)),true)
LOCAL_SHARED_LIBRARIES += libasound
endif

LOCAL_STATIC_LIBRARIES += cpufeatures

ifeq ($(BUILD_MS2), 1)
	LOCAL_SRC_FILES += \
		../tests/mediastream.c

	LOCAL_STATIC_LIBRARIES += \
		libgsm
	ifeq ($(LINPHONE_VIDEO),1)
		LOCAL_STATIC_LIBRARIES += \
			libgsm \
			libvpx \
			libavcodec \
			libswscale \
			libavcore \
			libavutil
	endif

	ifeq ($(BUILD_SRTP),1)
	LOCAL_SHARED_LIBRARIES += libsrtp
	endif

	LOCAL_LDLIBS    += -lGLESv2 -llog -ldl
	include $(BUILD_SHARED_LIBRARY)
else
	include $(BUILD_STATIC_LIBRARY)
endif

$(call import-module,android/cpufeatures)

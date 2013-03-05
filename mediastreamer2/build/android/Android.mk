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
	$(LOCAL_PATH)/base \
	$(LOCAL_PATH)/utils \
	$(LOCAL_PATH)/voip \
	$(LOCAL_PATH)/audiofilters \
	$(LOCAL_PATH)/otherfilters \
	$(LOCAL_PATH)/videofilters \
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
	base/mscommon.c \
	base/msfilter.c \
	base/msqueue.c \
	base/msticker.c \
	base/mssndcard.c \
	base/mtu.c \
	base/mswebcam.c \
	base/eventqueue.c \
	voip/audioconference.c \
	voip/mediastream.c \
	voip/msvoip.c \
	voip/ice.c \
	voip/audiostream.c \
	voip/ringstream.c \
	voip/qualityindicator.c \
	voip/bitratecontrol.c \
	voip/bitratedriver.c \
	voip/qosanalyzer.c \
	utils/dsptools.c \
	utils/kiss_fft.c \
	utils/kiss_fftr.c \
	utils/msjava.c \
	utils/g722_decode.c \
	utils/g722_encode.c \
	otherfilters/msrtp.c \
	otherfilters/tee.c \
	otherfilters/join.c \
	otherfilters/void.c \
	audiofilters/audiomixer.c \
	audiofilters/alaw.c \
	audiofilters/ulaw.c \
	audiofilters/msfileplayer.c \
	audiofilters/dtmfgen.c \
	audiofilters/msfilerec.c \
	audiofilters/msconf.c \
	audiofilters/msvolume.c \
	audiofilters/equalizer.c \
	audiofilters/tonedetector.c \
	audiofilters/msg722.c \
	audiofilters/l16.c \
	audiofilters/msresample.c \
	android/androidsound_depr.cpp \
	android/loader.cpp \
	android/android_echo.cpp \
	android/androidsound.cpp \
	android/AudioRecord.cpp \
	android/AudioTrack.cpp \
	android/AudioSystem.cpp \
	android/String8.cpp \

LOCAL_STATIC_LIBRARIES := 

##if BUILD_ALSA
ifeq ($(strip $(BOARD_USES_ALSA_AUDIO)),true)
LOCAL_SRC_FILES += audiofilters/alsa.c
LOCAL_CFLAGS += -D__ALSA_ENABLED__
endif

ifeq ($(BUILD_SRTP), 1)
	LOCAL_C_INCLUDES += $(SRTP_C_INCLUDE)
else

endif

ifeq ($(LINPHONE_VIDEO),1)
LOCAL_SRC_FILES += \
	voip/videostream.c \
	voip/rfc3984.c \
	voip/layouts.c \
	utils/shaders.c \
	utils/opengles_display.c \
	videofilters/videoenc.c \
	videofilters/videodec.c \
	videofilters/pixconv.c  \
	videofilters/sizeconv.c \
	videofilters/nowebcam.c \
	videofilters/h264dec.c \
	videofilters/mire.c \
	videofilters/vp8.c \
	videofilters/jpegwriter.c \
	android/android-display.c \
	android/android-display-bad.cpp \
	android/androidvideo.cpp \
	android/android-opengl-display.c

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
	LOCAL_CFLAGS += -DVIDEO_ENABLED
LOCAL_SRC_FILES+= \
	voip/scaler.c.neon \
	voip/scaler_arm.S.neon \
	voip/msvideo.c \
	voip/msvideo_neon.c.neon
else
LOCAL_SRC_FILES+= \
	voip/scaler.c \
	voip/msvideo.c
endif
endif

ifeq ($(BUILD_UPNP),1)
LOCAL_CFLAGS += -DBUILD_UPNP -DPTHREAD_MUTEX_RECURSIVE=PTHREAD_MUTEX_RECURSIVE
LOCAL_SRC_FILES += \
	upnp/upnp_igd.c \
	upnp/upnp_igd_cmd.c \
	upnp/upnp_igd_utils.c \

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../../../externals/build/libupnp/inc \
	$(LOCAL_PATH)/../../../externals/libupnp/upnp/inc \
        $(LOCAL_PATH)/../../../externals/libupnp/threadutil/inc \
	$(LOCAL_PATH)/../../../externals/libupnp/ixml/inc \

LOCAL_STATIC_LIBRARIES += libupnp 

endif

#LOCAL_SRC_FILES += voip/videostream.c
#
##if BUILD_THEORA
#LOCAL_SRC_FILES += videofilters/theora.c

#if BUILD_SPEEX
LOCAL_SRC_FILES += \
	audiofilters/msspeex.c \
	audiofilters/speexec.c

##if BUILD_GSM
LOCAL_SRC_FILES += audiofilters/gsm.c

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

LOCAL_STATIC_LIBRARIES += \
	libortp \
	libspeex \
	libspeexdsp

ifneq ($(BUILD_WEBRTC_AECM), 0)
ifneq ($(TARGET_ARCH_ABI), x86)
LOCAL_CFLAGS += -DBUILD_WEBRTC_AECM

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../../../externals/webrtc/ \
	$(LOCAL_PATH)/../../../externals/webrtc/modules/audio_processing/aecm/include

LOCAL_SRC_FILES += audiofilters/webrtc_aec.c

LOCAL_STATIC_LIBRARIES += \
	libwebrtc_aecm \
	libwebrtc_apm_utility \
	libwebrtc_spl \
	libwebrtc_system_wrappers
ifeq ($(TARGET_ARCH_ABI), armeabi-v7a)
LOCAL_STATIC_LIBRARIES += \
	libwebrtc_aecm_neon \
	libwebrtc_spl_neon
endif
endif
endif


ifeq ($(strip $(BOARD_USES_ALSA_AUDIO)),true)
LOCAL_SHARED_LIBRARIES += libasound
endif

LOCAL_STATIC_LIBRARIES += cpufeatures

ifeq ($(BUILD_MS2), 1)
	LOCAL_SRC_FILES += \
		../tools/mediastream.c

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


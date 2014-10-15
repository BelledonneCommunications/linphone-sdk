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
	$(LOCAL_PATH)/../../../externals/build/ffmpeg/$(TARGET_ARCH) \
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
	base/msfactory.c \
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
	voip/msmediaplayer.c \
	utils/dsptools.c \
	utils/kiss_fft.c \
	utils/kiss_fftr.c \
	utils/msjava.c \
	utils/g722_decode.c \
	utils/g722_encode.c \
	utils/audiodiff.c \
	otherfilters/msrtp.c \
	otherfilters/tee.c \
	otherfilters/join.c \
	otherfilters/void.c \
	otherfilters/itc.c \
	audiofilters/audiomixer.c \
	audiofilters/alaw.c \
	audiofilters/ulaw.c \
	audiofilters/g711.c \
	audiofilters/msfileplayer.c \
	audiofilters/dtmfgen.c \
	audiofilters/msfilerec.c \
	audiofilters/msvolume.c \
	audiofilters/equalizer.c \
	audiofilters/tonedetector.c \
	audiofilters/msg722.c \
	audiofilters/l16.c \
	audiofilters/msresample.c \
	audiofilters/devices.c \
	audiofilters/flowcontrol.c \
	audiofilters/aac-eld-android.cpp \
	android/hardware_echo_canceller.cpp \
	android/androidsound_depr.cpp \
	android/loader.cpp \
	android/androidsound.cpp \
	android/AudioRecord.cpp \
	android/AudioTrack.cpp \
	android/AudioSystem.cpp \
	android/String8.cpp \
	android/androidsound_opensles.cpp \

LOCAL_STATIC_LIBRARIES :=

##if BUILD_ALSA
ifeq ($(strip $(BOARD_USES_ALSA_AUDIO)),true)
LOCAL_SRC_FILES += audiofilters/alsa.c
LOCAL_CFLAGS += -D__ALSA_ENABLED__
endif

ifeq ($(BUILD_SRTP), 1)
	LOCAL_C_INCLUDES += $(SRTP_C_INCLUDE)
	LOCAL_CFLAGS += -DORTP_HAVE_SRTP
else

endif

ifeq ($(_BUILD_VIDEO),1)
LOCAL_SRC_FILES += \
	voip/videostarter.c \
	voip/videostream.c \
	voip/rfc3984.c \
	voip/vp8rtpfmt.c \
	voip/layouts.c \
	utils/shaders.c \
	utils/opengles_display.c \
	utils/ffmpeg-priv.c \
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
ifeq ($(TARGET_ARCH), x86)
	LOCAL_CFLAGS += -DVIDEO_ENABLED
endif
LOCAL_SRC_FILES+= \
	voip/scaler.c \
	voip/msvideo.c
endif

ifeq ($(BUILD_MATROSKA), 1)
LOCAL_CFLAGS += \
	-DHAVE_MATROSKA \
	-DCONFIG_EBML_WRITING \
	-DCONFIG_EBML_UNICODE \
	-DCONFIG_STDIO \
	-DCONFIG_FILEPOS_64 \
	-DNDEBUG

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../../../externals/libmatroska/corec \
	$(LOCAL_PATH)/../../../externals/libmatroska/libebml2 \
	$(LOCAL_PATH)/../../../externals/libmatroska/libmatroska2

LOCAL_SRC_FILES += \
	videofilters/mkv.c

LOCAL_STATIC_LIBRARIES += \
	libmatroska2

endif #BUILD_MATROSKA

endif #_BUILD_VIDEO

ifeq ($(BUILD_OPUS),1)
LOCAL_CFLAGS += -DHAVE_OPUS
LOCAL_SRC_FILES += \
	audiofilters/msopus.c

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../../../externals/opus/include
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

ifneq ($(BUILD_WEBRTC_AECM)$(BUILD_WEBRTC_ISAC), 00)
LOCAL_CFLAGS += -DHAVE_WEBRTC
LOCAL_STATIC_LIBRARIES += libmswebrtc
endif

ifneq ($(BUILD_WEBRTC_AECM), 0)
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

ifneq ($(BUILD_WEBRTC_ISAC), 0)
LOCAL_STATIC_LIBRARIES += libwebrtc_spl libwebrtc_isacfix
endif


ifeq ($(strip $(BOARD_USES_ALSA_AUDIO)),true)
LOCAL_SHARED_LIBRARIES += libasound
endif

LOCAL_STATIC_LIBRARIES += cpufeatures

ifeq ($(BUILD_MEDIASTREAMER2_SDK), 1)
	LOCAL_SRC_FILES += \
		../tools/mediastream.c

	ifneq ($(_BUILD_AMR), 0)
		LOCAL_CFLAGS += -DHAVE_AMR
		LOCAL_STATIC_LIBRARIES += libmsamr libopencoreamr
	endif
	ifneq ($(BUILD_AMRWB), 0)
		LOCAL_STATIC_LIBRARIES += libvoamrwbenc
	endif
	ifeq ($(BUILD_SILK),1)
		LOCAL_CFLAGS += -DHAVE_SILK
		LOCAL_STATIC_LIBRARIES += libmssilk
	endif
	LOCAL_STATIC_LIBRARIES += libgsm
	ifeq ($(BUILD_OPUS),1)
		LOCAL_STATIC_LIBRARIES += libopus
	endif
	ifeq ($(BUILD_G729),1)
		LOCAL_CFLAGS += -DHAVE_G729
		LOCAL_STATIC_LIBRARIES += libbcg729 libmsbcg729
	endif
	ifeq ($(_BUILD_VIDEO),1)
		LOCAL_STATIC_LIBRARIES += libvpx
		ifeq ($(BUILD_X264),1)
			LOCAL_STATIC_LIBRARIES += libmsx264 libx264
		endif
		ifeq ($(BUILD_OPENH264),1)
			LOCAL_STATIC_LIBRARIES += libmsopenh264 libopenh264
		endif
		LOCAL_SHARED_LIBRARIES += \
			libavcodec-linphone \
			libswscale-linphone \
			libavutil-linphone
		LOCAL_LDLIBS += -lGLESv2
	endif
	ifeq ($(BUILD_SRTP),1)
		LOCAL_SHARED_LIBRARIES += libsrtp
	endif
	ifeq ($(BUILD_GPLV3_ZRTP),1)
		LOCAL_SHARED_LIBRARIES += libssl-linphone libcrypto-linphone
		LOCAL_SHARED_LIBRARIES += libzrtpcpp
	endif

	LOCAL_LDLIBS += -llog -ldl
	LOCAL_MODULE_FILENAME := libmediastreamer2-$(TARGET_ARCH_ABI)
	include $(BUILD_SHARED_LIBRARY)
else
	include $(BUILD_STATIC_LIBRARY)
endif

$(call import-module,android/cpufeatures)


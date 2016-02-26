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
	$(LOCAL_PATH)/android \
	$(LOCAL_PATH)/otherfilters \
	$(LOCAL_PATH)/videofilters \
	$(LOCAL_PATH)/../include \
	$(LOCAL_PATH)/../../oRTP \
	$(LOCAL_PATH)/../../oRTP/include \
	$(LOCAL_PATH)/../../../bctoolbox/include \
	$(LOCAL_PATH)/../../../externals/speex/include \
	$(LOCAL_PATH)/../../../externals/build/speex \
	$(LOCAL_PATH)/../../../externals/gsm/inc \
	$(LOCAL_PATH)/../../../externals/ffmpeg \
	$(LOCAL_PATH)/../../../externals/ \
	$(LOCAL_PATH)/../../../externals/build/ffmpeg/$(TARGET_ARCH) \
	$(LOCAL_PATH)/../../../externals/libvpx/

LOCAL_MODULE := libmediastreamer2

LOCAL_SRC_FILES = \
	android/androidsound.cpp \
	android/androidsound_depr.cpp \
	android/androidsound_opensles.cpp \
	android/AudioRecord.cpp \
	android/AudioSystem.cpp \
	android/AudioTrack.cpp \
	android/hardware_echo_canceller.cpp \
	android/loader.cpp \
	android/String8.cpp \
	audiofilters/aac-eld-android.cpp \
	audiofilters/alaw.c \
	audiofilters/audiomixer.c \
	audiofilters/devices.c \
	audiofilters/dtmfgen.c \
	audiofilters/equalizer.c \
	audiofilters/flowcontrol.c \
	audiofilters/g711.c \
	audiofilters/genericplc.c \
	audiofilters/msgenericplc.c \
	audiofilters/l16.c \
	audiofilters/msfileplayer.c \
	audiofilters/msfilerec.c \
	audiofilters/msg722.c \
	audiofilters/msresample.c \
	audiofilters/msvaddtx.c \
	audiofilters/msvolume.c \
	audiofilters/tonedetector.c \
	audiofilters/ulaw.c \
	base/eventqueue.c \
	base/mscommon.c \
	base/msfactory.c \
	base/msfilter.c \
	base/msqueue.c \
	base/mssndcard.c \
	base/msticker.c \
	base/msvideopresets.c \
	base/mswebcam.c \
	base/mtu.c \
	crypto/dtls_srtp.c \
	crypto/ms_srtp.c \
	crypto/zrtp.c \
	otherfilters/itc.c \
	otherfilters/join.c \
	otherfilters/msrtp.c \
	otherfilters/tee.c \
	otherfilters/void.c \
	utils/audiodiff.c \
	utils/dsptools.c \
	utils/g722_decode.c \
	utils/g722_encode.c \
	utils/kiss_fft.c \
	utils/kiss_fftr.c \
	utils/msjava.c \
	utils/stream_regulator.c \
	voip/audioconference.c \
	voip/audiostream.c \
	voip/bitratecontrol.c \
	voip/bitratedriver.c \
	voip/ice.c \
	voip/mediastream.c \
	voip/msmediaplayer.c \
	voip/msvoip.c \
	voip/qosanalyzer.c \
	voip/qualityindicator.c \
	voip/ringstream.c \
	voip/stun.c \
	voip/stun_udp.c \
	otherfilters/rfc4103_source.c \
	otherfilters/rfc4103_sink.c \
	voip/rfc4103_textstream.c

LOCAL_STATIC_LIBRARIES := libbctoolbox

LOCAL_CFLAGS += -D_XOPEN_SOURCE=600

##if BUILD_ALSA
ifeq ($(strip $(BOARD_USES_ALSA_AUDIO)),true)
LOCAL_SRC_FILES += audiofilters/alsa.c
LOCAL_CFLAGS += -D__ALSA_ENABLED__
endif

ifeq ($(BUILD_SRTP), 1)
	LOCAL_C_INCLUDES += $(SRTP_C_INCLUDE)
	LOCAL_CFLAGS += -DHAVE_SRTP
else

endif

LOCAL_STATIC_LIBRARIES += polarssl
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../../externals/polarssl/include
LOCAL_CFLAGS += -DHAVE_POLARSSL_SSL_H=1 -DHAVE_DTLS=1

ifeq ($(_BUILD_VIDEO),1)
LOCAL_SRC_FILES += \
	voip/video_preset_high_fps.c \
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
	videofilters/msv4l2.c \
	android/mediacodech264dec.c \
	android/mediacodech264enc.c \
	android/android_mediacodec.cpp \
	android/android-display.c \
	android/android-display-bad.cpp \
	android/androidvideo.cpp \
	android/android-opengl-display.c

#uncomment this if you want to try loading v4l2 cameras on android (not really supported by google).
#LOCAL_CFLAGS += -DHAVE_LINUX_VIDEODEV2_H=1

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
	$(LOCAL_PATH)/../../../externals/libmatroska-c \
	$(LOCAL_PATH)/../../../externals/libmatroska-c/corec \
	$(LOCAL_PATH)/../../../externals/libmatroska-c/libebml2 \
	$(LOCAL_PATH)/../../../externals/libmatroska-c/libmatroska2

LOCAL_SRC_FILES += \
	utils/mkv_reader.c \
	videofilters/mkv.c


LOCAL_STATIC_LIBRARIES += \
	libmatroska2

endif #BUILD_MATROSKA

endif #_BUILD_VIDEO

ifeq ($(BUILD_NON_FREE_CODECS),1)
LOCAL_CFLAGS += -DHAVE_NON_FREE_CODECS=1
else
LOCAL_CFLAGS += -DHAVE_NON_FREE_CODECS=0
endif

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
	-D_POSIX_SOURCE \
	-Wall -Werror -Wno-error=strict-aliasing -Wuninitialized

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

ifeq ($(BUILD_ZRTP), 1)
LOCAL_STATIC_LIBRARIES += libbzrtp
LOCAL_CFLAGS += -DHAVE_ZRTP
LOCAL_C_INCLUDES += $(ZRTP_C_INCLUDE)
endif #ZRTP

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
		../tools/common.c \
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
	ifeq ($(BUILD_CODEC2),1)
		LOCAL_CFLAGS += -DHAVE_CODEC2
		LOCAL_STATIC_LIBRARIES += libcodec2 libmscodec2
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
			libffmpeg-linphone
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
LOCAL_CPPFLAGS = $(LOCAL_CLFAGS)
LOCAL_CFLAGS += -Wdeclaration-after-statement

$(call import-module,android/cpufeatures)


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

LOCAL_MODULE := libmediastreamer2


#LOCAL_CPP_EXTENSION := .cc

LOCAL_SRC_FILES = \
	mscommon.c \
	msfilter.c \
	msqueue.c \
	msticker.c \
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
	msresample.c \
	mswebcam.c \
	equalizer.c \
	dsptools.c \
	kiss_fft.c \
	kiss_fftr.c \
	void.c \
	msandroid.cpp \
	eventqueue.c	

LOCAL_SRC_FILES += audiostream.c

##if BUILD_RESAMPLE
#LOCAL_SRC_FILES += msresample.c

##if BUILD_ALSA
ifeq ($(strip $(BOARD_USES_ALSA_AUDIO)),true)
LOCAL_SRC_FILES += alsa.c
LOCAL_CFLAGS += -D__ALSA_ENABLED__
endif

##if BUILD_OSS
#LOCAL_SRC_FILES += oss.c

##if BUILD_ARTS
#LOCAL_SRC_FILES += arts.c

##if BUILD_PORTAUDIO
#LOCAL_SRC_FILES += pasnd.c

##if BUILD_MACSND
#LOCAL_SRC_FILES += macsnd.c

##if BUILD_MACAQSND
#LOCAL_SRC_FILES += aqsnd.c

ifeq ($(LINPHONE_VIDEO),1)

LOCAL_CFLAGS += -DVIDEO_ENABLED

LOCAL_SRC_FILES += \
	videoenc.c \
	videodec.c \
	pixconv.c  \
	sizeconv.c \
	nowebcam.c \
	msvideo.c \
	h264dec.c \
	rfc3984.c \
	mire.c \
	videostream.c

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
	-D_POSIX_SOURCE


ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
	LOCAL_CFLAGS += -DUSE_HARDWARE_RATE=1 
endif


#LOCAL_CFLAGS += -DDEBUG

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../build/android \
	$(LOCAL_PATH)/../include \
	$(LOCAL_PATH)/../../oRTP \
	$(LOCAL_PATH)/../../oRTP/include \
	$(LOCAL_PATH)/../../../externals/speex/include \
	$(LOCAL_PATH)/../../../externals/build/speex \
	$(LOCAL_PATH)/../../../externals/gsm/inc \
	$(LOCAL_PATH)/../../../externals/ffmpeg \
	$(LOCAL_PATH)/../../../externals/ \
	$(LOCAL_PATH)/../../../externals/build/ffmpeg

LOCAL_STATIC_LIBRARIES := \
	libortp \
	libspeex 


ifeq ($(strip $(BOARD_USES_ALSA_AUDIO)),true)
LOCAL_SHARED_LIBRARIES += libasound
endif



include $(BUILD_STATIC_LIBRARY)


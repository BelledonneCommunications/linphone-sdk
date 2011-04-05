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

LOCAL_PATH:= $(call my-dir)/../../coreapi


include $(CLEAR_VARS)

LOCAL_CPP_EXTENSION := .cc

LOCAL_SRC_FILES := \
	linphonecore.c \
	misc.c  \
	enum.c \
	presence.c \
	proxy.c \
	friend.c \
	authentication.c \
	lpconfig.c \
	chat.c \
	sipsetup.c \
	siplogin.c \
	address.c \
	linphonecore_jni.cc \
	sal.c \
	sal_eXosip2.c \
	sal_eXosip2_presence.c \
	sal_eXosip2_sdp.c \
	offeranswer.c \
	callbacks.c \
	linphonecall.c \
	ec-calibrator.c

ifndef MY_LOG_DOMAIN
MY_LOG_DOMAIN = \"Linphone\"
endif

LOCAL_CFLAGS += \
	-D_BYTE_ORDER=_LITTLE_ENDIAN \
	-DORTP_INET6 \
	-DENABLE_TRACE \
	-DLINPHONE_VERSION=\"3.4.0\" \
	-DLINPHONE_PLUGINS_DIR=\"\\tmp\" \
	-DLOG_DOMAIN=$(MY_LOG_DOMAIN)

LOCAL_CFLAGS += -DIN_LINPHONE


ifeq ($(LINPHONE_VIDEO),1)
LOCAL_CFLAGS += -DVIDEO_ENABLED
ifeq ($(BUILD_X264),1)
LOCAL_CFLAGS += -DHAVE_X264
endif
endif
LOCAL_C_INCLUDES += \
	$(LOCAL_PATH) \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/../oRTP/include \
	$(LOCAL_PATH)/../mediastreamer2/include \
	$(LOCAL_PATH)/../../externals/exosip/include \
	$(LOCAL_PATH)/../../externals/osip/include 

LOCAL_LDLIBS += -llog -ldl



LOCAL_STATIC_LIBRARIES := \
	libmediastreamer2 \
	libortp \
	libeXosip2 \
	libosip2 \
	libgsm


ifeq ($(LINPHONE_VIDEO),1)
ifeq ($(BUILD_X264),1)
LOCAL_STATIC_LIBRARIES += \
	libmsx264 \
	libx264 
endif
LOCAL_SHARED_LIBRARIES += \
	libavcodec \
	libswscale \
	libavcore \
	libavutil
endif

LOCAL_STATIC_LIBRARIES += libspeex 


ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
LOCAL_CFLAGS += -DHAVE_ILBC=1
LOCAL_STATIC_LIBRARIES += libmsilbc
else
LOCAL_STATIC_LIBRARIES += $(LIBLINPHONE_EXTENDED_STATIC_LIBS)
LOCAL_SRC_FILES  += $(LIBLINPHONE_EXTENDED_SRC_FILES)
LOCAL_C_INCLUDES += $(LIBLINPHONE_EXTENDED_C_INCLUDES) \
endif


LOCAL_MODULE := liblinphone
include $(BUILD_SHARED_LIBRARY)


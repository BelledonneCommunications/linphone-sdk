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

LOCAL_PATH:= $(call my-dir)/
include $(CLEAR_VARS)

LOCAL_MODULE := libring


LOCAL_SRC_FILES = \
	ring_jni.c

LOCAL_CFLAGS += \
	-DORTP_INET6

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH) \
	$(LOCAL_PATH)/../../oRTP/include \
	$(LOCAL_PATH)/../include 

LOCAL_LDLIBS += -llog -ldl



LOCAL_STATIC_LIBRARIES := \
	libmediastreamer2 \
	libortp \
	libgsm \

ifeq ($(LINPHONE_VIDEO),1)
LOCAL_STATIC_LIBRARIES += \
	libavcodec \
	libswscale \
	libavcore \
	libavutil \
	libmsx264 \
	libx264
endif

LOCAL_STATIC_LIBRARIES += libspeex 

LOCAL_MODULE_CLASS = SHARED_LIBRARIES
include $(BUILD_SHARED_LIBRARY)







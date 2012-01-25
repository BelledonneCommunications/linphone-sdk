LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := libmsamr

LOCAL_CFLAGS += -DVERSION=\"android\"

LOCAL_SRC_FILES = src/msamr.c

ifneq ($(BUILD_AMRNB),0)
LOCAL_CFLAGS += -DHAVE_AMRNB=1
LOCAL_SRC_FILES += src/amrnb.c
endif

ifeq ($(BUILD_AMRNB),light)
LOCAL_CFLAGS += -DUSE_ANDROID_AMR
endif

ifeq ($(BUILD_AMRWB),1)
LOCAL_CFLAGS += -DHAVE_AMRWB=1
LOCAL_SRC_FILES += src/amrwb.c
LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../externals/vo-amrwbenc
endif

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../linphone/oRTP/include \
	$(LOCAL_PATH)/../linphone/mediastreamer2/include \
	$(LOCAL_PATH)/../externals/opencore-amr

include $(BUILD_STATIC_LIBRARY)



LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := libmsamr

LOCAL_CFLAGS += -DVERSION=\"android\"

ifeq ($(BUILD_AMR),light)
LOCAL_CFLAGS += -DUSE_ANDROID_AMR
endif

LOCAL_SRC_FILES = src/msamr.c



LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../linphone/oRTP/include \
	$(LOCAL_PATH)/../linphone/mediastreamer2/include \
	$(LOCAL_PATH)/../externals/opencore-amr/amrnb


include $(BUILD_STATIC_LIBRARY)



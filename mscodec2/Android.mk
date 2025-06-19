LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := libmscodec2

LOCAL_SRC_FILES :=  mscodec2.c


LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../linphone/oRTP/include \
	$(LOCAL_PATH)/../linphone/mediastreamer2/include \
	$(LOCAL_PATH)/../externals/codec2/include

LOCAL_CFLAGS += -DVERSION=\"android\" -include $(LOCAL_PATH)/../externals/build/codec2/codec2_prefixed_symbols.h

LOCAL_STATIC_LIBRARIES = libcodec2

include $(BUILD_STATIC_LIBRARY)

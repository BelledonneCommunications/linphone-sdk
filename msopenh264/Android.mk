LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := libmsopenh264


LOCAL_SRC_FILES = src/msopenh264.cpp src/msopenh264dec.cpp src/msopenh264enc.cpp


LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../linphone/oRTP/include \
	$(LOCAL_PATH)/../linphone/mediastreamer2/include \
	$(LOCAL_PATH)/../externals/openh264/include

LOCAL_CFLAGS += -DVERSION=\"android\"

ifeq ($(ENABLE_OPENH264_DECODER), 1)
LOCAL_CFLAGS += -DOPENH264_DECODER_ENABLED
endif

include $(BUILD_STATIC_LIBRARY)



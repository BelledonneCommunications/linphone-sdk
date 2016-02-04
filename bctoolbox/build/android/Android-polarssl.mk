
LOCAL_PATH:= $(call my-dir)/../../src/
include $(CLEAR_VARS)

LOCAL_MODULE:= libbctoolbox

LOCAL_SRC_FILES := \
	crypto_polarssl.c	

LOCAL_CFLAGS += -Wno-maybe-uninitialized

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/ \
	$(LOCAL_PATH)/../include \
	$(LOCAL_PATH)/../../externals/polarssl/include

LOCAL_STATIC_LIBRARIES := \
        polarssl

include $(BUILD_STATIC_LIBRARY)


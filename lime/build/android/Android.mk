LOCAL_PATH:= $(call my-dir)/../../src

include $(CLEAR_VARS)

LOCAL_CPP_EXTENSION := .cpp

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../include \
	$(LOCAL_PATH)/../../lime/include \

LOCAL_SRC_FILES := \
	lime_x3dh.cpp

LOCAL_MODULE:= liblime

include $(BUILD_STATIC_LIBRARY)

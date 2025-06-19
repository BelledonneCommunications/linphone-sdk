LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS += -DHAVE_LIBXML2

LOCAL_MODULE := libbzrtp

LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES =  \
	src/bzrtp.c \
	src/cryptoUtils.c \
	src/packetParser.c \
	src/stateMachine.c \
	src/zidCache.c \
	src/pgpwords.c

LOCAL_STATIC_LIBRARIES += liblpxml2

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/../externals/libxml2/include \
	$(LOCAL_PATH)/../externals/build/libxml2

ifeq ($(BUILD_BCTOOLBOX_MBEDTLS),1)
LOCAL_SRC_FILES += src/cryptoMbedtls.c 
LOCAL_STATIC_LIBRARIES += mbedtls
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../externals/mbedtls/include
else
LOCAL_SRC_FILES += src/cryptoPolarssl.c
LOCAL_STATIC_LIBRARIES += polarssl
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../externals/polarssl/include
endif

include $(BUILD_STATIC_LIBRARY)

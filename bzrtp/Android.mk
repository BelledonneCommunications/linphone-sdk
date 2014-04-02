LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS += -DHAVE_LIBXML2

LOCAL_MODULE := libbzrtp

LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES =       src/bzrtp.c \
						src/cryptoPolarssl.c \
						src/cryptoUtils.c \
						src/packetParser.c \
						src/stateMachine.c \
						src/zidCache.c

LOCAL_STATIC_LIBRARIES += polarssl \
			  liblpxml2

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/../externals/polarssl/include \
	$(LOCAL_PATH)/../externals/libxml2/include \
	$(LOCAL_PATH)/../externals/build/libxml2


include $(BUILD_STATIC_LIBRARY)



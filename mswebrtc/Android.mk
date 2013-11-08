LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := libmsisac

ISAC_SRC_DIR    = $(LOCAL_PATH)/../externals/webrtc

LOCAL_SRC_FILES := isac_enc.c isac_dec.c


LOCAL_STATIC_LIBRARIES += libwebrtc_isacfix libwebrtc_isacfix_neon libwebrtc_spl libwebrtc_spl_neon 

LOCAL_C_INCLUDES += $(ISAC_SRC_DIR) \
    $(ISAC_SRC_DIR)/modules/audio_coding/codecs/isac/fix/source     \
    $(ISAC_SRC_DIR)/modules/audio_coding/codecs/isac/fix/util 		\
    $(ISAC_SRC_DIR)/modules/audio_coding/codecs/isac/fix/interface 	\
	$(ISAC_SRC_DIR)/common_audio/signal_processing/include          \
	$(LOCAL_PATH)/../linphone/oRTP/include                          \
	$(LOCAL_PATH)/../linphone/mediastreamer2/include

include $(BUILD_STATIC_LIBRARY)



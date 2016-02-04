LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := libmswebrtc

WEBRTC_SRC_DIR    = $(LOCAL_PATH)/webrtc/webrtc

LOCAL_SRC_FILES := mswebrtc.c

LOCAL_CFLAGS += -fPIC

LOCAL_C_INCLUDES += $(WEBRTC_SRC_DIR) \
	$(LOCAL_PATH)/webrtc \
	$(WEBRTC_SRC_DIR)/common_audio/signal_processing/include \
	$(LOCAL_PATH)/../linphone/oRTP/include \
	$(LOCAL_PATH)/../linphone/mediastreamer2/include

ifneq ($(BUILD_WEBRTC_AECM),0)
	LOCAL_CFLAGS += -DBUILD_AEC
	LOCAL_SRC_FILES += aec.c
	LOCAL_STATIC_LIBRARIES += libwebrtc_spl libwebrtc_spl_neon
	LOCAL_C_INCLUDES += $(WEBRTC_SRC_DIR)/modules/audio_processing/aecm/include
endif

ifneq ($(BUILD_WEBRTC_ISAC),0)
	LOCAL_CFLAGS += -DBUILD_ISAC
	LOCAL_SRC_FILES += isac_enc.c isac_dec.c
	LOCAL_STATIC_LIBRARIES += libwebrtc_isacfix libwebrtc_isacfix_neon libwebrtc_spl libwebrtc_spl_neon
	LOCAL_C_INCLUDES += \
		$(WEBRTC_SRC_DIR)/modules/audio_coding/codecs/isac/fix/source \
		$(WEBRTC_SRC_DIR)/modules/audio_coding/codecs/isac/fix/util \
		$(WEBRTC_SRC_DIR)/modules/audio_coding/codecs/isac/fix/interface \
		$(WEBRTC_SRC_DIR)/modules/audio_coding/codecs/isac/fix/include/
endif

ifneq ($(BUILD_ILBC),0)
	LOCAL_CFLAGS += -DBUILD_ILBC
	LOCAL_SRC_FILES += ilbc.c
	LOCAL_STATIC_LIBRARIES += libwebrtc_ilbc libwebrtc_spl libwebrtc_spl_neon
	LOCAL_C_INCLUDES += \
		$(WEBRTC_SRC_DIR)/modules/audio_coding/codecs/ilbc/include/
endif

include $(BUILD_STATIC_LIBRARY)

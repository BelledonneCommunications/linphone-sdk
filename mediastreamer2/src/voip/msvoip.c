/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006  Simon MORLAT (simon.morlat@linphone.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#include "gitversion.h"
#else
#   ifndef MEDIASTREAMER_VERSION
#   define MEDIASTREAMER_VERSION "unknown"
#   endif
#	ifndef GIT_VERSION
#	define GIT_VERSION "unknown"
#	endif
#endif

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/mscodecutils.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/ms_srtp.h"
#include "private.h"

#ifdef __cplusplus
extern "C"{
#endif

extern void __register_ffmpeg_encoders_if_possible(MSFactory *factory);
extern void __register_ffmpeg_h264_decoder_if_possible(MSFactory *factory);
extern void ms_ffmpeg_check_init(void);
extern bool_t libmsandroiddisplay_init(MSFactory *factory);
extern void libmsandroiddisplaybad_init(MSFactory *factory);
extern void libmsandroidopengldisplay_init(MSFactory *factory);

#if defined(__APPLE__) && defined(VIDEO_ENABLED)
extern void _register_videotoolbox_if_supported(MSFactory *factory);
#endif

#include "voipdescs.h"
#include "mediastreamer2/mssndcard.h"
#include "mediastreamer2/msvideopresets.h"
#include "mediastreamer2/mswebcam.h"

#ifdef __APPLE__
   #include "TargetConditionals.h"
#endif

#ifdef ANDROID
#include <android/log.h>
#endif


#ifdef MS2_FILTERS

#ifdef __ALSA_ENABLED__
extern MSSndCardDesc alsa_card_desc;
#endif

#ifdef __QSA_ENABLED__
extern MSSndCardDesc ms_qsa_card_desc;
#endif

#ifdef HAVE_SYS_SOUNDCARD_H
extern MSSndCardDesc oss_card_desc;
#endif

#ifdef __ARTS_ENABLED__
extern MSSndCardDesc arts_card_desc;
#endif

#ifdef MS2_WINDOWS_DESKTOP
extern MSSndCardDesc winsnd_card_desc;
#endif

#ifdef __DIRECTSOUND_ENABLED__
extern MSSndCardDesc winsndds_card_desc;
#endif

#ifdef __MACSND_ENABLED__
extern MSSndCardDesc ca_card_desc;
#endif

#ifdef __PORTAUDIO_ENABLED__
extern MSSndCardDesc pasnd_card_desc;
#endif

#ifdef __MAC_AQ_ENABLED__
extern MSSndCardDesc aq_card_desc;
#endif

#ifdef __PULSEAUDIO_ENABLED__
extern MSSndCardDesc pulse_card_desc;
#endif

#if TARGET_OS_IPHONE
extern MSSndCardDesc au_card_desc;
#endif

#ifdef ANDROID
extern MSSndCardDesc msandroid_sound_card_desc;
extern MSSndCardDesc android_native_snd_card_desc;
extern MSSndCardDesc android_native_snd_opensles_card_desc;
#endif

#endif /* MS2_FILTERS */

static MSSndCardDesc * ms_snd_card_descs[]={
#ifdef MS2_FILTERS

#ifdef __PULSEAUDIO_ENABLED__
        &pulse_card_desc,
#endif

#ifdef __ALSA_ENABLED__
	&alsa_card_desc,
#endif

#ifdef __QSA_ENABLED__
	&ms_qsa_card_desc,
#endif

#ifdef HAVE_SYS_SOUNDCARD_H
	&oss_card_desc,
#endif

#ifdef __ARTS_ENABLED__
	&arts_card_desc,
#endif

#ifdef MS2_WINDOWS_DESKTOP
	&winsnd_card_desc,
#endif

#ifdef __DIRECTSOUND_ENABLED__
	&winsndds_card_desc,
#endif

#ifdef __PORTAUDIO_ENABLED__
	&pasnd_card_desc,
#endif

#ifdef __MACSND_ENABLED__
	&ca_card_desc,
#endif

#if TARGET_OS_IPHONE
	&au_card_desc,
#endif

#ifdef __MAC_AQ_ENABLED__
	&aq_card_desc,
#endif

#ifdef ANDROID
	&android_native_snd_card_desc,
	&android_native_snd_opensles_card_desc,
	&msandroid_sound_card_desc,
#endif
#endif /* MS2_FILTERS */
NULL
};

#ifdef VIDEO_ENABLED

#ifdef MS2_FILTERS

#ifdef HAVE_LINUX_VIDEODEV_H
extern MSWebCamDesc v4l_desc;
#endif

#ifdef HAVE_LINUX_VIDEODEV2_H
extern MSWebCamDesc v4l2_card_desc;
#endif

#ifdef _WIN32
extern MSWebCamDesc ms_vfw_cam_desc;
#endif

#if defined(_WIN32) && defined(HAVE_DIRECTSHOW)
extern MSWebCamDesc ms_directx_cam_desc;
#endif

#if defined(__MINGW32__) || defined(HAVE_DIRECTSHOW)
extern MSWebCamDesc ms_dshow_cam_desc;
#endif

#if TARGET_OS_MAC
extern MSWebCamDesc ms_v4m_cam_desc;
#endif

extern MSWebCamDesc static_image_desc;

extern MSWebCamDesc ms_mire_webcam_desc;
#ifdef ANDROID
extern MSWebCamDesc ms_android_video_capture_desc;
extern MSFilterDesc ms_mediacodec_h264_dec_desc;
extern MSFilterDesc ms_mediacodec_h264_enc_desc;
extern bool_t AMediaImage_isAvailable(void);
#endif

#if TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR
extern MSWebCamDesc ms_v4ios_cam_desc;
#endif

#ifdef __QNX__
extern MSWebCamDesc ms_bb10_camera_desc;
#endif

#endif /* MS2_FILTERS */

static MSWebCamDesc * ms_web_cam_descs[]={
#ifdef MS2_FILTERS
#if defined (ANDROID)
	&ms_android_video_capture_desc,
#endif
#ifdef HAVE_LINUX_VIDEODEV2_H
	&v4l2_card_desc,
#endif
#ifdef HAVE_LINUX_VIDEODEV_H
	&v4l_desc,
#endif
#if defined(_WIN32) && defined(HAVE_VFW)
	&ms_vfw_cam_desc,
#endif
#if defined(__MINGW32__) || defined (HAVE_DIRECTSHOW)
	&ms_dshow_cam_desc,
#endif
#if TARGET_OS_MAC && !TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR && !TARGET_OS_EMBEDDED
	&ms_v4m_cam_desc,
#endif
#ifdef __QNX__
	&ms_bb10_camera_desc,
#endif
#if TARGET_OS_IPHONE &&  !TARGET_IPHONE_SIMULATOR
	&ms_v4ios_cam_desc,
#endif
	&ms_mire_webcam_desc,
	&static_image_desc,
#endif /*MS2_FILTERS */
	NULL
};

#endif


void ms_factory_init_voip(MSFactory *obj){
	MSSndCardManager *cm;
	int i;

	if (obj->voip_initd) return;

	ms_srtp_init();
	obj->devices_info = ms_devices_info_new();

#if defined(VIDEO_ENABLED) && defined(MS2_FILTERS) && !defined(NO_FFMPEG) && defined(HAVE_LIBAVCODEC_AVCODEC_H)
	ms_ffmpeg_check_init();
	__register_ffmpeg_encoders_if_possible(obj);
	__register_ffmpeg_h264_decoder_if_possible(obj);
#endif

#if defined(__APPLE__) && defined(VIDEO_ENABLED)
	_register_videotoolbox_if_supported(obj);
#endif

#if defined(ANDROID) && defined(VIDEO_ENABLED)
	if (AMediaImage_isAvailable()) {
		ms_factory_register_filter(obj, &ms_mediacodec_h264_dec_desc);
		ms_factory_register_filter(obj, &ms_mediacodec_h264_enc_desc);
	}
#endif
	
	/* register builtin VoIP MSFilter's */
	for (i=0;ms_voip_filter_descs[i]!=NULL;i++){
		ms_factory_register_filter(obj,ms_voip_filter_descs[i]);
	}

	cm=ms_snd_card_manager_new();
	ms_message("Registering all soundcard handlers");
	cm->factory=obj;
	obj->sndcardmanager = cm;
	for (i=0;ms_snd_card_descs[i]!=NULL;i++){
		ms_snd_card_manager_register_desc(cm,ms_snd_card_descs[i]);
	}

	{
		MSWebCamManager *wm;
		wm=ms_web_cam_manager_new();
		wm->factory = obj;
		obj->wbcmanager = wm;
#ifdef VIDEO_ENABLED
		ms_message("Registering all webcam handlers");
		for (i=0;ms_web_cam_descs[i]!=NULL;i++){
			ms_web_cam_manager_register_desc(wm,ms_web_cam_descs[i]);
		}
#endif
	}

#ifdef VIDEO_ENABLED
	{
		MSVideoPresetsManager *vpm = ms_video_presets_manager_new(obj);
		register_video_preset_high_fps(vpm);
	}
#endif

#if defined(ANDROID) && defined (VIDEO_ENABLED)
	{
		MSDevicesInfo *devices = ms_factory_get_devices_info(obj);
		SoundDeviceDescription *description = ms_devices_info_get_sound_device_description(devices);
		if (description && description->flags & DEVICE_HAS_CRAPPY_OPENGL) {
			if (!libmsandroiddisplay_init(obj)) {
				libmsandroiddisplaybad_init(obj);
			}
		} else {
			libmsandroidopengldisplay_init(obj);
		}
	}
#endif
	obj->voip_initd=TRUE;
	obj->voip_uninit_func = ms_factory_uninit_voip;
	ms_message("ms_factory_init_voip() done");
}

void ms_factory_uninit_voip(MSFactory *obj){
	if (obj->voip_initd){
		ms_snd_card_manager_destroy(obj->sndcardmanager);
		obj->sndcardmanager = NULL;
#ifdef VIDEO_ENABLED
		ms_web_cam_manager_destroy(obj->wbcmanager);
		obj->wbcmanager = NULL;
		ms_video_presets_manager_destroy(obj->video_presets_manager);
#endif
		ms_srtp_shutdown();
		if (obj->devices_info) ms_devices_info_free(obj->devices_info);
		obj->voip_initd = FALSE;
	}
}

MSFactory *ms_factory_new_with_voip(void){
	MSFactory *f = ms_factory_new();
	ms_factory_init_voip(f);
	ms_factory_init_plugins(f);
	return f;
}



PayloadType * ms_offer_answer_context_match_payload(MSOfferAnswerContext *context, const MSList *local_payloads, const PayloadType *remote_payload, const MSList *remote_payloads, bool_t is_reading){
	return context->match_payload(context, local_payloads, remote_payload, remote_payloads, is_reading);
}

 MSOfferAnswerContext *ms_offer_answer_create_simple_context(MSPayloadMatcherFunc func){
	 MSOfferAnswerContext *ctx = ms_new0(MSOfferAnswerContext,1);
	 ctx->match_payload = func;
	 ctx->destroy = (void (*)(MSOfferAnswerContext *)) ms_free;
	 return ctx;
}

void ms_offer_answer_context_destroy(MSOfferAnswerContext *ctx){
	if (ctx->destroy)
		ctx->destroy(ctx);
}


#ifdef _MSC_VER
#pragma warning(disable : 4996)
#else
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

static int ms_voip_ref=0;
void ms_voip_init(){
	if (ms_voip_ref++ >0 ) {
		ms_message ("Skipping ms_voip_init, because [%i] ref",ms_voip_ref);
		return;
	}
	ms_factory_init_voip(ms_factory_get_fallback());
}

void ms_voip_exit(){
	if (--ms_voip_ref >0 ) {
		ms_message ("Skipping ms_voip_exit, still [%i] ref",ms_voip_ref);
		return;
	}
	ms_factory_uninit_voip(ms_factory_get_fallback());
}


#ifdef __cplusplus
}
#endif

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msinterfaces.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msrtp.h"
#include "mediastreamer2/msvideoout.h"
#include "mediastreamer2/msextdisplay.h"
#include "mediastreamer2/msitc.h"
#include "private.h"

#include <ortp/zrtp.h>

static void configure_itc(VideoStream *stream);

void video_stream_free(VideoStream *stream) {
	/* Prevent filters from being destroyed two times */
	if (stream->source_performs_encoding == TRUE) {
		stream->ms.encoder = NULL;
	}
	if (stream->output_performs_decoding == TRUE) {
		stream->ms.decoder = NULL;
	}

	media_stream_free(&stream->ms);

	if (stream->source != NULL)
		ms_filter_destroy (stream->source);
	if (stream->output != NULL)
		ms_filter_destroy (stream->output);
	if (stream->sizeconv != NULL)
		ms_filter_destroy (stream->sizeconv);
	if (stream->pixconv!=NULL)
		ms_filter_destroy(stream->pixconv);
	if (stream->tee!=NULL)
		ms_filter_destroy(stream->tee);
	if (stream->tee2!=NULL)
		ms_filter_destroy(stream->tee2);
	if (stream->jpegwriter!=NULL)
		ms_filter_destroy(stream->jpegwriter);
	if (stream->output2!=NULL)
		ms_filter_destroy(stream->output2);
	if (stream->tee3)
		ms_filter_destroy(stream->tee3);
	if (stream->itcsink)
		ms_filter_destroy(stream->itcsink);
	if (stream->local_jpegwriter)
		ms_filter_destroy(stream->local_jpegwriter);
	
	if (stream->display_name!=NULL)
		ms_free(stream->display_name);

	ms_free(stream);
}

static void event_cb(void *ud, MSFilter* f, unsigned int event, void *eventdata){
	VideoStream *st=(VideoStream*)ud;
	if (st->eventcb!=NULL){
		st->eventcb(st->event_pointer,f,event,eventdata);
	}
}

static void internal_event_cb(void *ud, MSFilter *f, unsigned int event, void *eventdata) {
	VideoStream *stream = (VideoStream *)ud;
	const MSVideoCodecSLI *sli;
	const MSVideoCodecRPSI *rpsi;

	switch (event) {
		case MS_VIDEO_DECODER_SEND_PLI:
			ms_message("Request sending of PLI on videostream [%p]", stream);
			video_stream_send_pli(stream);
			break;
		case MS_VIDEO_DECODER_SEND_SLI:
			sli = (const MSVideoCodecSLI *)eventdata;
			ms_message("Request sending of SLI on videostream [%p]", stream);
			video_stream_send_sli(stream, sli->first, sli->number, sli->picture_id);
			break;
		case MS_VIDEO_DECODER_SEND_RPSI:
			rpsi = (const MSVideoCodecRPSI *)eventdata;
			ms_message("Request sending of RPSI on videostream [%p]", stream);
			video_stream_send_rpsi(stream, rpsi->bit_string, rpsi->bit_string_len);
			break;
		case MS_FILTER_OUTPUT_FMT_CHANGED:
			if (stream->itcsink) configure_itc(stream);
			break;
	}
}

static void video_stream_process_rtcp(MediaStream *media_stream, mblk_t *m){
	VideoStream *stream = (VideoStream *)media_stream;
	int i;

	if (rtcp_is_PSFB(m)) {
		if (rtcp_PSFB_get_type(m) == RTCP_PSFB_FIR) {
			/* Special case for FIR where the packet sender ssrc must be equal to 0. */
			if (rtcp_PSFB_get_media_source_ssrc(m) == rtp_session_get_send_ssrc(stream->ms.sessions.rtp_session)) {
				for (i = 0; ; i++) {
					rtcp_fb_fir_fci_t *fci = rtcp_PSFB_fir_get_fci(m, i);
					if (fci == NULL) break;
					if (rtcp_fb_fir_fci_get_ssrc(fci) == rtp_session_get_send_ssrc(stream->ms.sessions.rtp_session)) {
						uint8_t seq_nr = rtcp_fb_fir_fci_get_seq_nr(fci);
						ms_filter_call_method(stream->ms.encoder, MS_VIDEO_ENCODER_NOTIFY_FIR, &seq_nr);
						break;
					}
				}
			}
		} else {
			if ((rtcp_PSFB_get_media_source_ssrc(m) == rtp_session_get_send_ssrc(stream->ms.sessions.rtp_session))
				&& (rtcp_PSFB_get_packet_sender_ssrc(m) == rtp_session_get_recv_ssrc(stream->ms.sessions.rtp_session))) {
				switch (rtcp_PSFB_get_type(m)) {
					case RTCP_PSFB_PLI:
						ms_filter_call_method_noarg(stream->ms.encoder, MS_VIDEO_ENCODER_NOTIFY_PLI);
						break;
					case RTCP_PSFB_SLI:
						for (i = 0; ; i++) {
							rtcp_fb_sli_fci_t *fci = rtcp_PSFB_sli_get_fci(m, i);
							MSVideoCodecSLI sli;
							if (fci == NULL) break;
							sli.first = rtcp_fb_sli_fci_get_first(fci);
							sli.number = rtcp_fb_sli_fci_get_number(fci);
							sli.picture_id = rtcp_fb_sli_fci_get_picture_id(fci);
							ms_filter_call_method(stream->ms.encoder, MS_VIDEO_ENCODER_NOTIFY_SLI, &sli);
						}
						break;
					case RTCP_PSFB_RPSI:
					{
						rtcp_fb_rpsi_fci_t *fci = rtcp_PSFB_rpsi_get_fci(m);
						MSVideoCodecRPSI rpsi;
						rpsi.bit_string = rtcp_fb_rpsi_fci_get_bit_string(fci);
						rpsi.bit_string_len = rtcp_PSFB_rpsi_get_fci_bit_string_len(m);
						ms_filter_call_method(stream->ms.encoder, MS_VIDEO_ENCODER_NOTIFY_RPSI, &rpsi);
					}
						break;
					default:
						break;
				}
			}
		}
	}
}

static void stop_preload_graph(VideoStream *stream){
	ms_ticker_detach(stream->ms.sessions.ticker,stream->ms.rtprecv);
	ms_filter_unlink(stream->ms.rtprecv,0,stream->ms.voidsink,0);
	ms_filter_destroy(stream->ms.voidsink);
	ms_filter_destroy(stream->ms.rtprecv);
	stream->ms.voidsink=stream->ms.rtprecv=NULL;
}

void video_stream_iterate(VideoStream *stream){
	media_stream_iterate(&stream->ms);
}

const char *video_stream_get_default_video_renderer(void){
#ifdef WIN32
	return "MSDrawDibDisplay";
#elif defined(ANDROID)
	return "MSAndroidDisplay";
#elif __APPLE__ && !defined(__ios)
	return "MSOSXGLDisplay";
#elif defined (HAVE_XV)
	return "MSX11Video";
#elif defined(HAVE_GL)
	return "MSGLXVideo";
#elif defined(__ios)
	return "IOSDisplay";
#else
	return "MSVideoOut";
#endif
}

static void choose_display_name(VideoStream *stream){
	stream->display_name=ms_strdup(video_stream_get_default_video_renderer());
}

static float video_stream_get_rtcp_xr_average_quality_rating(unsigned long userdata) {
	VideoStream *stream = (VideoStream *)userdata;
	return stream ? media_stream_get_average_quality_rating(&stream->ms) : -1;
}

static float video_stream_get_rtcp_xr_average_lq_quality_rating(unsigned long userdata) {
	VideoStream *stream = (VideoStream *)userdata;
	return stream ? media_stream_get_average_lq_quality_rating(&stream->ms) : -1;
}


VideoStream *video_stream_new(int loc_rtp_port, int loc_rtcp_port, bool_t use_ipv6){
	MSMediaStreamSessions sessions={0};
	VideoStream *obj;
	sessions.rtp_session=create_duplex_rtpsession(loc_rtp_port,loc_rtcp_port,use_ipv6);
	obj=video_stream_new_with_sessions(&sessions);
	obj->ms.owns_sessions=TRUE;
	return obj;
}

VideoStream *video_stream_new_with_sessions(const MSMediaStreamSessions *sessions){
	VideoStream *stream = (VideoStream *)ms_new0 (VideoStream, 1);
	const OrtpRtcpXrMediaCallbacks rtcp_xr_media_cbs = {
		NULL,
		NULL,
		NULL,
		video_stream_get_rtcp_xr_average_quality_rating,
		video_stream_get_rtcp_xr_average_lq_quality_rating,
		(unsigned long)stream
	};

	stream->ms.type = MSVideo;
	stream->ms.sessions=*sessions;
	rtp_session_resync(stream->ms.sessions.rtp_session);
	stream->ms.qi=ms_quality_indicator_new(stream->ms.sessions.rtp_session);
	ms_quality_indicator_set_label(stream->ms.qi,"video");
	stream->ms.evq=ortp_ev_queue_new();
	stream->ms.rtpsend=ms_filter_new(MS_RTP_SEND_ID);
	stream->ms.ice_check_list=NULL;
	rtp_session_register_event_queue(stream->ms.sessions.rtp_session,stream->ms.evq);
	MS_VIDEO_SIZE_ASSIGN(stream->sent_vsize, CIF);
	stream->fps=0;
	stream->dir=VideoStreamSendRecv;
	stream->display_filter_auto_rotate_enabled=0;
	stream->freeze_on_error = FALSE;
	stream->source_performs_encoding = FALSE;
	stream->output_performs_decoding = FALSE;
	choose_display_name(stream);
	stream->ms.process_rtcp=video_stream_process_rtcp;
	/*
	 * In practice, these filters are needed only for audio+video recording.
	 */
	if (ms_factory_lookup_filter_by_id(ms_factory_get_fallback(), MS_MKV_RECORDER_ID)){
		stream->itcsink=ms_filter_new(MS_ITC_SINK_ID);
		stream->tee3=ms_filter_new(MS_TEE_ID);
	}

	rtp_session_set_rtcp_xr_media_callbacks(stream->ms.sessions.rtp_session, &rtcp_xr_media_cbs);

	return stream;
}

void video_stream_set_sent_video_size(VideoStream *stream, MSVideoSize vsize){
	ms_message("Setting video size %dx%d on stream [%p]", vsize.width, vsize.height,stream);
	stream->sent_vsize=vsize;
}

void video_stream_set_preview_size(VideoStream *stream, MSVideoSize vsize){
	ms_message("Setting preview video size %dx%d", vsize.width, vsize.height);
	stream->preview_vsize=vsize;
}

void video_stream_set_fps(VideoStream *stream, float fps){
	stream->fps=fps;
}

MSVideoSize video_stream_get_sent_video_size(const VideoStream *stream) {
	MSVideoSize vsize;
	MS_VIDEO_SIZE_ASSIGN(vsize, UNKNOWN);
	if (stream->ms.encoder != NULL) {
		ms_filter_call_method(stream->ms.encoder, MS_FILTER_GET_VIDEO_SIZE, &vsize);
	}
	return vsize;
}

MSVideoSize video_stream_get_received_video_size(const VideoStream *stream) {
	MSVideoSize vsize;
	MS_VIDEO_SIZE_ASSIGN(vsize, UNKNOWN);
	if (stream->ms.decoder != NULL) {
		ms_filter_call_method(stream->ms.decoder, MS_FILTER_GET_VIDEO_SIZE, &vsize);
	}
	return vsize;
}

float video_stream_get_sent_framerate(const VideoStream *stream){
	float fps=0;
	if (stream->source){
		if (ms_filter_has_method(stream->source, MS_FILTER_GET_FPS)){
			ms_filter_call_method(stream->source,MS_FILTER_GET_FPS,&fps);
		}else if (stream->pixconv && ms_filter_has_method(stream->pixconv, MS_FILTER_GET_FPS)){
			ms_filter_call_method(stream->pixconv,MS_FILTER_GET_FPS,&fps);
		}
	}
	return fps;
}

float video_stream_get_received_framerate(const VideoStream *stream){
	float fps=0;
	if (stream->ms.decoder != NULL && ms_filter_has_method(stream->ms.decoder, MS_FILTER_GET_FPS)) {
		ms_filter_call_method(stream->ms.decoder, MS_FILTER_GET_FPS, &fps);
	}
	return fps;
}

void video_stream_set_relay_session_id(VideoStream *stream, const char *id){
	ms_filter_call_method(stream->ms.rtpsend, MS_RTP_SEND_SET_RELAY_SESSION_ID,(void*)id);
}

void video_stream_enable_self_view(VideoStream *stream, bool_t val){
	MSFilter *out=stream->output;
	stream->corner=val ? 0 : -1;
	if (out){
		ms_filter_call_method(out,MS_VIDEO_DISPLAY_SET_LOCAL_VIEW_MODE,&stream->corner);
	}
}

void video_stream_set_render_callback (VideoStream *s, VideoStreamRenderCallback cb, void *user_pointer){
	s->rendercb=cb;
	s->render_pointer=user_pointer;
}

void video_stream_set_event_callback (VideoStream *s, VideoStreamEventCallback cb, void *user_pointer){
	s->eventcb=cb;
	s->event_pointer=user_pointer;
}

void video_stream_set_display_filter_name(VideoStream *s, const char *fname){
	if (s->display_name!=NULL){
		ms_free(s->display_name);
		s->display_name=NULL;
	}
	if (fname!=NULL)
		s->display_name=ms_strdup(fname);
}


static void ext_display_cb(void *ud, MSFilter* f, unsigned int event, void *eventdata){
	MSExtDisplayOutput *output=(MSExtDisplayOutput*)eventdata;
	VideoStream *st=(VideoStream*)ud;
	if (st->rendercb!=NULL){
		st->rendercb(st->render_pointer,
					output->local_view.w!=0 ? &output->local_view : NULL,
					output->remote_view.w!=0 ? &output->remote_view : NULL);
	}
}

void video_stream_set_direction(VideoStream *vs, VideoStreamDir dir){
	vs->dir=dir;
}

static MSVideoSize get_compatible_size(MSVideoSize maxsize, MSVideoSize wished_size){
	int max_area=maxsize.width*maxsize.height;
	int whished_area=wished_size.width*wished_size.height;
	if (whished_area>max_area){
		return maxsize;
	}
	return wished_size;
}

static MSVideoSize get_with_same_orientation_and_ratio(MSVideoSize size, MSVideoSize refsize){
	if (ms_video_size_get_orientation(refsize)!=ms_video_size_get_orientation(size)){
		int tmp;
		tmp=size.width;
		size.width=size.height;
		size.height=tmp;
	}
	size.height=(size.width*refsize.height)/refsize.width;
	return size;
}

static void configure_video_source(VideoStream *stream){
	MSVideoSize vsize,cam_vsize;
	float fps=15;
	MSPixFmt format=MS_PIX_FMT_UNKNOWN;
	MSVideoEncoderPixFmt encoder_supports_source_format;
	int ret;
	MSVideoSize preview_vsize;
	MSPinFormat pf={0};
	bool_t is_player=ms_filter_get_id(stream->source)==MS_ITC_SOURCE_ID;
	

	/* transmit orientation to source filter */
	if (ms_filter_has_method(stream->source, MS_VIDEO_CAPTURE_SET_DEVICE_ORIENTATION))
		ms_filter_call_method(stream->source,MS_VIDEO_CAPTURE_SET_DEVICE_ORIENTATION,&stream->device_orientation);
	/* initialize the capture device orientation for preview */
	if (!stream->display_filter_auto_rotate_enabled && ms_filter_has_method(stream->source, MS_VIDEO_DISPLAY_SET_DEVICE_ORIENTATION))
		ms_filter_call_method(stream->source,MS_VIDEO_DISPLAY_SET_DEVICE_ORIENTATION,&stream->device_orientation);

	/* transmit its preview window id if any to source filter*/
	if (stream->preview_window_id!=0){
		video_stream_set_native_preview_window_id(stream, stream->preview_window_id);
	}
	
	ms_filter_call_method(stream->ms.encoder,MS_FILTER_GET_VIDEO_SIZE,&vsize);
	vsize=get_compatible_size(vsize,stream->sent_vsize);
	if (stream->preview_vsize.width!=0){
		preview_vsize=stream->preview_vsize;
	}else{
		preview_vsize=vsize;
	}
	
	if (is_player){
		ms_filter_call_method(stream->source,MS_FILTER_GET_OUTPUT_FMT,&pf);
		if (pf.fmt==NULL || pf.fmt->vsize.width==0){
			MSVideoSize vsize={640,480};
			ms_error("Player does not give its format correctly [%s]",ms_fmt_descriptor_to_string(pf.fmt));
			/*put a default format as the error handling is complicated here*/
			pf.fmt=ms_factory_get_video_format(ms_factory_get_fallback(),"VP8",vsize,0,NULL);
		}
		cam_vsize=pf.fmt->vsize;
	}else{
		ms_filter_call_method(stream->source,MS_FILTER_SET_VIDEO_SIZE,&preview_vsize);
		/*the camera may not support the target size and suggest a one close to the target */
		ms_filter_call_method(stream->source,MS_FILTER_GET_VIDEO_SIZE,&cam_vsize);
	}

	if (cam_vsize.width*cam_vsize.height<=vsize.width*vsize.height){
		vsize=cam_vsize;
		ms_message("Output video size adjusted to match camera resolution (%ix%i)\n",vsize.width,vsize.height);
	} else {	
#if TARGET_IPHONE_SIMULATOR || defined(__arm__)
		ms_error("Camera is proposing a size bigger than encoder's suggested size (%ix%i > %ix%i) "
				   "Using the camera size as fallback because cropping or resizing is not implemented for arm.",
				   cam_vsize.width,cam_vsize.height,vsize.width,vsize.height);
		vsize=cam_vsize;
#else
		MSVideoSize resized=get_with_same_orientation_and_ratio(vsize,cam_vsize);
		if (resized.width & 0x1 || resized.height & 0x1){
			ms_warning("Resizing avoided because downsizing to an odd number of pixels (%ix%i)",resized.width,resized.height);
			vsize=cam_vsize;
		}else{
			vsize=resized;
			ms_warning("Camera video size greater than encoder one. A scaling filter will be used!\n");
		}
#endif
	}
	ms_filter_call_method(stream->ms.encoder,MS_FILTER_SET_VIDEO_SIZE,&vsize);
	ms_filter_call_method(stream->ms.encoder,MS_FILTER_GET_FPS,&fps);
	
	if (is_player){
		fps=pf.fmt->fps;
		if (fps==0) fps=15;
		ms_filter_call_method(stream->ms.encoder,MS_FILTER_SET_FPS,&fps);
	}else{
		if (stream->fps!=0)
			fps=stream->fps;
		ms_message("Setting sent vsize=%ix%i, fps=%f",vsize.width,vsize.height,fps);
		/* configure the filters */
		if (ms_filter_get_id(stream->source)!=MS_STATIC_IMAGE_ID) {
			ms_filter_call_method(stream->source,MS_FILTER_SET_FPS,&fps);
		}
		ms_filter_call_method(stream->ms.encoder,MS_FILTER_SET_FPS,&fps);
		/* get the output format for webcam reader */
		ms_filter_call_method(stream->source,MS_FILTER_GET_PIX_FMT,&format);
	}

	encoder_supports_source_format.supported = FALSE;
	encoder_supports_source_format.pixfmt = format;

	if (ms_filter_has_method(stream->ms.encoder, MS_VIDEO_ENCODER_SUPPORTS_PIXFMT) == TRUE) {
		ret = ms_filter_call_method(stream->ms.encoder, MS_VIDEO_ENCODER_SUPPORTS_PIXFMT, &encoder_supports_source_format);
	} else {
		ret = -1;
	}
	if (ret == -1) {
		/*encoder doesn't have MS_VIDEO_ENCODER_SUPPORTS_PIXFMT method*/
		/*we prefer in this case consider that it is not required to get the optimization of not going through pixconv and sizeconv*/
		encoder_supports_source_format.supported = FALSE;
	}

	if ((encoder_supports_source_format.supported == TRUE) || (stream->source_performs_encoding == TRUE)) {
		ms_filter_call_method(stream->ms.encoder, MS_FILTER_SET_PIX_FMT, &format);
	} else {
		if (format==MS_MJPEG){
			stream->pixconv=ms_filter_new(MS_MJPEG_DEC_ID);
		}else if (format==MS_PIX_FMT_UNKNOWN){
			stream->pixconv = ms_filter_create_decoder(pf.fmt->encoding);
		}else{
			stream->pixconv = ms_filter_new(MS_PIX_CONV_ID);
			/*set it to the pixconv */
			ms_filter_call_method(stream->pixconv,MS_FILTER_SET_PIX_FMT,&format);
			ms_filter_call_method(stream->pixconv,MS_FILTER_SET_VIDEO_SIZE,&cam_vsize);
		}
		stream->sizeconv=ms_filter_new(MS_SIZE_CONV_ID);
		ms_filter_call_method(stream->sizeconv,MS_FILTER_SET_VIDEO_SIZE,&vsize);
	}
	if (stream->ms.rc){
		ms_bitrate_controller_destroy(stream->ms.rc);
		stream->ms.rc=NULL;
	}
	if (stream->ms.rc_enable){
		switch (stream->ms.rc_algorithm){
		case MSQosAnalyzerAlgorithmSimple:
			stream->ms.rc=ms_av_bitrate_controller_new(NULL,NULL,stream->ms.sessions.rtp_session,stream->ms.encoder);
			break;
		case MSQosAnalyzerAlgorithmStateful:
			stream->ms.rc=ms_bandwidth_bitrate_controller_new(NULL, NULL, stream->ms.sessions.rtp_session,stream->ms.encoder);
			break;
		}
	}
}



static void configure_itc(VideoStream *stream){
	if (stream->itcsink){
		MSPinFormat pf={0};
		ms_filter_call_method(stream->ms.decoder,MS_FILTER_GET_OUTPUT_FMT,&pf);
		if (pf.fmt){
			MSPinFormat pinfmt={0};
			RtpSession *session=stream->ms.sessions.rtp_session;
			PayloadType *pt=rtp_profile_get_payload(rtp_session_get_profile(session),rtp_session_get_recv_payload_type(session));
			if (!pt) pt=rtp_profile_get_payload(rtp_session_get_profile(session),rtp_session_get_send_payload_type(session));
			if (pt){
				MSFmtDescriptor tmp=*pf.fmt;
				tmp.encoding=pt->mime_type;
				tmp.rate=pt->clock_rate;
				pinfmt.pin=0;
				pinfmt.fmt=ms_factory_get_format(ms_factory_get_fallback(),&tmp);
				ms_filter_call_method(stream->itcsink,MS_FILTER_SET_INPUT_FMT,&pinfmt);
				ms_message("configure_itc(): format set to %s",ms_fmt_descriptor_to_string(pinfmt.fmt));
			}
		}else ms_warning("configure_itc(): video decoder doesn't give output format.");
	}
}

static void configure_decoder(VideoStream *stream, PayloadType *pt){
	bool_t avpf_enabled=!!(pt->flags & PAYLOAD_TYPE_RTCP_FEEDBACK_ENABLED);
	ms_filter_call_method(stream->ms.decoder, MS_VIDEO_DECODER_ENABLE_AVPF, &avpf_enabled);
	ms_filter_call_method(stream->ms.decoder, MS_VIDEO_DECODER_FREEZE_ON_ERROR, &stream->freeze_on_error);
	ms_filter_add_notify_callback(stream->ms.decoder, event_cb, stream, FALSE);
	/* It is important that the internal_event_cb is called synchronously! */
	ms_filter_add_notify_callback(stream->ms.decoder, internal_event_cb, stream, TRUE);
}

static void video_stream_payload_type_changed(RtpSession *session, unsigned long data){
	VideoStream *stream = (VideoStream *)data;
	RtpProfile *prof = rtp_session_get_profile(session);
	PayloadType *pt = rtp_profile_get_payload(prof, rtp_session_get_recv_payload_type(session));
	if (mediastream_payload_type_changed(session,data) && pt)
		configure_decoder(stream,pt);
	configure_itc(stream);
}

int video_stream_start (VideoStream *stream, RtpProfile *profile, const char *rem_rtp_ip, int rem_rtp_port,
	const char *rem_rtcp_ip, int rem_rtcp_port, int payload, int jitt_comp, MSWebCam *cam){
	if (cam==NULL){
		cam = ms_web_cam_manager_get_default_cam( ms_web_cam_manager_get() );
	}
	return video_stream_start_with_source(stream, profile, rem_rtp_ip, rem_rtp_port, rem_rtcp_ip, rem_rtcp_port, payload, jitt_comp, cam, ms_web_cam_create_reader(cam));
}

static void apply_bitrate_limit(VideoStream *stream, PayloadType *pt) {
	MSVideoConfiguration *vconf_list = NULL;
	ms_message("Limiting bitrate of video encoder to %i bits/s for stream [%p]",pt->normal_bitrate,stream);
	ms_filter_call_method(stream->ms.encoder, MS_VIDEO_ENCODER_GET_CONFIGURATION_LIST, &vconf_list);
	if (vconf_list != NULL) {
		MSVideoConfiguration vconf = ms_video_find_best_configuration_for_bitrate(vconf_list, pt->normal_bitrate, ms_get_cpu_count());
		/* Adjust configuration video size to use the user preferred video size if it is lower that the configuration one. */
		if ((stream->sent_vsize.height * stream->sent_vsize.width) < (vconf.vsize.height * vconf.vsize.width)) {
			vconf.vsize = stream->sent_vsize;
		}
		ms_filter_call_method(stream->ms.encoder, MS_VIDEO_ENCODER_SET_CONFIGURATION, &vconf);
	} else {
		ms_filter_call_method(stream->ms.encoder, MS_FILTER_SET_BITRATE, &pt->normal_bitrate);
	}
	rtp_session_set_target_upload_bandwidth(stream->ms.sessions.rtp_session, pt->normal_bitrate);
}


int video_stream_start_with_source (VideoStream *stream, RtpProfile *profile, const char *rem_rtp_ip, int rem_rtp_port,
	const char *rem_rtcp_ip, int rem_rtcp_port, int payload, int jitt_comp, MSWebCam* cam, MSFilter* source){
	PayloadType *pt;
	RtpSession *rtps=stream->ms.sessions.rtp_session;
	MSPixFmt format;
	MSVideoSize disp_size;
	JBParameters jbp;
	const int socket_buf_size=2000000;
	bool_t avpf_enabled = FALSE;

	if( source == NULL ){
		ms_error("videostream.c: no defined source");
		return -1;
	}

	pt=rtp_profile_get_payload(profile,payload);
	if (pt==NULL){
		ms_error("videostream.c: undefined payload type %d.", payload);
		return -1;
	}
	if (pt->flags & PAYLOAD_TYPE_RTCP_FEEDBACK_ENABLED) avpf_enabled = TRUE;

	if ((cam != NULL) && (cam->desc->encode_to_mime_type != NULL) && (cam->desc->encode_to_mime_type(cam, pt->mime_type) == TRUE)) {
		stream->source_performs_encoding = TRUE;
	}

	rtp_session_set_profile(rtps,profile);
	if (rem_rtp_port>0)
		rtp_session_set_remote_addr_full(rtps,rem_rtp_ip,rem_rtp_port,rem_rtcp_ip,rem_rtcp_port);
	if (rem_rtcp_port > 0) {
		rtp_session_enable_rtcp(rtps, TRUE);
	} else {
		rtp_session_enable_rtcp(rtps, FALSE);
	}
	rtp_session_set_payload_type(rtps,payload);
	rtp_session_set_jitter_compensation(rtps,jitt_comp);

	rtp_session_signal_connect(stream->ms.sessions.rtp_session,"payload_type_changed",
			(RtpCallback)video_stream_payload_type_changed,(unsigned long)&stream->ms);

	rtp_session_get_jitter_buffer_params(stream->ms.sessions.rtp_session,&jbp);
	jbp.max_packets=1000;//needed for high resolution video
	rtp_session_set_jitter_buffer_params(stream->ms.sessions.rtp_session,&jbp);
	rtp_session_set_rtp_socket_recv_buffer_size(stream->ms.sessions.rtp_session,socket_buf_size);
	rtp_session_set_rtp_socket_send_buffer_size(stream->ms.sessions.rtp_session,socket_buf_size);
	
	if (stream->dir==VideoStreamSendRecv || stream->dir==VideoStreamSendOnly){
		MSConnectionHelper ch;
		/*plumb the outgoing stream */

		if (rem_rtp_port>0) ms_filter_call_method(stream->ms.rtpsend,MS_RTP_SEND_SET_SESSION,stream->ms.sessions.rtp_session);
		if (stream->source_performs_encoding == FALSE) {
			stream->ms.encoder=ms_filter_create_encoder(pt->mime_type);
			if (stream->ms.encoder==NULL){
				/* big problem: we don't have a registered codec for this payload...*/
				ms_error("videostream.c: No encoder available for payload %i:%s.",payload,pt->mime_type);
				return -1;
			}
		}
		/* creates the filters */
		stream->cam=cam;
		stream->source = source;
		stream->tee = ms_filter_new(MS_TEE_ID);
		stream->local_jpegwriter=ms_filter_new(MS_JPEG_WRITER_ID);
		if (stream->source_performs_encoding == TRUE) {
			stream->ms.encoder = stream->source;	/* Consider the encoder is the source */
		}

		if (pt->normal_bitrate>0){
			apply_bitrate_limit(stream, pt);
		}
		if (pt->send_fmtp){
			ms_filter_call_method(stream->ms.encoder,MS_FILTER_ADD_FMTP,pt->send_fmtp);
		}
		ms_filter_call_method(stream->ms.encoder, MS_VIDEO_ENCODER_ENABLE_AVPF, &avpf_enabled);
		if (stream->use_preview_window){
			if (stream->rendercb==NULL){
				stream->output2=ms_filter_new_from_name (stream->display_name);
			}
		}

		configure_video_source (stream);
		/* and then connect all */
		ms_connection_helper_start(&ch);
		ms_connection_helper_link(&ch, stream->source, -1, 0);
		if (stream->pixconv) {
			ms_connection_helper_link(&ch, stream->pixconv, 0, 0);
		}
		ms_connection_helper_link(&ch, stream->tee, 0, 0);
		if (stream->sizeconv) {
			ms_connection_helper_link(&ch, stream->sizeconv, 0, 0);
		}
		if (stream->source_performs_encoding == FALSE) {
			ms_connection_helper_link(&ch, stream->ms.encoder, 0, 0);
		}
		ms_connection_helper_link(&ch, stream->ms.rtpsend, 0, -1);
		if (stream->output2){
			if (stream->preview_window_id!=0){
				ms_filter_call_method(stream->output2, MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID,&stream->preview_window_id);
			}
			ms_filter_link(stream->tee,1,stream->output2,0);
		}
		if (stream->local_jpegwriter){
			ms_filter_link(stream->tee,2,stream->local_jpegwriter,0);
		}
	}
	if (stream->dir==VideoStreamSendRecv || stream->dir==VideoStreamRecvOnly){
		MSConnectionHelper ch;

		/* create decoder first */
		stream->ms.decoder=ms_filter_create_decoder(pt->mime_type);
		if (stream->ms.decoder==NULL){
			/* big problem: we don't have a registered decoderfor this payload...*/
			ms_error("videostream.c: No decoder available for payload %i:%s.",payload,pt->mime_type);
			return -1;
		}

		/* display logic */
		if (stream->rendercb!=NULL){
			/* rendering logic delegated to user supplied callback */
			stream->output=ms_filter_new(MS_EXT_DISPLAY_ID);
			ms_filter_add_notify_callback(stream->output,ext_display_cb,stream,TRUE);
		}else{
			/* no user supplied callback -> create filter */
			MSVideoDisplayDecodingSupport decoding_support;

			/* Check if the decoding filter can perform the rendering */
			decoding_support.mime_type = pt->mime_type;
			decoding_support.supported = FALSE;
			ms_filter_call_method(stream->ms.decoder, MS_VIDEO_DECODER_SUPPORT_RENDERING, &decoding_support);
			stream->output_performs_decoding = decoding_support.supported;

			if (stream->output_performs_decoding) {
				stream->output = stream->ms.decoder;
			} else {
				/* Create default display filter */
				stream->output = ms_filter_new_from_name (stream->display_name);
			}
		}

		/* Don't allow null output */
		if(stream->output == NULL) {
			ms_fatal("No video display filter could be instantiated. Please check build-time configuration");
		}


		stream->ms.rtprecv = ms_filter_new (MS_RTP_RECV_ID);
		ms_filter_call_method(stream->ms.rtprecv,MS_RTP_RECV_SET_SESSION,stream->ms.sessions.rtp_session);

		if (stream->output_performs_decoding == FALSE) {
			stream->jpegwriter=ms_filter_new(MS_JPEG_WRITER_ID);
			if (stream->jpegwriter){
				stream->tee2=ms_filter_new(MS_TEE_ID);
			}
		}

		/* Define target upload bandwidth for RTCP packets sending. */
		if (pt->normal_bitrate > 0) {
			rtp_session_set_target_upload_bandwidth(stream->ms.sessions.rtp_session, pt->normal_bitrate);
		}

		/* set parameters to the decoder*/
		if (pt->send_fmtp){
			ms_filter_call_method(stream->ms.decoder,MS_FILTER_ADD_FMTP,pt->send_fmtp);
		}
		if (pt->recv_fmtp!=NULL)
			ms_filter_call_method(stream->ms.decoder,MS_FILTER_ADD_FMTP,(void*)pt->recv_fmtp);
		configure_decoder(stream,pt);

		/*force the decoder to output YUV420P */
		format=MS_YUV420P;
		ms_filter_call_method(stream->ms.decoder,MS_FILTER_SET_PIX_FMT,&format);

		/*configure the display window */
		if(stream->output != NULL) {
			int autofit = 1;
			disp_size.width=MS_VIDEO_SIZE_CIF_W;
			disp_size.height=MS_VIDEO_SIZE_CIF_H;
			ms_filter_call_method(stream->output,MS_FILTER_SET_VIDEO_SIZE,&disp_size);

			/* if pixconv is used, force yuv420 */
			if (stream->pixconv || !stream->source)
				ms_filter_call_method(stream->output,MS_FILTER_SET_PIX_FMT,&format);
			/* else, use format from input */
			else {
				MSPixFmt source_format;
				ms_filter_call_method(stream->source,MS_FILTER_GET_PIX_FMT,&source_format);
				ms_filter_call_method(stream->output,MS_FILTER_SET_PIX_FMT,&source_format);
			}

			ms_filter_call_method(stream->output,MS_VIDEO_DISPLAY_SET_LOCAL_VIEW_MODE,&stream->corner);
			if (stream->window_id!=0){
				autofit = 0;
				ms_filter_call_method(stream->output, MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID,&stream->window_id);
			}
			ms_filter_call_method(stream->output,MS_VIDEO_DISPLAY_ENABLE_AUTOFIT,&autofit);
			if (stream->display_filter_auto_rotate_enabled && ms_filter_has_method(stream->output, MS_VIDEO_DISPLAY_SET_DEVICE_ORIENTATION)) {
				ms_filter_call_method(stream->output,MS_VIDEO_DISPLAY_SET_DEVICE_ORIENTATION,&stream->device_orientation);
			}
		}
		/* and connect the filters */
		ms_connection_helper_start (&ch);
		ms_connection_helper_link (&ch,stream->ms.rtprecv,-1,0);
		if (stream->output_performs_decoding == FALSE) {
			if (stream->itcsink){
				ms_connection_helper_link(&ch,stream->tee3,0,0);
				ms_filter_link(stream->tee3,1,stream->itcsink,0);
				configure_itc(stream);
			}
			ms_connection_helper_link(&ch,stream->ms.decoder,0,0);
		}
		if (stream->tee2){
			ms_connection_helper_link (&ch,stream->tee2,0,0);
			ms_filter_link(stream->tee2,1,stream->jpegwriter,0);
		}
		if (stream->output!=NULL)
			ms_connection_helper_link (&ch,stream->output,0,-1);
		/* the video source must be send for preview , if it exists*/
		if (stream->tee!=NULL && stream->output!=NULL && stream->output2==NULL)
			ms_filter_link(stream->tee,1,stream->output,1);
	}
	if (stream->dir == VideoStreamSendOnly) {
		stream->ms.rtprecv = ms_filter_new (MS_RTP_RECV_ID);
		ms_filter_call_method(stream->ms.rtprecv, MS_RTP_RECV_SET_SESSION, stream->ms.sessions.rtp_session);
		stream->ms.voidsink = ms_filter_new(MS_VOID_SINK_ID);
		ms_filter_link(stream->ms.rtprecv, 0, stream->ms.voidsink, 0);
	}

	/* create the ticker */
	if (stream->ms.sessions.ticker==NULL) media_stream_start_ticker(&stream->ms);

	stream->ms.start_time=ms_time(NULL);
	stream->ms.is_beginning=TRUE;

	/* attach the graphs */
	if (stream->source)
		ms_ticker_attach (stream->ms.sessions.ticker, stream->source);
	if (stream->ms.rtprecv)
		ms_ticker_attach (stream->ms.sessions.ticker, stream->ms.rtprecv);

	stream->ms.state=MSStreamStarted;
	return 0;
}

void video_stream_prepare_video(VideoStream *stream){
	video_stream_unprepare_video(stream);
	stream->ms.rtprecv=ms_filter_new(MS_RTP_RECV_ID);
	rtp_session_set_payload_type(stream->ms.sessions.rtp_session,0);
	rtp_session_enable_rtcp(stream->ms.sessions.rtp_session, FALSE);
	ms_filter_call_method(stream->ms.rtprecv,MS_RTP_RECV_SET_SESSION,stream->ms.sessions.rtp_session);
	stream->ms.voidsink=ms_filter_new(MS_VOID_SINK_ID);
	ms_filter_link(stream->ms.rtprecv,0,stream->ms.voidsink,0);
	media_stream_start_ticker(&stream->ms);
	ms_ticker_attach(stream->ms.sessions.ticker,stream->ms.rtprecv);
	stream->ms.state=MSStreamPreparing;
}

void video_stream_unprepare_video(VideoStream *stream){
	if (stream->ms.state==MSStreamPreparing) {
		stop_preload_graph(stream);
		stream->ms.state=MSStreamInitialized;
	}
}

void video_stream_update_video_params(VideoStream *stream){
	/*calling video_stream_change_camera() does the job of unplumbing/replumbing and configuring the new graph*/
	video_stream_change_camera(stream,stream->cam);
}

static void _video_stream_change_camera(VideoStream *stream, MSWebCam *cam, MSFilter *sink){
	PayloadType *pt;
	RtpProfile *profile;
	int payload;
	bool_t keep_source=!(cam!=stream->cam || (sink && !stream->player_active) || (!sink && stream->player_active));
	bool_t encoder_has_builtin_converter = (!stream->pixconv && !stream->sizeconv);

	if (stream->ms.sessions.ticker && stream->source){
		ms_ticker_detach(stream->ms.sessions.ticker,stream->source);
		/*unlink source filters and subsequent post processin filters */
		if (encoder_has_builtin_converter || (stream->source_performs_encoding == TRUE)) {
			ms_filter_unlink(stream->source, 0, stream->tee, 0);
		} else {
			ms_filter_unlink (stream->source, 0, stream->pixconv, 0);
			ms_filter_unlink (stream->pixconv, 0, stream->tee, 0);
			ms_filter_unlink (stream->tee, 0, stream->sizeconv, 0);
			if (stream->source_performs_encoding == FALSE) {
				ms_filter_unlink(stream->sizeconv, 0, stream->ms.encoder, 0);
			} else {
				ms_filter_unlink(stream->sizeconv, 0, stream->ms.rtpsend, 0);
			}
		}
		/*destroy the filters */
		if (!keep_source) ms_filter_destroy(stream->source);
		if (!encoder_has_builtin_converter && (stream->source_performs_encoding == FALSE)) {
			ms_filter_destroy(stream->pixconv);
			ms_filter_destroy(stream->sizeconv);
		}

		/*re create new ones and configure them*/
		if (!keep_source) {
			if (sink){
				stream->source = ms_filter_new(MS_ITC_SOURCE_ID);
				ms_filter_call_method(sink,MS_ITC_SINK_CONNECT,stream->source);
				stream->player_active=TRUE;
			}else{
				stream->source = ms_web_cam_create_reader(cam);
				stream->cam=cam;
				stream->player_active=FALSE;
			}
		}
		if (stream->source_performs_encoding == TRUE) {
			stream->ms.encoder = stream->source;	/* Consider the encoder is the source */
		}

		/* update orientation for video output*/
		if (stream->output && stream->display_filter_auto_rotate_enabled && ms_filter_has_method(stream->output, MS_VIDEO_DISPLAY_SET_DEVICE_ORIENTATION)) {
			ms_filter_call_method(stream->output,MS_VIDEO_DISPLAY_SET_DEVICE_ORIENTATION,&stream->device_orientation);
		}

		/* Apply bitrate limit to increase video size if the preferred one has changed. */
		profile = rtp_session_get_profile(stream->ms.sessions.rtp_session);
		payload = rtp_session_get_send_payload_type(stream->ms.sessions.rtp_session);
		pt = rtp_profile_get_payload(profile, payload);
		if (pt->normal_bitrate > 0){
			apply_bitrate_limit(stream ,pt);
		}

		configure_video_source(stream);

		if (encoder_has_builtin_converter || (stream->source_performs_encoding == TRUE)) {
			ms_filter_link (stream->source, 0, stream->tee, 0);
		}
		else {
			ms_filter_link (stream->source, 0, stream->pixconv, 0);
			ms_filter_link (stream->pixconv, 0, stream->tee, 0);
			ms_filter_link (stream->tee, 0, stream->sizeconv, 0);
			if (stream->source_performs_encoding == FALSE) {
				ms_filter_link(stream->sizeconv, 0, stream->ms.encoder, 0);
			} else {
				ms_filter_link(stream->sizeconv, 0, stream->ms.rtpsend, 0);
			}
		}

		ms_ticker_attach(stream->ms.sessions.ticker,stream->source);
	}
}

void video_stream_change_camera(VideoStream *stream, MSWebCam *cam){
	_video_stream_change_camera(stream, cam, NULL);
}

void video_stream_open_player(VideoStream *stream, MSFilter *sink){
	ms_message("video_stream_open_player(): sink=%p",sink);
	_video_stream_change_camera(stream,stream->cam,sink);
}

void video_stream_close_player(VideoStream *stream){
	_video_stream_change_camera(stream,stream->cam,NULL);
}

void video_stream_send_fir(VideoStream *stream) {
	if (stream->ms.sessions.rtp_session != NULL) {
		rtp_session_send_rtcp_fb_fir(stream->ms.sessions.rtp_session);
	}
}

void video_stream_send_pli(VideoStream *stream) {
	if (stream->ms.sessions.rtp_session != NULL) {
		rtp_session_send_rtcp_fb_pli(stream->ms.sessions.rtp_session);
	}
}

void video_stream_send_sli(VideoStream *stream, uint16_t first, uint16_t number, uint8_t picture_id) {
	if (stream->ms.sessions.rtp_session != NULL) {
		rtp_session_send_rtcp_fb_sli(stream->ms.sessions.rtp_session, first, number, picture_id);
	}
}

void video_stream_send_rpsi(VideoStream *stream, uint8_t *bit_string, uint16_t bit_string_len) {
	if (stream->ms.sessions.rtp_session != NULL) {
		rtp_session_send_rtcp_fb_rpsi(stream->ms.sessions.rtp_session, bit_string, bit_string_len);
	}
}

void video_stream_send_vfu(VideoStream *stream){
	if (stream->ms.encoder)
		ms_filter_call_method_noarg(stream->ms.encoder, MS_VIDEO_ENCODER_REQ_VFU);
}

void
video_stream_stop (VideoStream * stream)
{
	stream->eventcb = NULL;
	stream->event_pointer = NULL;
	if (stream->ms.sessions.ticker){
		if (stream->ms.state == MSStreamPreparing) {
			stop_preload_graph(stream);
		} else {
			if (stream->source)
				ms_ticker_detach(stream->ms.sessions.ticker,stream->source);
			if (stream->ms.rtprecv)
				ms_ticker_detach(stream->ms.sessions.ticker,stream->ms.rtprecv);

			if (stream->ms.ice_check_list != NULL) {
				ice_check_list_print_route(stream->ms.ice_check_list, "Video session's route");
				stream->ms.ice_check_list = NULL;
			}
			rtp_stats_display(rtp_session_get_stats(stream->ms.sessions.rtp_session),
				"             VIDEO SESSION'S RTP STATISTICS                ");

			if (stream->source){
				MSConnectionHelper ch;
				ms_connection_helper_start(&ch);
				ms_connection_helper_unlink(&ch, stream->source, -1, 0);
				if (stream->pixconv) {
					ms_connection_helper_unlink(&ch, stream->pixconv, 0, 0);
				}
				ms_connection_helper_unlink(&ch, stream->tee, 0, 0);
				if (stream->sizeconv) {
					ms_connection_helper_unlink(&ch, stream->sizeconv, 0, 0);
				}
				if (stream->source_performs_encoding == FALSE) {
					ms_connection_helper_unlink(&ch, stream->ms.encoder, 0, 0);
				}
				ms_connection_helper_unlink(&ch, stream->ms.rtpsend, 0, -1);
				if (stream->output2){
					ms_filter_unlink(stream->tee,1,stream->output2,0);
				}
				if (stream->local_jpegwriter){
					ms_filter_unlink(stream->tee,2,stream->local_jpegwriter,0);
				}
			}
			if (stream->ms.voidsink) {
				ms_filter_unlink(stream->ms.rtprecv, 0, stream->ms.voidsink, 0);
			} else if (stream->ms.rtprecv){
				MSConnectionHelper h;
				ms_connection_helper_start (&h);
				ms_connection_helper_unlink (&h,stream->ms.rtprecv,-1,0);
				if (stream->output_performs_decoding == FALSE) {
					if (stream->itcsink){
						ms_connection_helper_unlink(&h,stream->tee3,0,0);
						ms_filter_unlink(stream->tee3,1,stream->itcsink,0);
					}
					ms_connection_helper_unlink (&h,stream->ms.decoder,0,0);
				}
				if (stream->tee2){
					ms_connection_helper_unlink (&h,stream->tee2,0,0);
					ms_filter_unlink(stream->tee2,1,stream->jpegwriter,0);
				}
				if(stream->output)
					ms_connection_helper_unlink (&h,stream->output,0,-1);
				if (stream->tee && stream->output && stream->output2==NULL)
					ms_filter_unlink(stream->tee,1,stream->output,1);
			}
		}
	}
	rtp_session_set_rtcp_xr_media_callbacks(stream->ms.sessions.rtp_session, NULL);
	rtp_session_signal_disconnect_by_callback(stream->ms.sessions.rtp_session,"payload_type_changed",(RtpCallback)video_stream_payload_type_changed);
	video_stream_free(stream);
}


void video_stream_show_video(VideoStream *stream, bool_t show){
	if (stream->output){
		ms_filter_call_method(stream->output,MS_VIDEO_DISPLAY_SHOW_VIDEO,&show);
	}
}


unsigned long video_stream_get_native_window_id(VideoStream *stream){
	unsigned long id;
	if (stream->output){
		if (ms_filter_call_method(stream->output,MS_VIDEO_DISPLAY_GET_NATIVE_WINDOW_ID,&id)==0)
			return id;
	}
	return stream->window_id;
}

void video_stream_set_native_window_id(VideoStream *stream, unsigned long id){
	stream->window_id=id;
	if (stream->output){
		ms_filter_call_method(stream->output,MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID,&id);
	}
}

void video_stream_set_native_preview_window_id(VideoStream *stream, unsigned long id){
	stream->preview_window_id=id;
#ifndef __ios
	if (stream->output2){
		ms_filter_call_method(stream->output2,MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID,&id);
	}
#endif
	if (stream->source){
		ms_filter_call_method(stream->source,MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID,&id);
	}
}

unsigned long video_stream_get_native_preview_window_id(VideoStream *stream){
	unsigned long id=0;
	if (stream->output2){
		if (ms_filter_call_method(stream->output2,MS_VIDEO_DISPLAY_GET_NATIVE_WINDOW_ID,&id)==0)
			return id;
	}
	if (stream->source){
		if (ms_filter_has_method(stream->source,MS_VIDEO_DISPLAY_GET_NATIVE_WINDOW_ID)
			&& ms_filter_call_method(stream->source,MS_VIDEO_DISPLAY_GET_NATIVE_WINDOW_ID,&id)==0)
			return id;
	}
	return stream->preview_window_id;
}

void video_stream_use_preview_video_window(VideoStream *stream, bool_t yesno){
	stream->use_preview_window=yesno;
}

void video_stream_set_device_rotation(VideoStream *stream, int orientation){
	stream->device_orientation = orientation;
}

void video_stream_set_freeze_on_error(VideoStream *stream, bool_t yesno) {
	stream->freeze_on_error = yesno;
}

int video_stream_get_camera_sensor_rotation(VideoStream *stream) {
	int rotation = -1;
	if (stream->source) {
		if (ms_filter_has_method(stream->source, MS_VIDEO_CAPTURE_GET_CAMERA_SENSOR_ROTATION)
			&& ms_filter_call_method(stream->source, MS_VIDEO_CAPTURE_GET_CAMERA_SENSOR_ROTATION, &rotation) == 0)
			return rotation;
	}
	return -1;
}

VideoPreview * video_preview_new(void){
	VideoPreview *stream = (VideoPreview *)ms_new0 (VideoPreview, 1);
	MS_VIDEO_SIZE_ASSIGN(stream->sent_vsize, CIF);
	choose_display_name(stream);
	stream->ms.owns_sessions=TRUE;
	return stream;
}


void video_preview_start(VideoPreview *stream, MSWebCam *device){
	MSPixFmt format;
	float fps;
	int mirroring=1;
	int corner=-1;
	MSVideoSize disp_size=stream->sent_vsize;
	MSVideoSize vsize=disp_size;
	const char *displaytype=stream->display_name;

	if (stream->fps!=0) fps=stream->fps;
	else fps=(float)29.97;

	stream->source = ms_web_cam_create_reader(device);

	/* Transmit orientation to source filter. */
	if (ms_filter_has_method(stream->source, MS_VIDEO_CAPTURE_SET_DEVICE_ORIENTATION))
		ms_filter_call_method(stream->source, MS_VIDEO_CAPTURE_SET_DEVICE_ORIENTATION, &stream->device_orientation);
	/* Initialize the capture device orientation. */
	if (ms_filter_has_method(stream->source, MS_VIDEO_DISPLAY_SET_DEVICE_ORIENTATION)) {
		ms_filter_call_method(stream->source, MS_VIDEO_DISPLAY_SET_DEVICE_ORIENTATION, &stream->device_orientation);
	}

	/* configure the filters */
	ms_filter_call_method(stream->source,MS_FILTER_SET_VIDEO_SIZE,&vsize);
	if (ms_filter_get_id(stream->source)!=MS_STATIC_IMAGE_ID)
		ms_filter_call_method(stream->source,MS_FILTER_SET_FPS,&fps);
	ms_filter_call_method(stream->source,MS_FILTER_GET_PIX_FMT,&format);
	ms_filter_call_method(stream->source,MS_FILTER_GET_VIDEO_SIZE,&vsize);
	if (format==MS_MJPEG){
		stream->pixconv=ms_filter_new(MS_MJPEG_DEC_ID);
	}else{
		stream->pixconv=ms_filter_new(MS_PIX_CONV_ID);
		ms_filter_call_method(stream->pixconv,MS_FILTER_SET_PIX_FMT,&format);
		ms_filter_call_method(stream->pixconv,MS_FILTER_SET_VIDEO_SIZE,&vsize);
	}

	format=MS_YUV420P;

	stream->output2=ms_filter_new_from_name (displaytype);
	ms_filter_call_method(stream->output2,MS_FILTER_SET_PIX_FMT,&format);
	ms_filter_call_method(stream->output2,MS_FILTER_SET_VIDEO_SIZE,&disp_size);
	ms_filter_call_method(stream->output2,MS_VIDEO_DISPLAY_ENABLE_MIRRORING,&mirroring);
	ms_filter_call_method(stream->output2,MS_VIDEO_DISPLAY_SET_LOCAL_VIEW_MODE,&corner);
	/* and then connect all */

	ms_filter_link(stream->source,0, stream->pixconv,0);
	ms_filter_link(stream->pixconv, 0, stream->output2, 0);

	if (stream->preview_window_id!=0){
		video_stream_set_native_preview_window_id(stream, stream->preview_window_id);
	}

	/* create the ticker */
	stream->ms.sessions.ticker = ms_ticker_new();
	ms_ticker_set_name(stream->ms.sessions.ticker,"Video MSTicker");
	ms_ticker_attach (stream->ms.sessions.ticker, stream->source);
	stream->ms.state=MSStreamStarted;
}

void video_preview_stop(VideoPreview *stream){
	ms_ticker_detach(stream->ms.sessions.ticker, stream->source);
	ms_filter_unlink(stream->source,0,stream->pixconv,0);
	ms_filter_unlink(stream->pixconv,0,stream->output2,0);
	video_stream_free(stream);
}

MSFilter* video_preview_stop_reuse_source(VideoPreview *stream){
	MSFilter* source = stream->source;
	ms_ticker_detach(stream->ms.sessions.ticker, stream->source);
	ms_filter_unlink(stream->source,0,stream->pixconv,0);
	ms_filter_unlink(stream->pixconv,0,stream->output2,0);

	ms_message("video_preview_stop_reuse_source: keep %p", source);
	stream->source = NULL; // prevent destroy
	video_stream_free(stream);
	return source;
}


int video_stream_recv_only_start(VideoStream *videostream, RtpProfile *profile, const char *addr, int port, int used_pt, int jitt_comp){
	video_stream_set_direction(videostream,VideoStreamRecvOnly);
	return video_stream_start(videostream,profile,addr,port,addr,port+1,used_pt,jitt_comp,NULL);
}

int video_stream_send_only_start(VideoStream *videostream,
				RtpProfile *profile, const char *addr, int port, int rtcp_port,
				int used_pt, int  jitt_comp, MSWebCam *device){
	video_stream_set_direction (videostream,VideoStreamSendOnly);
	return video_stream_start(videostream,profile,addr,port,addr,rtcp_port,used_pt,jitt_comp,device);
}


void video_stream_recv_only_stop(VideoStream *vs){
	video_stream_stop(vs);
}

void video_stream_send_only_stop(VideoStream *vs){
	video_stream_stop(vs);
}

/* enable ZRTP on the video stream using information from the audio stream */
void video_stream_enable_zrtp(VideoStream *vstream, AudioStream *astream, OrtpZrtpParams *param){
	if (astream->ms.sessions.zrtp_context != NULL && vstream->ms.sessions.zrtp_context == NULL) {
		vstream->ms.sessions.zrtp_context=ortp_zrtp_multistream_new(astream->ms.sessions.zrtp_context, vstream->ms.sessions.rtp_session, param);
	} else if (vstream->ms.sessions.zrtp_context && !vstream->ms.sessions.is_secured)
		ortp_zrtp_reset_transmition_timer(vstream->ms.sessions.zrtp_context,vstream->ms.sessions.rtp_session);
}

void video_stream_enable_display_filter_auto_rotate(VideoStream* stream, bool_t enable) {
	stream->display_filter_auto_rotate_enabled = enable;
}

bool_t video_stream_is_decoding_error_to_be_reported(VideoStream *stream, uint32_t ms) {
	if (((stream->ms.sessions.ticker->time - stream->last_reported_decoding_error_time) > ms) || (stream->last_reported_decoding_error_time == 0))
		return TRUE;
	else
		return FALSE;
}

void video_stream_decoding_error_reported(VideoStream *stream) {
	stream->last_reported_decoding_error_time = stream->ms.sessions.ticker->time;
}

void video_stream_decoding_error_recovered(VideoStream *stream) {
	stream->last_reported_decoding_error_time = 0;
}

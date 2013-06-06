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

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msinterfaces.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msrtp.h"
#include "mediastreamer2/msvideoout.h"
#include "mediastreamer2/msextdisplay.h"
#include "private.h"

#include <ortp/zrtp.h>


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
	if (stream->display_name!=NULL)
		ms_free(stream->display_name);

	ms_free (stream);
}

static void event_cb(void *ud, MSFilter* f, unsigned int event, void *eventdata){
	VideoStream *st=(VideoStream*)ud;
	ms_message("event_cb called %u", event);
	if (st->eventcb!=NULL){
		st->eventcb(st->event_pointer,f,event,eventdata);
	}
}

static void video_steam_process_rtcp(VideoStream *stream, mblk_t *m){
	do{
		if (rtcp_is_SR(m)){
			const report_block_t *rb;
			ms_message("video_steam_process_rtcp: receiving RTCP SR");
			rb=rtcp_SR_get_report_block(m,0);
			if (rb){
				unsigned int ij;
				float rt=rtp_session_get_round_trip_propagation(stream->ms.session);
				float flost;
				ij=report_block_get_interarrival_jitter(rb);
				flost=(float)(100.0*report_block_get_fraction_lost(rb)/256.0);
				ms_message("video_steam_process_rtcp: interarrival jitter=%u , lost packets percentage since last report=%f, round trip time=%f seconds",ij,flost,rt);
				if (stream->ms.rc)
					ms_bitrate_controller_process_rtcp(stream->ms.rc,m);
			}
		}
	}while(rtcp_next_packet(m));
}

static void stop_preload_graph(VideoStream *stream){
	ms_ticker_detach(stream->ms.ticker,stream->ms.rtprecv);
	ms_filter_unlink(stream->ms.rtprecv,0,stream->ms.voidsink,0);
	ms_filter_destroy(stream->ms.voidsink);
	ms_filter_destroy(stream->ms.rtprecv);
	stream->ms.voidsink=stream->ms.rtprecv=NULL;
	stream->prepare_ongoing = FALSE;
}

void video_stream_iterate(VideoStream *stream){
	/*
	if (stream->output!=NULL)
		ms_filter_call_method_noarg(stream->output,
			MS_VIDEO_OUT_HANDLE_RESIZING);
	*/
	if (stream->ms.evq){
		OrtpEvent *ev;
		while (NULL != (ev=ortp_ev_queue_get(stream->ms.evq))) {
			OrtpEventType evt=ortp_event_get_type(ev);
			if (evt == ORTP_EVENT_RTCP_PACKET_RECEIVED){
				OrtpEventData *evd=ortp_event_get_data(ev);
				video_steam_process_rtcp(stream,evd->packet);
			}else if ((evt == ORTP_EVENT_STUN_PACKET_RECEIVED) && (stream->ms.ice_check_list)) {
				ice_handle_stun_packet(stream->ms.ice_check_list,stream->ms.session,ortp_event_get_data(ev));
			}
			ortp_event_destroy(ev);
		}
	}
	media_stream_iterate(&stream->ms);
}

static void choose_display_name(VideoStream *stream){
#ifdef WIN32
	stream->display_name=ms_strdup("MSDrawDibDisplay");
#elif defined(ANDROID)
	stream->display_name=ms_strdup("MSAndroidDisplay");
#elif __APPLE__ && !defined(__ios)
	stream->display_name=ms_strdup("MSOSXGLDisplay");
#elif defined(HAVE_GL)
	stream->display_name=ms_strdup("MSGLXVideo");
#elif defined (HAVE_XV)
	stream->display_name=ms_strdup("MSX11Video");
#elif defined(__ios)
	stream->display_name=ms_strdup("IOSDisplay");	
#else
	stream->display_name=ms_strdup("MSVideoOut");
#endif
}

VideoStream *video_stream_new(int loc_rtp_port, int loc_rtcp_port, bool_t use_ipv6){
	VideoStream *stream = (VideoStream *)ms_new0 (VideoStream, 1);
	stream->ms.type = VideoStreamType;
	stream->ms.session=create_duplex_rtpsession(loc_rtp_port,loc_rtcp_port,use_ipv6);
	stream->ms.qi=ms_quality_indicator_new(stream->ms.session);
	stream->ms.evq=ortp_ev_queue_new();
	stream->ms.rtpsend=ms_filter_new(MS_RTP_SEND_ID);
	stream->ms.ice_check_list=NULL;
	rtp_session_register_event_queue(stream->ms.session,stream->ms.evq);
	stream->sent_vsize.width=MS_VIDEO_SIZE_CIF_W;
	stream->sent_vsize.height=MS_VIDEO_SIZE_CIF_H;
	stream->dir=VideoStreamSendRecv;
	stream->display_filter_auto_rotate_enabled=0;
	stream->source_performs_encoding = FALSE;
	stream->output_performs_decoding = FALSE;
	choose_display_name(stream);

	return stream;
}

void video_stream_set_sent_video_size(VideoStream *stream, MSVideoSize vsize){
	ms_message("Setting video size %dx%d", vsize.width, vsize.height);
	stream->sent_vsize=vsize;
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

static MSVideoSize get_with_same_orientation(MSVideoSize size, MSVideoSize refsize){
	if (ms_video_size_get_orientation(refsize)!=ms_video_size_get_orientation(size)){
		int tmp;
		tmp=size.width;
		size.width=size.height;
		size.height=tmp;
	}
	return size;
}

static void configure_video_source(VideoStream *stream){
	MSVideoSize vsize,cam_vsize;
	float fps=15;
	MSPixFmt format;
	bool_t encoder_has_builtin_converter = FALSE;

	/* transmit orientation to source filter */
	ms_filter_call_method(stream->source,MS_VIDEO_CAPTURE_SET_DEVICE_ORIENTATION,&stream->device_orientation);
	
    /* transmit its preview window id if any to source filter*/
	if (stream->preview_window_id!=0){
		video_stream_set_native_preview_window_id(stream, stream->preview_window_id);
	}

	ms_filter_call_method(stream->ms.encoder, MS_VIDEO_ENCODER_HAS_BUILTIN_CONVERTER, &encoder_has_builtin_converter);
	ms_filter_call_method(stream->ms.encoder,MS_FILTER_GET_VIDEO_SIZE,&vsize);
	vsize=get_compatible_size(vsize,stream->sent_vsize);
	ms_filter_call_method(stream->source,MS_FILTER_SET_VIDEO_SIZE,&vsize);
	/*the camera may not support the target size and suggest a one close to the target */
	ms_filter_call_method(stream->source,MS_FILTER_GET_VIDEO_SIZE,&cam_vsize);
	if (cam_vsize.width*cam_vsize.height<=vsize.width*vsize.height &&
			cam_vsize.width != vsize.width){
		vsize=cam_vsize;
		ms_message("Output video size adjusted to match camera resolution (%ix%i)\n",vsize.width,vsize.height);
	} else if (cam_vsize.width*cam_vsize.height>vsize.width*vsize.height){
#if TARGET_IPHONE_SIMULATOR || defined(__arm__)
		ms_error("Camera is proposing a size bigger than encoder's suggested size (%ix%i > %ix%i) "
		           "Using the camera size as fallback because cropping or resizing is not implemented for arm.",
		           cam_vsize.width,cam_vsize.height,vsize.width,vsize.height);
		vsize=cam_vsize;
#else
		vsize=get_with_same_orientation(vsize,cam_vsize);
		ms_warning("Camera video size greater than encoder one. A scaling filter will be used!\n");
#endif
	}
	ms_filter_call_method(stream->ms.encoder,MS_FILTER_SET_VIDEO_SIZE,&vsize);
	ms_filter_call_method(stream->ms.encoder,MS_FILTER_GET_FPS,&fps);
	ms_message("Setting sent vsize=%ix%i, fps=%f",vsize.width,vsize.height,fps);
	/* configure the filters */
	if (ms_filter_get_id(stream->source)!=MS_STATIC_IMAGE_ID) {
		ms_filter_call_method(stream->source,MS_FILTER_SET_FPS,&fps);
	}
	/* get the output format for webcam reader */
	ms_filter_call_method(stream->source,MS_FILTER_GET_PIX_FMT,&format);

	if ((encoder_has_builtin_converter == TRUE) || (stream->source_performs_encoding == TRUE)) {
		ms_filter_call_method(stream->ms.encoder, MS_FILTER_SET_PIX_FMT, &format);
	} else {
		if (format==MS_MJPEG){
			stream->pixconv=ms_filter_new(MS_MJPEG_DEC_ID);
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
	if (stream->ms.use_rc){
		stream->ms.rc=ms_av_bitrate_controller_new(NULL,NULL,stream->ms.session,stream->ms.encoder);
	}
}

int video_stream_start (VideoStream *stream, RtpProfile *profile, const char *rem_rtp_ip, int rem_rtp_port,
	const char *rem_rtcp_ip, int rem_rtcp_port, int payload, int jitt_comp, MSWebCam *cam){
	PayloadType *pt;
	RtpSession *rtps=stream->ms.session;
	MSPixFmt format;
	MSVideoSize disp_size;
	int tmp;
	JBParameters jbp;
	const int socket_buf_size=2000000;

	if (cam==NULL){
		cam=ms_web_cam_manager_get_default_cam (
		      ms_web_cam_manager_get());
	}

	pt=rtp_profile_get_payload(profile,payload);
	if (pt==NULL){
		ms_error("videostream.c: undefined payload type.");
		return -1;
	}

	if ((cam != NULL) && (cam->desc->encode_to_mime_type != NULL) && (cam->desc->encode_to_mime_type(cam, pt->mime_type) == TRUE)) {
		stream->source_performs_encoding = TRUE;
	}

	rtp_session_set_profile(rtps,profile);
	if (rem_rtp_port>0) rtp_session_set_remote_addr_full(rtps,rem_rtp_ip,rem_rtp_port,rem_rtcp_ip,rem_rtcp_port);
	rtp_session_set_payload_type(rtps,payload);
	rtp_session_set_jitter_compensation(rtps,jitt_comp);

	rtp_session_signal_connect(stream->ms.session,"payload_type_changed",
			(RtpCallback)mediastream_payload_type_changed,(unsigned long)&stream->ms);

	rtp_session_get_jitter_buffer_params(stream->ms.session,&jbp);
	jbp.max_packets=1000;//needed for high resolution video
	rtp_session_set_jitter_buffer_params(stream->ms.session,&jbp);
	rtp_session_set_rtp_socket_recv_buffer_size(stream->ms.session,socket_buf_size);
	rtp_session_set_rtp_socket_send_buffer_size(stream->ms.session,socket_buf_size);

	if (stream->dir==VideoStreamSendRecv || stream->dir==VideoStreamSendOnly){
		MSConnectionHelper ch;
		/*plumb the outgoing stream */

		if (rem_rtp_port>0) ms_filter_call_method(stream->ms.rtpsend,MS_RTP_SEND_SET_SESSION,stream->ms.session);
		if (stream->source_performs_encoding == FALSE) {
			stream->ms.encoder=ms_filter_create_encoder(pt->mime_type);
			if ((stream->ms.encoder==NULL) ){
				/* big problem: we don't have a registered codec for this payload...*/
				ms_error("videostream.c: No encoder available for payload %i:%s.",payload,pt->mime_type);
				return -1;
			}
		}
		/* creates the filters */
		stream->cam=cam;
		stream->source = ms_web_cam_create_reader(cam);
		stream->tee = ms_filter_new(MS_TEE_ID);
		if (stream->source_performs_encoding == TRUE) {
			stream->ms.encoder = stream->source;	/* Consider the encoder is the source */
		}

		if (pt->normal_bitrate>0){
			ms_message("Limiting bitrate of video encoder to %i bits/s",pt->normal_bitrate);
			ms_filter_call_method(stream->ms.encoder,MS_FILTER_SET_BITRATE,&pt->normal_bitrate);
		}
		if (pt->send_fmtp){
			ms_filter_call_method(stream->ms.encoder,MS_FILTER_ADD_FMTP,pt->send_fmtp);
		}
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
		if (stream->sizeconv) {
			ms_connection_helper_link(&ch, stream->sizeconv, 0, 0);
		}
		ms_connection_helper_link(&ch, stream->tee, 0, 0);
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
	}
	if (stream->dir==VideoStreamSendRecv || stream->dir==VideoStreamRecvOnly){
		MSConnectionHelper ch;
		MSVideoDisplayDecodingSupport decoding_support;

		if (stream->rendercb!=NULL){
			stream->output=ms_filter_new(MS_EXT_DISPLAY_ID);
			ms_filter_set_notify_callback(stream->output,ext_display_cb,stream);
		}else{
			stream->output=ms_filter_new_from_name (stream->display_name);
		}

		/* Don't allow null output */
		if(stream->output == NULL) {
			ms_fatal("No video display filter could be instantiated. Please check build-time configuration");
		}

		/* Check if the output filter can perform the decoding process */
		decoding_support.mime_type = pt->mime_type;
		decoding_support.supported = FALSE;
		ms_filter_call_method(stream->output, MS_VIDEO_DISPLAY_SUPPORT_DECODING, &decoding_support);
		stream->output_performs_decoding = decoding_support.supported;

		/*plumb the incoming stream */
		if (stream->output_performs_decoding == TRUE) {
			stream->ms.decoder = stream->output;	/* Consider the decoder is the output */
		} else {
			stream->ms.decoder=ms_filter_create_decoder(pt->mime_type);
			if ((stream->ms.decoder==NULL) ){
				/* big problem: we don't have a registered decoderfor this payload...*/
				ms_error("videostream.c: No decoder available for payload %i:%s.",payload,pt->mime_type);
				ms_filter_destroy(stream->output);
				return -1;
			}
		}
		ms_filter_set_notify_callback(stream->ms.decoder, event_cb, stream);

		stream->ms.rtprecv = ms_filter_new (MS_RTP_RECV_ID);
		ms_filter_call_method(stream->ms.rtprecv,MS_RTP_RECV_SET_SESSION,stream->ms.session);

		if (stream->output_performs_decoding == FALSE) {
			stream->jpegwriter=ms_filter_new(MS_JPEG_WRITER_ID);
			if (stream->jpegwriter)
				stream->tee2=ms_filter_new(MS_TEE_ID);
		}

		/* set parameters to the decoder*/
		if (pt->send_fmtp){
			ms_filter_call_method(stream->ms.decoder,MS_FILTER_ADD_FMTP,pt->send_fmtp);
		}
		if (pt->recv_fmtp!=NULL)
			ms_filter_call_method(stream->ms.decoder,MS_FILTER_ADD_FMTP,(void*)pt->recv_fmtp);

		/*force the decoder to output YUV420P */
		format=MS_YUV420P;
		ms_filter_call_method(stream->ms.decoder,MS_FILTER_SET_PIX_FMT,&format);

		/*configure the display window */
		if(stream->output != NULL) {
			disp_size.width=MS_VIDEO_SIZE_CIF_W;
			disp_size.height=MS_VIDEO_SIZE_CIF_H;
			tmp=1;
			ms_filter_call_method(stream->output,MS_FILTER_SET_VIDEO_SIZE,&disp_size);
			ms_filter_call_method(stream->output,MS_VIDEO_DISPLAY_ENABLE_AUTOFIT,&tmp);
			ms_filter_call_method(stream->output,MS_FILTER_SET_PIX_FMT,&format);
			ms_filter_call_method(stream->output,MS_VIDEO_DISPLAY_SET_LOCAL_VIEW_MODE,&stream->corner);
			if (stream->window_id!=0){
				ms_filter_call_method(stream->output, MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID,&stream->window_id);
			}
			if (stream->display_filter_auto_rotate_enabled) {
				ms_filter_call_method(stream->output,MS_VIDEO_DISPLAY_SET_DEVICE_ORIENTATION,&stream->device_orientation);
			}
		}
		/* and connect the filters */
		ms_connection_helper_start (&ch);
		ms_connection_helper_link (&ch,stream->ms.rtprecv,-1,0);
		if (stream->output_performs_decoding == FALSE) {
			ms_connection_helper_link (&ch,stream->ms.decoder,0,0);
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
		ms_filter_call_method(stream->ms.rtprecv, MS_RTP_RECV_SET_SESSION, stream->ms.session);
		stream->ms.voidsink = ms_filter_new(MS_VOID_SINK_ID);
		ms_filter_link(stream->ms.rtprecv, 0, stream->ms.voidsink, 0);
	}

	/* create the ticker */
	if (stream->ms.ticker==NULL) start_ticker(&stream->ms);
	
	stream->ms.start_time=ms_time(NULL);
	stream->ms.is_beginning=TRUE;

	/* attach the graphs */
	if (stream->source)
		ms_ticker_attach (stream->ms.ticker, stream->source);
	if (stream->ms.rtprecv)
		ms_ticker_attach (stream->ms.ticker, stream->ms.rtprecv);
	return 0;
}

void video_stream_prepare_video(VideoStream *stream){
	stream->prepare_ongoing = TRUE;
	video_stream_unprepare_video(stream);
	stream->ms.rtprecv=ms_filter_new(MS_RTP_RECV_ID);
	rtp_session_set_payload_type(stream->ms.session,0);
	ms_filter_call_method(stream->ms.rtprecv,MS_RTP_RECV_SET_SESSION,stream->ms.session);
	stream->ms.voidsink=ms_filter_new(MS_VOID_SINK_ID);
	ms_filter_link(stream->ms.rtprecv,0,stream->ms.voidsink,0);
	start_ticker(&stream->ms);
	ms_ticker_attach(stream->ms.ticker,stream->ms.rtprecv);
}

void video_stream_unprepare_video(VideoStream *stream){
	if (stream->ms.voidsink) {
		stop_preload_graph(stream);
	}
}

void video_stream_update_video_params(VideoStream *stream){
	/*calling video_stream_change_camera() does the job of unplumbing/replumbing and configuring the new graph*/
	video_stream_change_camera(stream,stream->cam);
}

void video_stream_change_camera(VideoStream *stream, MSWebCam *cam){
	bool_t keep_source=(cam==stream->cam);
	bool_t encoder_has_builtin_converter = (!stream->pixconv && !stream->sizeconv);

	if (stream->ms.ticker && stream->source){
		ms_ticker_detach(stream->ms.ticker,stream->source);
		/*unlink source filters and subsequent post processin filters */
		if (encoder_has_builtin_converter || (stream->source_performs_encoding == TRUE)) {
			ms_filter_unlink(stream->source, 0, stream->tee, 0);
		} else {
			ms_filter_unlink (stream->source, 0, stream->pixconv, 0);
			ms_filter_unlink (stream->pixconv, 0, stream->sizeconv, 0);
			ms_filter_unlink (stream->sizeconv, 0, stream->tee, 0);
		}
		/*destroy the filters */
		if (!keep_source) ms_filter_destroy(stream->source);
		if (!encoder_has_builtin_converter && (stream->source_performs_encoding == FALSE)) {
			ms_filter_destroy(stream->pixconv);
			ms_filter_destroy(stream->sizeconv);
		}

		/*re create new ones and configure them*/
		if (!keep_source) stream->source = ms_web_cam_create_reader(cam);
		stream->cam=cam;

		/* update orientation */
		if (stream->source){
			ms_filter_call_method(stream->source,MS_VIDEO_CAPTURE_SET_DEVICE_ORIENTATION,&stream->device_orientation);
			if (!stream->display_filter_auto_rotate_enabled)
				ms_filter_call_method(stream->source,MS_VIDEO_DISPLAY_SET_DEVICE_ORIENTATION,&stream->device_orientation);
		}
		if (stream->output && stream->display_filter_auto_rotate_enabled) {
			ms_filter_call_method(stream->output,MS_VIDEO_DISPLAY_SET_DEVICE_ORIENTATION,&stream->device_orientation);
		}

		configure_video_source(stream);

		if (encoder_has_builtin_converter || (stream->source_performs_encoding == TRUE)) {
			ms_filter_link (stream->source, 0, stream->tee, 0);
		}
		else {
			ms_filter_link (stream->source, 0, stream->pixconv, 0);
			ms_filter_link (stream->pixconv, 0, stream->sizeconv, 0);
			ms_filter_link (stream->sizeconv, 0, stream->tee, 0);
		}

		ms_ticker_attach(stream->ms.ticker,stream->source);
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
	if (stream->ms.ticker){
		if (stream->prepare_ongoing == TRUE) {
			stop_preload_graph(stream);
		} else {
			if (stream->source)
				ms_ticker_detach(stream->ms.ticker,stream->source);
			if (stream->ms.rtprecv)
				ms_ticker_detach(stream->ms.ticker,stream->ms.rtprecv);

			if (stream->ms.ice_check_list != NULL) {
				ice_check_list_print_route(stream->ms.ice_check_list, "Video session's route");
				stream->ms.ice_check_list = NULL;
			}
			rtp_stats_display(rtp_session_get_stats(stream->ms.session),
				"             VIDEO SESSION'S RTP STATISTICS                ");

			if (stream->source){
				MSConnectionHelper ch;
				ms_connection_helper_start(&ch);
				ms_connection_helper_unlink(&ch, stream->source, -1, 0);
				if (stream->pixconv) {
					ms_connection_helper_unlink(&ch, stream->pixconv, 0, 0);
				}
				if (stream->sizeconv) {
					ms_connection_helper_unlink(&ch, stream->sizeconv, 0, 0);
				}
				ms_connection_helper_unlink(&ch, stream->tee, 0, 0);
				if (stream->source_performs_encoding == FALSE) {
					ms_connection_helper_unlink(&ch, stream->ms.encoder, 0, 0);
				}
				ms_connection_helper_unlink(&ch, stream->ms.rtpsend, 0, -1);
				if (stream->output2){
					ms_filter_unlink(stream->tee,1,stream->output2,0);
				}
			}
			if (stream->ms.voidsink) {
				ms_filter_unlink(stream->ms.rtprecv, 0, stream->ms.voidsink, 0);
			} else if (stream->ms.rtprecv){
				MSConnectionHelper h;
				ms_connection_helper_start (&h);
				ms_connection_helper_unlink (&h,stream->ms.rtprecv,-1,0);
				if (stream->output_performs_decoding == FALSE) {
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
	video_stream_free (stream);
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
	stream->sent_vsize.width=MS_VIDEO_SIZE_CIF_W;
	stream->sent_vsize.height=MS_VIDEO_SIZE_CIF_H;
	choose_display_name(stream);
	return stream;
}


void video_preview_start(VideoPreview *stream, MSWebCam *device){
	MSPixFmt format;
	float fps=(float)29.97;
	int mirroring=1;
	int corner=-1;
	MSVideoSize disp_size=stream->sent_vsize;
	MSVideoSize vsize=disp_size;
	const char *displaytype=stream->display_name;

	stream->source = ms_web_cam_create_reader(device);


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
	stream->ms.ticker = ms_ticker_new();
	ms_ticker_set_name(stream->ms.ticker,"Video MSTicker");
	ms_ticker_attach (stream->ms.ticker, stream->source);
}

void video_preview_stop(VideoStream *stream){
	ms_ticker_detach(stream->ms.ticker, stream->source);
	ms_filter_unlink(stream->source,0,stream->pixconv,0);
	ms_filter_unlink(stream->pixconv,0,stream->output2,0);
	video_stream_free(stream);
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
	if (astream->ms.zrtp_context != NULL) {
		vstream->ms.zrtp_context=ortp_zrtp_multistream_new(astream->ms.zrtp_context, vstream->ms.session, param);
	}
}

void video_stream_enable_display_filter_auto_rotate(VideoStream* stream, bool_t enable) {
    stream->display_filter_auto_rotate_enabled = enable;
}

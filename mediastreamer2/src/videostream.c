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
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msrtp.h"
#include "mediastreamer2/msvideoout.h"
#include "mediastreamer2/msextdisplay.h"

#include <ortp/zrtp.h>



extern RtpSession * create_duplex_rtpsession( int locport, bool_t ipv6);

#define MAX_RTP_SIZE	UDP_MAX_SIZE

/* this code is not part of the library itself, it is part of the mediastream program */
void video_stream_free (VideoStream * stream)
{
	if (stream->ortpZrtpContext)
		ortp_zrtp_context_destroy(stream->ortpZrtpContext);
	if (stream->session!=NULL){
		rtp_session_unregister_event_queue(stream->session,stream->evq);
		rtp_session_destroy(stream->session);
	}
	if (stream->rtprecv != NULL)
		ms_filter_destroy (stream->rtprecv);
	if (stream->rtpsend!=NULL)
		ms_filter_destroy (stream->rtpsend);
	if (stream->source != NULL)
		ms_filter_destroy (stream->source);
	if (stream->output != NULL)
		ms_filter_destroy (stream->output);
	if (stream->encoder != NULL)
		ms_filter_destroy (stream->encoder);
	if (stream->decoder != NULL)
		ms_filter_destroy (stream->decoder);
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
	if (stream->ticker != NULL)
		ms_ticker_destroy (stream->ticker);
	if (stream->evq!=NULL)
		ortp_ev_queue_destroy(stream->evq);
	if (stream->display_name!=NULL)
		ms_free(stream->display_name);
	if (stream->rc!=NULL){
		ms_bitrate_controller_destroy(stream->rc);
	}

	ms_free (stream);
}

static void event_cb(void *ud, MSFilter* f, unsigned int event, void *eventdata){
	VideoStream *st=(VideoStream*)ud;
	ms_message("event_cb called %u", event);
	if (st->eventcb!=NULL){
		st->eventcb(st->event_pointer,f,event,eventdata);
	}
}

/*this function must be called from the MSTicker thread:
it replaces one filter by another one.
This is a dirty hack that works anyway.
It would be interesting to have something that does the job
simplier within the MSTicker api
*/
void video_stream_change_decoder(VideoStream *stream, int payload){
	RtpSession *session=stream->session;
	RtpProfile *prof=rtp_session_get_profile(session);
	PayloadType *pt=rtp_profile_get_payload(prof,payload);
	if (pt!=NULL){
		MSFilter *dec;

		if (stream->decoder!=NULL && stream->decoder->desc->enc_fmt!=NULL &&
		    strcasecmp(pt->mime_type,stream->decoder->desc->enc_fmt)==0){
			/* same formats behind different numbers, nothing to do */
				return;
		}
		dec=ms_filter_create_decoder(pt->mime_type);
		if (dec!=NULL){
			ms_filter_unlink(stream->rtprecv, 0, stream->decoder, 0);
			if (stream->tee2)
				ms_filter_unlink(stream->decoder,0,stream->tee2,0);
			else
				ms_filter_unlink(stream->decoder,0,stream->output,0);
			ms_filter_postprocess(stream->decoder);
			ms_filter_destroy(stream->decoder);
			stream->decoder=dec;
			if (pt->recv_fmtp!=NULL)
				ms_filter_call_method(stream->decoder,MS_FILTER_ADD_FMTP,(void*)pt->recv_fmtp);
			ms_filter_link (stream->rtprecv, 0, stream->decoder, 0);
			if (stream->tee2)
				ms_filter_link(stream->decoder,0,stream->tee2,0);
			else
				ms_filter_link (stream->decoder,0 , stream->output, 0);
			ms_filter_preprocess(stream->decoder,stream->ticker);
			ms_filter_set_notify_callback(dec, event_cb, stream);
		}else{
			ms_warning("No decoder found for %s",pt->mime_type);
		}
	}else{
		ms_warning("No payload defined with number %i",payload);
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
				float rt=rtp_session_get_round_trip_propagation(stream->session);
				float flost;
				ij=report_block_get_interarrival_jitter(rb);
				flost=(float)(100.0*report_block_get_fraction_lost(rb)/256.0);
				ms_message("video_steam_process_rtcp: interarrival jitter=%u , lost packets percentage since last report=%f, round trip time=%f seconds",ij,flost,rt);
				if (stream->rc)
					ms_bitrate_controller_process_rtcp(stream->rc,m);
			}
		}
	}while(rtcp_next_packet(m));
}

void video_stream_iterate(VideoStream *stream){
	/*
	if (stream->output!=NULL)
		ms_filter_call_method_noarg(stream->output,
			MS_VIDEO_OUT_HANDLE_RESIZING);
	*/
	if (stream->evq){
		OrtpEvent *ev;
		while (NULL != (ev=ortp_ev_queue_get(stream->evq))) {
			OrtpEventType evt=ortp_event_get_type(ev);
			if (evt == ORTP_EVENT_RTCP_PACKET_RECEIVED){
				OrtpEventData *evd=ortp_event_get_data(ev);
				video_steam_process_rtcp(stream,evd->packet);
			}
			ortp_event_destroy(ev);
		}
	}
}

static void payload_type_changed(RtpSession *session, unsigned long data){
	VideoStream *stream=(VideoStream*)data;
	int pt=rtp_session_get_recv_payload_type(stream->session);
	video_stream_change_decoder(stream,pt);
}

static void choose_display_name(VideoStream *stream){
#ifdef WIN32
	stream->display_name=ms_strdup("MSDrawDibDisplay");
#elif defined(ANDROID)
	stream->display_name=ms_strdup("MSAndroidDisplay");
#elif __APPLE__ && !defined(__ios)
	stream->display_name=ms_strdup("MSOSXGLDisplay");
#elif defined (HAVE_X11_EXTENSIONS_XV_H)
	stream->display_name=ms_strdup("MSX11Video");
#elif defined(__ios)
	stream->display_name=ms_strdup("IOSDisplay");	
#else
	stream->display_name=ms_strdup("MSVideoOut");
#endif
}

VideoStream *video_stream_new(int locport, bool_t use_ipv6){
	VideoStream *stream = (VideoStream *)ms_new0 (VideoStream, 1);
	stream->session=create_duplex_rtpsession(locport,use_ipv6);
	stream->evq=ortp_ev_queue_new();
	stream->rtpsend=ms_filter_new(MS_RTP_SEND_ID);
	rtp_session_register_event_queue(stream->session,stream->evq);
	stream->sent_vsize.width=MS_VIDEO_SIZE_CIF_W;
	stream->sent_vsize.height=MS_VIDEO_SIZE_CIF_H;
	stream->dir=VideoStreamSendRecv;
	choose_display_name(stream);

	return stream;
}

void video_stream_set_sent_video_size(VideoStream *stream, MSVideoSize vsize){
	ms_message("Setting video size %dx%d", vsize.width, vsize.height);
	stream->sent_vsize=vsize;
}

void video_stream_set_relay_session_id(VideoStream *stream, const char *id){
	ms_filter_call_method(stream->rtpsend, MS_RTP_SEND_SET_RELAY_SESSION_ID,(void*)id);
}

void video_stream_enable_self_view(VideoStream *stream, bool_t val){
	MSFilter *out=stream->output;
	stream->corner=val ? 0 : -1;
	if (out){
		ms_filter_call_method(out,MS_VIDEO_DISPLAY_SET_LOCAL_VIEW_MODE,&stream->corner);
	}
}

void video_stream_enable_adaptive_bitrate_control(VideoStream *s, bool_t yesno){
	s->use_rc=yesno;
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

	/* transmit orientation to source filter */
	ms_filter_call_method(stream->source,MS_VIDEO_CAPTURE_SET_DEVICE_ORIENTATION,&stream->device_orientation);
	/* transmit its preview window id if any to source filter*/
	if (stream->preview_window_id!=0){
		video_stream_set_native_preview_window_id(stream, stream->preview_window_id);
	}

	ms_filter_call_method(stream->encoder,MS_FILTER_GET_VIDEO_SIZE,&vsize);
	vsize=get_compatible_size(vsize,stream->sent_vsize);
	ms_filter_call_method(stream->source,MS_FILTER_SET_VIDEO_SIZE,&vsize);
	/*the camera may not support the target size and suggest a one close to the target */
	ms_filter_call_method(stream->source,MS_FILTER_GET_VIDEO_SIZE,&cam_vsize);
	if (cam_vsize.width*cam_vsize.height<=vsize.width*vsize.height &&
			cam_vsize.width != vsize.width){
		vsize=cam_vsize;
		ms_message("Output video size adjusted to match camera resolution (%ix%i)\n",vsize.width,vsize.height);
	} else if (cam_vsize.width*cam_vsize.height>vsize.width*vsize.height){
#ifdef __arm__
		ms_error("Camera is proposing a size bigger than encoder's suggested size (%ix%i > %ix%i) "
		           "Using the camera size as fallback because cropping or resizing is not implemented for arm.",
		           cam_vsize.width,cam_vsize.height,vsize.width,vsize.height);
		vsize=cam_vsize;
#else
		vsize=get_with_same_orientation(vsize,cam_vsize);
		ms_warning("Camera video size greater than encoder one. A scaling filter will be used!\n");
#endif
	}
	ms_filter_call_method(stream->encoder,MS_FILTER_SET_VIDEO_SIZE,&vsize);
	ms_filter_call_method(stream->encoder,MS_FILTER_GET_FPS,&fps);
	ms_message("Setting sent vsize=%ix%i, fps=%f",vsize.width,vsize.height,fps);
	/* configure the filters */
	if (ms_filter_get_id(stream->source)!=MS_STATIC_IMAGE_ID) {
		ms_filter_call_method(stream->source,MS_FILTER_SET_FPS,&fps);
	}
	/* get the output format for webcam reader */
	ms_filter_call_method(stream->source,MS_FILTER_GET_PIX_FMT,&format);

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
	if (stream->rc){
		ms_bitrate_controller_destroy(stream->rc);
		stream->rc=NULL;
	}
	if (stream->use_rc){
		stream->rc=ms_av_bitrate_controller_new(NULL,NULL,stream->session,stream->encoder);
	}
}

int video_stream_start (VideoStream *stream, RtpProfile *profile, const char *remip, int remport,
	int rem_rtcp_port, int payload, int jitt_comp, MSWebCam *cam){
	PayloadType *pt;
	RtpSession *rtps=stream->session;
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

	rtp_session_set_profile(rtps,profile);
	if (remport>0) rtp_session_set_remote_addr_full(rtps,remip,remport,rem_rtcp_port);
	rtp_session_set_payload_type(rtps,payload);
	rtp_session_set_jitter_compensation(rtps,jitt_comp);

	rtp_session_signal_connect(stream->session,"payload_type_changed",
			(RtpCallback)payload_type_changed,(unsigned long)stream);

	rtp_session_set_recv_buf_size(stream->session,MAX_RTP_SIZE);

	rtp_session_get_jitter_buffer_params(stream->session,&jbp);
	jbp.max_packets=1000;//needed for high resolution video
	rtp_session_set_jitter_buffer_params(stream->session,&jbp);
	rtp_session_set_rtp_socket_recv_buffer_size(stream->session,socket_buf_size);
	rtp_session_set_rtp_socket_send_buffer_size(stream->session,socket_buf_size);

	if (stream->dir==VideoStreamSendRecv || stream->dir==VideoStreamSendOnly){
		/*plumb the outgoing stream */

		if (remport>0) ms_filter_call_method(stream->rtpsend,MS_RTP_SEND_SET_SESSION,stream->session);
		stream->encoder=ms_filter_create_encoder(pt->mime_type);
		if ((stream->encoder==NULL) ){
			/* big problem: we don't have a registered codec for this payload...*/
			ms_error("videostream.c: No encoder available for payload %i:%s.",payload,pt->mime_type);
			return -1;
		}
		/* creates the filters */
		stream->cam=cam;
		stream->source = ms_web_cam_create_reader(cam);
		stream->tee = ms_filter_new(MS_TEE_ID);

		if (pt->normal_bitrate>0){
			ms_message("Limiting bitrate of video encoder to %i bits/s",pt->normal_bitrate);
			ms_filter_call_method(stream->encoder,MS_FILTER_SET_BITRATE,&pt->normal_bitrate);
		}
		if (pt->send_fmtp){
			ms_filter_call_method(stream->encoder,MS_FILTER_ADD_FMTP,pt->send_fmtp);
		}
		if (stream->use_preview_window){
			if (stream->rendercb==NULL){
				stream->output2=ms_filter_new_from_name (stream->display_name);
			}
		}

		configure_video_source (stream);
			/* and then connect all */
		ms_filter_link (stream->source, 0, stream->pixconv, 0);
		ms_filter_link (stream->pixconv, 0, stream->sizeconv, 0);
		ms_filter_link (stream->sizeconv, 0, stream->tee, 0);
		ms_filter_link (stream->tee, 0 ,stream->encoder, 0 );
		ms_filter_link (stream->encoder,0, stream->rtpsend,0);
		if (stream->output2){
			if (stream->preview_window_id!=0){
				ms_filter_call_method(stream->output2, MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID,&stream->preview_window_id);
			}
			ms_filter_link(stream->tee,1,stream->output2,0);
		}
	}
	if (stream->dir==VideoStreamSendRecv || stream->dir==VideoStreamRecvOnly){
		MSConnectionHelper ch;
		/*plumb the incoming stream */
		stream->decoder=ms_filter_create_decoder(pt->mime_type);
		if ((stream->decoder==NULL) ){
			/* big problem: we don't have a registered decoderfor this payload...*/
			ms_error("videostream.c: No decoder available for payload %i:%s.",payload,pt->mime_type);
			return -1;
		}
		ms_filter_set_notify_callback(stream->decoder, event_cb, stream);

		stream->rtprecv = ms_filter_new (MS_RTP_RECV_ID);
		ms_filter_call_method(stream->rtprecv,MS_RTP_RECV_SET_SESSION,stream->session);


		stream->jpegwriter=ms_filter_new(MS_JPEG_WRITER_ID);
		if (stream->jpegwriter)
			stream->tee2=ms_filter_new(MS_TEE_ID);

		if (stream->rendercb!=NULL){
			stream->output=ms_filter_new(MS_EXT_DISPLAY_ID);
			ms_filter_set_notify_callback(stream->output,ext_display_cb,stream);
		}else{
			stream->output=ms_filter_new_from_name (stream->display_name);
		}
		/* set parameters to the decoder*/
		if (pt->send_fmtp){
			ms_filter_call_method(stream->decoder,MS_FILTER_ADD_FMTP,pt->send_fmtp);
		}
		if (pt->recv_fmtp!=NULL)
			ms_filter_call_method(stream->decoder,MS_FILTER_ADD_FMTP,(void*)pt->recv_fmtp);

		/*force the decoder to output YUV420P */
		format=MS_YUV420P;
		ms_filter_call_method(stream->decoder,MS_FILTER_SET_PIX_FMT,&format);

		/*configure the display window */
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

		/* and connect the filters */
		ms_connection_helper_start (&ch);
		ms_connection_helper_link (&ch,stream->rtprecv,-1,0);
		ms_connection_helper_link (&ch,stream->decoder,0,0);
		if (stream->tee2){
			ms_connection_helper_link (&ch,stream->tee2,0,0);
			ms_filter_link(stream->tee2,1,stream->jpegwriter,0);
		}
		ms_connection_helper_link (&ch,stream->output,0,-1);
		/* the video source must be send for preview , if it exists*/
		if (stream->tee!=NULL && stream->output2==NULL)
			ms_filter_link(stream->tee,1,stream->output,1);
	}

	/* create the ticker */
	stream->ticker = ms_ticker_new();
	ms_ticker_set_name(stream->ticker,"Video MSTicker");
#ifdef __ios
    ms_ticker_set_priority(stream->ticker,MS_TICKER_PRIO_HIGH);
#endif
	/* attach the graphs */
	if (stream->source)
		ms_ticker_attach (stream->ticker, stream->source);
	if (stream->rtprecv)
		ms_ticker_attach (stream->ticker, stream->rtprecv);
	return 0;
}

void video_stream_update_video_params(VideoStream *stream){
	/*calling video_stream_change_camera() does the job of unplumbing/replumbing and configuring the new graph*/
	video_stream_change_camera(stream,stream->cam);
}

void video_stream_change_camera(VideoStream *stream, MSWebCam *cam){
	bool_t keep_source=(cam==stream->cam);
	
	if (stream->ticker && stream->source){
		ms_ticker_detach(stream->ticker,stream->source);
		/*unlink source filters and subsequent post processin filters */
		ms_filter_unlink (stream->source, 0, stream->pixconv, 0);
		ms_filter_unlink (stream->pixconv, 0, stream->sizeconv, 0);
		ms_filter_unlink (stream->sizeconv, 0, stream->tee, 0);
		/*destroy the filters */
		if (!keep_source) ms_filter_destroy(stream->source);
		ms_filter_destroy(stream->pixconv);
		ms_filter_destroy(stream->sizeconv);

		/*re create new ones and configure them*/
		if (!keep_source) stream->source = ms_web_cam_create_reader(cam);
		stream->cam=cam;
		configure_video_source(stream);
		
		ms_filter_link (stream->source, 0, stream->pixconv, 0);
		ms_filter_link (stream->pixconv, 0, stream->sizeconv, 0);
		ms_filter_link (stream->sizeconv, 0, stream->tee, 0);

		ms_ticker_attach(stream->ticker,stream->source);
	}
}

void video_stream_send_vfu(VideoStream *stream){
	if (stream->encoder)
		ms_filter_call_method_noarg(stream->encoder,MS_FILTER_REQ_VFU);
}

void
video_stream_stop (VideoStream * stream)
{
	stream->eventcb = NULL;
	stream->event_pointer = NULL;
	if (stream->ticker){
		if (stream->source)
			ms_ticker_detach(stream->ticker,stream->source);
		if (stream->rtprecv)
			ms_ticker_detach(stream->ticker,stream->rtprecv);

		rtp_stats_display(rtp_session_get_stats(stream->session),"Video session's RTP statistics");

		if (stream->source){
			ms_filter_unlink(stream->source,0,stream->pixconv,0);
			ms_filter_unlink (stream->pixconv, 0, stream->sizeconv, 0);
			ms_filter_unlink (stream->sizeconv, 0, stream->tee, 0);
			ms_filter_unlink(stream->tee,0,stream->encoder,0);
			ms_filter_unlink(stream->encoder, 0, stream->rtpsend,0);
			if (stream->output2){
				ms_filter_unlink(stream->tee,1,stream->output2,0);
			}
		}
		if (stream->rtprecv){
			MSConnectionHelper h;
			ms_connection_helper_start (&h);
			ms_connection_helper_unlink (&h,stream->rtprecv,-1,0);
			ms_connection_helper_unlink (&h,stream->decoder,0,0);
			if (stream->tee2){
				ms_connection_helper_unlink (&h,stream->tee2,0,0);
				ms_filter_unlink(stream->tee2,1,stream->jpegwriter,0);
			}
			ms_connection_helper_unlink (&h,stream->output,0,-1);
			if (stream->tee && stream->output2==NULL)
				ms_filter_unlink(stream->tee,1,stream->output,1);
		}
	}
	video_stream_free (stream);
}


void video_stream_set_rtcp_information(VideoStream *st, const char *cname, const char *tool){
	if (st->session!=NULL){
		rtp_session_set_source_description(st->session,cname,NULL,NULL,NULL,NULL,tool,
											"This is free software (GPL) !");
	}
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
	if (stream->output2){
		ms_filter_call_method(stream->output2,MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID,&id);
	}
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
	MSFilter* target_filter;
	
	if (stream == 0)
		return;

	stream->device_orientation = orientation;
	target_filter=stream->source;
	if (target_filter){
		ms_filter_call_method(target_filter,MS_VIDEO_CAPTURE_SET_DEVICE_ORIENTATION,&orientation);
	}
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
	stream->ticker = ms_ticker_new();
	ms_ticker_set_name(stream->ticker,"Video MSTicker");
	ms_ticker_attach (stream->ticker, stream->source);
}

void video_preview_stop(VideoStream *stream){
	ms_ticker_detach(stream->ticker, stream->source);
	ms_filter_unlink(stream->source,0,stream->pixconv,0);
	ms_filter_unlink(stream->pixconv,0,stream->output2,0);
	video_stream_free(stream);
}

int video_stream_recv_only_start(VideoStream *videostream, RtpProfile *profile, const char *addr, int port, int used_pt, int jitt_comp){
	video_stream_set_direction(videostream,VideoStreamRecvOnly);
	return video_stream_start(videostream,profile,addr,port,port+1,used_pt,jitt_comp,NULL);
}

int video_stream_send_only_start(VideoStream *videostream,
				RtpProfile *profile, const char *addr, int port, int rtcp_port,
				int used_pt, int  jitt_comp, MSWebCam *device){
	video_stream_set_direction (videostream,VideoStreamSendOnly);
	return video_stream_start(videostream,profile,addr,port,rtcp_port,used_pt,jitt_comp,device);
}


void video_stream_recv_only_stop(VideoStream *vs){
	video_stream_stop(vs);
}

void video_stream_send_only_stop(VideoStream *vs){
	video_stream_stop(vs);
}

/* enable ZRTP on the video stream using information from the audio stream */
void video_stream_enable_zrtp(VideoStream *vstream, AudioStream *astream, OrtpZrtpParams *param){
	if (astream->ortpZrtpContext != NULL) {
		vstream->ortpZrtpContext=ortp_zrtp_multistream_new(astream->ortpZrtpContext, vstream->session, param);
	}
}

bool_t video_stream_enable_strp(VideoStream* stream, enum ortp_srtp_crypto_suite_t suite, const char* snd_key, const char* rcv_key) {
	// assign new srtp transport to stream->session
	// with 2 Master Keys
	RtpTransport *rtp_tpt, *rtcp_tpt;	
	
	if (!ortp_srtp_supported()) {
		ms_error("ortp srtp support not enabled");
		return FALSE;
	}
	
	ms_message("%s: stream=%p key='%s' key='%s'", __FUNCTION__,
		stream, snd_key, rcv_key);
	 
	stream->srtp_session = ortp_srtp_create_configure_session(suite, 
		rtp_session_get_send_ssrc(stream->session), 
		snd_key,
		rcv_key); 
	
	if (!stream->srtp_session) {
		return FALSE;
	}
	
	// TODO: check who will free rtp_tpt ?
	srtp_transport_new(stream->srtp_session, &rtp_tpt, &rtcp_tpt);
	
	rtp_session_set_transports(stream->session, rtp_tpt, rtcp_tpt);
	
	return TRUE;
}

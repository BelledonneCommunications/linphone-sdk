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

#include "mediastreamer2/dtmfgen.h"
#include "mediastreamer2/mssndcard.h"
#include "mediastreamer2/msrtp.h"
#include "mediastreamer2/msfileplayer.h"
#include "mediastreamer2/msfilerec.h"
#include "mediastreamer2/msvolume.h"
#include "mediastreamer2/msequalizer.h"
#include "mediastreamer2/mstee.h"
#include "mediastreamer2/msaudiomixer.h"
#include "mediastreamer2/mscodecutils.h"
#include "private.h"

#ifdef INET6
	#include <sys/types.h>
#ifndef WIN32
	#include <sys/socket.h>
	#include <netdb.h>
#endif
#endif

static void audio_stream_free(AudioStream *stream) {
	media_stream_free(&stream->ms);
	if (stream->soundread!=NULL) ms_filter_destroy(stream->soundread);
	if (stream->soundwrite!=NULL) ms_filter_destroy(stream->soundwrite);
	if (stream->dtmfgen!=NULL) ms_filter_destroy(stream->dtmfgen);
	if (stream->plc!=NULL)	ms_filter_destroy(stream->plc);
	if (stream->ec!=NULL)	ms_filter_destroy(stream->ec);
	if (stream->volrecv!=NULL) ms_filter_destroy(stream->volrecv);
	if (stream->volsend!=NULL) ms_filter_destroy(stream->volsend);
	if (stream->equalizer!=NULL) ms_filter_destroy(stream->equalizer);
	if (stream->read_resampler!=NULL) ms_filter_destroy(stream->read_resampler);
	if (stream->write_resampler!=NULL) ms_filter_destroy(stream->write_resampler);
	if (stream->dtmfgen_rtp!=NULL) ms_filter_destroy(stream->dtmfgen_rtp);
	if (stream->dummy) ms_filter_destroy(stream->dummy);
	if (stream->recv_tee) ms_filter_destroy(stream->recv_tee);
	if (stream->send_tee) ms_filter_destroy(stream->send_tee);
	if (stream->recorder) ms_filter_destroy(stream->recorder);
	if (stream->recorder_mixer) ms_filter_destroy(stream->recorder_mixer);
	if (stream->recorder_file) ms_free(stream->recorder_file);
	ms_free(stream);
}

static int dtmf_tab[16]={'0','1','2','3','4','5','6','7','8','9','*','#','A','B','C','D'};

static void on_dtmf_received(RtpSession *s, int dtmf, void * user_data)
{
	AudioStream *stream=(AudioStream*)user_data;
	if (dtmf>15){
		ms_warning("Unsupported telephone-event type.");
		return;
	}
	ms_message("Receiving dtmf %c.",dtmf_tab[dtmf]);
	if (stream->dtmfgen!=NULL && stream->play_dtmfs){
		ms_filter_call_method(stream->dtmfgen,MS_DTMF_GEN_PUT,&dtmf_tab[dtmf]);
	}
}

static void audio_stream_configure_resampler(MSFilter *resampler,MSFilter *from,MSFilter *to) {
	int from_rate=0, to_rate=0;
	int from_channels = 0, to_channels = 0;
	ms_filter_call_method(from,MS_FILTER_GET_SAMPLE_RATE,&from_rate);
	ms_filter_call_method(to,MS_FILTER_GET_SAMPLE_RATE,&to_rate);
	ms_filter_call_method(resampler,MS_FILTER_SET_SAMPLE_RATE,&from_rate);
	ms_filter_call_method(resampler,MS_FILTER_SET_OUTPUT_SAMPLE_RATE,&to_rate);
	ms_filter_call_method(from, MS_FILTER_GET_NCHANNELS, &from_channels);
	ms_filter_call_method(to, MS_FILTER_GET_NCHANNELS, &to_channels);
	if (from_channels == 0) {
		from_channels = 1;
		ms_error("Filter %s does not implement the MS_FILTER_GET_NCHANNELS method", from->desc->name);
	}
	if (to_channels == 0) {
		to_channels = 1;
		ms_error("Filter %s does not implement the MS_FILTER_GET_NCHANNELS method", to->desc->name);
	}
	ms_filter_call_method(resampler, MS_FILTER_SET_NCHANNELS, &from_channels);
	ms_filter_call_method(resampler, MS_FILTER_SET_OUTPUT_NCHANNELS, &to_channels);
	ms_message("configuring %s-->%s from rate [%i] to rate [%i] and from channel [%i] to channel [%i]",
	           from->desc->name, to->desc->name, from_rate, to_rate, from_channels, to_channels);
}

static void audio_stream_process_rtcp(AudioStream *stream, mblk_t *m){
	do{
		const report_block_t *rb=NULL;
		if (rtcp_is_SR(m)){
			rb=rtcp_SR_get_report_block(m,0);
		}else if (rtcp_is_RR(m)){
			rb=rtcp_RR_get_report_block(m,0);
		}
		if (rb){
			unsigned int ij;
			float rt=rtp_session_get_round_trip_propagation(stream->ms.session);
			float flost;
			ij=report_block_get_interarrival_jitter(rb);
			flost=(float)(100.0*report_block_get_fraction_lost(rb)/256.0);
			ms_message("audio_stream_iterate(): remote statistics available\n\tremote's interarrival jitter=%u\n"
			           "\tremote's lost packets percentage since last report=%f\n\tround trip time=%f seconds",ij,flost,rt);
			if (stream->ms.rc) ms_bitrate_controller_process_rtcp(stream->ms.rc,m);
			if (stream->ms.qi) ms_quality_indicator_update_from_feedback(stream->ms.qi,m);
		}
	}while(rtcp_next_packet(m));
}

void audio_stream_iterate(AudioStream *stream){
	if (stream->ms.evq){
		OrtpEvent *ev=ortp_ev_queue_get(stream->ms.evq);
		if (ev!=NULL){
			OrtpEventType evt=ortp_event_get_type(ev);
			if (evt==ORTP_EVENT_RTCP_PACKET_RECEIVED){
				audio_stream_process_rtcp(stream,ortp_event_get_data(ev)->packet);
				stream->last_packet_time=ms_time(NULL);
			}else if (evt==ORTP_EVENT_RTCP_PACKET_EMITTED){
				ms_message("audio_stream_iterate(): local statistics available\n\tLocal's current jitter buffer size:%f ms",rtp_session_get_jitter_stats(stream->ms.session)->jitter_buffer_size_ms);
			}else if ((evt==ORTP_EVENT_STUN_PACKET_RECEIVED)&&(stream->ms.ice_check_list)){
				ice_handle_stun_packet(stream->ms.ice_check_list,stream->ms.session,ortp_event_get_data(ev));
			}
			ortp_event_destroy(ev);
		}
	}
	media_stream_iterate(&stream->ms);
}

bool_t audio_stream_alive(AudioStream * stream, int timeout){
	const rtp_stats_t *stats=rtp_session_get_stats(stream->ms.session);
	if (stats->recv!=0){
		if (stats->recv!=stream->last_packet_count){
			stream->last_packet_count=stats->recv;
			stream->last_packet_time=ms_time(NULL);
		}
	}
	if (stats->recv!=0){
		if (ms_time(NULL)-stream->last_packet_time>timeout){
			/* more than timeout seconds of inactivity*/
			return FALSE;
		}
	}
	return TRUE;
}

/*invoked from FEC capable filters*/
static  mblk_t* audio_stream_payload_picker(MSRtpPayloadPickerContext* context,unsigned int sequence_number) {
	return rtp_session_pick_with_cseq(((AudioStream*)(context->filter_graph_manager))->ms.session, sequence_number);
}

static void stop_preload_graph(AudioStream *stream){
	ms_ticker_detach(stream->ms.ticker,stream->dummy);
	if (stream->soundwrite) {
		ms_filter_unlink(stream->dummy,0,stream->soundwrite,0);
	}
	if (stream->ms.voidsink) {
		ms_filter_unlink(stream->dummy,0,stream->ms.voidsink,0);
		ms_filter_destroy(stream->ms.voidsink);
		stream->ms.voidsink=NULL;
	}
	ms_filter_destroy(stream->dummy);
	stream->dummy=NULL;
}

bool_t audio_stream_started(AudioStream *stream){
	return stream->ms.start_time!=0;
}

/* This function is used either on IOS to workaround the long time to initialize the Audio Unit or for ICE candidates gathering. */
void audio_stream_prepare_sound(AudioStream *stream, MSSndCard *playcard, MSSndCard *captcard){
	audio_stream_unprepare_sound(stream);
	stream->dummy=ms_filter_new(MS_RTP_RECV_ID);
	rtp_session_set_payload_type(stream->ms.session,0);
	ms_filter_call_method(stream->dummy,MS_RTP_RECV_SET_SESSION,stream->ms.session);

	if (captcard && playcard){
#ifdef __ios
		stream->soundread=ms_snd_card_create_reader(captcard);
		stream->soundwrite=ms_snd_card_create_writer(playcard);
		ms_filter_link(stream->dummy,0,stream->soundwrite,0);
#else
		stream->ms.voidsink=ms_filter_new(MS_VOID_SINK_ID);
		ms_filter_link(stream->dummy,0,stream->ms.voidsink,0);
#endif
	} else {
		stream->ms.voidsink=ms_filter_new(MS_VOID_SINK_ID);
		ms_filter_link(stream->dummy,0,stream->ms.voidsink,0);
	}
	if (stream->ms.ticker == NULL) start_ticker(&stream->ms);
	ms_ticker_attach(stream->ms.ticker,stream->dummy);
}

void audio_stream_unprepare_sound(AudioStream *stream){
	if (stream->dummy){
		stop_preload_graph(stream);
#ifdef __ios
		if (stream->soundread) ms_filter_destroy(stream->soundread);
		stream->soundread=NULL;
		if (stream->soundwrite) ms_filter_destroy(stream->soundwrite);
		stream->soundwrite=NULL;
#endif
	}
}

int audio_stream_start_full(AudioStream *stream, RtpProfile *profile, const char *rem_rtp_ip,int rem_rtp_port,
	const char *rem_rtcp_ip, int rem_rtcp_port, int payload,int jitt_comp, const char *infile, const char *outfile,
	MSSndCard *playcard, MSSndCard *captcard, bool_t use_ec)
{
	RtpSession *rtps=stream->ms.session;
	PayloadType *pt,*tel_ev;
	int tmp;
	MSConnectionHelper h;
	int sample_rate;
	MSRtpPayloadPickerContext picker_context;
	bool_t has_builtin_ec=FALSE;

	rtp_session_set_profile(rtps,profile);
	if (rem_rtp_port>0) rtp_session_set_remote_addr_full(rtps,rem_rtp_ip,rem_rtp_port,rem_rtcp_ip,rem_rtcp_port);
	if (rem_rtcp_port<=0){
		rtp_session_enable_rtcp(rtps,FALSE);
	}
	rtp_session_set_payload_type(rtps,payload);
	rtp_session_set_jitter_compensation(rtps,jitt_comp);

	if (rem_rtp_port>0)
		ms_filter_call_method(stream->ms.rtpsend,MS_RTP_SEND_SET_SESSION,rtps);
	stream->ms.rtprecv=ms_filter_new(MS_RTP_RECV_ID);
	ms_filter_call_method(stream->ms.rtprecv,MS_RTP_RECV_SET_SESSION,rtps);
	stream->ms.session=rtps;

	if((stream->features & AUDIO_STREAM_FEATURE_DTMF) != 0)
		stream->dtmfgen=ms_filter_new(MS_DTMF_GEN_ID);
	else
		stream->dtmfgen=NULL;
	rtp_session_signal_connect(rtps,"telephone-event",(RtpCallback)on_dtmf_received,(unsigned long)stream);
	rtp_session_signal_connect(rtps,"payload_type_changed",(RtpCallback)mediastream_payload_type_changed,(unsigned long)&stream->ms);
	/* creates the local part */
	if (captcard!=NULL){
		if (stream->soundread==NULL)
			stream->soundread=ms_snd_card_create_reader(captcard);
		has_builtin_ec=!!(ms_snd_card_get_capabilities(captcard) & MS_SND_CARD_CAP_BUILTIN_ECHO_CANCELLER);
	}else {
		stream->soundread=ms_filter_new(MS_FILE_PLAYER_ID);
		stream->read_resampler=ms_filter_new(MS_RESAMPLE_ID);
		if (infile!=NULL) audio_stream_play(stream,infile);
	}
	if (playcard!=NULL) {
		if (stream->soundwrite==NULL)
			stream->soundwrite=ms_snd_card_create_writer(playcard);
	}else {
		stream->soundwrite=ms_filter_new(MS_FILE_REC_ID);
		if (outfile!=NULL) audio_stream_record(stream,outfile);
	}

	/* creates the couple of encoder/decoder */
	pt=rtp_profile_get_payload(profile,payload);
	if (pt==NULL){
		ms_error("audiostream.c: undefined payload type.");
		return -1;
	}
	tel_ev=rtp_profile_get_payload_from_mime (profile,"telephone-event");

	if ((stream->features & AUDIO_STREAM_FEATURE_DTMF_ECHO) != 0 && (tel_ev==NULL || ( (tel_ev->flags & PAYLOAD_TYPE_FLAG_CAN_RECV) && !(tel_ev->flags & PAYLOAD_TYPE_FLAG_CAN_SEND)))
	    && ( strcasecmp(pt->mime_type,"pcmu")==0 || strcasecmp(pt->mime_type,"pcma")==0)){
		/*if no telephone-event payload is usable and pcma or pcmu is used, we will generate
		  inband dtmf*/
		stream->dtmfgen_rtp=ms_filter_new (MS_DTMF_GEN_ID);
	} else {
		stream->dtmfgen_rtp=NULL;
	}
	
	if (ms_filter_call_method(stream->ms.rtpsend,MS_FILTER_GET_SAMPLE_RATE,&sample_rate)!=0){
		ms_error("Sample rate is unknown for RTP side !");
		return -1;
	}
	
	stream->ms.encoder=ms_filter_create_encoder(pt->mime_type);
	stream->ms.decoder=ms_filter_create_decoder(pt->mime_type);
	if ((stream->ms.encoder==NULL) || (stream->ms.decoder==NULL)){
		/* big problem: we have not a registered codec for this payload...*/
		ms_error("audio_stream_start_full: No decoder or encoder available for payload %s.",pt->mime_type);
		return -1;
	}
	if (ms_filter_has_method(stream->ms.decoder, MS_FILTER_SET_RTP_PAYLOAD_PICKER)) {
		ms_message(" decoder has FEC capabilities");
		picker_context.filter_graph_manager=stream;
		picker_context.picker=&audio_stream_payload_picker;
		ms_filter_call_method(stream->ms.decoder,MS_FILTER_SET_RTP_PAYLOAD_PICKER, &picker_context);
	}
	if((stream->features & AUDIO_STREAM_FEATURE_VOL_SND) != 0)
		stream->volsend=ms_filter_new(MS_VOLUME_ID);
	else
		stream->volsend=NULL;
	if((stream->features & AUDIO_STREAM_FEATURE_VOL_RCV) != 0)
		stream->volrecv=ms_filter_new(MS_VOLUME_ID);
	else
		stream->volrecv=NULL;
	audio_stream_enable_echo_limiter(stream,stream->el_type);
	audio_stream_enable_noise_gate(stream,stream->use_ng);

	if (stream->use_agc){
		int tmp=1;
		if (stream->volsend==NULL)
			stream->volsend=ms_filter_new(MS_VOLUME_ID);
		ms_filter_call_method(stream->volsend,MS_VOLUME_ENABLE_AGC,&tmp);
	}

	if (stream->dtmfgen) {
		ms_filter_call_method(stream->dtmfgen,MS_FILTER_SET_SAMPLE_RATE,&sample_rate);
		ms_filter_call_method(stream->dtmfgen,MS_FILTER_SET_NCHANNELS,&pt->channels);
	}
	if (stream->dtmfgen_rtp) {
		ms_filter_call_method(stream->dtmfgen_rtp,MS_FILTER_SET_SAMPLE_RATE,&sample_rate);
		ms_filter_call_method(stream->dtmfgen_rtp,MS_FILTER_SET_NCHANNELS,&pt->channels);
	}
	/* give the sound filters some properties */
	if (ms_filter_call_method(stream->soundread,MS_FILTER_SET_SAMPLE_RATE,&sample_rate) != 0) {
		/* need to add resampler*/
		if (stream->read_resampler == NULL) stream->read_resampler=ms_filter_new(MS_RESAMPLE_ID);
	}
	ms_filter_call_method(stream->soundread,MS_FILTER_SET_NCHANNELS,&pt->channels);

	if (ms_filter_call_method(stream->soundwrite,MS_FILTER_SET_SAMPLE_RATE,&sample_rate) != 0) {
		/* need to add resampler*/
		if (stream->write_resampler == NULL) stream->write_resampler=ms_filter_new(MS_RESAMPLE_ID);
	}
	ms_filter_call_method(stream->soundwrite,MS_FILTER_SET_NCHANNELS,&pt->channels);

	// Override feature
	if ( ((stream->features & AUDIO_STREAM_FEATURE_EC) && !use_ec) || has_builtin_ec )
		stream->features &=~AUDIO_STREAM_FEATURE_EC;

	/*configure the echo canceller if required */
	if ((stream->features & AUDIO_STREAM_FEATURE_EC) == 0 && stream->ec != NULL) {
		ms_filter_destroy(stream->ec);
		stream->ec=NULL;
	}
	if (stream->ec){
		if (!stream->is_ec_delay_set){
			int delay_ms=ms_snd_card_get_minimal_latency(captcard);
			if (delay_ms!=0){
				ms_message("Setting echo canceller delay with value provided by soundcard: %i ms",delay_ms);
				ms_filter_call_method(stream->ec,MS_ECHO_CANCELLER_SET_DELAY,&delay_ms);
			}
		}
		ms_filter_call_method(stream->ec,MS_FILTER_SET_SAMPLE_RATE,&sample_rate);
	}
	
	if (stream->features & AUDIO_STREAM_FEATURE_MIXED_RECORDING){
		int val=0;
		int pin=1;
		stream->recorder=ms_filter_new(MS_FILE_REC_ID);
		stream->recorder_mixer=ms_filter_new(MS_AUDIO_MIXER_ID);
		stream->recv_tee=ms_filter_new(MS_TEE_ID);
		stream->send_tee=ms_filter_new(MS_TEE_ID);
		ms_filter_call_method(stream->recorder_mixer,MS_AUDIO_MIXER_ENABLE_CONFERENCE_MODE,&val);
		ms_filter_call_method(stream->recorder_mixer,MS_FILTER_SET_SAMPLE_RATE,&sample_rate);
		ms_filter_call_method(stream->recorder_mixer,MS_FILTER_SET_NCHANNELS,&pt->channels);
		ms_filter_call_method(stream->recv_tee,MS_TEE_MUTE,&pin);
		ms_filter_call_method(stream->send_tee,MS_TEE_MUTE,&pin);
		ms_filter_call_method(stream->recorder,MS_FILTER_SET_SAMPLE_RATE,&sample_rate);
		ms_filter_call_method(stream->recorder,MS_FILTER_SET_NCHANNELS,&pt->channels);
		
	}

	/* give the encoder/decoder some parameters*/
	ms_filter_call_method(stream->ms.encoder,MS_FILTER_SET_SAMPLE_RATE,&sample_rate);
	ms_message("Payload's bitrate is %i",pt->normal_bitrate);
	if (pt->normal_bitrate>0){
		ms_message("Setting audio encoder network bitrate to %i",pt->normal_bitrate);
		ms_filter_call_method(stream->ms.encoder,MS_FILTER_SET_BITRATE,&pt->normal_bitrate);
	}
	ms_filter_call_method(stream->ms.encoder,MS_FILTER_SET_NCHANNELS,&pt->channels);
	ms_filter_call_method(stream->ms.decoder,MS_FILTER_SET_SAMPLE_RATE,&sample_rate);
	ms_filter_call_method(stream->ms.decoder,MS_FILTER_SET_NCHANNELS,&pt->channels);

	if (pt->send_fmtp!=NULL) {
		char value[16]={0};
		int ptime;
		if (ms_filter_has_method(stream->ms.encoder,MS_AUDIO_ENCODER_SET_PTIME)){
			if (fmtp_get_value(pt->send_fmtp,"ptime",value,sizeof(value)-1)){
				ptime=atoi(value);
				ms_filter_call_method(stream->ms.encoder,MS_AUDIO_ENCODER_SET_PTIME,&ptime);
			}
		}
		ms_filter_call_method(stream->ms.encoder,MS_FILTER_ADD_FMTP, (void*)pt->send_fmtp);
	}
	if (pt->recv_fmtp!=NULL) ms_filter_call_method(stream->ms.decoder,MS_FILTER_ADD_FMTP,(void*)pt->recv_fmtp);

	/*create the equalizer*/
	if ((stream->features & AUDIO_STREAM_FEATURE_EQUALIZER) != 0){
		stream->equalizer=ms_filter_new(MS_EQUALIZER_ID);
		if(stream->equalizer) {
			tmp=stream->eq_active;
			ms_filter_call_method(stream->equalizer,MS_EQUALIZER_SET_ACTIVE,&tmp);
		}
	}else
		stream->equalizer=NULL;
	

	/*configure resampler if needed*/
	ms_filter_call_method(stream->ms.rtpsend, MS_FILTER_SET_NCHANNELS, &pt->channels);
	ms_filter_call_method(stream->ms.rtprecv, MS_FILTER_SET_NCHANNELS, &pt->channels);
	if (stream->read_resampler){
		audio_stream_configure_resampler(stream->read_resampler,stream->soundread,stream->ms.rtpsend);
	}

	if (stream->write_resampler){
		audio_stream_configure_resampler(stream->write_resampler,stream->ms.rtprecv,stream->soundwrite);
	}

	if (stream->ms.use_rc){
		stream->ms.rc=ms_audio_bitrate_controller_new(stream->ms.session,stream->ms.encoder,0);
	}
	
	/* Create PLC */
	if ((stream->features & AUDIO_STREAM_FEATURE_PLC) != 0) {
		int decoder_have_plc = 0;
		if (ms_filter_has_method(stream->ms.decoder, MS_AUDIO_DECODER_HAVE_PLC)) {
			if (ms_filter_call_method(stream->ms.decoder, MS_AUDIO_DECODER_HAVE_PLC, &decoder_have_plc) != 0) {
				ms_warning("MS_AUDIO_DECODER_HAVE_PLC function error: enable default plc");
			}
		} else {
			ms_warning("MS_DECODER_HAVE_PLC function not implemented by the decoder: enable default plc");
		}
		if (decoder_have_plc == 0) {
			stream->plc = ms_filter_new(MS_GENERIC_PLC_ID);
		}

		if (stream->plc) {
			ms_filter_call_method(stream->plc, MS_FILTER_SET_NCHANNELS, &pt->channels);
			ms_filter_call_method(stream->plc, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
		}
	} else {
		stream->plc = NULL;
	}

	/* create ticker */
	if (stream->ms.ticker==NULL) start_ticker(&stream->ms);
	else{
		/*we were using the dummy preload graph, destroy it*/
		if (stream->dummy) stop_preload_graph(stream);
	}
	
	/* and then connect all */
	/* tip: draw yourself the picture if you don't understand */

	/*sending graph*/
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h,stream->soundread,-1,0);
	if (stream->read_resampler)
		ms_connection_helper_link(&h,stream->read_resampler,0,0);
	if (stream->ec)
		ms_connection_helper_link(&h,stream->ec,1,1);
	if (stream->volsend)
		ms_connection_helper_link(&h,stream->volsend,0,0);
	if (stream->dtmfgen_rtp)
		ms_connection_helper_link(&h,stream->dtmfgen_rtp,0,0);
	if (stream->send_tee)
		ms_connection_helper_link(&h,stream->send_tee,0,0);
	ms_connection_helper_link(&h,stream->ms.encoder,0,0);
	ms_connection_helper_link(&h,stream->ms.rtpsend,0,-1);

	/*receiving graph*/
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h,stream->ms.rtprecv,-1,0);
	ms_connection_helper_link(&h,stream->ms.decoder,0,0);
	if (stream->plc)
		ms_connection_helper_link(&h,stream->plc,0,0);
	if (stream->dtmfgen)
		ms_connection_helper_link(&h,stream->dtmfgen,0,0);
	if (stream->volrecv)
		ms_connection_helper_link(&h,stream->volrecv,0,0);
	if (stream->recv_tee)
		ms_connection_helper_link(&h,stream->recv_tee,0,0);
	if (stream->equalizer)
		ms_connection_helper_link(&h,stream->equalizer,0,0);
	if (stream->ec)
		ms_connection_helper_link(&h,stream->ec,0,0);
	if (stream->write_resampler)
		ms_connection_helper_link(&h,stream->write_resampler,0,0);
	ms_connection_helper_link(&h,stream->soundwrite,0,-1);

	/*call recording part, attached to both outgoing and incoming graphs*/
	if (stream->recorder){
		ms_filter_link(stream->send_tee,1,stream->recorder_mixer,0);
		ms_filter_link(stream->recv_tee,1,stream->recorder_mixer,1);
		ms_filter_link(stream->recorder_mixer,0,stream->recorder,0);
	}
	
	/*to make sure all preprocess are done before befre processing audio*/
	ms_ticker_attach_multiple(stream->ms.ticker
				,stream->soundread
				,stream->ms.rtprecv
				,NULL);

	stream->ms.start_time=ms_time(NULL);
	stream->ms.is_beginning=TRUE;

	return 0;
}

int audio_stream_start_with_files(AudioStream *stream, RtpProfile *prof,const char *remip, int remport,
	int rem_rtcp_port, int pt,int jitt_comp, const char *infile, const char * outfile)
{
	return audio_stream_start_full(stream,prof,remip,remport,remip,rem_rtcp_port,pt,jitt_comp,infile,outfile,NULL,NULL,FALSE);
}

AudioStream * audio_stream_start(RtpProfile *prof,int locport,const char *remip,int remport,int profile,int jitt_comp,bool_t use_ec)
{
	MSSndCard *sndcard_playback;
	MSSndCard *sndcard_capture;
	AudioStream *stream;
	sndcard_capture=ms_snd_card_manager_get_default_capture_card(ms_snd_card_manager_get());
	sndcard_playback=ms_snd_card_manager_get_default_playback_card(ms_snd_card_manager_get());
	if (sndcard_capture==NULL || sndcard_playback==NULL)
		return NULL;
	stream=audio_stream_new(locport, locport+1, ms_is_ipv6(remip));
	if (audio_stream_start_full(stream,prof,remip,remport,remip,remport+1,profile,jitt_comp,NULL,NULL,sndcard_playback,sndcard_capture,use_ec)==0) return stream;
	audio_stream_free(stream);
	return NULL;
}

AudioStream *audio_stream_start_with_sndcards(RtpProfile *prof,int locport,const char *remip,int remport,int profile,int jitt_comp,MSSndCard *playcard, MSSndCard *captcard, bool_t use_ec)
{
	AudioStream *stream;
	if (playcard==NULL) {
		ms_error("No playback card.");
		return NULL;
	}
	if (captcard==NULL) {
		ms_error("No capture card.");
		return NULL;
	}
	stream=audio_stream_new(locport, locport+1, ms_is_ipv6(remip));
	if (audio_stream_start_full(stream,prof,remip,remport,remip,remport+1,profile,jitt_comp,NULL,NULL,playcard,captcard,use_ec)==0) return stream;
	audio_stream_free(stream);
	return NULL;
}

// Pass NULL to stop playing
void audio_stream_play(AudioStream *st, const char *name){
	if (st->soundread == NULL) {
		ms_warning("Cannot play file: the stream hasn't been started");
		return;
	}
	if (ms_filter_get_id(st->soundread)==MS_FILE_PLAYER_ID){
		ms_filter_call_method_noarg(st->soundread,MS_FILE_PLAYER_CLOSE);
		if (name != NULL) {
			ms_filter_call_method(st->soundread,MS_FILE_PLAYER_OPEN,(void*)name);
			if (st->read_resampler){
				audio_stream_configure_resampler(st->read_resampler,st->soundread,st->ms.rtpsend);
			}
			ms_filter_call_method_noarg(st->soundread,MS_FILE_PLAYER_START);
		}
	}else{
		ms_error("Cannot play file: the stream hasn't been started with"
		" audio_stream_start_with_files");
	}
}

void audio_stream_record(AudioStream *st, const char *name){
	if (ms_filter_get_id(st->soundwrite)==MS_FILE_REC_ID){
		ms_filter_call_method_noarg(st->soundwrite,MS_FILE_REC_CLOSE);
		ms_filter_call_method(st->soundwrite,MS_FILE_REC_OPEN,(void*)name);
		ms_filter_call_method_noarg(st->soundwrite,MS_FILE_REC_START);
	}else{
		ms_error("Cannot record file: the stream hasn't been started with"
		" audio_stream_start_with_files");
	}
}


int audio_stream_mixed_record_open(AudioStream *st, const char* filename){
	if (!(st->features & AUDIO_STREAM_FEATURE_MIXED_RECORDING)){
		if (audio_stream_started(st)){
			ms_error("Too late - you cannot request a mixed recording when the stream is running because it did not have AUDIO_STREAM_FEATURE_MIXED_RECORDING feature.");
			return -1;
		}else{
			st->features|=AUDIO_STREAM_FEATURE_MIXED_RECORDING;
		}
	}
	if (st->recorder_file){
		audio_stream_mixed_record_stop(st);
	}
	st->recorder_file=filename ? ms_strdup(filename) : NULL;
	return 0;
}

int audio_stream_mixed_record_start(AudioStream *st){
	if (st->recorder && st->recorder_file){
		int pin=1;
		MSRecorderState state;
		ms_filter_call_method(st->recorder,MS_RECORDER_GET_STATE,&state);
		if (state==MSRecorderClosed){
			if (ms_filter_call_method(st->recorder,MS_RECORDER_OPEN,st->recorder_file)==-1)
				return -1;
		}
		ms_filter_call_method_noarg(st->recorder,MS_RECORDER_START);
		ms_filter_call_method(st->recv_tee,MS_TEE_UNMUTE,&pin);
		ms_filter_call_method(st->send_tee,MS_TEE_UNMUTE,&pin);
		return 0;
	}
	return -1;
}

int audio_stream_mixed_record_stop(AudioStream *st){
	if (st->recorder && st->recorder_file){
		int pin=1;
		ms_filter_call_method_noarg(st->recorder,MS_RECORDER_PAUSE);
		ms_filter_call_method(st->recv_tee,MS_TEE_MUTE,&pin);
		ms_filter_call_method(st->send_tee,MS_TEE_MUTE,&pin);
		ms_filter_call_method_noarg(st->recorder,MS_RECORDER_CLOSE);
	}
	return 0;
}

uint32_t audio_stream_get_features(AudioStream *st){
	return st->features;
}

void audio_stream_set_features(AudioStream *st, uint32_t features){
	st->features = features;
}

AudioStream *audio_stream_new(int loc_rtp_port, int loc_rtcp_port, bool_t ipv6){
	AudioStream *stream=(AudioStream *)ms_new0(AudioStream,1);
	MSFilterDesc *ec_desc=ms_filter_lookup_by_name("MSOslec");
	
	ms_filter_enable_statistics(TRUE);
	ms_filter_reset_statistics();

	stream->ms.type = AudioStreamType;
	stream->ms.session=create_duplex_rtpsession(loc_rtp_port,loc_rtcp_port,ipv6);
	/*some filters are created right now to allow configuration by the application before start() */
	stream->ms.rtpsend=ms_filter_new(MS_RTP_SEND_ID);
	stream->ms.ice_check_list=NULL;
	stream->ms.qi=ms_quality_indicator_new(stream->ms.session);

	if (ec_desc!=NULL)
		stream->ec=ms_filter_new_from_desc(ec_desc);
	else
#if defined(BUILD_WEBRTC_AECM)
		stream->ec=ms_filter_new(MS_WEBRTC_AEC_ID);
#else
		stream->ec=ms_filter_new(MS_SPEEX_EC_ID);
#endif

	stream->ms.evq=ortp_ev_queue_new();
	rtp_session_register_event_queue(stream->ms.session,stream->ms.evq);
	stream->play_dtmfs=TRUE;
	stream->use_gc=FALSE;
	stream->use_agc=FALSE;
	stream->use_ng=FALSE;
	stream->features=AUDIO_STREAM_FEATURE_ALL;
	return stream;
}

void audio_stream_play_received_dtmfs(AudioStream *st, bool_t yesno){
	st->play_dtmfs=yesno;
}

int audio_stream_start_now(AudioStream *stream, RtpProfile * prof,  const char *remip, int remport, int rem_rtcp_port, int payload_type, int jitt_comp, MSSndCard *playcard, MSSndCard *captcard, bool_t use_ec){
	return audio_stream_start_full(stream,prof,remip,remport,remip,rem_rtcp_port,
		payload_type,jitt_comp,NULL,NULL,playcard,captcard,use_ec);
}

void audio_stream_set_relay_session_id(AudioStream *stream, const char *id){
	ms_filter_call_method(stream->ms.rtpsend, MS_RTP_SEND_SET_RELAY_SESSION_ID,(void*)id);
}

void audio_stream_set_echo_canceller_params(AudioStream *stream, int tail_len_ms, int delay_ms, int framesize){
	if (stream->ec){
		if (tail_len_ms>0)
			ms_filter_call_method(stream->ec,MS_ECHO_CANCELLER_SET_TAIL_LENGTH,&tail_len_ms);
		if (delay_ms>0){
			stream->is_ec_delay_set=TRUE;
			ms_filter_call_method(stream->ec,MS_ECHO_CANCELLER_SET_DELAY,&delay_ms);
		}
		if (framesize>0)
			ms_filter_call_method(stream->ec,MS_ECHO_CANCELLER_SET_FRAMESIZE,&framesize);
	}
}

void audio_stream_enable_echo_limiter(AudioStream *stream, EchoLimiterType type){
	stream->el_type=type;
	if (stream->volsend){
		bool_t enable_noise_gate = stream->el_type==ELControlFull;
		ms_filter_call_method(stream->volrecv,MS_VOLUME_ENABLE_NOISE_GATE,&enable_noise_gate);
		ms_filter_call_method(stream->volsend,MS_VOLUME_SET_PEER,type!=ELInactive?stream->volrecv:NULL);
	} else {
		ms_warning("cannot set echo limiter to mode [%i] because no volume send",type);
	}
}

void audio_stream_enable_gain_control(AudioStream *stream, bool_t val){
	stream->use_gc=val;
}

void audio_stream_enable_automatic_gain_control(AudioStream *stream, bool_t val){
	stream->use_agc=val;
}

void audio_stream_enable_noise_gate(AudioStream *stream, bool_t val){
	stream->use_ng=val;
	if (stream->volsend){
		ms_filter_call_method(stream->volsend,MS_VOLUME_ENABLE_NOISE_GATE,&val);
	} else {
		ms_warning("cannot set noise gate mode to [%i] because no volume send",val);
	}
}

void audio_stream_set_mic_gain(AudioStream *stream, float gain){
	if (stream->volsend){
		ms_filter_call_method(stream->volsend,MS_VOLUME_SET_GAIN,&gain);
	}else ms_warning("Could not apply gain: gain control wasn't activated. "
			"Use audio_stream_enable_gain_control() before starting the stream.");
}

void audio_stream_enable_equalizer(AudioStream *stream, bool_t enabled){
	stream->eq_active=enabled;
	if (stream->equalizer){
		int tmp=enabled;
		ms_filter_call_method(stream->equalizer,MS_EQUALIZER_SET_ACTIVE,&tmp);
	}
}

void audio_stream_equalizer_set_gain(AudioStream *stream, int frequency, float gain, int freq_width){
	if (stream->equalizer){
		MSEqualizerGain d;
		d.frequency=frequency;
		d.gain=gain;
		d.width=freq_width;
		ms_filter_call_method(stream->equalizer,MS_EQUALIZER_SET_GAIN,&d);
	}
}

void audio_stream_stop(AudioStream * stream)
{
	if (stream->ms.ticker){
		MSConnectionHelper h;
		
		if (stream->dummy){
			stop_preload_graph(stream);
		}else if (stream->ms.start_time!=0){
		
			ms_ticker_detach(stream->ms.ticker,stream->soundread);
			ms_ticker_detach(stream->ms.ticker,stream->ms.rtprecv);

			if (stream->ms.ice_check_list != NULL) {
				ice_check_list_print_route(stream->ms.ice_check_list, "Audio session's route");
				stream->ms.ice_check_list = NULL;
			}
			rtp_stats_display(rtp_session_get_stats(stream->ms.session),
				"             AUDIO SESSION'S RTP STATISTICS                ");

			/*dismantle the outgoing graph*/
			ms_connection_helper_start(&h);
			ms_connection_helper_unlink(&h,stream->soundread,-1,0);
			if (stream->read_resampler!=NULL)
				ms_connection_helper_unlink(&h,stream->read_resampler,0,0);
			if (stream->ec!=NULL)
				ms_connection_helper_unlink(&h,stream->ec,1,1);
			if (stream->volsend!=NULL)
				ms_connection_helper_unlink(&h,stream->volsend,0,0);
			if (stream->dtmfgen_rtp)
				ms_connection_helper_unlink(&h,stream->dtmfgen_rtp,0,0);
			if (stream->send_tee)
				ms_connection_helper_unlink(&h,stream->send_tee,0,0);
			ms_connection_helper_unlink(&h,stream->ms.encoder,0,0);
			ms_connection_helper_unlink(&h,stream->ms.rtpsend,0,-1);

			/*dismantle the receiving graph*/
			ms_connection_helper_start(&h);
			ms_connection_helper_unlink(&h,stream->ms.rtprecv,-1,0);
			ms_connection_helper_unlink(&h,stream->ms.decoder,0,0);
			if (stream->plc!=NULL)
				ms_connection_helper_unlink(&h,stream->plc,0,0);
			if (stream->dtmfgen!=NULL)
				ms_connection_helper_unlink(&h,stream->dtmfgen,0,0);
			if (stream->volrecv!=NULL)
				ms_connection_helper_unlink(&h,stream->volrecv,0,0);
			if (stream->recv_tee)
				ms_connection_helper_unlink(&h,stream->recv_tee,0,0);
			if (stream->equalizer!=NULL)
				ms_connection_helper_unlink(&h,stream->equalizer,0,0);
			if (stream->ec!=NULL)
				ms_connection_helper_unlink(&h,stream->ec,0,0);
			if (stream->write_resampler!=NULL)
				ms_connection_helper_unlink(&h,stream->write_resampler,0,0);
			ms_connection_helper_unlink(&h,stream->soundwrite,0,-1);
			
			/*dismantle the call recording */
			if (stream->recorder){
				ms_filter_unlink(stream->send_tee,1,stream->recorder_mixer,0);
				ms_filter_unlink(stream->recv_tee,1,stream->recorder_mixer,1);
				ms_filter_unlink(stream->recorder_mixer,0,stream->recorder,0);
			}
		}
	}
	audio_stream_free(stream);
	ms_filter_log_statistics();
}

int audio_stream_send_dtmf(AudioStream *stream, char dtmf)
{
	if (stream->dtmfgen_rtp)
		ms_filter_call_method(stream->dtmfgen_rtp,MS_DTMF_GEN_PLAY,&dtmf);
	else if (stream->ms.rtpsend)
		ms_filter_call_method(stream->ms.rtpsend,MS_RTP_SEND_SEND_DTMF,&dtmf);
	return 0;
}

void audio_stream_mute_rtp(AudioStream *stream, bool_t val) 
{
	if (stream->ms.rtpsend){
		if (val)
			ms_filter_call_method(stream->ms.rtpsend,MS_RTP_SEND_MUTE_MIC,&val);
		else
			ms_filter_call_method(stream->ms.rtpsend,MS_RTP_SEND_UNMUTE_MIC,&val);
	}
}

float audio_stream_get_quality_rating(AudioStream *stream){
	return media_stream_get_quality_rating(&stream->ms);
}

float audio_stream_get_average_quality_rating(AudioStream *stream){
	return media_stream_get_average_quality_rating(&stream->ms);
}

void audio_stream_enable_zrtp(AudioStream *stream, OrtpZrtpParams *params){
	stream->ms.zrtp_context=ortp_zrtp_context_new(stream->ms.session, params);
}

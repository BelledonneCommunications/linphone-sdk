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

#ifdef INET6
	#include <sys/types.h>
#ifndef WIN32
	#include <sys/socket.h>
	#include <netdb.h>
#endif
#endif


#define MAX_RTP_SIZE	1500


/* this code is not part of the library itself, it is part of the mediastream program */
void audio_stream_free(AudioStream *stream)
{
	if (stream->session!=NULL) {
		rtp_session_unregister_event_queue(stream->session,stream->evq);
		rtp_session_destroy(stream->session);
	}
	if (stream->evq) ortp_ev_queue_destroy(stream->evq);
	if (stream->rtpsend!=NULL) ms_filter_destroy(stream->rtpsend);
	if (stream->rtprecv!=NULL) ms_filter_destroy(stream->rtprecv);
	if (stream->soundread!=NULL) ms_filter_destroy(stream->soundread);
	if (stream->soundwrite!=NULL) ms_filter_destroy(stream->soundwrite);
	if (stream->encoder!=NULL) ms_filter_destroy(stream->encoder);
	if (stream->decoder!=NULL) ms_filter_destroy(stream->decoder);
	if (stream->dtmfgen!=NULL) ms_filter_destroy(stream->dtmfgen);
	if (stream->ec!=NULL)	ms_filter_destroy(stream->ec);
	if (stream->volrecv!=NULL) ms_filter_destroy(stream->volrecv);
	if (stream->volsend!=NULL) ms_filter_destroy(stream->volsend);
	if (stream->equalizer!=NULL) ms_filter_destroy(stream->equalizer);
	if (stream->ticker!=NULL) ms_ticker_destroy(stream->ticker);
	if (stream->read_resampler!=NULL) ms_filter_destroy(stream->read_resampler);
	if (stream->write_resampler!=NULL) ms_filter_destroy(stream->write_resampler);
	if (stream->dtmfgen_rtp!=NULL) ms_filter_destroy(stream->dtmfgen_rtp);
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

bool_t ms_is_ipv6(const char *remote){
	bool_t ret=FALSE;
#ifdef INET6
	struct addrinfo hints, *res0;

	int err;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	err = getaddrinfo(remote,"8000", &hints, &res0);
	if (err!=0) {
		ms_warning ("get_local_addr_for: %s", gai_strerror(err));
		return FALSE;
	}
	ret=(res0->ai_addr->sa_family==AF_INET6);
	freeaddrinfo(res0);
#endif
	return ret;
}

static void audio_stream_configure_resampler(MSFilter *resampler,MSFilter *from,MSFilter *to) {
	int from_rate=0, to_rate=0;
	ms_filter_call_method(from,MS_FILTER_GET_SAMPLE_RATE,&from_rate);
	ms_filter_call_method(to,MS_FILTER_GET_SAMPLE_RATE,&to_rate);
	ms_filter_call_method(resampler,MS_FILTER_SET_SAMPLE_RATE,&from_rate);
	ms_filter_call_method(resampler,MS_FILTER_SET_OUTPUT_SAMPLE_RATE,&to_rate);
	ms_message("configuring %s-->%s from rate[%i] to rate [%i]",
	           from->desc->name, to->desc->name, from_rate,to_rate);
}

RtpSession * create_duplex_rtpsession( int locport, bool_t ipv6){
	RtpSession *rtpr;
	rtpr=rtp_session_new(RTP_SESSION_SENDRECV);
	rtp_session_set_recv_buf_size(rtpr,MAX_RTP_SIZE);
	rtp_session_set_scheduling_mode(rtpr,0);
	rtp_session_set_blocking_mode(rtpr,0);
	rtp_session_enable_adaptive_jitter_compensation(rtpr,TRUE);
	rtp_session_set_symmetric_rtp(rtpr,TRUE);
	rtp_session_set_local_addr(rtpr,ipv6 ? "::" : "0.0.0.0",locport);
	rtp_session_signal_connect(rtpr,"timestamp_jump",(RtpCallback)rtp_session_resync,(long)NULL);
	rtp_session_signal_connect(rtpr,"ssrc_changed",(RtpCallback)rtp_session_resync,(long)NULL);
	rtp_session_set_ssrc_changed_threshold(rtpr,0);
	return rtpr;
}

#if defined(_WIN32_WCE)
time_t
ms_time (time_t *t)
{
    DWORD timemillis = GetTickCount();
	if (timemillis>0)
	{
		if (t!=NULL)
			*t = timemillis/1000;
	}
	return timemillis/1000;
}
#endif

bool_t audio_stream_alive(AudioStream * stream, int timeout){
	RtpSession *session=stream->session;
	const rtp_stats_t *stats=rtp_session_get_stats(session);
	if (stats->recv!=0){
		if (stream->evq){
			OrtpEvent *ev=ortp_ev_queue_get(stream->evq);
			if (ev!=NULL){
				if (ortp_event_get_type(ev)==ORTP_EVENT_RTCP_PACKET_RECEIVED){
					stream->last_packet_time=ms_time(NULL);
				}
				ortp_event_destroy(ev);
			}
		}
		if (stats->recv!=stream->last_packet_count){
			stream->last_packet_count=stats->recv;
			stream->last_packet_time=ms_time(NULL);
		}else{
			if (ms_time(NULL)-stream->last_packet_time>timeout){
				/* more than timeout seconds of inactivity*/
				return FALSE;
			}
		}
	}
	return TRUE;
}

/*this function must be called from the MSTicker thread:
it replaces one filter by another one.
This is a dirty hack that works anyway.
It would be interesting to have something that does the job
simplier within the MSTicker api
*/
void audio_stream_change_decoder(AudioStream *stream, int payload){
	RtpSession *session=stream->session;
	RtpProfile *prof=rtp_session_get_profile(session);
	PayloadType *pt=rtp_profile_get_payload(prof,payload);
	if (pt!=NULL){
		MSFilter *dec=ms_filter_create_decoder(pt->mime_type);
		if (dec!=NULL){
			ms_filter_unlink(stream->rtprecv, 0, stream->decoder, 0);
			ms_filter_unlink(stream->decoder,0,stream->dtmfgen,0);
			ms_filter_postprocess(stream->decoder);
			ms_filter_destroy(stream->decoder);
			stream->decoder=dec;
			if (pt->recv_fmtp!=NULL)
				ms_filter_call_method(stream->decoder,MS_FILTER_ADD_FMTP,(void*)pt->recv_fmtp);
			ms_filter_link (stream->rtprecv, 0, stream->decoder, 0);
			ms_filter_link (stream->decoder,0 , stream->dtmfgen, 0);
			ms_filter_preprocess(stream->decoder,stream->ticker);

		}else{
			ms_warning("No decoder found for %s",pt->mime_type);
		}
	}else{
		ms_warning("No payload defined with number %i",payload);
	}
}

static void payload_type_changed(RtpSession *session, unsigned long data){
	AudioStream *stream=(AudioStream*)data;
	int pt=rtp_session_get_recv_payload_type(stream->session);
	audio_stream_change_decoder(stream,pt);
}

int audio_stream_start_full(AudioStream *stream, RtpProfile *profile, const char *remip,int remport,
	int rem_rtcp_port, int payload,int jitt_comp, const char *infile, const char *outfile,
	MSSndCard *playcard, MSSndCard *captcard, bool_t use_ec)
{
	RtpSession *rtps=stream->session;
	PayloadType *pt;
	int tmp;
	MSConnectionHelper h;
	int sample_rate;

	rtp_session_set_profile(rtps,profile);
	if (remport>0) rtp_session_set_remote_addr_full(rtps,remip,remport,rem_rtcp_port);
	rtp_session_set_payload_type(rtps,payload);
	rtp_session_set_jitter_compensation(rtps,jitt_comp);

	if (remport>0)
		ms_filter_call_method(stream->rtpsend,MS_RTP_SEND_SET_SESSION,rtps);
	stream->rtprecv=ms_filter_new(MS_RTP_RECV_ID);
	ms_filter_call_method(stream->rtprecv,MS_RTP_RECV_SET_SESSION,rtps);
	stream->session=rtps;

	stream->dtmfgen=ms_filter_new(MS_DTMF_GEN_ID);
	rtp_session_signal_connect(rtps,"telephone-event",(RtpCallback)on_dtmf_received,(unsigned long)stream);
	rtp_session_signal_connect(rtps,"payload_type_changed",(RtpCallback)payload_type_changed,(unsigned long)stream);
	/* creates the local part */
	if (captcard!=NULL) stream->soundread=ms_snd_card_create_reader(captcard);
	else {
		stream->soundread=ms_filter_new(MS_FILE_PLAYER_ID);
		stream->read_resampler=ms_filter_new(MS_RESAMPLE_ID);
		if (infile!=NULL) audio_stream_play(stream,infile);
	}
	if (playcard!=NULL) stream->soundwrite=ms_snd_card_create_writer(playcard);
	else {
		stream->soundwrite=ms_filter_new(MS_FILE_REC_ID);
		if (outfile!=NULL) audio_stream_record(stream,outfile);
	}

	/* creates the couple of encoder/decoder */
	pt=rtp_profile_get_payload(profile,payload);
	if (pt==NULL){
		ms_error("audiostream.c: undefined payload type.");
		return -1;
	}
	if (rtp_profile_get_payload_from_mime (profile,"telephone-event")==NULL
	    && ( strcasecmp(pt->mime_type,"pcmu")==0 || strcasecmp(pt->mime_type,"pcma")==0)){
		/*if no telephone-event payload is usable and pcma or pcmu is used, we will generate
		  inband dtmf*/
		stream->dtmfgen_rtp=ms_filter_new (MS_DTMF_GEN_ID);
	}
	
	if (ms_filter_call_method(stream->rtpsend,MS_FILTER_GET_SAMPLE_RATE,&sample_rate)!=0){
		ms_error("Sample rate is unknown for RTP side !");
		return -1;
	}
	
	stream->encoder=ms_filter_create_encoder(pt->mime_type);
	stream->decoder=ms_filter_create_decoder(pt->mime_type);
	if ((stream->encoder==NULL) || (stream->decoder==NULL)){
		/* big problem: we have not a registered codec for this payload...*/
		ms_error("mediastream.c: No decoder available for payload %i.",payload);
		return -1;
	}

	stream->volsend=ms_filter_new(MS_VOLUME_ID);
	stream->volrecv=ms_filter_new(MS_VOLUME_ID);
	audio_stream_enable_echo_limiter(stream,stream->el_type);
	audio_stream_enable_noise_gate(stream,stream->use_ng);

	if (stream->use_agc){
		int tmp=1;
		if (stream->volsend==NULL)
			stream->volsend=ms_filter_new(MS_VOLUME_ID);
		ms_filter_call_method(stream->volsend,MS_VOLUME_ENABLE_AGC,&tmp);
	}

	/* give the sound filters some properties */
	if (ms_filter_call_method(stream->soundread,MS_FILTER_SET_SAMPLE_RATE,&sample_rate) != 0) {
		/* need to add resampler*/
		if (stream->read_resampler == NULL) stream->read_resampler=ms_filter_new(MS_RESAMPLE_ID);
	}

	if (ms_filter_call_method(stream->soundwrite,MS_FILTER_SET_SAMPLE_RATE,&sample_rate) != 0) {
		/* need to add resampler*/
		if (stream->write_resampler == NULL) stream->write_resampler=ms_filter_new(MS_RESAMPLE_ID);
	}

	tmp=1;
	ms_filter_call_method(stream->soundwrite,MS_FILTER_SET_NCHANNELS, &tmp);

	/*configure the echo canceller if required */
	if (use_ec) {
		stream->ec=ms_filter_new(MS_SPEEX_EC_ID);
		ms_filter_call_method(stream->ec,MS_FILTER_SET_SAMPLE_RATE,&sample_rate);
		if (stream->ec_tail_len!=0)
			ms_filter_call_method(stream->ec,MS_ECHO_CANCELLER_SET_TAIL_LENGTH,&stream->ec_tail_len);
		if (stream->ec_delay!=0){
			ms_filter_call_method(stream->ec,MS_ECHO_CANCELLER_SET_DELAY,&stream->ec_delay);
		}else{
			/*configure from latency of sound card in case it is availlable */
			int latency=0;
			ms_filter_call_method(stream->soundread,MS_FILTER_GET_LATENCY,&latency);
			latency-=30; /*keep 30 milliseconds security margin*/
			if (latency<0) latency=0;
			ms_filter_call_method(stream->ec,MS_ECHO_CANCELLER_SET_DELAY,&latency);
		}
		if (stream->ec_framesize!=0)
			ms_filter_call_method(stream->ec,MS_ECHO_CANCELLER_SET_FRAMESIZE,&stream->ec_framesize);
	}

	/* give the encoder/decoder some parameters*/
	ms_filter_call_method(stream->encoder,MS_FILTER_SET_SAMPLE_RATE,&pt->clock_rate);
	ms_message("Payload's bitrate is %i",pt->normal_bitrate);
	if (pt->normal_bitrate>0){
		ms_message("Setting audio encoder network bitrate to %i",pt->normal_bitrate);
		ms_filter_call_method(stream->encoder,MS_FILTER_SET_BITRATE,&pt->normal_bitrate);
	}
	ms_filter_call_method(stream->decoder,MS_FILTER_SET_SAMPLE_RATE,&pt->clock_rate);

	if (pt->send_fmtp!=NULL) ms_filter_call_method(stream->encoder,MS_FILTER_ADD_FMTP, (void*)pt->send_fmtp);
	if (pt->recv_fmtp!=NULL) ms_filter_call_method(stream->decoder,MS_FILTER_ADD_FMTP,(void*)pt->recv_fmtp);

	/*create the equalizer*/
	stream->equalizer=ms_filter_new(MS_EQUALIZER_ID);
	tmp=stream->eq_active;
	ms_filter_call_method(stream->equalizer,MS_EQUALIZER_SET_ACTIVE,&tmp);
	/*configure resampler if needed*/
	if (stream->read_resampler){
		audio_stream_configure_resampler(stream->read_resampler,stream->soundread,stream->rtpsend);
	}

	if (stream->write_resampler){
		audio_stream_configure_resampler(stream->write_resampler,stream->rtprecv,stream->soundwrite);
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
	ms_connection_helper_link(&h,stream->encoder,0,0);
	ms_connection_helper_link(&h,stream->rtpsend,0,-1);

	/*receiving graph*/
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h,stream->rtprecv,-1,0);
	ms_connection_helper_link(&h,stream->decoder,0,0);
	ms_connection_helper_link(&h,stream->dtmfgen,0,0);
	if (stream->equalizer)
		ms_connection_helper_link(&h,stream->equalizer,0,0);
	if (stream->volrecv)
		ms_connection_helper_link(&h,stream->volrecv,0,0);
	if (stream->ec)
		ms_connection_helper_link(&h,stream->ec,0,0);
	if (stream->write_resampler)
		ms_connection_helper_link(&h,stream->write_resampler,0,0);
	ms_connection_helper_link(&h,stream->soundwrite,0,-1);

	/* create ticker */
	stream->ticker=ms_ticker_new();
	ms_ticker_set_name(stream->ticker,"Audio MSTicker");
	ms_ticker_attach(stream->ticker,stream->soundread);
	ms_ticker_attach(stream->ticker,stream->rtprecv);

	return 0;
}


int audio_stream_start_with_files(AudioStream *stream, RtpProfile *prof,const char *remip, int remport,
	int rem_rtcp_port, int pt,int jitt_comp, const char *infile, const char * outfile)
{
	return audio_stream_start_full(stream,prof,remip,remport,rem_rtcp_port,pt,jitt_comp,infile,outfile,NULL,NULL,FALSE);
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
	stream=audio_stream_new(locport, ms_is_ipv6(remip));
	if (audio_stream_start_full(stream,prof,remip,remport,remport+1,profile,jitt_comp,NULL,NULL,sndcard_playback,sndcard_capture,use_ec)==0) return stream;
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
	stream=audio_stream_new(locport, ms_is_ipv6(remip));
	if (audio_stream_start_full(stream,prof,remip,remport,remport+1,profile,jitt_comp,NULL,NULL,playcard,captcard,use_ec)==0) return stream;
	audio_stream_free(stream);
	return NULL;
}

void audio_stream_set_rtcp_information(AudioStream *st, const char *cname, const char *tool){
	if (st->session!=NULL){
		rtp_session_set_source_description(st->session,cname,NULL,NULL,NULL,NULL,tool , "This is free software (GPL) !");
	}
}

void audio_stream_play(AudioStream *st, const char *name){
	if (ms_filter_get_id(st->soundread)==MS_FILE_PLAYER_ID){
		ms_filter_call_method_noarg(st->soundread,MS_FILE_PLAYER_CLOSE);
		ms_filter_call_method(st->soundread,MS_FILE_PLAYER_OPEN,(void*)name);
		if (st->read_resampler){
			audio_stream_configure_resampler(st->read_resampler,st->soundread,st->rtpsend);
		}
		ms_filter_call_method_noarg(st->soundread,MS_FILE_PLAYER_START);
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


AudioStream *audio_stream_new(int locport, bool_t ipv6){
	AudioStream *stream=(AudioStream *)ms_new0(AudioStream,1);
	stream->session=create_duplex_rtpsession(locport,ipv6);
	stream->rtpsend=ms_filter_new(MS_RTP_SEND_ID);
	stream->evq=ortp_ev_queue_new();
	rtp_session_register_event_queue(stream->session,stream->evq);
	stream->play_dtmfs=TRUE;
	stream->use_gc=FALSE;
	stream->use_agc=FALSE;
	stream->use_ng=FALSE;
	ms_filter_enable_statistics(TRUE);
	ms_filter_reset_statistics();
	return stream;
}

void audio_stream_play_received_dtmfs(AudioStream *st, bool_t yesno){
	st->play_dtmfs=yesno;
}

int audio_stream_start_now(AudioStream *stream, RtpProfile * prof,  const char *remip, int remport, int rem_rtcp_port, int payload_type, int jitt_comp, MSSndCard *playcard, MSSndCard *captcard, bool_t use_ec){
	return audio_stream_start_full(stream,prof,remip,remport,rem_rtcp_port,
		payload_type,jitt_comp,NULL,NULL,playcard,captcard,use_ec);
}

void audio_stream_set_relay_session_id(AudioStream *stream, const char *id){
	ms_filter_call_method(stream->rtpsend, MS_RTP_SEND_SET_RELAY_SESSION_ID,(void*)id);
}

void audio_stream_set_echo_canceller_params(AudioStream *st, int tail_len_ms, int delay_ms, int framesize){
	st->ec_tail_len=tail_len_ms;
	st->ec_delay=delay_ms;
	st->ec_framesize=framesize;
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
	if (stream->ticker){
		MSConnectionHelper h;
		ms_ticker_detach(stream->ticker,stream->soundread);
		ms_ticker_detach(stream->ticker,stream->rtprecv);

		rtp_stats_display(rtp_session_get_stats(stream->session),"Audio session's RTP statistics");

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
		ms_connection_helper_unlink(&h,stream->encoder,0,0);
		ms_connection_helper_unlink(&h,stream->rtpsend,0,-1);

		/*dismantle the receiving graph*/
		ms_connection_helper_start(&h);
		ms_connection_helper_unlink(&h,stream->rtprecv,-1,0);
		ms_connection_helper_unlink(&h,stream->decoder,0,0);
		ms_connection_helper_unlink(&h,stream->dtmfgen,0,0);
		if (stream->equalizer)
			ms_connection_helper_unlink(&h,stream->equalizer,0,0);
		if (stream->volrecv!=NULL)
			ms_connection_helper_unlink(&h,stream->volrecv,0,0);
		if (stream->ec!=NULL)
			ms_connection_helper_unlink(&h,stream->ec,0,0);
		if (stream->write_resampler!=NULL)
			ms_connection_helper_unlink(&h,stream->write_resampler,0,0);
		ms_connection_helper_unlink(&h,stream->soundwrite,0,-1);

	}
	audio_stream_free(stream);
	ms_filter_log_statistics();
}

RingStream * ring_start(const char *file, int interval, MSSndCard *sndcard){
   return ring_start_with_cb(file,interval,sndcard,NULL,NULL);
}

RingStream * ring_start_with_cb(const char *file,int interval,MSSndCard *sndcard, MSFilterNotifyFunc func,void * user_data)
{
	RingStream *stream;
	int tmp;
	int srcrate,dstrate;
	MSConnectionHelper h;

	stream=(RingStream *)ms_new0(RingStream,1);
	stream->source=ms_filter_new(MS_FILE_PLAYER_ID);
	if (file)
		ms_filter_call_method(stream->source,MS_FILE_PLAYER_OPEN,(void*)file);
	
	ms_filter_call_method(stream->source,MS_FILE_PLAYER_LOOP,&interval);
	ms_filter_call_method_noarg(stream->source,MS_FILE_PLAYER_START);
	if (func!=NULL)
		ms_filter_set_notify_callback(stream->source,func,user_data);
	stream->gendtmf=ms_filter_new(MS_DTMF_GEN_ID);
	
	
	stream->sndwrite=ms_snd_card_create_writer(sndcard);
	ms_filter_call_method(stream->source,MS_FILTER_GET_SAMPLE_RATE,&srcrate);
	ms_filter_call_method(stream->gendtmf,MS_FILTER_SET_SAMPLE_RATE,&srcrate);
	ms_filter_call_method(stream->sndwrite,MS_FILTER_SET_SAMPLE_RATE,&srcrate);
	ms_filter_call_method(stream->sndwrite,MS_FILTER_GET_SAMPLE_RATE,&dstrate);
	if (srcrate!=dstrate){
		stream->write_resampler=ms_filter_new(MS_RESAMPLE_ID);
		ms_filter_call_method(stream->write_resampler,MS_FILTER_SET_SAMPLE_RATE,&srcrate);
		ms_filter_call_method(stream->write_resampler,MS_FILTER_SET_OUTPUT_SAMPLE_RATE,&dstrate);
		ms_message("configuring resampler from rate[%i] to rate [%i]", srcrate,dstrate);
	}
	ms_filter_call_method(stream->source,MS_FILTER_GET_NCHANNELS,&tmp);
	ms_filter_call_method(stream->gendtmf,MS_FILTER_SET_NCHANNELS,&tmp);
	ms_filter_call_method(stream->sndwrite,MS_FILTER_SET_NCHANNELS,&tmp);
	
	stream->ticker=ms_ticker_new();
	
	ms_ticker_set_name(stream->ticker,"Audio (ring) MSTicker");

	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h,stream->source,-1,0);
	ms_connection_helper_link(&h,stream->gendtmf,0,0);
	if (stream->write_resampler)
		ms_connection_helper_link(&h,stream->write_resampler,0,0);
	ms_connection_helper_link(&h,stream->sndwrite,0,-1);
	ms_ticker_attach(stream->ticker,stream->source);
	
	return stream;
}

void ring_play_dtmf(RingStream *stream, char dtmf, int duration_ms){
	if (duration_ms>0)
		ms_filter_call_method(stream->gendtmf, MS_DTMF_GEN_PLAY, &dtmf);
	else ms_filter_call_method(stream->gendtmf, MS_DTMF_GEN_START, &dtmf);
}

void ring_stop_dtmf(RingStream *stream){
	ms_filter_call_method_noarg(stream->gendtmf, MS_DTMF_GEN_STOP);
}

void ring_stop(RingStream *stream){
	MSConnectionHelper h;
	ms_ticker_detach(stream->ticker,stream->source);

	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h,stream->source,-1,0);
	ms_connection_helper_unlink(&h,stream->gendtmf,0,0);
	if (stream->write_resampler)
		ms_connection_helper_unlink(&h,stream->write_resampler,0,0);
	ms_connection_helper_unlink(&h,stream->sndwrite,0,-1);

	ms_ticker_destroy(stream->ticker);
	ms_filter_destroy(stream->source);
	ms_filter_destroy(stream->gendtmf);
	ms_filter_destroy(stream->sndwrite);
	ms_free(stream);
#ifdef _WIN32_WCE
	ms_warning("Sleeping a bit after closing the audio device...");
	ms_sleep(1);
#endif
}


int audio_stream_send_dtmf(AudioStream *stream, char dtmf)
{
	if (stream->dtmfgen_rtp)
		ms_filter_call_method(stream->dtmfgen_rtp,MS_DTMF_GEN_PLAY,&dtmf);
	else if (stream->rtpsend)
		ms_filter_call_method(stream->rtpsend,MS_RTP_SEND_SEND_DTMF,&dtmf);
	return 0;
}

void audio_stream_get_local_rtp_stats(AudioStream *stream, rtp_stats_t *lstats){
	if (stream->session){
		const rtp_stats_t *stats=rtp_session_get_stats(stream->session);
		memcpy(lstats,stats,sizeof(*stats));
	}else memset(lstats,0,sizeof(rtp_stats_t));
}


void audio_stream_mute_rtp(AudioStream *stream, bool_t val) 
{
  if (stream->rtpsend){
    if (val)
      ms_filter_call_method(stream->rtpsend,MS_RTP_SEND_MUTE_MIC,&val);
    else
      ms_filter_call_method(stream->rtpsend,MS_RTP_SEND_UNMUTE_MIC,&val);
  }
}

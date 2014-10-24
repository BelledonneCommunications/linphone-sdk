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
#include "mediastreamer2/msitc.h"
#include "private.h"

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif


#include <sys/types.h>

#ifndef WIN32
	#include <sys/socket.h>
	#include <netdb.h>
#endif

static void configure_av_recorder(AudioStream *stream);

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
	if (stream->recorder) ms_filter_destroy(stream->recorder);
	if (stream->recorder_mixer) ms_filter_destroy(stream->recorder_mixer);
	if (stream->local_mixer) ms_filter_destroy(stream->local_mixer);
	if (stream->local_player) ms_filter_destroy(stream->local_player);
	if (stream->local_player_resampler) ms_filter_destroy(stream->local_player_resampler);
	if (stream->av_recorder.encoder) ms_filter_destroy(stream->av_recorder.encoder);
	if (stream->av_recorder.recorder) ms_filter_destroy(stream->av_recorder.recorder);
	if (stream->av_recorder.resampler) ms_filter_destroy(stream->av_recorder.resampler);
	if (stream->av_recorder.video_input) ms_filter_destroy(stream->av_recorder.video_input);
	if (stream->outbound_mixer) ms_filter_destroy(stream->outbound_mixer);
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

/*
 * note: since not all filters implement MS_FILTER_GET_SAMPLE_RATE, fallback_from_rate and fallback_to_rate are expected to provide sample rates
 * obtained by another context, such as the RTP clock rate for example.
 */
static void audio_stream_configure_resampler(MSFilter *resampler,MSFilter *from, MSFilter *to, int fallback_from_rate, int fallback_to_rate) {
	int from_rate=0, to_rate=0;
	int from_channels = 0, to_channels = 0;
	ms_filter_call_method(from,MS_FILTER_GET_SAMPLE_RATE,&from_rate);
	ms_filter_call_method(to,MS_FILTER_GET_SAMPLE_RATE,&to_rate);
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
	if (from_rate == 0){
		ms_error("Filter %s does not implement the MS_FILTER_GET_SAMPLE_RATE method", from->desc->name);
		from_rate=fallback_from_rate;
	}
	if (to_rate == 0){
		ms_error("Filter %s does not implement the MS_FILTER_GET_SAMPLE_RATE method", to->desc->name);
		to_rate=fallback_to_rate;
	}
	ms_filter_call_method(resampler,MS_FILTER_SET_SAMPLE_RATE,&from_rate);
	ms_filter_call_method(resampler,MS_FILTER_SET_OUTPUT_SAMPLE_RATE,&to_rate);
	ms_filter_call_method(resampler, MS_FILTER_SET_NCHANNELS, &from_channels);
	ms_filter_call_method(resampler, MS_FILTER_SET_OUTPUT_NCHANNELS, &to_channels);
	ms_message("configuring %s-->%s from rate [%i] to rate [%i] and from channel [%i] to channel [%i]",
			   from->desc->name, to->desc->name, from_rate, to_rate, from_channels, to_channels);
}

static void audio_stream_process_rtcp(MediaStream *media_stream, mblk_t *m){
}

void audio_stream_iterate(AudioStream *stream){
	media_stream_iterate(&stream->ms);
}

bool_t audio_stream_alive(AudioStream * stream, int timeout){
	return media_stream_alive((MediaStream*)stream,timeout);
}

/*invoked from FEC capable filters*/
static  mblk_t* audio_stream_payload_picker(MSRtpPayloadPickerContext* context,unsigned int sequence_number) {
	return rtp_session_pick_with_cseq(((AudioStream*)(context->filter_graph_manager))->ms.sessions.rtp_session, sequence_number);
}

static void stop_preload_graph(AudioStream *stream){
	ms_ticker_detach(stream->ms.sessions.ticker,stream->dummy);

	if (stream->ms.voidsink) {
		ms_filter_unlink(stream->dummy,0,stream->ms.voidsink,0);
		ms_filter_destroy(stream->ms.voidsink);
		stream->ms.voidsink=NULL;
	}else if (stream->soundwrite) {
		ms_filter_unlink(stream->dummy,0,stream->soundwrite,0);
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
	rtp_session_set_payload_type(stream->ms.sessions.rtp_session,0);
	rtp_session_enable_rtcp(stream->ms.sessions.rtp_session, FALSE);
	ms_filter_call_method(stream->dummy,MS_RTP_RECV_SET_SESSION,stream->ms.sessions.rtp_session);

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
	if (stream->ms.sessions.ticker == NULL) media_stream_start_ticker(&stream->ms);
	ms_ticker_attach(stream->ms.sessions.ticker,stream->dummy);
	stream->ms.state=MSStreamPreparing;
}

static void _audio_stream_unprepare_sound(AudioStream *stream, bool_t keep_sound_resources){
	if (stream->ms.state==MSStreamPreparing){
		stop_preload_graph(stream);
#ifdef __ios
		if (!keep_sound_resources){
			if (stream->soundread) ms_filter_destroy(stream->soundread);
			stream->soundread=NULL;
			if (stream->soundwrite) ms_filter_destroy(stream->soundwrite);
			stream->soundwrite=NULL;
		}
#endif
	}
	stream->ms.state=MSStreamInitialized;
}

void audio_stream_unprepare_sound(AudioStream *stream){
	_audio_stream_unprepare_sound(stream,FALSE);
}

static void player_callback(void *ud, MSFilter *f, unsigned int id, void *arg){
	AudioStream *stream=(AudioStream *)ud;
	int sr=0;
	int channels=0;
	switch(id){
		case MS_FILTER_OUTPUT_FMT_CHANGED:
			ms_filter_call_method(f,MS_FILTER_GET_SAMPLE_RATE,&sr);
			ms_filter_call_method(f,MS_FILTER_GET_NCHANNELS,&channels);
			if (f==stream->local_player){
				ms_filter_call_method(stream->local_player_resampler,MS_FILTER_SET_SAMPLE_RATE,&sr);
				ms_filter_call_method(stream->local_player_resampler,MS_FILTER_SET_NCHANNELS,&channels);
			}
		break;
		default:
		break;
	}
}

static void setup_local_player(AudioStream *stream, int samplerate, int channels){
	MSConnectionHelper cnx;
	int master=0;

	stream->local_player=ms_filter_new(MS_FILE_PLAYER_ID);
	stream->local_player_resampler=ms_filter_new(MS_RESAMPLE_ID);

	ms_connection_helper_start(&cnx);
	ms_connection_helper_link(&cnx,stream->local_player,-1,0);
	ms_connection_helper_link(&cnx,stream->local_player_resampler,0,0);
	ms_connection_helper_link(&cnx,stream->local_mixer,1,-1);

	ms_filter_call_method(stream->local_player_resampler,MS_FILTER_SET_OUTPUT_SAMPLE_RATE,&samplerate);
	ms_filter_call_method(stream->local_player_resampler,MS_FILTER_SET_OUTPUT_NCHANNELS,&channels);
	ms_filter_call_method(stream->local_mixer,MS_FILTER_SET_SAMPLE_RATE,&samplerate);
	ms_filter_call_method(stream->local_mixer,MS_FILTER_SET_NCHANNELS,&channels);
	ms_filter_call_method(stream->local_mixer,MS_AUDIO_MIXER_SET_MASTER_CHANNEL,&master);
	ms_filter_add_notify_callback(stream->local_player,player_callback,stream,TRUE);
}

static OrtpRtcpXrPlcStatus audio_stream_get_rtcp_xr_plc_status(unsigned long userdata) {
	AudioStream *stream = (AudioStream *)userdata;
	if ((stream->features & AUDIO_STREAM_FEATURE_PLC) != 0) {
		int decoder_have_plc = 0;
		if (stream->ms.decoder && ms_filter_has_method(stream->ms.decoder, MS_AUDIO_DECODER_HAVE_PLC)) {
			ms_filter_call_method(stream->ms.decoder, MS_AUDIO_DECODER_HAVE_PLC, &decoder_have_plc);
		}
		if (decoder_have_plc == 0) {
			return OrtpRtcpXrSilencePlc;
		} else {
			return OrtpRtcpXrEnhancedPlc;
		}
	}
	return OrtpRtcpXrNoPlc;
}

static int audio_stream_get_rtcp_xr_signal_level(unsigned long userdata) {
	AudioStream *stream = (AudioStream *)userdata;
	if ((stream->features & AUDIO_STREAM_FEATURE_VOL_RCV) != 0) {
		float volume = 0.f;
		if (stream->volrecv)
			ms_filter_call_method(stream->volrecv, MS_VOLUME_GET_MAX, &volume);
		return (int)volume;
	}
	return ORTP_RTCP_XR_UNAVAILABLE_PARAMETER;
}

static int audio_stream_get_rtcp_xr_noise_level(unsigned long userdata) {
	AudioStream *stream = (AudioStream *)userdata;
	if ((stream->features & AUDIO_STREAM_FEATURE_VOL_RCV) != 0) {
		float volume = 0.f;
		if (stream->volrecv)
			ms_filter_call_method(stream->volrecv, MS_VOLUME_GET_MIN, &volume);
		return (int)volume;
	}
	return ORTP_RTCP_XR_UNAVAILABLE_PARAMETER;
}

static float audio_stream_get_rtcp_xr_average_quality_rating(unsigned long userdata) {
	AudioStream *stream = (AudioStream *)userdata;
	return audio_stream_get_average_quality_rating(stream);
}

static float audio_stream_get_rtcp_xr_average_lq_quality_rating(unsigned long userdata) {
	AudioStream *stream = (AudioStream *)userdata;
	return audio_stream_get_average_lq_quality_rating(stream);
}

static bool_t ci_ends_with(const char *filename, const char*suffix){
	int filename_len=strlen(filename);
	int suffix_len=strlen(suffix);
	if (filename_len<suffix_len) return FALSE;
	return strcasecmp(filename+filename_len-suffix_len,suffix)==0;
}

static MSFilter *create_av_player(const char *filename){
	if (ci_ends_with(filename,".mkv"))
		return ms_filter_new(MS_MKV_PLAYER_ID);
	else if (ci_ends_with(filename,".wav"))
		return ms_filter_new(MS_FILE_PLAYER_ID);
	return NULL;
}

static void unplumb_av_player(AudioStream *stream){
	struct _AVPlayer *player=&stream->av_player;
	MSConnectionHelper ch;
	bool_t reattach=stream->ms.state==MSStreamStarted;

	if (!player->plumbed) return;

	/*detach the outbound graph before modifying the graph*/
	ms_ticker_detach(stream->ms.sessions.ticker,stream->soundread);
	if (player->videopin!=-1){
		ms_connection_helper_start(&ch);
		ms_connection_helper_unlink(&ch,player->player,-1,player->videopin);
		ms_connection_helper_unlink(&ch,player->video_output,0,0);
	}
	ms_connection_helper_start(&ch);
	ms_connection_helper_unlink(&ch,player->player,-1,player->audiopin);
	if (player->decoder)
		ms_connection_helper_unlink(&ch,player->decoder,0,0);
	ms_connection_helper_unlink(&ch,player->resampler,0,0);
	ms_connection_helper_unlink(&ch,stream->outbound_mixer,1,-1);
	/*and attach back*/
	if (reattach) ms_ticker_attach(stream->ms.sessions.ticker,stream->soundread);
	player->plumbed = FALSE;
}

static void close_av_player(AudioStream *stream){
	struct _AVPlayer *player=&stream->av_player;

	if (player->player){
		MSPlayerState st=MSPlayerClosed;
		unplumb_av_player(stream);
		if (ms_filter_call_method(player->player,MS_PLAYER_GET_STATE,&st)==0){
			if (st!=MSPlayerClosed)
				ms_filter_call_method_noarg(player->player,MS_PLAYER_CLOSE);
		}
		ms_filter_destroy(player->player);
		player->player=NULL;
	}
	if (player->resampler){
		ms_filter_destroy(player->resampler);
		player->resampler=NULL;
	}
	if (player->decoder){
		ms_filter_destroy(player->decoder);
		player->decoder=NULL;
	}
}

static void configure_av_player(AudioStream *stream, const MSFmtDescriptor *audiofmt, const MSFmtDescriptor *videofmt){
	struct _AVPlayer *player=&stream->av_player;
	int stream_rate=0;
	int stream_channels=0;
	
	ms_message("AudioStream [%p] Configure av_player, audiofmt=%s videofmt=%s",stream,ms_fmt_descriptor_to_string(audiofmt),ms_fmt_descriptor_to_string(videofmt));
	
	if (audiofmt){
		if (player->decoder){
			if (audiofmt->nchannels>0){
				ms_filter_call_method(player->decoder,MS_FILTER_SET_NCHANNELS,(void*)&audiofmt->nchannels);
			}
			if (audiofmt->rate>0){
				ms_filter_call_method(player->decoder,MS_FILTER_SET_SAMPLE_RATE,(void*)&audiofmt->rate);
			}
		}
		ms_filter_call_method(player->resampler,MS_FILTER_SET_NCHANNELS,(void*)&audiofmt->nchannels);
		ms_filter_call_method(player->resampler,MS_FILTER_SET_SAMPLE_RATE,(void*)&audiofmt->rate);
	}

	ms_filter_call_method(stream->outbound_mixer,MS_FILTER_GET_SAMPLE_RATE,&stream_rate);
	ms_filter_call_method(stream->outbound_mixer,MS_FILTER_GET_NCHANNELS,&stream_channels);
	ms_filter_call_method(player->resampler,MS_FILTER_SET_OUTPUT_NCHANNELS,&stream_channels);
	ms_filter_call_method(player->resampler,MS_FILTER_SET_OUTPUT_SAMPLE_RATE,&stream_rate);
	if (videofmt){
		MSPinFormat pf;
		pf.pin=0;
		pf.fmt=videofmt;
		ms_filter_call_method(player->video_output,MS_FILTER_SET_INPUT_FMT,&pf);
	}
}

static void plumb_av_player(AudioStream *stream){
	struct _AVPlayer *player=&stream->av_player;
	MSConnectionHelper ch;
	bool_t reattach=stream->ms.state==MSStreamStarted;

	if (player->videopin!=-1){
		ms_connection_helper_start(&ch);
		ms_connection_helper_link(&ch,player->player,-1,player->videopin);
		ms_connection_helper_link(&ch,player->video_output,0,0);
	}
	ms_connection_helper_start(&ch);
	ms_connection_helper_link(&ch,player->player,-1,player->audiopin);
	if (player->decoder)
		ms_connection_helper_link(&ch,player->decoder,0,0);
	ms_connection_helper_link(&ch,player->resampler,0,0);
	/*detach the outbound graph before attaching to the outbound mixer*/
	if (reattach) ms_ticker_detach(stream->ms.sessions.ticker,stream->soundread);
	ms_connection_helper_link(&ch,stream->outbound_mixer,1,-1);
	/*and attach back*/
	if (reattach) ms_ticker_attach(stream->ms.sessions.ticker,stream->soundread);
	player->plumbed=TRUE;
}

static int open_av_player(AudioStream *stream, const char *filename){
	struct _AVPlayer *player=&stream->av_player;
	MSPinFormat fmt1={0},fmt2={0};
	MSPinFormat *audiofmt=NULL;
	MSPinFormat *videofmt=NULL;

	if (player->player) close_av_player(stream);
	player->player=create_av_player(filename);
	if (player->player==NULL){
		ms_warning("AudioStream[%p]: no way to open [%s].",stream,filename);
		return -1;
	}
	if (ms_filter_call_method(player->player,MS_PLAYER_OPEN,(void*)filename)==-1){
		close_av_player(stream);
		return -1;
	}
	fmt1.pin=0;
	ms_filter_call_method(player->player,MS_FILTER_GET_OUTPUT_FMT,&fmt1);
	fmt2.pin=1;
	ms_filter_call_method(player->player,MS_FILTER_GET_OUTPUT_FMT,&fmt2);
	if (fmt1.fmt==NULL && fmt2.fmt==NULL){
		/*assume PCM*/
		int sr=8000;
		int channels=1;
		ms_filter_call_method(player->player,MS_FILTER_GET_SAMPLE_RATE,&sr);
		ms_filter_call_method(player->player,MS_FILTER_GET_NCHANNELS,&channels);
		fmt1.fmt=ms_factory_get_audio_format(ms_factory_get_fallback(),"pcm", sr, channels, NULL);
		audiofmt=&fmt1;
	}else{
		if (fmt1.fmt && fmt1.fmt->type==MSAudio) {
			audiofmt=&fmt1;
			videofmt=&fmt2;
			player->audiopin=0;
			player->videopin=1;
		}else{
			videofmt=&fmt1;
			audiofmt=&fmt2;
			player->audiopin=1;
			player->videopin=0;
		}
	}
	if (audiofmt && audiofmt->fmt && strcasecmp(audiofmt->fmt->encoding,"pcm")!=0){
		player->decoder=ms_filter_create_decoder(audiofmt->fmt->encoding);
		if (player->decoder==NULL){
			ms_warning("AudioStream[%p]: no way to decode [%s]",stream,filename);
			close_av_player(stream);
			return -1;
		}
	}
	player->resampler=ms_filter_new(MS_RESAMPLE_ID);
	if (videofmt && videofmt->fmt) player->video_output=ms_filter_new(MS_ITC_SINK_ID);
	else player->videopin=-1;
	configure_av_player(stream,audiofmt ? audiofmt->fmt : NULL ,videofmt ? videofmt->fmt : NULL);
	if (stream->videostream) video_stream_open_player(stream->videostream,player->video_output);
	plumb_av_player(stream);
	return 0;
}

MSFilter * audio_stream_open_remote_play(AudioStream *stream, const char *filename){
	if (stream->ms.state!=MSStreamStarted){
		ms_warning("AudioStream[%p]: audio_stream_play_to_remote() works only when the stream is started.",stream);
		return NULL;
	}
	if (stream->outbound_mixer==NULL){
		ms_warning("AudioStream[%p]: audio_stream_play_to_remote() works only when the stream has AUDIO_STREAM_FEATURE_REMOTE_PLAYING capability.",stream);
		return NULL;
	}
	if (open_av_player(stream,filename)==-1){
		return NULL;
	}
	return stream->av_player.player;
}

void audio_stream_close_remote_play(AudioStream *stream){
	MSPlayerState state;
	if (stream->av_player.player){
		ms_filter_call_method(stream->av_player.player,MS_PLAYER_GET_STATE,&state);
		if (state!=MSPlayerClosed)
			ms_filter_call_method_noarg(stream->av_player.player,MS_PLAYER_CLOSE);
	}
	if (stream->videostream) video_stream_close_player(stream->videostream);
}

static void video_input_updated(void *stream, MSFilter *f, unsigned int event_id, void *arg){
	if (event_id==MS_FILTER_OUTPUT_FMT_CHANGED){
		ms_message("Video ITC source updated.");
		configure_av_recorder((AudioStream*)stream);
	}
}

static void setup_av_recorder(AudioStream *stream, int sample_rate, int nchannels){
	stream->av_recorder.recorder=ms_filter_new(MS_MKV_RECORDER_ID);
	if (stream->av_recorder.recorder){
		MSPinFormat pinfmt={0};
		stream->av_recorder.video_input=ms_filter_new(MS_ITC_SOURCE_ID);
		stream->av_recorder.resampler=ms_filter_new(MS_RESAMPLE_ID);
		stream->av_recorder.encoder=ms_filter_new(MS_OPUS_ENC_ID);

		if (stream->av_recorder.encoder==NULL){
			int g711_rate=8000;
			int g711_nchannels=1;
			stream->av_recorder.encoder=ms_filter_new(MS_ULAW_ENC_ID);
			ms_filter_call_method(stream->av_recorder.resampler,MS_FILTER_SET_SAMPLE_RATE,&sample_rate);
			ms_filter_call_method(stream->av_recorder.resampler,MS_FILTER_SET_OUTPUT_SAMPLE_RATE,&g711_rate);
			ms_filter_call_method(stream->av_recorder.resampler,MS_FILTER_SET_NCHANNELS,&nchannels);
			ms_filter_call_method(stream->av_recorder.resampler,MS_FILTER_SET_OUTPUT_NCHANNELS,&g711_nchannels);
			pinfmt.fmt=ms_factory_get_audio_format(ms_factory_get_fallback(),"pcmu",g711_rate,g711_nchannels,NULL);

		}else{
			int got_sr=0;
			ms_filter_call_method(stream->av_recorder.encoder,MS_FILTER_SET_SAMPLE_RATE,&sample_rate);
			ms_filter_call_method(stream->av_recorder.encoder,MS_FILTER_GET_SAMPLE_RATE,&got_sr);
			ms_filter_call_method(stream->av_recorder.encoder,MS_FILTER_SET_NCHANNELS,&nchannels);
			ms_filter_call_method(stream->av_recorder.resampler,MS_FILTER_SET_SAMPLE_RATE,&sample_rate);
			ms_filter_call_method(stream->av_recorder.resampler,MS_FILTER_SET_OUTPUT_SAMPLE_RATE,&got_sr);
			ms_filter_call_method(stream->av_recorder.resampler,MS_FILTER_SET_NCHANNELS,&nchannels);
			ms_filter_call_method(stream->av_recorder.resampler,MS_FILTER_SET_OUTPUT_NCHANNELS,&nchannels);
			pinfmt.fmt=ms_factory_get_audio_format(ms_factory_get_fallback(),"opus",48000,nchannels,NULL);
		}
		pinfmt.pin=1;
		ms_message("Configuring av recorder with audio format %s",ms_fmt_descriptor_to_string(pinfmt.fmt));
		ms_filter_call_method(stream->av_recorder.recorder,MS_FILTER_SET_INPUT_FMT,&pinfmt);
		ms_filter_add_notify_callback(stream->av_recorder.video_input,video_input_updated,stream,TRUE);
	}
}

static void plumb_av_recorder(AudioStream *stream){
	MSConnectionHelper ch;
	ms_connection_helper_start(&ch);
	ms_connection_helper_link(&ch, stream->recorder_mixer,-1, 1);
	ms_connection_helper_link(&ch, stream->av_recorder.resampler,0,0);
	ms_connection_helper_link(&ch, stream->av_recorder.encoder,0,0);
	ms_connection_helper_link(&ch, stream->av_recorder.recorder,1,-1);

	ms_filter_link(stream->av_recorder.video_input,0,stream->av_recorder.recorder,0);
}

static void unplumb_av_recorder(AudioStream *stream){
	MSConnectionHelper ch;
	MSRecorderState rstate;
	ms_connection_helper_start(&ch);
	ms_connection_helper_unlink(&ch, stream->recorder_mixer,-1, 1);
	ms_connection_helper_unlink(&ch, stream->av_recorder.resampler,0,0);
	ms_connection_helper_unlink(&ch, stream->av_recorder.encoder,0,0);
	ms_connection_helper_unlink(&ch, stream->av_recorder.recorder,1,-1);

	ms_filter_unlink(stream->av_recorder.video_input,0,stream->av_recorder.recorder,0);

	if (ms_filter_call_method(stream->av_recorder.recorder,MS_RECORDER_GET_STATE,&rstate)==0){
		if (rstate!=MSRecorderClosed){
			ms_filter_call_method_noarg(stream->av_recorder.recorder, MS_RECORDER_CLOSE);
		}
	}
}

static void setup_recorder(AudioStream *stream, int sample_rate, int nchannels){
	int val=0;
	int pin=1;
	MSAudioMixerCtl mctl={0};

	stream->recorder=ms_filter_new(MS_FILE_REC_ID);
	stream->recorder_mixer=ms_filter_new(MS_AUDIO_MIXER_ID);
	stream->recv_tee=ms_filter_new(MS_TEE_ID);
	ms_filter_call_method(stream->recorder_mixer,MS_AUDIO_MIXER_ENABLE_CONFERENCE_MODE,&val);
	ms_filter_call_method(stream->recorder_mixer,MS_FILTER_SET_SAMPLE_RATE,&sample_rate);
	ms_filter_call_method(stream->recorder_mixer,MS_FILTER_SET_NCHANNELS,&nchannels);
	ms_filter_call_method(stream->recv_tee,MS_TEE_MUTE,&pin);
	mctl.pin=pin;
	mctl.param.enabled=FALSE;
	ms_filter_call_method(stream->outbound_mixer,MS_AUDIO_MIXER_ENABLE_OUTPUT,&mctl);
	ms_filter_call_method(stream->recorder,MS_FILTER_SET_SAMPLE_RATE,&sample_rate);
	ms_filter_call_method(stream->recorder,MS_FILTER_SET_NCHANNELS,&nchannels);

	setup_av_recorder(stream,sample_rate,nchannels);
}

int audio_stream_start_full(AudioStream *stream, RtpProfile *profile, const char *rem_rtp_ip,int rem_rtp_port,
	const char *rem_rtcp_ip, int rem_rtcp_port, int payload,int jitt_comp, const char *infile, const char *outfile,
	MSSndCard *playcard, MSSndCard *captcard, bool_t use_ec){
	RtpSession *rtps=stream->ms.sessions.rtp_session;
	PayloadType *pt,*tel_ev;
	int tmp;
	MSConnectionHelper h;
	int sample_rate;
	MSRtpPayloadPickerContext picker_context;
	int nchannels;
	bool_t has_builtin_ec=FALSE;

	rtp_session_set_profile(rtps,profile);
	if (rem_rtp_port>0) rtp_session_set_remote_addr_full(rtps,rem_rtp_ip,rem_rtp_port,rem_rtcp_ip,rem_rtcp_port);
	if (rem_rtcp_port > 0) {
		rtp_session_enable_rtcp(rtps, TRUE);
	} else {
		rtp_session_enable_rtcp(rtps, FALSE);
	}
	rtp_session_set_payload_type(rtps,payload);
	rtp_session_set_jitter_compensation(rtps,jitt_comp);

	if (rem_rtp_port>0)
		ms_filter_call_method(stream->ms.rtpsend,MS_RTP_SEND_SET_SESSION,rtps);
	stream->ms.rtprecv=ms_filter_new(MS_RTP_RECV_ID);
	ms_filter_call_method(stream->ms.rtprecv,MS_RTP_RECV_SET_SESSION,rtps);
	stream->ms.sessions.rtp_session=rtps;

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
	}
	if (playcard!=NULL) {
		if (stream->soundwrite==NULL)
			stream->soundwrite=ms_snd_card_create_writer(playcard);
	}else {
		stream->soundwrite=ms_filter_new(MS_FILE_REC_ID);
	}

	/* creates the couple of encoder/decoder */
	pt=rtp_profile_get_payload(profile,payload);
	if (pt==NULL){
		ms_error("audiostream.c: undefined payload type.");
		return -1;
	}
	nchannels=pt->channels;
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

	/* sample rate is already set for rtpsend and rtprcv, check if we have to adjust it to */
	/* be able to use the echo canceller wich may be limited (webrtc aecm max frequency is 16000 Hz) */
	// First check if we need to use the echo canceller
	// Overide feature if not requested or done at sound card level
	if ( ((stream->features & AUDIO_STREAM_FEATURE_EC) && !use_ec) || has_builtin_ec )
		stream->features &=~AUDIO_STREAM_FEATURE_EC;

	/*configure the echo canceller if required */
	if ((stream->features & AUDIO_STREAM_FEATURE_EC) == 0 && stream->ec != NULL) {
		ms_filter_destroy(stream->ec);
		stream->ec=NULL;
	}

	if ((stream->ms.encoder==NULL) || (stream->ms.decoder==NULL)){
		/* big problem: we have not a registered codec for this payload...*/
		ms_error("audio_stream_start_full: No decoder or encoder available for payload %s.",pt->mime_type);
		return -1;
	}

	/* check echo canceller max frequency and adjust sampling rate if needed when codec used is opus */
	if (stream->ec!=NULL) {
		int ec_sample_rate = sample_rate;
		ms_filter_call_method(stream->ec, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
		ms_filter_call_method(stream->ec, MS_FILTER_GET_SAMPLE_RATE, &ec_sample_rate);
		if (sample_rate != ec_sample_rate) {
			if (ms_filter_get_id(stream->ms.encoder) == MS_OPUS_ENC_ID) {
				sample_rate = ec_sample_rate;
				ms_message("Sampling rate forced to %iHz to allow the use of echo canceller", sample_rate);
			} else {
				ms_warning("Echo canceller does not support sampling rate %iHz, so it has been disabled", sample_rate);
				ms_filter_destroy(stream->ec);
				stream->ec = NULL;
			}
		}
	}
	/*hack for opus, that claims stereo all the time, but we can't support stereo yet*/
	if (strcasecmp(pt->mime_type,"opus")==0)
		nchannels=1;

	if (ms_filter_has_method(stream->ms.decoder, MS_AUDIO_DECODER_SET_RTP_PAYLOAD_PICKER) || ms_filter_has_method(stream->ms.decoder, MS_FILTER_SET_RTP_PAYLOAD_PICKER)) {
		ms_message("Decoder has FEC capabilities");
		picker_context.filter_graph_manager=stream;
		picker_context.picker=&audio_stream_payload_picker;
		ms_filter_call_method(stream->ms.decoder,MS_AUDIO_DECODER_SET_RTP_PAYLOAD_PICKER, &picker_context);
	}
	if ((stream->features & AUDIO_STREAM_FEATURE_VOL_SND) != 0)
		stream->volsend=ms_filter_new(MS_VOLUME_ID);
	else
		stream->volsend=NULL;
	if ((stream->features & AUDIO_STREAM_FEATURE_VOL_RCV) != 0)
		stream->volrecv=ms_filter_new(MS_VOLUME_ID);
	else
		stream->volrecv=NULL;
	audio_stream_enable_echo_limiter(stream,stream->el_type);
	audio_stream_enable_noise_gate(stream,stream->use_ng);

	if (ms_filter_implements_interface(stream->soundread,MSFilterPlayerInterface) && infile){
		audio_stream_play(stream,infile);
	}
	if (ms_filter_implements_interface(stream->soundwrite,MSFilterRecorderInterface) && outfile){
		audio_stream_record(stream,outfile);
	}

	if (stream->use_agc){
		int tmp=1;
		if (stream->volsend==NULL)
			stream->volsend=ms_filter_new(MS_VOLUME_ID);
		ms_filter_call_method(stream->volsend,MS_VOLUME_ENABLE_AGC,&tmp);
	}

	if (stream->dtmfgen) {
		ms_filter_call_method(stream->dtmfgen,MS_FILTER_SET_SAMPLE_RATE,&sample_rate);
		ms_filter_call_method(stream->dtmfgen,MS_FILTER_SET_NCHANNELS,&nchannels);
	}
	if (stream->dtmfgen_rtp) {
		ms_filter_call_method(stream->dtmfgen_rtp,MS_FILTER_SET_SAMPLE_RATE,&sample_rate);
		ms_filter_call_method(stream->dtmfgen_rtp,MS_FILTER_SET_NCHANNELS,&nchannels);
	}
	/* give the sound filters some properties */
	if (ms_filter_call_method(stream->soundread,MS_FILTER_SET_SAMPLE_RATE,&sample_rate) != 0) {
		/* need to add resampler*/
		if (stream->read_resampler == NULL) stream->read_resampler=ms_filter_new(MS_RESAMPLE_ID);
	}
	ms_filter_call_method(stream->soundread,MS_FILTER_SET_NCHANNELS,&nchannels);

	if (ms_filter_call_method(stream->soundwrite,MS_FILTER_SET_SAMPLE_RATE,&sample_rate) != 0) {
		/* need to add resampler*/
		if (stream->write_resampler == NULL) stream->write_resampler=ms_filter_new(MS_RESAMPLE_ID);
	}
	ms_filter_call_method(stream->soundwrite,MS_FILTER_SET_NCHANNELS,&nchannels);

	if (stream->ec){
		if (!stream->is_ec_delay_set){
			int delay_ms=ms_snd_card_get_minimal_latency(captcard);
			if (delay_ms!=0){
				ms_message("Setting echo canceller delay with value provided by soundcard: %i ms",delay_ms);
			}
			ms_filter_call_method(stream->ec,MS_ECHO_CANCELLER_SET_DELAY,&delay_ms);
		}else {
			ms_message("Setting echo canceller delay with value configured by application.");
		}
		ms_filter_call_method(stream->ec,MS_FILTER_SET_SAMPLE_RATE,&sample_rate);
	}

	if (stream->features & AUDIO_STREAM_FEATURE_MIXED_RECORDING || stream->features & AUDIO_STREAM_FEATURE_REMOTE_PLAYING){
		stream->outbound_mixer=ms_filter_new(MS_AUDIO_MIXER_ID);
	}

	if (stream->features & AUDIO_STREAM_FEATURE_MIXED_RECORDING) setup_recorder(stream,sample_rate,nchannels);

	/* give the encoder/decoder some parameters*/
	ms_filter_call_method(stream->ms.encoder,MS_FILTER_SET_SAMPLE_RATE,&sample_rate);
	if (stream->ms.target_bitrate<=0) {
		ms_message("target bitrate not set for stream [%p] using payload's bitrate is %i",stream,stream->ms.target_bitrate=pt->normal_bitrate);
	}
	if (stream->ms.target_bitrate>0){
		ms_message("Setting audio encoder network bitrate to [%i] on stream [%p]",stream->ms.target_bitrate,stream);
		ms_filter_call_method(stream->ms.encoder,MS_FILTER_SET_BITRATE,&stream->ms.target_bitrate);
	}
	rtp_session_set_target_upload_bandwidth(rtps, stream->ms.target_bitrate);
	ms_filter_call_method(stream->ms.encoder,MS_FILTER_SET_NCHANNELS,&nchannels);
	ms_filter_call_method(stream->ms.decoder,MS_FILTER_SET_SAMPLE_RATE,&sample_rate);
	ms_filter_call_method(stream->ms.decoder,MS_FILTER_SET_NCHANNELS,&nchannels);

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
			ms_filter_call_method(stream->equalizer,MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
		}
	}else
		stream->equalizer=NULL;

	/*configure resamplers if needed*/
	if (stream->read_resampler){
		audio_stream_configure_resampler(stream->read_resampler,stream->soundread,stream->ms.encoder,8000,pt->clock_rate);
	}

	if (stream->write_resampler){
		audio_stream_configure_resampler(stream->write_resampler,stream->ms.decoder,stream->soundwrite,pt->clock_rate,8000);
	}

	if (stream->ms.rc_enable){
		switch (stream->ms.rc_algorithm){
		case MSQosAnalyzerAlgorithmSimple:
			stream->ms.rc=ms_audio_bitrate_controller_new(stream->ms.sessions.rtp_session,stream->ms.encoder,0);
			break;
		case MSQosAnalyzerAlgorithmStateful:
			stream->ms.rc=ms_bandwidth_bitrate_controller_new(stream->ms.sessions.rtp_session,stream->ms.encoder, NULL, NULL);
			break;
		}
	}

	/* Create generic PLC if not handled by the decoder directly*/
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
			ms_filter_call_method(stream->plc, MS_FILTER_SET_NCHANNELS, &nchannels);
			ms_filter_call_method(stream->plc, MS_FILTER_SET_SAMPLE_RATE, &sample_rate);
		}
	} else {
		stream->plc = NULL;
	}

	if (stream->features & AUDIO_STREAM_FEATURE_LOCAL_PLAYING){
		stream->local_mixer=ms_filter_new(MS_AUDIO_MIXER_ID);
	}

	if (stream->outbound_mixer){
		ms_filter_call_method(stream->outbound_mixer,MS_FILTER_SET_SAMPLE_RATE,&sample_rate);
		ms_filter_call_method(stream->outbound_mixer,MS_FILTER_SET_NCHANNELS,&nchannels);
	}

	/* create ticker */
	if (stream->ms.sessions.ticker==NULL) media_stream_start_ticker(&stream->ms);
	if (stream->ms.state==MSStreamPreparing){
		/*we were using the dummy preload graph, destroy it but keep sound filters*/
		_audio_stream_unprepare_sound(stream,TRUE);
	}

	/* and then connect all */
	/* tip: draw yourself the picture if you don't understand */

	/*sending graph*/
	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h,stream->soundread,-1,0);
	if (stream->read_resampler)
		ms_connection_helper_link(&h,stream->read_resampler,0,0);
	if( stream->equalizer && stream->eq_loc == MSEqualizerMic )
		ms_connection_helper_link(&h,stream->equalizer, 0, 0);
	if (stream->ec)
		ms_connection_helper_link(&h,stream->ec,1,1);
	if (stream->volsend)
		ms_connection_helper_link(&h,stream->volsend,0,0);
	if (stream->dtmfgen_rtp)
		ms_connection_helper_link(&h,stream->dtmfgen_rtp,0,0);
	if (stream->outbound_mixer)
		ms_connection_helper_link(&h,stream->outbound_mixer,0,0);
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
	if (stream->equalizer && stream->eq_loc == MSEqualizerHP)
		ms_connection_helper_link(&h,stream->equalizer,0,0);
	if (stream->local_mixer){
		ms_connection_helper_link(&h,stream->local_mixer,0,0);
		setup_local_player(stream,sample_rate, nchannels);
	}
	if (stream->ec)
		ms_connection_helper_link(&h,stream->ec,0,0);
	if (stream->write_resampler)
		ms_connection_helper_link(&h,stream->write_resampler,0,0);
	ms_connection_helper_link(&h,stream->soundwrite,0,-1);

	/*call recording part, attached to both outgoing and incoming graphs*/
	if (stream->av_recorder.recorder)
		plumb_av_recorder(stream);
	if (stream->recorder){
		ms_filter_link(stream->outbound_mixer,1,stream->recorder_mixer,0);
		ms_filter_link(stream->recv_tee,1,stream->recorder_mixer,1);
		ms_filter_link(stream->recorder_mixer,0,stream->recorder,0);
	}

	/*to make sure all preprocess are done before befre processing audio*/
	ms_ticker_attach_multiple(stream->ms.sessions.ticker
				,stream->soundread
				,stream->ms.rtprecv
				,NULL);

	stream->ms.start_time=stream->ms.last_packet_time=ms_time(NULL);
	stream->ms.is_beginning=TRUE;
	stream->ms.state=MSStreamStarted;

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
				int fallback_to_rate=8000;
				ms_filter_call_method(st->ms.rtpsend,MS_FILTER_GET_SAMPLE_RATE,&fallback_to_rate);
				audio_stream_configure_resampler(st->read_resampler,st->soundread,st->ms.encoder, 8000, fallback_to_rate);
			}
			ms_filter_call_method_noarg(st->soundread,MS_FILE_PLAYER_START);
		}
	}else{
		ms_error("Cannot play file: the stream hasn't been started with"
		" audio_stream_start_with_files");
	}
}

MSFilter * audio_stream_get_local_player(AudioStream *st) {
	return st->local_player;
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

static MSFilter *get_recorder(AudioStream *stream){
	const char *fname=stream->recorder_file;
	int len=strlen(fname);

	if (strstr(fname,".mkv")==fname+len-4){
		if (stream->av_recorder.recorder){
			return stream->av_recorder.recorder;
		}else{
			ms_error("Cannot record in mkv format, not supported in this build.");
			return NULL;
		}
	}
	return stream->recorder;
}

int audio_stream_mixed_record_start(AudioStream *st){
	if (st->recorder && st->recorder_file){
		int pin=1;
		MSRecorderState state;
		MSAudioMixerCtl mctl={0};
		MSFilter *recorder=get_recorder(st);

		if (recorder==NULL) return -1;
		ms_filter_call_method(recorder,MS_RECORDER_GET_STATE,&state);
		if (state==MSRecorderClosed){
			if (ms_filter_call_method(recorder,MS_RECORDER_OPEN,st->recorder_file)==-1)
				return -1;
		}
		ms_filter_call_method_noarg(recorder,MS_RECORDER_START);
		ms_filter_call_method(st->recv_tee,MS_TEE_UNMUTE,&pin);
		mctl.pin=pin;
		mctl.param.enabled=TRUE;
		ms_filter_call_method(st->outbound_mixer,MS_AUDIO_MIXER_ENABLE_OUTPUT,&mctl);
		return 0;
	}
	return -1;
}

int audio_stream_mixed_record_stop(AudioStream *st){
	if (st->recorder && st->recorder_file){
		int pin=1;
		MSFilter *recorder=get_recorder(st);
		MSAudioMixerCtl mctl={0};

		if (recorder==NULL) return -1;
		ms_filter_call_method(st->recv_tee,MS_TEE_MUTE,&pin);
		mctl.pin=pin;
		mctl.param.enabled=FALSE;
		ms_filter_call_method(st->outbound_mixer,MS_AUDIO_MIXER_ENABLE_OUTPUT,&mctl);
		ms_filter_call_method_noarg(recorder,MS_RECORDER_PAUSE);
		ms_filter_call_method_noarg(recorder,MS_RECORDER_CLOSE);
	}
	return 0;
}

uint32_t audio_stream_get_features(AudioStream *st){
	return st->features;
}

void audio_stream_set_features(AudioStream *st, uint32_t features){
	st->features = features;
}

AudioStream *audio_stream_new_with_sessions(const MSMediaStreamSessions *sessions){
	AudioStream *stream=(AudioStream *)ms_new0(AudioStream,1);
	MSFilterDesc *ec_desc=ms_filter_lookup_by_name("MSWebRTCAEC");
	const OrtpRtcpXrMediaCallbacks rtcp_xr_media_cbs = {
		audio_stream_get_rtcp_xr_plc_status,
		audio_stream_get_rtcp_xr_signal_level,
		audio_stream_get_rtcp_xr_noise_level,
		audio_stream_get_rtcp_xr_average_quality_rating,
		audio_stream_get_rtcp_xr_average_lq_quality_rating,
		(unsigned long)stream
	};

	ms_filter_enable_statistics(TRUE);
	ms_filter_reset_statistics();


	stream->ms.type = MSAudio;
	stream->ms.sessions=*sessions;
	rtp_session_resync(stream->ms.sessions.rtp_session);
	/*some filters are created right now to allow configuration by the application before start() */
	stream->ms.rtpsend=ms_filter_new(MS_RTP_SEND_ID);
	stream->ms.ice_check_list=NULL;
	stream->ms.qi=ms_quality_indicator_new(stream->ms.sessions.rtp_session);
	ms_quality_indicator_set_label(stream->ms.qi,"audio");
	stream->ms.process_rtcp=audio_stream_process_rtcp;
	if (ec_desc!=NULL){
		stream->ec=ms_filter_new_from_desc(ec_desc);
	}else{
		stream->ec=ms_filter_new(MS_SPEEX_EC_ID);
	}
	stream->ms.evq=ortp_ev_queue_new();
	rtp_session_register_event_queue(stream->ms.sessions.rtp_session,stream->ms.evq);
	stream->play_dtmfs=TRUE;
	stream->use_gc=FALSE;
	stream->use_agc=FALSE;
	stream->use_ng=FALSE;
	stream->features=AUDIO_STREAM_FEATURE_ALL;

	rtp_session_set_rtcp_xr_media_callbacks(stream->ms.sessions.rtp_session, &rtcp_xr_media_cbs);

	return stream;
}

AudioStream *audio_stream_new(int loc_rtp_port, int loc_rtcp_port, bool_t ipv6){
	AudioStream *obj;
	MSMediaStreamSessions sessions={0};
	sessions.rtp_session=create_duplex_rtpsession(loc_rtp_port,loc_rtcp_port,ipv6);
	obj=audio_stream_new_with_sessions(&sessions);
	obj->ms.owns_sessions=TRUE;
	return obj;
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

static void dismantle_local_player(AudioStream *stream){
	MSConnectionHelper cnx;
	ms_connection_helper_start(&cnx);
	ms_connection_helper_unlink(&cnx,stream->local_player,-1,0);
	ms_connection_helper_unlink(&cnx,stream->local_player_resampler,0,0);
	ms_connection_helper_unlink(&cnx,stream->local_mixer,1,-1);
}

void audio_stream_stop(AudioStream * stream){
	if (stream->ms.sessions.ticker){
		MSConnectionHelper h;

		if (stream->ms.state==MSStreamPreparing){
			audio_stream_unprepare_sound(stream);
		}else if (stream->ms.state==MSStreamStarted){
			stream->ms.state=MSStreamStopped;
			ms_ticker_detach(stream->ms.sessions.ticker,stream->soundread);
			ms_ticker_detach(stream->ms.sessions.ticker,stream->ms.rtprecv);

			if (stream->ms.ice_check_list != NULL) {
				ice_check_list_print_route(stream->ms.ice_check_list, "Audio session's route");
				stream->ms.ice_check_list = NULL;
			}
			rtp_stats_display(rtp_session_get_stats(stream->ms.sessions.rtp_session),
				"             AUDIO SESSION'S RTP STATISTICS                ");

			/*dismantle the outgoing graph*/
			ms_connection_helper_start(&h);
			ms_connection_helper_unlink(&h,stream->soundread,-1,0);
			if (stream->read_resampler!=NULL)
				ms_connection_helper_unlink(&h,stream->read_resampler,0,0);
			if( stream->equalizer && stream->eq_loc == MSEqualizerMic)
				ms_connection_helper_unlink(&h, stream->equalizer, 0,0);
			if (stream->ec!=NULL)
				ms_connection_helper_unlink(&h,stream->ec,1,1);
			if (stream->volsend!=NULL)
				ms_connection_helper_unlink(&h,stream->volsend,0,0);
			if (stream->dtmfgen_rtp)
				ms_connection_helper_unlink(&h,stream->dtmfgen_rtp,0,0);
			if (stream->outbound_mixer)
				ms_connection_helper_unlink(&h,stream->outbound_mixer,0,0);
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
			if (stream->equalizer!=NULL && stream->eq_loc == MSEqualizerHP)
				ms_connection_helper_unlink(&h,stream->equalizer,0,0);
			if (stream->local_mixer){
				ms_connection_helper_unlink(&h,stream->local_mixer,0,0);
				dismantle_local_player(stream);
			}
			if (stream->ec!=NULL)
				ms_connection_helper_unlink(&h,stream->ec,0,0);
			if (stream->write_resampler!=NULL)
				ms_connection_helper_unlink(&h,stream->write_resampler,0,0);
			ms_connection_helper_unlink(&h,stream->soundwrite,0,-1);

			/*dismantle the call recording */
			if (stream->av_recorder.recorder)
				unplumb_av_recorder(stream);
			if (stream->recorder){
				ms_filter_unlink(stream->outbound_mixer,1,stream->recorder_mixer,0);
				ms_filter_unlink(stream->recv_tee,1,stream->recorder_mixer,1);
				ms_filter_unlink(stream->recorder_mixer,0,stream->recorder,0);
			}
			/*dismantle the remote play part*/
			close_av_player(stream);
		}
	}
	rtp_session_set_rtcp_xr_media_callbacks(stream->ms.sessions.rtp_session, NULL);
	rtp_session_signal_disconnect_by_callback(stream->ms.sessions.rtp_session,"telephone-event",(RtpCallback)on_dtmf_received);
	rtp_session_signal_disconnect_by_callback(stream->ms.sessions.rtp_session,"payload_type_changed",(RtpCallback)mediastream_payload_type_changed);
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

float audio_stream_get_lq_quality_rating(AudioStream *stream) {
	return media_stream_get_lq_quality_rating(&stream->ms);
}

float audio_stream_get_average_lq_quality_rating(AudioStream *stream) {
	return media_stream_get_average_lq_quality_rating(&stream->ms);
}

void audio_stream_enable_zrtp(AudioStream *stream, OrtpZrtpParams *params){
	if (stream->ms.sessions.zrtp_context==NULL)
		stream->ms.sessions.zrtp_context=ortp_zrtp_context_new(stream->ms.sessions.rtp_session, params);
	else if (!stream->ms.sessions.is_secured)
		ortp_zrtp_reset_transmition_timer(stream->ms.sessions.zrtp_context,stream->ms.sessions.rtp_session);
}

bool_t audio_stream_zrtp_enabled(const AudioStream *stream) {
	return stream->ms.sessions.zrtp_context!=NULL;
}

static void configure_av_recorder(AudioStream *stream){
	if (stream->av_recorder.video_input && stream->av_recorder.recorder){
		MSPinFormat pinfmt={0};
		ms_filter_call_method(stream->av_recorder.video_input,MS_FILTER_GET_OUTPUT_FMT,&pinfmt);
		if (pinfmt.fmt){
			ms_message("Configuring av recorder with video format %s",ms_fmt_descriptor_to_string(pinfmt.fmt));
			pinfmt.pin=0;
			ms_filter_call_method(stream->av_recorder.recorder,MS_FILTER_SET_INPUT_FMT,&pinfmt);
		}
	}
}

void audio_stream_link_video(AudioStream *stream, VideoStream *video){
	stream->videostream=video;
	if (stream->av_recorder.video_input && video->itcsink){
		ms_message("audio_stream_link_video() connecting itc filters");
		ms_filter_call_method(video->itcsink,MS_ITC_SINK_CONNECT,stream->av_recorder.video_input);
		configure_av_recorder(stream);
	}
}

void audio_stream_unlink_video(AudioStream *stream, VideoStream *video){
	stream->videostream=NULL;
	if (stream->av_recorder.video_input && video->itcsink){
		ms_filter_call_method(video->itcsink,MS_ITC_SINK_CONNECT,NULL);
	}
}




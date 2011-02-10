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


#ifndef MEDIASTREAM_H
#define MEDIASTREAM_H

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/mssndcard.h"
#include "mediastreamer2/mswebcam.h"
#include "mediastreamer2/msvideo.h"
#include "ortp/ortp.h"
#include "ortp/event.h"




typedef enum EchoLimiterType{
	ELInactive,
	ELControlMic,
	ELControlFull
} EchoLimiterType;

struct _AudioStream
{
	MSTicker *ticker;
	RtpSession *session;
	MSFilter *soundread;
	MSFilter *soundwrite;
	MSFilter *encoder;
	MSFilter *decoder;
	MSFilter *rtprecv;
	MSFilter *rtpsend;
	MSFilter *dtmfgen;
	MSFilter *dtmfgen_rtp;
	MSFilter *ec;/*echo canceler*/
	MSFilter *volsend,*volrecv; /*MSVolumes*/
	MSFilter *read_resampler;
	MSFilter *write_resampler;
	MSFilter *equalizer;
	uint64_t last_packet_count;
	time_t last_packet_time;
	EchoLimiterType el_type; /*use echo limiter: two MSVolume, measured input level controlling local output level*/
	int ec_tail_len; /*milliseconds*/
	int ec_delay;	/*milliseconds*/
	int ec_framesize; /* number of fft points */
	OrtpEvQueue *evq;
	bool_t play_dtmfs;
	bool_t use_gc;
	bool_t use_agc;
	bool_t eq_active;
	bool_t use_ng;/*noise gate*/
};

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _AudioStream AudioStream;

struct _RingStream
{
	MSTicker *ticker;
	MSFilter *source;
	MSFilter *gendtmf;
	MSFilter *write_resampler;
	MSFilter *sndwrite;
};

typedef struct _RingStream RingStream;



/* start a thread that does sampling->encoding->rtp_sending|rtp_receiving->decoding->playing */
MS2_PUBLIC AudioStream *audio_stream_start (RtpProfile * prof, int locport, const char *remip,
				 int remport, int payload_type, int jitt_comp, bool_t echo_cancel);

MS2_PUBLIC AudioStream *audio_stream_start_with_sndcards(RtpProfile * prof, int locport, const char *remip4, int remport, int payload_type, int jitt_comp, MSSndCard *playcard, MSSndCard *captcard, bool_t echocancel);

MS2_PUBLIC int audio_stream_start_with_files (AudioStream * stream, RtpProfile * prof,
					    const char *remip, int remport, int rem_rtcp_port,
					    int pt, int jitt_comp,
					    const char * infile,  const char * outfile);

MS2_PUBLIC int audio_stream_start_full(AudioStream *stream, RtpProfile *profile, const char *remip,int remport,
	int rem_rtcp_port, int payload,int jitt_comp, const char *infile, const char *outfile,
	MSSndCard *playcard, MSSndCard *captcard, bool_t use_ec);

MS2_PUBLIC void audio_stream_play(AudioStream *st, const char *name);
MS2_PUBLIC void audio_stream_record(AudioStream *st, const char *name);

MS2_PUBLIC void audio_stream_set_rtcp_information(AudioStream *st, const char *cname, const char *tool);

MS2_PUBLIC void audio_stream_play_received_dtmfs(AudioStream *st, bool_t yesno);

/* those two function do the same as audio_stream_start() but in two steps
this is useful to make sure that sockets are open before sending an invite;
or to start to stream only after receiving an ack.*/
MS2_PUBLIC AudioStream *audio_stream_new(int locport, bool_t ipv6);
MS2_PUBLIC int audio_stream_start_now(AudioStream * stream, RtpProfile * prof,  const char *remip, int remport, int rem_rtcp_port, int payload_type, int jitt_comp,MSSndCard *playcard, MSSndCard *captcard, bool_t echo_cancel);
MS2_PUBLIC void audio_stream_set_relay_session_id(AudioStream *stream, const char *relay_session_id);
/*returns true if we are still receiving some data from remote end in the last timeout seconds*/
MS2_PUBLIC bool_t audio_stream_alive(AudioStream * stream, int timeout);

/*enable echo-limiter dispositve: one MSVolume in input branch controls a MSVolume in the output branch*/
MS2_PUBLIC void audio_stream_enable_echo_limiter(AudioStream *stream, EchoLimiterType type);

/*enable gain control, to be done before start() */
MS2_PUBLIC void audio_stream_enable_gain_control(AudioStream *stream, bool_t val);

/*enable automatic gain control, to be done before start() */
MS2_PUBLIC void audio_stream_enable_automatic_gain_control(AudioStream *stream, bool_t val);

/*to be done before start */
MS2_PUBLIC void audio_stream_set_echo_canceller_params(AudioStream *st, int tail_len_ms, int delay_ms, int framesize);

MS2_PUBLIC void audio_stream_set_mic_gain(AudioStream *stream, float gain);

/* enable/disable rtp stream */ 
MS2_PUBLIC void audio_stream_mute_rtp(AudioStream *stream, bool_t val);

/*enable noise gate, must be done before start()*/
MS2_PUBLIC void audio_stream_enable_noise_gate(AudioStream *stream, bool_t val);

/*enable parametric equalizer in the stream that goes to the speaker*/
MS2_PUBLIC void audio_stream_enable_equalizer(AudioStream *stream, bool_t enabled);

MS2_PUBLIC void audio_stream_equalizer_set_gain(AudioStream *stream, int frequency, float gain, int freq_width);

/* stop the audio streaming thread and free everything*/
MS2_PUBLIC void audio_stream_stop (AudioStream * stream);

MS2_PUBLIC RingStream *ring_start (const char * file, int interval, MSSndCard *sndcard);
MS2_PUBLIC RingStream *ring_start_with_cb(const char * file, int interval, MSSndCard *sndcard, MSFilterNotifyFunc func, void * user_data);
MS2_PUBLIC void ring_stop (RingStream * stream);


/* send a dtmf */
MS2_PUBLIC int audio_stream_send_dtmf (AudioStream * stream, char dtmf);

MS2_PUBLIC void audio_stream_set_default_card(int cardindex);

/* retrieve RTP statistics*/
MS2_PUBLIC void audio_stream_get_local_rtp_stats(AudioStream *stream, rtp_stats_t *stats);


/*****************
  Video Support
 *****************/

typedef void (*VideoStreamRenderCallback)(void *user_pointer, const MSPicture *local_view, const MSPicture *remote_view);
typedef void (*VideoStreamEventCallback)(void *user_pointer, const MSFilter *f, const unsigned int event_id, const void *args);



typedef enum _VideoStreamDir{
	VideoStreamSendRecv,
	VideoStreamSendOnly,
	VideoStreamRecvOnly
}VideoStreamDir;

struct _VideoStream
{
	MSTicker *ticker;
	RtpSession *session;
	MSFilter *source;
	MSFilter *pixconv;
	MSFilter *sizeconv;
	MSFilter *tee;
	MSFilter *output;
	MSFilter *encoder;
	MSFilter *decoder;
	MSFilter *rtprecv;
	MSFilter *rtpsend;
	MSFilter *tee2;
	MSFilter *jpegwriter;
	MSFilter *output2;
	OrtpEvQueue *evq;
	MSVideoSize sent_vsize;
	int corner; /*for selfview*/
	VideoStreamRenderCallback rendercb;
	void *render_pointer;
	VideoStreamEventCallback eventcb;
	void *event_pointer;
	char *display_name;
	unsigned long window_id;
	unsigned long preview_window_id;
	VideoStreamDir dir;
	MSWebCam *cam;
	bool_t use_preview_window;
	bool_t adapt_bitrate;
};

typedef struct _VideoStream VideoStream;



MS2_PUBLIC VideoStream *video_stream_new(int locport, bool_t use_ipv6);
MS2_PUBLIC void video_stream_set_direction(VideoStream *vs, VideoStreamDir dir);
MS2_PUBLIC void video_stream_enable_adaptive_bitrate_control(VideoStream *s, bool_t yesno);
MS2_PUBLIC void video_stream_set_render_callback(VideoStream *s, VideoStreamRenderCallback cb, void *user_pointer);
MS2_PUBLIC void video_stream_set_event_callback(VideoStream *s, VideoStreamEventCallback cb, void *user_pointer);
MS2_PUBLIC void video_stream_set_display_filter_name(VideoStream *s, const char *fname);
MS2_PUBLIC int video_stream_start(VideoStream * stream, RtpProfile *profile, const char *remip, int remport, int rem_rtcp_port,
		int payload, int jitt_comp, MSWebCam *device);


MS2_PUBLIC void video_stream_set_relay_session_id(VideoStream *stream, const char *relay_session_id);
MS2_PUBLIC void video_stream_set_rtcp_information(VideoStream *st, const char *cname, const char *tool);
MS2_PUBLIC void video_stream_change_camera(VideoStream *stream, MSWebCam *cam);
/* Calling video_stream_set_sent_video_size() or changing the bitrate value in the used PayloadType during a stream is running does nothing.
The following function allows to take into account new parameters by redrawing the sending graph*/
MS2_PUBLIC void video_stream_update_video_params(VideoStream *stream);
/*function to call periodically to handle various events */
MS2_PUBLIC void video_stream_iterate(VideoStream *stream);
MS2_PUBLIC void video_stream_send_vfu(VideoStream *stream);
MS2_PUBLIC void video_stream_stop(VideoStream * stream);
MS2_PUBLIC void video_stream_set_sent_video_size(VideoStream *stream, MSVideoSize vsize);
MS2_PUBLIC void video_stream_enable_self_view(VideoStream *stream, bool_t val);
MS2_PUBLIC unsigned long video_stream_get_native_window_id(VideoStream *stream);
MS2_PUBLIC void video_stream_set_native_window_id(VideoStream *stream, unsigned long id);
MS2_PUBLIC void video_stream_set_native_preview_window_id(VideoStream *stream, unsigned long id);
MS2_PUBLIC unsigned long video_stream_get_native_preview_window_id(VideoStream *stream);
MS2_PUBLIC void video_stream_use_preview_video_window(VideoStream *stream, bool_t yesno);

/*provided for compatibility, use video_stream_set_direction() instead */
MS2_PUBLIC int video_stream_recv_only_start(VideoStream *videostream, RtpProfile *profile, const char *addr, int port, int used_pt, int jitt_comp);
MS2_PUBLIC int video_stream_send_only_start(VideoStream *videostream,
				RtpProfile *profile, const char *addr, int port, int rtcp_port, 
				int used_pt, int  jitt_comp, MSWebCam *device);
MS2_PUBLIC void video_stream_recv_only_stop(VideoStream *vs);
MS2_PUBLIC void video_stream_send_only_stop(VideoStream *vs);


/**
 * Small API to display a local preview window.
**/

typedef VideoStream VideoPreview;

MS2_PUBLIC VideoPreview * video_preview_new();
#define video_preview_set_size(p,s) 							video_stream_set_sent_video_size(p,s)
#define video_preview_set_display_filter_name(p,dt)	video_stream_set_display_filter_name(p,dt)
#define video_preview_set_native_window_id(p,id)		video_stream_set_native_preview_window_id (p,id)
#define video_preview_get_native_window_id(p)			video_stream_get_native_preview_window_id (p)
MS2_PUBLIC void video_preview_start(VideoPreview *stream, MSWebCam *device);
MS2_PUBLIC void video_preview_stop(VideoPreview *stream);

MS2_PUBLIC bool_t ms_is_ipv6(const char *address);

#ifdef __cplusplus
}
#endif


#endif

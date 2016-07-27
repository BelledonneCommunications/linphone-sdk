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


#ifndef MEDIASTREAM_H
#define MEDIASTREAM_H

#include <ortp/ortp.h>
#include <ortp/event.h>

#include <mediastreamer2/msfactory.h>
#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/msticker.h>
#include <mediastreamer2/mssndcard.h>
#include <mediastreamer2/mswebcam.h>
#include <mediastreamer2/msvideo.h>
#include <mediastreamer2/bitratecontrol.h>
#include <mediastreamer2/qualityindicator.h>
#include <mediastreamer2/ice.h>
#include <mediastreamer2/zrtp.h>
#include <mediastreamer2/dtls_srtp.h>
#include <mediastreamer2/ms_srtp.h>
#include <mediastreamer2/msequalizer.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup ring_api
 * @{
**/

struct _RingStream
{
	MSTicker *ticker;
	MSFilter *source;
	MSFilter *gendtmf;
	MSFilter *write_resampler;
	MSFilter *sndwrite;
	MSFilter *decoder;
	int srcpin;
};

typedef struct _RingStream RingStream;

MS2_PUBLIC RingStream *ring_start(MSFactory *factory, const char * file, int interval, MSSndCard *sndcard);
MS2_PUBLIC RingStream *ring_start_with_cb(MSFactory *factory, const char * file, int interval, MSSndCard *sndcard, MSFilterNotifyFunc func, void * user_data);
MS2_PUBLIC void ring_stop (RingStream * stream);

/**
 * @}
**/
/**
 * The MediaStream is an object describing a stream (one of AudioStream or VideoStream).
**/
typedef struct _MediaStream MediaStream;

/*
 * internal cb to process rtcp stream
 * */
typedef void (*media_stream_process_rtcp_callback_t)(MediaStream *stream, mblk_t *m);

struct _MSMediaStreamSessions{
	RtpSession *rtp_session;
	MSSrtpCtx* srtp_context;
	MSZrtpContext *zrtp_context;
	MSDtlsSrtpContext *dtls_context;
	MSTicker *ticker;
};

#ifndef MS_MEDIA_STREAM_SESSIONS_DEFINED
typedef struct _MSMediaStreamSessions MSMediaStreamSessions;
#define MS_MEDIA_STREAM_SESSIONS_DEFINED 1
#endif

MS2_PUBLIC void ms_media_stream_sessions_uninit(MSMediaStreamSessions *sessions);

typedef enum _MSStreamState{
	MSStreamInitialized,
	MSStreamPreparing,
	MSStreamStarted,
	MSStreamStopped
}MSStreamState;


typedef enum MediaStreamDir{
	MediaStreamSendRecv,
	MediaStreamSendOnly,
	MediaStreamRecvOnly
}MediaStreamDir;

/**
 * Base struct for both AudioStream and VideoStream structure.
**/
struct _MediaStream {
	MSFormatType type;
	MSStreamState state;
	MSMediaStreamSessions sessions;
	OrtpEvQueue *evq;
	MSFilter *rtprecv;
	MSFilter *rtpsend;
	MSFilter *encoder;
	MSFilter *decoder;
	MSFilter *voidsink;
	MSBitrateController *rc;
	MSQualityIndicator *qi;
	IceCheckList *ice_check_list;
	time_t start_time;
	time_t last_iterate_time;
	uint64_t last_packet_count;
	time_t last_packet_time;
	MSQosAnalyzerAlgorithm rc_algorithm;
	PayloadType *current_pt;/*doesn't need to be freed*/
	bool_t rc_enable;
	bool_t is_beginning;
	bool_t owns_sessions;
	bool_t pad;
	int dscp;
	/**
	 * defines encoder target network bit rate, uses #media_stream_set_target_network_bitrate() setter.
	 * */
	int target_bitrate;
	media_stream_process_rtcp_callback_t process_rtcp;
	OrtpEvDispatcher *evd;
	MSFactory *factory;
};

MS2_PUBLIC void media_stream_init(MediaStream *stream, MSFactory *factory, const MSMediaStreamSessions *sessions);


/**
 * @addtogroup audio_stream_api
 * @{
**/

MS2_PUBLIC bool_t media_stream_started(MediaStream *stream);

MS2_PUBLIC int media_stream_join_multicast_group(MediaStream *stream, const char *ip);

MS2_PUBLIC bool_t media_stream_dtls_supported(void);

/* enable DTLS on the media stream */
MS2_PUBLIC void media_stream_enable_dtls(MediaStream *stream, MSDtlsSrtpParams *params);

MS2_PUBLIC void media_stream_set_rtcp_information(MediaStream *stream, const char *cname, const char *tool);

MS2_PUBLIC void media_stream_get_local_rtp_stats(MediaStream *stream, rtp_stats_t *stats);

MS2_PUBLIC int media_stream_set_dscp(MediaStream *stream, int dscp);

MS2_PUBLIC void media_stream_enable_adaptive_bitrate_control(MediaStream *stream, bool_t enabled);

MS2_PUBLIC void media_stream_set_adaptive_bitrate_algorithm(MediaStream *stream, MSQosAnalyzerAlgorithm algorithm);

MS2_PUBLIC void media_stream_enable_adaptive_jittcomp(MediaStream *stream, bool_t enabled);

MS2_PUBLIC void media_stream_set_ice_check_list(MediaStream *stream, IceCheckList *cl);

/*
 * deprecated, use media_stream_set_srtp_recv_key and media_stream_set_srtp_send_key.
**/
MS2_PUBLIC bool_t media_stream_enable_srtp(MediaStream* stream, MSCryptoSuite suite, const char* snd_key, const char* rcv_key);

/**
 * @param[in] stream MediaStream object
 * @return true if stream is encrypted
 * */
MS2_PUBLIC bool_t media_stream_secured(const MediaStream *stream);
#define media_stream_is_secured media_stream_secured

/**
 * Tells whether AVPF is enabled or not.
 * @param[in] stream #MediaStream object.
 * @return True if AVPF is enabled, false otherwise.
 */
MS2_PUBLIC bool_t media_stream_avpf_enabled(const MediaStream *stream);

/**
 * Gets the AVPF Regular RTCP report interval.
 * @param[in] stream #MediaStream object.
 * @return The AVPF Regular RTCP report interval in seconds.
 */
MS2_PUBLIC uint16_t media_stream_get_avpf_rr_interval(const MediaStream *stream);

/**
 * Gets the RTP session of the media stream.
 * @param[in] stream #MediaStream object.
 * @return The RTP session of the media stream.
 */
MS2_PUBLIC RtpSession * media_stream_get_rtp_session(const MediaStream *stream);

MS2_PUBLIC const MSQualityIndicator *media_stream_get_quality_indicator(MediaStream *stream);
/* *
 * returns a realtime indicator of the stream quality between 0 and 5
 * */
MS2_PUBLIC float media_stream_get_quality_rating(MediaStream *stream);

MS2_PUBLIC float media_stream_get_average_quality_rating(MediaStream *stream);

MS2_PUBLIC float media_stream_get_lq_quality_rating(MediaStream *stream);

MS2_PUBLIC float media_stream_get_average_lq_quality_rating(MediaStream *stream);

/**
 * <br>For multirate codecs like OPUS, encoder output target bitrate must be set.
 * <br>Encoder will compute output codec bitrate from this value.
 * <br> default value is the value corresponding the rtp PayloadType
 * @param stream stream to apply parameter on
 * @param target_bitrate in bit per seconds
 * @return 0 if succeed
 * */
MS2_PUBLIC int media_stream_set_target_network_bitrate(MediaStream *stream,int target_bitrate);

/**
 * get the stream target bitrate.
 * @param stream stream to apply parameter on
 * @return target_bitrate in bit per seconds
 * */
MS2_PUBLIC int media_stream_get_target_network_bitrate(const MediaStream *stream);

/**
 * get current stream  upload bitrate. Value is updated every seconds
 * @param stream
 * @return bitrate in bit per seconds
 * */
MS2_PUBLIC float media_stream_get_up_bw(const MediaStream *stream);

/**
 * get current stream download bitrate. Value is updated every seconds
 * @param stream
 * @return bitrate in bit per seconds
 * */
MS2_PUBLIC float media_stream_get_down_bw(const MediaStream *stream);

/**
 * get current stream rtcp upload bitrate. Value is updated every seconds
 * @param stream
 * @return bitrate in bit per seconds
 * */
MS2_PUBLIC float media_stream_get_rtcp_up_bw(const MediaStream *stream);

/**
 * get current stream rtcp download bitrate. Value is updated every seconds
 * @param stream
 * @return bitrate in bit per seconds
 * */
MS2_PUBLIC float media_stream_get_rtcp_down_bw(const MediaStream *stream);

/**
 * Returns the sessions that were used in the media stream (RTP, SRTP, ZRTP...) so that they can be re-used.
 * As a result of calling this function, the media stream no longer owns the sessions and thus will not free them.
**/
MS2_PUBLIC void media_stream_reclaim_sessions(MediaStream *stream, MSMediaStreamSessions *sessions);


MS2_PUBLIC void media_stream_iterate(MediaStream * stream);

/**
 * Returns TRUE if stream was still actively receiving packets (RTP or RTCP) in the last period specified in timeout_seconds.
**/
MS2_PUBLIC bool_t media_stream_alive(MediaStream *stream, int timeout_seconds);

/**
 * @return current streams state
 * */
MS2_PUBLIC MSStreamState media_stream_get_state(const MediaStream *stream);

MS2_PUBLIC OrtpEvDispatcher* media_stream_get_event_dispatcher(const MediaStream *stream);

typedef enum EchoLimiterType{
	ELInactive,
	ELControlMic,
	ELControlFull
} EchoLimiterType;


typedef enum EqualizerLocation {
	MSEqualizerHP = 0,
	MSEqualizerMic
} EqualizerLocation;


typedef enum MSResourceType{
	MSResourceInvalid,
	MSResourceDefault,
	MSResourceFile,
	MSResourceRtp,
	MSResourceCamera,
	MSResourceSoundcard
}MSResourceType;

MS2_PUBLIC const char *ms_resource_type_to_string(MSResourceType type);

/**
 * Structure describing the input or the output of a MediaStream.
 * type must be set to one the member of the MSResourceType enum, and the correspoding
 * resource argument must be set: the file name (const char*) for MSResourceFile,
 * the RtpSession for MSResourceRtp, an MSWebCam for MSResourceCamera, an MSSndCard for MSResourceSoundcard.
 * @warning due to implementation, if RTP is to be used for input and output, the same RtpSession must be passed for both sides.
 */
typedef struct _MSMediaResource{
	MSResourceType type;
	union{
		void *resource_arg;
		const char *file;
		RtpSession *session;
		MSWebCam *camera;
		MSSndCard *soundcard;
	};
}MSMediaResource;


MS2_PUBLIC bool_t ms_media_resource_is_consistent(const MSMediaResource *r);
#define ms_media_resource_get_file(r)		(((r)->type == MSResourceFile) ? (r)->file : NULL)
#define ms_media_resource_get_rtp_session(r)	(((r)->type == MSResourceRtp) ? (r)->session : NULL)
#define ms_media_resource_get_camera(r)		(((r)->type == MSResourceCamera) ? (r)->camera : NULL)
#define ms_media_resource_get_soundcard(r)	(((r)->type == MSResourceSoundcard) ? (r)->souncard : NULL)
/**
 * Structure describing the input/output of a MediaStream.
 * Input and output are described as MSMediaResource.
 */
typedef struct _MSMediaStreamIO {
	MSMediaResource input;
	MSMediaResource output;
} MSMediaStreamIO;

#define MS_MEDIA_STREAM_IO_INITIALIZER { {0}, {0} }

MS2_PUBLIC bool_t ms_media_stream_io_is_consistent(const MSMediaStreamIO *io);

struct _AudioStream
{
	MediaStream ms;
	MSFilter *soundread;
	MSFilter *soundwrite;
	MSFilter *dtmfgen;
	MSFilter *dtmfgen_rtp;
	MSFilter *plc;
	MSFilter *ec;/*echo canceler*/
	MSFilter *volsend,*volrecv; /*MSVolumes*/
	MSFilter *local_mixer;
	MSFilter *local_player;
	MSFilter *local_player_resampler;
	MSFilter *read_decoder; /* Used when the input is done via RTP */
	MSFilter *write_encoder; /* Used when the output is done via RTP */
	MSFilter *read_resampler;
	MSFilter *write_resampler;
	MSFilter *mic_equalizer;
	MSFilter *spk_equalizer;
	MSFilter *dummy;
	MSFilter *recv_tee;
	MSFilter *recorder_mixer;
	MSFilter *recorder;
	MSFilter *outbound_mixer;
	struct {
		MSFilter *resampler;
		MSFilter *encoder;
		MSFilter *recorder;
		MSFilter *video_input;
	}av_recorder;
	struct _AVPlayer{
		MSFilter *player;
		MSFilter *resampler;
		MSFilter *decoder;
		MSFilter *video_output;
		int audiopin;
		int videopin;
		bool_t plumbed;
	}av_player;
	RtpSession *rtp_io_session; /**< The RTP session used for RTP input/output. */
	MSFilter *vaddtx;
	char *recorder_file;
	EchoLimiterType el_type; /*use echo limiter: two MSVolume, measured input level controlling local output level*/
	EqualizerLocation eq_loc;
	uint32_t features;
	int sample_rate;
	int nchannels;
	struct _VideoStream *videostream;/*the stream with which this audiostream is paired*/
	MSAudioRoute audio_route;
	bool_t play_dtmfs;
	bool_t use_ec;
	bool_t use_gc;
	bool_t use_agc;
	
	bool_t mic_eq_active;
	bool_t spk_eq_active;
	bool_t use_ng;/*noise gate*/
	bool_t is_ec_delay_set;
};

/**
 * The AudioStream holds all resources to create and run typical VoIP audiostream.
**/
typedef struct _AudioStream AudioStream;


/* start a thread that does sampling->encoding->rtp_sending|rtp_receiving->decoding->playing */
MS2_PUBLIC AudioStream *audio_stream_start(MSFactory* factory, RtpProfile * prof, int locport, const char *remip,
				 int remport, int payload_type, int jitt_comp, bool_t echo_cancel);

MS2_PUBLIC AudioStream *audio_stream_start_with_sndcards(MSFactory* factory, RtpProfile* prof, int locport, const char *remip4, int remport, int payload_type, int jitt_comp, MSSndCard *playcard,
														 MSSndCard *captcard, bool_t echocancel);

MS2_PUBLIC int audio_stream_start_with_files (AudioStream * stream, RtpProfile * prof,
						const char *remip, int remport, int rem_rtcp_port,
						int pt, int jitt_comp,
						const char * infile,  const char * outfile);

/**
 * Start an audio stream according to the specified AudioStreamIO.
 *
 * @param[in] stream AudioStream object previously created with audio_stream_new().
 * @param[in] profile RtpProfile object holding the PayloadType that can be used during the audio session.
 * @param[in] rem_rtp_ip The remote IP address where to send the encoded audio to.
 * @param[in] rem_rtp_port The remote port where to send the encoded audio to.
 * @param[in] rem_rtcp_ip The remote IP address for RTCP.
 * @param[in] rem_rtcp_port The remote port for RTCP.
 * @param[in] payload_type The payload type number used to send the audio stream. A valid PayloadType must be available at this index in the profile.
 * @param[in] io A MSMediaStreamIO describing the local input/output of the audio stream.
 */
MS2_PUBLIC int audio_stream_start_from_io(AudioStream *stream, RtpProfile *profile, const char *rem_rtp_ip, int rem_rtp_port,
	const char *rem_rtcp_ip, int rem_rtcp_port, int payload_type, const MSMediaStreamIO *io);

/**
 * Starts an audio stream from/to local wav files or soundcards.
 *
 * This method starts the processing of the audio stream, that is playing from wav file or soundcard, voice processing, encoding,
 * sending through RTP, receiving from RTP, decoding, voice processing and wav file recording or soundcard playback.
 *
 *
 * @param stream an AudioStream previously created with audio_stream_new().
 * @param profile a RtpProfile containing all PayloadType possible during the audio session.
 * @param rem_rtp_ip remote IP address where to send the encoded audio.
 * @param rem_rtp_port remote IP port where to send the encoded audio.
 * @param rem_rtcp_ip remote IP address for RTCP.
 * @param rem_rtcp_port remote port for RTCP.
 * @param payload payload type index to use for the sending stream. This index must point to a valid PayloadType in the RtpProfile.
 * @param jitt_comp Nominal jitter buffer size in milliseconds.
 * @param infile path to wav file to play out (can be NULL)
 * @param outfile path to wav file to record into (can be NULL)
 * @param playcard The soundcard to be used for playback (can be NULL)
 * @param captcard The soundcard to be used for catpure. (can be NULL)
 * @param use_ec whether echo cancellation is to be performed.
 * @return 0 if sucessful, -1 otherwise.
**/
MS2_PUBLIC int audio_stream_start_full(AudioStream *stream, RtpProfile *profile, const char *rem_rtp_ip,int rem_rtp_port,
	const char *rem_rtcp_ip, int rem_rtcp_port, int payload,int jitt_comp, const char *infile, const char *outfile,
	MSSndCard *playcard, MSSndCard *captcard, bool_t use_ec);


MS2_PUBLIC void audio_stream_play(AudioStream *st, const char *name);
MS2_PUBLIC void audio_stream_record(AudioStream *st, const char *name);

static MS2_INLINE void audio_stream_set_rtcp_information(AudioStream *st, const char *cname, const char *tool) {
	media_stream_set_rtcp_information(&st->ms, cname, tool);
}

MS2_PUBLIC void audio_stream_play_received_dtmfs(AudioStream *st, bool_t yesno);

/**
 * Creates an AudioStream object listening on a RTP port.
 * @param loc_rtp_port the local UDP port to listen for RTP packets.
 * @param loc_rtcp_port the local UDP port to listen for RTCP packets
 * @param ipv6 TRUE if ipv6 must be used.
 * @param factory 
 * @return a new AudioStream.
**/
MS2_PUBLIC AudioStream *audio_stream_new(MSFactory* factory, int loc_rtp_port, int loc_rtcp_port, bool_t ipv6);

/**
 * Creates an AudioStream object listening on a RTP port for a dedicated address.
 * @param loc_ip the local ip to listen for RTP packets. Can be ::, O.O.O.O or any ip4/6 addresses
 * @param loc_rtp_port the local UDP port to listen for RTP packets.
 * @param loc_rtcp_port the local UDP port to listen for RTCP packets
 * @param factory
 * @return a new AudioStream.
**/
MS2_PUBLIC AudioStream *audio_stream_new2(MSFactory* factory, const char* ip, int loc_rtp_port, int loc_rtcp_port);


/**Creates an AudioStream object from initialized MSMediaStreamSessions.
 * @param sessions the MSMediaStreamSessions
 * @param factory the MSFActory from the core object
 * @return a new AudioStream
**/
MS2_PUBLIC AudioStream *audio_stream_new_with_sessions(MSFactory* factory, const MSMediaStreamSessions *sessions);

#define AUDIO_STREAM_FEATURE_PLC 		(1 << 0)
#define AUDIO_STREAM_FEATURE_EC 		(1 << 1)
#define AUDIO_STREAM_FEATURE_EQUALIZER		(1 << 2)
#define AUDIO_STREAM_FEATURE_VOL_SND 		(1 << 3)
#define AUDIO_STREAM_FEATURE_VOL_RCV 		(1 << 4)
#define AUDIO_STREAM_FEATURE_DTMF		(1 << 5)
#define AUDIO_STREAM_FEATURE_DTMF_ECHO		(1 << 6)
#define AUDIO_STREAM_FEATURE_MIXED_RECORDING	(1 << 7)
#define AUDIO_STREAM_FEATURE_LOCAL_PLAYING	(1 << 8)
#define AUDIO_STREAM_FEATURE_REMOTE_PLAYING	(1 << 9)

#define AUDIO_STREAM_FEATURE_ALL	(\
					AUDIO_STREAM_FEATURE_PLC | \
					AUDIO_STREAM_FEATURE_EC | \
					AUDIO_STREAM_FEATURE_EQUALIZER | \
					AUDIO_STREAM_FEATURE_VOL_SND | \
					AUDIO_STREAM_FEATURE_VOL_RCV | \
					AUDIO_STREAM_FEATURE_DTMF | \
					AUDIO_STREAM_FEATURE_DTMF_ECHO |\
					AUDIO_STREAM_FEATURE_MIXED_RECORDING |\
					AUDIO_STREAM_FEATURE_LOCAL_PLAYING | \
					AUDIO_STREAM_FEATURE_REMOTE_PLAYING \
					)


MS2_PUBLIC uint32_t audio_stream_get_features(AudioStream *st);
MS2_PUBLIC void audio_stream_set_features(AudioStream *st, uint32_t features);

MS2_PUBLIC void audio_stream_prepare_sound(AudioStream *st, MSSndCard *playcard, MSSndCard *captcard);
MS2_PUBLIC void audio_stream_unprepare_sound(AudioStream *st);
MS2_PUBLIC bool_t audio_stream_started(AudioStream *stream);
/**
 * Starts an audio stream from local soundcards.
 *
 * This method starts the processing of the audio stream, that is capture from soundcard, voice processing, encoding,
 * sending through RTP, receiving from RTP, decoding, voice processing and soundcard playback.
 *
 * @param stream an AudioStream previously created with audio_stream_new().
 * @param prof a RtpProfile containing all PayloadType possible during the audio session.
 * @param remip remote IP address where to send the encoded audio.
 * @param remport remote IP port where to send the encoded audio
 * @param rem_rtcp_port remote port for RTCP.
 * @param payload_type payload type index to use for the sending stream. This index must point to a valid PayloadType in the RtpProfile.
 * @param jitt_comp Nominal jitter buffer size in milliseconds.
 * @param playcard The soundcard to be used for playback
 * @param captcard The soundcard to be used for catpure.
 * @param echo_cancel whether echo cancellation is to be performed.
**/
MS2_PUBLIC int audio_stream_start_now(AudioStream * stream, RtpProfile * prof,  const char *remip, int remport, int rem_rtcp_port, int payload_type, int jitt_comp,MSSndCard *playcard, MSSndCard *captcard, bool_t echo_cancel);
MS2_PUBLIC void audio_stream_set_relay_session_id(AudioStream *stream, const char *relay_session_id);
/*returns true if we are still receiving some data from remote end in the last timeout seconds*/
MS2_PUBLIC bool_t audio_stream_alive(AudioStream * stream, int timeout);

/**
 * Executes background low priority tasks related to audio processing (RTP statistics analysis).
 * It should be called periodically, for example with an interval of 100 ms or so.
 */
MS2_PUBLIC void audio_stream_iterate(AudioStream *stream);

/**
 * enable echo-limiter dispositve: one MSVolume in input branch controls a MSVolume in the output branch
 * */
MS2_PUBLIC void audio_stream_enable_echo_limiter(AudioStream *stream, EchoLimiterType type);

/**
 * enable gain control, to be done before start()
 * */
MS2_PUBLIC void audio_stream_enable_gain_control(AudioStream *stream, bool_t val);

/**
 * enable automatic gain control, to be done before start()
 * */
MS2_PUBLIC void audio_stream_enable_automatic_gain_control(AudioStream *stream, bool_t val);

/**
 * to be done before start
 *  */
MS2_PUBLIC void audio_stream_set_echo_canceller_params(AudioStream *st, int tail_len_ms, int delay_ms, int framesize);


/**
 * to be done before start
 *  */
MS2_PUBLIC void audio_stream_enable_echo_canceller(AudioStream *st, bool_t enabled);
/**
 * enable adaptive rate control
 * */
static MS2_INLINE void audio_stream_enable_adaptive_bitrate_control(AudioStream *stream, bool_t enabled) {
	media_stream_enable_adaptive_bitrate_control(&stream->ms, enabled);
}

/**
 *  Enable adaptive jitter compensation
 *  */
static MS2_INLINE void audio_stream_enable_adaptive_jittcomp(AudioStream *stream, bool_t enabled) {
	media_stream_enable_adaptive_jittcomp(&stream->ms, enabled);
}

/**
 * Apply a software gain on the microphone.
 * Be note that this method neither changes the volume gain
 * of the sound card nor the system mixer one. If you intends
 * to control the volume gain at sound card level, you should
 * use audio_stream_set_sound_card_input_gain() instead.
 * 
 * @param stream The stream.
 * @param gain_db Gain to apply in dB.
 */
MS2_PUBLIC void audio_stream_set_mic_gain_db(AudioStream *stream, float gain_db);


/**
 * Like audio_stream_set_mic_gain_db() excepted that the gain is specified
 * in percentage.
 * 
 * @param stream The stream.
 * @param gain Gain to apply in percetage of the max supported gain.
 */
MS2_PUBLIC void audio_stream_set_mic_gain(AudioStream *stream, float gain);

/**
 *  enable/disable rtp stream
 */
MS2_PUBLIC void audio_stream_mute_rtp(AudioStream *stream, bool_t val);

/**
 * Apply a gain on received RTP packets.
 * @param stream An AudioStream.
 * @param gain_db Gain to apply in dB.
 */
MS2_PUBLIC void audio_stream_set_spk_gain_db(AudioStream *stream, float gain_db);

/**
 * Set microphone volume gain.
 * If the sound backend supports it, the set volume gain will be synchronized
 * with the host system mixer. If you intended to apply a static software gain,
 * you should use audio_stream_set_mic_gain_db() or audio_stream_set_mic_gain().
 * 
 * @param stream The audio stream.
 * @param gain Percentage of the max supported volume gain. Valid values are in [0.0 : 1.0].
 */
MS2_PUBLIC void audio_stream_set_sound_card_input_gain(AudioStream *stream, float gain);

/**
 * Get microphone volume gain.
 * @param stream The audio stream.
 * @return double Volume gain in percentage of the max suppored gain.
 * Valid returned values are in [0.0 : 1.0]. A negative value is returned in case of failure.
 */
MS2_PUBLIC float audio_stream_get_sound_card_input_gain(const AudioStream *stream);

/**
 * Set speaker volume gain.
 * If the sound backend supports it, the set volume gain will be synchronized
 * with the host system mixer.
 * @param stream The audio stream.
 * @param gain Percentage of the max supported volume gain. Valid values are in [0.0 : 1.0].
 */
MS2_PUBLIC void audio_stream_set_sound_card_output_gain(AudioStream *stream, float volume);

/**
 * Get speaker volume gain.
 * @param stream The audio stream.
 * @return Volume gain in percentage of the max suppored gain.
 * Valid returned values are in [0.0 : 1.0]. A negative value is returned in case of failure.
 */
MS2_PUBLIC float audio_stream_get_sound_card_output_gain(const AudioStream *stream);

/**
 * enable noise gate, must be done before start()
 * */
MS2_PUBLIC void audio_stream_enable_noise_gate(AudioStream *stream, bool_t val);

/**
 * Enable a parametric equalizer
 * @param[in] stream An AudioStream
 * @param[in] location Location of the equalizer to enable (speaker or microphone)
 * @param[in] enabled Whether the equalizer must be enabled
 */
MS2_PUBLIC void audio_stream_enable_equalizer(AudioStream *stream, EqualizerLocation location, bool_t enabled);

/**
 * Apply a gain on a given frequency band.
 * @param[in] stream An AudioStream
 * @param[in] location Location of the concerned equalizer (speaker or microphone)
 * @param[in] gain Description of the band and the gain to apply.
 */
MS2_PUBLIC void audio_stream_equalizer_set_gain(AudioStream *stream, EqualizerLocation location, const MSEqualizerGain *gain);

/**
 *  stop the audio streaming thread and free everything
 *  */
MS2_PUBLIC void audio_stream_stop (AudioStream * stream);

/**
 *  send a dtmf
 *  */
MS2_PUBLIC int audio_stream_send_dtmf (AudioStream * stream, char dtmf);

MS2_PUBLIC MSFilter *audio_stream_get_local_player(AudioStream *stream);

MS2_PUBLIC int audio_stream_mixed_record_open(AudioStream *st, const char*filename);

MS2_PUBLIC int audio_stream_mixed_record_start(AudioStream *st);

MS2_PUBLIC int audio_stream_mixed_record_stop(AudioStream *st);

/**
 * Open a player to play an audio/video file to remote end.
 * The player is returned as a MSFilter so that application can make usual player controls on it using the MSPlayerInterface.
**/
MS2_PUBLIC MSFilter * audio_stream_open_remote_play(AudioStream *stream, const char *filename);

MS2_PUBLIC void audio_stream_close_remote_play(AudioStream *stream);

MS2_PUBLIC void audio_stream_set_default_card(int cardindex);

/* retrieve RTP statistics*/
static MS2_INLINE void audio_stream_get_local_rtp_stats(AudioStream *stream, rtp_stats_t *stats) {
	media_stream_get_local_rtp_stats(&stream->ms, stats);
}

/* returns a realtime indicator of the stream quality between 0 and 5 */
MS2_PUBLIC float audio_stream_get_quality_rating(AudioStream *stream);

/* returns the quality rating as an average since the start of the streaming session.*/
MS2_PUBLIC float audio_stream_get_average_quality_rating(AudioStream *stream);

/* returns a realtime indicator of the listening quality of the stream between 0 and 5 */
MS2_PUBLIC float audio_stream_get_lq_quality_rating(AudioStream *stream);

/* returns the listening quality rating as an average since the start of the streaming session.*/
MS2_PUBLIC float audio_stream_get_average_lq_quality_rating(AudioStream *stream);

/* enable ZRTP on the audio stream */
MS2_PUBLIC void audio_stream_enable_zrtp(AudioStream *stream, MSZrtpParams *params);
MS2_PUBLIC void audio_stream_start_zrtp(AudioStream *stream);

/**
 * return TRUE if zrtp is enabled, it does not mean that stream is encrypted, but only that zrtp is configured to know encryption status, uses #
 * */
bool_t  audio_stream_zrtp_enabled(const AudioStream *stream);

/* enable SRTP on the audio stream */
static MS2_INLINE bool_t audio_stream_enable_srtp(AudioStream* stream, MSCryptoSuite suite, const char* snd_key, const char* rcv_key) {
	return media_stream_enable_srtp(&stream->ms, suite, snd_key, rcv_key);
}

static MS2_INLINE int audio_stream_set_dscp(AudioStream *stream, int dscp) {
	return media_stream_set_dscp(&stream->ms, dscp);
}

/**
 * Gets the RTP session of an audio stream.
 * @param[in] stream #MediaStream object.
 * @return The RTP session of the audio stream.
 */
static MS2_INLINE RtpSession * audio_stream_get_rtp_session(const AudioStream *stream) {
	return media_stream_get_rtp_session(&stream->ms);
}



/**
 * @}
**/


/**
 * @addtogroup video_stream_api
 * @{
**/

typedef void (*VideoStreamRenderCallback)(void *user_pointer, const MSPicture *local_view, const MSPicture *remote_view);
typedef void (*VideoStreamEventCallback)(void *user_pointer, const MSFilter *f, const unsigned int event_id, const void *args);

struct _MediastreamVideoStat
{
    int counter_rcvd_pli; /*Picture Loss Indication counter */
    int counter_rcvd_sli;/* Slice Loss Indication counter */
    int counter_rcvd_rpsi; /*Reference Picture Selection Indication */
    int counter_rcvd_fir; /* Full INTRA-frame Request */
};

typedef struct _MediastreamVideoStat MediaStreamVideoStat;

struct _VideoStream
{
	MediaStream ms;
	MSFilter *void_source;
	MSFilter *source;
	MSFilter *pixconv;
	MSFilter *sizeconv;
	MSFilter *tee;
	MSFilter *output;
	MSFilter *tee2;
	MSFilter *jpegwriter;
	MSFilter *output2;
	MSFilter *tee3;
	MSFilter *recorder_output; /*can be an ItcSink to send video to the audiostream's multimedia recorder, or directly a MkvRecorder */
	MSFilter *local_jpegwriter;
	MSVideoSize sent_vsize;
	MSVideoSize preview_vsize;
	float fps; /*the target fps explicitely set by application, overrides internally selected fps*/
	float configured_fps; /*the fps that was configured to the encoder. It might be different from the one really obtained from camera.*/
	int corner; /*for selfview*/
	VideoStreamRenderCallback rendercb;
	void *render_pointer;
	VideoStreamEventCallback eventcb;
	void *event_pointer;
	char *display_name;
	void *window_id;
	void *preview_window_id;
	MediaStreamDir dir;
	MSWebCam *cam;
	RtpSession *rtp_io_session; /**< The RTP session used for RTP input/output. */
	char *preset;
	int device_orientation; /* warning: meaning of this variable depends on the platform (Android, iOS, ...) */
	uint64_t last_reported_decoding_error_time;
	uint64_t last_fps_check;
	MediaStreamVideoStat ms_video_stat;
	bool_t use_preview_window;
	bool_t freeze_on_error;
	bool_t display_filter_auto_rotate_enabled;
	bool_t source_performs_encoding;
	
	bool_t output_performs_decoding;
	bool_t player_active;
	bool_t staticimage_webcam_fps_optimization; /* if TRUE, the StaticImage webcam will ignore the fps target in order to save CPU time. Default is TRUE */
	
};

typedef struct _VideoStream VideoStream;


    
MS2_PUBLIC VideoStream *video_stream_new(MSFactory* factory, int loc_rtp_port, int loc_rtcp_port, bool_t use_ipv6);
/**
 * Creates a VideoStream object listening on a RTP port for a dedicated address.
 * @param loc_ip the local ip to listen for RTP packets. Can be ::, O.O.O.O or any ip4/6 addresses
 * @param [in] loc_rtp_port the local UDP port to listen for RTP packets.
 * @param [in] loc_rtcp_port the local UDP port to listen for RTCP packets
 * @return a new VideoStream.
**/
MS2_PUBLIC VideoStream *video_stream_new2(MSFactory* factory, const char* ip, int loc_rtp_port, int loc_rtcp_port);

MS2_PUBLIC VideoStream *video_stream_new_with_sessions(MSFactory* factory, const MSMediaStreamSessions *sessions);
MS2_PUBLIC void video_stream_set_direction(VideoStream *vs, MediaStreamDir dir);
static MS2_INLINE void video_stream_enable_adaptive_bitrate_control(VideoStream *stream, bool_t enabled) {
	media_stream_enable_adaptive_bitrate_control(&stream->ms, enabled);
}
static MS2_INLINE void video_stream_enable_adaptive_jittcomp(VideoStream *stream, bool_t enabled) {
	media_stream_enable_adaptive_jittcomp(&stream->ms, enabled);
}
MS2_PUBLIC void video_stream_set_render_callback(VideoStream *s, VideoStreamRenderCallback cb, void *user_pointer);
MS2_PUBLIC void video_stream_set_event_callback(VideoStream *s, VideoStreamEventCallback cb, void *user_pointer);
MS2_PUBLIC void video_stream_set_display_filter_name(VideoStream *s, const char *fname);
MS2_PUBLIC int video_stream_start_with_source(VideoStream *stream, RtpProfile *profile, const char *rem_rtp_ip, int rem_rtp_port,
		const char *rem_rtcp_ip, int rem_rtcp_port, int payload, int jitt_comp, MSWebCam* cam, MSFilter* source);
MS2_PUBLIC int video_stream_start(VideoStream * stream, RtpProfile *profile, const char *rem_rtp_ip, int rem_rtp_port, const char *rem_rtcp_ip, 
				  int rem_rtcp_port, int payload, int jitt_comp, MSWebCam *device);
MS2_PUBLIC int video_stream_start_with_files(VideoStream *stream, RtpProfile *profile, const char *rem_rtp_ip, int rem_rtp_port,
        const char *rem_rtcp_ip, int rem_rtcp_port, int payload_type, const char *play_file, const char *record_file);

/**
 * Start a video stream according to the specified VideoStreamIO.
 *
 * @param[in] stream VideoStream object previously created with video_stream_new().
 * @param[in] profile RtpProfile object holding the PayloadType that can be used during the video session.
 * @param[in] rem_rtp_ip The remote IP address where to send the encoded video to.
 * @param[in] rem_rtp_port The remote port where to send the encoded video to.
 * @param[in] rem_rtcp_ip The remote IP address for RTCP.
 * @param[in] rem_rtcp_port The remote port for RTCP.
 * @param[in] payload_type The payload type number used to send the video stream. A valid PayloadType must be available at this index in the profile.
 * @param[in] io A VideoStreamIO describing the input/output of the video stream.
 */
MS2_PUBLIC int video_stream_start_from_io(VideoStream *stream, RtpProfile *profile, const char *rem_rtp_ip, int rem_rtp_port,
	const char *rem_rtcp_ip, int rem_rtcp_port, int payload_type, const MSMediaStreamIO *io);

MS2_PUBLIC void video_stream_prepare_video(VideoStream *stream);
MS2_PUBLIC void video_stream_unprepare_video(VideoStream *stream);


MS2_PUBLIC void video_stream_set_relay_session_id(VideoStream *stream, const char *relay_session_id);
static MS2_INLINE void video_stream_set_rtcp_information(VideoStream *st, const char *cname, const char *tool) {
	media_stream_set_rtcp_information(&st->ms, cname, tool);
}
/*
 * returns current MSWebCam for a given stream
 * */
MS2_PUBLIC const MSWebCam * video_stream_get_camera(const VideoStream *stream);

/**
 * Returns the current video stream source filter. Be careful, this source will be
 * destroyed if the stream is stopped.
 * @return current stream source
 */
MS2_PUBLIC MSFilter* video_stream_get_source_filter(const VideoStream* stream);

MS2_PUBLIC void video_stream_change_camera(VideoStream *stream, MSWebCam *cam);

/**
 * @brief This functions changes the source filter for the passed video stream.
 * @details This is quite the same function as \ref video_stream_change_camera, but this one
 * allows you to pass the source filter that is created for the camera and reuse it. This gives you the
 * ability to switch rapidly between two streams, whereas re-creating them each time would be
 * costly (especially with webcams).
 *
 * @note Since the \ref video_stream_stop() will automatically destroy the source, it is
 *		advised that you use \ref video_stream_stop_keep_source() instead, so that you
 *		can manually destroy the source filters after the stream is stopped.
 *
 * Example usage:
 *
 *		video_stream_start(stream, profile, [...], noWebcamDevice);
 *		// We manage the sources for the stream ourselves:
 *		MSFilter* noWebCamFilter = video_stream_get_source_filter(stream);
 *		MSFilter* frontCamFilter = ms_web_cam_create_reader(frontCamDevice);
 *
 * 		sleep(1);
 * 		video_stream_change_source_filter(stream, frontCamDevice, frontCamFilter, TRUE); // will keep the previous filter
 * 		sleep(1);
 * 		video_stream_change_source_filter(stream, noWebcamDevice, noWebCamFilter, TRUE); // keep the previous filter
 *
 *		sleep(1)
 *		video_stream_stop_keep_source(stream);
 *		ms_filter_destroy(noWebCamFilter);
 *		ms_filter_destroy(frontCamFilter);
 *
 *
 * @param stream the video stream to modify
 * @param cam the camera that you want to set as the new source
 * @param cam_filter the filter for this camera. It can be obtained with ms_web_cam_create_reader(cam)
 * @return the previous source if keep_previous_source is TRUE, otherwise NULL
 */
MS2_PUBLIC MSFilter* video_stream_change_source_filter(VideoStream *stream, MSWebCam* cam, MSFilter* filter, bool_t keep_previous_source );

/**
 * @brief This is the same function as \ref video_stream_change_source_filter() called with keep_source=1, but
 *  the new filter will be created from the MSWebcam that is passed as argument.
 *
 *  @param stream the video stream
 *  @param cam the MSWebcam from which the new source filter should be created.
 *  @return the previous source filter
 */
MS2_PUBLIC MSFilter* video_stream_change_camera_keep_previous_source(VideoStream *stream, MSWebCam *cam);


/* Calling video_stream_set_sent_video_size() or changing the bitrate value in the used PayloadType during a stream is running does nothing.
The following function allows to take into account new parameters by redrawing the sending graph*/
MS2_PUBLIC void video_stream_update_video_params(VideoStream *stream);
/*function to call periodically to handle various events */
MS2_PUBLIC void video_stream_iterate(VideoStream *stream);

/**
 * Asks the video stream to send a Full-Intra Request.
 * @param[in] stream The videostream object.
 */
MS2_PUBLIC void video_stream_send_fir(VideoStream *stream);

/**
 * Asks the video stream to generate a Video Fast Update (generally after receiving a Full-Intra Request.
 * @param[in] stream The videostream object.
 */
MS2_PUBLIC void video_stream_send_vfu(VideoStream *stream);

MS2_PUBLIC void video_stream_stop(VideoStream * stream);

/**
 * Stop the video stream, but does not destroy the source of the video. This function
 * can be use in conjunction with \ref video_stream_change_source_filter() to allow
 * manual management of the source filters for a video stream.
 * @param stream the stream to stop
 * @return returns the source of the video stream, which you should manually destroy when appropriate.
 */
MS2_PUBLIC MSFilter* video_stream_stop_keep_source(VideoStream * stream);

MS2_PUBLIC bool_t video_stream_started(VideoStream *stream);

/**
 * Try to set the size of the video that is sent. Since this relies also on the
 * bitrate specified, make sure to set the payload bitrate accordingly with
 * rtp_profile_get_payload and normal_bitrate value otherwise the best
 * possible resolution will be taken instead of the requested one.
 * @param[in] stream The videostream for which to get the sent video size.
 * @param[in] vsize The sent video size wished.
 */
MS2_PUBLIC void video_stream_set_sent_video_size(VideoStream *stream, MSVideoSize vsize);

/**
 * Gets the size of the video that is sent.
 * @param[in] stream The videostream for which to get the sent video size.
 * @return The sent video size or MS_VIDEO_SIZE_UNKNOWN if not available.
 */
MS2_PUBLIC MSVideoSize video_stream_get_sent_video_size(const VideoStream *stream);

/**
 * Gets the size of the video that is received.
 * @param[in] stream The videostream for which to get the received video size.
 * @return The received video size or MS_VIDEO_SIZE_UNKNOWN if not available.
 */
MS2_PUBLIC MSVideoSize video_stream_get_received_video_size(const VideoStream *stream);

/**
 * Gets the framerate of the video that is sent.
 * @param[in] stream The videostream.
 * @return The actual framerate, 0 if not available..
 */
MS2_PUBLIC float video_stream_get_sent_framerate(const VideoStream *stream);

/**
 * Gets the framerate of the video that is received.
 * @param[in] stream The videostream.
 * @return The received framerate or 0 if not available.
 */
MS2_PUBLIC float video_stream_get_received_framerate(const VideoStream *stream);

/**
 * Returns the name of the video display filter on the current platform.
**/
MS2_PUBLIC const char *video_stream_get_default_video_renderer(void);

MS2_PUBLIC void video_stream_enable_self_view(VideoStream *stream, bool_t val);
MS2_PUBLIC void * video_stream_get_native_window_id(VideoStream *stream);
MS2_PUBLIC void video_stream_set_native_window_id(VideoStream *stream, void *id);
MS2_PUBLIC void video_stream_set_native_preview_window_id(VideoStream *stream, void *id);
MS2_PUBLIC void * video_stream_get_native_preview_window_id(VideoStream *stream);
MS2_PUBLIC void video_stream_use_preview_video_window(VideoStream *stream, bool_t yesno);
MS2_PUBLIC void video_stream_set_device_rotation(VideoStream *stream, int orientation);
MS2_PUBLIC void video_stream_show_video(VideoStream *stream, bool_t show);
MS2_PUBLIC void video_stream_set_freeze_on_error(VideoStream *stream, bool_t yesno);

/**
 * @brief Gets the camera sensor rotation.
 *
 * This is needed on some mobile platforms to get the number of degrees the camera sensor
 * is rotated relative to the screen.
 *
 * @param stream The video stream related to the operation
 * @return The camera sensor rotation in degrees (0 to 360) or -1 if it could not be retrieved
 */
MS2_PUBLIC int video_stream_get_camera_sensor_rotation(VideoStream *stream);

/*provided for compatibility, use video_stream_set_direction() instead */
MS2_PUBLIC int video_stream_recv_only_start(VideoStream *videostream, RtpProfile *profile, const char *addr, int port, int used_pt, int jitt_comp);
MS2_PUBLIC int video_stream_send_only_start(VideoStream *videostream,
				RtpProfile *profile, const char *addr, int port, int rtcp_port,
				int used_pt, int  jitt_comp, MSWebCam *device);
MS2_PUBLIC void video_stream_recv_only_stop(VideoStream *vs);
MS2_PUBLIC void video_stream_send_only_stop(VideoStream *vs);

/* enable ZRTP on the video stream using information from the audio stream */
MS2_PUBLIC void video_stream_enable_zrtp(VideoStream *vstream, AudioStream *astream);
MS2_PUBLIC void video_stream_start_zrtp(VideoStream *stream);

/* enable SRTP on the video stream */
static MS2_INLINE bool_t video_stream_enable_strp(VideoStream* stream, MSCryptoSuite suite, const char* snd_key, const char* rcv_key) {
	return media_stream_enable_srtp(&stream->ms, suite, snd_key, rcv_key);
}

/* if enabled, the display filter will internaly rotate the video, according to the device orientation */
MS2_PUBLIC void video_stream_enable_display_filter_auto_rotate(VideoStream* stream, bool_t enable);

/* retrieve RTP statistics*/
static MS2_INLINE void video_stream_get_local_rtp_stats(VideoStream *stream, rtp_stats_t *stats) {
	media_stream_get_local_rtp_stats(&stream->ms, stats);
}

static MS2_INLINE int video_stream_set_dscp(VideoStream *stream, int dscp) {
	return media_stream_set_dscp(&stream->ms, dscp);
}

/**
 * Gets the RTP session of a video stream.
 * @param[in] stream #MediaStream object.
 * @return The RTP session of the video stream.
 */
static MS2_INLINE RtpSession * video_stream_get_rtp_session(const VideoStream *stream) {
	return media_stream_get_rtp_session(&stream->ms);
}

/**
 * Ask the video stream whether a decoding error should be reported (eg. to send a VFU request).
 * @param[in] stream The VideoStream object.
 * @param[in] ms The minimum interval in milliseconds between to decoding error report.
 * @return TRUE if the decoding error should be reported, FALSE otherwise.
 */
MS2_PUBLIC bool_t video_stream_is_decoding_error_to_be_reported(VideoStream *stream, uint32_t ms);

/**
 * Tell the video stream that a decoding error has been reported.
 * @param[in] stream The VideoStream object.
 */
MS2_PUBLIC void video_stream_decoding_error_reported(VideoStream *stream);

/**
 * Tell the video stream that a decoding error has been recovered so that new decoding can be reported sooner.
 * @param[in] stream The VideoStream object.
 */
MS2_PUBLIC void video_stream_decoding_error_recovered(VideoStream *stream);


/**
 * Force a resolution for the preview.
 * @param[in] stream The VideoStream object.
 * @param[in] vsize video resolution.
**/
MS2_PUBLIC void video_stream_set_preview_size(VideoStream *stream, MSVideoSize vsize);

/**
 * Force a resolution for the preview.
 * @param[in] stream The VideoStream object.
 * @param[in] fps the frame rate in frame/seconds. A value of zero means "use encoder default value".
**/
MS2_PUBLIC void video_stream_set_fps(VideoStream *stream, float fps);

/**
 * Link the audio stream with an existing video stream.
 * This is necessary to enable recording of audio & video into a multimedia file.
 */
MS2_PUBLIC void audio_stream_link_video(AudioStream *stream, VideoStream *video);

/**
 * Unlink the audio stream from the video stream.
 * This must be done if the video stream is about to be stopped.
**/
MS2_PUBLIC void audio_stream_unlink_video(AudioStream *stream, VideoStream *video);

/**
 * Set a video preset to be used for the video stream.
 * @param[in] stream VideoStream object
 * @param[in] preset The name of the video preset to be used.
 */
MS2_PUBLIC void video_stream_use_video_preset(VideoStream *stream, const char *preset);


/**
 * Open a player to play a video file (mkv) to remote end.
 * The player is returned as a MSFilter so that application can make usual player controls on it using the MSPlayerInterface.
**/
MS2_PUBLIC MSFilter * video_stream_open_remote_play(VideoStream *stream, const char *filename);

MS2_PUBLIC void video_stream_close_remote_play(VideoStream *stream);

/**
 * Open a recorder to record the video coming from remote end into a mkv file.
 * This must be done before the stream is started.
**/
MS2_PUBLIC MSFilter * video_stream_open_remote_record(VideoStream *stream, const char *filename);

MS2_PUBLIC void video_stream_close_remote_record(VideoStream *stream);

/**
 * Small API to display a local preview window.
**/

typedef VideoStream VideoPreview;

MS2_PUBLIC VideoPreview * video_preview_new(MSFactory *factory);
#define video_preview_set_size(p,s)			video_stream_set_sent_video_size(p,s)
#define video_preview_set_display_filter_name(p,dt)	video_stream_set_display_filter_name(p,dt)
#define video_preview_set_native_window_id(p,id)	video_stream_set_native_preview_window_id(p,id)
#define video_preview_get_native_window_id(p)		video_stream_get_native_preview_window_id(p)
#define video_preview_set_fps(p,fps)			video_stream_set_fps((VideoStream*)p,fps)
#define video_preview_set_device_rotation(p, r) video_stream_set_device_rotation(p, r)
MS2_PUBLIC void video_preview_start(VideoPreview *stream, MSWebCam *device);
MS2_PUBLIC MSVideoSize video_preview_get_current_size(VideoPreview *stream);
MS2_PUBLIC void video_preview_stop(VideoPreview *stream);
MS2_PUBLIC void video_preview_change_camera(VideoPreview *stream, MSWebCam *cam);
MS2_PUBLIC void video_preview_update_video_params(VideoPreview *stream);


/**
 * Stops the video preview graph but keep the source filter for reuse.
 * This is useful when transitioning from a preview-only to a duplex video.
 * The filter needs to be passed to the #video_stream_start_with_source function,
 * otherwise you should destroy it.
 * @param[in] stream VideoPreview object
 * @return The source filter to be passed to the #video_stream_start_with_source function.
 */
MS2_PUBLIC MSFilter* video_preview_stop_reuse_source(VideoPreview *stream);

/*
 * Returns the web cam descriptor for the mire kind of camera.
**/
MS2_PUBLIC MSWebCamDesc *ms_mire_webcam_desc_get(void);


/**
 * Create an RTP session for duplex communication.
 * @param[in] local_ip The local IP to bind the RTP and RTCP sockets to.
 * @param[in] local_rtp_port The local port to bind the RTP socket to.
 * @param[in] local_rtcp_port The local port to bind the RTCP socket to.
 */
MS2_PUBLIC RtpSession * ms_create_duplex_rtp_session(const char* local_ip, int loc_rtp_port, int loc_rtcp_port, int mtu);

/**
 * Asks the audio playback filter to route to the selected device (currently only used for blackberry)
 * @param[in] stream The AudioStream object
 * @param[in] route The wanted audio output device (earpiece, speaker)
 */
MS2_PUBLIC void audio_stream_set_audio_route(AudioStream *stream, MSAudioRoute route);

/**
 * @}
**/

/**
 * @addtogroup text_stream_api
 * @{
**/

struct _TextStream
{
	MediaStream ms;
	MSFilter *rttsource;
	MSFilter *rttsink;
	int pt_t140;
	int pt_red;
};

typedef struct _TextStream TextStream;

/**
 * Creates a TextStream object listening on a RTP port.
 * @param loc_rtp_port the local UDP port to listen for RTP packets.
 * @param loc_rtcp_port the local UDP port to listen for RTCP packets
 * @param ipv6 TRUE if ipv6 must be used.
 * @param factory 
 * @return a new TextStream.
**/
MS2_PUBLIC TextStream *text_stream_new(MSFactory *factory, int loc_rtp_port, int loc_rtcp_port, bool_t ipv6);

/**
 * Creates a TextStream object from initialized MSMediaStreamSessions.
 * @param sessions the MSMediaStreamSessions
 * @param factory
 * @return a new TextStream
**/
MS2_PUBLIC TextStream *text_stream_new_with_sessions(MSFactory *factory, const MSMediaStreamSessions *sessions);

/**
 * Creates a TextStream object listening on a RTP port for a dedicated address.
 * @param loc_ip the local ip to listen for RTP packets. Can be ::, O.O.O.O or any ip4/6 addresses
 * @param [in] loc_rtp_port the local UDP port to listen for RTP packets.
 * @param [in] loc_rtcp_port the local UDP port to listen for RTCP packets
 * @param factory 
 * @return a new TextStream.
**/
MS2_PUBLIC TextStream *text_stream_new2(MSFactory *factory, const char* ip, int loc_rtp_port, int loc_rtcp_port);

/**
 * Starts a text stream.
 *
 * @param[in] stream TextStream object previously created with text_stream_new().
 * @param[in] profile RtpProfile object holding the PayloadType that can be used during the text session.
 * @param[in] rem_rtp_addr The remote IP address where to send the text to.
 * @param[in] rem_rtp_port The remote port where to send the text to.
 * @param[in] rem_rtcp_addr The remote IP address for RTCP.
 * @param[in] rem_rtcp_port The remote port for RTCP.
 * @param[in] payload_type The payload type number used to send the text stream. A valid PayloadType must be available at this index in the profile.
 * @param[in] factory
 */
MS2_PUBLIC TextStream* text_stream_start(TextStream *stream, RtpProfile *profile, const char *rem_rtp_addr, int rem_rtp_port, const char *rem_rtcp_addr, int rem_rtcp_port,
										 int payload_type);

/**
 *  Stops the text streaming thread and free everything
**/
MS2_PUBLIC void text_stream_stop (TextStream * stream);

/**
 * Executes background low priority tasks related to text processing (RTP statistics analysis).
 * It should be called periodically, for example with an interval of 100 ms or so.
 * 
 * @param[in] stream TextStream object previously created with text_stream_new().
 */
MS2_PUBLIC void text_stream_iterate(TextStream *stream);

/**
 * Writes a character to stream in UTF-32 format.
 * 
 * @param[in] stream TextStream object previously created with text_stream_new().
 * @param[in] i the Char in UTF-32 format.
 **/
MS2_PUBLIC void text_stream_putchar32(TextStream *stream, uint32_t i);

MS2_PUBLIC void text_stream_prepare_text(TextStream *stream);
MS2_PUBLIC void text_stream_unprepare_text(TextStream *stream);

/**
 * @}
**/

#ifdef __cplusplus
}
#endif

#endif

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

#include <math.h>

#include "mediastreamer2/mediastream.h"
#include "mediastreamer2/msequalizer.h"
#include "mediastreamer2/msvolume.h"
#ifdef VIDEO_ENABLED
#include "mediastreamer2/msv4l.h"
#endif

#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#ifndef WIN32
#include <unistd.h>
#include <poll.h>
#else
#include <malloc.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#include <CoreFoundation/CFRunLoop.h>
#endif
#if  defined(__ios) || defined (ANDROID)
#ifdef __ios
#import <UIKit/UIKit.h>
#include <AudioToolbox/AudioToolbox.h>
#endif
extern void ms_set_video_stream(VideoStream* video);
#ifdef HAVE_X264
extern void libmsx264_init();
#endif
#ifdef HAVE_SILK
extern void libmssilk_init();
#endif 
#endif

#ifdef ANDROID
#include <android/log.h>
#include <jni.h>
#endif

#include <ortp/b64.h>

#define MEDIASTREAM_MAX_ICE_CANDIDATES 3


static int cond=1;


typedef struct _MediastreamIceCandidate {
	char ip[64];
	char type[6];
	int port;
} MediastreamIceCandidate;

typedef struct _MediastreamDatas {
	int localport,remoteport,payload;
	char ip[64];
	char *fmtp;
	int jitter;
	int bitrate;
	int mtu;
	MSVideoSize vs;
	bool_t ec;
	bool_t agc;
	bool_t eq;
	bool_t is_verbose;
	int device_rotation;

#ifdef VIDEO_ENABLED
	VideoStream *video;
#endif
	char * capture_card;
	char * playback_card;
	char * camera;
	char *infile,*outfile;
	float ng_threshold;
	bool_t use_ng;
	bool_t two_windows;
	bool_t el;
	bool_t use_rc;
	bool_t enable_srtp;
	bool_t interactive;
	bool_t pad[2];
	float el_speed;
	float el_thres;
	float el_force;
	int el_sustain;
	float el_transmit_thres;
	float ng_floorgain;
	char * zrtp_secrets;
	PayloadType *custom_pt;
	int video_window_id;
	int preview_window_id;
	/* starting values echo canceller */
	int ec_len_ms, ec_delay_ms, ec_framesize;
	char* srtp_local_master_key;
	char* srtp_remote_master_key;
	int netsim_bw;
	int netsim_lossrate;
	float zoom;
	float zoom_cx, zoom_cy;
	
	AudioStream *audio;	
	PayloadType *pt;
	RtpSession *session;
	OrtpEvQueue *q;
	RtpProfile *profile;

	IceSession *ice_session;
	MediastreamIceCandidate ice_local_candidates[MEDIASTREAM_MAX_ICE_CANDIDATES];
	MediastreamIceCandidate ice_remote_candidates[MEDIASTREAM_MAX_ICE_CANDIDATES];
	int ice_local_candidates_nb;
	int ice_remote_candidates_nb;
} MediastreamDatas;


// MAIN METHODS
/* init default arguments */
MediastreamDatas* init_default_args();
/* parse args */
bool_t parse_args(int argc, char** argv, MediastreamDatas* out);
/* setup streams */
void setup_media_streams(MediastreamDatas* args);
/* run loop*/
void mediastream_run_loop(MediastreamDatas* args);
/* exit */
void clear_mediastreams(MediastreamDatas* args);

// HELPER METHODS
static void stop_handler(int signum);
static bool_t parse_addr(const char *addr, char *ip, int len, int *port);
static bool_t parse_ice_addr(char* addr, char* type, int type_len, char* ip, int ip_len, int* port);
static void display_items(void *user_data, uint32_t csrc, rtcp_sdes_type_t t, const char *content, uint8_t content_len);
static void parse_rtcp(mblk_t *m);
static void parse_events(RtpSession *session, OrtpEvQueue *q);
static PayloadType* create_custom_payload_type(const char *type, const char *subtype, const char *rate, const char *channels, int number);
static PayloadType* parse_custom_payload(const char *name);
static bool_t parse_window_ids(const char *ids, int* video_id, int* preview_id);

const char *usage="mediastream --local <port> --remote <ip:port> \n"
								"--payload <payload type number or payload name like 'audio/pmcu/8000'>\n"
								"[ --fmtp <fmtpline> ]\n"
								"[ --jitter <miliseconds> ]\n"
								"[ --width <pixels> ]\n"
								"[ --height <pixels> ]\n"
								"[ --bitrate <bits per seconds> ]\n"
								"[ --ec (enable echo canceller) ]\n"
								"[ --ec-tail <echo canceller tail length in ms> ]\n"
								"[ --ec-delay <echo canceller delay in ms> ]\n"
								"[ --ec-framesize <echo canceller framesize in samples> ]\n"
								"[ --agc (enable automatic gain control) ]\n"
								"[ --ng (enable noise gate)] \n"
								"[ --ng-threshold <(float) [0-1]> (noise gate threshold) ]\n"
								"[ --ng-floorgain <(float) [0-1]> (gain applied to the signal when its energy is below the threshold.) ]\n"
								"[ --capture-card <name> ]\n"
								"[ --playback-card <name> ]\n"
								"[ --infile	<input wav file> specify a wav file to be used for input, instead of soundcard ]\n"
								"[ --outfile <output wav file> specify a wav file to write audio into, instead of soundcard ]\n"
								"[ --camera <camera id as listed at startup> ]\n"
								"[ --el (enable echo limiter) ]\n"
								"[ --el-speed <(float) [0-1]> (gain changes are smoothed with a coefficent) ]\n"
								"[ --el-thres <(float) [0-1]> (Threshold above which the system becomes active) ]\n"
								"[ --el-force <(float) [0-1]> (The proportional coefficient controlling the mic attenuation) ]\n"
								"[ --el-sustain <(int)> (Time in milliseconds for which the attenuation is kept unchanged after) ]\n"
								"[ --el-transmit-thres <(float) [0-1]> (TO BE DOCUMENTED) ]\n"
								"[ --rc (enable adaptive rate control) ]\n"
								"[ --zrtp <secrets file> (enable zrtp) ]\n"
								"[ --verbose (most verbose messages) ]\n"
								"[ --video-windows-id <video surface:preview surface>]\n"
								"[ --srtp <local master_key> <remote master_key> (enable srtp, master key is generated if absent from comand line)\n"
								"[ --netsim-bandwidth <bandwidth limit in bits/s> (simulates a network download bandwidth limit)\n"
								"[ --netsim-lossrate <0-100> (simulates a network lost rate)\n"
								"[ --zoom zoomfactor]\n"
								"[ --ice-local-candidate <ip:port:[host|srflx|prflx|relay]> ]\n"
								"[ --ice-remote-candidate <ip:port:[host|srflx|prflx|relay]> ]\n"
								"[ --mtu <mtu> (specify MTU)]\n"
								"[ --interactive (run in interactive mode)]\n"
		;

#if TARGET_OS_IPHONE
int g_argc;
char** g_argv;
static int _main(int argc, char * argv[]);
 
static void* apple_main(void* data) {
	 _main(g_argc,g_argv);
	 return NULL;
	}
int main(int argc, char * argv[]) {
	pthread_t main_thread;
	g_argc=argc;
	g_argv=argv;
	pthread_create(&main_thread,NULL,apple_main,NULL);
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	int value = UIApplicationMain(0, nil, nil, nil);
	[pool release];
	return value;
	cond=0;
	pthread_join(main_thread,NULL);
	return 0;
}
static int _main(int argc, char * argv[])
#endif

#if !__APPLE__ && !ANDROID
int main(int argc, char * argv[])
#endif

#if !ANDROID && !TARGET_OS_MAC || TARGET_OS_IPHONE
{
	MediastreamDatas* args;
	cond = 1;

	args = init_default_args();

	if (!parse_args(argc, argv, args))
		return 0;

	setup_media_streams(args);

	mediastream_run_loop(args);
	
	clear_mediastreams(args);

	free(args);
	
	return 0;
}

#endif


MediastreamDatas* init_default_args() {
	MediastreamDatas* args = (MediastreamDatas*)ms_malloc0(sizeof(MediastreamDatas));
	args->localport=0;
	args->remoteport=0;
	args->payload=0;
	memset(args->ip, 0, sizeof(args->ip));
	args->fmtp=NULL;
	args->jitter=50;
	args->bitrate=0;
	args->ec=FALSE;
	args->agc=FALSE;
	args->eq=FALSE;
	args->interactive=FALSE;
	args->is_verbose=FALSE;
	args->device_rotation=-1;

#ifdef VIDEO_ENABLED
	args->video=NULL;
#endif
	args->capture_card=NULL;
	args->playback_card=NULL;
	args->camera=NULL;
	args->infile=args->outfile=NULL;
	args->ng_threshold=-1;
	args->use_ng=FALSE;
	args->two_windows=FALSE;
	args->el=FALSE;
	args->el_speed=-1;
	args->el_thres=-1;
	args->el_force=-1;
	args->el_sustain=-1;
	args->el_transmit_thres=-1;
	args->ng_floorgain=-1;
	args->use_rc=FALSE;
	args->zrtp_secrets=NULL;
	args->custom_pt=NULL;
	args->video_window_id = -1;
	args->preview_window_id = -1;
	/* starting values echo canceller */
	args->ec_len_ms=args->ec_delay_ms=args->ec_framesize=0;
	args->enable_srtp = FALSE;
	args->srtp_local_master_key = args->srtp_remote_master_key = NULL;
	args->zoom = 1.0;
	args->zoom_cx = args->zoom_cy = 0.5;

	args->audio = NULL;
	args->session = NULL;
	args->pt = NULL;
	args->q = NULL;
	args->profile = NULL;

	args->ice_session = NULL;
	memset(args->ice_local_candidates, 0, sizeof(args->ice_local_candidates));
	memset(args->ice_remote_candidates, 0, sizeof(args->ice_remote_candidates));
	args->ice_local_candidates_nb = args->ice_remote_candidates_nb = 0;

	return args;
}

bool_t parse_args(int argc, char** argv, MediastreamDatas* out) {
	int i;

	if (argc<4) {
		printf("%s",usage);
		return FALSE;
	}

	/* default size */
	out->vs.width=MS_VIDEO_SIZE_CIF_W;
	out->vs.height=MS_VIDEO_SIZE_CIF_H;

	for (i=1;i<argc;i++){
		if (strcmp(argv[i],"--local")==0){
			i++;
			out->localport=atoi(argv[i]);
		}else if (strcmp(argv[i],"--remote")==0){
			i++;
			if (!parse_addr(argv[i],out->ip,sizeof(out->ip),&out->remoteport)) {
				printf("%s",usage);
				return FALSE;
			}
			printf("Remote addr: ip=%s port=%i\n",out->ip,out->remoteport);
		}else if (strcmp(argv[i],"--ice-local-candidate")==0) {
			MediastreamIceCandidate *candidate;
			i++;
			if (out->ice_local_candidates_nb>=MEDIASTREAM_MAX_ICE_CANDIDATES) {
				printf("Ignore ICE local candidate \"%s\" (maximum %d candidates allowed)\n",argv[i],MEDIASTREAM_MAX_ICE_CANDIDATES);
				continue;
			}
			candidate=&out->ice_local_candidates[out->ice_local_candidates_nb];
			if (!parse_ice_addr(argv[i],candidate->type,sizeof(candidate->type),candidate->ip,sizeof(candidate->ip),&candidate->port)) {
				printf("%s",usage);
				return FALSE;
			}
			out->ice_local_candidates_nb++;
			printf("ICE local candidate: type=%s ip=%s port=%i\n",candidate->type,candidate->ip,candidate->port);
		}else if (strcmp(argv[i],"--ice-remote-candidate")==0) {
			MediastreamIceCandidate *candidate;
			i++;
			if (out->ice_remote_candidates_nb>=MEDIASTREAM_MAX_ICE_CANDIDATES) {
				printf("Ignore ICE remote candidate \"%s\" (maximum %d candidates allowed)\n",argv[i],MEDIASTREAM_MAX_ICE_CANDIDATES);
				continue;
			}
			candidate=&out->ice_remote_candidates[out->ice_remote_candidates_nb];
			if (!parse_ice_addr(argv[i],candidate->type,sizeof(candidate->type),candidate->ip,sizeof(candidate->ip),&candidate->port)) {
				printf("%s",usage);
				return FALSE;
			}
			out->ice_remote_candidates_nb++;
			printf("ICE remote candidate: type=%s ip=%s port=%i\n",candidate->type,candidate->ip,candidate->port);
		}else if (strcmp(argv[i],"--payload")==0){
			i++;
			if (isdigit(argv[i][0])){
				out->payload=atoi(argv[i]);
			}else {
				out->payload=114;
				out->custom_pt=parse_custom_payload(argv[i]);
			}
		}else if (strcmp(argv[i],"--fmtp")==0){
			i++;
			out->fmtp=argv[i];
		}else if (strcmp(argv[i],"--jitter")==0){
			i++;
			out->jitter=atoi(argv[i]);
		}else if (strcmp(argv[i],"--bitrate")==0){
			i++;
			out->bitrate=atoi(argv[i]);
		}else if (strcmp(argv[i],"--width")==0){
			i++;
			out->vs.width=atoi(argv[i]);
		}else if (strcmp(argv[i],"--height")==0){
			i++;
			out->vs.height=atoi(argv[i]);
		}else if (strcmp(argv[i],"--capture-card")==0){
			i++;
			out->capture_card=argv[i];
		}else if (strcmp(argv[i],"--playback-card")==0){
			i++;
			out->playback_card=argv[i];
		}else if (strcmp(argv[i],"--ec")==0){
			out->ec=TRUE;
		}else if (strcmp(argv[i],"--ec-tail")==0){
			i++;
			out->ec_len_ms=atoi(argv[i]);
		}else if (strcmp(argv[i],"--ec-delay")==0){
			i++;
			out->ec_delay_ms=atoi(argv[i]);
		}else if (strcmp(argv[i],"--ec-framesize")==0){
			i++;
			out->ec_framesize=atoi(argv[i]);
		}else if (strcmp(argv[i],"--agc")==0){
			out->agc=TRUE;
		}else if (strcmp(argv[i],"--eq")==0){
			out->eq=TRUE;
		}else if (strcmp(argv[i],"--ng")==0){
			out->use_ng=1;
		}else if (strcmp(argv[i],"--rc")==0){
			out->use_rc=1;
		}else if (strcmp(argv[i],"--ng-threshold")==0){
			i++;
			out->ng_threshold=atof(argv[i]);
		}else if (strcmp(argv[i],"--ng-floorgain")==0){
			i++;
			out->ng_floorgain=atof(argv[i]);
		}else if (strcmp(argv[i],"--two-windows")==0){
			out->two_windows=TRUE;
		}else if (strcmp(argv[i],"--infile")==0){
			i++;
			out->infile=argv[i];
		}else if (strcmp(argv[i],"--outfile")==0){
			i++;
			out->outfile=argv[i];
		}else if (strcmp(argv[i],"--camera")==0){
			i++;
			out->camera=argv[i];
		}else if (strcmp(argv[i],"--el")==0){
			out->el=TRUE;
		}else if (strcmp(argv[i],"--el-speed")==0){
			i++;
			out->el_speed=atof(argv[i]);
		}else if (strcmp(argv[i],"--el-thres")==0){
			i++;
			out->el_thres=atof(argv[i]);
		}else if (strcmp(argv[i],"--el-force")==0){
			i++;
			out->el_force=atof(argv[i]);
		}else if (strcmp(argv[i],"--el-sustain")==0){
			i++;
			out->el_sustain=atoi(argv[i]);
		}else if (strcmp(argv[i],"--el-transmit-thres")==0){
			i++;
			out->el_transmit_thres=atof(argv[i]);
		} else if (strcmp(argv[i],"--zrtp")==0){
			out->zrtp_secrets=argv[++i];
		} else if (strcmp(argv[i],"--verbose")==0){
			out->is_verbose=TRUE;
		} else if (strcmp(argv[i], "--video-windows-id")==0) {
			i++;
			if (!parse_window_ids(argv[i],&out->video_window_id, &out->preview_window_id)) {
				printf("%s",usage);
				return FALSE;
			}
		} else if (strcmp(argv[i], "--device-rotation")==0) {
			i++;
			out->device_rotation=atoi(argv[i]);
		} else if (strcmp(argv[i], "--srtp")==0) {
			if (!ortp_srtp_supported()) {
				ms_error("ortp srtp support not enabled");
				return FALSE;
			}
			out->enable_srtp = TRUE;
			i++;
			// check if we're being given keys
			if (i + 1 < argc) {
				out->srtp_local_master_key = argv[i++];
				out->srtp_remote_master_key = argv[i++];
			}
		} else if (strcmp(argv[i],"--netsim-bandwidth")==0){
			i++;
			out->netsim_bw=atoi(argv[i]);
		} else if (strcmp(argv[i],"--netsim-lossrate")==0){
			i++;
			out->netsim_lossrate=atoi(argv[i]);
			if(out->netsim_lossrate < 0) {
				ms_warning("%d < 0, wrong value for --lost-rate: set to 0", out->netsim_lossrate);
				out->netsim_lossrate=0;
			}
			if(out->netsim_lossrate > 100) {
				ms_warning("%d > 100, wrong value for --lost-rate: set to 100", out->netsim_lossrate);
				out->netsim_lossrate=100;
			}
		} else if (strcmp(argv[i],"--zoom")==0){
			i++;
			if (sscanf(argv[i], "%f,%f,%f", &out->zoom, &out->zoom_cx, &out->zoom_cy) != 3) {
				ms_error("Invalid zoom triplet");
				return FALSE;
			}
		}else if (strcmp(argv[i],"--mtu")==0){
			i++;
			if (sscanf(argv[i], "%i", &out->mtu) != 1) {
				ms_error("Invalid mtu value");
				return FALSE;
			}
		}else if (strcmp(argv[i],"--interactive")==0){
			i++;
			out->interactive=TRUE;
		}
		else if (strcmp(argv[i],"--help")==0){
			printf("%s",usage);
			return FALSE;
		}
	}
	return TRUE;
}


void setup_media_streams(MediastreamDatas* args) {
	/*create the rtp session */
	OrtpNetworkSimulatorParams params={0};
#ifdef VIDEO_ENABLED	
	MSWebCam *cam=NULL;
#endif
	ortp_init();
	if (args->is_verbose) {
		ortp_set_log_level_mask(ORTP_DEBUG|ORTP_MESSAGE|ORTP_WARNING|ORTP_ERROR|ORTP_FATAL);
	} else {
		ortp_set_log_level_mask(ORTP_MESSAGE|ORTP_WARNING|ORTP_ERROR|ORTP_FATAL);
	}


#if defined(__ios) || defined(ANDROID)
#if defined (HAVE_X264) && defined (VIDEO_ENABLED)
	libmsx264_init(); /*no plugin on IOS*/
#endif
#if defined (HAVE_SILK)
	libmssilk_init(); /*no plugin on IOS*/
#endif
	
#endif
	
	rtp_profile_set_payload(&av_profile,110,&payload_type_speex_nb);
	rtp_profile_set_payload(&av_profile,111,&payload_type_speex_wb);
	rtp_profile_set_payload(&av_profile,112,&payload_type_ilbc);
	rtp_profile_set_payload(&av_profile,113,&payload_type_amr);
	rtp_profile_set_payload(&av_profile,114,args->custom_pt);
	rtp_profile_set_payload(&av_profile,115,&payload_type_lpc1015);
#ifdef VIDEO_ENABLED
#if defined (__ios) && defined (HAVE_X264)
	libmsx264_init(); /*no plugin on IOS*/
#endif
	rtp_profile_set_payload(&av_profile,26,&payload_type_jpeg);
	rtp_profile_set_payload(&av_profile,98,&payload_type_h263_1998);
	rtp_profile_set_payload(&av_profile,97,&payload_type_theora);
	rtp_profile_set_payload(&av_profile,99,&payload_type_mp4v);
	rtp_profile_set_payload(&av_profile,100,&payload_type_x_snow);
	rtp_profile_set_payload(&av_profile,102,&payload_type_h264);
	rtp_profile_set_payload(&av_profile,103,&payload_type_vp8);
	
	args->video=NULL;
#endif
	args->profile=rtp_profile_clone_full(&av_profile);
	args->q=ortp_ev_queue_new();

	ms_init();
	if (args->mtu) ms_set_mtu(args->mtu);
	ms_filter_enable_statistics(TRUE);
	ms_filter_reset_statistics();
	args->ice_session=ice_session_new();
	ice_session_set_remote_credentials(args->ice_session,"1234","1234567890abcdef123456");
	// ICE local credentials are assigned when creating the ICE session, but force them here to simplify testing
	ice_session_set_local_credentials(args->ice_session,"1234","1234567890abcdef123456");
	ice_dump_session(args->ice_session);

	signal(SIGINT,stop_handler);
	args->pt=rtp_profile_get_payload(args->profile,args->payload);
	if (args->pt==NULL){
		printf("Error: no payload defined with number %i.\n",args->payload);
		exit(-1);
	}
	if (args->fmtp!=NULL) payload_type_set_send_fmtp(args->pt,args->fmtp);
	if (args->bitrate>0) args->pt->normal_bitrate=args->bitrate;
	
	if (args->pt->normal_bitrate==0){
		printf("Error: no default bitrate specified for codec %s/%i. "
			"Please specify a network bitrate with --bitrate option.\n",args->pt->mime_type,args->pt->clock_rate);
		exit(-1);
	}

	// do we need to generate srtp keys ?
	if (args->enable_srtp) {
		// default profile require key-length = 30 bytes
		//  -> input : 40 b64 encoded bytes
		if (!args->srtp_local_master_key) {
			uint8_t tmp[30];			
			ortp_crypto_get_random(tmp, 30);
			args->srtp_local_master_key = (char*) malloc(41);
			b64_encode((const char*)tmp, 30, args->srtp_local_master_key, 40);
			args->srtp_local_master_key[40] = '\0';
			ms_message("Generated local srtp key: '%s'", args->srtp_local_master_key);
		}
		if (!args->srtp_remote_master_key) {
			uint8_t tmp[30];			
			ortp_crypto_get_random(tmp, 30);
			args->srtp_remote_master_key = (char*) malloc(41);
			b64_encode((const char*)tmp, 30, args->srtp_remote_master_key, 40);
			args->srtp_remote_master_key[40] = '\0';
			ms_message("Generated remote srtp key: '%s'", args->srtp_remote_master_key);
		}
	}	

	if (args->pt->type!=PAYLOAD_VIDEO){
		MSSndCardManager *manager=ms_snd_card_manager_get();
		MSSndCard *capt= args->capture_card==NULL ? ms_snd_card_manager_get_default_capture_card(manager) :
				ms_snd_card_manager_get_card(manager,args->capture_card);
		MSSndCard *play= args->playback_card==NULL ? ms_snd_card_manager_get_default_playback_card(manager) :
				ms_snd_card_manager_get_card(manager,args->playback_card);
		args->audio=audio_stream_new(args->localport,args->localport+1,ms_is_ipv6(args->ip));
		audio_stream_enable_automatic_gain_control(args->audio,args->agc);
		audio_stream_enable_noise_gate(args->audio,args->use_ng);
		audio_stream_set_echo_canceller_params(args->audio,args->ec_len_ms,args->ec_delay_ms,args->ec_framesize);
		audio_stream_enable_echo_limiter(args->audio,args->el);
		audio_stream_enable_adaptive_bitrate_control(args->audio,args->use_rc);
		printf("Starting audio stream.\n");

		audio_stream_start_full(args->audio,args->profile,args->ip,args->remoteport,args->ip,args->remoteport+1, args->payload, args->jitter,args->infile,args->outfile,
		                        args->outfile==NULL ? play : NULL ,args->infile==NULL ? capt : NULL,args->infile!=NULL ? FALSE: args->ec);

		if (args->ice_local_candidates_nb || args->ice_remote_candidates_nb) {
			args->audio->ms.ice_check_list = ice_check_list_new();
			rtp_session_set_pktinfo(args->audio->ms.session,TRUE);
			ice_session_add_check_list(args->ice_session, args->audio->ms.ice_check_list);
		}
		if (args->ice_local_candidates_nb) {
			MediastreamIceCandidate *candidate;
			int c;
			for (c=0;c<args->ice_local_candidates_nb;c++){
				candidate=&args->ice_local_candidates[c];
				ice_add_local_candidate(args->audio->ms.ice_check_list,candidate->type,candidate->ip,candidate->port,1,NULL);
				ice_add_local_candidate(args->audio->ms.ice_check_list,candidate->type,candidate->ip,candidate->port+1,2,NULL);
			}
		}
		if (args->ice_remote_candidates_nb) {
			char foundation[4];
			MediastreamIceCandidate *candidate;
			int c;
			for (c=0;c<args->ice_remote_candidates_nb;c++){
				candidate=&args->ice_remote_candidates[c];
				memset(foundation, '\0', sizeof(foundation));
				snprintf(foundation, sizeof(foundation) - 1, "%u", c + 1);
				ice_add_remote_candidate(args->audio->ms.ice_check_list,candidate->type,candidate->ip,candidate->port,1,0,foundation,FALSE);
				ice_add_remote_candidate(args->audio->ms.ice_check_list,candidate->type,candidate->ip,candidate->port+1,2,0,foundation,FALSE);
			}
		}

		if (args->audio) {
			if (args->el) {
				if (args->el_speed!=-1)
					ms_filter_call_method(args->audio->volsend,MS_VOLUME_SET_EA_SPEED,&args->el_speed);
				if (args->el_force!=-1)
					ms_filter_call_method(args->audio->volsend,MS_VOLUME_SET_EA_FORCE,&args->el_force);
				if (args->el_thres!=-1)
					ms_filter_call_method(args->audio->volsend,MS_VOLUME_SET_EA_THRESHOLD,&args->el_thres);
				if (args->el_sustain!=-1)
					ms_filter_call_method(args->audio->volsend,MS_VOLUME_SET_EA_SUSTAIN,&args->el_sustain);
				if (args->el_transmit_thres!=-1)
					ms_filter_call_method(args->audio->volsend,MS_VOLUME_SET_EA_TRANSMIT_THRESHOLD,&args->el_transmit_thres);

			}
			if (args->use_ng){
				if (args->ng_threshold!=-1) {
					ms_filter_call_method(args->audio->volsend,MS_VOLUME_SET_NOISE_GATE_THRESHOLD,&args->ng_threshold);
					ms_filter_call_method(args->audio->volrecv,MS_VOLUME_SET_NOISE_GATE_THRESHOLD,&args->ng_threshold);
				}
				if (args->ng_floorgain != -1) {
					ms_filter_call_method(args->audio->volsend,MS_VOLUME_SET_NOISE_GATE_FLOORGAIN,&args->ng_floorgain);
					ms_filter_call_method(args->audio->volrecv,MS_VOLUME_SET_NOISE_GATE_FLOORGAIN,&args->ng_floorgain);
				}
			}

            #ifndef TARGET_OS_IPHONE
			if (args->zrtp_secrets != NULL) {
				OrtpZrtpParams params;
				params.zid_file=args->zrtp_secrets;
				audio_stream_enable_zrtp(args->audio,&params);
			}
            #endif

			args->session=args->audio->ms.session;
		}
		
		if (args->enable_srtp) {
			ms_message("SRTP enabled: %d", 
				audio_stream_enable_srtp(
					args->audio, 
					AES_128_SHA1_80,
					args->srtp_local_master_key, 
					args->srtp_remote_master_key));
		}
	}else{
#ifdef VIDEO_ENABLED
		float zoom[] = {
			args->zoom,
			args->zoom_cx, args->zoom_cy };

		if (args->eq){
			ms_fatal("Cannot put an audio equalizer in a video stream !");
			exit(-1);
		}
		ms_message("Starting video stream.\n");
		args->video=video_stream_new(args->localport, args->localport+1, ms_is_ipv6(args->ip));
#ifdef ANDROID
		if (args->device_rotation >= 0)
			video_stream_set_device_rotation(args->video, args->device_rotation);
#endif
		video_stream_set_sent_video_size(args->video,args->vs);
		video_stream_use_preview_video_window(args->video,args->two_windows);
#ifdef __ios
		NSBundle* myBundle = [NSBundle mainBundle];
		const char*  nowebcam = [[myBundle pathForResource:@"nowebcamCIF"ofType:@"jpg"] cStringUsingEncoding:[NSString defaultCStringEncoding]];
		ms_static_image_set_default_image(nowebcam);
#endif

		video_stream_enable_adaptive_bitrate_control(args->video,args->use_rc);
		if (args->camera)
			cam=ms_web_cam_manager_get_cam(ms_web_cam_manager_get(),args->camera);
		if (cam==NULL)
			cam=ms_web_cam_manager_get_default_cam(ms_web_cam_manager_get());
		video_stream_start(args->video,args->profile,
					args->ip,args->remoteport,
					args->ip,args->remoteport+1,
					args->payload,
					args->jitter,cam
					);
		args->session=args->video->ms.session;

		ms_filter_call_method(args->video->output,MS_VIDEO_DISPLAY_ZOOM, zoom);
		if (args->enable_srtp) {
			ms_message("SRTP enabled: %d", 
				video_stream_enable_strp(
					args->video, 
					AES_128_SHA1_80,
					args->srtp_local_master_key, 
					args->srtp_remote_master_key));
		}
#else
		printf("Error: video support not compiled.\n");
#endif
	}
	ice_session_set_base_for_srflx_candidates(args->ice_session);
	ice_session_compute_candidates_foundations(args->ice_session);
	ice_session_choose_default_candidates(args->ice_session);
	ice_session_choose_default_remote_candidates(args->ice_session);
	ice_session_start_connectivity_checks(args->ice_session);

	if (args->netsim_bw>0){
		params.enabled=TRUE;
		params.max_bandwidth=args->netsim_bw;
	}
	if (args->netsim_lossrate>0){
		params.enabled=TRUE;
		params.loss_rate=args->netsim_lossrate;
	}
	if (params.enabled){
		rtp_session_enable_network_simulation(args->session,&params);
	}

}


static void mediastream_tool_iterate(MediastreamDatas* args) {
#ifndef WIN32
	struct pollfd pfd;
	int err;
	
	if (args->interactive){
		pfd.fd=STDIN_FILENO;
		pfd.events=POLL_IN;
		pfd.revents=0;
	
		err=poll(&pfd,1,10);
		if (err==1 && (pfd.revents & POLL_IN)){
			char commands[128];
			int intarg;
			commands[127]='\0';
			ms_sleep(1);  /* ensure following text be printed after ortp messages */
			if (args->eq)
			printf("\nPlease enter equalizer requests, such as 'eq active 1', 'eq active 0', 'eq 1200 0.1 200'\n");

			if (fgets(commands,sizeof(commands)-1,stdin)!=NULL){
				int active,freq,freq_width;

				float gain;
				if (sscanf(commands,"eq active %i",&active)==1){
					audio_stream_enable_equalizer(args->audio,active);
					printf("OK\n");
				}else if (sscanf(commands,"eq %i %f %i",&freq,&gain,&freq_width)==3){
					audio_stream_equalizer_set_gain(args->audio,freq,gain,freq_width);
					printf("OK\n");
				}else if (sscanf(commands,"eq %i %f",&freq,&gain)==2){
					audio_stream_equalizer_set_gain(args->audio,freq,gain,0);
					printf("OK\n");
				}else if (strstr(commands,"dump")){
					int n=0,i;
					float *t;
					ms_filter_call_method(args->audio->equalizer,MS_EQUALIZER_GET_NUM_FREQUENCIES,&n);
					t=(float*)alloca(sizeof(float)*n);
					ms_filter_call_method(args->audio->equalizer,MS_EQUALIZER_DUMP_STATE,t);
					for(i=0;i<n;++i){
						if (fabs(t[i]-1)>0.01){
						printf("%i:%f:0 ",(i*args->pt->clock_rate)/(2*n),t[i]);
						}
					}
					printf("\nOK\n");
				}else if (sscanf(commands,"lossrate %i",&intarg)==1){
					OrtpNetworkSimulatorParams params={0};
					params.enabled=TRUE;
					params.loss_rate=intarg;
					rtp_session_enable_network_simulation(args->session,&params);
				}
				else if (strstr(commands,"quit")){
					cond=0;
				}else printf("Cannot understand this.\n");
			}
		}else if (err==-1 && errno!=EINTR){
			ms_fatal("mediastream's poll() returned %s",strerror(errno));
		}
	}else{
		ms_usleep(10000);
	}
#else
	MSG msg;
	Sleep(10);
	while (PeekMessage(&msg, NULL, 0, 0,1)){
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	/*no interactive mode on windows*/
#endif
}

void mediastream_run_loop(MediastreamDatas* args) {
	rtp_session_register_event_queue(args->session,args->q);

#ifdef __ios
	if (args->video) ms_set_video_stream(args->video); /*for IOS*/
#endif

	while(cond)
	{
		int n;
		for(n=0;n<100;++n){
			mediastream_tool_iterate(args);
#if defined(VIDEO_ENABLED)
			if (args->video) video_stream_iterate(args->video);
#endif
			if (args->audio) audio_stream_iterate(args->audio);
		}
		rtp_stats_display(rtp_session_get_stats(args->session),"RTP stats");
		if (args->session){
			ms_message("Bandwidth usage: download=%f kbits/sec, upload=%f kbits/sec\n",
				rtp_session_compute_recv_bandwidth(args->session)*1e-3,
				rtp_session_compute_send_bandwidth(args->session)*1e-3);
			parse_events(args->session,args->q);
			ms_message("Quality indicator : %f\n",args->audio ? audio_stream_get_quality_rating(args->audio) : -1);
		}
	}
}

void clear_mediastreams(MediastreamDatas* args) {
	ms_message("stopping all...\n");
	ms_message("Average quality indicator: %f",args->audio ? audio_stream_get_average_quality_rating(args->audio) : -1);

	if (args->audio) {
		audio_stream_stop(args->audio);
	}
#ifdef VIDEO_ENABLED
	if (args->video) {
		if (args->video->ms.ice_check_list) ice_check_list_destroy(args->video->ms.ice_check_list);
		video_stream_stop(args->video);
		ms_filter_log_statistics();
	}
#endif
	if (args->ice_session) ice_session_destroy(args->ice_session);
	ortp_ev_queue_destroy(args->q);
	rtp_profile_destroy(args->profile);

	ms_exit();
}

// ANDROID JNI WRAPPER
#ifdef ANDROID
JNIEXPORT jint JNICALL  JNI_OnLoad(JavaVM *ajvm, void *reserved)
{
	ms_set_jvm(ajvm);

	return JNI_VERSION_1_2;
}

JNIEXPORT void JNICALL Java_org_linphone_mediastream_MediastreamerActivity_setVideoWindowId
  (JNIEnv *env, jobject obj, jobject id, jint _args) {
	MediastreamDatas* args =  (MediastreamDatas*)_args;
#ifdef VIDEO_ENABLED
	if (!args->video)
		return;
	video_stream_set_native_window_id(args->video,(unsigned long)id);
#endif
}

JNIEXPORT void JNICALL Java_org_linphone_mediastream_MediastreamerActivity_setVideoPreviewWindowId
  (JNIEnv *env, jobject obj, jobject id, jint _args) {
	MediastreamDatas* args =  (MediastreamDatas*)_args;
#ifdef VIDEO_ENABLED
	if (!args->video)
		return;
	video_stream_set_native_preview_window_id(args->video,(unsigned long)id);
#endif
}

JNIEXPORT void JNICALL Java_org_linphone_mediastream_MediastreamerActivity_setDeviceRotation
  (JNIEnv *env, jobject thiz, jint rotation, jint _args) {
	MediastreamDatas* args =  (MediastreamDatas*)_args;
#ifdef VIDEO_ENABLED
	if (!args->video)
		return;
	video_stream_set_device_rotation(args->video, rotation);
#endif
}

JNIEXPORT void JNICALL Java_org_linphone_mediastream_MediastreamerActivity_changeCamera
  (JNIEnv *env, jobject obj, jint camId, jint _args) {
	MediastreamDatas* args =  (MediastreamDatas*)_args;
#ifdef VIDEO_ENABLED
	if (!args->video)
		return;
	char* id = (char*)malloc(15);
	snprintf(id, 15, "Android%d", camId);
	ms_message("Changing camera, trying to use: '%s'\n", id);
	video_stream_change_camera(args->video, ms_web_cam_manager_get_cam(ms_web_cam_manager_get(), id));
#endif
}

JNIEXPORT jint JNICALL Java_org_linphone_mediastream_MediastreamerActivity_stopMediaStream
  (JNIEnv *env, jobject obj) {
	ms_message("Requesting mediastream to stop\n");
	stop_handler(0);
	return 0;
}

JNIEXPORT jint JNICALL Java_org_linphone_mediastream_MediastreamerActivity_initDefaultArgs
  (JNIEnv *env, jobject obj) {
	cond = 1;
	return (unsigned int)init_default_args();
}

JNIEXPORT jboolean JNICALL Java_org_linphone_mediastream_MediastreamerActivity_parseArgs
  (JNIEnv *env, jobject obj, jint jargc, jobjectArray jargv, jint args) {
	// translate java String[] to c char*[]
	char** argv = (char**) malloc(jargc * sizeof(char*));
	int i;

	for(i=0; i<jargc; i++) {
		jstring arg = (jstring) (*env)->GetObjectArrayElement(env, jargv, i);
		const char *str = (*env)->GetStringUTFChars(env, arg, NULL);
		if (str == NULL)
			argv[i] = NULL;
		else {
			argv[i] = strdup(str);
			(*env)->ReleaseStringUTFChars(env, arg, str);
		}
	}

	bool_t result = parse_args(jargc, argv, (MediastreamDatas*)args);

	for(i=0; i<jargc; i++) {
		if (argv[i])
			free(argv[i]);
	}
	return result;
}

JNIEXPORT void JNICALL Java_org_linphone_mediastream_MediastreamerActivity_setupMediaStreams
  (JNIEnv *env, jobject obj, jint args) {
	setup_media_streams((MediastreamDatas*)args);
}

JNIEXPORT void JNICALL Java_org_linphone_mediastream_MediastreamerActivity_runLoop
  (JNIEnv *env, jobject obj, jint args) {
	run_non_interactive_loop((MediastreamDatas*)args);
}

JNIEXPORT void JNICALL Java_org_linphone_mediastream_MediastreamerActivity_clear
  (JNIEnv *env, jobject obj, jint args) {
	clear_mediastreams((MediastreamDatas*)args);
	free((MediastreamDatas*)args);
}
#endif

// HELPER METHODS
static void stop_handler(int signum)
{
	cond--;
	if (cond<0) {
		ms_error("Brutal exit (%d)\n", cond);
		exit(-1);
	}
}

static bool_t parse_addr(const char *addr, char *ip, int len, int *port)
{
	const char *semicolon=NULL;
	int iplen;
	int slen;
	const char *p;

	*port=0;
	semicolon=strchr(addr,':');
	for (p=addr+strlen(addr)-1;p>addr;p--){
		if (*p==':') {
			semicolon=p;
			break;
		}
	}
	if (semicolon==NULL) return FALSE;
	iplen=semicolon-addr;
	slen=MIN(iplen,len-1);
	strncpy(ip,addr,slen);
	ip[slen]='\0';
	*port=atoi(semicolon+1);
	return TRUE;
}

static bool_t parse_ice_addr(char *addr, char *type, int type_len, char *ip, int ip_len, int *port)
{
	char *semicolon=NULL;
	int slen;

	semicolon=strrchr(addr,':');
	if (semicolon==NULL) return FALSE;
	slen=MIN(strlen(semicolon+1),type_len);
	strncpy(type,semicolon+1,slen);
	type[slen]='\0';
	*semicolon='\0';
	return parse_addr(addr,ip,ip_len,port);
}

static void display_items(void *user_data, uint32_t csrc, rtcp_sdes_type_t t, const char *content, uint8_t content_len){
	char str[256];
	int len=MIN(sizeof(str)-1,content_len);
	strncpy(str,content,len);
	str[len]='\0';
	switch(t){
		case RTCP_SDES_CNAME:
			ms_message("Found CNAME=%s",str);
		break;
		case RTCP_SDES_TOOL:
			ms_message("Found TOOL=%s",str);
		break;
		case RTCP_SDES_NOTE:
			ms_message("Found NOTE=%s",str);
		break;
		default:
			ms_message("Unhandled SDES item (%s)",str);
	}
}

static void parse_rtcp(mblk_t *m){
	do{
		if (rtcp_is_RR(m)){
			ms_message("Receiving RTCP RR");
		}else if (rtcp_is_SR(m)){
			ms_message("Receiving RTCP SR");
		}else if (rtcp_is_SDES(m)){
			ms_message("Receiving RTCP SDES");
			rtcp_sdes_parse(m,display_items,NULL);
		}else {
			ms_message("Receiving unhandled RTCP message");
		}
	}while(rtcp_next_packet(m));
}

static void parse_events(RtpSession *session, OrtpEvQueue *q){
	OrtpEvent *ev;

	while((ev=ortp_ev_queue_get(q))!=NULL){
		OrtpEventData *d=ortp_event_get_data(ev);
		switch(ortp_event_get_type(ev)){
			case ORTP_EVENT_RTCP_PACKET_RECEIVED:
				parse_rtcp(d->packet);
			break;
			case ORTP_EVENT_RTCP_PACKET_EMITTED:
				ms_message("Jitter buffer size: %f ms",rtp_session_get_jitter_stats(session)->jitter_buffer_size_ms);
			break;
			default:
			break;
		}
		ortp_event_destroy(ev);
	}
}

static PayloadType* create_custom_payload_type(const char *type, const char *subtype, const char *rate, const char *channels, int number){
	PayloadType *pt=payload_type_new();
	if (strcasecmp(type,"audio")==0){
		pt->type=PAYLOAD_AUDIO_PACKETIZED;
	}else if (strcasecmp(type,"video")==0){
		pt->type=PAYLOAD_VIDEO;
	}else{
		fprintf(stderr,"Unsupported payload type should be audio or video, not %s\n",type);
		exit(-1);
	}
	pt->mime_type=ms_strdup(subtype);
	pt->clock_rate=atoi(rate);
	pt->channels=atoi(channels);
	return pt;	
}

static PayloadType* parse_custom_payload(const char *name){
	char type[64]={0};
	char subtype[64]={0};
	char clockrate[64]={0};
	char nchannels[64];
	char *separator;

	if (strlen(name)>=sizeof(clockrate)-1){
		fprintf(stderr,"Cannot parse %s: too long.\n",name);
		exit(-1);
	}

	separator=strchr(name,'/');
	if (separator){
		char *separator2;

		strncpy(type,name,separator-name);
		separator2=strchr(separator+1,'/');
		if (separator2){
			char *separator3;

			strncpy(subtype,separator+1,separator2-separator-1);
			separator3=strchr(separator2+1,'/');
			if (separator3){
				strncpy(clockrate,separator2+1,separator3-separator2-1);
				strcpy(nchannels,separator3+1);
			} else {
				nchannels[0]='1';
				nchannels[1]='\0';
				strcpy(clockrate,separator2+1);
			}
			fprintf(stdout,"Found custom payload type=%s, mime=%s, clockrate=%s nchannels=%s\n", type, subtype, clockrate, nchannels);
			return create_custom_payload_type(type,subtype,clockrate,nchannels,114);
		}
	}
	fprintf(stderr,"Error parsing payload name %s.\n",name);
	exit(-1);
}

static bool_t parse_window_ids(const char *ids, int* video_id, int* preview_id)
{
	char* copy = strdup(ids);
	char *semicolon=strchr(copy,':');
	if (semicolon==NULL) {
		free(copy);
		return FALSE;
	}
	*semicolon = '\0';

	*video_id=atoi(copy);
	*preview_id=atoi(semicolon+1);
	free(copy);
	return TRUE;
}

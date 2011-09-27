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
#else
#include <malloc.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __APPLE__
#include <CoreFoundation/CFRunLoop.h>
#endif

#ifdef ANDROID
#include <android/log.h>
#include <jni.h>
#endif

static int cond=1;

#ifdef VIDEO_ENABLED
static VideoStream *video=NULL;
#endif
static const char * capture_card=NULL;
static const char * playback_card=NULL;
static const char * camera=NULL;
static const char *infile=NULL,*outfile=NULL;
static float ng_threshold=-1;
static bool_t use_ng=FALSE;
static bool_t two_windows=FALSE;
static bool_t el=FALSE;
static float el_speed=-1;
static float el_thres=-1;
static float el_force=-1;
static int el_sustain=-1;
static float el_transmit_thres=-1;
static float ng_floorgain=-1;
static bool_t use_rc=FALSE;
static const char * zrtp_id=NULL;
static const char * zrtp_secrets=NULL;
static PayloadType *custom_pt=NULL;
static int video_window_id = -1;
static int preview_window_id = -1;

/* starting values echo canceller */
static int ec_len_ms=0, ec_delay_ms=0, ec_framesize=0;

static void run_media_streams(int localport, const char *remote_ip, int remoteport, int payload, const char *fmtp,
          int jitter, int bitrate, MSVideoSize vs, bool_t ec, bool_t agc, bool_t eq, int device_rotation);

static void stop_handler(int signum)
{
	cond--;
	if (cond<0) {
		ms_error("Brutal exit (%)\n", cond);
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
			default:
			break;
		}
		ortp_event_destroy(ev);
	}
}

static void create_custom_payload_type(const char *type, const char *subtype, const char *rate, int number){
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
	custom_pt=pt;
}

static void parse_custom_payload(const char *name){
	char type[64]={0};
	char subtype[64]={0};
	char clockrate[64]={0};
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
			strncpy(subtype,separator+1,separator2-separator-1);
			strcpy(clockrate,separator2+1);
			fprintf(stdout,"Found custom payload type=%s, mime=%s, clockrate=%s\n",type,subtype,clockrate);
			create_custom_payload_type(type,subtype,clockrate,114);
			return;
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
								"[ --zrtp <zid> <secrets file> (enable zrtp) ]\n"
								"[ --verbose (most verbose messages) ]\n"
								"[ --video-windows-id <video surface:preview surface>]\n"
		;

#ifdef ANDROID
//#include <execinfo.h>
int _main(int argc, char * argv[]);

static struct sigaction old_sa[NSIG];

void android_sigaction(int signal, siginfo_t *info, void *reserved)
{
	// signal crash to JAVA
	JNIEnv *env = ms_get_jni_env();
	// (*env)->CallStaticVoidMethod(env, obj, nativeCrashed);

	old_sa[signal].sa_handler(signal);
}

JNIEXPORT jint JNICALL  JNI_OnLoad(JavaVM *ajvm, void *reserved)
{
	ms_set_jvm(ajvm);

	struct sigaction handler;
	memset(&handler, 0, sizeof(sigaction));
	handler.sa_sigaction = android_sigaction;
	handler.sa_flags = SA_RESETHAND;
	#define CATCHSIG(X) sigaction(X, &handler, &old_sa[X])
	CATCHSIG(SIGILL);
	CATCHSIG(SIGABRT);
	CATCHSIG(SIGBUS);
	CATCHSIG(SIGFPE);
	CATCHSIG(SIGSEGV);
	CATCHSIG(SIGSTKFLT);
	CATCHSIG(SIGPIPE);

	return JNI_VERSION_1_2;
}

JNIEXPORT void JNICALL Java_org_linphone_mediastream_MediastreamerActivity_setVideoWindowId
  (JNIEnv *env, jobject obj, jobject id) {
#ifdef VIDEO_ENABLED
	if (!video)
		return;
	video_stream_set_native_window_id(video,(unsigned long)id);
#endif
}

JNIEXPORT void JNICALL Java_org_linphone_mediastream_MediastreamerActivity_setVideoPreviewWindowId
  (JNIEnv *env, jobject obj, jobject id) {
#ifdef VIDEO_ENABLED
	if (!video)
		return;
	video_stream_set_native_preview_window_id(video,(unsigned long)id);
#endif
}

JNIEXPORT void JNICALL Java_org_linphone_mediastream_MediastreamerActivity_setDeviceRotation
  (JNIEnv *env, jobject thiz, jint rotation) {
#ifdef VIDEO_ENABLED
	if (!video)
		return;
	video_stream_set_device_rotation(video, rotation);
#endif
}

JNIEXPORT jint JNICALL Java_org_linphone_mediastream_MediastreamerActivity_stopMediaStream
  (JNIEnv *env, jobject obj) {
	ms_message("Requesting mediastream to stop\n");
	stop_handler(0);
	return 0;
}

JNIEXPORT void JNICALL Java_org_linphone_mediastream_MediastreamerActivity_changeCamera
  (JNIEnv *env, jobject obj, jint camId) {
#ifdef VIDEO_ENABLED
	if (!video)
		return;
	char* id = (char*)malloc(15);
	snprintf(id, 15, "Android%d", camId);
	ms_message("Changing camera, trying to use: '%s'\n", id);
	video_stream_change_camera(video, ms_web_cam_manager_get_cam(ms_web_cam_manager_get(), id));
#endif
}

JNIEXPORT jint JNICALL Java_org_linphone_mediastream_MediastreamerActivity_runMediaStream
  (JNIEnv *env, jobject obj, jint jargc, jobjectArray jargv) {
	// translate java String[] to c char*[]
	char** argv = (char**) malloc(jargc * sizeof(char*));
	int i, res;

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

	res = _main(jargc, argv);

	for(i=0; i<jargc; i++) {
		if (argv[i])
			free(argv[i]);
	}
	return res;
}


int _main(int argc, char * argv[])
#else
int main(int argc, char * argv[])
#endif
{
	int i;
	int localport=0,remoteport=0,payload=0;
	char ip[50];
	const char *fmtp=NULL;
	int jitter=50;
	int bitrate=0;
	MSVideoSize vs;
	bool_t ec=FALSE;
	bool_t agc=FALSE;
	bool_t eq=FALSE;
	bool_t is_verbose=FALSE;
	int device_rotation=-1;

	cond = 1;

	if (argc<4) {
		printf("%s",usage);
		return -1;
	}

	/* default size */
	vs.width=MS_VIDEO_SIZE_CIF_W;
	vs.height=MS_VIDEO_SIZE_CIF_H;

	for (i=1;i<argc;i++){
		if (strcmp(argv[i],"--local")==0){
			i++;
			localport=atoi(argv[i]);
		}else if (strcmp(argv[i],"--remote")==0){
			i++;
			if (!parse_addr(argv[i],ip,sizeof(ip),&remoteport)) {
				printf("%s",usage);
				return -1;
			}
			printf("Remote addr: ip=%s port=%i\n",ip,remoteport);
		}else if (strcmp(argv[i],"--payload")==0){
			i++;
			if (isdigit(argv[i][0])){
				payload=atoi(argv[i]);
			}else {
				payload=114;
				parse_custom_payload(argv[i]);
			}
		}else if (strcmp(argv[i],"--fmtp")==0){
			i++;
			fmtp=argv[i];
		}else if (strcmp(argv[i],"--jitter")==0){
			i++;
			jitter=atoi(argv[i]);
		}else if (strcmp(argv[i],"--bitrate")==0){
			i++;
			bitrate=atoi(argv[i]);
		}else if (strcmp(argv[i],"--width")==0){
			i++;
			vs.width=atoi(argv[i]);
		}else if (strcmp(argv[i],"--height")==0){
			i++;
			vs.height=atoi(argv[i]);
		}else if (strcmp(argv[i],"--capture-card")==0){
			i++;
			capture_card=argv[i];
		}else if (strcmp(argv[i],"--playback-card")==0){
			i++;
			playback_card=argv[i];
		}else if (strcmp(argv[i],"--ec")==0){
			ec=TRUE;
		}else if (strcmp(argv[i],"--ec-tail")==0){
			i++;
			ec_len_ms=atoi(argv[i]);
		}else if (strcmp(argv[i],"--ec-delay")==0){
			i++;
			ec_delay_ms=atoi(argv[i]);
		}else if (strcmp(argv[i],"--ec-framesize")==0){
			i++;
			ec_framesize=atoi(argv[i]);
		}else if (strcmp(argv[i],"--agc")==0){
			agc=TRUE;
		}else if (strcmp(argv[i],"--eq")==0){
			eq=TRUE;
		}else if (strcmp(argv[i],"--ng")==0){
			use_ng=1;
		}else if (strcmp(argv[i],"--rc")==0){
			use_rc=1;
		}else if (strcmp(argv[i],"--ng-threshold")==0){
			i++;
			ng_threshold=atof(argv[i]);
		}else if (strcmp(argv[i],"--ng-floorgain")==0){
			i++;
			ng_floorgain=atof(argv[i]);
		}else if (strcmp(argv[i],"--two-windows")==0){
			two_windows=TRUE;
		}else if (strcmp(argv[i],"--infile")==0){
			i++;
			infile=argv[i];
		}else if (strcmp(argv[i],"--outfile")==0){
			i++;
			outfile=argv[i];
		}else if (strcmp(argv[i],"--camera")==0){
			i++;
			camera=argv[i];
		}else if (strcmp(argv[i],"--el")==0){
			el=TRUE;
		}else if (strcmp(argv[i],"--el-speed")==0){
			i++;
			el_speed=atof(argv[i]);
		}else if (strcmp(argv[i],"--el-thres")==0){
			i++;
			el_thres=atof(argv[i]);
		}else if (strcmp(argv[i],"--el-force")==0){
			i++;
			el_force=atof(argv[i]);
		}else if (strcmp(argv[i],"--el-sustain")==0){
			i++;
			el_sustain=atoi(argv[i]);
		}else if (strcmp(argv[i],"--el-transmit-thres")==0){
			i++;
			el_transmit_thres=atof(argv[i]);
		} else if (strcmp(argv[i],"--zrtp")==0){
			zrtp_id=argv[++i];
			zrtp_secrets=argv[++i];
		} else if (strcmp(argv[i],"--verbose")==0){
			is_verbose=TRUE;
		} else if (strcmp(argv[i], "--video-windows-id")==0) {
			i++;
			if (!parse_window_ids(argv[i],&video_window_id, &preview_window_id)) {
				printf("%s",usage);
				return -1;
			}
		} else if (strcmp(argv[i], "--device-rotation")==0) {
			i++;
			device_rotation=atoi(argv[i]);
		}else if (strcmp(argv[i],"--help")==0){
			printf("%s",usage);
			return -1;
		}
	}


	/*create the rtp session */
	ortp_init();
	if (is_verbose) {
		ortp_set_log_level_mask(ORTP_DEBUG|ORTP_MESSAGE|ORTP_WARNING|ORTP_ERROR|ORTP_FATAL);
	} else {
		ortp_set_log_level_mask(ORTP_MESSAGE|ORTP_WARNING|ORTP_ERROR|ORTP_FATAL);
	}

	rtp_profile_set_payload(&av_profile,110,&payload_type_speex_nb);
	rtp_profile_set_payload(&av_profile,111,&payload_type_speex_wb);
	rtp_profile_set_payload(&av_profile,112,&payload_type_ilbc);
	rtp_profile_set_payload(&av_profile,113,&payload_type_amr);
	rtp_profile_set_payload(&av_profile,114,custom_pt);
	rtp_profile_set_payload(&av_profile,115,&payload_type_lpc1015);
#ifdef VIDEO_ENABLED
	rtp_profile_set_payload(&av_profile,26,&payload_type_jpeg);
	rtp_profile_set_payload(&av_profile,98,&payload_type_h263_1998);
	rtp_profile_set_payload(&av_profile,97,&payload_type_theora);
	rtp_profile_set_payload(&av_profile,99,&payload_type_mp4v);
	rtp_profile_set_payload(&av_profile,100,&payload_type_x_snow);
	rtp_profile_set_payload(&av_profile,102,&payload_type_h264);
	rtp_profile_set_payload(&av_profile,103,&payload_type_vp8);
#endif
	run_media_streams(localport,ip,remoteport,payload,fmtp,jitter,bitrate,vs,ec,agc,eq,device_rotation);

	ms_exit();

#ifdef VIDEO_ENABLED
	video=NULL;
#endif

	return 0;
}

static void run_media_streams(
	int localport, 
	const char *remote_ip, 
	int remoteport, 
	int payload, 
	const char *fmtp,
	int jitter, 
	int bitrate, 
	MSVideoSize vs, 
	bool_t ec, 
	bool_t agc, 
	bool_t eq,
	int device_rotation
) {
	AudioStream *audio=NULL;
#ifdef VIDEO_ENABLED
	MSWebCam *cam=NULL;
#endif
	RtpSession *session=NULL;
	PayloadType *pt;
	RtpProfile *profile=rtp_profile_clone_full(&av_profile);
	OrtpEvQueue *q=ortp_ev_queue_new();

	ms_init();
	ms_filter_enable_statistics(TRUE);
	ms_filter_reset_statistics();


ms_message("YOUPI: %d\n", bitrate);

	signal(SIGINT,stop_handler);
	pt=rtp_profile_get_payload(profile,payload);
	if (pt==NULL){
		printf("Error: no payload defined with number %i.",payload);
		exit(-1);
	}
	if (fmtp!=NULL) payload_type_set_send_fmtp(pt,fmtp);
	if (bitrate>0) pt->normal_bitrate=bitrate;

	if (pt->type!=PAYLOAD_VIDEO){
		MSSndCardManager *manager=ms_snd_card_manager_get();
		MSSndCard *capt= capture_card==NULL ? ms_snd_card_manager_get_default_capture_card(manager) :
				ms_snd_card_manager_get_card(manager,capture_card);
		MSSndCard *play= playback_card==NULL ? ms_snd_card_manager_get_default_playback_card(manager) :
				ms_snd_card_manager_get_card(manager,playback_card);
		audio=audio_stream_new(localport,ms_is_ipv6(remote_ip));
		audio_stream_enable_automatic_gain_control(audio,agc);
		audio_stream_enable_noise_gate(audio,use_ng);
		audio_stream_set_echo_canceller_params(audio,ec_len_ms,ec_delay_ms,ec_framesize);
		audio_stream_enable_echo_limiter(audio,el);
		audio_stream_enable_adaptive_bitrate_control(audio,use_rc);
		printf("Starting audio stream.\n");

		audio_stream_start_full(audio,profile,remote_ip,remoteport,remoteport+1, payload, jitter,infile,outfile,
		                        outfile==NULL ? play : NULL ,infile==NULL ? capt : NULL,infile!=NULL ? FALSE: ec);

		if (audio) {
			if (el) {
				if (el_speed!=-1)
					ms_filter_call_method(audio->volsend,MS_VOLUME_SET_EA_SPEED,&el_speed);
				if (el_force!=-1)
					ms_filter_call_method(audio->volsend,MS_VOLUME_SET_EA_FORCE,&el_force);
				if (el_thres!=-1)
					ms_filter_call_method(audio->volsend,MS_VOLUME_SET_EA_THRESHOLD,&el_thres);
				if (el_sustain!=-1)
					ms_filter_call_method(audio->volsend,MS_VOLUME_SET_EA_SUSTAIN,&el_sustain);
				if (el_transmit_thres!=-1)
					ms_filter_call_method(audio->volsend,MS_VOLUME_SET_EA_TRANSMIT_THRESHOLD,&el_transmit_thres);

			}
			if (use_ng){
				if (ng_threshold!=-1) {
					ms_filter_call_method(audio->volsend,MS_VOLUME_SET_NOISE_GATE_THRESHOLD,&ng_threshold);
					ms_filter_call_method(audio->volrecv,MS_VOLUME_SET_NOISE_GATE_THRESHOLD,&ng_threshold);
				}
				if (ng_floorgain != -1) {
					ms_filter_call_method(audio->volsend,MS_VOLUME_SET_NOISE_GATE_FLOORGAIN,&ng_floorgain);
					ms_filter_call_method(audio->volrecv,MS_VOLUME_SET_NOISE_GATE_FLOORGAIN,&ng_floorgain);
				}
			}

			if (zrtp_id != NULL) {
				OrtpZrtpParams params;
				params.zid=zrtp_id;
				params.zid_file=zrtp_secrets;
				audio_stream_enable_zrtp(audio,&params);
			}

			session=audio->session;
		}
	}else{
#ifdef VIDEO_ENABLED
		if (eq){
			ms_fatal("Cannot put an audio equalizer in a video stream !");
			exit(-1);
		}
		printf("Starting video stream.\n");
		video=video_stream_new(localport, ms_is_ipv6(remote_ip));
#ifdef ANDROID
		if (device_rotation >= 0)
			video_stream_set_device_rotation(video, device_rotation);
#endif
		video_stream_set_sent_video_size(video,vs);
		video_stream_use_preview_video_window(video,two_windows);

		if (camera)
			cam=ms_web_cam_manager_get_cam(ms_web_cam_manager_get(),camera);
		if (cam==NULL)
			cam=ms_web_cam_manager_get_default_cam(ms_web_cam_manager_get());
		video_stream_start(video,profile,
					remote_ip,
					remoteport,remoteport+1,
					payload,
					jitter,cam
					);
		session=video->session;
#else
		printf("Error: video support not compiled.\n");
#endif
	}
	if (eq){ /*read from stdin interactive commands */
		char commands[128];
		commands[127]='\0';
		ms_sleep(1);  /* ensure following text be printed after ortp messages */
		if (eq)
		printf("\nPlease enter equalizer requests, such as 'eq active 1', 'eq active 0', 'eq 1200 0.1 200'\n");

 		while(fgets(commands,sizeof(commands)-1,stdin)!=NULL){
			int active,freq,freq_width;

			float gain;
			if (sscanf(commands,"eq active %i",&active)==1){
				audio_stream_enable_equalizer(audio,active);
				printf("OK\n");
			}else if (sscanf(commands,"eq %i %f %i",&freq,&gain,&freq_width)==3){
				audio_stream_equalizer_set_gain(audio,freq,gain,freq_width);
				printf("OK\n");
			}else if (sscanf(commands,"eq %i %f",&freq,&gain)==2){
				audio_stream_equalizer_set_gain(audio,freq,gain,0);
				printf("OK\n");
			}else if (strstr(commands,"dump")){
				int n=0,i;
				float *t;
				ms_filter_call_method(audio->equalizer,MS_EQUALIZER_GET_NUM_FREQUENCIES,&n);
				t=(float*)alloca(sizeof(float)*n);
				ms_filter_call_method(audio->equalizer,MS_EQUALIZER_DUMP_STATE,t);
				for(i=0;i<n;++i){
					if (fabs(t[i]-1)>0.01){
					printf("%i:%f:0 ",(i*pt->clock_rate)/(2*n),t[i]);
					}
				}
				printf("\nOK\n");
			} else if (strstr(commands,"quit")){
				break;
			}else printf("Cannot understand this.\n");
		}
	}else{  /* no interactive stuff - continuous debug output */
		rtp_session_register_event_queue(session,q);

		#ifdef __APPLE__
		CFRunLoopRun();
		#else
		while(cond)
		{
			int n;
			for(n=0;n<100;++n){
	#ifdef WIN32
				MSG msg;
				Sleep(10);
				while (PeekMessage(&msg, NULL, 0, 0,1)){
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
	#else
				struct timespec ts;
				ts.tv_sec=0;
				ts.tv_nsec=10000000;
				nanosleep(&ts,NULL);
	#endif
	#if defined(VIDEO_ENABLED)
				if (video) video_stream_iterate(video);
	#endif
				if (audio) audio_stream_iterate(audio);
			}
			rtp_stats_display(rtp_session_get_stats(session),"RTP stats");
			if (session){
				printf("Bandwidth usage: download=%f kbits/sec, upload=%f kbits/sec\n",
					rtp_session_compute_recv_bandwidth(session)*1e-3,
					rtp_session_compute_send_bandwidth(session)*1e-3);
				parse_events(session,q);
				printf("Quality indicator : %f\n",audio ? audio_stream_get_quality_rating(audio) : -1);
			}
		}
	#endif // target MAC
	}

	printf("stopping all...\n");
	printf("Average quality indicator: %f",audio ? audio_stream_get_average_quality_rating(audio) : -1);

	if (audio) audio_stream_stop(audio);
#ifdef VIDEO_ENABLED
	if (video) {
		video_stream_stop(video);
		ms_filter_log_statistics();
	}
#endif
	ortp_ev_queue_destroy(q);
	rtp_profile_destroy(profile);
}

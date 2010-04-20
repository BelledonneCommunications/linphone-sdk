
/*
linphone
Copyright (C) 2000  Simon MORLAT (simon.morlat@linphone.org)

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

#include "private.h"
#include "mediastreamer2/mediastream.h"
#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_SIGHANDLER_T
#include <signal.h>
#endif /*HAVE_SIGHANDLER_T*/

#include <string.h>
#if !defined(_WIN32_WCE)
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#endif /*_WIN32_WCE*/

#undef snprintf
#include <ortp/stun.h>

#ifdef HAVE_GETIFADDRS
#include <net/if.h>
#include <ifaddrs.h>
#endif


#if !defined(WIN32)

static char lock_name[80];
static char lock_set=0;
/* put a lock file in /tmp. this is called when linphone runs as a daemon*/
int set_lock_file()
{
	FILE *lockfile;

	snprintf(lock_name,80,"/tmp/linphone.%i",getuid());
	lockfile=fopen(lock_name,"w");
	if (lockfile==NULL)
	{
		printf("Failed to create lock file.\n");
		return(-1);
	}
	fprintf(lockfile,"%i",getpid());
	fclose(lockfile);
	lock_set=1;
	return(0);
}

/* looks if there is a lock file. If presents return its content (the pid of the already running linphone), if not found, returns -1*/
int get_lock_file()
{
	int pid;
	FILE *lockfile;

	snprintf(lock_name,80,"/tmp/linphone.%i",getuid());
	lockfile=fopen(lock_name,"r");
	if (lockfile==NULL)
		return(-1);
	if (fscanf(lockfile,"%i",&pid)!=1){
		ms_warning("Could not read pid in lock file.");
		fclose(lockfile);
		return -1;
	}
	fclose(lockfile);
	return pid;
}

/* remove the lock file if it was set*/
int remove_lock_file()
{
	int err=0;
	if (lock_set)
	{
		err=unlink(lock_name);
		lock_set=0;
	}
	return(err);
}

#endif

char *int2str(int number)
{
	char *numstr=ms_malloc(10);
	snprintf(numstr,10,"%i",number);
	return numstr;
}

void check_sound_device(LinphoneCore *lc)
{
#ifdef _linux
	int fd=0;
	int len;
	int a;
	char *file=NULL;
	char *i810_audio=NULL;
	char *snd_pcm_oss=NULL;
	char *snd_mixer_oss=NULL;
	char *snd_pcm=NULL;
	fd=open("/proc/modules",O_RDONLY);

	if (fd>0){
		/* read the entire /proc/modules file and check if sound conf seems correct */
		/*a=fstat(fd,&statbuf);
		if (a<0) ms_warning("Can't stat /proc/modules:%s.",strerror(errno));
		len=statbuf.st_size;
		if (len==0) ms_warning("/proc/modules has zero size!");
		*/
		/***** fstat does not work on /proc/modules for unknown reason *****/
		len=6000;
		file=ms_malloc(len+1);
		a=read(fd,file,len);
		if (a<len) file=ms_realloc(file,a+1);
		file[a]='\0';
		i810_audio=strstr(file,"i810_audio");
		if (i810_audio!=NULL){
			/* I'm sorry i put this warning in comments because
			 * i don't use yet the right driver !! */
/*			lc->vtable.display_warning(lc,_("You are currently using the i810_audio driver.\nThis driver is buggy and so does not work with Linphone.\nWe suggest that you replace it by its equivalent ALSA driver,\neither with packages from your distribution, or by downloading\nALSA drivers at http://www.alsa-project.org."));*/
			goto end;
		}
		snd_pcm=strstr(file,"snd-pcm");
		if (snd_pcm!=NULL){
			snd_pcm_oss=strstr(file,"snd-pcm-oss");
			snd_mixer_oss=strstr(file,"snd-mixer-oss");
			if (snd_pcm_oss==NULL){
				lc->vtable.display_warning(lc,_("Your computer appears to be using ALSA sound drivers.\nThis is the best choice. However the pcm oss emulation module\nis missing and linphone needs it. Please execute\n'modprobe snd-pcm-oss' as root to load it."));
			}
			if (snd_mixer_oss==NULL){
				lc->vtable.display_warning(lc,_("Your computer appears to be using ALSA sound drivers.\nThis is the best choice. However the mixer oss emulation module\nis missing and linphone needs it. Please execute\n 'modprobe snd-mixer-oss' as root to load it."));
			}
		}
	}else {

		ms_warning("Could not open /proc/modules.");
	}
	/* now check general volume. Some user forget to rise it and then complain that linphone is
	not working */
	/* but some other users complain that linphone should not change levels...
	if (lc->sound_conf.sndcard!=NULL){
		a=snd_card_get_level(lc->sound_conf.sndcard,SND_CARD_LEVEL_GENERAL);
		if (a<50){
			ms_warning("General level is quite low (%i). Linphone rises it up for you.",a);
			snd_card_set_level(lc->sound_conf.sndcard,SND_CARD_LEVEL_GENERAL,80);
		}
	}
	*/
	end:
	if (file!=NULL) ms_free(file);
	if (fd>0) close(fd);
#endif
}

#define UDP_HDR_SZ 8
#define RTP_HDR_SZ 12
#define IP4_HDR_SZ 20   /*20 is the minimum, but there may be some options*/

static void payload_type_set_enable(PayloadType *pt,int value)
{
	if ((value)!=0) payload_type_set_flag(pt,PAYLOAD_TYPE_ENABLED); \
	else payload_type_unset_flag(pt,PAYLOAD_TYPE_ENABLED);
}

static bool_t payload_type_enabled(PayloadType *pt) {
	return (((pt)->flags & PAYLOAD_TYPE_ENABLED)!=0);
}

bool_t linphone_core_payload_type_enabled(LinphoneCore *lc, PayloadType *pt){
	if (ms_list_find(lc->codecs_conf.audio_codecs,pt) || ms_list_find(lc->codecs_conf.video_codecs,pt)){
		return payload_type_enabled(pt);
	}
	ms_error("Getting enablement status of codec not in audio or video list of PayloadType !");
	return FALSE;
}

int linphone_core_enable_payload_type(LinphoneCore *lc, PayloadType *pt, bool_t enabled){
	if (ms_list_find(lc->codecs_conf.audio_codecs,pt) || ms_list_find(lc->codecs_conf.video_codecs,pt)){
		payload_type_set_enable(pt,enabled);
		return 0;
	}
	ms_error("Enabling codec not in audio or video list of PayloadType !");
	return -1;
}

const char *linphone_core_get_payload_type_description(LinphoneCore *lc, PayloadType *pt){
	if (ms_filter_codec_supported(pt->mime_type)){
		MSFilterDesc *desc=ms_filter_get_encoder(pt->mime_type);
		return _(desc->text);
	}
	return NULL;
}


/*this function makes a special case for speex/8000.
This codec is variable bitrate. The 8kbit/s mode is interesting when having a low upload bandwidth, but its quality
is not very good. We 'd better use its 15kbt/s mode when we have enough bandwidth*/
static int get_codec_bitrate(LinphoneCore *lc, const PayloadType *pt){
	int upload_bw=linphone_core_get_upload_bandwidth(lc);
	if (bandwidth_is_greater(upload_bw,129) || (bandwidth_is_greater(upload_bw,33) && !linphone_core_video_enabled(lc)) ) {
		if (strcmp(pt->mime_type,"speex")==0 && pt->clock_rate==8000){
			return 15000;
		}
	}
	return pt->normal_bitrate;
}

static double get_audio_payload_bandwidth(LinphoneCore *lc, const PayloadType *pt){
	double npacket=50;
	double packet_size;
	int bitrate;
	bitrate=get_codec_bitrate(lc,pt);
	packet_size= (((double)bitrate)/(50*8))+UDP_HDR_SZ+RTP_HDR_SZ+IP4_HDR_SZ;
	return packet_size*8.0*npacket;
}

void linphone_core_update_allocated_audio_bandwidth_in_call(LinphoneCore *lc, const PayloadType *pt){
	lc->audio_bw=(int)(get_audio_payload_bandwidth(lc,pt)/1000.0);
	/*update*/
	linphone_core_set_download_bandwidth(lc,lc->net_conf.download_bw);
	linphone_core_set_upload_bandwidth(lc,lc->net_conf.upload_bw);
}

void linphone_core_update_allocated_audio_bandwidth(LinphoneCore *lc){
	const MSList *elem;
	PayloadType *max=NULL;
	for(elem=linphone_core_get_audio_codecs(lc);elem!=NULL;elem=elem->next){
		PayloadType *pt=(PayloadType*)elem->data;
		if (payload_type_enabled(pt)){
			int pt_bitrate=get_codec_bitrate(lc,pt);
			if (max==NULL) max=pt;
			else if (max->normal_bitrate<pt_bitrate){
				max=pt;
			}
		}
	}
	if (max) {
		linphone_core_update_allocated_audio_bandwidth_in_call(lc,max);
	}
}

/* return TRUE if codec can be used with bandwidth, FALSE else*/
bool_t linphone_core_check_payload_type_usability(LinphoneCore *lc, PayloadType *pt)
{
	double codec_band;
	int min_audio_bw;
	int min_video_bw;
	bool_t ret=FALSE;
	/*
	  update allocated audio bandwidth to allocate the remaining to video.
	  This must be done outside calls, because after sdp negociation
	  the audio bandwidth is refined to the selected codec
	*/
	if (!linphone_core_in_call(lc)) linphone_core_update_allocated_audio_bandwidth(lc);
	min_audio_bw=get_min_bandwidth(linphone_core_get_download_bandwidth(lc),
					linphone_core_get_upload_bandwidth(lc));
	if (min_audio_bw==0) min_audio_bw=-1;
	min_video_bw=get_min_bandwidth(lc->dw_video_bw,lc->up_video_bw);

	switch (pt->type){
		case PAYLOAD_AUDIO_CONTINUOUS:
		case PAYLOAD_AUDIO_PACKETIZED:
			codec_band=get_audio_payload_bandwidth(lc,pt);
			ret=bandwidth_is_greater(min_audio_bw*1000,codec_band);
			/*hack to avoid using uwb codecs when having low bitrate and video*/
			if (bandwidth_is_greater(199,min_audio_bw)){
				if (linphone_core_video_enabled(lc) && pt->clock_rate>16000){
					ret=FALSE;
				}
			}
			//ms_message("Payload %s: %g",pt->mime_type,codec_band);
			break;
		case PAYLOAD_VIDEO:
			if (min_video_bw!=0) {/* infinite (-1) or strictly positive*/
				/*let the video use all the bandwidth minus the maximum bandwidth used by audio */
				if (min_video_bw>0)
					pt->normal_bitrate=min_video_bw*1000;
				else
					pt->normal_bitrate=1500000; /*around 1.5 Mbit/s*/
				ret=TRUE;
			}
			else ret=FALSE;
			break;
	}
	/*if (!ret) ms_warning("Payload %s is not usable with your internet connection.",pt->mime_type);*/

	return ret;
}

bool_t lp_spawn_command_line_sync(const char *command, char **result,int *command_ret){
#if !defined(_WIN32_WCE)
	FILE *f=popen(command,"r");
	if (f!=NULL){
		int err;
		*result=ms_malloc(4096);
		err=fread(*result,1,4096-1,f);
		if (err<0){
			ms_warning("Error reading command output:%s",strerror(errno));
			ms_free(result);
			return FALSE;
		}
		(*result)[err]=0;
		err=pclose(f);
		if (command_ret!=NULL) *command_ret=err;
		return TRUE;
	}
#endif /*_WIN32_WCE*/
	return FALSE;
}

#if defined(HAVE_GETIFADDRS) && defined(INET6)
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
bool_t host_has_ipv6_network()
{
	struct ifaddrs *ifp;
	struct ifaddrs *ifpstart;
	bool_t ipv6_present=FALSE;

	if (getifaddrs (&ifpstart) < 0)
	{
		return FALSE;
	}

	for (ifp=ifpstart; ifp != NULL; ifp = ifp->ifa_next)
	{
		if (!ifp->ifa_addr)
		  continue;

		switch (ifp->ifa_addr->sa_family) {
		case AF_INET:

			break;
		case AF_INET6:
		    ipv6_present=TRUE;
			break;
		default:
		        continue;
  		}
	}

	freeifaddrs (ifpstart);

	return ipv6_present;
}
#else

bool_t host_has_ipv6_network()
{
	return FALSE;
}


#endif

static ortp_socket_t create_socket(int local_port){
	struct sockaddr_in laddr;
	ortp_socket_t sock;
	int optval;
	sock=socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP);
	if (sock<0) {
		ms_error("Fail to create socket");
		return -1;
	}
	memset (&laddr,0,sizeof(laddr));
	laddr.sin_family=AF_INET;
	laddr.sin_addr.s_addr=INADDR_ANY;
	laddr.sin_port=htons(local_port);
	if (bind(sock,(struct sockaddr*)&laddr,sizeof(laddr))<0){
		ms_error("Bind socket to 0.0.0.0:%i failed: %s",local_port,getSocketError());
		close_socket(sock);
		return -1;
	}
	optval=1;
	if (setsockopt (sock, SOL_SOCKET, SO_REUSEADDR,
				(SOCKET_OPTION_VALUE)&optval, sizeof (optval))<0){
		ms_warning("Fail to set SO_REUSEADDR");
	}
	set_non_blocking_socket(sock);
	return sock;
}

static int sendStunRequest(int sock, const struct sockaddr *server, socklen_t addrlen, int id, bool_t changeAddr){
	char buf[STUN_MAX_MESSAGE_SIZE];
	int len = STUN_MAX_MESSAGE_SIZE;
	StunAtrString username;
   	StunAtrString password;
	StunMessage req;
	int err;
	memset(&req, 0, sizeof(StunMessage));
	memset(&username,0,sizeof(username));
	memset(&password,0,sizeof(password));
	stunBuildReqSimple( &req, &username, changeAddr , changeAddr , id);
	len = stunEncodeMessage( &req, buf, len, &password);
	if (len<=0){
		ms_error("Fail to encode stun message.");
		return -1;
	}
	err=sendto(sock,buf,len,0,server,addrlen);
	if (err<0){
		ms_error("sendto failed: %s",strerror(errno));
		return -1;
	}
	return 0;
}

static int parse_stun_server_addr(const char *server, struct sockaddr_storage *ss, socklen_t *socklen){
	struct addrinfo hints,*res=NULL;
	int ret;
	const char *port;
	char host[NI_MAXHOST];
	char *p;
	host[NI_MAXHOST-1]='\0';
	strncpy(host,server,sizeof(host)-1);
	p=strchr(host,':');
	if (p) {
		*p='\0';
		port=p+1;
	}else port="3478";
	memset(&hints,0,sizeof(hints));
	hints.ai_family=PF_INET;
	hints.ai_socktype=SOCK_DGRAM;
	hints.ai_protocol=IPPROTO_UDP;
	ret=getaddrinfo(host,port,&hints,&res);
	if (ret!=0){
		ms_error("getaddrinfo() failed for %s:%s : %s",host,port,gai_strerror(ret));
		return -1;
	}
	if (!res) return -1;
	memcpy(ss,res->ai_addr,res->ai_addrlen);
	*socklen=res->ai_addrlen;
	freeaddrinfo(res);
	return 0;
}

static int recvStunResponse(ortp_socket_t sock, char *ipaddr, int *port, int *id){
	char buf[STUN_MAX_MESSAGE_SIZE];
   	int len = STUN_MAX_MESSAGE_SIZE;
	StunMessage resp;
	len=recv(sock,buf,len,0);
	if (len>0){
		struct in_addr ia;
		stunParseMessage(buf,len, &resp );
		*id=resp.msgHdr.tr_id.octet[0];
		*port = resp.mappedAddress.ipv4.port;
		ia.s_addr=htonl(resp.mappedAddress.ipv4.addr);
		strncpy(ipaddr,inet_ntoa(ia),LINPHONE_IPADDR_SIZE);
	}
	return len;
}

void linphone_core_run_stun_tests(LinphoneCore *lc, LinphoneCall *call){
	const char *server=linphone_core_get_stun_server(lc);

	if (lc->sip_conf.ipv6_enabled){
		ms_warning("stun support is not implemented for ipv6");
		return;
	}
	if (server!=NULL){
		struct sockaddr_storage ss;
		socklen_t ss_len;
		ortp_socket_t sock1=-1, sock2=-1;
		bool_t video_enabled=linphone_core_video_enabled(lc);
		bool_t got_audio,got_video;
		bool_t cone_audio=FALSE,cone_video=FALSE;
		struct timeval init,cur;
		SalEndpointCandidate *ac,*vc;
		
		ac=&call->localdesc->streams[0].candidates[0];
		vc=&call->localdesc->streams[1].candidates[0];
		
		if (parse_stun_server_addr(server,&ss,&ss_len)<0){
			ms_error("Fail to parser stun server address: %s",server);
			return;
		}
		if (lc->vtable.display_status!=NULL)
			lc->vtable.display_status(lc,_("Stun lookup in progress..."));

		/*create the two audio and video RTP sockets, and send STUN message to our stun server */
		sock1=create_socket(linphone_core_get_audio_port(lc));
		if (sock1<0) return;
		if (video_enabled){
			sock2=create_socket(linphone_core_get_video_port(lc));
			if (sock2<0) return ;
		}
		sendStunRequest(sock1,(struct sockaddr*)&ss,ss_len,11,TRUE);
		sendStunRequest(sock1,(struct sockaddr*)&ss,ss_len,1,FALSE);
		if (sock2>=0){
			sendStunRequest(sock2,(struct sockaddr*)&ss,ss_len,22,TRUE);
			sendStunRequest(sock2,(struct sockaddr*)&ss,ss_len,2,FALSE);
		}
		got_audio=FALSE;
		got_video=FALSE;
		gettimeofday(&init,NULL);
		do{
			double elapsed;
			int id;
#ifdef WIN32
			Sleep(10);
#else
			usleep(10000);
#endif

			if (recvStunResponse(sock1,ac->addr,
						&ac->port,&id)>0){
				ms_message("STUN test result: local audio port maps to %s:%i",
						ac->addr,
						ac->port);
				if (id==11)
					cone_audio=TRUE;
				got_audio=TRUE;
			}
			if (recvStunResponse(sock2,vc->addr,
							&vc->port,&id)>0){
				ms_message("STUN test result: local video port maps to %s:%i",
					vc->addr,
					vc->port);
				if (id==22)
					cone_video=TRUE;
				got_video=TRUE;
			}
			gettimeofday(&cur,NULL);
			elapsed=((cur.tv_sec-init.tv_sec)*1000.0) +  ((cur.tv_usec-init.tv_usec)/1000.0);
			if (elapsed>2000)  break;
		}while(!(got_audio && (got_video||sock2<0)  ) );
		if (!got_audio){
			ms_error("No stun server response for audio port.");
		}else{
			if (!cone_audio) {
				ms_warning("NAT is symmetric for audio port");
				ac->addr[0]='\0';
				ac->port=0;
			}
		}
		if (sock2>=0){
			if (!got_video){
				ms_error("No stun server response for video port.");
			}else{
				if (!cone_video) {
					ms_warning("NAT is symmetric for video port.");
					vc->addr[0]='\0';
					vc->port=0;
				}
			}
		}
		close_socket(sock1);
		if (sock2>=0) close_socket(sock2);
	}
}

static int extract_sip_port(const char *config){
	char line[512];
	char port[12];
	int ret=-1;
	FILE *f=fopen(config,"r");
	if (f){
		while(fgets(line,sizeof(line),f)!=NULL){
			if (fmtp_get_value(line,"sip_port",port,sizeof(port))){
				ret=atoi(port);
			}
		}
		fclose(f);
	}
	return ret;
}

int linphone_core_wake_up_possible_already_running_instance(
    const char * config_file, const char * addr_to_call)
{
	int port=extract_sip_port(config_file);
	const char *wakeup="WAKEUP sip:127.0.0.1 SIP/2.0\r\n"
		"Via: SIP/2.0/UDP 127.0.0.1:%i;rport;branch=z9hG4bK%u\r\n"
		"From: <sip:another_linphone@127.0.0.1>;tag=%u\r\n"
		"To:   <sip:you@127.0.0.1>\r\n"
		"CSeq: 1 WAKEUP\r\n"
		"Call-ID: %u@onsantape\r\n"
		"Content-length: 0\r\n\r\n";
	const char * call = "REFER sip:127.0.0.1 SIP/2.0\r\n"
		"Via: SIP/2.0/UDP 127.0.0.1:%i;rport;branch=z9hG4bK%u\r\n"
		"From: <sip:another_linphone@127.0.0.1>;tag=%u\r\n"
		"To:   <sip:you@127.0.0.1>\r\n"
		"Refer-To: %s\r\n"
		"CSeq: 1 WAKEUP\r\n"
		"Call-ID: %u@onsantape\r\n"
		"Content-length: 0\r\n\r\n";

	/*make sure ortp is initialized (it initializes win32 socket api)*/
	ortp_init();
	if (port>0){
		struct sockaddr_storage ss;
		socklen_t sslen;
		char tmp[100];
		snprintf(tmp,sizeof(tmp),"127.0.0.1:%i",port);
		if (parse_stun_server_addr(tmp,&ss,&sslen)==0){
			int locport=57123;
			ortp_socket_t sock=create_socket(locport);
			if (sock<0) sock=create_socket(++locport);
			if (sock>=0){
				char req[512];
				if (addr_to_call != NULL)
					snprintf(req, sizeof(req), call, locport,
			     		random(), random(), addr_to_call, random());
				else
					snprintf(req, sizeof(req), wakeup, locport,
			     		random(), random(), random());
				if (connect(sock,(struct sockaddr*)&ss,sslen)<0){
					fprintf(stderr,"connect failed: %s\n",getSocketError());
				}else if (send(sock,req,strlen(req),0)>0){
					/*wait a bit for a response*/
					int i;
					for(i=0;i<10;++i){
						if (recv(sock,req,sizeof(req),0)>0){
							close_socket(sock);
							return 0;
						}else if (getSocketErrorCode()!=EWOULDBLOCK){
							break;
						}
#ifdef WIN32
						Sleep(100);
#else
						usleep(100000);
#endif
					}
				}else{
					ms_message("sendto() of WAKEUP request failed, nobody to wakeup.");
				}
			}
			close_socket(sock);
		}
	}
	return -1;
}

#ifdef HAVE_GETIFADDRS

#include <ifaddrs.h>
static int get_local_ip_with_getifaddrs(int type, char *address, int size)
{
	struct ifaddrs *ifp;
	struct ifaddrs *ifpstart;
	int ret = 0;

	if (getifaddrs(&ifpstart) < 0) {
		return -1;
	}

	for (ifp = ifpstart; ifp != NULL; ifp = ifp->ifa_next) {
		if (ifp->ifa_addr && ifp->ifa_addr->sa_family == type
			&& (ifp->ifa_flags & IFF_RUNNING) && !(ifp->ifa_flags & IFF_LOOPBACK))
		{
			getnameinfo(ifp->ifa_addr,
						(type == AF_INET6) ?
						sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in),
						address, size, NULL, 0, NI_NUMERICHOST);
			if (strchr(address, '%') == NULL) {	/*avoid ipv6 link-local addresses */
				/*ms_message("getifaddrs() found %s",address);*/
				ret++;
			}
		}
	}
	freeifaddrs(ifpstart);
	return ret;
}
#endif


static int get_local_ip_for_with_connect(const char *dest, char *result){
	int err,tmp;
	struct addrinfo hints;
	struct addrinfo *res=NULL;
	struct sockaddr_storage addr;
	struct sockaddr *p_addr=(struct sockaddr*)&addr;
	ortp_socket_t sock;
	socklen_t s;

	memset(&hints,0,sizeof(hints));
	hints.ai_family=PF_UNSPEC;
	hints.ai_socktype=SOCK_DGRAM;
	/*hints.ai_flags=AI_NUMERICHOST|AI_CANONNAME;*/
	err=getaddrinfo(dest,"5060",&hints,&res);
	if (err!=0){
		ms_error("getaddrinfo() error: %s",gai_strerror(err));
		return -1;
	}
	if (res==NULL){
		ms_error("bug: getaddrinfo returned nothing.");
		return -1;
	}
	sock=socket(res->ai_family,SOCK_DGRAM,0);
	tmp=1;
	err=setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(SOCKET_OPTION_VALUE)&tmp,sizeof(int));
	if (err<0){
		ms_warning("Error in setsockopt: %s",strerror(errno));
	}
	err=connect(sock,res->ai_addr,res->ai_addrlen);
	if (err<0) {
		ms_error("Error in connect: %s",strerror(errno));
 		freeaddrinfo(res);
 		close_socket(sock);
		return -1;
	}
	freeaddrinfo(res);
	res=NULL;
	s=sizeof(addr);
	err=getsockname(sock,(struct sockaddr*)&addr,&s);
	if (err!=0) {
		ms_error("Error in getsockname: %s",strerror(errno));
		close_socket(sock);
		return -1;
	}
	if (p_addr->sa_family==AF_INET){
		struct sockaddr_in *p_sin=(struct sockaddr_in*)p_addr;
		if (p_sin->sin_addr.s_addr==0){
			close_socket(sock);
			return -1;
		}
	}
	err=getnameinfo((struct sockaddr *)&addr,s,result,LINPHONE_IPADDR_SIZE,NULL,0,NI_NUMERICHOST);
	if (err!=0){
		ms_error("getnameinfo error: %s",strerror(errno));
	}
	close_socket(sock);
	ms_message("Local interface to reach %s is %s.",dest,result);
	return 0;
}

int linphone_core_get_local_ip_for(int type, const char *dest, char *result){
	if (dest==NULL) {
		if (type==AF_INET)
			dest="87.98.157.38"; /*a public IP address*/
		else dest="2a00:1450:8002::68";
	}
	strcpy(result,type==AF_INET ? "127.0.0.1" : "::1");
#ifdef HAVE_GETIFADDRS
	{
		int found_ifs;
	
		found_ifs=get_local_ip_with_getifaddrs(type,result,LINPHONE_IPADDR_SIZE);
		if (found_ifs==1){
			return 0;
		}else if (found_ifs<=0){
			/*absolutely no network on this machine */
			return -1;
		}
	}
#endif
	/*else use connect to find the best local ip address */
	return get_local_ip_for_with_connect(dest,result);
}

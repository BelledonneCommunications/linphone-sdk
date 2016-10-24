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

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "ffmpeg-priv.h"

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/mscodecutils.h"

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <netinet/in.h>			/* ntohl(3) */
#endif

#include "rfc2429.h"


#if LIBAVCODEC_VERSION_MAJOR >= 57

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#else
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#endif

#define RATE_CONTROL_MARGIN 15000 /*bits/second*/

#define MS_VIDEOENC_CONF(required_bitrate, bitrate_limit, resolution, fps, cpu, qminvalue) \
	{ required_bitrate, bitrate_limit, { MS_VIDEO_SIZE_ ## resolution ## _W, MS_VIDEO_SIZE_ ## resolution ## _H }, fps, cpu, (void *)&qmin ## qminvalue }

static bool_t avcodec_initialized=FALSE;

static const int qmin2 = 2;
static const int qmin3 = 3;
#if HAVE_AVCODEC_SNOW
static const int qmin4 = 4;
#endif
static const int qmin5 = 5;

static const MSVideoConfiguration h263_conf_list[] = {
	MS_VIDEOENC_CONF( 800000, 1024000,  4CIF, 25, 2, 2),
	MS_VIDEOENC_CONF( 512000,  800000,  CIF, 25, 1, 2),
	MS_VIDEOENC_CONF( 256000,  512000,  CIF, 17, 1, 3),
	MS_VIDEOENC_CONF( 128000,  256000, QCIF, 10, 1, 3),
	MS_VIDEOENC_CONF(      0,  128000, QCIF,  5, 1, 5)
};

static const MSVideoConfiguration h263p_conf_list[] = {
	MS_VIDEOENC_CONF(800000, 1024000, 4CIF, 25, 2, 2),
	MS_VIDEOENC_CONF(512000,  800000,  CIF, 25, 1, 2),
	MS_VIDEOENC_CONF(256000,  512000,  CIF, 17, 1, 3),
	MS_VIDEOENC_CONF(128000,  256000, QCIF, 10, 1, 3),
	MS_VIDEOENC_CONF(     0,  128000, QCIF,  5, 1, 5)
};

static const MSVideoConfiguration mjpeg_conf_list[] = {
	MS_VIDEOENC_CONF(1024000, 1536000, SVGA, 25, 1, 2),
	MS_VIDEOENC_CONF( 800000, 1024000,  VGA, 25, 1, 2),
	MS_VIDEOENC_CONF( 512000,  800000,  CIF, 25, 1, 2),
	MS_VIDEOENC_CONF( 256000,  512000,  CIF, 17, 1, 3),
	MS_VIDEOENC_CONF( 170000,  256000, QVGA, 15, 1, 3),
	MS_VIDEOENC_CONF( 128000,  170000, QCIF, 10, 1, 3),
	MS_VIDEOENC_CONF(      0,  128000, QCIF,  5, 1, 5)
};

static const MSVideoConfiguration mpeg4_conf_list[] = {
    	MS_VIDEOENC_CONF(2048000, 2560000, SXGA_MINUS, 25, 4, 2),
	MS_VIDEOENC_CONF(2048000, 2560000, 720P, 25, 4, 2),
    	MS_VIDEOENC_CONF(1536000, 2048000,  XGA, 25, 4, 2),
	MS_VIDEOENC_CONF(1024000, 1536000, SVGA, 25, 2, 2),
	MS_VIDEOENC_CONF( 800000, 1024000,  VGA, 25, 1, 2),
	MS_VIDEOENC_CONF( 512000,  800000,  CIF, 25, 1, 2),
	MS_VIDEOENC_CONF( 256000,  512000,  CIF, 17, 1, 3),
	MS_VIDEOENC_CONF( 170000,  256000, QVGA, 15, 1, 3),
	MS_VIDEOENC_CONF( 128000,  170000, QCIF, 10, 1, 3),
	MS_VIDEOENC_CONF(      0,  128000, QCIF,  5, 1, 5)
};

#if HAVE_AVCODEC_SNOW
static const MSVideoConfiguration snow_conf_list[] = {
	MS_VIDEOENC_CONF(1024000, 1536000, SVGA, 25, 2, 2),
	MS_VIDEOENC_CONF( 800000, 1024000,  VGA, 25, 1, 2),
	MS_VIDEOENC_CONF( 512000,  800000,  CIF, 25, 1, 2),
	MS_VIDEOENC_CONF( 256000,  512000,  CIF, 17, 1, 3),
	MS_VIDEOENC_CONF( 170000,  256000, QVGA, 15, 1, 3),
	MS_VIDEOENC_CONF( 128000,  170000, QCIF, 10, 1, 3),
	MS_VIDEOENC_CONF(  64000,  128000, QCIF,  7, 1, 4),
	MS_VIDEOENC_CONF(      0,   64000, QCIF,  5, 1, 5)
};
#endif

#ifndef FF_I_TYPE
#define FF_I_TYPE AV_PICTURE_TYPE_I
#endif

#ifdef ENABLE_LOG_FFMPEG

void ms_ffmpeg_log_callback(void* ptr, int level, const char* fmt, va_list vl)
{
	ortp_logv(ORTP_MESSAGE,fmt,vl);
}

#endif

void ms_ffmpeg_check_init(){
	if(!avcodec_initialized){
		avcodec_register_all();
		avcodec_initialized=TRUE;
#ifdef ENABLE_LOG_FFMPEG
		av_log_set_level(AV_LOG_WARNING);
		av_log_set_callback(&ms_ffmpeg_log_callback);
#endif
	}
}

typedef struct EncState{
	AVCodecContext av_context;
	AVCodec *av_codec;
	AVFrame* pict;
	enum CodecID codec;
	mblk_t *comp_buf;
	int mtu;	/* network maximum transmission unit in bytes */
	int profile;
	int qmin;
	uint32_t framenum;
	MSVideoStarter starter;
	bool_t req_vfu;
	const MSVideoConfiguration *vconf_list;
	MSVideoConfiguration vconf;
}EncState;

static bool_t parse_video_fmtp(const char *fmtp, float *fps, MSVideoSize *vsize){
	char *tmp=ms_strdup(fmtp);
	char *semicolon;
	char *equal;
	bool_t ret=TRUE;

	ms_message("parsing %s",fmtp);
	/*extract fisrt pair */
	if ((semicolon=strchr(tmp,';'))!=NULL){
		*semicolon='\0';
	}
	if ((equal=strchr(tmp,'='))!=NULL){
		int divider;
		*equal='\0';
		if (strcasecmp(tmp,"CIF")==0){
			if (vsize->width>=MS_VIDEO_SIZE_CIF_W){
				vsize->width=MS_VIDEO_SIZE_CIF_W;
				vsize->height=MS_VIDEO_SIZE_CIF_H;
			}
		}else if (strcasecmp(tmp,"QCIF")==0){
			vsize->width=MS_VIDEO_SIZE_QCIF_W;
			vsize->height=MS_VIDEO_SIZE_QCIF_H;
		}else{
			ms_warning("unsupported video size %s",tmp);
			ret=FALSE;
			goto end;
		}
		divider=atoi(equal+1);
		if (divider!=0){
			float newfps=29.97f/divider;
			if (*fps>newfps) *fps=newfps;
		}else{
			ms_warning("Could not find video fps");
			ret=FALSE;
		}
	}else ret=FALSE;

end:
	ms_free(tmp);
	return ret;
}

static int enc_add_fmtp(MSFilter *f,void *arg){
	EncState *s=(EncState*)f->data;
	const char *fmtp=(const char*)arg;
	char val[10];
	if (fmtp_get_value(fmtp,"profile",val,sizeof(val))){
		s->profile=atoi(val);
	}else parse_video_fmtp(fmtp,&s->vconf.fps,&s->vconf.vsize);
	return 0;
}

static int enc_req_vfu(MSFilter *f, void *unused){
	EncState *s=(EncState*)f->data;
	s->req_vfu=TRUE;
	return 0;
}

static const MSVideoConfiguration * get_vconf_list(EncState *s) {
	switch (s->codec) {
		case CODEC_ID_H263:
			return  &h263_conf_list[0];
		case CODEC_ID_H263P:
			return &h263p_conf_list[0];
		case CODEC_ID_MJPEG:
			return &mjpeg_conf_list[0];
		case CODEC_ID_MPEG4:
		default:
			return &mpeg4_conf_list[0];
#if HAVE_AVCODEC_SNOW
		case CODEC_ID_SNOW:
			return &snow_conf_list[0];
#endif
	}
}

static void enc_init(MSFilter *f, enum CodecID codec)
{
	EncState *s=ms_new0(EncState,1);
	f->data=s;
	ms_ffmpeg_check_init();
	s->profile=0;/*always default to profile 0*/
	s->comp_buf=NULL;
	s->mtu=ms_factory_get_payload_max_size(f->factory)-2;/*-2 for the H263 payload header*/
//	s->mtu=ms_get_payload_max_size()-2;/*-2 for the H263 payload header*/
	s->codec=codec;
	s->qmin=2;
	s->req_vfu=FALSE;
	s->framenum=0;
	s->av_context.codec=NULL;
	s->vconf_list = get_vconf_list(s);
	s->vconf = ms_video_find_best_configuration_for_bitrate(s->vconf_list, 500000,ms_factory_get_cpu_count(f->factory));
	s->pict = av_frame_alloc();
}

static void enc_h263_init(MSFilter *f){
	enc_init(f,CODEC_ID_H263P);
}

static void enc_mpeg4_init(MSFilter *f){
	enc_init(f,CODEC_ID_MPEG4);
}
#if HAVE_AVCODEC_SNOW
static void enc_snow_init(MSFilter *f){
	enc_init(f,CODEC_ID_SNOW);
}
#endif
static void enc_mjpeg_init(MSFilter *f){
	enc_init(f,CODEC_ID_MJPEG);
}

static void prepare(EncState *s){
	AVCodecContext *c=&s->av_context;
	const int max_br_vbv=128000;

	avcodec_get_context_defaults3(c, NULL);
	if (s->codec==CODEC_ID_MJPEG)
	{
		ms_message("Codec bitrate set to %i",(int)c->bit_rate);
		c->width = s->vconf.vsize.width;
		c->height = s->vconf.vsize.height;
		c->time_base.num = 1;
		c->time_base.den = (int)s->vconf.fps;
		c->gop_size=(int)s->vconf.fps*5; /*emit I frame every 5 seconds*/
		c->pix_fmt=AV_PIX_FMT_YUVJ420P;
		s->comp_buf=allocb(c->bit_rate*2,0);
		return;
	}

	/* put codec parameters */
	/* in order to take in account RTP protocol overhead and avoid possible
	 bitrate peaks especially on low bandwidth, we make a correction on the
	 codec's target bitrate.
	*/
	c->bit_rate=(int)((float)s->vconf.required_bitrate*0.92f);
	if (c->bit_rate>RATE_CONTROL_MARGIN){
		 c->bit_rate -= RATE_CONTROL_MARGIN;
	}
	c->bit_rate_tolerance=s->vconf.fps>1.0f?(int)((float)c->bit_rate/(s->vconf.fps-1.0f)):c->bit_rate;

	/* ffmpeg vbv rate control consumes too much cpu above a certain target bitrate.
	We don't use it above max_br_vbv */
	if (
#if HAVE_AVCODEC_SNOW
		s->codec!=CODEC_ID_SNOW &&
#endif
		s->vconf.required_bitrate<max_br_vbv){
		/*snow does not like 1st pass rate control*/
		c->rc_max_rate=c->bit_rate;
		c->rc_min_rate=0;
		c->rc_buffer_size=c->rc_max_rate;
	}else{
		/*use qmin instead*/
		c->qmin=s->qmin;
	}

	c->width = s->vconf.vsize.width;
	c->height = s->vconf.vsize.height;
	c->time_base.num = 1;
	c->time_base.den = (int)s->vconf.fps;
	c->gop_size=(int)s->vconf.fps*10; /*emit I frame every 10 seconds*/
	c->pix_fmt=AV_PIX_FMT_YUV420P;
	s->comp_buf=allocb(c->bit_rate*2,0);
#if HAVE_AVCODEC_SNOW
	if (s->codec==CODEC_ID_SNOW){
		c->strict_std_compliance=-2;
	}
#endif
	ms_message("Codec size set to w=%i/h=%i, bitrate=%i",c->width, c->height, (int)c->bit_rate);

}

static void prepare_h263(EncState *s){
	AVCodecContext *c=&s->av_context;
	/* we don't use the rtp_callback but use rtp_mode that forces ffmpeg to insert
	Start Codes as much as possible in the bitstream */
#if LIBAVCODEC_VERSION_INT < ((52<<16)+(0<<8)+0)
        c->rtp_mode = 1;
#endif
	c->rtp_payload_size = s->mtu/2;
	if (s->profile==0){
		s->codec=CODEC_ID_H263;
	}else{
		/*
		c->flags|=CODEC_FLAG_H263P_UMV;
		c->flags|=CODEC_FLAG_AC_PRED;
		c->flags|=CODEC_FLAG_H263P_SLICE_STRUCT;
		c->flags|=CODEC_FLAG_OBMC;
		c->flags|=CODEC_FLAG_AC_PRED;
		*/
		s->codec=CODEC_ID_H263P;
	}
}

static void prepare_mpeg4(EncState *s){
	AVCodecContext *c=&s->av_context;
	c->max_b_frames=0; /*don't use b frames*/
}

static void enc_uninit(MSFilter  *f){
	EncState *s=(EncState*)f->data;
	if (s->pict) av_frame_free(&s->pict);
	ms_free(s);
}

static void enc_preprocess(MSFilter *f){
	EncState *s=(EncState*)f->data;
	int error;
	prepare(s);
	if (s->codec==CODEC_ID_H263P || s->codec==CODEC_ID_H263)
		prepare_h263(s);
	else if (s->codec==CODEC_ID_MPEG4)
		prepare_mpeg4(s);
#if HAVE_AVCODEC_SNOW
	else if (s->codec==CODEC_ID_SNOW){
		/**/
	}
#endif
	else if (s->codec==CODEC_ID_MJPEG){
		/**/
	}else {
		ms_error("Unsupported codec id %i",s->codec);
		return;
	}
	s->av_codec=avcodec_find_encoder(s->codec);
	if (s->av_codec==NULL){
		ms_error("could not find encoder for codec id %i",s->codec);
		return;
	}
	error=avcodec_open2(&s->av_context, s->av_codec, NULL);
	if (error!=0) {
		ms_error("avcodec_open() failed: %i",error);
		return;
	}
	ms_video_starter_init(&s->starter);
	ms_debug("image format is %i.",s->av_context.pix_fmt);
	ms_message("qmin=%i qmax=%i",s->av_context.qmin,s->av_context.qmax);
	s->framenum=0;
}

static void enc_postprocess(MSFilter *f){
	EncState *s=(EncState*)f->data;
	if (s->av_context.codec!=NULL){
		avcodec_close(&s->av_context);
		s->av_context.codec=NULL;
	}
	if (s->comp_buf!=NULL)	{
		freemsg(s->comp_buf);
		s->comp_buf=NULL;
	}
}

static void add_rfc2190_header(mblk_t **packet, AVCodecContext *context, bool_t is_iframe){
	mblk_t *header;
	header = allocb(4, 0);
	memset(header->b_wptr, 0, 4);
	// assume video size is CIF or QCIF
	if (context->width == 352 && context->height == 288) header->b_wptr[1] = 0x60;
	else header->b_wptr[1] = 0x40;
	if (is_iframe == TRUE) header->b_wptr[1] |= 0x10;
	header->b_wptr += 4;
	header->b_cont = *packet;
	*packet = header;
}

#if 0
static int get_gbsc(uint8_t *psc, uint8_t *end)
{
	int len = end-psc;
	uint32_t buf;
	int i, j, k;
	k = len;
	for (i = 2; i < len-4; i++) {
		buf = *((uint32_t *)(psc+i));
		for (j = 0; j < 8; j++) {
			if (((buf >> j) & 0x00FCFFFF) == 0x00800000) {/*PSC*/
				i += 2;
				k=i;
				break;
			} else if (((buf >> j) & 0x0080FFFF) == 0x00800000) {/*GBSC*/
				i += 2;
				k = i;
				break;
			}
		}
	}
	return k;
}
#else
static int get_gbsc_bytealigned(uint8_t *begin, uint8_t *end){
	int i;
	int len = end - begin;
	for (i = len - 2;  /*len + length of scan window*/
	   i > 2 + 2; /*length of scan window + 2 avoidance of 1st gob or psc*/
	   i--){
		if(*(begin + i) == 0 &&
		   *(begin + i+1) == 0 &&
		   (*(begin + i+2) & 0x80) == 0x80){
		  /*ms_message("JV psc/gob found! %2x %2x %2x", *(begin + i), *(begin + i+1), *(begin + i + 2));*/
		  return i;
		}
	}
	/*ms_message("JV no psc or gob found!");*/
	return len;
}
#endif

static void rfc2190_generate_packets(MSFilter *f, EncState *s, mblk_t *frame, uint32_t timestamp, bool_t is_iframe){
	mblk_t *packet=NULL;

	while (frame->b_rptr<frame->b_wptr){
		packet=dupb(frame);
		/*frame->b_rptr=packet->b_wptr=packet->b_rptr+get_gbsc(packet->b_rptr, MIN(packet->b_rptr+s->mtu,frame->b_wptr));*/
		frame->b_rptr = packet->b_wptr =
			packet->b_rptr + get_gbsc_bytealigned(packet->b_rptr, MIN(packet->b_rptr+s->mtu,frame->b_wptr));
		add_rfc2190_header(&packet, &s->av_context ,is_iframe);
		mblk_set_timestamp_info(packet,timestamp);
		ms_queue_put(f->outputs[0],packet);
	}
	/* the marker bit is set on the last packet, if any.*/
	mblk_set_marker_info(packet,TRUE);
}

static void mpeg4_fragment_and_send(MSFilter *f,EncState *s,mblk_t *frame, uint32_t timestamp){
	uint8_t *rptr;
	mblk_t *packet=NULL;
	int len;
	for (rptr=frame->b_rptr;rptr<frame->b_wptr;){
		len=MIN(s->mtu,(frame->b_wptr-rptr));
		packet=dupb(frame);
		packet->b_rptr=rptr;
		packet->b_wptr=rptr+len;
		mblk_set_timestamp_info(packet,timestamp);
		ms_queue_put(f->outputs[0],packet);
		rptr+=len;
	}
	/*set marker bit on last packet*/
	mblk_set_marker_info(packet,TRUE);
}

static void rfc4629_generate_follow_on_packets(MSFilter *f, EncState *s, mblk_t *frame, uint32_t timestamp, uint8_t *psc, uint8_t *end, bool_t last_packet){
	mblk_t *packet;
	int len=end-psc;

	packet=dupb(frame);
	packet->b_rptr=psc;
	packet->b_wptr=end;
	/*ms_message("generating packet of size %i",end-psc);*/
	rfc2429_set_P(psc,1);
	mblk_set_timestamp_info(packet,timestamp);


	if (len>s->mtu){
		/*need to slit the packet using "follow-on" packets */
		/*compute the number of packets need (rounded up)*/
		int num=(len+s->mtu-1)/s->mtu;
		int i;
		uint8_t *pos;
		/*adjust the first packet generated*/
		pos=packet->b_wptr=packet->b_rptr+s->mtu;
		ms_queue_put(f->outputs[0],packet);
		ms_debug("generating %i follow-on packets",num);
		for (i=1;i<num;++i){
			mblk_t *header;
			packet=dupb(frame);
			packet->b_rptr=pos;
			pos=packet->b_wptr=MIN(pos+s->mtu,end);
			header=allocb(2,0);
			header->b_wptr[0]=0;
			header->b_wptr[1]=0;
			header->b_wptr+=2;
			/*no P bit is set */
			header->b_cont=packet;
			packet=header;
			mblk_set_timestamp_info(packet,timestamp);
			ms_queue_put(f->outputs[0],packet);
		}
	}else ms_queue_put(f->outputs[0],packet);
	/* the marker bit is set on the last packet, if any.*/
	mblk_set_marker_info(packet,last_packet);
}

/* returns the last psc position just below packet_size */
static uint8_t *get_psc(uint8_t *begin,uint8_t *end, int packet_size){
	int i;
	uint8_t *ret=NULL;
	uint8_t *p;
	if (begin==end) return NULL;
	for(i=1,p=begin+1;p<end && i<packet_size;++i,++p){
		if (p[-1]==0 && p[0]==0){
			ret=p-1;
		}
		p++;/* to skip possible 0 after the PSC that would make a double detection */
	}
	return ret;
}


struct jpeghdr {
	//unsigned int tspec:8;   /* type-specific field */
	unsigned int off:32;    /* fragment byte offset */
	uint8_t type;            /* id of jpeg decoder params */
	uint8_t q;               /* quantization factor (or table id) */
	uint8_t width;           /* frame width in 8 pixel blocks */
	uint8_t height;          /* frame height in 8 pixel blocks */
};

struct jpeghdr_rst {
	uint16_t dri;
	unsigned int f:1;
	unsigned int l:1;
	unsigned int count:14;
};

struct jpeghdr_qtable {
	uint8_t  mbz;
	uint8_t  precision;
	uint16_t length;
};

#define RTP_JPEG_RESTART           0x40

/* Procedure SendFrame:
 *
 *  Arguments:
 *    start_seq: The sequence number for the first packet of the current
 *               frame.
 *    ts: RTP timestamp for the current frame
 *    ssrc: RTP SSRC value
 *    jpeg_data: Huffman encoded JPEG scan data
 *    len: Length of the JPEG scan data
 *    type: The value the RTP/JPEG type field should be set to
 *    typespec: The value the RTP/JPEG type-specific field should be set
 *              to
 *    width: The width in pixels of the JPEG image
 *    height: The height in pixels of the JPEG image
 *    dri: The number of MCUs between restart markers (or 0 if there
 *         are no restart markers in the data
 *    q: The Q factor of the data, to be specified using the Independent
 *       JPEG group's algorithm if 1 <= q <= 99, specified explicitly
 *       with lqt and cqt if q >= 128, or undefined otherwise.
 *    lqt: The quantization table for the luminance channel if q >= 128
 *    cqt: The quantization table for the chrominance channels if
 *         q >= 128
 *
 *  Return value:
 *    the sequence number to be sent for the first packet of the next
 *    frame.
 *
 * The following are assumed to be defined:
 *
 * PACKET_SIZE                         - The size of the outgoing packet
 * send_packet(u_int8 *data, int len)  - Sends the packet to the network
 */

static void mjpeg_fragment_and_send(MSFilter *f,EncState *s,mblk_t *frame, uint32_t timestamp,
							 uint8_t type,	uint8_t typespec, int dri,
							 uint8_t q, mblk_t *lqt, mblk_t *cqt) {
	struct jpeghdr jpghdr;
	struct jpeghdr_rst rsthdr;
	struct jpeghdr_qtable qtblhdr;
	int bytes_left = (int)msgdsize(frame);
	int data_len;

	mblk_t *packet;

	/* Initialize JPEG header
	 */
	//jpghdr.tspec = typespec;
	jpghdr.off = 0;
	jpghdr.type = type | ((dri != 0) ? RTP_JPEG_RESTART : 0);
	jpghdr.q = q;
	jpghdr.width = s->vconf.vsize.width / 8;
	jpghdr.height = s->vconf.vsize.height / 8;

	/* Initialize DRI header
	 */
	if (dri != 0) {
		rsthdr.dri = htons(dri);
		rsthdr.f = 1;        /* This code does not align RIs */
		rsthdr.l = 1;
		rsthdr.count = 0x3fff;
	}

	/* Initialize quantization table header
	 */
	if (q >= 128) {
		qtblhdr.mbz = 0;
		qtblhdr.precision = 0; /* This code uses 8 bit tables only */
		qtblhdr.length = htons((uint16_t)(msgdsize(lqt)+msgdsize(cqt)));  /* 2 64-byte tables */
	}

	while (bytes_left > 0) {
		packet = allocb(s->mtu, 0);

		jpghdr.off = htonl(jpghdr.off);
		memcpy(packet->b_wptr, &jpghdr, sizeof(jpghdr));
		jpghdr.off = ntohl(jpghdr.off);
		packet->b_wptr += sizeof(jpghdr);

		if (dri != 0) {
			memcpy(packet->b_wptr, &rsthdr, sizeof(rsthdr));
			packet->b_wptr += sizeof(rsthdr);
		}

		if (q >= 128 && jpghdr.off == 0) {
			memcpy(packet->b_wptr, &qtblhdr, sizeof(qtblhdr));
			packet->b_wptr += sizeof(qtblhdr);
			if (msgdsize(lqt)){
				memcpy(packet->b_wptr, lqt->b_rptr, msgdsize(lqt));
				packet->b_wptr += msgdsize(lqt);
			}
			if (msgdsize(cqt)){
				memcpy(packet->b_wptr, cqt->b_rptr, msgdsize(cqt));
				packet->b_wptr += msgdsize(cqt);
			}
		}

		data_len = s->mtu - (packet->b_wptr - packet->b_rptr);
		if (data_len >= bytes_left) {
			data_len = bytes_left;
			mblk_set_marker_info(packet,TRUE);
		}

		memcpy(packet->b_wptr, frame->b_rptr + jpghdr.off, data_len);
		packet->b_wptr=packet->b_wptr + data_len;

		mblk_set_timestamp_info(packet,timestamp);
		ms_queue_put(f->outputs[0],packet);

		jpghdr.off += data_len;
		bytes_left -= data_len;
	}
}

static int find_marker(uint8_t **pbuf_ptr, uint8_t *buf_end){

	uint8_t *buf_ptr;
	unsigned int v, v2;
	int val;

	buf_ptr = *pbuf_ptr;
	while (buf_ptr < buf_end) {
		v = *buf_ptr++;
		v2 = *buf_ptr;
		if ((v == 0xff) && (v2 >= 0xc0) && (v2 <= 0xfe) && buf_ptr < buf_end) {
			val = *buf_ptr++;
			*pbuf_ptr = buf_ptr;
			return val;
		}
	}
	val = -1;
	return val;
}

static mblk_t *skip_jpeg_headers(mblk_t *full_frame, mblk_t **lqt, mblk_t **cqt){
	int err;
	uint8_t *pbuf_ptr=full_frame->b_rptr;
	uint8_t *buf_end=full_frame->b_wptr;

	ms_message("image size: %li)", (long)(buf_end-pbuf_ptr));

	*lqt=NULL;
	*cqt=NULL;

	err = find_marker(&pbuf_ptr, buf_end);
	while (err!=-1)
	{
		ms_message("marker found: %x (offset from beginning %li)", err, (long)(pbuf_ptr-full_frame->b_rptr));
		if (err==0xdb)
		{
			/* copy DQT table */
			int len = ntohs(*(uint16_t*)(pbuf_ptr));
			if (*lqt==NULL)
			{
				mblk_t *_lqt = allocb(len-3, 0);
				memcpy(_lqt->b_rptr, pbuf_ptr+3, len-3);
				_lqt->b_wptr += len-3;
				*lqt = _lqt;
				//*cqt = dupb(*lqt);
			}
			else
			{
				mblk_t *_cqt = allocb(len-3, 0);
				memcpy(_cqt->b_rptr, pbuf_ptr+3, len-3);
				_cqt->b_wptr += len-3;
				*cqt = _cqt;
			}
		}
		if (err==0xda)
		{
			uint16_t *bistream=(uint16_t *)pbuf_ptr;
			uint16_t len = ntohs(*bistream);
			full_frame->b_rptr = pbuf_ptr+len;
		}
		err = find_marker(&pbuf_ptr, buf_end);
	}
	return full_frame;
}

static void split_and_send(MSFilter *f, EncState *s, mblk_t *frame, bool_t is_iframe){
	uint8_t *lastpsc;
	uint8_t *psc;
	uint32_t timestamp=(uint32_t)(f->ticker->time*90LL);

	if (s->codec==CODEC_ID_MPEG4
#if HAVE_AVCODEC_SNOW
	|| s->codec==CODEC_ID_SNOW
#endif
	)
	{
		mpeg4_fragment_and_send(f,s,frame,timestamp);
		return;
	}
	else if (s->codec==CODEC_ID_MJPEG)
	{
		mblk_t *lqt=NULL;
		mblk_t *cqt=NULL;
		skip_jpeg_headers(frame, &lqt, &cqt);
		mjpeg_fragment_and_send(f,s,frame,timestamp,
								1, /* 420? */
								0,
								0, /* dri ?*/
								255, /* q */
								lqt,
								cqt);
		return;
	}

	ms_debug("processing frame of size %i",frame->b_wptr-frame->b_rptr);
	if (f->desc->id==MS_H263_ENC_ID){
		lastpsc=frame->b_rptr;
		while(1){
			psc=get_psc(lastpsc+2,frame->b_wptr,s->mtu);
			if (psc!=NULL){
				rfc4629_generate_follow_on_packets(f,s,frame,timestamp,lastpsc,psc,FALSE);
				lastpsc=psc;
			}else break;
		}
		/* send the end of frame */
		rfc4629_generate_follow_on_packets(f,s,frame, timestamp,lastpsc,frame->b_wptr,TRUE);
	}else if (f->desc->id==MS_H263_OLD_ENC_ID){
		rfc2190_generate_packets(f,s,frame,timestamp,is_iframe);
	}else{
		ms_fatal("Ca va tres mal.");
	}
}

static void process_frame(MSFilter *f, mblk_t *inm){
	EncState *s=(EncState*)f->data;

	AVCodecContext *c=&s->av_context;
	int error,got_packet;
	mblk_t *comp_buf=s->comp_buf;
	int comp_buf_sz=dblk_lim(comp_buf->b_datap)-dblk_base(comp_buf->b_datap);
	YuvBuf yuv;
	struct AVPacket packet;
	memset(&packet, 0, sizeof(packet));

	ms_yuv_buf_init_from_mblk(&yuv, inm);
	/* convert image if necessary */
	av_frame_unref(s->pict);
	avpicture_fill((AVPicture*)s->pict,yuv.planes[0],c->pix_fmt,c->width,c->height);

	/* timestamp used by ffmpeg, unset here */
	s->pict->pts=AV_NOPTS_VALUE;

	if (ms_video_starter_need_i_frame (&s->starter, f->ticker->time)){
		/*sends an I frame at 2 seconds and 4 seconds after the beginning of the call*/
		s->req_vfu=TRUE;
	}
	if (s->req_vfu){
		s->pict->pict_type=FF_I_TYPE;
		s->req_vfu=FALSE;
	}
	comp_buf->b_rptr=comp_buf->b_wptr=dblk_base(comp_buf->b_datap);
#if HAVE_AVCODEC_SNOW
	if (s->codec==CODEC_ID_SNOW){
		//prepend picture size
		uint32_t header=((s->vconf.vsize.width&0xffff)<<16) | (s->vconf.vsize.height&0xffff);
		*(uint32_t*)comp_buf->b_wptr=htonl(header);
		comp_buf->b_wptr+=4;
		comp_buf_sz-=4;
	}
#endif
	packet.data=comp_buf->b_wptr;
	packet.size=comp_buf_sz;
	error=avcodec_encode_video2(c, &packet, s->pict, &got_packet);

	if (error<0) ms_warning("ms_AVencoder_process: error %i.",error);
	else if (got_packet){
		bool_t is_iframe = FALSE;
		s->framenum++;
		if (s->framenum==1){
			ms_video_starter_first_frame(&s->starter, f->ticker->time);
		}
#ifdef AV_PKT_FLAG_KEY
		if (packet.flags & AV_PKT_FLAG_KEY) {
#else
		if (c->coded_frame->pict_type==FF_I_TYPE){
#endif
			ms_message("Emitting I-frame");
			is_iframe = TRUE;
		}
		comp_buf->b_wptr+=packet.size;
		split_and_send(f,s,comp_buf,is_iframe);
	}
	freemsg(inm);
}

static void enc_process(MSFilter *f){
	mblk_t *inm;
	EncState *s=(EncState*)f->data;
	if (s->av_context.codec==NULL) {
		ms_queue_flush(f->inputs[0]);
		return;
	}
	ms_filter_lock(f);
	while((inm=ms_queue_get(f->inputs[0]))!=0){
		process_frame(f,inm);
	}
	ms_filter_unlock(f);
}


static int enc_set_configuration(MSFilter *f, void *data) {
	EncState *s = (EncState *)f->data;
	const MSVideoConfiguration *vconf = (const MSVideoConfiguration *)data;
	if (vconf != &s->vconf) memcpy(&s->vconf, vconf, sizeof(MSVideoConfiguration));

	if (s->vconf.required_bitrate > s->vconf.bitrate_limit)
		s->vconf.required_bitrate = s->vconf.bitrate_limit;
	if (s->av_context.codec != NULL) {
		/* When we are processing, apply new settings immediately */
		ms_filter_lock(f);
		enc_postprocess(f);
		enc_preprocess(f);
		ms_filter_unlock(f);
		return 0;
	}

	if (vconf->extra != NULL) {
		s->qmin = *((int *)vconf->extra);
	}
	ms_message("Video configuration set: bitrate=%dbits/s, fps=%f, vsize=%dx%d for encoder [%p]", s->vconf.required_bitrate
																								, s->vconf.fps
																								, s->vconf.vsize.width
																								, s->vconf.vsize.height
																								, f);
	return 0;
}

static int enc_set_fps(MSFilter *f, void *arg){
	EncState *s=(EncState*)f->data;
	s->vconf.fps=*(float*)arg;
	enc_set_configuration(f, &s->vconf);
	return 0;
}

static int enc_get_fps(MSFilter *f, void *arg){
	EncState *s=(EncState*)f->data;
	*(float*)arg=s->vconf.fps;
	return 0;
}

static int enc_set_vsize(MSFilter *f, void *arg) {
	MSVideoConfiguration best_vconf;
	MSVideoSize *vs = (MSVideoSize *)arg;
	EncState *s=(EncState*)f->data;
	best_vconf = ms_video_find_best_configuration_for_size(s->vconf_list, *vs, ms_factory_get_cpu_count(f->factory));
	s->vconf.vsize = *vs;
	s->vconf.fps = best_vconf.fps;
	s->vconf.bitrate_limit = best_vconf.bitrate_limit;
	enc_set_configuration(f, &s->vconf);
	return 0;
}

static int enc_get_vsize(MSFilter *f,void *arg){
	EncState *s=(EncState*)f->data;
	*(MSVideoSize*)arg=s->vconf.vsize;
	return 0;
}

static int enc_set_mtu(MSFilter *f,void *arg){
	EncState *s=(EncState*)f->data;
	s->mtu=*(int*)arg;
	return 0;
}

static int enc_get_br(MSFilter *f, void *arg){
	EncState *s=(EncState*)f->data;
	*(int*)arg=s->vconf.required_bitrate;
	return 0;
}

static int enc_set_br(MSFilter *f, void *arg) {
	EncState *s = (EncState *)f->data;
	int br = *(int *)arg;
	if (s->av_context.codec != NULL) {
		/* Encoding is already ongoing, do not change video size, only bitrate. */
		s->vconf.required_bitrate = br;
		enc_set_configuration(f, &s->vconf);
	} else {
		MSVideoConfiguration best_vconf = ms_video_find_best_configuration_for_bitrate(s->vconf_list, br, ms_factory_get_cpu_count(f->factory));
		enc_set_configuration(f, &best_vconf);
	}
	return 0;
}

static int enc_get_configuration_list(MSFilter *f, void *data) {
	EncState *s = (EncState *)f->data;
	const MSVideoConfiguration **vconf_list = (const MSVideoConfiguration **)data;
	*vconf_list = s->vconf_list;
	return 0;
}


static MSFilterMethod methods[] = {
	{ MS_FILTER_SET_FPS,                       enc_set_fps                },
	{ MS_FILTER_GET_FPS,                       enc_get_fps                },
	{ MS_FILTER_SET_VIDEO_SIZE,                enc_set_vsize              },
	{ MS_FILTER_GET_VIDEO_SIZE,                enc_get_vsize              },
	{ MS_FILTER_ADD_FMTP,                      enc_add_fmtp               },
	{ MS_FILTER_SET_BITRATE,                   enc_set_br                 },
	{ MS_FILTER_GET_BITRATE,                   enc_get_br                 },
	{ MS_FILTER_SET_MTU,                       enc_set_mtu                },
	{ MS_FILTER_REQ_VFU,                       enc_req_vfu                },
	{ MS_VIDEO_ENCODER_REQ_VFU,                enc_req_vfu                },
	{ MS_VIDEO_ENCODER_GET_CONFIGURATION_LIST, enc_get_configuration_list },
	{ MS_VIDEO_ENCODER_SET_CONFIGURATION,      enc_set_configuration      },
	{ 0,                                       NULL                       }
};

#ifdef _MSC_VER

MSFilterDesc ms_h263_enc_desc={
	MS_H263_ENC_ID,
	"MSH263Enc",
	N_("A video H.263 encoder using ffmpeg library."),
	MS_FILTER_ENCODER,
	"H263-1998",
	1, /*MS_YUV420P is assumed on this input */
	1,
	enc_h263_init,
	enc_preprocess,
	enc_process,
	enc_postprocess,
	enc_uninit,
	methods
};

MSFilterDesc ms_h263_old_enc_desc={
	MS_H263_OLD_ENC_ID,
	"MSH263OldEnc",
	N_("A video H.263 encoder using ffmpeg library. It is compliant with old RFC2190 spec."),
	MS_FILTER_ENCODER,
	"H263",
	1, /*MS_YUV420P is assumed on this input */
	1,
	enc_h263_init,
	enc_preprocess,
	enc_process,
	enc_postprocess,
	enc_uninit,
	methods
};

MSFilterDesc ms_mpeg4_enc_desc={
	MS_MPEG4_ENC_ID,
	"MSMpeg4Enc",
	N_("A video MPEG4 encoder using ffmpeg library."),
	MS_FILTER_ENCODER,
	"MP4V-ES",
	1, /*MS_YUV420P is assumed on this input */
	1,
	enc_mpeg4_init,
	enc_preprocess,
	enc_process,
	enc_postprocess,
	enc_uninit,
	methods
};
#if HAVE_AVCODEC_SNOW
MSFilterDesc ms_snow_enc_desc={
	MS_SNOW_ENC_ID,
	"MSSnowEnc",
	N_("A video snow encoder using ffmpeg library."),
	MS_FILTER_ENCODER,
	"x-snow",
	1, /*MS_YUV420P is assumed on this input */
	1,
	enc_snow_init,
	enc_preprocess,
	enc_process,
	enc_postprocess,
	enc_uninit,
	methods
};
#endif
MSFilterDesc ms_mjpeg_enc_desc={
	MS_JPEG_ENC_ID,
	"MSJpegEnc",
	N_("A RTP/MJPEG encoder using ffmpeg library."),
	MS_FILTER_ENCODER,
	"JPEG",
	1, /*MS_YUV420P is assumed on this input */
	1,
	enc_mjpeg_init,
	enc_preprocess,
	enc_process,
	enc_postprocess,
	enc_uninit,
	methods
};

#else

MSFilterDesc ms_h263_enc_desc={
	.id=MS_H263_ENC_ID,
	.name="MSH263Enc",
	.text=N_("A video H.263 encoder using ffmpeg library."),
	.category=MS_FILTER_ENCODER,
	.enc_fmt="H263-1998",
	.ninputs=1, /*MS_YUV420P is assumed on this input */
	.noutputs=1,
	.init=enc_h263_init,
	.preprocess=enc_preprocess,
	.process=enc_process,
	.postprocess=enc_postprocess,
	.uninit=enc_uninit,
	.methods=methods
};

MSFilterDesc ms_h263_old_enc_desc={
	.id=MS_H263_OLD_ENC_ID,
	.name="MSH263Enc",
	.text=N_("A video H.263 encoder using ffmpeg library, compliant with old RFC2190 spec."),
	.category=MS_FILTER_ENCODER,
	.enc_fmt="H263",
	.ninputs=1, /*MS_YUV420P is assumed on this input */
	.noutputs=1,
	.init=enc_h263_init,
	.preprocess=enc_preprocess,
	.process=enc_process,
	.postprocess=enc_postprocess,
	.uninit=enc_uninit,
	.methods=methods
};

MSFilterDesc ms_mpeg4_enc_desc={
	.id=MS_MPEG4_ENC_ID,
	.name="MSMpeg4Enc",
	.text=N_("A video MPEG4 encoder using ffmpeg library."),
	.category=MS_FILTER_ENCODER,
	.enc_fmt="MP4V-ES",
	.ninputs=1, /*MS_YUV420P is assumed on this input */
	.noutputs=1,
	.init=enc_mpeg4_init,
	.preprocess=enc_preprocess,
	.process=enc_process,
	.postprocess=enc_postprocess,
	.uninit=enc_uninit,
	.methods=methods
};
#if HAVE_AVCODEC_SNOW
MSFilterDesc ms_snow_enc_desc={
	.id=MS_SNOW_ENC_ID,
	.name="MSSnowEnc",
	.text=N_("The snow codec is royalty-free and is open-source. \n"
		"It uses innovative techniques that makes it one of most promising video "
		"codec. It is implemented within the ffmpeg project.\n"
		"However it is under development, quite unstable and compatibility with other versions "
		"cannot be guaranteed."),
	.category=MS_FILTER_ENCODER,
	.enc_fmt="x-snow",
	.ninputs=1, /*MS_YUV420P is assumed on this input */
	.noutputs=1,
	.init=enc_snow_init,
	.preprocess=enc_preprocess,
	.process=enc_process,
	.postprocess=enc_postprocess,
	.uninit=enc_uninit,
	.methods=methods
};
#endif
MSFilterDesc ms_mjpeg_enc_desc={
	.id=MS_JPEG_ENC_ID,
	.name="MSMJpegEnc",
	.text=N_("A MJPEG encoder using ffmpeg library."),
	.category=MS_FILTER_ENCODER,
	.enc_fmt="JPEG",
	.ninputs=1, /*MS_YUV420P is assumed on this input */
	.noutputs=1,
	.init=enc_mjpeg_init,
	.preprocess=enc_preprocess,
	.process=enc_process,
	.postprocess=enc_postprocess,
	.uninit=enc_uninit,
	.methods=methods
};

#endif

void __register_ffmpeg_encoders_if_possible(MSFactory *obj){
	ms_ffmpeg_check_init();
	if (avcodec_find_encoder(CODEC_ID_MPEG4) && HAVE_NON_FREE_CODECS)
		ms_factory_register_filter(obj,&ms_mpeg4_enc_desc);
	if (avcodec_find_encoder(CODEC_ID_H263) && HAVE_NON_FREE_CODECS){
		ms_factory_register_filter(obj,&ms_h263_enc_desc);
		ms_factory_register_filter(obj,&ms_h263_old_enc_desc);
	}
#if HAVE_AVCODEC_SNOW
	if (avcodec_find_encoder(CODEC_ID_SNOW))
		ms_factory_register_filter(obj,&ms_snow_enc_desc);
#endif
	if (avcodec_find_encoder(CODEC_ID_MJPEG))
	{
		ms_factory_register_filter(obj,&ms_mjpeg_enc_desc);
	}
}


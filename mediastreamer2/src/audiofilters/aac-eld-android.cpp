/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2013  Belledonne Communications SARL

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

#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/msjava.h>
#include <mediastreamer2/msticker.h>
#include <mediastreamer2/mscodecutils.h>

/* frame are encoded/decoded for a constant sample number of 512(but an RTP packet may contain several frames */
/* Although the AAC-ELD encoder that is included in the Android platform supports framesizes of 480 and
   512 for encoding, with the current Java API only a fixed framesize of 512 can be used. */
#define SIGNAL_FRAME_SIZE (512*sizeof(short))
/* RFC3640 3.3.6: aac-hbr mode have a 4 bytes header(we use only one frame per packet) */
#define AU_HDR_SZ 4

/*  defines needed to compute network bit rate, copied from misc.c */
#define UDP_HDR_SZ 8
#define RTP_HDR_SZ 12
#define IP4_HDR_SZ 20

/* ptime can be increased to mitigate network overhead */
const int sample_rate_min_ptime[] = {
	10, /* 16 kHz */
	10, /* 22 kHz */
	10, /* 32 kHz */
	10, /* 44 kHz */
	10, /* 48 kHz */
};

static int sample_rate_to_index(int sampleRate) {
	switch (sampleRate) {
		case 16000: return 0;
		case 22050: return 1;
		case 32000: return 2;
		case 44100: return 3;
		case 48000: return 4;
		default: return 4;
	}
}

static int ip_bitrate_to_codec_bitrate(int sampleRate, int ip_br) {
	/* 1 frame per packet */
	int pkt_per_sec = sampleRate / 512;
	int overhead = AU_HDR_SZ + RTP_HDR_SZ + UDP_HDR_SZ + IP4_HDR_SZ;
	int codec_br = (ip_br / 8 - pkt_per_sec * overhead) * 8;

	/* SoftAACEncoder2  W  Requested bitrate XXXX unsupported, using 16008 */
	codec_br = MAX(codec_br, 16000);

	ms_message("ip_bitrate_to_codec_bitrate(sample_rate=%d, ip_br=%d) -> %d", sampleRate, ip_br, codec_br);
	return codec_br;
}

static int codec_bitrate_to_ip_bitrate(int sampleRate, int codec_br) {
	/* 1 frame per packet */
	int pkt_per_sec = sampleRate / 512;
	int overhead = AU_HDR_SZ + RTP_HDR_SZ + UDP_HDR_SZ + IP4_HDR_SZ;
	int ip_br = (codec_br / 8 + pkt_per_sec * overhead) * 8;
	ms_message("codec_bitrate_to_ip_bitrate(sample_rate=%d, codec_br=%d) -> %d", sampleRate, codec_br, ip_br);
	return ip_br;
}

static int get_sdk_version(){
	static int sdk_version = 0;
	if (sdk_version==0){
		/* Get Android SDK version. */
		JNIEnv *jni_env = ms_get_jni_env();
		jclass version_class = jni_env->FindClass("android/os/Build$VERSION");
		jfieldID fid = jni_env->GetStaticFieldID(version_class, "SDK_INT", "I");
		sdk_version = jni_env->GetStaticIntField(version_class, fid);
		ms_message("SDK version [%i] detected", sdk_version);
		jni_env->DeleteLocalRef(version_class);
	}
	return sdk_version;
}


/* Helper class to manage jni interface */
class AACFilterJniWrapper {
	enum Enum {
		Push = 0,
		Pull,
	};

	jclass jniClass;
	jobject AACFilterInstance;

	jmethodID preProcessMethod;
	jmethodID encoder[2];
	jmethodID decoder[2];
	jmethodID postProcessMethod;

	jbyteArray array;


	jmethodID lookupMethod(JNIEnv* jni_env, const char* name, const char* signature, bool isStatic);

public:
	void init(JNIEnv* jni_env);
	int preprocess(JNIEnv* jni_env, int sampleRate, int channelCount, int bitrate, bool_t sbr_enabled);
	bool postprocess(JNIEnv* jni_env);
	mblk_t* pullFromEncoder(JNIEnv* jni_env);
	void pushToEncoder(JNIEnv* jni_env, uint8_t* data, int size);
	mblk_t* pullFromDecoder(JNIEnv* jni_env);
	void pushToDecoder(JNIEnv* jni_env, uint8_t* data, int size);
	void uninit(JNIEnv* jni_env);
};

struct EncState {
	AACFilterJniWrapper jni_wrapper;

	uint32_t timeStamp; /* timeStamp is actually expressed in number of sample processed, needed for encoder only, inserted in the encoded message block */
	int ptime; /* wait until we have at least this amount of data in input before processing it */
	int maxptime; /* at first call to set ptime, set this value to max of 50, given parameter in order to avoid automatic ptime changing reaching high values unless we enforce it at init */
	int samplingRate; /* sampling rate of signal to be encoded */
	int bitRate; /* bit rate of encode signal */
	int nbytes; /* amount of data in a processedTime window usually sampling rate*time*number of byte per sample*number of channels, this one is set to a integer number of 512 samples frame */
	int nchannels; /* number of channels, default 1(mono) */
	MSBufferizer *bufferizer; /* buffer to store data from input queue before processing them */
	uint8_t *inputBuffer; /* buffer to store nbytes of input data and send them to the encoder */
	bool_t sbr_enabled;
};

/* Encoder */
/* called at init and any modification of ptime: compute the input data length */
static void enc_update ( struct EncState *s ) {
	s->nbytes= ( sizeof ( short ) *s->nchannels*s->samplingRate*s->ptime ) /1000; /* input is 16 bits LPCM: 2 bytes per sample, ptime is in ms so /1000 */

	/* the nbytes must be a multiple of SIGNAL_FRAME_SIZE(512 sample frame) */
	/* nbytes % SIGNAL_FRAME_SIZE must be 0, min SIGNAL_FRAME_SIZE */
	/* this gives for sampling rate at 22050Hz available values of ptime: 23.2ms and multiples up to 92.8ms */
	/* at 44100Hz: 11.6ms and multiples up to 92.8ms */
	/* given an input ptime, select the one immediatly inferior in the list of available possibilities */
	if ( s->nbytes > (int)SIGNAL_FRAME_SIZE ) {
		s->nbytes -= ( s->nbytes % SIGNAL_FRAME_SIZE );
	} else {
		s->nbytes = SIGNAL_FRAME_SIZE;
	}
}

/* init the encoder: create an encoder State structure, initialise it and attach it to the MSFilter structure data property */
static void enc_init ( MSFilter *f ) {
	if (get_sdk_version() < 16) {
		f->data = NULL;
		return;
	}
	struct EncState *s= ( struct EncState* ) ms_new ( struct EncState,1 );

	s->jni_wrapper.init(ms_get_jni_env());

	s->timeStamp=0;
	s->sbr_enabled = 0;
	s->bufferizer=ms_bufferizer_new();
	s->ptime=10;
	s->maxptime = -1;
	s->samplingRate=22050; /* default is narrow band : 22050Hz and 32000b/s */
	s->bitRate=32000;
	s->nchannels=1;
	s->inputBuffer = ( uint8_t * ) malloc ( SIGNAL_FRAME_SIZE ); /* allocate input buffer */

	/* attach it to the MSFilter structure */
	f->data=s;
}

/* pre process: at this point the sampling rate, ptime, number of channels have been correctly setted, so we can initialize the encoder */
static void enc_preprocess ( MSFilter *f ) {
	struct EncState *s= ( struct EncState* ) f->data;
	if (!s) return;

	s->ptime = MAX(s->ptime, sample_rate_min_ptime[sample_rate_to_index(s->samplingRate)]);

	s->bitRate = s->jni_wrapper.preprocess(ms_get_jni_env(), s->samplingRate, s->nchannels, s->bitRate, s->sbr_enabled);

	if (s->bitRate == -1) {
		ms_error("AAC encoder pre-process went wrong");
	}
	ms_message("AAC encoder bitrate: %d, ptime: %d", s->bitRate, s->ptime);

	/* update the nbytes value */
	enc_update ( s );
}

/* process: get data from MSFilter queue and put it into the EncState buffer, then process it until there is less than nbytes in the input buffer */
static void enc_process ( MSFilter *f ) {
	struct EncState *s= ( struct EncState* ) f->data;
	if (!s) return;

	JNIEnv* jni_env = ms_get_jni_env();

	/* lock the MSFilter properties in order to delay any change of properties(mostly ptimes which impact nbytes) */
	ms_filter_lock ( f );

	/* Encoder works asynchronously */
	/* Read coded audio from encoder */
	{
		int frameCount = 0;
		mblk_t* frame = NULL, *au_headers = NULL, *frames = NULL;

		/* Read available frame if any */
		while ( (frame = s->jni_wrapper.pullFromEncoder(jni_env) )) {
			/* Build frame AU-header according to RFC3640 3.3.6 */
			int frame_length = msgdsize(frame);
			/* ms_message("Pull from encoder %d bytes", frame_length); */

			mblk_t* header = allocb(2, 0);
			header->b_wptr[0] = (uint8_t) (((frame_length << 3) & 0xFF00) >> 8);
			header->b_wptr[1] = (uint8_t) ((frame_length << 3) & 0xFF);
			header->b_wptr += 2;

			/* Add to message */
			if (au_headers)
				concatb(au_headers, header);
			else
				au_headers = header;

			if (frames) {
				concatb(frames, frame);
			} else {
				frames = frame;
			}
			frameCount++;
		}

		if (au_headers) {
			/* Prepend AU Header Section */
			/* 2 bytes, specifiying the length in bit of AU-headers */
			mblk_t* global_header = allocb(2, 0);
			global_header->b_wptr[0] = (uint8_t)(frameCount>>4);
			global_header->b_wptr[1] = (uint8_t)(frameCount<<4);
			global_header->b_wptr += 2;

			concatb(au_headers, frames);
			concatb(global_header, au_headers);

			/* set timeStamp in the output Message */
			mblk_set_timestamp_info ( global_header, s->timeStamp );
			/* increment timeStamp by the number of sample processed (divided by the number of channels) */
			int sample_count = (frameCount * 512);
			s->timeStamp += sample_count;

			msgpullup(global_header, -1);

			/* insert the output message into the output queue of MSFilter */
			ms_queue_put ( f->outputs[0], global_header );
		}
	}

	/* Feed encoder with pcm data */
	{
		ms_bufferizer_put_from_queue ( s->bufferizer,f->inputs[0] );
		/* Wait until s->nbytes bytes are available... */
		while ( (int)ms_bufferizer_get_avail ( s->bufferizer ) >=s->nbytes ) {
			/* ... then feed the encoder with chunks of SIGNAL_FRAME_SIZE bytes */
			ms_bufferizer_read ( s->bufferizer, s->inputBuffer, SIGNAL_FRAME_SIZE );
			/* ms_message("Push to encoder %d bytes", SIGNAL_FRAME_SIZE); */
			s->jni_wrapper.pushToEncoder(jni_env, s->inputBuffer, SIGNAL_FRAME_SIZE);
		}
	}

	/* release the lock */
	ms_filter_unlock ( f );
}

static void enc_postprocess ( MSFilter *f ) {
	struct EncState *s= ( struct EncState* ) f->data;
	if (!s) return;
	s->jni_wrapper.postprocess(ms_get_jni_env());
}

static void enc_uninit ( MSFilter *f ) {
	struct EncState *s= ( struct EncState* ) f->data;
	if (!s) return;
	ms_bufferizer_destroy ( s->bufferizer );
	free ( s->inputBuffer );
	s->jni_wrapper.uninit(ms_get_jni_env());
	ms_free ( s );
	f->data = 0;
}


static int set_ptime ( struct EncState *s, int value ) {
	/* at first call to this function, set the maxptime to MAX (50, value) but not higher than 100 */
	if (s->maxptime<0) {
		s->maxptime = MIN(100,MAX(value, 50));
	}

	if ( value>0 && value<=s->maxptime ) {
		s->ptime = MAX(value, sample_rate_min_ptime[sample_rate_to_index(s->samplingRate)]);
		ms_message ( "AAC-ELD encoder using ptime=%i",value );
		enc_update ( s );
		return 0;
	} else {
		return -1; /* to tell the automatic ptime adjustment to stop trying changing ptime */
	}
}

static int enc_add_attr ( MSFilter *f, void *arg ) {
	const char *fmtp= ( const char* ) arg;
	struct EncState *s= ( struct EncState* ) f->data;
	if (!s) return -1;
	int retval=0;
	if ( strstr ( fmtp,"ptime:" ) ) {
		ms_filter_lock ( f );
		retval = set_ptime ( s,atoi ( fmtp+6 ) );
		ms_filter_unlock ( f );
	}
	return retval;
};


static int enc_add_fmtp ( MSFilter *f, void *arg ) {
	const char *fmtp= ( const char* ) arg;
	struct EncState *s= ( struct EncState* ) f->data;
	if (!s) return -1;
	char tmp[16]= {0};
	int retval=0;

	if ( fmtp_get_value ( fmtp,"ptime",tmp,sizeof ( tmp ) ) ) {
		ms_filter_lock ( f );
		retval = set_ptime ( s,atoi ( tmp ) );
		ms_filter_unlock ( f );
	}
	if( fmtp_get_value( fmtp, "SBR-enabled", tmp, sizeof(tmp) )){
		int enabled = atoi(tmp);
		s->sbr_enabled = enabled;
		ms_message("AAC Encoder SBR enabled: %d", enabled);
	}
	return retval;
}

static int enc_set_sr ( MSFilter *f, void *arg ) {
	struct EncState *s= ( struct EncState* ) f->data;
	if (!s) return -1;

	int ip_br = codec_bitrate_to_ip_bitrate(s->samplingRate, s->bitRate);
	s->samplingRate = ( ( int* ) arg ) [0];
	s->bitRate = ip_bitrate_to_codec_bitrate(s->samplingRate, ip_br);

	return 0;
}

static int enc_get_sr ( MSFilter *f, void *arg ) {
	struct EncState *s= ( struct EncState* ) f->data;
	if (!s) return -1;
	( ( int* ) arg ) [0]=s->samplingRate;
	return 0;
}

static int enc_set_br ( MSFilter *f, void *arg ) {
	struct EncState *s= ( struct EncState* ) f->data;
	s->bitRate = ip_bitrate_to_codec_bitrate(s->samplingRate, *((int*) arg));
	return 0;
}

static int enc_get_br ( MSFilter *f, void *arg ) {
	struct EncState *s= ( struct EncState* ) f->data;
	if (!s) return -1;
	/* in s->bitRate, we have the codec bitrate but this function must return the network bitrate */
	/* network_bitrate = ((codec_bitrate*ptime/8) + AU Header + RTP Header + UDP Header + IP Header)*8/ptime */
	( ( int* ) arg ) [0]= codec_bitrate_to_ip_bitrate(s->samplingRate, s->bitRate);
	ms_message("READ back sample rate from jni");
	return 0;
}

static int enc_set_nchannels(MSFilter *f, void *arg) {
	struct EncState *s = (struct EncState *)f->data;
	if (!s) return -1;
	s->nchannels = *(int *)arg;
	return 0;
}

static int enc_set_ptime(MSFilter *f, void *arg) {
	struct EncState *s = (struct EncState *)f->data;
	if (!s) return -1;
	int retval=0;

	ms_filter_lock ( f );
	retval = set_ptime ( s, *(int *)arg );
	ms_filter_unlock ( f );


	return retval;
}

/* attach encoder methods to MSFilter IDs */
static MSFilterMethod enc_methods[]= {
	{	MS_FILTER_ADD_ATTR		,	enc_add_attr},
	{	MS_FILTER_ADD_FMTP		,	enc_add_fmtp},
	{	MS_FILTER_SET_SAMPLE_RATE	,	enc_set_sr	},
	{	MS_FILTER_GET_SAMPLE_RATE	,	enc_get_sr	},
	{	MS_FILTER_SET_BITRATE		,	enc_set_br	},
	{	MS_FILTER_GET_BITRATE		,	enc_get_br	},
	{	MS_FILTER_SET_NCHANNELS		,	enc_set_nchannels},
	{	MS_AUDIO_ENCODER_SET_PTIME	,	enc_set_ptime	},
	{0, NULL}
};


MSFilterDesc ms_aac_eld_enc_desc= {
	MS_AAC_ELD_ENC_ID, /**< this ID must be declared in allfilter.h */
	"MSAACELDEnc", /**< filter name */
	"AAC-ELD encoder", /**< filter Description */
	MS_FILTER_ENCODER, /**< filter category */
	"mpeg4-generic", /**< MIME type for AAC-ELD */
	1, /**< number of inputs */
	1, /**< number of outputs */
	enc_init, /**< Filter's init function*/
	enc_preprocess, /**< Filter's preprocess function, called one time before starting to process*/
	enc_process, /**< Filter's process function, called every tick by the MSTicker to do the filter's job*/
	enc_postprocess, /**< Filter's postprocess function, called once after processing (the filter is no longer called in process() after)*/
	enc_uninit, /**< Filter's uninit function, used to deallocate internal structures*/
	enc_methods /**<Filter's method table*/
};

/* Decoder */

struct DecState {
	AACFilterJniWrapper jni_wrapper;

	int samplingRate; /* sampling rate of decoded signal */
	uint8_t audioConverterProperty[512]; /* config string retrieved from SDP message  useless?*/
	int audioConverterProperty_size; /* previous property size */
	int nchannels; /* number of channels, default 1(mono) */
	int nbytes; /* size of ouput buffer for the decoder */
	MSConcealerTsContext *concealer; /* concealment management */
	bool_t sbr_enabled;
};

static void dec_init ( MSFilter *f ) {
	if (get_sdk_version() < 16) {
		f->data = NULL;
		return;
	}

	/* instanciate the decoder State structure */
	struct DecState *s= ( struct DecState * ) ms_new ( struct DecState,1 );

	s->jni_wrapper.init(ms_get_jni_env());

	s->samplingRate=44100;
	s->nchannels=1; /* default mono */

	/* attach it to the MSFilter */
	f->data=s;

}

static void dec_preprocess ( MSFilter *f ) {
	struct DecState *s= ( struct DecState* ) f->data;
	if (!s) return;
	s->concealer = ms_concealer_ts_context_new(0xffffffff);
}

static void dec_process ( MSFilter *f ) {
	struct DecState *s= ( struct DecState* ) f->data;
	if (!s) return;

	JNIEnv* jni_env = ms_get_jni_env();

	/* Decoder works asynchronously */
	/* Read decoded audio */
	{
		int frameCount = 0;
		mblk_t* m = NULL;
		/* Read available frames from decoder if any */
		while ( ( m = s->jni_wrapper.pullFromDecoder(jni_env) )) {
			ms_queue_put ( f->outputs[0], m );
			frameCount++;
		}
	}

	/* Feed the decoder with coded audio */
	{

		/* get the input message from queue */
		mblk_t *inputMessage;
		while ( ( inputMessage=ms_queue_get ( f->inputs[0] ) ) ) {
			/* process the input message */

			/* parse the header to get the number of frames in the message au-headers-length is length of headers in bits, for each frame, 16 bits header */
			uint16_t frameCount = ((((uint16_t)(inputMessage->b_rptr[0]))<<8) + ((uint16_t)(inputMessage->b_rptr[1])))/16;
			int headerOffset = 2 * frameCount + 2;

			int frameIndex;
			/* actual frame start at this offset in input message */
			uint8_t* decodeBuffer = inputMessage->b_rptr + headerOffset; /* initialise the read pointer to the beginning of the first frame */
			for (frameIndex=0; frameIndex<frameCount; frameIndex++) {

				/* get the frame length from the header */
				uint16_t frame_length = ((((uint16_t)(inputMessage->b_rptr[2+2*frameIndex]))<<8) + ((uint16_t)(inputMessage->b_rptr[2+2*frameIndex+1])))>>3;

				s->jni_wrapper.pushToDecoder(jni_env, decodeBuffer, frame_length);

				decodeBuffer += frame_length; /* increase the read pointer to the begining of next frame (if any, otherwise, it won't be used) */
			}

			/* signal to concealment context the reception of data */
			ms_concealer_ts_context_inc_sample_ts(s->concealer, f->ticker->time * s->samplingRate / 1000, frameCount * SIGNAL_FRAME_SIZE / sizeof(uint16_t), 1);

			freemsg ( inputMessage );
		}
	}

	/* is concealment needed? */
	if(ms_concealer_ts_context_is_concealement_required(s->concealer, f->ticker->time*s->samplingRate/1000)) {
		/* send a zero frame to the decoder (2 bytes at 0) */
		uint16_t zerobytes = 0;

		s->jni_wrapper.pushToDecoder(jni_env, (uint8_t*)&zerobytes, 2);

		ms_concealer_ts_context_inc_sample_ts(s->concealer, f->ticker->time*s->samplingRate/1000, SIGNAL_FRAME_SIZE/sizeof(uint16_t), 0);
	}
}

static void dec_postprocess ( MSFilter *f ) {
	struct DecState *s= ( struct DecState* ) f->data;
	if (!s) return;

	s->jni_wrapper.postprocess(ms_get_jni_env());
	ms_concealer_ts_context_destroy(s->concealer);
}

static void dec_uninit ( MSFilter *f ) {
	struct DecState *s= ( struct DecState* ) f->data;
	if (!s) return;

	ms_free ( s );
	f->data = 0;
}

static int dec_add_fmtp ( MSFilter *f, void *data ) {
	const char *fmtp= ( const char* ) data;
	struct DecState *s= ( struct DecState* ) f->data;
	if (!s) return -1;
	char config[512];
	if ( fmtp_get_value ( fmtp,"config",config,sizeof ( config ) ) ) {
		//convert hexa decimal config string into a bitstream
		int i,j,max=strlen ( config );
		char octet[3];
		octet[2]=0;
		for ( i=0,j=0; i<max; i+=2,++j ) {
			octet[0]=config[i];
			octet[1]=config[i+1];
			s->audioConverterProperty[0]=0;
			s->audioConverterProperty[j]= ( uint8_t ) strtol ( octet,NULL,16 );
		}
		s->audioConverterProperty_size=j;
		// ms_message ( "Got mpeg4 config string: %s",config );
	}
	if( fmtp_get_value( fmtp, "SBR-enabled", config, sizeof(config) )){
		int enabled = atoi(config);
		s->sbr_enabled = enabled;
		ms_message("AAC Decoder SBR enabled: %d", enabled);
	}
	return 0;
}

/* set the sampling rate (22050 or 44100 Hz) */
static int dec_set_sr ( MSFilter *f, void *arg ) {
	struct DecState *s= ( struct DecState* ) f->data;
	if (!s) return -1;
	s->samplingRate = ( ( int* ) arg ) [0];
	return 0;
}

static int dec_get_sr ( MSFilter *f, void *arg ) {
	struct DecState *s= ( struct DecState* ) f->data;
	if (!s) return -1;
	( ( int* ) arg ) [0]=s->samplingRate;
	return 0;
}

/* decoder support PLC, allow Mediastreamer2 to know about it */
static int dec_have_plc ( MSFilter *f, void *arg ) {
	* ( ( int * ) arg ) = 1;
	return 0;
}

/* attach decoder methods to MSFilter IDs */
static MSFilterMethod dec_methods[]= {
	{	MS_FILTER_SET_SAMPLE_RATE	,	dec_set_sr	},
	{	MS_FILTER_GET_SAMPLE_RATE	,	dec_get_sr	},
	{	MS_FILTER_ADD_FMTP		,	dec_add_fmtp	},
	{ 	MS_DECODER_HAVE_PLC		, 	dec_have_plc	},
	{	0				,	NULL		}
};

MSFilterDesc ms_aac_eld_dec_desc= {
	MS_AAC_ELD_DEC_ID, /**< this ID must be declared in allfilter.h */
	"MSAACELDDec", /**< filter name */
	"AAC-ELD decoder", /**< filter Description */
	MS_FILTER_DECODER, /**< filter category */
	"mpeg4-generic", /**< MIME type for AAC-ELD */
	1, /**< number of inputs */
	1, /**< number of outputs */
	dec_init, /**< Filter's init function*/
	dec_preprocess, /**< Filter's preprocess function, called one time before starting to process*/
	dec_process, /**< Filter's process function, called every tick by the MSTicker to do the filter's job*/
	dec_postprocess, /**< Filter's postprocess function, called once after processing (the filter is no longer called in process() after)*/
	dec_uninit, /**< Filter's uninit function, used to deallocate internal structures*/
	dec_methods /**<Filter's method table*/
};


MS_FILTER_DESC_EXPORT ( ms_aac_eld_dec_desc )
MS_FILTER_DESC_EXPORT ( ms_aac_eld_enc_desc )



jmethodID AACFilterJniWrapper::lookupMethod(JNIEnv* jni_env, const char* name, const char* signature, bool isStatic) {
	jmethodID mID;

	if (isStatic) {
		mID = jni_env->GetStaticMethodID(jniClass, name, signature);
	} else {
		mID = jni_env->GetMethodID(jniClass, name, signature);
	}

	if (!mID) {
		ms_error("aac-eld: couldn't find method '%s' signature '%s'", name, signature);
	}

	return mID;
}

void AACFilterJniWrapper::init(JNIEnv* jni_env) {
	/* Java Class */
	jclass localClass = jni_env->FindClass("org/linphone/mediastream/AACFilter");;
	jniClass = reinterpret_cast<jclass>(jni_env->NewGlobalRef(localClass));

	/* Methods */
	jmethodID instanceMethod = lookupMethod(jni_env, "instance", "()Lorg/linphone/mediastream/AACFilter;", true);
	preProcessMethod = lookupMethod(jni_env, "preprocess", "(IIIZ)Z", false);
	encoder[AACFilterJniWrapper::Push] = lookupMethod(jni_env, "pushToEncoder", "([BI)Z", false);
	encoder[AACFilterJniWrapper::Pull] = lookupMethod(jni_env, "pullFromEncoder", "([B)I", false);
	decoder[AACFilterJniWrapper::Push] = lookupMethod(jni_env, "pushToDecoder", "([BI)Z", false);
	decoder[AACFilterJniWrapper::Pull] = lookupMethod(jni_env, "pullFromDecoder", "([B)I", false);
	postProcessMethod = lookupMethod(jni_env, "postprocess", "()Z", false);

	/* Instance */
	AACFilterInstance = reinterpret_cast<jobject> (jni_env->NewGlobalRef(
		(jni_env->CallStaticObjectMethod(localClass, instanceMethod))));
	if (AACFilterInstance == 0) {
		ms_error("Failed to instanciate AACFilter JNI");
	}
	array = (jbyteArray)jni_env->NewGlobalRef(jni_env->NewByteArray(8192));
}

int AACFilterJniWrapper::preprocess(JNIEnv* jni_env, int sampleRate, int channelCount, int bitrate, bool_t sbr_enabled) {

	bool p = jni_env->CallBooleanMethod(AACFilterInstance, preProcessMethod, sampleRate, channelCount, bitrate, sbr_enabled);
	if (!p)
		return -1;
	return bitrate;
}
bool AACFilterJniWrapper::postprocess(JNIEnv* jni_env) {
	return jni_env->CallBooleanMethod(AACFilterInstance, postProcessMethod);
}

void AACFilterJniWrapper::pushToDecoder(JNIEnv* jni_env, uint8_t* data, int size) {
	if (data && size > 0) {
		/* create byte[] */
		jni_env->SetByteArrayRegion(array, 0, size, (const jbyte*) data);

		bool success = jni_env->CallBooleanMethod(AACFilterInstance, decoder[AACFilterJniWrapper::Push], array, size);
		if (!success) {
			ms_error("Failed to push %d bytes to decoder", size);
		}
	}
}

mblk_t* AACFilterJniWrapper::pullFromDecoder(JNIEnv* jni_env) {
	int length = jni_env->CallIntMethod(AACFilterInstance, decoder[AACFilterJniWrapper::Pull], array);

	if (length) {
		mblk_t* out = allocb ( length, 0 );
		jni_env->GetByteArrayRegion(array, 0, length, (jbyte*)out->b_wptr);
		out->b_wptr += length;
		return out;
	}

	return NULL;
}

void AACFilterJniWrapper::pushToEncoder(JNIEnv* jni_env, uint8_t* data, int size) {
	if (data && size > 0) {
		jni_env->SetByteArrayRegion(array, 0, size, (const jbyte*) data);

		bool success = jni_env->CallBooleanMethod(AACFilterInstance, encoder[AACFilterJniWrapper::Push], array, size);
		if (!success) {
			ms_error("Failed to push %d bytes to encoder", size);
		}
	}
}

mblk_t* AACFilterJniWrapper::pullFromEncoder(JNIEnv* jni_env) {
	int length = jni_env->CallIntMethod(AACFilterInstance, encoder[AACFilterJniWrapper::Pull], array);

	if (length) {
		mblk_t* out = allocb ( length, 0 );
		jni_env->GetByteArrayRegion(array, 0, length, (jbyte*)out->b_wptr);
		out->b_wptr += length;
		return out;
	}
	return NULL;
}

void AACFilterJniWrapper::uninit(JNIEnv* jni_env) {
	jni_env->DeleteGlobalRef(AACFilterInstance);
	jni_env->DeleteGlobalRef(jniClass);
}

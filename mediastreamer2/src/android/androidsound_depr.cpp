/*
 * msandroid.cpp -Android Media plugin for Linphone-
 *
 *
 * Copyright (C) 2009  Belledonne Communications, Grenoble, France
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "mediastreamer2/mssndcard.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msjava.h"
#include "mediastreamer2/devices.h"
#include <jni.h>

#include <sys/time.h>
#include <sys/resource.h>

#include "mediastreamer2/zrtp.h"
#include <cpu-features.h>

#include "hardware_echo_canceller.h"

#define USE_HARDWARE_RATE 1

static const float sndwrite_flush_threshold=0.020;	//ms
static const float sndread_flush_threshold=0.020; //ms
static void sound_read_setup(MSFilter *f);

#ifdef __cplusplus
extern "C" {
#endif
JNIEXPORT jboolean JNICALL Java_org_linphone_mediastream_Version_nativeHasNeon(JNIEnv *env, jclass c) {
	if (android_getCpuFamily() == ANDROID_CPU_FAMILY_ARM && (android_getCpuFeatures() & ANDROID_CPU_ARM_FEATURE_NEON) != 0)
	{
		return 1;
	}
	return 0;
}

JNIEXPORT jboolean JNICALL Java_org_linphone_mediastream_Version_nativeHasZrtp(JNIEnv *env, jclass c) {
	return ms_zrtp_available();
}

JNIEXPORT jboolean JNICALL Java_org_linphone_mediastream_Version_nativeHasVideo(JNIEnv *env, jclass c) {
#ifdef VIDEO_ENABLED
	return JNI_TRUE;
#else
	return JNI_FALSE;
#endif
}

#ifdef __cplusplus
}
#endif

static void set_high_prio(void){
	/*
		This pthread based code does nothing on linux. The linux kernel has
		sched_get_priority_max(SCHED_OTHER)=sched_get_priority_max(SCHED_OTHER)=0.
		As long as we can't use SCHED_RR or SCHED_FIFO, the only way to increase priority of a calling thread
		is to use setpriority().
	*/
#if 0
	struct sched_param param;
	int result=0;
	memset(&param,0,sizeof(param));
	int policy=SCHED_OTHER;
	param.sched_priority=sched_get_priority_max(policy);
	if((result=pthread_setschedparam(pthread_self(),policy, &param))) {
		ms_warning("Set sched param failed with error code(%i)\n",result);
	} else {
		ms_message("msandroid thread priority set to max (%i, min=%i)",sched_get_priority_max(policy),sched_get_priority_min(policy));
	}
#endif
	if (setpriority(PRIO_PROCESS,0,-20)==-1){
		ms_warning("msandroid set_high_prio() failed: %s",strerror(errno));
	}
}
/*
 mediastreamer2 sound card functions
 */

void msandroid_sound_set_level(MSSndCard *card, MSSndCardMixerElem e, int percent)
{
}

int msandroid_sound_get_level(MSSndCard *card, MSSndCardMixerElem e)
{
	return 0;
}

void msandroid_sound_set_source(MSSndCard *card, MSSndCardCapture source)
{
}

static int sdk_version=0;

void msandroid_sound_init(MSSndCard *card){
	/*get running sdk version*/
	JNIEnv *jni_env = ms_get_jni_env();
	jclass version_class = jni_env->FindClass("android/os/Build$VERSION");
	jfieldID fid = jni_env->GetStaticFieldID(version_class, "SDK_INT", "I");
	sdk_version=jni_env->GetStaticIntField(version_class, fid);
	ms_message("SDK version [%i] detected",sdk_version);
	jni_env->DeleteLocalRef(version_class);
}

void msandroid_sound_uninit(MSSndCard *card){
}

void msandroid_sound_detect(MSSndCardManager *m);
MSSndCard *msandroid_sound_duplicate(MSSndCard *obj);

MSFilter *msandroid_sound_read_new(MSSndCard *card);
MSFilter *msandroid_sound_write_new(MSSndCard *card);

MSSndCardDesc msandroid_sound_card_desc = {
/*.driver_type=*/"ANDROID SND (deprecated)",
/*.detect=*/ msandroid_sound_detect,
/*.init=*/msandroid_sound_init,
/*.set_level=*/msandroid_sound_set_level,
/*.get_level=*/msandroid_sound_get_level,
/*.set_capture=*/msandroid_sound_set_source,
/*.set_control=*/NULL,
/*.get_control=*/NULL,
/*.create_reader=*/msandroid_sound_read_new,
/*.create_writer=*/msandroid_sound_write_new,
/*.uninit=*/msandroid_sound_uninit,
/*.duplicate=*/msandroid_sound_duplicate
};

MSSndCard *msandroid_sound_duplicate(MSSndCard *obj){
	MSSndCard *card=ms_snd_card_new(&msandroid_sound_card_desc);
	card->name=ms_strdup(obj->name);
	return card;
}

MSSndCard *msandroid_sound_card_new(MSSndCardManager *m) {
	SoundDeviceDescription *d = NULL;
	MSDevicesInfo *devices = NULL;
	MSSndCard *card = ms_snd_card_new(&msandroid_sound_card_desc);
	card->name = ms_strdup("Android Sound card");

	devices = ms_factory_get_devices_info(m->factory);
	d = ms_devices_info_get_sound_device_description(devices);
	
	if (d->flags & DEVICE_HAS_BUILTIN_AEC) {
		card->capabilities |= MS_SND_CARD_CAP_BUILTIN_ECHO_CANCELLER;
	}
	card->data = d;
	return card;
}

void msandroid_sound_detect(MSSndCardManager *m) {
	ms_debug("msandroid_sound_detect");
	MSSndCard *card = msandroid_sound_card_new(m);
	ms_snd_card_manager_add_card(m, card);
}


/*************filter commun functions*********/
class msandroid_sound_data {
public:
	msandroid_sound_data() : bits(16),rate(8000),nchannels(1),started(false),thread_id(0),forced_rate(false){
		ms_mutex_init(&mutex,NULL);
	};
	~msandroid_sound_data() {
		ms_mutex_destroy(&mutex);
	}
	unsigned int	bits;
	unsigned int	rate;
	unsigned int	nchannels;
	bool			started;
	ms_thread_t     thread_id;
	ms_mutex_t		mutex;
	int	buff_size; /*buffer size in bytes*/
	bool	forced_rate;
};


static int get_rate(MSFilter *f, void *data){
	msandroid_sound_data *d=(msandroid_sound_data*)f->data;
	*(int*)data=d->rate;
	return 0;
}


static int set_nchannels(MSFilter *f, void *arg){
	ms_debug("set_nchannels %d", *((int*)arg));
	msandroid_sound_data *d=(msandroid_sound_data*)f->data;
	d->nchannels=*(int*)arg;
	return 0;
}

static int get_nchannels(MSFilter *f, void *arg){
	ms_debug("get_nchannels %d", *((int*)arg));
	msandroid_sound_data *d=(msandroid_sound_data*)f->data;
	*(int*)arg = d->nchannels;
	return 0;
}


static unsigned int get_supported_rate(unsigned int prefered_rate) {
	JNIEnv *jni_env = ms_get_jni_env();
	jclass audio_record_class = jni_env->FindClass("android/media/AudioRecord");
	int size = jni_env->CallStaticIntMethod(audio_record_class
											,jni_env->GetStaticMethodID(audio_record_class,"getMinBufferSize", "(III)I")
											,prefered_rate
											,2/*CHANNEL_CONFIGURATION_MONO*/
											,2/*  ENCODING_PCM_16BIT */);


	if (size > 0) {
		return prefered_rate;
	} else {
		ms_warning("Cannot configure recorder with rate [%i]",prefered_rate);
		if (prefered_rate>48000) {
			return get_supported_rate(48000);
		}
		switch (prefered_rate) {
		case 12000:
		case 24000: return get_supported_rate(48000);
		case 48000: return get_supported_rate(44100);
		case 44100: return get_supported_rate(16000);
		case 16000: return get_supported_rate(8000);
				default: {
					ms_error("This Android sound card doesn't support any standard sample rate");
					return 0;
				}
		}

		return 0;
	}

}

/***********************************read filter********************/
static int set_read_rate(MSFilter *f, void *arg){
	unsigned int proposed_rate = *((unsigned int*)arg);
	ms_debug("set_rate %d",proposed_rate);
	msandroid_sound_data *d=(msandroid_sound_data*)f->data;
	if (d->forced_rate) {
		ms_warning("sample rate is forced by mediastreamer2 device table, skipping...");
		return -1;
	}

	d->rate=get_supported_rate(proposed_rate);
	if (d->rate == proposed_rate)
		return 0;
	else
		return -1;
}

static int get_latency(MSFilter *f, void *arg){
	msandroid_sound_data *d=(msandroid_sound_data*)f->data;
	if (!d->started){
		sound_read_setup(f);
		*((int*)arg)=(1000*d->buff_size)/(d->nchannels*2*d->rate);
	}
	return 0;
}

static int msandroid_hack_speaker_state(MSFilter *f, void *arg);

MSFilterMethod msandroid_sound_read_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE	, set_read_rate	},
	{	MS_FILTER_GET_SAMPLE_RATE	, get_rate	},
	{	MS_FILTER_SET_NCHANNELS		, set_nchannels	},
	{	MS_FILTER_GET_NCHANNELS		, get_nchannels	},
	{	MS_FILTER_GET_LATENCY	, get_latency},
	{	MS_AUDIO_CAPTURE_FORCE_SPEAKER_STATE,	msandroid_hack_speaker_state	},
	{	0				, NULL		}
};


class msandroid_sound_read_data : public msandroid_sound_data{
public:
	msandroid_sound_read_data() : audio_record(0),audio_record_class(0),read_buff(0),read_chunk_size(0) {
		ms_bufferizer_init(&rb);
		aec=NULL;
	}
	~msandroid_sound_read_data() {
		ms_bufferizer_uninit (&rb);
	}
	jobject			audio_record;
	jclass 			audio_record_class;
	jbyteArray		read_buff;
	MSBufferizer 		rb;
	int			read_chunk_size;
	int framesize;
	int outgran_ms;
	int min_avail;
	int64_t start_time;
	int64_t read_samples;
	MSTickerSynchronizer *ticker_synchronizer;
	jobject aec;
	bool builtin_aec;
};

static void compute_timespec(msandroid_sound_read_data *d) {
	static int count = 0;
	uint64_t ns = ((1000 * d->read_samples) / (uint64_t) d->rate) * 1000000;
	MSTimeSpec ts;
	ts.tv_nsec = ns % 1000000000;
	ts.tv_sec = ns / 1000000000;
	double av_skew = ms_ticker_synchronizer_set_external_time(d->ticker_synchronizer, &ts);
	if ((++count) % 100 == 0)
		ms_message("sound/wall clock skew is average=%f ms", av_skew);
}

static void* msandroid_read_cb(msandroid_sound_read_data* d) {
	mblk_t *m;
	int nread;
	jmethodID read_id=0;
	jmethodID record_id=0;

	set_high_prio();

	JNIEnv *jni_env = ms_get_jni_env();
	record_id = jni_env->GetMethodID(d->audio_record_class,"startRecording", "()V");
	if(record_id==0) {
		ms_error("cannot find AudioRecord.startRecording() method");
		goto end;
	}
	//start recording
	ms_message("Start recording");
	jni_env->CallVoidMethod(d->audio_record,record_id);

	// int read (byte[] audioData, int offsetInBytes, int sizeInBytes)
	read_id = jni_env->GetMethodID(d->audio_record_class,"read", "([BII)I");
	if(read_id==0) {
		ms_error("cannot find AudioRecord.read() method");
		goto end;
	}

	while (d->started && (nread=jni_env->CallIntMethod(d->audio_record,read_id,d->read_buff,0, d->read_chunk_size))>0) {
		m = allocb(nread,0);
		jni_env->GetByteArrayRegion(d->read_buff, 0,nread, (jbyte*)m->b_wptr);
		//ms_error("%i octets read",nread);
		m->b_wptr += nread;
		d->read_samples+=nread/(2*d->nchannels);
		compute_timespec(d);
		ms_mutex_lock(&d->mutex);
		ms_bufferizer_put (&d->rb,m);
		ms_mutex_unlock(&d->mutex);
	};

	goto end;
	end: {
		ms_thread_exit(NULL);
		return 0;
	}
}

static void sound_read_setup(MSFilter *f){
	ms_debug("andsnd_read_preprocess");
	msandroid_sound_read_data *d=(msandroid_sound_read_data*)f->data;
	jmethodID constructor_id=0, methodID = 0;
	int audio_record_state=0;
	jmethodID min_buff_size_id;
	//jmethodID set_notification_period;
	int rc;

	JNIEnv *jni_env = ms_get_jni_env();
	d->audio_record_class = (jclass)jni_env->NewGlobalRef(jni_env->FindClass("android/media/AudioRecord"));
	if (d->audio_record_class == 0) {
		ms_error("cannot find android/media/AudioRecord");
		return;
	}

	constructor_id = jni_env->GetMethodID(d->audio_record_class,"<init>", "(IIIII)V");
	if (constructor_id == 0) {
		ms_error("cannot find AudioRecord (int audioSource, int sampleRateInHz, int channelConfig, int audioFormat, int bufferSizeInBytes)");
		return;
	}
	min_buff_size_id = jni_env->GetStaticMethodID(d->audio_record_class,"getMinBufferSize", "(III)I");
	if (min_buff_size_id == 0) {
		ms_error("cannot find AudioRecord.getMinBufferSize(int sampleRateInHz, int channelConfig, int audioFormat)");
		return;
	}
	d->buff_size = jni_env->CallStaticIntMethod(d->audio_record_class,min_buff_size_id,d->rate,2/*CHANNEL_CONFIGURATION_MONO*/,2/*  ENCODING_PCM_16BIT */);
	d->read_chunk_size = d->buff_size/4;
	d->buff_size*=2;/*double the size for configuring the recorder: this does not affect latency but prevents "AudioRecordThread: buffer overflow"*/

	if (d->buff_size > 0) {
		ms_message("Configuring recorder with [%i] bits  rate [%i] nchanels [%i] buff size [%i], chunk size [%i]"
				,d->bits
				,d->rate
				,d->nchannels
				,d->buff_size
				,d->read_chunk_size);
	} else {
		ms_message("Cannot configure recorder with [%i] bits  rate [%i] nchanels [%i] buff size [%i] chunk size [%i]"
				,d->bits
				,d->rate
				,d->nchannels
				,d->buff_size
				,d->read_chunk_size);
		return;
	}

	d->read_buff = jni_env->NewByteArray(d->buff_size);
	d->read_buff = (jbyteArray)jni_env->NewGlobalRef(d->read_buff);
	if (d->read_buff == 0) {
		ms_error("cannot instanciate read buff");
		return;
	}

	d->audio_record =  jni_env->NewObject(d->audio_record_class
			,constructor_id
			,sdk_version<11?1/*MIC*/:7/*VOICE_COMMUNICATION*/
			,d->rate
			,2/*CHANNEL_CONFIGURATION_MONO*/
			,2/*  ENCODING_PCM_16BIT */
			,d->buff_size);

	//Check the state of the AudioRecord (uninitialized = 1
	methodID = jni_env->GetMethodID(d->audio_record_class,"getState", "()I");
	if (methodID == 0) {
		ms_error("cannot find AudioRecord getState() method");
		return;
	}
	audio_record_state = jni_env->CallIntMethod(d->audio_record, methodID);

	if(audio_record_state == 1) {
		d->audio_record = jni_env->NewGlobalRef(d->audio_record);
		if (d->audio_record == 0) {
			ms_error("cannot instantiate AudioRecord");
			return;
		}
	} else {
		d->audio_record = NULL;
		ms_error("AudioRecord is not initialized properly. It may be caused by RECORD_AUDIO permission not granted");
	}

	d->min_avail=-1;
	d->read_samples=0;
	d->ticker_synchronizer = ms_ticker_synchronizer_new();
	d->outgran_ms=20;
	d->start_time=-1;
	d->framesize=(d->outgran_ms*d->rate)/1000;
	d->started=true;
	// start reader thread
	if(d->audio_record) {
		rc = ms_thread_create(&d->thread_id, 0, (void*(*)(void*))msandroid_read_cb, d);
		if (rc){
			ms_error("cannot create read thread return code  is [%i]", rc);
			d->started=false;
        }
	}
}

static void sound_read_preprocess(MSFilter *f){
	msandroid_sound_read_data *d=(msandroid_sound_read_data*)f->data;
	ms_debug("andsnd_read_preprocess");
	if (!d->started)
		sound_read_setup(f);
	ms_ticker_set_time_func(f->ticker,(uint64_t (*)(void*))ms_ticker_synchronizer_get_corrected_time, d->ticker_synchronizer);

	if (d->builtin_aec && d->audio_record) {
		JNIEnv *env=ms_get_jni_env();
		jmethodID getsession_id=0;
		int sessionId=-1;
		getsession_id = env->GetMethodID(d->audio_record_class,"getAudioSessionId", "()I");
		if(getsession_id==0) {
			ms_error("cannot find AudioRecord.getAudioSessionId() method");
			return;
		}
		sessionId = env->CallIntMethod(d->audio_record,getsession_id);
		ms_message("AudioRecord.getAudioSessionId() returned %i", sessionId);
		if (sessionId==-1) {
			return;
		}
		d->aec = enable_hardware_echo_canceller(env, sessionId);
	}
}

static void sound_read_postprocess(MSFilter *f){
	msandroid_sound_read_data *d=(msandroid_sound_read_data*)f->data;
	jmethodID stop_id=0;
	jmethodID release_id=0;

	JNIEnv *jni_env = ms_get_jni_env();

	ms_ticker_set_time_func(f->ticker,NULL,NULL);
	d->read_samples=0;

	//stop recording
	stop_id = jni_env->GetMethodID(d->audio_record_class,"stop", "()V");
	if(stop_id==0) {
		ms_error("cannot find AudioRecord.stop() method");
		goto end;
	}

	d->started = false;
	if (d->thread_id !=0 ){
		ms_thread_join(d->thread_id,0);
		d->thread_id = 0;
	}

	if (d->audio_record) {
		jni_env->CallVoidMethod(d->audio_record,stop_id);

		//release recorder
		release_id = jni_env->GetMethodID(d->audio_record_class,"release", "()V");
		if(release_id==0) {
			ms_error("cannot find AudioRecord.release() method");
			goto end;
		}
		jni_env->CallVoidMethod(d->audio_record,release_id);
	}
	if (d->aec) {
		delete_hardware_echo_canceller(jni_env, d->aec);
		d->aec = NULL;
	}
	goto end;
	end: {
		if (d->audio_record) jni_env->DeleteGlobalRef(d->audio_record);
		jni_env->DeleteGlobalRef(d->audio_record_class);
		if (d->read_buff) jni_env->DeleteGlobalRef(d->read_buff);
		return;
	}
}

static void sound_read_process(MSFilter *f){
	msandroid_sound_read_data *d=(msandroid_sound_read_data*)f->data;
	int nbytes=d->framesize*d->nchannels*2;
	int avail;
	bool_t flush=FALSE;
	bool_t can_output=(d->start_time==-1 || ((f->ticker->time-d->start_time)%d->outgran_ms==0));

	ms_mutex_lock(&d->mutex);
	if (!d->started) {
		ms_mutex_unlock(&d->mutex);
		return;
	}
	avail=ms_bufferizer_get_avail(&d->rb);
	if (f->ticker->time % 5000==0){
		if (d->min_avail>=(sndread_flush_threshold*(float)d->rate*2.0*(float)d->nchannels)){
			int excess_ms=(d->min_avail*1000)/(d->rate*2*d->nchannels);
			ms_warning("Excess of audio samples in capture side bytes=%i (%i ms)",d->min_avail,excess_ms);
			can_output=TRUE;
			flush=TRUE;
		}
		d->min_avail=-1;
	}
	do{
		if (can_output && (avail>=nbytes*2)){//bytes*2 is to insure smooth output, we leave at least one packet in the buffer for next time*/
			mblk_t *om=allocb(nbytes,0);
			ms_bufferizer_read(&d->rb,om->b_wptr,nbytes);
			om->b_wptr+=nbytes;
			ms_queue_put(f->outputs[0],om);
			//ms_message("Out time=%llu ",f->ticker->time);
			if (d->start_time==-1) d->start_time=f->ticker->time;
			avail-=nbytes;
		}else break;
	}while(flush);
	ms_mutex_unlock(&d->mutex);
	if (d->min_avail==-1 || avail<d->min_avail) d->min_avail=avail;
}


static MSFilterDesc msandroid_sound_read_desc={
/*.id=*/MS_FILTER_PLUGIN_ID,
/*.name=*/"MSAndSoundRead",
/*.text=*/N_("Sound capture filter for Android"),
/*.category=*/MS_FILTER_OTHER,
/*.enc_fmt*/NULL,
/*.ninputs=*/0,
/*.noutputs=*/1,
/*.init*/NULL,
/*.preprocess=*/sound_read_preprocess,
/*.process=*/sound_read_process,
/*.postprocess=*/sound_read_postprocess,
/*.uninit*/NULL,
/*.methods=*/msandroid_sound_read_methods
};

MSFilter *msandroid_sound_read_new(MSSndCard *card){
	ms_debug("msandroid_sound_read_new");
	MSFilter *f=ms_factory_create_filter_from_desc(ms_snd_card_get_factory(card), &msandroid_sound_read_desc);
	msandroid_sound_read_data *data=new msandroid_sound_read_data();
	data->builtin_aec = card->capabilities & MS_SND_CARD_CAP_BUILTIN_ECHO_CANCELLER;
	if (card->data != NULL) {
		SoundDeviceDescription *d = (SoundDeviceDescription *)card->data;
		if (d->recommended_rate > 0) {
			data->rate = d->recommended_rate;
			data->forced_rate = true;
			ms_warning("Using forced sample rate %i", data->rate);
		}
	}
	f->data=data;
	return f;
}

/***********************************write filter********************/
static int set_write_rate(MSFilter *f, void *arg){
#ifndef USE_HARDWARE_RATE
	msandroid_sound_data *d=(msandroid_sound_data*)f->data;
	if (d->forced_rate) {
		ms_warning("sample rate is forced by mediastreamer2 device table, skipping...");
		return -1;
	}

	int proposed_rate = *((int*)arg);
	ms_debug("set_rate %d",proposed_rate);
	d->rate=proposed_rate;
	return 0;
#else
/*audioflingler resampling is really bad
we prefer do resampling by ourselves if cpu allows it*/
	return -1;
#endif
}

MSFilterMethod msandroid_sound_write_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE	, set_write_rate	},
	{	MS_FILTER_GET_SAMPLE_RATE	, get_rate	},
	{	MS_FILTER_SET_NCHANNELS		, set_nchannels	},
	{	MS_FILTER_GET_NCHANNELS		, get_nchannels	},
	{	0				, NULL		}
};


class msandroid_sound_write_data : public msandroid_sound_data{
public:
	msandroid_sound_write_data() :audio_track_class(0),audio_track(0),write_chunk_size(0),writtenBytes(0),last_sample_date(0){
		bufferizer = ms_bufferizer_new();
		ms_cond_init(&cond,0);
		JNIEnv *jni_env = ms_get_jni_env();
		audio_track_class = (jclass)jni_env->NewGlobalRef(jni_env->FindClass("android/media/AudioTrack"));
		if (audio_track_class == 0) {
			ms_error("cannot find  android/media/AudioTrack\n");
			return;
		}
		jmethodID hwrate_id = jni_env->GetStaticMethodID(audio_track_class,"getNativeOutputSampleRate", "(I)I");
		if (hwrate_id == 0) {
			ms_error("cannot find  int AudioRecord.getNativeOutputSampleRate(int streamType)");
			return;
		}
		rate = jni_env->CallStaticIntMethod(audio_track_class,hwrate_id,0 /*STREAM_VOICE_CALL*/);
		ms_message("Hardware sample rate is %i",rate);
	};
	~msandroid_sound_write_data() {
		ms_bufferizer_flush(bufferizer);
		ms_bufferizer_destroy(bufferizer);
		ms_cond_destroy(&cond);
		if (audio_track_class!=0){
			JNIEnv *env = ms_get_jni_env();
			env->DeleteGlobalRef(audio_track_class);
		}
	}
	jclass 			audio_track_class;
	jobject			audio_track;
	MSBufferizer	*bufferizer;
	ms_cond_t		cond;
	int 			write_chunk_size;
	unsigned int	writtenBytes;
	unsigned long 	last_sample_date;
	bool sleeping;
	unsigned int getWriteBuffSize() {
		return buff_size;
	}
	int getWrittenFrames() {
		return writtenBytes/(nchannels*(bits/8));
	}
};

static void* msandroid_write_cb(msandroid_sound_write_data* d) {
	jbyteArray 		write_buff;
	jmethodID 		write_id=0;
	jmethodID play_id=0;
	int min_size=-1;
	int count;
	int max_size=sndwrite_flush_threshold*(float)d->rate*(float)d->nchannels*2.0;
	int check_point_size=3*(float)d->rate*(float)d->nchannels*2.0; /*3 seconds*/
	int nwrites=0;

	int buff_size = d->write_chunk_size;
	uint8_t tmpBuff[buff_size];
	JNIEnv *jni_env = ms_get_jni_env();

	set_high_prio();

	// int write  (byte[] audioData, int offsetInBytes, int sizeInBytes)
	write_id = jni_env->GetMethodID(d->audio_track_class,"write", "([BII)I");
	if(write_id==0) {
		ms_error("cannot find AudioTrack.write() method");
		goto end;
	}
	play_id = jni_env->GetMethodID(d->audio_track_class,"play", "()V");
	if(play_id==0) {
		ms_error("cannot find AudioTrack.play() method");
		goto end;
	}
	write_buff = jni_env->NewByteArray(buff_size);

	//start playing
	jni_env->CallVoidMethod(d->audio_track,play_id);

	ms_mutex_lock(&d->mutex);
	ms_bufferizer_flush(d->bufferizer);
	ms_mutex_unlock(&d->mutex);

	while(d->started) {
		int bufferizer_size;

		ms_mutex_lock(&d->mutex);
		min_size=-1;
		count=0;
		while((bufferizer_size = ms_bufferizer_get_avail(d->bufferizer)) >= d->write_chunk_size) {
			if (min_size==-1) min_size=bufferizer_size;
			else if (bufferizer_size<min_size) min_size=bufferizer_size;

			ms_bufferizer_read(d->bufferizer, tmpBuff, d->write_chunk_size);
			ms_mutex_unlock(&d->mutex);
			jni_env->SetByteArrayRegion(write_buff,0,d->write_chunk_size,(jbyte*)tmpBuff);
			int result = jni_env->CallIntMethod(d->audio_track,write_id,write_buff,0,d->write_chunk_size);
			d->writtenBytes+=result;
			if (result <= 0) {
				ms_error("write operation has failed [%i]",result);
			}
			nwrites++;
			ms_mutex_lock(&d->mutex);
			count+=d->write_chunk_size;
			if (count>check_point_size){
				if (min_size > max_size) {
					ms_warning("we are late, flushing %i bytes",min_size);
					ms_bufferizer_skip_bytes(d->bufferizer,min_size);
				}
				count=0;
			}
		}
		if (d->started) {
			d->sleeping=true;
			ms_cond_wait(&d->cond,&d->mutex);
			d->sleeping=false;
		}
		ms_mutex_unlock(&d->mutex);
	}


	goto end;
	end: {
		ms_thread_exit(NULL);
		return NULL;
	}
}

void msandroid_sound_write_preprocess(MSFilter *f){
	ms_debug("andsnd_write_preprocess");
	msandroid_sound_write_data *d=(msandroid_sound_write_data*)f->data;
	jmethodID constructor_id=0;

	int rc;
	jmethodID min_buff_size_id;

	JNIEnv *jni_env = ms_get_jni_env();

	if (d->audio_track_class == 0) {
		return;
	}

	constructor_id = jni_env->GetMethodID(d->audio_track_class,"<init>", "(IIIIII)V");
	if (constructor_id == 0) {
		ms_error("cannot find  AudioTrack(int streamType, int sampleRateInHz, \
		int channelConfig, int audioFormat, int bufferSizeInBytes, int mode)");
		return;
	}

	min_buff_size_id = jni_env->GetStaticMethodID(d->audio_track_class,"getMinBufferSize", "(III)I");
	if (min_buff_size_id == 0) {
		ms_error("cannot find  AudioTrack.getMinBufferSize(int sampleRateInHz, int channelConfig, int audioFormat)");
		return;
	}
	d->buff_size = jni_env->CallStaticIntMethod(d->audio_track_class,min_buff_size_id,d->rate,2/*CHANNEL_CONFIGURATION_MONO*/,2/*  ENCODING_PCM_16BIT */);
	d->write_chunk_size= (d->rate*(d->bits/8)*d->nchannels)*0.02;
	//d->write_chunk_size=d->buff_size;
	if (d->buff_size > 0) {
		ms_message("Configuring player with [%i] bits  rate [%i] nchanels [%i] buff size [%i] chunk size [%i]"
				,d->bits
				,d->rate
				,d->nchannels
				,d->buff_size
				,d->write_chunk_size);
	} else {
		ms_message("Cannot configure player with [%i] bits  rate [%i] nchanels [%i] buff size [%i] chunk size [%i]"
				,d->bits
				,d->rate
				,d->nchannels
				,d->buff_size
				,d->write_chunk_size);
		return;
	}
	d->audio_track =  jni_env->NewObject(d->audio_track_class
			,constructor_id
			,0/*STREAM_VOICE_CALL*/
			,d->rate
			,2/*CHANNEL_CONFIGURATION_MONO*/
			,2/*  ENCODING_PCM_16BIT */
			,d->buff_size
			,1/*MODE_STREAM */);
	d->audio_track = jni_env->NewGlobalRef(d->audio_track);
	if (d->audio_track == 0) {
		ms_error("cannot instanciate AudioTrack");
		return;
	}


	// start reader thread
	d->started = true;
	rc = ms_thread_create(&d->thread_id, 0, (void*(*)(void*))msandroid_write_cb, d);
	if (rc){
		ms_error("cannot create write thread return code  is [%i]", rc);
		d->started = false;
		return;
	}
}

void msandroid_sound_write_postprocess(MSFilter *f){
	msandroid_sound_write_data *d=(msandroid_sound_write_data*)f->data;
	jmethodID flush_id=0;
	jmethodID stop_id=0;
	jmethodID release_id=0;
	JNIEnv *jni_env = ms_get_jni_env();

	d->started=false;
	ms_mutex_lock(&d->mutex);
	ms_cond_signal(&d->cond);
	ms_mutex_unlock(&d->mutex);
	if (d->thread_id != 0){
		ms_thread_join(d->thread_id, NULL);
		d->thread_id = 0;
	}
	// flush
	flush_id = jni_env->GetMethodID(d->audio_track_class,"flush", "()V");
	if(flush_id==0) {
		ms_error("cannot find AudioTrack.flush() method");
		goto end;
	}
	if (d->audio_track) {

		jni_env->CallVoidMethod(d->audio_track,flush_id);

		//stop playing
		stop_id = jni_env->GetMethodID(d->audio_track_class,"stop", "()V");
		if(stop_id==0) {
			ms_error("cannot find AudioTrack.stop() method");
			goto end;
		}
		jni_env->CallVoidMethod(d->audio_track,stop_id);

		//release playing
		release_id = jni_env->GetMethodID(d->audio_track_class,"release", "()V");
		if(release_id==0) {
			ms_error("cannot find AudioTrack.release() method");
			goto end;
		}
		jni_env->CallVoidMethod(d->audio_track,release_id);
	}

	goto end;
end: {
	if (d->audio_track) jni_env->DeleteGlobalRef(d->audio_track);
	//d->jvm->DetachCurrentThread();
	return;
}

}



void msandroid_sound_write_process(MSFilter *f){
	msandroid_sound_write_data *d=(msandroid_sound_write_data*)f->data;

	mblk_t *m;
	while((m=ms_queue_get(f->inputs[0]))!=NULL){
		if (d->started){
			ms_mutex_lock(&d->mutex);
			ms_bufferizer_put(d->bufferizer,m);
			if (d->sleeping)
				ms_cond_signal(&d->cond);
			d->last_sample_date=f->ticker->time;
			ms_mutex_unlock(&d->mutex);
		}else freemsg(m);
	}
}


static MSFilterDesc msandroid_sound_write_desc={
/*.id=*/MS_FILTER_PLUGIN_ID,
/*.name=*/"MSAndSoundWrite",
/*.text=*/N_("Sound playback filter for Android"),
/*.category=*/MS_FILTER_OTHER,
/*.enc_fmt*/NULL,
/*.ninputs=*/1,
/*.noutputs=*/0,
/*.init*/NULL,
/*.preprocess=*/msandroid_sound_write_preprocess,
/*.process=*/msandroid_sound_write_process,
/*.postprocess=*/msandroid_sound_write_postprocess,
/*.uninit*/NULL,
/*.methods=*/msandroid_sound_write_methods
};


MSFilter *msandroid_sound_write_new(MSSndCard *card){
	ms_debug("msandroid_sound_write_new");
	MSFilter *f=ms_factory_create_filter_from_desc(ms_snd_card_get_factory(card), &msandroid_sound_write_desc);
	msandroid_sound_write_data *data = new msandroid_sound_write_data();
	if (card->data != NULL) {
		SoundDeviceDescription *d = (SoundDeviceDescription *)card->data;
		if (d->recommended_rate > 0) {
			data->rate = d->recommended_rate;
			data->forced_rate = true;
			ms_warning("Using forced sample rate %i", data->rate);
		}
	}
	f->data = data;
	return f;
}




/******* Hack for Galaxy S ***********/

static int msandroid_hack_speaker_state(MSFilter *f, void *arg) {
	msandroid_sound_read_data *d=(msandroid_sound_read_data*)f->data;
	bool speakerOn = *((bool *)arg);

	if (!d->started) {
		ms_error("Audio recorder not started, can't hack speaker");
		return -1;
	}

	JNIEnv *jni_env = ms_get_jni_env();

	// First, check that required methods are found
	jclass LinphoneManager_class = (jclass)jni_env->NewGlobalRef(jni_env->FindClass("org/linphone/LinphoneManager"));
	if (LinphoneManager_class == 0) {
		ms_error("Cannot find org/linphone/LinphoneManager");
		return -1;
	}
	jclass LinphoneCoreImpl_class = (jclass)jni_env->NewGlobalRef(jni_env->FindClass("org/linphone/core/LinphoneCoreImpl"));
	if (LinphoneCoreImpl_class == 0) {
		ms_error("Cannot find org/linphone/core/LinphoneCoreImpl");
		return -1;
	}
	jmethodID getLc_id = jni_env->GetStaticMethodID(LinphoneManager_class, "getLc", "()Lorg/linphone/core/LinphoneCore;");
	if (getLc_id == 0) {
		ms_error("Cannot find LinphoneManager.getLc()");
		return -1;
	}
	jmethodID routeAudioToSpeakerHelper_id = jni_env->GetMethodID(LinphoneCoreImpl_class,"routeAudioToSpeakerHelper", "(Z)V");
	if (routeAudioToSpeakerHelper_id == 0) {
		ms_error("Cannot find LinphoneCoreImpl.routeAudioToSpeakerHelper()");
		return -1;
	}
	jobject lc = jni_env->CallStaticObjectMethod(LinphoneManager_class, getLc_id);

	ms_mutex_lock(&d->mutex);
	d->started = false;
	ms_mutex_unlock(&d->mutex);

	// Stop audio recorder
	ms_message("Hacking speaker state: calling sound_read_postprocess()");
	sound_read_postprocess(f);

	// Flush eventual sound in the buffer
	// No need to lock as reader_cb is stopped
	ms_bufferizer_flush(&d->rb);

	// Change speaker state by calling back to java code
	// as there seems to be no way to get a reference to AudioManager service.
	ms_message("Hacking speaker state: do magic from LinphoneCoreImpl.RouteAudioToSpeakerHelper()");
	jni_env->CallVoidMethod(lc, routeAudioToSpeakerHelper_id, speakerOn);

	// Re-open audio and set d->started=true
	ms_message("Hacking speaker state: calling sound_read_preprocess()");
	sound_read_preprocess(f);

	return 0;
}

/******* End Hack for Galaxy S ***********/

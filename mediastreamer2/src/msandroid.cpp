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
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "mediastreamer2/mssndcard.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include <jni.h>

JavaVM *ms_andsnd_jvm;

static void sound_read_setup(MSFilter *f);

static void set_high_prio(void){
	struct sched_param param;
	int result=0;
	memset(&param,0,sizeof(param));
	int policy=SCHED_OTHER;
	param.sched_priority=sched_get_priority_max(policy);
	if((result=pthread_setschedparam(pthread_self(),policy, &param))) {
		ms_warning("Set sched param failed with error code(%i)\n",result);
	} else {
		ms_message("msandroid thread priority set to max");
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

void msandroid_sound_init(MSSndCard *card){
}

void msandroid_sound_uninit(MSSndCard *card){
}

void msandroid_sound_detect(MSSndCardManager *m);
MSSndCard *msandroid_sound_duplicate(MSSndCard *obj);

MSFilter *msandroid_sound_read_new(MSSndCard *card);
MSFilter *msandroid_sound_write_new(MSSndCard *card);

MSSndCardDesc msandroid_sound_card_desc = {
/*.driver_type=*/"ANDROID SND",
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
	MSSndCard *card=card=ms_snd_card_new(&msandroid_sound_card_desc);
	card->name=ms_strdup(obj->name);
	return card;
}

MSSndCard *msandroid_sound_card_new(){
	MSSndCard *card=ms_snd_card_new(&msandroid_sound_card_desc);
	card->name=ms_strdup("Android Sound card");
	return card;
}

void msandroid_sound_detect(MSSndCardManager *m){
	ms_debug("msandroid_sound_detect");
	MSSndCard *card=msandroid_sound_card_new();
	ms_snd_card_manager_add_card(m,card);
}


/*************filter commun functions*********/
class msandroid_sound_data {
public:
	msandroid_sound_data() : jvm(ms_andsnd_jvm),bits(16),rate(8000),nchannels(1),started(false),thread_id(0){
		ms_mutex_init(&mutex,NULL);
	};
	~msandroid_sound_data() {
		ms_mutex_destroy(&mutex);
	}
	JavaVM			*jvm;
	unsigned int	rate;
	unsigned int	bits;
	unsigned int	nchannels;
	bool			started;
	ms_mutex_t		mutex;
	ms_thread_t     thread_id;
	int	buff_size; /*buffer size in bytes*/
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




/***********************************read filter********************/
static int set_read_rate(MSFilter *f, void *arg){
	int proposed_rate = *((int*)arg);
	ms_debug("set_rate %d",proposed_rate);
	msandroid_sound_data *d=(msandroid_sound_data*)f->data;
	d->rate=proposed_rate;
	return 0;
}

static int get_latency(MSFilter *f, void *arg){
	msandroid_sound_data *d=(msandroid_sound_data*)f->data;
	if (!d->started){
		sound_read_setup(f);
		*((int*)arg)=(1000*d->buff_size)/(d->nchannels*2*d->rate);
	}
	return 0;
}
             

MSFilterMethod msandroid_sound_read_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE	, set_read_rate	},
	{	MS_FILTER_GET_SAMPLE_RATE	, get_rate	},
	{	MS_FILTER_SET_NCHANNELS		, set_nchannels	},
	{	MS_FILTER_GET_LATENCY	, get_latency},
	{	0				, NULL		}
};


class msandroid_sound_read_data : public msandroid_sound_data{
public:
	msandroid_sound_read_data() : audio_record(0),audio_record_class(0),read_buff(0),read_chunk_size(0) {
		ms_bufferizer_init(&rb);
	}
	~msandroid_sound_read_data() {
		ms_bufferizer_uninit (&rb);
	}
	jobject			audio_record;
	jclass 			audio_record_class;
	jbyteArray		read_buff;
	MSBufferizer 		rb;
	int			read_chunk_size;
};

static void* msandroid_read_cb(msandroid_sound_read_data* d) {
	mblk_t *m;
	int nread;
	JNIEnv *jni_env = 0;
	jmethodID read_id=0;
	jmethodID record_id=0;

	set_high_prio();

	jint result = d->jvm->AttachCurrentThread(&jni_env,NULL);
	if (result != 0) {
		ms_error("cannot attach VM\n");
		goto end;
	}
	record_id = jni_env->GetMethodID(d->audio_record_class,"startRecording", "()V");
	if(record_id==0) {
		ms_error("cannot find AudioRecord.startRecording() method");
		goto end;
	}
	//start recording
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
		ms_mutex_lock(&d->mutex);
		ms_bufferizer_put (&d->rb,m);
		ms_mutex_unlock(&d->mutex);
	};
	goto end;
	end: {
		d->jvm->DetachCurrentThread();
		return 0;
	}
}

static void sound_read_setup(MSFilter *f){
	ms_debug("andsnd_read_preprocess");
	msandroid_sound_read_data *d=(msandroid_sound_read_data*)f->data;
	JNIEnv *jni_env = 0;
	jmethodID constructor_id=0;
	jmethodID min_buff_size_id;
	//jmethodID set_notification_period;
	int rc;

	jint result = d->jvm->AttachCurrentThread(&jni_env,NULL);
	if (result != 0) {
		ms_error("cannot attach VM\n");
		goto end;
	}
	d->audio_record_class = (jclass)jni_env->NewGlobalRef(jni_env->FindClass("android/media/AudioRecord"));
	if (d->audio_record_class == 0) {
		ms_error("cannot find  android/media/AudioRecord\n");
		goto end;
	}

	constructor_id = jni_env->GetMethodID(d->audio_record_class,"<init>", "(IIIII)V");
	if (constructor_id == 0) {
		ms_error("cannot find  AudioRecord (int audioSource, int sampleRateInHz, \
		int channelConfig, int audioFormat, int bufferSizeInBytes)");
		goto end;
	}
	min_buff_size_id = jni_env->GetStaticMethodID(d->audio_record_class,"getMinBufferSize", "(III)I");
	if (min_buff_size_id == 0) {
		ms_error("cannot find  AudioRecord.getMinBufferSize(int sampleRateInHz, int channelConfig, int audioFormat)");
		goto end;
	}
	d->buff_size = jni_env->CallStaticIntMethod(d->audio_record_class,min_buff_size_id,d->rate,2/*CHANNEL_CONFIGURATION_MONO*/,2/*  ENCODING_PCM_16BIT */);
	d->read_chunk_size = d->buff_size/2;

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
		goto end;
	}

	d->read_buff = jni_env->NewByteArray(d->buff_size);
	d->read_buff = (jbyteArray)jni_env->NewGlobalRef(d->read_buff);
	if (d->read_buff == 0) {
		ms_error("cannot instanciate read buff");
		goto end;
	}

	d->audio_record =  jni_env->NewObject(d->audio_record_class
			,constructor_id
			,1/*MIC*/
			,d->rate
			,2/*CHANNEL_CONFIGURATION_MONO*/
			,2/*  ENCODING_PCM_16BIT */
			,d->buff_size);


	d->audio_record = jni_env->NewGlobalRef(d->audio_record);
	if (d->audio_record == 0) {
		ms_error("cannot instanciate AudioRecord");
		goto end;
	}

	d->started=true;
	// start reader thread
	rc = ms_thread_create(&d->thread_id, 0, (void*(*)(void*))msandroid_read_cb, d);
	if (rc){
		ms_error("cannot create read thread return code  is [%i]", rc);
		d->started=false;
		goto end;

	}

	goto end;
	end: {
		//d->jvm->DetachCurrentThread();
		return;
	}
}

static void sound_read_preprocess(MSFilter *f){
	msandroid_sound_read_data *d=(msandroid_sound_read_data*)f->data;
	ms_debug("andsnd_read_preprocess");
	if (!d->started)
		sound_read_setup(f);
}

static void sound_read_postprocess(MSFilter *f){
	msandroid_sound_read_data *d=(msandroid_sound_read_data*)f->data;
	JNIEnv *jni_env = 0;
	jmethodID flush_id=0;
	jmethodID stop_id=0;
	jmethodID release_id=0;
	jint result = d->jvm->AttachCurrentThread(&jni_env,NULL);
	if (result != 0) {
		ms_error("cannot attach VM\n");
		goto end;
	}

	//stop recording
	stop_id = jni_env->GetMethodID(d->audio_record_class,"stop", "()V");
	if(stop_id==0) {
		ms_error("cannot find AudioRecord.stop() method");
		goto end;
	}

	d->started = false;
	if (d->thread_id !=0) ms_thread_join(d->thread_id,0);

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
	goto end;
	end: {
		if (d->audio_record) jni_env->DeleteGlobalRef(d->audio_record);
		jni_env->DeleteGlobalRef(d->audio_record_class);
		if (d->read_buff) jni_env->DeleteGlobalRef(d->read_buff);

		//d->jvm->DetachCurrentThread();
		return;
	}
}

static void sound_read_process(MSFilter *f){
	msandroid_sound_read_data *d=(msandroid_sound_read_data*)f->data;
	mblk_t *m;
	int nbytes=0.02*(float)d->rate*2.0*(float)d->nchannels;

	// output a buffer only every 2 ticks + alpha
	if ((f->ticker->time % 20)==0 || (f->ticker->time % 510)==0){
		mblk_t *om=allocb(nbytes,0);
		int err;
		ms_mutex_lock(&d->mutex);
		err=ms_bufferizer_read(&d->rb,om->b_wptr,nbytes);
		ms_mutex_unlock(&d->mutex);
		if (err==nbytes){
			om->b_wptr+=nbytes;
			ms_queue_put(f->outputs[0],om);
		}else freemsg(om);
	}
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
	MSFilter *f=ms_filter_new_from_desc(&msandroid_sound_read_desc);
	f->data=new msandroid_sound_read_data();
	return f;
}

MS_FILTER_DESC_EXPORT(msandroid_sound_read_desc)

/***********************************write filter********************/
static int set_write_rate(MSFilter *f, void *arg){
	msandroid_sound_data *d=(msandroid_sound_data*)f->data;
#ifndef USE_HARDWARE_RATE
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
	{	0				, NULL		}
};


class msandroid_sound_write_data : public msandroid_sound_data{
public:
	msandroid_sound_write_data() :audio_track_class(0),audio_track(0),write_chunk_size(0),writtenBytes(0),last_sample_date(0){
		JNIEnv *jni_env=NULL;
		bufferizer = ms_bufferizer_new();
		ms_cond_init(&cond,0);
		if (jvm->AttachCurrentThread(&jni_env,NULL)!=0){
			ms_error("msandroid_sound_write_data(): could not attach current thread.");
			return;
		}
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
		JNIEnv *jni_env=NULL;
		ms_mutex_lock(&mutex);
		ms_bufferizer_flush(bufferizer);
		ms_mutex_unlock(&mutex);
		ms_bufferizer_destroy(bufferizer);
		ms_cond_destroy(&cond);
		if (audio_track_class!=0){
			if (jvm->AttachCurrentThread(&jni_env,NULL)!=0){
				ms_error("~msandroid_sound_write_data(): could not attach current thread.");
				return;
			}
			jni_env->DeleteGlobalRef(audio_track_class);
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
	JNIEnv 			*jni_env = 0;
	jbyteArray 		write_buff;
	jmethodID 		write_id=0;
	jmethodID play_id=0;

	jint result;
	set_high_prio();
	int buff_size = d->getWriteBuffSize();
	result = d->jvm->AttachCurrentThread(&jni_env,NULL);
	if (result != 0) {
		ms_error("cannot attach VM\n");
		goto end;
	}

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
	uint8_t tmpBuff[buff_size];

	//start playing
	jni_env->CallVoidMethod(d->audio_track,play_id);

	ms_bufferizer_flush(d->bufferizer);
	while(d->started) {
		mblk_t *m;
		ms_mutex_lock(&d->mutex);
		int bufferizer_size;
		while((bufferizer_size = ms_bufferizer_get_avail(d->bufferizer)) >= d->write_chunk_size) {
			if (bufferizer_size > (d->rate*(d->bits/8)*d->nchannels)*.250) { //250 ms
				ms_warning("we are late [%i] bytes, flushing",bufferizer_size);
				ms_bufferizer_flush(d->bufferizer);

			} else {
				ms_bufferizer_read(d->bufferizer, tmpBuff, d->write_chunk_size);
				ms_mutex_unlock(&d->mutex);
				jni_env->SetByteArrayRegion(write_buff,0,d->write_chunk_size,(jbyte*)tmpBuff);
				int result = jni_env->CallIntMethod(d->audio_track,write_id,write_buff,0,d->write_chunk_size);
				d->writtenBytes+=result;
				if (result <= 0) {
					ms_error("write operation has failed [%i]",result);
				}
				ms_mutex_lock(&d->mutex);
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
		d->jvm->DetachCurrentThread();
		return 0;
	}

}

void msandroid_sound_write_preprocess(MSFilter *f){
	ms_debug("andsnd_write_preprocess");
	msandroid_sound_write_data *d=(msandroid_sound_write_data*)f->data;
	JNIEnv *jni_env = 0;
	jmethodID constructor_id=0;

	int rc;
	jmethodID min_buff_size_id;

	jint result = d->jvm->AttachCurrentThread(&jni_env,NULL);
	if (result != 0) {
		ms_error("cannot attach VM\n");
		goto end;
	}
	
	if (d->audio_track_class == 0) {
		goto end;
	}

	constructor_id = jni_env->GetMethodID(d->audio_track_class,"<init>", "(IIIIII)V");
	if (constructor_id == 0) {
		ms_error("cannot find  AudioTrack(int streamType, int sampleRateInHz, \
		int channelConfig, int audioFormat, int bufferSizeInBytes, int mode)");
		goto end;
	}

	min_buff_size_id = jni_env->GetStaticMethodID(d->audio_track_class,"getMinBufferSize", "(III)I");
	if (min_buff_size_id == 0) {
		ms_error("cannot find  AudioTrack.getMinBufferSize(int sampleRateInHz, int channelConfig, int audioFormat)");
		goto end;
	}
	d->buff_size = jni_env->CallStaticIntMethod(d->audio_track_class,min_buff_size_id,d->rate,2/*CHANNEL_CONFIGURATION_MONO*/,2/*  ENCODING_PCM_16BIT */);
	d->write_chunk_size= (d->rate*(d->bits/8)*d->nchannels)*0.02;

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
		goto end;
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
		goto end;
	}


	// start reader thread
	d->started = true;
	rc = ms_thread_create(&d->thread_id, 0, (void*(*)(void*))msandroid_write_cb, d);
	if (rc){
		ms_error("cannot create write thread return code  is [%i]", rc);
		d->started = false;
		goto end;
	}

	goto end;
	end: {
		//d->jvm->DetachCurrentThread();
		return;
	}

}

void msandroid_sound_write_postprocess(MSFilter *f){
	msandroid_sound_write_data *d=(msandroid_sound_write_data*)f->data;
	JNIEnv *jni_env = 0;
	jmethodID flush_id=0;
	jmethodID stop_id=0;
	jmethodID release_id=0;
	jint result = d->jvm->AttachCurrentThread(&jni_env,NULL);
	if (result != 0) {
		ms_error("cannot attach VM\n");
		goto end;
	}

	d->started=false;
	ms_mutex_lock(&d->mutex);
	ms_cond_signal(&d->cond);
	ms_mutex_unlock(&d->mutex);
	ms_thread_join(d->thread_id,0);
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
	MSFilter *f=ms_filter_new_from_desc(&msandroid_sound_write_desc);
	f->data=new msandroid_sound_write_data();
	return f;
}


MS_FILTER_DESC_EXPORT(msandroid_sound_write_desc)

extern "C" void ms_andsnd_set_jvm(JavaVM *jvm) {

	ms_andsnd_jvm=jvm;
}


	

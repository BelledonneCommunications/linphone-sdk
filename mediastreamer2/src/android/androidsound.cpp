/*
 * androidsound.cpp -Android Media plugin for Linphone, based on C++ sound apis.
 *
 * Copyright (C) 2009-2012  Belledonne Communications, Grenoble, France
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

#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/msjava.h>
#include <mediastreamer2/msticker.h>
#include <mediastreamer2/mssndcard.h>
#include <mediastreamer2/devices.h>

#include "AudioTrack.h"
#include "AudioRecord.h"
#include "String8.h"

#include <jni.h>

#include "hardware_echo_canceller.h"

#define NATIVE_USE_HARDWARE_RATE 1
//#define TRACE_SND_WRITE_TIMINGS

using namespace::fake_android;

/*notification duration for audio callbacks, in ms*/
static const float audio_buf_ms=0.01;

static MSSndCard * android_snd_card_new(SoundDeviceDescription *d);
static MSFilter * ms_android_snd_read_new(MSFactory* factory);
static MSFilter * ms_android_snd_write_new(MSFactory* factory);
static Library *libmedia=0;
static Library *libutils=0;

static const int flowControlIntervalMs = 1000;
static const int flowControlThresholdMs = 40;

static int std_sample_rates[]={
	48000,44100,32000,22050,16000,8000,-1
};

struct AndroidNativeSndCardData{
	AndroidNativeSndCardData(int forced_rate, int flags): mVoipMode(0) ,mIoHandle(0),mFlags(flags){
		/* try to use the same sampling rate as the playback.*/
		int hwrate;
		mCaptureSource = flags & DEVICE_USE_ANDROID_MIC ? AUDIO_SOURCE_MIC:AUDIO_SOURCE_VOICE_COMMUNICATION;
		enableVoipMode();
		if (AudioSystem::getOutputSamplingRate(&hwrate,AUDIO_STREAM_VOICE_CALL)==0){
			ms_message("Hardware output sampling rate is %i",hwrate);
		}
		if (forced_rate){
			ms_message("Hardware is known to have bugs at default sampling rate, using %i Hz instead.",forced_rate);
			hwrate=forced_rate;
		}
		mPlayRate=mRecRate=hwrate;
		for(int i=0;;){
			int stdrate=std_sample_rates[i];
			if (stdrate>mRecRate) {
				i++;
				continue;
			}
			if (AudioRecord::getMinFrameCount(&mRecFrames, mRecRate, AUDIO_FORMAT_PCM_16_BIT,1)==0){
				ms_message("Minimal AudioRecord buf frame size at %i Hz is %i",mRecRate,mRecFrames);
				break;
			}else{
				ms_warning("Recording at  %i hz is not supported",mRecRate);
				i++;
				if (std_sample_rates[i]==-1){
					ms_error("Cannot find suitable sampling rate for recording !");
					return;
				}
				mRecRate=std_sample_rates[i];
			}
		}
		disableVoipMode();
#if 0
		mIoHandle=AudioSystem::getInput(AUDIO_SOURCE_VOICE_COMMUNICATION,mRecRate,AUDIO_FORMAT_PCM_16_BIT,AUDIO_CHANNEL_IN_MONO,(audio_in_acoustics_t)0,0);
		if (mIoHandle==0){
			ms_message("No io handle for AUDIO_SOURCE_VOICE_COMMUNICATION, trying AUDIO_SOURCE_VOICE_CALL");
			mIoHandle=AudioSystem::getInput(AUDIO_SOURCE_VOICE_CALL,mRecRate,AUDIO_FORMAT_PCM_16_BIT,AUDIO_CHANNEL_IN_MONO,(audio_in_acoustics_t)0,0);
			if (mIoHandle==0){
				ms_warning("No io handle for capture.");
			}
		}
#endif
	}
	void enableVoipMode(){
		mVoipMode++;
		if (mVoipMode==1){
			//hack for samsung devices
			status_t err;
			String8 params("voip=on");
			if ((err = AudioSystem::setParameters(mIoHandle,params))==0){
				ms_message("voip=on is set.");
			}else ms_warning("Could not set voip=on: err=%d.", err);
		}
	}
	void disableVoipMode(){
		mVoipMode--;
		if (mVoipMode==0){
			status_t err;
			String8 params("voip=off");
			if ((err = AudioSystem::setParameters(mIoHandle,params))==0){
				ms_message("voip=off is set.");
			}else ms_warning("Could not set voip=off: err=%d.", err);
		}
	}
	int mVoipMode;
	int mPlayRate;
	int mRecRate;
	int mRecFrames;
	audio_io_handle_t mIoHandle;
	audio_source_t mCaptureSource;
	int mFlags;
};

struct AndroidSndReadData{
	AndroidSndReadData() : rec(0){
		rate=8000;
		nchannels=1;
		qinit(&q);
		ms_mutex_init(&mutex,NULL);
		started=false;
		nbufs=0;
		audio_source=AUDIO_SOURCE_DEFAULT;
		mTickerSynchronizer=NULL;
		aec=NULL;
	}
	~AndroidSndReadData(){
		ms_mutex_destroy(&mutex);
		flushq(&q,0);
		rec=0;
	}
	void setCard(MSSndCard *card){
		mCard=(AndroidNativeSndCardData*)card->data;
#ifdef NATIVE_USE_HARDWARE_RATE
		rate=mCard->mRecRate;
#endif
		rec_buf_size=mCard->mRecFrames * 4;
		builtin_aec=card->capabilities & MS_SND_CARD_CAP_BUILTIN_ECHO_CANCELLER;
	}
	MSFilter *mFilter;
	AndroidNativeSndCardData *mCard;
	audio_source_t audio_source;
	int rate;
	int nchannels;
	ms_mutex_t mutex;
	queue_t q;
	sp<AudioRecord> rec;
	int nbufs;
	int rec_buf_size;
	MSTickerSynchronizer *mTickerSynchronizer;
	int64_t read_samples;
	audio_io_handle_t iohandle;
	jobject aec;
	double av_skew;
	bool started;
	bool builtin_aec;
};

struct AndroidSndWriteData{
	AndroidSndWriteData(){
		stype=AUDIO_STREAM_VOICE_CALL;
		rate=8000;
		nchannels=1;
		nFramesRequested=0;
		ms_mutex_init(&mutex,NULL);
		ms_bufferizer_init(&bf);
	}
	~AndroidSndWriteData(){
		ms_mutex_destroy(&mutex);
		ms_bufferizer_uninit(&bf);
	}
	void setCard(AndroidNativeSndCardData *card){
		mCard=card;
#ifdef NATIVE_USE_HARDWARE_RATE
		rate=card->mPlayRate;
#endif
	}
	AndroidNativeSndCardData *mCard;
	audio_stream_type_t stype;
	int rate;
	int nchannels;
	ms_mutex_t mutex;
	MSBufferizer bf;
	sp<AudioTrack> tr;
	int nbufs;
	int nFramesRequested;
	bool mStarted;
	uint64_t flowControlStart;
	int minBufferFilling;
};

static MSFilter *android_snd_card_create_reader(MSSndCard *card){
	MSFilter *f=ms_android_snd_read_new(ms_snd_card_get_factory(card));
	(static_cast<AndroidSndReadData*>(f->data))->setCard(card);
	return f;
}

static MSFilter *android_snd_card_create_writer(MSSndCard *card){
	MSFilter *f=ms_android_snd_write_new(ms_snd_card_get_factory(card));
	(static_cast<AndroidSndWriteData*>(f->data))->setCard((AndroidNativeSndCardData*)card->data);
	return f;
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

static void android_snd_card_detect(MSSndCardManager *m) {
	bool audio_record_loaded = false;
	bool audio_track_loaded = false;
	bool audio_system_loaded = false;
	bool string8_loaded = false;
	bool refbase_loaded = false;
	
	SoundDeviceDescription *d = NULL;
	MSDevicesInfo *devices = NULL;

	if (get_sdk_version() > 19) {
		/*it is actually working well on android 5 on Nexus 4 but crashes on Samsung S5, due to, maybe
		 * calling convention of C++ method being different. Arguments received by AudioTrack constructor do not match the arguments
		 * sent by the caller (TransferType maps to uid argument!).
		 * Until we find a rational explanation to this, the native module is disabled on Android 5.
		**/
		ms_message("Native android sound support not tested on SDK [%i], disabled.",get_sdk_version());
		return;
	}
	
	devices = ms_factory_get_devices_info(m->factory);
	d = ms_devices_info_get_sound_device_description(devices);
	if (d->flags & DEVICE_HAS_UNSTANDARD_LIBMEDIA){
		ms_message("Native android sound support is blacklisted for this device.");
		return;
	}
	
	/* libmedia and libutils static variable may survive to Linphone restarts
	 It is then necessary to perform the *::init() calls even if the libmedia and libutils are there.*/
	if (!libmedia) libmedia=Library::load("/system/lib/libmedia.so");
	if (!libutils) libutils=Library::load("/system/lib/libutils.so");
	
	if (libmedia && libutils) {
		/*perform initializations in order rather than in a if statement so that all missing symbols are shown in logs*/
		string8_loaded = String8Impl::init(libutils);
		refbase_loaded = RefBaseImpl::init(libutils);
		audio_record_loaded = AudioRecordImpl::init(libmedia);
		audio_track_loaded = AudioTrackImpl::init(libmedia);
		audio_system_loaded = AudioSystemImpl::init(libmedia);
	}
	
	
	
	if (audio_record_loaded && audio_track_loaded && audio_system_loaded && string8_loaded && refbase_loaded) {
		ms_message("Native android sound support available.");
		MSSndCard* card = android_snd_card_new(d);
		ms_snd_card_set_manager(m, card);
		ms_snd_card_manager_add_card(m, card);
		return;
	}
	ms_message("Native android sound support is NOT available.");
}

static void android_native_snd_card_init(MSSndCard *card) {
	
}

static void android_native_snd_card_uninit(MSSndCard *card){
	
	ms_warning("Deletion of [%p]",card->data);
	delete static_cast<AndroidNativeSndCardData*>(card->data);
}

MSSndCardDesc android_native_snd_card_desc={
	"libmedia",
	android_snd_card_detect,
	android_native_snd_card_init,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	android_snd_card_create_reader,
	android_snd_card_create_writer,
	android_native_snd_card_uninit
};



static MSSndCard * android_snd_card_new(SoundDeviceDescription *d)
{
	MSSndCard * obj;
	
	
	obj=ms_snd_card_new(&android_native_snd_card_desc);
	obj->name=ms_strdup("android sound card");
	
	if (d->flags & DEVICE_HAS_BUILTIN_AEC) obj->capabilities|=MS_SND_CARD_CAP_BUILTIN_ECHO_CANCELLER;
	obj->latency=d->delay;
	obj->data = new AndroidNativeSndCardData(	d->recommended_rate
												, d->flags);
	return obj;
}


static void android_snd_read_init(MSFilter *obj){
	AndroidSndReadData *ad=new AndroidSndReadData();
	obj->data=ad;
}

static void compute_timespec(AndroidSndReadData *d) {
	uint64_t ns = ((1000 * d->read_samples) / (uint64_t) d->rate) * 1000000;
	MSTimeSpec ts;
	ts.tv_nsec = ns % 1000000000;
	ts.tv_sec = ns / 1000000000;
	d->av_skew = ms_ticker_synchronizer_set_external_time(d->mTickerSynchronizer, &ts);
}

/*
 * This is a callback function called by AudioRecord's thread. This thread is not created by ortp/ms2 and is not able to attach to a JVM without crashing
 * at the end, despite it is detached (since android 4.4).
 * We must not output a single log within this callback in the event that the application is using LinphoneCoreFactory.setLogHandler(), in which case
 * the log would be upcalled to java, which will attach the thread to the jvm.
**/
static void android_snd_read_cb(int event, void* user, void *p_info){
	AndroidSndReadData *ad=(AndroidSndReadData*)user;
	
	if (!ad->started) return;
	if (ad->mTickerSynchronizer==NULL){
		MSFilter *obj=ad->mFilter;
		/*
		 * ABSOLUTE HORRIBLE HACK. We temporarily disable logs to prevent ms_ticker_set_time_func() to output a debug log.
		 * This is horrible because this also suspends logs for all concurrent threads during these two lines of code.
		 * Possible way to do better:
		 *  1) understand why AudioRecord thread doesn't detach.
		 *  2) disable logs just for this thread (using a TLS)
		 */
		int loglevel=ortp_get_log_level_mask(ORTP_LOG_DOMAIN);
		ortp_set_log_level_mask(NULL, ORTP_ERROR|ORTP_FATAL);
		ad->mTickerSynchronizer = ms_ticker_synchronizer_new();
		ms_ticker_set_time_func(obj->ticker,(uint64_t (*)(void*))ms_ticker_synchronizer_get_corrected_time, ad->mTickerSynchronizer);
		ortp_set_log_level_mask(ORTP_LOG_DOMAIN, loglevel);
	}
	if (event==AudioRecord::EVENT_MORE_DATA){
		AudioRecord::Buffer info;
		AudioRecord::readBuffer(p_info,&info);
		if (info.size > 0) {
			mblk_t *m=allocb(info.size,0);
			memcpy(m->b_wptr,info.raw,info.size);
			m->b_wptr+=info.size;
			ad->read_samples+=info.frameCount;

			ms_mutex_lock(&ad->mutex);
			compute_timespec(ad);
			putq(&ad->q,m);
			ms_mutex_unlock(&ad->mutex);
		}
	}else if (event==AudioRecord::EVENT_OVERRUN){
#ifdef TRACE_SND_READ_TIMINGS
		ms_warning("AudioRecord overrun");
#endif
	}
}

static void android_snd_read_activate_hardware_aec(MSFilter *obj){
	AndroidSndReadData *ad=(AndroidSndReadData*)obj->data;
	JNIEnv *env=ms_get_jni_env();
	int sessionId=ad->rec->getSessionId();
	ms_message("AudioRecord.getAudioSessionId() returned %i", sessionId);
	
	if (sessionId==-1) {
		return;
	}
	
	ad->aec = enable_hardware_echo_canceller(env, sessionId);
}

static void android_snd_read_preprocess(MSFilter *obj){
	AndroidSndReadData *ad=(AndroidSndReadData*)obj->data;
	status_t  ss;
	int notify_frames=(int)(audio_buf_ms*(float)ad->rate);
	
	ad->mCard->enableVoipMode();
	
	ad->mFilter=obj;
	ad->read_samples=0;
	ad->started=FALSE;
	ad->audio_source=ad->mCard->mCaptureSource; /*some device require to capture from MIC instead of from voice communications*/
	for(int i=0;i<2;i++){
		ad->rec=new AudioRecord(ad->audio_source,
						ad->rate,
						AUDIO_FORMAT_PCM_16_BIT,
						audio_channel_in_mask_from_count(ad->nchannels),
						ad->rec_buf_size,
						android_snd_read_cb,ad,notify_frames,0,AudioRecord::TRANSFER_DEFAULT, 
						(ad->mCard->mFlags & DEVICE_HAS_CRAPPY_ANDROID_FASTRECORD) ? AUDIO_INPUT_FLAG_NONE : AUDIO_INPUT_FLAG_FAST);
		ss=ad->rec->initCheck();
		ms_message("Setting up AudioRecord  source=%i,rate=%i,framecount=%i",ad->audio_source,ad->rate,ad->rec_buf_size);

		if (ss!=0){
			ms_error("Problem when setting up AudioRecord:%s ",strerror(-ss));
			ad->rec=0;
			if (i == 0) {
				ms_error("Retrying with AUDIO_SOURCE_MIC");
				ad->audio_source=AUDIO_SOURCE_MIC;
			}
		}else break;
	}

	if (ad->rec != 0) {
		if (ad->builtin_aec) android_snd_read_activate_hardware_aec(obj);
		ad->rec->start();
	}
}

static void android_snd_read_postprocess(MSFilter *obj){
	AndroidSndReadData *ad=(AndroidSndReadData*)obj->data;
	ms_message("Stopping sound capture");
	if (ad->rec!=0) {
		ad->started=false;
		ad->rec->stop();
		if (ad->aec){
			JNIEnv *env=ms_get_jni_env();
			delete_hardware_echo_canceller(env, ad->aec);
			ad->aec = NULL;
		}
		ad->rec=0;
	}
	ms_ticker_set_time_func(obj->ticker,NULL,NULL);
	ms_mutex_lock(&ad->mutex);
	ms_ticker_synchronizer_destroy(ad->mTickerSynchronizer);
	ad->mTickerSynchronizer=NULL;
	ms_mutex_unlock(&ad->mutex);
	ms_message("Sound capture stopped");
	ad->mCard->disableVoipMode();
}

static void android_snd_read_uninit(MSFilter *obj){
	AndroidSndReadData *ad=(AndroidSndReadData*)obj->data;
	delete ad;
}

static void android_snd_read_process(MSFilter *obj){
	AndroidSndReadData *ad=(AndroidSndReadData*)obj->data;
	mblk_t *om;
	
	ms_mutex_lock(&ad->mutex);
	if (ad->rec == 0 ) {
		ms_mutex_unlock(&ad->mutex);
		return;
	}
	if (!ad->started)
		ad->started=TRUE; //so that the callback can now start to queue buffers.

	while ((om=getq(&ad->q))!=NULL) {
		//ms_message("android_snd_read_process: Outputing %i bytes",msgdsize(om));
		ms_queue_put(obj->outputs[0],om);
		ad->nbufs++;
		if (ad->nbufs % 100 == 0)
			ms_message("sound/wall clock skew is average=%g ms", ad->av_skew);
	}
	ms_mutex_unlock(&ad->mutex);
}

static int android_snd_read_set_sample_rate(MSFilter *obj, void *param){
#ifndef NATIVE_USE_HARDWARE_RATE
	AndroidSndReadData *ad=(AndroidSndReadData*)obj->data;
	ad->rate=*((int*)param);
	return 0;
#else
	return -1;
#endif
}

static int android_snd_read_get_sample_rate(MSFilter *obj, void *param){
	AndroidSndReadData *ad=(AndroidSndReadData*)obj->data;
	*(int*)param=ad->rate;
	return 0;
}

static int android_snd_read_set_nchannels(MSFilter *obj, void *param){
	AndroidSndReadData *ad=(AndroidSndReadData*)obj->data;
	ad->nchannels=*((int*)param);
	return 0;
}

static int android_snd_read_get_nchannels(MSFilter *obj, void *param){
	AndroidSndReadData *ad=(AndroidSndReadData*)obj->data;
	*((int*)param)=ad->nchannels;
	return 0;
}

static int android_snd_read_hack_speaker_state(MSFilter *f, void *arg) {
	AndroidSndReadData *ad  = (AndroidSndReadData *)f->data;
	bool speakerOn = *((bool *)arg);

	if (!ad->started) {
		ms_error("Audio recorder not started, can't hack speaker");
		return -1;
	}

	ms_mutex_lock(&ad->mutex);
	ad->started = false;
	ms_mutex_unlock(&ad->mutex);

	// Stop audio recorder
	ms_message("Hacking speaker state: calling android_snd_read_postprocess()");
	android_snd_read_postprocess(f);

	// Flush eventual sound in the buffer
	// No need to lock as reader_cb is stopped
	flushq(&ad->q, 0);

	AudioSystem::setPhoneState(AUDIO_MODE_IN_CALL);
	if (speakerOn)
		AudioSystem::setForceUse(AUDIO_POLICY_FORCE_FOR_COMMUNICATION, AUDIO_POLICY_FORCE_SPEAKER);
	else
		AudioSystem::setForceUse(AUDIO_POLICY_FORCE_FOR_COMMUNICATION, AUDIO_POLICY_FORCE_NONE);

	// Re-open audio and set d->started=true
	ms_message("Hacking speaker state: calling android_snd_read_preprocess()");
	android_snd_read_preprocess(f);

	return 0;
}

MSFilterMethod android_snd_read_methods[]={
	{MS_FILTER_SET_SAMPLE_RATE, android_snd_read_set_sample_rate},
	{MS_FILTER_GET_SAMPLE_RATE, android_snd_read_get_sample_rate},
	{MS_FILTER_SET_NCHANNELS, android_snd_read_set_nchannels},
	{MS_FILTER_GET_NCHANNELS, android_snd_read_get_nchannels},
	{MS_AUDIO_CAPTURE_FORCE_SPEAKER_STATE, android_snd_read_hack_speaker_state},
	{0,NULL}
};

MSFilterDesc android_snd_read_desc={
	MS_FILTER_PLUGIN_ID,
	"MSAndroidSndRead",
	"android sound source",
	MS_FILTER_OTHER,
	NULL,
	0,
	1,
	android_snd_read_init,
	android_snd_read_preprocess,
	android_snd_read_process,
	android_snd_read_postprocess,
	android_snd_read_uninit,
	android_snd_read_methods
};

static MSFilter * ms_android_snd_read_new(MSFactory *factory){
	MSFilter *f=ms_factory_create_filter_from_desc(factory, &android_snd_read_desc);
	return f;
}


static void android_snd_write_init(MSFilter *obj){
	AndroidSndWriteData *ad=new AndroidSndWriteData();
	obj->data=ad;
}

static void android_snd_write_uninit(MSFilter *obj){
	AndroidSndWriteData *ad=(AndroidSndWriteData*)obj->data;
	delete ad;
}

static int android_snd_write_set_sample_rate(MSFilter *obj, void *data){
#ifndef NATIVE_USE_HARDWARE_RATE
	int *rate=(int*)data;
	AndroidSndWriteData *ad=(AndroidSndWriteData*)obj->data;
	ad->rate=*rate;
	return 0;
#else
	return -1;
#endif
}

static int android_snd_write_get_sample_rate(MSFilter *obj, void *data){
	AndroidSndWriteData *ad=(AndroidSndWriteData*)obj->data;
	*(int*)data=ad->rate;
	return 0;
}

static int android_snd_write_set_nchannels(MSFilter *obj, void *data){
	int *n=(int*)data;
	AndroidSndWriteData *ad=(AndroidSndWriteData*)obj->data;
	ad->nchannels=*n;
	return 0;
}

static int android_snd_write_get_nchannels(MSFilter *obj, void *data){
	int *n=(int*)data;
	AndroidSndWriteData *ad=(AndroidSndWriteData*)obj->data;
	*n=ad->nchannels;
	return 0;
}

/*
 * This is a callback function called by AudioTrack's thread. This thread is not created by ortp/ms2 and is not able to attach to a JVM without crashing
 * at the end, despite it is detached (since android 4.4).
 * We must not output a single log within this callback in the event that the application is using LinphoneCoreFactory.setLogHandler(), in which case
 * the log would be upcalled to java, which will attach the thread to the jvm.
**/
static void android_snd_write_cb(int event, void *user, void * p_info){
	AndroidSndWriteData *ad=(AndroidSndWriteData*)user;
	
	if (event==AudioTrack::EVENT_MORE_DATA){
		AudioTrack::Buffer info;
		int avail;
		int ask;
		
		AudioTrack::readBuffer(p_info,&info);

		ms_mutex_lock(&ad->mutex);
		ask = info.size;
		avail = ms_bufferizer_get_avail(&ad->bf);
		/* Drop the samples accumulated before the first callback asking for data. */
		if ((ad->nbufs == 0) && (avail > (ask * 2))) {
			ms_bufferizer_skip_bytes(&ad->bf, avail - (ask * 2));
		}
		if (avail != 0) {
			if (ad->minBufferFilling == -1) {
				ad->minBufferFilling = avail;
			} else if (avail < ad->minBufferFilling) {
				ad->minBufferFilling = avail;
			}
		}
		info.size = MIN(avail, ask);
		info.frameCount = info.size / 2;
#ifdef TRACE_SND_WRITE_TIMINGS
		{
			MSTimeSpec ts;
			ms_get_cur_time(&ts);
			ms_message("%03u.%03u: AudioTrack ask %d, given %d, available %d [%f ms]", (uint32_t) ts.tv_sec, (uint32_t) (ts.tv_nsec / 1000),
				ask, info.size, avail, (avail / (2 * ad->nchannels)) / (ad->rate / 1000.0));
		}
#endif
		if (info.size > 0){
			ms_bufferizer_read(&ad->bf,(uint8_t*)info.raw,info.size);
		}else{
			/* we have an underrun (no more samples to deliver to the callback). We need to reset minBufferFilling*/
			ad->minBufferFilling=-1;
		}
		ms_mutex_unlock(&ad->mutex);
		ad->nbufs++;
		ad->nFramesRequested+=info.frameCount;
		/*
		if (ad->nbufs %100){
			uint32_t pos;
			if (ad->tr->getPosition(&pos)==0){
				ms_message("Requested frames: %i, playback position: %i, diff=%i",ad->nFramesRequested,pos,ad->nFramesRequested-pos);
			}
		}
		*/
		AudioTrack::writeBuffer(p_info,&info);
	}else if (event==AudioTrack::EVENT_UNDERRUN){
		ms_mutex_lock(&ad->mutex);
#ifdef TRACE_SND_WRITE_TIMINGS
		{
			MSTimeSpec ts;
			ms_get_cur_time(&ts);
			ms_warning("%03u.%03u: PCM playback underrun: available %d", (uint32_t) ts.tv_sec, (uint32_t) (ts.tv_nsec / 1000), ms_bufferizer_get_avail(&ad->bf));
		}
#endif
		ms_mutex_unlock(&ad->mutex);
	}else{
#ifdef TRACE_SND_WRITE_TIMINGS
		ms_error("Untracked event %i",event);
#endif
	}
}

static int channel_mask_for_audio_track(int nchannels) {
	int channel_mask;
	channel_mask = audio_channel_out_mask_from_count(nchannels);
	if (get_sdk_version() < 14) {
		ms_message("Android version older than ICS, apply audio channel hack for AudioTrack");
		if ((channel_mask & AUDIO_CHANNEL_OUT_MONO) == AUDIO_CHANNEL_OUT_MONO) {
			channel_mask = 0x4;
		} else if ((channel_mask & AUDIO_CHANNEL_OUT_STEREO) == AUDIO_CHANNEL_OUT_STEREO) {
			channel_mask = 0x4|0x8;
		}
	}
	return channel_mask;
}

static void android_snd_write_preprocess(MSFilter *obj){
	AndroidSndWriteData *ad=(AndroidSndWriteData*)obj->data;
	int play_buf_size;
	status_t s;
	int notify_frames=(int)(audio_buf_ms*(float)ad->rate);
	
	ad->mCard->enableVoipMode();
	ad->nFramesRequested=0;
	
	if (AudioTrack::getMinFrameCount(&play_buf_size,ad->stype,ad->rate)==0){
		ms_message("AudioTrack: min frame count is %i",play_buf_size);
	}else{
		ms_error("AudioTrack::getMinFrameCount() error");
		return;
	}
	
	ad->tr=new AudioTrack(ad->stype,
                     ad->rate,
                     AUDIO_FORMAT_PCM_16_BIT,
                     channel_mask_for_audio_track(ad->nchannels),
                     play_buf_size,
                     (ad->mCard->mFlags & DEVICE_HAS_CRAPPY_ANDROID_FASTTRACK) ? AUDIO_OUTPUT_FLAG_NONE : AUDIO_OUTPUT_FLAG_FAST,
                     android_snd_write_cb, ad,notify_frames,0, AudioTrack::TRANSFER_CALLBACK);
	s=ad->tr->initCheck();
	if (s!=0) {
		ms_error("Problem setting up AudioTrack: %s",strerror(-s));
		ad->tr=NULL;
		return;
	}
	ad->nbufs=0;
	ms_message("AudioTrack latency estimated to %i ms",ad->tr->latency());
	ad->mStarted=false;
	ad->flowControlStart = obj->ticker->time;
	ad->minBufferFilling = -1;
}

static int bytes_to_ms(AndroidSndWriteData *ad, int bytes){
	return bytes*1000/(2*ad->nchannels*ad->rate);
}

static void android_snd_write_process(MSFilter *obj){
	AndroidSndWriteData *ad=(AndroidSndWriteData*)obj->data;
	
	if (!ad->tr) {
		ms_queue_flush(obj->inputs[0]);
		return;
	}
	if (!ad->mStarted)
		ad->tr->start();
	ms_mutex_lock(&ad->mutex);
#ifdef TRACE_SND_WRITE_TIMINGS
	{
		MSTimeSpec ts;
		int prev_size = ad->bf.size;
		ms_get_cur_time(&ts);
		ms_bufferizer_put_from_queue(&ad->bf,obj->inputs[0]);
		ms_message("%03u.%03u: enqueue buffer %d", (uint32_t) ts.tv_sec, (uint32_t) (ts.tv_nsec / 1000), ad->bf.size - prev_size);
	}
#else
	ms_bufferizer_put_from_queue(&ad->bf,obj->inputs[0]);
#endif
	if (((uint32_t)(obj->ticker->time - ad->flowControlStart)) >= flowControlIntervalMs) {
		int threshold = (flowControlThresholdMs * ad->nchannels * 2 * ad->rate) / 1000;
		//ms_message("Time to flow control: minBufferFilling=%i, threshold=%i",ad->minBufferFilling, threshold);
		if (ad->minBufferFilling > threshold) {
			int drop=ad->minBufferFilling - (threshold/4); //keep a bit in order not to risk an underrun in the next period.
			ms_warning("Too many samples waiting in sound writer (minBufferFilling=%i ms, threshold=%i ms), dropping %i ms", 
					   bytes_to_ms(ad,ad->minBufferFilling), bytes_to_ms(ad,threshold), bytes_to_ms(ad,drop));
			ms_bufferizer_skip_bytes(&ad->bf, drop);
		}
		ad->flowControlStart = obj->ticker->time;
		ad->minBufferFilling = -1;
	}
	ms_mutex_unlock(&ad->mutex);
	if (ad->tr->stopped()) {
		ms_warning("AudioTrack stopped unexpectedly, needs to be restarted");
		ad->tr->start();
	}
}

static void android_snd_write_postprocess(MSFilter *obj){
	AndroidSndWriteData *ad=(AndroidSndWriteData*)obj->data;
	if (!ad->tr) return;
	ms_message("Stopping sound playback");
	ad->tr->stop();
	while(!ad->tr->stopped()){
		usleep(20000);
	}
	ms_message("Sound playback stopped");
	ad->tr->flush();
	ms_message("Sound playback flushed, deleting");
	ad->tr=NULL;
	ad->mCard->disableVoipMode();
	ad->mStarted=false;
}

static MSFilterMethod android_snd_write_methods[]={
	{MS_FILTER_SET_SAMPLE_RATE, android_snd_write_set_sample_rate},
	{MS_FILTER_GET_SAMPLE_RATE, android_snd_write_get_sample_rate},
	{MS_FILTER_SET_NCHANNELS, android_snd_write_set_nchannels},
	{MS_FILTER_GET_NCHANNELS, android_snd_write_get_nchannels},
	{0,NULL}
};

MSFilterDesc android_snd_write_desc={
	MS_FILTER_PLUGIN_ID,
	"MSAndroidSndWrite",
	"android sound output",
	MS_FILTER_OTHER,
	NULL,
	1,
	0,
	android_snd_write_init,
	android_snd_write_preprocess,
	android_snd_write_process,
	android_snd_write_postprocess,
	android_snd_write_uninit,
	android_snd_write_methods
};

static MSFilter * ms_android_snd_write_new(MSFactory* factory){
	MSFilter *f=ms_factory_create_filter_from_desc(factory, &android_snd_write_desc);
	return f;
}


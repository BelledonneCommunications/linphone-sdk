#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/msticker.h>
#include <mediastreamer2/mssndcard.h>

#include "AudioTrack.h"
#include "AudioRecord.h"
#include "String8.h"

#define NATIVE_USE_HARDWARE_RATE 1

using namespace::fake_android;

/*notification duration for audio callbacks, in ms*/
static const float audio_buf_ms=0.01;

static MSSndCard * android_snd_card_new(void);
static MSFilter * ms_android_snd_read_new(void);
static MSFilter * ms_android_snd_write_new(void);
static Library *libmedia;
static Library *libutils;

static int std_sample_rates[]={
	48000,44100,32000,22050,16000,8000,-1
};

struct AndroidNativeSndCardData{
	AndroidNativeSndCardData(): mVoipMode(0) ,mIoHandle(0){
		/* try to use the same sampling rate as the playback.*/
		int hwrate;
		enableVoipMode();
		if (AudioSystem::getOutputSamplingRate(&hwrate,AUDIO_STREAM_VOICE_CALL)==0){
			ms_message("Hardware output sampling rate is %i",hwrate);
		}
		mPlayRate=mRecRate=hwrate;
		for(int i=0;;i++){
			int stdrate=std_sample_rates[i];
			if (stdrate>mRecRate) continue;
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
			String8 params("voip=on");
			if (AudioSystem::setParameters(mIoHandle,params)==0){
				ms_message("voip=on is set.");
			}else ms_warning("Could not set voip=on.");
		}
	}
	void disableVoipMode(){
		mVoipMode--;
		if (mVoipMode==0){
			String8 params("voip=off");
			if (AudioSystem::setParameters(mIoHandle,params)==0){
				ms_message("voip=off is set.");
			}else ms_warning("Could not set voip=off.");
		}
	}
	int mVoipMode;
	int mPlayRate;
	int mRecRate;
	int mRecFrames;
	audio_io_handle_t mIoHandle;
};

struct android_sndReadData{
	android_sndReadData() : rec(0){
		rate=8000;
		nchannels=1;
		qinit(&q);
		ms_mutex_init(&mutex,NULL);
		started=false;
		nbufs=0;
		audio_source=AUDIO_SOURCE_DEFAULT;
	}
	~android_sndReadData(){
		ms_mutex_destroy(&mutex);
		flushq(&q,0);
		delete rec;
	}
	void setCard(AndroidNativeSndCardData *card){
		mCard=card;
#ifdef NATIVE_USE_HARDWARE_RATE
		rate=card->mRecRate;
		rec_buf_size=card->mRecFrames;
#endif
	}
	AndroidNativeSndCardData *mCard;
	audio_source_t audio_source;
	int rate;
	int nchannels;
	ms_mutex_t mutex;
	queue_t q;
	AudioRecord *rec;
	int nbufs;
	int rec_buf_size;
	MSTickerSynchronizer *ticker_synchronizer;
	int64_t read_samples;
	audio_io_handle_t iohandle;
	bool started;
};

struct android_sndWriteData{
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
	AudioTrack *tr;
	int nbufs;
	int nFramesRequested;
};

static MSFilter *android_snd_card_create_reader(MSSndCard *card){
	MSFilter *f=ms_android_snd_read_new();
	(static_cast<android_sndReadData*>(f->data))->setCard((AndroidNativeSndCardData*)card->data);
	return f;
}

static MSFilter *android_snd_card_create_writer(MSSndCard *card){
	MSFilter *f=ms_android_snd_write_new();
	(static_cast<android_sndWriteData*>(f->data))->setCard((AndroidNativeSndCardData*)card->data);
	return f;
}

static void android_snd_card_detect(MSSndCardManager *m){
	libmedia=Library::load("/system/lib/libmedia.so");
	libutils=Library::load("/system/lib/libutils.so");
	if (libmedia && libutils){
		if (AudioRecordImpl::init(libmedia) && AudioTrackImpl::init(libmedia) && AudioSystemImpl::init(libmedia) && String8Impl::init(libutils)){
			ms_message("Native android sound support available.");
			MSSndCard *card=android_snd_card_new();
			ms_snd_card_manager_add_card(m,card);
			return;
		}
	}
	ms_message("Native android sound support is NOT available.");

}

static void android_native_snd_card_uninit(MSSndCard *card){
	delete static_cast<AndroidNativeSndCardData*>(card->data);
}

MSSndCardDesc android_native_snd_card_desc={
	"libmedia",
	android_snd_card_detect,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	android_snd_card_create_reader,
	android_snd_card_create_writer,
	android_native_snd_card_uninit
};



static MSSndCard * android_snd_card_new(void)
{
	MSSndCard * obj;
	obj=ms_snd_card_new(&android_native_snd_card_desc);
	obj->name=ms_strdup("android sound card");
	obj->data=new AndroidNativeSndCardData();
	return obj;
}


static void android_snd_read_init(MSFilter *obj){
	android_sndReadData *ad=new android_sndReadData();
	obj->data=ad;
}

static void compute_timespec(android_sndReadData *d) {
	static int count = 0;
	uint64_t ns = ((1000 * d->read_samples) / (uint64_t) d->rate) * 1000000;
	MSTimeSpec ts;
	ts.tv_nsec = ns % 1000000000;
	ts.tv_sec = ns / 1000000000;
	double av_skew = ms_ticker_synchronizer_set_external_time(d->ticker_synchronizer, &ts);
	if ((++count) % 100 == 0)
		ms_message("sound/wall clock skew is average=%f ms", av_skew);
}

static void android_snd_read_cb(int event, void* user, void *p_info){
	android_sndReadData *ad=(android_sndReadData*)user;
	if (event==AudioRecord::EVENT_MORE_DATA){
		AudioRecord::Buffer * info=reinterpret_cast<AudioRecord::Buffer*>(p_info);
		mblk_t *m=allocb(info->size,0);
		memcpy(m->b_wptr,info->raw,info->size);
		m->b_wptr+=info->size;
		ad->read_samples+=info->frameCount;
		compute_timespec(ad);
		ms_mutex_lock(&ad->mutex);
		putq(&ad->q,m);
		ms_mutex_unlock(&ad->mutex);
		//ms_message("android_snd_read_cb: got %i bytes",info->size);
	}else if (event==AudioRecord::EVENT_OVERRUN){
		ms_warning("AudioRecord overrun");
	}
}

static void android_snd_read_preprocess(MSFilter *obj){
	android_sndReadData *ad=(android_sndReadData*)obj->data;
	status_t  ss;
	int notify_frames=(int)(audio_buf_ms*(float)ad->rate);
	
	ad->mCard->enableVoipMode();
	
	ad->rec_buf_size*=4;
	ad->ticker_synchronizer = ms_ticker_synchronizer_new();
	ad->read_samples=0;
	ad->audio_source=AUDIO_SOURCE_VOICE_COMMUNICATION;
	for(int i=0;i<3;i++){
		ad->rec=new AudioRecord(ad->audio_source,
						ad->rate,
						AUDIO_FORMAT_PCM_16_BIT,
						audio_channel_in_mask_from_count(ad->nchannels),
						ad->rec_buf_size,
						(AudioRecord::record_flags)0 /*flags ??*/
						,android_snd_read_cb,ad,notify_frames,0);
		ss=ad->rec->initCheck();
		if (ss!=0){
			ms_error("Problem when setting up AudioRecord:%s  source=%i,rate=%i,framecount=%i",strerror(-ss),ad->audio_source,ad->rate,ad->rec_buf_size);
			delete ad->rec;
			ad->rec=0;
			if (i==0) {
				ms_error("Retrying with AUDIO_SOURCE_VOICE_CALL");
				ad->audio_source=AUDIO_SOURCE_VOICE_CALL;
			}else if (i==1){
				ms_error("Retrying with AUDIO_SOURCE_MIC");
				ad->audio_source=AUDIO_SOURCE_MIC;
			}
		}else break;
	}
	
}

static void android_snd_read_postprocess(MSFilter *obj){
	android_sndReadData *ad=(android_sndReadData*)obj->data;
	ms_message("Stopping sound capture");
	if (ad->rec!=0) {
		ad->rec->stop();
		delete ad->rec;
		ad->rec=0;
		ad->started=false;
	}
	ms_ticker_synchronizer_destroy(ad->ticker_synchronizer);
	ad->ticker_synchronizer=NULL;
	ms_message("Sound capture stopped");
	ad->mCard->disableVoipMode();
}

static void android_snd_read_uninit(MSFilter *obj){
	android_sndReadData *ad=(android_sndReadData*)obj->data;
	delete ad;
}

static void android_snd_read_process(MSFilter *obj){
	android_sndReadData *ad=(android_sndReadData*)obj->data;
	mblk_t *om;
	if (ad->rec==0) return;
	if (!ad->started) {
		ad->rec->start();
		ad->started=true;
		ms_ticker_set_time_func(obj->ticker,(uint64_t (*)(void*))ms_ticker_synchronizer_get_corrected_time, ad->ticker_synchronizer);
	}

	ms_mutex_lock(&ad->mutex);
	while ((om=getq(&ad->q))!=NULL) {
		//ms_message("android_snd_read_process: Outputing %i bytes",msgdsize(om));
		ms_queue_put(obj->outputs[0],om);
		ad->nbufs++;
	}
	ms_mutex_unlock(&ad->mutex);
}

static int android_snd_read_set_sample_rate(MSFilter *obj, void *param){
#ifndef NATIVE_USE_HARDWARE_RATE
	android_sndReadData *ad=(android_sndReadData*)obj->data;
	ad->rate=*((int*)param);
	return 0;
#else
	return -1;
#endif
}

static int android_snd_read_get_sample_rate(MSFilter *obj, void *param){
	android_sndReadData *ad=(android_sndReadData*)obj->data;
	*(int*)param=ad->rate;
	return 0;
}

static int android_snd_read_set_nchannels(MSFilter *obj, void *param){
	android_sndReadData *ad=(android_sndReadData*)obj->data;
	ad->nchannels=*((int*)param);
	return 0;
}

MSFilterMethod android_snd_read_methods[]={
	{MS_FILTER_SET_SAMPLE_RATE, android_snd_read_set_sample_rate},
	{MS_FILTER_GET_SAMPLE_RATE, android_snd_read_get_sample_rate},
	{MS_FILTER_SET_NCHANNELS, android_snd_read_set_nchannels},
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

static MSFilter * ms_android_snd_read_new(){
	MSFilter *f=ms_filter_new_from_desc(&android_snd_read_desc);
	return f;
}


static void android_snd_write_init(MSFilter *obj){
	android_sndWriteData *ad=new android_sndWriteData;
	ad->stype=AUDIO_STREAM_VOICE_CALL;
	ad->rate=8000;
	ad->nchannels=1;
	ad->nFramesRequested=0;
	ms_mutex_init(&ad->mutex,NULL);
	ms_bufferizer_init(&ad->bf);
	obj->data=ad;
}

static void android_snd_write_uninit(MSFilter *obj){
	android_sndWriteData *ad=(android_sndWriteData*)obj->data;
	ms_mutex_destroy(&ad->mutex);
	ms_bufferizer_uninit(&ad->bf);
	delete ad;
}

static int android_snd_write_set_sample_rate(MSFilter *obj, void *data){
#ifndef NATIVE_USE_HARDWARE_RATE
	int *rate=(int*)data;
	android_sndWriteData *ad=(android_sndWriteData*)obj->data;
	ad->rate=*rate;
	return 0;
#else
	return -1;
#endif
}

static int android_snd_write_get_sample_rate(MSFilter *obj, void *data){
	android_sndWriteData *ad=(android_sndWriteData*)obj->data;
	*(int*)data=ad->rate;
	return 0;
}

static int android_snd_write_set_nchannels(MSFilter *obj, void *data){
	int *n=(int*)data;
	android_sndWriteData *ad=(android_sndWriteData*)obj->data;
	ad->nchannels=*n;
	return 0;
}

static void android_snd_write_cb(int event, void *user, void * p_info){
	android_sndWriteData *ad=(android_sndWriteData*)user;
	
	if (event==AudioTrack::EVENT_MORE_DATA){
		int avail;
		AudioTrack::Buffer *info=reinterpret_cast<AudioTrack::Buffer *>(p_info);
		//ms_message("android_snd_write_cb: need to provide %i bytes",info->size);
		
		if (ad->nbufs==0){
			
			/*the audio subsystem takes time to start: purge the accumulated buffers while
			it was not ready*/
			ms_mutex_lock(&ad->mutex);
			while((avail=ms_bufferizer_get_avail(&ad->bf))>0){
				ms_bufferizer_skip_bytes(&ad->bf,avail);
			}
			ms_mutex_unlock(&ad->mutex);
		}
		ms_mutex_lock(&ad->mutex);
		if ((avail=ms_bufferizer_get_avail(&ad->bf))>=(int) info->size){
			ms_bufferizer_read(&ad->bf,(uint8_t*)info->raw,info->size);
			avail-=info->size;
			if (avail>((100*ad->nchannels*2*ad->rate)/1000)){
				ms_warning("Too many samples waiting in sound writer, dropping %i bytes",avail);
				ms_bufferizer_skip_bytes(&ad->bf,avail);
			}
			ms_mutex_unlock(&ad->mutex);
			//ms_message("%i bytes sent to the device",info->size);
		}else{
			ms_mutex_unlock(&ad->mutex);
			ms_message("Filling callback with silence %i bytes",info->size);
			memset(info->raw,0,info->size);
		}
		ad->nbufs++;
		ad->nFramesRequested+=info->frameCount;
		/*
		if (ad->nbufs %100){
			uint32_t pos;
			if (ad->tr->getPosition(&pos)==0){
				ms_message("Requested frames: %i, playback position: %i, diff=%i",ad->nFramesRequested,pos,ad->nFramesRequested-pos);
			}
		}
		*/
	}else if (event==AudioTrack::EVENT_UNDERRUN){
		ms_warning("PCM playback underrun");
	}else ms_error("Untracked event %i",event);
}

static void android_snd_write_preprocess(MSFilter *obj){
	android_sndWriteData *ad=(android_sndWriteData*)obj->data;
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
                     audio_channel_out_mask_from_count(ad->nchannels),
                     play_buf_size,
                     AUDIO_OUTPUT_FLAG_NONE, // AUDIO_OUTPUT_FLAG_NONE,
                     android_snd_write_cb, ad,notify_frames,0);
	s=ad->tr->initCheck();
	if (s!=0) {
		ms_error("Problem setting up AudioTrack: %s",strerror(-s));
		delete ad->tr;
		ad->tr=NULL;
		return;
	}
	ad->nbufs=0;
	ms_message("AudioTrack latency estimated to %i ms",ad->tr->latency());
	ad->tr->start();
}

static void android_snd_write_process(MSFilter *obj){
	android_sndWriteData *ad=(android_sndWriteData*)obj->data;
	
	if (!ad->tr) {
		ms_queue_flush(obj->inputs[0]);
		return;
	}
	ms_mutex_lock(&ad->mutex);
	ms_bufferizer_put_from_queue(&ad->bf,obj->inputs[0]);
	ms_mutex_unlock(&ad->mutex);
	if (ad->tr->stopped()) {
		ms_warning("AudioTrack stopped unexpectedly, needs to be restarted");
		ad->tr->start();
	}
}

static void android_snd_write_postprocess(MSFilter *obj){
	android_sndWriteData *ad=(android_sndWriteData*)obj->data;
	if (!ad->tr) return;
	ms_message("Stopping sound playback");
	ad->tr->stop();
	ad->tr->flush();
	ms_message("Sound playback stopped");
	delete ad->tr;
	ad->tr=NULL;
	ad->mCard->disableVoipMode();
}

static MSFilterMethod android_snd_write_methods[]={
	{MS_FILTER_SET_SAMPLE_RATE, android_snd_write_set_sample_rate},
	{MS_FILTER_GET_SAMPLE_RATE, android_snd_write_get_sample_rate},
	{MS_FILTER_SET_NCHANNELS, android_snd_write_set_nchannels},
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

static MSFilter * ms_android_snd_write_new(void){
	MSFilter *f=ms_filter_new_from_desc(&android_snd_write_desc);
	return f;
}


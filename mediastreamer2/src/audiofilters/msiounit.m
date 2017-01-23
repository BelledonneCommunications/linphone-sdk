/*
 * auiosnd.c -I/O unit Media plugin for Linphone-
 *
 *
 * Copyright (C) 2009  Belledonne Comunications, Grenoble, France
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
#import <AVFoundation/AVAudioSession.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSString.h>
#include <AudioToolbox/AudioToolbox.h>
#include "mediastreamer2/mssndcard.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"

/*                          -------------------------
							| i                   o |
-- BUS 1 -- from mic -->	| n    REMOTE I/O     u | -- BUS 1 -- to app -->
							| p      AUDIO        t |
-- BUS 0 -- from app -->	| u       UNIT        p | -- BUS 0 -- to speaker -->
							| t                   u |
							|                     t |
							-------------------------
 */

static AudioUnitElement inputBus = 1;
static AudioUnitElement outputBus = 0;


static const char * audio_unit_format_error (OSStatus error) {
	switch (error) {
		case kAudioUnitErr_InvalidProperty: return "kAudioUnitErr_InvalidProperty";
		case kAudioUnitErr_InvalidParameter: return "kAudioUnitErr_InvalidParameter";
		case kAudioUnitErr_InvalidElement: return "kAudioUnitErr_InvalidElement";
		case kAudioUnitErr_NoConnection: return "kAudioUnitErr_NoConnection";
		case kAudioUnitErr_FailedInitialization: return "kAudioUnitErr_FailedInitialization";
		case kAudioUnitErr_TooManyFramesToProcess: return "kAudioUnitErr_TooManyFramesToProcess";
		case kAudioUnitErr_InvalidFile: return "kAudioUnitErr_InvalidFile";
		case kAudioUnitErr_UnknownFileType: return "kAudioUnitErr_UnknownFileType";
		case kAudioUnitErr_FileNotSpecified: return "kAudioUnitErr_FileNotSpecified";
		case kAudioUnitErr_FormatNotSupported: return "kAudioUnitErr_FormatNotSupported";
		case kAudioUnitErr_Uninitialized: return "kAudioUnitErr_Uninitialized";
		case kAudioUnitErr_InvalidScope: return "kAudioUnitErr_InvalidScope";
		case kAudioUnitErr_PropertyNotWritable: return "kAudioUnitErr_PropertyNotWritable";
		case kAudioUnitErr_CannotDoInCurrentContext: return "kAudioUnitErr_CannotDoInCurrentContext";
		case kAudioUnitErr_InvalidPropertyValue: return "kAudioUnitErr_InvalidPropertyValue";
		case kAudioUnitErr_PropertyNotInUse: return "kAudioUnitErr_PropertyNotInUse";
		case kAudioUnitErr_Initialized: return "kAudioUnitErr_Initialized";
		case kAudioUnitErr_InvalidOfflineRender: return "kAudioUnitErr_InvalidOfflineRender";
		case kAudioUnitErr_Unauthorized: return "kAudioUnitErr_Unauthorized";
		default: return "unknown error";
	}

}


static const char *audio_session_format_error(OSStatus error)
{
	switch (error) {
		case kAudioSessionNotInitialized:
			return "kAudioSessionNotInitialized";
		case kAudioSessionAlreadyInitialized:
			return "kAudioSessionAlreadyInitialized";
		case kAudioSessionInitializationError:
			return "kAudioSessionInitializationError";
		case kAudioSessionUnsupportedPropertyError:
			return "kAudioSessionUnsupportedPropertyError";
		case kAudioSessionBadPropertySizeError:
			return "kAudioSessionBadPropertySizeError";
		case kAudioSessionNotActiveError:
			return "kAudioSessionNotActiveError";
		case kAudioServicesNoHardwareError:
			return "kAudioServicesNoHardwareError";
		case kAudioSessionNoCategorySet:
			return "kAudioSessionNoCategorySet";
		case kAudioSessionIncompatibleCategory:
			return "kAudioSessionIncompatibleCategory";
		case kAudioSessionUnspecifiedError:
			return "kAudioSessionUnspecifiedError";
		default: return "unkown error";
	}

 }

#define check_au_session_result(au,method) \
if (au!=AVAudioSessionErrorInsufficientPriority && au!=0) ms_error("AudioSession error for %s: ret=%s (%li) (%s:%d)",method, audio_session_format_error(au), (long)au, __FILE__, __LINE__ )

#define check_au_unit_result(au,method) \
if (au!=0) ms_error("AudioUnit error for %s: ret=%s (%li) (%s:%d)",method, audio_unit_format_error(au), (long)au, __FILE__, __LINE__ )


#define check_session_call(call)   do { OSStatus res = (call); check_au_session_result(res, #call); } while(0)
#define check_audiounit_call(call) do { OSStatus res = (call); check_au_unit_result(res, #call); } while(0)


#if 0
#undef ms_debug
#define ms_debug ms_message
#endif
static const char* AU_CARD_RECEIVER = "Audio Unit Receiver";
static const char* AU_CARD_NOVOICEPROC = "Audio Unit NoVoiceProc";
static const char* AU_CARD_FAST_IOUNIT = "Audio Unit Fast Receiver"; /*Same as AU_CARD_RECEIVER but whiout audio session handling which are delagated to the application*/
static const char* AU_CARD_SPEAKER = "Audio Unit Speaker";
static const char* AU_CARD_TESTER = "Audio Unit Tester";


static MSFilter *ms_au_read_new(MSSndCard *card);
static MSFilter *ms_au_write_new(MSSndCard *card);

typedef  struct au_filter_read_data au_filter_read_data_t;
typedef  struct au_filter_write_data au_filter_write_data_t;


typedef  struct  au_card {
	AudioUnit	io_unit;
	ms_mutex_t	mutex;
	unsigned int	rate;
	unsigned int	bits;
	unsigned int	nchannels;
	uint64_t last_failed_iounit_start_time;
	au_filter_read_data_t* read_data;
	au_filter_write_data_t* write_data;
	MSSndCard* ms_snd_card;
	CFRunLoopTimerRef shutdown_timer;
	bool_t is_ringer;
	bool_t is_fast;
	bool_t is_tester;
	bool_t io_unit_started;
	bool_t audio_session_configured;
	bool_t read_started;
	bool_t write_started;
	bool_t use_shutdowntimer;
}au_card_t;

typedef  struct au_filter_base {
	au_card_t* card;
	int muted;
}au_filter_base_t;

struct au_filter_read_data{
	au_filter_base_t base;
	ms_mutex_t	mutex;
	queue_t		rq;
	AudioTimeStamp readTimeStamp;
	unsigned int n_lost_frame;

} ;

struct au_filter_write_data{
	au_filter_base_t base;
	ms_mutex_t	mutex;
	MSBufferizer	*bufferizer;
	unsigned int n_lost_frame;
	bool first_frame_wrote;

};

static void  stop_audio_unit (au_card_t* d);
static void cancel_audio_unit_timer(au_card_t* card);


/*
 mediastreamer2 function
 */

static void au_set_level(MSSndCard *card, MSSndCardMixerElem e, int percent)
{
}

static int au_get_level(MSSndCard *card, MSSndCardMixerElem e)
{
	return 0;
}

static void au_set_source(MSSndCard *card, MSSndCardCapture source)
{
}

static OSStatus au_render_cb (
							  void                        *inRefCon,
							  AudioUnitRenderActionFlags  *ioActionFlags,
							  const AudioTimeStamp        *inTimeStamp,
							  UInt32                      inBusNumber,
							  UInt32                      inNumberFrames,
							  AudioBufferList             *ioData
							  );

static void create_io_unit (AudioUnit* au, MSSndCard* sndcard) {
	AudioComponentDescription au_description;
	AudioComponent foundComponent;

	bool_t noVoiceProc = (strcasecmp(sndcard->name, AU_CARD_NOVOICEPROC) == 0);
	OSType subtype = noVoiceProc ? kAudioUnitSubType_RemoteIO : kAudioUnitSubType_VoiceProcessingIO;

	au_description.componentType          = kAudioUnitType_Output;
	au_description.componentSubType       = subtype;
	au_description.componentManufacturer  = kAudioUnitManufacturer_Apple;
	au_description.componentFlags         = 0;
	au_description.componentFlagsMask     = 0;

	foundComponent = AudioComponentFindNext (NULL,&au_description);

	check_audiounit_call( AudioComponentInstanceNew(foundComponent, au) );

	ms_message("AudioUnit created with type %s.", subtype==kAudioUnitSubType_RemoteIO ? "kAudioUnitSubType_RemoteIO" : "kAudioUnitSubType_VoiceProcessingIO" );
}


/* the interruption listener is not reliable, it can be overriden by other parts of the application */
/* as a result, we do nothing with it*/
static void au_interruption_listener (void     *inClientData, UInt32   inInterruptionState){
/*
	if (inInterruptionState==kAudioSessionBeginInterruption){
		CFRunLoopPerformBlock(CFRunLoopGetMain(), kCFRunLoopCommonModes, ^(void) {
			stop_audio_unit((au_card_t*)inClientData);
		});
	}
*/
}

static void au_init(MSSndCard *card){
	ms_debug("au_init");
	au_card_t *d=ms_new0(au_card_t,1);

	if (strcmp(card->name,AU_CARD_SPEAKER)==0) {
		d->is_ringer=TRUE;
	} else if (strcmp(card->name,AU_CARD_FAST_IOUNIT)==0) {
		d->is_fast=TRUE;
	} else if( strcmp(card->name,AU_CARD_TESTER)==0){
		d->is_tester=TRUE;
	}
	d->bits=16;
	d->rate=0; /*not set*/
	d->nchannels=1;
	d->ms_snd_card=card;
	d->use_shutdowntimer=FALSE;
	card->preferred_sample_rate=44100;
	card->capabilities|=MS_SND_CARD_CAP_BUILTIN_ECHO_CANCELLER|MS_SND_CARD_CAP_IS_SLOW;
	ms_mutex_init(&d->mutex,NULL);
	AudioSessionInitialize(NULL, NULL, au_interruption_listener, d);
	card->data=d;
}

static void au_uninit(MSSndCard *card){
	au_card_t *d=(au_card_t*)card->data;
	cancel_audio_unit_timer(d);
	stop_audio_unit(d);
	ms_mutex_destroy(&d->mutex);
	ms_free(d);
	card->data = NULL;
}

static void au_usage_hint(MSSndCard *card, bool_t used){
	au_card_t *d=(au_card_t*)card->data;
	if (!used &&d){
		if(d->io_unit) {
			if (d->shutdown_timer) {
				cancel_audio_unit_timer(d);
				stop_audio_unit(d);
			} else {
				d->use_shutdowntimer = FALSE;
			}
		} else {
			d->use_shutdowntimer = FALSE;
		}
	} else {
		if(d) {
			d->use_shutdowntimer = TRUE;
		}
	}
}



static void au_detect(MSSndCardManager *m);
static MSSndCard *au_duplicate(MSSndCard *obj);

MSSndCardDesc au_card_desc={
.driver_type="AU",
.detect=au_detect,
.init=au_init,
.set_level=au_set_level,
.get_level=au_get_level,
.set_capture=au_set_source,
.set_control=NULL,
.get_control=NULL,
.create_reader=ms_au_read_new,
.create_writer=ms_au_write_new,
.uninit=au_uninit,
.duplicate=au_duplicate,
.usage_hint=au_usage_hint
};

static MSSndCard *au_duplicate(MSSndCard *obj){
	MSSndCard *card=ms_snd_card_new_with_name(&au_card_desc,obj->name);
	return card;
}

static MSSndCard *au_card_new(const char* name){
	MSSndCard *card=ms_snd_card_new_with_name(&au_card_desc,name);
	return card;
}

static void au_detect(MSSndCardManager *m){
	ms_debug("au_detect");
	MSSndCard *card=au_card_new(AU_CARD_RECEIVER);
	ms_snd_card_manager_add_card(m,card);
	card=au_card_new(AU_CARD_FAST_IOUNIT);
	ms_snd_card_manager_add_card(m,card);
	card = au_card_new(AU_CARD_NOVOICEPROC);
	ms_snd_card_manager_add_card(m,card);
	card = au_card_new(AU_CARD_TESTER);
	ms_snd_card_manager_add_card(m,card);
}

static OSStatus au_read_cb (
							  void                        *inRefCon,
							  AudioUnitRenderActionFlags  *ioActionFlags,
							  const AudioTimeStamp        *inTimeStamp,
							  UInt32                      inBusNumber,
							  UInt32                      inNumberFrames,
							  AudioBufferList             *ioData
)
{

	au_card_t* card = (au_card_t*)inRefCon;
	ms_mutex_lock(&card->mutex);
	if (!card->read_data) {
		//just return from now;
		ms_mutex_unlock(&card->mutex);
		return 0;
	}
	au_filter_read_data_t *d=card->read_data;
	if (d->readTimeStamp.mSampleTime <0) {
		d->readTimeStamp=*inTimeStamp;
	}
	OSStatus err=0;
	mblk_t * rm=NULL;
	AudioBufferList readAudioBufferList;
	readAudioBufferList.mBuffers[0].mDataByteSize=inNumberFrames*d->base.card->bits/8;
	readAudioBufferList.mNumberBuffers=1;
	readAudioBufferList.mBuffers[0].mNumberChannels=d->base.card->nchannels;

	if (d->base.card->read_started) {
		rm=allocb(readAudioBufferList.mBuffers[0].mDataByteSize,0);
		readAudioBufferList.mBuffers[0].mData=rm->b_wptr;
		err = AudioUnitRender(d->base.card->io_unit, ioActionFlags, &d->readTimeStamp, inBusNumber,inNumberFrames, &readAudioBufferList);
		if (err == 0) {
			rm->b_wptr += readAudioBufferList.mBuffers[0].mDataByteSize;
			ms_mutex_lock(&d->mutex);
			putq(&d->rq,rm);
			ms_mutex_unlock(&d->mutex);
			d->readTimeStamp.mSampleTime+=readAudioBufferList.mBuffers[0].mDataByteSize/(d->base.card->bits/2);
		} else {
			check_au_unit_result(err, "AudioUnitRender");
			freeb(rm);
		}
	}
	ms_mutex_unlock(&card->mutex);
	return err;
}

/********************write cb only used for write operation******************/

static OSStatus au_write_cb (
							  void                        *inRefCon,
							  AudioUnitRenderActionFlags  *ioActionFlags,
							  const AudioTimeStamp        *inTimeStamp,
							  UInt32                      inBusNumber,
							  UInt32                      inNumberFrames,
							  AudioBufferList             *ioData
							 ) {
	ms_debug("render cb");
	au_card_t* card = (au_card_t*)inRefCon;
	ms_mutex_lock(&card->mutex);
	if( !card->write_data ){
		ms_mutex_unlock(&card->mutex);
		return -1;
	}
	ioData->mBuffers[0].mDataByteSize=inNumberFrames*card->bits/8;
	ioData->mNumberBuffers=1;

	au_filter_write_data_t *d=card->write_data;

	if (d!=NULL){
		ms_mutex_lock(&d->mutex);
		if (!d->first_frame_wrote) {
			ms_bufferizer_flush(d->bufferizer); /*to avoid keeping delay from first start which can be around 100ms becasue io unit takes time to start*/
			d->first_frame_wrote=true;
		}
		if(ms_bufferizer_get_avail(d->bufferizer) >= inNumberFrames*d->base.card->bits/8) {
			ms_bufferizer_read(d->bufferizer, ioData->mBuffers[0].mData, inNumberFrames*d->base.card->bits/8);
			/*basic algo,  can be enhanced with a more advanced bufferizer computing average value*/
			if (ms_bufferizer_get_avail(d->bufferizer) > card->rate* (card->nchannels * card->bits / 8)/5 ) {
				ms_warning("we are at least 200ms late, bufferizer sise is %li bytes in framezize is %li bytes"
						,(long)ms_bufferizer_get_avail(d->bufferizer)
						,(long)inNumberFrames*d->base.card->bits/8);
				ms_bufferizer_flush(d->bufferizer);
			}
			ms_mutex_unlock(&d->mutex);
			ms_mutex_unlock(&card->mutex);
			return 0;//break
		} else {
			d->n_lost_frame+=inNumberFrames;
			ms_mutex_unlock(&d->mutex);
		}
	}
	//writing silence;
	memset(ioData->mBuffers[0].mData, 0,ioData->mBuffers[0].mDataByteSize);
	ms_debug("nothing to write, pushing silences,  framezize is %u bytes mDataByteSize %u"
			 ,inNumberFrames*card->bits/8
			 ,(unsigned int)ioData->mBuffers[0].mDataByteSize);
	ms_mutex_unlock(&card->mutex);
	return 0;
}

/****************config**************/
static void configure_audio_session (au_card_t* d,uint64_t time) {
	UInt32 audioCategory;
	UInt32 audioMode;
	UInt32 audioCategorySize=sizeof(audioCategory);
	bool_t changed;

	if (!d->is_fast){

		if (d->audio_session_configured){
			/*check that category wasn't changed*/
			check_session_call(AudioSessionGetProperty(kAudioSessionProperty_AudioCategory,&audioCategorySize,&audioCategory));

			changed=(audioCategory!=kAudioSessionCategory_AmbientSound && d->is_ringer)
			||(audioCategory!=kAudioSessionCategory_PlayAndRecord && !d->is_ringer);
		}

		if (!d->audio_session_configured || changed) {
			check_session_call( AudioSessionSetActive(true) );

			if (d->is_ringer && kCFCoreFoundationVersionNumber > kCFCoreFoundationVersionNumber10_6 /*I.E is >=OS4*/) {
				audioCategory= kAudioSessionCategory_AmbientSound;
				audioMode = kAudioSessionMode_Default;
				ms_message("Configuring audio session for playback");
			} else {
				audioCategory = kAudioSessionCategory_PlayAndRecord;
				audioMode = kAudioSessionMode_VoiceChat;
				ms_message("Configuring audio session for playback/record");
			}

			check_audiounit_call(AudioSessionSetProperty(kAudioSessionProperty_AudioCategory, sizeof(audioCategory), &audioCategory));
			check_audiounit_call(AudioSessionSetProperty(kAudioSessionProperty_Mode, sizeof(audioMode), &audioMode));

		}else{
			ms_message("Audio session already correctly configured.");
		}
		d->audio_session_configured=TRUE;
	} else {
		ms_message("Fast iounit mode, audio session configuration must be done at application level.");
	}

}

static bool_t  start_audio_unit (au_filter_base_t* d,uint64_t time) {
	au_card_t* card=d->card;
	if (card->io_unit == NULL) {
		create_io_unit(&card->io_unit, card->ms_snd_card);
		if (card->io_unit == NULL) ms_fatal("io_unit is NULL");
	}
	if (!card->io_unit_started && (card->last_failed_iounit_start_time == 0 || (time - card->last_failed_iounit_start_time)>100)) {

		check_audiounit_call(AudioUnitInitialize(card->io_unit));
		ms_message("io unit initialized");

		Float64 delay;
		UInt32 delaySize = sizeof(delay);
		check_audiounit_call(AudioUnitGetProperty(card->io_unit
									  ,kAudioUnitProperty_Latency
									  , kAudioUnitScope_Global
									  , 0
									  , &delay
									  , &delaySize));

		UInt32 quality;
		UInt32 qualitySize = sizeof(quality);
		check_audiounit_call(AudioUnitGetProperty(card->io_unit
									  ,kAudioUnitProperty_RenderQuality
									  , kAudioUnitScope_Global
									  , 0
									  , &quality
									  , &qualitySize));
		ms_message("I/O unit latency [%f], quality [%u]",delay,(unsigned)quality);
		Float32 hwoutputlatency;
		UInt32 hwoutputlatencySize=sizeof(hwoutputlatency);

		check_session_call(AudioSessionGetProperty(kAudioSessionProperty_CurrentHardwareOutputLatency
										 ,&hwoutputlatencySize
										 , &hwoutputlatency));
		Float32 hwinputlatency;
		UInt32 hwinputlatencySize=sizeof(hwoutputlatency);
		check_session_call(AudioSessionGetProperty(kAudioSessionProperty_CurrentHardwareInputLatency
										 ,&hwinputlatencySize
										 , &hwinputlatency));

		Float32 hwiobuf;
		UInt32 hwiobufSize=sizeof(hwiobuf);
		check_session_call( AudioSessionGetProperty(kAudioSessionProperty_CurrentHardwareIOBufferDuration
										 ,&hwiobufSize
										 , &hwiobuf));

		Float64 hwsamplerate;
		UInt32 hwsamplerateSize=sizeof(hwsamplerate);
		check_session_call( AudioSessionGetProperty(kAudioSessionProperty_CurrentHardwareSampleRate
										 ,&hwsamplerateSize
										 ,&hwsamplerate));

		OSStatus auresult;
		check_audiounit_call( (auresult = AudioOutputUnitStart(card->io_unit)) );
		card->io_unit_started = (auresult ==0);
		if (!card->io_unit_started) {
			ms_message("AudioUnit could not be started, current hw output latency [%f] input [%f] iobuf[%f] hw sample rate [%f]",hwoutputlatency,hwinputlatency,hwiobuf,hwsamplerate);
			d->card->last_failed_iounit_start_time=time;
		} else {
			ms_message("AudioUnit started, current hw output latency [%f] input [%f] iobuf[%f] hw sample rate [%f]",hwoutputlatency,hwinputlatency,hwiobuf,hwsamplerate);
			d->card->last_failed_iounit_start_time=0;
		}
	}
	return card->io_unit_started;

}

static void destroy_audio_unit (au_card_t* d) {
	if (d->io_unit) {
		AudioComponentInstanceDispose (d->io_unit);
		d->io_unit=NULL;
		if (!d->is_fast) {
			AudioSessionSetActiveWithFlags(false, kAudioSessionSetActiveFlag_NotifyOthersOnDeactivation);
		}
		ms_message("AudioUnit destroyed");
	}
}
static void stop_audio_unit (au_card_t* d) {
	if (d->io_unit && d->io_unit_started) {
		check_audiounit_call( AudioOutputUnitStop(d->io_unit) );
		ms_message("AudioUnit stopped");
		d->io_unit_started=FALSE;
		d->audio_session_configured=FALSE;

	}
	if (d->io_unit) {
		check_audiounit_call( AudioUnitUninitialize(d->io_unit) );
		destroy_audio_unit(d);
	}
	d->rate=0; /*uninit*/
}

static void cancel_audio_unit_timer(au_card_t* card){
	if (card->shutdown_timer){
		CFRunLoopRemoveTimer(CFRunLoopGetMain(), card->shutdown_timer,kCFRunLoopCommonModes);
		CFRunLoopTimerInvalidate(card->shutdown_timer);
		card->shutdown_timer=NULL;
	}
}

/***********************************read function********************/

static void au_read_preprocess(MSFilter *f){
	ms_debug("au_read_preprocess");
	au_filter_read_data_t *d= (au_filter_read_data_t*)f->data;
	au_card_t* card=d->base.card;

	cancel_audio_unit_timer(card);
	configure_audio_session(card, f->ticker->time);

	if (!card->io_unit) create_io_unit(&card->io_unit, card->ms_snd_card);

	//Always configure readcb
	AURenderCallbackStruct renderCallbackStruct;
	renderCallbackStruct.inputProc       = au_read_cb;
	renderCallbackStruct.inputProcRefCon = card;
	check_audiounit_call(AudioUnitSetProperty (
								   card->io_unit,
								   kAudioOutputUnitProperty_SetInputCallback,
								   kAudioUnitScope_Input,
								   outputBus,
								   &renderCallbackStruct,
								   sizeof (renderCallbackStruct)
								   ));

	if (card->io_unit_started) {
		ms_message("Audio Unit already started");
		return;
	}

	/*format are always set in the write preprocess*/
	Float32 preferredBufferSize;
	switch (card->rate) {
		case 11025:
		case 22050:
			preferredBufferSize= .020;
			break;
		default:
			preferredBufferSize= .015;
	}

	check_session_call( AudioSessionSetProperty(kAudioSessionProperty_PreferredHardwareIOBufferDuration
									 ,sizeof(preferredBufferSize)
									 , &preferredBufferSize) );
}

static void au_read_postprocess(MSFilter *f){
	au_filter_read_data_t *d= (au_filter_read_data_t*)f->data;
	ms_mutex_lock(&d->mutex);
	flushq(&d->rq,0);
	ms_mutex_unlock(&d->mutex);
}

static void au_read_process(MSFilter *f){
	au_filter_read_data_t *d=(au_filter_read_data_t*)f->data;
	mblk_t *m;

	if (!(d->base.card->read_started=d->base.card->io_unit_started)) {
		//make sure audio unit is started
		start_audio_unit((au_filter_base_t*)d,f->ticker->time);
	}
	do {
		ms_mutex_lock(&d->mutex);
		m=getq(&d->rq);
		ms_mutex_unlock(&d->mutex);
		if (m != NULL) ms_queue_put(f->outputs[0],m);
	}while(m!=NULL);
}



/***********************************write function********************/

static void au_write_preprocess(MSFilter *f){
	ms_debug("au_write_preprocess");
	OSStatus auresult;
	au_filter_write_data_t *d= (au_filter_write_data_t*)f->data;
	au_card_t* card=d->base.card;

	cancel_audio_unit_timer(card);

	if (card->io_unit_started) {
		ms_message("AudioUnit already started");
		return;
	}
	configure_audio_session(card, f->ticker->time);


	if (!card->io_unit) create_io_unit(&card->io_unit, card->ms_snd_card);



	AudioStreamBasicDescription audioFormat;
	/*card sampling rate is fixed at that time*/
	audioFormat.mSampleRate			= card->rate;
	audioFormat.mFormatID			= kAudioFormatLinearPCM;
	audioFormat.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
	audioFormat.mFramesPerPacket	= 1;
	audioFormat.mChannelsPerFrame	= card->nchannels;
	audioFormat.mBitsPerChannel		= card->bits;
	audioFormat.mBytesPerPacket		= card->bits / 8;
	audioFormat.mBytesPerFrame		= card->nchannels * card->bits / 8;

	UInt32 doNotSetProperty    = 0;
	UInt32 doSetProperty    = 1;
	Float64 hwsamplerate;
	UInt32 hwsamplerateSize=sizeof(hwsamplerate);


	//enable speaker output
	auresult =AudioUnitSetProperty (
									card->io_unit,
									kAudioOutputUnitProperty_EnableIO,
									kAudioUnitScope_Output ,
									outputBus,
									&doSetProperty,
									sizeof (doSetProperty)
									);
	check_au_unit_result(auresult,"kAudioOutputUnitProperty_EnableIO,kAudioUnitScope_Output");

	/*enable mic for scheduling render call back, why ?*/
	auresult=AudioUnitSetProperty (/*enable mic input*/
								   card->io_unit,
								   kAudioOutputUnitProperty_EnableIO,
								   kAudioUnitScope_Input ,
								   inputBus,
								   &doSetProperty,
								   sizeof (doSetProperty)
								   );

	check_au_unit_result(auresult,"kAudioOutputUnitProperty_EnableIO,kAudioUnitScope_Input");
	auresult=AudioUnitSetProperty (
								   card->io_unit,
								   kAudioUnitProperty_StreamFormat,
								   kAudioUnitScope_Output,
								   inputBus,
								   &audioFormat,
								   sizeof (audioFormat)
								   );
	check_au_unit_result(auresult,"kAudioUnitProperty_StreamFormat,kAudioUnitScope_Output");
	/*end of: enable mic for scheduling render call back, why ?*/

	//setup stream format
	auresult=AudioUnitSetProperty (
								   card->io_unit,
								   kAudioUnitProperty_StreamFormat,
								   kAudioUnitScope_Input,
								   outputBus,
								   &audioFormat,
								   sizeof (audioFormat)
								   );
	check_au_unit_result(auresult,"kAudioUnitProperty_StreamFormat,kAudioUnitScope_Input");

	//disable unit buffer allocation
	auresult=AudioUnitSetProperty (
								   card->io_unit,
								   kAudioUnitProperty_ShouldAllocateBuffer,
								   kAudioUnitScope_Output,
								   outputBus,
								   &doNotSetProperty,
								   sizeof (doNotSetProperty)
								   );
	check_au_unit_result(auresult,"kAudioUnitProperty_ShouldAllocateBuffer,kAudioUnitScope_Output");
	AURenderCallbackStruct renderCallbackStruct;
	renderCallbackStruct.inputProc       = au_write_cb;
	renderCallbackStruct.inputProcRefCon = card;

	auresult=AudioUnitSetProperty (
								   card->io_unit,
								   kAudioUnitProperty_SetRenderCallback,
								   kAudioUnitScope_Input,
								   outputBus,
								   &renderCallbackStruct,
								   sizeof (renderCallbackStruct)
								   );
	check_au_unit_result(auresult,"kAudioUnitProperty_SetRenderCallback,kAudioUnitScope_Input");

	auresult=AudioSessionGetProperty(kAudioSessionProperty_CurrentHardwareSampleRate
									 ,&hwsamplerateSize
									 ,&hwsamplerate);
	check_au_session_result(auresult,"get kAudioSessionProperty_PreferredHardwareSampleRate");


	/*
	 * Bluetooth bug: on iphone5s at least, some low end BT headset create a bug in iOS when requesting 48khz hardware sampling rate.
	 * These headset don't support this frequency, which result in the hardware sampling rate finally set to 8khz by iOS.
	 * The bug traduces in no audio chunks that can be pushed or pulled from the AudioUnit.
	 *
	 * Apparently the driver doesn't recover from this situation.
	 * The workaround is then to request 44100 Hz instead of 48khz.
	 */
	if(card->rate == 8000 && (floor(NSFoundationVersionNumber) >= NSFoundationVersionNumber_iOS_9_x_Max)) {
		hwsamplerate=48000;
		auresult=AudioSessionSetProperty(kAudioSessionProperty_PreferredHardwareSampleRate
										 , sizeof(hwsamplerate)
										 , &hwsamplerate);
		return;
	}

	if( hwsamplerate != card->rate) {
		if(card->rate <= 44100 ){
			hwsamplerate=card->rate;
			auresult=AudioSessionSetProperty(kAudioSessionProperty_PreferredHardwareSampleRate
											 ,sizeof(hwsamplerate)
											 , &hwsamplerate);
			check_au_session_result(auresult,"set kAudioSessionProperty_PreferredHardwareSampleRate");
		} else {
			ms_message("Not applying kAudioSessionProperty_PreferredHardwareSampleRate because asked rate is too high [%i]",((int)hwsamplerate));
		}
	} else {
		ms_message("Not applying kAudioSessionProperty_PreferredHardwareSampleRate because HW rate already correct [%i]",((int)hwsamplerate));
	}
}

static void au_write_postprocess(MSFilter *f){
	ms_debug("au_write_postprocess");
	au_filter_write_data_t *d= (au_filter_write_data_t*)f->data;
	ms_mutex_lock(&d->mutex);
	ms_bufferizer_flush(d->bufferizer);
	ms_mutex_unlock(&d->mutex);
}



static void au_write_process(MSFilter *f){
	ms_debug("au_write_process");
	au_filter_write_data_t *d=(au_filter_write_data_t*)f->data;
	mblk_t *m;

	if (!(d->base.card->write_started=d->base.card->io_unit_started)) {
		//make sure audio unit is started
		start_audio_unit((au_filter_base_t*)d,f->ticker->time);
	}

	if (d->base.muted){
		ms_queue_flush(f->inputs[0]);
		return;
	}
	while((m=ms_queue_get(f->inputs[0]))!=NULL){
		ms_mutex_lock(&d->mutex);
		ms_bufferizer_put(d->bufferizer,m);
		ms_mutex_unlock(&d->mutex);
	}
}

static int set_rate(MSFilter *f, void *arg){
	int proposed_rate = *((int*)arg);
	ms_debug("set_rate %d",proposed_rate);
	au_filter_base_t *d=(au_filter_base_t*)f->data;
	if (proposed_rate != d->card->rate){
		return -1;//only support 1 rate
	} else {
		return 0;
	}
}

static int get_rate(MSFilter *f, void *data){
	au_filter_base_t *d=(au_filter_base_t*)f->data;
	*(int*)data=d->card->rate;
	return 0;
}


static int set_nchannels(MSFilter *f, void *arg){
	ms_debug("set_nchannels %d", *((int*)arg));
	au_filter_base_t *d=(au_filter_base_t*)f->data;
	d->card->nchannels=*(int*)arg;
	return 0;
}

static int get_nchannels(MSFilter *f, void *data) {
	au_filter_base_t *d=(au_filter_base_t *)f->data;
	*(int *)data = d->card->nchannels;
	return 0;
}

static int set_muted(MSFilter *f, void *data){
	au_filter_base_t *d=(au_filter_base_t*)f->data;
	d->muted = *(int*)data;
	return 0;
}

static MSFilterMethod au_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE	, set_rate	},
	{	MS_FILTER_GET_SAMPLE_RATE	, get_rate	},
	{	MS_FILTER_SET_NCHANNELS		, set_nchannels	},
	{	MS_FILTER_GET_NCHANNELS		, get_nchannels	},
	{	MS_AUDIO_PLAYBACK_MUTE	 	, set_muted	},
	{	0				, NULL		}
};

static void shutdown_timer(CFRunLoopTimerRef timer, void *info){
	au_card_t *card=(au_card_t*)info;
	stop_audio_unit(card);
}

static void check_unused(au_card_t *card){
	if (card->read_data==NULL && card->write_data==NULL ){

		if (!card->is_tester && card->shutdown_timer==NULL && card->use_shutdowntimer){
			/*program the shutdown of the audio unit in a few seconds*/
			CFRunLoopTimerContext ctx={0};
			ctx.info=card;
			card->shutdown_timer=CFRunLoopTimerCreate (
												kCFAllocatorDefault,
												CFAbsoluteTimeGetCurrent() + 2.5,
												0,
												0,
												0,
												shutdown_timer,
												&ctx
												);
			CFRunLoopAddTimer(CFRunLoopGetMain(), card->shutdown_timer,kCFRunLoopCommonModes);
		} else if( card->is_tester ) {
			stop_audio_unit(card);
		}
	}
}

static void au_read_uninit(MSFilter *f) {
	au_filter_read_data_t *d=(au_filter_read_data_t*)f->data;
	au_card_t* card=d->base.card;

	ms_mutex_lock(&card->mutex);
	card->read_data=NULL;
	ms_mutex_unlock(&card->mutex);

	check_unused(card);

	ms_mutex_destroy(&d->mutex);

	flushq(&d->rq,0);
	ms_free(d);
}

static void au_write_uninit(MSFilter *f) {
	au_filter_write_data_t *d=(au_filter_write_data_t*)f->data;
	au_card_t* card=d->base.card;

	ms_mutex_lock(&card->mutex);
	card->write_data=NULL;
	ms_mutex_unlock(&card->mutex);

	check_unused(card);

	ms_mutex_destroy(&d->mutex);
	ms_bufferizer_destroy(d->bufferizer);
	ms_free(d);
}

MSFilterDesc au_read_desc={
.id=MS_IOUNIT_READ_ID,
.name="MSAURead",
.text=N_("Sound capture filter for iOS Audio Unit Service"),
.category=MS_FILTER_OTHER,
.ninputs=0,
.noutputs=1,
.preprocess=au_read_preprocess,
.process=au_read_process,
.postprocess=au_read_postprocess,
.uninit=au_read_uninit,
.methods=au_methods
};


MSFilterDesc au_write_desc={
.id=MS_IOUNIT_WRITE_ID,
.name="MSAUWrite",
.text=N_("Sound playback filter for iOS Audio Unit Service"),
.category=MS_FILTER_OTHER,
.ninputs=1,
.noutputs=0,
.preprocess=au_write_preprocess,
.process=au_write_process,
.postprocess=au_write_postprocess,
.uninit=au_write_uninit,
.methods=au_methods
};

static MSFilter *ms_au_read_new(MSSndCard *mscard){
	ms_debug("ms_au_read_new");
	au_card_t* card=(au_card_t*)(mscard->data);
	MSFilter *f=ms_factory_create_filter_from_desc(ms_snd_card_get_factory(mscard), &au_read_desc);
	au_filter_read_data_t *d=ms_new0(au_filter_read_data_t,1);
	qinit(&d->rq);
	d->readTimeStamp.mSampleTime=-1;
	ms_mutex_init(&d->mutex,NULL);
	d->base.card=card;
	card->read_data=d;
	f->data=d;
	return f;
}

static MSFilter *ms_au_write_new(MSSndCard *mscard){
	ms_debug("ms_au_write_new");
	au_card_t* card=(au_card_t*)(mscard->data);
	MSFilter *f=ms_factory_create_filter_from_desc(ms_snd_card_get_factory(mscard), &au_write_desc);
	au_filter_write_data_t *d=ms_new0(au_filter_write_data_t,1);
	d->bufferizer= ms_bufferizer_new();
	ms_mutex_init(&d->mutex,NULL);
	d->base.card=card;
	card->write_data=d;
	f->data=d;

	if (card->rate == 0){ /*iounit stopped set initial value*/
		card->rate=ms_snd_card_get_preferred_sample_rate(card->ms_snd_card);
	}
	return f;
}


MS_FILTER_DESC_EXPORT(au_read_desc)
MS_FILTER_DESC_EXPORT(au_write_desc)

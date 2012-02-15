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
#include <AudioToolbox/AudioToolbox.h>
#include "mediastreamer2/mssndcard.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"

#define PREFERRED_HW_SAMPLE_RATE 44100

#if 0
#undef ms_debug
#define ms_debug ms_message
#endif
static const char* AU_CARD_RECEIVER = "Audio Unit Receiver";
static const char* AU_CARD_SPEAKER = "Audio Unit Speaker";


static MSFilter *ms_au_read_new(MSSndCard *card);
static MSFilter *ms_au_write_new(MSSndCard *card);

typedef struct AUData_t{
	AudioUnit	io_unit;
	unsigned int	rate;
	unsigned int	bits;
	unsigned int	nchannels;
	ms_mutex_t	mutex;
	queue_t		rq;
	MSBufferizer	*bufferizer;
    uint64_t last_failed_config_time;
	bool_t		started;
	bool_t		read_started;
	bool_t		write_started;
	AudioTimeStamp readTimeStamp;
	unsigned int n_lost_frame;
	bool_t io_unit_must_be_started;
	bool_t is_ringer;
} AUData;


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

static bool filter_valid = false;

static void au_init(MSSndCard *card){
	ms_debug("au_init");
	AUData *d=ms_new(AUData,1);
	//OSStatus auresult;
	d->bits=16;
	d->rate=PREFERRED_HW_SAMPLE_RATE;
	d->nchannels=1;
	
	d->read_started=FALSE;
	d->write_started=FALSE;
	d->bufferizer=ms_bufferizer_new();
	d->n_lost_frame=0;
	d->started=FALSE;
	d->io_unit_must_be_started=FALSE;
	qinit(&d->rq);
    d->last_failed_config_time = 0;
	d->readTimeStamp.mSampleTime=-1;
	ms_mutex_init(&d->mutex,NULL);
	
	if (strcmp(card->name,AU_CARD_SPEAKER)==0) {
		d->is_ringer=TRUE;
	} else {
		d->is_ringer=FALSE;
	}
	card->data=d;
    
    filter_valid = TRUE;
}

static void au_uninit(MSSndCard *card){
    filter_valid = FALSE;
    
	AUData *d=(AUData*)card->data;
	ms_bufferizer_destroy(d->bufferizer);
	ms_mutex_destroy(&d->mutex);
	ms_free(d);
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
.duplicate=au_duplicate
};

static MSSndCard *au_duplicate(MSSndCard *obj){
	MSSndCard *card=ms_snd_card_new_with_name(&au_card_desc,obj->name);
	return card;
}

static const char *FormatError(OSStatus error)
{
	// not re-entrant but..
	static char str[64];
    
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
    }

    // see if it appears to be a 4-char-code
    *(UInt32 *)(str + 1) = CFSwapInt32HostToBig(error);
    if (isprint(str[1]) && isprint(str[2]) && isprint(str[3]) && isprint(str[4])) {
        str[0] = str[5] = '\'';
        str[6] = '\0';
    } else
        // no, format it as an integer
        sprintf(str, "%d", (int)error);
    return str;
}


#define check_auresult(au,method) \
if (au!=0) ms_error("AudioUnit error for %s: ret=%s (%li) ",method, FormatError(au), au)

static MSSndCard *au_card_new(const char* name){
	MSSndCard *card=ms_snd_card_new_with_name(&au_card_desc,name);
	OSStatus auresult = AudioSessionInitialize(NULL, NULL, NULL, card);
	if (auresult != kAudioSessionAlreadyInitialized) {
		check_auresult(auresult,"AudioSessionInitialize");
	}
	return card;
}

static void au_detect(MSSndCardManager *m){
	ms_debug("au_detect");
	MSSndCard *card=au_card_new(AU_CARD_RECEIVER);
	ms_snd_card_manager_add_card(m,card);
	//card=au_card_new(AU_CARD_SPEAKER); //Disabled because iounit cannot be used in play only mode on IOS 5.0
	//ms_snd_card_manager_add_card(m,card);	
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
	AUData *d=(AUData*)inRefCon;
	if (d->readTimeStamp.mSampleTime <0) {
		d->readTimeStamp=*inTimeStamp;
	}
	OSStatus err=0;
	mblk_t * rm=NULL;
	if (d->read_started) {
		rm=allocb(ioData->mBuffers[0].mDataByteSize,0);
		ioData->mBuffers[0].mData=rm->b_wptr;
	}
	err = AudioUnitRender(d->io_unit, ioActionFlags, &d->readTimeStamp, inBusNumber,inNumberFrames, ioData);
	if (d->read_started){
		if (err == 0) {
			rm->b_wptr += ioData->mBuffers[0].mDataByteSize;
			ms_mutex_lock(&d->mutex);
			putq(&d->rq,rm);
			ms_mutex_unlock(&d->mutex);
			d->readTimeStamp.mSampleTime+=ioData->mBuffers[0].mDataByteSize/(d->bits/2);
		} else {
            ms_warning("AudioUnitRender() failed: %s (%li)", FormatError(err), err);
            freeb(rm);
        }
	}
	return err;
}

/********************read/write cb******************/

static OSStatus au_render_cb (
							  void                        *inRefCon,
							  AudioUnitRenderActionFlags  *ioActionFlags,
							  const AudioTimeStamp        *inTimeStamp,
							  UInt32                      inBusNumber,
							  UInt32                      inNumberFrames,
							  AudioBufferList             *ioData
) {
	ms_debug("render cb");
	AUData *d=(AUData*)inRefCon;
	
	if (d->write_started == TRUE) {
		ioData->mBuffers[0].mDataByteSize=inNumberFrames*d->bits/8;
		ioData->mNumberBuffers=1;
		
		ms_mutex_lock(&d->mutex);
		if(ms_bufferizer_get_avail(d->bufferizer) >= inNumberFrames*d->bits/8) {
			ms_bufferizer_read(d->bufferizer, ioData->mBuffers[0].mData, inNumberFrames*d->bits/8);

			if (ms_bufferizer_get_avail(d->bufferizer) >10*inNumberFrames*d->bits/8) {
				ms_debug("we are late, bufferizer sise is %i bytes in framezize is %i bytes",ms_bufferizer_get_avail(d->bufferizer),inNumberFrames*d->bits/8);
				ms_bufferizer_flush(d->bufferizer);
			}
			ms_mutex_unlock(&d->mutex);
			
		} else {
			
			ms_mutex_unlock(&d->mutex);
			memset(ioData->mBuffers[0].mData, 0,ioData->mBuffers[0].mDataByteSize);
			ms_debug("nothing to write, pushing silences, bufferizer size is %i bytes in framezize is %i bytes mDataByteSize %i"
					 ,ms_bufferizer_get_avail(d->bufferizer)
					 ,inNumberFrames*d->bits/8
					 ,ioData->mBuffers[0].mDataByteSize);
			d->n_lost_frame+=inNumberFrames;
		}
	}
	if (!d->is_ringer) { // no need to read in ringer mode
		AudioBufferList readAudioBufferList;
		readAudioBufferList.mBuffers[0].mDataByteSize=inNumberFrames*d->bits/8; 
		readAudioBufferList.mNumberBuffers=1;
		readAudioBufferList.mBuffers[0].mData=NULL;
		readAudioBufferList.mBuffers[0].mNumberChannels=d->nchannels;
		AudioUnitElement inputBus = 1;
		au_read_cb(d, ioActionFlags, inTimeStamp, inputBus, inNumberFrames, &readAudioBufferList);
	}
	return 0;
}

/****************config**************/

/* returns TRUE only if configuration is successful */
static bool_t au_configure(AUData *d, uint64_t time) {
	AudioStreamBasicDescription audioFormat;
	AudioComponentDescription au_description;
	AudioComponent foundComponent;
	OSStatus auresult;
	UInt32 doSetProperty      = 1;
	UInt32 doNotSetProperty    = 0;	

	UInt32 audioCategory;
    
    if (d->last_failed_config_time && (time - d->last_failed_config_time) < 1000) {
        /* only try to reconfigure every 1 sec */
        return FALSE;
    }
    
    auresult = AudioSessionSetActive(true);
	check_auresult(auresult,"AudioSessionSetActive(true)");
    if (auresult != 0) {
        ms_warning("AudioUnit configuration failed. Will retry in 1 s");
        d->last_failed_config_time = time;
        return FALSE;
    }
	
	if (d->is_ringer && kCFCoreFoundationVersionNumber > kCFCoreFoundationVersionNumber10_6 /*I.E is >=OS4*/) {
        audioCategory= kAudioSessionCategory_AmbientSound;
        ms_message("Configuring audio session for play back");
	} else {
		audioCategory = kAudioSessionCategory_PlayAndRecord;
		ms_message("Configuring audio session for play back/record");
		
	}
	auresult =AudioSessionSetProperty(kAudioSessionProperty_AudioCategory, sizeof(audioCategory), &audioCategory);
	check_auresult(auresult,"Configuring audio session ");
    if (d->is_ringer && !(kCFCoreFoundationVersionNumber > kCFCoreFoundationVersionNumber10_6 /*I.E is <OS4*/)) {
        //compatibility with 3.1
        auresult=AudioSessionSetProperty (kAudioSessionProperty_OverrideCategoryDefaultToSpeaker,sizeof (doSetProperty),&doSetProperty);
        check_auresult(auresult,"kAudioSessionProperty_OverrideAudioRoute");
        ms_message("Configuring audio session default route to speaker");            

    }
    
	if (d->started == TRUE) {
		//nothing else to do
		return TRUE;
	}
	
	au_description.componentType          = kAudioUnitType_Output;
	au_description.componentSubType       = kAudioUnitSubType_VoiceProcessingIO;
	au_description.componentManufacturer  = kAudioUnitManufacturer_Apple;
	au_description.componentFlags         = 0;
	au_description.componentFlagsMask     = 0;
	
	foundComponent = AudioComponentFindNext (NULL,&au_description);
	
	auresult=AudioComponentInstanceNew (foundComponent, &d->io_unit);
	
	check_auresult(auresult,"AudioComponentInstanceNew");

	audioFormat.mSampleRate			= d->rate;
	audioFormat.mFormatID			= kAudioFormatLinearPCM;
	audioFormat.mFormatFlags		= kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
	audioFormat.mFramesPerPacket	= 1;
	audioFormat.mChannelsPerFrame	= d->nchannels;
	audioFormat.mBitsPerChannel		= d->bits;
	audioFormat.mBytesPerPacket		= d->bits / 8;
	audioFormat.mBytesPerFrame		= d->nchannels * d->bits / 8;
	AudioUnitElement outputBus = 0;
	AudioUnitElement inputBus = 1;
	auresult=AudioUnitUninitialize (d->io_unit);
	
	check_auresult(auresult,"AudioUnitUninitialize");
	
if (!d->is_ringer || kCFCoreFoundationVersionNumber <= kCFCoreFoundationVersionNumber10_6 /*I.E is <OS4*/) {
		//read
		auresult=AudioUnitSetProperty (
									   d->io_unit,
									   kAudioOutputUnitProperty_EnableIO,
									   kAudioUnitScope_Input ,
									   inputBus,
									   &doSetProperty,
									   sizeof (doSetProperty)
									   );
		check_auresult(auresult,"kAudioOutputUnitProperty_EnableIO,kAudioUnitScope_Input");
		auresult=AudioUnitSetProperty (
									   d->io_unit,
									   kAudioUnitProperty_StreamFormat,
									   kAudioUnitScope_Output,
									   inputBus,
									   &audioFormat,
									   sizeof (audioFormat)
									   );
		check_auresult(auresult,"kAudioUnitProperty_StreamFormat,kAudioUnitScope_Output");
}
		//write	
	//enable output bus
	auresult =AudioUnitSetProperty (
									d->io_unit,
									kAudioOutputUnitProperty_EnableIO,
									kAudioUnitScope_Output ,
									outputBus,
									&doSetProperty,
									sizeof (doSetProperty)
									);
	check_auresult(auresult,"kAudioOutputUnitProperty_EnableIO,kAudioUnitScope_Output");
	
	//setup stream format
	auresult=AudioUnitSetProperty (
								   d->io_unit,
								   kAudioUnitProperty_StreamFormat,
								   kAudioUnitScope_Input,
								   outputBus,
								   &audioFormat,
								   sizeof (audioFormat)
								   );
	check_auresult(auresult,"kAudioUnitProperty_StreamFormat,kAudioUnitScope_Input");
	
	//disable unit buffer allocation
	auresult=AudioUnitSetProperty (
								   d->io_unit,
								   kAudioUnitProperty_ShouldAllocateBuffer,
								   kAudioUnitScope_Output,
								   outputBus,
								   &doNotSetProperty,
								   sizeof (doNotSetProperty)
								   );
	check_auresult(auresult,"kAudioUnitProperty_ShouldAllocateBuffer,kAudioUnitScope_Output");
	
	
	AURenderCallbackStruct renderCallbackStruct;            
	renderCallbackStruct.inputProc       = au_render_cb;  
	renderCallbackStruct.inputProcRefCon = d;          
	
	auresult=AudioUnitSetProperty (
								   d->io_unit,                                  
								   kAudioUnitProperty_SetRenderCallback,        
								   kAudioUnitScope_Input,                       
								   outputBus,                                   
								   &renderCallbackStruct,                              
								   sizeof (renderCallbackStruct)                       
								   );
	check_auresult(auresult,"kAudioUnitProperty_SetRenderCallback,kAudioUnitScope_Input");
	
	const Float64 preferredSampleRate = d->rate;//PREFERRED_HW_SAMPLE_RATE; /*optimum to minimize delay, must put a software resampler to deal with 8khz*/
	
	auresult=AudioSessionSetProperty(kAudioSessionProperty_PreferredHardwareSampleRate
									 ,sizeof(preferredSampleRate)
									 , &preferredSampleRate);
	check_auresult(auresult,"kAudioSessionProperty_PreferredHardwareSampleRate");
	
	
	//start io unit
	auresult=AudioUnitInitialize (d->io_unit);
	Float32 preferredBufferSize;
	switch (d->rate) {
		case 11025:
		case 22050: 
			preferredBufferSize= .020;
			break;
		default:
			preferredBufferSize= .015;
	}
			
	auresult=AudioSessionSetProperty(kAudioSessionProperty_PreferredHardwareIOBufferDuration
									 ,sizeof(preferredBufferSize)
									 , &preferredBufferSize);
	
	
	if (auresult != 0) ms_message("kAudioSessionProperty_PreferredHardwareIOBufferDuration returns %li ",auresult);
	
	Float64 delay;
	UInt32 delaySize = sizeof(delay);
	auresult=AudioUnitGetProperty(d->io_unit
								  ,kAudioUnitProperty_Latency
								  , kAudioUnitScope_Global
								  , 0
								  , &delay
								  , &delaySize);

	UInt32 quality;
	UInt32 qualitySize = sizeof(quality);
	auresult=AudioUnitGetProperty(d->io_unit
								  ,kAudioUnitProperty_RenderQuality
								  , kAudioUnitScope_Global
								  , 0
								  , &quality
								  , &qualitySize);
	
	
	
	ms_message("I/O unit latency [%f], quality [%li]",delay,quality);
	Float32 hwoutputlatency;
	UInt32 hwoutputlatencySize=sizeof(hwoutputlatency);
	auresult=AudioSessionGetProperty(kAudioSessionProperty_CurrentHardwareOutputLatency
									 ,&hwoutputlatencySize
									 , &hwoutputlatency);
	Float32 hwinputlatency;
	UInt32 hwinputlatencySize=sizeof(hwoutputlatency);
	auresult=AudioSessionGetProperty(kAudioSessionProperty_CurrentHardwareInputLatency
									 ,&hwinputlatencySize
									 , &hwinputlatency);
	
	Float32 hwiobuf;
	UInt32 hwiobufSize=sizeof(hwiobuf);
	auresult=AudioSessionGetProperty(kAudioSessionProperty_CurrentHardwareIOBufferDuration
									 ,&hwiobufSize
									 , &hwiobuf);
	
	Float64 hwsamplerate;
	UInt32 hwsamplerateSize=sizeof(hwsamplerate);
	auresult=AudioSessionGetProperty(kAudioSessionProperty_CurrentHardwareSampleRate
									 ,&hwsamplerateSize
									 ,&hwsamplerate);

	ms_message("current hw output latency [%f] input [%f] iobuf[%f] sample rate [%f]",hwoutputlatency,hwinputlatency,hwiobuf,hwsamplerate);
	auresult=AudioOutputUnitStart(d->io_unit);
	check_auresult(auresult,"AudioOutputUnitStart");
	d->started=TRUE;
	return TRUE;
}	

static void au_configure_read(AUData *d, uint64_t t) {
    d->read_started=au_configure(d, t);
    ms_mutex_lock(&d->mutex);
    flushq(&d->rq,0);
    ms_mutex_unlock(&d->mutex);
}

static void au_configure_write(AUData *d, uint64_t t) {
    d->write_started=au_configure(d, t);
    ms_mutex_lock(&d->mutex);
    ms_bufferizer_flush(d->bufferizer);
    ms_mutex_unlock(&d->mutex);	
}


static void au_unconfigure(AUData *d) {
	if (d->write_started==FALSE && d->read_started==FALSE) {
		AudioUnitUninitialize(d->io_unit);
		AudioOutputUnitStop(d->io_unit);
		AudioComponentInstanceDispose (d->io_unit);
		d->started=FALSE;
		check_auresult(AudioSessionSetActive(false),"AudioSessionSetActive(false)");
	}
}

static void au_unconfigure_read(AUData *d){
	if(d->read_started == TRUE) {
		d->read_started=FALSE;
		AudioUnitElement inputBus = 1;
		AudioUnitReset(d->io_unit,
			kAudioUnitScope_Global,
			inputBus);
	}
	au_unconfigure(d);
	ms_mutex_lock(&d->mutex);
	flushq(&d->rq,0);
    flushq(&d->rq,0);
	ms_mutex_unlock(&d->mutex);
}

static void au_unconfigure_write(AUData *d){
	if(d->write_started == TRUE) {
		d->write_started=FALSE;
		AudioUnitElement outputBus = 0;
		AudioUnitReset(d->io_unit,
			kAudioUnitScope_Global,
			outputBus);		
	}
	ms_message("[%i] frames of silence inserted for [%i] ms.",d->n_lost_frame,(d->n_lost_frame*1000)/d->rate);
	au_unconfigure(d);
	ms_mutex_lock(&d->mutex);
	ms_bufferizer_flush(d->bufferizer);
	ms_mutex_unlock(&d->mutex);
}




/***********************************read function********************/

static void au_read_preprocess(MSFilter *f){
	ms_debug("au_read_preprocess");
    AUData *d=(AUData*)((MSSndCard*)f->data)->data;
    au_configure_read(d, f->ticker->time);
}

static void au_read_postprocess(MSFilter *f){
	AUData *d=(AUData*)((MSSndCard*)f->data)->data;
	au_unconfigure_read(d);
}

static void au_read_process(MSFilter *f){
	AUData *d=(AUData*)((MSSndCard*)f->data)->data;
	mblk_t *m;
    
    if (!d->read_started)
        au_configure_read(d, f->ticker->time);
		
	if (d->io_unit_must_be_started) {
		if (f->ticker->time % 100 == 0) { /*more or less every 100ms*/
			if (AudioOutputUnitStart(d->io_unit) == 0) {
				d->io_unit_must_be_started=FALSE;
			};
		}
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
    AUData *d=(AUData*)((MSSndCard*)f->data)->data;
    au_configure_write(d, f->ticker->time);
}

static void au_write_postprocess(MSFilter *f){
	ms_debug("au_write_postprocess");
	AUData *d=(AUData*)((MSSndCard*)f->data)->data;
	au_unconfigure_write(d);
}



static void au_write_process(MSFilter *f){
	ms_debug("au_write_process");
	mblk_t *m;
	AUData *d=(AUData*)((MSSndCard*)f->data)->data;
	
    if (!d->write_started)
        au_configure_write(d, f->ticker->time);
    
	while((m=ms_queue_get(f->inputs[0]))!=NULL){
		ms_mutex_lock(&d->mutex);
		ms_bufferizer_put(d->bufferizer,m);
		ms_mutex_unlock(&d->mutex);
	}
}

static int set_rate(MSFilter *f, void *arg){
	int proposed_rate = *((int*)arg);
	ms_debug("set_rate %d",proposed_rate);
	/*if (proposed_rate != PREFERRED_HW_SAMPLE_RATE) {
		return -1;//only support 1 rate
	}*/
	MSSndCard *card=(MSSndCard*)f->data;
	AUData *d=(AUData*)card->data;
	d->rate=proposed_rate;
	return 0;
}

static int get_rate(MSFilter *f, void *data){
	MSSndCard *card=(MSSndCard*)f->data;
	AUData *d=(AUData*)card->data;
	*(int*)data=d->rate;
	return 0;
}


static int set_nchannels(MSFilter *f, void *arg){
	ms_debug("set_nchannels %d", *((int*)arg));
	MSSndCard *card=(MSSndCard*)f->data;
	AUData *d=(AUData*)card->data;
	d->nchannels=*(int*)arg;
	return 0;
}

static MSFilterMethod au_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE	, set_rate	},
	{	MS_FILTER_GET_SAMPLE_RATE	, get_rate	},
	{	MS_FILTER_SET_NCHANNELS		, set_nchannels	},
	{	0				, NULL		}
};

MSFilterDesc au_read_desc={
.id=MS_IOUNIT_READ_ID,
.name="MSAURead",
.text=N_("Sound capture filter for MacOS X Audio Unit Service"),
.category=MS_FILTER_OTHER,
.ninputs=0,
.noutputs=1,
.preprocess=au_read_preprocess,
.process=au_read_process,
.postprocess=au_read_postprocess,
.methods=au_methods
};


MSFilterDesc au_write_desc={
.id=MS_IOUNIT_WRITE_ID,
.name="MSAUWrite",
.text=N_("Sound playback filter for MacOS X Audio Unit Service"),
.category=MS_FILTER_OTHER,
.ninputs=1,
.noutputs=0,
.preprocess=au_write_preprocess,
.process=au_write_process,
.postprocess=au_write_postprocess,
.methods=au_methods
};

static MSFilter *ms_au_read_new(MSSndCard *card){
	ms_debug("ms_au_read_new");
	MSFilter *f=ms_filter_new_from_desc(&au_read_desc);
	f->data=card;
	return f;
}


static MSFilter *ms_au_write_new(MSSndCard *card){
	ms_debug("ms_au_write_new");
	MSFilter *f=ms_filter_new_from_desc(&au_write_desc);
	f->data=card;
	return f;
}

MS_FILTER_DESC_EXPORT(au_read_desc)
MS_FILTER_DESC_EXPORT(au_write_desc)

void ms_au_register_card() {
	/**
	 * register audio unit plugin should be move to linphone code
	 */
	ms_snd_card_manager_register_desc(ms_snd_card_manager_get(),&au_card_desc);
}	

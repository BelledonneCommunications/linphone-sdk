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

#define PREFERRED_HW_SAMPLE_RATE 44100

#if 0
#undef ms_debug
#define ms_debug ms_message
#endif

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
	bool_t		started;
	bool_t		read_started;
	bool_t		write_started;
	AudioTimeStamp readTimeStamp;
	unsigned int n_lost_frame;
	bool_t io_unit_must_be_started;
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
	d->readTimeStamp.mSampleTime=-1;
	ms_mutex_init(&d->mutex,NULL);
	card->data=d;
}

static void au_uninit(MSSndCard *card){
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
	MSSndCard *card=ms_snd_card_new(&au_card_desc);
	card->name=ms_strdup(obj->name);
	return card;
}
#define check_auresult(au,method) \
if (au!=0) ms_error("AudioUnit error for %s: ret=%i",method,au)

static void au_interuption_listener(void* inClientData, UInt32 inInterruptionState) {
	if (((MSSndCard*)inClientData)->data == NULL) return;
	
	AUData *d=(AUData*)(((MSSndCard*)inClientData)->data);
	if (d->started == FALSE) {
		//nothing to do
		return;
	}
	switch (inInterruptionState) {
		case kAudioSessionBeginInterruption:
			ms_message ("IO unit interruption begin");
			AudioOutputUnitStop(d->io_unit);
			break;
		case kAudioSessionEndInterruption:
			// make sure we are again the active session
			ms_message ("IO unit interruption end");
			OSStatus auresult = AudioSessionSetActive(true);
			check_auresult(auresult,"AudioSessionSetActive");
			d->io_unit_must_be_started=TRUE;
			break;
		default:
			ms_warning ("unexpected interuption %i",inInterruptionState);
	}
}

static MSSndCard *au_card_new(){
	MSSndCard *card=ms_snd_card_new(&au_card_desc);
	card->name=ms_strdup("Audio Unit");
	OSStatus auresult = AudioSessionInitialize(NULL, NULL, au_interuption_listener, card);
	check_auresult(auresult,"AudioSessionInitialize");
	return card;
}

static void au_detect(MSSndCardManager *m){
	ms_debug("au_detect");
	MSSndCard *card=au_card_new();
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
		}else ms_warning("AudioUnitRender() failed: %i",err);
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
		ms_mutex_lock(&d->mutex);
		if(ms_bufferizer_get_avail(d->bufferizer) >= inNumberFrames*d->bits/8) {
			ms_bufferizer_read(d->bufferizer, ioData->mBuffers[0].mData, inNumberFrames*d->bits/8);

			if (ms_bufferizer_get_avail(d->bufferizer) >10*inNumberFrames*d->bits/8) {
				ms_debug("we are late, bufferizer sise is %i bytes in framezize is %i bytes",ms_bufferizer_get_avail(d->bufferizer),inNumberFrames*d->bits/8);
				ms_bufferizer_flush(d->bufferizer);
			}
			ioData->mBuffers[0].mDataByteSize=inNumberFrames*d->bits/8;
			ms_mutex_unlock(&d->mutex);
			
			
		} else {
			
			ms_mutex_unlock(&d->mutex);
			memset(ioData->mBuffers[0].mData, 0,ioData->mBuffers[0].mDataByteSize);
			ms_debug("nothing to write, pushing silences, bufferizer sise is %i bytes in framezize is %i bytes mDataByteSize %i"
					 ,ms_bufferizer_get_avail(d->bufferizer)
					 ,inNumberFrames*d->bits/8
					 ,ioData->mBuffers[0].mDataByteSize);
			d->n_lost_frame+=inNumberFrames;
		}
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

static void au_configure(AUData *d) {
	AudioStreamBasicDescription audioFormat;
	AudioComponentDescription au_description;
	AudioComponent foundComponent;
	OSStatus auresult;
	
	if (d->started == TRUE) {
		//nothing to do
		return;
	}
	
	auresult = AudioSessionSetActive(true);
	check_auresult(auresult,"AudioSessionSetActive");

	UInt32 audioCategory = kAudioSessionCategory_PlayAndRecord;
	auresult =AudioSessionSetProperty(kAudioSessionProperty_AudioCategory, sizeof(audioCategory), &audioCategory);
	check_auresult(auresult,"Configuring audio session for play/record");
	
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
	UInt32 doSetProperty      = 1;
	UInt32 doNotSetProperty    = 0;
	AudioUnitElement outputBus = 0;
	AudioUnitElement inputBus = 1;
	auresult=AudioUnitUninitialize (d->io_unit);
	
	check_auresult(auresult,"AudioUnitUninitialize");
	
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
	
	
	if (auresult != 0) ms_message("kAudioSessionProperty_PreferredHardwareIOBufferDuration returns %i ",auresult);
	
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
	
	
	
	ms_message("I/O unit latency [%f], quality [%i]",delay,quality);
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
	return;
}	

static void au_configure_read(AUData *d) {
	d->read_started=TRUE;
	au_configure(d);
	ms_mutex_lock(&d->mutex);
	flushq(&d->rq,0);
	ms_mutex_unlock(&d->mutex);
	
}

static void au_configure_write(AUData *d) {
	d->write_started=TRUE;
	au_configure(d);
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
	ms_message("[%i] frames of silence inserted for [%i] ms",d->n_lost_frame,(d->n_lost_frame*1000)/d->rate);
	au_unconfigure(d);
	ms_mutex_lock(&d->mutex);
	ms_bufferizer_flush(d->bufferizer);
	ms_mutex_unlock(&d->mutex);
}




/***********************************read function********************/

static void au_read_preprocess(MSFilter *f){
	ms_debug("au_read_preprocess");
	AUData *d=(AUData*)((MSSndCard*)f->data)->data;
	au_configure_read(d);
	
}

static void au_read_postprocess(MSFilter *f){
	AUData *d=(AUData*)((MSSndCard*)f->data)->data;
	au_unconfigure_read(d);
}

static void au_read_process(MSFilter *f){
	AUData *d=(AUData*)((MSSndCard*)f->data)->data;
	mblk_t *m;
	struct timeval tv;
	
	if (d->io_unit_must_be_started) {
		gettimeofday(&tv, 0);
		if ((tv.tv_usec % 10) == 0) { /*more or less every 100ms*/
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
	au_configure_write(d);
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

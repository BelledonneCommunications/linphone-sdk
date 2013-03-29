/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2010-2011  Belledonne Communications (simon.morlat@linphone.org)

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

/*
A prior version of this file was developed by Hiroki Mori and was contributed to the project
under a BSD license.
The source code has then largely evolved and was refactored.
The BSD license below is for the original work.
*/

/**
* Copyright (C) 2007  Hiroki Mori (himori@users.sourceforge.net)
* All rights reserved.
* 
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above copyright
*       notice, this list of conditions and the following disclaimer in the
*       documentation and/or other materials provided with the distribution.
*     * Neither the name of the <organization> nor the
*       names of its contributors may be used to endorse or promote products
*       derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY <copyright holder> ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL <copyright holder> BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**/

#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>
//#include <CoreServices/CarbonCore/Debugging.h>

#include "mediastreamer2/mssndcard.h"
#include "mediastreamer2/msfilter.h"

#if __LP64__ 
#define UINT32_PRINTF "u"
#define UINT32_X_PRINTF "x"
#else
#define UINT32_PRINTF "lu"
#define UINT32_X_PRINTF "lx"
#endif

MSFilter *ms_au_read_new(MSSndCard *card);
MSFilter *ms_au_write_new(MSSndCard *card);

#define CHECK_AURESULT(call)	do{ int _err; if ((_err=(call))!=noErr) ms_error( #call ": error [%i] %s %s",_err,GetMacOSStatusErrorString(_err),GetMacOSStatusCommentString(_err)); }while(0)
/*#undef ms_debug
#define ms_debug ms_message*/
static void show_format(const char *name, AudioStreamBasicDescription * deviceFormat)
{
	ms_debug("Format for %s", name);
	ms_debug("mSampleRate = %g", deviceFormat->mSampleRate);
	unsigned int fcc= ntohl(deviceFormat->mFormatID);
	char outName[5];
	memcpy(outName,&fcc,4);
	outName[4] = 0;
	ms_debug("mFormatID = %s", outName);
	ms_debug("mFormatFlags = %08lX", deviceFormat->mFormatFlags);
	ms_debug("mBytesPerPacket = %ld", deviceFormat->mBytesPerPacket);
	ms_debug("mFramesPerPacket = %ld", deviceFormat->mFramesPerPacket);
	ms_debug("mChannelsPerFrame = %ld", deviceFormat->mChannelsPerFrame);
	ms_debug("mBytesPerFrame = %ld", deviceFormat->mBytesPerFrame);
	ms_debug("mBitsPerChannel = %ld", deviceFormat->mBitsPerChannel);
	ms_message("Format for [%s] rate [%g] channels [%"UINT32_PRINTF"]", outName,deviceFormat->mSampleRate,deviceFormat->mChannelsPerFrame);
}


typedef struct AUCommon{
	AudioDeviceID dev;
	int rate;
	int nchannels;
	AudioUnit au;
	ms_mutex_t mutex;
} AUCommon;

typedef struct AURead{
	AUCommon common;
	queue_t rq;
}AURead;

typedef struct AUWrite{
	AUCommon common;
	MSBufferizer *buffer;
}AUWrite;


typedef struct AuCard {
	char * uidname;
	int removed;
	int rate; /*the nominal rate of the device*/
} AuCard;

static void au_card_set_level(MSSndCard *card, MSSndCardMixerElem e, int percent)
{
}

static int au_card_get_level(MSSndCard *card, MSSndCardMixerElem e)
{
	return -1;
}

static void au_card_set_source(MSSndCard *card, MSSndCardCapture source)
{
}

static void au_card_init(MSSndCard * card)
{
	AuCard *c = (AuCard *) ms_new0(AuCard, 1);
	c->removed = 0;
	card->data = c;
}

static void au_card_uninit(MSSndCard * card)
{
	AuCard *d = (AuCard *) card->data;
	if (d->uidname != NULL)
		ms_free(d->uidname);
	ms_free(d);
}

static void au_card_detect(MSSndCardManager *m);
static MSSndCard *au_card_duplicate(MSSndCard *obj);

MSSndCardDesc ca_card_desc={
	.driver_type="AudioUnit",
	.detect=au_card_detect,
	.init=au_card_init,
	.set_level=au_card_set_level,
	.get_level=au_card_get_level,
	.set_capture=au_card_set_source,
	.set_control=NULL,
	.get_control=NULL,
	.create_reader=ms_au_read_new,
	.create_writer=ms_au_write_new,
	.uninit=au_card_uninit,
	.duplicate=au_card_duplicate
};

static MSSndCard *au_card_duplicate(MSSndCard * obj)
{
	AuCard *ca;
	AuCard *cadup;
	MSSndCard *card = ms_snd_card_new(&ca_card_desc);
	card->name = ms_strdup(obj->name);
	card->data = ms_new0(AuCard, 1);
	memcpy(card->data, obj->data, sizeof(AuCard));
	ca = obj->data;
	cadup = card->data;
	cadup->uidname = ms_strdup(ca->uidname);
	return card;
}

static MSSndCard *ca_card_new(const char *name, const char * uidname, AudioDeviceID dev, unsigned cap)
{
	MSSndCard *card = ms_snd_card_new(&ca_card_desc);
	AudioStreamBasicDescription format;
	AuCard *d = (AuCard *) card->data;
	UInt32 slen;
	int err;

	d->uidname = ms_strdup(uidname);
	card->name = ms_strdup(name);
	card->capabilities = cap;
	
	slen = sizeof(format);
	d->rate=44100;
	if (dev != -1) {
		AudioObjectPropertyScope theScope = cap & MS_SND_CARD_CAP_CAPTURE ? kAudioDevicePropertyScopeInput :
		kAudioDevicePropertyScopeOutput;
		AudioObjectPropertyAddress theAddress = { kAudioDevicePropertyStreamFormat,
				theScope,
				0 };

		err = AudioObjectGetPropertyData(dev
				,&theAddress
				,0
				,NULL
				,&slen
				,&format);

		//		err = AudioDeviceGetProperty(dev, 0, cap & MS_SND_CARD_CAP_CAPTURE, kAudioDevicePropertyStreamFormat, &slen, &format);
		if (err == kAudioHardwareNoError) {
			show_format("device", &format);
			d->rate=format.mSampleRate;
		}
	}
	return card;
}

static bool_t check_card_capability(AudioDeviceID id, bool_t is_input, char * devname, char *uidname, size_t name_len){
	UInt32 slen=name_len;
	CFStringRef dUID=NULL;
	bool_t ret=FALSE;
	OSStatus err;
	AudioObjectPropertyScope theScope = is_input ? kAudioDevicePropertyScopeInput :
	kAudioDevicePropertyScopeOutput;
	AudioObjectPropertyAddress theAddress = { kAudioDevicePropertyDeviceName,
			theScope,
			0 };

	err = AudioObjectGetPropertyData(id
			,&theAddress
			,0
			,NULL
			,&slen
			,devname);
	/*int err =AudioDeviceGetProperty(id, 0, is_input, kAudioDevicePropertyDeviceName, &slen,devname);*/
	if (err != kAudioHardwareNoError) {
		ms_error("get kAudioDevicePropertyDeviceName error %"UINT32_PRINTF, err);
		return FALSE;
	}
	theAddress.mSelector =  kAudioDevicePropertyStreamConfiguration;

	err = AudioObjectGetPropertyDataSize(id
			,&theAddress
			,0
			,NULL
			,&slen);
	/*err =AudioDeviceGetPropertyInfo(id, 0, is_input, kAudioDevicePropertyStreamConfiguration, &slen, &writable);*/
	if (err != kAudioHardwareNoError) {
		ms_error("get kAudioDevicePropertyDeviceName error %"UINT32_PRINTF, err);
		return FALSE;
	}

	AudioBufferList *buflist = ms_malloc(slen);

	theAddress.mSelector =  kAudioDevicePropertyStreamConfiguration;
		err = AudioObjectGetPropertyData(id
				,&theAddress
				,0
				,NULL
				,&slen
				,buflist);
	/*err = 	AudioDeviceGetProperty(id, 0, is_input, kAudioDevicePropertyStreamConfiguration, &slen, buflist);*/
	if (err != kAudioHardwareNoError) {
		ms_error("get kAudioDevicePropertyDeviceName error %"UINT32_PRINTF, err);
		ms_free(buflist);
		return FALSE;
	}

	UInt32 j;
	for (j = 0; j < buflist->mNumberBuffers; j++) {
		if (buflist->mBuffers[j].mNumberChannels > 0) {
			ret=TRUE;
			break;
		}
	}
	ms_free(buflist);
	if (ret==FALSE) return FALSE;
	
	slen = sizeof(CFStringRef);
	theAddress.mSelector =  kAudioDevicePropertyDeviceUID;
		err = AudioObjectGetPropertyData(id
				,&theAddress
				,0
				,NULL
				,&slen
				,&dUID);
	//err =AudioDeviceGetProperty(id, 0, is_input, kAudioDevicePropertyDeviceUID, &slen,&dUID);
	if (err != kAudioHardwareNoError) {
		ms_error("get kAudioHardwarePropertyDevices error %"UINT32_PRINTF, err);
		return FALSE;
	}
	CFStringGetCString(dUID, uidname, name_len,CFStringGetSystemEncoding());
	ms_message("CA: devname:%s uidname:%s", devname, uidname);
			
	
	return ret;
}

static void au_card_detect(MSSndCardManager * m)
{
	OSStatus err;
	UInt32 slen;
	int count;
	Boolean writable;
	int i;
	writable = 0;
	slen = 0;
	AudioObjectPropertyAddress theAddress = { kAudioHardwarePropertyDevices,
												  kAudioObjectPropertyScopeGlobal,
												  kAudioObjectPropertyElementMaster };

	err = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject
			,&theAddress
			,0
			,NULL
			,&slen);
	/*err =
	AudioHardwareGetPropertyInfo(kAudioHardwarePropertyDevices, &slen,
								 &writable);*/
	if (err != kAudioHardwareNoError) {
		ms_error("get kAudioHardwarePropertyDevices error %"UINT32_PRINTF, err);
		return;
	}
	AudioDeviceID devices[slen / sizeof(AudioDeviceID)];
	err = AudioObjectGetPropertyData(kAudioObjectSystemObject
				,&theAddress
				,0
				,NULL
				,&slen
				,&devices);
	/*err =
	AudioHardwareGetProperty(kAudioHardwarePropertyDevices, &slen, devices);*/
	if (err != kAudioHardwareNoError) {
		ms_error("get kAudioHardwarePropertyDevices error %"UINT32_PRINTF, err);
		return;
	}
	/*first, add Default AudioUnit
	does not work: why ?
	*/
	/*ms_snd_card_manager_add_card(m,ca_card_new("Default", "",-1, MS_SND_CARD_CAP_CAPTURE|MS_SND_CARD_CAP_PLAYBACK));
	*/

	count = slen / sizeof(AudioDeviceID);
	for (i = 0; i < count; i++) {
		MSSndCard *card;
		char uidname[256]={0},devname[256]={0};
		int card_capacity=0;
		if (check_card_capability(devices[i],FALSE,devname,uidname,sizeof(uidname))){
			card_capacity|=MS_SND_CARD_CAP_PLAYBACK;
		}
		if (check_card_capability(devices[i],TRUE,devname,uidname,sizeof(uidname))){
			card_capacity|=MS_SND_CARD_CAP_CAPTURE;
		}
		if (card_capacity) {
			card=ca_card_new(devname, uidname, devices[i], card_capacity);
			ms_snd_card_manager_add_card(m, card);
		}

	}
}


static OSStatus readRenderProc(void *inRefCon, 
						AudioUnitRenderActionFlags *inActionFlags,
						const AudioTimeStamp *inTimeStamp, 
						UInt32 inBusNumber,
						UInt32 inNumFrames, 
						AudioBufferList *ioData)
{
	AURead *d=(AURead*)inRefCon;
	AudioBufferList lreadAudioBufferList={0};
	mblk_t *rm;
	OSStatus err;

	

	lreadAudioBufferList.mNumberBuffers=1;
	lreadAudioBufferList.mBuffers[0].mDataByteSize=inNumFrames*sizeof(int16_t)*d->common.nchannels;
	rm=allocb(lreadAudioBufferList.mBuffers[0].mDataByteSize,0);
	lreadAudioBufferList.mBuffers[0].mData=rm->b_wptr;
	lreadAudioBufferList.mBuffers[0].mNumberChannels = 1;

	//ms_message("request to render %i bytes, inNumFrames=%i",lreadAudioBufferList.mBuffers[0].mDataByteSize,inNumFrames);
	err=AudioUnitRender(d->common.au, inActionFlags, inTimeStamp, inBusNumber, inNumFrames, &lreadAudioBufferList);
	
	if (err!=noErr){
		ms_error("AudioUnitRender() for read returned [%"UINT32_PRINTF"] %s %s",err,GetMacOSStatusErrorString(err),GetMacOSStatusCommentString(err));
		return 0;
	}
	//ms_message("Got input buffer of size %i",lreadAudioBufferList.mBuffers[0].mDataByteSize);

	rm->b_wptr+=lreadAudioBufferList.mBuffers[0].mDataByteSize;
	
	ms_mutex_lock(&d->common.mutex);
	putq(&d->rq,rm);
	ms_mutex_unlock(&d->common.mutex);

	return 0;
}

static OSStatus writeRenderProc(void *inRefCon, 
						 AudioUnitRenderActionFlags *inActionFlags,
						 const AudioTimeStamp *inTimeStamp, 
						 UInt32 inBusNumber,
						 UInt32 inNumFrames, 
						 AudioBufferList *ioData)
{
	AUWrite *d=(AUWrite*)inRefCon;
	int read;

	if (ioData->mNumberBuffers!=1) ms_warning("writeRenderProc: %"UINT32_PRINTF" buffers",ioData->mNumberBuffers);
	ms_mutex_lock(&d->common.mutex);
	read=ms_bufferizer_read(d->buffer,ioData->mBuffers[0].mData,ioData->mBuffers[0].mDataByteSize);
	if (ms_bufferizer_get_avail(d->buffer) >10*inNumFrames*2) {
		ms_message("we are late, bufferizer sise is [%i] bytes in framezize is [%"UINT32_PRINTF"] bytes"
					,ms_bufferizer_get_avail(d->buffer)
					,inNumFrames*2);
		ms_bufferizer_flush(d->buffer);
	}

	ms_mutex_unlock(&d->common.mutex);
	if (read==0){
		ms_debug("Silence inserted in audio output unit (%"UINT32_PRINTF" bytes)",ioData->mBuffers[0].mDataByteSize);
		memset(ioData->mBuffers[0].mData,0,ioData->mBuffers[0].mDataByteSize);
	}
	return 0;
}

static int audio_unit_open(AUCommon *d, bool_t is_read){
	OSStatus result;
	UInt32 param;
	ComponentDescription desc;  
	Component comp;
	AudioStreamBasicDescription asbd;
	const int input_bus=1;
	const int output_bus=0;
	
	// Get Default Input audio unit
	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = d->dev!=-1?kAudioUnitSubType_HALOutput:kAudioUnitSubType_DefaultOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;
	
	comp = FindNextComponent(NULL, &desc);
	if (comp == NULL)
	{
		ms_message("Cannot find audio component");
		return -1;
	}
	
	result = OpenAComponent(comp, &d->au);
	if(result != noErr)
	{
		ms_message("Cannot open audio component %"UINT32_X_PRINTF, result);
		return -1;
	}
	
	param = is_read;
	if (d->dev!=-1) {
		CHECK_AURESULT(AudioUnitSetProperty(d->au,
					  kAudioOutputUnitProperty_EnableIO,
					  kAudioUnitScope_Input,
					  input_bus,
					  &param,
					  sizeof(UInt32)));

		param = !is_read;
		CHECK_AURESULT(AudioUnitSetProperty(d->au,
					  kAudioOutputUnitProperty_EnableIO,
					  kAudioUnitScope_Output,
					  output_bus,
					  &param,
					  sizeof(UInt32)));



		// Set the current device
	
			CHECK_AURESULT(AudioUnitSetProperty(d->au,
					  kAudioOutputUnitProperty_CurrentDevice,
					  kAudioUnitScope_Global,
					  output_bus,
					  &d->dev,
					  sizeof(AudioDeviceID)));
	}

	param=0;
	CHECK_AURESULT(AudioUnitSetProperty(d->au,
					  kAudioUnitProperty_ShouldAllocateBuffer,
					  is_read ? kAudioUnitScope_Input : kAudioUnitScope_Output ,
					  is_read ? input_bus : output_bus ,
					  &param,
					  sizeof(param)));

	UInt32 asbdsize = sizeof(AudioStreamBasicDescription);
	memset((char *)&asbd, 0, asbdsize);
	
	CHECK_AURESULT(AudioUnitGetProperty(d->au,
			   kAudioUnitProperty_StreamFormat,
			   is_read ? kAudioUnitScope_Input : kAudioUnitScope_Output,
			   is_read ? input_bus : output_bus,
			   &asbd,
			   &asbdsize));
	
	show_format(is_read ? "Input audio unit" : "Output audio unit",&asbd);
	
	asbd.mSampleRate=d->rate;
			
	asbd.mBytesPerPacket=asbd.mBytesPerFrame = 2*d->nchannels;
	asbd.mChannelsPerFrame = d->nchannels;
	asbd.mBitsPerChannel=16;
	asbd.mFormatID=kAudioFormatLinearPCM;
	asbd.mFormatFlags=kAudioFormatFlagIsPacked|kAudioFormatFlagIsSignedInteger;
	

	
	CHECK_AURESULT(AudioUnitSetProperty(d->au,
					  kAudioUnitProperty_StreamFormat,
					  is_read ?  kAudioUnitScope_Output : kAudioUnitScope_Input,
					  is_read ?  input_bus : output_bus ,
					  &asbd,
					  sizeof(AudioStreamBasicDescription)));
	CHECK_AURESULT(AudioUnitGetProperty(d->au,
			   kAudioUnitProperty_StreamFormat,
			   is_read ? kAudioUnitScope_Output : kAudioUnitScope_Input,
			   is_read ? input_bus : output_bus ,
			   &asbd,
			   &asbdsize));
	
	show_format(is_read ? "Input audio unit after configuration" : "Output audio unit after configuration",&asbd);

	
	// Get the number of frames in the IO buffer(s)
	param = sizeof(UInt32);
	UInt32 numFrames;
	CHECK_AURESULT(AudioUnitGetProperty(d->au,
					  kAudioDevicePropertyBufferFrameSize,
					  kAudioUnitScope_Input,
					  input_bus,
					  &numFrames,
					  &param));
	ms_message("Number of frames per buffer = %"UINT32_PRINTF, numFrames);
	
	AURenderCallbackStruct cbs;
	
	cbs.inputProcRefCon = d;
	if (is_read){
		cbs.inputProc = readRenderProc;
		CHECK_AURESULT(AudioUnitSetProperty(d->au,
					kAudioOutputUnitProperty_SetInputCallback,
					kAudioUnitScope_Global,
					input_bus,
					&cbs,
					sizeof(AURenderCallbackStruct)));
	}else{
		cbs.inputProc = writeRenderProc;
		CHECK_AURESULT(AudioUnitSetProperty (d->au, 
                            kAudioUnitProperty_SetRenderCallback, 
                            kAudioUnitScope_Global, 
                            output_bus,
                            &cbs, 
                            sizeof(AURenderCallbackStruct)));
	}
	result = AudioUnitInitialize(d->au);
	if(result != noErr)
	{
		ms_error("failed to AudioUnitInitialize %"UINT32_PRINTF" , is_read=%i", result,(int)is_read);
		return -1;
	}

	CHECK_AURESULT(AudioOutputUnitStart(d->au));
	return 0;
}

static void audio_unit_close(AUCommon *d){
	
	CHECK_AURESULT(AudioOutputUnitStop(d->au));
	CHECK_AURESULT(AudioUnitUninitialize(d->au));
	d->au=NULL;
}


static mblk_t *au_read_get(AURead *d){
	mblk_t *m;
	ms_mutex_lock(&d->common.mutex);
	m=getq(&d->rq);
	ms_mutex_unlock(&d->common.mutex);
	return m;
}

static void au_write_put(AUWrite *d, mblk_t *m){
	ms_mutex_lock(&d->common.mutex);
	ms_bufferizer_put(d->buffer,m);
	ms_mutex_unlock(&d->common.mutex);
}

static void au_common_init(AUCommon *d){
	d->rate=44100;
	d->nchannels=1;
	ms_mutex_init(&d->mutex,NULL);
}

static void au_common_uninit(AUCommon *d){
	ms_mutex_destroy(&d->mutex);
}

static void au_read_init(MSFilter *f){
	AURead *d = ms_new0(AURead, 1);
	au_common_init(&d->common);
	qinit(&d->rq);
	f->data=d;
}	

static void au_read_preprocess(MSFilter *f){
	AURead *d = (AURead *) f->data;
	audio_unit_open(&d->common,TRUE);
}

static void au_read_process(MSFilter *f){
	AURead *d = (AURead *) f->data;
	mblk_t *m;
	while((m=au_read_get(d))!=NULL){
		ms_queue_put(f->outputs[0],m);
	}
}

static void au_read_postprocess(MSFilter *f){
	AURead *d = (AURead *) f->data;
	audio_unit_close(&d->common);
}

static void au_read_uninit(MSFilter *f){
	AURead *d = (AURead *) f->data;
	flushq(&d->rq,0);
	au_common_uninit(&d->common);
	ms_free(d);
}

/* Audio unit write filter */

static void au_write_init(MSFilter *f){
	AUWrite *d = ms_new0(AUWrite, 1);
	au_common_init(&d->common);
	d->buffer=ms_bufferizer_new();
	f->data=d;
}	

static void au_write_preprocess(MSFilter *f){
	AUWrite *d = (AUWrite *) f->data;
	audio_unit_open(&d->common,FALSE);
}

static void au_write_process(MSFilter *f){
	AUWrite *d = (AUWrite *) f->data;
	mblk_t *m;
	while((m=ms_queue_get(f->inputs[0]))!=NULL){
		au_write_put(d,m);
	}
}

static void au_write_postprocess(MSFilter *f){
	AUWrite *d = (AUWrite *) f->data;
	audio_unit_close(&d->common);
}

static void au_write_uninit(MSFilter *f){
	AUWrite *d = (AUWrite *) f->data;
	ms_bufferizer_destroy(d->buffer);
	au_common_uninit(&d->common);
	ms_free(d);
}

static int set_rate(MSFilter *f, void *arg){
	AUCommon *d = (AUCommon *) f->data;
	/*the hal audio unit does not accept custom rates*/
	return (d->rate==*(int*)arg) ? 0 : -1;
}

static int get_rate(MSFilter * f, void *arg)
{
	AUCommon *d = (AUCommon *) f->data;
	*((int *) arg) = d->rate;
	return 0;
}

static int set_nchannels(MSFilter *f, void *arg){
	AUCommon *d = (AUCommon *) f->data;
	d->nchannels=*((int*)arg);
	return 0;
}

static int get_nchannels(MSFilter *f, void *arg){
	AUCommon *d = (AUCommon *) f->data;
	*((int*)arg)=d->nchannels;
	return 0;
}

static MSFilterMethod au_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE	, set_rate	},
	{	MS_FILTER_GET_SAMPLE_RATE	, get_rate },
	{	MS_FILTER_SET_NCHANNELS		, set_nchannels	},
	{	MS_FILTER_GET_NCHANNELS		, get_nchannels	},
	{	0				, NULL		}
};

MSFilterDesc ms_au_read_desc={
	.id=MS_CA_READ_ID,
	.name="MSAuRead",
	.text=N_("Sound capture filter for MacOS X Audio Unit"),
	.category=MS_FILTER_OTHER,
	.ninputs=0,
	.noutputs=1,
	.init=au_read_init,
	.preprocess=au_read_preprocess,
	.process=au_read_process,
	.postprocess=au_read_postprocess,
	.uninit=au_read_uninit,
	.methods=au_methods
};

MSFilterDesc ms_au_write_desc={
	.id=MS_CA_WRITE_ID,
	.name="MSAuWrite",
	.text=N_("Sound playback filter for MacOS X Audio Unit"),
	.category=MS_FILTER_OTHER,
	.ninputs=1,
	.noutputs=0,
	.init=au_write_init,
	.preprocess=au_write_preprocess,
	.process=au_write_process,
	.postprocess=au_write_postprocess,
	.uninit=au_write_uninit,
	.methods=au_methods
};

static void set_audio_device_id(AuCard *wc,AUCommon* d, bool_t is_read) {
	CFStringRef devUid = CFStringCreateWithCString(NULL,wc->uidname,CFStringGetSystemEncoding());
	AudioValueTranslation avt;
	UInt32 len;
	OSStatus err;
	avt.mInputData = (CFStringRef *)(&devUid);
	avt.mInputDataSize = sizeof(CFStringRef *);
	avt.mOutputData = &d->dev;
	avt.mOutputDataSize = sizeof(AudioDeviceID);
	len = sizeof(AudioValueTranslation);

	AudioObjectPropertyAddress theAddress = { kAudioHardwarePropertyDeviceForUID,
											  kAudioObjectPropertyScopeGlobal,
											  kAudioObjectPropertyElementMaster };

	err = AudioObjectGetPropertyData(kAudioObjectSystemObject,
												   &theAddress,
												   0,
												   NULL,
												   &len,
												   &avt);

	/*err = AudioHardwareGetProperty(kAudioHardwarePropertyDeviceForUID, &len, &avt);*/
	if (err != kAudioHardwareNoError || d->dev == 0) {
		ms_warning("Unable to query for AudioDeviceID for [%s], using default instead.",wc->uidname);
		len=sizeof(AudioDeviceID);
		theAddress.mSelector=is_read?kAudioHardwarePropertyDefaultInputDevice:kAudioHardwarePropertyDefaultOutputDevice;
		err = AudioObjectGetPropertyData(kAudioObjectSystemObject,
													   &theAddress,
													   0,
													   NULL,
													   &len,
													   &d->dev);
		/*AudioHardwareGetProperty(is_read?kAudioHardwarePropertyDefaultInputDevice:kAudioHardwarePropertyDefaultOutputDevice, &len, &d->dev);*/
		if (err != kAudioHardwareNoError) {
			ms_error("Unable to query for default input AudioDevice");
		}
	}
	CFRelease(devUid);
}
MSFilter *ms_au_read_new(MSSndCard *card){
	MSFilter *f = ms_filter_new_from_desc(&ms_au_read_desc);
	AuCard *wc = (AuCard *) card->data;
	AURead *d = (AURead *) f->data;
	/*d->common.dev = wc->dev;*/
	set_audio_device_id(wc,(struct AUCommon*)d,TRUE);
	d->common.rate=wc->rate;
	return f;
}


MSFilter *ms_au_write_new(MSSndCard *card){
	MSFilter *f=ms_filter_new_from_desc(&ms_au_write_desc);
	AuCard *wc = (AuCard *) card->data;
	AUWrite *d = (AUWrite *) f->data;
	/*d->common.dev = wc->dev;*/
	set_audio_device_id(wc,(struct AUCommon*)d,FALSE);
	d->common.rate=wc->rate;
	return f;
}

MS_FILTER_DESC_EXPORT(ms_au_read_desc)
MS_FILTER_DESC_EXPORT(ms_au_write_desc)



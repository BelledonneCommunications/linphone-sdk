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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

/* this file is specifically distributed under a BSD license */

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

#include "mediastreamer2/mssndcard.h"
#include "mediastreamer2/msfilter.h"

MSFilter *ms_ca_read_new(MSSndCard *card);
MSFilter *ms_ca_write_new(MSSndCard *card);

static float gain_volume_in=1.0;
static float gain_volume_out=1.0;
static bool gain_changed_in = true;
static bool gain_changed_out = true;

typedef struct CAData{
	int dev;
	
	AudioUnit caOutAudioUnit;
	AudioUnit caInAudioUnit;
	AudioStreamBasicDescription caOutASBD;
	AudioStreamBasicDescription caInASBD;
	AURenderCallbackStruct caOutRenderCallback;
	AURenderCallbackStruct caInRenderCallback;
	AudioConverterRef caOutConverter;
	AudioConverterRef caInConverter;
	int rate;
	int bits;
	ms_mutex_t mutex;
	queue_t rq;
	MSBufferizer * bufferizer;
	bool_t read_started;
	bool_t write_started;
	bool_t stereo;
	void *caSourceBuffer;
	AudioBufferList	*fAudioBuffer, *fMSBuffer;
} CAData;


typedef struct CaSndDsCard {
	CFStringRef uidname;
	AudioDeviceID dev;
	int removed;
} CaSndDsCard;

static void cacard_set_level(MSSndCard *card, MSSndCardMixerElem e, int percent)
{
	switch(e){
		case MS_SND_CARD_PLAYBACK:
		case MS_SND_CARD_MASTER:
			gain_volume_out =((float)percent)/100.0f;
			gain_changed_out = true;
			return;
		case MS_SND_CARD_CAPTURE:
			gain_volume_in =((float)percent)/100.0f;
			gain_changed_in = true;
			return;
		default:
			ms_warning("cacard_set_level: unsupported command.");
	}
}

static int cacard_get_level(MSSndCard *card, MSSndCardMixerElem e)
{
	switch(e){
		case MS_SND_CARD_PLAYBACK:
		case MS_SND_CARD_MASTER:
			return (int)(gain_volume_out*100.0f);
		case MS_SND_CARD_CAPTURE:
			return (int)(gain_volume_in*100.0f);
		default:
			ms_warning("cacard_get_level: unsupported command.");
	}
	return -1;
}

static void cacard_set_source(MSSndCard *card, MSSndCardCapture source)
{
}

static void cacard_init(MSSndCard * card)
{
	CaSndDsCard *c = (CaSndDsCard *) ms_new0(CaSndDsCard, 1);
	c->removed = 0;
	card->data = c;
}

static void cacard_uninit(MSSndCard * card)
{
	CaSndDsCard *d = (CaSndDsCard *) card->data;
	if (d->uidname != NULL)
		CFRelease(d->uidname);
	ms_free(d);
}

static void cacard_detect(MSSndCardManager *m);
static MSSndCard *cacard_duplicate(MSSndCard *obj);

MSSndCardDesc ca_card_desc={
	.driver_type="CA",
	.detect=cacard_detect,
	.init=cacard_init,
	.set_level=cacard_set_level,
	.get_level=cacard_get_level,
	.set_capture=cacard_set_source,
	.set_control=NULL,
	.get_control=NULL,
	.create_reader=ms_ca_read_new,
	.create_writer=ms_ca_write_new,
	.uninit=cacard_uninit,
	.duplicate=cacard_duplicate
};

static MSSndCard *cacard_duplicate(MSSndCard * obj)
{
	CaSndDsCard *ca;
	CaSndDsCard *cadup;
	MSSndCard *card = ms_snd_card_new(&ca_card_desc);
	card->name = ms_strdup(obj->name);
	card->data = ms_new0(CaSndDsCard, 1);
	memcpy(card->data, obj->data, sizeof(CaSndDsCard));
	ca = obj->data;
	cadup = card->data;
	cadup->uidname = CFStringCreateCopy(NULL, ca->uidname);
	return card;
}

static MSSndCard *ca_card_new(const char *name, CFStringRef uidname, AudioDeviceID dev, unsigned cap)
{
	MSSndCard *card = ms_snd_card_new(&ca_card_desc);
	CaSndDsCard *d = (CaSndDsCard *) card->data;
	d->uidname = uidname;
	d->dev = dev;
	card->name = ms_strdup(name);
	card->capabilities = cap;
	return card;
}

static void show_format(char *name,
						AudioStreamBasicDescription * deviceFormat)
{
	ms_message("Format for %s", name);
	ms_message("mSampleRate = %g", deviceFormat->mSampleRate);
	char *the4CCString = (char *) &deviceFormat->mFormatID;
	char outName[5];
	outName[0] = the4CCString[0];
	outName[1] = the4CCString[1];
	outName[2] = the4CCString[2];
	outName[3] = the4CCString[3];
	outName[4] = 0;
	ms_message("mFormatID = %s", outName);
	ms_message("mFormatFlags = %08lX", deviceFormat->mFormatFlags);
	ms_message("mBytesPerPacket = %ld", deviceFormat->mBytesPerPacket);
	ms_message("mFramesPerPacket = %ld", deviceFormat->mFramesPerPacket);
	ms_message("mChannelsPerFrame = %ld", deviceFormat->mChannelsPerFrame);
	ms_message("mBytesPerFrame = %ld", deviceFormat->mBytesPerFrame);
	ms_message("mBitsPerChannel = %ld", deviceFormat->mBitsPerChannel);
}

static void cacard_detect(MSSndCardManager * m)
{
#ifndef TARGET_OS_IPHONE
	OSStatus err;
	UInt32 slen;
	int count;
	Boolean writable;
	int i;
	writable = 0;
	slen = 0;
	err =
	AudioHardwareGetPropertyInfo(kAudioHardwarePropertyDevices, &slen,
								 &writable);
	if (err != kAudioHardwareNoError) {
		ms_error("get kAudioHardwarePropertyDevices error %ld", err);
		return;
	}
	AudioDeviceID V[slen / sizeof(AudioDeviceID)];
	err =
	AudioHardwareGetProperty(kAudioHardwarePropertyDevices, &slen, V);
	if (err != kAudioHardwareNoError) {
		ms_error("get kAudioHardwarePropertyDevices error %ld", err);
		return;
	}
	count = slen / sizeof(AudioDeviceID);
	for (i = 0; i < count; i++) {
		char devname_in[256];
		char uidname_in[256];
		char devname_out[256];
		char uidname_out[256];
		int cap = 0;
		
		/* OUTPUT CARDS */
		slen = 256;
		err =
		AudioDeviceGetProperty(V[i], 0, FALSE,
							   kAudioDevicePropertyDeviceName, &slen,
							   devname_out);
		if (err != kAudioHardwareNoError) {
			ms_error("get kAudioDevicePropertyDeviceName error %ld", err);
			continue;
		}
		slen = strlen(devname_out);
		/* trim whitespace */
		while ((slen > 0) && (devname_out[slen - 1] == ' ')) {
			slen--;
		}
		devname_out[slen] = '\0';
		
		err =
		AudioDeviceGetPropertyInfo(V[i], 0, FALSE,
								   kAudioDevicePropertyStreamConfiguration,
								   &slen, &writable);
		if (err != kAudioHardwareNoError) {
			ms_error("get kAudioDevicePropertyDeviceName error %ld", err);
			continue;
		}
		
		AudioBufferList *buflist = ms_malloc(slen);
		if (buflist == NULL) {
			ms_error("alloc AudioBufferList %ld", err);
			continue;
		}
		
		err =
		AudioDeviceGetProperty(V[i], 0, FALSE,
							   kAudioDevicePropertyStreamConfiguration,
							   &slen, buflist);
		if (err != kAudioHardwareNoError) {
			ms_error("get kAudioDevicePropertyDeviceName error %ld", err);
			ms_free(buflist);
			continue;
		}
		
		UInt32 j;
		for (j = 0; j < buflist->mNumberBuffers; j++) {
			if (buflist->mBuffers[j].mNumberChannels > 0) {
				cap = MS_SND_CARD_CAP_PLAYBACK;
				break;
			}
		}
		
		ms_free(buflist);
		
		/* INPUT CARDS */
		slen = 256;
		err =
		AudioDeviceGetProperty(V[i], 0, TRUE,
							   kAudioDevicePropertyDeviceName, &slen,
							   devname_in);
		if (err != kAudioHardwareNoError) {
			ms_error("get kAudioDevicePropertyDeviceName error %ld", err);
			continue;
		}
		slen = strlen(devname_in);
		/* trim whitespace */
		while ((slen > 0) && (devname_in[slen - 1] == ' ')) {
			slen--;
		}
		devname_in[slen] = '\0';
		
		err =
		AudioDeviceGetPropertyInfo(V[i], 0, TRUE,
								   kAudioDevicePropertyStreamConfiguration,
								   &slen, &writable);
		if (err != kAudioHardwareNoError) {
			ms_error("get kAudioDevicePropertyDeviceName error %ld", err);
			continue;
		}
		
		
		err =
		AudioDeviceGetPropertyInfo(V[i], 0, TRUE,
								   kAudioDevicePropertyStreamConfiguration,
								   &slen, &writable);
		if (err != kAudioHardwareNoError) {
			ms_error("get kAudioDevicePropertyDeviceName error %ld", err);
			continue;
		}
		buflist = ms_malloc(slen);
		if (buflist == NULL) {
			ms_error("alloc error %ld", err);
			continue;
		}
		
		err =
		AudioDeviceGetProperty(V[i], 0, TRUE,
							   kAudioDevicePropertyStreamConfiguration,
							   &slen, buflist);
		if (err != kAudioHardwareNoError) {
			ms_error("get kAudioDevicePropertyDeviceName error %ld", err);
			ms_free(buflist);
			continue;
		}
		
		for (j = 0; j < buflist->mNumberBuffers; j++) {
			if (buflist->mBuffers[j].mNumberChannels > 0) {
				cap |= MS_SND_CARD_CAP_CAPTURE;
				break;
			}
		}
		
		ms_free(buflist);
		
		if (cap & MS_SND_CARD_CAP_PLAYBACK) {
			CFStringRef dUID_out;
			dUID_out = NULL;
			slen = sizeof(CFStringRef);
			err =
		    AudioDeviceGetProperty(V[i], 0, false,
								   kAudioDevicePropertyDeviceUID, &slen,
								   &dUID_out);
			if (err != kAudioHardwareNoError) {
				ms_error("get kAudioHardwarePropertyDevices error %ld", err);
				continue;
			}
			CFStringGetCString(dUID_out, uidname_out, 256,
							   CFStringGetSystemEncoding());
			ms_message("CA: devname_out:%s uidname_out:%s", devname_out, uidname_out);
			
			AudioStreamBasicDescription devicewriteFormat;
			slen = sizeof(devicewriteFormat);
			err = AudioDeviceGetProperty(V[i], 0, false,
										 kAudioDevicePropertyStreamFormat,
										 &slen, &devicewriteFormat);
			if (err == kAudioHardwareNoError) {
				show_format("output device", &devicewriteFormat);
			}
			MSSndCard *card = ca_card_new(devname_out, dUID_out, V[i], MS_SND_CARD_CAP_PLAYBACK);
			ms_snd_card_manager_add_card(m, card);
		}
		
		if (cap & MS_SND_CARD_CAP_CAPTURE) {
			CFStringRef dUID_in;
			dUID_in = NULL;
			slen = sizeof(CFStringRef);
			err =
		    AudioDeviceGetProperty(V[i], 0, true,
								   kAudioDevicePropertyDeviceUID, &slen,
								   &dUID_in);
			if (err != kAudioHardwareNoError) {
				ms_error("get kAudioHardwarePropertyDevices error %ld", err);
				continue;
			}
			CFStringGetCString(dUID_in, uidname_in, 256,
							   CFStringGetSystemEncoding());
			ms_message("CA: devname_in:%s uidname_in:%s", devname_in, uidname_in);
			
			AudioStreamBasicDescription devicereadFormat;
			slen = sizeof(devicereadFormat);
			err = AudioDeviceGetProperty(V[i], 0, true,
										 kAudioDevicePropertyStreamFormat,
										 &slen, &devicereadFormat);
			if (err == kAudioHardwareNoError) {
				show_format("input device", &devicereadFormat);
			}
			MSSndCard *card = ca_card_new(devname_in, dUID_in, V[i], MS_SND_CARD_CAP_CAPTURE);
			ms_snd_card_manager_add_card(m, card);
		}
	}
#else
	AudioStreamBasicDescription deviceFormat;
	memset(&deviceFormat, 0, sizeof(AudioStreamBasicDescription));
	
	MSSndCard *card = ca_card_new("AudioUnit Device", NULL, 0 /*?*/, MS_SND_CARD_CAP_PLAYBACK|MS_SND_CARD_CAP_CAPTURE);
	ms_snd_card_manager_add_card(m, card);
#endif
}


// Convenience function to dispose of our audio buffers
void DestroyAudioBufferList(AudioBufferList* list)
{
	UInt32						i;
	
	if(list) {
		for(i = 0; i < list->mNumberBuffers; i++) {
			if(list->mBuffers[i].mData)
				free(list->mBuffers[i].mData);
		}
		free(list);
	}
}

// Convenience function to allocate our audio buffers
AudioBufferList *AllocateAudioBufferList(UInt32 numChannels, UInt32 size)
{
	AudioBufferList*			list;
	UInt32						i;
	
	list = (AudioBufferList*)calloc(1, sizeof(AudioBufferList) + numChannels * sizeof(AudioBuffer));
	if(list == NULL)
		return NULL;
	
	list->mNumberBuffers = numChannels;
	for(i = 0; i < numChannels; ++i) {
		list->mBuffers[i].mNumberChannels = 1;
		list->mBuffers[i].mDataByteSize = size;
		list->mBuffers[i].mData = malloc(size);
		if(list->mBuffers[i].mData == NULL) {
			DestroyAudioBufferList(list);
			return NULL;
		}
	}
	return list;
}

OSStatus writeACInputProc (
						   AudioConverterRef inAudioConverter,
						   UInt32 *ioNumberDataPackets,
						   AudioBufferList *ioData,
						   AudioStreamPacketDescription **outDataPacketDescription,
						   void* inUserData)
{
    OSStatus    err = noErr;
	CAData *d=(CAData*)inUserData;
	UInt32 packetSize = (d->bits / 8) * (d->stereo ? 2 : 1);
	
	if(*ioNumberDataPackets) {
		if(d->caSourceBuffer != NULL) {
			free(d->caSourceBuffer);
			d->caSourceBuffer = NULL;
		}
		
		d->caSourceBuffer = (void *) calloc (1, *ioNumberDataPackets * packetSize);
		
		ioData->mBuffers[0].mData = d->caSourceBuffer;			// tell the Audio Converter where it's source data is
		
		ms_mutex_lock(&d->mutex);
		int readsize = ms_bufferizer_read(d->bufferizer,d->caSourceBuffer,*ioNumberDataPackets * packetSize);
		ms_mutex_unlock(&d->mutex);
		if(readsize != *ioNumberDataPackets * packetSize) {
			memset(d->caSourceBuffer, 0, *ioNumberDataPackets * packetSize);
			ioData->mBuffers[0].mDataByteSize = *ioNumberDataPackets * packetSize;		// tell the Audio Converter how much source data there is
		} else {
			ioData->mBuffers[0].mDataByteSize = readsize;		// tell the Audio Converter how much source data there is
		}
	}
	
	return err;
}

OSStatus readACInputProc (AudioConverterRef inAudioConverter,
						  UInt32* ioNumberDataPackets,
						  AudioBufferList* ioData,
						  AudioStreamPacketDescription** ioASPD,
						  void* inUserData)
{
	CAData *d=(CAData*)inUserData;
	AudioBufferList* l_inputABL = d->fAudioBuffer;
	UInt32 totalInputBufferSizeBytes = ((*ioNumberDataPackets) * sizeof (float));
	int counter = d->caInASBD.mChannelsPerFrame;
	ioData->mNumberBuffers = d->caInASBD.mChannelsPerFrame;
	
	while (--counter >= 0)  {
		AudioBuffer* l_ioD_AB = &(ioData->mBuffers[counter]);
		l_ioD_AB->mNumberChannels = 1;
		l_ioD_AB->mData = (float*)(l_inputABL->mBuffers[counter].mData);
		l_ioD_AB->mDataByteSize = totalInputBufferSizeBytes;
	}
	
	return (noErr);
}

OSStatus readRenderProc(void *inRefCon, 
						AudioUnitRenderActionFlags *inActionFlags,
						const AudioTimeStamp *inTimeStamp, 
						UInt32 inBusNumber,
						UInt32 inNumFrames, 
						AudioBufferList *ioData)
{
	CAData *d=(CAData*)inRefCon;
	OSStatus	err = noErr;
	
	err = AudioUnitRender(d->caInAudioUnit, inActionFlags, inTimeStamp, inBusNumber,
						  inNumFrames, d->fAudioBuffer);
	if(err != noErr)
	{
		ms_error("AudioUnitRender %d size = %d", err, d->fAudioBuffer->mBuffers[0].mDataByteSize);
		return err;
	}
	
	UInt32 AvailableOutputBytes = inNumFrames * sizeof (float) * d->caInASBD.mChannelsPerFrame;
    UInt32 propertySize = sizeof (AvailableOutputBytes);
    err = AudioConverterGetProperty (d->caInConverter,
									 kAudioConverterPropertyCalculateOutputBufferSize,
									 &propertySize,
									 &AvailableOutputBytes);
	
	if(err != noErr)
	{
		ms_error("AudioConverterGetProperty kAudioConverterPropertyCalculateOutputBufferSize %d", err);
		return err;
	}
	
	if (AvailableOutputBytes>d->fMSBuffer->mBuffers[0].mDataByteSize)
	{	
		DestroyAudioBufferList(d->fMSBuffer);
		d->fMSBuffer = AllocateAudioBufferList(d->stereo ? 2 : 1,
											   AvailableOutputBytes);
	}
	
	UInt32 ActualOutputFrames = AvailableOutputBytes / ((d->bits / 8) * 1) / d->caInASBD.mChannelsPerFrame;
	err = AudioConverterFillComplexBuffer (d->caInConverter,
										   (AudioConverterComplexInputDataProc)(readACInputProc),
										   inRefCon,
										   &ActualOutputFrames,
										   d->fMSBuffer,
										   NULL);
	if(err != noErr)
	{
		ms_error("readRenderProc:AudioConverterFillComplexBuffer %d", err);
		return err;
	}
	
	mblk_t *rm=NULL;
	rm=allocb(ActualOutputFrames*2,0);
	memcpy(rm->b_wptr, d->fMSBuffer->mBuffers[0].mData, ActualOutputFrames*2);
	rm->b_wptr+=ActualOutputFrames*2;
	
	if (gain_volume_in != 1.0f)
	{
		int16_t *ptr=(int16_t *)rm->b_rptr;
		for (;ptr<(int16_t *)rm->b_wptr;ptr++)
		{
			*ptr=(int16_t)(((float)(*ptr))*gain_volume_in);
		}
	}
	
	ms_mutex_lock(&d->mutex);
	putq(&d->rq,rm);
	ms_mutex_unlock(&d->mutex);
	rm=NULL;
	
	return err;
}

OSStatus writeRenderProc(void *inRefCon, 
						 AudioUnitRenderActionFlags *inActionFlags,
						 const AudioTimeStamp *inTimeStamp, 
						 UInt32 inBusNumber,
						 UInt32 inNumFrames, 
						 AudioBufferList *ioData)
{
    OSStatus err= noErr;
    void *inInputDataProcUserData=NULL;
	CAData *d=(CAData*)inRefCon;
	if (gain_changed_out == true)
	{
		err = AudioUnitSetParameter(d->caOutAudioUnit, kAudioUnitParameterUnit_LinearGain,
									   kAudioUnitScope_Global, 0, (Float32)gain_volume_out, 0);
		if(err != noErr)
		{
			ms_error("failed to set output volume %i", err);
		}
	    gain_changed_out = false;
		err= noErr;
	}
	
	
	if(d->write_started != FALSE) {
		AudioStreamPacketDescription* outPacketDescription = NULL;
		err = AudioConverterFillComplexBuffer(d->caOutConverter, writeACInputProc, inRefCon,
											  &inNumFrames, ioData, outPacketDescription);
		if(err != noErr)
			ms_error("writeRenderProc:AudioConverterFillComplexBuffer err %08x %d", err, ioData->mNumberBuffers);
	}
    return err;
}

static int ca_open_r(CAData *d){
	OSStatus result;
	UInt32 param;
	AudioDeviceID fInputDeviceID;
	
	ComponentDescription desc;  
	Component comp;
	
	// Get Default Input audio unit
	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_HALOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;
	
	comp = FindNextComponent(NULL, &desc);
	if (comp == NULL)
	{
		ms_message("Cannot find audio component");
		return -1;
	}
	
	result = OpenAComponent(comp, &d->caInAudioUnit);
	if(result != noErr)
	{
		ms_message("Cannot open audio component %x", result);
		return -1;
	}
	
	param = 1;
	result = AudioUnitSetProperty(d->caInAudioUnit,
								  kAudioOutputUnitProperty_EnableIO,
								  kAudioUnitScope_Input,
								  1,
								  &param,
								  sizeof(UInt32));
	ms_message("AudioUnitSetProperty %i %x", result, result);
	
	param = 0;
	result = AudioUnitSetProperty(d->caInAudioUnit,
								  kAudioOutputUnitProperty_EnableIO,
								  kAudioUnitScope_Output,
								  0,
								  &param,
								  sizeof(UInt32));
	
	ms_message("AudioUnitSetProperty %i %x", result, result);
	
	// Set the current device to the default input unit.
	result = AudioUnitSetProperty(d->caInAudioUnit,
								  kAudioOutputUnitProperty_CurrentDevice,
								  kAudioUnitScope_Global,
								  0,
								  &d->dev,
								  sizeof(AudioDeviceID));
	ms_message("AudioUnitSetProperty %i %x", result, result);
	
	UInt32 asbdsize = sizeof(AudioStreamBasicDescription);
	memset((char *)&d->caInASBD, 0, asbdsize);
	
	result = AudioUnitGetProperty (d->caInAudioUnit,
								   kAudioUnitProperty_StreamFormat,
								   kAudioUnitScope_Input,
								   1,
								   &d->caInASBD,
								   &asbdsize);
	
	ms_message("AudioUnitGetProperty %i %x", result, result);
	
	
	if (d->caInASBD.mChannelsPerFrame>1)
	{
		d->caInASBD.mBytesPerFrame = d->caInASBD.mBytesPerFrame / d->caInASBD.mChannelsPerFrame;
		d->caInASBD.mBytesPerPacket = d->caInASBD.mBytesPerPacket / d->caInASBD.mChannelsPerFrame;		
		d->caInASBD.mChannelsPerFrame = 1;
	}
	
	result = AudioUnitSetProperty(d->caInAudioUnit,
								  kAudioUnitProperty_StreamFormat,
								  kAudioUnitScope_Output,
								  1,
								  &d->caInASBD,
								  sizeof(AudioStreamBasicDescription));
	ms_message("AudioUnitSetProperty %i %x", result, result);
	
	
	d->caSourceBuffer=NULL;
	
	// Get the number of frames in the IO buffer(s)
	param = sizeof(UInt32);
	UInt32 fAudioSamples;
	result = AudioUnitGetProperty(d->caInAudioUnit,
								  kAudioDevicePropertyBufferFrameSize,
								  kAudioUnitScope_Input,
								  1,
								  &fAudioSamples,
								  &param);
	if(result != noErr)
	{
		ms_error("failed to get audio sample size");
		return -1;
	}
	
	result = AudioUnitInitialize(d->caInAudioUnit);
	if(result != noErr)
	{
		ms_error("failed to AudioUnitInitialize input %i", result);
		return -1;
	}
	
	// Allocate our low device audio buffers
	d->fAudioBuffer = AllocateAudioBufferList(d->caInASBD.mChannelsPerFrame,
											  fAudioSamples * d->caInASBD.mBytesPerFrame * 2);
	if(d->fAudioBuffer == NULL)
	{
		ms_error("failed to allocate buffers fAudioBuffer");
		return -1;
	}
	// Allocate our low device audio buffers
	d->fMSBuffer = AllocateAudioBufferList( d->stereo ? 2 : 1,
										   fAudioSamples * ((d->bits / 8)*(d->stereo ? 2 : 1)) *2);
	if(d->fMSBuffer == NULL)
	{
		ms_error("failed to allocate buffers fMSBuffer");
		return -1;
	}
	
	return 0;
}

static int ca_open_w(CAData *d){
	OSStatus result;
	UInt32 param;
	AudioDeviceID fInputDeviceID;
	
	ComponentDescription desc;  
	Component comp;
	
	// Get Default Input audio unit
	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_HALOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;
	
	comp = FindNextComponent(NULL, &desc);
	if (comp == NULL)
	{
		ms_message("Cannot find audio component");
		return -1;
	}
	
	result = OpenAComponent(comp, &d->caOutAudioUnit);
	if(result != noErr)
	{
		ms_message("Cannot open audio component %x", result);
		return -1;
	}
	
	param = 1;
	result = AudioUnitSetProperty(d->caOutAudioUnit,
								  kAudioOutputUnitProperty_EnableIO,
								  kAudioUnitScope_Output,
								  0,
								  &param,
								  sizeof(UInt32));
	ms_message("AudioUnitSetProperty %i %x", result, result);
	
	param = 0;
	result = AudioUnitSetProperty(d->caOutAudioUnit,
								  kAudioOutputUnitProperty_EnableIO,
								  kAudioUnitScope_Input,
								  1,
								  &param,
								  sizeof(UInt32));
	ms_message("AudioUnitSetProperty %i %x", result, result);
	
	// Set the current device to the default input unit.
	result = AudioUnitSetProperty(d->caOutAudioUnit,
								  kAudioOutputUnitProperty_CurrentDevice,
								  kAudioUnitScope_Global,
								  0,
								  &d->dev,
								  sizeof(AudioDeviceID));
	if (result == kAudioUnitErr_InvalidPropertyValue)
		ms_message("AudioUnitSetProperty kAudioUnitErr_InvalidPropertyValue");
	else
		ms_message("AudioUnitSetProperty %i %x", result, result);
	
	UInt32 asbdsize = sizeof(AudioStreamBasicDescription);
	memset((char *)&d->caOutASBD, 0, asbdsize);
	
	// Setup Output audio unit
	result = AudioUnitGetProperty (d->caOutAudioUnit,
								   kAudioUnitProperty_StreamFormat,
								   kAudioUnitScope_Output,
								   0,
								   &d->caOutASBD,
								   &asbdsize);
	ms_message("AudioUnitGetProperty %i %x", result, result);
	result = AudioUnitSetProperty (d->caOutAudioUnit,
								   kAudioUnitProperty_StreamFormat,
								   kAudioUnitScope_Input,
								   0,
								   &d->caOutASBD,
								   asbdsize);
	ms_message("AudioUnitSetProperty %i %x", result, result);
	
	d->caSourceBuffer=NULL;
	
	result = AudioUnitInitialize(d->caOutAudioUnit);
	if(result != noErr)
	{
		ms_error("failed to AudioUnitInitialize output %i", result);
		return -1;
	}
	return 0;
}

static void ca_start_r(CAData *d){
	OSStatus err= noErr;
	
	if (d->read_started==FALSE){
		AudioStreamBasicDescription outASBD;
		int i;
		
		i = ca_open_r(d);
		if (i<0)
			return;
		
		outASBD = d->caInASBD;
		outASBD.mSampleRate = d->rate;
		outASBD.mFormatID = kAudioFormatLinearPCM;
		outASBD.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
		if (htonl(0x1234) == 0x1234)
		  outASBD.mFormatFlags |= kLinearPCMFormatFlagIsBigEndian;
		outASBD.mChannelsPerFrame = d->stereo ? 2 : 1;
		outASBD.mBytesPerPacket = (d->bits / 8) * outASBD.mChannelsPerFrame;
		outASBD.mBytesPerFrame = (d->bits / 8) * outASBD.mChannelsPerFrame;
		outASBD.mFramesPerPacket = 1;
		outASBD.mBitsPerChannel = d->bits;

		err = AudioConverterNew( &d->caInASBD, &outASBD, &d->caInConverter);
		if(err != noErr)
			ms_error("AudioConverterNew %x %d", err, outASBD.mBytesPerFrame);
		else
			CAShow(d->caInConverter);

		d->caInRenderCallback.inputProc = readRenderProc;
		d->caInRenderCallback.inputProcRefCon = d;
		err = AudioUnitSetProperty(d->caInAudioUnit,
						kAudioOutputUnitProperty_SetInputCallback,
						kAudioUnitScope_Global,
						0,
						&d->caInRenderCallback,
						sizeof(AURenderCallbackStruct));

		if(AudioOutputUnitStart(d->caInAudioUnit) == noErr)
			d->read_started = TRUE;
	}
}

static void ca_stop_r(CAData *d){
	OSErr err;
	if(d->read_started == TRUE) {
		if(AudioOutputUnitStop(d->caInAudioUnit) == noErr)
			d->read_started=FALSE;
	}
	if (d->caInConverter!=NULL)
	{
		AudioConverterDispose(d->caInConverter);
		d->caInConverter=NULL;
	}
	if (d->caInAudioUnit!=NULL)
	{
		AudioUnitUninitialize(d->caInAudioUnit);
		d->caInAudioUnit=NULL;
	}
	if (d->fAudioBuffer)
		DestroyAudioBufferList(d->fAudioBuffer);
	d->fAudioBuffer=NULL;
	if (d->fMSBuffer)
		DestroyAudioBufferList(d->fMSBuffer);
	d->fMSBuffer=NULL;
}

static void ca_start_w(CAData *d){
	OSStatus err= noErr;

	if (d->write_started==FALSE){
		AudioStreamBasicDescription inASBD;
		int i;
		
		i = ca_open_w(d);
		if (i<0)
			return;

		inASBD = d->caOutASBD;
		inASBD.mSampleRate = d->rate;
		inASBD.mFormatID = kAudioFormatLinearPCM;
		inASBD.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
		if (htonl(0x1234) == 0x1234)
		  inASBD.mFormatFlags |= kLinearPCMFormatFlagIsBigEndian;
		inASBD.mChannelsPerFrame = d->stereo ? 2 : 1;
		inASBD.mBytesPerPacket = (d->bits / 8) * inASBD.mChannelsPerFrame;
		inASBD.mBytesPerFrame = (d->bits / 8) * inASBD.mChannelsPerFrame;
		inASBD.mFramesPerPacket = 1;
		inASBD.mBitsPerChannel = d->bits;


		err = AudioConverterNew( &inASBD, &d->caOutASBD, &d->caOutConverter);
		if(err != noErr)
			ms_error("AudioConverterNew %x %d", err, inASBD.mBytesPerFrame);
		else
			CAShow(d->caOutConverter);

		if (inASBD.mChannelsPerFrame == 1 && d->caOutASBD.mChannelsPerFrame == 2)
		{
			if (d->caOutConverter)
			{
				// This should be as large as the number of output channels,
				// each element specifies which input channel's data is routed to that output channel
				SInt32 channelMap[] = { 0, 0 };
				err = AudioConverterSetProperty(d->caOutConverter, kAudioConverterChannelMap, 2*sizeof(SInt32), channelMap);
			}
		}

		memset((char*)&d->caOutRenderCallback, 0, sizeof(AURenderCallbackStruct));
		d->caOutRenderCallback.inputProc = writeRenderProc;
		d->caOutRenderCallback.inputProcRefCon = d;
		err = AudioUnitSetProperty (d->caOutAudioUnit, 
                            kAudioUnitProperty_SetRenderCallback, 
                            kAudioUnitScope_Input, 
                            0,
                            &d->caOutRenderCallback, 
                            sizeof(AURenderCallbackStruct));
		if(err != noErr)
			ms_error("AudioUnitSetProperty %x", err);

		if(err == noErr) {
			if(AudioOutputUnitStart(d->caOutAudioUnit) == noErr)
				d->write_started=TRUE;
		}
	}
}

static void ca_stop_w(CAData *d){
	OSErr err;
	if(d->write_started == TRUE) {
		if(AudioOutputUnitStop(d->caOutAudioUnit) == noErr)
			d->write_started=FALSE;
	}
	if (d->caOutConverter!=NULL)
	{
		AudioConverterDispose(d->caOutConverter);
		d->caOutConverter=NULL;
	}
	if (d->caOutAudioUnit!=NULL)
	{
		AudioUnitUninitialize(d->caOutAudioUnit);
		d->caOutAudioUnit=NULL;
	}	
}


static mblk_t *ca_get(CAData *d){
	mblk_t *m;
	ms_mutex_lock(&d->mutex);
	m=getq(&d->rq);
	ms_mutex_unlock(&d->mutex);
	return m;
}

static void ca_put(CAData *d, mblk_t *m){
	ms_mutex_lock(&d->mutex);
	ms_bufferizer_put(d->bufferizer,m);
	ms_mutex_unlock(&d->mutex);
}


static void ca_init(MSFilter *f){
	CAData *d = ms_new0(CAData, 1);
	d->read_started=FALSE;
	d->write_started=FALSE;
	d->bits=16;
	d->rate=8000;
	d->stereo=FALSE;
	qinit(&d->rq);
	d->bufferizer=ms_bufferizer_new();
	ms_mutex_init(&d->mutex,NULL);
	f->data=d;
}	

static void ca_uninit(MSFilter *f){
	CAData *d = (CAData *) f->data;
	ms_bufferizer_destroy(d->bufferizer);
	flushq(&d->rq,0);
	ms_mutex_destroy(&d->mutex);
	ms_free(d);
}

static void ca_read_preprocess(MSFilter *f){
	CAData *d = (CAData *) f->data;
	ca_start_r(d);
}

static void ca_read_postprocess(MSFilter *f){
	CAData *d = (CAData *) f->data;
	ca_stop_r(d);
}

static void ca_read_process(MSFilter *f){
	CAData *d = (CAData *) f->data;
	mblk_t *m;
	while((m=ca_get(d))!=NULL){
		ms_queue_put(f->outputs[0],m);
	}
}

static void ca_write_preprocess(MSFilter *f){
	CAData *d = (CAData *) f->data;
	ca_start_w(d);
}

static void ca_write_postprocess(MSFilter *f){
	CAData *d = (CAData *) f->data;
	ca_stop_w(d);
}

static void ca_write_process(MSFilter *f){
	CAData *d = (CAData *) f->data;
	mblk_t *m;
	while((m=ms_queue_get(f->inputs[0]))!=NULL){
		ca_put(d,m);
	}
}

static int set_rate(MSFilter *f, void *arg){
	CAData *d = (CAData *) f->data;
	d->rate = *((int *) arg);
	return 0;
}

static int get_rate(MSFilter * f, void *arg)
{
	CAData *d = (CAData *) f->data;
	*((int *) arg) = d->rate;
	return 0;
}

static int set_nchannels(MSFilter *f, void *arg){
	CAData *d = (CAData *) f->data;
	d->stereo=(*((int*)arg)==2);
	return 0;
}

static MSFilterMethod ca_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE	, set_rate	},
	{	MS_FILTER_GET_SAMPLE_RATE	, get_rate },
	{	MS_FILTER_SET_NCHANNELS		, set_nchannels	},
	{	0				, NULL		}
};

MSFilterDesc ca_read_desc={
	.id=MS_CA_READ_ID,
	.name="MSCARead",
	.text=N_("Sound capture filter for MacOS X Core Audio drivers"),
	.category=MS_FILTER_OTHER,
	.ninputs=0,
	.noutputs=1,
	.init=ca_init,
	.preprocess=ca_read_preprocess,
	.process=ca_read_process,
	.postprocess=ca_read_postprocess,
	.uninit=ca_uninit,
	.methods=ca_methods
};

MSFilterDesc ca_write_desc={
	.id=MS_CA_WRITE_ID,
	.name="MSCAWrite",
	.text=N_("Sound playback filter for MacOS X Core Audio drivers"),
	.category=MS_FILTER_OTHER,
	.ninputs=1,
	.noutputs=0,
	.init=ca_init,
	.preprocess=ca_write_preprocess,
	.process=ca_write_process,
	.postprocess=ca_write_postprocess,
	.uninit=ca_uninit,
	.methods=ca_methods
};

MSFilter *ms_ca_read_new(MSSndCard *card){
	MSFilter *f = ms_filter_new_from_desc(&ca_read_desc);
	CaSndDsCard *wc = (CaSndDsCard *) card->data;
	CAData *d = (CAData *) f->data;
	d->dev = wc->dev;
	return f;
}


MSFilter *ms_ca_write_new(MSSndCard *card){
	MSFilter *f=ms_filter_new_from_desc(&ca_write_desc);
	CaSndDsCard *wc = (CaSndDsCard *) card->data;
	CAData *d = (CAData *) f->data;
	d->dev = wc->dev;
	return f;
}

MS_FILTER_DESC_EXPORT(ca_read_desc)
MS_FILTER_DESC_EXPORT(ca_write_desc)

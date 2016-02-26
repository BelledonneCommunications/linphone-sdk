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
* Copyright (C) 2008  Hiroki Mori (himori@users.sourceforge.net)
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

/*
 This is MacOS X Audio Queue Service support code for mediastreamer2.
 Audio Queue Support MacOS X 10.5 or later.
 http://developer.apple.com/documentation/MusicAudio/Conceptual/AudioQueueProgrammingGuide/
 */

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif

#include <AudioToolbox/AudioToolbox.h>
#if !TARGET_OS_IPHONE
#include <CoreAudio/AudioHardware.h>
#endif

#include "mediastreamer2/mssndcard.h"
#include "mediastreamer2/msfilter.h"

MSFilter *ms_aq_read_new(MSSndCard * card);
MSFilter *ms_aq_write_new(MSSndCard * card);

#define kSecondsPerBuffer		0.02	/*0.04 */
#define kNumberAudioOutDataBuffers	4
#define kNumberAudioInDataBuffers	4

static float gain_volume_in=1.0;
static float gain_volume_out=1.0;
static bool gain_changed_in = true;
static bool gain_changed_out = true;


const char* aq_format_error(OSStatus status) {
	switch (status) {
		case kAudioQueueErr_InvalidBuffer: return "kAudioQueueErr_InvalidBuffer";
		case kAudioQueueErr_BufferEmpty: return"kAudioQueueErr_BufferEmpty";
		case kAudioQueueErr_DisposalPending: return"kAudioQueueErr_DisposalPending";
		case kAudioQueueErr_InvalidProperty: return"kAudioQueueErr_InvalidProperty";
		case kAudioQueueErr_InvalidPropertySize: return"kAudioQueueErr_InvalidPropertySize";
		case kAudioQueueErr_InvalidParameter: return"kAudioQueueErr_InvalidParameter";
		case kAudioQueueErr_CannotStart: return"kAudioQueueErr_CannotStart";
		case kAudioQueueErr_InvalidDevice: return"kAudioQueueErr_InvalidDevice";
		case kAudioQueueErr_BufferInQueue: return"kAudioQueueErr_BufferInQueue";
		case kAudioQueueErr_InvalidRunState: return"kAudioQueueErr_InvalidRunState";
		case kAudioQueueErr_InvalidQueueType: return"kAudioQueueErr_InvalidQueueType";
		case kAudioQueueErr_Permissions: return"kAudioQueueErr_Permissions";
		case kAudioQueueErr_InvalidPropertyValue: return"kAudioQueueErr_InvalidPropertyValue";
		case kAudioQueueErr_PrimeTimedOut: return"kAudioQueueErr_PrimeTimedOut";
		case kAudioQueueErr_CodecNotFound: return"kAudioQueueErr_CodecNotFound";
		case kAudioQueueErr_InvalidCodecAccess: return"kAudioQueueErr_InvalidCodecAccess";
		case kAudioQueueErr_QueueInvalidated: return"kAudioQueueErr_QueueInvalidated";
		case kAudioQueueErr_RecordUnderrun: return"kAudioQueueErr_RecordUnderrun";
		case kAudioQueueErr_EnqueueDuringReset: return"kAudioQueueErr_EnqueueDuringReset";
		case kAudioQueueErr_InvalidOfflineMode: return"kAudioQueueErr_InvalidOfflineMode";
		default:
			return "unkown error code";
	}
}
#ifdef __ios
#define CFStringRef void *
#define CFRelease(A) {}
#define CFStringGetCString(A, B, LEN, encoding)  {}
#define CFStringCreateCopy(A, B) NULL
#define check_aqresult(aq,method) \
if (aq!=0) ms_error("AudioQueue error for %s: ret=%s",method,aq_format_error(aq))
#endif

typedef struct AQData {
	CFStringRef uidname;
	AudioStreamBasicDescription devicereadFormat;
	AudioStreamBasicDescription devicewriteFormat;

	int rate;
	int bits;
	bool_t stereo;

	ms_mutex_t mutex;
	queue_t rq;
	bool_t read_started;
	bool_t write_started;
#if 0
	AudioConverterRef readAudioConverter;
#endif
	AudioQueueRef readQueue;
	AudioStreamBasicDescription readAudioFormat;
	UInt32 readBufferByteSize;

#if 0
	AudioConverterRef writeAudioConverter;
#endif
	AudioQueueRef writeQueue;
	AudioStreamBasicDescription writeAudioFormat;
	UInt32 writeBufferByteSize;
	AudioQueueBufferRef writeBuffers[kNumberAudioOutDataBuffers];
	int curWriteBuffer;
	MSBufferizer *bufferizer;
} AQData;



/*
 mediastreamer2 function
 */

typedef struct AqSndDsCard {
	CFStringRef uidname;
	AudioStreamBasicDescription devicereadFormat;
	AudioStreamBasicDescription devicewriteFormat;
	int removed;
} AqSndDsCard;

static void aqcard_set_level(MSSndCard * card, MSSndCardMixerElem e,
							 int percent)
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
			ms_warning("aqcard_set_level: unsupported command.");
	}
}

static int aqcard_get_level(MSSndCard * card, MSSndCardMixerElem e)
{
	switch(e){
		case MS_SND_CARD_PLAYBACK:
		case MS_SND_CARD_MASTER:
		  return (int)(gain_volume_out*100.0f);
		case MS_SND_CARD_CAPTURE:
		  return (int)(gain_volume_in*100.0f);
		default:
			ms_warning("aqcard_get_level: unsupported command.");
	}
	return -1;
}

static void aqcard_set_source(MSSndCard * card, MSSndCardCapture source)
{
}

static void aqcard_init(MSSndCard * card)
{
	AqSndDsCard *c = (AqSndDsCard *) ms_new0(AqSndDsCard, 1);
	c->removed = 0;
	card->data = c;
}

static void aqcard_uninit(MSSndCard * card)
{
	AqSndDsCard *d = (AqSndDsCard *) card->data;
	if (d->uidname != NULL)
		CFRelease(d->uidname);
	ms_free(d);
}

static void aqcard_detect(MSSndCardManager * m);
static MSSndCard *aqcard_duplicate(MSSndCard * obj);

MSSndCardDesc aq_card_desc = {
	.driver_type = "AQ",
	.detect = aqcard_detect,
	.init = aqcard_init,
	.set_level = aqcard_set_level,
	.get_level = aqcard_get_level,
	.set_capture = aqcard_set_source,
	.set_control = NULL,
	.get_control = NULL,
	.create_reader = ms_aq_read_new,
	.create_writer = ms_aq_write_new,
	.uninit = aqcard_uninit,
	.duplicate = aqcard_duplicate
};

static MSSndCard *aqcard_duplicate(MSSndCard * obj)
{
	AqSndDsCard *ca;
	AqSndDsCard *cadup;
	MSSndCard *card = ms_snd_card_new(&aq_card_desc);
	card->name = ms_strdup(obj->name);
	card->data = ms_new0(AqSndDsCard, 1);
	memcpy(card->data, obj->data, sizeof(AqSndDsCard));
	ca = obj->data;
	cadup = card->data;
	cadup->uidname = CFStringCreateCopy(NULL, ca->uidname);
	return card;
}

static MSSndCard *aq_card_new(const char *name, CFStringRef uidname,
							  AudioStreamBasicDescription *
							  devicereadFormat,
							  AudioStreamBasicDescription *
							  devicewriteFormat, unsigned cap)
{
	MSSndCard *card = ms_snd_card_new(&aq_card_desc);
	AqSndDsCard *d = (AqSndDsCard *) card->data;
	d->uidname = uidname;
	if (devicereadFormat!=NULL)
	  memcpy(&d->devicereadFormat, devicereadFormat,
		 sizeof(AudioStreamBasicDescription));
	if (devicewriteFormat!=NULL)
	  memcpy(&d->devicewriteFormat, devicewriteFormat,
		 sizeof(AudioStreamBasicDescription));
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
	ms_message("mFormatFlags = %08lX", (long)deviceFormat->mFormatFlags);
	ms_message("mBytesPerPacket = %ld", (long)deviceFormat->mBytesPerPacket);
	ms_message("mFramesPerPacket = %ld", (long)deviceFormat->mFramesPerPacket);
	ms_message("mChannelsPerFrame = %ld", (long)deviceFormat->mChannelsPerFrame);
	ms_message("mBytesPerFrame = %ld", (long)deviceFormat->mBytesPerFrame);
	ms_message("mBitsPerChannel = %ld", (long)deviceFormat->mBitsPerChannel);
}

static void aqcard_detect(MSSndCardManager * m)
{
#ifdef __ios
	AudioStreamBasicDescription deviceFormat;
	memset(&deviceFormat, 0, sizeof(AudioStreamBasicDescription));
	
	MSSndCard *card = aq_card_new("Audio Queue Device", NULL, &deviceFormat,
								  &deviceFormat, MS_SND_CARD_CAP_PLAYBACK/*|MS_SND_CARD_CAP_CAPTURE*/);
	ms_snd_card_manager_add_card(m, card);
#else
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
		ms_error("get kAudioHardwarePropertyDevices error %ld", (long)err);
		return;
	}
	AudioDeviceID V[slen / sizeof(AudioDeviceID)];
	err =
		AudioHardwareGetProperty(kAudioHardwarePropertyDevices, &slen, V);
	if (err != kAudioHardwareNoError) {
		ms_error("get kAudioHardwarePropertyDevices error %ld", (long)err);
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
			ms_error("get kAudioDevicePropertyDeviceName error %ld", (long)err);
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
			ms_error("get kAudioDevicePropertyDeviceName error %ld", (long)err);
			continue;
		}

		AudioBufferList *buflist = ms_malloc(slen);
		if (buflist == NULL) {
			ms_error("alloc AudioBufferList %ld", (long)err);
			continue;
		}

		err =
			AudioDeviceGetProperty(V[i], 0, FALSE,
								   kAudioDevicePropertyStreamConfiguration,
								   &slen, buflist);
		if (err != kAudioHardwareNoError) {
			ms_error("get kAudioDevicePropertyDeviceName error %ld", (long)err);
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
			ms_error("get kAudioDevicePropertyDeviceName error %ld", (long)err);
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
			ms_error("get kAudioDevicePropertyDeviceName error %ld", (long)err);
			continue;
		}


		err =
			AudioDeviceGetPropertyInfo(V[i], 0, TRUE,
									   kAudioDevicePropertyStreamConfiguration,
									   &slen, &writable);
		if (err != kAudioHardwareNoError) {
			ms_error("get kAudioDevicePropertyDeviceName error %ld", (long)err);
			continue;
		}
		buflist = ms_malloc(slen);
		if (buflist == NULL) {
			ms_error("alloc error %ld", (long)err);
			continue;
		}

		err =
			AudioDeviceGetProperty(V[i], 0, TRUE,
								   kAudioDevicePropertyStreamConfiguration,
								   &slen, buflist);
		if (err != kAudioHardwareNoError) {
			ms_error("get kAudioDevicePropertyDeviceName error %ld", (long)err);
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
		    ms_error("get kAudioHardwarePropertyDevices error %ld", (long)err);
		    continue;
		  }
		  CFStringGetCString(dUID_out, uidname_out, 256,
				     CFStringGetSystemEncoding());
		  ms_message("AQ: devname_out:%s uidname_out:%s", devname_out, uidname_out);

		  AudioStreamBasicDescription devicewriteFormat;
		  slen = sizeof(devicewriteFormat);
		  err = AudioDeviceGetProperty(V[i], 0, false,
					       kAudioDevicePropertyStreamFormat,
					       &slen, &devicewriteFormat);
		  if (err == kAudioHardwareNoError) {
		    show_format("output device", &devicewriteFormat);
		  }
		  MSSndCard *card = aq_card_new(devname_out, dUID_out, NULL,
						&devicewriteFormat, MS_SND_CARD_CAP_PLAYBACK);
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
		    ms_error("get kAudioHardwarePropertyDevices error %ld", (long)err);
		    continue;
		  }
		  CFStringGetCString(dUID_in, uidname_in, 256,
				     CFStringGetSystemEncoding());
		  ms_message("AQ: devname_in:%s uidname_in:%s", devname_in, uidname_in);
		  
		  AudioStreamBasicDescription devicereadFormat;
		  slen = sizeof(devicereadFormat);
		  err = AudioDeviceGetProperty(V[i], 0, true,
					       kAudioDevicePropertyStreamFormat,
					       &slen, &devicereadFormat);
		  if (err == kAudioHardwareNoError) {
		    show_format("input device", &devicereadFormat);
		  }
		  MSSndCard *card = aq_card_new(devname_in, dUID_in, &devicereadFormat,
						NULL, MS_SND_CARD_CAP_CAPTURE);
		  ms_snd_card_manager_add_card(m, card);
		}
	}
#endif
}


/*
 Audio Queue recode callback
 */

static void readCallback(void *aqData,
						 AudioQueueRef inAQ,
						 AudioQueueBufferRef inBuffer,
						 const AudioTimeStamp * inStartTime,
						 UInt32 inNumPackets,
						 const AudioStreamPacketDescription * inPacketDesc)
{
	AQData *d = (AQData *) aqData;
	OSStatus err;
	mblk_t *rm = NULL;

	UInt32 len =
		(inBuffer->mAudioDataByteSize * d->readAudioFormat.mSampleRate /
		 1) / d->devicereadFormat.mSampleRate /
		d->devicereadFormat.mChannelsPerFrame;

	ms_mutex_lock(&d->mutex);
	if (d->read_started == FALSE) {
		ms_mutex_unlock(&d->mutex);
		return;
	}

	rm = allocb(len, 0);

#if 0
	err = AudioConverterConvertBuffer(d->readAudioConverter,
									  inBuffer->mAudioDataByteSize,
									  inBuffer->mAudioData,
									  &len, rm->b_wptr);
	if (err != noErr) {
		ms_error("readCallback: AudioConverterConvertBuffer %d", (int)err);
		ms_warning("readCallback: inBuffer->mAudioDataByteSize = %d",
				   (int)inBuffer->mAudioDataByteSize);
		ms_warning("readCallback: outlen = %d", (int)len);
		ms_warning("readCallback: origlen = %i",
				   (int)((inBuffer->mAudioDataByteSize *
					d->readAudioFormat.mSampleRate / 1) /
				   d->devicereadFormat.mSampleRate /
				   d->devicereadFormat.mChannelsPerFrame));
		freeb(rm);
	} else {

	  rm->b_wptr += len;
	  if (gain_volume_in != 1.0f)
	    {
	      int16_t *ptr=(int16_t *)rm->b_rptr;
	      for (;ptr<(int16_t *)rm->b_wptr;ptr++)
		{
		  *ptr=(int16_t)(((float)(*ptr))*gain_volume_in);
		}
	    }
	  putq(&d->rq, rm);
	}
#else
	memcpy(rm->b_wptr, inBuffer->mAudioData, len);
	rm->b_wptr += len;
	if (gain_volume_in != 1.0f)
	{
		int16_t *ptr=(int16_t *)rm->b_rptr;
		for (;ptr<(int16_t *)rm->b_wptr;ptr++)
		{
			*ptr=(int16_t)(((float)(*ptr))*gain_volume_in);
		}
	}
	putq(&d->rq, rm);	
#endif
	
	err = AudioQueueEnqueueBuffer(d->readQueue, inBuffer, 0, NULL);
	if (err != noErr) {
		ms_error("readCallback:AudioQueueEnqueueBuffer %ld", (long)err);
	}
	ms_mutex_unlock(&d->mutex);
}

/*
 Audio Queue play callback
 */

static void writeCallback(void *aqData,
						  AudioQueueRef inAQ, AudioQueueBufferRef inBuffer)
{
	AQData *d = (AQData *) aqData;
	OSStatus err;

	int len =
		(d->writeBufferByteSize * d->writeAudioFormat.mSampleRate / 1) /
		d->devicewriteFormat.mSampleRate /
		d->devicewriteFormat.mChannelsPerFrame;

	ms_mutex_lock(&d->mutex);
	if (d->write_started == FALSE) {
		ms_mutex_unlock(&d->mutex);
		return;
	}
	if (d->bufferizer->size >= len) {
#if 0
		UInt32 bsize = d->writeBufferByteSize;
		uint8_t *pData = ms_malloc(len);

		ms_bufferizer_read(d->bufferizer, pData, len);
		err = AudioConverterConvertBuffer(d->writeAudioConverter,
										  len,
										  pData,
										  &bsize, inBuffer->mAudioData);
		if (err != noErr) {
			ms_error("writeCallback: AudioConverterConvertBuffer %d", (int)err);
		}
		ms_free(pData);

		if (bsize != d->writeBufferByteSize)
			ms_warning("d->writeBufferByteSize = %i len = %i bsize = %i",
					   (int)d->writeBufferByteSize, (int)len, (int)bsize);
#else
		ms_bufferizer_read(d->bufferizer, inBuffer->mAudioData, len);
#endif
	} else {
		memset(inBuffer->mAudioData, 0, d->writeBufferByteSize);
	}
	inBuffer->mAudioDataByteSize = d->writeBufferByteSize;

	if (gain_changed_out == true)
	  {
	    AudioQueueSetParameter (d->writeQueue,
				    kAudioQueueParam_Volume,
				    gain_volume_out);
	    gain_changed_out = false;
	  }

	err = AudioQueueEnqueueBuffer(d->writeQueue, inBuffer, 0, NULL);
	if (err != noErr) {
		ms_error("AudioQueueEnqueueBuffer %ld", (long)err);
	}
	ms_mutex_unlock(&d->mutex);
}

void putWriteAQ(void *aqData, int queuenum)
{
	AQData *d = (AQData *) aqData;
	OSStatus err;
	err = AudioQueueEnqueueBuffer(d->writeQueue,
								  d->writeBuffers[queuenum], 0, NULL);
	if (err != noErr) {
		ms_error("AudioQueueEnqueueBuffer %ld", (long)err);
	}
}

/*
 play buffer setup function
 */

void setupWrite(MSFilter * f)
{
	AQData *d = (AQData *) f->data;
	OSStatus err;

	int bufferIndex;

	for (bufferIndex = 0; bufferIndex < kNumberAudioOutDataBuffers;
		 ++bufferIndex) {

		err = AudioQueueAllocateBuffer(d->writeQueue,
									   d->writeBufferByteSize,
									   &d->writeBuffers[bufferIndex]
			);
		if (err != noErr) {
			ms_error("setupWrite:AudioQueueAllocateBuffer %ld", (long)err);
		}
	}
}

/*
 recode buffer setup function
 */

void setupRead(MSFilter * f)
{
	AQData *d = (AQData *) f->data;
	OSStatus err;

	// allocate and enqueue buffers
	int bufferIndex;

	for (bufferIndex = 0; bufferIndex < kNumberAudioInDataBuffers;
		 ++bufferIndex) {

		AudioQueueBufferRef buffer;

		err = AudioQueueAllocateBuffer(d->readQueue,
									   d->readBufferByteSize, &buffer);
		if (err != noErr) {
			ms_error("setupRead:AudioQueueAllocateBuffer %ld", (long)err);
		}

		err = AudioQueueEnqueueBuffer(d->readQueue, buffer, 0, NULL);
		if (err != noErr) {
			ms_error("AudioQueueEnqueueBuffer %ld", (long)err);
		}
	}
}


static void aq_start_r(MSFilter * f)
{
	AQData *d = (AQData *) f->data;
	if (d->read_started == FALSE) {
		OSStatus aqresult;

		d->readAudioFormat.mSampleRate = d->rate;
		d->readAudioFormat.mFormatID = kAudioFormatLinearPCM;
		d->readAudioFormat.mFormatFlags =
			kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
		d->readAudioFormat.mFramesPerPacket = 1;
		d->readAudioFormat.mChannelsPerFrame = 1;
		d->readAudioFormat.mBitsPerChannel = d->bits;
		d->readAudioFormat.mBytesPerPacket = d->bits / 8;
		d->readAudioFormat.mBytesPerFrame = d->bits / 8;

		//show_format("input device", &d->devicereadFormat);
		//show_format("data from input filter", &d->readAudioFormat);

		memcpy(&d->devicereadFormat, &d->readAudioFormat,
			   sizeof(d->readAudioFormat));
		d->readBufferByteSize =
			kSecondsPerBuffer * d->devicereadFormat.mSampleRate *
			(d->devicereadFormat.mBitsPerChannel / 8) *
			d->devicereadFormat.mChannelsPerFrame;

#if 0
		aqresult = AudioConverterNew(&d->devicereadFormat,
									 &d->readAudioFormat,
									 &d->readAudioConverter);
		if (aqresult != noErr) {
			ms_error("d->readAudioConverter = %d", (int)aqresult);
			d->readAudioConverter = NULL;
		}
#endif
		
		aqresult = AudioQueueNewInput(&d->devicereadFormat, readCallback, d,	// userData
									  NULL,	// run loop
									  NULL,	// run loop mode
									  0,	// flags
									  &d->readQueue);
		if (aqresult != noErr) {
			ms_error("AudioQueueNewInput = %ld", (long)aqresult);
		}

		if (d->uidname!=NULL){
			char uidname[256];
			CFStringGetCString(d->uidname, uidname, 256,
							   CFStringGetSystemEncoding());
			ms_message("AQ: using uidname:%s", uidname);
			aqresult =
				AudioQueueSetProperty(d->readQueue,
								  kAudioQueueProperty_CurrentDevice,
								  &d->uidname, sizeof(CFStringRef));
			if (aqresult != noErr) {
				ms_error
					("AudioQueueSetProperty on kAudioQueueProperty_CurrentDevice %ld",
					 (long)aqresult);
			}
		}

		setupRead(f);
		aqresult = AudioQueueStart(d->readQueue, NULL);	// start time. NULL means ASAP.
		check_aqresult(aqresult,"AudioQueueStart - read");
		if (aqresult == noErr) {
			d->read_started = TRUE;
		}
		
	}
}

static void aq_stop_r(MSFilter * f)
{
	AQData *d = (AQData *) f->data;

	if (d->read_started == TRUE) {
		ms_mutex_lock(&d->mutex);
		d->read_started = FALSE;	/* avoid a deadlock related to buffer conversion in callback  */
		ms_mutex_unlock(&d->mutex);
#if 0
		AudioConverterDispose(d->readAudioConverter);
#endif
		AudioQueueStop(d->readQueue, true);
		AudioQueueDispose(d->readQueue, true);

#if TARGET_OS_IPHONE
        OSStatus aqresult = AudioSessionSetActiveWithFlags(false, kAudioSessionSetActiveFlag_NotifyOthersOnDeactivation);
 		check_aqresult(aqresult,"AudioSessionSetActive(false)");
#endif
	}
}

static void aq_start_w(MSFilter * f)
{
	AQData *d = (AQData *) f->data;
	if (d->write_started == FALSE) {
		OSStatus aqresult;
#if TARGET_OS_IPHONE
		UInt32 audioCategory;
		audioCategory= kAudioSessionCategory_AmbientSound;
		ms_message("AQ: Configuring audio session for playback");
		aqresult =AudioSessionSetProperty(kAudioSessionProperty_AudioCategory, sizeof(audioCategory), &audioCategory);
		check_aqresult(aqresult,"Configuring audio session ");

		aqresult = AudioSessionSetActive(true);
		check_aqresult(aqresult,"AudioSessionSetActive");
#endif
		d->writeAudioFormat.mSampleRate = d->rate;
		d->writeAudioFormat.mFormatID = kAudioFormatLinearPCM;
		d->writeAudioFormat.mFormatFlags =
			kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
		d->writeAudioFormat.mFramesPerPacket = 1;
		d->writeAudioFormat.mChannelsPerFrame = 1;
		d->writeAudioFormat.mBitsPerChannel = d->bits;
		d->writeAudioFormat.mBytesPerPacket = d->bits / 8;
		d->writeAudioFormat.mBytesPerFrame = d->bits / 8;

		//show_format("data provided to output filter",	&d->writeAudioFormat);
		//show_format("output device", &d->devicewriteFormat);

		memcpy(&d->devicewriteFormat, &d->writeAudioFormat,
			   sizeof(d->writeAudioFormat));
		d->writeBufferByteSize =
			kSecondsPerBuffer * d->devicewriteFormat.mSampleRate *
			(d->devicewriteFormat.mBitsPerChannel / 8) *
			d->devicewriteFormat.mChannelsPerFrame;

#if 0
		aqresult = AudioConverterNew(&d->writeAudioFormat,
									 &d->devicewriteFormat,
									 &d->writeAudioConverter);
		if (aqresult != noErr) {
			ms_error("d->writeAudioConverter = %d", (int)aqresult);
			d->writeAudioConverter = NULL;
		}
#endif
		
		// create the playback audio queue object
		aqresult = AudioQueueNewOutput(&d->devicewriteFormat, writeCallback, d, NULL,	/*CFRunLoopGetCurrent () */
									   NULL,	/*kCFRunLoopCommonModes */
									   0,	// run loop flags
									   &d->writeQueue);
		if (aqresult != noErr) {
			ms_error("AudioQueueNewOutput = %ld", (long)aqresult);
		}

		AudioQueueSetParameter (d->writeQueue,
					kAudioQueueParam_Volume,
					gain_volume_out);

		if (d->uidname!=NULL){
			char uidname[256];
			CFStringGetCString(d->uidname, uidname, 256,
							   CFStringGetSystemEncoding());
			ms_message("AQ: using uidname:%s", uidname);
			aqresult =
				AudioQueueSetProperty(d->writeQueue,
									  kAudioQueueProperty_CurrentDevice,
									  &d->uidname, sizeof(CFStringRef));
			if (aqresult != noErr) {
				ms_error
					("AudioQueueSetProperty on kAudioQueueProperty_CurrentDevice %ld",
					 (long)aqresult);
			}
		}

		setupWrite(f);
		d->curWriteBuffer = 0;
	}
}

static void aq_stop_w(MSFilter * f)
{
	AQData *d = (AQData *) f->data;
	if (d->write_started == TRUE) {
		ms_mutex_lock(&d->mutex);
		d->write_started = FALSE;	/* avoid a deadlock related to buffer conversion in callback */
		ms_mutex_unlock(&d->mutex);
#if 0
		AudioConverterDispose(d->writeAudioConverter);
#endif
		AudioQueueStop(d->writeQueue, true);

		AudioQueueDispose(d->writeQueue, true);

#if TARGET_OS_IPHONE
        OSStatus aqresult = AudioSessionSetActiveWithFlags(false, kAudioSessionSetActiveFlag_NotifyOthersOnDeactivation);
 		check_aqresult(aqresult,"AudioSessionSetActive(false)");
#endif
	}
}

static mblk_t *aq_get(MSFilter * f)
{
	AQData *d = (AQData *) f->data;
	mblk_t *m;
	ms_mutex_lock(&d->mutex);
	m = getq(&d->rq);
	ms_mutex_unlock(&d->mutex);
	return m;
}

static void aq_put(MSFilter * f, mblk_t * m)
{
	AQData *d = (AQData *) f->data;
	ms_mutex_lock(&d->mutex);
	ms_bufferizer_put(d->bufferizer, m);
	ms_mutex_unlock(&d->mutex);

	int len =
		(d->writeBufferByteSize * d->writeAudioFormat.mSampleRate / 1) /
		d->devicewriteFormat.mSampleRate /
		d->devicewriteFormat.mChannelsPerFrame;
	if (d->write_started == FALSE && d->bufferizer->size >= len && d->curWriteBuffer<4) {
		AudioQueueBufferRef curbuf = d->writeBuffers[d->curWriteBuffer];
#if 0
		OSStatus err;
		UInt32 bsize = d->writeBufferByteSize;
		uint8_t *pData = ms_malloc(len);

		ms_bufferizer_read(d->bufferizer, pData, len);
		err = AudioConverterConvertBuffer(d->writeAudioConverter,
										  len,
										  pData,
										  &bsize, curbuf->mAudioData);
		if (err != noErr) {
			ms_error("writeCallback: AudioConverterConvertBuffer %d", (int)err);
		}
		ms_free(pData);

		if (bsize != d->writeBufferByteSize)
			ms_warning("d->writeBufferByteSize = %i len = %i bsize = %i",
					   (int)d->writeBufferByteSize, (int)len, (int)bsize);
#else
		ms_bufferizer_read(d->bufferizer, curbuf->mAudioData, len);
#endif
		curbuf->mAudioDataByteSize = d->writeBufferByteSize;
		putWriteAQ(d, d->curWriteBuffer);
		++d->curWriteBuffer;
	}
	if (d->write_started == FALSE
		&& d->curWriteBuffer == kNumberAudioOutDataBuffers - 1) {
		OSStatus err;
		err = AudioQueueStart(d->writeQueue, NULL	// start time. NULL means ASAP.
			);
		check_aqresult(err, "AudioQueueStart -write-");
		if (err == noErr) {
			d->write_started = TRUE;		
		} 
		

	}
}

static void aq_init(MSFilter * f)
{
	AQData *d = ms_new(AQData, 1);
	d->bits = 16;
	d->rate = 8000;
	d->stereo = FALSE;

	d->read_started = FALSE;
	d->write_started = FALSE;
	qinit(&d->rq);
	d->bufferizer = ms_bufferizer_new();
	ms_mutex_init(&d->mutex, NULL);
	f->data = d;
}

static void aq_uninit(MSFilter * f)
{
	AQData *d = (AQData *) f->data;
	flushq(&d->rq, 0);
	ms_bufferizer_destroy(d->bufferizer);
	ms_mutex_destroy(&d->mutex);
	if (d->uidname != NULL)
		CFRelease(d->uidname);
	ms_free(d);
}

static void aq_read_preprocess(MSFilter * f)
{
	aq_start_r(f);
}

static void aq_read_postprocess(MSFilter * f)
{
	aq_stop_r(f);
}

static void aq_read_process(MSFilter * f)
{
	mblk_t *m;
	while ((m = aq_get(f)) != NULL) {
		ms_queue_put(f->outputs[0], m);
	}
}

static void aq_write_preprocess(MSFilter * f)
{
	aq_start_w(f);
}

static void aq_write_postprocess(MSFilter * f)
{
	aq_stop_w(f);
}

static void aq_write_process(MSFilter * f)
{
	mblk_t *m;
	while ((m = ms_queue_get(f->inputs[0])) != NULL) {
		aq_put(f, m);
	}
}

static int set_rate(MSFilter * f, void *arg)
{
	AQData *d = (AQData *) f->data;
	d->rate = *((int *) arg);
	return 0;
}

static int read_get_rate(MSFilter * f, void *arg)
{
	AQData *d = (AQData *) f->data;
	*((int *) arg) = d->rate;
	return 0;
}

static int write_get_rate(MSFilter * f, void *arg)
{
	AQData *d = (AQData *) f->data;
	*((int *) arg) = d->rate;
	return 0;
}

/*
static int set_nchannels(MSFilter *f, void *arg){
	AQData *d=(AQData*)f->data;
	d->stereo=(*((int*)arg)==2);
	return 0;
}
*/

static MSFilterMethod aq_read_methods[] = {
	{MS_FILTER_SET_SAMPLE_RATE, set_rate},
	{MS_FILTER_GET_SAMPLE_RATE, read_get_rate},
/* not support yet
	{	MS_FILTER_SET_NCHANNELS		, set_nchannels	},
*/
	{0, NULL}
};

MSFilterDesc aq_read_desc = {
	.id = MS_AQ_READ_ID,
	.name = "MSAQRead",
	.text = N_("Sound capture filter for MacOS X Audio Queue Service"),
	.category = MS_FILTER_OTHER,
	.ninputs = 0,
	.noutputs = 1,
	.init = aq_init,
	.preprocess = aq_read_preprocess,
	.process = aq_read_process,
	.postprocess = aq_read_postprocess,
	.uninit = aq_uninit,
	.methods = aq_read_methods
};

static MSFilterMethod aq_write_methods[] = {
	{MS_FILTER_SET_SAMPLE_RATE, set_rate},
	{MS_FILTER_GET_SAMPLE_RATE, write_get_rate},
/* not support yet
	{	MS_FILTER_SET_NCHANNELS		, set_nchannels	},
*/
	{0, NULL}
};

MSFilterDesc aq_write_desc = {
	.id = MS_AQ_WRITE_ID,
	.name = "MSAQWrite",
	.text = N_("Sound playback filter for MacOS X Audio Queue Service"),
	.category = MS_FILTER_OTHER,
	.ninputs = 1,
	.noutputs = 0,
	.init = aq_init,
	.preprocess = aq_write_preprocess,
	.process = aq_write_process,
	.postprocess = aq_write_postprocess,
	.uninit = aq_uninit,
	.methods = aq_write_methods
};

MSFilter *ms_aq_read_new(MSSndCard * card)
{
	MSFilter *f = ms_factory_create_filter_from_desc(ms_snd_card_get_factory(card), &aq_read_desc);
	AqSndDsCard *wc = (AqSndDsCard *) card->data;
	AQData *d = (AQData *) f->data;
	d->uidname = NULL;
	if (wc->uidname != NULL)
		d->uidname = CFStringCreateCopy(NULL, wc->uidname);
	memcpy(&d->devicereadFormat, &wc->devicereadFormat,
		   sizeof(AudioStreamBasicDescription));
	memcpy(&d->devicewriteFormat, &wc->devicewriteFormat,
		   sizeof(AudioStreamBasicDescription));
	return f;
}


MSFilter *ms_aq_write_new(MSSndCard * card)
{
	MSFilter *f = ms_factory_create_filter_from_desc(ms_snd_card_get_factory(card), &aq_write_desc);
	AqSndDsCard *wc = (AqSndDsCard *) card->data;
	AQData *d = (AQData *) f->data;
	d->uidname = NULL;
	if (wc->uidname != NULL)
		d->uidname = CFStringCreateCopy(NULL, wc->uidname);
	memcpy(&d->devicereadFormat, &wc->devicereadFormat,
		   sizeof(AudioStreamBasicDescription));
	memcpy(&d->devicewriteFormat, &wc->devicewriteFormat,
		   sizeof(AudioStreamBasicDescription));
	return f;
}

MS_FILTER_DESC_EXPORT(aq_read_desc)
MS_FILTER_DESC_EXPORT(aq_write_desc)

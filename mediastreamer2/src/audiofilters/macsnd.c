/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
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

#include <bctoolbox/defs.h>

#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include <CoreServices/CoreServices.h>

// #include <CoreServices/CarbonCore/Debugging.h>

#include "mediastreamer2/devices.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/mssndcard.h"
#include "mediastreamer2/msticker.h"

#if __LP64__
#define UINT32_PRINTF "u"
#define UINT32_X_PRINTF "x"
#else
#define UINT32_PRINTF "lu"
#define UINT32_X_PRINTF "lx"
#endif

MSFilter *ms_au_read_new(MSSndCard *card);
MSFilter *ms_au_write_new(MSSndCard *card);

static const int flow_control_interval = 5000; // ms
static const int flow_control_threshold = 40;  // ms

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#define CHECK_AURESULT(call)                                                                                           \
	do {                                                                                                               \
		int _err;                                                                                                      \
		if ((_err = (call)) != noErr)                                                                                  \
			ms_error("[MSAU] " #call ": error [%i] %s %s", _err, GetMacOSStatusErrorString(_err),                      \
			         GetMacOSStatusCommentString(_err));                                                               \
	} while (0)
/*#undef ms_debug
#define ms_debug ms_message*/
static void show_format(BCTBX_UNUSED(const char *name), AudioStreamBasicDescription *deviceFormat) {
	ms_debug("[MSAU] === Format for %s", name);
	ms_debug("[MSAU] mSampleRate = %g", deviceFormat->mSampleRate);
	unsigned int fcc = ntohl(deviceFormat->mFormatID);
	char outName[5];
	memcpy(outName, &fcc, 4);
	outName[4] = 0;
	ms_debug("[MSAU] mFormatID = %s", outName);
	ms_debug("[MSAU] mFormatFlags = %08lX", (unsigned long)deviceFormat->mFormatFlags);
	ms_debug("[MSAU] mBytesPerPacket = %ld", (unsigned long)deviceFormat->mBytesPerPacket);
	ms_debug("[MSAU] mFramesPerPacket = %ld", (unsigned long)deviceFormat->mFramesPerPacket);
	ms_debug("[MSAU] mChannelsPerFrame = %ld", (unsigned long)deviceFormat->mChannelsPerFrame);
	ms_debug("[MSAU] mBytesPerFrame = %ld", (unsigned long)deviceFormat->mBytesPerFrame);
	ms_debug("[MSAU] mBitsPerChannel = %ld", (unsigned long)deviceFormat->mBitsPerChannel);
	ms_message("[MSAU] Format for [%s] rate [%g] channels [%" UINT32_PRINTF "]", outName, deviceFormat->mSampleRate,
	           deviceFormat->mChannelsPerFrame);
	ms_debug("[MSAU] ===");
}

typedef struct AUCommon {
	AudioDeviceID dev;
	int rate;
	int nchannels;
	bool_t follow_default;
	bool_t route_changed;
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
	AudioComponentInstance au;
#else
	AudioUnit au;
#endif
	ms_mutex_t mutex;
} AUCommon;

typedef struct AURead {
	AUCommon common;
	queue_t rq;
	MSTickerSynchronizer *ticker_synchronizer;
	uint64_t timestamp;
	bool_t first_process;
} AURead;

typedef struct AUWrite {
	AUCommon common;
	MSFlowControlledBufferizer *buffer;
} AUWrite;

typedef struct AuCard {
	char *uidname;
	int removed;
	int rate; /*the nominal rate of the device*/
} AuCard;

static int au_get_default_device_id(AudioDeviceID *id, bool_t is_read) {
	UInt32 len = sizeof(AudioDeviceID);
	OSStatus err;
	AudioObjectPropertyAddress theAddress = {is_read ? kAudioHardwarePropertyDefaultInputDevice
	                                                 : kAudioHardwarePropertyDefaultOutputDevice,
	                                         kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster};
	err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &theAddress, 0, NULL, &len, id);
	// AudioHardwareGetProperty(is_read?kAudioHardwarePropertyDefaultInputDevice:kAudioHardwarePropertyDefaultOutputDevice,&len,
	// &d->dev);
	if (err != kAudioHardwareNoError) {
		ms_error("[MSAU] Unable to query for default %s AudioDevice", is_read ? "Capture" : "Playback");
		return -1;
	}
	return 0;
}

static int au_get_device_sample_rate(AudioDeviceID dev, bool_t is_read, int *rate) {
	AudioObjectPropertyScope theScope = is_read ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;
	AudioObjectPropertyAddress theAddress = {kAudioDevicePropertyStreamFormat, theScope, 0};
	AudioStreamBasicDescription format;
	UInt32 slen;
	int err;

	err = AudioObjectGetPropertyData(dev, &theAddress, 0, NULL, &slen, &format);

	//		err = AudioDeviceGetProperty(dev, 0, cap & MS_SND_CARD_CAP_CAPTURE, kAudioDevicePropertyStreamFormat,
	//&slen, &format);
	if (err == kAudioHardwareNoError && format.mSampleRate > 0) {
		// show_format("device", &format);
		*rate = format.mSampleRate;
		return 0;
	} else return -1;
}

static void
au_card_set_level(BCTBX_UNUSED(MSSndCard *card), BCTBX_UNUSED(MSSndCardMixerElem e), BCTBX_UNUSED(int percent)) {
}

static int au_card_get_level(BCTBX_UNUSED(MSSndCard *card), BCTBX_UNUSED(MSSndCardMixerElem e)) {
	return -1;
}

static void au_card_set_source(BCTBX_UNUSED(MSSndCard *card), BCTBX_UNUSED(MSSndCardCapture source)) {
}

static void au_card_init(MSSndCard *card) {
	AuCard *c = (AuCard *)ms_new0(AuCard, 1);
	c->removed = 0;
	card->data = c;
}

static void au_card_uninit(MSSndCard *card) {
	AuCard *d = (AuCard *)card->data;
	if (d->uidname != NULL) ms_free(d->uidname);
	ms_free(d);
}

static void au_card_detect(MSSndCardManager *m);
static void au_card_unload(MSSndCardManager *m);
static bool_t au_card_reload_requested(MSSndCardManager *m);
static MSSndCard *au_card_duplicate(MSSndCard *obj);

MSSndCardDesc ca_card_desc = {.driver_type = "AudioUnit",
                              .detect = au_card_detect,
                              .unload = au_card_unload,
                              .reload_requested = au_card_reload_requested,
                              .init = au_card_init,
                              .set_level = au_card_set_level,
                              .get_level = au_card_get_level,
                              .set_capture = au_card_set_source,
                              .create_reader = ms_au_read_new,
                              .create_writer = ms_au_write_new,
                              .uninit = au_card_uninit,
                              .duplicate = au_card_duplicate};

static MSSndCard *au_card_duplicate(MSSndCard *obj) {
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

static void ca_set_device_type(AudioDeviceID deviceId, MSSndCard *card) {
	AudioObjectPropertyAddress request;
	UInt32 terminalType = 0;
	UInt32 returnSize = 0;

	// Check transport
	AudioDevicePropertyID transportType;
	request.mSelector = kAudioDevicePropertyTransportType;
	request.mScope = kAudioObjectPropertyScopeGlobal;
	request.mElement = kAudioObjectPropertyElementMaster;
	returnSize = sizeof(AudioDevicePropertyID);
	AudioObjectGetPropertyData(deviceId, &request, 0, 0, &returnSize, &transportType);
	// Check Terminal Type
	request.mSelector = kAudioDevicePropertyDataSource;
	request.mScope = kAudioObjectPropertyScopeGlobal;
	request.mElement = kAudioObjectPropertyElementMaster;
	returnSize = sizeof(UInt32);
	AudioObjectGetPropertyData(deviceId, &request, 0, NULL, &returnSize, &terminalType);
	// Terminal type can be not set on same cases like in Builtin card. If unknown,  check deeper in the source that
	// depends of the capability
	if (terminalType == kAudioStreamTerminalTypeUnknown) {
		request.mSelector = kAudioDevicePropertyDataSource;
		request.mElement = kAudioObjectPropertyElementMaster;
		returnSize = sizeof(UInt32);
		if (card->capabilities == MS_SND_CARD_CAP_CAPTURE) { // Multiple capability is not supported
			request.mScope = kAudioObjectPropertyScopeInput;
			returnSize = sizeof(UInt32);
			AudioObjectGetPropertyData(deviceId, &request, 0, NULL, &returnSize, &terminalType);
		} else if (card->capabilities == MS_SND_CARD_CAP_PLAYBACK) {
			request.mScope = kAudioObjectPropertyScopeOutput;
			AudioObjectGetPropertyData(deviceId, &request, 0, NULL, &returnSize, &terminalType);
		}
	}
	// 'ispk', 'espk', 'imic' and 'emic' are undocumented terminalType on Mac. This seems to be IOS types but can be
	// retrieve by the mac API
	if (transportType == kAudioDeviceTransportTypeBluetooth || transportType == kAudioDeviceTransportTypeBluetoothLE)
		card->device_type = MS_SND_CARD_DEVICE_TYPE_BLUETOOTH;
	else if (terminalType == kAudioStreamTerminalTypeReceiverSpeaker)
		card->device_type = MS_SND_CARD_DEVICE_TYPE_EARPIECE;
	else if (terminalType == kAudioStreamTerminalTypeSpeaker || terminalType == 'ispk' || terminalType == 'espk')
		card->device_type = MS_SND_CARD_DEVICE_TYPE_SPEAKER;
	else if (terminalType == kAudioStreamTerminalTypeMicrophone ||
	         terminalType == kAudioStreamTerminalTypeReceiverMicrophone || terminalType == 'imic' ||
	         terminalType == 'emic')
		card->device_type = MS_SND_CARD_DEVICE_TYPE_MICROPHONE;
	else if (terminalType == kAudioStreamTerminalTypeHeadsetMicrophone)
		card->device_type = MS_SND_CARD_DEVICE_TYPE_HEADSET;
	else if (terminalType == kAudioStreamTerminalTypeHeadphones) card->device_type = MS_SND_CARD_DEVICE_TYPE_HEADPHONES;
	else if (transportType == kAudioDeviceTransportTypeUSB) card->device_type = MS_SND_CARD_DEVICE_TYPE_GENERIC_USB;
	else card->device_type = MS_SND_CARD_DEVICE_TYPE_UNKNOWN;
}

static MSSndCard *ca_card_new(const char *name, const char *uidname, AudioDeviceID dev, unsigned cap) {
	MSSndCard *card = ms_snd_card_new(&ca_card_desc);
	AuCard *d = (AuCard *)card->data;
	bool is_read = cap & MS_SND_CARD_CAP_CAPTURE;

	d->uidname = ms_strdup(uidname);
	if (dev == (AudioDeviceID)-1) card->name = ms_strdup_printf("%s", name);
	else card->name = ms_strdup_printf("%s (%s)", name, uidname); /*include uid so that names are uniques*/
	card->capabilities = cap;
	if (is_read) {
		card->latency = 70; /* Sound card latency seems always not least than 70ms on mac*/
	}
	d->rate = 44100;

	// If default device, use the current device for the case it will not changed between now and using the device. That
	// way, we get the correct rate from start.
	if (dev == (AudioDeviceID)-1) au_get_default_device_id(&dev, is_read);
	au_get_device_sample_rate(dev, is_read, &d->rate);
	ca_set_device_type(dev, card);
	return card;
}

static bool_t check_card_capability(AudioDeviceID id, bool_t is_input, char *devname, char *uidname, size_t name_len) {
	UInt32 slen = name_len;
	CFStringRef dUID = NULL;
	bool_t ret = FALSE;
	OSStatus err;
	AudioObjectPropertyScope theScope = is_input ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;
	AudioObjectPropertyAddress theAddress = {kAudioDevicePropertyDeviceName, theScope, 0};

	err = AudioObjectGetPropertyData(id, &theAddress, 0, NULL, &slen, devname);
	/*int err =AudioDeviceGetProperty(id, 0, is_input, kAudioDevicePropertyDeviceName, &slen,devname);*/
	if (err != kAudioHardwareNoError) {
		ms_error("[MSAU] Get kAudioDevicePropertyDeviceName error %" UINT32_PRINTF, err);
		return FALSE;
	}
	theAddress.mSelector = kAudioDevicePropertyStreamConfiguration;

	err = AudioObjectGetPropertyDataSize(id, &theAddress, 0, NULL, &slen);
	/*err =AudioDeviceGetPropertyInfo(id, 0, is_input, kAudioDevicePropertyStreamConfiguration, &slen, &writable);*/
	if (err != kAudioHardwareNoError) {
		ms_error("[MSAU] Get kAudioDevicePropertyStreamConfiguration error %" UINT32_PRINTF, err);
		return FALSE;
	}

	AudioBufferList *buflist = ms_malloc(slen);

	theAddress.mSelector = kAudioDevicePropertyStreamConfiguration;
	err = AudioObjectGetPropertyData(id, &theAddress, 0, NULL, &slen, buflist);
	/*err = 	AudioDeviceGetProperty(id, 0, is_input, kAudioDevicePropertyStreamConfiguration, &slen, buflist);*/
	if (err != kAudioHardwareNoError) {
		ms_error("[MSAU] Get kAudioDevicePropertyStreamConfiguration error %" UINT32_PRINTF, err);
		ms_free(buflist);
		return FALSE;
	}

	UInt32 j;
	for (j = 0; j < buflist->mNumberBuffers; j++) {
		if (buflist->mBuffers[j].mNumberChannels > 0) {
			ret = TRUE;
			break;
		}
	}
	ms_free(buflist);
	if (ret == FALSE) return FALSE;

	slen = sizeof(CFStringRef);
	theAddress.mSelector = kAudioDevicePropertyDeviceUID;
	err = AudioObjectGetPropertyData(id, &theAddress, 0, NULL, &slen, &dUID);
	// err =AudioDeviceGetProperty(id, 0, is_input, kAudioDevicePropertyDeviceUID, &slen,&dUID);
	if (err != kAudioHardwareNoError) {
		ms_error("[MSAU] Get kAudioDevicePropertyDeviceUID error %" UINT32_PRINTF, err);
		return FALSE;
	}
	CFStringGetCString(dUID, uidname, name_len, CFStringGetSystemEncoding());
	ms_message("[MSAU] CA: devname:%s uidname:%s", devname, uidname);

	return ret;
}

/* Would be nice if this boolean could be part of the MSSndCardManager */
static bool_t reload_requested = FALSE;

static OSStatus au_card_listener(BCTBX_UNUSED(AudioObjectID inObjectID),
                                 BCTBX_UNUSED(UInt32 inNumberAddresses),
                                 BCTBX_UNUSED(const AudioObjectPropertyAddress *inAddresses),
                                 BCTBX_UNUSED(void *inClientData)) {
	ms_message("[MSAU] A change happend with the list of available sound devices, will reload");
	reload_requested = TRUE;
	return 0;
}

static void au_card_unload(MSSndCardManager *m) {
	AudioObjectPropertyAddress theAddress = {kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal,
	                                         kAudioObjectPropertyElementMaster};
	OSStatus err = AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &theAddress, au_card_listener, m);
	if (err != kAudioHardwareNoError) {
		ms_error("[MSAU] Unable to remove listener for audio objects.");
	} else {
		ms_message("[MSAU] Removed listener for available sound devices.");
	}
}

static bool_t au_card_reload_requested(BCTBX_UNUSED(MSSndCardManager *m)) {
	return reload_requested;
}

static void au_card_detect(MSSndCardManager *m) {
	OSStatus err;
	UInt32 slen;
	int count;
	int i;

	slen = 0;
	AudioObjectPropertyAddress theAddress = {kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal,
	                                         kAudioObjectPropertyElementMaster};

	err = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &theAddress, 0, NULL, &slen);
	/*err =
	AudioHardwareGetPropertyInfo(kAudioHardwarePropertyDevices, &slen,
	                             &writable);*/
	if (err != kAudioHardwareNoError) {
		ms_error("get kAudioHardwarePropertyDevices error %" UINT32_PRINTF, err);
		return;
	}
	AudioDeviceID devices[slen / sizeof(AudioDeviceID)];
	err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &theAddress, 0, NULL, &slen, &devices);
	/*err =
	AudioHardwareGetProperty(kAudioHardwarePropertyDevices, &slen, devices);*/
	if (err != kAudioHardwareNoError) {
		ms_error("get kAudioHardwarePropertyDevices error %" UINT32_PRINTF, err);
		return;
	}

	ms_snd_card_manager_add_card(m, ca_card_new("Default Capture", "", -1, MS_SND_CARD_CAP_CAPTURE));
	ms_snd_card_manager_add_card(m, ca_card_new("Default Playback", "", -1, MS_SND_CARD_CAP_PLAYBACK));

	count = slen / sizeof(AudioDeviceID);
	for (i = 0; i < count; i++) {
		MSSndCard *card;
		char uidname[256] = {0}, devname[256] = {0};
		int card_capacity = 0;
		if (check_card_capability(devices[i], FALSE, devname, uidname, sizeof(uidname))) {
			card_capacity |= MS_SND_CARD_CAP_PLAYBACK;
		}
		if (check_card_capability(devices[i], TRUE, devname, uidname, sizeof(uidname))) {
			card_capacity |= MS_SND_CARD_CAP_CAPTURE;
		}

		if (card_capacity) {
			card = ca_card_new(devname, uidname, devices[i], card_capacity);
			ms_snd_card_manager_add_card(m, card);
		}
	}

	/* Attempt to remove the property listener possibly previously set - in case of reload */
	AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &theAddress, au_card_listener, m);

	err = AudioObjectAddPropertyListener(kAudioObjectSystemObject, &theAddress, au_card_listener, m);
	if (err != kAudioHardwareNoError) {
		ms_error("[MSAU] Unable to set listener for audio objects.");
	} else {
		ms_message("[MSAU] Set listener for available sound devices.");
	}
	reload_requested = FALSE;
}

static OSStatus readRenderProc(void *inRefCon,
                               AudioUnitRenderActionFlags *inActionFlags,
                               const AudioTimeStamp *inTimeStamp,
                               UInt32 inBusNumber,
                               UInt32 inNumFrames,
                               BCTBX_UNUSED(AudioBufferList *ioData)) {
	AURead *d = (AURead *)inRefCon;
	AudioBufferList lreadAudioBufferList = {0};
	mblk_t *rm;
	OSStatus err;

	lreadAudioBufferList.mNumberBuffers = 1;
	lreadAudioBufferList.mBuffers[0].mDataByteSize = inNumFrames * sizeof(int16_t) * d->common.nchannels;
	rm = allocb(lreadAudioBufferList.mBuffers[0].mDataByteSize, 0);
	lreadAudioBufferList.mBuffers[0].mData = rm->b_wptr;
	lreadAudioBufferList.mBuffers[0].mNumberChannels = d->common.nchannels;

	err = AudioUnitRender(d->common.au, inActionFlags, inTimeStamp, inBusNumber, inNumFrames, &lreadAudioBufferList);

	if (err != noErr) {
		ms_error("[MSAU] AudioUnitRender() for read returned [%" UINT32_PRINTF "] %s %s", err,
		         GetMacOSStatusErrorString(err), GetMacOSStatusCommentString(err));
		return 0;
	}

	rm->b_wptr += lreadAudioBufferList.mBuffers[0].mDataByteSize;
	ms_mutex_lock(&d->common.mutex);
	if (inTimeStamp->mFlags & kAudioTimeStampSampleTimeValid) {
		d->timestamp = inTimeStamp->mSampleTime;
	}

	putq(&d->rq, rm);
	ms_mutex_unlock(&d->common.mutex);

	return 0;
}

static OSStatus writeRenderProc(void *inRefCon,
                                BCTBX_UNUSED(AudioUnitRenderActionFlags *inActionFlags),
                                BCTBX_UNUSED(const AudioTimeStamp *inTimeStamp),
                                BCTBX_UNUSED(UInt32 inBusNumber),
                                BCTBX_UNUSED(UInt32 inNumFrames),
                                AudioBufferList *ioData) {
	AUWrite *d = (AUWrite *)inRefCon;
	int read;

	if (ioData->mNumberBuffers != 1)
		ms_warning("[MSAU] writeRenderProc: %" UINT32_PRINTF " buffers", ioData->mNumberBuffers);
	ms_mutex_lock(&d->common.mutex);
	read = ms_flow_controlled_bufferizer_read(d->buffer, ioData->mBuffers[0].mData, ioData->mBuffers[0].mDataByteSize);
	ms_mutex_unlock(&d->common.mutex);
	if (read == 0) {
		ms_debug("[MSAU] Silence inserted in audio output unit (%" UINT32_PRINTF " bytes)",
		         ioData->mBuffers[0].mDataByteSize);
		memset(ioData->mBuffers[0].mData, 0, ioData->mBuffers[0].mDataByteSize);
	}
	return 0;
}

static OSStatus audio_unit_default_listener(BCTBX_UNUSED(AudioObjectID inObjectID),
                                            BCTBX_UNUSED(UInt32 inNumberAddresses),
                                            BCTBX_UNUSED(const AudioObjectPropertyAddress *inAddresses),
                                            BCTBX_UNUSED(void *inClientData)) {
	AUCommon *common = (AUCommon *)inClientData;
	ms_message("[MSAU] Default %s audio route changed detected",
	           inAddresses->mSelector == kAudioHardwarePropertyDefaultInputDevice ? "Capture" : "Playback");
	ms_mutex_lock(&common->mutex);
	common->route_changed = TRUE;
	reload_requested = TRUE;
	ms_mutex_unlock(&common->mutex);
	return 0;
}

static void audio_unit_notif_register(AUCommon *common, bool_t is_read) {
	ms_message("[MSAU] Registering to default devices changes for %s", is_read ? "Capture" : "Playback");
	AudioObjectPropertyAddress audioDevicesAddress = {
	    is_read ? kAudioHardwarePropertyDefaultInputDevice : kAudioHardwarePropertyDefaultOutputDevice,
	    kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster};

	CHECK_AURESULT(AudioObjectAddPropertyListener(kAudioObjectSystemObject, &audioDevicesAddress,
	                                              audio_unit_default_listener, common));
}

static void audio_unit_notif_unregister(AUCommon *common, bool_t is_read) {
	AudioObjectPropertyAddress audioDevicesAddress = {
	    is_read ? kAudioHardwarePropertyDefaultInputDevice : kAudioHardwarePropertyDefaultOutputDevice,
	    kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster};
	CHECK_AURESULT(AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &audioDevicesAddress,
	                                                 audio_unit_default_listener, common));
}

static int audio_unit_open(MSFilter *f, AUCommon *d, bool_t is_read) {
	OSStatus result;
	UInt32 param;
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
	AudioComponentDescription desc;
	AudioComponent comp;
#else
	ComponentDescription desc;
	Component comp;
#endif
	AudioStreamBasicDescription asbd;
	const int input_bus = 1;
	const int output_bus = 0;
	int device_rate = 0;
	// kAudioUnitSubType_DefaultOutput cannot be used for microphones. Also it appears that some properties cannot be
	// set like format/buffer storage. Get Default audio unit
	if (d->follow_default) {
		au_get_default_device_id(&d->dev, is_read);
	}
	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = d->dev != (AudioDeviceID)-1 ? kAudioUnitSubType_HALOutput : kAudioUnitSubType_DefaultOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
	comp = AudioComponentFindNext(NULL, &desc);
#else
	comp = FindNextComponent(NULL, &desc);
#endif
	if (comp == NULL) {
		ms_error("[MSAU] Cannot find audio component");
		return -1;
	}
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
	result = AudioComponentInstanceNew(comp, &d->au);
#else
	result = OpenAComponent(comp, &d->au);
#endif
	if (result != noErr) {
		ms_error("[MSAU] Cannot open audio component %" UINT32_X_PRINTF, result);
		return -1;
	}
	param = is_read;
	if (d->dev != (AudioDeviceID)-1) { // Cannot manually enabling IO for defaults
		CHECK_AURESULT(AudioUnitSetProperty(d->au, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, input_bus,
		                                    &param, sizeof(UInt32)));

		param = !is_read;
		CHECK_AURESULT(AudioUnitSetProperty(d->au, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output,
		                                    output_bus, &param, sizeof(UInt32)));

		// Set the current device
		CHECK_AURESULT(AudioUnitSetProperty(d->au, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global,
		                                    output_bus, &d->dev, sizeof(AudioDeviceID)));
	}

	param = 0;
	CHECK_AURESULT(AudioUnitSetProperty(d->au, kAudioUnitProperty_ShouldAllocateBuffer,
	                                    is_read ? kAudioUnitScope_Input : kAudioUnitScope_Output,
	                                    is_read ? input_bus : output_bus, &param, sizeof(param)));

	UInt32 asbdsize = sizeof(AudioStreamBasicDescription);
	memset((char *)&asbd, 0, asbdsize);

	CHECK_AURESULT(AudioUnitGetProperty(d->au, kAudioUnitProperty_StreamFormat,
	                                    is_read ? kAudioUnitScope_Input : kAudioUnitScope_Output,
	                                    is_read ? input_bus : output_bus, &asbd, &asbdsize));

	device_rate = asbd.mSampleRate;
	if (device_rate == 0) au_get_device_sample_rate(d->dev, is_read, &device_rate);
	if (device_rate != d->rate) {
		// Filter rate and device rate are different. HAL cannot manage different rates alone.
		// If rate is forced, we should need to use a resampling unit (kAudioUnitType_FormatConverter) and add it into a
		// graph. As MS2 have his own resampler (which is better), reload format with new rate and warn MS2 for the
		// change.
		d->rate = device_rate;
		ms_filter_notify_no_arg(f, MS_FILTER_OUTPUT_FMT_CHANGED);
	}
	// Keep this afftection in case where mSampleRate is 0. Getter can give 0 but setter doesn't accept it. Use the
	// default rate.
	asbd.mSampleRate = d->rate;
	asbd.mBytesPerPacket = asbd.mBytesPerFrame = 2 * d->nchannels;
	asbd.mChannelsPerFrame = d->nchannels;
	asbd.mBitsPerChannel = 16;
	asbd.mFormatID = kAudioFormatLinearPCM;
	asbd.mFormatFlags = kAudioFormatFlagIsPacked | kAudioFormatFlagIsSignedInteger;

	CHECK_AURESULT(AudioUnitSetProperty(d->au, kAudioUnitProperty_StreamFormat,
	                                    is_read ? kAudioUnitScope_Output : kAudioUnitScope_Input,
	                                    is_read ? input_bus : output_bus, &asbd, sizeof(AudioStreamBasicDescription)));

	CHECK_AURESULT(AudioUnitGetProperty(d->au, kAudioUnitProperty_StreamFormat,
	                                    is_read ? kAudioUnitScope_Output : kAudioUnitScope_Input,
	                                    is_read ? input_bus : output_bus, &asbd, &asbdsize));

	show_format(is_read ? "Input audio unit after configuration" : "Output audio unit after configuration", &asbd);

	// Attempt to set the I/O buffer size
	param = sizeof(UInt32);
	UInt32 numFrames = 128;
	CHECK_AURESULT(
	    AudioUnitSetProperty(d->au, kAudioDevicePropertyBufferFrameSize, kAudioUnitScope_Global, 0, &numFrames, param));

	// Get the number of frames in the IO buffer(s)
	param = sizeof(UInt32);
	CHECK_AURESULT(AudioUnitGetProperty(d->au, kAudioDevicePropertyBufferFrameSize,
	                                    is_read ? kAudioUnitScope_Output : kAudioUnitScope_Input,
	                                    is_read ? input_bus : output_bus, &numFrames, &param));
	ms_message("[MSAU] %s: number of frames per buffer = %" UINT32_PRINTF,
	           is_read ? "Input AudioUnit" : "Output AudioUnit", numFrames);

	if (!is_read) {
		/* Latency is only provided for output AudioUnit */
		param = sizeof(UInt32);
		UInt32 latency = 0;
		CHECK_AURESULT(AudioUnitGetProperty(d->au, kAudioDevicePropertyLatency,
		                                    is_read ? kAudioUnitScope_Output : kAudioUnitScope_Input,
		                                    is_read ? input_bus : output_bus, &latency, &param));
		ms_message("[MSAU] Latency = %" UINT32_PRINTF, latency);
	}

	param = sizeof(int);
	int safetyOffset = 0;
	CHECK_AURESULT(AudioUnitGetProperty(d->au, kAudioDevicePropertySafetyOffset,
	                                    is_read ? kAudioUnitScope_Output : kAudioUnitScope_Input,
	                                    is_read ? input_bus : output_bus, &safetyOffset, &param));
	ms_message("[MSAU] Safety offset = %i", safetyOffset);

	AURenderCallbackStruct cbs;

	cbs.inputProcRefCon = d;
	if (is_read) {
		cbs.inputProc = readRenderProc;
		CHECK_AURESULT(AudioUnitSetProperty(d->au, kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global, 0,
		                                    &cbs, sizeof(AURenderCallbackStruct)));
	} else {
		cbs.inputProc = writeRenderProc;
		CHECK_AURESULT(AudioUnitSetProperty(d->au, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Global,
		                                    output_bus, &cbs, sizeof(AURenderCallbackStruct)));
	}
	result = AudioUnitInitialize(d->au);
	if (result != noErr) {
		ms_error("[MSAU] failed to AudioUnitInitialize %" UINT32_PRINTF " , is_read=%i", result, (int)is_read);
		return -1;
	}

	CHECK_AURESULT(AudioOutputUnitStart(d->au));
	return 0;
}

static void audio_unit_close(AUCommon *d) {
	if (d->au) {
		CHECK_AURESULT(AudioOutputUnitStop(d->au));
		CHECK_AURESULT(AudioUnitUninitialize(d->au));
		d->au = NULL;
	}
}

static void au_write_put(AUWrite *d, mblk_t *m) {
	ms_mutex_lock(&d->common.mutex);
	ms_flow_controlled_bufferizer_put(d->buffer, m);
	ms_mutex_unlock(&d->common.mutex);
}

static void au_common_init(AUCommon *d) {
	d->rate = 44100;
	d->nchannels = 1;
	ms_mutex_init(&d->mutex, NULL);
}

static void au_common_uninit(AUCommon *d) {
	ms_mutex_destroy(&d->mutex);
}

// 1 = Changed
static int au_common_check_route_changed(MSFilter *f, AUCommon *common, bool_t is_read) {
	if (common->route_changed) {
		common->route_changed = FALSE;
		MSAudioRouteChangedEvent ev;
		memset(&ev, 0, sizeof(ev));
		ev.need_update_device_list = true;
		ms_filter_notify(f, MS_AUDIO_ROUTE_CHANGED, &ev);
		audio_unit_close(common);
		audio_unit_open(f, common, is_read);
		return 1;
	} else return 0;
}

static void au_read_init(MSFilter *f) {
	AURead *d = ms_new0(AURead, 1);
	au_common_init(&d->common);
	qinit(&d->rq);
	d->ticker_synchronizer = ms_ticker_synchronizer_new();
	f->data = d;
}

static void au_read_preprocess(MSFilter *f) {
	AURead *d = (AURead *)f->data;
	ms_ticker_set_synchronizer(f->ticker, d->ticker_synchronizer);
	audio_unit_open(f, &d->common, TRUE);
	d->first_process = TRUE;
	if (d->common.follow_default) audio_unit_notif_register(&d->common, TRUE);
}
static void au_read_process(MSFilter *f) {
	AURead *d = (AURead *)f->data;
	mblk_t *m;

	ms_mutex_lock(&d->common.mutex);
	if (au_common_check_route_changed(f, &d->common, TRUE)) flushq(&d->rq, 0);
	else if (d->first_process) {
		/* The read queue must be flushed on first
		 * process() call because it contains samples
		 * that has been produced since the ticker
		 * was attached. Do not skiped this data would
		 * make the clock skew estimation to be wrong. */
		d->first_process = FALSE;
		flushq(&d->rq, 0);
	} else {
		bool_t got_something = FALSE;
		while ((m = getq(&d->rq)) != NULL) {
			ms_queue_put(f->outputs[0], m);
			got_something = TRUE;
		}
		if (got_something) ms_ticker_synchronizer_update(d->ticker_synchronizer, d->timestamp, d->common.rate);
	}
	ms_mutex_unlock(&d->common.mutex);
}

static void au_read_postprocess(MSFilter *f) {
	AURead *d = (AURead *)f->data;
	if (d->common.follow_default) audio_unit_notif_unregister(&d->common, TRUE);
	audio_unit_close(&d->common);
	ms_ticker_set_synchronizer(f->ticker, NULL);
}

static void au_read_uninit(MSFilter *f) {
	AURead *d = (AURead *)f->data;
	flushq(&d->rq, 0);
	ms_ticker_synchronizer_destroy(d->ticker_synchronizer);
	au_common_uninit(&d->common);
	ms_free(d);
}

/* Audio unit write filter */

static void au_write_init(MSFilter *f) {
	AUWrite *d = ms_new0(AUWrite, 1);
	au_common_init(&d->common);
	d->buffer = ms_flow_controlled_bufferizer_new(f, d->common.rate, d->common.nchannels);
	ms_flow_controlled_bufferizer_set_max_size_ms(d->buffer, flow_control_threshold);
	ms_flow_controlled_bufferizer_set_flow_control_interval_ms(d->buffer, flow_control_interval);
	f->data = d;
}

static void au_write_preprocess(MSFilter *f) {
	AUWrite *d = (AUWrite *)f->data;
	audio_unit_open(f, &d->common, FALSE);
	if (d->common.follow_default) audio_unit_notif_register(&d->common, FALSE);
}

static void au_write_process(MSFilter *f) {
	AUWrite *d = (AUWrite *)f->data;
	mblk_t *m;
	if (au_common_check_route_changed(f, &d->common, FALSE)) ms_queue_flush(f->inputs[0]);
	else
		while ((m = ms_queue_get(f->inputs[0])) != NULL) {
			au_write_put(d, m);
		}
}

static void au_write_postprocess(MSFilter *f) {
	AUWrite *d = (AUWrite *)f->data;
	if (d->common.follow_default) audio_unit_notif_unregister(&d->common, FALSE);
	audio_unit_close(&d->common);
}

static void au_write_uninit(MSFilter *f) {
	AUWrite *d = (AUWrite *)f->data;
	ms_flow_controlled_bufferizer_destroy(d->buffer);
	au_common_uninit(&d->common);
	ms_free(d);
}

static int set_rate(MSFilter *f, void *arg) {
	AUCommon *d = (AUCommon *)f->data;
	/*the hal audio unit does not accept custom rates whitout restarting audio unit and check applying new format
	 * (TODO)*/
	return (d->rate == *(int *)arg) ? 0 : -1;
}

static int get_rate(MSFilter *f, void *arg) {
	AUCommon *d = (AUCommon *)f->data;
	*((int *)arg) = d->rate;
	return 0;
}

static int read_set_nchannels(MSFilter *f, void *arg) {
	AURead *d = (AURead *)f->data;
	d->common.nchannels = *((int *)arg);
	return 0;
}

static int write_set_nchannels(MSFilter *f, void *arg) {
	AUWrite *d = (AUWrite *)f->data;
	d->common.nchannels = *((int *)arg);
	ms_flow_controlled_bufferizer_set_nchannels(d->buffer, d->common.nchannels);
	return 0;
}

static int get_nchannels(MSFilter *f, void *arg) {
	AUCommon *d = (AUCommon *)f->data;
	*((int *)arg) = d->nchannels;
	return 0;
}

static int ms_macsnd_get_volume(MSFilter *f, void *arg, bool isCapture) {
	AUCommon *d = (AUCommon *)f->data;
	float *pvolume = (float *)arg;
	AudioObjectPropertyAddress volumeAddr = {
	    kAudioHardwareServiceDeviceProperty_VirtualMasterVolume /*kAudioDevicePropertyVolumeScalar*/,
	    isCapture ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput,
	    kAudioObjectPropertyElementMaster};

	UInt32 size = sizeof(*pvolume);

	OSStatus err = AudioObjectGetPropertyData(d->dev, &volumeAddr, 0, NULL, &size, pvolume);

	if (err != noErr) {
		ms_warning("[MSAU] Cannot get capture volume from #[%d]. Err = %d", d->dev, err);
		*pvolume = 0.0f;
		return -1;
	}

	return 0;
}

static int ms_macsnd_capture_get_volume(MSFilter *f, void *arg) {
	return ms_macsnd_get_volume(f, arg, true);
}

static int ms_macsnd_playback_get_volume(MSFilter *f, void *arg) {
	return ms_macsnd_get_volume(f, arg, false);
}

static int ms_macsnd_set_volume(MSFilter *f, void *arg, bool isCapture) {
	AUCommon *d = (AUCommon *)f->data;
	float *pvolume = (float *)arg;
	Boolean isWritable;
	AudioObjectPropertyAddress volumeAddr = {kAudioHardwareServiceDeviceProperty_VirtualMasterVolume,
	                                         isCapture ? kAudioDevicePropertyScopeInput
	                                                   : kAudioDevicePropertyScopeOutput,
	                                         kAudioObjectPropertyElementMaster};

	OSStatus err = AudioObjectIsPropertySettable(d->dev, &volumeAddr, &isWritable);

	if (err == noErr) {
		if (isWritable) {
			err = AudioObjectSetPropertyData(d->dev, &volumeAddr, 0, NULL, sizeof(float), pvolume);
			if (err != noErr) {
				ms_error("[MSAU] Could not set volume of device #[%d]. Err = %d", d->dev, err);
				return -1;
			}
		} else {
			ms_error("[MSAU] Volume of device #[%d] is not settable.", d->dev);
			return -2;
		}
	} else {
		ms_error("[MSAU] Could not set volume of device #[%d]. Err = %d (step 2)", d->dev, err);
		return -3;
	}
	return 0;
}

static int ms_macsnd_capture_set_volume(MSFilter *f, void *arg) {
	return ms_macsnd_set_volume(f, arg, true);
}

static int ms_macsnd_playback_set_volume(MSFilter *f, void *arg) {
	return ms_macsnd_set_volume(f, arg, false);
}

static MSFilterMethod au_read_methods[] = {{MS_FILTER_SET_SAMPLE_RATE, set_rate},
                                           {MS_FILTER_GET_SAMPLE_RATE, get_rate},
                                           {MS_FILTER_SET_NCHANNELS, read_set_nchannels},
                                           {MS_FILTER_GET_NCHANNELS, get_nchannels},
                                           {MS_AUDIO_CAPTURE_SET_VOLUME_GAIN, ms_macsnd_capture_set_volume},
                                           {MS_AUDIO_CAPTURE_GET_VOLUME_GAIN, ms_macsnd_capture_get_volume},
                                           {0, NULL}};

static MSFilterMethod au_write_methods[] = {{MS_FILTER_SET_SAMPLE_RATE, set_rate},
                                            {MS_FILTER_GET_SAMPLE_RATE, get_rate},
                                            {MS_FILTER_SET_NCHANNELS, write_set_nchannels},
                                            {MS_FILTER_GET_NCHANNELS, get_nchannels},
                                            {MS_AUDIO_PLAYBACK_SET_VOLUME_GAIN, ms_macsnd_playback_set_volume},
                                            {MS_AUDIO_PLAYBACK_GET_VOLUME_GAIN, ms_macsnd_playback_get_volume},
                                            {0, NULL}};

MSFilterDesc ms_au_read_desc = {.id = MS_CA_READ_ID,
                                .name = "MSAuRead",
                                .text = N_("Sound capture filter for MacOS X Audio Unit"),
                                .category = MS_FILTER_OTHER,
                                .ninputs = 0,
                                .noutputs = 1,
                                .init = au_read_init,
                                .preprocess = au_read_preprocess,
                                .process = au_read_process,
                                .postprocess = au_read_postprocess,
                                .uninit = au_read_uninit,
                                .methods = au_read_methods};

MSFilterDesc ms_au_write_desc = {.id = MS_CA_WRITE_ID,
                                 .name = "MSAuWrite",
                                 .text = N_("Sound playback filter for MacOS X Audio Unit"),
                                 .category = MS_FILTER_OTHER,
                                 .ninputs = 1,
                                 .noutputs = 0,
                                 .init = au_write_init,
                                 .preprocess = au_write_preprocess,
                                 .process = au_write_process,
                                 .postprocess = au_write_postprocess,
                                 .uninit = au_write_uninit,
                                 .methods = au_write_methods};

static void set_audio_device_id(AuCard *wc, AUCommon *d, bool_t is_read) {
	if (strcmp(wc->uidname, "") == 0) {
		d->follow_default = TRUE;
		au_get_default_device_id(&d->dev, is_read);
		return;
	}
	CFStringRef devUid = CFStringCreateWithCString(NULL, wc->uidname, CFStringGetSystemEncoding());
	AudioValueTranslation avt;
	UInt32 len;
	OSStatus err;
	avt.mInputData = (CFStringRef *)(&devUid);
	avt.mInputDataSize = sizeof(CFStringRef *);
	avt.mOutputData = &d->dev;
	avt.mOutputDataSize = sizeof(AudioDeviceID);
	len = sizeof(AudioValueTranslation);

	AudioObjectPropertyAddress theAddress = {kAudioHardwarePropertyDeviceForUID, kAudioObjectPropertyScopeGlobal,
	                                         kAudioObjectPropertyElementMaster};

	err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &theAddress, 0, NULL, &len, &avt);

	/*err = AudioHardwareGetProperty(kAudioHardwarePropertyDeviceForUID, &len, &avt);*/
	if (err != kAudioHardwareNoError || d->dev == 0) {
		ms_warning("[MSAU] Unable to query for AudioDeviceID for [%s], using default instead.", wc->uidname);
		au_get_default_device_id(&d->dev, is_read);
	}
	CFRelease(devUid);
}
MSFilter *ms_au_read_new(MSSndCard *card) {
	MSFilter *f = ms_factory_create_filter_from_desc(ms_snd_card_get_factory(card), &ms_au_read_desc);
	AuCard *wc = (AuCard *)card->data;
	AURead *d = (AURead *)f->data;
	/*d->common.dev = wc->dev;*/
	set_audio_device_id(wc, (struct AUCommon *)d, TRUE);
	d->common.rate = wc->rate;
	return f;
}

MSFilter *ms_au_write_new(MSSndCard *card) {
	MSFilter *f = ms_factory_create_filter_from_desc(ms_snd_card_get_factory(card), &ms_au_write_desc);
	AuCard *wc = (AuCard *)card->data;
	AUWrite *d = (AUWrite *)f->data;
	/*d->common.dev = wc->dev;*/
	set_audio_device_id(wc, (struct AUCommon *)d, FALSE);
	d->common.rate = wc->rate;
	return f;
}

MS_FILTER_DESC_EXPORT(ms_au_read_desc)
MS_FILTER_DESC_EXPORT(ms_au_write_desc)

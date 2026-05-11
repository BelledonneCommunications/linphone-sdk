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
#include <optional>
#include <vector>

#if __LP64__
#define UINT32_PRINTF "u"
#define UINT32_X_PRINTF "x"
#else
#define UINT32_PRINTF "lu"
#define UINT32_X_PRINTF "lx"
#endif

MSFilter *msAuReadNew(MSSndCard *card);
MSFilter *msAuWriteNew(MSSndCard *card);

static const int kFlowControlInterval = 5000; // ms
static const int kFlowControlThreshold = 40;  // ms

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
void showFormat(BCTBX_UNUSED(const char *name), AudioStreamBasicDescription *deviceFormat) {
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
	int nChannels;
	bool_t followDefault;
	bool_t routeChanged;
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
	AudioComponentInstance au;
#else
	AudioUnit au;
#endif
	ms_mutex_t mutex;
	MSSndCard *card;
} AUCommon;

typedef struct AURead {
	AUCommon common;
	queue_t rq;
	MSTickerSynchronizer *tickerSynchronizer;
	uint64_t timestamp;
	bool_t firstProcess;
} AURead;

typedef struct AUWrite {
	AUCommon common;
	MSFlowControlledBufferizer *buffer;
} AUWrite;

class LoopbackInfo {
public:
	int mNbChannelsTotal;
	int mChannelOffset;
	LoopbackInfo(int nbChannelsTotal, int channelOffset) {
		mNbChannelsTotal = nbChannelsTotal;
		mChannelOffset = channelOffset;
	}
};

typedef struct AUCard {
	char *uidName;
	int removed;
	int rate; /*the nominal rate of the device*/

	std::optional<LoopbackInfo> loopbackInfo;
} AUCard;

int getNbChannel(AUCommon const &common) {
	AUCard *card = static_cast<AUCard *>(common.card->data);
	return card->loopbackInfo.has_value() ? card->loopbackInfo.value().mNbChannelsTotal : common.nChannels;
}

int auGetDefaultDeviceId(AudioDeviceID *id, bool_t isRead) {
	UInt32 len = sizeof(AudioDeviceID);
	OSStatus err;
	AudioObjectPropertyAddress theAddress = {isRead ? kAudioHardwarePropertyDefaultInputDevice
	                                                : kAudioHardwarePropertyDefaultOutputDevice,
	                                         kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster};
	err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &theAddress, 0, NULL, &len, id);
	// AudioHardwareGetProperty(is_read?kAudioHardwarePropertyDefaultInputDevice:kAudioHardwarePropertyDefaultOutputDevice,&len,
	// &d->dev);
	if (err != kAudioHardwareNoError) {
		ms_error("[MSAU] Unable to query for default %s AudioDevice", isRead ? "Capture" : "Playback");
		return -1;
	}
	return 0;
}

int auGetDeviceSampleRate(AudioDeviceID dev, bool_t isRead, int *rate) {
	AudioObjectPropertyScope inputScope = isRead ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;
	AudioObjectPropertyAddress streamFormatAddress = {kAudioDevicePropertyStreamFormat, inputScope, 0};
	AudioStreamBasicDescription format = {0};
	UInt32 slen;
	int err;

	err = AudioObjectGetPropertyData(dev, &streamFormatAddress, 0, NULL, &slen, &format);

	//		err = AudioDeviceGetProperty(dev, 0, cap & MS_SND_CARD_CAP_CAPTURE, kAudioDevicePropertyStreamFormat,
	//&slen, &format);
	if (err == kAudioHardwareBadObjectError) {
		ms_warning("[MSAU] Trying to get rate from bad ID %d", (int)dev);
	} else if (err != kAudioHardwareNoError) { // Try another method
		AudioObjectPropertyAddress nominalSampleAddress = {kAudioDevicePropertyNominalSampleRate, inputScope,
		                                                   kAudioObjectPropertyElementMaster};
		err = AudioObjectGetPropertyData(dev, &nominalSampleAddress, 0, NULL, &slen, &format);
	}
	if (err == kAudioHardwareNoError && (int)format.mSampleRate > 0) {
		// show_format("device", &format);
		*rate = format.mSampleRate;
		return 0;
	} else {
		return -1;
	}
}

void auCardSetLevel(BCTBX_UNUSED(MSSndCard *card), BCTBX_UNUSED(MSSndCardMixerElem e), BCTBX_UNUSED(int percent)) {
}

int auCardGetLevel(BCTBX_UNUSED(MSSndCard *card), BCTBX_UNUSED(MSSndCardMixerElem e)) {
	return -1;
}

void auCardSetSource(BCTBX_UNUSED(MSSndCard *card), BCTBX_UNUSED(MSSndCardCapture source)) {
}

void auCardInit(MSSndCard *card) {
	AUCard *c = (AUCard *)ms_new0(AUCard, 1);
	c->removed = 0;
	card->data = c;
}

void auCardUninit(MSSndCard *card) {
	AUCard *d = static_cast<AUCard *>(card->data);
	if (d->uidName != NULL) ms_free(d->uidName);
	ms_free(d);
}

void auCardDetect(MSSndCardManager *m);
void auCardUnload(MSSndCardManager *m);
bool_t auCardReloadRequested(MSSndCardManager *m);
MSSndCard *auCardDuplicate(MSSndCard *obj);

MSSndCardDesc ca_card_desc = {.driver_type = "AudioUnit",
                              .detect = auCardDetect,
                              .init = auCardInit,
                              .set_level = auCardSetLevel,
                              .get_level = auCardGetLevel,
                              .set_capture = auCardSetSource,
                              .create_reader = msAuReadNew,
                              .create_writer = msAuWriteNew,
                              .uninit = auCardUninit,
                              .duplicate = auCardDuplicate,
                              .unload = auCardUnload,
                              .reload_requested = auCardReloadRequested};

MSSndCard *auCardDuplicate(MSSndCard *obj) {
	MSSndCard *card = ms_snd_card_new(&ca_card_desc);
	card->name = ms_strdup(obj->name);
	card->data = ms_new0(AUCard, 1);
	AUCard *ca = static_cast<AUCard *>(obj->data);
	AUCard *cadup = static_cast<AUCard *>(card->data);
	*cadup = *ca;
	return card;
}

void caSetDeviceType(AudioDeviceID deviceId, MSSndCard *card) {
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

MSSndCard *caCardNew(const char *name,
                     const char *uidName,
                     AudioDeviceID dev,
                     unsigned cap,
                     std::optional<LoopbackInfo> loopbackInfo = {}) {
	MSSndCard *card = ms_snd_card_new(&ca_card_desc);
	AUCard *d = (AUCard *)card->data;
	bool isRead = cap & MS_SND_CARD_CAP_CAPTURE;

	d->uidName = ms_strdup(uidName);
	if (dev == (AudioDeviceID)-1) card->name = ms_strdup_printf("%s", name);
	else card->name = ms_strdup_printf("%s (%s)", name, uidName); /*include uid so that names are uniques*/
	card->capabilities = cap;
	if (isRead) {
		card->latency = 40; /* Sound card latency seems always not least than 40ms on mac*/
	}
	d->rate = 44100;
	d->loopbackInfo = loopbackInfo;

	// If default device, use the current device for the case it will not changed between now and using the device. That
	// way, we get the correct rate from start.
	if (dev == (AudioDeviceID)-1) auGetDefaultDeviceId(&dev, isRead);
	auGetDeviceSampleRate(dev, isRead, &d->rate);
	caSetDeviceType(dev, card);
	return card;
}

std::string cfStringToString(CFStringRef str) {
	if (!str) return "";
	CFIndex maxSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8) + 1;
	std::vector<char> buffer(maxSize);
	if (CFStringGetCString(str, buffer.data(), maxSize, kCFStringEncodingUTF8)) {
		return std::string(buffer.data());
	} else {
		return "";
	}
}

bool_t isChannelLoopback(std::string const &channelName) {
	std::string filteredName;
	// Only keep letters, and upper case them
	for (unsigned char c : channelName) {
		if (std::isalpha(c)) {
			filteredName += std::toupper(c);
		}
	}
	return filteredName.find("LOOPBACK") != std::string::npos;
}

UInt32 audioDeviceGetNbChannels(AudioDeviceID deviceID) {
	OSStatus err;
	AudioObjectPropertyAddress addr = {kAudioDevicePropertyStreamConfiguration, kAudioDevicePropertyScopeInput,
	                                   kAudioObjectPropertyElementMaster};

	UInt32 size = 0;
	err = AudioObjectGetPropertyDataSize(deviceID, &addr, 0, NULL, &size);
	if (err != noErr) return 0;

	auto *bufferList = static_cast<AudioBufferList *>(malloc(size));
	err = AudioObjectGetPropertyData(deviceID, &addr, 0, NULL, &size, bufferList);
	if (err != noErr) {
		free(bufferList);
		return 0;
	}

	UInt32 totalChannels = 0;
	for (UInt32 i = 0; i < bufferList->mNumberBuffers; i++) {
		totalChannels += bufferList->mBuffers[i].mNumberChannels;
	}
	return totalChannels;
}

bool_t checkCardCapability(AudioDeviceID id, bool_t isInput, char *devName, char *uidName, size_t nameLen) {
	UInt32 slen = nameLen;
	CFStringRef dUID = NULL;
	bool_t ret = FALSE;
	OSStatus err;
	AudioObjectPropertyScope theScope = isInput ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput;
	AudioObjectPropertyAddress theAddress = {kAudioDevicePropertyDeviceName, theScope, 0};

	err = AudioObjectGetPropertyData(id, &theAddress, 0, NULL, &slen, devName);
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

	AudioBufferList *buffList = static_cast<AudioBufferList *>(ms_malloc(slen));

	theAddress.mSelector = kAudioDevicePropertyStreamConfiguration;
	err = AudioObjectGetPropertyData(id, &theAddress, 0, NULL, &slen, buffList);
	/*err = 	AudioDeviceGetProperty(id, 0, is_input, kAudioDevicePropertyStreamConfiguration, &slen, buflist);*/
	if (err != kAudioHardwareNoError) {
		ms_error("[MSAU] Get kAudioDevicePropertyStreamConfiguration error %" UINT32_PRINTF, err);
		ms_free(buffList);
		return FALSE;
	}

	for (UInt32 j = 0; j < buffList->mNumberBuffers; j++) {
		if (buffList->mBuffers[j].mNumberChannels > 0) {
			ret = TRUE;
			break;
		}
	}
	ms_free(buffList);
	if (ret == FALSE) return FALSE;

	slen = sizeof(CFStringRef);
	theAddress.mSelector = kAudioDevicePropertyDeviceUID;
	err = AudioObjectGetPropertyData(id, &theAddress, 0, NULL, &slen, &dUID);
	// err =AudioDeviceGetProperty(id, 0, is_input, kAudioDevicePropertyDeviceUID, &slen,&dUID);
	if (err != kAudioHardwareNoError) {
		ms_error("[MSAU] Get kAudioDevicePropertyDeviceUID error %" UINT32_PRINTF, err);
		return FALSE;
	}
	CFStringGetCString(dUID, uidName, nameLen, CFStringGetSystemEncoding());
	ms_message("[MSAU] CA: devname:%s uidname:%s", devName, uidName);

	return ret;
}

/* Would be nice if this boolean could be part of the MSSndCardManager */
bool_t reloadRequested = FALSE;

OSStatus auCardListener(BCTBX_UNUSED(AudioObjectID inObjectID),
                        BCTBX_UNUSED(UInt32 inNumberAddresses),
                        BCTBX_UNUSED(const AudioObjectPropertyAddress *inAddresses),
                        BCTBX_UNUSED(void *inClientData)) {
	ms_message("[MSAU] A change happend with the list of available sound devices, will reload");
	reloadRequested = TRUE;
	return 0;
}

void auCardUnload(MSSndCardManager *m) {
	AudioObjectPropertyAddress theAddress = {kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal,
	                                         kAudioObjectPropertyElementMaster};
	OSStatus err = AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &theAddress, auCardListener, m);
	if (err != kAudioHardwareNoError) {
		ms_error("[MSAU] Unable to remove listener for audio objects.");
	} else {
		ms_message("[MSAU] Removed listener for available sound devices.");
	}
}

bool_t auCardReloadRequested(BCTBX_UNUSED(MSSndCardManager *m)) {
	return reloadRequested;
}

void auCardDetect(MSSndCardManager *m) {
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
	std::vector<AudioDeviceID> devices(slen / sizeof(AudioDeviceID));
	err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &theAddress, 0, NULL, &slen, devices.data());
	/*err =
	AudioHardwareGetProperty(kAudioHardwarePropertyDevices, &slen, devices);*/
	if (err != kAudioHardwareNoError) {
		ms_error("get kAudioHardwarePropertyDevices error %" UINT32_PRINTF, err);
		return;
	}

	ms_snd_card_manager_add_card(m, caCardNew("Default Capture", "", -1, MS_SND_CARD_CAP_CAPTURE));
	ms_snd_card_manager_add_card(m, caCardNew("Default Playback", "", -1, MS_SND_CARD_CAP_PLAYBACK));

	count = slen / sizeof(AudioDeviceID);
	for (i = 0; i < count; i++) {
		MSSndCard *card;
		char uidname[256] = {0}, devname[256] = {0};
		AudioDeviceID deviceId = devices[i];
		std::optional<LoopbackInfo> loopbackInfo;
		int card_capacity = 0;
		if (checkCardCapability(deviceId, FALSE, devname, uidname, sizeof(uidname))) {
			card_capacity |= MS_SND_CARD_CAP_PLAYBACK;
		}
		if (checkCardCapability(deviceId, TRUE, devname, uidname, sizeof(uidname))) {
			card_capacity |= MS_SND_CARD_CAP_CAPTURE;
			UInt32 nb_channels = audioDeviceGetNbChannels(deviceId);
			if (nb_channels > 4) {
				ms_warning("Device %s has more than 4 input channels (%d detected), unexpected behaviour could happen",
				           devname, nb_channels);
			}

			// For input devices, we loop through all available channels and try to find a mention of the word
			// "LOOPBACK" after processing the channel name to force uppercase and remove all non-letter caracters On
			// the first loopback channel found, we initialise the loopbackInfo var with its index for later processing
			// in readRenderProc()
			for (UInt32 chanId = 1; chanId <= nb_channels; chanId++) {
				AudioObjectPropertyAddress nameAddr = {kAudioObjectPropertyElementName, kAudioDevicePropertyScopeInput,
				                                       chanId};
				CFStringRef cfName = NULL;
				UInt32 nameSize = sizeof(cfName);
				std::string channelName;
				if (AudioObjectGetPropertyData(deviceId, &nameAddr, 0, NULL, &nameSize, &cfName) == noErr &&
				    cfName != NULL) {
					channelName = cfStringToString(cfName);
					CFRelease(cfName);
				}
				if (isChannelLoopback(channelName)) {
					loopbackInfo = LoopbackInfo(static_cast<int>(nb_channels), static_cast<int>(chanId - 1));
					break;
				}
			}
			if (!loopbackInfo.has_value() && nb_channels == 4 &&
			    std::string(devname).find("EVO4") != std::string::npos) {
				// When upgrading MacOS from Sequoia to Tahoe, channels names for the EVO4 became "3" and "4"
				// While it used to be "Loop-back 1 (L)" and "Loop-back 2 (R)"
				// This is a fallback until hopefully the firmware gets updated
				loopbackInfo = LoopbackInfo(4, 2);
			}
		}

		if (card_capacity) {
			ms_snd_card_manager_add_card(m, caCardNew(devname, uidname, deviceId, card_capacity));
			if (loopbackInfo.has_value()) {
				ms_message("Device %s has loopback channels, add an extra device with only capture capability",
				           devname);
				std::string loopbackDevName = std::string(devname) + " (Loopback)";
				card = caCardNew(loopbackDevName.c_str(), uidname, deviceId, MS_SND_CARD_CAP_CAPTURE, loopbackInfo);
				ms_snd_card_manager_add_card(m, card);
			}
		}
	}

	/* Attempt to remove the property listener possibly previously set - in case of reload */
	AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &theAddress, auCardListener, m);

	err = AudioObjectAddPropertyListener(kAudioObjectSystemObject, &theAddress, auCardListener, m);
	if (err != kAudioHardwareNoError) {
		ms_error("[MSAU] Unable to set listener for audio objects.");
	} else {
		ms_message("[MSAU] Set listener for available sound devices.");
	}
	reloadRequested = FALSE;
}

OSStatus readRenderProc(void *inRefCon,
                        AudioUnitRenderActionFlags *inActionFlags,
                        const AudioTimeStamp *inTimeStamp,
                        UInt32 inBusNumber,
                        UInt32 inNumFrames,
                        BCTBX_UNUSED(AudioBufferList *ioData)) {
	AURead *d = (AURead *)inRefCon;
	AUCard *auCard = static_cast<AUCard *>(d->common.card->data);

	AudioBufferList lreadAudioBufferList = {0};
	mblk_t *rm;
	OSStatus err;

	bool hasLoopback = auCard->loopbackInfo.has_value();
	int nbChannels = getNbChannel(d->common);
	lreadAudioBufferList.mNumberBuffers = 1;
	lreadAudioBufferList.mBuffers[0].mDataByteSize = inNumFrames * sizeof(int16_t) * nbChannels;
	rm = allocb(lreadAudioBufferList.mBuffers[0].mDataByteSize, 0);
	lreadAudioBufferList.mBuffers[0].mData = rm->b_wptr;
	lreadAudioBufferList.mBuffers[0].mNumberChannels = nbChannels;

	err = AudioUnitRender(d->common.au, inActionFlags, inTimeStamp, inBusNumber, inNumFrames, &lreadAudioBufferList);

	if (err != noErr) {
		ms_error("[MSAU] AudioUnitRender() for read returned [%" UINT32_PRINTF "] %s %s", err,
		         GetMacOSStatusErrorString(err), GetMacOSStatusCommentString(err));
		return 0;
	}

	mblk_t *rmExtracted;
	if (hasLoopback) {
		// Example: NbChannelsTotal = 4, ChannelOffset = 2
		// src buffer : [a, b, c, d] * inNumFrames
		// dst buffer :       [c] * inNumFrames if d->common.nchannels == 1
		//                    [c, d] * inNumFrames if d->common.nchannels == 2
		rmExtracted = allocb(inNumFrames * sizeof(int16_t) * d->common.nChannels, 0);
		int16_t *src = (int16_t *)rm->b_wptr;
		int16_t *dst = (int16_t *)rmExtracted->b_wptr;
		for (UInt32 i = 0; i < inNumFrames; i++) {
			for (int iChannel = 0; iChannel < d->common.nChannels; iChannel++) {
				dst[i * d->common.nChannels + iChannel] =
				    src[i * nbChannels + auCard->loopbackInfo.value().mChannelOffset + iChannel];
			}
		}
		rmExtracted->b_wptr += inNumFrames * sizeof(int16_t) * d->common.nChannels;
	}
	rm->b_wptr += lreadAudioBufferList.mBuffers[0].mDataByteSize;
	ms_mutex_lock(&d->common.mutex);
	if (inTimeStamp->mFlags & kAudioTimeStampSampleTimeValid) {
		d->timestamp = inTimeStamp->mSampleTime;
	}

	if (hasLoopback) {
		putq(&d->rq, rmExtracted);
		freemsg(rm);
	} else {
		putq(&d->rq, rm);
	}
	ms_mutex_unlock(&d->common.mutex);

	return 0;
}

OSStatus writeRenderProc(void *inRefCon,
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
	read = ms_flow_controlled_bufferizer_read(d->buffer, static_cast<uint8_t *>(ioData->mBuffers[0].mData),
	                                          ioData->mBuffers[0].mDataByteSize);
	ms_mutex_unlock(&d->common.mutex);
	if (read == 0) {
		ms_debug("[MSAU] Silence inserted in audio output unit (%" UINT32_PRINTF " bytes)",
		         ioData->mBuffers[0].mDataByteSize);
		memset(ioData->mBuffers[0].mData, 0, ioData->mBuffers[0].mDataByteSize);
	}
	return 0;
}

AudioStreamBasicDescription auGetStreamBasicDscription(AUCommon *d, bool_t isRead) {
	OSStatus result;
	UInt32 param;
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
	AudioComponentDescription desc;
	AudioComponent comp;
#else
	ComponentDescription desc;
	Component comp;
#endif
	AudioStreamBasicDescription asbd = {0};
	const int inputBus = 1;
	const int outputBus = 0;
	int deviceRate = 0;
	AudioUnitScope inputScope = isRead ? kAudioUnitScope_Input : kAudioUnitScope_Output;
	AudioUnitElement element = isRead ? inputBus : outputBus;
	UInt32 asbdSize = sizeof(AudioStreamBasicDescription);
	// This is undocumented and this note is based only on user feedbacks :
	// kAudioUnitSubType_DefaultOutput cannot be used for microphones.
	// Also it appears that some properties cannot be set like format/buffer storage.
	// If there are issues on data that cannot be retrieved/set (like sample rates),
	// set componentSubType to kAudioUnitSubType_HALOutput on is_read

	if (d->followDefault) { // Get Default device
		auGetDefaultDeviceId(&d->dev, isRead);
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
		return asbd;
	}
#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_5
	result = AudioComponentInstanceNew(comp, &d->au);
#else
	result = OpenAComponent(comp, &d->au);
#endif
	if (result != noErr) {
		ms_error("[MSAU] Cannot open audio component %" UINT32_X_PRINTF, result);
		return asbd;
	}
	param = isRead;
	if (d->dev != (AudioDeviceID)-1) { // Cannot manually enabling IO for defaults
		CHECK_AURESULT(AudioUnitSetProperty(d->au, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, inputBus,
		                                    &param, sizeof(UInt32)));

		param = !isRead;
		CHECK_AURESULT(AudioUnitSetProperty(d->au, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, outputBus,
		                                    &param, sizeof(UInt32)));

		// Set the current device
		CHECK_AURESULT(AudioUnitSetProperty(d->au, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global,
		                                    outputBus, &d->dev, sizeof(AudioDeviceID)));
	}
	param = 0;
	CHECK_AURESULT(AudioUnitSetProperty(d->au, kAudioUnitProperty_ShouldAllocateBuffer, inputScope, element, &param,
	                                    sizeof(param)));
	memset((char *)&asbd, 0, asbdSize);
	CHECK_AURESULT(AudioUnitGetProperty(d->au, kAudioUnitProperty_StreamFormat, inputScope, element, &asbd, &asbdSize));

	// Device rate (especially with HAL) is better than AudioUnit
	if (!auGetDeviceSampleRate(d->dev, isRead, &deviceRate)) {
		asbd.mSampleRate = deviceRate;
	}
	return asbd;
}

OSStatus audioUnitDefaultListener(BCTBX_UNUSED(AudioObjectID inObjectID),
                                  BCTBX_UNUSED(UInt32 inNumberAddresses),
                                  BCTBX_UNUSED(const AudioObjectPropertyAddress *inAddresses),
                                  BCTBX_UNUSED(void *inClientData)) {
	AUCommon *common = (AUCommon *)inClientData;
	ms_message("[MSAU] Default %s audio route changed detected",
	           inAddresses->mSelector == kAudioHardwarePropertyDefaultInputDevice ? "Capture" : "Playback");
	ms_mutex_lock(&common->mutex);
	common->routeChanged = TRUE;
	reloadRequested = TRUE;
	ms_mutex_unlock(&common->mutex);
	return 0;
}

void audioUnitNotifRegister(AUCommon *common, bool_t isRead) {
	ms_message("[MSAU] Registering to default devices changes for %s", isRead ? "Capture" : "Playback");
	AudioObjectPropertyAddress audioDevicesAddress = {
	    isRead ? kAudioHardwarePropertyDefaultInputDevice : kAudioHardwarePropertyDefaultOutputDevice,
	    kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster};

	CHECK_AURESULT(AudioObjectAddPropertyListener(kAudioObjectSystemObject, &audioDevicesAddress,
	                                              audioUnitDefaultListener, common));
}

void audioUnitNotifUnregister(AUCommon *common, bool_t isRead) {
	AudioObjectPropertyAddress audioDevicesAddress = {
	    isRead ? kAudioHardwarePropertyDefaultInputDevice : kAudioHardwarePropertyDefaultOutputDevice,
	    kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster};
	CHECK_AURESULT(AudioObjectRemovePropertyListener(kAudioObjectSystemObject, &audioDevicesAddress,
	                                                 audioUnitDefaultListener, common));
}

int audioUnitOpen(MSFilter *f, AUCommon *d, bool_t isRead) {
	OSStatus result;
	UInt32 param = 0;
	AudioStreamBasicDescription asbd = auGetStreamBasicDscription(d, isRead);
	UInt32 asbdSize = sizeof(AudioStreamBasicDescription);
	const int inputBus = 1;
	const int outputBus = 0;
	AudioUnitScope outputScope = isRead ? kAudioUnitScope_Output : kAudioUnitScope_Input;
	AudioUnitElement element = isRead ? inputBus : outputBus;

	if (asbd.mSampleRate != d->rate && asbd.mSampleRate > 0) {
		// Filter rate and device rate are different. HAL cannot manage different rates alone.
		// If rate is forced, we should need to use a resampling unit (kAudioUnitType_FormatConverter) and add it into a
		// graph. As MS2 have his own resampler (which is better), reload format with new rate and warn MS2 for the
		// change.
		ms_message("[MSAU] %s filter and device have different rate %d/%d", isRead ? "Input" : "Output",
		           (int)asbd.mSampleRate, d->rate);
		d->rate = asbd.mSampleRate;
		ms_filter_notify_no_arg(f, MS_FILTER_OUTPUT_FMT_CHANGED);
	}
	// Keep this afftection in case where mSampleRate is 0. Getter can give 0 but setter doesn't accept it. Use the
	// default rate.
	asbd.mSampleRate = d->rate;
	int nbChannels = getNbChannel(*d);
	asbd.mBytesPerPacket = asbd.mBytesPerFrame = 2 * nbChannels;
	asbd.mChannelsPerFrame = nbChannels;
	asbd.mBitsPerChannel = 16;
	asbd.mFormatID = kAudioFormatLinearPCM;
	asbd.mFormatFlags = kAudioFormatFlagIsPacked | kAudioFormatFlagIsSignedInteger;

	CHECK_AURESULT(AudioUnitSetProperty(d->au, kAudioUnitProperty_StreamFormat, outputScope, element, &asbd,
	                                    sizeof(AudioStreamBasicDescription)));

	CHECK_AURESULT(
	    AudioUnitGetProperty(d->au, kAudioUnitProperty_StreamFormat, outputScope, element, &asbd, &asbdSize));

	showFormat(isRead ? "Input audio unit after configuration" : "Output audio unit after configuration", &asbd);

	// Attempt to set the I/O buffer size
	param = sizeof(UInt32);
	UInt32 numFrames = 128;
	CHECK_AURESULT(
	    AudioUnitSetProperty(d->au, kAudioDevicePropertyBufferFrameSize, kAudioUnitScope_Global, 0, &numFrames, param));

	// Get the number of frames in the IO buffer(s)
	param = sizeof(UInt32);
	CHECK_AURESULT(
	    AudioUnitGetProperty(d->au, kAudioDevicePropertyBufferFrameSize, outputScope, element, &numFrames, &param));
	ms_message("[MSAU] %s: number of frames per buffer = %" UINT32_PRINTF,
	           isRead ? "Input AudioUnit" : "Output AudioUnit", numFrames);

	if (!isRead) {
		/* Latency is only provided for output AudioUnit */
		param = sizeof(UInt32);
		UInt32 latency = 0;
		CHECK_AURESULT(
		    AudioUnitGetProperty(d->au, kAudioDevicePropertyLatency, outputScope, element, &latency, &param));
		ms_message("[MSAU] Latency = %" UINT32_PRINTF, latency);
	}

	param = sizeof(int);
	int safetyOffset = 0;
	CHECK_AURESULT(
	    AudioUnitGetProperty(d->au, kAudioDevicePropertySafetyOffset, outputScope, element, &safetyOffset, &param));
	ms_message("[MSAU] Safety offset = %i", safetyOffset);

	AURenderCallbackStruct cbs;

	cbs.inputProcRefCon = d;
	if (isRead) {
		cbs.inputProc = readRenderProc;
		CHECK_AURESULT(AudioUnitSetProperty(d->au, kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global, 0,
		                                    &cbs, sizeof(AURenderCallbackStruct)));
	} else {
		cbs.inputProc = writeRenderProc;
		CHECK_AURESULT(AudioUnitSetProperty(d->au, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Global,
		                                    outputBus, &cbs, sizeof(AURenderCallbackStruct)));
	}
	result = AudioUnitInitialize(d->au);
	if (result != noErr) {
		ms_error("[MSAU] failed to AudioUnitInitialize %" UINT32_PRINTF " , is_read=%i", result, (int)isRead);
		return -1;
	}

	CHECK_AURESULT(AudioOutputUnitStart(d->au));
	return 0;
}

void audioUnitClose(AUCommon *d) {
	if (d->au) {
		CHECK_AURESULT(AudioOutputUnitStop(d->au));
		CHECK_AURESULT(AudioUnitUninitialize(d->au));
		d->au = NULL;
	}
}

void auWritePut(AUWrite *d, mblk_t *m) {
	ms_mutex_lock(&d->common.mutex);
	ms_flow_controlled_bufferizer_put(d->buffer, m);
	ms_mutex_unlock(&d->common.mutex);
}

void auCommonInit(AUCommon *d) {
	d->rate = 44100;
	d->nChannels = 1;
	ms_mutex_init(&d->mutex, NULL);
}

void auCommonUninit(AUCommon *d) {
	ms_snd_card_unref(d->card);
	ms_mutex_destroy(&d->mutex);
}

// 1 = Changed
int auCommonCheckRouteChanged(MSFilter *f, AUCommon *common, bool_t isRead) {
	if (common->routeChanged) {
		common->routeChanged = FALSE;
		MSAudioRouteChangedEvent ev;
		memset(&ev, 0, sizeof(ev));
		ev.need_update_device_list = true;
		ms_filter_notify(f, MS_AUDIO_ROUTE_CHANGED, &ev);
		audioUnitClose(common);
		audioUnitOpen(f, common, isRead);
		return 1;
	} else return 0;
}

void auReadInit(MSFilter *f) {
	AURead *d = ms_new0(AURead, 1);
	auCommonInit(&d->common);
	qinit(&d->rq);
	d->tickerSynchronizer = ms_ticker_synchronizer_new();
	f->data = d;
}

void auReadPreprocess(MSFilter *f) {
	AURead *d = (AURead *)f->data;
	ms_ticker_set_synchronizer(f->ticker, d->tickerSynchronizer);
	audioUnitOpen(f, &d->common, TRUE);
	d->firstProcess = TRUE;
	if (d->common.followDefault) audioUnitNotifRegister(&d->common, TRUE);
}
void auReadProcess(MSFilter *f) {
	AURead *d = (AURead *)f->data;
	mblk_t *m;

	ms_mutex_lock(&d->common.mutex);
	if (auCommonCheckRouteChanged(f, &d->common, TRUE)) flushq(&d->rq, 0);
	else if (d->firstProcess) {
		/* The read queue must be flushed on first
		 * process() call because it contains samples
		 * that has been produced since the ticker
		 * was attached. Do not skiped this data would
		 * make the clock skew estimation to be wrong. */
		d->firstProcess = FALSE;
		flushq(&d->rq, 0);
	} else {
		bool_t got_something = FALSE;
		while ((m = getq(&d->rq)) != NULL) {
			ms_queue_put(f->outputs[0], m);
			got_something = TRUE;
		}
		if (got_something) ms_ticker_synchronizer_update(d->tickerSynchronizer, d->timestamp, d->common.rate);
	}
	ms_mutex_unlock(&d->common.mutex);
}

void auReadPostprocess(MSFilter *f) {
	AURead *d = (AURead *)f->data;
	if (d->common.followDefault) audioUnitNotifUnregister(&d->common, TRUE);
	audioUnitClose(&d->common);
	ms_ticker_set_synchronizer(f->ticker, NULL);
}

void auReadUninit(MSFilter *f) {
	AURead *d = (AURead *)f->data;
	flushq(&d->rq, 0);
	ms_ticker_synchronizer_destroy(d->tickerSynchronizer);
	auCommonUninit(&d->common);
	ms_free(d);
}

/* Audio unit write filter */

void auWriteInit(MSFilter *f) {
	AUWrite *d = ms_new0(AUWrite, 1);
	auCommonInit(&d->common);
	d->buffer = ms_flow_controlled_bufferizer_new(f, d->common.rate, d->common.nChannels);
	ms_flow_controlled_bufferizer_set_max_size_ms(d->buffer, kFlowControlThreshold);
	ms_flow_controlled_bufferizer_set_flow_control_interval_ms(d->buffer, kFlowControlInterval);
	f->data = d;
}

void auWritePreprocess(MSFilter *f) {
	AUWrite *d = (AUWrite *)f->data;
	audioUnitOpen(f, &d->common, FALSE);
	if (d->common.followDefault) audioUnitNotifRegister(&d->common, FALSE);
}

void auWriteProcess(MSFilter *f) {
	AUWrite *d = (AUWrite *)f->data;
	mblk_t *m;
	if (auCommonCheckRouteChanged(f, &d->common, FALSE)) ms_queue_flush(f->inputs[0]);
	else
		while ((m = ms_queue_get(f->inputs[0])) != NULL) {
			auWritePut(d, m);
		}
}

void auWritePostprocess(MSFilter *f) {
	AUWrite *d = (AUWrite *)f->data;
	if (d->common.followDefault) audioUnitNotifUnregister(&d->common, FALSE);
	audioUnitClose(&d->common);
}

void auWriteUninit(MSFilter *f) {
	AUWrite *d = (AUWrite *)f->data;
	ms_flow_controlled_bufferizer_destroy(d->buffer);
	auCommonUninit(&d->common);
	ms_free(d);
}

int setRate(MSFilter *f, void *arg) {
	AUCommon *d = (AUCommon *)f->data;
	/*the hal audio unit does not accept custom rates whitout restarting audio unit and check applying new format
	 * (TODO)*/
	return (d->rate == *(int *)arg) ? 0 : -1;
}

int getRate(MSFilter *f, void *arg) {
	AUCommon *d = (AUCommon *)f->data;
	*((int *)arg) = d->rate;
	return 0;
}

int readSetNChannels(MSFilter *f, void *arg) {
	AURead *d = (AURead *)f->data;
	d->common.nChannels = *((int *)arg);
	return 0;
}

int writeSetNChannels(MSFilter *f, void *arg) {
	AUWrite *d = (AUWrite *)f->data;
	d->common.nChannels = *((int *)arg);
	ms_flow_controlled_bufferizer_set_nchannels(d->buffer, d->common.nChannels);
	return 0;
}

int getNChannels(MSFilter *f, void *arg) {
	AUCommon *d = (AUCommon *)f->data;
	*((int *)arg) = d->nChannels;
	return 0;
}

int msMacSndGetVolume(MSFilter *f, void *arg, bool isCapture) {
	AUCommon *d = (AUCommon *)f->data;
	float *pVolume = (float *)arg;
	AudioObjectPropertyAddress volumeAddr = {
	    kAudioHardwareServiceDeviceProperty_VirtualMasterVolume /*kAudioDevicePropertyVolumeScalar*/,
	    isCapture ? kAudioDevicePropertyScopeInput : kAudioDevicePropertyScopeOutput,
	    kAudioObjectPropertyElementMaster};

	UInt32 size = sizeof(*pVolume);

	OSStatus err = AudioObjectGetPropertyData(d->dev, &volumeAddr, 0, NULL, &size, pVolume);

	if (err != noErr) {
		ms_warning("[MSAU] Cannot get capture volume from #[%d]. Err = %d", d->dev, err);
		*pVolume = 0.0f;
		return -1;
	}

	return 0;
}

int msMacSndCaptureGetVolume(MSFilter *f, void *arg) {
	return msMacSndGetVolume(f, arg, true);
}

int msMacSndPlaybackGetVolume(MSFilter *f, void *arg) {
	return msMacSndGetVolume(f, arg, false);
}

int msMacSndSetVolume(MSFilter *f, void *arg, bool isCapture) {
	AUCommon *d = (AUCommon *)f->data;
	float *pVolume = (float *)arg;
	Boolean isWritable;
	AudioObjectPropertyAddress volumeAddr = {kAudioHardwareServiceDeviceProperty_VirtualMasterVolume,
	                                         isCapture ? kAudioDevicePropertyScopeInput
	                                                   : kAudioDevicePropertyScopeOutput,
	                                         kAudioObjectPropertyElementMaster};

	OSStatus err = AudioObjectIsPropertySettable(d->dev, &volumeAddr, &isWritable);

	if (err == noErr) {
		if (isWritable) {
			err = AudioObjectSetPropertyData(d->dev, &volumeAddr, 0, NULL, sizeof(float), pVolume);
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

int msMacSndCaptureSetVolume(MSFilter *f, void *arg) {
	return msMacSndSetVolume(f, arg, true);
}

int msMacSndPlaybackSetVolume(MSFilter *f, void *arg) {
	return msMacSndSetVolume(f, arg, false);
}

MSFilterMethod auReadMethods[] = {{MS_FILTER_SET_SAMPLE_RATE, setRate},
                                  {MS_FILTER_GET_SAMPLE_RATE, getRate},
                                  {MS_FILTER_SET_NCHANNELS, readSetNChannels},
                                  {MS_FILTER_GET_NCHANNELS, getNChannels},
                                  {MS_AUDIO_CAPTURE_SET_VOLUME_GAIN, msMacSndCaptureSetVolume},
                                  {MS_AUDIO_CAPTURE_GET_VOLUME_GAIN, msMacSndCaptureGetVolume},
                                  {0, NULL}};

MSFilterMethod auWriteMethods[] = {{MS_FILTER_SET_SAMPLE_RATE, setRate},
                                   {MS_FILTER_GET_SAMPLE_RATE, getRate},
                                   {MS_FILTER_SET_NCHANNELS, writeSetNChannels},
                                   {MS_FILTER_GET_NCHANNELS, getNChannels},
                                   {MS_AUDIO_PLAYBACK_SET_VOLUME_GAIN, msMacSndPlaybackSetVolume},
                                   {MS_AUDIO_PLAYBACK_GET_VOLUME_GAIN, msMacSndPlaybackGetVolume},
                                   {0, NULL}};

MSFilterDesc msAuReadDesc = {.id = MS_CA_READ_ID,
                             .name = "MSAuRead",
                             .text = N_("Sound capture filter for MacOS X Audio Unit"),
                             .category = MS_FILTER_OTHER,
                             .ninputs = 0,
                             .noutputs = 1,
                             .init = auReadInit,
                             .preprocess = auReadPreprocess,
                             .process = auReadProcess,
                             .postprocess = auReadPostprocess,
                             .uninit = auReadUninit,
                             .methods = auReadMethods};

MSFilterDesc msAuWriteDesc = {.id = MS_CA_WRITE_ID,
                              .name = "MSAuWrite",
                              .text = N_("Sound playback filter for MacOS X Audio Unit"),
                              .category = MS_FILTER_OTHER,
                              .ninputs = 1,
                              .noutputs = 0,
                              .init = auWriteInit,
                              .preprocess = auWritePreprocess,
                              .process = auWriteProcess,
                              .postprocess = auWritePostprocess,
                              .uninit = auWriteUninit,
                              .methods = auWriteMethods};

void setAudioDeviceId(AUCard *wc, AUCommon *d, bool_t isRead) {
	if (strcmp(wc->uidName, "") == 0) {
		d->followDefault = TRUE;
		auGetDefaultDeviceId(&d->dev, isRead);
		return;
	}
	CFStringRef devUid = CFStringCreateWithCString(NULL, wc->uidName, CFStringGetSystemEncoding());
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
		ms_warning("[MSAU] Unable to query for AudioDeviceID for [%s], using default instead.", wc->uidName);
		auGetDefaultDeviceId(&d->dev, isRead);
	}
	CFRelease(devUid);
}
MSFilter *msAuReadNew(MSSndCard *card) {
	MSFilter *f = ms_factory_create_filter_from_desc(ms_snd_card_get_factory(card), &msAuReadDesc);
	AUCard *wc = (AUCard *)card->data;
	AURead *d = (AURead *)f->data;
	d->common.card = ms_snd_card_ref(card);
	/*d->common.dev = wc->dev;*/
	setAudioDeviceId(wc, &d->common, TRUE);
	AudioStreamBasicDescription asbd = auGetStreamBasicDscription(&d->common, TRUE);
	d->common.rate = asbd.mSampleRate;
	ms_message("[MSAU] Init with rate=%d for capture", d->common.rate);
	return f;
}

MSFilter *msAuWriteNew(MSSndCard *card) {
	MSFilter *f = ms_factory_create_filter_from_desc(ms_snd_card_get_factory(card), &msAuWriteDesc);
	AUCard *wc = (AUCard *)card->data;
	AUWrite *d = (AUWrite *)f->data;
	d->common.card = ms_snd_card_ref(card);
	/*d->common.dev = wc->dev;*/
	setAudioDeviceId(wc, &d->common, FALSE);
	AudioStreamBasicDescription asbd = auGetStreamBasicDscription(&d->common, FALSE);
	d->common.rate = asbd.mSampleRate;
	ms_message("[MSAU] Init with rate=%d for playback", d->common.rate);
	return f;
}

MS_FILTER_DESC_EXPORT(msAuReadDesc);
MS_FILTER_DESC_EXPORT(msAuWriteDesc)

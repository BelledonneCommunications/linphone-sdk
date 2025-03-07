/*
 * Copyright (c) 2010-2025 Belledonne Communications SARL.
 *
 * This file is part of mswasapi library - modular sound and video processing and streaming Windows Audio Session API
 * sound card plugin for mediastreamer2 (see https://gitlab.linphone.org/BC/public/mswasapi).
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

#include "mswasapi_reader.h"
#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msticker.h"
#include "utils.h"

#define REFTIME_250MS 2500000
static const int minBufferDurationMs = 200; // ms

#ifdef MS2_WINDOWS_UNIVERSAL
bool MSWASAPIReader::smInstantiated = false;
#endif

MSWASAPIReader::MSWASAPIReader(MSFilter *filter)
    : MSWasapi(filter, MS_SND_CARD_CAP_CAPTURE), mAudioCaptureClient(NULL) {
	mTickerSynchronizer = ms_ticker_synchronizer_new();
}

MSWASAPIReader::~MSWASAPIReader() {
	ms_ticker_synchronizer_destroy(mTickerSynchronizer);
}

int MSWASAPIReader::activate() {
	return MSWasapi::activate(true, (void **)&mAudioCaptureClient);
}

int MSWASAPIReader::deactivate() {
	SAFE_RELEASE(mAudioCaptureClient)
	return MSWasapi::deactivate();
}

void MSWASAPIReader::start() {
	MSWasapi::start();
	if (mIsStarted) {
		mSampleTime = (UINT64)-1;
		ms_ticker_set_synchronizer(mFilter->ticker, mTickerSynchronizer);
	}
}

void MSWASAPIReader::stop() {
	MSWasapi::stop();
	ms_ticker_set_synchronizer(mFilter->ticker, nullptr);
}

int MSWASAPIReader::feed(MSFilter *f) {
	HRESULT result;
	DWORD flags;
	BYTE *pData;
	UINT32 numFramesAvailable;
	UINT32 numFramesInNextPacket = 0;
	UINT64 devicePosition = (UINT64)-1;
	mblk_t *m;
	int bytesPerFrame = mNBlockAlign;

	if (isStarted()) {
		result = mAudioCaptureClient->GetNextPacketSize(&numFramesInNextPacket);
		while (numFramesInNextPacket != 0) {
			REPORT_ERROR("mswasapi: Could not get next packet size for the MSWASAPI audio input interface [%x]",
			             result);

			result = mAudioCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, &devicePosition, NULL);
			REPORT_ERROR("mswasapi: Could not get buffer from the MSWASAPI audio input interface [%x]", result);
			if (numFramesAvailable > 0) {
				m = allocb(numFramesAvailable * bytesPerFrame, 0);
				if (m == NULL) {
					ms_error("mswasapi: Could not allocate memory for the captured data from the MSWASAPI audio input "
					         "interface");
					goto error;
				}

				if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
					memset(m->b_wptr, 0, numFramesAvailable * bytesPerFrame);
				} else {
					memcpy(m->b_wptr, pData, numFramesAvailable * bytesPerFrame);
				}
				result = mAudioCaptureClient->ReleaseBuffer(numFramesAvailable);
				REPORT_ERROR("mswasapi: Could not release buffer of the MSWASAPI audio input interface [%x]", result);

				m->b_wptr += numFramesAvailable * bytesPerFrame;
				// Ticker has been desynchronized either by not setting the device position, or by being erroneous.
				// It's supposed that when having an error, the position from GetBuffer cannot be reliable.
				// Frame counting is done since then.
				if (mSampleTime != (UINT64)-1) {
					mSampleTime += numFramesAvailable;
					devicePosition = mSampleTime;
				} else if ((flags & AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR) || devicePosition == (UINT64)-1) {
					ms_warning("mswasapi: Timestamp is erroneous. Switch to frame counting.");
					mSampleTime = numFramesAvailable;
					devicePosition = mSampleTime;
					ms_ticker_synchronizer_resync(mTickerSynchronizer);
				}

				ms_ticker_synchronizer_update(mTickerSynchronizer, devicePosition, (unsigned int)mCurrentRate);

				ms_queue_put(f->outputs[0], m);
				result = mAudioCaptureClient->GetNextPacketSize(&numFramesInNextPacket);
			}
		}
	} else {
		silence(f);
	}
	return 0;

error:
	return -1;
}

void MSWASAPIReader::silence(MSFilter *f) {
	mblk_t *om;
	unsigned int bufsize;
	unsigned int nsamples;
	nsamples = (f->ticker->interval * mTargetRate) / 1000;
	bufsize = nsamples * mTargetNChannels * 2;
	om = allocb(bufsize, 0);
	memset(om->b_wptr, 0, bufsize);
	om->b_wptr += bufsize;
	ms_queue_put(f->outputs[0], om);
}

#ifndef MS2_WINDOWS_PHONE
// Called from main mswasapi.
MSWASAPIReaderPtr MSWASAPIReaderNew(MSFilter *f) {
	MSWASAPIReaderPtr w = new MSWASAPIReaderWrapper();
#ifdef MS2_WINDOWS_UNIVERSAL
	w->reader = Make<MSWASAPIReader>(f);
#else
	w->reader = new MSWASAPIReader(f);
#endif
	return w;
}
void MSWASAPIReaderDelete(MSWASAPIReaderPtr ptr) {
#ifdef MS2_WINDOWS_UNIVERSAL
	ptr->reader->setInstantiated(false);
#endif
	ptr->reader->unregister();
	ptr->reader->Release();
	ptr->reader = nullptr;
	delete ptr;
}
#else
MSWASAPIReaderPtr MSWASAPIReaderNew(MSFilter *f) {
	return (MSWASAPIReaderPtr) new MSWASAPIReader(f);
}

void MSWASAPIReaderDelete(MSWASAPIReaderPtr ptr) {
	ptr->reader->unregister();
	delete ptr;
}
#endif

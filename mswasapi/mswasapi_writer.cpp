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

#include "mswasapi_writer.h"
#include "mediastreamer2/flowcontrol.h"
#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msticker.h"
#include "utils.h"

static const int flowControlInterval = 5000; // ms
static const int flowControlThreshold = 40;  // ms

#ifdef MS2_WINDOWS_UNIVERSAL
bool MSWASAPIWriter::smInstantiated = false;
#endif

MSWASAPIWriter::MSWASAPIWriter(MSFilter *filter)
    : MSWasapi(filter, MS_SND_CARD_CAP_PLAYBACK), mAudioRenderClient(NULL) {
}

MSWASAPIWriter::~MSWASAPIWriter() {
}

int MSWASAPIWriter::activate() {
	return MSWasapi::activate(false, (void **)&mAudioRenderClient);
}

int MSWASAPIWriter::deactivate() {
	SAFE_RELEASE(mAudioRenderClient)
	return MSWasapi::deactivate();
}

/* We go here everytick, as the filter is declared with the IS_PUMP flag.*/
int MSWASAPIWriter::feed(MSFilter *f) {
	HRESULT result;
	UINT32 numFramesPadding = 0;
	UINT32 numFramesWritable;
	mblk_t *im;

	if (!isStarted()) goto error;

	result = mAudioClient->GetCurrentPadding(&numFramesPadding);
	REPORT_ERROR("mswasapi: Could not get current buffer padding for the MSWASAPI audio output interface [%x]", result);
	numFramesWritable = mBufferFrameCount - numFramesPadding;
	REPORT_ERROR("mswasapi: Could not get the frame size for the MSWASAPI audio output interface [%x]", result);

	while ((im = ms_queue_get(f->inputs[0])) != NULL) {
		int inputFrames = (int)(msgdsize(im) / mNBlockAlign);
		int writtenFrames = inputFrames;
		if (msgdsize(im) % 8 !=
		    0) // Get one more space to put unexpected data. This is a workaround to a bug from special audio driver
		       // that could write/use outside the requested buffer and where it is not on fully 8 octets
			++inputFrames;
		msgpullup(im, -1);
		if (inputFrames >= (int)numFramesWritable) {
			/*This case should not happen because of upstream flow control, except in rare disaster cases.*/
			if (mMinFrameCount != -1)
				ms_error("mswasapi: cannot write output buffer of %i samples, not enough space [%i=%i-%i].",
				         inputFrames, numFramesWritable, mBufferFrameCount, numFramesPadding);
		} else {
			BYTE *buffer;
			result = mAudioRenderClient->GetBuffer(inputFrames, &buffer);
			if (result == S_OK) {
				memcpy(buffer, im->b_rptr, im->b_wptr - im->b_rptr);
				result = mAudioRenderClient->ReleaseBuffer(writtenFrames, 0); // Use only the needed frame
			} else {
				ms_error("mswasapi: Could not get buffer from the MSWASAPI audio output interface %i [%x]", inputFrames,
				         result);
			}
		}
		freemsg(im);
	}
	/* Compute the minimum number of queued samples during a 5 seconds period.*/
	if (mMinFrameCount == -1) mMinFrameCount = numFramesPadding;
	if ((int)numFramesPadding < mMinFrameCount) mMinFrameCount = (int)numFramesPadding;
	if (f->ticker->time % flowControlInterval == 0) {
		int minExcessMs = (mMinFrameCount * 1000) / mCurrentRate;
		if (minExcessMs >= flowControlThreshold) {
			/* Send a notification to request the flow controller to drop samples in excess.*/
			MSAudioFlowControlDropEvent ev;
			ev.flow_control_interval_ms = flowControlInterval;
			ev.drop_ms = minExcessMs / 2;
			if (ev.drop_ms > 0) {
				ms_warning("mswasapi: output buffer was filled with at least %i ms in the last %i ms, asking to drop.",
				           (int)ev.drop_ms, flowControlInterval);
				ms_filter_notify(f, MS_AUDIO_FLOW_CONTROL_DROP_EVENT, &ev);
			}
		} else {
			ms_debug("mswasapi: output buffer was filled with a minimum of %i ms in the last %i ms, all is good.",
			         (int)((mMinFrameCount * 1000) / mCurrentRate), flowControlInterval);
		}
		mMinFrameCount = -1;
	}

error:
	ms_queue_flush(f->inputs[0]);
	return 0;
}

#ifndef MS2_WINDOWS_PHONE
// Called from main mswasapi.
MSWASAPIWriterPtr MSWASAPIWriterNew(MSFilter *f) {
	MSWASAPIWriterPtr w = new MSWASAPIWriterWrapper();
#ifdef MS2_WINDOWS_UNIVERSAL
	w->writer = Make<MSWASAPIWriter>(f);
#else
	w->writer = new MSWASAPIWriter(f);
#endif
	return w;
}
void MSWASAPIWriterDelete(MSWASAPIWriterPtr ptr) {
#ifdef MS2_WINDOWS_UNIVERSAL
	ptr->writer->setInstantiated(false);
#endif
	ptr->writer->unregister();
	ptr->writer->Release();
	ptr->writer = nullptr;
	delete ptr;
}
#else
MSWASAPIWriterPtr MSWASAPIWriterNew(MSFilter *f) {
	return (MSWASAPIWriterPtr) new MSWASAPIWriter(f);
}

void MSWASAPIWriterDelete(MSWASAPIWriterPtr ptr) {
	ptr->writer->unregister();
	delete ptr;
}
#endif

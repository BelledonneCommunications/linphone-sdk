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

#pragma once

#include "mediastreamer2/msfilter.h"

#include "mswasapi.h"

class MSWASAPIWriter : public MSWasapi {
public:
	MSWASAPIWriter(MSFilter *filter);
	virtual ~MSWASAPIWriter();

	virtual int activate() override;
	virtual int deactivate() override;
	int feed(MSFilter *f);

#ifdef MS2_WINDOWS_UNIVERSAL
	static bool smInstantiated;
	virtual bool isInstantiated() override {
		return smInstantiated;
	}
	virtual void setInstantiated(bool instantiated) override {
		smInstantiated = instantiated;
	}
#endif

private:
	void drop(MSFilter *f);

	IAudioRenderClient *mAudioRenderClient;
	int mMinFrameCount = -1; /* The minimum samples queued into the wasapi during a 5 second period*/
};

#ifndef MS2_WINDOWS_PHONE
#define MSWASAPI_WRITER(w) ((MSWASAPIWriterType)((MSWASAPIWriterPtr)(w))->writer)
#ifdef MS2_WINDOWS_UNIVERSAL
typedef ComPtr<MSWASAPIWriter> MSWASAPIWriterType;
#else
typedef MSWASAPIWriter *MSWASAPIWriterType;
#endif
struct MSWASAPIWriterWrapper {
	MSWASAPIWriterType writer;
};
typedef struct MSWASAPIWriterWrapper *MSWASAPIWriterPtr;
#else
#define MSWASAPI_WRITER(w) ((MSWASAPIWriterType)(w))
typedef MSWASAPIWriter *MSWASAPIWriterPtr;
typedef MSWASAPIWriter *MSWASAPIWriterType;
#endif

MSWASAPIWriterPtr MSWASAPIWriterNew(MSFilter *filter);
void MSWASAPIWriterDelete(MSWASAPIWriterPtr ptr);

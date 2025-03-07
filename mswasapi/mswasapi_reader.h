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
#include "mediastreamer2/msticker.h"

#include "mswasapi.h"

class MSWASAPIReader : public MSWasapi {
public:
	MSWASAPIReader(MSFilter *filter);
	virtual ~MSWASAPIReader();

	virtual int activate() override;
	virtual int deactivate() override;
	virtual void start() override;
	virtual void stop() override;
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
	void silence(MSFilter *f);

	IAudioCaptureClient *mAudioCaptureClient;
	MSTickerSynchronizer *mTickerSynchronizer;
	// If timestamp cannot be retrieved from API, use it to count timestamp for ticker synchronizer: -1=not used
	UINT64 mSampleTime = (UINT64)-1;
};

#ifndef MS2_WINDOWS_PHONE
#define MSWASAPI_READER(w) ((MSWASAPIReaderType)((MSWASAPIReaderPtr)(w))->reader)
#ifdef MS2_WINDOWS_UNIVERSAL
typedef ComPtr<MSWASAPIReader> MSWASAPIReaderType;
#else
typedef MSWASAPIReader *MSWASAPIReaderType;
#endif
struct MSWASAPIReaderWrapper {
	MSWASAPIReaderType reader;
};
typedef struct MSWASAPIReaderWrapper *MSWASAPIReaderPtr;
#else
#define MSWASAPI_READER(w) ((MSWASAPIReaderType)(w))
typedef MSWASAPIReader *MSWASAPIReaderPtr;
typedef MSWASAPIReader *MSWASAPIReaderType;
#endif

MSWASAPIReaderPtr MSWASAPIReaderNew(MSFilter *f);
void MSWASAPIReaderDelete(MSWASAPIReaderPtr ptr);

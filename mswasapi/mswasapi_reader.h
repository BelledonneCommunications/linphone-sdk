/*
mswasapi_reader.h

mediastreamer2 library - modular sound and video processing and streaming
Windows Audio Session API sound card plugin for mediastreamer2
Copyright (C) 2010-2013 Belledonne Communications, Grenoble, France

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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#pragma once

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"

#include "mswasapi.h"

class MSWASAPIReader : public MSWasapi {
public:
	MSWASAPIReader(MSFilter *filter);
	virtual ~MSWASAPIReader();

	int activate() override;
	int deactivate() override;
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

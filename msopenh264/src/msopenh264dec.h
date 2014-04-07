/*
H.264 encoder/decoder plugin for mediastreamer2 based on the openh264 library.
Copyright (C) 2006-2012 Belledonne Communications, Grenoble

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


#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/rfc3984.h"
#include "wels/codec_api.h"


class MSOpenH264Decoder {
public:
	MSOpenH264Decoder(MSFilter *f);
	virtual ~MSOpenH264Decoder();
	bool isInitialized() const { return mInitialized; }
	void initialize();
	void feed();
	void uninitialize();
	void provideSpropParameterSets(char *value, int valueSize);
	void resetFirstImageDecoded();
	MSVideoSize getSize() const;

private:
	mblk_t * addNalMarker(mblk_t *nal);

	MSFilter *mFilter;
	SDecodingParam mDecoderParams;
	ISVCDecoder *mDecoder;
	Rfc3984Context mUnpacker;
	MSPicture mOutbuf;
	MSAverageFPS mFPS;
	mblk_t *mSPS;
	mblk_t *mPPS;
	mblk_t *mYUVMsg;
	uint64_t mLastErrorReportTime;
	uint32_t mPacketCount;
	int mWidth;
	int mHeight;
	bool mInitialized;
	bool mFirstImageDecoded;
};

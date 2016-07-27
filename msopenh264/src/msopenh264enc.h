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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/


#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/rfc3984.h"
#include "mediastreamer2/mscodecutils.h"
#include "wels/codec_api.h"
#include "msopenh264.h"


class MSOpenH264Encoder {
public:
	MSOpenH264Encoder(MSFilter *f);
	virtual ~MSOpenH264Encoder();
	void initialize();
	bool isInitialized() const { return mInitialized; }
	void feed();
	void uninitialize();
	void setFPS(float fps);
	float getFPS() const { return mVConf.fps; }
	void setBitrate(int bitrate);
	int getBitrate() const { return mVConf.required_bitrate; }
	void setSize(MSVideoSize size);
	MSVideoSize getSize() const { return mVConf.vsize; }
	void addFmtp(const char *fmtp);
	const MSVideoConfiguration *getConfigurationList() const { return mVConfList; }
	void setConfigurationList(const MSVideoConfiguration *confList);
	void setConfiguration(MSVideoConfiguration conf);
	void requestVFU();
	void enableAVPF(bool enable) { mAVPFEnabled = enable; }
	void notifyPLI();
	void notifyFIR(uint8_t seqnr);

private:
	void generateKeyframe();
	void fillNalusQueue(SFrameBSInfo& sFbi, MSQueue* nalus);
	void calcBitrates(int &targetBitrate, int &maxBitrate) const;
	void applyBitrate();

	MSFilter *mFilter;
	Rfc3984Context *mPacker;
	int mPacketisationMode;
	ISVCEncoder *mEncoder;
	const MSVideoConfiguration *mVConfList;
	MSVideoConfiguration mVConf;
	MSVideoStarter mVideoStarter;
	MSIFrameRequestsLimiterCtx mIFrameLimiter;
	uint64_t mFrameCount;
	bool mInitialized;
	bool mPacketisationModeSet;
	bool mAVPFEnabled;
};

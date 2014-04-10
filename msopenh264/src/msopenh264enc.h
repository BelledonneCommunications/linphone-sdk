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
#include "msopenh264.h"


/**
 * The goal of this small object is to tell when to send I frames at startup: at 2 and 4 seconds
 */
class VideoStarter {
public:
	VideoStarter();
	virtual ~VideoStarter();
	void firstFrame(uint64_t curtime);
	bool needIFrame(uint64_t curtime);
	void deactivate();

private:
	bool mActive;
	uint64_t mNextTime;
	int mFrameCount;
};


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
	void setConfiguration(MSVideoConfiguration conf);
	void requestVFU();

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
	VideoStarter mVideoStarter;
	uint64_t mFrameCount;
	bool mInitialized;
};

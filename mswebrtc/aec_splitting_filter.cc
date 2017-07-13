/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2017  Belledonne Communications, Grenoble, France

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

#include "audio_util.h"
#include "signal_processing_library.h"
#include "three_band_filter_bank.h"

#include "aec_splitting_filter.h"


struct MSWebRtcAecSplittingFilterStruct {
	MSWebRtcAecSplittingFilterStruct(int nbands, int bandsize);
	~MSWebRtcAecSplittingFilterStruct();
	void analysis(int16_t *ref, int16_t *echo);
	void synthesis(int16_t *oecho);

	float *mRef;
	float *mEcho;
	float *mOEcho;
	float * mRefBandsArray[3];
	float * mEchoBandsArray[3];
	float * mOEchoBandsArray[3];
	int16_t *mBandsRef;
	int16_t *mBandsEcho;
	int16_t *mBandsOEcho;
	float *mBandsRefFloat;
	float *mBandsEchoFloat;
	float *mBandsOEchoFloat;
	webrtc::ThreeBandFilterBank *mThreeBandFilterBankRef;
	webrtc::ThreeBandFilterBank *mThreeBandFilterBankEcho;
	webrtc::ThreeBandFilterBank *mThreeBandFilterBankOEcho;
	int mNbands;
	int mBandsize;
	int mFramesize;
	int mRefState1[6];
	int mRefState2[6];
	int mEchoState1[6];
	int mEchoState2[6];
	int mOEchoState1[6];
	int mOEchoState2[6];
};


void intbuf2floatbuf(int16_t *intbuf, float *floatbuf, int framesize) {
	int i;
	for (i = 0; i < framesize; i++) {
		floatbuf[i] = (float)intbuf[i];
	}
}

void floatbuf2intbuf(float *floatbuf, int16_t *intbuf, int framesize) {
	int i;
	for (i = 0; i < framesize; i++) {
		intbuf[i] = webrtc::FloatS16ToS16(floatbuf[i]);
	}
}


MSWebRtcAecSplittingFilterStruct::MSWebRtcAecSplittingFilterStruct(int nbands, int bandsize)
	: mRef(0), mEcho(0), mOEcho(0), mBandsRef(0), mBandsEcho(0), mBandsOEcho(0),
	mBandsRefFloat(0), mBandsEchoFloat(0), mBandsOEchoFloat(0),
	mThreeBandFilterBankRef(0), mThreeBandFilterBankEcho(0), mThreeBandFilterBankOEcho(0),
	mNbands(nbands), mBandsize(bandsize), mFramesize(nbands * bandsize) {

	mRef = new float[mFramesize];
	mEcho = new float[mFramesize];
	mOEcho = new float[mFramesize];
	memset(mRefBandsArray, 0, sizeof(mRefBandsArray));
	memset(mEchoBandsArray, 0, sizeof(mEchoBandsArray));
	memset(mOEchoBandsArray, 0, sizeof(mOEchoBandsArray));
	memset(mRefState1, 0, sizeof(mRefState1));
	memset(mRefState2, 0, sizeof(mRefState2));
	memset(mEchoState1, 0, sizeof(mEchoState1));
	memset(mEchoState2, 0, sizeof(mEchoState2));
	memset(mOEchoState1, 0, sizeof(mOEchoState1));
	memset(mOEchoState2, 0, sizeof(mOEchoState2));

	if (mNbands == 3) {
		mThreeBandFilterBankRef = new webrtc::ThreeBandFilterBank(mFramesize);
		mThreeBandFilterBankEcho = new webrtc::ThreeBandFilterBank(mFramesize);
		mThreeBandFilterBankOEcho = new webrtc::ThreeBandFilterBank(mFramesize);
		mBandsRefFloat = new float[mFramesize];
		mBandsEchoFloat = new float[mFramesize];
		mBandsOEchoFloat = new float[mFramesize];
		mRefBandsArray[0] = mBandsRefFloat;
		mRefBandsArray[1] = mBandsRefFloat + mBandsize;
		mRefBandsArray[2] = mBandsRefFloat + 2 * mBandsize;
		mEchoBandsArray[0] = mBandsEchoFloat;
		mEchoBandsArray[1] = mBandsEchoFloat + mBandsize;
		mEchoBandsArray[2] = mBandsEchoFloat + 2 * mBandsize;
		mOEchoBandsArray[0] = mBandsOEchoFloat;
		mOEchoBandsArray[1] = mBandsOEchoFloat + mBandsize;
		mOEchoBandsArray[2] = mBandsOEchoFloat + 2 * mBandsize;
	} else if (mNbands == 2) {
		mBandsRef = new int16_t[mFramesize];
		mBandsEcho = new int16_t[mFramesize];
		mBandsOEcho = new int16_t[mFramesize];
		mRefBandsArray[0] = mRef;
		mEchoBandsArray[0] = mEcho;
		mEchoBandsArray[1] = mEcho + mBandsize;
		mOEchoBandsArray[0] = mOEcho;
		mOEchoBandsArray[1] = mOEcho + mBandsize;
	} else {
		mRefBandsArray[0] = mRef;
		mEchoBandsArray[0] = mEcho;
		mOEchoBandsArray[0] = mOEcho;
	}
}

MSWebRtcAecSplittingFilterStruct::~MSWebRtcAecSplittingFilterStruct() {
	delete[] mRef;
	delete[] mEcho;
	delete[] mOEcho;
	if (mBandsRef) delete[] mBandsRef;
	if (mBandsEcho) delete[] mBandsEcho;
	if (mBandsOEcho) delete[] mBandsOEcho;
	if (mBandsRefFloat) delete[] mBandsRefFloat;
	if (mBandsEchoFloat) delete[] mBandsEchoFloat;
	if (mBandsOEchoFloat) delete[] mBandsOEchoFloat;
	if (mThreeBandFilterBankRef) delete mThreeBandFilterBankRef;
	if (mThreeBandFilterBankEcho) delete mThreeBandFilterBankEcho;
	if (mThreeBandFilterBankOEcho) delete mThreeBandFilterBankOEcho;
}

void MSWebRtcAecSplittingFilterStruct::analysis(int16_t *ref, int16_t *echo) {
	if (mNbands == 3) {
		intbuf2floatbuf(ref, mRef, mFramesize);
		mThreeBandFilterBankRef->Analysis(mRef, mFramesize, mRefBandsArray);
		intbuf2floatbuf(echo, mEcho, mFramesize);
		mThreeBandFilterBankEcho->Analysis(mEcho, mFramesize, mEchoBandsArray);
	} else if (mNbands == 2) {
		WebRtcSpl_AnalysisQMF(ref, mFramesize, mBandsRef, mBandsRef + mBandsize, mRefState1, mRefState2);
		intbuf2floatbuf(mBandsRef, mRef, mFramesize);
		WebRtcSpl_AnalysisQMF(echo, mFramesize, mBandsEcho, mBandsEcho + mBandsize, mEchoState1, mEchoState2);
		intbuf2floatbuf(mBandsEcho, mEcho, mFramesize);
	} else {
		intbuf2floatbuf(ref, mRef, mFramesize);
		intbuf2floatbuf(echo, mEcho, mFramesize);
	}
}

void MSWebRtcAecSplittingFilterStruct::synthesis(int16_t *oecho) {
	if (mNbands == 3) {
		mThreeBandFilterBankOEcho->Synthesis(mOEchoBandsArray, mBandsize, mOEcho);
		floatbuf2intbuf(mOEcho, oecho, mFramesize);
	} else if (mNbands == 2) {
		floatbuf2intbuf(mOEcho, mBandsOEcho, mFramesize);
		WebRtcSpl_SynthesisQMF(mBandsOEcho, mBandsOEcho + mBandsize, mBandsize, oecho, mOEchoState1, mOEchoState2);
	} else {
		floatbuf2intbuf(mOEcho, oecho, mFramesize);
	}
}


MSWebRtcAecSplittingFilter * mswebrtc_aec_splitting_filter_create(int nbands, int bandsize) {
	return new MSWebRtcAecSplittingFilterStruct(nbands, bandsize);
}

void mswebrtc_aec_splitting_filter_destroy(MSWebRtcAecSplittingFilter *filter) {
	delete filter;
}

void mswebrtc_aec_splitting_filter_analysis(MSWebRtcAecSplittingFilter *filter, int16_t *ref, int16_t *echo) {
	filter->analysis(ref, echo);
}

void mswebrtc_aec_splitting_filter_synthesis(MSWebRtcAecSplittingFilter *filter, int16_t *oecho) {
	filter->synthesis(oecho);
}

float * mswebrtc_aec_splitting_filter_get_ref(MSWebRtcAecSplittingFilter *filter) {
	return filter->mRefBandsArray[0];
}

const float * const * mswebrtc_aec_splitting_filter_get_echo_bands(MSWebRtcAecSplittingFilter *filter) {
	return filter->mEchoBandsArray;
}

float * const * mswebrtc_aec_splitting_filter_get_output_bands(MSWebRtcAecSplittingFilter *filter) {
	return filter->mOEchoBandsArray;
}

int mswebrtc_aec_splitting_filter_get_number_of_bands(MSWebRtcAecSplittingFilter *filter) {
	return filter->mNbands;
}

int mswebrtc_aec_splitting_filter_get_bandsize(MSWebRtcAecSplittingFilter *filter) {
	return filter->mBandsize;
}

/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/msjpegwriter.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msticker.h"
#include "../voip/nowebcam.h"
#include "turbojpeg.h"

extern "C" {
	MS2_PUBLIC mblk_t *jpeg2yuv_details(uint8_t *jpgbuf, int bufsize, MSVideoSize *reqsize, tjhandle turbojpegDec, tjhandle yuvEncoder, MSYuvBufAllocator * allocator, uint8_t **gRgbBuf, size_t *gRgbBufLen);
}
class MSTurboJpegDec{
public:
	MSTurboJpegDec(){
		mFps=60.0;
		mDciSize = 0;
		mRgbBufferSize = 0;
		mRgbBuffer = nullptr;
		mSize.width = MS_VIDEO_SIZE_UNKNOWN_W;
		mSize.height = MS_VIDEO_SIZE_UNKNOWN_H;
		mAllocator = ms_yuv_buf_allocator_new();
	}
	~MSTurboJpegDec(){
		if( mRgbBufferSize > 0){
			bctbx_free(mRgbBuffer);// jpeg2yuv_details use bctbx_new
		}
		ms_free(mAllocator);
	}

	MSVideoSize mSize;
	MSAverageFPS mAvgFps;
	uint8_t mDci[512];
	int mDciSize;
	float mFps;

	tjhandle mTurboJpegDecompressor;
	tjhandle mTurboJpegCompressor;
	MSYuvBufAllocator * mAllocator;
	uint8_t * mRgbBuffer;
	size_t mRgbBufferSize;

	float getFps(){return mFps;}

	void decodeFrame(MSFilter *filter, mblk_t *inm){
		if (inm->b_cont!=NULL) inm=inm->b_cont; /*skip potential video header */
		if( mTurboJpegDecompressor){
			mblk_t *m = jpeg2yuv_details(inm->b_rptr, inm->b_wptr-inm->b_rptr, &mSize,mTurboJpegDecompressor,mTurboJpegCompressor,mAllocator,&mRgbBuffer,&mRgbBufferSize );
			if(m){
				uint32_t timestamp;
				timestamp = (uint32_t)(filter->ticker->time * 90);// rtp uses a 90000 Hz clockrate for video
				mblk_set_timestamp_info(m, timestamp);
				ms_queue_put(filter->outputs[0], m);
				ms_average_fps_update(&mAvgFps,filter->ticker->time);
			}
		}
	}
};


//-------------------------------------------------------------------------

static void ms_turbojpeg_dec_mjpeg_init(MSFilter *filter){
	MSTurboJpegDec *dec = new MSTurboJpegDec();
	dec->mTurboJpegDecompressor = tjInitDecompress();
	dec->mTurboJpegCompressor = tjInitCompress();
	filter->data = dec;
}

static void ms_turbojpeg_dec_preprocess(MSFilter *filter) {
	MSTurboJpegDec *dec = static_cast<MSTurboJpegDec *>(filter->data);
	ms_average_fps_init(&dec->mAvgFps,"[MSTMJpegDec] fps=%f");
}

static void ms_turbojpeg_dec_process(MSFilter *filter) {
	MSTurboJpegDec *dec = static_cast<MSTurboJpegDec *>(filter->data);
	mblk_t *inm;
	while((inm=ms_queue_get(filter->inputs[0]))!=0){
		dec->decodeFrame(filter,inm);
		freemsg(inm);
	}
}

static void ms_turbojpeg_dec_postprocess(MSFilter *filter) {
}

static void ms_turbojpeg_dec_uninit(MSFilter *filter) {
	MSTurboJpegDec *dec = static_cast<MSTurboJpegDec *>(filter->data);
	if( dec->mTurboJpegDecompressor)
		tjDestroy(dec->mTurboJpegDecompressor);
	if( dec->mTurboJpegCompressor)
		tjDestroy(dec->mTurboJpegCompressor);
}
//---------------------------------------------------------------------------------

static int ms_turbojpeg_dec_add_fmtp(MSFilter *f, void *data){
	const char *fmtp=(const char*)data;
	MSTurboJpegDec *s=(MSTurboJpegDec*)f->data;
	char config[512];
	if (fmtp_get_value(fmtp,"config",config,sizeof(config))){
		/*convert hexa decimal config string into a bitstream */
		size_t i;
		int j;
		size_t max = strlen(config);
		char octet[3];
		octet[2]=0;
		for(i=0,j=0;i<max;i+=2,++j){
			octet[0]=config[i];
			octet[1]=config[i+1];
			s->mDci[j]=(uint8_t)strtol(octet,NULL,16);
		}
		s->mDciSize=j;
		ms_message("Got mpeg4 config string: %s",config);
	}
	return 0;
}

static int ms_turbojpeg_dec_get_vsize(MSFilter *f, void *data) {
	MSTurboJpegDec *s = (MSTurboJpegDec *)f->data;
	MSVideoSize *vsize = (MSVideoSize *)data;
	vsize->width = s->mSize.width;
	vsize->height = s->mSize.height;
	return 0;
}
static int ms_turbojpeg_dec_set_vsize(MSFilter *f, void *data) {
	MSTurboJpegDec *s = (MSTurboJpegDec *)f->data;
	MSVideoSize *vsize = (MSVideoSize *)data;
	s->mSize.width = vsize->width;
	s->mSize.height = vsize->height;
	return 0;
}
static int ms_turbojpeg_dec_get_fps(MSFilter *filter, void *arg){
	MSTurboJpegDec *dec=(MSTurboJpegDec*)filter->data;
	if (filter->ticker){
		*((float*)arg) = ms_average_fps_get(&dec->mAvgFps);
	} else {
		*((float*)arg) = dec->getFps();
	}
	return 0;
}

extern "C" {

static MSFilterMethod methods[]={
	{	MS_FILTER_ADD_FMTP		,	ms_turbojpeg_dec_add_fmtp	},
	{	MS_FILTER_GET_VIDEO_SIZE,	ms_turbojpeg_dec_get_vsize	},
	{	MS_FILTER_SET_VIDEO_SIZE,	ms_turbojpeg_dec_set_vsize	},
	{	MS_FILTER_GET_FPS,		ms_turbojpeg_dec_get_fps	},
	{	0		,		NULL			}
};

#ifndef _MSC_VER

MSFilterDesc ms_turbojpeg_mjpeg_dec_desc={
	.id=MS_MJPEG_DEC_ID,
	.name="MSTMJpegDec",
	.text="A MJPEG decoder using turbojpeg library",
	.category=MS_FILTER_DECODER,
	.enc_fmt="MJPEG",
	.ninputs=1,
	.noutputs=1,
	.init=ms_turbojpeg_dec_mjpeg_init,
	.preprocess=ms_turbojpeg_dec_preprocess,
	.process=ms_turbojpeg_dec_process,
	.postprocess=ms_turbojpeg_dec_postprocess,
	.uninit=ms_turbojpeg_dec_uninit,
	.methods= methods
};

#else

MSFilterDesc ms_turbojpeg_mjpeg_dec_desc={
	MS_MJPEG_DEC_ID,
	"MSTMJpegDec",
	N_("A MJPEG decoder using turbojpeg library"),
	MS_FILTER_DECODER,
	"MJPEG",
	1,
	1,
	ms_turbojpeg_dec_mjpeg_init,
	ms_turbojpeg_dec_preprocess,
	ms_turbojpeg_dec_process,
	ms_turbojpeg_dec_postprocess,
	ms_turbojpeg_dec_uninit,
	methods
};

#endif
}// extern "C"
MS_FILTER_DESC_EXPORT(ms_turbojpeg_mjpeg_dec_desc)

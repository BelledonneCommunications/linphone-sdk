/*
 * mediastreamer2 library - modular sound and video processing and streaming
 * Copyright (C) 2018  Simon MORLAT (simon.morlat@linphone.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "../utils/qrcode_image.hpp"
#include "mediastreamer2/msqrcodereader.h"
#include "mediastreamer2/msvideo.h"

#include <vector>

#include <zxing/Binarizer.h>
#include <zxing/MultiFormatReader.h>
#include <zxing/Result.h>
#include <zxing/ReaderException.h>
#include <zxing/common/GlobalHistogramBinarizer.h>
#include <zxing/common/HybridBinarizer.h>
#include <zxing/Exception.h>
#include <zxing/common/IllegalArgumentException.h>
#include <zxing/BinaryBitmap.h>
#include <zxing/DecodeHints.h>
#include <zxing/qrcode/QRCodeReader.h>


using namespace std;
using namespace zxing;
using namespace zxing::qrcode;

typedef struct {
	QRCodeImage *image;
	char* resultText;
	bool_t searchQRCode;
	MSRect decoderRect;
	MSYuvBufAllocator *msAllocator;
	MSFilter *f;
}QRCodeReaderStruct;

typedef struct {
	char resultText[255];//Can be changed depending on the size of the URL
}QRCodeReaderNotifyStruct;

static void qrcode_init(MSFilter *f) {
	QRCodeReaderStruct *qrc = ms_new0(QRCodeReaderStruct, 1);
	qrc->searchQRCode = TRUE;
	qrc->decoderRect.h = 0;
	qrc->decoderRect.w = 0;
	qrc->image = NULL;
	qrc->msAllocator = ms_yuv_buf_allocator_new();
	f->data = qrc;
}

static void qrcode_uninit(MSFilter *f) {
	QRCodeReaderStruct *qrc = (QRCodeReaderStruct *)f->data;
	qrc->f = NULL;
	f->data = NULL;
	if (qrc->msAllocator) ms_yuv_buf_allocator_free(qrc->msAllocator);
	ms_free(qrc);
}

static int reset_search(MSFilter *f, void *arg) {
	QRCodeReaderStruct *qrc = (QRCodeReaderStruct *)f->data;
	qrc->searchQRCode = TRUE;
	return 0;
}

static int set_decoder_rect(MSFilter *f, void *arg) {
	MSRect *rect = (MSRect*)arg;
	QRCodeReaderStruct *qrc = (QRCodeReaderStruct *)f->data;
	qrc->decoderRect = *rect;
	return 0;
}

static void read_qrcode(MSFilter *f) {
	QRCodeReaderStruct *qrc = (QRCodeReaderStruct *)f->data;
	if (qrc->image) {
		QRCodeReaderNotifyStruct qrcNotify;
		vector<Ref<Result>> results;
		Ref<Binarizer> binarizer;
		binarizer = new HybridBinarizer(qrc->image->getLuminanceSource());
		DecodeHints hints(DecodeHints::DEFAULT_HINT);
		Ref<BinaryBitmap> binary(new BinaryBitmap(binarizer));
		Ref<Reader> reader(new MultiFormatReader);
		try {
			results = vector<Ref<Result>>(1, reader->decode(binary, hints));
		} catch (ReaderException const& re) {
			(void)re;
			return;
		}
		Ref<String> text = results[0]->getText();
		snprintf(qrcNotify.resultText, sizeof(qrcNotify.resultText), "%s", text->getText().c_str());
		qrc->searchQRCode = FALSE;
		ms_filter_notify(f, MS_QRCODE_READER_QRCODE_FOUND, qrcNotify.resultText);
	}
}

static mblk_t * cropBeforeDecode(QRCodeReaderStruct *qrc, MSPicture *p, MSPicture *newP) {
	if (qrc->decoderRect.h == 0 && qrc->decoderRect.w == 0) return NULL;

	MSRect newRect;
	mblk_t *mblk;
	int pixStride[] = {1, 1, 1};

	newRect = qrc->decoderRect;
	newRect.x = 0;
	newRect.y = 0;
	mblk = ms_yuv_buf_allocator_get(qrc->msAllocator, newP, newRect.w, newRect.h);

	ms_yuv_buf_copy_with_pix_strides(p->planes, p->strides, pixStride, qrc->decoderRect,
		newP->planes, newP->strides, pixStride, newRect);

	return mblk;
}

static void qrcode_process(MSFilter *f) {
	mblk_t *m;
	MSPicture yuvBuf;
	QRCodeReaderStruct *qrc = (QRCodeReaderStruct*)f->data;

	ms_filter_lock(f);
	while((m = ms_queue_get(f->inputs[0])) != NULL) {
		if (qrc->searchQRCode) {
			mblk_t *mblk;
			MSPicture newYuvbuf;

			ms_yuv_buf_init_from_mblk(&yuvBuf,m);
			mblk = cropBeforeDecode(qrc, &yuvBuf, &newYuvbuf);
			if (!mblk) newYuvbuf = yuvBuf;
			qrc->image = new QRCodeImage(newYuvbuf.w, newYuvbuf.h, newYuvbuf.planes[0], newYuvbuf.strides[0]);
			read_qrcode(f);

			if (mblk) freeb(mblk);
		}
		ms_queue_put(f->outputs[0], m);
	}
	ms_filter_unlock(f);
}

#ifdef __cplusplus
extern "C" {
#endif

static MSFilterMethod qrcode_methods[] = {
	{MS_QRCODE_READER_RESET_SEARCH, reset_search},
	{MS_QRCODE_READET_SET_DECODER_RECT, set_decoder_rect},
	{0, NULL}
};

MSFilterDesc ms_qrcode_reader_desc = {
	MS_QRCODE_READER_ID,
	"MSQRCodeReader",
	"QRCode reader",
	MS_FILTER_OTHER,
	NULL,
	1,
	1,
	qrcode_init,
	NULL,
	qrcode_process,
	NULL,
	qrcode_uninit,
	qrcode_methods
};

MS_FILTER_DESC_EXPORT(ms_qrcode_reader_desc)

#ifdef __cplusplus
}
#endif

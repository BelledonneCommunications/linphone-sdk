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

#include "../utils/qrcode_image.hpp"
#include "mediastreamer2/msqrcodereader.h"
#include "mediastreamer2/msvideo.h"

#include <vector>

#if defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#endif
#if ((__GNUC__ == 4 && __GNUC_MINOR__ >= 6) || __GNUC__ > 4)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsuggest-override"
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#endif

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

#if defined(__clang__) || ((__GNUC__ == 4 && __GNUC_MINOR__ >= 6) || __GNUC__ > 4)
#pragma GCC diagnostic pop
#endif


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
		Ref<Result> result;
		Ref<Binarizer> binarizer;
		binarizer = new HybridBinarizer(qrc->image->getLuminanceSource());
		DecodeHints hints(DecodeHints::DEFAULT_HINT);
		Ref<BinaryBitmap> binary(new BinaryBitmap(binarizer));
		Ref<Reader> reader(new QRCodeReader);
		try {
			result = reader->decode(binary, hints);
		} catch (ReaderException const& re) {
			(void)re;
			return;
		}
		Ref<String> text = result->getText();
		MSQrCodeReaderEventData data = {{0}};
		snprintf(data.data, sizeof(data.data), "%s", text->getText().c_str());
		qrc->searchQRCode = FALSE;
		ms_filter_notify(f, MS_QRCODE_READER_QRCODE_FOUND, &data);
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

void qrcode_process(MSFilter *f) {
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
			// Do not call delete for qrc-image, zxing will automatically delete this after decode.
			qrc->image = new QRCodeImage(newYuvbuf.w, newYuvbuf.h, newYuvbuf.planes[0], newYuvbuf.strides[0]);
			read_qrcode(f);
			qrc->image = NULL;

			if (mblk) freemsg(mblk);
		}
		ms_queue_put(f->outputs[0], m);
	}
	ms_filter_unlock(f);
}

extern "C" {

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

}

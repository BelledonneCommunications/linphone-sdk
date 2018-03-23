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
	MSFilter *f;
}QRCodeReaderStruct;

static void qrcode_init(MSFilter *f) {
	QRCodeReaderStruct *qrc = ms_new0(QRCodeReaderStruct, 1);
	qrc->searchQRCode = TRUE;
	f->data = qrc;
}

static void qrcode_uninit(MSFilter *f) {
	QRCodeReaderStruct *qrc = (QRCodeReaderStruct *)f->data;
	qrc->f = NULL;
	f->data = NULL;
	if (qrc->resultText) ms_free(qrc->resultText);
	ms_free(qrc);
}

static int reset_search(MSFilter *f, void *arg) {
	QRCodeReaderStruct *qrc = (QRCodeReaderStruct *)f->data;
	qrc->searchQRCode = TRUE;
	return 0;
}

static void read_qrcode(MSFilter *f) {
	QRCodeReaderStruct *qrc = (QRCodeReaderStruct *)f->data;
	if (qrc->image) {
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
		if (qrc->resultText) ms_free(qrc->resultText);
		qrc->resultText = ms_strdup(text->getText().c_str());
		qrc->searchQRCode = FALSE;
		ms_filter_notify(f, MS_QRCODE_READER_QRCODE_FOUND, qrc->resultText);
	}
}

static void qrcode_process(MSFilter *f) {
	mblk_t *m;
	MSPicture yuvbuf;
	QRCodeReaderStruct *qrc = (QRCodeReaderStruct*)f->data;

	ms_filter_lock(f);
	while((m = ms_queue_get(f->inputs[0])) != NULL) {
		if (qrc->searchQRCode) {
			ms_yuv_buf_init_from_mblk(&yuvbuf,m);
			qrc->image = new QRCodeImage(yuvbuf.w, yuvbuf.h, yuvbuf.planes[0], yuvbuf.strides[0]);
			read_qrcode(f);
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

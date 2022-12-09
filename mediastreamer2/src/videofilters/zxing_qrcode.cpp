/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2 
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
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

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

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

#define ZX_USE_UTF8
#include <ZXing/ReadBarcode.h>

#if defined(__clang__) || ((__GNUC__ == 4 && __GNUC_MINOR__ >= 6) || __GNUC__ > 4)
#pragma GCC diagnostic pop
#endif

using namespace std;
using namespace ZXing;

typedef struct {
	ImageView image;
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
	qrc->image = ImageView(NULL, 0, 0, ImageFormat::RGB);
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
	if (qrc->image.data(0,0)) {
		DecodeHints hints;
		Result result = ReadBarcode(qrc->image, hints);
		if (result.error())
			ms_warning("[MSQRCodeReader] Cannot decode QRCode : %s", ToString(result.error()).c_str());
		else if(result.format() == BarcodeFormat::None){
		}else if(!result.isValid())
			ms_warning("[MSQRCodeReader] QRCode is not valid");
		else{
			MSQrCodeReaderEventData data = {{0}};
			snprintf(data.data, sizeof(data.data), "%s", result.text().c_str());
			qrc->searchQRCode = FALSE;
			ms_filter_notify(f, MS_QRCODE_READER_QRCODE_FOUND, &data);
		
		}
	}
}

void qrcode_process(MSFilter *f) {
	mblk_t *m;
	MSPicture yuvBuf;
	QRCodeReaderStruct *qrc = (QRCodeReaderStruct*)f->data;

	ms_filter_lock(f);
	while((m = ms_queue_get(f->inputs[0])) != NULL) {
		if (qrc->searchQRCode) {
			ms_yuv_buf_init_from_mblk(&yuvBuf,m);
			qrc->image = ImageView(yuvBuf.planes[0], yuvBuf.w, yuvBuf.h, ImageFormat::RGB, yuvBuf.strides[0], 1);
			if (qrc->decoderRect.h != 0 && qrc->decoderRect.w != 0)	// Crop before decode
				qrc->image = qrc->image.cropped(qrc->decoderRect.x, qrc->decoderRect.y, qrc->decoderRect.w, qrc->decoderRect.h);
			read_qrcode(f);
			qrc->image = ImageView(NULL, 0,0,ImageFormat::RGB);// Reset
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

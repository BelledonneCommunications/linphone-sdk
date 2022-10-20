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

#include "mediastreamer2_tester.h"
#include "mediastreamer2_tester_private.h"
#include "mediastreamer2/msqrcodereader.h"

#ifndef QRCODELINPHONE_JPG
#define QRCODELINPHONE_JPG "qrcodesite"
#endif

#ifndef QRCODELINPHONE_SCREEN_JPG
#define QRCODELINPHONE_SCREEN_JPG "qrcodesite_screen"
#endif

#ifndef QRCODELINPHONE_INCLINED_JPG
#define QRCODELINPHONE_INCLINED_JPG "qrcodesite_inclined"
#endif

#ifndef QRCODELINPHONE_CAPTURED_JPG
#define QRCODELINPHONE_CAPTURED_JPG "qrcodesite_captured"
#endif

typedef struct struct_qrcode_callback_data {
	int qrcode_found;
	char *text;
}qrcode_callback_data;

static void qrcode_found_cb(void *data, MSFilter *f, unsigned int event_id, void *arg) {
	if (event_id == MS_QRCODE_READER_QRCODE_FOUND) {
		qrcode_callback_data *found = (qrcode_callback_data *)data;
		found->qrcode_found = TRUE;
		MSQrCodeReaderEventData *data = (MSQrCodeReaderEventData *)arg;
		if (arg) found->text = ms_strdup((char *)data->data);
	}
}

static void _decode_qr_code(const char *_image_path, bool_t want_decode, MSRect *capture_rect, bool_t with_reset, unsigned int number_of_run) {
	MSConnectionHelper h;
	MSWebCam *camera;
	char* image_path, *image_res_path;
	MSFilter *nowebcam_qrcode = NULL;
	MSFilter *zxing_qrcode = NULL;
	MSFilter *void_sink = NULL;
	qrcode_callback_data qrcode_cb_data;
	MSFactory* _factory = NULL;

	qrcode_cb_data.qrcode_found = FALSE;
	qrcode_cb_data.text = NULL;

	image_path = ms_strdup_printf("images/%s.jpg", _image_path);
	image_res_path = bc_tester_res(image_path);
	ms_static_image_set_default_image(image_res_path);
	_factory = ms_factory_new_with_voip();

	ms_factory_create_filter(_factory, TRUE);

	ms_tester_create_ticker();

	// Get create nowebcam filter with qrcode image
	camera = ms_web_cam_manager_get_cam(ms_factory_get_web_cam_manager(_factory),"StaticImage: Static picture");
	nowebcam_qrcode = ms_web_cam_create_reader(camera);
	ms_filter_notify(nowebcam_qrcode, MS_STATIC_IMAGE_SET_IMAGE, image_res_path);
	zxing_qrcode = ms_factory_create_filter(_factory, MS_QRCODE_READER_ID);
	void_sink = ms_factory_create_filter(_factory, MS_VOID_SINK_ID);
	ms_filter_add_notify_callback(zxing_qrcode, (MSFilterNotifyFunc)qrcode_found_cb, &qrcode_cb_data, TRUE);
	if (capture_rect) {
		MSVideoSize size;
		ms_filter_call_method(zxing_qrcode, MS_QRCODE_READET_SET_DECODER_RECT, capture_rect);
		size.width = 960;
		size.height = 1280;
		ms_filter_call_method(nowebcam_qrcode, MS_FILTER_SET_VIDEO_SIZE, &size);
	}

	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, nowebcam_qrcode, -1, 0);
	ms_connection_helper_link(&h, zxing_qrcode, 0, 0);
	ms_connection_helper_link(&h, void_sink, 0, -1);
	ms_ticker_attach(ms_tester_ticker, nowebcam_qrcode);

	while(number_of_run-- > 0) {
		if (want_decode) {
			BC_ASSERT_TRUE(wait_for_until(NULL, NULL, &qrcode_cb_data.qrcode_found, TRUE, 2000));
		} else {
			BC_ASSERT_FALSE(wait_for_until(NULL, NULL, &qrcode_cb_data.qrcode_found, TRUE, 2000));
		}

		if (qrcode_cb_data.qrcode_found) {
			if (BC_ASSERT_PTR_NOT_NULL(qrcode_cb_data.text)) {
				ms_message("QRCode decode: %s", qrcode_cb_data.text);
				BC_ASSERT_STRING_EQUAL(qrcode_cb_data.text, "https://www.linphone.org/");
				ms_free(qrcode_cb_data.text);
				qrcode_cb_data.qrcode_found = FALSE;
			}
		}

		if (with_reset) {
			ms_filter_call_method_noarg(zxing_qrcode, MS_QRCODE_READER_RESET_SEARCH);
		} else {
			want_decode = FALSE;
		}
	}

	ms_ticker_detach(ms_tester_ticker, nowebcam_qrcode);
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, nowebcam_qrcode, -1, 0);
	ms_connection_helper_unlink(&h, zxing_qrcode, 0, 0);
	ms_connection_helper_unlink(&h, void_sink, 0, -1);
	ms_factory_log_statistics(_factory);

	if (image_path) ms_free(image_path);
	if (image_res_path) ms_free(image_res_path);
	if (nowebcam_qrcode) ms_filter_destroy(nowebcam_qrcode);
	if (zxing_qrcode) ms_filter_destroy(zxing_qrcode);
	if (void_sink) ms_filter_destroy(void_sink);
	if (_factory) ms_factory_destroy(_factory);
	ms_tester_destroy_ticker();
}

static void decode_qr_code(void) {
	_decode_qr_code(QRCODELINPHONE_JPG, TRUE, NULL, FALSE, 1);
}

static void decode_qr_code_screen(void) {
	_decode_qr_code(QRCODELINPHONE_SCREEN_JPG, TRUE, NULL, FALSE, 1);
}

static void decode_qr_code_inclined(void) {
	_decode_qr_code(QRCODELINPHONE_INCLINED_JPG, TRUE, NULL, FALSE, 1);
}

static void decode_qr_code_rect(void) {
	MSRect rect;
	rect.x = 332;
	rect.y = 470;
	rect.w = 268;
	rect.h = 262;
	_decode_qr_code(QRCODELINPHONE_CAPTURED_JPG, TRUE, &rect, FALSE, 1);
}

static void cannot_decode(void) {
	_decode_qr_code(QRCODELINPHONE_CAPTURED_JPG, FALSE, NULL, FALSE, 1);
}

static void decode_qr_code_twice(void) {
	_decode_qr_code(QRCODELINPHONE_SCREEN_JPG, TRUE, NULL, TRUE, 2);
}

static void decode_qr_code_once(void) {
	_decode_qr_code(QRCODELINPHONE_SCREEN_JPG, TRUE, NULL, FALSE, 2);
}

static test_t tests[] = {
	TEST_NO_TAG("Decode qr code", decode_qr_code),
	TEST_NO_TAG("Decode qr code from screen", decode_qr_code_screen),
	TEST_NO_TAG("Decode inclined qr code from screen", decode_qr_code_inclined),
	TEST_NO_TAG("Decode qr code with rect", decode_qr_code_rect),
	TEST_NO_TAG("Cannot decode qr code", cannot_decode),
	TEST_NO_TAG("Decode qr code twice", decode_qr_code_twice),
	TEST_NO_TAG("Decode qr code once", decode_qr_code_once)
};

test_suite_t qrcode_test_suite = {
	"QRCode",
	NULL,
	NULL,
	NULL,
	NULL,
	sizeof(tests) / sizeof(tests[0]),
	tests
};

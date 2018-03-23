/*
 m ediastreamer2 li*brary - modular sound and video processing and streaming
 Copyright (C) 2006-2013 Belledonne Communications, Grenoble

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

typedef struct struct_qrcode_callback_data {
	int qrcode_found;
	char *text;
}qrcode_callback_data;

static void qrcode_found_cb(void *data, MSFilter *f, unsigned int event_id, void *arg) {
	if (event_id == MS_QRCODE_READER_QRCODE_FOUND) {
		qrcode_callback_data *found = (qrcode_callback_data *)data;
		found->qrcode_found = TRUE;
		if (arg) found->text = (char *)arg;
	}
}

static void _decode_qr_code(const char* _image_path) {
	MSConnectionHelper h;
	MSWebCam *camera;
	char* image_path, *image_res_path;
	MSFilter *nowebcam_qrcode = NULL;
	MSFilter *zxing_qrcode = NULL;
	MSFilter *void_sing = NULL;
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
	void_sing = ms_factory_create_filter(_factory, MS_VOID_SINK_ID);
	ms_filter_add_notify_callback(zxing_qrcode, (MSFilterNotifyFunc)qrcode_found_cb, &qrcode_cb_data, TRUE);

	ms_connection_helper_start(&h);
	ms_connection_helper_link(&h, nowebcam_qrcode, -1, 0);
	ms_connection_helper_link(&h, zxing_qrcode, 0, 0);
	ms_connection_helper_link(&h, void_sing, 0, -1);
	ms_ticker_attach(ms_tester_ticker, nowebcam_qrcode);

	BC_ASSERT_TRUE(wait_for_until(NULL, NULL, &qrcode_cb_data.qrcode_found, TRUE, 1000));
	if (qrcode_cb_data.qrcode_found) {
		if (BC_ASSERT_PTR_NOT_NULL(qrcode_cb_data.text)) {
			ms_message("QRCode decode: %s", qrcode_cb_data.text);
			BC_ASSERT_STRING_EQUAL(qrcode_cb_data.text, "https://www.linphone.org/");
		}
	}

	ms_ticker_detach(ms_tester_ticker, nowebcam_qrcode);
	ms_connection_helper_start(&h);
	ms_connection_helper_unlink(&h, nowebcam_qrcode, -1, 0);
	ms_connection_helper_unlink(&h, zxing_qrcode, 0, 0);
	ms_connection_helper_unlink(&h, void_sing, 0, -1);
	ms_factory_log_statistics(_factory);

	if (image_path) ms_free(image_path);
	if (image_res_path) ms_free(image_res_path);
	if (nowebcam_qrcode) ms_filter_destroy(nowebcam_qrcode);
	if (zxing_qrcode) ms_filter_destroy(zxing_qrcode);
	if (void_sing) ms_filter_destroy(void_sing);
	if (_factory) ms_factory_destroy(_factory);
	ms_tester_destroy_ticker();
}

static void decode_qr_code(void) {
	_decode_qr_code(QRCODELINPHONE_JPG);
}

static void decode_qr_code_screen(void) {
	_decode_qr_code(QRCODELINPHONE_SCREEN_JPG);
}

static void decode_qr_code_inclined(void) {
	_decode_qr_code(QRCODELINPHONE_INCLINED_JPG);
}

static test_t tests[] = {
	TEST_NO_TAG("Decode qr code" , decode_qr_code),
	TEST_NO_TAG("Decode qr code from screen" , decode_qr_code_screen),
	TEST_NO_TAG("Decode inclined qr code from screen" , decode_qr_code_inclined)
	//TODO Adding somes tests with bad pictures
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

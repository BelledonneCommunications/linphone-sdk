/*
 mediastreamer2 library - modular sound and video processing and streaming
 Copyright (C) 2018 Belledonne Communications, Grenoble

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

#include <fstream>
#include <list>
#include <sstream>
#include <string>
#include <vector>

#include <bctoolbox/tester.h>

#include "h26x/h26x-utils.h"

using namespace mediastreamer;
using namespace std;


static std::vector<uint8_t> loadBinaryFile(const std::string &filename) {
	char c;
	vector<uint8_t> data;
	fstream f;
	f.exceptions(fstream::badbit | fstream::failbit);
	f.open(filename, fstream::in | fstream::binary);
	while ((c = f.get()) != char_traits<char>::eof()) {
		data.push_back(c);
	}
	f.close();
	return data;
}

static void bytestream_transcoding_test(const std::string &frame) {
	ostringstream filename;
	filename << bc_tester_get_resource_dir_prefix() << "/raw/" << frame;

	vector<uint8_t> byteStream = loadBinaryFile(filename.str());

	MSQueue nalus;
	ms_queue_init(&nalus);
	H26xUtils::byteStreamToNalus(byteStream, &nalus);
	BC_ASSERT(!ms_queue_empty(&nalus));

	vector<uint8_t> byteStream2;
	H26xUtils::nalusToByteStream(&nalus, byteStream2);
	BC_ASSERT(ms_queue_empty(&nalus));
	BC_ASSERT(byteStream == byteStream2);
}

static void paramter_sets_bytestream_transcoding_test() {
	bytestream_transcoding_test("h265-parameter-sets-frame");
}

static void iframe_bytestream_transcoding_test() {
	bytestream_transcoding_test("h265-iframe");
}

static test_t tests[] = {
	TEST_NO_TAG("Bytestream transcoding (parameter sets frame)", paramter_sets_bytestream_transcoding_test),
	TEST_NO_TAG("Bytestream transcoding (parameter sets frame)", iframe_bytestream_transcoding_test)
};

test_suite_t h26x_tools_test_suite = {
	.name = "H26x Tools",
	.before_all = nullptr,
	.after_all = nullptr,
	.before_each = nullptr,
	.after_each = nullptr,
	.nb_tests = sizeof(tests)/sizeof(test_t),
	.tests = tests
};

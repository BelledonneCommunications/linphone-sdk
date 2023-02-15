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

#include <fstream>
#include <list>
#include <sstream>
#include <string>
#include <vector>

#include <bctoolbox/tester.h>

#include <mediastreamer2/msfactory.h>

#include "h26x/h26x-utils.h"

using namespace mediastreamer;
using namespace std;

static MSFactory *msFactory = nullptr;

static int initMSFactory() {
	msFactory = ms_factory_new_with_voip();
	return 0;
}

static int releaseMSFactory() {
	if (msFactory) ms_factory_destroy(msFactory);
	return 0;
}

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

static std::vector<uint8_t> loadFrameByteStream(const std::string &frame) {
	ostringstream filename;
	filename << bc_tester_get_resource_dir_prefix() << "/raw/" << frame;
	vector<uint8_t> byteStream = loadBinaryFile(filename.str());
	return byteStream;
}

static void bytestream_transcoding_test(const std::vector<uint8_t> &byteStream) {
	MSQueue nalus;
	ms_queue_init(&nalus);
	H26xUtils::byteStreamToNalus(byteStream, &nalus);
	BC_ASSERT(!ms_queue_empty(&nalus));

	vector<uint8_t> byteStream2;
	byteStream2.resize(byteStream.size() * 2);
	size_t filled = H26xUtils::nalusToByteStream(&nalus, &byteStream2[0], byteStream2.size());
	byteStream2.resize(filled);
	BC_ASSERT(ms_queue_empty(&nalus));
	BC_ASSERT(byteStream == byteStream2);

	ms_queue_flush(&nalus);
}

static void paramter_sets_bytestream_transcoding_test() {
	vector<uint8_t> byteStream = loadFrameByteStream("h265-parameter-sets-frame");
	bytestream_transcoding_test(byteStream);
}

static void iframe_bytestream_transcoding_test() {
	vector<uint8_t> byteStream = loadFrameByteStream("h265-iframe");
	bytestream_transcoding_test(byteStream);
}

static void bytestream_transcoding_two_consecutive_prevention_thee_bytes() {
	vector<uint8_t> byteStream = {0, 0, 0, 1, 0, 0, 3, 0, 0, 3, 0};
	bytestream_transcoding_test(byteStream);
}

static void packing_unpacking_test(const std::vector<uint8_t> &byteStream, const std::string &mime) {
	MSQueue nalus, rtp;

	ms_queue_init(&nalus);
	ms_queue_init(&rtp);

	const H26xToolFactory &factory = H26xToolFactory::get(mime);
	std::unique_ptr<NalPacker> packer(factory.createNalPacker(ms_factory_get_payload_max_size(msFactory)));
	std::unique_ptr<NalUnpacker> unpacker(factory.createNalUnpacker());
	packer->setPacketizationMode(NalPacker::NonInterleavedMode);
	packer->enableAggregation(true);

	H26xUtils::byteStreamToNalus(byteStream, &nalus);
	packer->pack(&nalus, &rtp, 0);

	BC_ASSERT(ms_queue_empty(&nalus));
	BC_ASSERT(!ms_queue_empty(&rtp));

	NalUnpacker::Status status;
	ms_queue_flush(&nalus);
	while (mblk_t *m = ms_queue_get(&rtp)) {
		status = unpacker->unpack(m, &nalus);
		if (status.frameAvailable) break;
	}

	BC_ASSERT(status.frameAvailable);
	BC_ASSERT(!status.frameCorrupted);
	BC_ASSERT(ms_queue_empty(&rtp));
	BC_ASSERT(!ms_queue_empty(&nalus));

	vector<uint8_t> byteStream2;
	byteStream2.resize(2 * byteStream.size());
	size_t filled = H26xUtils::nalusToByteStream(&nalus, &byteStream2[0], byteStream2.size());
	byteStream2.resize(filled);

	BC_ASSERT(byteStream == byteStream2);
}

static void packing_unpacking_test_h265_ps() {
	vector<uint8_t> byteStream = loadFrameByteStream("h265-parameter-sets-frame");
	packing_unpacking_test(byteStream, "video/hevc");
}

static void packing_unpacking_test_h265_iframe() {
	vector<uint8_t> byteStream = loadFrameByteStream("h265-iframe");
	packing_unpacking_test(byteStream, "video/hevc");
}

static test_t tests[] = {
    TEST_NO_TAG("Bytestream transcoding - paramter sets frame", paramter_sets_bytestream_transcoding_test),
    TEST_NO_TAG("Bytestream transcoding - i-frame", iframe_bytestream_transcoding_test),
    TEST_NO_TAG("Bytestream transcoding - two consecutive prevention three bytes",
                bytestream_transcoding_two_consecutive_prevention_thee_bytes),
    TEST_NO_TAG("H265 Packing/Unpacking - paramter sets frame", packing_unpacking_test_h265_ps),
    TEST_NO_TAG("H265 Packing/Unpacking - i-frame", packing_unpacking_test_h265_iframe)};

extern "C" {
test_suite_t h26x_tools_test_suite = {
    "H26x Tools", initMSFactory, releaseMSFactory, nullptr, nullptr, sizeof(tests) / sizeof(test_t), tests};
}

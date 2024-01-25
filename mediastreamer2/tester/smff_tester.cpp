/*
 * Copyright (c) 2024-2024 Belledonne Communications SARL.
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

#include "smff/smff.h"

using namespace mediastreamer;
using namespace std;

string testerRandomFileName(const std::string &name, const std::string &extension) {
	string fileName;
	char *random_filename = ms_tester_get_random_filename(name.c_str(), extension.c_str());
	char *file = bc_tester_file(random_filename);
	fileName = file;
	bc_free(random_filename);
	bc_free(file);
	return fileName;
}

static void write_and_read(void) {
	string fileName = testerRandomFileName("basic-", ".smff");
	SMFF::FileWriter fw;
	int i;
	const int numAudioRecords = 10;

	BC_ASSERT_TRUE(fw.open(fileName, false) == 0);

	BC_ASSERT_TRUE(bctbx_file_exist(fileName.c_str()) == 0);

	TrackWriterInterface &tw = fw.addTrack(0, "opus", TrackInterface::MediaType::Audio, 48000, 2).value();
	for (i = 0; i < numAudioRecords; ++i) {
		RecordInterface rec;
		ostringstream ostr;
		ostr << "buffer-" << i;
		string tmp = ostr.str();
		rec.timestamp = i;
		rec.data.inputBuffer = (const uint8_t *)tmp.c_str();
		rec.size = tmp.size();
		tw.addRecord(rec);
	}
	fw.close();

	SMFF::FileReader fr;
	BC_ASSERT_TRUE(fr.open(fileName) == 0);
	auto trackReaderList = fr.getTrackReaders();
	if (BC_ASSERT_TRUE(trackReaderList.size() == 1)) {
		TrackReaderInterface &tr = trackReaderList.front();
		BC_ASSERT_STRING_EQUAL(tr.getCodec().c_str(), "opus");
		BC_ASSERT_EQUAL(tr.getClockRate(), 48000, int, "%i");
		BC_ASSERT_EQUAL(tr.getChannels(), 2, int, "%i");
		BC_ASSERT_TRUE(tr.getType() == TrackInterface::MediaType::Audio);

		for (i = 0; i < numAudioRecords; ++i) {
			RecordInterface rec;
			uint8_t buffer[40];
			ostringstream ostr;
			ostr << "buffer-" << i;
			rec.timestamp = i;
			BC_ASSERT_TRUE(tr.read(rec));
			BC_ASSERT_EQUAL(rec.size, ostr.str().size(), int, "%i");
			rec.data.outputBuffer = buffer;
			rec.size = sizeof(buffer);
			BC_ASSERT_TRUE(tr.read(rec));
			BC_ASSERT_EQUAL(rec.size, ostr.str().size(), int, "%i");
			BC_ASSERT_TRUE(memcmp(rec.data.outputBuffer, ostr.str().c_str(), rec.size) == 0);
			tr.next();
		}
	}
	fr.close();
}

static void two_synchronized_tracks(void) {
	string fileName = testerRandomFileName("2tracks-", ".smff");
	SMFF::FileWriter fw;

	BC_ASSERT_TRUE(fw.open(fileName, false) == 0);

	BC_ASSERT_TRUE(bctbx_file_exist(fileName.c_str()) == 0);

	// will throw exception if the tracks are not returned.
	TrackWriterInterface &tw1 = fw.addTrack(0, "opus", TrackInterface::MediaType::Audio, 48000, 2).value();
	TrackWriterInterface &tw2 = fw.addTrack(1, "H265", TrackInterface::MediaType::Video, 90000, 1).value();

	fw.synchronizeTracks();

	RecordInterface rec;

	rec.timestamp = 4800;
	rec.data.inputBuffer = (const uint8_t *)"abcd";
	rec.size = 4;
	tw1.addRecord(rec);

	rec.timestamp = 1234000;
	rec.data.inputBuffer = (const uint8_t *)"efghijkl";
	rec.size = 8;
	tw2.addRecord(rec);

	fw.synchronizeTracks();

	rec.timestamp = 7000;
	rec.data.inputBuffer = (const uint8_t *)"xyz0";
	rec.size = 4;
	tw1.addRecord(rec);
	rec.timestamp = 7000 + 24000;
	rec.data.inputBuffer = (const uint8_t *)"dodo";
	rec.size = 4;
	tw1.addRecord(rec);

	rec.timestamp = 28000;
	rec.data.inputBuffer = (const uint8_t *)"efghijkl";
	rec.size = 8;
	tw2.addRecord(rec);

	rec.timestamp = 38000;
	rec.data.inputBuffer = (const uint8_t *)"gnagnagn";
	rec.size = 8;
	tw2.addRecord(rec);

	fw.close();

	SMFF::FileReader fr;
	BC_ASSERT_TRUE(fr.open(fileName) == 0);
	auto trackReaderList = fr.getTrackReaders();
	BC_ASSERT_EQUAL(trackReaderList.size(), 2, int, "%i");
	if (trackReaderList.size() >= 2) {
		TrackReaderInterface &tr1 = trackReaderList.front();
		TrackReaderInterface &tr2 = trackReaderList.back();
		BC_ASSERT_STRING_EQUAL(tr1.getCodec().c_str(), "opus");
		BC_ASSERT_EQUAL(tr1.getClockRate(), 48000, int, "%i");
		BC_ASSERT_EQUAL(tr1.getChannels(), 2, int, "%i");
		BC_ASSERT_TRUE(tr1.getType() == TrackInterface::MediaType::Audio);

		BC_ASSERT_STRING_EQUAL(tr2.getCodec().c_str(), "H265");
		BC_ASSERT_EQUAL(tr2.getClockRate(), 90000, int, "%i");
		BC_ASSERT_EQUAL(tr2.getChannels(), 1, int, "%i");
		BC_ASSERT_TRUE(tr2.getType() == TrackInterface::MediaType::Video);

		RecordInterface rec;
		rec.timestamp = 0;
		BC_ASSERT_TRUE(tr1.read(rec));
		BC_ASSERT_EQUAL(rec.size, 4, int, "%i");
		BC_ASSERT_TRUE(rec.timestamp == 0);
		tr1.next();

		rec = {};
		rec.timestamp = 0;
		BC_ASSERT_TRUE(tr2.read(rec));
		BC_ASSERT_EQUAL(rec.size, 8, int, "%i");
		BC_ASSERT_TRUE(rec.timestamp == 0);
		tr2.next();

		rec = {};
		rec.timestamp = 2000;
		BC_ASSERT_FALSE(tr1.read(rec));

		rec = {};
		rec.timestamp = 4800;
		BC_ASSERT_TRUE(tr1.read(rec));
		BC_ASSERT_TRUE(rec.timestamp == 4800);
		tr1.next();

		rec.timestamp = 12000;
		BC_ASSERT_FALSE(tr1.read(rec));

		rec.timestamp = 4800 + 24000;
		BC_ASSERT_TRUE(tr1.read(rec));
		BC_ASSERT_TRUE(rec.timestamp == 4800 + 24000);
		tr1.next();

		rec = {};
		rec.timestamp = 15000;
		BC_ASSERT_FALSE(tr2.read(rec));

		rec.timestamp = 45000 + 9000;
		BC_ASSERT_TRUE(tr2.read(rec));
		BC_ASSERT_EQUAL(rec.size, 8, int, "%i");
		BC_ASSERT_TRUE(rec.timestamp == 54000);
		tr2.next();

		rec = {};
		rec.timestamp = 60000;
		BC_ASSERT_FALSE(tr2.read(rec));

		rec.timestamp = 64000;
		BC_ASSERT_TRUE(tr2.read(rec));
		BC_ASSERT_EQUAL(rec.size, 8, int, "%i");
		BC_ASSERT_TRUE(rec.timestamp == 64000);
	}
	fr.close();
}

static void write_append_and_read(void) {
	string fileName = testerRandomFileName("append-", ".smff");
	SMFF::FileWriter fw;
	int i;
	const int numAudioRecords = 10;

	BC_ASSERT_TRUE(fw.open(fileName, false) == 0);

	BC_ASSERT_TRUE(bctbx_file_exist(fileName.c_str()) == 0);

	TrackWriterInterface &tw = fw.addTrack(0, "opus", TrackInterface::MediaType::Audio, 48000, 2).value();
	for (i = 0; i < numAudioRecords; ++i) {
		RecordInterface rec;
		ostringstream ostr;
		ostr << "buffer-" << i;
		string tmp = ostr.str();
		rec.timestamp = i;
		rec.data.inputBuffer = (const uint8_t *)tmp.c_str();
		rec.size = tmp.size();
		tw.addRecord(rec);
	}
	fw.close();

	SMFF::FileWriter fw2;

	BC_ASSERT_TRUE(fw2.open(fileName, true) == 0);
	fw2.close();

	SMFF::FileReader fr;
	BC_ASSERT_TRUE(fr.open(fileName) == 0);
	auto trackReaderList = fr.getTrackReaders();
	if (BC_ASSERT_TRUE(trackReaderList.size() == 1)) {
		TrackReaderInterface &tr = trackReaderList.front();
		BC_ASSERT_STRING_EQUAL(tr.getCodec().c_str(), "opus");
		BC_ASSERT_EQUAL(tr.getClockRate(), 48000, int, "%i");
		BC_ASSERT_EQUAL(tr.getChannels(), 2, int, "%i");
		BC_ASSERT_TRUE(tr.getType() == TrackInterface::MediaType::Audio);
		BC_ASSERT_EQUAL(dynamic_cast<SMFF::TrackReader &>(tr).getNumRecords(), numAudioRecords, int, "%i");

		for (i = 0; i < numAudioRecords; ++i) {
			RecordInterface rec;
			uint8_t buffer[40];
			ostringstream ostr;
			ostr << "buffer-" << i;
			rec.timestamp = i;
			BC_ASSERT_TRUE(tr.read(rec));
			BC_ASSERT_EQUAL(rec.size, ostr.str().size(), int, "%i");
			rec.data.outputBuffer = buffer;
			rec.size = sizeof(buffer);
			BC_ASSERT_TRUE(tr.read(rec));
			BC_ASSERT_EQUAL(rec.size, ostr.str().size(), int, "%i");
			BC_ASSERT_TRUE(memcmp(rec.data.outputBuffer, ostr.str().c_str(), rec.size) == 0);
			tr.next();
		}
	}
	fr.close();
}

static test_t tests[] = {TEST_NO_TAG("Write and read", write_and_read),
                         TEST_NO_TAG("With 2 synchronized tracks.", two_synchronized_tracks),
                         TEST_NO_TAG("Write, append, and read", write_append_and_read)};

test_suite_t smff_test_suite = {"Simple Multimedia File Format",  NULL,  NULL, NULL, NULL,
                                sizeof(tests) / sizeof(tests[0]), tests, 0};

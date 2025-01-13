/*
 * Copyright (c) 2010-2024 Belledonne Communications SARL.
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

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "bctoolbox/tester.h"
#include "mediastreamer2/baudot.h"
#include "mediastreamer2/msfactory.h"
#include "mediastreamer2/msfileplayer.h"
#include "mediastreamer2/msutils.h"

#include "baudot/baudot_encoding_context.h"
#include "mediastreamer2_tester_private.h"

using namespace mediastreamer;
using namespace std;

#define MARIELLE_RTP_PORT (base_port + 1)
#define MARIELLE_RTCP_PORT (base_port + 2)

#define MARGAUX_RTP_PORT (base_port + 3)
#define MARGAUX_RTCP_PORT (base_port + 4)

#define BAUDOT_ALPHABET_US_INPUT_FILE "sounds/baudot_mono_alphabet_us.wav"
#define BAUDOT_DIGITS_US_INPUT_FILE "sounds/baudot_mono_digits_us.wav"
#define BAUDOT_LETTER_BY_LETTER_US_INPUT_FILE "sounds/baudot_mono_alphabet_letter_by_letter_us.wav"
#define BAUDOT_STEREO_ALPHABET_US_INPUT_FILE "sounds/baudot_stereo_alphabet_us.wav"
#define BAUDOT_ALPHABET_SILENCE_INPUT_FILE "sounds/baudot_alphabet_silence.wav"
#define HELLO_8K_FILE "sounds/hello8000.wav"
#define RECORDED_8K_FILE "recorded_baudot-"

class BaudotTestStats {
public:
	BaudotTestStats() = default;
	~BaudotTestStats() = default;

	void checkNoDetection() {
		BC_ASSERT_EQUAL(mNbDetectorStateTty45, 0, int, "%d");
		BC_ASSERT_EQUAL(mNbDetectorStateTty50, 0, int, "%d");
		BC_ASSERT_EQUAL((int)mReceivedCharacters.size(), 0, int, "%d");
	}

	void checkReceivedCharacters(const std::string &expectedText) {
		std::vector<uint8_t> expectedOutputVec;
		const char *expectedTextC = expectedText.c_str();
		for (int i = 0; i < (int)expectedText.length(); i++) {
			expectedOutputVec.push_back(toupper(expectedTextC[i]));
		}

		for (auto receivedIt = mReceivedCharacters.begin(), expectedIt = expectedOutputVec.begin();
		     (receivedIt != mReceivedCharacters.end()) && (expectedIt != expectedOutputVec.end());
		     receivedIt++, expectedIt++) {
			BC_ASSERT_EQUAL(*receivedIt, *expectedIt, uint8_t, "%02X");
		}

		BC_ASSERT_EQUAL((int)mReceivedCharacters.size(), (int)expectedOutputVec.size(), int, "%d");
	}

	void checkStandard(MSBaudotStandard standard) {
		switch (standard) {
			default:
			case MSBaudotStandardUs:
				BC_ASSERT_GREATER(mNbDetectorStateTty45, 1, int, "%d");
				BC_ASSERT_EQUAL(mNbDetectorStateTty50, 0, int, "%d");
				break;
			case MSBaudotStandardEurope:
				BC_ASSERT_GREATER(mNbDetectorStateTty50, 1, int, "%d");
				BC_ASSERT_EQUAL(mNbDetectorStateTty45, 0, int, "%d");
				break;
		}
	}

	void receiveCharacter(char receivedCharacter) {
		mReceivedCharacters.push_back((uint8_t)receivedCharacter);
	}

	void reset() {
		memset(&mRtpStats, 0, sizeof(rtp_stats_t));
		mReceivedCharacters.clear();
		mNbEndOfFile = 0;
		mNbDetectorStateTty45 = 0;
		mNbDetectorStateTty50 = 0;
	}

	rtp_stats_t mRtpStats;
	int mNbEndOfFile = 0;
	int mNbDetectorStateTty45 = 0;
	int mNbDetectorStateTty50 = 0;

private:
	std::vector<uint8_t> mReceivedCharacters;
};

static const std::string sgUppercaseAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const std::string sgLowercaseAlphabet = "abcdefghijklmnopqrstuvwxyz";
static const std::string sgDigits = "0123456789";
static const std::string sgSpecialCharsUs = " \n-\a\r$\',!:(\")=?+./;\b";
static const std::string sgSpecialCharsEurope = " \n-\'\r\x05\a,!:(+)\x9c?&./=\b";
static const std::string sgLoremIpsum = "Lorem ipsum dolor sit amet, consectetur adipiscing elit sed do eiusmod "
                                        "tempor incididunt ut labore et dolore magna aliqua. ";
static const uint8_t sgAlphabetExpectedOutput[] = {
    0b11111111, // CARRIER
    0b11111110, // LETTERS
    0b11000110, // A
    0b11110010, // B
    0b11011100, // C
    0b11010010, // D
    0b11000010, // E
    0b11011010, // F
    0b11110100, // G
    0b11101000, // H
    0b11001100, // I
    0b11010110, // J
    0b11011110, // K
    0b11100100, // L
    0b11111000, // M
    0b11011000, // N
    0b11110000, // O
    0b11101100, // P
    0b11101110, // Q
    0b11010100, // R
    0b11001010, // S
    0b11100000, // T
    0b11001110, // U
    0b11111100, // V
    0b11100110, // W
    0b11111010, // X
    0b11101010, // Y
    0b11100010, // Z
};
static const uint8_t sgDigitsExpectedOutput[] = {
    0b11111111, // CARRIER
    0b11111110, // LETTERS
    0b11110110, // FIGURES
    0b11101100, // 0
    0b11101110, // 1
    0b11100110, // 2
    0b11000010, // 3
    0b11010100, // 4
    0b11100000, // 5
    0b11101010, // 6
    0b11001110, // 7
    0b11001100, // 8
    0b11110000, // 9
};
static const uint8_t sgSpecialCharsUsExpectedOutput[] = {
    0b11111111, // CARRIER
    0b11111110, // LETTERS
    0b11001000, // SPACE
    0b11000100, // LF
    0b11110110, // FIGURES
    0b11000110, // -
    0b11001010, // BELL
    0b11010000, // CR
    0b11010010, // $
    0b11010110, // '
    0b11011000, // ,
    0b11011010, // !
    0b11011100, // :
    0b11011110, // (
    0b11100010, // "
    0b11100100, // )
    0b11101000, // =
    0b11110010, // ?
    0b11110100, // +
    0b11111000, // .
    0b11111010, // /
    0b11111100, // ;
    0b11000000, // BACKSPACE
};
static const uint8_t sgSpecialCharsEuropeExpectedOutput[] = {
    0b11111111, // CARRIER
    0b11111110, // LETTERS
    0b11001000, // SPACE
    0b11000100, // LF
    0b11110110, // FIGURES
    0b11000110, // -
    0b11001010, // '
    0b11010000, // CR
    0b11010010, // WRU
    0b11010110, // BELL
    0b11011000, // ,
    0b11011010, // !
    0b11011100, // :
    0b11011110, // (
    0b11100010, // +
    0b11100100, // )
    0b11101000, // £
    0b11110010, // ?
    0b11110100, // &
    0b11111000, // .
    0b11111010, // /
    0b11111100, // =
    0b11000000, // BACKSPACE
};
static const uint8_t sgLoremIpsumExpectedOutput[] = {
    0b11111111, // CARRIER
    0b11111110, // LETTERS
    0b11100100, // L
    0b11110000, // o
    0b11010100, // r
    0b11000010, // e
    0b11111000, // m
    0b11001000, // SPACE
    0b11001100, // i
    0b11101100, // p
    0b11001010, // s
    0b11001110, // u
    0b11111000, // m
    0b11001000, // SPACE
    0b11010010, // d
    0b11110000, // o
    0b11100100, // l
    0b11110000, // o
    0b11010100, // r
    0b11001000, // SPACE
    0b11001010, // s
    0b11001100, // i
    0b11100000, // t
    0b11001000, // SPACE
    0b11000110, // a
    0b11111000, // m
    0b11000010, // e
    0b11100000, // t
    0b11110110, // FIGURES
    0b11011000, // ,
    0b11001000, // SPACE
    0b11111110, // LETTERS
    0b11011100, // c
    0b11110000, // o
    0b11011000, // n
    0b11001010, // s
    0b11000010, // e
    0b11011100, // c
    0b11100000, // t
    0b11000010, // e
    0b11100000, // t
    0b11001110, // u
    0b11010100, // r
    0b11001000, // SPACE
    0b11000110, // a
    0b11010010, // d
    0b11001100, // i
    0b11101100, // p
    0b11001100, // i
    0b11001010, // s
    0b11011100, // c
    0b11001100, // i
    0b11011000, // n
    0b11110100, // g
    0b11001000, // SPACE
    0b11000010, // e
    0b11100100, // l
    0b11001100, // i
    0b11100000, // t
    0b11001000, // SPACE
    0b11001010, // s
    0b11000010, // e
    0b11010010, // d
    0b11001000, // SPACE
    0b11010010, // d
    0b11110000, // o
    0b11010000, // CR
    0b11001000, // SPACE
    0b11000010, // e
    0b11001100, // i
    0b11001110, // u
    0b11001010, // s
    0b11111000, // m
    0b11110000, // o
    0b11010010, // d
    0b11001000, // SPACE
    0b11100000, // t
    0b11000010, // e
    0b11111000, // m
    0b11101100, // p
    0b11110000, // o
    0b11010100, // r
    0b11001000, // SPACE
    0b11001100, // i
    0b11011000, // n
    0b11011100, // c
    0b11001100, // i
    0b11010010, // d
    0b11001100, // i
    0b11010010, // d
    0b11001110, // u
    0b11011000, // n
    0b11100000, // t
    0b11001000, // SPACE
    0b11001110, // u
    0b11100000, // t
    0b11001000, // SPACE
    0b11100100, // l
    0b11000110, // a
    0b11110010, // b
    0b11110000, // o
    0b11010100, // r
    0b11000010, // e
    0b11001000, // SPACE
    0b11111110, // LETTERS
    0b11000010, // e
    0b11100000, // t
    0b11001000, // SPACE
    0b11010010, // d
    0b11110000, // o
    0b11100100, // l
    0b11110000, // o
    0b11010100, // r
    0b11000010, // e
    0b11001000, // SPACE
    0b11111000, // m
    0b11000110, // a
    0b11110100, // g
    0b11011000, // n
    0b11000110, // a
    0b11001000, // SPACE
    0b11000110, // a
    0b11100100, // l
    0b11001100, // i
    0b11101110, // q
    0b11001110, // u
    0b11000110, // a
    0b11110110, // FIGURES
    0b11111000, // .
    0b11010000, // CR
    0b11001000, // SPACE
};
static const uint8_t sgAlphabetWithPauseExpectedOutput[] = {
    0b11111111, // CARRIER
    0b11111110, // LETTERS
    0b11000110, // A
    0b11110010, // B
    0b11011100, // C
    0b11010010, // D
    0b11000010, // E
    0b11011010, // F
    0b11110100, // G
    0b11101000, // H
    0b11001100, // I
    0b11010110, // J
    0b11011110, // K
    0b11100100, // L
    0b11111000, // M
    0b11111111, // CARRIER
    0b11111110, // LETTERS
    0b11011000, // N
    0b11110000, // O
    0b11101100, // P
    0b11101110, // Q
    0b11010100, // R
    0b11001010, // S
    0b11100000, // T
    0b11001110, // U
    0b11111100, // V
    0b11100110, // W
    0b11111010, // X
    0b11101010, // Y
    0b11100010, // Z
};

static MSFactory *sgFactory = nullptr;

static int initMSFactory() {
	sgFactory = ms_tester_factory_new();
	return 0;
}

static int releaseMSFactory() {
	if (sgFactory) ms_factory_destroy(sgFactory);
	return 0;
}

static void test_baudot_text_encoding(MSBaudotStandard standard,
                                      const string &input,
                                      const uint8_t *expectedOutput,
                                      int expectedOutputLength) {
	BaudotEncodingContext context;
	context.setStandard(standard);
	context.encodeString(input);
	std::optional<uint8_t> code;
	int nbCodes = 0;
	do {
		code = context.nextCode();
		if (code.has_value()) {
			BC_ASSERT_EQUAL(code.value(), expectedOutput[nbCodes], uint8_t, "%02X");
			nbCodes++;
		}
	} while (code.has_value() && (nbCodes < expectedOutputLength));
	BC_ASSERT_EQUAL(nbCodes, expectedOutputLength, int, "%d");
}

#define TEST_BAUDOT_TEXT_ENCODING(standard, input, output)                                                             \
	test_baudot_text_encoding(standard, input, output, sizeof(output) / sizeof(uint8_t))

static void test_baudot_text_encoding_uppercase_alphabet() {
	TEST_BAUDOT_TEXT_ENCODING(MSBaudotStandardUs, sgUppercaseAlphabet, sgAlphabetExpectedOutput);
}

static void test_baudot_text_encoding_lowercase_alphabet() {
	TEST_BAUDOT_TEXT_ENCODING(MSBaudotStandardEurope, sgLowercaseAlphabet, sgAlphabetExpectedOutput);
}

static void test_baudot_text_encoding_digits() {
	TEST_BAUDOT_TEXT_ENCODING(MSBaudotStandardUs, sgDigits, sgDigitsExpectedOutput);
}

static void test_baudot_text_encoding_special_chars_us() {
	TEST_BAUDOT_TEXT_ENCODING(MSBaudotStandardUs, sgSpecialCharsUs, sgSpecialCharsUsExpectedOutput);
}

static void test_baudot_text_encoding_special_chars_europe() {
	TEST_BAUDOT_TEXT_ENCODING(MSBaudotStandardEurope, sgSpecialCharsEurope, sgSpecialCharsEuropeExpectedOutput);
}

// Tests CR/LF and LETTERS retransmission.
static void test_baudot_text_encoding_lorem_ipsum() {
	TEST_BAUDOT_TEXT_ENCODING(MSBaudotStandardUs, sgLoremIpsum, sgLoremIpsumExpectedOutput);
}

static void test_baudot_text_encoding_letter_by_letter() {
	const std::string &input = sgUppercaseAlphabet;
	static const uint8_t *expectedOutput = sgAlphabetWithPauseExpectedOutput;
	int expectedOutputLength = sizeof(sgAlphabetWithPauseExpectedOutput) / sizeof(uint8_t);

	BaudotEncodingContext context;
	context.setPauseTimeout(1);

	auto middleIt = input.begin() + (input.size() / 2) - 1;
	for (auto it = input.begin(); it != input.end(); it++) {
		context.encodeChar(*it);
		if (it == middleIt) {
			bctbx_sleep_ms(1100); // Wait for a duration longer than the pause timeout.
		} else {
			bctbx_sleep_ms(50);
		}
	}

	std::optional<uint8_t> code;
	int nbCodes = 0;
	do {
		code = context.nextCode();
		if (code.has_value()) {
			BC_ASSERT_EQUAL(code.value(), expectedOutput[nbCodes], uint8_t, "%02X");
			nbCodes++;
		}
	} while (code.has_value() && (nbCodes < expectedOutputLength));
	BC_ASSERT_EQUAL(nbCodes, expectedOutputLength, int, "%d");
}

static void test_baudot_detector_notify_cb(void *data, MSFilter *f, unsigned int eventId, void *event) {
	BaudotTestStats *stats = reinterpret_cast<BaudotTestStats *>(data);
	if (eventId == MS_BAUDOT_DETECTOR_STATE_EVENT) {
		MSBaudotDetectorState state = *reinterpret_cast<MSBaudotDetectorState *>(event);
		ms_message("Baudot detector state change notified: %s",
		           state == MSBaudotDetectorStateTty45 ? "TTY45" : "TTY50");
		switch (state) {
			default:
			case MSBaudotDetectorStateTty45:
				stats->mNbDetectorStateTty45++;
				break;
			case MSBaudotDetectorStateTty50:
				stats->mNbDetectorStateTty50++;
				break;
		}
		bool_t decodingEnabled = true;
		ms_filter_call_method(f, MS_BAUDOT_DETECTOR_ENABLE_DECODING, &decodingEnabled);
	} else if (eventId == MS_BAUDOT_DETECTOR_CHARACTER_EVENT) {
		char receivedCharacter = *reinterpret_cast<char *>(event);
		ms_message("Baudot detector char notified: %02X", receivedCharacter);
		stats->receiveCharacter(receivedCharacter);
	}
}

static void test_baudot_soundread_notify_cb(void *data,
                                            BCTBX_UNUSED(MSFilter *f),
                                            unsigned int eventId,
                                            BCTBX_UNUSED(void *event)) {
	BaudotTestStats *stats = reinterpret_cast<BaudotTestStats *>(data);
	switch (eventId) {
		case MS_FILE_PLAYER_EOF:
			ms_message("EndOfFile received");
			stats->mNbEndOfFile++;
			break;
		default:
			break;
	}
}

static void test_baudot_audio_stream_base_2(const std::string &marielleLocalIp,
                                            const std::string &marielleRemoteIp,
                                            int marielleLocalRtpPort,
                                            int marielleRemoteRtpPort,
                                            int marielleLocalRtcpPort,
                                            int marielleRemoteRtcpPort,
                                            const std::string &margauxLocalIp,
                                            const std::string &margauxRemoteIp,
                                            int margauxLocalRtpPort,
                                            int margauxRemoteRtpPort,
                                            int margauxLocalRtcpPort,
                                            int margauxRemoteRtcpPort,
                                            std::optional<const string> inputAudioFilename,
                                            std::optional<const string> silenceAudioFilename,
                                            const std::string &baudotText,
                                            MSBaudotStandard standard,
                                            bool isCharacterByCharacter,
                                            std::optional<int> customTimeout) {
	int lossPercentage = 0;
	int dummy = 0;

	AudioStream *marielle =
	    audio_stream_new2(sgFactory, marielleLocalIp.c_str(), marielleLocalRtpPort, marielleLocalRtcpPort);
	BaudotTestStats marielleStats;
	AudioStream *margaux =
	    audio_stream_new2(sgFactory, margauxLocalIp.c_str(), margauxLocalRtpPort, margauxLocalRtcpPort);
	BaudotTestStats margauxStats;

	char *inputAudioFile = nullptr;
	if (inputAudioFilename.has_value()) {
		inputAudioFile = bc_tester_res(inputAudioFilename.value().c_str());
	}
	char *recordedFileName = ms_tester_get_random_filename(RECORDED_8K_FILE, ".wav");
	char *recordedFile = bc_tester_file(recordedFileName);
	bctbx_free(recordedFileName);

	rtp_session_set_multicast_loopback(marielle->ms.sessions.rtp_session, TRUE);
	rtp_session_set_multicast_loopback(margaux->ms.sessions.rtp_session, TRUE);
	rtp_session_set_rtcp_report_interval(marielle->ms.sessions.rtp_session, 1000);
	rtp_session_set_rtcp_report_interval(margaux->ms.sessions.rtp_session, 1000);

	marielleStats.reset();
	margauxStats.reset();

	RtpProfile *profile = rtp_profile_new("default profile");
	rtp_profile_set_payload(profile, 0, &payload_type_pcmu8000);

	BC_ASSERT_EQUAL(audio_stream_start_full(
	                    margaux, profile,
	                    ms_is_multicast(margauxLocalIp.c_str()) ? margauxLocalIp.c_str() : margauxRemoteIp.c_str(),
	                    ms_is_multicast(margauxLocalIp.c_str()) ? margauxLocalRtpPort : margauxRemoteRtpPort,
	                    margauxRemoteIp.c_str(), margauxRemoteRtcpPort, 0, 50, NULL, recordedFile, NULL, NULL, 0),
	                0, int, "%d");
	ms_filter_add_notify_callback(margaux->baudot_detector, test_baudot_detector_notify_cb, &margauxStats, TRUE);

	BC_ASSERT_EQUAL(audio_stream_start_full(marielle, profile, marielleRemoteIp.c_str(), marielleRemoteRtpPort,
	                                        marielleRemoteIp.c_str(), marielleRemoteRtcpPort, 0, 50, inputAudioFile,
	                                        NULL, NULL, NULL, 0),
	                0, int, "%d");

	if (inputAudioFilename) {
		ms_filter_add_notify_callback(marielle->soundread, test_baudot_soundread_notify_cb, &marielleStats, TRUE);

		wait_for_until(&marielle->ms, &margaux->ms, &marielleStats.mNbEndOfFile, 1,
		               customTimeout.has_value()
		                   ? customTimeout.value()
		                   : ((int)baudotText.size() * 8 * (standard == MSBaudotStandardUs ? 22 : 20)) + 1500);

		audio_stream_play(marielle, NULL);
		BC_ASSERT_EQUAL(marielleStats.mNbEndOfFile, 1, int, "%d");
	} else {
		audio_stream_set_baudot_sending_mode(marielle, (standard == MSBaudotStandardEurope) ? MSBaudotModeTty50
		                                                                                    : MSBaudotModeTty45);
		if (isCharacterByCharacter) {
			for (const auto &c : baudotText) {
				audio_stream_send_baudot_character(marielle, c);
				wait_for_until(&marielle->ms, &margaux->ms, &dummy, 1,
				               8 * ((standard == MSBaudotStandardUs) ? 22 : 20) + 60);
			}
			wait_for_until(&marielle->ms, &margaux->ms, &dummy, 1, 1000);
		} else {
			audio_stream_send_baudot_string(marielle, baudotText.c_str());
			wait_for_until(&marielle->ms, &margaux->ms, &dummy, 1,
			               customTimeout.has_value()
			                   ? customTimeout.value()
			                   : ((int)baudotText.size() * 8 * (standard == MSBaudotStandardUs ? 22 : 20)) + 1000);
		}
	}

	// if (rtp_session_rtcp_enabled(marielle->ms.sessions.rtp_session) &&
	//     rtp_session_rtcp_enabled(margaux->ms.sessions.rtp_session)) {
	// 	BC_ASSERT_GREATER_STRICT(rtp_session_get_round_trip_propagation(marielle->ms.sessions.rtp_session), 0, float,
	// 	                         "%f");
	// 	BC_ASSERT_GREATER_STRICT(rtp_session_get_stats(marielle->ms.sessions.rtp_session)->recv_rtcp_packets, 0,
	// 	                         unsigned long long, "%llu");
	// }

	audio_stream_get_local_rtp_stats(marielle, &marielleStats.mRtpStats);
	audio_stream_stop(marielle);
	wait_for_until(&margaux->ms, NULL, &dummy, 1, 100);

	audio_stream_get_local_rtp_stats(margaux, &margauxStats.mRtpStats);
	audio_stream_stop(margaux);
	BC_ASSERT_TRUE(margauxStats.mRtpStats.hw_recv >= ((marielleStats.mRtpStats.sent * (100 - lossPercentage)) / 100));

	margauxStats.checkStandard(standard);
	margauxStats.checkReceivedCharacters(baudotText);

	if (silenceAudioFilename) {
		// Check that the Baudot detection silenced the received audio.
		const MSAudioDiffParams audio_cmp_params = {10, 200};
		double similar = 0.0;
		const double threshold = 0.85;
		char *silenceAudioFile = bc_tester_res(silenceAudioFilename.value().c_str());
		BC_ASSERT_EQUAL(ms_audio_diff(silenceAudioFile, recordedFile, &similar, &audio_cmp_params, NULL, NULL), 0, int,
		                "%d");
		BC_ASSERT_GREATER(similar, threshold, double, "%f");
		BC_ASSERT_LOWER(similar, 1.0, double, "%f");
		free(silenceAudioFile);
	}

	rtp_profile_destroy(profile);
	unlink(recordedFile);
	free(recordedFile);
	if (inputAudioFile) {
		free(inputAudioFile);
	}
}

static void test_baudot_audio_stream_base(const std::string &marielleLocalIp,
                                          int marielleLocalRtpPort,
                                          int marielleLocalRtcpPort,
                                          const std::string &margauxLocalIp,
                                          int margauxLocalRtpPort,
                                          int margauxLocalRtcpPort,
                                          std::optional<const std::string> inputAudioFilename,
                                          std::optional<const std::string> silenceAudioFilename,
                                          const std::string &baudotText,
                                          MSBaudotStandard standard,
                                          bool isCharacterByCharacter,
                                          std::optional<int> customTimeout) {
	test_baudot_audio_stream_base_2(marielleLocalIp, margauxLocalIp, marielleLocalRtpPort, margauxLocalRtpPort,
	                                marielleLocalRtcpPort, margauxLocalRtcpPort, margauxLocalIp, marielleLocalIp,
	                                margauxLocalRtpPort, marielleLocalRtpPort, margauxLocalRtcpPort,
	                                marielleLocalRtcpPort, inputAudioFilename, silenceAudioFilename, baudotText,
	                                standard, isCharacterByCharacter, customTimeout);
}

#define TEST_BAUDOT_AUDIO_STREAM(inputFilename, silenceFilename, text, standard, isCharacterByCharacter)               \
	test_baudot_audio_stream_base(MARIELLE_IP, MARIELLE_RTP_PORT, MARIELLE_RTCP_PORT, MARGAUX_IP, MARGAUX_RTP_PORT,    \
	                              MARGAUX_RTCP_PORT, inputFilename, silenceFilename, text, standard,                   \
	                              isCharacterByCharacter, std::nullopt);

static void test_baudot_audio_stream_uppercase_alphabet_us() {
	TEST_BAUDOT_AUDIO_STREAM(std::nullopt, std::nullopt, sgUppercaseAlphabet, MSBaudotStandardUs, false);
}

static void test_baudot_audio_stream_uppercase_alphabet_europe() {
	TEST_BAUDOT_AUDIO_STREAM(std::nullopt, std::nullopt, sgUppercaseAlphabet, MSBaudotStandardEurope, false);
}

static void test_baudot_audio_stream_lowercase_alphabet_us() {
	TEST_BAUDOT_AUDIO_STREAM(std::nullopt, std::nullopt, sgLowercaseAlphabet, MSBaudotStandardUs, false);
}

static void test_baudot_audio_stream_lowercase_alphabet_europe() {
	TEST_BAUDOT_AUDIO_STREAM(std::nullopt, std::nullopt, sgLowercaseAlphabet, MSBaudotStandardEurope, false);
}

static void test_baudot_audio_stream_digits_us() {
	TEST_BAUDOT_AUDIO_STREAM(std::nullopt, std::nullopt, sgDigits, MSBaudotStandardUs, false);
}

static void test_baudot_audio_stream_digits_europe() {
	TEST_BAUDOT_AUDIO_STREAM(std::nullopt, std::nullopt, sgDigits, MSBaudotStandardEurope, false);
}

static void test_baudot_audio_stream_special_chars_us() {
	TEST_BAUDOT_AUDIO_STREAM(std::nullopt, std::nullopt, sgSpecialCharsUs, MSBaudotStandardUs, false);
}

static void test_baudot_audio_stream_special_chars_europe() {
	TEST_BAUDOT_AUDIO_STREAM(std::nullopt, std::nullopt, sgSpecialCharsEurope, MSBaudotStandardEurope, false);
}

static void test_baudot_audio_stream_character_by_character_uppercase_alphabet_us() {
	TEST_BAUDOT_AUDIO_STREAM(std::nullopt, std::nullopt, sgUppercaseAlphabet, MSBaudotStandardUs, true);
}

static void test_baudot_audio_stream_character_by_character_uppercase_alphabet_europe() {
	TEST_BAUDOT_AUDIO_STREAM(std::nullopt, std::nullopt, sgUppercaseAlphabet, MSBaudotStandardEurope, true);
}

static void test_baudot_audio_stream_character_by_character_lowercase_alphabet_us() {
	TEST_BAUDOT_AUDIO_STREAM(std::nullopt, std::nullopt, sgLowercaseAlphabet, MSBaudotStandardUs, true);
}

static void test_baudot_audio_stream_character_by_character_lowercase_alphabet_europe() {
	TEST_BAUDOT_AUDIO_STREAM(std::nullopt, std::nullopt, sgLowercaseAlphabet, MSBaudotStandardEurope, true);
}

static void test_baudot_audio_stream_character_by_character_digits_us() {
	TEST_BAUDOT_AUDIO_STREAM(std::nullopt, std::nullopt, sgDigits, MSBaudotStandardUs, true);
}

static void test_baudot_audio_stream_character_by_character_digits_europe() {
	TEST_BAUDOT_AUDIO_STREAM(std::nullopt, std::nullopt, sgDigits, MSBaudotStandardEurope, true);
}

static void test_baudot_audio_stream_character_by_character_special_chars_us() {
	TEST_BAUDOT_AUDIO_STREAM(std::nullopt, std::nullopt, sgSpecialCharsUs, MSBaudotStandardUs, true);
}

static void test_baudot_audio_stream_character_by_character_special_chars_europe() {
	TEST_BAUDOT_AUDIO_STREAM(std::nullopt, std::nullopt, sgSpecialCharsEurope, MSBaudotStandardEurope, true);
}

static void test_baudot_detection_from_file_alphabet_us() {
	TEST_BAUDOT_AUDIO_STREAM(BAUDOT_ALPHABET_US_INPUT_FILE, BAUDOT_ALPHABET_SILENCE_INPUT_FILE, sgUppercaseAlphabet,
	                         MSBaudotStandardUs, false);
}

static void test_baudot_detection_from_file_digits_us() {
	TEST_BAUDOT_AUDIO_STREAM(BAUDOT_DIGITS_US_INPUT_FILE, std::nullopt, sgDigits, MSBaudotStandardUs, false);
}

// Tests with some silence between characters & with some long marks of 1.5x bit duration to mark the end of characters.
static void test_baudot_detection_from_file_letter_by_letter_us() {
	test_baudot_audio_stream_base(MARIELLE_IP, MARIELLE_RTP_PORT, MARIELLE_RTCP_PORT, MARGAUX_IP, MARGAUX_RTP_PORT,
	                              MARGAUX_RTCP_PORT, BAUDOT_LETTER_BY_LETTER_US_INPUT_FILE, std::nullopt,
	                              sgUppercaseAlphabet, MSBaudotStandardUs, false, 8000);
}

static void test_baudot_detection_from_file_stereo_alphabet_us() {
	TEST_BAUDOT_AUDIO_STREAM(BAUDOT_STEREO_ALPHABET_US_INPUT_FILE, std::nullopt, sgUppercaseAlphabet,
	                         MSBaudotStandardUs, false);
}

static void test_baudot_detection_from_file_no_detection_base_2(const std::string &marielleLocalIp,
                                                                const std::string &marielleRemoteIp,
                                                                int marielleLocalRtpPort,
                                                                int marielleRemoteRtpPort,
                                                                int marielleLocalRtcpPort,
                                                                int marielleRemoteRtcpPort,
                                                                const std::string &margauxLocalIp,
                                                                const std::string &margauxRemoteIp,
                                                                int margauxLocalRtpPort,
                                                                int margauxRemoteRtpPort,
                                                                int margauxLocalRtcpPort,
                                                                int margauxRemoteRtcpPort) {
	int lossPercentage = 0;
	int dummy = 0;

	AudioStream *marielle =
	    audio_stream_new2(sgFactory, marielleLocalIp.c_str(), marielleLocalRtpPort, marielleLocalRtcpPort);
	BaudotTestStats marielleStats;
	AudioStream *margaux =
	    audio_stream_new2(sgFactory, margauxLocalIp.c_str(), margauxLocalRtpPort, margauxLocalRtcpPort);
	BaudotTestStats margauxStats;

	char *inputAudioFile = bc_tester_res(HELLO_8K_FILE);
	char *recordedFileName = ms_tester_get_random_filename(RECORDED_8K_FILE, ".wav");
	char *recordedFile = bc_tester_file(recordedFileName);
	bctbx_free(recordedFileName);

	rtp_session_set_multicast_loopback(marielle->ms.sessions.rtp_session, TRUE);
	rtp_session_set_multicast_loopback(margaux->ms.sessions.rtp_session, TRUE);
	rtp_session_set_rtcp_report_interval(marielle->ms.sessions.rtp_session, 1000);
	rtp_session_set_rtcp_report_interval(margaux->ms.sessions.rtp_session, 1000);

	marielleStats.reset();
	margauxStats.reset();

	RtpProfile *profile = rtp_profile_new("default profile");
	rtp_profile_set_payload(profile, 0, &payload_type_pcmu8000);

	BC_ASSERT_EQUAL(audio_stream_start_full(
	                    margaux, profile,
	                    ms_is_multicast(margauxLocalIp.c_str()) ? margauxLocalIp.c_str() : margauxRemoteIp.c_str(),
	                    ms_is_multicast(margauxLocalIp.c_str()) ? margauxLocalRtpPort : margauxRemoteRtpPort,
	                    margauxRemoteIp.c_str(), margauxRemoteRtcpPort, 0, 50, NULL, recordedFile, NULL, NULL, 0),
	                0, int, "%d");
	ms_filter_add_notify_callback(margaux->baudot_detector, test_baudot_detector_notify_cb, &margauxStats, TRUE);

	BC_ASSERT_EQUAL(audio_stream_start_full(marielle, profile, marielleRemoteIp.c_str(), marielleRemoteRtpPort,
	                                        marielleRemoteIp.c_str(), marielleRemoteRtcpPort, 0, 50, inputAudioFile,
	                                        NULL, NULL, NULL, 0),
	                0, int, "%d");

	ms_filter_add_notify_callback(marielle->soundread, test_baudot_soundread_notify_cb, &marielleStats, TRUE);

	wait_for_until(&marielle->ms, &margaux->ms, &marielleStats.mNbEndOfFile, 1, 12000);
	audio_stream_play(marielle, NULL);
	BC_ASSERT_EQUAL(marielleStats.mNbEndOfFile, 1, int, "%d");

	if (rtp_session_rtcp_enabled(marielle->ms.sessions.rtp_session) &&
	    rtp_session_rtcp_enabled(margaux->ms.sessions.rtp_session)) {
		BC_ASSERT_GREATER_STRICT(rtp_session_get_round_trip_propagation(marielle->ms.sessions.rtp_session), 0, float,
		                         "%f");
		BC_ASSERT_GREATER_STRICT(rtp_session_get_stats(marielle->ms.sessions.rtp_session)->recv_rtcp_packets, 0,
		                         unsigned long long, "%llu");
	}

	audio_stream_get_local_rtp_stats(marielle, &marielleStats.mRtpStats);
	audio_stream_stop(marielle);
	wait_for_until(&margaux->ms, NULL, &dummy, 1, 100);

	audio_stream_get_local_rtp_stats(margaux, &margauxStats.mRtpStats);
	audio_stream_stop(margaux);
	BC_ASSERT_TRUE(margauxStats.mRtpStats.hw_recv >= ((marielleStats.mRtpStats.sent * (100 - lossPercentage)) / 100));

	margauxStats.checkNoDetection();

	// Check that the Baudot detection did not alter the received audio.
	const MSAudioDiffParams audio_cmp_params = {10, 200};
	double similar = 0.0;
	const double threshold = 0.85;
	BC_ASSERT_EQUAL(ms_audio_diff(inputAudioFile, recordedFile, &similar, &audio_cmp_params, NULL, NULL), 0, int, "%d");
	BC_ASSERT_GREATER(similar, threshold, double, "%f");
	BC_ASSERT_LOWER(similar, 1.0, double, "%f");

	rtp_profile_destroy(profile);
	unlink(recordedFile);
	free(recordedFile);
	if (inputAudioFile) {
		free(inputAudioFile);
	}
}

static void test_baudot_detection_from_file_no_detection_base(const std::string &marielleLocalIp,
                                                              int marielleLocalRtpPort,
                                                              int marielleLocalRtcpPort,
                                                              const std::string &margauxLocalIp,
                                                              int margauxLocalRtpPort,
                                                              int margauxLocalRtcpPort) {
	test_baudot_detection_from_file_no_detection_base_2(
	    marielleLocalIp, margauxLocalIp, marielleLocalRtpPort, margauxLocalRtpPort, marielleLocalRtcpPort,
	    margauxLocalRtcpPort, margauxLocalIp, marielleLocalIp, margauxLocalRtpPort, marielleLocalRtpPort,
	    margauxLocalRtcpPort, marielleLocalRtcpPort);
}

static void test_baudot_detection_from_file_no_detection() {
	test_baudot_detection_from_file_no_detection_base(MARIELLE_IP, MARIELLE_RTP_PORT, MARIELLE_RTCP_PORT, MARGAUX_IP,
	                                                  MARGAUX_RTP_PORT, MARGAUX_RTCP_PORT);
}

static test_t tests[] = {
    TEST_NO_TAG("Baudot text encoding - Uppercase alphabet", test_baudot_text_encoding_uppercase_alphabet),
    TEST_NO_TAG("Baudot text encoding - Lowercase alphabet", test_baudot_text_encoding_lowercase_alphabet),
    TEST_NO_TAG("Baudot text encoding - Digits", test_baudot_text_encoding_digits),
    TEST_NO_TAG("Baudot text encoding - Special chars US", test_baudot_text_encoding_special_chars_us),
    TEST_NO_TAG("Baudot text encoding - Special chars Europe", test_baudot_text_encoding_special_chars_europe),
    TEST_NO_TAG("Baudot text encoding - Lorem Ipsum", test_baudot_text_encoding_lorem_ipsum),
    TEST_NO_TAG("Baudot text encoding - Letter by letter", test_baudot_text_encoding_letter_by_letter),
    TEST_NO_TAG("Baudot audio stream - Uppercase alphabet US", test_baudot_audio_stream_uppercase_alphabet_us),
    TEST_NO_TAG("Baudot audio stream - Uppercase alphabet Europe", test_baudot_audio_stream_uppercase_alphabet_europe),
    TEST_NO_TAG("Baudot audio stream - Lowercase alphabet US", test_baudot_audio_stream_lowercase_alphabet_us),
    TEST_NO_TAG("Baudot audio stream - Lowercase alphabet Europe", test_baudot_audio_stream_lowercase_alphabet_europe),
    TEST_NO_TAG("Baudot audio stream - Digits US", test_baudot_audio_stream_digits_us),
    TEST_NO_TAG("Baudot audio stream - Digits Europe", test_baudot_audio_stream_digits_europe),
    TEST_NO_TAG("Baudot audio stream - Special chars US", test_baudot_audio_stream_special_chars_us),
    TEST_NO_TAG("Baudot audio stream - Special chars Europe", test_baudot_audio_stream_special_chars_europe),
    TEST_NO_TAG("Baudot audio stream - Character by character uppercase alphabet US",
                test_baudot_audio_stream_character_by_character_uppercase_alphabet_us),
    TEST_NO_TAG("Baudot audio stream - Character by character uppercase alphabet Europe",
                test_baudot_audio_stream_character_by_character_uppercase_alphabet_europe),
    TEST_NO_TAG("Baudot audio stream - Character by character lowercase alphabet US",
                test_baudot_audio_stream_character_by_character_lowercase_alphabet_us),
    TEST_NO_TAG("Baudot audio stream - Character by character lowercase alphabet Europe",
                test_baudot_audio_stream_character_by_character_lowercase_alphabet_europe),
    TEST_NO_TAG("Baudot audio stream - Character by character digits US",
                test_baudot_audio_stream_character_by_character_digits_us),
    TEST_NO_TAG("Baudot audio stream - Character by character digits Europe",
                test_baudot_audio_stream_character_by_character_digits_europe),
    TEST_NO_TAG("Baudot audio stream - Character by character special chars US",
                test_baudot_audio_stream_character_by_character_special_chars_us),
    TEST_NO_TAG("Baudot audio stream - Character by character special chars Europe",
                test_baudot_audio_stream_character_by_character_special_chars_europe),
    TEST_NO_TAG("Baudot detection from file - Alphabet US", test_baudot_detection_from_file_alphabet_us),
    TEST_NO_TAG("Baudot detection from file - Digits US", test_baudot_detection_from_file_digits_us),
    TEST_NO_TAG("Baudot detection from file - Letter by letter US",
                test_baudot_detection_from_file_letter_by_letter_us),
    TEST_NO_TAG("Baudot detection from file - Stereo Alphabet US", test_baudot_detection_from_file_stereo_alphabet_us),
    TEST_NO_TAG("Baudot detection from file - No detection", test_baudot_detection_from_file_no_detection),
};

extern "C" {
test_suite_t baudot_test_suite = {
    "Baudot", initMSFactory, releaseMSFactory, nullptr, nullptr, sizeof(tests) / sizeof(test_t), tests, 0};
}

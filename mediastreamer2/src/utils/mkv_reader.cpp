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

#include <stdint.h>
#include <algorithm>
#include <array>
#include <cwchar>
#include <list>
#include <memory>
#include <vector>


#define bool_t mkv_bool_t
extern "C" {
#include <matroska/matroska.h>
#include <matroska/matroska_sem.h>
}
#undef bool_t

#undef min
#undef max

#include "mkv_reader.h"

#define EBML_MasterForEachChild(it, e, c) for(it=EBML_MasterFindChild(e, c); it; it=EBML_MasterNextChild(e, it))

extern "C" {
extern const nodemeta LangStr_Class[];
extern const nodemeta UrlPart_Class[];
extern const nodemeta BufStream_Class[];
extern const nodemeta MemStream_Class[];
extern const nodemeta Streams_Class[];
extern const nodemeta File_Class[];
extern const nodemeta Stdio_Class[];
extern const nodemeta Matroska_Class[];
extern const nodemeta EBMLElement_Class[];
extern const nodemeta EBMLMaster_Class[];
extern const nodemeta EBMLBinary_Class[];
extern const nodemeta EBMLString_Class[];
extern const nodemeta EBMLInteger_Class[];
extern const nodemeta EBMLCRC_Class[];
extern const nodemeta EBMLDate_Class[];
extern const nodemeta EBMLVoid_Class[];
}

using namespace std;

template <typename MkvStrT>
static std::string mkvStringToCppString(const MkvStrT &mkvString) {
	std::string strOut{};
#ifdef UNICODE
	strOut.resize(wcslen(mkvString.data()) * 2);
#ifdef _WIN32
	WideCharToMultiByte(CP_UTF8, 0, mkvString.data(), -1, &strOut[0], (int)mkvString.size(), NULL, NULL);
#else
	wcstombs(&strOut[0], mkvString.data(), strOut.size());
#endif
	strOut.resize(strlen(strOut.c_str()));
#else
	strOut.resize(strlen(mkvString.data()));
	strncpy(&strOut[0], mkvString.data(), strOut.size());
#endif
	return strOut;
}

static std::vector<tchar_t> cppStringToMkvString(const std::string &str) {
	std::vector<tchar_t> mkvString{};
#ifdef UNICODE
	mkvString.resize(str.size() + 1);
#ifdef _WIN32
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, mkvString.data(), int(mkvString.size()-1));
#else
	mbstowcs(mkvString.data(), str.c_str(), mkvString.size()-1);
	mkvString.resize(wcslen(mkvString.data()));
#endif
#else
	mkvString.reserve(str.end() - str.begin() + 1);
	mkvString.assign(str.begin(), str.end());
	mkvString.push_back('\0');
#endif
	return mkvString;
}

MKVParserCtx::MKVParserCtx() {
	try {
		ParserContext_Init(&mParserCtx, nullptr, nullptr, nullptr);
		loadModules();
		auto err = MATROSKA_Init((nodecontext *)&mParserCtx);
		if(err != ERR_NONE) {
			throw runtime_error("Parser opening failed. Could not initialize Matroska parser. err=" + to_string(err));
		}
	} catch (...) {
		ParserContext_Done(&mParserCtx);
		throw;
	}
}

MKVParserCtx::~MKVParserCtx() noexcept {
	MATROSKA_Done(reinterpret_cast<nodecontext *>(&mParserCtx));
	ParserContext_Done(&mParserCtx);
}

void MKVParserCtx::loadModules() noexcept {
	auto parser = reinterpret_cast<nodemodule *>(&mParserCtx);
	NodeRegisterClassEx(parser, Streams_Class);
	NodeRegisterClassEx(parser, File_Class);
	NodeRegisterClassEx(parser, Matroska_Class);
	NodeRegisterClassEx(parser, EBMLElement_Class);
	NodeRegisterClassEx(parser, EBMLMaster_Class);
	NodeRegisterClassEx(parser, EBMLBinary_Class);
	NodeRegisterClassEx(parser, EBMLString_Class);
	NodeRegisterClassEx(parser, EBMLInteger_Class);
	NodeRegisterClassEx(parser, EBMLCRC_Class);
	NodeRegisterClassEx(parser, EBMLDate_Class);
	NodeRegisterClassEx(parser, EBMLVoid_Class);
}

int MKVSegmentInfo::parse(const ebml_element *seg_info_elt) noexcept {
	std::array<tchar_t, 256> tmp{};

	if(!EBML_MasterCheckMandatory((ebml_master *)seg_info_elt, FALSE)) {
		ms_error("MKVParser: fail to parse segment info. Missing elements");
		return -1;
	}
	mDuration = EBML_FloatValue((ebml_float *)EBML_MasterFindChild((ebml_master *)seg_info_elt, &MATROSKA_ContextDuration));
	mTimecodeScale = EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild((ebml_master *)seg_info_elt, &MATROSKA_ContextTimecodeScale));

	EBML_StringGet((ebml_string *)EBML_MasterFindChild((ebml_master *)seg_info_elt, &MATROSKA_ContextMuxingApp), tmp.data(), tmp.size());
	mMuxingApp = mkvStringToCppString(tmp);

	EBML_StringGet((ebml_string *)EBML_MasterFindChild((ebml_master *)seg_info_elt, &MATROSKA_ContextWritingApp), tmp.data(), tmp.size());
	mWritingApp = mkvStringToCppString(tmp);

	return 0;
}

std::unique_ptr<MKVTrack> MKVTrack::parseTrack(const ebml_element *track_elt) noexcept {
	unique_ptr<MKVTrack> track{};
	auto track_type = EBML_IntegerValue((ebml_integer *)EBML_MasterFindChild(track_elt, &MATROSKA_ContextTrackType));
	switch (track_type) {
		case TRACK_TYPE_VIDEO: {
			auto vtrack = make_unique<MKVVideoTrack>();
			vtrack->parse(track_elt);
			track = move(vtrack);
			break;
		}
		case TRACK_TYPE_AUDIO: {
			auto atrack = make_unique<MKVAudioTrack>();
			atrack->parse(track_elt);
			track = move(atrack);
			break;
		}
		default: break;
	}
	return track;
}

void MKVTrack::parse(const ebml_element *track_elt) noexcept {
	mNum = (uint8_t)EBML_IntegerValue((ebml_integer *)EBML_MasterFindChild(track_elt, &MATROSKA_ContextTrackNumber));
	mUID = EBML_IntegerValue((ebml_integer *)EBML_MasterFindChild(track_elt, &MATROSKA_ContextTrackUID));
	mType = (uint8_t)EBML_IntegerValue((ebml_integer *)EBML_MasterFindChild(track_elt, &MATROSKA_ContextTrackType));
	mEnabled = EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild((ebml_master *)track_elt, &MATROSKA_ContextFlagEnabled)) == 0 ? FALSE : TRUE;
	mDef = EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild((ebml_master *)track_elt, &MATROSKA_ContextFlagDefault)) == 0 ? FALSE : TRUE;
	mForced = EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild((ebml_master *)track_elt, &MATROSKA_ContextFlagForced)) == 0 ? FALSE : TRUE;
	mLacing = EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild((ebml_master *)track_elt, &MATROSKA_ContextFlagLacing)) == 0 ? FALSE : TRUE;
	mMinCache = (int)EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild((ebml_master *)track_elt, &MATROSKA_ContextMinCache));
	mMaxBlockAdditionId = (int)EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild((ebml_master *)track_elt, &MATROSKA_ContextMaxBlockAdditionID));

	std::array<tchar_t, 256> codec_id{};
	EBML_StringGet((ebml_string *)EBML_MasterFindChild(track_elt, &MATROSKA_ContextCodecID), codec_id.data(), codec_id.size());
	mCodecId = mkvStringToCppString(codec_id);

	auto codec_private_elt = EBML_MasterFindChild(track_elt, &MATROSKA_ContextCodecPrivate);
	if(codec_private_elt) {
		auto data_size = EBML_ElementDataSize(codec_private_elt, FALSE);
		auto data = EBML_BinaryGetData((ebml_binary *)codec_private_elt);
		mCodecPrivate.assign(data, data + data_size);
	}
	mSeekPreroll = (int)EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild((ebml_master *)track_elt, &MATROSKA_ContextSeekPreRoll));
}

void MKVVideoTrack::parse(const ebml_element *track_elt) noexcept {
	auto video_info_elt = EBML_MasterFindChild(track_elt, &MATROSKA_ContextVideo);
	if(video_info_elt == nullptr) return;

	MKVTrack::parse(track_elt);

	mInterlaced = EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild((ebml_master *)video_info_elt, &MATROSKA_ContextFlagInterlaced)) == 0 ? FALSE : TRUE;
	mWidth = (int)EBML_IntegerValue((ebml_integer *)EBML_MasterFindChild(video_info_elt, &MATROSKA_ContextPixelWidth));
	mHeight = (int)EBML_IntegerValue((ebml_integer *)EBML_MasterFindChild(video_info_elt, &MATROSKA_ContextPixelHeight));
	auto elt = EBML_MasterFindChild(video_info_elt, &MATROSKA_ContextFrameRate);
	mFrameRate = elt ? EBML_FloatValue((ebml_float *)elt) : 0.0;
}

void MKVAudioTrack::parse(const ebml_element *track_elt) noexcept {
	auto audio_info_elt = EBML_MasterFindChild(track_elt, &MATROSKA_ContextAudio);
	if(audio_info_elt==NULL) return;

	MKVTrack::parse(track_elt);

	mSamplingFreq = (int)EBML_FloatValue((ebml_float *)EBML_MasterGetChild((ebml_master *)audio_info_elt, &MATROSKA_ContextSamplingFrequency));
	mChannels = (uint8_t)EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild((ebml_master *)audio_info_elt, &MATROSKA_ContextChannels));
}

void MKVReader::open(const std::string &filename) {
	try {
		mParserCtx = make_unique<MKVParserCtx>();

		auto fname = cppStringToMkvString(filename);
		mFile.reset(StreamOpen(mParserCtx.get(), fname.data(), SFLAG_RDONLY));
		if(mFile == NULL) {
			throw runtime_error("Parser opening failed. Could not open " + filename);
		}
		if(parseHeaders() < 0) {
			throw runtime_error("MKVParser: error while parsing EBML header");
		}
	} catch (...) {
		close();
		throw;
	}
}

void MKVReader::close() noexcept {
	mFile.reset();
	mInfoElt.reset();
	mTracksElt.clear();
	mCues.reset();
	mInfo.reset();
	mTracks.clear();
	mFirstClusterPos = 0;
	mLastClusterEnd = 0;
	mFirstLevel1Pos = 0;
	mReaders.clear();
	mParserCtx.reset();
}

const MKVTrack *MKVReader::getDefaultTrack(int track_type) const noexcept {
	auto it = find_if(mTracks.cbegin(), mTracks.cend(),
		[track_type](const auto &track) {return track->mType == track_type && track->mDef;}
	);
	return it != mTracks.cend() ? it->get() : nullptr;
}

const MKVTrack *MKVReader::getFirstTrack(int track_type) const noexcept {
	auto it = find_if(mTracks.cbegin(), mTracks.cend(),
		[track_type](const auto &track) {return track->mType == track_type;}
	);
	return it != mTracks.cend() ? it->get() : nullptr;
}

MKVTrackReader *MKVReader::getTrackReader(int track_num) noexcept {
	int upper_levels = 0;
	int i = -1;

	auto it = find_if(mTracks.cbegin(), mTracks.cend(),
		[track_num, &i](const auto &track){
			++i;
			return track->mNum == track_num;
		}
	);
	if(it == mTracks.cend()) return nullptr;

	const auto &trackElt = mTracksElt.at(i);

	auto track_reader = new MKVTrackReader{};
	track_reader->mRoot = this;
	track_reader->mTrackNum = track_num;
	track_reader->mTrackElt = trackElt.get();
	track_reader->mFile.reset(Stream_Duplicate(mFile.get(), SFLAG_RDONLY));
	track_reader->mParser.Context = &MATROSKA_ContextSegment;
	track_reader->mParser.EndPosition = mLastClusterEnd;
	track_reader->mParser.UpContext = NULL;
	Stream_Seek(track_reader->mFile.get(), mFirstClusterPos, SEEK_SET);
	track_reader->mCurrentCluster.reset(EBML_FindNextElement(track_reader->mFile.get(), &track_reader->mParser, &upper_levels, FALSE));
	EBML_ElementReadData(track_reader->mCurrentCluster.get(), track_reader->mFile.get(), &track_reader->mParser, FALSE, SCOPE_PARTIAL_DATA, FALSE);
	mReaders.emplace_back(track_reader);
	return track_reader;
}

int MKVReader::seek(int pos_ms) noexcept {
	ebml_element *cue_point = nullptr;

	if (mCues) {
		EBML_MasterForEachChild(cue_point, mCues.get(), &MATROSKA_ContextCuePoint) {
			MATROSKA_LinkCueSegmentInfo((matroska_cuepoint *)cue_point, (ebml_master *)mInfoElt.get());
			if(MATROSKA_CueTimecode((matroska_cuepoint *)cue_point) >= (timecode_t)pos_ms * 1000000LL) break;
		}
	}

	if(cue_point) {
		filepos_t pos = INVALID_FILEPOS_T;
		ebml_element *track_position;
		for (const auto &r : mReaders) {r->mNeedSeeking = true;}
		for(track_position=EBML_MasterFindChild((ebml_master *)cue_point, &MATROSKA_ContextCueTrackPositions);
			track_position!=NULL;
			track_position = EBML_MasterFindNextElt((ebml_master *)cue_point, track_position, FALSE, FALSE)) {
			int track_num = (int)EBML_IntegerValue((ebml_integer *)EBML_MasterFindChild((ebml_master *)track_position, &MATROSKA_ContextCueTrack));
			auto it = find_if(mReaders.cbegin(), mReaders.cend(),
				[track_num](const unique_ptr<MKVTrackReader> &r){return r->mTrackNum == track_num;}
			);
			if(it != mReaders.cend()) {
				const auto &t_reader = *it;
				pos = EBML_IntegerValue((ebml_integer *)EBML_MasterFindChild((ebml_master *)track_position, &MATROSKA_ContextCueClusterPosition));
				pos += mFirstLevel1Pos;
				t_reader->seek(pos);
			}
		}
		for(const auto &t_reader : mReaders) {
			if(t_reader->mNeedSeeking) t_reader->seek(pos);
		}
		return (int)(MATROSKA_CueTimecode((matroska_cuepoint *)cue_point) / 1000000LL);
	} else {
		constexpr auto invalidTimePos = numeric_limits<int>::max();
		auto time_pos = invalidTimePos;
		auto pos = findClusterPosition(pos_ms);
		for (const auto &r : mReaders) {
			time_pos = min(r->seek(pos, pos_ms), time_pos);
		}
		return time_pos != invalidTimePos ? time_pos : -1;
	}
}

int MKVReader::parseHeaders() noexcept {
	EbmlElementPtr level0{}, level1{};
	ebml_parser_context pctx, seg_pctx;
	tchar_t doc_type[9];
	int doc_type_version;
	err_t err;
	int upper_level = 0;
	bool_t cluster_found = FALSE;
	bool_t level1_found = FALSE;
	const tchar_t *matroska_doc_type =
#ifdef UNICODE
		L"matroska";
#else
		"matroska";
#endif

	pctx.Context = &MATROSKA_ContextStream;
	pctx.EndPosition = INVALID_FILEPOS_T;
	pctx.Profile = 0;
	pctx.UpContext = NULL;

	Stream_Seek(mFile.get(), 0, SEEK_SET);
	level0.reset(EBML_FindNextElement(mFile.get(), &pctx, &upper_level, FALSE));
	if(level0 == NULL) {
		ms_error("MKVParser: file is empty");
		return -1;
	}
	if(!EBML_ElementIsType(level0.get(), &EBML_ContextHead)) {
		ms_error("MKVParser: first element is not an EBML header");
		return -1;
	}
	err = EBML_ElementReadData(level0.get(), mFile.get(), &pctx, FALSE, SCOPE_ALL_DATA, FALSE);
	if(err != ERR_NONE) {
		ms_error("MKVParser: could not parse EBML header. err=%d", (int)err);
		return -1;
	}
	if(!EBML_MasterCheckMandatory((ebml_master *)level0.get(), FALSE)) {
		ms_error("MKVParser: missing elements in the EBML header");
		return -1;
	}
	EBML_StringGet((ebml_string *)EBML_MasterGetChild((ebml_master *)level0.get(), &EBML_ContextDocType), doc_type, sizeof(doc_type));
	err = tcscmp(doc_type, matroska_doc_type);
	if(err != 0) {
		ms_error("MKVParser: not a matroska file");
		return -1;
	}
	doc_type_version = (int)EBML_IntegerValue((ebml_integer *)EBML_MasterGetChild((ebml_master *)level0.get(), &EBML_ContextDocTypeVersion));

	level0.reset(EBML_FindNextElement(mFile.get(), &pctx, &upper_level, FALSE));
	if(level0 == NULL) {
		ms_error("MKVParser: no matroska element");
		return -1;
	}
	if(!EBML_ElementIsType(level0.get(), &MATROSKA_ContextSegment)) {
		ms_error("MKVParser: second element is not a segment");
		return -1;
	}
	seg_pctx.Context = &MATROSKA_ContextSegment;
	seg_pctx.EndPosition = EBML_ElementPositionEnd(level0.get());
	seg_pctx.Profile = doc_type_version;
	seg_pctx.UpContext = &pctx;
	upper_level = 0;
	while((level1.reset(EBML_FindNextElement(mFile.get(), &seg_pctx, &upper_level, FALSE)), level1)
			&& upper_level == 0) {
		if(!level1_found) {
			mFirstLevel1Pos = EBML_ElementPosition(level1.get());
			level1_found = TRUE;
		}
		if(EBML_ElementIsType(level1.get(), &MATROSKA_ContextInfo)) {
			err = EBML_ElementReadData(level1.get(), mFile.get(), &seg_pctx, FALSE, SCOPE_ALL_DATA, FALSE);
			if(err != ERR_NONE) {
				ms_error("MKVParser: fail to read segment information");
				return -1;
			}
			mInfo = make_unique<MKVSegmentInfo>();
			if(mInfo->parse(level1.get()) < 0) {
				ms_error("MKVParser: fail to parse segment information");
				return -1;
			}
			mInfoElt = move(level1);
		} else if(EBML_ElementIsType(level1.get(), &MATROSKA_ContextTracks)) {
			ebml_element *track;
			err = EBML_ElementReadData(level1.get(), mFile.get(), &seg_pctx, FALSE, SCOPE_ALL_DATA, FALSE);
			if(err != ERR_NONE) {
				ms_error("MKVParser: fail to read tracks information");
				return -1;
			}
			if(parseTracks(level1.get()) < 0) {
				ms_error("MKVParser: fail to parse tracks information");
				return -1;
			}
			EBML_MasterForEachChild(track, level1.get(), &MATROSKA_ContextTrackEntry) {
				mTracksElt.emplace_back(EBML_ElementCopy(track, NULL));
			}
		} else if(EBML_ElementIsType(level1.get(), &MATROSKA_ContextCluster)) {
			if(!cluster_found) {
				mFirstClusterPos = EBML_ElementPosition(level1.get());
				cluster_found = TRUE;
			}
			mLastClusterEnd = EBML_ElementPositionEnd(level1.get());
			EBML_ElementSkipData(level1.get(), mFile.get(), &seg_pctx, NULL, FALSE);
		} else if(EBML_ElementIsType(level1.get(), &MATROSKA_ContextCues)) {
			err = EBML_ElementReadData(level1.get(), mFile.get(), &seg_pctx, FALSE, SCOPE_ALL_DATA, FALSE);
			if(err != ERR_NONE) {
				ms_error("MKVParser: fail to read the table of cues");
			} else if(!EBML_MasterCheckMandatory((ebml_master *)level1.get(), FALSE)) {
				ms_error("MKVParser: fail to parse the table of cues");
			} else {
				mCues = move(level1);
			}
		} else {
			EBML_ElementSkipData(level1.get(), mFile.get(), &seg_pctx, NULL, FALSE);
		}
	}
	if(!cluster_found) return -1;
	return 0;
}

int MKVReader::parseTracks(const ebml_element *tracks_elt) noexcept {
	ebml_element *elt;

	mTracks.clear();

	if(!EBML_MasterCheckMandatory((ebml_master *)tracks_elt, FALSE)) {
		ms_error("MKVParser: fail to parse tracks info. Missing elements");
		return -1;
	}
	EBML_MasterForEachChild(elt, tracks_elt, &MATROSKA_ContextTrackEntry) {
		mTracks.emplace_back(MKVTrack::parseTrack(elt));
	}
	return 0;
}

filepos_t MKVReader::findClusterPosition(int pos_ms) noexcept {
	auto &f = mFile;

	Stream_Seek(f.get(), mFirstClusterPos, SEEK_SET);

	int upper_level = 0;
	const ebml_parser_context seg_parser_ctx = {&MATROSKA_ContextSegment, nullptr, mLastClusterEnd, 0};
	EbmlElementPtr cluster{}, prev_cluster{};
	while (cluster.reset(EBML_FindNextElement(f.get(), &seg_parser_ctx, &upper_level, FALSE)), cluster) {
		if (!EBML_ElementIsType(cluster.get(), &MATROSKA_ContextCluster)) {
			EBML_ElementSkipData(cluster.get(), f.get(), &seg_parser_ctx, nullptr, false);
			continue;
		}
		int upper_level2 = 0;
		ebml_parser_context cluster_parser_context = {&MATROSKA_ContextCluster, nullptr, EBML_ElementPositionEnd(cluster.get()), 0};
		EbmlElementPtr timecode_elt{};
		while ((timecode_elt.reset(EBML_FindNextElement(f.get(), &cluster_parser_context, &upper_level2, FALSE)), timecode_elt)
			&& !EBML_ElementIsType(timecode_elt.get(), &MATROSKA_ContextTimecode));
		if (timecode_elt == nullptr) return INVALID_FILEPOS_T;

		EBML_ElementReadData(timecode_elt.get(), f.get(), &cluster_parser_context, FALSE, SCOPE_ALL_DATA, 0);
		auto timecode = EBML_IntegerValue(reinterpret_cast<ebml_integer *>(timecode_elt.get()));
		auto timestamp = timecode * mInfo->mTimecodeScale / 1000000;
		if (timestamp >= pos_ms) break;

		EBML_ElementSkipData(cluster.get(), f.get(), &seg_parser_ctx, nullptr, false);
		prev_cluster = std::move(cluster);
	}
	if (!prev_cluster && !cluster) return INVALID_FILEPOS_T;
	if (prev_cluster) cluster = std::move(prev_cluster);

	return EBML_ElementPosition(cluster.get());
}

void MKVTrackReader::nextBlock(std::unique_ptr<MKVBlock> &block, bool &end_of_track) noexcept {
	matroska_block *block_elt = NULL;
	matroska_frame m_frame;
	int upper_levels = 0;

	block = nullptr;
	end_of_track = false;

	do {
		do {
			do {
				if(mCurrentFrameElt==NULL) {
					mCurrentFrameElt = EBML_MasterChildren(mCurrentCluster.get());
				} else {
					mCurrentFrameElt = EBML_MasterNext(mCurrentFrameElt);
				}
			} while(mCurrentFrameElt
					&& !EBML_ElementIsType(mCurrentFrameElt, &MATROSKA_ContextSimpleBlock)
					&& !EBML_ElementIsType(mCurrentFrameElt, &MATROSKA_ContextBlockGroup));

			if(mCurrentFrameElt==NULL) {
				Stream_Seek(mFile.get(), EBML_ElementPositionEnd(mCurrentCluster.get()), SEEK_SET);
				mCurrentCluster.reset(EBML_FindNextElement(mFile.get(), &mParser, &upper_levels, FALSE));
				if(mCurrentCluster) {
					EBML_ElementReadData(mCurrentCluster.get(), mFile.get(), &mParser, FALSE, SCOPE_PARTIAL_DATA, FALSE);
				}
			}
		} while(mCurrentCluster && mCurrentFrameElt == NULL);

		if(mCurrentCluster == NULL) {
			end_of_track = true;
			return;
		}
		block_elt = frameToBlock(mCurrentFrameElt);
	} while(block_elt->TrackNumber != mTrackNum);

	MATROSKA_LinkBlockReadSegmentInfo(block_elt, (ebml_master *)mRoot->mInfoElt.get(), TRUE);
	MATROSKA_LinkBlockReadTrack(block_elt, (ebml_master *)mTrackElt, TRUE);
	MATROSKA_BlockReadData(block_elt, mFile.get());
	block = make_unique<MKVBlock>();
	if(EBML_ElementIsType(mCurrentFrameElt, &MATROSKA_ContextBlockGroup)) {
		ebml_element *codec_state_elt = EBML_MasterFindChild(mCurrentFrameElt, &MATROSKA_ContextCodecState);
		if(codec_state_elt) {
			auto codec_state_size = EBML_ElementDataSize(codec_state_elt, FALSE);
			auto codec_state_data = EBML_BinaryGetData((ebml_binary *)codec_state_elt);
			block->mCodecState.assign(codec_state_data, codec_state_data + codec_state_size);
		}
	}
	block->mKeyframe = (bool_t)MATROSKA_BlockKeyframe(block_elt);
	block->mTrackNum = (uint8_t)MATROSKA_BlockTrackNum(block_elt);
	MATROSKA_BlockGetFrame(block_elt, 0, &m_frame, TRUE);
	block->mTimestamp = (uint32_t)(MATROSKA_BlockTimecode(block_elt) / 1000000LL);
	block->mData.assign(m_frame.Data, m_frame.Data + m_frame.Size);
	MATROSKA_BlockReleaseData(block_elt, TRUE);
}

void MKVTrackReader::reset() noexcept {
	int upper_levels = 0;
	Stream_Seek(mFile.get(), mRoot->mFirstClusterPos, SEEK_SET);
	mCurrentCluster.reset(EBML_FindNextElement(mFile.get(), &mParser, &upper_levels, FALSE));
	EBML_ElementReadData(mCurrentCluster.get(), mFile.get(), &mParser, FALSE, SCOPE_PARTIAL_DATA, FALSE);
	mCurrentFrameElt = NULL;
}

void MKVTrackReader::seek(filepos_t clusterPos) noexcept {
	int upper_level = 0;
	mCurrentFrameElt = nullptr;
	Stream_Seek(mFile.get(), clusterPos, SEEK_SET);
	mCurrentCluster.reset(EBML_FindNextElement(mFile.get(), &mParser, &upper_level, false));
	EBML_ElementReadData(mCurrentCluster.get(), mFile.get(), &mParser, false, SCOPE_PARTIAL_DATA, 0);
	mNeedSeeking = false;
}

int MKVTrackReader::seek(filepos_t clusterPos, int pos_ms) noexcept {
	seek(clusterPos);

	ebml_element *prev_frame = nullptr, *frame = nullptr;
	for (frame = EBML_MasterChildren(mCurrentCluster.get()); frame; frame = EBML_MasterNext(frame)) {
		matroska_block *block = nullptr;
		if (EBML_ElementIsType(frame, &MATROSKA_ContextSimpleBlock)) {
			block = reinterpret_cast<matroska_block *>(frame);
		} else if (EBML_ElementIsType(frame, &MATROSKA_ContextBlockGroup)) {
			block = reinterpret_cast<matroska_block *>(EBML_MasterFindChild(frame, &MATROSKA_ContextBlock));
		} else continue;

		if (block == nullptr) continue;

		MATROSKA_LinkBlockReadSegmentInfo(block, (ebml_master *)mRoot->mInfoElt.get(), true);
		MATROSKA_LinkBlockReadTrack(block, (ebml_master *)mTrackElt, true);
		auto timestamp = MATROSKA_BlockTimecode(block) / 1000000;
		if (timestamp > pos_ms) break;

		prev_frame = frame;
	}
	if (prev_frame) mCurrentFrameElt = prev_frame;

	if (mCurrentFrameElt) {
		return int(MATROSKA_BlockTimecode(frameToBlock(mCurrentFrameElt)) / 1000000);
	} else {
		auto timecodeElt = EBML_MasterFindChild(mCurrentCluster.get(), &MATROSKA_ContextTimecode);
		return (timecodeElt)
			? int(EBML_IntegerValue((ebml_integer *)timecodeElt) * mRoot->mInfo->mTimecodeScale / 1000000)
			: -1;
	}
}

matroska_block *MKVTrackReader::frameToBlock(const ebml_element *frameElt) noexcept {
	const ebml_element *blockElt = nullptr;
	if (EBML_ElementIsType(frameElt, &MATROSKA_ContextSimpleBlock)) {
		blockElt = frameElt;
	} else if (EBML_ElementIsType(frameElt, &MATROSKA_ContextBlockGroup)) {
		blockElt = EBML_MasterFindChild(frameElt, &MATROSKA_ContextBlock);
	}
	return (matroska_block *)blockElt;
}

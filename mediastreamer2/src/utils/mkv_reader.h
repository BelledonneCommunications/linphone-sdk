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

#pragma once

#include <memory>
#include <list>
#include <vector>

#include "mediastreamer2/mscommon.h"

class MKVReader;

template <typename T>
struct NodeDeleter {
	void operator()(T *ptr) {NodeDelete(reinterpret_cast<node *>(ptr));}
};
using EbmlElementPtr = std::unique_ptr<ebml_element, NodeDeleter<ebml_element>>;

struct StreamCloser {
	void operator()(stream *ptr) {StreamClose(ptr);}
};
using StreamPtr = std::unique_ptr<stream, StreamCloser>;

class MKVParserCtx {
public:
	MKVParserCtx();
	~MKVParserCtx() noexcept;
	MKVParserCtx(const MKVParserCtx &) = delete;
	MKVParserCtx(MKVParserCtx &&) = delete;

	parsercontext *get() noexcept;

private:
	void loadModules() noexcept;

	parsercontext mParserCtx;
};

struct MKVSegmentInfo {
	int64_t mTimecodeScale{0};
	double mDuration{0.0};
	std::string mMuxingApp{};
	std::string mWritingApp{};

	int parse(const ebml_element *seg_info_elt) noexcept;
};

struct MKVBlock {
	uint8_t mTrackNum{0};
	uint32_t mTimestamp{0}; // ms
	bool mKeyframe{false};
	std::vector<uint8_t> mData{};
	std::vector<uint8_t>mCodecState{};
};

#define MKV_TRACK_TYPE_VIDEO    0x01
#define MKV_TRACK_TYPE_AUDIO    0x02
#define MKV_TRACK_TYPE_COMPLEX  0x03
#define MKV_TRACK_TYPE_LOGO     0x10
#define MKV_TRACK_TYPE_SUBTITLE 0x11
#define MKV_TRACK_TYPE_BUTTONS  0x12
#define MKV_TRACK_TYPE_CONTROL  0x20

struct MKVTrack {
	uint8_t mNum{0};
	uint64_t mUID{0};
	uint8_t mType{0};
	bool mEnabled{false};
	bool mDef{false};
	bool mForced{false};
	bool mLacing{false};
	int mMinCache{0};
	int mMaxBlockAdditionId{0};
	std::string mCodecId{};
	std::vector<uint8_t> mCodecPrivate{};
	int mSeekPreroll{0};

	virtual ~MKVTrack() = default;
	virtual void parse(const ebml_element *track_elt) noexcept;

	static std::unique_ptr<MKVTrack> parseTrack(const ebml_element *track_elt) noexcept;
};

struct MKVVideoTrack : public MKVTrack {
	bool mInterlaced{false};
	int mWidth{0};
	int mHeight{0};
	double mFrameRate{0.0};

	void parse(const ebml_element *track_elt) noexcept override;
};

struct MKVAudioTrack : public MKVTrack {
	int mSamplingFreq{0};
	uint8_t mChannels{0};

	void parse(const ebml_element *track_elt) noexcept override;
};

class MKVTrackReader;

class MKVReader{
public:
	MKVReader() = default;
	MKVReader(const std::string &filename) {open(filename);}
	MKVReader(const MKVReader &) = delete;
	MKVReader(MKVReader &&) = delete;
	~MKVReader() noexcept {close();}

	/**
	 * @brief Open a MKV file for reading
	 * @param filename Name of the file to open
	 * @return A pointer on a MKVReader. NULL if opening fails
	 */
	void open(const std::string &filename);
	/**
	 * @brief Close a MKV file.
	 * All associated track readers will be automatically destroyed
	 * @param obj MKVReader
	 */
	void close() noexcept;

	/**
     * @brief Get information about the Matroska segment
     * @return Matroska segment information
     */
	const MKVSegmentInfo *getSegmentInfo() const noexcept {return mInfo.get();}
	/**
	 * @brief Get the default track for a specified track type
	 * @param r MKVReader object
	 * @param track_type Type of the track
	 * @return A pointer on a track descriptor
	 */
	const MKVTrack *getDefaultTrack(int track_type) const noexcept;
	/**
	 * @brief Get the first track of the specified type
	 * @param r MKVReader object
	 * @param track_type Type of the track
	 * @return A track descriptor
	 */
	const MKVTrack *getFirstTrack(int track_type) const noexcept;

	/**
	 * @brief Create a track reader from its track number
	 * @param reader MKVReader
	 * @param track_num Track number
	 * @return A pointer on a track reader
	 */
	MKVTrackReader *getTrackReader(int track_num) noexcept;

	/**
	 * @brief Set the reading head of each assocated track reader at a specific position
	 * @param reader MKVReader
	 * @param pos_ms Position of the head in miliseconds
	 * @return The effective position of the head after the operation
	 */
	int seek(int pos_ms) noexcept;

private:
	void resetPrivateData() noexcept;
	int parseHeaders() noexcept;
	int parseTracks(const ebml_element *tracks_elt) noexcept;
	filepos_t findClusterPosition(int pos_ms) noexcept;

	std::unique_ptr<MKVParserCtx> mParserCtx{};
	StreamPtr mFile{};
	EbmlElementPtr mInfoElt{};
	std::vector<EbmlElementPtr> mTracksElt{};
	EbmlElementPtr mCues{};
	std::unique_ptr<MKVSegmentInfo> mInfo{};
	std::vector<std::unique_ptr<MKVTrack>> mTracks{};
	filepos_t mFirstClusterPos{0};
	filepos_t mLastClusterEnd{0};
	filepos_t mFirstLevel1Pos{0};
	std::list<std::unique_ptr<MKVTrackReader>> mReaders{};

	friend class MKVTrackReader;
};

class MKVTrackReader {
public:
	/**
	 * @brief Get the next block
	 * @param reader MKVReader
	 * @param block Block data
	 * @param end_of_track Return TRUE when the end of the track has been reached
	 */
	void nextBlock(std::unique_ptr<MKVBlock> &block, bool &end_of_track) noexcept;
	/**
	 * @brief Reset the track reader.
	 * The reading head is set at the start of the track
	 * @param reader MKVReader
	 */
	void reset() noexcept;

private:
	void seek(filepos_t clusterPos) noexcept;
	int seek(filepos_t clusterPos, int pos_ms) noexcept;

	static matroska_block *frameToBlock(const ebml_element *frameElt) noexcept;

	int mTrackNum{0};
	ebml_parser_context mParser{0};
	const ebml_element *mTrackElt{nullptr};
	EbmlElementPtr mCurrentCluster{};
	const ebml_element *mCurrentFrameElt{nullptr};
	StreamPtr mFile{};
	MKVReader *mRoot{nullptr};
	bool mNeedSeeking{false};

	friend class MKVReader;
};

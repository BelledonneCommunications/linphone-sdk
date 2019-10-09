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

#ifndef H264_UTILS_H
#define H264_UTILS_H

#include "mediastreamer2/msqueue.h"
#include "mediastreamer2/msvideo.h"

#ifdef __cplusplus
#include "h26x-utils.h"
#endif

typedef int MSH264NaluType;

const MSH264NaluType MSH264NaluTypeDataPartA = 2;
const MSH264NaluType MSH264NaluTypeDataPartB = 3;
const MSH264NaluType MSH264NaluTypeDataPartC = 4;
const MSH264NaluType MSH264NaluTypeIDR = 5;
const MSH264NaluType MSH264NaluTypeSEI = 6;
const MSH264NaluType MSH264NaluTypeSPS = 7;
const MSH264NaluType MSH264NaluTypePPS = 8;
const MSH264NaluType MSH264NaluTypeSTAPA = 24;
const MSH264NaluType MSH264NaluTypeFUA = 28;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Extract the NRI value from NALu header.
 */
uint8_t ms_h264_nalu_get_nri(const mblk_t *nalu);

/**
 * Gets the type of a NAL unit.
 * @param nalu The NAL unit to analyze.
 * @return The NALu type or #MSH264NaluTypeUnknown if the type of the NALu is unknown.
 */
MSH264NaluType ms_h264_nalu_get_type(const mblk_t *nalu);

/**
 * @brief Get the ID of a SPS NALu.
 * @warning If the passed NALu is not a SPS, the behavior is undefined.
 */
unsigned int ms_h264_sps_get_id(const mblk_t *sps);

/**
 * @brief Get the ID of a PPS NALu.
 * @warning If the passed NALu is not a PPS, the behavior is undefined.
 */
unsigned int ms_h264_pps_get_id(const mblk_t *pps);

/**
 * Slices a bitstream buffer into several nal units.
 *
 * Nal units are stored in freshly allocated mblk_t buffers which are pushed into
 * a queue. This function does not alter the buffer where the bitstream is contained in.
 * @param bitstream Pointer on a memory segment that contains the bitstream to slice.
 * @param size Size of the memory segment.
 * @param nalus A queue where produced nal units will be pushed into.
 */
void ms_h264_bitstream_to_nalus(const uint8_t *bitstream, size_t size, MSQueue *nalus);

/**
 * @brief Slices a frame into several nal units.
 *
 * Same as ms_h264_bitstream_to_nalus() except that the stream must be in size-prefixed
 * format i.e. each nalu in the stream must be prefixed by its size encoded on 4 bytes big-endian.
 *
 * @param frame Buffer containing the stream to slice
 * @param size Size of the buffer
 * @param nalus The queue where produced nal units will be pushed into
 * @param idr_count If not NULL, use the pointer to store the number of IDR nalus found in the stream.
 *
 * @deprecated Use mediastreamer::naluStreamToNalus() instead. Deprecated since 2018-05-21.
 */
void ms_h264_stream_to_nalus(const uint8_t *frame, size_t size, MSQueue *nalus, int *idr_count);

/**
 * @brief Extract video size from a SPS NALu.
 * @param sps A mblk_t holding the SPS.
 * @return The video size.
 */
MSVideoSize ms_h264_sps_get_video_size(const mblk_t* sps);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
namespace mediastreamer {

class H264NaluType: public H26xNaluType {
public:
	H264NaluType() = default;
	H264NaluType(uint8_t value);

	bool isVcl() const override {return _value < 6;}
	bool isParameterSet() const override {return *this == Sps || *this == Pps;}
	bool isKeyFramePart() const override {return *this == Idr || *this == DataPartA;}

	static const H264NaluType DataPartA;
	static const H264NaluType DataPartB;
	static const H264NaluType DataPartC;
	static const H264NaluType Idr;
	static const H264NaluType Sei;
	static const H264NaluType Sps;
	static const H264NaluType Pps;
	static const H264NaluType StapA;
	static const H264NaluType FuA;
};

class H264NaluHeader: public H26xNaluHeader {
public:
	H264NaluHeader(): H26xNaluHeader() {}
	H264NaluHeader(const uint8_t *header) {parse(header);}

	void setNri(uint8_t nri);
	uint8_t getNri() const {return _nri;}

	void setType(H264NaluType type) {_type = type;}
	H264NaluType getType() const {return _type;}

	const H26xNaluType &getAbsType() const override {return _type;}

	bool operator==(const H264NaluHeader &h2) const;
	bool operator!=(const H264NaluHeader &h2) const {return !(*this == h2);}

	void parse(const uint8_t *header) override;
	mblk_t *forge() const override;

	static const size_t length = 1;

private:
	uint8_t _nri = 0;
	H264NaluType _type;
};

class H264FrameAnalyser {
public:
	struct Info {
		bool hasSps = false;
		bool hasPps = false;
		bool hasIdr = false;
		bool newSps = false;
		bool newPps = false;

		unsigned int toUInt() const;
	};

	~H264FrameAnalyser();
	Info analyse(const MSQueue *frame);

private:
	bool updateParameterSet(const mblk_t *new_parameter_set);

	mblk_t *_lastSps = nullptr;
	mblk_t *_lastPps = nullptr;
};

class H264Tools {
public:
	static void nalHeaderInit(uint8_t *h, uint8_t nri, uint8_t type) {*h=((nri&0x3)<<5) | (type & ((1<<5)-1));}
	static mblk_t *prependFuIndicatorAndHeader(mblk_t *m, uint8_t indicator, bool_t start, bool_t end, uint8_t type);
};

class H264ParameterSetsInserter: public H26xParameterSetsInserter {
public:
	~H264ParameterSetsInserter();
	void process(MSQueue *in, MSQueue *out) override;
	void flush() override;

private:
	mblk_t *_sps = nullptr;
	mblk_t *_pps = nullptr;
};

class H264ParameterSetsStore: public H26xParameterSetsStore {
public:
	H264ParameterSetsStore(): H26xParameterSetsStore("video/avc", {MSH264NaluTypeSPS, MSH264NaluTypePPS}) {}
};

class H264ToolFactory: public H26xToolFactory {
public:
	H26xNaluHeader *createNaluHeader() const override;
	NalPacker *createNalPacker(MSFactory *factory) const override;
	NalUnpacker *createNalUnpacker() const override;
	H26xParameterSetsInserter *createParameterSetsInserter() const override;
	H26xParameterSetsStore *createParameterSetsStore() const override;
};

} // namespace mediastreamer

#endif // __cplusplus

#endif /* defined(H264_UTILS_H) */

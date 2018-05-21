/*
 mediastreamer2 library - modular sound and video processing and streaming
 Copyright (C) 2015  Belledonne Communications <info@belledonne-communications.com>
 
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

#ifndef H264_UTILS_H
#define H264_UTILS_H

#include <mediastreamer2/msqueue.h>
#include "mediastreamer2/msvideo.h"

/**
 * Enumeration that lists the different type of NAL unit
 */
typedef enum {
	MSH264NaluTypeUnknown = -1,
	MSH264NaluTypeIDR = 5,
	MSH264NaluTypeSEI = 6,
	MSH264NaluTypeSPS = 7,
	MSH264NaluTypePPS = 8,
	MSH264NaluTypeSTAPA = 24,
	MSH264NaluTypeFUA = 28
} MSH264NaluType;


#ifdef __cplusplus
extern "C" {
#endif

/**
 * Extract the NRI value from NALu header.
 */
uint8_t ms_h264_nalu_get_nri(const mblk_t *nalu);

/**
 * @brief Finds out the NALu type corresponding to a given integer according ITU's H264 standard and RFC3984.
 * @return Returns the type should the integer be mapped to a known type, or #MSH264NaluTypeUnknown if shouldn't.
 */
MSH264NaluType ms_h264_int_to_nalu_type(uint8_t val);

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
 * @param sps A mblk_t holding the SPS.
 * @return The video size.
 */
MSVideoSize ms_h264_sps_get_video_size(const mblk_t* sps);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
namespace mediastreamer {

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

};
#endif

#endif /* defined(H264_UTILS_H) */

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

/**
 * Enumeration that lists the different type of NAL unit
 */
typedef enum {
    MSH264NaluTypeIDR = 5,
	MSH264NaluTypeSEI = 6,
    MSH264NaluTypeSPS = 7,
    MSH264NaluTypePPS = 8
} MSH264NaluType;

/**
 * Get the type of a NAL unit
 * @param nalu The NAL unit to analyse
 * @return The nalu type
 */
MSH264NaluType ms_h264_nalu_get_type(const mblk_t *nalu);

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
 * Slices a frame into several nal units.
 *
 * Same as ms_h264_bitstream_to_nalus() except that the stream must be in size-prefixed
 * format i.e. each nalu in the stream must be prefixed by its size encoded on 4 bytes big-endian.
 * @param frame Buffer containing the stream to slice
 * @param size Size of the buffer
 * @param nalus The queue where produced nal units will be pushed into
 * @param idr_count If not NULL, use the pointer to store the number of IDR nalus found in the stream.
 */
void ms_h264_stream_to_nalus(const uint8_t *frame, size_t size, MSQueue *nalus, int *idr_count);

#endif /* defined(H264_UTILS_H) */

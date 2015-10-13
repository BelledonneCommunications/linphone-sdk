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
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef H264_UTILS_H
#define H264_UTILS_H

#include <mediastreamer2/msqueue.h>

/**
 * Slices a frame in the bitstream format into several nal units.
 *
 * Nal units are stored into freshly allocated mblk_t buffers which are pushed into
 * a queue. This function does not alter the buffer where the bitstream is contained in.
 * That function assumes that the input bitstream discribes a whole encoded frame.
 * @param bitstream Pointer on a memory segment that contains the bitstream to slice.
 * @param size Size of the memory segment.
 * @param A queue where produced nal units will be push into.
 */
void ms_h264_bitstream_frame_to_nalus(const uint8_t *bitstream, size_t size, MSQueue *nalus);

/**
 * Slices a frame into several nal units.
 *
 * Same as bitstream_frame_to_nalus() except that the frame must be in the size-prefixed
 * format i.e. each nalu in the frame must be prefixed by its size encoded on 4 bytes big-endian.
 * @param frame The frame to slice
 * @param size Size of the frame
 * @param nalus The queue where produced nal units will be pushed
 * @param idr_count Pointer on a variable where the number of IDR nalus will be stored into. Can be NULL.
 */
void ms_h264_frame_to_nalus(const uint8_t *frame, size_t size, MSQueue *nalus, int *idr_count);

#endif /* defined(H264_UTILS_H) */

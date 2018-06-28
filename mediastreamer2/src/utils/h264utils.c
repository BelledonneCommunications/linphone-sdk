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

#include "h264utils.h"
#include <mediastreamer2/msqueue.h>
#include "mediastreamer2/bits_rw.h"

#include <math.h>

static void  push_nalu(const uint8_t *begin, const uint8_t *end, MSQueue *nalus) {
	unsigned ecount = 0;
	const uint8_t *src = begin;
	size_t nalu_len = (end - begin);
	uint8_t nalu_byte  = *src++;

	mblk_t *m = allocb(nalu_len, 0);

	// Removal of the 3 in a 003x sequence
	// This emulation prevention byte is normally part of a NAL unit.
	/* H.264 standard sys in par 7.4.1 page 58
	 emulation_prevention_three_byte is a byte equal to 0x03.
	 When an emulation_prevention_three_byte is present in a NAL unit, it shall be discarded by the decoding process.
	 Within the NAL unit, the following three-byte sequence shall not occur at any byte-aligned position: 0x000000, 0x000001, 0x00002
	 */
	*m->b_wptr++ = nalu_byte;
	while (src < end - 3) {
		if (src[0] == 0 && src[1] == 0 && src[2] == 3) {
			*m->b_wptr++ = 0;
			*m->b_wptr++ = 0;
			// drop the emulation_prevention_three_byte
			src += 3;
			++ecount;
			continue;
		}
		*m->b_wptr++ = *src++;
	}
	*m->b_wptr++ = *src++;
	*m->b_wptr++ = *src++;
	*m->b_wptr++ = *src++;

	ms_queue_put(nalus, m);
}

void ms_h264_bitstream_to_nalus(const uint8_t *bitstream, size_t size, MSQueue *nalus) {
	size_t i;
	const uint8_t *p, *begin = NULL;
	int zeroes = 0;

	for (i = 0, p = bitstream; i < size; ++i) {
		if (*p == 0) {
			++zeroes;
		} else if (zeroes >= 2 && *p == 1) {
			if (begin) {
				push_nalu(begin, p - zeroes, nalus);
			}
			begin = p + 1;
		} else zeroes = 0;
		++p;
	}
	if (begin) push_nalu(begin, p, nalus);
}

MSH264NaluType ms_h264_nalu_get_type(const mblk_t *nalu) {
	return (*nalu->b_rptr) & ((1 << 5) - 1);
}

unsigned int _ms_h264_get_id(const mblk_t *parameter_set, size_t offset, const char *symbol_name) {
	unsigned int id;
	MSBitsReader reader;
	const uint8_t *buffer = parameter_set->b_rptr + offset;
	size_t bufsize = parameter_set->b_wptr - buffer;
	ms_bits_reader_init(&reader, buffer, bufsize);
	return ms_bits_reader_ue(&reader, &id, symbol_name) == 0 ? id : 0;
}

unsigned int ms_h264_sps_get_id(const mblk_t *sps) {
	return _ms_h264_get_id(sps, 4, "seq_parameter_set_id");
}

unsigned int ms_h264_pps_get_id(const mblk_t *pps) {
	return _ms_h264_get_id(pps, 1, "pic_parameter_set_id");
}

void ms_h264_stream_to_nalus(const uint8_t *frame, size_t size, MSQueue *nalus, int *idr_count) {
	const uint8_t *ptr = frame;

	if (idr_count) *idr_count = 0;

	while (ptr < frame + size) {
		uint32_t nalu_size;
		mblk_t *nalu;
		MSH264NaluType nalu_type;

		memcpy(&nalu_size, ptr, 4);
		nalu_size = ntohl(nalu_size);

		nalu = allocb(nalu_size, 0);
		memcpy(nalu->b_wptr, ptr + 4, nalu_size);
		ptr += nalu_size + 4;
		nalu->b_wptr += nalu_size;

		nalu_type = ms_h264_nalu_get_type(nalu);
		if (idr_count && nalu_type == MSH264NaluTypeIDR)(*idr_count)++;

		ms_queue_put(nalus, nalu);
	}
}

MSVideoSize ms_h264_sps_get_video_size(const mblk_t *sps) {
	MSVideoSize video_size;
	MSBitsReader reader;
	unsigned int profile_idc;
	unsigned int pic_order_cnt_type;
	unsigned int pic_width_in_mbs_minus1;
	unsigned int pic_height_in_map_units_minus1;
	unsigned int frame_mbs_only_flag;
	unsigned int frame_cropping_flag;
	/* init reader, but skip 1 byte (nal_unit_type) */
	ms_bits_reader_init(&reader, sps->b_rptr + 1, sps->b_wptr - sps->b_rptr - 1);

	ms_bits_reader_n_bits(&reader, 8, &profile_idc, "profile_idc");
	ms_bits_reader_n_bits(&reader, 1, NULL, "constraint_set0_flag");
	ms_bits_reader_n_bits(&reader, 1, NULL, "constraint_set1_flag");
	ms_bits_reader_n_bits(&reader, 1, NULL, "constraint_set2_flag");
	ms_bits_reader_n_bits(&reader, 5, NULL, "reserved_zero_5bits");
	ms_bits_reader_n_bits(&reader, 8, NULL, "level_idc");
	ms_bits_reader_ue(&reader, NULL, "seq_parameter_set_id");

	if (profile_idc == 100) {
		{
			ms_bits_reader_ue(&reader, NULL, "chroma_format_idc");
		}
		ms_bits_reader_ue(&reader, NULL, "bit_depth_luma_minus8");
		ms_bits_reader_ue(&reader, NULL, "bit_depth_chroma_minus8");
		ms_bits_reader_n_bits(&reader, 1, NULL, "qpprime_y_zero_transform_bypass_flag");
		ms_bits_reader_n_bits(&reader, 1, NULL, "seq_scaling_matrix_present_flag");
	}
	ms_bits_reader_ue(&reader, NULL, "log2_max_frame_num_minus4");

	ms_bits_reader_ue(&reader, &pic_order_cnt_type, "pic_order_cnt_type");

	if (pic_order_cnt_type == 0) {
		ms_bits_reader_ue(&reader, NULL, "log2_max_pic_order_cnt_lsb_minus4");
	} else if (pic_order_cnt_type == 1) {
		int i;
		{
			ms_bits_reader_n_bits(&reader, 1, NULL, "delta_pic_order_always_zero_flag");
		}
		ms_bits_reader_se(&reader, NULL, "offset_for_non_ref_pic");
		ms_bits_reader_se(&reader, NULL, "offset_for_top_to_bottom_field");
		{
			unsigned int num_ref_frames_in_pic_order_cnt_cycle;
			ms_bits_reader_ue(&reader, &num_ref_frames_in_pic_order_cnt_cycle, "num_ref_frames_in_pic_order_cnt_cycle");
			for (i = 0; i < (int)num_ref_frames_in_pic_order_cnt_cycle; i++) {
				ms_bits_reader_se(&reader, NULL, "offset_for_ref_frame[ i ]");
			}
		}
	}

	ms_bits_reader_ue(&reader, NULL, "num_ref_frames");
	ms_bits_reader_n_bits(&reader, 1, NULL, "gaps_in_frame_num_value_allowed_flag");

	ms_bits_reader_ue(&reader, &pic_width_in_mbs_minus1, "pic_width_in_mbs_minus1");

	ms_bits_reader_ue(&reader, &pic_height_in_map_units_minus1, "pic_height_in_map_units_minus1");

	ms_bits_reader_n_bits(&reader, 1, &frame_mbs_only_flag, "frame_mbs_only_flag");

	if (!frame_mbs_only_flag) {
		ms_bits_reader_n_bits(&reader, 1, NULL, "mb_adaptive_frame_field_flag");
	}

	ms_bits_reader_n_bits(&reader, 1, NULL, "direct_8x8_inference_flag");

	ms_bits_reader_n_bits(&reader, 1, &frame_cropping_flag, "frame_cropping_flag");

	if (frame_cropping_flag) {
		unsigned int frame_crop_left_offset;
		unsigned int frame_crop_right_offset;
		unsigned int frame_crop_top_offset;
		unsigned int frame_crop_bottom_offset;
		ms_bits_reader_ue(&reader, &frame_crop_left_offset, "frame_crop_left_offset");
		ms_bits_reader_ue(&reader, &frame_crop_right_offset, "frame_crop_right_offset");
		video_size.width = ((pic_width_in_mbs_minus1 + 1) * 16) - frame_crop_left_offset * 2 - frame_crop_right_offset * 2;
		ms_bits_reader_ue(&reader, &frame_crop_top_offset, "frame_crop_top_offset");
		ms_bits_reader_ue(&reader, &frame_crop_bottom_offset, "frame_crop_bottom_offset");
		video_size.height = ((2 - frame_mbs_only_flag) * (pic_height_in_map_units_minus1 + 1) * 16) - (frame_crop_top_offset * 2) - (frame_crop_bottom_offset * 2);
	} else {
		video_size.width = (pic_width_in_mbs_minus1 + 1) * 16;
		video_size.height = (2 - frame_mbs_only_flag) * (pic_height_in_map_units_minus1 + 1) * 16;
	}

	ms_bits_reader_n_bits(&reader, 1, NULL, "vui_parameters_present_flag");
	return video_size;
}

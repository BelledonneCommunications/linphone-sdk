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

#include <math.h>

// #include <ortp/str_utils.h>

#include "mediastreamer2/bits_rw.h"
#include "mediastreamer2/msqueue.h"
#include "mediastreamer2/rfc3984.h"

#include "h264-nal-packer.h"
#include "h264-nal-unpacker.h"
#include "h264-utils.h"
#include "h26x-utils.h"

using namespace mediastreamer;
using namespace std;

extern "C" {

void ms_h264_bitstream_to_nalus(const uint8_t *bitstream, size_t size, MSQueue *nalus) {
	H26xUtils::byteStreamToNalus(bitstream, size, nalus);
}

uint8_t ms_h264_nalu_get_nri(const mblk_t *nalu) {
	return ((*nalu->b_rptr) >> 5) & 0x3;
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

} // extern "C"


namespace mediastreamer {

H264NaluType::H264NaluType(uint8_t value) {
	if (value & 0xe0) throw out_of_range("H264 NALu type higher than 31");
	_value = value;
}

const H264NaluType H264NaluType::Idr = 5;
const H264NaluType H264NaluType::Sps = 7;
const H264NaluType H264NaluType::Pps = 8;
const H264NaluType H264NaluType::StapA = 24;
const H264NaluType H264NaluType::FuA = 28;

void H264NaluHeader::setNri(uint8_t nri) {
	if (nri > 3) throw out_of_range("H264 NALu NRI higher than 3");
	_nri = nri;
}

bool H264NaluHeader::operator==(const H264NaluHeader &h2) const {
	return _fBit == h2._fBit && _type == h2._type && _nri == h2._nri;
}

void H264NaluHeader::parse(const uint8_t *header) {
	uint8_t h = *header;
	_type = H264NaluType(h & 0x1f);
	h >>= 5;
	_nri = (h & 0x3);
	h >>= 2;
	_fBit = (h != 0);
}

mblk_t *H264NaluHeader::forge() const {
	uint8_t h = _fBit ? 1 : 0;
	h <<= 2;
	h |= _nri;
	h <<= 5;
	h |= uint8_t(_type);

	mblk_t *m = allocb(1, 0);
	*m->b_wptr++ = h;
	return m;
}

unsigned int H264FrameAnalyser::Info::toUInt() const {
	unsigned int res = 0;
	if (this->hasIdr) res |= Rfc3984HasIDR;
	if (this->hasSps) res |= Rfc3984HasSPS;
	if (this->hasPps) res |= Rfc3984HasPPS;
	if (this->newSps) res |= Rfc3984NewSPS;
	if (this->newPps) res |= Rfc3984NewPPS;
	return res;
}

H264FrameAnalyser::~H264FrameAnalyser() {
	if (_lastSps) freemsg(_lastSps);
	if (_lastPps) freemsg(_lastPps);
}

H264FrameAnalyser::Info H264FrameAnalyser::analyse(const MSQueue *frame) {
	Info info;
	for (const mblk_t *nalu = qbegin(&frame->q); !qend(&frame->q, nalu); nalu = qnext(&frame->q, nalu)) {
		MSH264NaluType type = ms_h264_nalu_get_type(nalu);
		if (type == MSH264NaluTypeIDR) {
			info.hasIdr = true;
		} else if (type == MSH264NaluTypeSPS) {
			info.hasSps = true;
			info.newSps = updateParameterSet(nalu);
		} else if (type == MSH264NaluTypePPS) {
			info.hasPps = true;
			info.newPps = updateParameterSet(nalu);
		}
	}
	return info;
}

bool H264FrameAnalyser::updateParameterSet(const mblk_t *new_parameter_set) {
	mblk_t *&last_parameter_set = ms_h264_nalu_get_type(new_parameter_set) == MSH264NaluTypePPS ? _lastPps : _lastSps;
	if (last_parameter_set != nullptr) {
		size_t last_size = last_parameter_set->b_wptr - last_parameter_set->b_rptr;
		size_t new_size = new_parameter_set->b_wptr - new_parameter_set->b_rptr;
		if (last_size != new_size || memcmp(last_parameter_set->b_rptr, new_parameter_set->b_rptr, new_size) != 0) {
			freemsg(last_parameter_set);
			last_parameter_set = copyb(new_parameter_set);
			return true;
		} else {
			return false;
		}
	} else {
		last_parameter_set = copyb(new_parameter_set);
		return true;
	}
}

mblk_t *H264Tools::prependFuIndicatorAndHeader(mblk_t *m, uint8_t indicator, bool_t start, bool_t end, uint8_t type) {
	mblk_t *h = allocb(2, 0);
	h->b_wptr[0] = indicator;
	h->b_wptr[1] = ((start & 0x1) << 7) | ((end & 0x1) << 6) | type;
	h->b_wptr += 2;
	h->b_cont = m;
	if (start) m->b_rptr++;/*skip original nalu header */
		return h;
}

H264ParameterSetsInserter::~H264ParameterSetsInserter() {
	flush();
}

void H264ParameterSetsInserter::process(MSQueue *in, MSQueue *out) {
	bool psBeforeIdr = false;
	while (mblk_t *m = ms_queue_get(in)) {
		MSH264NaluType type = ms_h264_nalu_get_type(m);
		if (type == MSH264NaluTypeSPS) {
			psBeforeIdr = true;
			replaceParameterSet(_sps, m);
		} else if (type == MSH264NaluTypePPS) {
			psBeforeIdr = true;
			replaceParameterSet(_pps, m);
		} else {
			if (_sps && _pps) {
				if (type == MSH264NaluTypeIDR && !psBeforeIdr) {
					ms_queue_put(out, dupmsg(_sps));
					ms_queue_put(out, dupmsg(_pps));
				}
				ms_queue_put(out, m);
			} else {
				freemsg(m);
			}
			psBeforeIdr = false;
		}
	}
}

void H264ParameterSetsInserter::flush() {
	replaceParameterSet(_sps, nullptr);
	replaceParameterSet(_pps, nullptr);
}

H26xNaluHeader *H264ToolFactory::createNaluHeader() const {
	return new H264NaluHeader();
}

NalPacker *H264ToolFactory::createNalPacker(MSFactory *factory) const {
	return new H264NalPacker();
}

NalUnpacker *H264ToolFactory::createNalUnpacker() const {
	return new H264NalUnpacker();
}

H26xParameterSetsInserter *H264ToolFactory::createParamterSetsInserter() const {
	return new H264ParameterSetsInserter();
}

H26xParameterSetsStore *H264ToolFactory::createParameterSetsStore() const {
	return new H264ParameterSetsStore();
}

} // namespace mediastreamer2

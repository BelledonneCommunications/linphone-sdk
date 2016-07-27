/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006  Simon MORLAT (simon.morlat@linphone.org)

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


#ifndef VP8RTPFMT_H
#define VP8RTPFMT_H

#include <mediastreamer2/mscommon.h>
#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/msqueue.h>
#include <mediastreamer2/msvideo.h>

/**
 * This file declares an API useful to pack/unpack a VP8 stream in RTP packets
 * as described in draft-ietf-payload-vp8-11
 * (http://tools.ietf.org/html/draft-ietf-payload-vp8-11)
 */

#ifdef __cplusplus
extern "C"{
#endif

	typedef enum Vp8RtpFmtErrorCode {
		Vp8RtpFmtOk = 0,
		Vp8RtpFmtInvalidPayloadDescriptor = -1,
		Vp8RtpFmtIncompleteFrame = -2,
		Vp8RtpFmtInvalidFrame = -3
	} Vp8RtpFmtErrorCode;

	typedef struct Vp8RtpFmtPayloadDescriptor {
		uint16_t pictureid;
		uint8_t pid;
		uint8_t tl0picidx;
		uint8_t tid;
		uint8_t keyidx;
		bool_t extended_control_bits_present;
		bool_t non_reference_frame;
		bool_t start_of_partition;
		bool_t pictureid_present;
		bool_t tl0picidx_present;
		bool_t tid_present;
		bool_t keyidx_present;
		bool_t layer_sync;
	} Vp8RtpFmtPayloadDescriptor;

	typedef struct Vp8RtpFmtPacket {
		mblk_t *m;
		Vp8RtpFmtPayloadDescriptor *pd;
		Vp8RtpFmtErrorCode error;
		bool_t cseq_inconsistency;
	} Vp8RtpFmtPacket;

	typedef struct Vp8RtpFmtPartition {
		MSList *packets_list;
		mblk_t *m;
		size_t size;
		bool_t has_start;
		bool_t has_marker;
		bool_t outputted;
	} Vp8RtpFmtPartition;

	typedef struct Vp8RtpFmtFramePartitionsInfo {
		uint32_t partition_sizes[8];
		uint8_t nb_partitions;
	} Vp8RtpFmtFramePartitionsInfo;

	typedef struct Vp8RtpFmtFrame {
		Vp8RtpFmtFramePartitionsInfo partitions_info;
		Vp8RtpFmtPartition *partitions[9];
		Vp8RtpFmtErrorCode error;
		uint16_t pictureid;
		bool_t pictureid_present;
		bool_t unnumbered_partitions;
		bool_t keyframe;
		bool_t reference;
		bool_t outputted;
		bool_t discarded;
	} Vp8RtpFmtFrame;

	typedef struct Vp8RtpFmtFrameInfo {
		uint16_t pictureid;
		bool_t pictureid_present;
		bool_t keyframe;
	} Vp8RtpFmtFrameInfo;


	typedef struct Vp8RtpFmtUnpackerCtx {
		MSFilter *filter;
		MSList *frames_list;
		MSList *non_processed_packets_list;
		MSVideoSize video_size;
		uint32_t last_ts;
		uint16_t ref_cseq;
		bool_t avpf_enabled;
		bool_t freeze_on_error;
		bool_t output_partitions;
		bool_t waiting_for_reference_frame;
		bool_t error_notified;
		bool_t valid_keyframe_received;
		bool_t initialized_last_ts;
		bool_t initialized_ref_cseq;
	} Vp8RtpFmtUnpackerCtx;

	typedef struct Vp8RtpFmtPackerCtx {
		MSQueue *output_queue;
		MSFactory *factory;
	} Vp8RtpFmtPackerCtx;


	void vp8rtpfmt_packer_init(Vp8RtpFmtPackerCtx *ctx);
	void vp8rtpfmt_packer_uninit(Vp8RtpFmtPackerCtx *ctx);
	void vp8rtpfmt_packer_process(Vp8RtpFmtPackerCtx *ctx, MSList *in, MSQueue *out, MSFactory *f);

	void vp8rtpfmt_unpacker_init(Vp8RtpFmtUnpackerCtx *ctx, MSFilter *f, bool_t avpf_enabled, bool_t freeze_on_error, bool_t output_partitions);
	void vp8rtpfmt_unpacker_uninit(Vp8RtpFmtUnpackerCtx *ctx);
	void vp8rtpfmt_unpacker_feed(Vp8RtpFmtUnpackerCtx *ctx, MSQueue *in);
	int vp8rtpfmt_unpacker_get_frame(Vp8RtpFmtUnpackerCtx *ctx, MSQueue *out, Vp8RtpFmtFrameInfo *frame_info);
	uint32_t vp8rtpfmt_unpacker_calc_extended_cseq(Vp8RtpFmtUnpackerCtx *ctx, uint16_t cseq);
	void vp8rtpfmt_send_rpsi(Vp8RtpFmtUnpackerCtx *ctx, uint16_t pictureid);

#ifdef __cplusplus
}
#endif

#endif /* VP8RTPFMT_H */

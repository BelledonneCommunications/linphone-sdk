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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/


#ifndef VP8RTPFMT_H
#define VP8RTPFMT_H

#include <mediastreamer2/mscommon.h>
#include <mediastreamer2/msqueue.h>

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
		Vp8RtpFmtIncompleteFrame = -2
	} Vp8RtpFmtErrorCode;


	typedef struct Vp8RtpFmtContext {
		MSList *list;
		MSQueue output_queue;
		MSQueue frame_queue;
		uint32_t last_ts;
		uint32_t last_cseq;
		uint32_t ref_cseq;
		bool_t initialized_last_ts;
		bool_t initialized_ref_cseq;
	} Vp8RtpFmtContext;

	typedef struct Vp8PayloadDescriptor {
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
	} Vp8PayloadDescriptor;

	typedef struct Vp8RtpFmtPacket {
		mblk_t *m;
		Vp8PayloadDescriptor *pd;
		uint32_t extended_cseq;
		Vp8RtpFmtErrorCode error;
		bool_t processed;
	} Vp8RtpFmtPacket;


	Vp8RtpFmtContext *vp8rtpfmt_new(void);
	void vp8rtpfmt_destroy(Vp8RtpFmtContext *ctx);
	void vp8rtpfmt_init(Vp8RtpFmtContext *ctx);
	void vp8rtpfmt_uninit(Vp8RtpFmtContext *ctx);
	void vp8rtpfmt_unpack(Vp8RtpFmtContext *ctx, MSQueue *in);
	uint32_t vp8rtpfmt_extended_cseq(Vp8RtpFmtContext *ctx, uint16_t cseq);

#ifdef __cplusplus
}
#endif

#endif /* VP8RTPFMT_H */
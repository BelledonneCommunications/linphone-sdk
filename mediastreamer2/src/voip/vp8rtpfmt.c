/*
vp8rtpfmt.c

mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2011-2012 Belledonne Communications, Grenoble, France

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


#include "vp8rtpfmt.h"


/*#define VP8RTPFMT_DEBUG*/


#ifdef VP8RTPFMT_DEBUG
static uint16_t get_partition_pictureid(Vp8RtpFmtPartition *partition) {
	Vp8RtpFmtPacket *packet = ms_list_nth_data(partition->packets_list, 0);
	return packet->pd->pictureid;
}

static uint8_t get_partition_id(Vp8RtpFmtPartition *partition) {
	Vp8RtpFmtPacket *packet = ms_list_nth_data(partition->packets_list, 0);
	return packet->pd->pid;
}

static int is_partition_non_reference(Vp8RtpFmtPartition *partition) {
	Vp8RtpFmtPacket *packet = ms_list_nth_data(partition->packets_list, 0);
	return packet->pd->non_reference_frame;
}

static uint32_t get_partition_ts(Vp8RtpFmtPartition *partition) {
	return mblk_get_timestamp_info(partition->m);
}

static uint16_t get_frame_pictureid(Vp8RtpFmtFrame *frame) {
	return get_partition_pictureid(ms_list_nth_data(frame->partitions_list, 0));
}

static int is_frame_non_reference(Vp8RtpFmtFrame *frame) {
	return is_partition_non_reference(ms_list_nth_data(frame->partitions_list, 0));
}

static uint32_t get_frame_ts(Vp8RtpFmtFrame *frame) {
	return get_partition_ts(ms_list_nth_data(frame->partitions_list, 0));
}

static void print_packet(void *data) {
	Vp8RtpFmtPacket *packet = (Vp8RtpFmtPacket *)data;
	ms_message("\t\tcseq=%10u\tS=%d\terror=%d",
		packet->extended_cseq, packet->pd->start_of_partition, packet->error);
}

static void print_partition(void *data) {
	Vp8RtpFmtPartition *partition = (Vp8RtpFmtPartition *)data;
	ms_message("\tpartition [%p]:\tpid=%d\tlast=%d\terror=%d",
		partition, get_partition_id(partition), partition->last_partition_of_frame, partition->error);
	ms_list_for_each(partition->packets_list, print_packet);
}

static void print_frame(void *data) {
	Vp8RtpFmtFrame *frame = (Vp8RtpFmtFrame *)data;
	ms_message("frame [%p]:\tts=%u\tpictureid=%u\tN=%d\terror=%d",
		frame, get_frame_ts(frame), get_frame_pictureid(frame), is_frame_non_reference(frame), frame->error);
	ms_list_for_each(frame->partitions_list, print_partition);
}
#endif /* VP8RTPFMT_DEBUG */

static void free_packet(void *data) {
	Vp8RtpFmtPacket *packet = (Vp8RtpFmtPacket *)data;
	if (packet->m != NULL) {
		freemsg(packet->m);
	}
	ms_free(packet->pd);
	ms_free(packet);
}

static void free_outputted_partition(void *data) {
	Vp8RtpFmtPartition *partition = (Vp8RtpFmtPartition *)data;
	ms_list_for_each(partition->packets_list, free_packet);
	ms_list_free(partition->packets_list);
	ms_free(partition);
}

static void free_discarded_partition(void *data) {
	Vp8RtpFmtPartition *partition = (Vp8RtpFmtPartition *)data;
	if (partition->m != NULL) {
		freemsg(partition->m);
	}
	ms_list_for_each(partition->packets_list, free_packet);
	ms_list_free(partition->packets_list);
	ms_free(partition);
}

static void free_partition(void *data) {
	Vp8RtpFmtPartition *partition = (Vp8RtpFmtPartition *)data;
	if (partition->outputted == TRUE) {
		free_outputted_partition(partition);
	} else {
		free_discarded_partition(partition);
	}
}

static int free_outputted_frame(const void *data, const void *b) {
	Vp8RtpFmtFrame *frame = (Vp8RtpFmtFrame *)data;
	bool_t *outputted = (bool_t *)b;
	bool_t foutputted = frame->outputted;
	if (foutputted == *outputted) ms_free(frame);
	return (foutputted != *outputted);
}

static int free_discarded_frame(const void *data, const void *d) {
	Vp8RtpFmtFrame *frame = (Vp8RtpFmtFrame *)data;
	bool_t *discarded = (bool_t *)d;
	bool_t fdiscarded = frame->discarded;
	if (fdiscarded == *discarded) ms_free(frame);
	return (fdiscarded != *discarded);
}

static int cseq_compare(const void *d1, const void *d2) {
	Vp8RtpFmtPacket *p1 = (Vp8RtpFmtPacket *)d1;
	Vp8RtpFmtPacket *p2 = (Vp8RtpFmtPacket *)d2;
	return (p1->extended_cseq > p2->extended_cseq);
}

static void add_partition_to_frame(Vp8RtpFmtFrame *frame, Vp8RtpFmtPartition *partition) {
	Vp8RtpFmtPacket *packet;
	int nb_packets;
	int i;

	nb_packets = ms_list_size(partition->packets_list);
	if (partition->error == Vp8RtpFmtOk) {
		for (i = 0; i < nb_packets; i++) {
			packet = ms_list_nth_data(partition->packets_list, i);
			if (partition->m == NULL) {
				partition->m = packet->m;
			} else {
				concatb(partition->m, packet->m);
			}
			packet->m = NULL;
		}
		if (partition->m != NULL) {
			msgpullup(partition->m, -1);
		}

		frame->partitions_list = ms_list_append(frame->partitions_list, (void *)partition);
	} else {
		for (i = 0; i < nb_packets; i++) {
			packet = ms_list_nth_data(partition->packets_list, i);
			if (packet->error == Vp8RtpFmtOk) {
				packet->error = Vp8RtpFmtIncompletePartition;
			}
		}
	}
}

static void generate_partitions_list(Vp8RtpFmtUnpackerCtx *ctx, Vp8RtpFmtFrame *frame, MSList *packets_list) {
	Vp8RtpFmtPacket *packet = NULL;
	Vp8RtpFmtPartition *partition = NULL;
	int nb_packets = ms_list_size(packets_list);
	uint32_t last_cseq;
	int i;

	for (i = 0; i < nb_packets; i++) {
		packet = (Vp8RtpFmtPacket *)ms_list_nth_data(packets_list, i);
		if ((i > 0) && (packet->extended_cseq != (last_cseq + 1))) {
			if (partition != NULL) {
				/* There was a gap in the sequence numbers, the current partition is incomplete. */
				partition->packets_list = ms_list_append(partition->packets_list, (void *)packet);
				partition->error = Vp8RtpFmtIncompletePartition;
				frame->error = Vp8RtpFmtIncompleteFrame;
			}
		}
		if (packet->error == Vp8RtpFmtOk) {
			if (packet->pd->start_of_partition == TRUE) {
				if (partition != NULL) {
					/* The current partition is complete, and the current packet is part of a new partition. */
					add_partition_to_frame(frame, partition);
				}
				partition = ms_new0(Vp8RtpFmtPartition, 1);
				partition->packets_list = ms_list_append(partition->packets_list, (void *)packet);
			} else {
				if (partition != NULL) {
					/* The current packet is a part of the current partition. */
					partition->packets_list = ms_list_append(partition->packets_list, (void *)packet);
				} else {
					/* The current packet is not a start of partition, but a start of partition was expected. */
					partition = ms_new0(Vp8RtpFmtPartition, 1);
					partition->packets_list = ms_list_append(partition->packets_list, (void *)packet);
					partition->error = Vp8RtpFmtIncompletePartition;
					frame->error = Vp8RtpFmtIncompleteFrame;
				}
			}
		} else {
			/* Malformed packet. */
			if (partition == NULL) {
				partition = ms_new0(Vp8RtpFmtPartition, 1);
			}
			partition->packets_list = ms_list_append(partition->packets_list, (void *)packet);
			partition->error = Vp8RtpFmtInvalidPartition;
			frame->error = Vp8RtpFmtInvalidFrame;
		}
		if (mblk_get_marker_info(packet->m) == TRUE) {
			partition->last_partition_of_frame = TRUE;
			add_partition_to_frame(frame, partition);
			partition = NULL;
		}
		last_cseq = packet->extended_cseq;
	}
}

static bool_t is_first_partition_present_in_frame(Vp8RtpFmtFrame *frame) {
	Vp8RtpFmtPartition *partition = ms_list_nth_data(frame->partitions_list, 0);
	Vp8RtpFmtPacket *packet = ms_list_nth_data(partition->packets_list, 0);
	if ((packet->pd->start_of_partition == TRUE) && (packet->pd->pid == 0)) {
		return TRUE;
	}
	return FALSE;
}

static void add_frame(Vp8RtpFmtUnpackerCtx *ctx, MSList **packets_list) {
	Vp8RtpFmtFrame *frame;

	if (ms_list_size(*packets_list) > 0) {
		frame = ms_new0(Vp8RtpFmtFrame, 1);
		generate_partitions_list(ctx, frame, *packets_list);
		if (ms_list_size(frame->partitions_list) > 0) {
			if (is_first_partition_present_in_frame(frame) == TRUE) {
				/* The first packet of the frame is really the start of the frame. */
			} else {
				frame->error = Vp8RtpFmtIncompleteFrame;
			}
			ctx->frames_list = ms_list_append(ctx->frames_list, (void *)frame);
		} else {
			/* There are no valid partitions in the frame. */
			ms_warning("VP8 frame without any valid partition.");
			ms_free(frame);
			ms_filter_notify_no_arg(ctx->filter, MS_VIDEO_DECODER_DECODING_ERRORS);
			ms_filter_notify_no_arg(ctx->filter, MS_VIDEO_DECODER_SEND_PLI);
		}
	}
	ms_list_free(*packets_list);
	*packets_list = NULL;
}

static void generate_frames_list(Vp8RtpFmtUnpackerCtx *ctx, MSList *packets_list) {
	Vp8RtpFmtPacket *packet;
	MSList *frame_packets_list = NULL;
	int nb_packets = ms_list_size(packets_list);
	uint32_t ts;
	int i;

	for (i = 0; i < nb_packets; i++) {
		packet = ms_list_nth_data(packets_list, i);
		ts = mblk_get_timestamp_info(packet->m);

		if ((ctx->initialized_last_ts == TRUE) && (ts != ctx->last_ts)) {
			/* The current packet is from a frame different than the previous one
			* (that apparently is not complete). */
			add_frame(ctx, &frame_packets_list);
		}
		ctx->last_ts = ts;
		ctx->initialized_last_ts = TRUE;

		/* Add the current packet to the current frame. */
		frame_packets_list = ms_list_append(frame_packets_list, packet);

		if (mblk_get_marker_info(packet->m)) {
			/* The current packet is the last of the current frame. */
			add_frame(ctx, &frame_packets_list);
		}
	}
}

static bool_t is_frame_marker_present(Vp8RtpFmtFrame *frame) {
	int nb_partitions = ms_list_size(frame->partitions_list);
	Vp8RtpFmtPartition *partition = ms_list_nth_data(frame->partitions_list, nb_partitions - 1);
	return (bool_t)mblk_get_marker_info(partition->m);
}

static void output_partition(MSQueue *out, Vp8RtpFmtPartition *partition) {
	if (partition->last_partition_of_frame) {
		mblk_set_marker_info(partition->m, 1);
	}
	ms_queue_put(out, (void *)partition->m);
	partition->outputted = TRUE;
}

static void output_frame(MSQueue *out, Vp8RtpFmtFrame *frame) {
	Vp8RtpFmtPartition *partition;
	int nb_partitions = ms_list_size(frame->partitions_list);
	mblk_t *om;
	mblk_t *curm;
	int i;

	for (i = 0; i < nb_partitions; i++) {
		partition = ms_list_nth_data(frame->partitions_list, i);
		if (i == 0) {
			om = partition->m;
			curm = om;
		} else {
			curm = concatb(curm, partition->m);
		}
		partition->outputted = TRUE;
	}
	mblk_set_marker_info(om, 1);
	ms_queue_put(out, (void *)om);
}

static bool_t is_keyframe(Vp8RtpFmtFrame *frame) {
	Vp8RtpFmtPartition *partition;
	int nb_partitions = ms_list_size(frame->partitions_list);

	if (nb_partitions < 1) return FALSE;
	partition = (Vp8RtpFmtPartition *)ms_list_nth_data(frame->partitions_list, 0);
	if (partition->m->b_rptr[0] & 0x01) return FALSE;

	return TRUE;
}

static void output_valid_partitions(Vp8RtpFmtUnpackerCtx *ctx, MSQueue *out) {
	Vp8RtpFmtPartition *partition = NULL;
	Vp8RtpFmtFrame *frame;
	int nb_frames = ms_list_size(ctx->frames_list);
	int nb_partitions;
	int i;
	int j;

	for (i = 0; i < nb_frames; i++) {
		frame = ms_list_nth_data(ctx->frames_list, i);
		if (frame->error == Vp8RtpFmtOk) {
			if (is_keyframe(frame) == TRUE) {
				ctx->valid_keyframe_received = TRUE;
			}
			if (ctx->valid_keyframe_received == TRUE) {
				/* Output the complete valid frame if the first keyframe has been received. */
				if (ctx->output_partitions == TRUE) {
					nb_partitions = ms_list_size(frame->partitions_list);
					for (j = 0; j < nb_partitions; j++) {
						partition = ms_list_nth_data(frame->partitions_list, j);
						output_partition(out, partition);
					}
				} else {
					/* Output the full frame in one mblk_t. */
					output_frame(out, frame);
				}
				frame->outputted = TRUE;
			} else {
				/* Drop frames until the first keyframe is successfully received. */
				frame->discarded = TRUE;
			}
		} else if (is_frame_marker_present(frame) == TRUE) {
			if (is_first_partition_present_in_frame(frame) == TRUE) {
				if (ctx->output_partitions == TRUE) {
					/* Output the valid partitions of the frame. */
					nb_partitions = ms_list_size(frame->partitions_list);
					for (i = 0; i < nb_partitions; i++) {
						partition = ms_list_nth_data(frame->partitions_list, i);
						if (partition->error == Vp8RtpFmtOk) {
							if (i == (nb_partitions - 1)) {
								partition->last_partition_of_frame = TRUE;
							}
							output_partition(out, partition);
						}
					}
					frame->outputted = TRUE;
				} else {
					/* Drop the frame for which some partitions are missing/invalid. */
					frame->discarded = TRUE;
				}
				ms_filter_notify_no_arg(ctx->filter, MS_VIDEO_DECODER_DECODING_ERRORS);
				ms_filter_notify_no_arg(ctx->filter, MS_VIDEO_DECODER_SEND_PLI);
			} else {
				/* Drop the frame for which the first partition is missing. */
				ms_warning("VP8 frame without first partition.");
				frame->discarded = TRUE;
				ms_filter_notify_no_arg(ctx->filter, MS_VIDEO_DECODER_DECODING_ERRORS);
				ms_filter_notify_no_arg(ctx->filter, MS_VIDEO_DECODER_SEND_PLI);
			}
		} else {
			/* The last packet of the frame has not been received.
			 * Wait for the next iteration of the filter to see if we have it then. */
			ms_warning("VP8 frame without last packet.");
			// TODO: Try to get the missing packets at the next iteration of the filter.
			frame->discarded = TRUE;
			ms_filter_notify_no_arg(ctx->filter, MS_VIDEO_DECODER_DECODING_ERRORS);
			ms_filter_notify_no_arg(ctx->filter, MS_VIDEO_DECODER_SEND_PLI);
		}
	}
}

static void free_partitions_of_outputted_frames(void *data) {
	Vp8RtpFmtFrame *frame = (Vp8RtpFmtFrame *)data;
	if (frame->outputted == TRUE) {
		ms_list_for_each(frame->partitions_list, free_partition);
		ms_list_free(frame->partitions_list);
	}
}

static void free_partitions_of_discarded_frames(void *data) {
	Vp8RtpFmtFrame *frame = (Vp8RtpFmtFrame *)data;
	if (frame->discarded == TRUE) {
		ms_list_for_each(frame->partitions_list, free_partition);
		ms_list_free(frame->partitions_list);
	}
}

static void clean_outputted_partitions(Vp8RtpFmtUnpackerCtx *ctx) {
	bool_t outputted = TRUE;
	ms_list_for_each(ctx->frames_list, free_partitions_of_outputted_frames);
	ctx->frames_list = ms_list_remove_custom(ctx->frames_list, free_outputted_frame, &outputted);
}

static void clean_discarded_partitions(Vp8RtpFmtUnpackerCtx *ctx) {
	bool_t discarded = TRUE;
	ms_list_for_each(ctx->frames_list, free_partitions_of_discarded_frames);
	ctx->frames_list = ms_list_remove_custom(ctx->frames_list, free_discarded_frame, &discarded);
}

static Vp8RtpFmtErrorCode parse_payload_descriptor(Vp8RtpFmtPacket *packet) {
	uint8_t *h = packet->m->b_rptr;
	Vp8RtpFmtPayloadDescriptor *pd = packet->pd;
	unsigned int packet_size = packet->m->b_wptr - packet->m->b_rptr;
	uint8_t offset = 0;

	if (packet_size == 0) return Vp8RtpFmtInvalidPayloadDescriptor;

	memset(pd, 0, sizeof(Vp8RtpFmtPayloadDescriptor));

	/* Parse mandatory first octet of payload descriptor. */
	if (h[offset] & (1 << 7)) pd->extended_control_bits_present = TRUE;
	if (h[offset] & (1 << 5)) pd->non_reference_frame = TRUE;
	if (h[offset] & (1 << 4)) pd->start_of_partition = TRUE;
	pd->pid = (h[offset] & 0x07);
	offset++;
	if (offset >= packet_size) return Vp8RtpFmtInvalidPayloadDescriptor;
	/* Parse the first extension octet if needed. */
	if (pd->extended_control_bits_present == TRUE) {
		if (h[offset] & (1 << 7)) pd->pictureid_present = TRUE;
		if (h[offset] & (1 << 6)) pd->tl0picidx_present = TRUE;
		if (h[offset] & (1 << 5)) pd->tid_present = TRUE;
		if (h[offset] & (1 << 4)) pd->keyidx_present = TRUE;
		if ((pd->tl0picidx_present == TRUE) && (pd->tid_present != TRUE)) {
			/* Invalid payload descriptor. */
			return Vp8RtpFmtInvalidPayloadDescriptor;
		}
		offset++;
		if (offset >= packet_size) return Vp8RtpFmtInvalidPayloadDescriptor;
	}
	/* Parse the pictureID if needed. */
	if (pd->pictureid_present == TRUE) {
		if (h[offset] & (1 << 7)) {
			/* The pictureID is 16 bits long. */
			if ((offset + 1) >= packet_size) return Vp8RtpFmtInvalidPayloadDescriptor;
			pd->pictureid = (h[offset] << 8) | h[offset + 1];
			offset += 2;
		} else {
			/* The pictureId is 8 bits long. */
			pd->pictureid = h[offset];
			offset++;
		}
		if (offset >= packet_size) return Vp8RtpFmtInvalidPayloadDescriptor;
	}
	/* Parse the tl0picidx if needed. */
	if (pd->tl0picidx_present == TRUE) {
		pd->tl0picidx = h[offset];
		offset++;
		if (offset >= packet_size) return Vp8RtpFmtInvalidPayloadDescriptor;
	}
	/* Parse the tid and/or keyidx if needed. */
	if (pd->tid_present == TRUE) {
		pd->tid = (h[offset] & 0xC0) >> 6;
		if (h[offset] & (1 << 5)) pd->layer_sync = TRUE;
	}
	if (pd->keyidx_present == TRUE) {
		pd->keyidx = (h[offset] & 0x1F);
	}
	if ((pd->tid_present == TRUE) || (pd->keyidx_present == TRUE)) {
		offset++;
		if (offset >= packet_size) return Vp8RtpFmtInvalidPayloadDescriptor;
	}

	packet->m->b_rptr = &h[offset];
	return Vp8RtpFmtOk;
}


void vp8rtpfmt_unpacker_init(Vp8RtpFmtUnpackerCtx *ctx, MSFilter *f, bool_t output_partitions) {
	ctx->filter = f;
	ctx->frames_list = NULL;
	ms_queue_init(&ctx->output_queue);
	ctx->output_partitions = output_partitions;
	ctx->valid_keyframe_received = FALSE;
	ctx->initialized_last_ts = FALSE;
	ctx->initialized_ref_cseq = FALSE;
}

void vp8rtpfmt_unpacker_uninit(Vp8RtpFmtUnpackerCtx *ctx) {
	ms_list_free(ctx->frames_list);
	ms_queue_flush(&ctx->output_queue);
}

void vp8rtpfmt_unpacker_process(Vp8RtpFmtUnpackerCtx *ctx, MSQueue *inout) {
	MSList *packets_list = NULL;
	Vp8RtpFmtPacket *packet;
	mblk_t *m;

#ifdef VP8RTPFMT_DEBUG
	ms_message("vp8rtpfmt_unpacker_process:");
#endif
	while ((m = ms_queue_get(inout)) != NULL) {
		packet = ms_new(Vp8RtpFmtPacket, 1);
		packet->m = m;
		packet->extended_cseq = vp8rtpfmt_unpacker_calc_extended_cseq(ctx, mblk_get_cseq(m));
		packet->pd = ms_new0(Vp8RtpFmtPayloadDescriptor, 1);

		if (m->b_cont) msgpullup(m, -1);
		packet->error = parse_payload_descriptor(packet);
		packets_list = ms_list_insert_sorted(packets_list, (void *)packet, cseq_compare);
	}

	generate_frames_list(ctx, packets_list);
	ms_list_free(packets_list);
#ifdef VP8RTPFMT_DEBUG
	ms_list_for_each(ctx->frames_list, print_frame);
#endif /* VP8RTPFMT_DEBUG */
	output_valid_partitions(ctx, inout);
	clean_outputted_partitions(ctx);
	clean_discarded_partitions(ctx);

	if (ms_list_size(ctx->frames_list) == 0) {
		ms_list_free(ctx->frames_list);
		ctx->frames_list = NULL;
	} else {
		ms_message("VP8 frames are remaining for next iteration of the filter.");
	}
}

uint32_t vp8rtpfmt_unpacker_calc_extended_cseq(Vp8RtpFmtUnpackerCtx *ctx, uint16_t cseq) {
	uint32_t extended_cseq;
	uint32_t cseq_a;
	uint32_t cseq_b;
	uint32_t diff_a;
	uint32_t diff_b;

	if (ctx->initialized_ref_cseq != TRUE) {
		ctx->ref_cseq = cseq | 0x80000000;
		ctx->initialized_ref_cseq = TRUE;
		extended_cseq = ctx->ref_cseq;
	} else {
		cseq_a = cseq | (ctx->ref_cseq & 0xFFFF0000);
		if (ctx->ref_cseq < cseq_a) {
			cseq_b = cseq_a - 0x00010000;
			diff_a = cseq_a - ctx->ref_cseq;
			diff_b = ctx->ref_cseq - cseq_b;
		} else {
			cseq_b = cseq_a + 0x00010000;
			diff_a = ctx->ref_cseq - cseq_a;
			diff_b = cseq_b - ctx->ref_cseq;
		}
		if (diff_a < diff_b) {
			extended_cseq = cseq_a;
		} else {
			extended_cseq = cseq_b;
		}
		ctx->ref_cseq = extended_cseq;
	}

	return extended_cseq;
}



static void packer_process_frame_part(void *p, void *c) {
	Vp8RtpFmtPacket *packet = (Vp8RtpFmtPacket *)p;
	Vp8RtpFmtPackerCtx *ctx = (Vp8RtpFmtPackerCtx *)c;
	mblk_t *pdm;
	mblk_t *dm;
	uint8_t *rptr;
	uint8_t pdsize = 1;
	int max_size = ms_get_payload_max_size();
	int dlen;
	bool_t marker_info = mblk_get_marker_info(packet->m);

	/* Calculate the payload descriptor size. */
	if (packet->pd->extended_control_bits_present == TRUE) pdsize++;
	if (packet->pd->pictureid_present == TRUE) {
		pdsize++;
		if (packet->pd->pictureid & 0x8000) pdsize++;
	}
	if (packet->pd->tl0picidx_present == TRUE) pdsize++;
	if ((packet->pd->tid_present == TRUE) || (packet->pd->keyidx_present == TRUE)) pdsize++;

	for (rptr = packet->m->b_rptr; rptr < packet->m->b_wptr;) {
		/* Allocate the payload descriptor. */
		pdm = allocb(pdsize, 0);
		memset(pdm->b_wptr, 0, pdsize);
		mblk_set_timestamp_info(pdm, mblk_get_timestamp_info(packet->m));
		mblk_set_marker_info(pdm, FALSE);
		mblk_set_marker_info(dm, FALSE);
		/* Fill the mandatory octet of the payload descriptor. */
		if (packet->pd->extended_control_bits_present == TRUE) *pdm->b_wptr |= (1 << 7);
		if (packet->pd->non_reference_frame == TRUE) *pdm->b_wptr |= (1 << 5);
		if (packet->pd->start_of_partition == TRUE) {
			if (packet->m->b_rptr == rptr) *pdm->b_wptr |= (1 << 4);
		}
		*pdm->b_wptr |= (packet->pd->pid & 0x07);
		pdm->b_wptr++;
		/* Fill the extension bit field octet of the payload descriptor. */
		if (packet->pd->extended_control_bits_present == TRUE) {
			if (packet->pd->pictureid_present == TRUE) *pdm->b_wptr |= (1 << 7);
			if (packet->pd->tl0picidx_present == TRUE) *pdm->b_wptr |= (1 << 6);
			if (packet->pd->tid_present == TRUE) *pdm->b_wptr |= (1 << 5);
			if (packet->pd->keyidx_present == TRUE) *pdm->b_wptr |= (1 << 4);
			pdm->b_wptr++;
		}
		/* Fill the pictureID field of the payload descriptor. */
		if (packet->pd->pictureid_present == TRUE) {
			if (packet->pd->pictureid & 0x8000) {
				*pdm->b_wptr |= ((packet->pd->pictureid >> 8) & 0xFF);
				pdm->b_wptr++;
			}
			*pdm->b_wptr |= (packet->pd->pictureid & 0xFF);
			pdm->b_wptr++;
		}
		/* Fill the tl0picidx octet of the payload descriptor. */
		if (packet->pd->tl0picidx_present == TRUE) {
			*pdm->b_wptr = packet->pd->tl0picidx;
			pdm->b_wptr++;
		}
		if ((packet->pd->tid_present == TRUE) || (packet->pd->keyidx_present == TRUE)) {
			if (packet->pd->tid_present == TRUE) {
				*pdm->b_wptr |= (packet->pd->tid & 0xC0);
				if (packet->pd->layer_sync == TRUE) *pdm->b_wptr |= (1 << 5);
			}
			if (packet->pd->keyidx_present == TRUE) {
				*pdm->b_wptr |= (packet->pd->keyidx & 0x1F);
			}
			pdm->b_wptr++;
		}

		dlen = MIN((max_size - pdsize), (packet->m->b_wptr - rptr));
		dm = dupb(packet->m);
		dm->b_rptr = rptr;
		dm->b_wptr = rptr + dlen;
		dm->b_wptr = dm->b_rptr + dlen;
		pdm->b_cont = dm;
		rptr += dlen;

		ms_queue_put(ctx->output_queue, pdm);
	}

	/* Set marker bit on last packet if required. */
	mblk_set_marker_info(pdm, marker_info);
	mblk_set_marker_info(dm, marker_info);

	freeb(packet->m);
	packet->m = NULL;
}


void vp8rtpfmt_packer_init(Vp8RtpFmtPackerCtx *ctx) {
}

void vp8rtpfmt_packer_uninit(Vp8RtpFmtPackerCtx *ctx) {
}

void vp8rtpfmt_packer_process(Vp8RtpFmtPackerCtx *ctx, MSList *in, MSQueue *out) {
	ctx->output_queue = out;
	ms_list_for_each2(in, packer_process_frame_part, ctx);
	ms_list_for_each(in, free_packet);
	ms_list_free(in);
}

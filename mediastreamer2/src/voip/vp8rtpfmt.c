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


#define VP8RTPFMT_DEBUG


static void free_packet(void *data) {
	Vp8RtpFmtPacket *packet = (Vp8RtpFmtPacket *)data;
	if (packet->error != Vp8RtpFmtOk) {
		freemsg(packet->m);
	}
	ms_free(packet->pd);
	ms_free(packet);
}

#ifdef VP8RTPFMT_DEBUG
static void print_packet(void *data) {
	Vp8RtpFmtPacket *packet = (Vp8RtpFmtPacket *)data;
	int marker;
	uint32_t ts;

	marker = mblk_get_marker_info(packet->m);
	ts = mblk_get_timestamp_info(packet->m);
	ms_message("ts=%10u\tcseq=%10u\t%s\tS=%d\tN=%d\tpid=%1d\terror=%d\tpictureid=%5u",
		ts, packet->extended_cseq, (marker == 0) ? " " : "M", packet->pd->start_of_partition, packet->pd->non_reference_frame, packet->pd->pid, packet->error, packet->pd->pictureid);
}
#endif /* VP8RTPFMT_DEBUG */

static void free_partition(void *data) {
	Vp8RtpFmtPartition *partition = (Vp8RtpFmtPartition *)data;
	if (partition->error == Vp8RtpFmtOk) {
		freemsg(partition->m);
	}
	ms_list_free(partition->packets_list);
	ms_free(partition);
}

static void free_frame(void *data) {
	Vp8RtpFmtFrame *frame = (Vp8RtpFmtFrame *)data;
	ms_list_for_each(frame->partitions_list, free_partition);
	ms_list_free(frame->packets_list);
	ms_list_free(frame->partitions_list);
	ms_free(frame);
}

static int cseq_compare(const void *d1, const void *d2) {
	Vp8RtpFmtPacket *p1 = (Vp8RtpFmtPacket *)d1;
	Vp8RtpFmtPacket *p2 = (Vp8RtpFmtPacket *)d2;
	return (p1->extended_cseq > p2->extended_cseq);
}

static int processed_packet(const void *data, const void *userdata) {
	Vp8RtpFmtPacket *packet = (Vp8RtpFmtPacket *)data;
	return  (packet->processed != TRUE);
}

static void generate_frames_list(void *data, void *userdata) {
	Vp8RtpFmtPacket *packet = (Vp8RtpFmtPacket *)data;
	Vp8RtpFmtUnpackerCtx *ctx = (Vp8RtpFmtUnpackerCtx *)userdata;
	uint32_t ts = mblk_get_timestamp_info(packet->m);

	if (ctx->current_frame == NULL) {
		ctx->current_frame = ms_new0(Vp8RtpFmtFrame, 1);
		ctx->last_ts = ts;
	}

	if (ts != ctx->last_ts) {
		/* The current packet is from a frame different than the previous one
		 * (that apparently is not complete). */
		ctx->frames_list = ms_list_append(ctx->frames_list, (void *)ctx->current_frame);
		ctx->current_frame = ms_new0(Vp8RtpFmtFrame, 1);
	}
	ctx->last_ts = ts;

	/* Add the current packet to the current frame. */
	ctx->current_frame->packets_list = ms_list_append(ctx->current_frame->packets_list, packet);

	if (mblk_get_marker_info(packet->m)) {
		/* The current packet is the last of the current frame. */
		ctx->frames_list = ms_list_append(ctx->frames_list, (void *)ctx->current_frame);
		ctx->current_frame = NULL;
	}
}

static void add_partition_to_frame(Vp8RtpFmtFrame *frame, Vp8RtpFmtPartition *partition) {
	Vp8RtpFmtPacket *current_packet;
	int nb_packets;
	int i;

	nb_packets = ms_list_size(partition->packets_list);
	for (i = 0; i < nb_packets; i++) {
		current_packet = ms_list_nth_data(partition->packets_list, i);
		current_packet->processed = TRUE;
		if (partition->m == NULL) {
			partition->m = current_packet->m;
		} else {
			concatb(partition->m, current_packet->m);
		}
	}
	if (partition->m != NULL) {
		msgpullup(partition->m, -1);
	}

	frame->partitions_list = ms_list_append(frame->partitions_list, (void *)partition);
}

static void generate_partitions_list(void *data, void *userdata) {
	Vp8RtpFmtFrame *frame = (Vp8RtpFmtFrame *)data;
	Vp8RtpFmtUnpackerCtx *ctx = (Vp8RtpFmtUnpackerCtx *)userdata;
	Vp8RtpFmtPacket *current_packet = NULL;
	Vp8RtpFmtPartition *current_partition = NULL;
	int nb_packets = ms_list_size(frame->packets_list);
	int i;

	for (i = 0; i < nb_packets; i++) {
		current_packet = (Vp8RtpFmtPacket *)ms_list_nth_data(frame->packets_list, i);
		if (current_packet->extended_cseq != (ctx->last_cseq + 1)) {
			if (current_partition != NULL) {
				/* There was a gap in the sequence numbers, the current partition is incomplete. */
				current_partition->packets_list = ms_list_append(current_partition->packets_list, (void *)current_packet);
				current_partition->error = Vp8RtpFmtIncompletePartition;
				frame->error = Vp8RtpFmtIncompleteFrame;
			}
		}
		if (current_packet->error == Vp8RtpFmtOk) {
			if (current_packet->pd->start_of_partition == TRUE) {
				if (current_partition != NULL) {
					/* The current partition is complete, and the current packet is part of a new partition. */
					add_partition_to_frame(frame, current_partition);
				}
				current_partition = ms_new0(Vp8RtpFmtPartition, 1);
				current_partition->packets_list = ms_list_append(current_partition->packets_list, (void *)current_packet);
			} else {
				if (current_partition != NULL) {
					/* The current packet is a part of the current partition. */
					current_partition->packets_list = ms_list_append(current_partition->packets_list, (void *)current_packet);
				} else {
					/* The current packet is not a start of partition, but a start of partition was expected. */
					current_partition = ms_new0(Vp8RtpFmtPartition, 1);
					current_partition->packets_list = ms_list_append(current_partition->packets_list, (void *)current_packet);
					current_partition->error = Vp8RtpFmtIncompletePartition;
					frame->error = Vp8RtpFmtIncompleteFrame;
				}
			}
		} else {
			/* Malformed packet. */
			if (current_partition == NULL) {
				current_partition = ms_new0(Vp8RtpFmtPartition, 1);
			}
			current_partition->packets_list = ms_list_append(current_partition->packets_list, (void *)current_packet);
			current_partition->error = Vp8RtpFmtInvalidPartition;
			frame->error = Vp8RtpFmtInvalidFrame;
		}
		if (mblk_get_marker_info(current_packet->m) == TRUE) {
			current_partition->last_partition_of_frame = TRUE;
			add_partition_to_frame(frame, current_partition);
			current_partition = NULL;
		}
		ctx->last_cseq = current_packet->extended_cseq;
	}
}

static void output_valid_partitions(void *data, void *userdata) {
	Vp8RtpFmtFrame *frame = (Vp8RtpFmtFrame *)data;
	Vp8RtpFmtUnpackerCtx *ctx = (Vp8RtpFmtUnpackerCtx *)userdata;
	Vp8RtpFmtPartition *current_partition = NULL;
	int nb_partitions;
	int i;

	if (frame->error == Vp8RtpFmtOk) {
		/* Output the complete valid frame. */
		nb_partitions = ms_list_size(frame->partitions_list);
		for (i = 0; i < nb_partitions; i++) {
			current_partition = ms_list_nth_data(frame->partitions_list, i);
			ctx->output_list = ms_list_append(ctx->output_list, (void *)current_partition);
		}
	} else {
		/* Output the valid partitions of the frame. */
		nb_partitions = ms_list_size(frame->partitions_list);
		for (i = 0; i < nb_partitions; i++) {
			current_partition = ms_list_nth_data(frame->partitions_list, i);
			if (current_partition->error == Vp8RtpFmtOk) {
				ctx->output_list = ms_list_append(ctx->output_list, (void *)current_partition);
			}
		}
	}
}

static Vp8RtpFmtErrorCode parse_payload_descriptor(Vp8RtpFmtPacket *packet) {
	uint8_t *h = packet->m->b_rptr;
	Vp8RtpFmtPayloadDescriptor *pd = packet->pd;

	memset(pd, 0, sizeof(Vp8RtpFmtPayloadDescriptor));

	/* Parse mandatory first octet of payload descriptor. */
	if (*h & (1 << 7)) pd->extended_control_bits_present = TRUE;
	if (*h & (1 << 5)) pd->non_reference_frame = TRUE;
	if (*h & (1 << 4)) pd->start_of_partition = TRUE;
	pd->pid = (*h & 0x07);
	h++;
	/* Parse the first extension octet if needed. */
	if (pd->extended_control_bits_present == TRUE) {
		if (*h & (1 << 7)) pd->pictureid_present = TRUE;
		if (*h & (1 << 6)) pd->tl0picidx_present = TRUE;
		if (*h & (1 << 5)) pd->tid_present = TRUE;
		if (*h & (1 << 4)) pd->keyidx_present = TRUE;
		if ((pd->tl0picidx_present == TRUE) && (pd->tid_present != TRUE)) {
			/* Invalid payload descriptor. */
			return Vp8RtpFmtInvalidPayloadDescriptor;
		}
		h++;
	}
	/* Parse the pictureID if needed. */
	if (pd->pictureid_present == TRUE) {
		if (*h & (1 << 7)) {
			/* The pictureID is 16 bits long. */
			pd->pictureid = (*h << 8) | *(h + 1);
			h += 2;
		} else {
			/* The pictureId is 8 bits long. */
			pd->pictureid = *h;
			h++;
		}
	}
	/* Parse the tl0picidx if needed. */
	if (pd->tl0picidx_present == TRUE) {
		pd->tl0picidx = *h;
		h++;
	}
	/* Parse the tid and/or keyidx if needed. */
	if (pd->tid_present == TRUE) {
		pd->tid = (*h & 0xC0) >> 6;
		if (*h & (1 << 5)) pd->layer_sync = TRUE;
	}
	if (pd->keyidx_present == TRUE) {
		pd->keyidx = (*h & 0x1F);
	}
	if ((pd->tid_present == TRUE) || (pd->keyidx_present == TRUE)) {
		h++;
	}

	packet->m->b_rptr = h;
	return Vp8RtpFmtOk;
}


void vp8rtpfmt_unpacker_init(Vp8RtpFmtUnpackerCtx *ctx) {
	ctx->packets_list = NULL;
	ctx->frames_list = NULL;
	ctx->output_list = NULL;
	ctx->current_frame = NULL;
	ctx->initialized_ref_cseq = FALSE;
}

void vp8rtpfmt_unpacker_uninit(Vp8RtpFmtUnpackerCtx *ctx) {
	ms_list_for_each(ctx->packets_list, free_packet);
	ms_list_free(ctx->packets_list);
	ms_list_free(ctx->frames_list);
	ms_list_free(ctx->output_list);
}

void vp8rtpfmt_unpacker_unpack(Vp8RtpFmtUnpackerCtx *ctx, MSQueue *in) {
	Vp8RtpFmtPacket *packet;
	mblk_t *m;

#ifdef VP8RTPFMT_DEBUG
	ms_message("vp8rtpfmt_unpacker_process:");
#endif
	while ((m = ms_queue_get(in)) != 0) {
		packet = ms_new(Vp8RtpFmtPacket, 1);
		packet->m = m;
		packet->extended_cseq = vp8rtpfmt_unpacker_calc_extended_cseq(ctx, mblk_get_cseq(m));
		packet->pd = ms_new0(Vp8RtpFmtPayloadDescriptor, 1);
		packet->processed = FALSE;

		if (m->b_cont) msgpullup(m, -1);
		packet->error = parse_payload_descriptor(packet);
		if (packet->error != Vp8RtpFmtOk) {
			ms_warning("VP8: Invalid payload descriptor.");
		} else {
			ctx->packets_list = ms_list_insert_sorted(ctx->packets_list, (void *)packet, cseq_compare);
		}
	}

#ifdef VP8RTPFMT_DEBUG
	ms_list_for_each(ctx->packets_list, print_packet);
#endif /* VP8RTPFMT_DEBUG */
	ms_list_for_each2(ctx->packets_list, generate_frames_list, (void *)ctx);
	ms_list_for_each2(ctx->frames_list, generate_partitions_list, (void *)ctx);
	ms_list_for_each2(ctx->frames_list, output_valid_partitions, (void *)ctx);
	ctx->packets_list = ms_list_remove_custom(ctx->packets_list, processed_packet, NULL);
	if (ms_list_size(ctx->packets_list) > 0) {
		/* If some packets have not been processed, set last_cseq to the one before the first packets remaining to be processed. */
		packet = (Vp8RtpFmtPacket *)ms_list_nth_data(ctx->packets_list, 0);
		ctx->last_cseq = packet->extended_cseq - 1;
	}
}

void vp8rtpfmt_unpacker_decode(Vp8RtpFmtUnpackerCtx *ctx, MSIterate2Func func, void *userdata) {
	ms_list_for_each2(ctx->output_list, func, userdata);
	ms_list_for_each(ctx->frames_list, free_frame);
	ms_list_free(ctx->frames_list);
	ctx->frames_list = NULL;
	ms_list_free(ctx->output_list);
	ctx->output_list = NULL;
}

uint32_t vp8rtpfmt_unpacker_calc_extended_cseq(Vp8RtpFmtUnpackerCtx *ctx, uint16_t cseq) {
	uint32_t extended_cseq;
	uint32_t cseq_a;
	uint32_t cseq_b;
	uint32_t diff_a;
	uint32_t diff_b;

	if (ctx->initialized_ref_cseq != TRUE) {
		ctx->ref_cseq = cseq | 0x80000000;
		ctx->last_cseq = ctx->ref_cseq - 1;
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

		ms_queue_put(&ctx->output_queue, pdm);
	}

	/* Set marker bit on last packet. */
	if (packet->pd->pid == ctx->nb_partitions) {
		mblk_set_marker_info(pdm, TRUE);
		mblk_set_marker_info(dm, TRUE);
	}

	freeb(packet->m);
	packet->m = NULL;
}


void vp8rtpfmt_packer_init(Vp8RtpFmtPackerCtx *ctx, uint8_t nb_partitions) {
	ms_queue_init(&ctx->output_queue);
	ctx->nb_partitions = nb_partitions;
}

void vp8rtpfmt_packer_uninit(Vp8RtpFmtPackerCtx *ctx) {
	ms_queue_flush(&ctx->output_queue);
}

void vp8rtpfmt_packer_process(Vp8RtpFmtPackerCtx *ctx, MSList *in) {
	ms_list_for_each2(in, packer_process_frame_part, ctx);
	ms_list_for_each(in, free_packet);
	ms_list_free(in);
}

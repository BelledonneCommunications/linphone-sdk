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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/


#include "vp8rtpfmt.h"

#include <limits.h>

/*#define VP8RTPFMT_DEBUG*/

#if (defined(__GNUC__) && __GNUC__) || defined(__SUNPRO_C)
#define DECLARE_ALIGNED(n,typ,val)  typ val __attribute__ ((aligned (n)))
#elif defined(_MSC_VER)
#define DECLARE_ALIGNED(n,typ,val)  __declspec(align(n)) typ val
#else
#warning No alignment directives known for this compiler.
#define DECLARE_ALIGNED(n,typ,val)  typ val
#endif

typedef size_t VP8_BD_VALUE;

#define VP8_BD_VALUE_SIZE ((int)sizeof(VP8_BD_VALUE)*CHAR_BIT)

#define VP8_LOTS_OF_BITS (0x40000000)

#define MB_LVL_MAX 2
#define MB_FEATURE_TREE_PROBS 3
#define MAX_MB_SEGMENTS 4
#define MAX_REF_LF_DELTAS 4
#define MAX_MODE_LF_DELTAS 4

typedef void (vp8_decrypt_cb)(void *decrypt_state, const unsigned char *input,
                              unsigned char *output, int count);

typedef struct
{
    const unsigned char *user_buffer_end;
    const unsigned char *user_buffer;
    VP8_BD_VALUE         value;
    int                  count;
    unsigned int         range;
    vp8_decrypt_cb      *decrypt_cb;
    void                *decrypt_state;
} BOOL_DECODER;

typedef unsigned char vp8_prob;
#define vp8_prob_half ( (vp8_prob) 128)
#define vp8_read vp8dx_decode_bool
#define vp8_read_literal vp8_decode_value
#define vp8_read_bit(R) vp8_read(R, vp8_prob_half)

DECLARE_ALIGNED(16, static const unsigned char, vp8_norm[256]) =
{
    0, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const int vp8_mb_feature_data_bits[MB_LVL_MAX] = {7, 6};

static void vp8dx_bool_decoder_fill(BOOL_DECODER *br)
{
    const unsigned char *bufptr = br->user_buffer;
    VP8_BD_VALUE value = br->value;
    int count = br->count;
    int shift = VP8_BD_VALUE_SIZE - 8 - (count + 8);
    size_t bytes_left = br->user_buffer_end - bufptr;
    size_t bits_left = bytes_left * CHAR_BIT;
    int x = (int)(shift + CHAR_BIT - bits_left);
    int loop_end = 0;
    unsigned char decrypted[sizeof(VP8_BD_VALUE) + 1];

    if (br->decrypt_cb) {
        size_t n = bytes_left > sizeof(decrypted) ? sizeof(decrypted) : bytes_left;
        br->decrypt_cb(br->decrypt_state, bufptr, decrypted, (int)n);
        bufptr = decrypted;
    }

    if(x >= 0)
    {
        count += VP8_LOTS_OF_BITS;
        loop_end = x;
    }

    if (x < 0 || bits_left)
    {
        while(shift >= loop_end)
        {
            count += CHAR_BIT;
            value |= (VP8_BD_VALUE)*bufptr << shift;
            ++bufptr;
            ++br->user_buffer;
            shift -= CHAR_BIT;
        }
    }

    br->value = value;
    br->count = count;
}

static int vp8dx_start_decode(BOOL_DECODER *br,
                       const unsigned char *source,
                       unsigned int source_sz,
                       vp8_decrypt_cb *decrypt_cb,
                       void *decrypt_state)
{
    br->user_buffer_end = source+source_sz;
    br->user_buffer     = source;
    br->value    = 0;
    br->count    = -8;
    br->range    = 255;
    br->decrypt_cb = decrypt_cb;
    br->decrypt_state = decrypt_state;

    if (source_sz && !source)
        return 1;

    /* Populate the buffer */
    vp8dx_bool_decoder_fill(br);

    return 0;
}

static int vp8dx_decode_bool(BOOL_DECODER *br, int probability) {
    unsigned int bit = 0;
    VP8_BD_VALUE value;
    unsigned int split;
    VP8_BD_VALUE bigsplit;
    int count;
    unsigned int range;

    split = 1 + (((br->range - 1) * probability) >> 8);

    if(br->count < 0)
        vp8dx_bool_decoder_fill(br);

    value = br->value;
    count = br->count;

    bigsplit = (VP8_BD_VALUE)split << (VP8_BD_VALUE_SIZE - 8);

    range = split;

    if (value >= bigsplit)
    {
        range = br->range - split;
        value = value - bigsplit;
        bit = 1;
    }

    {
        register unsigned int shift = vp8_norm[range];
        range <<= shift;
        value <<= shift;
        count -= shift;
    }
    br->value = value;
    br->count = count;
    br->range = range;

    return bit;
}

static int vp8_decode_value(BOOL_DECODER *br, int bits)
{
    int z = 0;
    int bit;

    for (bit = bits - 1; bit >= 0; bit--)
    {
        z |= (vp8dx_decode_bool(br, 0x80) << bit);
    }

    return z;
}

static mblk_t * concat_packets_of_partition(Vp8RtpFmtPartition *partition) {
	Vp8RtpFmtPacket *packet;
	int nb_packets = (int)bctbx_list_size(partition->packets_list);
	int i;

	if (partition->m != NULL) return partition->m;
	for (i = 0; i < nb_packets; i++) {
		packet = bctbx_list_nth_data(partition->packets_list, i);
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
	return partition->m;
}

static bool_t parse_frame_header(Vp8RtpFmtFrame *frame) {
	BOOL_DECODER bc;
	mblk_t *m = frame->partitions[0]->m;
	unsigned char *data;
	unsigned char *data_end;
	uint32_t size;
	uint32_t first_partition_length_in_bytes;
	uint32_t partition_size;
	bool_t segmentation_enabled;
	bool_t mode_ref_lf_delta_enabled;
	int i, j;
	int nb_partitions;

	if (m == NULL) {
		m = concat_packets_of_partition(frame->partitions[0]);
	}
	if (m == NULL) return FALSE;
	data = m->b_rptr;
	data_end = m->b_wptr;

	if (msgdsize(m) < 3) return FALSE;
	frame->keyframe = !(data[0] & 1);
	first_partition_length_in_bytes = (data[0] | (data[1] << 8) | (data[2] << 16)) >> 5;
	data += 3;
	if (frame->keyframe) {
		if (msgdsize(m) < 10) return FALSE;
		data += 7;
	}

	size = (unsigned int)(data_end - data);
	if (vp8dx_start_decode(&bc, data, size, NULL, NULL)) return FALSE;
	if (frame->keyframe) {
		(void)vp8_read_bit(&bc); /* color space */
		(void)vp8_read_bit(&bc); /* clamping type */
	}
	segmentation_enabled = vp8_read_bit(&bc);
	if (segmentation_enabled) {
		bool_t update_mb_segmentation_map = (bool_t)vp8_read_bit(&bc);
		bool_t update_mb_segmentation_data = (bool_t)vp8_read_bit(&bc);
		if (update_mb_segmentation_data) {
			(void)vp8_read_bit(&bc); /* segment feature mode */
			for (i = 0; i < MB_LVL_MAX; i++) {
				for (j = 0; j < MAX_MB_SEGMENTS; j++) {
					if (vp8_read_bit(&bc)) {
						(void)vp8_read_literal(&bc, vp8_mb_feature_data_bits[i]);
						(void)vp8_read_bit(&bc); /* sign */
					}
				}
			}
		}
		if (update_mb_segmentation_map) {
			for (i = 0; i < MB_FEATURE_TREE_PROBS; i++) {
				if (vp8_read_bit(&bc))
					(void)vp8_read_literal(&bc, 8);
            }
		}
	}
	(void)vp8_read_bit(&bc); /* filter type */
	(void)vp8_read_literal(&bc, 6); /* filter level */
	(void)vp8_read_literal(&bc, 3); /* sharpness level */
	mode_ref_lf_delta_enabled = (bool_t)vp8_read_bit(&bc);
	if (mode_ref_lf_delta_enabled) {
		bool_t mode_ref_lf_delta_update = (bool_t)vp8_read_bit(&bc);
		if (mode_ref_lf_delta_update) {
			for (i = 0; i < MAX_REF_LF_DELTAS; i++) {
				if (vp8_read_bit(&bc)) {
					(void)vp8_read_literal(&bc, 6);
					(void)vp8_read_bit(&bc); /* sign */
				}
			}
			for (i = 0; i < MAX_MODE_LF_DELTAS; i++) {
				if (vp8_read_bit(&bc)) {
					(void)vp8_read_literal(&bc, 6);
					(void)vp8_read_bit(&bc); /* sign */
				}
			}
		}
	}
	nb_partitions = (1 << vp8_read_literal(&bc, 2));
	if (nb_partitions > 8) return FALSE;
	frame->partitions_info.nb_partitions = nb_partitions;
	partition_size = (uint16_t)(data + first_partition_length_in_bytes - m->b_rptr + (3 * (nb_partitions - 1)));
	if (msgdsize(m) < partition_size) return FALSE;
	frame->partitions_info.partition_sizes[0] = partition_size;
	for (i = 1; i < nb_partitions; i++) {
		const unsigned char *partition_sizes = data + first_partition_length_in_bytes;
		const unsigned char *partition_size_ptr = partition_sizes + (i - 1) * 3;
		frame->partitions_info.partition_sizes[i] = partition_size_ptr[0] + (partition_size_ptr[1] << 8) + (partition_size_ptr[2] << 16);
	}
	return TRUE;
}



#ifdef VP8RTPFMT_DEBUG
static uint16_t get_partition_pictureid(Vp8RtpFmtPartition *partition) {
	Vp8RtpFmtPacket *packet = bctbx_list_nth_data(partition->packets_list, 0);
	return packet->pd->pictureid;
}

static uint8_t get_partition_id(Vp8RtpFmtPartition *partition) {
	Vp8RtpFmtPacket *packet = bctbx_list_nth_data(partition->packets_list, 0);
	return packet->pd->pid;
}

static int is_partition_non_reference(Vp8RtpFmtPartition *partition) {
	Vp8RtpFmtPacket *packet = bctbx_list_nth_data(partition->packets_list, 0);
	return packet->pd->non_reference_frame;
}

static uint32_t get_partition_ts(Vp8RtpFmtPartition *partition) {
	return mblk_get_timestamp_info(partition->m);
}

static uint16_t get_frame_pictureid(Vp8RtpFmtFrame *frame) {
	return get_partition_pictureid(frame->partitions[0]);
}

static int is_frame_non_reference(Vp8RtpFmtFrame *frame) {
	return is_partition_non_reference(frame->partitions[0]);
}

static uint32_t get_frame_ts(Vp8RtpFmtFrame *frame) {
	return get_partition_ts(frame->partitions[0]);
}

static void print_packet(void *data) {
	Vp8RtpFmtPacket *packet = (Vp8RtpFmtPacket *)data;
	ms_message("\t\tS=%d\terror=%d", packet->pd->start_of_partition, packet->error);
}

static void print_partition(Vp8RtpFmtPartition *partition) {
	ms_message("\tpartition [%p]:\tpid=%d", partition, get_partition_id(partition));
	bctbx_list_for_each(partition->packets_list, print_packet);
}

static void print_frame(void *data) {
	uint8_t i;
	Vp8RtpFmtFrame *frame = (Vp8RtpFmtFrame *)data;
	ms_message("frame [%p]:\tts=%u\tpictureid=0x%04x\tN=%d\terror=%d",
		frame, get_frame_ts(frame), get_frame_pictureid(frame), is_frame_non_reference(frame), frame->error);
	for (i = 0; i < frame->partitions_info.nb_partitions; i++) {
		print_partition(frame->partitions[i]);
	}
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

static void free_partition(void *data) {
	Vp8RtpFmtPartition *partition = (Vp8RtpFmtPartition *)data;
	if ((partition->m != NULL) && (partition->outputted != TRUE)) {
		freemsg(partition->m);
	}
	if (partition->packets_list != NULL) {
		bctbx_list_for_each(partition->packets_list, free_packet);
		bctbx_list_free(partition->packets_list);
	}
	ms_free(partition);
}

static MSVideoSize get_size_from_key_frame(Vp8RtpFmtFrame *frame) {
	MSVideoSize vs;
	Vp8RtpFmtPartition *partition = frame->partitions[0];
	vs.width = ((partition->m->b_rptr[7] << 8) | (partition->m->b_rptr[6])) & 0x3FFF;
	vs.height = ((partition->m->b_rptr[9] << 8) | (partition->m->b_rptr[8])) & 0x3FFF;
	return vs;
}

static void send_pli(Vp8RtpFmtUnpackerCtx *ctx) {
	if (ctx->avpf_enabled == TRUE) {
		if(ctx->filter) ms_filter_notify_no_arg(ctx->filter, MS_VIDEO_DECODER_SEND_PLI);
	} else {
		if(ctx->filter) ms_filter_notify_no_arg(ctx->filter, MS_VIDEO_DECODER_DECODING_ERRORS);
		ctx->error_notified = TRUE;
	}
}

static void send_sli(Vp8RtpFmtUnpackerCtx *ctx, Vp8RtpFmtFrame *frame) {
	if (ctx->avpf_enabled == TRUE) {
		if (frame->pictureid_present == TRUE) {
			MSVideoCodecSLI sli;
			sli.first = 0;
			sli.number = (ctx->video_size.width * ctx->video_size.height) / (16 * 16);
			sli.picture_id = frame->pictureid & 0x3F;
			if(ctx->filter) ms_filter_notify(ctx->filter, MS_VIDEO_DECODER_SEND_SLI, &sli);
		} else {
			send_pli(ctx);
		}
	} else {
		if(ctx->filter) ms_filter_notify_no_arg(ctx->filter, MS_VIDEO_DECODER_DECODING_ERRORS);
		ctx->error_notified = TRUE;
	}
}

void vp8rtpfmt_send_rpsi(Vp8RtpFmtUnpackerCtx *ctx, uint16_t pictureid) {
	if (ctx->avpf_enabled == TRUE) {
		MSVideoCodecRPSI rpsi;
		uint16_t picture_id16;
		uint8_t picture_id8;
		if (pictureid & 0x8000) {
			picture_id16 = htons(pictureid);
			rpsi.bit_string = (uint8_t *)&picture_id16;
			rpsi.bit_string_len = 16;
		} else {
			picture_id8 = pictureid & 0xFF;
			rpsi.bit_string = (uint8_t *)&picture_id8;
			rpsi.bit_string_len = 8;
		}
		if(ctx->filter) ms_filter_notify(ctx->filter, MS_VIDEO_DECODER_SEND_RPSI, &rpsi);
	}
}

static Vp8RtpFmtPartition * create_empty_partition(void) {
	Vp8RtpFmtPartition *partition = ms_new0(Vp8RtpFmtPartition, 1);
	partition->m = allocb(0, 0);
	return partition;
}

static void add_packet_to_frame(Vp8RtpFmtFrame *frame, Vp8RtpFmtPacket *packet) {
	uint8_t pid = packet->pd->pid;
	Vp8RtpFmtPartition *partition = frame->partitions[pid];
	if (!packet->pd->non_reference_frame) {
		frame->reference = TRUE;
	}
	if (packet->pd->pictureid_present) {
		frame->pictureid_present = TRUE;
		frame->pictureid = packet->pd->pictureid;
	}
	if (partition == NULL) {
		partition = frame->partitions[pid] = ms_new0(Vp8RtpFmtPartition, 1);
		partition->has_start = packet->pd->start_of_partition;
	}
	if (mblk_get_marker_info(packet->m)) {
		partition->has_marker = TRUE;
	}
	partition->packets_list = bctbx_list_append(partition->packets_list, (void *)packet);
	partition->size += msgdsize(packet->m);
}

static void generate_frame_partitions_list(Vp8RtpFmtFrame *frame, bctbx_list_t *packets_list) {
	Vp8RtpFmtPacket *packet = NULL;
	bctbx_list_t *it;
	
	frame->unnumbered_partitions = TRUE;
	for (it=packets_list;it!=NULL;it=it->next) {
		packet = (Vp8RtpFmtPacket *)it->data;
		if (packet->pd->pid != 0) {
			frame->unnumbered_partitions = FALSE;
			break;
		}
	}
	for (it=packets_list;it!=NULL;it=it->next) {
		packet = (Vp8RtpFmtPacket *)it->data;
		if (packet->error == Vp8RtpFmtOk) {
			add_packet_to_frame(frame, packet);
		} else {
			/* Malformed packet, ignore it. */
			free_packet(packet);
		}
	}
}

static void mark_frame_as_invalid(Vp8RtpFmtUnpackerCtx *ctx, Vp8RtpFmtFrame *frame) {
	frame->error = Vp8RtpFmtInvalidFrame;
	/*even if freeze_on_error is FALSE, if avpf is enabled it is preferable not to output an invalid frame.*/
	if (ctx->freeze_on_error || ctx->avpf_enabled) {
		ctx->waiting_for_reference_frame = TRUE;
	}
}

static void mark_frame_as_incomplete(Vp8RtpFmtUnpackerCtx *ctx, Vp8RtpFmtFrame *frame, uint8_t idx) {
	frame->error = Vp8RtpFmtIncompleteFrame;
	if (ctx->freeze_on_error == TRUE) {
		ctx->waiting_for_reference_frame = TRUE;
	}
}

static void check_frame_partitions_have_start(Vp8RtpFmtUnpackerCtx *ctx, Vp8RtpFmtFrame *frame) {
	Vp8RtpFmtPacket *packet;
	Vp8RtpFmtPartition *partition;
	int i;
	int j;

	if (frame->unnumbered_partitions == TRUE) return;

	for (i = 0; i <= frame->partitions_info.nb_partitions; i++) {
		partition = frame->partitions[i];
		if (partition == NULL) continue;
		for (j = 0; j < (int)bctbx_list_size(partition->packets_list); j++) {
			packet = (Vp8RtpFmtPacket *)bctbx_list_nth_data(partition->packets_list, j);
			if ((j == 0) && !partition->has_start && !packet->cseq_inconsistency) {
				/**
				 * We have detected a partition does not start at the beginning of a packet.
				 * Do not output partitions but the entire frame. Also consider frame has
				 * unnumbered partitions to prevent checks on the partitions.
				 * WARNING: This is a workaround because the partitions are now built according
				 * to the partition id of the packet header. However a packet can contain parts of
				 * several partitions. In this case we should split the packet in several parts and
				 * put these parts in the corresponding partitions and check from the partition sizes
				 * that we get from parsing the frame header.
				 */
				frame->unnumbered_partitions = TRUE;
				ctx->output_partitions = FALSE;
			}
		}
	}
}

static bool_t has_sequence_inconsistency(const bctbx_list_t *packets){
	Vp8RtpFmtPacket *packet;
	const bctbx_list_t *elem;
	for(elem=packets;elem!=NULL;elem=elem->next){
		packet=(Vp8RtpFmtPacket*)elem->data;
		if (packet->cseq_inconsistency) return TRUE;
	}
	return FALSE;
}

static void check_frame_partitions_list(Vp8RtpFmtUnpackerCtx *ctx, Vp8RtpFmtFrame *frame) {
	Vp8RtpFmtPartition *partition;
	int i;

	if (frame->partitions[0] == NULL) {
		mark_frame_as_invalid(ctx, frame);
		return;
	}
	if ((frame->partitions[0] != NULL) && (!frame->partitions[0]->has_start)) {
		mark_frame_as_invalid(ctx, frame);
		return;
	}
	
	if (has_sequence_inconsistency(frame->partitions[0]->packets_list)) {
		mark_frame_as_invalid(ctx, frame);
		return;
	}
	if (parse_frame_header(frame) != TRUE) {
		mark_frame_as_invalid(ctx, frame);
		return;
	}

	check_frame_partitions_have_start(ctx, frame);

	/* Do not try to perform next checks if the partitions are not numbered. */
	if (frame->unnumbered_partitions == TRUE) return;

	/* The partition 0 has been validated, check the following ones. */
	for (i = 1; i < frame->partitions_info.nb_partitions; i++) {
		partition = frame->partitions[i];
		if (partition == NULL) {
			mark_frame_as_incomplete(ctx, frame, i);
			continue;
		}
		if (partition->size != frame->partitions_info.partition_sizes[i]) {
			mark_frame_as_incomplete(ctx, frame, i);
			continue;
		}
	}

	/* Check the last partition of the frame. */
	partition = frame->partitions[frame->partitions_info.nb_partitions];
	if ((partition == NULL) || !partition->has_start || !partition->has_marker) {
		mark_frame_as_incomplete(ctx, frame, frame->partitions_info.nb_partitions);
	} else {
		if (has_sequence_inconsistency(partition->packets_list)){
			mark_frame_as_incomplete(ctx, frame, frame->partitions_info.nb_partitions);
		}
	}
}

static void notify_frame_error_if_any(Vp8RtpFmtUnpackerCtx *ctx, Vp8RtpFmtFrame *frame) {
	switch (frame->error) {
		case Vp8RtpFmtInvalidFrame:
		case Vp8RtpFmtIncompleteFrame:
			send_sli(ctx, frame);
			break;
		case Vp8RtpFmtOk:
		default:
			break;
	}
}

static void add_frame(Vp8RtpFmtUnpackerCtx *ctx, bctbx_list_t **packets_list) {
	Vp8RtpFmtFrame *frame;

	if (bctbx_list_size(*packets_list) > 0) {
		frame = ms_new0(Vp8RtpFmtFrame, 1);
		generate_frame_partitions_list(frame, *packets_list);
		check_frame_partitions_list(ctx, frame);
		notify_frame_error_if_any(ctx, frame);
		ctx->frames_list = bctbx_list_append(ctx->frames_list, (void *)frame);
	}
	bctbx_list_free(*packets_list);
	*packets_list = NULL;
}

static void generate_frames_list(Vp8RtpFmtUnpackerCtx *ctx, bctbx_list_t *packets_list) {
	Vp8RtpFmtPacket *packet;
	bctbx_list_t *frame_packets_list = NULL;
	bctbx_list_t *it;
	uint32_t ts;

	/* If we have some packets from the previous iteration, put them in the frame_packets_list. */
	for (it=ctx->non_processed_packets_list; it!=NULL; it=it->next) {
		packet = (Vp8RtpFmtPacket*) it->data;
		frame_packets_list = bctbx_list_append(frame_packets_list, packet);
	}
	bctbx_list_free(ctx->non_processed_packets_list);
	ctx->non_processed_packets_list = NULL;

	/* Process newly received packets. */
	for (it=packets_list; it!=NULL; it=it->next) {
		packet = (Vp8RtpFmtPacket*) it->data;
		ts = mblk_get_timestamp_info(packet->m);

		if ((ctx->initialized_last_ts == TRUE) && (ts != ctx->last_ts)) {
			/* The current packet is from a frame different than the previous one
			* (that apparently is not complete). */
			add_frame(ctx, &frame_packets_list);
		}
		ctx->last_ts = ts;
		ctx->initialized_last_ts = TRUE;

		/* Add the current packet to the current frame. */
		frame_packets_list = bctbx_list_append(frame_packets_list, packet);

		if (mblk_get_marker_info(packet->m)) {
			/* The current packet is the last of the current frame. */
			add_frame(ctx, &frame_packets_list);
		}
	}

	/* If some packets are left, put them in a list for next iteration. */
	ctx->non_processed_packets_list=frame_packets_list;
}

static void output_frame(MSQueue *out, Vp8RtpFmtFrame *frame) {
	Vp8RtpFmtPartition *partition;
	mblk_t *om = NULL;
	mblk_t *curm = NULL;
	int i;

	for (i = 0; i <= frame->partitions_info.nb_partitions; i++) {
		partition = frame->partitions[i];
		if (partition == NULL) continue;
		if (om == NULL) {
			om = concat_packets_of_partition(partition);
			curm = om;
		} else {
			curm = concatb(curm, concat_packets_of_partition(partition));
		}
		partition->outputted = TRUE;
	}
	if (om != NULL) {
		if (om->b_cont) msgpullup(om, -1);
		mblk_set_marker_info(om, 1);
		ms_queue_put(out, (void *)om);
	}
}

static void output_partition(MSQueue *out, Vp8RtpFmtPartition **partition, bool_t last) {
	if (*partition == NULL) {
		*partition = create_empty_partition();
	}
	concat_packets_of_partition(*partition);
	if ((*partition)->has_marker || (last == TRUE)) {
		mblk_set_marker_info((*partition)->m, 1);
	}
	ms_queue_put(out, (void *)(*partition)->m);
	(*partition)->outputted = TRUE;
}

static void output_partitions_of_frame(Vp8RtpFmtUnpackerCtx *ctx, MSQueue *out, Vp8RtpFmtFrame *frame) {
	int i;
	if (frame->unnumbered_partitions == TRUE) {
		output_frame(out, frame);
	} else {
#ifdef VP8RTPFMT_DEBUG
		if (frame->pictureid_present == TRUE) {
			ms_message("VP8 decoder: Output partitions of frame with pictureID=0x%04x", frame->pictureid);
		}
#endif
		for (i = 0; i <= frame->partitions_info.nb_partitions; i++) {
			output_partition(out, &frame->partitions[i], (i == frame->partitions_info.nb_partitions) ? TRUE : FALSE);
		}
	}
}

static int output_valid_partitions(Vp8RtpFmtUnpackerCtx *ctx, MSQueue *out) {
	Vp8RtpFmtFrame *frame;
	size_t nb_frames = bctbx_list_size(ctx->frames_list);

	if (nb_frames == 0) return -1;
	frame = (Vp8RtpFmtFrame *)bctbx_list_nth_data(ctx->frames_list, 0);
	switch (frame->error) {
		case Vp8RtpFmtOk:
			if (frame->keyframe == TRUE) {
				ctx->valid_keyframe_received = TRUE;
				ctx->video_size = get_size_from_key_frame(frame);
				ctx->waiting_for_reference_frame = FALSE;
				if (ctx->error_notified == TRUE) {
					if(ctx->filter) ms_filter_notify_no_arg(ctx->filter, MS_VIDEO_DECODER_RECOVERED_FROM_ERRORS);
					ctx->error_notified = FALSE;
				}
			}
			if ((ctx->avpf_enabled == TRUE) && (frame->reference == TRUE)) {
				ctx->waiting_for_reference_frame = FALSE;
			}
			if ((ctx->valid_keyframe_received == TRUE) && (ctx->waiting_for_reference_frame == FALSE)) {
				/* Output the complete valid frame if a reference frame has been received. */
				if (ctx->output_partitions == TRUE) {
					output_partitions_of_frame(ctx, out, frame);
				} else {
					/* Output the full frame in one mblk_t. */
					output_frame(out, frame);
				}
				frame->outputted = TRUE;
			} else {
				frame->discarded = TRUE;
				if (!ctx->valid_keyframe_received) send_pli(ctx);
				if (ctx->waiting_for_reference_frame == TRUE) {
					/* Do not decode frames while we are waiting for a reference frame. */
					if (frame->pictureid_present == TRUE)
						ms_warning("VP8 decoder: Drop frame because we are waiting for reference frame: pictureID=%i", (int)frame->pictureid);
					else
						ms_warning("VP8 decoder: Drop frame because we are waiting for reference frame.");
				} else {
					/* Drop frames until the first keyframe is successfully received. */
					ms_warning("VP8 frame dropped because keyframe has not been received yet.");
				}
			}
			break;
		case Vp8RtpFmtIncompleteFrame:
			if (frame->keyframe == TRUE) {
				/* Incomplete keyframe. */
				frame->discarded = TRUE;
			} else {
				if ((ctx->output_partitions == TRUE) && (ctx->valid_keyframe_received == TRUE) && (ctx->waiting_for_reference_frame == FALSE)) {
					output_partitions_of_frame(ctx, out, frame);
					frame->outputted = TRUE;
				} else {
					/* Drop the frame for which some partitions are missing/invalid. */
					if (frame->pictureid_present == TRUE)
						ms_warning("VP8 frame with some partitions missing/invalid: pictureID=%i", (int)frame->pictureid);
					else
						ms_warning("VP8 frame with some partitions missing/invalid.");
					frame->discarded = TRUE;
				}
			}
			break;
		default:
			/* Drop the invalid frame. */

			if (frame->pictureid_present == TRUE)
				ms_warning("VP8 invalid frame: pictureID=%i", (int)frame->pictureid);
			else
				ms_warning("VP8 invalid frame.");
			frame->discarded = TRUE;
			break;
	}

	if (frame->outputted == TRUE) return 0;
	return -1;
}

static void free_partitions_of_frame(void *data) {
	Vp8RtpFmtFrame *frame = (Vp8RtpFmtFrame *)data;
	int i;
	for (i = 0; i < 9; i++) {
		if (frame->partitions[i] != NULL) {
			free_partition(frame->partitions[i]);
			frame->partitions[i] = NULL;
		}
	}
}

static void clean_frame(Vp8RtpFmtUnpackerCtx *ctx) {
	if (bctbx_list_size(ctx->frames_list) > 0) {
		Vp8RtpFmtFrame *frame = bctbx_list_nth_data(ctx->frames_list, 0);
		free_partitions_of_frame(frame);
		ctx->frames_list = bctbx_list_remove(ctx->frames_list, frame);
		ms_free(frame);
	}
}

static void free_frame(void *data) {
	Vp8RtpFmtFrame *frame = (Vp8RtpFmtFrame *)data;
	free_partitions_of_frame(frame);
	ms_free(frame);
}

static Vp8RtpFmtErrorCode parse_payload_descriptor(Vp8RtpFmtPacket *packet) {
	uint8_t *h = packet->m->b_rptr;
	Vp8RtpFmtPayloadDescriptor *pd = packet->pd;
	unsigned int packet_size = (unsigned int)(packet->m->b_wptr - packet->m->b_rptr);
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
			if ((unsigned int)(offset + 1) >= packet_size) return Vp8RtpFmtInvalidPayloadDescriptor;
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


void vp8rtpfmt_unpacker_init(Vp8RtpFmtUnpackerCtx *ctx, MSFilter *f, bool_t avpf_enabled, bool_t freeze_on_error, bool_t output_partitions) {
	ctx->filter = f;
	ctx->frames_list = NULL;
	ctx->non_processed_packets_list = NULL;
	ctx->avpf_enabled = avpf_enabled;
	ctx->freeze_on_error = freeze_on_error;
	ctx->output_partitions = output_partitions;
	ctx->valid_keyframe_received = FALSE;
	ctx->waiting_for_reference_frame = TRUE;
	ctx->error_notified = FALSE;
	ctx->initialized_last_ts = FALSE;
	ctx->initialized_ref_cseq = FALSE;
}

void vp8rtpfmt_unpacker_uninit(Vp8RtpFmtUnpackerCtx *ctx) {
	bctbx_list_for_each(ctx->frames_list, free_frame);
	bctbx_list_free(ctx->frames_list);
	bctbx_list_for_each(ctx->non_processed_packets_list, free_packet);
	bctbx_list_free(ctx->non_processed_packets_list);
}

void vp8rtpfmt_unpacker_feed(Vp8RtpFmtUnpackerCtx *ctx, MSQueue *in) {
	bctbx_list_t *packets_list = NULL;
	Vp8RtpFmtPacket *packet;
	mblk_t *m;

#ifdef VP8RTPFMT_DEBUG
	ms_message("vp8rtpfmt_unpacker_feed:");
#endif
	while ((m = ms_queue_get(in)) != NULL) {
		uint16_t cseq=mblk_get_cseq(m);
		packet = ms_new0(Vp8RtpFmtPacket, 1);
		packet->m = m;
		packet->pd = ms_new0(Vp8RtpFmtPayloadDescriptor, 1);

		if (m->b_cont) msgpullup(m, -1);
		packet->error = parse_payload_descriptor(packet);
		if (!ctx->initialized_ref_cseq){
			ctx->initialized_ref_cseq=TRUE;
			ctx->ref_cseq=cseq;
		}else{
			ctx->ref_cseq++;
			if (ctx->ref_cseq!=cseq){
				packet->cseq_inconsistency=TRUE;
				ms_message("sequence inconsistency detected (diff=%i)",(int)(cseq-ctx->ref_cseq));
				ctx->ref_cseq=cseq;
			}
		}
		packets_list=bctbx_list_append(packets_list,packet);
	}
	generate_frames_list(ctx, packets_list);
	bctbx_list_free(packets_list);
#ifdef VP8RTPFMT_DEBUG
	bctbx_list_for_each(ctx->frames_list, print_frame);
#endif /* VP8RTPFMT_DEBUG */
}

int vp8rtpfmt_unpacker_get_frame(Vp8RtpFmtUnpackerCtx *ctx, MSQueue *out, Vp8RtpFmtFrameInfo *frame_info) {
	int ret = output_valid_partitions(ctx, out);
	if (ret == 0) {
		Vp8RtpFmtFrame *frame = (Vp8RtpFmtFrame *)bctbx_list_nth_data(ctx->frames_list, 0);
		frame_info->pictureid_present = frame->pictureid_present;
		frame_info->pictureid = frame->pictureid;
		frame_info->keyframe = frame->keyframe;
	} else if (bctbx_list_size(ctx->non_processed_packets_list) > 0) {
		ms_debug("VP8 packets are remaining for next iteration of the filter.");
	}
	clean_frame(ctx);
	return ret;
}


static void packer_process_frame_part(void *p, void *c) {
	Vp8RtpFmtPacket *packet = (Vp8RtpFmtPacket *)p;
	Vp8RtpFmtPackerCtx *ctx = (Vp8RtpFmtPackerCtx *)c;
	MSFactory* factory = ctx->factory;
	mblk_t *pdm = NULL;
	mblk_t *dm = NULL;
	uint8_t *rptr;
	uint8_t pdsize = 1;
	int max_size = ms_factory_get_payload_max_size(factory);
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

	mblk_set_marker_info(packet->m, FALSE);
	for (rptr = packet->m->b_rptr; rptr < packet->m->b_wptr;) {
		/* Allocate the payload descriptor. */
		pdm = allocb(pdsize, 0);
		memset(pdm->b_wptr, 0, pdsize);
		mblk_set_timestamp_info(pdm, mblk_get_timestamp_info(packet->m));
		mblk_set_marker_info(pdm, FALSE);
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

		dlen = MIN((max_size - pdsize), (int)(packet->m->b_wptr - rptr));
		dm = dupb(packet->m);
		dm->b_rptr = rptr;
		dm->b_wptr = rptr + dlen;
		dm->b_wptr = dm->b_rptr + dlen;
		pdm->b_cont = dm;
		rptr += dlen;

		ms_queue_put(ctx->output_queue, pdm);
	}

	/* Set marker bit on last packet if required. */
	if (pdm != NULL) mblk_set_marker_info(pdm, marker_info);
	if (dm != NULL) mblk_set_marker_info(dm, marker_info);

	freeb(packet->m);
	packet->m = NULL;
}


void vp8rtpfmt_packer_init(Vp8RtpFmtPackerCtx *ctx) {
}

void vp8rtpfmt_packer_uninit(Vp8RtpFmtPackerCtx *ctx) {
}

void vp8rtpfmt_packer_process(Vp8RtpFmtPackerCtx *ctx, bctbx_list_t *in, MSQueue *out, MSFactory *f) {
	ctx->output_queue = out;
	ctx->factory = f;
	bctbx_list_for_each2(in, packer_process_frame_part, ctx);
	bctbx_list_for_each(in, free_packet);
	bctbx_list_free(in);
}

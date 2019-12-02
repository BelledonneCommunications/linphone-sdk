/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef rfc3984_h
#define rfc3984_h

#include <mediastreamer2/mscommon.h>
#include <mediastreamer2/msqueue.h>
#include <mediastreamer2/msfactory.h>
/*
This file declares an API useful to pack/unpack H264 nals as described in RFC3984
It is part of the public API to allow external H264 plugins use this api.
*/

#ifdef __cplusplus
extern "C"{
#endif

typedef enum{
	Rfc3984FrameAvailable = 1,
	Rfc3984FrameCorrupted = 1<<1,
	Rfc3984IsKeyFrame = 1<<2, /*set when a frame has SPS + PPS or IDR (possibly both)*/
	Rfc3984NewSPS = 1<<3,
	Rfc3984NewPPS = 1<<4,
	Rfc3984HasSPS = 1<<5,
	Rfc3984HasPPS = 1<<6,
	Rfc3984HasIDR = 1<<7
}Rfc3984Status;

typedef struct _Rfc3984Context Rfc3984Context;

MS2_PUBLIC Rfc3984Context *rfc3984_new_with_factory(MSFactory *factory);

MS2_PUBLIC void rfc3984_destroy(Rfc3984Context *ctx);


MS2_PUBLIC void rfc3984_set_mode(Rfc3984Context *ctx, int mode);

/* some stupid phones don't decode STAP-A packets ...*/
MS2_PUBLIC void rfc3984_enable_stap_a(Rfc3984Context *ctx, bool_t yesno);

/*process NALUs and pack them into rtp payloads */
MS2_PUBLIC void rfc3984_pack(Rfc3984Context *ctx, MSQueue *naluq, MSQueue *rtpq, uint32_t ts);


MS2_PUBLIC void rfc3984_unpack_out_of_band_sps_pps(Rfc3984Context *ctx, mblk_t *sps, mblk_t *pps);

/**
 * Process incoming rtp data and output NALUs, whenever possible.
 * @param ctx the Rfc3984Context object
 * @param im a new H264 packet to process
 * @param naluq a MSQueue into which a frame ready to be decoded will be output, in the form of a sequence of NAL units.
 * @return a bitmask of Rfc3984Status values.
 * The return value is a bitmask of the #Rfc3984Status enum.
**/
MS2_PUBLIC unsigned int rfc3984_unpack2(Rfc3984Context *ctx, mblk_t *im, MSQueue *naluq);

#ifdef __cplusplus
}
#endif

#endif


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


#include "mediastreamer2/rfc3984.h"
#include "mediastreamer2/msfilter.h"
#include "h264utils.h"


#define TYPE_FU_A 28    /*fragmented unit 0x1C*/
#define TYPE_STAP_A 24  /*single time aggregation packet  0x18*/


static MS2_INLINE void nal_header_init(uint8_t *h, uint8_t nri, uint8_t type){
	*h=((nri&0x3)<<5) | (type & ((1<<5)-1));
}

static MS2_INLINE uint8_t nal_header_get_type(const uint8_t *h){
	return (*h) & ((1<<5)-1);
}

static MS2_INLINE uint8_t nal_header_get_nri(const uint8_t *h){
	return ((*h) >> 5) & 0x3;
}

Rfc3984Context *rfc3984_new(void){
	Rfc3984Context *ctx=ms_new0(Rfc3984Context,1);
	rfc3984_init(ctx);
	return ctx;
}

void rfc3984_destroy(Rfc3984Context *ctx){
	rfc3984_uninit (ctx);
	ms_free(ctx);
}

void rfc3984_init(Rfc3984Context *ctx){
	ms_queue_init(&ctx->q);
	ctx->m=NULL;
	ctx->maxsz=MS_DEFAULT_MAX_PAYLOAD_SIZE;
	ctx->mode=0;
	ctx->status = 0;
	ctx->last_ts=0x943FEA43;/*some random value*/
	ctx->stap_a_allowed=TRUE;
	ctx->last_sps = NULL;
	ctx->last_pps = NULL;
}

void rfc3984_set_max_payload_size(Rfc3984Context *ctx, int size){
	ctx->maxsz=size;
}

static void send_packet(Rfc3984Context *ctx, MSQueue *rtpq, uint32_t ts, mblk_t *m, bool_t marker){
	mblk_set_timestamp_info(m,ts);
	mblk_set_marker_info(m,marker);
	mblk_set_cseq(m, ctx->ref_cseq++);
	ms_queue_put(rtpq,m);
}

static void put_nal_size(mblk_t *m, size_t sz){
	uint16_t size=htons((uint16_t)sz);
	*(uint16_t*)m->b_wptr=size;
	m->b_wptr+=2;
}

static mblk_t * prepend_stapa(mblk_t *m){
	mblk_t *hm=allocb(3,0);
	nal_header_init(hm->b_wptr,nal_header_get_nri(m->b_rptr),TYPE_STAP_A);
	hm->b_wptr+=1;
	put_nal_size(hm,msgdsize(m));
	hm->b_cont=m;
	return hm;
}

static mblk_t * concat_nalus(mblk_t *m1, mblk_t *m2){
	mblk_t *l=allocb(2,0);
	/*eventually append a stap-A header to m1, if not already done*/
	if (nal_header_get_type(m1->b_rptr)!=TYPE_STAP_A){
		m1=prepend_stapa(m1);
	}
	put_nal_size(l,msgdsize(m2));
	l->b_cont=m2;
	concatb(m1,l);
	return m1;
}

static mblk_t *prepend_fu_indicator_and_header(mblk_t *m, uint8_t indicator,
	bool_t start, bool_t end, uint8_t type){
	mblk_t *h=allocb(2,0);
	h->b_wptr[0]=indicator;
	h->b_wptr[1]=((start&0x1)<<7)|((end&0x1)<<6)|type;
	h->b_wptr+=2;
	h->b_cont=m;
	if (start) m->b_rptr++;/*skip original nalu header */
	return h;
}

static void frag_nalu_and_send(Rfc3984Context *ctx, MSQueue *rtpq, uint32_t ts, mblk_t *nalu, bool_t marker, int maxsize){
	mblk_t *m;
	int payload_max_size=maxsize-2;/*minus FUA header*/
	uint8_t fu_indicator;
	uint8_t type=nal_header_get_type(nalu->b_rptr);
	uint8_t nri=nal_header_get_nri(nalu->b_rptr);
	bool_t start=TRUE;

	nal_header_init(&fu_indicator,nri,TYPE_FU_A);
	while(nalu->b_wptr-nalu->b_rptr>payload_max_size){
		m=dupb(nalu);
		nalu->b_rptr+=payload_max_size;
		m->b_wptr=nalu->b_rptr;
		m=prepend_fu_indicator_and_header(m,fu_indicator,start,FALSE,type);
		send_packet(ctx, rtpq,ts,m,FALSE);
		start=FALSE;
	}
	/*send last packet */
	m=prepend_fu_indicator_and_header(nalu,fu_indicator,FALSE,TRUE,type);
	send_packet(ctx, rtpq,ts,m,marker);
}

static void rfc3984_pack_mode_0(Rfc3984Context *ctx, MSQueue *naluq, MSQueue *rtpq, uint32_t ts){
	mblk_t *m;
	bool_t end;
	int size;
	while((m=ms_queue_get(naluq))!=NULL){
		end=ms_queue_empty(naluq);
		size=(int)(m->b_wptr-m->b_rptr);
		if (size>ctx->maxsz){
			ms_warning("This H264 packet does not fit into mtu: size=%i",size);
		}
		send_packet(ctx, rtpq,ts,m,end);
	}
}

/*process NALUs and pack them into rtp payloads */
static void rfc3984_pack_mode_1(Rfc3984Context *ctx, MSQueue *naluq, MSQueue *rtpq, uint32_t ts){
	mblk_t *m,*prevm=NULL;
	int prevsz=0,sz;
	bool_t end;
	while((m=ms_queue_get(naluq))!=NULL){
		end=ms_queue_empty(naluq);
		sz=(int)(m->b_wptr-m->b_rptr);
		if (ctx->stap_a_allowed){
			if (prevm!=NULL){
				if ((prevsz+sz)<(ctx->maxsz-2)){
					prevm=concat_nalus(prevm,m);
					m=NULL;
					prevsz+=sz+2;/*+2 for the stapa size field*/
					continue;
				}else{
					/*send prevm packet: either single nal or STAP-A*/
					if (prevm->b_cont!=NULL){
						ms_debug("Sending STAP-A");
					}else {
						ms_debug("Sending previous msg as single NAL");
					}
					send_packet(ctx, rtpq,ts,prevm,FALSE);
					prevm=NULL;
					prevsz=0;
				}
			}
			if (sz<(ctx->maxsz/2)){
				/*try to aggregate it with next packet*/
				prevm=m;
				prevsz=sz+3; /*STAP-A header + size*/
				m=NULL;
			}else{
				
				/*send as single nal or FU-A*/
				if (sz>ctx->maxsz){
					ms_debug("Sending FU-A packets");
					frag_nalu_and_send(ctx, rtpq,ts,m,end, ctx->maxsz);
				}else{
					ms_debug("Sending Single NAL");
					send_packet(ctx, rtpq,ts,m,end);
				}
			}
		}else{
			if (sz>ctx->maxsz){
				ms_debug("Sending FU-A packets");
				frag_nalu_and_send(ctx, rtpq,ts,m,end, ctx->maxsz);
			}else{
				ms_debug("Sending Single NAL");
				send_packet(ctx, rtpq,ts,m,end);
			}
		}
	}
	if (prevm){
		ms_debug("Sending Single NAL (2)");
		send_packet(ctx, rtpq,ts,prevm,TRUE);
	}
}

static mblk_t * aggregate_fua(Rfc3984Context *ctx, mblk_t *im){
	mblk_t *om=NULL;
	uint8_t fu_header;
	uint8_t nri,type;
	bool_t start,end;
	fu_header=im->b_rptr[1];
	type=nal_header_get_type(&fu_header);
	start=fu_header>>7;
	end=(fu_header>>6)&0x1;
	if (start){
		mblk_t *new_header;
		nri=nal_header_get_nri(im->b_rptr);
		if (ctx->m!=NULL){
			ms_error("receiving FU-A start while previous FU-A is not "
				"finished");
			freemsg(ctx->m);
			ctx->m=NULL;
		}
		im->b_rptr+=2; /*skip the nal header and the fu header*/
		new_header=allocb(1,0); /* allocate small fragment to put the correct nal header, this is to avoid to write on the buffer
					which can break processing of other users of the buffers */
		nal_header_init(new_header->b_wptr,nri,type);
		new_header->b_wptr++;
		mblk_meta_copy(im,new_header);
		concatb(new_header,im);
		ctx->m=new_header;
	}else{
		if (ctx->m!=NULL){
			im->b_rptr+=2;
			concatb(ctx->m,im);
		}else{
			ms_error("Receiving continuation FU packet but no start.");
			freemsg(im);
		}
	}
	if (end && ctx->m){
		msgpullup(ctx->m,-1);
		om=ctx->m;
		ctx->m=NULL;
	}
	return om;
}

void rfc3984_unpack_out_of_band_sps_pps(Rfc3984Context *ctx, mblk_t *sps, mblk_t *pps){
	if (ctx->sps){
		freemsg(ctx->sps);
	}
	if (ctx->pps){
		freemsg(ctx->pps);
	}
	ctx->sps = sps;
	ctx->pps = pps;
}

int rfc3984_unpack(Rfc3984Context *ctx, mblk_t *im, MSQueue *out){
	if (rfc3984_unpack2(ctx, im, out) & Rfc3984FrameCorrupted) return -1;
	return 0;
}


static unsigned int output_frame(Rfc3984Context * ctx, MSQueue *out, unsigned int flags){
	unsigned int res = ctx->status;
	
	if (!ms_queue_empty(out)){
		ms_warning("rfc3984_unpack: output_frame invoked several times in a row, this should not happen");
	}
	res |= flags;
	if ((res & Rfc3984IsKeyFrame) && ctx->sps && ctx->pps){
		/*prepend out of band provided sps and pps*/
		ms_queue_put(out, ctx->sps);
		ctx->sps = NULL;
		ms_queue_put(out, ctx->pps);
		ctx->pps = NULL;
	}
	
	/* Log some bizarre things */
	if ((res & Rfc3984FrameCorrupted) == 0){
		if ((res & Rfc3984HasSPS) && (res & Rfc3984HasPPS) && !(res & Rfc3984HasIDR)){
			/*some decoders may not be happy with this*/
			ms_warning("rfc3984_unpack: a frame with SPS+PPS but no IDR was output.");
		}
	}
	
	while(!ms_queue_empty(&ctx->q)){
		ms_queue_put(out,ms_queue_get(&ctx->q));
	}
	
	ctx->status = 0;
	return res;
}

static bool_t update_parameter_set(mblk_t **last_parameter_set, mblk_t *new_parameter_set) {
	if (*last_parameter_set != NULL) {
		size_t last_size = (*last_parameter_set)->b_wptr - (*last_parameter_set)->b_rptr;
		size_t new_size = new_parameter_set->b_wptr - new_parameter_set->b_rptr;
		if (last_size != new_size || memcmp((*last_parameter_set)->b_rptr, new_parameter_set->b_rptr, new_size) != 0) {
			freemsg(*last_parameter_set);
			*last_parameter_set = dupmsg(new_parameter_set);
			return TRUE;
		} else {
			return FALSE;
		}
	} else {
		*last_parameter_set = dupmsg(new_parameter_set);
		return TRUE;
	}
}

static void store_nal(Rfc3984Context *ctx, mblk_t *nal){
	uint8_t type=nal_header_get_type(nal->b_rptr);
	if (ms_queue_empty(&ctx->q) && (ctx->status & Rfc3984FrameCorrupted)
		&& (type == MSH264NaluTypeSPS || type == MSH264NaluTypePPS || type == MSH264NaluTypeIDR)){
		ms_message("Previous discontinuity ignored since we are restarting with a keyframe.");
		ctx->status = 0;
	}
	if (type == MSH264NaluTypeIDR){
		ctx->status |= Rfc3984HasIDR;
		ctx->status |= Rfc3984IsKeyFrame;
	}else if (type == MSH264NaluTypeSPS){
		ctx->status |= Rfc3984HasSPS;
		if (update_parameter_set(&ctx->last_sps, nal)) {
			ctx->status |= Rfc3984NewSPS;
		}
	}
	else if (type == MSH264NaluTypePPS){
		ctx->status |= Rfc3984HasPPS;
		if (update_parameter_set(&ctx->last_pps, nal)) {
			ctx->status |= Rfc3984NewPPS;
		}
	}
	ms_queue_put(&ctx->q,nal);
}

unsigned int rfc3984_unpack2(Rfc3984Context *ctx, mblk_t *im, MSQueue *out){
	uint8_t type=nal_header_get_type(im->b_rptr);
	uint8_t *p;
	int marker = mblk_get_marker_info(im);
	uint32_t ts=mblk_get_timestamp_info(im);
	uint16_t cseq=mblk_get_cseq(im);
	unsigned int ret = 0;

	if (ctx->last_ts!=ts){
		/*a new frame is arriving, in case the marker bit was not set in previous frame, output it now,
		 unless it is a FU-A packet (workaround for buggy implementations)*/
		ctx->last_ts=ts;
		if (ctx->m==NULL && !ms_queue_empty(&ctx->q)){
			ret = output_frame(ctx, out, Rfc3984FrameAvailable | Rfc3984FrameCorrupted);
			ms_warning("Incomplete H264 frame (missing marker bit)");
		}
	}

	if (im->b_cont) msgpullup(im,-1);

	if (!ctx->initialized_ref_cseq) {
		ctx->initialized_ref_cseq = TRUE;
		ctx->ref_cseq = cseq;
	} else {
		ctx->ref_cseq++;
		if (ctx->ref_cseq != cseq) {
			ms_message("sequence inconsistency detected (diff=%i)",(int)(cseq - ctx->ref_cseq));
			ctx->ref_cseq = cseq;
			ctx->status |= Rfc3984FrameCorrupted;
		}
	}

	if (type==TYPE_STAP_A){
		/*split into nalus*/
		uint16_t sz;
		uint8_t *buf=(uint8_t*)&sz;
		mblk_t *nal;
	
		ms_debug("Receiving STAP-A");
		for(p=im->b_rptr+1;p<im->b_wptr;){
			buf[0]=p[0];
			buf[1]=p[1];
			sz=ntohs(sz);
			nal=dupb(im);
			p+=2;
			nal->b_rptr=p;
			p+=sz;
			nal->b_wptr=p;
			if (p>im->b_wptr){
				ms_error("Malformed STAP-A packet");
				freemsg(nal);
				break;
			}
			store_nal(ctx, nal);
		}
		freemsg(im);
	}else if (type==TYPE_FU_A){
		mblk_t *o=aggregate_fua(ctx,im);
		ms_debug("Receiving FU-A");
		if (o) store_nal(ctx, o);
	}else{
		if (ctx->m){
			/*discontinued FU-A, purge it*/
			freemsg(ctx->m);
			ctx->m=NULL;
		}
		/*single nal unit*/
		ms_debug("Receiving single NAL");
		store_nal(ctx, im);
	}

	if (marker){
		ctx->last_ts=ts;
		ms_debug("Marker bit set");
		ret = output_frame(ctx, out, Rfc3984FrameAvailable);
	}

	return ret;
}


void rfc3984_pack(Rfc3984Context *ctx, MSQueue *naluq, MSQueue *rtpq, uint32_t ts){
	switch(ctx->mode){
		case 0:
			rfc3984_pack_mode_0(ctx,naluq,rtpq,ts);
			break;
		case 1:
			rfc3984_pack_mode_1(ctx,naluq,rtpq,ts);
			break;
		default:
			ms_error("Bad or unsupported mode %i",ctx->mode);
	}
}

void rfc3984_uninit(Rfc3984Context *ctx){
	ms_queue_flush(&ctx->q);
	if (ctx->m != NULL) freemsg(ctx->m);
	if (ctx->sps != NULL) freemsg(ctx->sps);
	if (ctx->pps != NULL) freemsg(ctx->pps);
	ctx->m = NULL;
	ctx->sps = NULL;
	ctx->pps = NULL;
}

void rfc3984_set_mode(Rfc3984Context *ctx, int mode){
	ctx->mode=mode;
}

void rfc3984_enable_stap_a(Rfc3984Context *ctx, bool_t yesno){
	ctx->stap_a_allowed=yesno;
}

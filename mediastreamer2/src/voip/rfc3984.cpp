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


#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/rfc3984.h"

#include "h264utils.h"
#include "rfc3984.hpp"


namespace mediastreamer2 {

Rfc3984Context::Rfc3984Context() {
	ms_queue_init(&this->q);
}

Rfc3984Context::Rfc3984Context::Rfc3984Context(MSFactory* factory) {
	ms_queue_init(&this->q);
	setMaxPayloadSize(ms_factory_get_payload_max_size(factory));
}


Rfc3984Context::~Rfc3984Context() {
	ms_queue_flush(&this->q);
	if (this->m != nullptr) freemsg(this->m);
	if (this->sps != nullptr) freemsg(this->sps);
	if (this->pps != nullptr) freemsg(this->pps);
	if (this->last_pps != nullptr) freemsg(this->last_pps);
	if (this->last_sps != nullptr) freemsg(this->last_sps);
}

void Rfc3984Context::setOutOfBandSpsPps(mblk_t *sps, mblk_t *pps){
	if (this->sps){
		freemsg(this->sps);
	}
	if (this->pps){
		freemsg(this->pps);
	}
	this->sps = sps;
	this->pps = pps;
}

void Rfc3984Context::pack(MSQueue *naluq, MSQueue *rtpq, uint32_t ts) {
	switch(this->mode){
		case 0:
			packMode0(naluq, rtpq, ts);
			break;
		case 1:
			packMode1(naluq, rtpq, ts);
			break;
		default:
			ms_error("Bad or unsupported mode %i",this->mode);
	}
}

unsigned int Rfc3984Context::unpack(mblk_t *im, MSQueue *out) {
	uint8_t type=ms_h264_nalu_get_type(im);
	uint8_t *p;
	int marker = mblk_get_marker_info(im);
	uint32_t ts=mblk_get_timestamp_info(im);
	uint16_t cseq=mblk_get_cseq(im);
	unsigned int ret = 0;

	//ms_message("Seeing timestamp %u, sequence %u", ts, (int)cseq);

	if (this->last_ts!=ts){
		/*a new frame is arriving, in case the marker bit was not set in previous frame, output it now,
		 *	 unless it is a FU-A packet (workaround for buggy implementations)*/
		this->last_ts=ts;
		if (this->m==NULL && !ms_queue_empty(&this->q)){
			ret = outputFrame(out, ((unsigned int)Status::FrameAvailable) | (unsigned int)Status::FrameCorrupted);
			ms_warning("Incomplete H264 frame (missing marker bit after seq number %u)",
					   mblk_get_cseq(ms_queue_peek_last(out)));
		}
	}

	if (im->b_cont) msgpullup(im,-1);

	if (!this->initialized_ref_cseq) {
		this->initialized_ref_cseq = TRUE;
		this->ref_cseq = cseq;
	} else {
		this->ref_cseq++;
		if (this->ref_cseq != cseq) {
			ms_message("sequence inconsistency detected (diff=%i)",(int)(cseq - this->ref_cseq));
			this->ref_cseq = cseq;
			this->status |= (unsigned int)Status::FrameCorrupted;
		}
	}

	if (type==MSH264NaluTypeSTAPA){
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
			storeNal(nal);
		}
		freemsg(im);
	}else if (type==MSH264NaluTypeFUA){
		mblk_t *o=aggregateFUA(im);
		ms_debug("Receiving FU-A");
		if (o) storeNal(o);
	}else{
		if (this->m){
			/*discontinued FU-A, purge it*/
			freemsg(this->m);
			this->m=NULL;
		}
		/*single nal unit*/
		ms_debug("Receiving single NAL");
		storeNal(im);
	}

	if (marker){
		this->last_ts=ts;
		ms_debug("Marker bit set");
		ret = outputFrame(out, (unsigned int)Status::FrameAvailable);
	}

	return ret;
}

// Private methods
void Rfc3984Context::packMode0(MSQueue *naluq, MSQueue *rtpq, uint32_t ts) {
	mblk_t *m;
	bool_t end;
	int size;
	while((m=ms_queue_get(naluq))!=NULL){
		end=ms_queue_empty(naluq);
		size=(int)(m->b_wptr-m->b_rptr);
		if (size>this->maxsz){
			ms_warning("This H264 packet does not fit into mtu: size=%i",size);
		}
		sendPacket(rtpq,ts,m,end);
	}
}

void Rfc3984Context::packMode1(MSQueue *naluq, MSQueue *rtpq, uint32_t ts){
	mblk_t *m,*prevm=NULL;
	int prevsz=0,sz;
	bool_t end;
	while((m=ms_queue_get(naluq))!=NULL){
		end=ms_queue_empty(naluq);
		sz=(int)(m->b_wptr-m->b_rptr);
		if (this->stap_a_allowed){
			if (prevm!=NULL){
				if ((prevsz+sz)<(this->maxsz-2)){
					prevm=concatNalus(prevm,m);
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
					sendPacket(rtpq,ts,prevm,FALSE);
					prevm=NULL;
					prevsz=0;
				}
			}
			if (sz<(this->maxsz/2)){
				/*try to aggregate it with next packet*/
				prevm=m;
				prevsz=sz+3; /*STAP-A header + size*/
				m=NULL;
			}else{

				/*send as single nal or FU-A*/
				if (sz>this->maxsz){
					ms_debug("Sending FU-A packets");
					fragNaluAndSend(rtpq,ts,m,end, this->maxsz);
				}else{
					ms_debug("Sending Single NAL");
					sendPacket(rtpq,ts,m,end);
				}
			}
		}else{
			if (sz>this->maxsz){
				ms_debug("Sending FU-A packets");
				fragNaluAndSend(rtpq,ts,m,end, this->maxsz);
			}else{
				ms_debug("Sending Single NAL");
				sendPacket(rtpq,ts,m,end);
			}
		}
	}
	if (prevm){
		ms_debug("Sending Single NAL (2)");
		sendPacket(rtpq,ts,prevm,TRUE);
	}
}

void Rfc3984Context::fragNaluAndSend(MSQueue *rtpq, uint32_t ts, mblk_t *nalu, bool_t marker, int maxsize){
	mblk_t *m;
	int payload_max_size=maxsize-2;/*minus FUA header*/
	uint8_t fu_indicator;
	uint8_t type=ms_h264_nalu_get_type(nalu);
	uint8_t nri=nalHeaderGetNri(nalu->b_rptr);
	bool_t start=TRUE;

	nalHeaderInit(&fu_indicator,nri,MSH264NaluTypeFUA);
	while(nalu->b_wptr-nalu->b_rptr>payload_max_size){
		m=dupb(nalu);
		nalu->b_rptr+=payload_max_size;
		m->b_wptr=nalu->b_rptr;
		m=prependFuIndicatorAndHeader(m,fu_indicator,start,FALSE,type);
		sendPacket(rtpq,ts,m,FALSE);
		start=FALSE;
	}
	/*send last packet */
	m=prependFuIndicatorAndHeader(nalu,fu_indicator,FALSE,TRUE,type);
	sendPacket(rtpq,ts,m,marker);
}

void Rfc3984Context::sendPacket(MSQueue *rtpq, uint32_t ts, mblk_t *m, bool_t marker){
	mblk_set_timestamp_info(m,ts);
	mblk_set_marker_info(m,marker);
	mblk_set_cseq(m, this->ref_cseq++);
	ms_queue_put(rtpq,m);
}

unsigned int Rfc3984Context::outputFrame(MSQueue *out, unsigned int flags){
	unsigned int res = this->status;

	if (!ms_queue_empty(out)){
		ms_warning("rfc3984_unpack: output_frame invoked several times in a row, this should not happen");
	}
	res |= flags;
	if ((res & Status::IsKeyFrame) && this->sps && this->pps){
		/*prepend out of band provided sps and pps*/
		ms_queue_put(out, this->sps);
		this->sps = NULL;
		ms_queue_put(out, this->pps);
		this->pps = NULL;
	}

	/* Log some bizarre things */
	if ((res & Status::FrameCorrupted) == 0){
		if ((res & Status::HasSPS) && (res & Status::HasPPS) && !(res & Status::HasIDR) && !(res & Status::IsKeyFrame)){
			/*some decoders may not be happy with this*/
			ms_warning("rfc3984_unpack: a frame with SPS+PPS but no IDR was output, starting at seq number %u",
					   mblk_get_cseq(ms_queue_peek_first(&this->q)));
		}
	}

	while(!ms_queue_empty(&this->q)){
		ms_queue_put(out,ms_queue_get(&this->q));
	}

	this->status = 0;
	return res;
}

void Rfc3984Context::storeNal(mblk_t *nal) {
	uint8_t type=ms_h264_nalu_get_type(nal);

	if ((this->status & Status::HasSPS) && (this->status & Status::HasPPS) && type != MSH264NaluTypeIDR && mblk_get_marker_info(nal) && isUniqueISlice(nal->b_rptr+1)){
		ms_warning("Receiving a nal unit which is not IDR but a single I-slice bundled with SPS & PPS - considering it as a key frame.");
		this->status |= Status::IsKeyFrame;
	}

	if (type == MSH264NaluTypeIDR){
		this->status |= Status::HasIDR;
		this->status |= Status::IsKeyFrame;
	}else if (type == MSH264NaluTypeSPS){
		this->status |= Status::HasSPS;
		if (updateParameterSet(&this->last_sps, nal)) {
			this->status |= Status::NewSPS;
		}
	}
	else if (type == MSH264NaluTypePPS){
		this->status |= Status::HasPPS;
		if (updateParameterSet(&this->last_pps, nal)) {
			this->status |= Status::NewPPS;
		}
	}
	ms_queue_put(&this->q,nal);
}

mblk_t *Rfc3984Context::aggregateFUA(mblk_t *im){
	mblk_t *om=NULL;
	uint8_t fu_header;
	uint8_t nri,type;
	bool_t start,end;
	bool_t marker = mblk_get_marker_info(im);

	fu_header=im->b_rptr[1];
	type=ms_h264_nalu_get_type(im);
	start=fu_header>>7;
	end=(fu_header>>6)&0x1;
	if (start){
		mblk_t *new_header;
		nri=nalHeaderGetNri(im->b_rptr);
		if (this->m!=NULL){
			ms_error("receiving FU-A start while previous FU-A is not "
			"finished");
			freemsg(this->m);
			this->m=NULL;
		}
		im->b_rptr+=2; /*skip the nal header and the fu header*/
		new_header=allocb(1,0); /* allocate small fragment to put the correct nal header, this is to avoid to write on the buffer
		which can break processing of other users of the buffers */
		nalHeaderInit(new_header->b_wptr,nri,type);
		new_header->b_wptr++;
		mblk_meta_copy(im,new_header);
		concatb(new_header,im);
		this->m=new_header;
	}else{
		if (this->m!=NULL){
			im->b_rptr+=2;
			concatb(this->m,im);
		}else{
			ms_error("Receiving continuation FU packet but no start.");
			freemsg(im);
		}
	}
	if (end && this->m){
		msgpullup(this->m,-1);
		om=this->m;
		mblk_set_marker_info(om, marker); /*set the marker bit of this aggregated NAL as the last fragment received.*/
		this->m=NULL;
	}
	return om;
}

// Private static methods
mblk_t *Rfc3984Context::concatNalus(mblk_t *m1, mblk_t *m2){
	mblk_t *l=allocb(2,0);
	/*eventually append a stap-A header to m1, if not already done*/
	if (ms_h264_nalu_get_type(m1)!=MSH264NaluTypeSTAPA){
		m1=prependStapA(m1);
	}
	putNalSize(l,msgdsize(m2));
	l->b_cont=m2;
	concatb(m1,l);
	return m1;
}

mblk_t *Rfc3984Context::prependStapA(mblk_t *m){
	mblk_t *hm=allocb(3,0);
	nalHeaderInit(hm->b_wptr,nalHeaderGetNri(m->b_rptr),MSH264NaluTypeSTAPA);
	hm->b_wptr+=1;
	putNalSize(hm,msgdsize(m));
	hm->b_cont=m;
	return hm;
}

void Rfc3984Context::putNalSize(mblk_t *m, size_t sz){
	uint16_t size=htons((uint16_t)sz);
	*(uint16_t*)m->b_wptr=size;
	m->b_wptr+=2;
}

mblk_t *Rfc3984Context::prependFuIndicatorAndHeader(mblk_t *m, uint8_t indicator, bool_t start, bool_t end, uint8_t type){
	mblk_t *h=allocb(2,0);
	h->b_wptr[0]=indicator;
	h->b_wptr[1]=((start&0x1)<<7)|((end&0x1)<<6)|type;
	h->b_wptr+=2;
	h->b_cont=m;
	if (start) m->b_rptr++;/*skip original nalu header */
		return h;
}

int Rfc3984Context::isUniqueISlice(const uint8_t *slice_header){
	ms_message("is_unique_I_slice: %i", (int)*slice_header);
	return slice_header[0] == 0x88; /*this corresponds to first_mb_in_slice to zero and slice_type = 7*/
}

bool_t Rfc3984Context::updateParameterSet(mblk_t **last_parameter_set, mblk_t *new_parameter_set) {
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

// Global public functions
unsigned int operator&(unsigned int val1, Rfc3984Context::Status val2) {
	return val1 & static_cast<unsigned int>(val2);
}

unsigned int &operator&=(unsigned int &val1, Rfc3984Context::Status val2) {
	return val1 &= static_cast<unsigned int>(val2);
}
unsigned int operator|(unsigned int val1, Rfc3984Context::Status val2) {
	return val1 | static_cast<unsigned int>(val2);
}
unsigned int &operator|=(unsigned int &val1, Rfc3984Context::Status val2) {
	return val1 |= static_cast<unsigned int>(val2);
}

};


//==================================================
// C wrapper implementation
//==================================================

extern "C" {

Rfc3984Context *rfc3984_new(void) {
	return reinterpret_cast<Rfc3984Context *>(new mediastreamer2::Rfc3984Context());
}

Rfc3984Context *rfc3984_new_with_factory(MSFactory *factory) {
	return reinterpret_cast<Rfc3984Context *>(new mediastreamer2::Rfc3984Context(factory));
}

void rfc3984_destroy(Rfc3984Context *ctx) {
	delete reinterpret_cast<mediastreamer2::Rfc3984Context *>(ctx);
}

void rfc3984_set_mode(Rfc3984Context *ctx, int mode) {
	reinterpret_cast<mediastreamer2::Rfc3984Context *>(ctx)->setMode(mode);
}

void rfc3984_enable_stap_a(Rfc3984Context *ctx, bool_t yesno) {
	reinterpret_cast<mediastreamer2::Rfc3984Context *>(ctx)->enableStapA(yesno);
}

void rfc3984_pack(Rfc3984Context *ctx, MSQueue *naluq, MSQueue *rtpq, uint32_t ts) {
	reinterpret_cast<mediastreamer2::Rfc3984Context *>(ctx)->pack(naluq, rtpq, ts);
}

void rfc3984_unpack_out_of_band_sps_pps(Rfc3984Context *ctx, mblk_t *sps, mblk_t *pps) {
	reinterpret_cast<mediastreamer2::Rfc3984Context *>(ctx)->setOutOfBandSpsPps(sps, pps);
}

unsigned int rfc3984_unpack2(Rfc3984Context *ctx, mblk_t *im, MSQueue *naluq) {
	return reinterpret_cast<mediastreamer2::Rfc3984Context *>(ctx)->unpack(im, naluq);
}

}

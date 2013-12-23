
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

#include "ffmpeg-priv.h"


#ifndef HAVE_FUN_avcodec_encode_video2
int avcodec_encode_video2 (AVCodecContext *avctx, AVPacket *avpkt, const AVFrame *frame, int *got_packet_ptr) {
	int error=avcodec_encode_video(avctx, avpkt->data, avpkt->size,frame);
	if (error<0){
		return error;
	}else{
		if (error>0) {
			*got_packet_ptr=1;
			avpkt->size=error;
		}else *got_packet_ptr=0;
	}
	return 0;
}
#endif



#ifndef HAVE_FUN_avcodec_get_context_defaults3 /**/
int avcodec_get_context_defaults3 (AVCodecContext *s, const AVCodec *codec) {
	avcodec_get_context_defaults(s);
	return 0;
}

AVCodecContext *avcodec_alloc_context3(AVCodec *codec){
	return avcodec_alloc_context();
}

#endif



#ifndef HAVE_FUN_avcodec_open2 /**/
int avcodec_open2 (AVCodecContext *avctx, const AVCodec *codec, /*AVDictionary*/ void **options) {
	return avcodec_open(avctx, (AVCodec*)codec);
}
#endif
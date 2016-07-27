
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

#ifndef HAVE_FUN_av_frame_alloc
AVFrame* av_frame_alloc (void) {
    return avcodec_alloc_frame();
}
#endif

#ifndef HAVE_FUN_av_frame_free
void av_frame_free (AVFrame** frame) {
/*
 From http://git.videolan.org/?p=ffmpeg.git;a=blob;f=doc/APIchanges
 2012-09-24 - 46a3595 / a42aada - lavc 54.59.100 / 54.28.0 - avcodec.h
    Add avcodec_free_frame(). This function must now
    be used for freeing an AVFrame.
*/
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(54,28,0)
    avcodec_free_frame(frame);
#else
    av_free(*frame);
#endif
}
#endif

#ifndef HAVE_FUN_av_frame_unref
void av_frame_unref (AVFrame *frame) {
    avcodec_get_frame_defaults(frame);
}
#endif

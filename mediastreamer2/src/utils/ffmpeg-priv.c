
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

#ifdef FF_API_ALLOC_CONTEXT
#if !FF_API_ALLOC_CONTEXT
AVCodecContext *avcodec_alloc_context(void) {
	return avcodec_alloc_context3(NULL);
}
void avcodec_get_context_defaults(AVCodecContext *s) {
	avcodec_get_context_defaults3(s, NULL);
}
#endif
#endif
#ifdef FF_API_AVCODEC_OPEN
#if !FF_API_AVCODEC_OPEN
int avcodec_open(AVCodecContext *avctx, AVCodec *codec) {
	return avcodec_open2(avctx, codec, NULL);
}
#endif
#endif

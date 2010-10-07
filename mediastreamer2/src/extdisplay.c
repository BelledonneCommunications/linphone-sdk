/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2010  Belledonne Communications SARL <simon.morlat@linphone.org>

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


#include "mediastreamer2/msextdisplay.h"

static void ext_display_init(MSFilter *f){
	ms_filter_enable_synchronous_notifcations (f,TRUE);
}

static void ext_display_uninit(MSFilter *f){
}

static void ext_display_process(MSFilter *f){
	MSExtDisplayOutput output;
	mblk_t *main_im=NULL;
	mblk_t *local_im=NULL;

	memset(&output,0,sizeof(output));
	
	/*get most recent message and draw it*/
	if ( f->inputs[1]!=NULL && (local_im=ms_queue_peek_last(f->inputs[1]))!=NULL) {
		if (ms_yuv_buf_init_from_mblk(&output.local_view,local_im)==0){
		}
	}
	
	if (f->inputs[0]!=NULL && (main_im=ms_queue_peek_last(f->inputs[0]))!=NULL) {
		if (ms_yuv_buf_init_from_mblk(&output.remote_view,main_im)==0){
		}
	}

	ms_filter_notify(f,MS_EXT_DISPLAY_ON_DRAW,&output);
	
	if (f->inputs[0]!=NULL)
		ms_queue_flush(f->inputs[0]);
	if (f->inputs[1]!=NULL)
		ms_queue_flush(f->inputs[1]);
}


#ifdef _MSC_VER

MSFilterDesc ms_ext_display_desc={
	MS_EXT_DISPLAY_ID,
	"MSExtDisplay",
	N_("A display filter sending the buffers to draw to the upper layer"),
	MS_FILTER_OTHER,
	NULL,
	2,
	0,
	ext_display_init,
	NULL,
	ext_display_process,
	NULL,
	ext_display_uninit,
};

#else

MSFilterDesc ms_ext_display_desc={
	.id=MS_EXT_DISPLAY_ID,
	.name="MSExtDisplay",
	.text=N_("A display filter sending the buffers to draw to the upper layer"),
	.category=MS_FILTER_OTHER,
	.ninputs=2,
	.noutputs=0,
	.init=ext_display_init,
	.process=ext_display_process,
	.uninit=ext_display_uninit,
};

#endif

MS_FILTER_DESC_EXPORT(ms_ext_display_desc)

/*
 * Copyright (c) 2010-2020 Belledonne Communications SARL.
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

#if defined(HAVE_CONFIG_H)
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/msanalysedisplay.h"

typedef struct _MSAnalyseDisplayData{
	MSPicture pict;
	int color;
}MSAnalyseDisplayData;

static void analyse_display_init(MSFilter *f){
	MSAnalyseDisplayData *d=ms_new0(MSAnalyseDisplayData,1);
	d->color = -1;
	f->data = d;
}

static void analyse_display_uninit(MSFilter *f){
	ms_free(f->data);
}

static void analyse_display_process(MSFilter *f){
	ms_filter_lock(f);
	MSAnalyseDisplayData* d = (MSAnalyseDisplayData*)f->data;
	mblk_t* m = 0;
	if (f->inputs[0]!=NULL && (m=ms_queue_peek_last(f->inputs[0]))!=NULL) {
		if (ms_yuv_buf_init_from_mblk(&d->pict,m)==0){
			d->color = d->pict.planes[0][0];
		}
	}

	if (f->inputs[0]!=NULL)
		ms_queue_flush(f->inputs[0]);
	if (f->inputs[1]!=NULL)
		ms_queue_flush(f->inputs[1]);
	ms_filter_unlock(f);
}

static int analysedisplay_compare_color(MSFilter *f, void* data) {
	MSMireControl c = *(MSMireControl *)data;
	ms_filter_lock(f);
	MSAnalyseDisplayData* d = (MSAnalyseDisplayData*)f->data;
	ms_message("[MSAnalyseDisplay] compare with source color %d",d->color);
	if (d->color < 0) return -1;
	for (int i=1;i<6;i++) {
		ms_message("[MSAnalyseDisplay] color %d",c.colors[i]);
		if ((c.colors[i]-2) <= d->color && d->color <= (c.colors[i]+2)) {
			ms_filter_unlock(f);
			return 0;
		}
	}
	ms_filter_unlock(f);
	return -1;
}

static MSFilterMethod analysedisplay_methods[] = {
	{ MS_ANALYSE_DISPLAY_COMPARE_COLOR, analysedisplay_compare_color},
	{ 0, NULL }
};

#ifdef _MSC_VER

MSFilterDesc ms_analyse_display_desc={
	MS_ANALYSE_DISPLAY_ID,
	"MSAnalyseDisplay",
	N_("A display filter analyse the video"),
	MS_FILTER_OTHER,
	NULL,
	2,
	0,
	analyse_display_init,
	NULL,
	analyse_display_process,
	NULL,
	analyse_display_uninit,
	analysedisplay_methods
};

#else

MSFilterDesc ms_analyse_display_desc={
	.id=MS_ANALYSE_DISPLAY_ID,
	.name="MSAnalyseDisplay",
	.text=N_("A display filter analyse the video"),
	.category=MS_FILTER_OTHER,
	.ninputs=2,
	.noutputs=0,
	.init=analyse_display_init,
	.process=analyse_display_process,
	.uninit=analyse_display_uninit,
	.methods=analysedisplay_methods
};

#endif

MS_FILTER_DESC_EXPORT(ms_analyse_display_desc)

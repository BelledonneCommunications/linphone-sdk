/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2 
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "ortp/rtp.h"

#include "framemarking_tester.h"

struct FrameMarkingTesterState {
	MSFrameMarkingTesterCb cb;
	void *user_data;
};

typedef struct FrameMarkingTesterState FrameMarkingTesterState;


static void framemarking_tester_init(MSFilter *f) {
	FrameMarkingTesterState *s = ms_new0(FrameMarkingTesterState, 1);
	s->cb = NULL;
	s->user_data = NULL;
	f->data = s;
}

static void framemarking_tester_uninit(MSFilter *f) {
	ms_free(f->data);
}

static void framemarking_tester_process(MSFilter *f) {
	FrameMarkingTesterState *s = (FrameMarkingTesterState *)f->data;
	mblk_t *m;
	
	m = ms_queue_get(f->inputs[0]);
	do {
		uint8_t marker = 0;

		if (rtp_get_frame_marker(m, RTP_EXTENSION_FRAME_MARKING, &marker) != 0) {
			if (marker & RTP_FRAME_MARKER_START || marker & RTP_FRAME_MARKER_END) {
				if (s->cb) s->cb(f, marker, s->user_data);
			}
		}

		freemsg(m);
	} while (f->inputs[0] != NULL && (m = ms_queue_get(f->inputs[0])) != NULL);
}

static int framemarking_tester_set_callback(MSFilter *f, void *arg) {
	FrameMarkingTesterState *s = (FrameMarkingTesterState *)f->data;
	MSFrameMarkingTesterCbData *data = (MSFrameMarkingTesterCbData*) arg;
	s->cb = data->cb;
	s->user_data = data->user_data;
	return 0;
}

MSFilterMethod framemarking_tester_methods[] = {
	{ MS_FRAMEMARKING_TESTER_SET_CALLBACK, framemarking_tester_set_callback },
	{ 0, NULL }
};

#ifdef _MSC_VER

MSFilterDesc ms_framemarking_tester_desc={
	MS_FRAMEMARKING_TESTER_ID,
	"MSFrameMarkingTester",
	"A filter to test frame marking.",
	MS_FILTER_OTHER,
	NULL,
	1,
	0,
	framemarking_tester_init,
	NULL,
	framemarking_tester_process,
	NULL,
	framemarking_tester_uninit,
	framemarking_tester_methods
};

#else

MSFilterDesc ms_framemarking_tester_desc={
	.id=MS_FRAMEMARKING_TESTER_ID,
	.name="MSFrameMarkingTester",
	.text="A filter to test frame marking.",
	.category=MS_FILTER_OTHER,
	.ninputs=1,
	.noutputs=0,
	.init=framemarking_tester_init,
	.process=framemarking_tester_process,
	.uninit=framemarking_tester_uninit,
	.methods=framemarking_tester_methods
};

#endif

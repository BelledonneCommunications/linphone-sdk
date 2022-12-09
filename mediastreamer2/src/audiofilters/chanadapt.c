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

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/mschanadapter.h"
#include "mediastreamer2/msticker.h"

/*
 This filter transforms stereo buffers to mono and vice versa.
*/

typedef struct AdapterState {
	int inputchans; // means the number of channels but not the number of filter input
	int outputchans; // means the number of channels but not the number of filter output
	int sample_rate;
	size_t buffer_size;
	uint8_t *buffer1;
	uint8_t *buffer2;
	MSFlowControlledBufferizer input_buffer1;
	MSFlowControlledBufferizer input_buffer2;
}AdapterState;

static void adapter_init(MSFilter *f) {
	AdapterState *s = ms_new0(AdapterState, 1);
	s->inputchans = 1;
	s->outputchans = 1;
	s->sample_rate = 8000;
	f->data = s;
}

static void adapter_uninit(MSFilter *f) {
	AdapterState *s = (AdapterState*)f->data;
	ms_free(s);
}

static void adapter_preprocess(MSFilter *f) {
	AdapterState *s = (AdapterState*)f->data;
	if (s->inputchans == 2 && s->outputchans == 1) {
		s->buffer_size = ((f->ticker->interval * s->sample_rate) / 1000) * 2;
		s->buffer1 = ms_new(uint8_t, s->buffer_size);
		s->buffer2 = ms_new(uint8_t, s->buffer_size);
		ms_flow_controlled_bufferizer_init(&s->input_buffer1, f, s->sample_rate, 1);
		ms_flow_controlled_bufferizer_set_drop_method(&s->input_buffer1, MSFlowControlledBufferizerImmediateDrop);
		ms_flow_controlled_bufferizer_set_max_size_ms(&s->input_buffer1, f->ticker->interval *2);
		ms_flow_controlled_bufferizer_init(&s->input_buffer2, f, s->sample_rate, 1);
		ms_flow_controlled_bufferizer_set_drop_method(&s->input_buffer2, MSFlowControlledBufferizerImmediateDrop);
		ms_flow_controlled_bufferizer_set_max_size_ms(&s->input_buffer2, f->ticker->interval *2);
	}
}

static void adapter_process_2_inputs_to_single_stereo_output(MSFilter *f){
	AdapterState *s = (AdapterState*)f->data;
	size_t buffer_size1, buffer_size2;

	ms_flow_controlled_bufferizer_put_from_queue(&s->input_buffer1, f->inputs[0]);
	ms_flow_controlled_bufferizer_put_from_queue(&s->input_buffer2, f->inputs[1]);

	buffer_size1 = ms_flow_controlled_bufferizer_get_avail(&s->input_buffer1);
	buffer_size2 = ms_flow_controlled_bufferizer_get_avail(&s->input_buffer2);

	if (buffer_size1 >= s->buffer_size || buffer_size2 >= s->buffer_size) {
		mblk_t *om;
		unsigned int i;

		if (buffer_size1 < s->buffer_size) memset(s->buffer1, 0, s->buffer_size);
		if (buffer_size2 < s->buffer_size) memset(s->buffer2, 0, s->buffer_size);

		ms_flow_controlled_bufferizer_read(&s->input_buffer1, s->buffer1, s->buffer_size);
		ms_flow_controlled_bufferizer_read(&s->input_buffer2, s->buffer2, s->buffer_size);

		om = allocb(s->buffer_size * 2, 0);

		for (i = 0 ; i < s->buffer_size / sizeof(int16_t) ; i++ , om->b_wptr += 4) {
			((int16_t*)om->b_wptr)[0] = ((int16_t*)s->buffer1)[i];
			((int16_t*)om->b_wptr)[1] = ((int16_t*)s->buffer2)[i];
		}

		ms_queue_put(f->outputs[0], om);
	}
}

static void adapter_process(MSFilter *f) {
	AdapterState *s = (AdapterState*)f->data;

	// Two mono inputs to stereo output
	if (f->inputs[0] != NULL && f->inputs[1] != NULL) {
		adapter_process_2_inputs_to_single_stereo_output(f);
	} else {
		mblk_t *im, *om;
		size_t msgsize;

		while ((im = ms_queue_get(f->inputs[0])) != NULL) {
			if (s->inputchans == s->outputchans) {
				ms_queue_put(f->outputs[0], im);
			} else if (s->outputchans == 2) {
				msgsize = msgdsize(im) * 2;
				om = allocb(msgsize, 0);
				for (;im->b_rptr < im->b_wptr ; im->b_rptr += 2 , om->b_wptr += 4) {
					((int16_t*)om->b_wptr)[0] = *(int16_t*)im->b_rptr;
					((int16_t*)om->b_wptr)[1] = *(int16_t*)im->b_rptr;
				}
				ms_queue_put(f->outputs[0], om);
				freemsg(im);
			} else if (s->inputchans == 2) {
				msgsize = msgdsize(im)/2;
				om = allocb(msgsize,0);
				for (;im->b_rptr < im->b_wptr ; im->b_rptr += 4 , om->b_wptr += 2) {
					*(int16_t*)om->b_wptr = *(int16_t*)im->b_rptr;
				}
				ms_queue_put(f->outputs[0], om);
				freemsg(im);
			}
		}
	}
}

static void adapter_postprocess(MSFilter *f) {
	AdapterState *s = (AdapterState*)f->data;
	if (s->buffer1 && s->buffer2){
		ms_flow_controlled_bufferizer_uninit(&s->input_buffer1);
		ms_flow_controlled_bufferizer_uninit(&s->input_buffer2);
		ms_free(s->buffer1);
		ms_free(s->buffer2);
		s->buffer1 = NULL;
		s->buffer2 = NULL;
	}
}

static int adapter_set_sample_rate(MSFilter *f, void *data) {
	AdapterState *s = (AdapterState*)f->data;
	s->sample_rate = *(int*)data;
	return 0;
}

static int adapter_get_sample_rate(MSFilter *f, void *data) {
	AdapterState *s = (AdapterState*)f->data;
	*(int*)data = s->sample_rate;
	return 0;
}

static int adapter_set_nchannels(MSFilter *f, void *data) {
	AdapterState *s = (AdapterState*)f->data;
	s->inputchans = *(int*)data;
	return 0;
}

static int adapter_get_nchannels(MSFilter *f, void *data) {
	AdapterState *s = (AdapterState*)f->data;
	*(int*)data = s->inputchans;
	return 0;
}

static int adapter_set_out_nchannels(MSFilter *f, void *data) {
	AdapterState *s = (AdapterState*)f->data;
	s->outputchans = *(int*)data;
	return 0;
}

static int adapter_get_out_nchannels(MSFilter *f, void *data) {
	AdapterState *s = (AdapterState*)f->data;
	*(int*)data = s->outputchans;
	return 0;
}

static MSFilterMethod methods[] = {
	{	MS_FILTER_SET_SAMPLE_RATE, adapter_set_sample_rate},
	{	MS_FILTER_GET_SAMPLE_RATE, adapter_get_sample_rate},
	{	MS_FILTER_SET_NCHANNELS , adapter_set_nchannels },
	{	MS_FILTER_GET_NCHANNELS, adapter_get_nchannels },
	{	MS_CHANNEL_ADAPTER_SET_OUTPUT_NCHANNELS, adapter_set_out_nchannels },
	{	MS_CHANNEL_ADAPTER_GET_OUTPUT_NCHANNELS, adapter_get_out_nchannels },
	{	0,	NULL }
};

MSFilterDesc ms_channel_adapter_desc = {
	MS_CHANNEL_ADAPTER_ID,
	"MSChannelAdapter",
	N_("A filter that converts from mono to stereo and vice versa."),
	MS_FILTER_OTHER,
	NULL,
	2,
	1,
	adapter_init,
	adapter_preprocess,
	adapter_process,
	adapter_postprocess,
	adapter_uninit,
	methods,
	MS_FILTER_IS_PUMP
};

MS_FILTER_DESC_EXPORT(ms_channel_adapter_desc)

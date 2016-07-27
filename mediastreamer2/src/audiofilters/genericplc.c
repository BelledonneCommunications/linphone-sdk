/*
 mediastreamer2 library - modular sound and video processing and streaming
 Copyright (C) 2015 Belledonne Communications, Grenoble

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


#include <stdint.h>
#include "mediastreamer2/mscommon.h"
#include "genericplc.h"
#include "mediastreamer2/dsptools.h"
#include <math.h>

#define PI 3.14159265

plc_context_t *generic_plc_create_context(int sample_rate) {
	int i;
	plc_context_t *context = (plc_context_t*) ms_new0(plc_context_t, 1);

	/* Continuity buffer usage :
	 * Regular: store the last TRANSITION_DELAY ms of signal of each arriving frame, restored at the begining of next frame
	 * PLC mode: store 2*TRANSITION_DELAY ms of PLC signal
	 *     - the first TRANSITION_DELAY ms will be used to populate the begining of first frame arrived after PLC, so we can save the end of arrived frame,
	 *     - the second TRANSITION_DELAY ms are mixed for smooth transition with the begining of arrived frame
	 */
	context->continuity_buffer = ms_malloc0(2*sample_rate*sizeof(int16_t)*TRANSITION_DELAY/1000); /* continuity buffer introduce a TRANSITION_DELAY ms delay */
	context->plc_buffer_len = (uint16_t)(sample_rate*sizeof(int16_t)*PLC_BUFFER_LEN); /* length in bytes of the plc_buffer */
	context->plc_buffer = ms_malloc0(context->plc_buffer_len);
	context->hamming_window = ms_malloc0(sample_rate*PLC_BUFFER_LEN*sizeof(float));
	context->plc_out_buffer = ms_malloc0(2*context->plc_buffer_len);
	context->plc_index=0;
	context->plc_samples_used=0;
	context->sample_rate = sample_rate;

	/* initialise the fft contexts, one with sample number being the plc buffer length,
	 * the complex to real is twice that number as buffer is doubled in frequency domain */
	context->fft_to_frequency_context = ms_fft_init(sample_rate*PLC_BUFFER_LEN);
	context->fft_to_time_context = ms_fft_init(2*sample_rate*PLC_BUFFER_LEN);

	/* initialise hamming window : h(t) = 0.75 - 0.25*cos(2pi*t/T) */
	for(i=0; i<sample_rate*PLC_BUFFER_LEN; i++) {
		context->hamming_window[i] = (float)(0.75 - 0.25*cos( 2*PI*i/(sample_rate*PLC_BUFFER_LEN)));
	}

	return context;
}

void generic_plc_destroy_context(plc_context_t *context) {
	ms_free(context->continuity_buffer);
	ms_free(context->plc_buffer);
	ms_free(context->hamming_window);
	ms_free(context->plc_out_buffer);
	ms_fft_destroy(context->fft_to_frequency_context);
	ms_fft_destroy(context->fft_to_time_context);

	ms_free(context);
}

void generic_plc_fftbf(plc_context_t *context, int16_t *input_buffer, int16_t *output_buffer, size_t input_buffer_len) {
	/* Allocate temporary buffers to perform FFT-> double buffer size in frequency domain -> inverse FFT */
	ms_word16_t *time_domain_buffer = ms_malloc0(input_buffer_len*sizeof(ms_word16_t));
	ms_word16_t *freq_domain_buffer = ms_malloc0(input_buffer_len*sizeof(ms_word16_t));
	ms_word16_t *freq_domain_buffer_double = ms_malloc0(2*input_buffer_len*sizeof(ms_word16_t));
	ms_word16_t *time_domain_buffer_double = ms_malloc0(2*input_buffer_len*sizeof(ms_word16_t));
	size_t i;

	/* convert to ms_word16_t the input buffer */
	for (i=0; i<input_buffer_len; i++) {
		time_domain_buffer[i] = (ms_word16_t)((float)input_buffer[i]*context->hamming_window[i]);
	}

	/* FFT */
	ms_fft(context->fft_to_frequency_context, time_domain_buffer, freq_domain_buffer);

	/* double the number of sample in frequency domain */
	for (i=0; i<input_buffer_len; i++) {
		freq_domain_buffer_double[2*i] = freq_domain_buffer[i]*ENERGY_ATTENUATION;
		freq_domain_buffer_double[2*i+1]  = 0;//freq_domain_buffer[i]/2 ;
	}

	/* inverse FFT, we have twice the number of original samples, discard the first half and use the second as new samples */
	ms_ifft(context->fft_to_time_context, freq_domain_buffer_double, time_domain_buffer_double);

	ms_free(time_domain_buffer);
	ms_free(freq_domain_buffer);
	ms_free(freq_domain_buffer_double);

	/* copy generated signal to the plc_out_buffer */
	for (i=0; i<2*input_buffer_len; i++) {
		output_buffer[i] = (int16_t)(time_domain_buffer_double[i]);
	}

	ms_free(time_domain_buffer_double);
}

void generic_plc_generate_samples(plc_context_t *context, int16_t *data, uint16_t sample_nbr) {
	uint16_t continuity_buffer_sample_nbr = context->sample_rate*TRANSITION_DELAY/1000;

	/* shall we just set everything to 0 */
	if (context->plc_samples_used>=MAX_PLC_LEN*context->sample_rate/1000) {
		context->plc_samples_used += sample_nbr;
		memset(data, 0, sample_nbr*sizeof(int16_t));
		memset(context->continuity_buffer, 0, 2*continuity_buffer_sample_nbr*sizeof(int16_t));

		return;
	}

	/* it's the first missing packet, we must generate samples */
	if (context->plc_samples_used == 0) {
		/* generate samples based on the plc_buffer(previously received signal)*/
		generic_plc_fftbf(context, (int16_t *)context->plc_buffer, context->plc_out_buffer, context->sample_rate*PLC_BUFFER_LEN);

		/* mix with continuity buffer */
		generic_plc_transition_mix(context->plc_out_buffer, (int16_t *)context->continuity_buffer, continuity_buffer_sample_nbr);
	}

	/* we being asked for more sample than we have in the buffer (save some for continuity buffer, we must have twice the TRANSITION_DELAY in buffer) */
	if (context->plc_index + sample_nbr + continuity_buffer_sample_nbr*2 > 2*context->sample_rate*PLC_BUFFER_LEN ) {
		uint16_t samples_ready_nbr = 2*context->sample_rate*PLC_BUFFER_LEN - context->plc_index - continuity_buffer_sample_nbr;

		if (samples_ready_nbr>sample_nbr) { /* we had more than one but less than two TRANSITION_DELAY ms in buffer */
			samples_ready_nbr=sample_nbr;
		}

		/* copy all the remaining sample to the data buffer (save some for continuity) */
		memcpy(data, context->plc_out_buffer+context->plc_index, samples_ready_nbr*sizeof(int16_t));
		memcpy(context->continuity_buffer, context->plc_out_buffer+context->plc_index+samples_ready_nbr, continuity_buffer_sample_nbr * sizeof(int16_t));

		/* generate sample based on the plc_out_buffer(previously generated signal) */
		generic_plc_fftbf(context, context->plc_out_buffer, context->plc_out_buffer, context->sample_rate*PLC_BUFFER_LEN);

		/* mix with continuity buffer */
		generic_plc_transition_mix(context->plc_out_buffer, (int16_t *)context->continuity_buffer, continuity_buffer_sample_nbr);

		/* copy the rest of requested samples to the data buffer */
		if (sample_nbr!=samples_ready_nbr) {
			memcpy(data+samples_ready_nbr, context->plc_out_buffer, (sample_nbr - samples_ready_nbr)*sizeof(int16_t));
		}

		context->plc_index = sample_nbr - samples_ready_nbr;
		/* manage continuity buffer */
		memcpy(context->continuity_buffer, context->plc_out_buffer + context->plc_index, 2*continuity_buffer_sample_nbr * sizeof(int16_t));
	} else {

		/* copy the requested amount of data to the given buffer and update continuity buffer */
		memcpy(data, context->plc_out_buffer+context->plc_index, sample_nbr*sizeof(int16_t));

		context->plc_index += sample_nbr;
		/* update continuity buffer */
		memcpy(context->continuity_buffer, context->plc_out_buffer + context->plc_index, 2*continuity_buffer_sample_nbr * sizeof(int16_t));

	}

	/* adjust volume when PLC_DECREASE_START samples point is reached */
	if ( context->plc_samples_used + sample_nbr > PLC_DECREASE_START*context->sample_rate/1000 ) {
		int i = PLC_DECREASE_START*context->sample_rate/1000 - context->plc_samples_used;
		if (i<0) i=0;
		for (; i<sample_nbr; i++) {
			if (context->plc_samples_used+i>=MAX_PLC_LEN*context->sample_rate/1000) {
				data[i] = 0;
			} else {
				data[i] = (int16_t)((1.0+((float)(PLC_DECREASE_START*context->sample_rate/1000 - (context->plc_samples_used + i))/(float)((MAX_PLC_LEN-PLC_DECREASE_START)*context->sample_rate/1000))) * (float)data[i]);
			}
		}
	}
	context->plc_samples_used += sample_nbr;
}

void generic_plc_update_plc_buffer(plc_context_t *context, unsigned char *data, size_t data_len) {
	/* check packet length to be greater than plc_buffer */
	if (data_len<context->plc_buffer_len) {
		/* move back the current plc_buffer to get enough room to insert incoming message */
		memmove(context->plc_buffer, context->plc_buffer+data_len, context->plc_buffer_len-data_len);
		/* append current msg at the end of plc buffer */
		memcpy(context->plc_buffer+context->plc_buffer_len-data_len, data, data_len);
	} else {
		memcpy(context->plc_buffer, data+data_len-context->plc_buffer_len, context->plc_buffer_len);
	}
}

void generic_plc_update_continuity_buffer(plc_context_t *context, unsigned char *data, size_t data_len) {
	size_t transitionBufferSize = context->sample_rate*sizeof(int16_t)*TRANSITION_DELAY/1000;
	unsigned char *buffer;

	if (transitionBufferSize > data_len) transitionBufferSize = data_len;
	buffer=ms_malloc(transitionBufferSize);

	/* get the last TRANSITION_DELAY ms in a temp buffer */
	memcpy(buffer, data+data_len-transitionBufferSize, transitionBufferSize);
	/* move by TRANSITION_DELAY ms the signal in msg toward future crashing the end of the msg we just saved in buf */
	memmove(data+transitionBufferSize, data, data_len - transitionBufferSize);
	/* retrieve from continuity buffer the last TRANSITION_DELAY ms of previous message and insert them at the begining of current msg */
	memcpy(data, context->continuity_buffer, transitionBufferSize);
	/* store in context for next msg the last TRANSITION_DELAY ms of current msg */
	memcpy(context->continuity_buffer, buffer, transitionBufferSize);

	ms_free(buffer);
}


/** Transition mix function, mix last received data with local generated one for smooth transition */
void generic_plc_transition_mix(int16_t *inout_buffer, int16_t *continuity_buffer, uint16_t fading_sample_nbr) {
	uint16_t i;
	for (i=0; i<fading_sample_nbr; i++) {
		float progress = ((float) i)/fading_sample_nbr;
		inout_buffer[i] = (int16_t)((float)continuity_buffer[i]*(1-progress) + (float)inout_buffer[i]*progress);
	}
}

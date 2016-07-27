/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2014 Belledonne Communications SARL
Author: Simon MORLAT (simon.morlat@linphone.org)

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

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msutils.h"
#include "waveheader.h"

#include <math.h>

typedef struct {
	int rate;
	int nchannels;
	int16_t *buffer;
	int nsamples;
	int fd;
}FileInfo;

static void file_info_destroy(FileInfo *fi){
	close(fi->fd);
	ms_free(fi->buffer);
	ms_free(fi);
}

static FileInfo *file_info_new(const char *file){
	int fd;
	FileInfo *fi;
	wave_header_t header;
	int hsize;
	struct stat stbuf;
	int size;

	if ((fd=open(file,O_RDONLY|O_BINARY))==-1){
		ms_error("Failed to open %s : %s",file,strerror(errno));
		return NULL;
	}
	if (fstat(fd,&stbuf)==-1){
		ms_error("could not fstat.");
		return NULL;
	}
	hsize=ms_read_wav_header_from_fd(&header,fd);
	if (hsize<=0){
		ms_error("%s: not a wav file", file);
		return NULL;
	}
	if (wave_header_get_channel(&header)<1){
		ms_error("%s: incorrect number of channels", file);
		return NULL;
	}
	
	fi=ms_new0(FileInfo,1);
	size=stbuf.st_size-hsize;
	fi->rate=wave_header_get_rate(&header);
	fi->nchannels=wave_header_get_channel(&header);
	fi->nsamples=size/(sizeof(int16_t)*fi->nchannels);
	fi->fd = fd;

	return fi;
}

static int file_info_read(FileInfo *fi, int zero_pad_samples, int zero_pad_end_samples){
	int err;
	int size = fi->nsamples * fi->nchannels *2;
	fi->buffer=ms_new0(int16_t,(fi->nsamples + 2*zero_pad_samples + 2*zero_pad_end_samples) * fi->nchannels);
	
	err = read(fi->fd,fi->buffer + (zero_pad_samples * fi->nchannels), size);
	if (err == -1){
		ms_error("Could not read file: %s",strerror(errno));
	}else if (err<size){
		ms_error("Partial read of %i bytes",err);
		err = -1;
	}else err = 0;
	fi->nsamples += zero_pad_end_samples; /*consider that the end-padding zero samples are part of the audio*/
	return err;
}

static int64_t scalar_product(int16_t *s1, int16_t *s2, int n, int step){
	int64_t acc = 0;
	int i;
	
	for (i=0; i< n-4; i += 4){
		acc += s1[i*step] * s2[i*step];
		acc += s1[(i+1)*step] * s2[(i+1)*step];
		acc += s1[(i+2)*step] * s2[(i+2)*step];
		acc += s1[(i+3)*step] * s2[(i+3)*step];
	}
	for (; i< n; i++){
		acc += s1[i*step] * s2[i*step];
	}
	return acc;
}

typedef struct _ProgressContext{
	MSAudioDiffProgressNotify func;
	void *user_data;
	int progress;
	int prev_progress;
	int cur_op_progress;
	float cur_op_weight;
}ProgressContext;

static void progress_context_init(ProgressContext *ctx, MSAudioDiffProgressNotify func, void *user_data){
	ctx->func = func;
	ctx->user_data = user_data;
	ctx->progress = 0;
	ctx->prev_progress = 0;
	ctx->cur_op_progress = 0;
	ctx->cur_op_weight = 1;
}

/*start a new operation with supplied weight*/
static void progress_context_push(const ProgressContext *ctx, ProgressContext* new_ctx, float weight){
	new_ctx->func = ctx->func;
	new_ctx->user_data = ctx->user_data;
	new_ctx->progress = ctx->progress;
	new_ctx->cur_op_progress = 0;
	new_ctx->prev_progress = 0;
	new_ctx->cur_op_weight = weight * ctx->cur_op_weight;
}

static void progress_context_pop(ProgressContext *ctx, const ProgressContext* new_ctx){
	ctx->progress += new_ctx->cur_op_progress;
	ctx->cur_op_progress += new_ctx->cur_op_progress;
}

static void progress_context_update(ProgressContext *ctx, int cur_progress){
	if (!ctx->func) return;
	ctx->cur_op_progress = (int)(cur_progress * (float)ctx->cur_op_weight);
	if (ctx->cur_op_progress != ctx->prev_progress){
		ctx->prev_progress = ctx->cur_op_progress;
		ctx->func(ctx->user_data, ctx->progress +  ctx->cur_op_progress);
	}
}

/* This function assumes the following:
 * - s1's length is inferior to s2's length
 * - s2 has been padded with 'len' initial and trailing zeroes 
 * The output is a normalized cross correlation.
**/
static int compute_cross_correlation(int16_t *s1, int n1, int16_t *s2_padded, float *xcorr, int xcorr_nsamples, ProgressContext *pctx, int step, int64_t *s1_energy){
	int max_index = 0;
	int i;
	int64_t tmp,max=0;
	int64_t norm1 = scalar_product(s1, s1, n1, step);
	int64_t norm2 = scalar_product(s2_padded, s2_padded, n1, step) - s2_padded[step*(n1-1)]*s2_padded[step*(n1-1)];
	
	for (i=0; i<xcorr_nsamples; i++){
		norm2 += s2_padded[step*(i+n1-1)]*s2_padded[step*(i+n1-1)];
		tmp = scalar_product(s1, s2_padded + i*step, n1, step);
		xcorr[i] = (float)((double)tmp / sqrt((double)(norm1)*(double)norm2));
		tmp = tmp < 0 ? -tmp : tmp;
		if (tmp > max){
			max = tmp;
			max_index = i;
		}
		norm2 -= s2_padded[step*i]*s2_padded[step*i];
		progress_context_update(pctx, 100 * i/xcorr_nsamples);
	}
	if (s1_energy) *s1_energy = norm1;
	return max_index;
}

static int _ms_audio_diff_one_chunk(int16_t *s1, int16_t *s2, int nsamples, int max_shift_samples, int nchannels, double *ret, int64_t *s1_energy, ProgressContext *pctx){
	int xcorr_size;
	int max_index_r;
	int max_index_l;
	int max_pos;
	ProgressContext local_pctx;
	int64_t er, el;
	
	xcorr_size=max_shift_samples*2;
	
	if (nchannels == 2){
		float *xcorr_r = ms_new0(float, xcorr_size);
		float *xcorr_l = ms_new0(float, xcorr_size);
		double max = 0;
		double max_r, max_l;
		int i;
		
		progress_context_push(pctx, &local_pctx, 0.5);
		max_index_r = compute_cross_correlation(s1, nsamples, s2, xcorr_r, xcorr_size, &local_pctx, 2, &er);
		max_r = xcorr_r[max_index_r];
		progress_context_pop(pctx, &local_pctx);

		progress_context_push(pctx, &local_pctx, 0.5);
		max_index_l = compute_cross_correlation(s1 + 1, nsamples, s2 + 1, xcorr_l, xcorr_size, &local_pctx, 2, &el);
		max_l = xcorr_l[max_index_l];
		progress_context_pop(pctx, &local_pctx);
		
		max_pos = 0;
		/*sum the square of r and l xcorr signals to determine the global maximum*/
		for (i = 0; i < max_shift_samples; ++i){
			xcorr_r[i] = xcorr_r[i]*xcorr_r[i] + xcorr_l[i]*xcorr_l[i];
			if (xcorr_r[i] > max){
				max = xcorr_r[i];
				max_pos = i;
			}
		}
		
		max = sqrt(max/2);
		ms_message("chunk - max stereo cross-correlation obtained at position [%i,%i], similarity factor=%g,%g",
			   max_index_r-max_shift_samples,max_index_l-max_shift_samples,max_r, max_l);
		max_pos = max_pos - max_shift_samples;
		ms_message("chunk - max stereo overall cross-correlation obtained at position [%i], similarity factor=[%g]", max_pos, max);  
		*ret = max;
		if (s1_energy) *s1_energy = (er + el)/2;
		ms_free(xcorr_r);
		ms_free(xcorr_l);
	}else{
		float *xcorr = ms_new0(float, xcorr_size);
		progress_context_push(pctx, &local_pctx, 1.0);
		max_index_r = compute_cross_correlation(s1, nsamples , s2, xcorr, xcorr_size, &local_pctx, 1, s1_energy);
		progress_context_pop(pctx, &local_pctx);
		*ret = xcorr[max_index_r];
		max_pos = max_index_r-max_shift_samples;
		ms_free(xcorr);
		ms_message("chunk - max cross-correlation obtained at position [%i], similarity factor=%g", max_pos, *ret);
	}
	return max_pos;
}

static int _ms_audio_diff_chunked(FileInfo *fi1, FileInfo *fi2, double *ret, int max_shift_samples, int chunk_size_samples, ProgressContext *pctx){
	int cur_chunk_samples;
	int samples = 0;
	int step = fi1->nchannels;
	double cum_res = 0;
	int64_t cum_maxpos = 0;
	int maxpos;
	int num_chunks = (fi1->nsamples + chunk_size_samples) / chunk_size_samples;
	int *max_pos_table = ms_new0(int, num_chunks);
	int64_t *chunk_energies = ms_new0(int64_t, num_chunks);
	double variance = 0;
	int i=0;
	ProgressContext local_pctx;
	int64_t tot_energy = 0;
	
	do{
		double chunk_ret = 0;
		int64_t chunk_energy;
		cur_chunk_samples = MIN(chunk_size_samples, fi1->nsamples - samples);
		progress_context_push(pctx, &local_pctx, (float)cur_chunk_samples/(float)fi1->nsamples);
		maxpos = _ms_audio_diff_one_chunk(fi1->buffer + samples*step, fi2->buffer + samples*step, cur_chunk_samples, max_shift_samples,
					 fi1->nchannels, &chunk_ret, &chunk_energy, &local_pctx);
		progress_context_pop(pctx, &local_pctx);
		samples += chunk_size_samples;
		cum_res += chunk_ret * chunk_energy;
		ms_message("chunk_energy is %li", (long int) chunk_energy);
		chunk_energies[i] = chunk_energy;
		max_pos_table[i] = maxpos; 
		cum_maxpos += maxpos * chunk_energy;
		tot_energy += chunk_energy;
		i++;
	}while(samples < fi1->nsamples);
	num_chunks = i;
	
	ms_message("tot_energy is %li", (long int) tot_energy);
	maxpos = (int)(cum_maxpos / tot_energy);
	ms_message("Maxpos is %i", maxpos);
	
	/*compute variance of max_pos among all chunks*/
	for(i=0; i<num_chunks; ++i){
		double tmp = (max_pos_table[i]-maxpos)*((double)chunk_energies[i]/(double)tot_energy);
		variance += tmp*tmp;
	}
	variance = sqrt(variance);
	ms_message("Max position variance is [%g], that is [%g] ms", variance, 1000.0 * variance / fi1->rate);
	variance = variance / (double) max_shift_samples;
	*ret = cum_res / (double)tot_energy;
	ms_message("Similarity factor weighted with most significant chunks is [%g]", *ret);
	*ret = *ret * (1-variance);
	ms_message("After integrating max position variance accross chunks, it is [%g]", *ret);
	ms_free(chunk_energies);
	ms_free(max_pos_table);
	return maxpos;
}

int ms_audio_diff(const char *ref_file, const char *matched_file, double *ret, const MSAudioDiffParams *params, MSAudioDiffProgressNotify func, void *user_data){
	FileInfo *fi1,*fi2;
	int max_shift_samples;
	int err = 0;
	ProgressContext pctx;
	int maxpos;
	int end_zero_pad_samples = 0;
	
	progress_context_init(&pctx, func, user_data);
	
	*ret=0;

	fi1=file_info_new(ref_file);
	if (fi1==NULL) return 0;
	fi2=file_info_new(matched_file);
	if (fi2==NULL){
		file_info_destroy(fi1);
		return -1;
	}

	if (fi1->rate!=fi2->rate){
		ms_error("Comparing files of different sampling rates is not supported (%d vs %d)", fi1->rate, fi2->rate);
		err = -1;
		goto end;
	}

	if (fi1->nchannels!=fi2->nchannels){
		ms_error("Comparing files with different number of channels is not supported (%d vs %d)", fi1->nchannels, fi2->nchannels);
		err = -1;
		goto end;
	}
	if (fi1->nsamples == 0){
		ms_error("Reference file has no samples !");
		err = -1;
		goto end;
	}
	if (fi2->nsamples == 0){
		ms_error("Matched file has no samples !");
		err = -1;
		goto end;
	}
	
	max_shift_samples = MIN(fi1->nsamples, fi2->nsamples) * MIN(MAX(1, params->max_shift_percent), 100) / 100;
	
	if (fi1->nsamples > fi2->nsamples){
		end_zero_pad_samples = fi1->nsamples - fi2->nsamples;
	}
	/*load the datas*/
	if (file_info_read(fi1, 0, 0)==-1){
		err = -1;
		goto end;
	}
	if (file_info_read(fi2, max_shift_samples, end_zero_pad_samples)==-1){
		err = -1;
		goto end;
	}
	
	if (params->chunk_size_ms == 0){
		maxpos = _ms_audio_diff_one_chunk(fi1->buffer, fi2->buffer, fi1->nsamples, max_shift_samples, fi1->nchannels, ret, NULL, &pctx);
	}else{
		int chunk_size_samples = params->chunk_size_ms * fi1->rate / 1000;
		maxpos = _ms_audio_diff_chunked(fi1, fi2, ret, max_shift_samples, chunk_size_samples, &pctx);
	}
	ms_message("Max cross-correlation obtained at position [%i], similarity factor=[%g]", maxpos, *ret);
end:
	file_info_destroy(fi1);
	file_info_destroy(fi2);
	return err;
}

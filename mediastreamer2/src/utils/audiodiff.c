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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
	int64_t energy_r;
	int64_t energy_l;
}FileInfo;

static void file_info_destroy(FileInfo *fi){
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
	int err;

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
	fi=ms_new0(FileInfo,1);
	size=stbuf.st_size-hsize;

	fi->buffer=ms_new0(int16_t,size/sizeof(int16_t));
	fi->rate=wave_header_get_rate(&header);
	fi->nchannels=wave_header_get_channel(&header);
	fi->nsamples=size/(sizeof(int16_t)*fi->nchannels);
	err=read(fd,fi->buffer,size);
	if (err==-1){
		ms_error("Could not read file: %s",strerror(errno));
		goto error;
	}else if (err<size){
		ms_error("Partial read of %i bytes",err);
		goto error;
	}
	return fi;
	error:
		file_info_destroy(fi);
		return NULL;
}

/*
 * compute cross correlation between two signals. The results should n1+n2 samples.
**/
static int compute_cross_correlation(int16_t *s1, int n1, int16_t *s2, int n2, int64_t *xcorr, int xcorr_nsamples, MSAudioDiffProgressNotify func, void *user_data, int delta){
	int64_t acc;
	int64_t max=0;
	int max_index=0;
	int i,k;
	int completion=0;
	int prev_completion=0;

#define STEP 4
#define ACC(k1,k2,s) acc+=(int64_t)( (int)s1[k1+s]*(int)s2[k2+s]);
	for(i=0;i<delta;++i){
		completion=100*i/(delta*2);
		acc=0;

		for(k=0; k<MIN(n1,n2-delta+i); k+=STEP){
			int k2=k+delta-i;
			ACC(k,k2,0);
			ACC(k,k2,1);
			ACC(k,k2,2);
			ACC(k,k2,3);
		}
		xcorr[i]=acc;
		acc = acc > 0 ? acc : -acc;
		if (acc>max) {
			max=acc;
			max_index=i;
		}
		if (func && completion>prev_completion)
			func(user_data,completion);
		prev_completion=completion;
	}
	for(i=delta;i<2*delta;++i){
		completion=100*i/(delta*2);
		acc=0;

		for(k=0; k<MIN(n1+delta-i,n2); k+=STEP){
			int k1;
			k1=k+i-delta;
			ACC(k1,k,0);
			ACC(k1,k,1);
			ACC(k1,k,2);
			ACC(k1,k,3);
		}
		xcorr[i] = acc;
		acc = acc > 0 ? acc : -acc;
		if (acc>max) {
			max=acc;
			max_index=i;
		}
		if (func && completion>prev_completion)
			func(user_data,completion);
		prev_completion=completion;
	}
	return max_index;
}

/*
 * compute cross correlation between two stereo signals, but for one channel only. The results should be at most n1+n2 samples.
**/
static int compute_cross_correlation_interleaved(int16_t *s1, int n1, int16_t *s2, int n2, int64_t *xcorr, int xcorr_nsamples, MSAudioDiffProgressNotify func, void *user_data, int channel_num, int delta){
	int64_t acc;
	int64_t max=0;
	int max_index=0;
	int i,k;
	int completion=0;
	int prev_completion=0;

	for (i=0;i<delta;++i){
		completion=100*(i+(2*delta*channel_num))/(delta*4);
		acc=0;

		for(k=0; k<MIN(n1,n2-delta+i); k++){
			int k2=k+delta-i;
			ACC((2*k)+channel_num,(2*k2)+channel_num,0);
		}
		xcorr[i]=acc;
		acc = acc > 0 ? acc : -acc;
		if (acc>max) {
			max=acc;
			max_index=i;
		}
		if (func && completion>prev_completion)
			func(user_data,completion);
		prev_completion=completion;
	}
	for (i=delta;i<2*delta;++i){
		completion=100*(i+(2*delta*channel_num))/(delta*4);
		acc=0;

		for(k=0; k<MIN(n1+delta-i,n2); k++){
			int k1;
			k1=k+i-delta;
			ACC((2*k1)+channel_num,(2*k)+channel_num,0);
		}
		xcorr[i] = acc;
		acc = acc > 0 ? acc : -acc;
		if (acc>max) {
			max=acc;
			max_index=i;
		}
		if (func && completion>prev_completion)
			func(user_data,completion);
		prev_completion=completion;
	}
	return max_index;
}

static int64_t energy(int16_t *s1, int n1){
	int i;
	int64_t ret=0;
	for(i=0;i<n1;++i){
		ret+=(int)s1[i]*(int)s1[i];
	}
	return ret;
}

static int64_t energy_interleaved(int16_t *s1, int n1, int channel_index){
	int i;
	int64_t ret=0;
	for(i=0;i<n1;i++){
		ret+=(int)s1[2*i+channel_index]*(int)s1[2*i+channel_index];
	}
	return ret;
}

void file_info_compute_energy(FileInfo *fi){
	if (fi->nchannels==2){
		fi->energy_r=energy_interleaved(fi->buffer,fi->nsamples,0);
		fi->energy_l=energy_interleaved(fi->buffer,fi->nsamples,1);
		ms_message("energy_r=%li energy_l=%li",(long int)fi->energy_r, (long int)fi->energy_l);
	}else{
		fi->energy_r=energy(fi->buffer,fi->nsamples);
		ms_message("energy=%li",(long int)fi->energy_r);
	}
}


/**
 * Utility that compares two PCM 16 bits audio files and returns a similarity factor between 0 and 1.
 * @param file1 a wav file path
 * @param file2 a wav file path
 * @param ret the similarity factor, set in return
 * @param overlap_p percentage of overlap between the two signals, used to restrict the cross correlation around t=0.
 * @param func a callback called to show progress of the operation
 * @param user_data a user data passed to the callback when invoked.
 * @return -1 on error, 0 if succesful.
**/
int ms_audio_diff(const char *file1, const char *file2, double *ret, int max_shift_percent, MSAudioDiffProgressNotify func, void *user_data){
	FileInfo *fi1,*fi2;
	int64_t *xcorr;
	int xcorr_size;
	int max_shift_samples;
	int max_index_r;
	int max_index_l;
	double max_r, max_l;

	*ret=0;

	fi1=file_info_new(file1);
	if (fi1==NULL) return 0;
	fi2=file_info_new(file2);
	if (fi2==NULL){
		file_info_destroy(fi1);
		return -1;
	}

	if (fi1->rate!=fi2->rate){
		ms_error("Comparing files of different sampling rates is not supported (%d vs %d)", fi1->rate, fi2->rate);
		return -1;
	}

	if (fi1->nchannels!=fi2->nchannels){
		ms_error("Comparing files with different number of channels is not supported (%d vs %d)", fi1->nchannels, fi2->nchannels);
		return -1;
	}

	file_info_compute_energy(fi1);
	file_info_compute_energy(fi2);

	if (fi1->energy_r==0 || fi2->energy_r==0){
		/*avoid division by zero*/
		ms_error("One of the two files is pure silence.");
		return -1;
	}

	max_shift_samples = MIN(fi1->nsamples, fi2->nsamples) * max_shift_percent / 100;
	xcorr_size=max_shift_samples*2;
	xcorr=ms_new0(int64_t,xcorr_size);
	if (fi1->nchannels == 2){
		max_index_r=compute_cross_correlation_interleaved(fi1->buffer,fi1->nsamples,fi2->buffer,fi2->nsamples,xcorr,xcorr_size, func, user_data, 0, max_shift_samples);
		max_r=xcorr[max_index_r];
		ms_message("max_r=%g", (double)max_r);
		max_r/=sqrt((double)fi1->energy_r*(double)fi2->energy_r);

		max_index_l=compute_cross_correlation_interleaved(fi1->buffer,fi1->nsamples,fi2->buffer,fi2->nsamples,xcorr,xcorr_size, func, user_data, 1, max_shift_samples);
		max_l=xcorr[max_index_l];
		ms_message("max_l=%g", (double)max_l);
		max_l/=sqrt((double)fi1->energy_l*(double)fi2->energy_l);
		ms_message("Max stereo cross-correlation obtained at position [%i,%i], similarity factor=%g,%g",
			   max_index_r-max_shift_samples,max_index_l-max_shift_samples,max_r, max_l);
		*ret = 0.5 * (fabs(max_r) + fabs(max_l)) * (1 - (double)abs(max_index_r-max_index_l)/(double)xcorr_size);
	}else{
		max_index_r=compute_cross_correlation(fi1->buffer,fi1->nsamples,fi2->buffer,fi2->nsamples,xcorr,xcorr_size, func, user_data, max_shift_samples);
		max_r=xcorr[max_index_r];
		max_r/=(sqrt(fi1->energy_r)*sqrt(fi2->energy_r));
		*ret=max_r;
		ms_message("Max cross-correlation obtained at position [%i], similarity factor=%g",max_index_r-max_shift_samples,*ret);
	}
	ms_free(xcorr);
	file_info_destroy(fi1);
        file_info_destroy(fi2);
	return 0;
}

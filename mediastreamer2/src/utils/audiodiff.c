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
	double energy_r;
	double energy_l;
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
		ms_error("not a wav file");
		return NULL;
	}
	fi=ms_new0(FileInfo,1);
	size=stbuf.st_size-hsize;
	
	fi->buffer=ms_new0(int16_t,size);
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
static int compute_cross_correlation(int16_t *s1, int n1, int16_t *s2, int n2, double *xcorr, int xcorr_nsamples, MSAudioDiffProgressNotify func, void *user_data, int min_overlap){
	int64_t acc;
	int64_t max=0;
	int max_index=0;
	int i,j,k;
	int b1min,b2min,b1max,b2max;
	int completion=0;
	int prev_completion=0;
	int width, l;
	
#define STEP 4
#define ACC(s) acc+=(int64_t)( (int)s1[j+s]*(int)s2[k+s]);
	for(i=0;i<xcorr_nsamples;++i){
		completion=100*i/xcorr_nsamples;
		acc=0;
		b1min=MAX(n1-i,0);
		b2min=MAX(0,i-n1);
		b1max=b1min+n1-STEP;
		b2max=b2min+n2-STEP;
		width=MIN(b1max,b2max) - MAX(b1min, b2min);
		if (width < min_overlap) {
			continue;
		}
		for(j=b1min,k=b2min, l=0; l<width ; j+=STEP,k+=STEP, l+=STEP){
			ACC(0);
			ACC(1);
			ACC(2);
			ACC(3);
		}
		xcorr[i]=acc;
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
 * compute cross correlation between two stereo signals, but for one channel only. The results should n1+n2 samples.
**/
static int compute_cross_correlation_interleaved(int16_t *s1, int n1, int16_t *s2, int n2, double *xcorr, int xcorr_nsamples, MSAudioDiffProgressNotify func, void *user_data, int channel_num, int min_overlap){
	int64_t acc;
	int64_t max=0;
	int max_index=0;
	int i,j,k;
	int b1min,b2min;
	int b1max,b2max;
	int completion=0;
	int prev_completion=0;
	int width;
	int l;
	
	s1+=channel_num;
	s2+=channel_num;
	
#define STEP2 8
#define ACC2(s) acc+=(int64_t)( (int)s1[j+s]*(int)s2[k+s]);
	for(i=0;i<xcorr_nsamples;++i){
		completion=100*(i+(xcorr_nsamples*channel_num))/(xcorr_nsamples*2);
		acc=0;
		b1min=MAX(n1-i,0);
		b2min=MAX(0,i-n1);
		b1max=b1min+n1-STEP2;
		b2max=b2min+n2-STEP2;
		width=MIN(b1max,b2max) - MAX(b1min, b2min);
		//ms_message(" width %i overlap %i",width, min_overlap);
		if (width < min_overlap) {
			continue;
		}
		for(j=b1min,k=b2min,l=0; l<width; j+=STEP2,k+=STEP2,l+=STEP2){
			ACC2(0);
			ACC2(2);
			ACC2(4);
			ACC2(6);
		}
		xcorr[i]=acc;
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

static double energy(int16_t *s1, int n1){
	int i;
	int64_t ret=0;
	for(i=0;i<n1;++i){
		ret+=(int)s1[i]*(int)s1[i];
	}
	return (double)ret;
}

static double energy_interleaved(int16_t *s1, int n1){
	int i;
	int64_t ret=0;
	for(i=0;i<n1;i+=2){
		ret+=(int)s1[i]*(int)s1[i];
	}
	return (double)ret;
}

void file_info_compute_energy(FileInfo *fi){
	if (fi->nchannels==2){
		fi->energy_r=energy_interleaved(fi->buffer,fi->nsamples);
		fi->energy_l=energy_interleaved(fi->buffer+1,fi->nsamples);
	}else{
		fi->energy_r=energy(fi->buffer,fi->nsamples);
		ms_message("energy=%g",fi->energy_r);
	}
}


/**
 * Utility that compares two PCM 16 bits audio files and returns a similarity factor between 0 and 1.
 * @param file1 a wav file path
 * @param file2 a wav file path
 * @param ret the similarity factor, set in return
 * @param min_overlap_p percentage of minimum overlap between the two signals, used to restrict the cross correlation around t=0.
 * @param func a callback called to show progress of the operation
 * @param user_data a user data passed to the callback when invoked.
 * @return -1 on error, 0 if succesful.
**/
int ms_audio_diff(const char *file1, const char *file2, double *ret, double min_overlap_p, MSAudioDiffProgressNotify func, void *user_data){
	FileInfo *fi1,*fi2;
	double *xcorr;
	int xcorr_size;
	int min_overlap;
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
		ms_error("Comparing files of different sampling rates is not supported");
		return -1;
	}
	
	if (fi1->nchannels!=fi2->nchannels){
		ms_error("Comparing files with different number of channels is not supported");
		return -1;
	}
	
	file_info_compute_energy(fi1);
	file_info_compute_energy(fi2);
	
	if (fi1->energy_r==0 || fi2->energy_r==0){
		/*avoid division by zero*/
		ms_error("One of the two files is pure silence.");
		return -1;
	}
	
	xcorr_size=fi1->nsamples+fi2->nsamples;
	min_overlap = MIN(fi1->nsamples, fi2->nsamples) * min_overlap_p / 100.0;
	xcorr=ms_new0(double,xcorr_size);
	if (fi1->nchannels == 2){
		max_index_r=compute_cross_correlation_interleaved(fi1->buffer,fi1->nsamples,fi2->buffer,fi2->nsamples,xcorr,xcorr_size, func, user_data, 0, min_overlap);
		max_r=xcorr[max_index_r];
		max_r/=(sqrt(fi1->energy_r)*sqrt(fi2->energy_r));
		max_index_l=compute_cross_correlation_interleaved(fi1->buffer,fi1->nsamples,fi2->buffer,fi2->nsamples,xcorr,xcorr_size, func, user_data, 1, min_overlap);
		max_l=xcorr[max_index_l];
		max_l/=(sqrt(fi1->energy_l)*sqrt(fi2->energy_l));
		ms_message("Max stereo cross-correlation obtained at position [%i,%i], similarity factor=%g,%g",max_index_r,max_index_l,max_r, max_l);
		
		*ret = 0.5 * (max_r + max_l) * (1 - (double)abs(max_index_r-max_index_l)/(double)xcorr_size); 
	}else{
		max_index_r=compute_cross_correlation(fi1->buffer,fi1->nsamples,fi2->buffer,fi2->nsamples,xcorr,xcorr_size, func, user_data, min_overlap);
		max_r=xcorr[max_index_r];
		max_r/=(sqrt(fi1->energy_r)*sqrt(fi2->energy_r));
		*ret=max_r;
		ms_message("Max cross-correlation obtained at position [%i], similarity factor=%g",max_index_r,*ret);
	}
	ms_free(xcorr);
	file_info_destroy(fi1);
        file_info_destroy(fi2);
	return 0;
}

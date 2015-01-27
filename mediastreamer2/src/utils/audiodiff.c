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
	int16_t *buffer;
	int nsamples;
	double energy;
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
	if (wave_header_get_channel(&header)==2){
		ms_error("stereo files are not supported");
		return NULL;
	}
	fi=ms_new0(FileInfo,1);
	size=stbuf.st_size-hsize;
	fi->nsamples=size/2;
	fi->buffer=ms_new0(int16_t,size);
	fi->rate=wave_header_get_rate(&header);
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
static int compute_cross_correlation(int16_t *s1, int n1, int16_t *s2, int n2, double *xcorr, int xcorr_nsamples, MSAudioDiffProgressNotify func, void *user_data){
	int64_t acc;
	int64_t max=0;
	int max_index=0;
	int i,j,k;
	int b1min,b2min;
	int completion=0;
	int prev_completion=0;
	
#define STEP 4
#define ACC(s) acc+=(int64_t)( (int)s1[j+s]*(int)s2[k+s]);
	for(i=0;i<xcorr_nsamples;++i){
		completion=100*i/xcorr_nsamples;
		acc=0;
		b1min=MAX(n1-i,0);
		b2min=MAX(0,i-n1);
		for(j=b1min,k=b2min; j<b1min+n1-STEP && k<b2min+n2-STEP; j+=STEP,k+=STEP){
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

static double energy(int16_t *s1, int n1){
	int i;
	int64_t ret=0;
	for(i=0;i<n1;++i){
		ret+=(int)s1[i]*(int)s1[i];
	}
	return (double)ret;
}

void file_info_compute_energy(FileInfo *fi){
	fi->energy=energy(fi->buffer,fi->nsamples);
	ms_message("energy=%g",fi->energy);
}


/**
 * Utility that compares two PCM 16 bits audio files and returns a similarity factor between 0 and 1.
**/
int ms_audio_diff(const char *file1, const char *file2, double *ret, MSAudioDiffProgressNotify func, void *user_data){
	FileInfo *fi1,*fi2;
	double *xcorr;
	int xcorr_size;
	int max_index;
	double max;
	
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
	
	file_info_compute_energy(fi1);
	file_info_compute_energy(fi2);
	
	if (fi1->energy==0 || fi2->energy==0){
		/*avoid division by zero*/
		ms_error("One of the two files is pure silence.");
		return -1;
	}
	
	xcorr_size=fi1->nsamples+fi2->nsamples;
	xcorr=ms_new0(double,xcorr_size);
	max_index=compute_cross_correlation(fi1->buffer,fi1->nsamples,fi2->buffer,fi2->nsamples,xcorr,xcorr_size, func, user_data);
	max=xcorr[max_index];
	ms_free(xcorr);
	*ret=max/(sqrt(fi1->energy)*sqrt(fi2->energy));
	ms_message("Max cross-correlation obtained at position [%i], similarity factor=%g",max_index,*ret);
	file_info_destroy(fi1);
        file_info_destroy(fi2);
	return 0;
}

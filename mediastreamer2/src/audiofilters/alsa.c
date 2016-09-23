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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#if defined(HAVE_CONFIG_H)
#include "mediastreamer-config.h"
#endif

#include <alsa/asoundlib.h>


#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/mssndcard.h"

static int forced_rate=-1;

void ms_alsa_card_set_forced_sample_rate(int samplerate){
	if (samplerate==0 || samplerate<-1) {
		ms_warning("ms_alsa_card_set_forced_sample_rate(): bad value %i",samplerate);
		return;
	}
	forced_rate=samplerate;
}

//#define THREADED_VERSION

/*in case of troubles with a particular driver, try incrementing ALSA_PERIOD_SIZE
to 512, 1024, 2048, 4096...
then try incrementing the number of periods*/
#define ALSA_PERIODS 8
#define ALSA_PERIOD_SIZE 256

/*uncomment the following line if you have problems with an alsa driver
having sound quality trouble:*/
/*#define EPIPE_BUGFIX 1*/

static MSSndCard * alsa_card_new(const char *id, const char *pcmbasename);
static MSSndCard *alsa_card_duplicate(MSSndCard *obj);
static MSFilter * ms_alsa_read_new(MSFactory *factory, const char *dev);
static MSFilter * ms_alsa_write_new(MSFactory *factory, const char *dev);


struct _AlsaData{
	char *pcmdev;
	char *mixdev;
};

typedef struct _AlsaData AlsaData;


static void alsa_resume(snd_pcm_t *handle){
	int err;
	snd_pcm_status_t *status=NULL;

	snd_pcm_status_alloca(&status);

	if ((err=snd_pcm_status(handle,status))!=0){
		ms_warning("snd_pcm_status() failed: %s",snd_strerror(err));
		return;
	}

	if (snd_pcm_status_get_state(status)==SND_PCM_STATE_SUSPENDED){
		ms_warning("Maybe suspended, trying resume");
		if ((err=snd_pcm_resume(handle))!=0){
			if (err!=EWOULDBLOCK) ms_warning("snd_pcm_resume() failed: %s",snd_strerror(err));
		}
	}
}

static int alsa_set_params(snd_pcm_t *pcm_handle, int rw, int bits, int stereo, int rate)
{
	snd_pcm_hw_params_t *hwparams=NULL;
	snd_pcm_sw_params_t *swparams=NULL;
	int dir;
	uint exact_uvalue;
	unsigned long exact_ulvalue;
	int channels;
	int periods=ALSA_PERIODS;
	int periodsize=ALSA_PERIOD_SIZE;
	snd_pcm_uframes_t buffersize;
	int err;
	int format;

	/* Allocate the snd_pcm_hw_params_t structure on the stack. */
	snd_pcm_hw_params_alloca(&hwparams);

	/* Init hwparams with full configuration space */
	if (snd_pcm_hw_params_any(pcm_handle, hwparams) < 0) {
		ms_warning("alsa_set_params: Cannot configure this PCM device.");
		return -1;
	}

	if (snd_pcm_hw_params_set_access(pcm_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
		ms_warning("alsa_set_params: Error setting access.");
		return -1;
	}
	/* Set sample format */
	format=SND_PCM_FORMAT_S16;
	if (snd_pcm_hw_params_set_format(pcm_handle, hwparams, format) < 0) {
		ms_warning("alsa_set_params: Error setting format.");
		return -1;
	}
	/* Set number of channels */
	if (stereo) channels=2;
	else channels=1;
	if (snd_pcm_hw_params_set_channels(pcm_handle, hwparams, channels) < 0) {
		ms_warning("alsa_set_params: Error setting channels.");
		return -1;
	}
	/* Set sample rate. If the exact rate is not supported */
	/* by the hardware, use nearest possible rate.         */
	exact_uvalue=rate;
	dir=0;
	if ((err=snd_pcm_hw_params_set_rate_near(pcm_handle, hwparams, &exact_uvalue, &dir))<0){
		ms_warning("alsa_set_params: Error setting rate to %i:%s",rate,snd_strerror(err));
		return -1;
	}
	if (dir != 0) {
		ms_warning("alsa_set_params: The rate %d Hz is not supported by your hardware.\n "
		"==> Using %d Hz instead.", rate, exact_uvalue);
	}
	/* choose greater period size when rate is high */
	periodsize=periodsize*(rate/8000);

	/* Set buffer size (in frames). The resulting latency is given by */
	/* latency = periodsize * periods / (rate * bytes_per_frame)     */
	/* set period size */
	exact_ulvalue=periodsize;
	dir=0;
	if (snd_pcm_hw_params_set_period_size_near(pcm_handle, hwparams, &exact_ulvalue, &dir) < 0) {
		ms_warning("alsa_set_params: Error setting period size.");
		return -1;
	}
	if (dir != 0) {
		ms_warning("alsa_set_params: The period size %d is not supported by your hardware.\n "
		"==> Using %d instead.", periodsize, (int)exact_ulvalue);
	}
	ms_warning("alsa_set_params: periodsize:%d Using %d", periodsize, (int)exact_ulvalue);
	periodsize=exact_ulvalue;
	/* Set number of periods. Periods used to be called fragments. */
	exact_uvalue=periods;
	dir=0;
	if (snd_pcm_hw_params_set_periods_near(pcm_handle, hwparams, &exact_uvalue, &dir) < 0) {
		ms_warning("alsa_set_params: Error setting periods.");
		return -1;
	}
	ms_warning("alsa_set_params: period:%d Using %d", periods, exact_uvalue);
	if (dir != 0) {
		ms_warning("alsa_set_params: The number of periods %d is not supported by your hardware.\n "
		"==> Using %d instead.", periods, exact_uvalue);
	}
	periods=exact_uvalue;

	if (snd_pcm_hw_params_get_buffer_size(hwparams, &buffersize)<0){
		buffersize=0;
		ms_warning("alsa_set_params: could not obtain hw buffer size.");
	}

	/* Apply HW parameter settings to */
	/* PCM device and prepare device  */
	if ((err=snd_pcm_hw_params(pcm_handle, hwparams)) < 0) {
		ms_warning("alsa_set_params: Error setting HW params:%s",snd_strerror(err));
		return -1;
	}
	/*prepare sw params */
	if (rw){
		snd_pcm_sw_params_alloca(&swparams);
		snd_pcm_sw_params_current(pcm_handle, swparams);
		//ms_message("periodsize=%i, buffersize=%i",(int) periodsize, (int)buffersize);
		if ((err=snd_pcm_sw_params_set_start_threshold(pcm_handle, swparams,periodsize*2 ))<0){
			ms_warning("alsa_set_params: Error setting start threshold:%s",snd_strerror(err));
		}
		if ((err=snd_pcm_sw_params_set_stop_threshold(pcm_handle, swparams,periodsize*periods))<0){
			ms_warning("alsa_set_params: Error setting stop threshold:%s",snd_strerror(err));
		}
		if ((err=snd_pcm_sw_params(pcm_handle, swparams))<0){
			ms_warning("alsa_set_params: Error setting SW params:%s",snd_strerror(err));
			return -1;
		}
	}
	return 0;
}

#ifdef EPIPE_BUGFIX
static void alsa_fill_w (snd_pcm_t *pcm_handle)
{
	snd_pcm_hw_params_t *hwparams=NULL;
	int channels;
		snd_pcm_uframes_t buffer_size;
	int buffer_size_bytes;
	void *buffer;

	/* Allocate the snd_pcm_hw_params_t structure on the stack. */
	snd_pcm_hw_params_alloca(&hwparams);
	snd_pcm_hw_params_current(pcm_handle, hwparams);

	/* get channels */
	snd_pcm_hw_params_get_channels (hwparams, &channels);

	/* get buffer size */
	snd_pcm_hw_params_get_buffer_size (hwparams, &buffer_size);

	/* fill half */
	buffer_size /= 2;

	/* allocate buffer assuming 2 bytes per sample */
	buffer_size_bytes = buffer_size * channels * 2;
	buffer = alloca (buffer_size_bytes);
	memset (buffer, 0, buffer_size_bytes);

	/* write data */
	snd_pcm_writei(pcm_handle, buffer, buffer_size);
}
#endif

static snd_pcm_t * alsa_open_r(const char *pcmdev,int bits,int stereo,int rate)
{
	snd_pcm_t *pcm_handle;
	int err;

	ms_message("alsa_open_r: opening %s at %iHz, bits=%i, stereo=%i",pcmdev,rate,bits,stereo);


#ifndef THREADED_VERSION
	if (snd_pcm_open(&pcm_handle, pcmdev,SND_PCM_STREAM_CAPTURE,SND_PCM_NONBLOCK) < 0) {
		ms_warning("alsa_open_r: Error opening PCM device %s",pcmdev );
		return NULL;
	}
#else
	/* want blocking mode for threaded version */
	if (snd_pcm_open(&pcm_handle, pcmdev,SND_PCM_STREAM_CAPTURE,0) < 0) {
		ms_warning("alsa_open_r: Error opening PCM device %s",pcmdev );
		return NULL;
	}
#endif
	{
	struct timeval tv1;
	struct timeval tv2;
	struct timezone tz;
	int diff = 0;
	err = gettimeofday(&tv1, &tz);
	while (1) {
		if (!(alsa_set_params(pcm_handle,0,bits,stereo,rate)<0)){
			ms_message("alsa_open_r: Audio params set");
			break;
		}
		if (!gettimeofday(&tv2, &tz) && !err) {
			diff = ((tv2.tv_sec - tv1.tv_sec) * 1000000) + (tv2.tv_usec - tv1.tv_usec);
		} else {
			diff = -1;
		}
		if ((diff < 0) || (diff > 3000000)) { /* 3 secondes */
			ms_error("alsa_open_r: Error setting params for more than 3 seconds");
			snd_pcm_close(pcm_handle);
			return NULL;
		}
		ms_warning("alsa_open_r: Error setting params (for %d micros)", diff);
		usleep(200000);
	}
	}

	err=snd_pcm_start(pcm_handle);
	if (err<0){
		ms_warning("snd_pcm_start() failed: %s", snd_strerror(err));
	}
	return pcm_handle;
}

static snd_pcm_t * alsa_open_w(const char *pcmdev,int bits,int stereo,int rate)
{
	snd_pcm_t *pcm_handle;

	ms_message("alsa_open_w: opening %s at %iHz, bits=%i, stereo=%i",pcmdev,rate,bits,stereo);

	if (snd_pcm_open(&pcm_handle, pcmdev,SND_PCM_STREAM_PLAYBACK,SND_PCM_NONBLOCK) < 0) {
		ms_warning("alsa_open_w: Error opening PCM device %s",pcmdev );
		return NULL;
	}
	alsa_resume(pcm_handle);
	{
	struct timeval tv1;
	struct timeval tv2;
	struct timezone tz;
	int diff = 0;
	int err;
	err = gettimeofday(&tv1, &tz);
	while (1) {
		if (!(alsa_set_params(pcm_handle,1,bits,stereo,rate)<0)){
			ms_message("alsa_open_w: Audio params set");
			break;
		}
		if (!gettimeofday(&tv2, &tz) && !err) {
			diff = ((tv2.tv_sec - tv1.tv_sec) * 1000000) + (tv2.tv_usec - tv1.tv_usec);
		} else {
			diff = -1;
		}
		if ((diff < 0) || (diff > 3000000)) { /* 3 secondes */
			ms_error("alsa_open_w: Error setting params for more than 3 seconds");
			snd_pcm_close(pcm_handle);
			return NULL;
		}
		ms_warning("alsa_open_w: Error setting params (for %d micros)", diff);
		usleep(200000);
	}
	}

	return pcm_handle;
}

static int alsa_can_read(snd_pcm_t *dev)
{
	snd_pcm_sframes_t avail;
	int err;

	alsa_resume(dev);
	avail = snd_pcm_avail_update(dev);
	/* A buggy driver does not return an error while being in Xrun */
	if (avail >= 0 && snd_pcm_state(dev) == SND_PCM_STATE_XRUN) avail=-EPIPE;
	if (avail < 0) {
		ms_error("snd_pcm_avail_update: %s", snd_strerror(avail));	// most probably -EPIPE
		/* overrun occured, snd_pcm_state() would return SND_PCM_STATE_XRUN
		 FIXME: handle other error conditions*/
		ms_error("*** alsa_can_read fixup, trying to recover");
		snd_pcm_drain(dev); /* Ignore possible error, at least -EAGAIN.*/
		err = snd_pcm_recover(dev, avail, 0);
		if (err){
			ms_error("snd_pcm_recover() failed with err %d: %s", err, snd_strerror(err));
			return -1;
		}
		err = snd_pcm_start(dev);
		if (err){
			ms_error("snd_pcm_start() failed with err %d: %s", err, snd_strerror(err));
			return -1;
		}
		ms_message("Recovery done");
	}
	return avail;
}


static int alsa_read(snd_pcm_t *handle,unsigned char *buf,int nsamples)
{
	int err;
	err=snd_pcm_readi(handle,buf,nsamples);
	if (err<0) {
		ms_warning("alsa_read: snd_pcm_readi() returned %i",err);
		if (err==-EPIPE){
			snd_pcm_prepare(handle);
			err=snd_pcm_readi(handle,buf,nsamples);
			if (err<0) ms_warning("alsa_read: snd_pcm_readi() failed:%s.",snd_strerror(err));
		}else if (err==-ESTRPIPE){
			alsa_resume(handle);
		}else if (err!=-EWOULDBLOCK){
			ms_warning("alsa_read: snd_pcm_readi() failed:%s.",snd_strerror(err));
		}
	}else if (err==0){
		ms_warning("alsa_read: snd_pcm_readi() returned 0");
	}
	return err;
}


static int alsa_write(snd_pcm_t *handle,unsigned char *buf,int nsamples)
{
	int err;
	if ((err=snd_pcm_writei(handle,buf,nsamples))<0){
		if (err==-EPIPE){
			snd_pcm_prepare(handle);
#ifdef EPIPE_BUGFIX
			alsa_fill_w (handle);
#endif
			err=snd_pcm_writei(handle,buf,nsamples);
			if (err<0) ms_warning("alsa_card_write: Error writing sound buffer (nsamples=%i):%s",nsamples,snd_strerror(err));
		}else if (err==-ESTRPIPE){
			alsa_resume(handle);
		}else if (err!=-EWOULDBLOCK){
			ms_warning("alsa_card_write: snd_pcm_writei() failed:%s.",snd_strerror(err));
		}
	}else if (err!=nsamples) {
		ms_debug("Only %i samples written instead of %i",err,nsamples);
	}
	return err;
}


static snd_mixer_t *alsa_mixer_open(const char *mixdev){
	snd_mixer_t *mixer=NULL;
	int err;
	err=snd_mixer_open(&mixer,0);
	if (err<0){
		ms_warning("Could not open alsa mixer: %s",snd_strerror(err));
		return NULL;
	}
	if ((err = snd_mixer_attach (mixer, mixdev)) < 0){
		ms_warning("Could not attach mixer to card: %s",snd_strerror(err));
		snd_mixer_close(mixer);
		return NULL;
	}
	if ((err = snd_mixer_selem_register (mixer, NULL, NULL)) < 0){
		ms_warning("snd_mixer_selem_register: %s",snd_strerror(err));
		snd_mixer_close(mixer);
		return NULL;
	}
	if ((err = snd_mixer_load (mixer)) < 0){
		ms_warning("snd_mixer_load: %s",snd_strerror(err));
		snd_mixer_close(mixer);
		return NULL;
	}
	return mixer;
}

static void alsa_mixer_close(snd_mixer_t *mix){
	snd_mixer_close(mix);
}

typedef enum {CAPTURE, PLAYBACK, CAPTURE_SWITCH, PLAYBACK_SWITCH} MixerAction;

static int get_mixer_element(snd_mixer_t *mixer,const char *name, MixerAction action){
	long value=0;
	const char *elemname;
	snd_mixer_elem_t *elem;
	int err;
	long sndMixerPMin=0;
	long sndMixerPMax=0;
	long newvol=0;
	elem=snd_mixer_first_elem(mixer);
	while (elem!=NULL){
		elemname=snd_mixer_selem_get_name(elem);
		//ms_message("Found alsa mixer element %s.",elemname);
		if (strcmp(elemname,name)==0){
			switch (action){
				case CAPTURE:
				if (snd_mixer_selem_has_capture_volume(elem)){
					snd_mixer_selem_get_capture_volume_range(elem, &sndMixerPMin, &sndMixerPMax);
					err=snd_mixer_selem_get_capture_volume(elem,SND_MIXER_SCHN_UNKNOWN,&newvol);
					newvol-=sndMixerPMin;
					value=(100*newvol)/(sndMixerPMax-sndMixerPMin);
					if (err<0) ms_warning("Could not get capture volume for %s:%s",name,snd_strerror(err));
					//else ms_message("Successfully get capture level for %s.",elemname);
					break;
				}
				break;
				case PLAYBACK:
				if (snd_mixer_selem_has_playback_volume(elem)){
					snd_mixer_selem_get_playback_volume_range(elem, &sndMixerPMin, &sndMixerPMax);
					err=snd_mixer_selem_get_playback_volume(elem,SND_MIXER_SCHN_FRONT_LEFT,&newvol);
					newvol-=sndMixerPMin;
					value=(100*newvol)/(sndMixerPMax-sndMixerPMin);
					if (err<0) ms_warning("Could not get playback volume for %s:%s",name,snd_strerror(err));
					//else ms_message("Successfully get playback level for %s.",elemname);
					break;
				}
				break;
				case CAPTURE_SWITCH:

				break;
				case PLAYBACK_SWITCH:

				break;
			}
		}
		elem=snd_mixer_elem_next(elem);
	}

	return value;
}


static void set_mixer_element(snd_mixer_t *mixer,const char *name, int level,MixerAction action){
	const char *elemname;
	snd_mixer_elem_t *elem;
	long sndMixerPMin=0;
	long sndMixerPMax=0;
	long newvol=0;

	elem=snd_mixer_first_elem(mixer);

	while (elem!=NULL){
		elemname=snd_mixer_selem_get_name(elem);
		//ms_message("Found alsa mixer element %s.",elemname);
		if (strcmp(elemname,name)==0){
			switch(action){
				case CAPTURE:
				if (snd_mixer_selem_has_capture_volume(elem)){
					snd_mixer_selem_get_capture_volume_range(elem, &sndMixerPMin, &sndMixerPMax);
					newvol=(((sndMixerPMax-sndMixerPMin)*level)/100)+sndMixerPMin;
					snd_mixer_selem_set_capture_volume_all(elem,newvol);
					//ms_message("Successfully set capture level for %s.",elemname);
					return;
				}
				break;
				case PLAYBACK:
				if (snd_mixer_selem_has_playback_volume(elem)){
					snd_mixer_selem_get_playback_volume_range(elem, &sndMixerPMin, &sndMixerPMax);
					newvol=(((sndMixerPMax-sndMixerPMin)*level)/100)+sndMixerPMin;
					snd_mixer_selem_set_playback_volume_all(elem,newvol);
					//ms_message("Successfully set playback level for %s.",elemname);
					return;
				}
				break;
				case CAPTURE_SWITCH:
				if (snd_mixer_selem_has_capture_switch(elem)){
					snd_mixer_selem_set_capture_switch_all(elem,level);
					//ms_message("Successfully set capture switch for %s.",elemname);
				}
				break;
				case PLAYBACK_SWITCH:
				if (snd_mixer_selem_has_playback_switch(elem)){
					snd_mixer_selem_set_playback_switch_all(elem,level);
					//ms_message("Successfully set capture switch for %s.",elemname);
				}
				break;

			}
		}
		elem=snd_mixer_elem_next(elem);
	}

	return ;
}


static void alsa_card_set_level(MSSndCard *obj,MSSndCardMixerElem e,int a)
{
	snd_mixer_t *mixer;
	AlsaData *ad=(AlsaData*)obj->data;
	mixer=alsa_mixer_open(ad->mixdev);
	if (mixer==NULL) return ;
	switch(e){
		case MS_SND_CARD_MASTER:
			set_mixer_element(mixer,"Master",a,PLAYBACK);
		break;
		case MS_SND_CARD_CAPTURE:
			set_mixer_element(mixer,"Capture",a,CAPTURE);
		break;
		case MS_SND_CARD_PLAYBACK:
			set_mixer_element(mixer,"PCM",a,PLAYBACK);
		break;
		default:
			ms_warning("alsa_card_set_level: unsupported command.");
	}
	alsa_mixer_close(mixer);
}

static int alsa_card_get_level(MSSndCard *obj, MSSndCardMixerElem e)
{
	snd_mixer_t *mixer;
	AlsaData *ad=(AlsaData*)obj->data;
	int value = -1;
	mixer=alsa_mixer_open(ad->mixdev);
	if (mixer==NULL) return 0;
	switch(e){
		case MS_SND_CARD_MASTER:
			value=get_mixer_element(mixer,"Master",PLAYBACK);
			break;
		case MS_SND_CARD_CAPTURE:
			value=get_mixer_element(mixer,"Capture",CAPTURE);
			break;
		case MS_SND_CARD_PLAYBACK:
			value=get_mixer_element(mixer,"PCM",PLAYBACK);
			break;
		default:
			ms_warning("alsa_card_set_level: unsupported command.");
	}
	alsa_mixer_close(mixer);
	return value;
}

static void alsa_card_set_source(MSSndCard *obj,MSSndCardCapture source)
{
	snd_mixer_t *mixer;
	AlsaData *ad=(AlsaData*)obj->data;
	mixer=alsa_mixer_open(ad->mixdev);
	if (mixer==NULL) return;
	switch (source){
		case MS_SND_CARD_MIC:
			set_mixer_element(mixer,"Mic",1,CAPTURE_SWITCH);
			set_mixer_element(mixer,"Capture",1,CAPTURE_SWITCH);
			break;
		case MS_SND_CARD_LINE:
			set_mixer_element(mixer,"Line",1,CAPTURE_SWITCH);
			set_mixer_element(mixer,"Capture",1,CAPTURE_SWITCH);
			break;
	}
	alsa_mixer_close(mixer);
}

static MSFilter *alsa_card_create_reader(MSSndCard *card)
{
	AlsaData *ad=(AlsaData*)card->data;
	MSFilter *f=ms_alsa_read_new(ms_snd_card_get_factory(card), ad->pcmdev);
	return f;
}

static MSFilter *alsa_card_create_writer(MSSndCard *card)
{
	AlsaData *ad=(AlsaData*)card->data;
	MSFilter *f=ms_alsa_write_new(ms_snd_card_get_factory(card), ad->pcmdev);
	return f;
}


void alsa_error_log_handler(const char *file, int line, const char *function, int err, const char *fmt, ...) {
	char * format = ms_strdup_printf("also error in %s:%d - %s", file, line, fmt);
	va_list args;
	va_start (args, fmt);
	ortp_logv(ORTP_LOG_DOMAIN, ORTP_MESSAGE, format, args);
	va_end (args);
	ms_free(format);
}

static void alsa_card_init(MSSndCard *obj){
	AlsaData *ad=ms_new0(AlsaData,1);
	obj->data=ad;
	snd_lib_error_set_handler(alsa_error_log_handler);

}

static void alsa_card_uninit(MSSndCard *obj){
	AlsaData *ad=(AlsaData*)obj->data;
	if (ad->pcmdev!=NULL) ms_free(ad->pcmdev);
	if (ad->mixdev!=NULL) ms_free(ad->mixdev);
	ms_free(ad);
}

static void alsa_card_detect(MSSndCardManager *m){
	int i,j,k;
	void **hints=NULL;
	int hint_device_count=0;
	int device_count=0;
	const char *pcm_base_name = "plug";
	bool_t found_duplicate;
	const int MAX_NUM_DEVICE_ID = 100;
	const int MAX_PLUG_NAME_CHARS = 50;
	char *plug_names[MAX_NUM_DEVICE_ID];
	char *card_names[MAX_NUM_DEVICE_ID], *device_names[MAX_NUM_DEVICE_ID];
	char *unique_card_names[MAX_NUM_DEVICE_ID], *unique_device_names[MAX_NUM_DEVICE_ID];

	memset(plug_names, 0, sizeof(plug_names));

	/* Get list of devices from alsa device hints */
	if (snd_device_name_hint(-1, "pcm", &hints)==0){
		for(i=0; hints[i]!=NULL; ++i){
			char *hint = snd_device_name_get_hint(hints[i],"NAME");
			char *hint_ptr = hint;
			if (hint != NULL) {
				char *device_name = strsep(&hint, ":");
				if (device_name){
					char *card_hint = strsep(&hint, ",");
					if (card_hint){
						if (strcmp(strsep(&card_hint, "="), "CARD")==0){
							char *card_name = card_hint;
							card_names[hint_device_count] = ms_strdup(card_name);
							device_names[hint_device_count] = ms_strdup(device_name);
							hint_device_count++;
						}
					}
				}
				free(hint_ptr);
			}
		}
		snd_device_name_free_hint(hints);
	}

	/* Produce unique device_names[] */

	for (j=0; j<hint_device_count; j++){
		found_duplicate = FALSE;
		if (j == 0){
			unique_card_names[0] = card_names[0];
			unique_device_names[0] = device_names[0];
			plug_names[device_count] = (char*) ms_new(char, MAX_PLUG_NAME_CHARS);
                        sprintf(plug_names[0],"%s:%s", device_names[0], card_names[0]);

			device_count++;
			continue;
		}
		for (k=0; k<device_count; k++){

			if ( (strcmp(card_names[j], unique_card_names[k])==0) && (strcmp(device_names[j], unique_device_names[k])==0) ){
				found_duplicate = TRUE;
				break;
			}
		}
		if ( !found_duplicate ){
			unique_card_names[device_count] = card_names[j];
			unique_device_names[device_count] = device_names[j];

			plug_names[device_count] = (char*) ms_new(char,MAX_PLUG_NAME_CHARS);
			sprintf(plug_names[device_count],"%s:%s", device_names[j], card_names[j]);

			device_count++;
		}
	}

	/* Add unique devices to card manager */

	for (i=-1;i<device_count;i++){
		MSSndCard *card=NULL;
		if (i==-1){
			card=alsa_card_new("default", pcm_base_name);
		}
		else {
			// Currently, only 'default' devices are supported.
			if (strcmp(unique_device_names[i], "default")==0) {
				card=alsa_card_new(plug_names[i], pcm_base_name);
			}
		}

		if (card!=NULL)
		{
			ms_snd_card_manager_add_card(m,card);
		}
	}

	atexit((void(*)(void))snd_config_update_free_global);

	for (i=0; i<hint_device_count; i++) {
		if (plug_names[i]) ms_free(plug_names[i]);
		ms_free(device_names[i]);
		ms_free(card_names[i]);
	}
}

MSSndCardDesc alsa_card_desc={
	.driver_type="ALSA",
	.detect=alsa_card_detect,
	.init=alsa_card_init,
	.set_level=alsa_card_set_level,
	.get_level=alsa_card_get_level,
	.set_capture=alsa_card_set_source,
	.set_control=NULL,
	.get_control=NULL,
	.create_reader=alsa_card_create_reader,
	.create_writer=alsa_card_create_writer,
	.uninit=alsa_card_uninit,
	.duplicate=alsa_card_duplicate
};

static MSSndCard *alsa_card_duplicate(MSSndCard *obj){
	MSSndCard *card=ms_snd_card_new(&alsa_card_desc);
	AlsaData* dcard=(AlsaData*)card->data;
	AlsaData* dobj=(AlsaData*)obj->data;
	card->name=ms_strdup(obj->name);
	card->id=ms_strdup(obj->id);
	dcard->pcmdev=ms_strdup(dobj->pcmdev);
	dcard->mixdev=ms_strdup(dobj->mixdev);
	return card;
}

MSSndCard * ms_alsa_card_new_custom(const char *pcmdev, const char *mixdev){
	MSSndCard * obj;
	AlsaData *ad;
	obj=ms_snd_card_new(&alsa_card_desc);
	ad=(AlsaData*)obj->data;
	obj->name=ms_strdup(pcmdev);
	ad->pcmdev=ms_strdup(pcmdev);
	ad->mixdev=ms_strdup(mixdev);
	return obj;
}

static char *get_card_name(snd_pcm_t *handle, unsigned int *card_index_ret){
	snd_pcm_info_t *info=NULL;
	unsigned int card_index=-1;
	char *ret = NULL;

	snd_pcm_info_malloc(&info);
	if (snd_pcm_info(handle, info)==0){
		card_index = snd_pcm_info_get_card(info);
	}
	snd_pcm_info_free(info);
	if (card_index != (unsigned int) -1){
		char *name=NULL;
		*card_index_ret = card_index;
		snd_card_get_name(card_index, &name);
		if (name){
			/* remove trailing spaces from card name */
			char *pos2;
			ret =ms_strdup(name);
			pos2 = ret + strlen(ret) - 1;
			for (; pos2>name && *pos2==' '; pos2--) *pos2='\0';
			free(name);
		}
	}
	if (!ret) ret = ms_strdup("default");
	return ret;
}

static unsigned int get_card_capabilities(const char *devname, char **card_name, unsigned int *card_index){
	snd_pcm_t *pcm_handle;
	unsigned int ret = 0;

	*card_name = NULL;
	if (snd_pcm_open(&pcm_handle,devname,SND_PCM_STREAM_CAPTURE,SND_PCM_NONBLOCK)==0) {
		*card_name = get_card_name(pcm_handle, card_index);
		ret|=MS_SND_CARD_CAP_CAPTURE;
		snd_pcm_close(pcm_handle);
	}
	if (snd_pcm_open(&pcm_handle,devname,SND_PCM_STREAM_PLAYBACK,SND_PCM_NONBLOCK)==0) {
		if (*card_name == NULL) *card_name = get_card_name(pcm_handle, card_index);
		ret|=MS_SND_CARD_CAP_PLAYBACK;
		snd_pcm_close(pcm_handle);
	}
	return ret;
}

static MSSndCard * alsa_card_new(const char *id, const char *pcmbasename){
	MSSndCard * obj;
	AlsaData *ad;
	unsigned int hw_index=0;

	obj=ms_snd_card_new(&alsa_card_desc);
	ad=(AlsaData*)obj->data;
	if (strcmp(id,"default")==0) {
		/* the default pcm device */
		obj->name=ms_strdup("default device");
		ad->pcmdev=ms_strdup("default");
		ad->mixdev=ms_strdup("default");
	}else{
		snd_mixer_t *mixer;
		ad->pcmdev=ms_strdup_printf("%s:%s",pcmbasename, id);
		obj->capabilities = get_card_capabilities(ad->pcmdev, &obj->name, &hw_index);
		if (obj->capabilities == 0){
			ms_snd_card_destroy(obj);
			return NULL;
		}
		ad->mixdev=ms_strdup_printf("hw:%i",hw_index);
		mixer = alsa_mixer_open(ad->mixdev);
		if (!mixer) ms_warning("Fail to get a mixer for device %s", ad->mixdev);
		else alsa_mixer_close(mixer);
	}
	return obj;
}

struct _AlsaReadData{
	char *pcmdev;
	snd_pcm_t *handle;
	int rate;
	int nchannels;
	uint64_t read_samples;
	MSTickerSynchronizer *ticker_synchronizer;
	bool_t read_started;
	bool_t write_started;

#ifdef THREADED_VERSION
	ms_thread_t thread;
	ms_mutex_t mutex;
	MSBufferizer * bufferizer;
#endif
};

typedef struct _AlsaReadData AlsaReadData;

void alsa_read_init(MSFilter *obj){
	AlsaReadData *ad=ms_new0(AlsaReadData,1);
	ad->pcmdev=NULL;
	ad->handle=NULL;
	ad->rate=forced_rate!=-1 ? forced_rate : 8000;
	ad->nchannels=1;
	ad->ticker_synchronizer = ms_ticker_synchronizer_new();
	obj->data=ad;

#ifdef THREADED_VERSION
	ad->read_started=FALSE;
	ad->write_started=FALSE;
	ad->bufferizer=ms_bufferizer_new();
	ms_mutex_init(&ad->mutex,NULL);
	ad->thread=0;
#endif
}

#ifdef THREADED_VERSION

static void * alsa_write_thread(void *p){
	AlsaReadData *ad=(AlsaReadData*)p;
	int samples=(160*ad->rate)/8000;
	int err;
	int count=0;
	mblk_t *om=NULL;
	struct timeval timeout;
	if (ad->handle==NULL && ad->pcmdev!=NULL){
		ad->handle=alsa_open_r(ad->pcmdev,16,ad->nchannels==2,ad->rate);
	}
	if (ad->handle==NULL) return NULL;

	while (ad->read_started)
	  {
		count = alsa_can_read(ad->handle,samples);
		if (count==24)
		  { /* keep this value for this driver */ }
		else if (count<=0)
		  {
		count = samples;
		  }
		else if (count>0)
		  {
		//ms_warning("%i count", count);
		//count = samples;
		  }

		int size=count*2;
		om=allocb(size,0);

		if ((err=alsa_read(ad->handle,om->b_wptr,count))<=0)
		  {
		ms_warning("nothing to read");
		//ms_warning("Fail to read samples %i", count);
		freemsg(om); /* leak fixed */
		continue;
		  }
		//ms_warning(" read %i", err);

		size=err*2;
		om->b_wptr+=size;

		ms_mutex_lock(&ad->mutex);
		ms_bufferizer_put(ad->bufferizer,om);
		ms_mutex_unlock(&ad->mutex);

		if (count==24)
		  {
		timeout.tv_sec = 0;
		timeout.tv_usec = 2000;
		select(0, 0, NULL, NULL, &timeout );
		  }
		else
		  {
		/* select will be less active than locking on "read" */
		timeout.tv_sec = 0;
		timeout.tv_usec = 5000;
		select(0, 0, NULL, NULL, &timeout );
		  }
	  }

	if (ad->handle!=NULL) snd_pcm_close(ad->handle);
	ad->handle=NULL;
	return NULL;
}

static void alsa_start_r(AlsaReadData *d){
	if (d->read_started==FALSE){
		d->read_started=TRUE;
		ms_thread_create(&d->thread,NULL,alsa_write_thread,d);
	}else d->read_started=TRUE;
}

static void alsa_stop_r(AlsaReadData *d){
	d->read_started=FALSE;
	if (d->thread!=0)
	  {
		ms_thread_join(d->thread,NULL);
		d->thread=0;
	  }
}
#endif

static void compute_timespec(AlsaReadData *d) {
	static int count = 0;
	uint64_t ns = ((1000 * d->read_samples) / (uint64_t) d->rate) * 1000000;
	double av_skew;
	MSTimeSpec ts;
	ts.tv_nsec = ns % 1000000000;
	ts.tv_sec = ns / 1000000000;
	av_skew = ms_ticker_synchronizer_set_external_time(d->ticker_synchronizer, &ts);
	if ((++count) % 100 == 0)
		ms_message("sound/wall clock skew is average=%f ms", av_skew);
}

void alsa_read_preprocess(MSFilter *obj){
#ifdef THREADED_VERSION
	AlsaReadData *ad=(AlsaReadData*)obj->data;
	alsa_start_r(ad);
#endif
}


void alsa_read_postprocess(MSFilter *obj){
	AlsaReadData *ad=(AlsaReadData*)obj->data;
#ifdef THREADED_VERSION
	alsa_stop_r(ad);
#endif
	ms_ticker_set_time_func(obj->ticker,NULL,NULL);
	if (ad->handle!=NULL) snd_pcm_close(ad->handle);
	ad->read_started=FALSE;
	ad->handle=NULL;
}

void alsa_read_uninit(MSFilter *obj){
	AlsaReadData *ad=(AlsaReadData*)obj->data;
#ifdef THREADED_VERSION
	alsa_stop_r(ad);
#endif
	if (ad->pcmdev!=NULL) ms_free(ad->pcmdev);
	if (ad->handle!=NULL) snd_pcm_close(ad->handle);
#ifdef THREADED_VERSION
	ms_bufferizer_destroy(ad->bufferizer);
	ms_mutex_destroy(&ad->mutex);
#endif
	ms_ticker_synchronizer_destroy(ad->ticker_synchronizer);
	ms_free(ad);
}

#ifndef THREADED_VERSION
void alsa_read_process(MSFilter *obj){
	AlsaReadData *ad=(AlsaReadData*)obj->data;
	int samples=(128*ad->rate)/8000;
	int err;
	mblk_t *om=NULL;
	if (ad->handle==NULL && ad->pcmdev!=NULL && !ad->read_started){
		ad->read_started=TRUE;
		ad->handle=alsa_open_r(ad->pcmdev,16,ad->nchannels==2,ad->rate);
		if (ad->handle){
			ad->read_samples=0;
			ms_ticker_set_time_func(obj->ticker,(uint64_t (*)(void*))ms_ticker_synchronizer_get_corrected_time, ad->ticker_synchronizer);
		}
	}
	if (ad->handle==NULL) return;
	while (alsa_can_read(ad->handle)>=samples){

		int size=samples*2*ad->nchannels;
		om=allocb(size,0);
		if ((err=alsa_read(ad->handle,om->b_wptr,samples))<=0) {
			ms_warning("Fail to read samples");
			freemsg(om);
			return;
		}
		ad->read_samples+=err;
		size=err*2*ad->nchannels;
		om->b_wptr+=size;
		compute_timespec(ad);

		/*ms_message("alsa_read_process: Outputing %i bytes",size);*/
		ms_queue_put(obj->outputs[0],om);
	}
}
#endif

#ifdef THREADED_VERSION
void alsa_read_process(MSFilter *obj){
	AlsaReadData *ad=(AlsaReadData*)obj->data;
	mblk_t *om=NULL;
	int samples=(160*ad->rate)/8000;
	int size=samples*2*ad->nchannels;

	ms_mutex_lock(&ad->mutex);
	while (ms_bufferizer_get_avail(ad->bufferizer)>=size){

	  om=allocb(size,0);
	  ms_bufferizer_read(ad->bufferizer,om->b_wptr,size);
	  om->b_wptr+=size;
	  /*ms_message("alsa_read_process: Outputing %i bytes",size);*/
	  ms_queue_put(obj->outputs[0],om);
	}
	ms_mutex_unlock(&ad->mutex);
}
#endif

static int alsa_read_get_sample_rate(MSFilter *obj, void *param){
	AlsaReadData *ad=(AlsaReadData*)obj->data;
	*((int*)param)=ad->rate;
	return 0;
}

static int alsa_read_set_sample_rate(MSFilter *obj, void *param){
	AlsaReadData *ad=(AlsaReadData*)obj->data;
	if (forced_rate!=-1) return -1;
	ad->rate=*((int*)param);
	return 0;
}

static int alsa_read_get_nchannels(MSFilter *obj, void *param) {
	AlsaReadData *ad = (AlsaReadData *)obj->data;
	*((int *)param) = ad->nchannels;
	return 0;
}

static int alsa_read_set_nchannels(MSFilter *obj, void *param){
	AlsaReadData *ad=(AlsaReadData*)obj->data;
	ad->nchannels=*((int*)param);
	return 0;
}

MSFilterMethod alsa_read_methods[]={
	{MS_FILTER_GET_SAMPLE_RATE,	alsa_read_get_sample_rate},
	{MS_FILTER_SET_SAMPLE_RATE, alsa_read_set_sample_rate},
	{MS_FILTER_GET_NCHANNELS, alsa_read_get_nchannels},
	{MS_FILTER_SET_NCHANNELS, alsa_read_set_nchannels},
	{0,NULL}
};

MSFilterDesc alsa_read_desc={
	.id=MS_ALSA_READ_ID,
	.name="MSAlsaRead",
	.text=N_("Alsa sound source"),
	.category=MS_FILTER_OTHER,
	.ninputs=0,
	.noutputs=1,
	.init=alsa_read_init,
	.preprocess=alsa_read_preprocess,
	.process=alsa_read_process,
	.postprocess=alsa_read_postprocess,
	.uninit=alsa_read_uninit,
	.methods=alsa_read_methods
};

static MSFilter * ms_alsa_read_new(MSFactory *factory, const char *dev){
	MSFilter *f=ms_factory_create_filter_from_desc(factory, &alsa_read_desc);
	AlsaReadData *ad=(AlsaReadData*)f->data;
	ad->pcmdev=ms_strdup(dev);
	return f;
}

typedef struct _AlsaReadData AlsaWriteData;

void alsa_write_init(MSFilter *obj){
	AlsaWriteData *ad=ms_new0(AlsaWriteData,1);
	ad->pcmdev=NULL;
	ad->handle=NULL;
	ad->rate=forced_rate!=-1 ? forced_rate : 8000;
	ad->nchannels=1;
	obj->data=ad;
}

void alsa_write_postprocess(MSFilter *obj){
	AlsaReadData *ad=(AlsaReadData*)obj->data;
	if (ad->handle!=NULL) snd_pcm_close(ad->handle);
	ad->write_started=FALSE;
	ad->handle=NULL;
}

void alsa_write_uninit(MSFilter *obj){
	AlsaWriteData *ad=(AlsaWriteData*)obj->data;
	if (ad->pcmdev!=NULL) ms_free(ad->pcmdev);
	if (ad->handle!=NULL) snd_pcm_close(ad->handle);
	ms_free(ad);
}

static int alsa_write_get_sample_rate(MSFilter *obj, void *data){
	AlsaWriteData *ad=(AlsaWriteData*)obj->data;
	*((int*)data)=ad->rate;
	return 0;
}

int alsa_write_set_sample_rate(MSFilter *obj, void *data){
	int *rate=(int*)data;
	AlsaWriteData *ad=(AlsaWriteData*)obj->data;
	if (forced_rate!=-1) return -1;
	ad->rate=*rate;
	return 0;
}

int alsa_write_get_nchannels(MSFilter *obj, void *data) {
	AlsaWriteData *ad = (AlsaWriteData *)obj->data;
	*((int *)data) = ad->nchannels;
	return 0;
}

int alsa_write_set_nchannels(MSFilter *obj, void *data){
	int *n=(int*)data;
	AlsaWriteData *ad=(AlsaWriteData*)obj->data;
	ad->nchannels=*n;
	return 0;
}

void alsa_write_process(MSFilter *obj){
	AlsaWriteData *ad=(AlsaWriteData*)obj->data;
	mblk_t *im=NULL;
	int size;
	int samples;
	int err;
	if (ad->handle==NULL && ad->pcmdev!=NULL && !ad->write_started){
		ad->write_started=TRUE;
		ad->handle=alsa_open_w(ad->pcmdev,16,ad->nchannels==2,ad->rate);
#ifdef EPIPE_BUGFIX
		alsa_fill_w (ad->pcmdev);
#endif
	}
	if (ad->handle==NULL) {
		ms_queue_flush(obj->inputs[0]);
		return;
	}
	while ((im=ms_queue_get(obj->inputs[0]))!=NULL){
		while((size=im->b_wptr-im->b_rptr)>0){
			samples=size/(2*ad->nchannels);
			err=alsa_write(ad->handle,im->b_rptr,samples);
			if (err>0) {
				im->b_rptr+=err*(2*ad->nchannels);
			}
			else break;
		}
		freemsg(im);
	}
}

MSFilterMethod alsa_write_methods[]={
	{MS_FILTER_GET_SAMPLE_RATE,	alsa_write_get_sample_rate},
	{MS_FILTER_SET_SAMPLE_RATE, alsa_write_set_sample_rate},
	{MS_FILTER_GET_NCHANNELS, alsa_write_get_nchannels},
	{MS_FILTER_SET_NCHANNELS, alsa_write_set_nchannels},
	{0,NULL}
};

MSFilterDesc alsa_write_desc={
	.id=MS_ALSA_WRITE_ID,
	.name="MSAlsaWrite",
	.text=N_("Alsa sound output"),
	.category=MS_FILTER_OTHER,
	.ninputs=1,
	.noutputs=0,
	.init=alsa_write_init,
	.process=alsa_write_process,
	.postprocess=alsa_write_postprocess,
	.uninit=alsa_write_uninit,
	.methods=alsa_write_methods
};


static MSFilter * ms_alsa_write_new(MSFactory *factory, const char *dev){
	MSFilter *f = ms_factory_create_filter_from_desc(factory, &alsa_write_desc);
	AlsaWriteData *ad=(AlsaWriteData*)f->data;
	ad->pcmdev=ms_strdup(dev);
	return f;
}


MS_FILTER_DESC_EXPORT(alsa_write_desc)

MS_FILTER_DESC_EXPORT(alsa_read_desc)


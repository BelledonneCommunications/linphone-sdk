/*
 silk_enc.c
 Copyright (C) 2011 Belledonne Communications, Grenoble, France
 
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
#include "mediastreamer2/msfilter.h"
#include "silk/SKP_Silk_SDK_API.h"

/* Define codec specific settings */
#define MAX_BYTES_PER_FRAME     250 // Equals peak bitrate of 100 kbps 
#define MAX_INPUT_FRAMES        5

/*filter common method*/
struct silk_enc_struct {
	SKP_SILK_SDK_EncControlStruct control;
	void* psEnc;
	uint32_t ts;
	MSBufferizer *bufferizer;
	unsigned char ptime;
	unsigned char max_ptime;
	unsigned int max_network_bitrate;
};

static void filter_init(MSFilter *f){
    struct silk_enc_struct* obj;
	f->data = ms_new0(struct silk_enc_struct,1);
	obj = (struct silk_enc_struct*) f->data;
	SKP_int16 ret;
	SKP_int32 encSizeBytes;
	
    /* Create encoder */
    ret = SKP_Silk_SDK_Get_Encoder_Size(&encSizeBytes );
    if( ret ) {
        ms_error("SKP_Silk_SDK_Get_Encoder_Size returned %i", ret );
    }
    obj->psEnc = ms_malloc(encSizeBytes);
    /* Reset decoder */
    ret = SKP_Silk_SDK_InitEncoder(obj->psEnc,&obj->control);
    if(ret) {
        ms_error( "SKP_Silk_SDK_InitEncoder returned %i", ret );
    }
	obj->ptime=20;
	obj->max_ptime=100;
	obj->bufferizer=ms_bufferizer_new();
	obj->control.useInBandFEC=1;
	obj->control.complexity=1;
	obj->control.packetLossPercentage=5;
}

static void filter_preprocess(MSFilter *f){

    
}

static void filter_process(MSFilter *f){
	mblk_t *im;
	mblk_t *om=NULL;
	SKP_int16 ret;
	SKP_int16 nBytes;
	uint8_t * buff=NULL;
	struct silk_enc_struct* obj= (struct silk_enc_struct*) f->data;
	obj->control.packetSize = obj->control.API_sampleRate*obj->ptime/1000; /*in sample*/
	
	while((im=ms_queue_get(f->inputs[0]))!=NULL){
		ms_bufferizer_put(obj->bufferizer,im);
	}
	while(ms_bufferizer_get_avail(obj->bufferizer)>=obj->control.packetSize*2){
		/* max payload size */
        nBytes = MAX_BYTES_PER_FRAME * MAX_INPUT_FRAMES;			
		om = allocb(nBytes,0);
		if (!buff) buff=ms_malloc(obj->control.packetSize*2);
		ms_bufferizer_read(obj->bufferizer,buff,obj->control.packetSize*2);
		ret = SKP_Silk_SDK_Encode(obj->psEnc
								  , &obj->control
								  , (const SKP_int16*)buff 
								  , (SKP_int16)(obj->control.packetSize)
								  , om->b_wptr
								  , &nBytes );
		if(ret) {
			ms_error( "SKP_Silk_Encode returned %i", ret );
			freeb(om);
		} else  if (nBytes > 0) {
			obj->ts+=obj->control.packetSize;
			om->b_wptr+=nBytes;	
			mblk_set_timestamp_info(om,obj->ts);
			ms_queue_put(f->outputs[0],om);
			om=NULL;
		} 
		
	}
	if (buff!=NULL) {
		ms_free(buff);
	}

}

static void filter_postprocess(MSFilter *f){
    
}

static void filter_unit(MSFilter *f){
    struct silk_enc_struct* obj= (struct silk_enc_struct*) f->data;
	ms_bufferizer_destroy(obj->bufferizer);
	ms_free(obj->psEnc);
	ms_free(f->data);
}


/*filter specific method*/

static int filter_set_sample_rate(MSFilter *f, void *arg) {
	struct silk_enc_struct* obj= (struct silk_enc_struct*) f->data;
	switch (*(SKP_int32*)arg) {
		case 8000:
		case 12000:
		case 16000:
		case 24000:
			obj->control.maxInternalSampleRate=*(SKP_int32*)arg;
			obj->control.API_sampleRate=*(SKP_int32*)arg;
			break;
		default:
			ms_warning("unsupported max sampling rate [%i] for silk, using 16 000",*(SKP_int32*)arg);
			obj->control.API_sampleRate=obj->control.maxInternalSampleRate=16000;
	}

	return 0;
}

static int filter_get_sample_rate(MSFilter *f, void *arg) {
	struct silk_enc_struct* obj= (struct silk_enc_struct*) f->data;
    *(int*)arg = obj->control.maxInternalSampleRate;
	return 0;
}

static int filter_add_fmtp(MSFilter *f, void *arg){
	char buf[64];
	struct silk_enc_struct* obj= (struct silk_enc_struct*) f->data;
	const char *fmtp=(const char *)arg;
	buf[0] ='\0';
	
	if (fmtp_get_value(fmtp,"maxptime:",buf,sizeof(buf))){
		obj->max_ptime=atoi(buf);
		if (obj->max_ptime <20 || obj->max_ptime >100 ) {
			ms_warning("MSSilkEnc unknown value [%i] for maxptime, use default value (100) instead",obj->max_ptime);
			obj->max_ptime=100;
		}
		ms_message("MSSilkEnc: got useinbandfec=%i",obj->max_ptime);
	} else 	if (fmtp_get_value(fmtp,"ptime",buf,sizeof(buf))){
		obj->ptime=atoi(buf);
		if (obj->ptime > obj->max_ptime) {
			obj->ptime=obj->max_ptime;
		} else if (obj->ptime%20) {
		//if the ptime is not a mulptiple of 20, go to the next multiple
		obj->ptime = obj->ptime - obj->ptime%20 + 20; 
		}
		
		ms_message("MSSilkEnc: got ptime=%i",obj->ptime);
	} else 	if (fmtp_get_value(fmtp,"useinbandfec",buf,sizeof(buf))){
		obj->control.useInBandFEC=atoi(buf);
		if (obj->control.useInBandFEC != 0 && obj->control.useInBandFEC != 1) {
			ms_warning("MSSilkEnc unknown value [%i] for useinbandfec, use default value (0) instead",obj->control.useInBandFEC);
			obj->control.useInBandFEC=1;
		}
		ms_message("MSSilkEnc: got useinbandfec=%i",obj->control.useInBandFEC);
	} 
	
	return 0;
}
static int filter_set_bitrate(MSFilter *f, void *arg){
	struct silk_enc_struct* obj= (struct silk_enc_struct*) f->data;
	int inital_cbr=0;
	int normalized_cbr=0;	
	int pps=1000/obj->ptime;
	obj->max_network_bitrate=*(int*)arg;
	normalized_cbr=inital_cbr=(int)( ((((float)obj->max_network_bitrate)/(pps*8))-20-12-8)*pps*8);
	switch(obj->control.maxInternalSampleRate) {
		case 8000:
			normalized_cbr=MIN(normalized_cbr,20000);
			normalized_cbr=MAX(normalized_cbr,5000);
			break;
		case 12000:
			normalized_cbr=MIN(normalized_cbr,25000);
			normalized_cbr=MAX(normalized_cbr,7000);
			break;
		case 16000:
			normalized_cbr=MIN(normalized_cbr,32000);
			normalized_cbr=MAX(normalized_cbr,8000);
			break;
		case 24000:
			normalized_cbr=MIN(normalized_cbr,40000);
			normalized_cbr=MAX(normalized_cbr,20000);
			break;
			
	}
	if (normalized_cbr!=inital_cbr) {
		ms_warning("Silk enc unsupported codec bitrate [%i], normalizing",inital_cbr); 
	}
	obj->control.bitRate=normalized_cbr;
	ms_message("Setting silk codec birate to [%i] from network bitrate [%i] with ptime [%i]",obj->control.bitRate,obj->max_network_bitrate,obj->ptime);
	return 0;
}

static int filter_get_bitrate(MSFilter *f, void *arg){
	struct silk_enc_struct* obj= (struct silk_enc_struct*) f->data;	
	*(int*)arg=obj->max_network_bitrate;
	return 0;
}


static MSFilterMethod filter_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE , filter_set_sample_rate },
    {	MS_FILTER_GET_SAMPLE_RATE , filter_get_sample_rate },
	{	MS_FILTER_SET_BITRATE		,	filter_set_bitrate	},
	{	MS_FILTER_GET_BITRATE		,	filter_get_bitrate	},
	{	MS_FILTER_ADD_FMTP		,	filter_add_fmtp },
	{	0, NULL}
};



MSFilterDesc ms_silk_enc_desc={
	.id=MS_FILTER_PLUGIN_ID, /* from Allfilters.h*/
	.name="MSSILKEnc",
	.text="SILK audio encoder filter.",
	.category=MS_FILTER_ENCODER,
	.enc_fmt="SILK",
	.ninputs=1, /*number of inputs*/
	.noutputs=1, /*number of outputs*/
	.init=filter_init,
	.preprocess=filter_preprocess,
	.process=filter_process,
    .postprocess=filter_postprocess,
	.uninit=filter_unit,
	.methods=filter_methods
};
MS_FILTER_DESC_EXPORT(ms_silk_enc_desc)

extern MSFilterDesc ms_silk_dec_desc;
#ifndef VERSION
	#define VERSION "debug"
#endif
void libmssilk_init(){
	ms_filter_register(&ms_silk_enc_desc);
	ms_filter_register(&ms_silk_dec_desc);
	ms_message(" libmssilk " VERSION " plugin loaded");
}

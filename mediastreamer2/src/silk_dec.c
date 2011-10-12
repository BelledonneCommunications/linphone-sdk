/*
 filter-template.m
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
#include "msfilter.h"
#include "SKP_Silk_SDK_API.h"

/*filter common method*/
struct silk_dec_struct {
    SKP_SILK_SDK_DecControlStruct control;
	void  *psDec;
	
};

static void filter_init(MSFilter *f){
        f->data = ms_new0(struct silk_dec_struct,1);
}

static void filter_preprocess(MSFilter *f){
	struct silk_dec_struct* obj= (struct silk_dec_struct*) f->data;
	SKP_int16 ret;
	SKP_int32 decSizeBytes;
	/* Initialize to one frame per packet, for proper concealment before first packet arrives */
    obj->control.framesPerPacket = 1;
    /* Create decoder */
    ret = SKP_Silk_SDK_Get_Decoder_Size(&decSizeBytes );
    if( ret ) {
        ms_error("SKP_Silk_SDK_Get_Decoder_Size returned %d", ret );
    }
    obj->psDec = ms_malloc(decSizeBytes);
    /* Reset decoder */
    ret = SKP_Silk_SDK_InitDecoder(obj->psDec);
    if(ret) {
        ms_error( "SKP_Silk_InitDecoder returned %d", ret );
    }
}

static void filter_process(MSFilter *f){
	struct silk_dec_struct* obj= (struct silk_dec_struct*) f->data;
	mblk_t *im;
	mblk_t *om;
	SKP_int16 len;
	SKP_int16 ret;
	
	while((im=ms_queue_get(f->inputs[0]))) {
		
		do {
			/* Decode 20 ms */
			om=allocb(obj->control.API_sampleRate*4/100,0); /*samplingrate*0.02*2*/ 
			ret = SKP_Silk_SDK_Decode( obj->psDec, &obj->control, 0, im->b_rptr, im->b_wptr - im->b_rptr, (SKP_int16*)om->b_wptr, &len );
			if( ret ) {
				ms_error( "SKP_Silk_SDK_Decode returned %d", ret );
				ms_free(om);
			} else {
				
				om->b_wptr+=len*2;
				ms_queue_put(f->outputs[0],om);
			}
			/* Until last 20 ms frame of packet has been decoded */
		} while(obj->control.moreInternalDecoderFrames); 
	}
	
}

static void filter_postprocess(MSFilter *f){
    struct silk_dec_struct* obj= (struct silk_dec_struct*) f->data;
	ms_free(obj->psDec);
}

static void filter_unit(MSFilter *f){
    ms_free(f->data);
}


/*filter specific method*/

static int filter_set_sample_rate(MSFilter *f, void *arg) {
	struct silk_dec_struct* obj= (struct silk_dec_struct*) f->data;
	switch (*(SKP_int32*)arg) {
		case 8000:
		case 12000:
		case 16000:
		case 24000:
		case 32000:
		case 44000:
		case 48000:	
			obj->control.API_sampleRate=*(SKP_int32*)arg;
			break;
		default:
			ms_warning("unsupported output sampling rate [%i] for silk, using 44 000",*(SKP_int32*)arg);
			obj->control.API_sampleRate=44000;
	}
	return 0;
}

static int filter_get_sample_rate(MSFilter *f, void *arg) {
	struct silk_dec_struct* obj= (struct silk_dec_struct*) f->data;
    *(int*)arg = obj->control.API_sampleRate;
	return 0;
}

static MSFilterMethod filter_methods[]={
	{	MS_FILTER_SET_SAMPLE_RATE , filter_set_sample_rate },
    {	MS_FILTER_GET_SAMPLE_RATE , filter_get_sample_rate },
	{	0, NULL}
};



MSFilterDesc ms_silk_dec_desc={
	.id=MS_FILTER_PLUGIN_ID, /* from Allfilters.h*/
	.name="MSSILKDec",
	.text="Silk decoder filter.",
	.category=MS_FILTER_DECODER,
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
MS_FILTER_DESC_EXPORT(ms_silk_dec_desc)
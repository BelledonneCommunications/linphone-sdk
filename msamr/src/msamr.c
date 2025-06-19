/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2010  Yann DIORCET
Belledonne Communications SARL, All rights reserved.
yann.diorcet@belledonne-communications.com

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

#include <mediastreamer2/msfilter.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _MSC_VER
#define MS_PLUGIN_DECLARE(type) __declspec(dllexport) type
#else
#define MS_PLUGIN_DECLARE(type) type
#endif

#ifdef HAVE_AMRNB
extern MSFilterDesc amrnb_dec_desc;
extern MSFilterDesc amrnb_enc_desc;
#endif

#ifdef HAVE_AMRWB
extern MSFilterDesc amrwb_dec_desc;
extern MSFilterDesc amrwb_enc_desc;
#endif

#ifdef USE_ANDROID_AMR
int opencore_amr_wrapper_init(const char **missing);
#endif

static PayloadType * amr_match(MSOfferAnswerContext *ctx, const MSList *local_payloads, const PayloadType *refpt, const MSList *remote_payloads, bool_t reading_response){
	PayloadType *pt;
	char value[10];
	const MSList *elem;
	PayloadType *candidate=NULL;

	for (elem=local_payloads;elem!=NULL;elem=elem->next){
		pt=(PayloadType*)elem->data;
		
		if ( pt->mime_type && refpt->mime_type 
			&& strcasecmp(pt->mime_type, refpt->mime_type)==0
			&& pt->clock_rate==refpt->clock_rate
			&& pt->channels==refpt->channels) {
			int octedalign1=0,octedalign2=0;
			if (pt->recv_fmtp!=NULL && fmtp_get_value(pt->recv_fmtp,"octet-align",value,sizeof(value))){
				octedalign1=atoi(value);
			}
			if (refpt->send_fmtp!=NULL && fmtp_get_value(refpt->send_fmtp,"octet-align",value,sizeof(value))){
				octedalign2=atoi(value);
			}
			if (octedalign1==octedalign2) {
				candidate=pt;
				break; /*exact match */
			}
		}
	}
	return candidate ? payload_type_clone(candidate) : NULL;
}

static MSOfferAnswerContext *amr_offer_answer_create_context(void){
	static MSOfferAnswerContext amr_oa = {amr_match, NULL, NULL};
	return &amr_oa;
}

MSOfferAnswerProvider amr_offer_answer_provider={
	"AMR",
	amr_offer_answer_create_context
};



MS_PLUGIN_DECLARE(void) libmsamr_init(MSFactory *f) {
#ifdef HAVE_AMRNB
#ifdef USE_ANDROID_AMR
	const char *missing=NULL;
	if (opencore_amr_wrapper_init(&missing)==-1) {
		ms_error("Could not find AMR codec of android, no AMR support possible (missing symbol=%s)",missing);
		return;
	}
#endif
	ms_factory_register_filter(f, &amrnb_dec_desc);
	ms_factory_register_filter(f, &amrnb_enc_desc);
#endif
#ifdef HAVE_AMRWB
	ms_factory_register_filter(f, &amrwb_dec_desc);
	ms_factory_register_filter(f, &amrwb_enc_desc);
#endif
	
	ms_factory_register_offer_answer_provider(f, &amr_offer_answer_provider);

	ms_message("libmsamr " VERSION " plugin loaded");
}

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

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/mssndcard.h"


MSSndCardManager * ms_snd_card_manager_new(void){
	MSSndCardManager *obj=(MSSndCardManager *)ms_new0(MSSndCardManager,1);
	obj->factory = NULL;
	obj->cards=NULL;
	obj->descs=NULL;
	return obj;
}

void ms_snd_card_manager_destroy(MSSndCardManager* scm){
	if (scm!=NULL){
		bctbx_list_t *elem;
		for(elem=scm->descs;elem!=NULL;elem=elem->next){
			MSSndCardDesc *desc = (MSSndCardDesc*)elem->data;
			if (desc->unload!=NULL)
				desc->unload(scm);
		}
		bctbx_list_for_each(scm->cards,(void (*)(void*))ms_snd_card_destroy);
		bctbx_list_free(scm->cards);
		bctbx_list_free(scm->descs);
	}
	ms_free(scm);
	scm=NULL;
}

MSFactory * ms_snd_card_get_factory(MSSndCard * c){
	return c->sndcardmanager->factory;
}

MSSndCard * ms_snd_card_manager_get_card(MSSndCardManager *m, const char *id){
	bctbx_list_t *elem;
	for (elem=m->cards;elem!=NULL;elem=elem->next){
		MSSndCard *card=(MSSndCard*)elem->data;
		if (id==NULL) return card;
		if (strcmp(ms_snd_card_get_string_id(card),id)==0)	return card;
	}
	if (id!=NULL) ms_warning("no card with id %s",id);
	return NULL;
}

static MSSndCard *get_card_with_cap(MSSndCardManager *m, const char *id, unsigned int caps){
	bctbx_list_t *elem;
	for (elem=m->cards;elem!=NULL;elem=elem->next){
		MSSndCard *card=(MSSndCard*)elem->data;
		if ((id== NULL || strcmp(ms_snd_card_get_string_id(card),id)==0) && (card->capabilities & caps) == caps)	return card;
	}
	return NULL;
}

MSSndCard * ms_snd_card_manager_get_playback_card(MSSndCardManager *m, const char *id){
	MSSndCard *ret;
	ret = get_card_with_cap(m, id, MS_SND_CARD_CAP_PLAYBACK);
	if (ret) return ret;
	if (id != NULL) ms_warning("No playback card with id %s",id);
	return NULL;
}

MSSndCard * ms_snd_card_manager_get_capture_card(MSSndCardManager *m, const char *id){
	MSSndCard *ret;
	ret = get_card_with_cap(m, id, MS_SND_CARD_CAP_CAPTURE);
	if (ret) return ret;
	if (id != NULL) ms_warning("No capture card with id %s",id);
	return NULL;
}

MSSndCard * ms_snd_card_manager_get_default_card(MSSndCardManager *m){
	/*return the first card that has the capture+playback capability */
	return get_card_with_cap(m, NULL, MS_SND_CARD_CAP_PLAYBACK | MS_SND_CARD_CAP_CAPTURE);
}

MSSndCard * ms_snd_card_manager_get_default_capture_card(MSSndCardManager *m){
	return get_card_with_cap(m, NULL, MS_SND_CARD_CAP_CAPTURE);
}

MSSndCard * ms_snd_card_manager_get_default_playback_card(MSSndCardManager *m){
	return get_card_with_cap(m, NULL, MS_SND_CARD_CAP_PLAYBACK);
}

const bctbx_list_t * ms_snd_card_manager_get_list(MSSndCardManager *m){
	return m->cards;
}

void ms_snd_card_set_manager(MSSndCardManager*m, MSSndCard *c){
	if (c->sndcardmanager == NULL) c->sndcardmanager = m;
}

static const char *cap_to_string(unsigned int cap){
	if ((cap & MS_SND_CARD_CAP_CAPTURE) && (cap & MS_SND_CARD_CAP_PLAYBACK)) return "capture, playback";
	if (cap & MS_SND_CARD_CAP_CAPTURE) return "capture";
	if (cap & MS_SND_CARD_CAP_PLAYBACK) return "playback";
	return "none";
}

void ms_snd_card_manager_add_card(MSSndCardManager *m, MSSndCard *c){
	ms_snd_card_set_manager(m,c);
	ms_message("Card '%s' added with capabilities [%s]",ms_snd_card_get_string_id(c), cap_to_string(c->capabilities));
	m->cards=bctbx_list_append(m->cards,c);
}

void ms_snd_card_manager_prepend_cards(MSSndCardManager *m, bctbx_list_t *l) {
	bctbx_list_t *elem;
	bctbx_list_t *lcopy = bctbx_list_copy(l);
	if (m->cards != NULL) m->cards = bctbx_list_concat(lcopy, m->cards);
	else m->cards = lcopy;
	for (elem = l; elem != NULL; elem = elem->next) {
		MSSndCard *card = (MSSndCard *)elem->data;
		ms_snd_card_set_manager(m, card);
		ms_message("Card '%s' added", ms_snd_card_get_string_id(card));
	}
}

static void card_detect(MSSndCardManager *m, MSSndCardDesc *desc){
	if (desc->detect!=NULL)
		desc->detect(m);
}

void ms_snd_card_manager_register_desc(MSSndCardManager *m, MSSndCardDesc *desc){
	if (bctbx_list_find(m->descs, desc) == NULL){
		m->descs=bctbx_list_append(m->descs,desc);
		card_detect(m,desc);
	}
}

void ms_snd_card_manager_reload(MSSndCardManager *m){
	bctbx_list_t *elem;
	bctbx_list_for_each(m->cards,(void (*)(void*))ms_snd_card_destroy);
	bctbx_list_free(m->cards);
	m->cards=NULL;
	for(elem=m->descs;elem!=NULL;elem=elem->next)
		card_detect(m,(MSSndCardDesc*)elem->data);
}

MSSndCard * ms_snd_card_dup(MSSndCard *card){
	MSSndCard *obj=NULL;
	if (card->desc->duplicate!=NULL)
		obj=card->desc->duplicate(card);
	return obj;
}

MSSndCard * ms_snd_card_new(MSSndCardDesc *desc){
	return ms_snd_card_new_with_name(desc,NULL);
}

MSSndCard * ms_snd_card_new_with_name(MSSndCardDesc *desc,const char* name) {
	MSSndCard *obj=(MSSndCard *)ms_new0(MSSndCard,1);
	obj->sndcardmanager = NULL;
	obj->desc=desc;
	obj->name=name?ms_strdup(name):NULL;
	obj->data=NULL;
	obj->id=NULL;
	obj->capabilities=MS_SND_CARD_CAP_CAPTURE|MS_SND_CARD_CAP_PLAYBACK;
	if (desc->init!=NULL)
		desc->init(obj);
	return obj;
}

const char *ms_snd_card_get_driver_type(const MSSndCard *obj){
	return obj->desc->driver_type;
}

const char *ms_snd_card_get_name(const MSSndCard *obj){
	return obj->name;
}

unsigned int ms_snd_card_get_capabilities(const MSSndCard *obj){
	return obj->capabilities;
}

MS2_PUBLIC int ms_snd_card_get_minimal_latency(MSSndCard *obj){
	return obj->latency;
}

const char *ms_snd_card_get_string_id(MSSndCard *obj){
	if (obj->id==NULL)	obj->id=ms_strdup_printf("%s: %s",obj->desc->driver_type,obj->name);
	return obj->id;
}

void ms_snd_card_set_level(MSSndCard *obj, MSSndCardMixerElem e, int percent){
	if (obj->desc->set_level!=NULL)
		obj->desc->set_level(obj,e,percent);
	else ms_warning("ms_snd_card_set_level: unimplemented by %s wrapper",obj->desc->driver_type);
}

int ms_snd_card_get_level(MSSndCard *obj, MSSndCardMixerElem e){
	if (obj->desc->get_level!=NULL)
		return obj->desc->get_level(obj,e);
	else {
		ms_warning("ms_snd_card_get_level: unimplemented by %s wrapper",obj->desc->driver_type);
		return -1;
	}
}

void ms_snd_card_set_capture(MSSndCard *obj, MSSndCardCapture c){
	if (obj->desc->set_capture!=NULL)
		obj->desc->set_capture(obj,c);
	else ms_warning("ms_snd_card_set_capture: unimplemented by %s wrapper",obj->desc->driver_type);
}

int ms_snd_card_set_control(MSSndCard *obj, MSSndCardControlElem e, int val)
{
	if (obj->desc->set_control!=NULL)
		return obj->desc->set_control(obj,e,val);
	else {
		ms_warning("ms_snd_card_set_control: unimplemented by %s wrapper",obj->desc->driver_type);
		return -1;
	}
}

int ms_snd_card_get_control(MSSndCard *obj, MSSndCardControlElem e)
{
	if (obj->desc->get_control!=NULL)
		return obj->desc->get_control(obj,e);
	else {
		ms_warning("ms_snd_card_get_control: unimplemented by %s wrapper",obj->desc->driver_type);
		return -1;
	}
}

struct _MSFilter * ms_snd_card_create_reader(MSSndCard *obj){
	if (obj->desc->create_reader!=NULL)
		return obj->desc->create_reader(obj);
	else ms_warning("ms_snd_card_create_reader: unimplemented by %s wrapper",obj->desc->driver_type);
	return NULL;
}

struct _MSFilter * ms_snd_card_create_writer(MSSndCard *obj){
	if (obj->desc->create_writer!=NULL)
		return obj->desc->create_writer(obj);
	else ms_warning("ms_snd_card_create_writer: unimplemented by %s wrapper",obj->desc->driver_type);
	return NULL;
}

void ms_snd_card_set_usage_hint(MSSndCard *obj, bool_t is_going_to_be_used){
	if (obj->desc->usage_hint!=NULL)
		obj->desc->usage_hint(obj, is_going_to_be_used);
}

void ms_snd_card_destroy(MSSndCard *obj){
	if (obj->desc->uninit!=NULL) obj->desc->uninit(obj);
	if (obj->name!=NULL) ms_free(obj->name);
	if (obj->id!=NULL)	ms_free(obj->id);
	ms_free(obj);
}

int ms_snd_card_get_preferred_sample_rate(const MSSndCard *obj) {
	return obj->preferred_sample_rate;
}

int ms_snd_card_set_preferred_sample_rate(MSSndCard *obj,int rate) {
	obj->preferred_sample_rate=rate;
	return 0;
}

#ifdef __linux
#ifndef __ALSA_ENABLED__
MSSndCard * ms_alsa_card_new_custom(const char *pcmdev, const char *mixdev){
	ms_warning("Alsa support not available in this build of mediastreamer2");
	return NULL;
}

void ms_alsa_card_set_forced_sample_rate(int samplerate){
	ms_warning("Alsa support not available in this build of mediastreamer2");
}
#endif
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#else
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

MSSndCardManager* ms_snd_card_manager_get(void) {
	return ms_factory_get_snd_card_manager(ms_factory_get_fallback());
}


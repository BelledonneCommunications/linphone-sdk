/*
 * Copyright (c) 2010-2020 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/mssndcard.h"
#include "bctoolbox/regex.h"

static bool_t bypass_sndcard_detection = FALSE;



MSSndCardManager * ms_snd_card_manager_new(void){
	MSSndCardManager *obj=(MSSndCardManager *)ms_new0(MSSndCardManager,1);
	obj->factory = NULL;
	obj->cards=NULL;
	obj->descs=NULL;
	obj->paramString=bctbx_strdup_printf("FAST=false;NOVOICEPROC=false;TESTER=false;RINGER=false");
	return obj;
}

void ms_snd_card_manager_destroy(MSSndCardManager* scm){
	if (scm!=NULL && !bypass_sndcard_detection){
		bctbx_list_t *elem;
		for(elem=scm->descs;elem!=NULL;elem=elem->next){
			MSSndCardDesc *desc = (MSSndCardDesc*)elem->data;
			if (desc->unload!=NULL)
				desc->unload(scm);
		}
		bctbx_list_for_each(scm->cards,(void (*)(void*))ms_snd_card_unref);
		bctbx_list_free(scm->cards);
		bctbx_list_free(scm->descs);
	}
	if (scm!=NULL && scm->paramString!= NULL) {
		bctbx_free(scm->paramString);
	}
	ms_free(scm);
}

void ms_snd_card_manager_set_param_string(MSSndCardManager *m, const char *paramString){
	if (m->paramString!=NULL) {
		bctbx_free(m->paramString);
	}
	m->paramString = bctbx_strdup(paramString);
}

MSFactory * ms_snd_card_get_factory(MSSndCard * c){
	return c->sndcardmanager->factory;
}

static char *ms_snd_card_get_legacy_string_id(MSSndCard *obj) {
	return ms_strdup_printf("%s: %s", obj->desc->driver_type, obj->name);
}

MSSndCard * ms_snd_card_manager_get_card(MSSndCardManager *m, const char *id) {
	bctbx_list_t *elem;
	for (elem = m->cards; elem != NULL; elem = elem->next) {
		MSSndCard *card = (MSSndCard*)elem->data;
// Exact search
		if (id == NULL) return card;
		const char * card_id = ms_snd_card_get_string_id(card);
		if (strcmp(card_id, id) == 0) return card;
		
		char * legacy_id = ms_snd_card_get_legacy_string_id(card);
		if (strcmp(legacy_id, id) == 0) {
			ms_message("Found match using legacy sound card id");
			ms_free(legacy_id);
			return card;
		}
		ms_free(legacy_id);
// Regex search on card id that will contains at least "Filter : Model"
		if( bctbx_is_matching_regex_log(card_id, id, FALSE)){
			return card;
		}
	}

	if (id != NULL) ms_warning("no card with id %s", id);
	return NULL;
}

MSSndCard * ms_snd_card_manager_get_card_with_capabilities(MSSndCardManager *m, const char *id, unsigned int capabilities) {
	bctbx_list_t *elem;
	for (elem = m->cards; elem != NULL; elem = elem->next) {
		MSSndCard *card = (MSSndCard*)elem->data;
		if ((card->capabilities & capabilities) != capabilities) continue;
// Exact search
		if (id == NULL) return card;
		const char * card_id = ms_snd_card_get_string_id(card);
		if (strcmp(card_id, id) == 0) return card;
		
		char * legacy_id = ms_snd_card_get_legacy_string_id(card);
		if (strcmp(legacy_id, id) == 0) {
			ms_message("Found match using legacy sound card id");
			ms_free(legacy_id);
			return card;
		}
		ms_free(legacy_id);
// Regex search on card id that will contains at least "Filter : Model"
		if( bctbx_is_matching_regex_log(card_id, id, FALSE)){
			return card;
		}
	}

	if (id != NULL) ms_warning("no card with id %s", id);
	return NULL;
}

MSSndCard * ms_snd_card_manager_get_card_by_type(MSSndCardManager *m, const MSSndCardDeviceType type, const char * driver_type) {
	bctbx_list_t *elem;
	for (elem = m->cards; elem != NULL; elem = elem->next) {
		MSSndCard *c = (MSSndCard*)elem->data;
		if ((strcmp(c->desc->driver_type, driver_type) == 0) && (type == ms_snd_card_get_device_type(c))) return c;
	}
	return NULL;
}

static MSSndCard *get_card_with_cap(MSSndCardManager *m, const char *id, unsigned int caps){
	bctbx_list_t *elem;
	for (elem = m->cards; elem != NULL; elem = elem->next) {
		MSSndCard *card = (MSSndCard*)elem->data;
		if ((id == NULL || strcmp(ms_snd_card_get_string_id(card), id) == 0) && (card->capabilities & caps) == caps) return card;
	}
	return NULL;
}

bctbx_list_t * ms_snd_card_manager_get_all_cards_with_name(MSSndCardManager *m, const char *name){
	bctbx_list_t *cards = NULL;
	bctbx_list_t *elem;
	for (elem = m->cards; elem != NULL; elem = elem->next) {
		MSSndCard *card = (MSSndCard*)elem->data;
		if (strcmp(ms_snd_card_get_name(card),name) == 0) {
			cards = bctbx_list_append(cards, ms_snd_card_ref(card));
		}
	}
	return cards;
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
	m->cards=bctbx_list_append(m->cards, ms_snd_card_ref(c));
}

void ms_snd_card_manager_prepend_card(MSSndCardManager *m, MSSndCard *c){
	ms_snd_card_set_manager(m,c);
	ms_message("Card '%s' prepended with capabilities [%s]",ms_snd_card_get_string_id(c), cap_to_string(c->capabilities));
	m->cards=bctbx_list_prepend(m->cards, ms_snd_card_ref(c));
}

bool_t ms_snd_card_manager_swap_cards(MSSndCardManager *m, MSSndCard *card0, MSSndCard *card1){

	if (!card0) return FALSE;
	if (!card1) return FALSE;

	bctbx_list_t *elem = NULL;
	bctbx_list_t *ltmp = NULL;
	bool_t card0_found = FALSE;
	bool_t card1_found = FALSE;
	for (elem = m->cards; elem != NULL; elem = elem->next) {
		MSSndCard *card = (MSSndCard *)elem->data;
		MSSndCard *c = NULL;
		if (strcmp(ms_snd_card_get_string_id(card),ms_snd_card_get_string_id(card0))==0){
			card0_found = TRUE;
			c = card1;
		} else if (strcmp(ms_snd_card_get_string_id(card),ms_snd_card_get_string_id(card1))==0){
			card1_found = TRUE;
			c = card0;
		} else {
			c = card;
		}
		ltmp=bctbx_list_append(ltmp, c);
	}
	if (card0_found && card1_found) {
		m->cards=ltmp;
		return TRUE;
	} else {
		ms_message("[Card Swap] Unable to swap position of card '%s' and card '%s' because %s has not been found",ms_snd_card_get_string_id(card0), ms_snd_card_get_string_id(card1), (card0_found ? "latter" : "former"));
		return FALSE;
	}

}

void ms_snd_card_manager_prepend_cards(MSSndCardManager *m, bctbx_list_t *l) {
	bctbx_list_t *elem;
	bctbx_list_t *lcopy = bctbx_list_copy(l);
	if (m->cards != NULL) m->cards = bctbx_list_concat(lcopy, m->cards);
	else m->cards = lcopy;
	for (elem = l; elem != NULL; elem = elem->next) {
		MSSndCard *card = (MSSndCard *)elem->data;
		ms_snd_card_ref(card);// Add a ref for the copy
		ms_snd_card_set_manager(m, card);
		ms_message("Card '%s' added", ms_snd_card_get_string_id(card));
	}
}

void ms_snd_card_manager_bypass_soundcard_detection(bool_t value) {
	bypass_sndcard_detection = value;
}

static void card_detect(MSSndCardManager *m, MSSndCardDesc *desc){
	if (bypass_sndcard_detection) return;

	if (desc->detect!=NULL)
		desc->detect(m);
}

void ms_snd_card_manager_register_desc(MSSndCardManager *m, MSSndCardDesc *desc){
	if (bctbx_list_find(m->descs, desc) == NULL) {
		m->descs = bctbx_list_append(m->descs, desc);
		card_detect(m, desc);
	}
#ifdef __ANDROID__
	// Put earpiece and speaker as first devices of every filter
	ms_snd_card_sort(m);
#endif // __ANDROID__
}

void ms_snd_card_manager_unregister_desc(MSSndCardManager *m, MSSndCardDesc *desc){
	if (bctbx_list_find(m->descs, desc) != NULL) {
		m->descs = bctbx_list_remove(m->descs, desc);
	}
}

bool_t ms_snd_card_equals(const MSSndCard *c1, const MSSndCard *c2){
	if (strcmp(ms_snd_card_get_string_id(c1), ms_snd_card_get_string_id(c2)) == 0
		&& c1->capabilities == c2->capabilities
		&& ms_sound_devices_description_equals(c1->device_description, c2->device_description)
	) {
		return TRUE;
	}
	return FALSE;
}

bool_t ms_snd_card_manager_reload_requested(MSSndCardManager *m){
	bctbx_list_t *elem;
	for (elem = m->descs; elem != NULL; elem = elem->next) {
		MSSndCardDesc *desc = (MSSndCardDesc*)elem->data;
		if (desc->reload_requested!=NULL && desc->reload_requested(m))
			return TRUE;
	}
	return FALSE;
}

void ms_snd_card_manager_reload(MSSndCardManager *m){
	bctbx_list_t *elem;
	bctbx_list_t *cardsToKeep = NULL;
	bctbx_list_t *oldCardsIt, *newCardsIt;
	
	// Keep a copy of the old cards
	for (oldCardsIt = m->cards; oldCardsIt != NULL; oldCardsIt = oldCardsIt->next) {
		MSSndCard *sndCard = (MSSndCard *)oldCardsIt->data;
		cardsToKeep = bctbx_list_append(cardsToKeep, ms_snd_card_ref(sndCard));
	}
	bctbx_list_free_with_data(m->cards, (void (*)(void*))ms_snd_card_unref);
	m->cards = NULL;
	for (elem = m->descs; elem != NULL; elem = elem->next) {
		card_detect(m, (MSSndCardDesc*)elem->data);
	}
	
	// When a new card is actually the same as the old one, keep using the old one instead
	for (newCardsIt = m->cards; newCardsIt != NULL; newCardsIt = newCardsIt->next) {
		MSSndCard *newCard = (MSSndCard *)newCardsIt->data;
		for (oldCardsIt = cardsToKeep; oldCardsIt != NULL; oldCardsIt = oldCardsIt->next) {
			MSSndCard *oldCard = (MSSndCard *)oldCardsIt->data;
			if (ms_snd_card_equals(oldCard, newCard)){
				ms_snd_card_ref(oldCard);
				newCardsIt->data = oldCard;
				ms_snd_card_unref(newCard);
				break;
			}
		}
	}
	
	bctbx_list_free_with_data(cardsToKeep, (void (*)(void*))ms_snd_card_unref);
	
#ifdef __ANDROID__
	// Put earpiece and speaker as first devices of every filter
	ms_snd_card_sort(m);
#endif // __ANDROID__
}

MSSndCard* ms_snd_card_dup(MSSndCard *card){
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
	obj->internal_id=-1;
	obj->device_type=MS_SND_CARD_DEVICE_TYPE_UNKNOWN;
	obj->capabilities=MS_SND_CARD_CAP_CAPTURE|MS_SND_CARD_CAP_PLAYBACK;
	obj->streamType=MS_SND_CARD_STREAM_VOICE;
	obj->device_description=NULL;
	if (desc->init!=NULL)
		desc->init(obj);
	return obj;
}

MSSndCardDeviceType ms_snd_card_get_device_type(const MSSndCard *obj){
	return obj->device_type;
}

const char * ms_snd_card_device_type_to_string(const MSSndCardDeviceType type){
	switch(type) {
		case MS_SND_CARD_DEVICE_TYPE_TELEPHONY:
			return "Telephony";
		case MS_SND_CARD_DEVICE_TYPE_AUX_LINE:
			return "Aux line";
		case MS_SND_CARD_DEVICE_TYPE_GENERIC_USB:
			return "USB device";
		case MS_SND_CARD_DEVICE_TYPE_HEADSET:
			return "Headset";
		case MS_SND_CARD_DEVICE_TYPE_MICROPHONE:
			return "Microphone";
		case MS_SND_CARD_DEVICE_TYPE_EARPIECE:
			return "Earpiece";
		case MS_SND_CARD_DEVICE_TYPE_HEADPHONES:
			return "Headphones";
		case MS_SND_CARD_DEVICE_TYPE_SPEAKER:
			return "Speaker";
		case MS_SND_CARD_DEVICE_TYPE_BLUETOOTH:
			return "Bluetooth";
		case MS_SND_CARD_DEVICE_TYPE_BLUETOOTH_A2DP:
			return "Bluetooth A2DP";
		case MS_SND_CARD_DEVICE_TYPE_UNKNOWN:
			return "Unknown";
		default:
			return "bad type";
	}

	return "bad type";
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

const char *ms_snd_card_get_string_id(const MSSndCard *obj) {
	MSSndCard *mutable_card = (MSSndCard*)obj;
	if (obj->id == NULL) {
		bool_t addExtraData = ((obj->device_type == MS_SND_CARD_DEVICE_TYPE_BLUETOOTH) && (strcmp(obj->desc->driver_type, "openSLES") != 0));
		if (addExtraData == TRUE) {
			mutable_card->id = ms_strdup_printf("%s %s %s: %s",obj->desc->driver_type, ms_snd_card_device_type_to_string(obj->device_type), cap_to_string(obj->capabilities), obj->name);
		} else {
			mutable_card->id = ms_strdup_printf("%s %s: %s",obj->desc->driver_type, ms_snd_card_device_type_to_string(obj->device_type), obj->name);
		}
	}
	return obj->id;
}

void ms_snd_card_set_internal_id(MSSndCard *obj, int id){
	obj->internal_id = id;
}

int ms_snd_card_get_internal_id(MSSndCard *obj){
	return obj->internal_id;
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

void ms_snd_card_notify_audio_session_activated(MSSndCard *obj, bool_t activated) {
	if (obj->desc->audio_session_activated != NULL)
		obj->desc->audio_session_activated(obj,activated);
}

void ms_snd_card_notify_audio_route_changed(MSSndCard *obj) {
	if (obj->desc->audio_route_changed != NULL)
		obj->desc->audio_route_changed(obj);
}

void ms_snd_card_app_notifies_activation(MSSndCard *obj, bool_t yesno) {
	if (obj->desc->callkit_enabled != NULL)
		obj->desc->callkit_enabled(obj, yesno);
}

void ms_snd_card_configure_audio_session(MSSndCard *obj) {
	if (obj->desc->configure != NULL)
		obj->desc->configure(obj);
}

void ms_snd_card_destroy(MSSndCard *obj){
	ms_snd_card_unref(obj);
}

int ms_snd_card_get_preferred_sample_rate(const MSSndCard *obj) {
	return obj->preferred_sample_rate;
}

int ms_snd_card_set_preferred_sample_rate(MSSndCard *obj,int rate) {
	obj->preferred_sample_rate=rate;
	return 0;
}

void ms_snd_card_set_stream_type(MSSndCard *obj, MSSndCardStreamType type) {
	obj->streamType = type;
}

MSSndCardStreamType ms_snd_card_get_stream_type(MSSndCard *obj) {
	return obj->streamType;
}

#ifdef __linux__
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

MSSndCard* ms_snd_card_ref(MSSndCard *sndCard) {
	sndCard->ref_count++;
	return sndCard;
}

void ms_snd_card_unref(MSSndCard *sndCard) {
	sndCard->ref_count--;
	if (sndCard->ref_count <= 0) {
		if (sndCard->desc->uninit) sndCard->desc->uninit(sndCard);
		if (sndCard->name != NULL) ms_free(sndCard->name);
		if (sndCard->id != NULL) ms_free(sndCard->id);
		sndCard->device_description = NULL;
		ms_free(sndCard);
	}
}

bool_t ms_snd_card_is_card_duplicate(MSSndCardManager *m, MSSndCard * card, bool_t checkCapabilities) {
	MSSndCard *duplicate = ms_snd_card_get_card_duplicate(m, card, checkCapabilities);
	if (duplicate != NULL) {
		ms_snd_card_unref(duplicate);
		return TRUE;
	}
	return FALSE;
}

MSSndCard* ms_snd_card_get_card_duplicate(MSSndCardManager *m, MSSndCard * card, bool_t checkCapabilities) {
	bctbx_list_t* cards = ms_snd_card_manager_get_all_cards_with_name(m, card->name);

	// Distinguish card by playback and capture.
	// Ignore other capatibilirties such as built-in echo cancelling
	unsigned int caps_mask = (MS_SND_CARD_CAP_PLAYBACK | MS_SND_CARD_CAP_CAPTURE);
	unsigned int card_caps = ms_snd_card_get_capabilities(card) & caps_mask;

	// In modern devices, some devices are duplicated to improve performance.
	// Only one of them will be added to the card manager. In order to simplify the logic, the device added to the manager will depend on the order devices are in the list.
	// For any given combination of <name>,<driver_type>,<device_type> only the first one inn the list will be added to the card manager
	// If a card with same driver type and device_type has already been added to the sound card manager
	bctbx_list_t* elem;
	MSSndCard *duplicate = NULL;
	for (elem = cards; elem != NULL; elem = elem->next) {
		MSSndCard *c = (MSSndCard*)elem->data;
		unsigned int elem_caps = ms_snd_card_get_capabilities(c) & caps_mask;
		if ((c->device_type == card->device_type) && (strcmp(c->desc->driver_type, card->desc->driver_type) == 0) && ((checkCapabilities == FALSE) || (card_caps == elem_caps))) {
			duplicate = ms_snd_card_ref(c);
			break;
		}
	}

	// Free memory and unref sound cards
	bctbx_list_free_with_data(cards, (bctbx_list_free_func)ms_snd_card_unref);

	return duplicate;
}

void ms_snd_card_remove_type_from_list_head(MSSndCardManager *m, MSSndCardDeviceType type) {
	MSSndCard * head = ms_snd_card_ref(ms_snd_card_manager_get_card(m, NULL));
	// Loop until the type of the head of the list is not a bluetooh device
	while(ms_snd_card_get_device_type(head) == type) {
		bctbx_list_t * elem;
		for (elem=m->cards;elem!=NULL;elem=elem->next){
			MSSndCard *c=(MSSndCard*)elem->data;
			if(ms_snd_card_get_device_type(c) != type) {
				ms_snd_card_manager_swap_cards(m, head, c);
				// Exit for loop once swap occurred
				break;
			}
		}
		ms_snd_card_unref(head);
		head = ms_snd_card_ref(ms_snd_card_manager_get_card(m, NULL));
	}
	ms_snd_card_unref(head);
}

#ifdef __ANDROID__
void ms_snd_card_sort(MSSndCardManager *m) {
	bctbx_list_t * elem = NULL;
	bctbx_list_t *ltmp = NULL;
	const char * curr_driver = NULL;
	for (elem=m->cards;elem!=NULL;elem=elem->next){
		MSSndCard *c=(MSSndCard*)elem->data;
		// If current driver is the same as card driver, then search for earpiece and speaker device
		// Note: This implemenetation assumes that all devices belonging to the same filter are grouped together in the sound card manager
		if ((curr_driver == NULL) || (strcmp(c->desc->driver_type, curr_driver)!=0)) {
			// Update current driver
			curr_driver = c->desc->driver_type;

			// Search earpiece and speaker
			MSSndCard *earpiece = ms_snd_card_manager_get_card_by_type(m, MS_SND_CARD_DEVICE_TYPE_EARPIECE, curr_driver);
			if (earpiece) ltmp=bctbx_list_append(ltmp, earpiece);
			MSSndCard *speaker = ms_snd_card_manager_get_card_by_type(m, MS_SND_CARD_DEVICE_TYPE_SPEAKER, curr_driver);
			if (speaker) ltmp=bctbx_list_append(ltmp, speaker);
		}

		// Check if c is not an earpiece or speaker - as these devices are built-in into the phone, there is only 1 instance for each of them and it is searched using ms_snd_card_manager_get_card_by_type
		if ((ms_snd_card_get_device_type(c) != MS_SND_CARD_DEVICE_TYPE_EARPIECE) && (ms_snd_card_get_device_type(c) != MS_SND_CARD_DEVICE_TYPE_SPEAKER)) {
			// Append card
			ltmp=bctbx_list_append(ltmp, c);
		}
	}
	m->cards=ltmp;
}
#endif // __ANDROID__

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef sndcard_h
#define sndcard_h

#include <mediastreamer2/mscommon.h>

/**
 * @file mssndcard.h
 * @brief mediastreamer2 mssndcard.h include file
 *
 * This file provide the API needed to manage
 * soundcard filters.
 *
 */

/**
 * @defgroup mediastreamer2_soundcard Sound Card API - manage audio capture/play filters.
 * @ingroup mediastreamer2_api
 * @{
 */

struct _MSSndCardManager{
	MSList *cards;
	MSList *descs;
};

/**
 * Structure for sound card manager object.
 * @var MSSndCardManager
 */
typedef struct _MSSndCardManager MSSndCardManager;

enum _MSSndCardMixerElem{
	MS_SND_CARD_MASTER,
	MS_SND_CARD_PLAYBACK,
	MS_SND_CARD_CAPTURE
};

/**
 * Structure for sound card mixer values.
 * @var MSSndCardMixerElem
 */
typedef enum _MSSndCardMixerElem MSSndCardMixerElem;

enum _MSSndCardCapture {
	MS_SND_CARD_MIC,
	MS_SND_CARD_LINE
};

/**
 * Structure for sound card capture source values.
 * @var MSSndCardCapture
 */
typedef enum _MSSndCardCapture MSSndCardCapture;

enum _MSSndCardControlElem {
	MS_SND_CARD_MASTER_MUTE,
	MS_SND_CARD_PLAYBACK_MUTE,
	MS_SND_CARD_CAPTURE_MUTE
};

/**
 * Structure for sound card mixer values.
 * @var MSSndCardControlElem
 */
typedef enum _MSSndCardControlElem MSSndCardControlElem;

struct _MSSndCard;

typedef void (*MSSndCardDetectFunc)(MSSndCardManager *obj);
typedef void (*MSSndCardInitFunc)(struct _MSSndCard *obj);
typedef void (*MSSndCardUninitFunc)(struct _MSSndCard *obj);
typedef void (*MSSndCardSetLevelFunc)(struct _MSSndCard *obj, MSSndCardMixerElem e, int percent);
typedef void (*MSSndCardSetCaptureFunc)(struct _MSSndCard *obj, MSSndCardCapture e);
typedef int (*MSSndCardGetLevelFunc)(struct _MSSndCard *obj, MSSndCardMixerElem e);
typedef int (*MSSndCardSetControlFunc)(struct _MSSndCard *obj, MSSndCardControlElem e, int val);
typedef int (*MSSndCardGetControlFunc)(struct _MSSndCard *obj, MSSndCardControlElem e);
typedef struct _MSFilter * (*MSSndCardCreateReaderFunc)(struct _MSSndCard *obj);
typedef struct _MSFilter * (*MSSndCardCreateWriterFunc)(struct _MSSndCard *obj);
typedef struct _MSSndCard * (*MSSndCardDuplicateFunc)(struct _MSSndCard *obj);
typedef void (*MSSndCardUnloadFunc)(MSSndCardManager *obj);

struct _MSSndCardDesc{
	const char *driver_type;
	MSSndCardDetectFunc detect;
	MSSndCardInitFunc init;
	MSSndCardSetLevelFunc set_level;
	MSSndCardGetLevelFunc get_level;
	MSSndCardSetCaptureFunc set_capture;
	MSSndCardSetControlFunc set_control;
	MSSndCardGetControlFunc get_control;
	MSSndCardCreateReaderFunc create_reader;
	MSSndCardCreateWriterFunc create_writer;
	MSSndCardUninitFunc uninit;
	MSSndCardDuplicateFunc duplicate;
	MSSndCardUnloadFunc unload;

};

/**
 * Structure for sound card description object.
 * @var MSSndCardDesc
 */
typedef struct _MSSndCardDesc MSSndCardDesc;

#define MS_SND_CARD_CAP_DISABLED (0) /**<This soundcard is disabled.*/
#define MS_SND_CARD_CAP_CAPTURE (1) /**<This sound card can capture sound */
#define MS_SND_CARD_CAP_PLAYBACK (1<<1) /**<This sound card can playback sound */
#define MS_SND_CARD_CAP_BUILTIN_ECHO_CANCELLER (1<<2) /**<This sound card has built-in echo cancellation*/

struct _MSSndCard{
	MSSndCardDesc *desc;
	char *name;
	char *id;
	unsigned int capabilities;
	void *data;
	int preferred_sample_rate;
	int latency;
};

/**
 * Structure for sound card object.
 * @var MSSndCard
 */
typedef struct _MSSndCard MSSndCard;

#ifdef __cplusplus
extern "C"{
#endif

/**
 * @defgroup mediastreamer2_soundcardmanager Sound Card Manager API
 * @ingroup mediastreamer2_soundcard
 * @{
 */

/**
 * Retreive a sound card manager object.
 *
 * Returns: MSSndCardManager if successfull, NULL otherwise.
 */
MS2_PUBLIC MSSndCardManager * ms_snd_card_manager_get(void);

/**
 * Destroy a sound card manager object.
 *
 */
MS2_PUBLIC void ms_snd_card_manager_destroy(void);

/**
 * Retreive a sound card object based on its name.
 *
 * @param m    A sound card manager containing sound cards.
 * @param id   A name for card to search.
 *
 * Returns: MSSndCard if successfull, NULL otherwise.
 */
MS2_PUBLIC MSSndCard * ms_snd_card_manager_get_card(MSSndCardManager *m, const char *id);

/**
 * Retreive the default sound card object.
 *
 * @param m    A sound card manager containing sound cards.
 *
 * Returns: MSSndCard if successfull, NULL otherwise.
 */
MS2_PUBLIC MSSndCard * ms_snd_card_manager_get_default_card(MSSndCardManager *m);

/**
 * Retreive the default capture sound card object.
 *
 * @param m    A sound card manager containing sound cards.
 *
 * Returns: MSSndCard if successfull, NULL otherwise.
 */
MS2_PUBLIC MSSndCard * ms_snd_card_manager_get_default_capture_card(MSSndCardManager *m);

/**
 * Retreive the default playback sound card object.
 *
 * @param m    A sound card manager containing sound cards.
 *
 * Returns: MSSndCard if successfull, NULL otherwise.
 */
MS2_PUBLIC MSSndCard * ms_snd_card_manager_get_default_playback_card(MSSndCardManager *m);

/**
 * Retreive the list of sound card objects.
 *
 * @param m    A sound card manager containing sound cards.
 *
 * Returns: MSList of cards if successfull, NULL otherwise.
 */
MS2_PUBLIC const MSList * ms_snd_card_manager_get_list(MSSndCardManager *m);

/**
 * Add a sound card object in a sound card manager's list.
 *
 * @param m    A sound card manager containing sound cards.
 * @param c    A sound card object.
 *
 */
MS2_PUBLIC void ms_snd_card_manager_add_card(MSSndCardManager *m, MSSndCard *c);

/**
 * Register a sound card description in a sound card manager.
 *
 * @param m      A sound card manager containing sound cards.
 * @param desc   A sound card description object.
 *
 */
MS2_PUBLIC void ms_snd_card_manager_register_desc(MSSndCardManager *m, MSSndCardDesc *desc);

/**
 * Ask all registered MSSndCardDesc to re-detect their soundcards.
 * @param m The sound card manager.
**/
MS2_PUBLIC void ms_snd_card_manager_reload(MSSndCardManager *m);


/** @} */

/**
 * @defgroup mediastreamer2_soundcardfilter Sound Card Filter API
 * @ingroup mediastreamer2_soundcard
 * @{
 */

/**
 * Create an INPUT filter based on the selected sound card.
 *
 * @param obj      A sound card object.
 *
 * Returns: A MSFilter if successfull, NULL otherwise.
 */
MS2_PUBLIC struct _MSFilter * ms_snd_card_create_reader(MSSndCard *obj);

/**
 * Create an OUPUT filter based on the selected sound card.
 *
 * @param obj      A sound card object.
 *
 * Returns: A MSFilter if successfull, NULL otherwise.
 */
MS2_PUBLIC struct _MSFilter * ms_snd_card_create_writer(MSSndCard *obj);

/**
 * Create a new sound card object.
 *
 * @param desc   A sound card description object.
 *
 * Returns: MSSndCard if successfull, NULL otherwise.
 */
MS2_PUBLIC MSSndCard * ms_snd_card_new(MSSndCardDesc *desc);

/**
 * Create a new sound card object.
 *
 * @param desc   A sound card description object.
 * @param card mame
 *
 * Returns: MSSndCard if successfull, NULL otherwise.
 */
	
MS2_PUBLIC MSSndCard * ms_snd_card_new_with_name(MSSndCardDesc *desc,const char* name);
/**
 * Destroy sound card object.
 *
 * @param obj   A MSSndCard object.
 */
MS2_PUBLIC void ms_snd_card_destroy(MSSndCard *obj);

/**
 * Duplicate a sound card object.
 *
 * This helps to open several time a sound card.
 *
 * @param card   A sound card object.
 *
 * Returns: MSSndCard if successfull, NULL otherwise.
 */
MS2_PUBLIC MSSndCard * ms_snd_card_dup(MSSndCard *card);

/**
 * Retreive a sound card's driver type string.
 *
 * Internal driver types are either: "OSS, ALSA, WINSND, PASND, CA"
 *
 * @param obj   A sound card object.
 *
 * Returns: a string if successfull, NULL otherwise.
 */
MS2_PUBLIC const char *ms_snd_card_get_driver_type(const MSSndCard *obj);

/**
 * Retreive a sound card's name.
 *
 * @param obj   A sound card object.
 *
 * Returns: a string if successfull, NULL otherwise.
 */
MS2_PUBLIC const char *ms_snd_card_get_name(const MSSndCard *obj);

/**
 * Retreive sound card's name ($driver_type: $name).
 *
 * @param obj    A sound card object.
 *
 * Returns: A string if successfull, NULL otherwise.
 */
MS2_PUBLIC const char *ms_snd_card_get_string_id(MSSndCard *obj);


/**
 * Retreive sound card's capabilities.
 *
 * <PRE>
 *   MS_SND_CARD_CAP_CAPTURE
 *   MS_SND_CARD_CAP_PLAYBACK
 *   MS_SND_CARD_CAP_CAPTURE|MS_SND_CARD_CAP_PLAYBACK
 *   MS_SND_CARD_CAP_BUILTIN_ECHO_CANCELLER
 * </PRE>
 *
 * @param obj    A sound card object.
 *
 * Returns: A unsigned int if successfull, 0 otherwise.
 */
MS2_PUBLIC unsigned int ms_snd_card_get_capabilities(const MSSndCard *obj);

/**
 * Returns the sound card minimal latency (playback+record), in milliseconds.
 * This value is to be used by the software echo cancellers to know where to search for the echo (optimization).
 * Typically, an echo shall not be found before the value returned by this function.
 * If this value is not known, then it should return 0.
 * @param obj    A sound card object.
**/
MS2_PUBLIC int ms_snd_card_get_minimal_latency(MSSndCard *obj);

/**
 * Set some mixer level value.
 *
 * <PRE>
 *   MS_SND_CARD_MASTER,
 *   MS_SND_CARD_PLAYBACK,
 *   MS_SND_CARD_CAPTURE
 * </PRE>
 * Note: not implemented on all sound card filters.
 *
 * @param obj      A sound card object.
 * @param e        A sound card mixer object.
 * @param percent  A volume level.
 *
 */
MS2_PUBLIC void ms_snd_card_set_level(MSSndCard *obj, MSSndCardMixerElem e, int percent);

/**
 * Get some mixer level value.
 *
 * <PRE>
 *   MS_SND_CARD_MASTER,
 *   MS_SND_CARD_PLAYBACK,
 *   MS_SND_CARD_CAPTURE
 * </PRE>
 * Note: not implemented on all sound card filters.
 *
 * @param obj      A sound card object.
 * @param e        A sound card mixer object.
 *
 * Returns: A int if successfull, <0 otherwise.
 */
MS2_PUBLIC int ms_snd_card_get_level(MSSndCard *obj, MSSndCardMixerElem e);

/**
 * Set some source for capture.
 *
 * <PRE>
 *   MS_SND_CARD_MIC,
 *   MS_SND_CARD_LINE
 * </PRE>
 * Note: not implemented on all sound card filters.
 *
 * @param obj      A sound card object.
 * @param c        A sound card capture value.
 *
 * Returns: A int if successfull, 0 otherwise.
 */
MS2_PUBLIC void ms_snd_card_set_capture(MSSndCard *obj, MSSndCardCapture c);

/**
 * Set some mixer control.
 *
 * <PRE>
 *   MS_SND_CARD_MASTER_MUTE, -> 0: unmute, 1: mute
 *   MS_SND_CARD_PLAYBACK_MUTE, -> 0: unmute, 1: mute
 *   MS_SND_CARD_CAPTURE_MUTE -> 0: unmute, 1: mute
 * </PRE>
 * Note: not implemented on all sound card filters.
 *
 * @param obj      A sound card object.
 * @param e        A sound card control object.
 * @param percent  A value for control.
 *
 * Returns: 0 if successfull, <0 otherwise.
 */
MS2_PUBLIC int ms_snd_card_set_control(MSSndCard *obj, MSSndCardControlElem e, int val);

/**
 * Get some mixer control.
 *
 * <PRE>
 *   MS_SND_CARD_MASTER_MUTE, -> return 0: unmute, 1: mute
 *   MS_SND_CARD_PLAYBACK_MUTE, -> return 0: unmute, 1: mute
 *   MS_SND_CARD_CAPTURE_MUTE -> return 0: unmute, 1: mute
 * </PRE>
 * Note: not implemented on all sound card filters.
 *
 * @param obj      A sound card object.
 * @param e        A sound card mixer object.
 *
 * Returns: A int if successfull, <0 otherwise.
 */
MS2_PUBLIC int ms_snd_card_get_control(MSSndCard *obj, MSSndCardControlElem e);

/**
 * Get preferred sample rate
 *
 * @param obj      A sound card object.
 *
 * Returns: return sample rate in khz
 */
MS2_PUBLIC int ms_snd_card_get_preferred_sample_rate(const MSSndCard *obj);

/**
 * set preferred sample rate. The underlying card will try to avoid any resampling for this samplerate.
 *
 * @param obj      A sound card object.
 * @param rate     sampling rate. 
 *
 * Returns:  0 if successfull, <0 otherwise.
 */
MS2_PUBLIC int ms_snd_card_set_preferred_sample_rate(MSSndCard *obj,int rate);
	

/**
 * Create a alsa card with user supplied pcm name and mixer name.
 * @param pcmdev The pcm device name following alsa conventions (ex: plughw:0)
 * @param mixdev The mixer device name following alsa conventions.
 *
 * Returns: a MSSndCard object, NULL if alsa support is not available.
 */
MS2_PUBLIC MSSndCard * ms_alsa_card_new_custom(const char *pcmdev, const char *mixdev);


/**
 * Use supplied sample rate to open alsa devices (forced rate).
 * Has no interest except workarouding driver bugs.
 * Use -1 to revert to normal behavior.
**/
MS2_PUBLIC void ms_alsa_card_set_forced_sample_rate(int samplerate);

/** @} */

#ifdef __cplusplus
}
#endif

/** @} */

#endif

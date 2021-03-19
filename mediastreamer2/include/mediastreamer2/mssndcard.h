/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
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

#ifndef sndcard_h
#define sndcard_h

#include <mediastreamer2/mscommon.h>
#include <mediastreamer2/msfactory.h>
/**
 * @file mssndcard.h
 * @brief mediastreamer2 mssndcard.h include file
 *
 * This file provide the API needed to manage
 * soundcard filters.
 *
 */

/**
 * @addtogroup mediastreamer2_soundcard
 * @{
 */

struct _MSSndCardManager{
	MSFactory* factory;
	MSList *cards;
	MSList *descs;
	char *paramString;
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
typedef void (*MSSndCardSetUsageHintFunc)(struct _MSSndCard *obj, bool_t is_going_to_be_used);
typedef void (*MSSndCardUnloadFunc)(MSSndCardManager *obj);
typedef void (*MSSndCardAudioSessionFunc)(struct _MSSndCard *obj, bool_t actived);
typedef void (*MSSndCardCallKitFunc)(struct _MSSndCard *obj, bool_t enabled);
typedef void (*MSSndCardAudioRouteFunc)(struct _MSSndCard *obj);
typedef void (*MSSndCardConfigureFunc)(struct _MSSndCard *obj);


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
	MSSndCardSetUsageHintFunc usage_hint;
	MSSndCardAudioSessionFunc audio_session_activated;
	MSSndCardCallKitFunc callkit_enabled;
	MSSndCardAudioRouteFunc audio_route_changed;
	MSSndCardConfigureFunc configure;
};

/**
 * Structure for sound card description object.
 * @var MSSndCardDesc
 */
typedef struct _MSSndCardDesc MSSndCardDesc;

/**
 * Structure for sound card object.
 * @var MSSndCard
 */
typedef struct _MSSndCard MSSndCard;

enum _MSSndCardStreamType{
	MS_SND_CARD_STREAM_VOICE,
	MS_SND_CARD_STREAM_RING,
	MS_SND_CARD_STREAM_MEDIA,
	MS_SND_CARD_STREAM_DTMF,
};

/**
 * Structure for sound card stream type.
 */
typedef enum _MSSndCardStreamType MSSndCardStreamType;

enum _MSSndCardDeviceType {
	MS_SND_CARD_DEVICE_TYPE_TELEPHONY,
	MS_SND_CARD_DEVICE_TYPE_AUX_LINE,
	MS_SND_CARD_DEVICE_TYPE_GENERIC_USB,
	MS_SND_CARD_DEVICE_TYPE_HEADSET,
	MS_SND_CARD_DEVICE_TYPE_MICROPHONE,
	MS_SND_CARD_DEVICE_TYPE_EARPIECE,
	MS_SND_CARD_DEVICE_TYPE_HEADPHONES,
	MS_SND_CARD_DEVICE_TYPE_SPEAKER,
	MS_SND_CARD_DEVICE_TYPE_BLUETOOTH,
	MS_SND_CARD_DEVICE_TYPE_BLUETOOTH_A2DP,
	MS_SND_CARD_DEVICE_TYPE_UNKNOWN
};

/**
 * device type enum.
 * @var DeviceType
**/
typedef enum _MSSndCardDeviceType MSSndCardDeviceType;

#define MS_SND_CARD_CAP_DISABLED (0) /**<This soundcard is disabled.*/
#define MS_SND_CARD_CAP_CAPTURE (1) /**<This sound card can capture sound */
#define MS_SND_CARD_CAP_PLAYBACK (1<<1) /**<This sound card can playback sound */
#define MS_SND_CARD_CAP_BUILTIN_ECHO_CANCELLER (1<<2) /**<This sound card has built-in echo cancellation*/
#define MS_SND_CARD_CAP_IS_SLOW (1<<3) /**<This sound card is very slow to start*/

struct _MSSndCard{
	MSSndCardDesc *desc;
	MSSndCardManager* sndcardmanager;
	char *name;
	char *id;
	int  internal_id;
	unsigned int capabilities;
	MSSndCardDeviceType device_type;
	void *data;
	int preferred_sample_rate;
	int latency;
	MSSndCardStreamType streamType;
	int ref_count;
};

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
 * @deprecated use ms_factory_get_snd_card_manager()
 */
MS2_PUBLIC MS2_DEPRECATED MSSndCardManager * ms_snd_card_manager_get(void);

/**
 * Retrieve a factory from a sound card object.
 * @param c MSSndCard object.
 * Returns: MSFactory pointer.
 */
MS2_PUBLIC MSFactory * ms_snd_card_get_factory(MSSndCard * c);

/**
 * Create a sound card manager object.
 * You usually do not need this function, instead get the sound card manager from a factory
 * with ms_factory_get_snd_card_manager().
 */
MS2_PUBLIC MSSndCardManager * ms_snd_card_manager_new(void);

/**
 * Destroy a sound card manager object.
 * You usually do not need this function, the ms_factory_destroy() doing this job for you.
 */
MS2_PUBLIC void ms_snd_card_manager_destroy(MSSndCardManager* sndcardmanager);

/**
 * Set the sound card manager's parameter string
 *
 * @param m    A sound card manager.
 * @param paramString  A string of the form "param1=true;param2=42;param3=false"
 *
 * Returns: MSSndCard if successfull, NULL otherwise.
 */
MS2_PUBLIC void ms_snd_card_manager_set_param_string(MSSndCardManager *m, const char *paramString);

/**
 * Retreive a sound card object based on it's id.
 *
 * @param m    A sound card manager containing sound cards.
 * @param id   An id for card to search.
 *
 * Returns: MSSndCard if successfull, NULL otherwise.
 */
MS2_PUBLIC MSSndCard * ms_snd_card_manager_get_card(MSSndCardManager *m, const char *id);

/**
 * Retreive a sound card object based on it's id and capabilities.
 *
 * @param m    A sound card manager containing sound cards.
 * @param id   An id for card to search.
 * @param capabilities A capabilities mask of MS_SND_CARD_CAP_PLAYBACK and/or MS_SND_CARD_CAP_CAPTURE
 *
 * Returns: MSSndCard if successfull, NULL otherwise.
 */
MS2_PUBLIC MSSndCard * ms_snd_card_manager_get_card_with_capabilities(MSSndCardManager *m, const char *id, unsigned int capabilities);

/**
 * Retreive the first sound card object in the card manager based on its type and driver type.
 *
 * @param m             A sound card manager containing sound cards.
 * @param driver_type   The type of the driver.
 * @param type          The type of the card.
 *
 * Returns: MSSndCard if successfull, NULL otherwise.
 */
MS2_PUBLIC MSSndCard * ms_snd_card_manager_get_card_by_type(MSSndCardManager *m, const MSSndCardDeviceType type, const char * driver_type);

/**
 * Retreive a playback capable sound card object based on its name.
 *
 * @param m    A sound card manager containing sound cards.
 * @param id   A name for card to search.
 *
 * Returns: MSSndCard if successfull, NULL otherwise.
 */
MS2_PUBLIC MSSndCard * ms_snd_card_manager_get_playback_card(MSSndCardManager *m, const char *id);

/**
 * Retreive all sound cards having the name provided as input.
 *
 * @param m      A sound card manager containing sound cards.
 * @param name   A name for card to search.
 *
 * Returns: MSSndCard list of cards if successfull, NULL otherwise.
 */
MS2_PUBLIC bctbx_list_t * ms_snd_card_manager_get_all_cards_with_name(MSSndCardManager *m, const char *name);

/**
 * Retreive a capture capable sound card object based on its name.
 *
 * @param m    A sound card manager containing sound cards.
 * @param id   A name for card to search.
 *
 * Returns: MSSndCard if successfull, NULL otherwise.
 */
MS2_PUBLIC MSSndCard * ms_snd_card_manager_get_capture_card(MSSndCardManager *m, const char *id);

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
 * Prepend a sound card object in a sound card manager's list.
 *
 * @param m    A sound card manager containing sound cards.
 * @param c    A sound card object.
 *
 */
MS2_PUBLIC void ms_snd_card_manager_prepend_card(MSSndCardManager *m, MSSndCard *c);
	
/**
 * Set the sound card manager of a sound card.
 *
 * @param m    A sound card manager containing sound cards.
 * @param c    A sound card object.
 *
 */
MS2_PUBLIC void ms_snd_card_set_manager(MSSndCardManager*m, MSSndCard *c);

/**
 * Prepend a list of sound card object to the sound card manager's list.
 * @param[in] m A sound card manager containing sound cards.
 * @param[in] l A list of sound card objects to be prepended to the sound card manager's list.
 */
MS2_PUBLIC void ms_snd_card_manager_prepend_cards(MSSndCardManager *m, MSList *l);

/**
 * Register a sound card description in a sound card manager.
 *
 * @param m      A sound card manager containing sound cards.
 * @param desc   A sound card description object.
 *
 */
MS2_PUBLIC void ms_snd_card_manager_register_desc(MSSndCardManager *m, MSSndCardDesc *desc);

/**
 * Unregister a sound card description in a sound card manager.
 *
 * @param m      A sound card manager containing sound cards.
 * @param desc   A sound card description object.
 *
 */
MS2_PUBLIC void ms_snd_card_manager_unregister_desc(MSSndCardManager *m, MSSndCardDesc *desc);

/**
 * Ask all registered MSSndCardDesc to re-detect their soundcards.
 * @param m The sound card manager.
**/
MS2_PUBLIC void ms_snd_card_manager_reload(MSSndCardManager *m);

/**
 * Check if there is another card in the manager having same driver_type, name and device_type
 * @param m    Card Manager
 * @param card Card to compare properties against
 * @param checkCapabilities flag to check capabilities
 *
 * Returns: true if a duplicate has been found, false otherwise
**/
MS2_PUBLIC bool_t ms_snd_card_is_card_duplicate(MSSndCardManager *m, MSSndCard * card, bool_t checkCapabilities);

/**
 * Prevent card type to be at the head fo the list
 * @param m    Card Manager
 * @param type Card type to remove from the head of list of cards
 *
**/
MS2_PUBLIC void ms_snd_card_remove_type_from_list_head(MSSndCardManager *m, MSSndCardDeviceType type);

/**
 * Swap two position of 2 sound cards in the sound card manager.
 * @param m       Card Manager
 * @param card0   Card to be swapped
 * @param card1   Card to be swapped
 *
 * Returns: true if card0 and card1 are not null and both are found among the list of sound cards in the card manager, false otherwise
**/
MS2_PUBLIC bool_t ms_snd_card_manager_swap_cards(MSSndCardManager *m, MSSndCard *card0, MSSndCard *card1);

/* This function is available for testing only, this should not be used in a real application! */
MS2_PUBLIC void ms_snd_card_manager_bypass_soundcard_detection(bool_t value);

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
 * @param name The card name
 *
 * Returns: MSSndCard if successfull, NULL otherwise.
 */
	
MS2_PUBLIC MSSndCard * ms_snd_card_new_with_name(MSSndCardDesc *desc,const char* name);
/**
 * Destroy sound card object.
 *
 * @param obj   A MSSndCard object.
 * @deprecated, use ms_snd_card_unref instead
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
 * Retreive a sound card's device type.
 *
 * @param obj   A sound card object.
 *
 * Returns: an MSSndCardDeviceType enum type.
 * Default value is MSSndCardDeviceType::MS_SND_CARD_DEVICE_TYPE_UNKNOWN.
 */
MS2_PUBLIC MSSndCardDeviceType ms_snd_card_get_device_type(const MSSndCard *obj);

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
 * Retreive a sound card's device type string.
 *
 * @param type   A sound card type.
 *
 * Returns: a string if successfull, "bad type" otherwise.
 */
MS2_PUBLIC const char * ms_snd_card_device_type_to_string(const MSSndCardDeviceType type);

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
 * Retrieve sound card's internal ID.
 *
 * @param obj    A sound card object.
 *
 * Returns: An integer storing the internal ID value.
 */
MS2_PUBLIC int ms_snd_card_get_internal_id(MSSndCard *obj);

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
 * Set internal ID of the sound card.
 *
 * @param obj      A sound card object.
 * @param id       A sound card internal ID.
 *
 */
MS2_PUBLIC void ms_snd_card_set_internal_id(MSSndCard *obj, int id);

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
 * @param val  A value for control.
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
 * Enable application to tell that the soundcard is going to be used or will cease to be used.
 * This is recommended for cards which are known to be slow (see flag MS_SND_CARD_CAP_IS_SLOW ).
**/
MS2_PUBLIC void ms_snd_card_set_usage_hint(MSSndCard *obj, bool_t is_going_to_be_used);

/**
 * Used by application to notify whether audio access is allowed for the process.
 * On most platform this function is useless, but in an iOS application using Callkit, the system decides when audio (through the AVAudioSession singleton) is open or closed.
 * Such application needs to explicitely notify mediastreamer2 with ms_snd_card_notify_audio_session_activated() about the state of the audio session.
 *
 * @param obj      A sound card object.
 * @param actived    TRUE if audio session is activated, FALSE otherwise.
 */
MS2_PUBLIC void ms_snd_card_notify_audio_session_activated(MSSndCard *obj, bool_t activated);

/**
 * Used by application to notify whether audio route is changed. On most platform this function is useless.
 * But  an iOS application needs to explicitely notify mediastreamer2 with ms_snd_card_notify_audio_route_changed() about the changment of audio route to ajust the sample rate for playback/record.
 *
 * @param obj      A sound card object.
*/
MS2_PUBLIC void ms_snd_card_notify_audio_route_changed(MSSndCard *obj);

/**
 * Used by application to tell the MSSndCard if rely on notifications of activation of audio session.
 * When yesno is set to FALSE, the MSSndCard will not rely on notifications of activation of audio session, and will assume that audio is always usable.
 * If set to TRUE, the mediastreamer2 will require explicit calls to ms_snd_card_notify_audio_session_activated().
 *
 * @param obj      A sound card object.
 * @param yesno    TRUE if app notifies is activated, FALSE otherwise. The default value is FALSE.
 */
MS2_PUBLIC void ms_snd_card_app_notifies_activation(MSSndCard *obj, bool_t yesno);

/**
 * Used to configure audio session with default settings. Callkit usage.
 * @param obj      A sound card object.
 */
MS2_PUBLIC void ms_snd_card_configure_audio_session(MSSndCard *obj);

/**
 * Sets the stream type for this soundcard, default is VOICE
**/
MS2_PUBLIC void ms_snd_card_set_stream_type(MSSndCard *obj, MSSndCardStreamType type);

/**
 * Gets the stream type for this soundcard, default is VOICE
**/
MS2_PUBLIC MSSndCardStreamType ms_snd_card_get_stream_type(MSSndCard *obj);

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

/**
 * Returns a string value of the given MSSndCardDeviceType enum
 */
MS2_PUBLIC const char* ms_snd_card_device_type_to_string(MSSndCardDeviceType type);

/**
 * Takes a ref on a MSSndCard
 */
MS2_PUBLIC MSSndCard* ms_snd_card_ref(MSSndCard *sndCard);

/**
 * Removes a ref from a MSSndCard
 */
MS2_PUBLIC void ms_snd_card_unref(MSSndCard *sndCard);

#ifdef __ANDROID__
/**
 * Sort cards in order to put earpiece and speaker as first devices of every filter.
**/
void ms_snd_card_sort(MSSndCardManager *m);
#endif // __ANDROID__

/** @} */

#ifdef __cplusplus
}
#endif

/** @} */

#endif

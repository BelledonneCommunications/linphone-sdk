/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2014  Belledonne Communications SARL

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

#ifndef msfactory_h
#define msfactory_h


#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/devices.h"

/*do not use these fields directly*/
struct _MSFactory{
	MSList *desc_list;
	MSList *stats_list;
	MSList *offer_answer_provider_list;
#ifdef _WIN32
	MSList *ms_plugins_loaded_list;
#endif
	MSList *formats;
	MSList *platform_tags;
	char *plugins_dir;
	struct _MSVideoPresetsManager *video_presets_manager;
	int cpu_count;
	struct _MSEventQueue *evq;
	int max_payload_size;
	int mtu;
	struct _MSSndCardManager* sndcardmanager;
	struct _MSWebCamManager* wbcmanager;
	void (*voip_uninit_func)(struct _MSFactory*);
	bool_t statistics_enabled;
	bool_t voip_initd;
	MSDevicesInfo *devices_info;
};

typedef struct _MSFactory MSFactory;

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MS2_DEPRECATED
#if defined(_MSC_VER)
#define MS2_DEPRECATED __declspec(deprecated)
#else
#define MS2_DEPRECATED __attribute__ ((deprecated))
#endif
#endif
	
/**
 * Create a mediastreamer2 factory. This is the root object that will create everything else from mediastreamer2.
**/
MS2_PUBLIC MSFactory *ms_factory_new(void);

/**
 * Create a mediastreamer2 factory and initialize all voip related filter, card and webcam managers.
**/
MS2_PUBLIC MSFactory* ms_factory_new_with_voip(void);

/**
 * Create the fallback factory (for compatibility with applications not using MSFactory to create ms2 object)
**/
MS2_DEPRECATED MS2_PUBLIC MSFactory *ms_factory_create_fallback(void);

/**
 * Used by the legacy functions before MSFactory was added.
 * Do not use in an application.
**/
MS2_DEPRECATED MS2_PUBLIC MSFactory *ms_factory_get_fallback(void);

/**
 * Destroy the factory.
 * This should be done after destroying all objects created by the factory.
**/
MS2_PUBLIC void ms_factory_destroy(MSFactory *factory);

/*
 * Obtain the soundcard manager.
**/
MS2_PUBLIC struct _MSSndCardManager* ms_factory_get_snd_card_manager(MSFactory *f);

/**
 * Obtain the webcam manager.
*/
MS2_PUBLIC struct _MSWebCamManager* ms_factory_get_web_cam_manager(MSFactory* f);


/**
 * Register a filter descriptor (MSFilterDesc) into the factory.
**/
MS2_PUBLIC void ms_factory_register_filter(MSFactory *factory, MSFilterDesc *desc);

/**
 * Retrieve capture filter that supports encoding to codec name.
 *
 * @param mime    A string indicating the codec.
 *
 * @return a MSFilterDesc if successfull, NULL otherwise.
 */
MS2_PUBLIC MSFilterDesc * ms_factory_get_encoding_capturer(MSFactory *factory, const char *mime);

/**
 * Retrieve render filter that supports decoding to codec name.
 *
 * @param mime    A string indicating the codec.
 *
 * @return a MSFilterDesc if successfull, NULL otherwise.
 */
MS2_PUBLIC MSFilterDesc * ms_factory_get_decoding_renderer(MSFactory *factory, const char *mime);

/**
 * Retrieve encoders according to codec name.
 *
 * @param mime    A string indicating the codec.
 *
 * @return a MSFilterDesc if successfull, NULL otherwise.
 */
MS2_PUBLIC MSFilterDesc * ms_factory_get_encoder(MSFactory *factory, const char *mime);

/**
 * Retrieve decoders according to codec name.
 *
 * @param mime    A string indicating the codec.
 *
 * @return a MSFilterDesc if successfull, NULL otherwise.
 */
MS2_PUBLIC MSFilterDesc * ms_factory_get_decoder(MSFactory *factory, const char *mime);

/**
 * Lookup a mediastreamer2 filter using its name.
 * If found, the descriptor (MSFilterDesc) is returned.
 * This descriptor can be used to instanciate the filter using ms_filter_new_from_desc()
 * This function can be useful to query the presence of a filter loaded as a plugin, for example.
 *
 * @param filter_name The filter name.
**/
MS2_PUBLIC MSFilterDesc *ms_factory_lookup_filter_by_name(const MSFactory *factory, const char *filter_name);

/**
 * Lookup a mediastreamer2 filter using its id.
 * If found, the descriptor (MSFilterDesc) is returned.
 * This descriptor can be used to instanciate the filter using ms_filter_new_from_desc()
 * This function can be useful to query the presence of a filter loaded as a plugin, for example.
 *
 * @param id The filter id.
**/
MS2_PUBLIC MSFilterDesc* ms_factory_lookup_filter_by_id( MSFactory* factory, MSFilterId id);

/**
 * Returns a list of filter descriptions implementing a given interface.
 * The list itself must be freed by the caller of this function, but not the MSFilterDesc pointed by the list elements.
 * @param id a filter interface id
 * @return a newly allocated MSList of #MSFilterDesc.
**/
MS2_PUBLIC MSList *ms_factory_lookup_filter_by_interface(MSFactory *factory, MSFilterInterfaceId id);

/**
 * Create encoder filter according to codec name.
 *
 * @param mime    A string indicating the codec.
 *
 * @return a MSFilter if successfull, NULL otherwise.
 */
MS2_PUBLIC MSFilter * ms_factory_create_encoder(MSFactory *factory, const char *mime);

/**
 * Create decoder filter according to codec name.
 *
 * @param mime    A string indicating the codec.
 *
 * @return a MSFilter if successfull, NULL otherwise.
 */
MS2_PUBLIC MSFilter * ms_factory_create_decoder(MSFactory *factory, const char *mime);

/**
 * Check if a encode or decode filter exists for a codec name.
 *
 * @param mime    A string indicating the codec.
 *
 * @return TRUE if successfull, FALSE otherwise.
 */
MS2_PUBLIC bool_t ms_factory_codec_supported(MSFactory *factory, const char *mime);

/**
 * Create decoder filter according to a filter's MSFilterId.
 *
 * @param id     A MSFilterId identifier for the filter.
 *
 * @return a MSFilter if successfull, NULL otherwise.
 */
MS2_PUBLIC MSFilter *ms_factory_create_filter(MSFactory *factory, MSFilterId id);

/**
 * Create decoder filter according to a filter's name.
 *
 * @param name   A name for the filter.
 *
 * @return a MSFilter if successfull, NULL otherwise.
 */
MS2_PUBLIC MSFilter *ms_factory_create_filter_from_name(MSFactory *factory, const char *name);

/**
 * Create decoder filter according to a filter's description.
 *
 * The primary use is to create your own filter's in your
 * application and avoid registration inside mediastreamer2.
 *
 * @param desc   A MSFilterDesc for the filter.
 *
 * @return a MSFilter if successfull, NULL otherwise.
 */
MS2_PUBLIC MSFilter *ms_factory_create_filter_from_desc(MSFactory *factory, MSFilterDesc *desc);

/**
 * Enable filter statistics measurement at run time.
**/
MS2_PUBLIC void ms_factory_enable_statistics(MSFactory* obj, bool_t enabled);

/**
 * Obtain a list of MSFilterStats.
**/
MS2_PUBLIC const MSList * ms_factory_get_statistics(MSFactory* obj);

/**
 * Reset filter's statistics.
**/
MS2_PUBLIC void ms_factory_reset_statistics(MSFactory *obj);

/**
 * Output statistics to logs.
**/
MS2_PUBLIC void ms_factory_log_statistics(MSFactory *obj);

/**
 * Get number of available cpus for processing.
 * The factory initializes this value to the number of logicial processors
 * available on the machine where it runs.
**/
MS2_PUBLIC unsigned int ms_factory_get_cpu_count(MSFactory *obj);

/**
 * Set the number of available cpus for processing.
**/
MS2_PUBLIC void ms_factory_set_cpu_count(MSFactory *obj, unsigned int c);

MS2_PUBLIC void ms_factory_add_platform_tag(MSFactory *obj, const char *tag);

MS2_PUBLIC MSList * ms_factory_get_platform_tags(MSFactory *obj);

MS2_PUBLIC char * ms_factory_get_platform_tags_as_string(MSFactory *obj);

MS2_PUBLIC struct _MSVideoPresetsManager * ms_factory_get_video_presets_manager(MSFactory *factory);

MS2_PUBLIC void ms_factory_init_plugins(MSFactory *obj);

/**
 * Set directory where plugins are to be loaded.
**/
MS2_PUBLIC void ms_factory_set_plugins_dir(MSFactory *obj, const char *path);

MS2_PUBLIC int ms_factory_load_plugins(MSFactory *factory, const char *dir);

MS2_PUBLIC void ms_factory_uninit_plugins(MSFactory *obj);

/**
 * Init VOIP features (registration of codecs, sound card and webcam managers).
**/
MS2_PUBLIC void ms_factory_init_voip(MSFactory *obj);

MS2_PUBLIC void ms_factory_uninit_voip(MSFactory *obj);

/**
 * Creates an event queue.
 * Only one can exist so if it has already been created the same one will be returned.
 * @param[in] obj MSFactory object.
 * @return The created event queue.
 */
MS2_PUBLIC struct _MSEventQueue * ms_factory_create_event_queue(MSFactory *obj);
	
MS2_PUBLIC void ms_factory_destroy_event_queue(MSFactory *obj);
	
	/**
 * Gets the event queue associated with the factory.
 * Can be NULL if no event queue has been created.
 * @param[in] obj MSFactory object.
 * @return The event queue associated with the factory.
 */
MS2_PUBLIC struct _MSEventQueue * ms_factory_get_event_queue(MSFactory *obj);

MS2_PUBLIC void ms_factory_set_event_queue(MSFactory *obj,struct _MSEventQueue *q);

MS2_PUBLIC int ms_factory_get_payload_max_size(MSFactory *factory);

MS2_PUBLIC void ms_factory_set_payload_max_size(MSFactory *obj, int size);
	
MS2_PUBLIC void ms_factory_set_mtu(MSFactory *obj, int mtu);

MS2_PUBLIC int ms_factory_get_mtu(MSFactory *obj);

MS2_PUBLIC const struct _MSFmtDescriptor * ms_factory_get_audio_format(MSFactory *obj, const char *mime, int rate, int channels, const char *fmtp);

MS2_PUBLIC const struct _MSFmtDescriptor * ms_factory_get_video_format(MSFactory *obj, const char *mime, MSVideoSize size, float fps, const char *fmtp);

MS2_PUBLIC const MSFmtDescriptor *ms_factory_get_format(MSFactory *obj, const MSFmtDescriptor *ref);

/**
 * Specifies if a filter is enabled or not. Only enabled filter are return by functions like ms_filter_get_encoder
 * @param factory
 * @param name   A name for the filter.
 * @param enable, true/false
 * @return 0 in case of success
 *
 */
MS2_PUBLIC int ms_factory_enable_filter_from_name(MSFactory *factory, const char *name, bool_t enable);

/**
 * Specifies if a filter is enabled or not. Only enabled filter are return by functions like ms_filter_get_encoder
 *
 * @param factory
 * @param name   A name for the filter.
 * @return true/false if enabled
 *
 */
MS2_PUBLIC bool_t ms_factory_filter_from_name_enabled(const MSFactory *factory, const char *name);


#ifndef MS_OFFER_ANSWER_CONTEXT_DEFINED
#define MS_OFFER_ANSWER_CONTEXT_DEFINED
typedef struct _MSOfferAnswerContext MSOfferAnswerContext;
#endif
typedef struct _MSOfferAnswerProvider MSOfferAnswerProvider;

/**
 * Registers an offer-answer provider. An offer answer provider is a kind of factory that creates
 * context objects able to execute the particular offer/answer logic for a given codec.
 * Indeed, several codecs have complex parameter handling specified in their RFC, and hence cannot be
 * treated in a generic way by the global SDP offer answer logic.
 * Mediastreamer2 plugins can then register with this method their offer/answer logic together with the encoder
 * and decoder filters, so that it can be used by the signaling layer of the application.
 * @param factory 
 * @param offer_answer_prov the offer answer provider descriptor.
**/
MS2_PUBLIC void ms_factory_register_offer_answer_provider(MSFactory *f, MSOfferAnswerProvider *offer_answer_prov);

/**
 * Retrieve an offer answer provider previously registered, giving the codec name.
 * @param f the factory
 * @param mime_type the codec mime type.
 * @return an MSOfferAnswerProvider or NULL if none was registered for this codec.
**/
MS2_PUBLIC MSOfferAnswerProvider * ms_factory_get_offer_answer_provider(MSFactory *f, const char *mime_type);

/**
 * Directly creates an offer-answer context giving the codec mime-type.
 * @param f the factory
 * @param the mime-type of the codec.
 * @return an MSOfferAnswerContext or NULL if none was registered for this codec.
**/
MS2_PUBLIC MSOfferAnswerContext * ms_factory_create_offer_answer_context(MSFactory *f, const char *mime_type);

MS2_PUBLIC MSDevicesInfo* ms_factory_get_devices_info(MSFactory *f);

#ifdef __cplusplus
}
#endif
	
#endif

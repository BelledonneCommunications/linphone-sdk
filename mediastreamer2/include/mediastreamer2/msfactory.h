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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef msfactory_h
#define msfactory_h

/*do not use these fields directly*/
struct _MSFactory{
	MSList *desc_list;
	MSList *stats_list;
#ifdef WIN32
	MSList *ms_plugins_loaded_list;
#endif
	MSList *formats;
	char *plugins_dir;
	int cpu_count;
	struct _MSEventQueue *evq;
	int max_payload_size;
	int mtu;
	bool_t statistics_enabled;
	bool_t voip_initd;
};

typedef struct _MSFactory MSFactory;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create a mediastreamer2 factory. This is the root object that will create everything else from mediastreamer2.
**/
MS2_PUBLIC MSFactory *ms_factory_new(void);

/**
 * Create the factory default callback.
**/
MS2_PUBLIC MSFactory *ms_factory_create_fallback(void);

/**
 * Used by the legacy functions before MSFactory was added.
 * Do not use in an application.
**/
MS2_PUBLIC MSFactory *ms_factory_get_fallback(void);

/**
 * Destroy the factory.
 * This should be done after destroying all objects created by the factory.
**/
MS2_PUBLIC void ms_factory_destroy(MSFactory *factory);

MS2_PUBLIC void ms_factory_register_filter(MSFactory *factory, MSFilterDesc *desc);

/**
 * Retrieve capture filter that supports encoding to codec name.
 *
 * @param mime    A string indicating the codec.
 *
 * Returns: a MSFilterDesc if successfull, NULL otherwise.
 */
MS2_PUBLIC MSFilterDesc * ms_factory_get_encoding_capturer(MSFactory *factory, const char *mime);

/**
 * Retrieve render filter that supports decoding to codec name.
 *
 * @param mime    A string indicating the codec.
 *
 * Returns: a MSFilterDesc if successfull, NULL otherwise.
 */
MS2_PUBLIC MSFilterDesc * ms_factory_get_decoding_renderer(MSFactory *factory, const char *mime);

/**
 * Retrieve encoders according to codec name.
 *
 * Internal supported codecs:
 *    PCMU, PCMA, speex, gsm
 * Existing Public plugins:
 *    iLBC
 *
 * @param mime    A string indicating the codec.
 *
 * Returns: a MSFilterDesc if successfull, NULL otherwise.
 */
MS2_PUBLIC MSFilterDesc * ms_factory_get_encoder(MSFactory *factory, const char *mime);

/**
 * Retrieve decoders according to codec name.
 *
 * Internal supported codecs:
 *    PCMU, PCMA, speex, gsm
 * Existing Public plugins:
 *    iLBC
 *
 * @param mime    A string indicating the codec.
 *
 * Returns: a MSFilterDesc if successfull, NULL otherwise.
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
MS2_PUBLIC MSFilterDesc *ms_factory_lookup_filter_by_name(MSFactory *factory, const char *filter_name);

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
MSList *ms_factory_lookup_filter_by_interface(MSFactory *factory, MSFilterInterfaceId id);

/**
 * Create encoder filter according to codec name.
 *
 * Internal supported codecs:
 *    PCMU, PCMA, speex, gsm
 * Existing Public plugins:
 *    iLBC
 *
 * @param mime    A string indicating the codec.
 *
 * Returns: a MSFilter if successfull, NULL otherwise.
 */
MS2_PUBLIC MSFilter * ms_factory_create_encoder(MSFactory *factory, const char *mime);

/**
 * Create decoder filter according to codec name.
 *
 * Internal supported codecs:
 *    PCMU, PCMA, speex, gsm
 * Existing Public plugins:
 *    iLBC
 *
 * @param mime    A string indicating the codec.
 *
 * Returns: a MSFilter if successfull, NULL otherwise.
 */
MS2_PUBLIC MSFilter * ms_factory_create_decoder(MSFactory *factory, const char *mime);

/**
 * Check if a encode or decode filter exists for a codec name.
 *
 * Internal supported codecs:
 *    PCMU, PCMA, speex, gsm
 * Existing Public plugins:
 *    iLBC
 *
 * @param mime    A string indicating the codec.
 *
 * Returns: TRUE if successfull, FALSE otherwise.
 */
MS2_PUBLIC bool_t ms_factory_codec_supported(MSFactory *factory, const char *mime);

/**
 * Create decoder filter according to a filter's MSFilterId.
 *
 * @param id     A MSFilterId identifier for the filter.
 *
 * Returns: a MSFilter if successfull, NULL otherwise.
 */
MS2_PUBLIC MSFilter *ms_factory_create_filter(MSFactory *factory, MSFilterId id);

/**
 * Create decoder filter according to a filter's name.
 *
 * @param name   A name for the filter.
 *
 * Returns: a MSFilter if successfull, NULL otherwise.
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
 * Returns: a MSFilter if successfull, NULL otherwise.
 */
MS2_PUBLIC MSFilter *ms_factory_create_filter_from_desc(MSFactory *factory, MSFilterDesc *desc);

MS2_PUBLIC void ms_factory_enable_statistics(MSFactory* obj, bool_t enabled);

MS2_PUBLIC const MSList * ms_factory_get_statistics(MSFactory* obj);

MS2_PUBLIC void ms_factory_reset_statistics(MSFactory *obj);

MS2_PUBLIC void ms_factory_log_statistics(MSFactory *obj);

MS2_PUBLIC unsigned int ms_factory_get_cpu_count(MSFactory *obj);

MS2_PUBLIC void ms_factory_set_cpu_count(MSFactory *obj, unsigned int c);

MS2_PUBLIC void ms_factory_init_plugins(MSFactory *obj);

MS2_PUBLIC void ms_factory_set_plugins_dir(MSFactory *obj, const char *path);

MS2_PUBLIC int ms_factory_load_plugins(MSFactory *factory, const char *dir);

MS2_PUBLIC void ms_factory_uninit_plugins(MSFactory *obj);

MS2_PUBLIC void ms_factory_init_voip(MSFactory *obj);

MS2_PUBLIC void ms_factory_uninit_voip(MSFactory *obj);

MS2_PUBLIC struct _MSEventQueue * ms_factory_get_event_queue(MSFactory *obj);

MS2_PUBLIC void ms_factory_set_event_queue(MSFactory *obj,struct _MSEventQueue *q);

MS2_PUBLIC int ms_factory_get_payload_max_size(MSFactory *factory);

MS2_PUBLIC void ms_factory_set_payload_max_size(MSFactory *obj, int size);

MS2_PUBLIC const struct _MSFmtDescriptor * ms_factory_get_audio_format(MSFactory *obj, const char *mime, int rate, int channels, const char *fmtp);

MS2_PUBLIC const struct _MSFmtDescriptor * ms_factory_get_video_format(MSFactory *obj, const char *mime, MSVideoSize size, float fps, const char *fmtp);

MS2_PUBLIC const MSFmtDescriptor *ms_factory_get_format(MSFactory *obj, const MSFmtDescriptor *ref);

#ifdef __cplusplus
}
#endif
	
#endif

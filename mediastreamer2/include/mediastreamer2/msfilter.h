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

#ifndef msfilter_h
#define msfilter_h

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msqueue.h"
#include "mediastreamer2/allfilters.h"
#include "mediastreamer2/formats.h"

/**
 * @file msfilter.h
 * @brief mediastreamer2 msfilter.h include file
 *
 * This file provide the API needed to create, link,
 * unlink, find and destroy filter.
 *
 * It also provides definitions if you wish to implement
 * your own filters.
 *
 */

/**
 * @addtogroup mediastreamer2_filter
 * @{
 */

/**
 * Structure for filter's methods (init, preprocess, process, postprocess, uninit).
 * @var MSFilterFunc
 */
typedef void (*MSFilterFunc)(struct _MSFilter *f);

/**
 * Structure for filter's methods used to set filter's options.
 * @var MSFilterMethodFunc
 */
typedef int (*MSFilterMethodFunc)(struct _MSFilter *f, void *arg);

/**
 * Structure for filter's methods used as a callback to notify events.
 * @var MSFilterNotifyFunc
 */
typedef void (*MSFilterNotifyFunc)(void *userdata, struct _MSFilter *f, unsigned int id, void *arg);

struct _MSFilterMethod{
	unsigned int id;
	MSFilterMethodFunc method;
};


/**
 * Interface IDs, used to generate method names (see MS_FILTER_METHOD macro).
 * The purpose of these interfaces is to allow different filter implementations to share the same methods, by implementing the method definitions for these interfaces.
 * For example every video encoder implementation would need a method to request the generation of a key frame. Instead of having each implementation defining its own method to do this,
 * each implementation can just implement the MS_VIDEO_ENCODER_REQ_VFU method of the MSFilterVideoEncoderInterface.
**/
enum _MSFilterInterfaceId{
	MSFilterInterfaceBegin=16384,
	MSFilterPlayerInterface, /**<Player interface, used to control playing of files.*/
	MSFilterRecorderInterface,/**<Recorder interface, used to control recording of stream into files.*/
	MSFilterVideoDisplayInterface,/**<Video display interface, used to control the rendering of raw pictures onscreen.*/
	MSFilterEchoCancellerInterface,/**Echo canceller interface, used to control echo canceller implementations.*/
	MSFilterVideoDecoderInterface,/**<Video decoder interface*/
	MSFilterVideoCaptureInterface,/**<Video capture interface*/
	MSFilterAudioDecoderInterface,/**<Audio Decoder interface*/
	MSFilterVideoEncoderInterface,/**<Video encoder interface*/
	MSFilterAudioCaptureInterface,/**<Interface for audio capture filters*/
	MSFilterAudioPlaybackInterface,/**Interface for audio playback filters.*/
	MSFilterAudioEncoderInterface,/**<Video encoder interface*/
	MSFilterVoidInterface,/**<Void source/sink interface*/
};

/**
 * Interface IDs, used to generate method names (see MS_FILTER_METHOD macro).
 *
**/
typedef enum _MSFilterInterfaceId MSFilterInterfaceId;

/**
 * Structure for holding filter's methods to set filter's options.
 * @var MSFilterMethod
 */
typedef struct _MSFilterMethod MSFilterMethod;
/**
 * Filter's category
 *
 */
enum _MSFilterCategory{
	/**others*/
	MS_FILTER_OTHER,
	/**used by encoders*/
	MS_FILTER_ENCODER,
	/**used by decoders*/
	MS_FILTER_DECODER,
	/**used by capture filters that perform encoding*/
	MS_FILTER_ENCODING_CAPTURER,
	/**used by filters that perform decoding and rendering */
	MS_FILTER_DECODER_RENDERER
};

/**
 * Structure to describe filter's category.
 * <PRE>
 *     MS_FILTER_OTHER
 *     MS_FILTER_ENCODER
 *     MS_FILTER_DECODER
 *     MS_FILTER_ENCODING_CAPTURER
 *     MS_FILTER_DECODING_RENDERER
 * </PRE>
 * @var MSFilterCategory
 */
typedef enum _MSFilterCategory MSFilterCategory;

/**
 * Filter's flags controlling special behaviours.
**/
enum _MSFilterFlags{
	MS_FILTER_IS_PUMP = 1, /**< The filter must be called in process function every tick.*/
	/*...*/
	/*private flags: don't use it in filters.*/
	MS_FILTER_IS_ENABLED = 1<<31 /*<Flag to specify if a filter is enabled or not. Only enabled filters are returned by function ms_filter_get_encoder */
};

/**
 * Filter's flags controlling special behaviours.
**/
typedef enum _MSFilterFlags MSFilterFlags;


struct _MSFilterStats{
	const char *name; /*<filter name*/
	uint64_t elapsed; /*<cumulative number of nanoseconds elapsed */
	unsigned int count; /*<number of time the filter is called for processing*/
};

typedef struct _MSFilterStats MSFilterStats;

struct _MSFilterDesc{
	MSFilterId id;	/**< the id declared in allfilters.h */
	const char *name; /**< the filter name*/
	const char *text; /**< short text describing the filter's function*/
	MSFilterCategory category; /**< filter's category*/
	const char *enc_fmt; /**< sub-mime of the format, must be set if category is MS_FILTER_ENCODER or MS_FILTER_DECODER */
	int ninputs; /**< number of inputs */
	int noutputs; /**< number of outputs */
	MSFilterFunc init; /**< Filter's init function*/
	MSFilterFunc preprocess; /**< Filter's preprocess function, called one time before starting to process*/
	MSFilterFunc process; /**< Filter's process function, called every tick by the MSTicker to do the filter's job*/
	MSFilterFunc postprocess; /**< Filter's postprocess function, called once after processing (the filter is no longer called in process() after)*/
	MSFilterFunc uninit; /**< Filter's uninit function, used to deallocate internal structures*/
	MSFilterMethod *methods; /**<Filter's method table*/
	unsigned int flags; /**<Filter's special flags, from the MSFilterFlags enum.*/
};

/**
 * Structure for filter's description.
 * @var MSFilterDesc
 */
typedef struct _MSFilterDesc MSFilterDesc;

struct _MSFilter{
	MSFilterDesc *desc; /**<Back pointer to filter's descriptor.*/
	/*protected attributes, do not move or suppress any of them otherwise plugins will be broken */
	ms_mutex_t lock;
	MSQueue **inputs; /**<Table of input queues.*/
	MSQueue **outputs;/**<Table of output queues */
	struct _MSFactory *factory;/**<the factory that created this filter*/
	void *padding; /**Unused - to be reused later when new protected fields have to added*/
	void *data; /**< Pointer used by the filter for internal state and computations.*/
	struct _MSTicker *ticker; /**<Pointer to the ticker object. It is not NULL when being called process()*/
	/*private attributes, they can be moved and changed at any time*/
	MSList *notify_callbacks;
	uint32_t last_tick;
	MSFilterStats *stats;
	int postponed_task; /*number of postponed tasks*/
	bool_t seen;
};


/**
 * Structure of filter's object.
 * @var MSFilter
 */
typedef struct _MSFilter MSFilter;

struct _MSConnectionPoint{
	MSFilter *filter; /**<Pointer to filter*/
	int pin; /**<Pin index on the filter*/
};

/**
 * Structure that represents a connection point of a MSFilter
 * @var MSConnectionPoint
 */
typedef struct _MSConnectionPoint MSConnectionPoint;

struct _MSConnectionHelper{
	MSConnectionPoint last;
};

/**
 * Structure that holds data when using the ms_connection_helper_* functions.
 * @var MSConnectionHelper
**/
typedef struct _MSConnectionHelper MSConnectionHelper;


#ifdef __cplusplus
extern "C"{
#endif

/**
 * Register a filter description. (plugins use only!)
 *
 * When you build your own plugin, this method will
 * add the encoder or decoder to the internal list
 * of supported codec. Then, this plugin can be used
 * transparently from the application.
 *
 * ms_filter_get_encoder, ms_filter_get_decoder,
 * ms_filter_create_encoder, ms_filter_create_decoder
 * and ms_filter_codec_supported
 * can then be used as if the codec was internally.
 * supported.
 *
 * @param desc    a filter description.
 * @deprecated use ms_factory_register_filter().
 */
MS2_PUBLIC MS2_DEPRECATED void ms_filter_register(MSFilterDesc *desc);

/**
 * Retrieve capture filter that supports encoding to codec name.
 *
 * @param mime    A string indicating the codec.
 *
 * @return a MSFilterDesc if successfull, NULL otherwise.
 * @deprecated use ms_factory_get_encoding_capturer().
 */
MS2_PUBLIC MS2_DEPRECATED MSFilterDesc * ms_filter_get_encoding_capturer(const char *mime);

/**
 * Retrieve render filter that supports decoding to codec name.
 *
 * @param mime    A string indicating the codec.
 *
 * @returns a MSFilterDesc if successfull, NULL otherwise.
 * @deprecated use ms_factory_get_decoding_renderer()
 */
MS2_PUBLIC MS2_DEPRECATED MSFilterDesc * ms_filter_get_decoding_renderer(const char *mime);

/**
 * Retrieve encoders according to codec name.
 *
 *
 * @param mime    A string indicating the codec.
 *
 * @return a MSFilterDesc if successfull, NULL otherwise.
 * @deprecated use ms_factory_get_encoder().
 */
MS2_PUBLIC MS2_DEPRECATED MSFilterDesc * ms_filter_get_encoder(const char *mime);

/**
 * Retrieve decoders according to codec name.
 *
 *
 * @param mime    A string indicating the codec.
 *
 * @return a MSFilterDesc if successfull, NULL otherwise.
 * @deprecated use ms_factory_get_decoder().
 */
MS2_PUBLIC MS2_DEPRECATED MSFilterDesc * ms_filter_get_decoder(const char *mime);

/**
 * Lookup a mediastreamer2 filter using its name.
 * If found, the descriptor (MSFilterDesc) is returned.
 * This descriptor can be used to instanciate the filter using ms_filter_new_from_desc()
 * This function can be useful to query the presence of a filter loaded as a plugin, for example.
 *
 * @param filter_name The filter name.
 * @return a MSFilterDesc or NULL if no match.
 * @deprecated use ms_factory_lookup_filter_by_name().
**/
MS2_PUBLIC MS2_DEPRECATED MSFilterDesc *ms_filter_lookup_by_name(const char *filter_name);

/**
 * Returns a list of filter descriptions implementing a given interface.
 * The list itself must be freed by the caller of this function, but not the MSFilterDesc pointed by the list elements.
 * @param id a filter interface id
 * @return a newly allocated MSList of #MSFilterDesc.
 * @deprecated use ms_factory_lookup_filter_by_interface().
**/
MS2_PUBLIC MS2_DEPRECATED MSList *ms_filter_lookup_by_interface(MSFilterInterfaceId id);

/**
 * Create encoder filter according to codec name.
 
 * @param mime    A string indicating the codec.
 *
 * @return a MSFilter if successfull, NULL otherwise.
 * @deprecated use ms_factory_create_encoder().
 */
MS2_PUBLIC MS2_DEPRECATED MSFilter * ms_filter_create_encoder(const char *mime);

/**
 * Create decoder filter according to codec name.
 *
 *
 * @param mime    A string indicating the codec.
 *
 * @return a MSFilter if successfull, NULL otherwise.
 * @deprecated use ms_factory_create_decoder().
 */
MS2_PUBLIC MS2_DEPRECATED MSFilter * ms_filter_create_decoder(const char *mime);

/**
 * Check if both an encoder and a decoder filter exists for a codec name.
 *
 * @param mime    A string indicating the codec.
 *
 * @return TRUE if successfull, FALSE otherwise.
 * @deprecated use ms_factory_codec_supported().
 */
MS2_PUBLIC MS2_DEPRECATED bool_t ms_filter_codec_supported(const char *mime);

/**
 * Create decoder filter according to a filter's MSFilterId.
 *
 * @param id     A MSFilterId identifier for the filter.
 *
 * @returns a MSFilter if successfull, NULL otherwise.
 * @deprecated use ms_factory_create_filter().
 */
MS2_PUBLIC MS2_DEPRECATED MSFilter *ms_filter_new(MSFilterId id);

/**
 * Create decoder filter according to a filter's name.
 *
 * @param name   A name for the filter.
 *
 * @return a MSFilter if successfull, NULL otherwise.
 * @deprecated use ms_factory_create_filter_from_name().
 */
MS2_PUBLIC MS2_DEPRECATED MSFilter *ms_filter_new_from_name(const char *name);

/**
 * Create decoder filter according to a filter's description.
 *
 * The primary use is to create your own filter's in your
 * application and avoid registration inside mediastreamer2.
 *
 * @param desc   A MSFilterDesc for the filter.
 *
 * @return a MSFilter if successfull, NULL otherwise.
 * @deprecated use ms_factory_create_filter_from_desc()
 */
MS2_PUBLIC MS2_DEPRECATED MSFilter *ms_filter_new_from_desc(MSFilterDesc *desc);

/**
 * Link one OUTPUT pin from a filter to an INPUT pin of another filter.
 *
 * All data coming from the OUTPUT pin of one filter will be distributed
 * to the INPUT pin of the second filter.
 *
 * @param f1   A MSFilter object containing the OUTPUT pin
 * @param pin1 An index of an OUTPUT pin.
 * @param f2   A MSFilter object containing the INPUT pin
 * @param pin2 An index of an INPUT pin.
 *
 * Returns: 0 if sucessful, -1 otherwise.
 */
MS2_PUBLIC int ms_filter_link(MSFilter *f1, int pin1, MSFilter *f2, int pin2);

/**
 * Unlink one OUTPUT pin from a filter to an INPUT pin of another filter.
 *
 * @param f1   A MSFilter object containing the OUTPUT pin
 * @param pin1 An index of an OUTPUT pin.
 * @param f2   A MSFilter object containing the INPUT pin
 * @param pin2 An index of an INPUT pin.
 *
 * Returns: 0 if sucessful, -1 otherwise.
 */
MS2_PUBLIC int ms_filter_unlink(MSFilter *f1, int pin1, MSFilter *f2, int pin2);

/**
 * Call a filter's method to set or get options.
 *
 * @param f    A MSFilter object.
 * @param id   A private filter ID for the option.
 * @param arg  A private user data for the filter.
 *
 * Returns: 0 if successfull, -1 otherwise.
 */
MS2_PUBLIC int ms_filter_call_method(MSFilter *f, unsigned int id, void *arg);

/**
 * Call a filter's method to set options.
 *
 * @param f    A MSFilter object.
 * @param id   A method ID.
 *
 * Returns: 0 if successfull, -1 otherwise.
 */
MS2_PUBLIC int ms_filter_call_method_noarg(MSFilter *f, unsigned int id);


/**
 * Returns whether the filter implements a given method
 *
 * @param f    A MSFilter object.
 * @param id   A method ID.
 *
 * Returns: TRUE if method is implemented, FALSE otherwise.
 */
MS2_PUBLIC bool_t ms_filter_has_method(MSFilter *f, unsigned int id);

/**
 * Returns whether a filter implements a given interface.
 * @param f a MSFilter object
 * @param id an interface id.
 * 
 * Returns TRUE if interface is implemented, FALSE, otherwise.
**/
MS2_PUBLIC bool_t ms_filter_implements_interface(MSFilter *f, MSFilterInterfaceId id);

/**
 * Returns whether a filter implements a given interface, based on the filter's descriptor.
 * @param f a MSFilter object
 * @param id an interface id.
 * 
 * Returns TRUE if interface is implemented, FALSE, otherwise.
**/
MS2_PUBLIC bool_t ms_filter_desc_implements_interface(MSFilterDesc *desc, MSFilterInterfaceId id);

/**
 * Set a callback on filter's to be informed of private filter's event.
 * This callback is called from the filter's MSTicker, unless a global event queue
 * is created to receive all filter's notification asynchronously.
 * See ms_event_queue_new() for details.
 *
 * @param f        A MSFilter object.
 * @param fn       A MSFilterNotifyFunc that will be called.
 * @param userdata A pointer to private data.
 * @deprecated use ms_filter_add_notify_callback()
 *
 */


/**
 * Set a callback on filter's to be informed of private filter's event.
 * This callback is called from the filter's MSTicker, unless a global event queue
 * is created to receive all filter's notification or synchronous flag is TRUE.
 * See ms_event_queue_new() for details.
 *
 * @param f        A MSFilter object.
 * @param fn       A MSFilterNotifyFunc that will be called.
 * @param userdata A pointer to private data.
 * @param synchronous boolean that indicates whether this callback must be called synchronously.
 *
 */
MS2_PUBLIC void ms_filter_add_notify_callback(MSFilter *f, MSFilterNotifyFunc fn, void *userdata, bool_t synchronous);

/**
 * Remove a notify callback previously entered with ms_filter_add_notify_callback()
 *
 * @param f        A MSFilter object.
 * @param fn       A MSFilterNotifyFunc that will be called.
 * @param userdata A pointer to private data.
 *
 */
MS2_PUBLIC void ms_filter_remove_notify_callback(MSFilter *f, MSFilterNotifyFunc fn, void *userdata);

/**
 * Get MSFilterId's filter.
 *
 * @param f        A MSFilter object.
 *
 * Returns: MSFilterId if successfull, -1 otherwise.
 */
MS2_PUBLIC MSFilterId ms_filter_get_id(MSFilter *f);

/**
 * Get filter's name.
 * @param[in] f #MSFilter object
 * @return The name of the filter.
 */
MS2_PUBLIC const char * ms_filter_get_name(MSFilter *f);


/**
 * Obtain the list of current filter's neighbours, ie filters that are part of same graph.
 *
 * Returns: a MSList of MSFilter, that needs to be freed by the caller when no more needed.
**/
MS2_PUBLIC MSList * ms_filter_find_neighbours(MSFilter *me);

/**
 * Destroy a filter object.
 *
 * @param f        A MSFilter object.
 *
 */
MS2_PUBLIC void ms_filter_destroy(MSFilter *f);

/**
 * Initialize a MSConnectionHelper.
 *
 * @param h A MSConnectionHelper, usually (but not necessarily) on stack
 *
**/
MS2_PUBLIC void ms_connection_helper_start(MSConnectionHelper *h);

/**
 * \brief Enter a MSFilter to be connected into the MSConnectionHelper object.
 *
 * This functions enters a MSFilter to be connected into the MSConnectionHelper
 * object and connects it to the last entered if not the first one.
 * The MSConnectionHelper is useful to reduce the amount of code necessary to create graphs in case
 * the connections are made in an ordered manner and some filters are present conditionally in graphs.
 * For example, instead of writing
 * \code
 * ms_filter_link(f1,0,f2,1);
 * ms_filter_link(f2,0,f3,0);
 * ms_filter_link(f3,1,f4,0);
 * \endcode
 * You can write:
 * \code
 * MSConnectionHelper h;
 * ms_connection_helper_start(&h);
 * ms_connection_helper_link(&h,f1,-1,0);
 * ms_connection_helper_link(&h,f2,1,0);
 * ms_connection_helper_link(&h,f3,0,1);
 * ms_connection_helper_link(&h,f4,0,-1);
 * \endcode
 * Which is a bit longer to write here, but now imagine f2 needs to be present in the graph only
 * in certain conditions: in the first case you have rewrite the two first lines, in the second case
 * you just need to replace the fourth line by:
 * \code
 * if (my_condition) ms_connection_helper_link(&h,f2,1,0);
 * \endcode
 *
 * @param h a connection helper
 * @param f a MSFilter
 * @param inpin an input pin number with which the MSFilter needs to connect to previously entered MSFilter
 * @param outpin an output pin number with which the MSFilter needs to be connected to the next entered MSFilter
 *
 * Returns: the return value of ms_filter_link() that is called internally to this function.
**/
MS2_PUBLIC int ms_connection_helper_link(MSConnectionHelper *h, MSFilter *f, int inpin, int outpin);


/**
 * \brief Enter a MSFilter to be disconnected into the MSConnectionHelper object.
 * Process exactly the same way as ms_connection_helper_link() but calls ms_filter_unlink() on the
 * entered filters.
**/
MS2_PUBLIC int ms_connection_helper_unlink(MSConnectionHelper *h, MSFilter *f, int inpin, int outpin);


/**
 * \brief Enable processing time measurements statistics for filters.
 *
**/
MS2_PUBLIC MS2_DEPRECATED void ms_filter_enable_statistics(bool_t enabled);


/**
 * \brief Reset processing time statistics for filters.
 *
**/
MS2_PUBLIC MS2_DEPRECATED void ms_filter_reset_statistics(void);

/**
 * \brief Retrieves statistics for running filters.
 * Returns a list of MSFilterStats
**/
MS2_PUBLIC MS2_DEPRECATED const MSList * ms_filter_get_statistics(void);

/**
 * \brief Logs runtime statistics for running filters.
 *
**/
MS2_PUBLIC MS2_DEPRECATED void ms_filter_log_statistics(void);




/* I define the id taking the lower bits of the address of the MSFilterDesc object,
the method index (_cnt_) and the argument size */
/* I hope using this to avoid type mismatch (calling a method on the wrong filter)*/
#define MS_FILTER_METHOD_ID(_id_,_cnt_,_argsize_) \
	(  (((unsigned long)(_id_)) & 0xFFFF)<<16 | (_cnt_<<8) | (_argsize_ & 0xFF ))

/**
 * Macro to create a method id, unique per filter.
 * First argument shall be the filter's ID (MSFilterId) or interface ID (MSFilterInterfaceId).
 * Second argument is the method index within the context of the filter. It should start from 0 and increment for each new method.
 * Third argument is the argument type of the method, for example "int", "float" or any structure.
**/
#define MS_FILTER_METHOD(_id_,_count_,_argtype_) \
	MS_FILTER_METHOD_ID(_id_,_count_,sizeof(_argtype_))

/**
 * Same as MS_FILTER_METHOD, but for method that do not take any argument.
**/
#define MS_FILTER_METHOD_NO_ARG(_id_,_count_) \
	MS_FILTER_METHOD_ID(_id_,_count_,0)


#define MS_FILTER_BASE_METHOD(_count_,_argtype_) \
	MS_FILTER_METHOD_ID(MS_FILTER_BASE_ID,_count_,sizeof(_argtype_))

#define MS_FILTER_BASE_METHOD_NO_ARG(_count_) \
	MS_FILTER_METHOD_ID(MS_FILTER_BASE_ID,_count_,0)

#define MS_FILTER_EVENT(_id_,_count_,_argtype_) \
	MS_FILTER_METHOD_ID(_id_,_count_,sizeof(_argtype_))

#define MS_FILTER_EVENT_NO_ARG(_id_,_count_)\
	MS_FILTER_METHOD_ID(_id_,_count_,0)

	
#define MS_FILTER_BASE_EVENT(_count_,_argtype_) \
	MS_FILTER_EVENT(MS_FILTER_BASE_ID,_count_,_argtype_)

#define MS_FILTER_BASE_EVENT_NO_ARG(_count_) \
	MS_FILTER_EVENT_NO_ARG(MS_FILTER_BASE_ID,_count_)
	
/**
 *  some MSFilter base generic methods:
 **/
/**
 * Set filter output/input sampling frequency in hertz
 */
#define MS_FILTER_SET_SAMPLE_RATE	MS_FILTER_BASE_METHOD(0,int)
/**
 * Get filter output/input sampling frequency in hertz
 */

#define MS_FILTER_GET_SAMPLE_RATE	MS_FILTER_BASE_METHOD(1,int)
/**
 * Set filter output network bitrate in bit per seconds, this value include IP+UDP+RTP overhead
 */
#define MS_FILTER_SET_BITRATE		MS_FILTER_BASE_METHOD(2,int)
/**
 * Get filter output network bitrate in bit per seconds, this value include IP+UDP+RTP overhead
 */
#define MS_FILTER_GET_BITRATE		MS_FILTER_BASE_METHOD(3,int)
#define MS_FILTER_GET_NCHANNELS		MS_FILTER_BASE_METHOD(5,int)
#define MS_FILTER_SET_NCHANNELS		MS_FILTER_BASE_METHOD(6,int)
/**
 * Set codec dependent attributes as taken from the SDP
 */
#define MS_FILTER_ADD_FMTP		MS_FILTER_BASE_METHOD(7,const char)

#define MS_FILTER_ADD_ATTR		MS_FILTER_BASE_METHOD(8,const char)
#define MS_FILTER_SET_MTU		MS_FILTER_BASE_METHOD(9,int)
#define MS_FILTER_GET_MTU		MS_FILTER_BASE_METHOD(10,int)
/**Filters can return their latency in milliseconds (if known) using this method:*/
#define MS_FILTER_GET_LATENCY	MS_FILTER_BASE_METHOD(11,int)

typedef struct _MSPinFormat{
	uint16_t pin;
	const MSFmtDescriptor *fmt;
}MSPinFormat;

/**
 * Obtain the format of a filter on a given input
 */
#define MS_FILTER_GET_INPUT_FMT MS_FILTER_BASE_METHOD(30,MSPinFormat)
/**
 * Set the format of a filter on a given input
 */
#define MS_FILTER_SET_INPUT_FMT MS_FILTER_BASE_METHOD(31,MSPinFormat)
/**
 * Obtain the format of a filter on a given output
 */
#define MS_FILTER_GET_OUTPUT_FMT MS_FILTER_BASE_METHOD(32,MSPinFormat)
/**
 * Set the format of a filter on a given output
 */
#define MS_FILTER_SET_OUTPUT_FMT MS_FILTER_BASE_METHOD(33,MSPinFormat)


/**
 * MSFilter generic events
**/
#define MS_FILTER_OUTPUT_FMT_CHANGED MS_FILTER_BASE_EVENT_NO_ARG(0) /**<triggered whenever a filter decides to change its output format for one or more more output pins*/


/* DEPRECATED  specific methods: to be moved into implementation specific header files - DO NOT USE IN NEW CODE*/
#define MS_FILTER_SET_FILTERLENGTH 	MS_FILTER_BASE_METHOD(12,int)
#define MS_FILTER_SET_OUTPUT_SAMPLE_RATE MS_FILTER_BASE_METHOD(13,int)
#define MS_FILTER_ENABLE_DIRECTMODE	MS_FILTER_BASE_METHOD(14,int)
#define MS_FILTER_ENABLE_VAD		MS_FILTER_BASE_METHOD(15,int)
#define MS_FILTER_GET_STAT_DISCARDED	MS_FILTER_BASE_METHOD(16,int)
#define MS_FILTER_GET_STAT_MISSED	MS_FILTER_BASE_METHOD(17,int)
#define MS_FILTER_GET_STAT_INPUT	MS_FILTER_BASE_METHOD(18,int)
#define MS_FILTER_GET_STAT_OUTPUT	MS_FILTER_BASE_METHOD(19,int)
#define MS_FILTER_ENABLE_AGC 		MS_FILTER_BASE_METHOD(20,int)
#define MS_FILTER_SET_PLAYBACKDELAY MS_FILTER_BASE_METHOD(21,int)
#define MS_FILTER_ENABLE_HALFDUPLEX MS_FILTER_BASE_METHOD(22,int)
#define MS_FILTER_SET_VAD_PROB_START MS_FILTER_BASE_METHOD(23,int)
#define MS_FILTER_SET_VAD_PROB_CONTINUE MS_FILTER_BASE_METHOD(24,int)
#define MS_FILTER_SET_MAX_GAIN  MS_FILTER_BASE_METHOD(25,int)
#define MS_VIDEO_CAPTURE_SET_AUTOFOCUS MS_FILTER_BASE_METHOD(26,int)
/* pass value of type MSRtpPayloadPickerContext copied by the filter*/
#define MS_FILTER_SET_RTP_PAYLOAD_PICKER MS_FILTER_BASE_METHOD(27,void*)
#define MS_FILTER_SET_OUTPUT_NCHANNELS	MS_FILTER_BASE_METHOD(28,int)


/** @} */

/*protected/ private methods*/
MS2_PUBLIC void ms_filter_process(MSFilter *f);
MS2_PUBLIC void ms_filter_preprocess(MSFilter *f, struct _MSTicker *t);
MS2_PUBLIC void ms_filter_postprocess(MSFilter *f);
MS2_PUBLIC bool_t ms_filter_inputs_have_data(MSFilter *f);
MS2_PUBLIC void ms_filter_notify(MSFilter *f, unsigned int id, void *arg);
MS2_PUBLIC void ms_filter_notify_no_arg(MSFilter *f, unsigned int id);
void ms_filter_clear_notify_callback(MSFilter *f);
void ms_filter_clean_pending_events(MSFilter *f);
#define ms_filter_lock(f)	ms_mutex_lock(&(f)->lock)
#define ms_filter_unlock(f)	ms_mutex_unlock(&(f)->lock)
MS2_PUBLIC void ms_filter_unregister_all(void);

struct _MSFilterTask{
	MSFilter *f;
	MSFilterFunc taskfunc;
};
typedef struct _MSFilterTask MSFilterTask;
MS2_PUBLIC void ms_filter_task_process(MSFilterTask *task);

/**
 * Allow a filter to request the ticker to call him the tick after.
 * The ticker will call the taskfunc prior to all filter's process func.
**/
MS2_PUBLIC void ms_filter_postpone_task(MSFilter *f, MSFilterFunc taskfunc);

#ifdef __cplusplus
}
#endif

#include "mediastreamer2/msinterfaces.h"
#include "mediastreamer2/msfactory.h"
/* used by awk script in Makefile.am to generate alldescs.c */
#define MS_FILTER_DESC_EXPORT(desc)

#endif

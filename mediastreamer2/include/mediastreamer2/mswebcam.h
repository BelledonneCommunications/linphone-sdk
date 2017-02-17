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

#ifndef webcam_h
#define webcam_h

#include <mediastreamer2/mscommon.h>
#include <mediastreamer2/msfactory.h>

/**
 * @file mswebcam.h
 * @brief mediastreamer2 mswebcam.h include file
 *
 * This file provide the API needed to manage
 * soundcard filters.
 *
 */

/**
 * @defgroup mediastreamer2_webcam Camera API - manage video capture devices
 * @ingroup mediastreamer2_api
 * @{
 */

struct _MSWebCamManager{
	MSFactory* factory;
	MSList *cams;
	MSList *descs;
};

/**
 * Structure for webcam manager object.
 * @var MSWebCamManager
 */
typedef struct _MSWebCamManager MSWebCamManager;


struct _MSWebCam;

typedef void (*MSWebCamDetectFunc)(MSWebCamManager *obj);
typedef void (*MSWebCamInitFunc)(struct _MSWebCam *obj);
typedef void (*MSWebCamUninitFunc)(struct _MSWebCam *obj);
typedef struct _MSFilter * (*MSWebCamCreateReaderFunc)(struct _MSWebCam *obj);
typedef bool_t (*MSWebCamEncodeToMimeType)(struct _MSWebCam *obj, const char *mime_type);

struct _MSWebCamDesc{
	const char *driver_type;
	MSWebCamDetectFunc detect;
	MSWebCamInitFunc init;
	MSWebCamCreateReaderFunc create_reader;
	MSWebCamUninitFunc uninit;
	MSWebCamEncodeToMimeType encode_to_mime_type;
};

/**
 * Structure for sound card description object.
 * @var MSWebCamDesc
 */
typedef struct _MSWebCamDesc MSWebCamDesc;

struct _MSWebCam{
	MSWebCamManager* wbcmanager;
	MSWebCamDesc *desc;
	char *name;
	char *id;
	void *data;
};

/**
 * Structure for sound card object.
 * @var MSWebCam
 */
typedef struct _MSWebCam MSWebCam;

#ifdef __cplusplus
extern "C"{
#endif

/**
 * Retrieve a webcam manager object.
 *
 * @returns: MSWebCamManager if successfull, NULL otherwise.
 * @deprecated use ms_factory_get_web_cam_manager().
 */
MS2_PUBLIC MS2_DEPRECATED MSWebCamManager * ms_web_cam_manager_get(void);

/**
 * Returns the factory from the webcam object.
 * @param c MSWebCam used to get to the factory.
 */
MS2_PUBLIC MSFactory * ms_web_cam_get_factory(MSWebCam *c);

/**
 * Create a webcam manager object.
 * You usually do not need this function, instead get the webcam manager from a factory
 * with ms_factory_get_web_cam_manager().
 */
MS2_PUBLIC MSWebCamManager * ms_web_cam_manager_new(void);

/**
 * Destroy the webcam manager object.
 * You usually don't need this function, ms_factory_destroy() doing the job for you.
 */
MS2_PUBLIC void ms_web_cam_manager_destroy(MSWebCamManager* scm);

/**
 * Retreive a webcam object based on its name.
 *
 * @param m    A webcam manager containing webcam.
 * @param id   A name for card to search.
 *
 * Returns: MSWebCam if successfull, NULL otherwise.
 */
MS2_PUBLIC MSWebCam * ms_web_cam_manager_get_cam(MSWebCamManager *m, const char *id);

/**
 * Retreive the default webcam object.
 *
 * @param m    A webcam manager containing webcams.
 *
 * Returns: MSWebCam if successfull, NULL otherwise.
 */
MS2_PUBLIC MSWebCam * ms_web_cam_manager_get_default_cam(MSWebCamManager *m);

/**
 * Retreive the list of webcam objects.
 *
 * @param m    A webcam manager containing webcams.
 *
 * Returns: MSList of cards if successfull, NULL otherwise.
 */
MS2_PUBLIC const MSList * ms_web_cam_manager_get_list(MSWebCamManager *m);

/**
 * Add a webcam object in a webcam  manager's list.
 *
 * @param m    A webcam  manager containing webcams
 * @param c    A web cam object.
 *
 */
MS2_PUBLIC void ms_web_cam_manager_add_cam(MSWebCamManager *m, MSWebCam *c);

MS2_PUBLIC MSWebCam * ms_web_cam_manager_create_cam(MSWebCamManager *m, MSWebCamDesc *desc);

MS2_PUBLIC void ms_web_cam_set_manager(MSWebCamManager*m, MSWebCam *c);
	
/**
 * Add a webcam object on top of list of the webcam  manager's list.
 *
 * @param m    A webcam  manager containing webcams
 * @param c    A web cam object.
 *
 */
MS2_PUBLIC void ms_web_cam_manager_prepend_cam(MSWebCamManager *m, MSWebCam *c);


/**
 * Register a webcam descriptor in a webcam manager.
 *
 * @param m      A webcam manager containing sound cards.
 * @param desc   A webcam descriptor object.
 *
 */
MS2_PUBLIC void ms_web_cam_manager_register_desc(MSWebCamManager *m, MSWebCamDesc *desc);


/**
 * Ask all registered MSWebCamDesc to detect the webcams again.
 *
 * @param m A webcam manager
**/
MS2_PUBLIC void ms_web_cam_manager_reload(MSWebCamManager *m);

/**
 * Create an INPUT filter based on the selected camera.
 *
 * @param obj      A webcam object.
 *
 * Returns: A MSFilter if successfull, NULL otherwise.
 */
MS2_PUBLIC struct _MSFilter * ms_web_cam_create_reader(MSWebCam *obj);

/**
 * Create a new webcam object.
 *
 * @param desc   A webcam description object.
 *
 * Returns: MSWebCam if successfull, NULL otherwise.
 */
MS2_PUBLIC MSWebCam * ms_web_cam_new(MSWebCamDesc *desc);

/**
 * Destroy webcam object.
 *
 * @param obj   A MSWebCam object.
 */
MS2_PUBLIC void ms_web_cam_destroy(MSWebCam *obj);


/**
 * Retreive a webcam's driver type string.
 *
 * Internal driver types are either: "V4L V4LV2"
 *
 * @param obj   A webcam object.
 *
 * Returns: a string if successfull, NULL otherwise.
 */
MS2_PUBLIC const char *ms_web_cam_get_driver_type(const MSWebCam *obj);

/**
 * Retreive a webcam's name.
 *
 * @param obj   A webcam object.
 *
 * Returns: a string if successfull, NULL otherwise.
 */
MS2_PUBLIC const char *ms_web_cam_get_name(const MSWebCam *obj);

/**
 * Retreive webcam's id: ($driver_type: $name).
 *
 * @param obj    A webcam object.
 *
 * Returns: A string if successfull, NULL otherwise.
 */
MS2_PUBLIC const char *ms_web_cam_get_string_id(MSWebCam *obj);


/*specific methods for static image:*/

MS2_PUBLIC void ms_static_image_set_default_image(const char *path);
MS2_PUBLIC const char *ms_static_image_get_default_image(void);

/** method for the "nowebcam" filter */
#define MS_STATIC_IMAGE_SET_IMAGE \
	MS_FILTER_METHOD(MS_STATIC_IMAGE_ID,0,const char)

#ifdef __cplusplus
}
#endif

/** @} */

#endif

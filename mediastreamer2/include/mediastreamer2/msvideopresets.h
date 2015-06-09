/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2015  Belledonne Communications SARL

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

#ifndef MSVIDEOPRESETS_H
#define MSVIDEOPRESETS_H

#include <mediastreamer2/mscommon.h>
#include <mediastreamer2/msvideo.h>

/**
 * @file msvideopresets.h
 * @brief mediastreamer2 msvideopresets.h include file
 *
 * This file provide the API needed to manage video presets.
 *
 */

/**
 * @ingroup mediastreamer2_api
 * @{
 */

/**
 * Structure for video presets manager object.
 */
typedef struct _MSVideoPresetsManager MSVideoPresetsManager;


#ifdef __cplusplus
extern "C"{
#endif

/**
 * Retreive a video presets manager object.
 * @return A MSVideoPresetsManager object if successfull, NULL otherwise.
 */
MS2_PUBLIC MSVideoPresetsManager * ms_video_presets_manager_get(void);

/**
 * Destroy the video presets manager object.
 */
MS2_PUBLIC void ms_video_presets_manager_destroy(void);

/**
 * Register a video preset configuration.
 * @param[in] manager The MSVideoPresetsManager object.
 * @param[in] name The name of the video preset to register.
 * @param[in] tags A comma-separated list of tags describing the video preset.
 * @param[in] config The MSVideoConfiguration that is to be registered in the specified preset with the specified tags.
 */
MS2_PUBLIC void ms_video_presets_manager_register_preset_configuration(MSVideoPresetsManager *manager,
	const char *name, const char *tags, MSVideoConfiguration *config);

#ifdef __cplusplus
}
#endif

/** @} */

#endif

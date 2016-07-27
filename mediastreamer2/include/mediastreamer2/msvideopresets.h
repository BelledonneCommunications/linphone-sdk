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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef MSVIDEOPRESETS_H
#define MSVIDEOPRESETS_H

#include <mediastreamer2/mscommon.h>
#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/msfactory.h>
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

/**
 * Structure for video preset configuration object.
 */
typedef struct _MSVideoPresetConfiguration MSVideoPresetConfiguration;


#ifdef __cplusplus
extern "C"{
#endif

/**
 * Create a video presets manager object.
 * @param[in] The MSFactory to add the new video presets manager to.
 * @return The new MSVideoPresetsManager object.
 */
MS2_PUBLIC MSVideoPresetsManager * ms_video_presets_manager_new(MSFactory *factory);

/**
 * Destroy the video presets manager object.
 * @param[in] manager The MSVideoPresetsManager to destroy.
 */
MS2_PUBLIC void ms_video_presets_manager_destroy(MSVideoPresetsManager *manager);

/**
 * Register a video preset configuration.
 * @param[in] manager The MSVideoPresetsManager object.
 * @param[in] name The name of the video preset to register.
 * @param[in] tags A comma-separated list of tags describing the video preset.
 * @param[in] config The MSVideoConfiguration that is to be registered in the specified preset with the specified tags.
 */
MS2_PUBLIC void ms_video_presets_manager_register_preset_configuration(MSVideoPresetsManager *manager,
	const char *name, const char *tags, MSVideoConfiguration *config);

/**
 * Search for a video preset configuration.
 * @param[in] manager The MSVideoPresetsManager object.
 * @param[in] name The name of the video preset to search for.
 * @param[in] codecs_tags A list of tags describing the codec that will be used to select the video configuration to return.
 * @return The MSVideoConfiguration corresponding to the video preset being searched for and matching the codec_tags and
 * the platform tags.
 */
MS2_PUBLIC MSVideoPresetConfiguration * ms_video_presets_manager_find_preset_configuration(MSVideoPresetsManager *manager,
	const char *name, MSList *codec_tags);

/**
 * Get the video configuration corresponding to a video preset configuration.
 * @param[in] vpc MSVideoPresetConfiguration object obtained with ms_video_presets_manager_find_preset_configuration()
 * @return The MSVideoConfiguration corresponding to the video preset configuration.
 */
MS2_PUBLIC MSVideoConfiguration * ms_video_preset_configuration_get_video_configuration(MSVideoPresetConfiguration *vpc);

/**
 * Get the tags corresponding to a video preset configuration.
 * @param[in] vpc MSVideoPresetConfiguration object obtained with ms_video_presets_manager_find_preset_configuration()
 * @return A comma-separated list of tags describing the video preset configuration.
 */
MS2_PUBLIC char * ms_video_preset_configuration_get_tags_as_string(MSVideoPresetConfiguration *vpc);

#ifdef __cplusplus
}
#endif

/** @} */

#endif

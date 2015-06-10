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

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/msvideopresets.h"


typedef struct _MSVideoPresetConfiguration {
	MSList *tags;
	MSVideoConfiguration *config;
} MSVideoPresetConfiguration;

typedef struct _MSVideoPreset {
	char *name;
	MSList *configs; /**< List of MSVideoPresetConfiguration objects */
} MSVideoPreset;

struct _MSVideoPresetsManager {
	MSList *presets; /**< List of MSVideoPreset objects */
};


static MSVideoPresetsManager *vpm = NULL;

static void free_preset_config(MSVideoPresetConfiguration *vpc) {
	ms_list_for_each(vpc->tags, ms_free);
}

static void free_preset(MSVideoPreset *vp) {
	ms_free(vp->name);
	ms_list_for_each(vp->configs, (MSIterateFunc)free_preset_config);
}

static MSVideoPreset * add_video_preset(MSVideoPresetsManager *manager, const char *name) {
	MSVideoPreset *vp = ms_new0(MSVideoPreset, 1);
	vp->name = ms_strdup(name);
	manager->presets = ms_list_append(manager->presets, vp);
	return vp;
}

static MSVideoPreset * find_video_preset(MSVideoPresetsManager *manager, const char *name) {
	MSList *elem = manager->presets;
	while (elem != NULL) {
		MSVideoPreset *vp = (MSVideoPreset *)elem->data;
		if (strcmp(name, vp->name) == 0) {
			return vp;
		}
		elem = elem->next;
	}
	return NULL;
}

static MSList * parse_tags(const char *tags) {
	MSList *tags_list = NULL;
	char *t = ms_strdup(tags);
	char *p = t;
	while (p != NULL) {
		char *next = strstr(p, ",");
		if (next != NULL) {
			*(next++) = '\0';
		}
		tags_list = ms_list_append(tags_list, ms_strdup(p));
		p = next;
	}
	ms_free(t);
	return tags_list;
}

static void add_video_preset_configuration(MSVideoPreset *preset, const char *tags, MSVideoConfiguration *config) {
	MSVideoPresetConfiguration *vpc = ms_new0(MSVideoPresetConfiguration, 1);
	vpc->tags = parse_tags(tags);
	vpc->config = config;
	preset->configs = ms_list_append(preset->configs, vpc);
}

void ms_video_presets_manager_destroy(void) {
	if (vpm != NULL) {
		ms_list_for_each(vpm->presets, (MSIterateFunc)free_preset);
		ms_list_free(vpm->presets);
		ms_free(vpm);
	}
	vpm = NULL;
}

MSVideoPresetsManager * ms_video_presets_manager_get(void) {
	if (vpm == NULL) vpm = (MSVideoPresetsManager *)ms_new0(MSVideoPresetsManager, 1);
	return vpm;
}

void ms_video_presets_manager_register_preset_configuration(MSVideoPresetsManager *manager,
	const char *name, const char *tags, MSVideoConfiguration *config) {
	MSVideoPreset *preset = find_video_preset(manager, name);
	if (preset == NULL) {
		preset = add_video_preset(manager, name);
	}
	add_video_preset_configuration(preset, tags, config);
}

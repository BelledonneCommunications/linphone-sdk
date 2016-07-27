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

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/msvideopresets.h"


struct _MSVideoPresetConfiguration {
	bctbx_list_t *tags;
	MSVideoConfiguration *config;
};

typedef struct _MSVideoPreset {
	char *name;
	bctbx_list_t *configs; /**< List of MSVideoPresetConfiguration objects */
} MSVideoPreset;

struct _MSVideoPresetsManager {
	MSFactory *factory;
	bctbx_list_t *presets; /**< List of MSVideoPreset objects */
};


static void free_preset_config(MSVideoPresetConfiguration *vpc) {
	bctbx_list_for_each(vpc->tags, ms_free);
	bctbx_list_free(vpc->tags);
	ms_free(vpc);
}

static void free_preset(MSVideoPreset *vp) {
	ms_free(vp->name);
	bctbx_list_for_each(vp->configs, (MSIterateFunc)free_preset_config);
	bctbx_list_free(vp->configs);
	ms_free(vp);
}

static MSVideoPreset * add_video_preset(MSVideoPresetsManager *manager, const char *name) {
	MSVideoPreset *vp = ms_new0(MSVideoPreset, 1);
	vp->name = ms_strdup(name);
	manager->presets = bctbx_list_append(manager->presets, vp);
	return vp;
}

static MSVideoPreset * find_video_preset(MSVideoPresetsManager *manager, const char *name) {
	bctbx_list_t *elem = manager->presets;
	while (elem != NULL) {
		MSVideoPreset *vp = (MSVideoPreset *)elem->data;
		if (strcmp(name, vp->name) == 0) {
			return vp;
		}
		elem = elem->next;
	}
	return NULL;
}

static bctbx_list_t * parse_tags(const char *tags) {
	bctbx_list_t *tags_list = NULL;
	char *t;
	char *p;
	if ((tags == NULL) || (tags[0] == '\0')) return NULL;
	p = t = ms_strdup(tags);
	while (p != NULL) {
		char *next = strstr(p, ",");
		if (next != NULL) {
			*(next++) = '\0';
		}
		tags_list = bctbx_list_append(tags_list, ms_strdup(p));
		p = next;
	}
	ms_free(t);
	return tags_list;
}

static void add_video_preset_configuration(MSVideoPreset *preset, const char *tags, MSVideoConfiguration *config) {
	MSVideoPresetConfiguration *vpc = ms_new0(MSVideoPresetConfiguration, 1);
	vpc->tags = parse_tags(tags);
	vpc->config = config;
	preset->configs = bctbx_list_append(preset->configs, vpc);
}

static int video_preset_configuration_match(MSVideoPresetConfiguration *vpc, bctbx_list_t *platform_tags, bctbx_list_t *codec_tags) {
	bctbx_list_t *elem = vpc->tags;
	int nb = 0;
	while (elem != NULL) {
		char *tag = (char *)elem->data;
		if (!ms_tags_list_contains_tag(platform_tags, tag) && !ms_tags_list_contains_tag(codec_tags, tag))
			return 0;
		nb++;
		elem = elem->next;
	}
	return nb;
}

void ms_video_presets_manager_destroy(MSVideoPresetsManager *manager) {
	if (manager != NULL) {
		bctbx_list_for_each(manager->presets, (MSIterateFunc)free_preset);
		bctbx_list_free(manager->presets);
		ms_free(manager);
	}
}

MSVideoPresetsManager * ms_video_presets_manager_new(MSFactory *factory) {
	MSVideoPresetsManager *manager = (MSVideoPresetsManager *)ms_new0(MSVideoPresetsManager, 1);
	manager->factory = factory;
	if (factory->video_presets_manager != NULL) {
		ms_video_presets_manager_destroy(factory->video_presets_manager);
	}
	factory->video_presets_manager = manager;
	return manager;
}

void ms_video_presets_manager_register_preset_configuration(MSVideoPresetsManager *manager,
	const char *name, const char *tags, MSVideoConfiguration *config) {
	MSVideoPreset *preset = find_video_preset(manager, name);
	if (preset == NULL) {
		preset = add_video_preset(manager, name);
	}
	add_video_preset_configuration(preset, tags, config);
}

MSVideoPresetConfiguration * ms_video_presets_manager_find_preset_configuration(MSVideoPresetsManager *manager,
	const char *name, bctbx_list_t *codec_tags) {
	bctbx_list_t *elem = NULL;
	MSVideoPreset *preset = find_video_preset(manager, name);
	MSVideoPresetConfiguration *best_vpc = NULL;
	int best_nb = -1;

	if (preset == NULL) return NULL;
	elem = preset->configs;
	while (elem != NULL) {
		MSVideoPresetConfiguration *vpc = (MSVideoPresetConfiguration *)elem->data;
		int nb = video_preset_configuration_match(vpc, ms_factory_get_platform_tags(manager->factory), codec_tags);
		if (nb > best_nb) {
			best_vpc = vpc;
			best_nb = nb;
		}
		elem = elem->next;
	}
	return best_vpc;
}

MSVideoConfiguration * ms_video_preset_configuration_get_video_configuration(MSVideoPresetConfiguration *vpc) {
	return vpc->config;
}

char * ms_video_preset_configuration_get_tags_as_string(MSVideoPresetConfiguration *vpc) {
	return ms_tags_list_as_string(vpc->tags);
}

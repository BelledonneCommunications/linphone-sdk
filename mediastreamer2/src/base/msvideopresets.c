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


struct _MSVideoPresetsManager {
	MSList *presets;
};


static MSVideoPresetsManager *vpm = NULL;

void ms_video_presets_manager_destroy(void) {
	if (vpm != NULL){
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
	// TODO
}

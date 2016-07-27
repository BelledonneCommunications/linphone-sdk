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


static MSVideoConfiguration custom_conf_list[] = {
	MS_VIDEO_CONF(0, 10000000, HDTVP, 30, 1)
};


static MSVideoConfiguration high_fps_desktop_vp8_conf_list[] = {
	MS_VIDEO_CONF(2000000,  3000000,  720P, 30, 4),
	MS_VIDEO_CONF(1500000,  2000000,   XGA, 30, 4),
	MS_VIDEO_CONF(1000000,  1500000,  SVGA, 30, 4),
	MS_VIDEO_CONF( 600000,  1000000,   VGA, 30, 2),
	MS_VIDEO_CONF( 350000,   600000,   CIF, 30, 2),
	MS_VIDEO_CONF( 280000,   350000,  QVGA, 30, 1),
	MS_VIDEO_CONF( 150000,   280000,  QCIF, 30, 1),
	MS_VIDEO_CONF( 120000,   150000,  QCIF, 20, 1),
	MS_VIDEO_CONF(      0,   120000,  QCIF, 10 ,1)
};

static MSVideoConfiguration high_fps_embedded_vp8_conf_list[] = {
	MS_VIDEO_CONF(600000, 3000000,  VGA, 30, 2),
	MS_VIDEO_CONF(350000,  600000,  CIF, 30, 2),
	MS_VIDEO_CONF(280000,  350000, QVGA, 30, 2),
	MS_VIDEO_CONF(150000,  280000, QCIF, 30, 1),
	MS_VIDEO_CONF(120000,  150000, QCIF, 20, 1),
	MS_VIDEO_CONF(     0,  120000, QCIF, 10, 1)
};

static MSVideoConfiguration high_fps_desktop_h264_conf_list[] = {
	MS_VIDEO_CONF(2000000, 3000000, 720P, 30, 4),
	MS_VIDEO_CONF(1500000, 2000000,  XGA, 30, 4),
	MS_VIDEO_CONF(1000000, 1500000, SVGA, 30, 4),
	MS_VIDEO_CONF( 600000, 1000000,  VGA, 30, 2),
	MS_VIDEO_CONF( 400000,  600000,  CIF, 30, 2),
	MS_VIDEO_CONF( 350000,  400000, QVGA, 30, 2),
	MS_VIDEO_CONF( 200000,  350000, QCIF, 30, 1),
	MS_VIDEO_CONF( 150000,  200000, QCIF, 15, 1),
	MS_VIDEO_CONF(      0,  150000, QCIF, 10, 1)
};

static MSVideoConfiguration high_fps_embedded_h264_conf_list[] = {
	MS_VIDEO_CONF(600000, 3000000,  VGA, 30, 2),
	MS_VIDEO_CONF(400000,  600000,  CIF, 30, 2),
	MS_VIDEO_CONF(350000,  400000, QVGA, 30, 2),
	MS_VIDEO_CONF(200000,  350000, QCIF, 30, 1),
	MS_VIDEO_CONF(150000,  200000, QCIF, 15, 1),
	MS_VIDEO_CONF(     0,  150000, QCIF, 10, 1)
};


void register_video_preset_high_fps(MSVideoPresetsManager *manager) {
	ms_video_presets_manager_register_preset_configuration(manager, "custom", NULL, custom_conf_list);
	ms_video_presets_manager_register_preset_configuration(manager, "high-fps", "desktop,vp8", high_fps_desktop_vp8_conf_list);
	ms_video_presets_manager_register_preset_configuration(manager, "high-fps", "embedded,vp8", high_fps_embedded_vp8_conf_list);
	ms_video_presets_manager_register_preset_configuration(manager, "high-fps", "desktop,h264", high_fps_desktop_h264_conf_list);
	ms_video_presets_manager_register_preset_configuration(manager, "high-fps", "embedded,h264", high_fps_embedded_h264_conf_list);
}

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



static MSVideoConfiguration high_fps_desktop_vp8_conf_list[] = {
	MS_VIDEO_CONF(1536000,  2560000, SXGA_MINUS, 25, 4),
	MS_VIDEO_CONF(800000,  2000000,       720P, 25, 4),
	MS_VIDEO_CONF(800000,  1536000,        XGA, 25, 4),
	MS_VIDEO_CONF( 600000,  1024000,       SVGA, 25, 2),
	MS_VIDEO_CONF( 350000,   600000,        VGA, 25, 2),
	MS_VIDEO_CONF( 350000,   600000,        VGA, 15, 1),
	MS_VIDEO_CONF( 200000,   350000,        CIF, 18, 1),
	MS_VIDEO_CONF( 150000,   200000,       QVGA, 15, 1),
	MS_VIDEO_CONF( 100000,   150000,       QVGA, 10, 1),
	MS_VIDEO_CONF(  64000,   100000,       QCIF, 12, 1),
	MS_VIDEO_CONF(      0,    64000,       QCIF,  5 ,1)
};

static MSVideoConfiguration high_fps_embedded_vp8_conf_list[] = {
	MS_VIDEO_CONF(2048000, 2560000,       UXGA, 12, 2),
	MS_VIDEO_CONF(1024000, 1536000, SXGA_MINUS, 12, 2),
	MS_VIDEO_CONF( 750000, 1024000,        XGA, 12, 2),
	MS_VIDEO_CONF( 500000,  750000,       SVGA, 12, 2),
	MS_VIDEO_CONF( 300000,  500000,        VGA, 12, 2),
	MS_VIDEO_CONF( 100000,  300000,       QVGA, 18, 2),
	MS_VIDEO_CONF(  64000,  100000,       QCIF, 12, 2),
	MS_VIDEO_CONF(300000, 600000,          VGA, 12, 1),
	MS_VIDEO_CONF(100000, 300000,         QVGA, 10, 1),
	MS_VIDEO_CONF( 64000, 100000,         QCIF, 10, 1),
	MS_VIDEO_CONF(      0,   64000,       QCIF,  5, 1)
};


void register_video_preset_high_fps(MSVideoPresetsManager *manager) {
	ms_video_presets_manager_register_preset_configuration(manager, "high-fps", "desktop,vp8", high_fps_desktop_vp8_conf_list);
	ms_video_presets_manager_register_preset_configuration(manager, "high-fps", "embedded,vp8", high_fps_embedded_vp8_conf_list);
}

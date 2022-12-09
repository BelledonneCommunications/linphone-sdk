/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2 
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef MS_VIDEO_NEON_H
#define MS_VIDEO_NEON_H

void rotate_down_scale_plane_neon_anticlockwise(int wDest, int hDest, int full_width, const uint8_t* src, uint8_t* dst,bool_t down_scale);

void rotate_down_scale_plane_neon_anticlockwise(int wDest, int hDest, int full_width, const uint8_t* src, uint8_t* dst,bool_t down_scale);

void rotate_down_scale_cbcr_to_cr_cb(int wDest, int hDest, int full_width, const uint8_t* cbcr_src, uint8_t* cr_dst, uint8_t* cb_dst,bool_t clockWise,bool_t down_scale);

void rotate_down_scale_plane_neon_clockwise(int wDest, int hDest, int full_width, const uint8_t* src, uint8_t* dst,bool_t down_scale);

#endif


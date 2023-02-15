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

#include <mediastreamer2/mscodecutils.h>

void ms_video_starter_init(MSVideoStarter *vs) {
	vs->next_time = 0;
	vs->i_frame_count = 0;
	vs->active = TRUE;
}

void ms_video_starter_first_frame(MSVideoStarter *vs, uint64_t curtime) {
	vs->next_time = curtime + 2000;
}

bool_t ms_video_starter_need_i_frame(MSVideoStarter *vs, uint64_t curtime) {
	if ((vs->active == FALSE) || (vs->next_time == 0)) return FALSE;
	if (curtime >= vs->next_time) {
		vs->i_frame_count++;
		if (vs->i_frame_count == 1) {
			vs->next_time += 2000;
		} else {
			vs->next_time = 0;
		}
		return TRUE;
	}
	return FALSE;
}

void ms_video_starter_deactivate(MSVideoStarter *vs) {
	vs->active = FALSE;
}

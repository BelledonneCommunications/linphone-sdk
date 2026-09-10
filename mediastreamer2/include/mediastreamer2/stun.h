/*
 * Copyright (c) 2010-2026 Belledonne Communications SARL.
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

#ifndef MS_STUN_H
#define MS_STUN_H

typedef struct _MSTurnContext MSTurnContext;

typedef struct {
	uint32_t nb_send_indication;
	uint32_t nb_data_indication;
	uint32_t nb_received_channel_msg;
	uint32_t nb_sent_channel_msg;
	uint16_t nb_successful_allocate;
	uint16_t nb_successful_refresh;
	uint16_t nb_successful_create_permission;
	uint16_t nb_successful_channel_bind;
} MSTurnContextStatistics;

#endif /* MS_STUN_H */

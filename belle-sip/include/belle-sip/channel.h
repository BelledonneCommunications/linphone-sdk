/*
 * Copyright (c) 2026 Belledonne Communications SARL.
 *
 * This file is part of belle-sip.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BELLE_SIP_CHANNEL_H
#define BELLE_SIP_CHANNEL_H

#include "belle-sip/defs.h"
#include "belle-sip/types.h"

BELLE_SIP_BEGIN_DECLS

typedef struct belle_sip_channel belle_sip_channel_t;

typedef enum belle_sip_channel_state {
	BELLE_SIP_CHANNEL_INIT,
	BELLE_SIP_CHANNEL_RES_IN_PROGRESS,
	BELLE_SIP_CHANNEL_RES_DONE,
	BELLE_SIP_CHANNEL_CONNECTING,
	BELLE_SIP_CHANNEL_RETRY,
	BELLE_SIP_CHANNEL_READY,
	BELLE_SIP_CHANNEL_ERROR,
	BELLE_SIP_CHANNEL_DISCONNECTED
} belle_sip_channel_state_t;

BELLESIP_EXPORT const char *belle_sip_channel_state_to_string(belle_sip_channel_state_t state);

BELLESIP_EXPORT const char *belle_sip_channel_get_public_address(belle_sip_channel_t *obj, int *port);
BELLESIP_EXPORT const char *belle_sip_channel_get_transport_name(const belle_sip_channel_t *obj);
BELLESIP_EXPORT const char *belle_sip_channel_get_transport_name_lower_case(const belle_sip_channel_t *obj);
BELLESIP_EXPORT belle_sip_channel_state_t belle_sip_channel_get_state(const belle_sip_channel_t *chan);

BELLE_SIP_END_DECLS

#endif

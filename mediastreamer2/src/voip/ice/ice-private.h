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

#ifndef ICE_PRIVATE_H
#define ICE_PRIVATE_H

#include <mediastreamer2/ice.h>

#ifdef __cplusplus
extern "C" {
#endif

void ice_check_list_set_rtp_session(IceCheckList *cl, RtpSession *rtp_session);

void ice_check_list_process(IceCheckList *cl, RtpSession *rtp_session);

void ice_handle_stun_packet(IceCheckList *cl, RtpSession *rtp_session, const OrtpEventData *evt_data);

void ice_check_list_print_route(const IceCheckList *cl, const char *message);

#ifdef __cplusplus
}
#endif

#endif

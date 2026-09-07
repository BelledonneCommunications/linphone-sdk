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

#include <bctoolbox/defs.h>

#include "mediastreamer2/mscodecutils.h"
#include "mediastreamer2/msfactory.h"

static int get_packetization_mode(const char *fmtp) {
	char mode_as_string[2];
	if (fmtp && fmtp_get_value(fmtp, "packetization-mode", mode_as_string, sizeof(mode_as_string))) {
		// packetization-mode provided, so 2 cases, 0 our 1
		mode_as_string[1] = 0;
		return atoi(mode_as_string);
	} else return 0; // default value
}

static bool_t get_profile_level_id(const char *fmtp, char *plid, size_t plid_size) {
	return fmtp && fmtp_get_value(fmtp, "profile-level-id", plid, plid_size) && strlen(plid) == 6;
}

static bool_t has_main_profile(const char *fmtp) {
	char plid[16];
	return get_profile_level_id(fmtp, plid, sizeof(plid)) && strncasecmp(plid, "4d", 2) == 0;
}

/*
 * RFC 6184 8.2.2: the profile is part of the payload type identity, so the answer must carry the offered
 * profile and constraints; only the level part remains ours.
 */
static void mirror_remote_profile(PayloadType *pt, const char *remote_fmtp) {
	char remote_plid[16];
	char local_plid[16];
	char answered_plid[7];
	char *new_fmtp;

	if (!get_profile_level_id(remote_fmtp, remote_plid, sizeof(remote_plid))) return;
	if (get_profile_level_id(pt->recv_fmtp, local_plid, sizeof(local_plid))) {
		const char *found;
		memcpy(answered_plid, remote_plid, 4);
		memcpy(answered_plid + 4, local_plid + 4, 2);
		answered_plid[6] = 0;
		found = strstr(pt->recv_fmtp, local_plid);
		if (found == NULL || strcmp(local_plid, answered_plid) == 0) return;
		new_fmtp = ms_strdup_printf("%.*s%s%s", (int)(found - pt->recv_fmtp), pt->recv_fmtp, answered_plid,
		                            found + strlen(local_plid));
	} else if (pt->recv_fmtp) {
		new_fmtp = ms_strdup_printf("%s; profile-level-id=%s", pt->recv_fmtp, remote_plid);
	} else {
		new_fmtp = ms_strdup_printf("profile-level-id=%s", remote_plid);
	}
	payload_type_set_recv_fmtp(pt, new_fmtp);
	ms_free(new_fmtp);
}

/*
 * To start with, only implement case where a remote contains an H264 payload type with  packetization-mode=1. In such
 * case, if mime type match, answer packetization-mode=1 regardless of the local configuration.
 */
static PayloadType *h264_match(BCTBX_UNUSED(MSOfferAnswerContext *ctx),
                               const bctbx_list_t *local_payloads,
                               const PayloadType *refpt,
                               const bctbx_list_t *remote_payloads,
                               bool_t reading_response) {
	PayloadType *pt = NULL;
	const bctbx_list_t *it;
	PayloadType *local_h264_with_packetization_mode_1_pt = NULL;
	bctbx_list_t *local_h264_list = NULL;
	PayloadType *remote_h264_with_packetization_mode_1_pt = NULL;
	PayloadType *remote_h264_main_profile_pt = NULL;
	PayloadType *matching_pt = NULL;

	// extract h264 from remote list and get first one with packetization-mode=1 if any,
	// giving precedence to a main profile payload type over a baseline one
	for (it = remote_payloads; it != NULL; it = it->next) {
		pt = (PayloadType *)it->data;
		if (strcasecmp(pt->mime_type, "h264") == 0) {
			if (get_packetization_mode(pt->send_fmtp) == 1) {
				if (remote_h264_with_packetization_mode_1_pt == NULL) remote_h264_with_packetization_mode_1_pt = pt;
				if (remote_h264_main_profile_pt == NULL && has_main_profile(pt->send_fmtp))
					remote_h264_main_profile_pt = pt;
			}
		}
	}
	if (remote_h264_main_profile_pt != NULL) remote_h264_with_packetization_mode_1_pt = remote_h264_main_profile_pt;
	// same for local
	for (it = local_payloads; it != NULL; it = it->next) {
		pt = (PayloadType *)it->data;
		if (strcasecmp(pt->mime_type, "h264") == 0) {
			local_h264_list = bctbx_list_append(local_h264_list, pt);
			if (local_h264_with_packetization_mode_1_pt == NULL && get_packetization_mode(pt->recv_fmtp) == 1)
				local_h264_with_packetization_mode_1_pt = pt;
		}
	}

	if (bctbx_list_size(local_h264_list) < 1) {
		ms_message("No H264 payload configured locally");
		goto end;
	}
	// taking first one by default
	matching_pt = bctbx_list_get_data(local_h264_list);

	if (remote_h264_with_packetization_mode_1_pt != NULL) {
		// proceeding with packetization-mode=1
		// at least one offer has packetization-mode=1, so this is the one we want.
		if (remote_h264_with_packetization_mode_1_pt != refpt) {
			// not the right one
			matching_pt = NULL;
			goto end;
		} else {
			// this is our best choice.
			if (local_h264_with_packetization_mode_1_pt) {
				// there is also a packetization-mode=1 in local conf, so taking it
				matching_pt = local_h264_with_packetization_mode_1_pt;
			} else {
				// if only packetization-mode=0 locally configured, we assume packetization-mode=1
				// taking firt one from local
				/* FIXME: the PayloadType from the const list 'local_payloads' is modified here, is this intended ?*/
				ms_warning("h264_match(): fixing local payload type.");
				matching_pt = bctbx_list_get_data(local_h264_list);
				// "fixing" matching payload
				char *fixed_fmtp;
				if (matching_pt->recv_fmtp)
					fixed_fmtp = ms_strdup_printf("%s; packetization-mode=1", matching_pt->recv_fmtp);
				else fixed_fmtp = ms_strdup(matching_pt->recv_fmtp);
				payload_type_set_recv_fmtp(matching_pt, fixed_fmtp);
				ms_free(fixed_fmtp);

				if (matching_pt->send_fmtp)
					fixed_fmtp = ms_strdup_printf("%s ; packetization-mode=1", matching_pt->send_fmtp);
				else fixed_fmtp = ms_strdup(matching_pt->send_fmtp);
				payload_type_set_send_fmtp(matching_pt, fixed_fmtp);
				ms_free(fixed_fmtp);
			}
		}
	}
end:
	if (local_h264_list) {
		bctbx_list_free(local_h264_list);
	}

	if (matching_pt == NULL) return NULL;
	pt = payload_type_clone(matching_pt);
	if (!reading_response) mirror_remote_profile(pt, refpt->send_fmtp);
	return pt;
}

static MSOfferAnswerContext *h264_offer_answer_create_context(void) {
	static MSOfferAnswerContext h264_oa = {h264_match, NULL, NULL};
	return &h264_oa;
}

MSOfferAnswerProvider h264_offer_answer_provider = {"h264", h264_offer_answer_create_context};

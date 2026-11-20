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

static long get_profile_level_id_idc(const char *fmtp) {
	// Defines as 3 bytes in https://www.rfc-editor.org/info/rfc6184/#section-8.1:
	// - profile_idc
	// - profile-iop
	// - level_idc
	char profile_level_id_as_string[7];
	if (fmtp &&
	    fmtp_get_value(fmtp, "profile-level-id", profile_level_id_as_string, sizeof(profile_level_id_as_string))) {
		profile_level_id_as_string[6] = 0;
		char profile_idc_as_string[3];
		profile_idc_as_string[0] = profile_level_id_as_string[0];
		profile_idc_as_string[1] = profile_level_id_as_string[1];
		profile_idc_as_string[2] = 0;
		return strtol(profile_idc_as_string, NULL, 16);
	} else return 66; // default value defined in https://www.rfc-editor.org/info/rfc6184/#section-8.1
}

/*
 * To start with, only implement case where a remote contains an H264 payload type with  packetization-mode=1. In such
 * case, if mime type match, answer packetization-mode=1 regardless of the local configuration.
 */
static PayloadType *h264_match(BCTBX_UNUSED(MSOfferAnswerContext *ctx),
                               const bctbx_list_t *local_payloads,
                               const PayloadType *refpt,
                               const bctbx_list_t *remote_payloads,
                               BCTBX_UNUSED(bool_t reading_response)) {
	PayloadType *pt = NULL;
	PayloadType *pt2 = NULL;
	const bctbx_list_t *it, *it2;
	bctbx_list_t *local_h264_list = NULL;
	bctbx_list_t *remote_h264_list = NULL;
	PayloadType *remote_h264_with_packetization_mode_1_pt = NULL;
	PayloadType *remote_h264_with_packetization_mode_0_pt = NULL;
	PayloadType *local_h264_with_packetization_mode_1_pt = NULL;
	PayloadType *local_h264_with_packetization_mode_0_pt = NULL;
	PayloadType *preferred_remote_pt = NULL;
	PayloadType *matching_pt = NULL;

	// default value defined in https://www.rfc-editor.org/info/rfc6184/#section-8.1
	long remote_profile_idc = 66;

	// Get the all H264 payloads in the local and remote lists
	for (it = local_payloads; it != NULL; it = it->next) {
		pt = (PayloadType *)it->data;
		if (strcasecmp(pt->mime_type, "h264") == 0) {
			local_h264_list = bctbx_list_append(local_h264_list, pt);
		}
	}

	if (bctbx_list_size(local_h264_list) < 1) {
		ms_message("No H264 payload configured locally");
		goto end;
	}

	for (it = remote_payloads; it != NULL; it = it->next) {
		pt = (PayloadType *)it->data;
		if (strcasecmp(pt->mime_type, "h264") == 0) {
			remote_h264_list = bctbx_list_append(remote_h264_list, pt);
		}
	}

	// extract h264 from remote list and get first one with packetization-mode=1 if any
	// Search a H264 payload in the local and remote lists having the same profile_idc value
	for (it = remote_h264_list; it != NULL; it = it->next) {
		pt = (PayloadType *)it->data;
		remote_profile_idc = get_profile_level_id_idc(pt->send_fmtp);
		for (it2 = local_h264_list; it2 != NULL; it2 = it2->next) {
			pt2 = (PayloadType *)it2->data;
			if (remote_profile_idc == get_profile_level_id_idc(pt2->recv_fmtp)) {
				if (remote_h264_with_packetization_mode_1_pt == NULL && get_packetization_mode(pt->send_fmtp) == 1) {
					remote_h264_with_packetization_mode_1_pt = pt;
				} else if (remote_h264_with_packetization_mode_0_pt == NULL) {
					remote_h264_with_packetization_mode_0_pt = pt;
				}
			}
		}
	}

	// Prefer payload where the remote enabled packetisation mode
	if (remote_h264_with_packetization_mode_1_pt) {
		preferred_remote_pt = remote_h264_with_packetization_mode_1_pt;
	} else if (remote_h264_with_packetization_mode_0_pt) {
		preferred_remote_pt = remote_h264_with_packetization_mode_0_pt;
	}

	if (preferred_remote_pt == NULL) {
		ms_message("No remote H264 payload configured matching any profile_idc in the local H264 payload list");
		goto end;
	}

	bool_t remote_packetisation_mode_enabled = (get_packetization_mode(preferred_remote_pt->send_fmtp) == 1);

	// Find the first local H264 payload that has packetisation mode set to 0 and 1 having the same profile_idc as the
	// remote one.
	remote_profile_idc = get_profile_level_id_idc(preferred_remote_pt->send_fmtp);
	for (it = local_h264_list; it != NULL; it = it->next) {
		pt = (PayloadType *)it->data;
		if (remote_profile_idc == get_profile_level_id_idc(pt->recv_fmtp)) {
			if (!local_h264_with_packetization_mode_1_pt && get_packetization_mode(pt->recv_fmtp) == 1) {
				local_h264_with_packetization_mode_1_pt = pt;
			} else if (!local_h264_with_packetization_mode_0_pt) {
				local_h264_with_packetization_mode_0_pt = pt;
			}
		}
	}

	// Prefer a local payload having the same packetisation setting as the remote
	if (remote_packetisation_mode_enabled) {
		if (local_h264_with_packetization_mode_1_pt) {
			matching_pt = local_h264_with_packetization_mode_1_pt;
		} else if (local_h264_with_packetization_mode_0_pt) {
			matching_pt = local_h264_with_packetization_mode_0_pt;
		}
	} else {
		if (local_h264_with_packetization_mode_0_pt) {
			matching_pt = local_h264_with_packetization_mode_0_pt;
		} else if (local_h264_with_packetization_mode_1_pt) {
			matching_pt = local_h264_with_packetization_mode_1_pt;
		}
	}

	if (matching_pt == NULL) {
		ms_message("No local H264 payload configured matching any profile_idc put down by the remote");
		goto end;
	}

	if (remote_packetisation_mode_enabled) {
		// proceeding with packetization-mode=1
		// at least one offer has packetization-mode=1, so this is the one we want.
		if (preferred_remote_pt != refpt) {
			// not the right one
			matching_pt = NULL;
			goto end;
		} else {
			// this is our best choice.
			if (get_packetization_mode(matching_pt->recv_fmtp) == 0) {
				// if only packetization-mode=0 locally configured, we assume packetization-mode=1
				// taking firt one from local
				/* FIXME: the PayloadType from the const list 'local_payloads' is modified here, is this intended ?*/
				ms_warning("h264_match(): fixing local payload type.");
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
	if (remote_h264_list) {
		bctbx_list_free(remote_h264_list);
	}
	if (local_h264_list) {
		bctbx_list_free(local_h264_list);
	}

	return matching_pt ? payload_type_clone(matching_pt) : NULL;
}

static MSOfferAnswerContext *h264_offer_answer_create_context(void) {
	static MSOfferAnswerContext h264_oa = {h264_match, NULL, NULL};
	return &h264_oa;
}

MSOfferAnswerProvider h264_offer_answer_provider = {"h264", h264_offer_answer_create_context};

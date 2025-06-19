/*
 * Copyright (c) 2012-2019 Belledonne Communications SARL.
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

/**
 * non-INVITE client transaction implementation.
 **/

#include "belle_sip_internal.h"

static int nict_on_timer_K(belle_sip_nict_t *obj) {
	belle_sip_transaction_terminate((belle_sip_transaction_t *)obj);
	return BELLE_SIP_STOP;
}

static void nict_set_completed(belle_sip_nict_t *obj, belle_sip_response_t *resp) {
	belle_sip_transaction_t *base = (belle_sip_transaction_t *)obj;
	const belle_sip_timer_config_t *cfg = belle_sip_transaction_get_timer_config(base);
	int channel_is_reliable = belle_sip_channel_is_reliable(base->channel);
	belle_sip_transaction_set_state(base, BELLE_SIP_TRANSACTION_COMPLETED);
	if (obj->timer_K) belle_sip_fatal("Should never happen.");

	belle_sip_client_transaction_notify_response((belle_sip_client_transaction_t *)obj, resp);
	/* After the response is notified, the channel may be shutdown and cleared immediately, this is why it shall be
	 * accessed before.*/
	if (!channel_is_reliable) {
		obj->timer_K = belle_sip_timeout_source_new((belle_sip_source_func_t)nict_on_timer_K, obj, cfg->T4);
		belle_sip_object_set_name((belle_sip_object_t *)obj->timer_K, "timer_K");
		belle_sip_transaction_start_timer(base, obj->timer_K);
	} else belle_sip_transaction_terminate(base);
}

static void nict_on_response(belle_sip_nict_t *obj, belle_sip_response_t *resp) {
	belle_sip_transaction_t *base = (belle_sip_transaction_t *)obj;
	int code = belle_sip_response_get_status_code(resp);

	switch (base->state) {
		case BELLE_SIP_TRANSACTION_TRYING:
			if (code < 200) {
				belle_sip_transaction_set_state(base, BELLE_SIP_TRANSACTION_PROCEEDING);
				belle_sip_client_transaction_notify_response((belle_sip_client_transaction_t *)obj, resp);
			} else {
				nict_set_completed(obj, resp);
			}
			break;
		case BELLE_SIP_TRANSACTION_PROCEEDING:
			if (code >= 200) {
				nict_set_completed(obj, resp);
			}
			break;
		default:
			break;
	}
}

static void nict_on_terminate(belle_sip_nict_t *obj) {
	belle_sip_transaction_t *base = (belle_sip_transaction_t *)obj;
	if (obj->timer_F) {
		belle_sip_transaction_stop_timer(base, obj->timer_F);
		belle_sip_object_unref(obj->timer_F);
		obj->timer_F = NULL;
	}
	if (obj->timer_E) {
		belle_sip_transaction_stop_timer(base, obj->timer_E);
		belle_sip_object_unref(obj->timer_E);
		obj->timer_E = NULL;
	}
	if (obj->timer_K) {
		belle_sip_transaction_stop_timer(base, obj->timer_K);
		belle_sip_object_unref(obj->timer_K);
		obj->timer_K = NULL;
	}
}

static int nict_on_timer_F(belle_sip_nict_t *obj) {
	belle_sip_transaction_t *base = (belle_sip_transaction_t *)obj;
	switch (base->state) {
		case BELLE_SIP_TRANSACTION_TRYING:
		case BELLE_SIP_TRANSACTION_PROCEEDING:
			belle_sip_transaction_notify_timeout(base);
			break;
		default:
			break;
	}
	return BELLE_SIP_STOP;
}

static int nict_on_timer_E(belle_sip_nict_t *obj) {
	belle_sip_transaction_t *base = (belle_sip_transaction_t *)obj;
	const belle_sip_timer_config_t *cfg = belle_sip_transaction_get_timer_config(base);

	switch (base->state) {
		case BELLE_SIP_TRANSACTION_TRYING: {
			/*reset the timer */
			int64_t prev_timeout = belle_sip_source_get_timeout_int64(obj->timer_E);
			belle_sip_source_set_timeout_int64(obj->timer_E, MIN(2 * prev_timeout, cfg->T2));
			belle_sip_message("nict_on_timer_E: sending retransmission");
			belle_sip_channel_queue_message(base->channel, (belle_sip_message_t *)base->request);
		} break;
		case BELLE_SIP_TRANSACTION_PROCEEDING:
			belle_sip_source_set_timeout_int64(obj->timer_E, cfg->T2);
			belle_sip_message("nict_on_timer_E: sending retransmission");
			belle_sip_channel_queue_message(base->channel, (belle_sip_message_t *)base->request);
			break;
		default:
			/*if we are not in these cases, timer_E does nothing, so remove it*/
			return BELLE_SIP_STOP;
			break;
	}
	return BELLE_SIP_CONTINUE_WITHOUT_CATCHUP;
}

static void nict_stop_retransmissions(belle_sip_nict_t *obj) {
	belle_sip_transaction_t *base = (belle_sip_transaction_t *)obj;
	if (obj->timer_E) {
		belle_sip_transaction_stop_timer(base, obj->timer_E);
		belle_sip_object_unref(obj->timer_E);
		obj->timer_E = NULL;
	}
}

static void nict_send_request(belle_sip_nict_t *obj) {
	belle_sip_transaction_t *base = (belle_sip_transaction_t *)obj;
	const belle_sip_timer_config_t *cfg = belle_sip_transaction_get_timer_config(base);

	belle_sip_transaction_set_state(base, BELLE_SIP_TRANSACTION_TRYING);
	obj->timer_F = belle_sip_timeout_source_new((belle_sip_source_func_t)nict_on_timer_F, obj, cfg->T1 * 64);
	belle_sip_object_set_name((belle_sip_object_t *)obj->timer_F, "timer_F");
	belle_sip_transaction_start_timer(base, obj->timer_F);

	if (!belle_sip_channel_is_reliable(base->channel)) {
		obj->timer_E = belle_sip_timeout_source_new((belle_sip_source_func_t)nict_on_timer_E, obj, cfg->T1);
		belle_sip_object_set_name((belle_sip_object_t *)obj->timer_E, "timer_E");
		belle_sip_transaction_start_timer(base, obj->timer_E);
	}

	belle_sip_channel_queue_message(base->channel, (belle_sip_message_t *)base->request);
}

static void nict_destroy(belle_sip_nict_t *obj) {
	nict_on_terminate(obj);
}

BELLE_SIP_DECLARE_NO_IMPLEMENTED_INTERFACES(belle_sip_nict_t);

BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_BEGIN(belle_sip_nict_t){
    {{BELLE_SIP_VPTR_INIT(belle_sip_nict_t, belle_sip_client_transaction_t, TRUE),
      (belle_sip_object_destroy_t)nict_destroy, NULL, NULL, BELLE_SIP_DEFAULT_BUFSIZE_HINT},
     (void (*)(belle_sip_transaction_t *))nict_on_terminate},
    (void (*)(belle_sip_client_transaction_t *))nict_send_request,
    (void (*)(belle_sip_client_transaction_t *, belle_sip_response_t *))nict_on_response,
    (void (*)(belle_sip_client_transaction_t *))nict_stop_retransmissions} BELLE_SIP_INSTANCIATE_CUSTOM_VPTR_END

    belle_sip_nict_t *belle_sip_nict_new(belle_sip_provider_t *prov, belle_sip_request_t *req) {
	belle_sip_nict_t *obj = belle_sip_object_new(belle_sip_nict_t);
	belle_sip_client_transaction_init((belle_sip_client_transaction_t *)obj, prov, req);
	return obj;
}

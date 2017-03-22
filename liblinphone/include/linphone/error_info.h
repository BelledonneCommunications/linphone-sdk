/*
error_info.h
Copyright (C) 2016  Belledonne Communications SARL

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

#ifndef LINPHONE_ERROR_INFO_H
#define LINPHONE_ERROR_INFO_H

#include "linphone/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup misc
 * @{
 */

/**
 * Create an empty LinphoneErrorInfo object.
 * The LinphoneErrorInfo object carries these fields:
 * - a LinphoneReason enum member giving overall signification of the error reported.
 * - the "protocol" name in which the protocol reason code has meaning, for example SIP or Q.850
 * - the "protocol code", an integer referencing the kind of error reported
 * - the "phrase", a text phrase describing the error
 * - the "warning", the content of warning headers if any
 * - a sub "LinphoneErrorInfo" may be provided if a SIP response includes a Reason header (RFC3326).
**/
LINPHONE_PUBLIC LinphoneErrorInfo *linphone_error_info_new(void);

/**
 * Increment refcount.
 * @param[in] ei ErrorInfo object
**/
LINPHONE_PUBLIC LinphoneErrorInfo *linphone_error_info_ref(LinphoneErrorInfo *ei);

/**
 * Decrement refcount and possibly free the object.
 * @param[in] ei ErrorInfo object
**/
LINPHONE_PUBLIC void linphone_error_info_unref(LinphoneErrorInfo *ei);

/**
 * Get reason code from the error info.
 * @param[in] ei ErrorInfo object
 * @return A #LinphoneReason
**/
LINPHONE_PUBLIC LinphoneReason linphone_error_info_get_reason(const LinphoneErrorInfo *ei);

/**
 * Get textual phrase from the error info.
 * This is the text that is provided by the peer in the protocol (SIP).
 * @param[in] ei ErrorInfo object
 * @return The error phrase
**/
LINPHONE_PUBLIC const char * linphone_error_info_get_phrase(const LinphoneErrorInfo *ei);

/**
 * Provides additional information regarding the failure.
 * With SIP protocol, the content of "Warning" headers are returned.
 * @param[in] ei ErrorInfo object
 * @return More details about the failure
**/
LINPHONE_PUBLIC const char * linphone_error_info_get_warnings(const LinphoneErrorInfo *ei);


/**
 * Get the status code from the low level protocol (ex a SIP status code).
 * @param[in] ei ErrorInfo object
 * @return The status code
**/
LINPHONE_PUBLIC int linphone_error_info_get_protocol_code(const LinphoneErrorInfo *ei);

/**
 * Assign information to a LinphoneErrorInfo object.
 * @param[in] ei ErrorInfo object
 */
LINPHONE_PUBLIC void linphone_error_info_set(LinphoneErrorInfo *ei, LinphoneReason reason, int code, const char *status_string, const char *warning);

LINPHONE_PUBLIC void linphone_error_info_set_reason(LinphoneErrorInfo *ei, LinphoneReason reason);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* LINPHONE_ERROR_INFO_H */

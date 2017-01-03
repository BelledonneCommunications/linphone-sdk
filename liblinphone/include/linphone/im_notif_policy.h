/*
im_notif_policy.h
Copyright (C) 2010-2016 Belledonne Communications SARL

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

#ifndef LINPHONE_IM_NOTIF_POLICY_H_
#define LINPHONE_IM_NOTIF_POLICY_H_


#ifdef __cplusplus
extern "C" {
#endif


/**
 * @addtogroup chatroom
 * @{
 */

/**
 * Policy to use to send/display instant messaging composing/delivery/display notifications.
 * The sending of this information is done as in the RFCs 3994 (is_composing) and 5438 (imdn delivered/displayed).
 */
typedef struct _LinphoneImNotifPolicy LinphoneImNotifPolicy;


/**
 * Acquire a reference to the LinphoneImNotifPolicy object.
 * @param[in] policy LinphoneImNotifPolicy object.
 * @return The same LinphoneImNotifPolicy object.
**/
LINPHONE_PUBLIC LinphoneImNotifPolicy * linphone_im_notif_policy_ref(LinphoneImNotifPolicy *policy);

/**
 * Release reference to the LinphoneImNotifPolicy object.
 * @param[in] policy LinphoneImNotifPolicy object.
**/
LINPHONE_PUBLIC void linphone_im_notif_policy_unref(LinphoneImNotifPolicy *policy);

/**
 * Retrieve the user pointer associated with the LinphoneImNotifPolicy object.
 * @param[in] policy LinphoneImNotifPolicy object.
 * @return The user pointer associated with the LinphoneImNotifPolicy object.
**/
LINPHONE_PUBLIC void *linphone_im_notif_policy_get_user_data(const LinphoneImNotifPolicy *policy);

/**
 * Assign a user pointer to the LinphoneImNotifPolicy object.
 * @param[in] policy LinphoneImNotifPolicy object.
 * @param[in] ud The user pointer to associate with the LinphoneImNotifPolicy object.
**/
LINPHONE_PUBLIC void linphone_im_notif_policy_set_user_data(LinphoneImNotifPolicy *policy, void *ud);

/**
 * Clear an IM notif policy (deactivate all display and sending of notifications).
 * @param[in] policy LinphoneImNotifPolicy object.
 */
LINPHONE_PUBLIC void linphone_im_notif_policy_clear(LinphoneImNotifPolicy *policy);

/**
 * Tell whether is_composing notifications are being sent.
 * @param[in] policy LinphoneImNotifPolicy object
 * @return Boolean value telling whether is_composing notifications are being sent.
 */
LINPHONE_PUBLIC bool_t linphone_im_notif_policy_get_send_is_composing(const LinphoneImNotifPolicy *policy);

/**
 * Enable is_composing notifications sending.
 * @param[in] policy LinphoneImNotifPolicy object
 * @param[in] enable Boolean value telling whether to send is_composing notifications.
 */
LINPHONE_PUBLIC void linphone_im_notif_policy_set_send_is_composing(LinphoneImNotifPolicy *policy, bool_t enable);

/**
 * Tell whether is_composing notifications are being displayed.
 * @param[in] policy LinphoneImNotifPolicy object
 * @return Boolean value telling whether is_composing notifications are being displayed.
 */
LINPHONE_PUBLIC bool_t linphone_im_notif_policy_get_display_is_composing(const LinphoneImNotifPolicy *policy);

/**
 * Enable is_composing notifications display.
 * @param[in] policy LinphoneImNotifPolicy object
 * @param[in] enable Boolean value telling whether to display is_composing notifications.
 */
LINPHONE_PUBLIC void linphone_im_notif_policy_set_display_is_composing(LinphoneImNotifPolicy *policy, bool_t enable);

/**
 * Tell whether imdn delivered notifications are being sent.
 * @param[in] policy LinphoneImNotifPolicy object
 * @return Boolean value telling whether imdn delivered notifications are being sent.
 */
LINPHONE_PUBLIC bool_t linphone_im_notif_policy_get_send_imdn_delivered(const LinphoneImNotifPolicy *policy);

/**
 * Enable imdn delivered notifications sending.
 * @param[in] policy LinphoneImNotifPolicy object
 * @param[in] enable Boolean value telling whether to send imdn delivered notifications.
 */
LINPHONE_PUBLIC void linphone_im_notif_policy_set_send_imdn_delivered(LinphoneImNotifPolicy *policy, bool_t enable);

/**
 * Tell whether imdn delivered notifications are being displayed.
 * @param[in] policy LinphoneImNotifPolicy object
 * @return Boolean value telling whether imdn delivered notifications are being displayed.
 */
LINPHONE_PUBLIC bool_t linphone_im_notif_policy_get_display_imdn_delivered(const LinphoneImNotifPolicy *policy);

/**
 * Enable imdn delivered notifications display.
 * @param[in] policy LinphoneImNotifPolicy object
 * @param[in] enable Boolean value telling whether to display imdn delivered notifications.
 */
LINPHONE_PUBLIC void linphone_im_notif_policy_set_display_imdn_delivered(LinphoneImNotifPolicy *policy, bool_t enable);

/**
 * Tell whether imdn displayed notifications are being sent.
 * @param[in] policy LinphoneImNotifPolicy object
 * @return Boolean value telling whether imdn displayed notifications are being sent.
 */
LINPHONE_PUBLIC bool_t linphone_im_notif_policy_get_send_imdn_displayed(const LinphoneImNotifPolicy *policy);

/**
 * Enable imdn displayed notifications sending.
 * @param[in] policy LinphoneImNotifPolicy object
 * @param[in] enable Boolean value telling whether to send imdn displayed notifications.
 */
LINPHONE_PUBLIC void linphone_im_notif_policy_set_send_imdn_displayed(LinphoneImNotifPolicy *policy, bool_t enable);

/**
 * Tell whether imdn displayed notifications are being displayed.
 * @param[in] policy LinphoneImNotifPolicy object
 * @return Boolean value telling whether imdn displayed notifications are being displayed.
 */
LINPHONE_PUBLIC bool_t linphone_im_notif_policy_get_display_imdn_displayed(const LinphoneImNotifPolicy *policy);

/**
 * Enable imdn displayed notifications display.
 * @param[in] policy LinphoneImNotifPolicy object
 * @param[in] enable Boolean value telling whether to display imdn displayed notifications.
 */
LINPHONE_PUBLIC void linphone_im_notif_policy_set_display_imdn_displayed(LinphoneImNotifPolicy *policy, bool_t enable);

/**
 * Get the LinphoneImNotifPolicy object controlling the instant messaging notifications.
 * @param[in] lc LinphoneCore object
 * @return A LinphoneImNotifPolicy object.
 */
LINPHONE_PUBLIC LinphoneImNotifPolicy * linphone_core_get_im_notif_policy(const LinphoneCore *lc);

/**
 * @}
 */


#ifdef __cplusplus
}
#endif

#endif /* LINPHONE_IM_NOTIF_POLICY_H_ */

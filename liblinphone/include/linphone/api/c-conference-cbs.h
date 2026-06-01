/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of Liblinphone
 * (see https://gitlab.linphone.org/BC/public/liblinphone).
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

#ifndef _L_C_CONFERENCE_CBS_H_
#define _L_C_CONFERENCE_CBS_H_

#include "linphone/api/c-callbacks.h"
#include "linphone/api/c-types.h"

// =============================================================================

#ifdef __cplusplus
extern "C" {
#endif // ifdef __cplusplus

/**
 * @addtogroup group_conference
 * @{
 */

/**
 * Acquire a reference to the conference callbacks object.
 * @param[in] cbs The conference callbacks object
 * @return The same conference callbacks object
 **/
LINPHONE_PUBLIC LinphoneConferenceCbs *linphone_conference_cbs_ref(LinphoneConferenceCbs *cbs);

/**
 * Release reference to the conference callbacks object.
 * @param[in] cbs The conference callbacks object
 **/
LINPHONE_PUBLIC void linphone_conference_cbs_unref(LinphoneConferenceCbs *cbs);

/**
 * Retrieve the user pointer associated with the conference callbacks object.
 * @param[in] cbs The conference callbacks object
 * @return The user pointer associated with the conference callbacks object
 **/
LINPHONE_PUBLIC void *linphone_conference_cbs_get_user_data(const LinphoneConferenceCbs *cbs);

/**
 * Assign a user pointer to the conference callbacks object.
 * @param[in] cbs The conference callbacks object
 * @param[in] ud The user pointer to associate with the conference callbacks object
 **/
LINPHONE_PUBLIC void linphone_conference_cbs_set_user_data(LinphoneConferenceCbs *cbs, void *ud);

/**
 * Gets the allowed participant list changed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @return The current allowed participant list changed callback.
 */
LINPHONE_PUBLIC LinphoneConferenceCbsAllowedParticipantListChangedCb
linphone_conference_cbs_get_allowed_participant_list_changed(const LinphoneConferenceCbs *cbs);

/**
 * Sets the allowed participant list changed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @param[in] cb The allowed participant list changed callback to be used.
 */
LINPHONE_PUBLIC void
linphone_conference_cbs_set_allowed_participant_list_changed(LinphoneConferenceCbs *cbs,
                                                             LinphoneConferenceCbsAllowedParticipantListChangedCb cb);

/**
 * Gets the participant added callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @return The current participant added callback.
 */
LINPHONE_PUBLIC LinphoneConferenceCbsParticipantAddedCb
linphone_conference_cbs_get_participant_added(const LinphoneConferenceCbs *cbs);

/**
 * Sets the participant added callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @param[in] cb The participant added callback to be used.
 */
LINPHONE_PUBLIC void linphone_conference_cbs_set_participant_added(LinphoneConferenceCbs *cbs,
                                                                   LinphoneConferenceCbsParticipantAddedCb cb);

/**
 * Gets the participant removed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @return The current participant removed callback.
 */
LINPHONE_PUBLIC LinphoneConferenceCbsParticipantRemovedCb
linphone_conference_cbs_get_participant_removed(const LinphoneConferenceCbs *cbs);

/**
 * Sets the participant removed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @param[in] cb The participant removed callback to be used.
 */
LINPHONE_PUBLIC void linphone_conference_cbs_set_participant_removed(LinphoneConferenceCbs *cbs,
                                                                     LinphoneConferenceCbsParticipantRemovedCb cb);

/**
 * Gets the participant device added callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @return The current participant device added callback.
 */
LINPHONE_PUBLIC LinphoneConferenceCbsParticipantDeviceAddedCb
linphone_conference_cbs_get_participant_device_added(const LinphoneConferenceCbs *cbs);

/**
 * Sets the participant device added callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @param[in] cb The participant device added callback to be used.
 */
LINPHONE_PUBLIC void
linphone_conference_cbs_set_participant_device_added(LinphoneConferenceCbs *cbs,
                                                     LinphoneConferenceCbsParticipantDeviceAddedCb cb);

/**
 * Gets the participant device removed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @return The current participant device removed callback.
 */
LINPHONE_PUBLIC LinphoneConferenceCbsParticipantDeviceRemovedCb
linphone_conference_cbs_get_participant_device_removed(const LinphoneConferenceCbs *cbs);

/**
 * Sets the participant device removed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @param[in] cb The participant device removed callback to be used.
 */
LINPHONE_PUBLIC void
linphone_conference_cbs_set_participant_device_removed(LinphoneConferenceCbs *cbs,
                                                       LinphoneConferenceCbsParticipantDeviceRemovedCb cb);

/**
 * Gets the participant device joining request callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @return The current participant device joining request callback.
 */
LINPHONE_PUBLIC LinphoneConferenceCbsParticipantDeviceJoiningRequestCb
linphone_conference_cbs_get_participant_device_joining_request(const LinphoneConferenceCbs *cbs);

/**
 * Sets the participant device joining request callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @param[in] cb The participant device joining request callback to be used.
 */
LINPHONE_PUBLIC void linphone_conference_cbs_set_participant_device_joining_request(
    LinphoneConferenceCbs *cbs, LinphoneConferenceCbsParticipantDeviceJoiningRequestCb cb);

/**
 * Gets the participant role changed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @return The current participant role changed callback.
 */
LINPHONE_PUBLIC LinphoneConferenceCbsParticipantRoleChangedCb
linphone_conference_cbs_get_participant_role_changed(const LinphoneConferenceCbs *cbs);

/**
 * Sets the participant role changed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @param[in] cb The participant role changed callback to be used.
 */
LINPHONE_PUBLIC void
linphone_conference_cbs_set_participant_role_changed(LinphoneConferenceCbs *cbs,
                                                     LinphoneConferenceCbsParticipantRoleChangedCb cb);

/**
 * Gets the participant admin status changed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @return The current participant admin status changed callback.
 */
LINPHONE_PUBLIC LinphoneConferenceCbsParticipantAdminStatusChangedCb
linphone_conference_cbs_get_participant_admin_status_changed(const LinphoneConferenceCbs *cbs);

/**
 * Sets the participant admin status changed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @param[in] cb The participant admin status changed callback to be used.
 */
LINPHONE_PUBLIC void
linphone_conference_cbs_set_participant_admin_status_changed(LinphoneConferenceCbs *cbs,
                                                             LinphoneConferenceCbsParticipantAdminStatusChangedCb cb);

/**
 * Gets the participant device state changed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @return The current participant device state changed callback.
 */
LINPHONE_PUBLIC LinphoneConferenceCbsParticipantDeviceStateChangedCb
linphone_conference_cbs_get_participant_device_state_changed(const LinphoneConferenceCbs *cbs);

/**
 * Sets the participant device state changed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @param[in] cb The participant device state changed callback to be used.
 */
LINPHONE_PUBLIC void
linphone_conference_cbs_set_participant_device_state_changed(LinphoneConferenceCbs *cbs,
                                                             LinphoneConferenceCbsParticipantDeviceStateChangedCb cb);

/**
 * Gets the participant device is screen sharing changed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @return The current participant device is screen sharing changed callback.
 */
LINPHONE_PUBLIC LinphoneConferenceCbsParticipantDeviceScreenSharingChangedCb
linphone_conference_cbs_get_participant_device_screen_sharing_changed(const LinphoneConferenceCbs *cbs);

/**
 * Sets the participant device is screen sharing changed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @param[in] cb The participant device is screen sharing changed callback to be used.
 */
LINPHONE_PUBLIC void linphone_conference_cbs_set_participant_device_screen_sharing_changed(
    LinphoneConferenceCbs *cbs, LinphoneConferenceCbsParticipantDeviceScreenSharingChangedCb cb);

/**
 * Gets the participant device media availability changed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @return The current participant device media availability changed callback.
 */
LINPHONE_PUBLIC LinphoneConferenceCbsParticipantDeviceMediaAvailabilityChangedCb
linphone_conference_cbs_get_participant_device_media_availability_changed(const LinphoneConferenceCbs *cbs);

/**
 * Sets the participant device media availability changed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @param[in] cb The participant device media availability changed callback to be used.
 */
LINPHONE_PUBLIC void linphone_conference_cbs_set_participant_device_media_availability_changed(
    LinphoneConferenceCbs *cbs, LinphoneConferenceCbsParticipantDeviceMediaAvailabilityChangedCb cb);

/**
 * Gets the participant device media capabilities changed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @return The current participant device media capabilities changed callback.
 */
LINPHONE_PUBLIC LinphoneConferenceCbsParticipantDeviceMediaCapabilityChangedCb
linphone_conference_cbs_get_participant_device_media_capability_changed(const LinphoneConferenceCbs *cbs);

/**
 * Sets the participant device media capabilities changed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @param[in] cb The participant device media capabilities changed callback to be used.
 */
LINPHONE_PUBLIC void linphone_conference_cbs_set_participant_device_media_capability_changed(
    LinphoneConferenceCbs *cbs, LinphoneConferenceCbsParticipantDeviceMediaCapabilityChangedCb cb);

/**
 * Gets the leave failed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @return The current leave failed callback.
 */
LINPHONE_PUBLIC LinphoneConferenceCbsOperationFailedCb
linphone_conference_cbs_get_operation_failed(const LinphoneConferenceCbs *cbs);

/**
 * Sets the leave failed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @param[in] cb The leave failed callback to be used.
 */
LINPHONE_PUBLIC void linphone_conference_cbs_set_operation_failed(LinphoneConferenceCbs *cbs,
                                                                  LinphoneConferenceCbsOperationFailedCb cb);

/**
 * Gets the state changed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @return The current state changed callback.
 */
LINPHONE_PUBLIC LinphoneConferenceCbsStateChangedCb
linphone_conference_cbs_get_state_changed(const LinphoneConferenceCbs *cbs);

/**
 * Sets the state changed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @param[in] cb The state changed callback to be used.
 */
LINPHONE_PUBLIC void linphone_conference_cbs_set_state_changed(LinphoneConferenceCbs *cbs,
                                                               LinphoneConferenceCbsStateChangedCb cb);

/**
 * Gets the available media changed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @return The current available media changed callback.
 */
LINPHONE_PUBLIC LinphoneConferenceCbsAvailableMediaChangedCb
linphone_conference_cbs_get_available_media_changed(const LinphoneConferenceCbs *cbs);

/**
 * Sets the available media changed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @param[in] cb The available media changed callback to be used.
 */
LINPHONE_PUBLIC void
linphone_conference_cbs_set_available_media_changed(LinphoneConferenceCbs *cbs,
                                                    LinphoneConferenceCbsAvailableMediaChangedCb cb);

/**
 * Gets the subject changed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @return The current subject changed callback.
 */
LINPHONE_PUBLIC LinphoneConferenceCbsSubjectChangedCb
linphone_conference_cbs_get_subject_changed(const LinphoneConferenceCbs *cbs);

/**
 * Sets the subject changed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @param[in] cb The subject changed callback to be used.
 */
LINPHONE_PUBLIC void linphone_conference_cbs_set_subject_changed(LinphoneConferenceCbs *cbs,
                                                                 LinphoneConferenceCbsSubjectChangedCb cb);

/**
 * Gets the participant device is speaking changed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @return The current participant device is speaking changed callback.
 */
LINPHONE_PUBLIC LinphoneConferenceCbsParticipantDeviceIsSpeakingChangedCb
linphone_conference_cbs_get_participant_device_is_speaking_changed(const LinphoneConferenceCbs *cbs);

/**
 * Sets the participant device is speaking changed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @param[in] cb The participant device is speaking changed callback to be used.
 */
LINPHONE_PUBLIC void linphone_conference_cbs_set_participant_device_is_speaking_changed(
    LinphoneConferenceCbs *cbs, LinphoneConferenceCbsParticipantDeviceIsSpeakingChangedCb cb);

/**
 * Gets the participant device is muted callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @return The current participant device is muted callback.
 */
LINPHONE_PUBLIC LinphoneConferenceCbsParticipantDeviceIsMutedCb
linphone_conference_cbs_get_participant_device_is_muted(const LinphoneConferenceCbs *cbs);

/**
 * Sets the participant device is muted callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @param[in] cb The participant device is muted callback to be used.
 */
LINPHONE_PUBLIC void
linphone_conference_cbs_set_participant_device_is_muted(LinphoneConferenceCbs *cbs,
                                                        LinphoneConferenceCbsParticipantDeviceIsMutedCb cb);

/**
 * Gets the audio device changed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @return The current audio device changed callback.
 */
LINPHONE_PUBLIC LinphoneConferenceCbsAudioDeviceChangedCb
linphone_conference_cbs_get_audio_device_changed(const LinphoneConferenceCbs *cbs);

/**
 * Sets the audio device changed callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @param[in] cb The audio device changed callback to be used.
 */
LINPHONE_PUBLIC void linphone_conference_cbs_set_audio_device_changed(LinphoneConferenceCbs *cbs,
                                                                      LinphoneConferenceCbsAudioDeviceChangedCb cb);

/**
 * Gets the actively speaking participant device callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @return The current active speaker participant device callback.
 */
LINPHONE_PUBLIC LinphoneConferenceCbsActiveSpeakerParticipantDeviceCb
linphone_conference_cbs_get_active_speaker_participant_device(const LinphoneConferenceCbs *cbs);

/**
 * Sets the actively speaking participant device callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @param[in] cb The active speaker participant device callback to be used.
 */
LINPHONE_PUBLIC void
linphone_conference_cbs_set_active_speaker_participant_device(LinphoneConferenceCbs *cbs,
                                                              LinphoneConferenceCbsActiveSpeakerParticipantDeviceCb cb);

/**
 * Gets the full state received callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @return The current full state received callback.
 */
LINPHONE_PUBLIC LinphoneConferenceCbsFullStateReceivedCb
linphone_conference_cbs_get_full_state_received(const LinphoneConferenceCbs *cbs);

/**
 * Sets the full state received callback.
 * @param[in] cbs #LinphoneConferenceCbs object.
 * @param[in] cb The full state received callback to be used.
 */
LINPHONE_PUBLIC void linphone_conference_cbs_set_full_state_received(LinphoneConferenceCbs *cbs,
                                                                     LinphoneConferenceCbsFullStateReceivedCb cb);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif // ifdef __cplusplus

#endif // ifndef _L_C_CONFERENCE_CBS_H_

/*
 * Copyright (c) 2010-2025 Belledonne Communications SARL.
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

#ifndef LINPHONECORE_H
#define LINPHONECORE_H

#include "ortp/ortp.h"
#include "ortp/payloadtype.h"

#include "linphone/account_creator.h"
#include "linphone/account_creator_service.h"
#include "linphone/api/c-api.h"
#include "linphone/api/c-buffer.h"
#include "linphone/call.h"
#include "linphone/call_params.h"
#include "linphone/call_stats.h"
#include "linphone/callbacks.h"
#include "linphone/chat.h"
#include "linphone/defs.h"
#include "linphone/enums/c-enums.h"
#include "linphone/error_info.h"
#include "linphone/factory.h"
#include "linphone/friendlist.h"
#include "linphone/headers.h"
#include "linphone/im_encryption_engine.h"
#include "linphone/im_notif_policy.h"
#include "linphone/info_message.h"
#include "linphone/logging.h"
#include "linphone/lpconfig.h"
#include "linphone/misc.h"
#include "linphone/player.h"
#include "linphone/presence.h"
#include "linphone/proxy_config.h"
#include "linphone/ringtoneplayer.h"
#include "linphone/sipsetup.h"
#include "linphone/types.h"
#include "linphone/vcard.h"
#include "linphone/video_definition.h"
#include "linphone/xmlrpc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Safely down-cast a belle_sip_object_t into #LinphoneCore
 * @ingroup initializing
 */
#define LINPHONE_CORE(object) BELLE_SIP_CAST(object, LinphoneCore)

/**
 * Creates a #LinphoneAddress object by parsing the user supplied address, given as a string.
 * @param core #LinphoneCore object @maybenil
 * @param address String containing the user supplied address @maybenil
 * @return The created #LinphoneAddress object @maybenil
 * @deprecated 04/06/2024 use linphone_factory_create_address().
 * @ingroup linphone_address
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneAddress *linphone_core_create_address(LinphoneCore *core,
                                                                                  const char *address);

/**
 * @addtogroup misc
 * @{
 */

/**
 * Creates a media file player, that can be used to play audio and video to the user, outside of any call or conference.
 * See linphone_core_get_supported_file_formats_list() for supported multimedia file types.
 * @param core A #LinphoneCore object @notnil
 * @param sound_card_name Playback sound card. If NULL, the ringer sound card set in #LinphoneCore will be used
 * @maybenil
 * @param video_display_name Video display. If NULL, the video display set in #LinphoneCore will be used @maybenil
 * @param window_id Id of the drawing window. See linphone_core_set_native_video_window_id() for a discussion about
 * supported native video window types. @maybenil
 * @return A pointer on the new instance. NULL if failed. @maybenil
 */
LINPHONE_PUBLIC LinphonePlayer *linphone_core_create_local_player(LinphoneCore *core,
                                                                  const char *sound_card_name,
                                                                  const char *video_display_name,
                                                                  void *window_id);

/**
 * Creates an object that holds recording parameters.
 * @see linphone_core_create_recorder()
 * @param core A #LinphoneCore object @notnil
 * @return A pointer on the newly created instance of #LinphoneRecorderParams @notnil
 */
LINPHONE_PUBLIC LinphoneRecorderParams *linphone_core_create_recorder_params(const LinphoneCore *core);

/**
 * Creates an independant media file recorder, that can be used to record user's voice or video
 * outside of any call or conference.
 * See linphone_core_get_supported_file_formats_list() for supported multimedia file types.
 * @param core A #LinphoneCore object. @notnil
 * @param params The #LinphoneRecorderParams that will contains all recorder parameters. @notnil
 * @return A pointer on the new instance. NULL if failed. @notnil
 */
LINPHONE_PUBLIC LinphoneRecorder *linphone_core_create_recorder(LinphoneCore *core,
                                                                const LinphoneRecorderParams *params);

/**
 * Creates an empty info message.
 * @param core the #LinphoneCore @maybenil
 * @return a new LinphoneInfoMessage. @notnil
 *
 * The info message can later be filled with information using linphone_info_message_add_header() or
 *linphone_info_message_set_content(), and finally sent with linphone_core_send_info_message().
 **/
LINPHONE_PUBLIC LinphoneInfoMessage *linphone_core_create_info_message(LinphoneCore *core);

/**
 * Creates a #LinphoneMagicSearch object.
 * @param core #LinphoneCore object @notnil
 * @return The created #LinphoneMagicSearch object @notnil
 */
LINPHONE_PUBLIC LinphoneMagicSearch *linphone_core_create_magic_search(LinphoneCore *core);

/**
 * Asynchronously checks if a new version of the application is available from a well-known http server URI
 * given by #LinphoneCore 's configuration.
 * The result of the check is given through the #LinphoneCoreCbs interface, see
 * linphone_core_cbs_set_version_update_check_result_received().
 * The http URI has to be given in [misc] section as key 'version_check_url_root'.
 * The subdirectory is appended to this root URI, per platform, and a "VERSION" file is fetched.
 * For example:
 * - https://download.linphone.org/releases/android/RELEASE
 * - https://download.linphone.org/releases/windows/RELEASE
 * The RELEASE file is expected to contain the most recent version number available followed by an http uri
 * where this version can be retrieved. For example:
 * 5.2.5	https://play.google.com/store/apps/details?id=org.linphone
 * @param core #LinphoneCore object @notnil
 * @param current_version The current version of the application @notnil
 */
LINPHONE_PUBLIC void linphone_core_check_for_update(LinphoneCore *core, const char *current_version);

/**
 * @}
 */

/**
 * Gets the remote address of the current call.
 * @param core #LinphoneCore object. @notnil
 * @return The remote address of the current call or NULL if there is no current call. @maybenil
 * @ingroup call_control
 */
LINPHONE_PUBLIC const LinphoneAddress *linphone_core_get_current_call_remote_address(LinphoneCore *core);

/**
 * Gets the support level of the 100rel attribute
 * @param core #LinphoneCore object. @notnil
 * @return The 100 rel support level
 * @ingroup call_control
 */
LINPHONE_PUBLIC LinphoneSupportLevel linphone_core_get_tag_100rel_support_level(const LinphoneCore *core);

/**
 * Defines what level of support is provided to the 100rel attribute
 * @param core #LinphoneCore object. @notnil
 * @param level support level of 100rel
 * @ingroup call_control
 */
LINPHONE_PUBLIC void linphone_core_set_tag_100rel_support_level(LinphoneCore *core, LinphoneSupportLevel level);

/**
 * @addtogroup initializing
 * @{
 **/

/*
 * Callback prototype
 */
typedef void (*LinphoneCoreCbFunc)(LinphoneCore *core, void *user_data);

/**
 * This structure holds all callbacks that the application should implement.
 * None is mandatory.
 * @donotwrap
 * @deprecated use #LinphoneCoreCbs instead.
 **/
typedef struct _LinphoneCoreVTable {
	LinphoneCoreGlobalStateChangedCb global_state_changed;             /**< Notifies global state changes*/
	LinphoneCoreRegistrationStateChangedCb registration_state_changed; /**< Notifies registration state changes*/
	LinphoneCoreCallStateChangedCb call_state_changed;                 /**< Notifies call state changes*/
	LinphoneCoreNotifyPresenceReceivedCb notify_presence_received;     /**< Notify received presence events*/
	LinphoneCoreNotifyPresenceReceivedForUriOrTelCb
	    notify_presence_received_for_uri_or_tel; /**< Notify received presence events*/
	LinphoneCoreNewSubscriptionRequestedCb
	    new_subscription_requested; /**< Notify about pending presence subscription request */
	LINPHONE_DEPRECATED LinphoneCoreAuthInfoRequestedCb
	    auth_info_requested; /**< @brief Ask the application some authentication information.
	                             @deprecated 21/09/2016 Use authentication_requested instead. */
	LinphoneCoreAuthenticationRequestedCb
	    authentication_requested;                  /**< Ask the application some authentication information */
	LinphoneCoreCallLogUpdatedCb call_log_updated; /**< Notifies that call log list has been updated */
	LinphoneCoreCallIdUpdatedCb call_id_updated;   /**< Notifies that callid of a call has been updated */
	LinphoneCoreCbsRemainingNumberOfFileTransferChangedCb
	    remaining_number_of_file_transfer_changed;     /**< the number of remaining attachment to download has changed*/
	LinphoneCoreCbsMessageReceivedCb message_received; /**< a message is received, can be text or external body*/
	LinphoneCoreCbsMessagesReceivedCb
	    messages_received; /**< one or more messages are received, can be text or external body*/
	LinphoneCoreCbsMessageReceivedUnableDecryptCb
	    message_received_unable_decrypt; /**< an encrypted message is received but we can't decrypt it*/
	LinphoneCoreIsComposingReceivedCb is_composing_received;   /**< An is-composing notification has been received */
	LinphoneCoreDtmfReceivedCb dtmf_received;                  /**< A dtmf has been received received */
	LinphoneCoreCbsReferReceivedCb refer_received;             /**< An out of call refer was received */
	LinphoneCoreCbsCallGoClearAckSentCb call_goclear_ack_sent; /**< Notifies on sending of GoClear Ack */
	LinphoneCoreCallEncryptionChangedCb
	    call_encryption_changed; /**< Notifies on change in the encryption of call streams */
	LinphoneCoreCbsCallSendMasterKeyChangedCb
	    call_send_master_key_changed; /**< Notifies on change of the send master key of call streams */
	LinphoneCoreCbsCallReceiveMasterKeyChangedCb
	    call_receive_master_key_changed; /**< Notifies on change of the send master key of call streams */
	LinphoneCoreTransferStateChangedCb transfer_state_changed; /**< Notifies when a transfer is in progress */
	LinphoneCoreBuddyInfoUpdatedCb buddy_info_updated;         /**< a LinphoneFriend's BuddyInfo has changed*/
	LinphoneCoreCallStatsUpdatedCb call_stats_updated;         /**< Notifies on refreshing of call's statistics. */
	LinphoneCoreInfoReceivedCb info_received; /**< Notifies an incoming informational message received.*/
	LinphoneCoreSubscriptionStateChangedCb subscription_state_changed; /**< Notifies subscription state change */
	LinphoneCoreCbsNotifySentCb notify_sent; /**< Notifies a an event notification, see linphone_core_subscribe() */
	LinphoneCoreNotifyReceivedCb
	    notify_received; /**< Notifies a an event notification, see linphone_core_subscribe() */
	LinphoneCoreSubscribeReceivedCb
	    subscribe_received; /**< Notifies a subscribe has been received, see linphone_core_subscribe() */
	LinphoneCorePublishStateChangedCb
	    publish_state_changed; /**Notifies publish state change (only from #LinphoneEvent api)*/
	LinphoneCorePublishReceivedCb
	    publish_received; /**< Notifies a publish has been received, see linphone_core_pulish() */
	LinphoneCoreConfiguringStatusCb configuring_status; /** Notifies configuring status changes */
	LINPHONE_DEPRECATED LinphoneCoreTextMessageReceivedCb
	    text_received; /**< @brief A text message has been received.
	                        @deprecated 19/11/2015 Use #message_received instead.*/
	LINPHONE_DEPRECATED LinphoneCoreFileTransferRecvCb
	    file_transfer_recv; /**< @brief Callback to store file received attached to a #LinphoneChatMessage.
	                             @deprecated 19/11/2015 */
	LINPHONE_DEPRECATED LinphoneCoreFileTransferSendCb
	    file_transfer_send; /**< @brief Callback to collect file chunk to be sent for a #LinphoneChatMessage.
	                             @deprecated 19/11/2015 */
	LINPHONE_DEPRECATED LinphoneCoreFileTransferProgressIndicationCb
	    file_transfer_progress_indication;            /**< @brief Callback to indicate file transfer progress.
	                                                       @deprecated 19/11/2015 */
	LinphoneCoreNetworkReachableCb network_reachable; /**< Callback to report IP network status (I.E up/down )*/
	LinphoneCoreLogCollectionUploadStateChangedCb
	    log_collection_upload_state_changed; /**< Callback to upload collected logs */
	LinphoneCoreLogCollectionUploadProgressIndicationCb
	    log_collection_upload_progress_indication; /**< Callback to indicate log collection upload progress */
	LinphoneCoreFriendListCreatedCb friend_list_created;
	LinphoneCoreFriendListRemovedCb friend_list_removed;
	LinphoneCoreCbsCallCreatedCb call_created;
	LinphoneCoreCbsVersionUpdateCheckResultReceivedCb version_update_check_result_received;
	LinphoneCoreCbsConferenceStateChangedCb conference_state_changed;
	LinphoneCoreCbsChatRoomStateChangedCb chat_room_state_changed;
	LinphoneCoreCbsQrcodeFoundCb qrcode_found;
	LinphoneCoreCbsEcCalibrationResultCb ec_calibration_result;
	LinphoneCoreCbsEcCalibrationAudioInitCb ec_calibration_audio_init;
	LinphoneCoreCbsEcCalibrationAudioUninitCb ec_calibration_audio_uninit;
	LinphoneCoreCbsMessageSentCb message_sent;
	LinphoneCoreCbsChatRoomReadCb chat_room_read;
	LinphoneCoreCbsChatRoomSessionStateChangedCb chat_room_session_state_changed;
	LinphoneCoreCbsChatRoomSubjectChangedCb chat_room_subject_changed;
	LinphoneCoreCbsChatRoomEphemeralMessageDeleteCb chat_room_ephemeral_message_deleted;
	LinphoneCoreCbsFirstCallStartedCb first_call_started;
	LinphoneCoreCbsLastCallEndedCb last_call_ended;
	LinphoneCoreCbsAudioDeviceChangedCb audio_device_changed;
	LinphoneCoreCbsAudioDevicesListUpdatedCb audio_devices_list_updated;
	LinphoneCoreCbsImeeUserRegistrationCb imee_user_registration;
	LinphoneCoreCbsChatRoomExhumedCb chat_room_exhumed;
	LinphoneCoreCbsAccountRegistrationStateChangedCb account_registration_state_changed;
	LinphoneCoreCbsConferenceInfoReceivedCb conference_info_received;
	LinphoneCoreCbsPushNotificationReceivedCb push_notification_received;
	LinphoneCoreCbsNewAlertTriggeredCb new_alert_triggered;
	LinphoneCoreCbsPreviewDisplayErrorOccurredCb preview_display_error_occurred;
	LinphoneCoreCbsNewMessageReactionCb new_message_reaction;
	LinphoneCoreCbsReactionRemovedCb reaction_removed;
	LinphoneCoreCbsReactionRemovedPrivateCb reaction_removed_private;
	LinphoneCoreCbsDefaultAccountChangedCb default_account_changed;
	LinphoneCoreCbsAccountAddedCb account_added;
	LinphoneCoreCbsAccountRemovedCb account_removed;
	LinphoneCoreCbsMessageWaitingIndicationChangedCb message_waiting_indication_changed;
	LinphoneCoreCbsSnapshotTakenCb snapshot_taken;
	void *user_data; /**<User data associated with the above callbacks */
} LinphoneCoreVTable;

/**
 * @brief Instantiate a vtable with all arguments set to NULL.
 * @return newly allocated vtable.
 * @donotwrap
 */
LINPHONE_PUBLIC LinphoneCoreVTable *linphone_core_v_table_new(void);

/**
 * @brief Sets a user data pointer in the vtable.
 * @param table the vtable.
 * @param data the user data to attach.
 * @donotwrap
 */
LINPHONE_PUBLIC void linphone_core_v_table_set_user_data(LinphoneCoreVTable *table, void *data);

/**
 * @brief Gets a user data pointer in the vtable.
 * @param table the vtable.
 * @return the data attached to the vtable.
 * @donotwrap
 */
LINPHONE_PUBLIC void *linphone_core_v_table_get_user_data(const LinphoneCoreVTable *table);

/**
 * Gets the current VTable.
 * This is meant only to be called from a callback to be able to get the user_data associated with the vtable that
 * called the callback.
 * @param core the linphonecore
 * @return the vtable that called the last callback
 * @donotwrap
 */
LINPHONE_PUBLIC LinphoneCoreVTable *linphone_core_get_current_vtable(LinphoneCore *core);

/**
 * @brief Destroy a vtable.
 * @param table to be destroyed.
 * @donotwrap
 */
LINPHONE_PUBLIC void linphone_core_v_table_destroy(LinphoneCoreVTable *table);

/**
 * Increment the reference counter.
 * @param cbs the #LinphoneCoreCbs object @notnil
 * @return the same #LinphoneCoreCbs object @notnil
 */
LINPHONE_PUBLIC LinphoneCoreCbs *linphone_core_cbs_ref(LinphoneCoreCbs *cbs);

/**
 * Decrement the reference counter.
 * @param cbs the #LinphoneCoreCbs object @notnil
 */
LINPHONE_PUBLIC void linphone_core_cbs_unref(LinphoneCoreCbs *cbs);

/**
 * Set private data to be get from each callbacks.
 * @param cbs the #LinphoneCoreCbs object @notnil
 * @param user_data the user data pointer. @maybenil
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_user_data(LinphoneCoreCbs *cbs, void *user_data);

/**
 * Get the user pointer.
 * @param cbs the #LinphoneCoreCbs object @notnil
 * @return the user data pointer. @maybenil
 */
LINPHONE_PUBLIC void *linphone_core_cbs_get_user_data(const LinphoneCoreCbs *cbs);

/**
 * Gets the current #LinphoneCoreCbs being invoked, if any.
 * This is meant only to be called from a callback to be able to get the user_data associated with the #LinphoneCoreCbs
 * that is calling the callback.
 * @param core the #LinphoneCore @notnil
 * @return the #LinphoneCoreCbs that has called the last callback @maybenil
 */
LINPHONE_PUBLIC LinphoneCoreCbs *linphone_core_get_current_callbacks(const LinphoneCore *core);

/**
 * Set the #LinphoneCoreGlobalStateChangedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_global_state_changed(LinphoneCoreCbs *cbs,
                                                                LinphoneCoreCbsGlobalStateChangedCb cb);

/**
 * Get the global state changed callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsGlobalStateChangedCb linphone_core_cbs_get_global_state_changed(LinphoneCoreCbs *cbs);

/**
 * Set the #LinphoneCoreCbsRegistrationStateChangedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 * @deprecated 30/09/2020. see linphone_account_cbs_set_registration_state_changed()
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_registration_state_changed(LinphoneCoreCbs *cbs,
                                                                      LinphoneCoreCbsRegistrationStateChangedCb cb);

/**
 * Get the #LinphoneCoreCbsRegistrationStateChangedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 * @deprecated 30/09/2020. see linphone_account_cbs_get_registration_state_changed()
 */
LINPHONE_PUBLIC LinphoneCoreCbsRegistrationStateChangedCb
linphone_core_cbs_get_registration_state_changed(LinphoneCoreCbs *cbs);

/**
 * Sets the conference info received callback.
 * @param cbs #LinphoneCoreCbs object. @notnil
 * @param cb The conference info received callback to be used.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_conference_info_received(LinphoneCoreCbs *cbs,
                                                                    LinphoneCoreCbsConferenceInfoReceivedCb cb);

/**
 * Get the conference info received callback.
 * @param cbs #LinphoneCoreCbs object. @notnil
 * @return The current conference info received callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsConferenceInfoReceivedCb
linphone_core_cbs_get_conference_info_received(LinphoneCoreCbs *cbs);

/**
 * Set the push notification received callback.
 * @param cbs #LinphoneCoreCbs object. @notnil
 * @param cb The push notification received callback to be used.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_push_notification_received(LinphoneCoreCbs *cbs,
                                                                      LinphoneCoreCbsPushNotificationReceivedCb cb);

/**
 * Get the push notification received callback.
 * @param cbs #LinphoneCoreCbs object. @notnil
 * @return The current push notification received callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsPushNotificationReceivedCb
linphone_core_cbs_get_push_notification_received(LinphoneCoreCbs *cbs);

/**
 * @brief Get the preview display error callback for preview.
 * @param cbs LinphoneCoreCbs object @notnil
 * @return The callback to use
 */
LINPHONE_PUBLIC LinphoneCoreCbsPreviewDisplayErrorOccurredCb
linphone_core_cbs_get_preview_display_error_occurred(const LinphoneCoreCbs *cbs);

/**
 * @brief Set the preview display error callback for preview.
 * @param cbs LinphoneCoreCbs object @notnil
 * @param cb The callback to use
 */
LINPHONE_PUBLIC void
linphone_core_cbs_set_preview_display_error_occurred(LinphoneCoreCbs *cbs,
                                                     LinphoneCoreCbsPreviewDisplayErrorOccurredCb cb);

/**
 * Set the #LinphoneCoreCbsCallStateChangedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_call_state_changed(LinphoneCoreCbs *cbs,
                                                              LinphoneCoreCbsCallStateChangedCb cb);

/**
 * Get the #LinphoneCoreCbsCallStateChangedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsCallStateChangedCb linphone_core_cbs_get_call_state_changed(LinphoneCoreCbs *cbs);

/**
 * Set the #LinphoneCoreCbsNotifyPresenceReceivedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_notify_presence_received(LinphoneCoreCbs *cbs,
                                                                    LinphoneCoreCbsNotifyPresenceReceivedCb cb);

/**
 * Get the #LinphoneCoreCbsNotifyPresenceReceivedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsNotifyPresenceReceivedCb
linphone_core_cbs_get_notify_presence_received(LinphoneCoreCbs *cbs);

/**
 * Set the #LinphoneCoreCbsNotifyPresenceReceivedForUriOrTelCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void
linphone_core_cbs_set_notify_presence_received_for_uri_or_tel(LinphoneCoreCbs *cbs,
                                                              LinphoneCoreCbsNotifyPresenceReceivedForUriOrTelCb cb);

/**
 * Get the #LinphoneCoreCbsNotifyPresenceReceivedForUriOrTelCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsNotifyPresenceReceivedForUriOrTelCb
linphone_core_cbs_get_notify_presence_received_for_uri_or_tel(LinphoneCoreCbs *cbs);

/**
 * Set the #LinphoneCoreCbsNewSubscriptionRequestedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_new_subscription_requested(LinphoneCoreCbs *cbs,
                                                                      LinphoneCoreCbsNewSubscriptionRequestedCb cb);

/**
 * Get the #LinphoneCoreCbsNewSubscriptionRequestedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsNewSubscriptionRequestedCb
linphone_core_cbs_get_new_subscription_requested(LinphoneCoreCbs *cbs);

/**
 * Set the #LinphoneCoreCbsAuthenticationRequestedCb callback.'
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_authentication_requested(LinphoneCoreCbs *cbs,
                                                                    LinphoneCoreCbsAuthenticationRequestedCb cb);

/**
 * Get the #LinphoneCoreCbsAuthenticationRequestedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsAuthenticationRequestedCb
linphone_core_cbs_get_authentication_requested(LinphoneCoreCbs *cbs);

/**
 * Set the #LinphoneCoreCbsCallLogUpdatedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_call_log_updated(LinphoneCoreCbs *cbs, LinphoneCoreCbsCallLogUpdatedCb cb);

/**
 * Get the #LinphoneCoreCbsCallLogUpdatedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsCallLogUpdatedCb linphone_core_cbs_get_call_log_updated(LinphoneCoreCbs *cbs);

/**
 * Set the #LinphoneCoreCbsCallIdUpdatedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_call_id_updated(LinphoneCoreCbs *cbs, LinphoneCoreCbsCallIdUpdatedCb cb);

/**
 * Get the #LinphoneCoreCbsCallIdUpdatedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsCallIdUpdatedCb linphone_core_cbs_get_call_id_updated(LinphoneCoreCbs *cbs);

/**
 * Set the #LinphoneCoreCbsRemainingNumberOfFileTransferChangedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_remaining_number_of_file_transfer_changed(
    LinphoneCoreCbs *cbs, LinphoneCoreCbsRemainingNumberOfFileTransferChangedCb cb);

/**
 * Get the #LinphoneCoreCbsRemainingNumberOfFileTransferChangedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsRemainingNumberOfFileTransferChangedCb
linphone_core_cbs_get_remaining_number_of_file_transfer_changed(LinphoneCoreCbs *cbs);

/**
 * Set the #LinphoneCoreCbsMessageReceivedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_message_received(LinphoneCoreCbs *cbs, LinphoneCoreCbsMessageReceivedCb cb);

/**
 * Get the #LinphoneCoreCbsMessageReceivedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsMessageReceivedCb linphone_core_cbs_get_message_received(LinphoneCoreCbs *cbs);

/**
 * Set the #LinphoneCoreCbsNewMessageReactionCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_new_message_reaction(LinphoneCoreCbs *cbs,
                                                                LinphoneCoreCbsNewMessageReactionCb cb);

void linphone_core_cbs_set_reaction_removed_private(LinphoneCoreCbs *cbs, LinphoneCoreCbsReactionRemovedPrivateCb cb);

/**
 * Get the #LinphoneCoreCbsNewMessageReactionCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsNewMessageReactionCb linphone_core_cbs_get_new_message_reaction(LinphoneCoreCbs *cbs);

/**
 * Set the #LinphoneCoreCbsReactionRemovedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_reaction_removed(LinphoneCoreCbs *cbs, LinphoneCoreCbsReactionRemovedCb cb);

/**
 * Get the #LinphoneCoreCbsReactionRemovedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsReactionRemovedCb linphone_core_cbs_get_reaction_removed(LinphoneCoreCbs *cbs);

/**
 * Set the #LinphoneCoreCbsMessagesReceivedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_messages_received(LinphoneCoreCbs *cbs,
                                                             LinphoneCoreCbsMessagesReceivedCb cb);

/**
 * Get the #LinphoneCoreCbsMessagesReceivedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsMessagesReceivedCb linphone_core_cbs_get_messages_received(LinphoneCoreCbs *cbs);

/**
 * Set the #LinphoneCoreCbsMessagesReceivedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_messages_received(LinphoneCoreCbs *cbs,
                                                             LinphoneCoreCbsMessagesReceivedCb cb);

/**
 * Get the #LinphoneCoreCbsMessagesReceivedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsMessagesReceivedCb linphone_core_cbs_get_messages_received(LinphoneCoreCbs *cbs);

/**
 * Set the #LinphoneCoreCbsMessageSentCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_message_sent(LinphoneCoreCbs *cbs, LinphoneCoreCbsMessageSentCb cb);

/**
 * Get the #LinphoneCoreCbsMessageSentCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsMessageSentCb linphone_core_cbs_get_message_sent(LinphoneCoreCbs *cbs);

/**
 * Set the #LinphoneCoreCbsChatRoomSessionStateChangedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void
linphone_core_cbs_set_chat_room_session_state_changed(LinphoneCoreCbs *cbs,
                                                      LinphoneCoreCbsChatRoomSessionStateChangedCb cb);

/**
 * Get the #LinphoneCoreCbsChatRoomSessionStateChangedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsChatRoomSessionStateChangedCb
linphone_core_cbs_get_chat_room_session_state_changed(LinphoneCoreCbs *cbs);

/**
 * Set the #LinphoneCoreCbsChatRoomReadCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_chat_room_read(LinphoneCoreCbs *cbs, LinphoneCoreCbsChatRoomReadCb cb);

/**
 * Get the #LinphoneCoreCbsChatRoomReadCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsChatRoomReadCb linphone_core_cbs_get_chat_room_read(LinphoneCoreCbs *cbs);

/**
 * Set the #LinphoneCoreCbsMessageReceivedUnableDecryptCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void
linphone_core_cbs_set_message_received_unable_decrypt(LinphoneCoreCbs *cbs,
                                                      LinphoneCoreCbsMessageReceivedUnableDecryptCb cb);

/**
 * Get the #LinphoneCoreCbsMessageReceivedUnableDecryptCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsMessageReceivedUnableDecryptCb
linphone_core_cbs_get_message_received_unable_decrypt(LinphoneCoreCbs *cbs);

/**
 * Set the #LinphoneCoreCbsIsComposingReceivedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_is_composing_received(LinphoneCoreCbs *cbs,
                                                                 LinphoneCoreCbsIsComposingReceivedCb cb);

/**
 * Get the #LinphoneCoreCbsIsComposingReceivedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsIsComposingReceivedCb linphone_core_cbs_get_is_composing_received(LinphoneCoreCbs *cbs);

/**
 * Set the #LinphoneCoreCbsDtmfReceivedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_dtmf_received(LinphoneCoreCbs *cbs, LinphoneCoreCbsDtmfReceivedCb cb);

/**
 * Get the #LinphoneCoreCbsDtmfReceivedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsDtmfReceivedCb linphone_core_cbs_get_dtmf_received(LinphoneCoreCbs *cbs);

/**
 * Set the #LinphoneCoreCbsReferReceivedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_refer_received(LinphoneCoreCbs *cbs, LinphoneCoreCbsReferReceivedCb cb);

/**
 * Get the #LinphoneCoreCbsReferReceivedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsReferReceivedCb linphone_core_cbs_get_refer_received(LinphoneCoreCbs *cbs);

/**
 * Sets the #LinphoneCoreCbsCallGoClearAckSentCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_call_goclear_ack_sent(LinphoneCoreCbs *cbs,
                                                                 LinphoneCoreCbsCallGoClearAckSentCb cb);

/**
 * Gets the #LinphoneCoreCbsCallGoClearAckSentCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsCallGoClearAckSentCb linphone_core_cbs_get_call_goclear_ack_sent(LinphoneCoreCbs *cbs);

/**
 * Sets the #LinphoneCoreCbsCallEncryptionChangedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_call_encryption_changed(LinphoneCoreCbs *cbs,
                                                                   LinphoneCoreCbsCallEncryptionChangedCb cb);

/**
 * Gets the #LinphoneCoreCbsCallEncryptionChangedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsCallEncryptionChangedCb
linphone_core_cbs_get_call_encryption_changed(LinphoneCoreCbs *cbs);

/**
 * Sets the #LinphoneCoreCbsCallSendMasterKeyChangedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_call_send_master_key_changed(LinphoneCoreCbs *cbs,
                                                                        LinphoneCoreCbsCallSendMasterKeyChangedCb cb);

/**
 * Gets the #LinphoneCoreCbsCallSendMasterKeyChangedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsCallSendMasterKeyChangedCb
linphone_core_cbs_get_call_send_master_key_changed(LinphoneCoreCbs *cbs);

/**
 * Sets the #LinphoneCoreCbsCallReceiveMasterKeyChangedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void
linphone_core_cbs_set_call_receive_master_key_changed(LinphoneCoreCbs *cbs,
                                                      LinphoneCoreCbsCallReceiveMasterKeyChangedCb cb);

/**
 * Gets the #LinphoneCoreCbsCallReceiveMasterKeyChangedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsCallReceiveMasterKeyChangedCb
linphone_core_cbs_get_call_receive_master_key_changed(LinphoneCoreCbs *cbs);

/**
 * Sets the #LinphoneCoreCbsTransferStateChangedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_transfer_state_changed(LinphoneCoreCbs *cbs,
                                                                  LinphoneCoreCbsTransferStateChangedCb cb);

/**
 * Gets the #LinphoneCoreCbsTransferStateChangedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsTransferStateChangedCb
linphone_core_cbs_get_transfer_state_changed(LinphoneCoreCbs *cbs);

/**
 * Sets the #LinphoneCoreCbsBuddyInfoUpdatedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_buddy_info_updated(LinphoneCoreCbs *cbs,
                                                              LinphoneCoreCbsBuddyInfoUpdatedCb cb);

/**
 * Gets the #LinphoneCoreCbsBuddyInfoUpdatedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsBuddyInfoUpdatedCb linphone_core_cbs_get_buddy_info_updated(LinphoneCoreCbs *cbs);

/**
 * Sets the #LinphoneCoreCbsCallStatsUpdatedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_call_stats_updated(LinphoneCoreCbs *cbs,
                                                              LinphoneCoreCbsCallStatsUpdatedCb cb);

/**
 * Gets the #LinphoneCoreCbsCallStatsUpdatedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsCallStatsUpdatedCb linphone_core_cbs_get_call_stats_updated(LinphoneCoreCbs *cbs);

/**
 * Sets the #LinphoneCoreCbsInfoReceivedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_info_received(LinphoneCoreCbs *cbs, LinphoneCoreCbsInfoReceivedCb cb);

/**
 * Gets the #LinphoneCoreCbsInfoReceivedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsInfoReceivedCb linphone_core_cbs_get_info_received(LinphoneCoreCbs *cbs);

/**
 * Sets the #LinphoneCoreCbsSubscriptionStateChangedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_subscription_state_changed(LinphoneCoreCbs *cbs,
                                                                      LinphoneCoreCbsSubscriptionStateChangedCb cb);

/**
 * Gets the #LinphoneCoreCbsSubscriptionStateChangedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsSubscriptionStateChangedCb
linphone_core_cbs_get_subscription_state_changed(LinphoneCoreCbs *cbs);

/**
 * Sets the #LinphoneCoreCbsNotifySentCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_notify_sent(LinphoneCoreCbs *cbs, LinphoneCoreCbsNotifySentCb cb);

/**
 * Gets the #LinphoneCoreCbsNotifySentCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsNotifySentCb linphone_core_cbs_get_notify_sent(LinphoneCoreCbs *cbs);

/**
 * Sets the #LinphoneCoreCbsNotifyReceivedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_notify_received(LinphoneCoreCbs *cbs, LinphoneCoreCbsNotifyReceivedCb cb);

/**
 * Gets the #LinphoneCoreCbsNotifyReceivedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsNotifyReceivedCb linphone_core_cbs_get_notify_received(LinphoneCoreCbs *cbs);

/**
 * Sets the #LinphoneCoreCbsSubscribeReceivedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_subscribe_received(LinphoneCoreCbs *cbs,
                                                              LinphoneCoreCbsSubscribeReceivedCb cb);

/**
 * Gets the #LinphoneCoreCbsSubscribeReceivedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsSubscribeReceivedCb linphone_core_cbs_get_subscribe_received(LinphoneCoreCbs *cbs);

/**
 * Sets the #LinphoneCoreCbsPublishStateChangedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_publish_state_changed(LinphoneCoreCbs *cbs,
                                                                 LinphoneCoreCbsPublishStateChangedCb cb);

/**
 * Gets the #LinphoneCoreCbsPublishStateChangedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsPublishStateChangedCb linphone_core_cbs_get_publish_state_changed(LinphoneCoreCbs *cbs);

/**
 * Sets the #LinphoneCoreCbsPublishReceivedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_publish_received(LinphoneCoreCbs *cbs, LinphoneCoreCbsPublishReceivedCb cb);

/**
 * Gets the #LinphoneCoreCbsPublishReceivedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsPublishReceivedCb linphone_core_cbs_get_publish_received(LinphoneCoreCbs *cbs);

/**
 * Sets the #LinphoneCoreCbsConfiguringStatusCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_configuring_status(LinphoneCoreCbs *cbs,
                                                              LinphoneCoreCbsConfiguringStatusCb cb);

/**
 * Gets the #LinphoneCoreCbsConfiguringStatusCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsConfiguringStatusCb linphone_core_cbs_get_configuring_status(LinphoneCoreCbs *cbs);

/**
 * Sets the #LinphoneCoreCbsNetworkReachableCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_network_reachable(LinphoneCoreCbs *cbs,
                                                             LinphoneCoreCbsNetworkReachableCb cb);

/**
 * Gets the #LinphoneCoreCbsNetworkReachableCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsNetworkReachableCb linphone_core_cbs_get_network_reachable(LinphoneCoreCbs *cbs);

/**
 * Sets the #LinphoneCoreCbsLogCollectionUploadStateChangedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void
linphone_core_cbs_set_log_collection_upload_state_changed(LinphoneCoreCbs *cbs,
                                                          LinphoneCoreCbsLogCollectionUploadStateChangedCb cb);

/**
 * Gets the #LinphoneCoreCbsLogCollectionUploadStateChangedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsLogCollectionUploadStateChangedCb
linphone_core_cbs_get_log_collection_upload_state_changed(LinphoneCoreCbs *cbs);

/**
 * Sets the #LinphoneCoreCbsLogCollectionUploadProgressIndicationCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_log_collection_upload_progress_indication(
    LinphoneCoreCbs *cbs, LinphoneCoreCbsLogCollectionUploadProgressIndicationCb cb);

/**
 * Gets the #LinphoneCoreCbsLogCollectionUploadProgressIndicationCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsLogCollectionUploadProgressIndicationCb
linphone_core_cbs_get_log_collection_upload_progress_indication(LinphoneCoreCbs *cbs);

/**
 * Sets the #LinphoneCoreCbsFriendListCreatedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_friend_list_created(LinphoneCoreCbs *cbs,
                                                               LinphoneCoreCbsFriendListCreatedCb cb);

/**
 * Gets the #LinphoneCoreCbsFriendListCreatedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsFriendListCreatedCb linphone_core_cbs_get_friend_list_created(LinphoneCoreCbs *cbs);

/**
 * Sets the #LinphoneCoreCbsFriendListRemovedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @param cb The callback.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_friend_list_removed(LinphoneCoreCbs *cbs,
                                                               LinphoneCoreCbsFriendListRemovedCb cb);

/**
 * Gets the #LinphoneCoreCbsFriendListRemovedCb callback.
 * @param cbs A #LinphoneCoreCbs. @notnil
 * @return The callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsFriendListRemovedCb linphone_core_cbs_get_friend_list_removed(LinphoneCoreCbs *cbs);

/**
 * Sets the call created callback.
 * @param cbs #LinphoneCallCbs object. @notnil
 * @param cb The call created callback to be used.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_call_created(LinphoneCoreCbs *cbs, LinphoneCoreCbsCallCreatedCb cb);

/**
 * Gets the call created callback.
 * @param cbs #LinphoneCoreCbs object. @notnil
 * @return The current call created callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsCallCreatedCb linphone_core_cbs_get_call_created(LinphoneCoreCbs *cbs);

/**
 * Sets the version update check result callback.
 * @param cbs #LinphoneCoreCbs object @notnil
 * @param cb The callback to use
 */
LINPHONE_PUBLIC void
linphone_core_cbs_set_version_update_check_result_received(LinphoneCoreCbs *cbs,
                                                           LinphoneCoreCbsVersionUpdateCheckResultReceivedCb cb);

/**
 * Gets the version update check result callback.
 * @param cbs #LinphoneCoreCbs object @notnil
 * @return The current callback
 */
LINPHONE_PUBLIC LinphoneCoreCbsVersionUpdateCheckResultReceivedCb
linphone_core_cbs_get_version_update_check_result_received(LinphoneCoreCbs *cbs);

/**
 * Gets the conference state changed callback.
 * @param[in] cbs #LinphoneCoreCbs object
 * @return The current callback
 */
LINPHONE_PUBLIC LinphoneCoreCbsConferenceStateChangedCb
linphone_core_cbs_get_conference_state_changed(LinphoneCoreCbs *cbs);

/**
 * Sets the conference state changed callback.
 * @param[in] cbs #LinphoneCoreCbs object
 * @param[in] cb The callback to use
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_conference_state_changed(LinphoneCoreCbs *cbs,
                                                                    LinphoneCoreCbsConferenceStateChangedCb cb);

/**
 * Gets the chat room state changed callback.
 * @param cbs #LinphoneCoreCbs object @notnil
 * @return The current callback
 */
LINPHONE_PUBLIC LinphoneCoreCbsChatRoomStateChangedCb
linphone_core_cbs_get_chat_room_state_changed(LinphoneCoreCbs *cbs);

/**
 * Sets the chat room state changed callback.
 * @param cbs #LinphoneCoreCbs object @notnil
 * @param cb The callback to use
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_chat_room_state_changed(LinphoneCoreCbs *cbs,
                                                                   LinphoneCoreCbsChatRoomStateChangedCb cb);

/**
 * Gets the chat room subject changed callback.
 * @param cbs #LinphoneCoreCbs object @notnil
 * @return The current callback
 */
LINPHONE_PUBLIC LinphoneCoreCbsChatRoomSubjectChangedCb
linphone_core_cbs_get_chat_room_subject_changed(LinphoneCoreCbs *cbs);

/**
 * Sets the chat room subject changed callback.
 * @param cbs #LinphoneCoreCbs object @notnil
 * @param cb The callback to use
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_chat_room_subject_changed(LinphoneCoreCbs *cbs,
                                                                     LinphoneCoreCbsChatRoomSubjectChangedCb cb);

/**
 * Gets the chat room ephemeral message deleted callback.
 * @param cbs #LinphoneCoreCbs object @notnil
 * @return The current callback
 */
LINPHONE_PUBLIC LinphoneCoreCbsChatRoomEphemeralMessageDeleteCb
linphone_core_cbs_get_chat_room_ephemeral_message_deleted(LinphoneCoreCbs *cbs);

/**
 * Sets the chat room ephemeral message deleted callback.
 * @param cbs #LinphoneCoreCbs object @notnil
 * @param cb The callback to use
 */
LINPHONE_PUBLIC void
linphone_core_cbs_set_chat_room_ephemeral_message_deleted(LinphoneCoreCbs *cbs,
                                                          LinphoneCoreCbsChatRoomEphemeralMessageDeleteCb cb);

/**
 * Gets the IMEE user registration callback
 * @param cbs #LinphoneCoreCbs object @notnil
 * @return The current callback
 */
LINPHONE_PUBLIC LinphoneCoreCbsImeeUserRegistrationCb
linphone_core_cbs_get_imee_user_registration(LinphoneCoreCbs *cbs);

/**
 * Sets the IMEE user registration callback.
 * @param cbs #LinphoneCoreCbs object @notnil
 * @param cb The callback to use
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_imee_user_registration(LinphoneCoreCbs *cbs,
                                                                  LinphoneCoreCbsImeeUserRegistrationCb cb);

/**
 * Gets the qrcode found callback.
 * @param cbs LinphoneCoreCbs object @notnil
 * @return The current callback
 */
LINPHONE_PUBLIC LinphoneCoreCbsQrcodeFoundCb linphone_core_cbs_get_qrcode_found(LinphoneCoreCbs *cbs);

/**
 * Sets the qrcode found callback.
 * @param cbs LinphoneCoreCbs object @notnil
 * @param cb The callback to use
 **/
LINPHONE_PUBLIC void linphone_core_cbs_set_qrcode_found(LinphoneCoreCbs *cbs, LinphoneCoreCbsQrcodeFoundCb cb);

/**
 * Gets the first call started callback.
 * @param cbs LinphoneCoreCbs object @notnil
 * @return The current callback
 */
LINPHONE_PUBLIC LinphoneCoreCbsFirstCallStartedCb linphone_core_cbs_get_first_call_started(LinphoneCoreCbs *cbs);

/**
 * Sets the first call started callback.
 * @param cbs LinphoneCoreCbs object @notnil
 * @param cb The callback to use
 **/
LINPHONE_PUBLIC void linphone_core_cbs_set_first_call_started(LinphoneCoreCbs *cbs,
                                                              LinphoneCoreCbsFirstCallStartedCb cb);

/**
 * Gets the last call ended callback.
 * @param cbs LinphoneCoreCbs object @notnil
 * @return The current callback
 */
LINPHONE_PUBLIC LinphoneCoreCbsLastCallEndedCb linphone_core_cbs_get_last_call_ended(LinphoneCoreCbs *cbs);

/**
 * Sets the last call ended callback.
 * @param cbs LinphoneCoreCbs object @notnil
 * @param cb The callback to use
 **/
LINPHONE_PUBLIC void linphone_core_cbs_set_last_call_ended(LinphoneCoreCbs *cbs, LinphoneCoreCbsLastCallEndedCb cb);

/**
 * Gets the audio device changed callback.
 * @param cbs LinphoneCoreCbs object @notnil
 * @return The current callback
 */
LINPHONE_PUBLIC LinphoneCoreCbsAudioDeviceChangedCb linphone_core_cbs_get_audio_device_changed(LinphoneCoreCbs *cbs);

/**
 * Sets the audio device changed callback.
 * @param cbs LinphoneCoreCbs object @notnil
 * @param cb The callback to use
 **/
LINPHONE_PUBLIC void linphone_core_cbs_set_audio_device_changed(LinphoneCoreCbs *cbs,
                                                                LinphoneCoreCbsAudioDeviceChangedCb cb);

/**
 * Gets the audio devices list updated callback.
 * @param cbs LinphoneCoreCbs object @notnil
 * @return The current callback
 */
LINPHONE_PUBLIC LinphoneCoreCbsAudioDevicesListUpdatedCb
linphone_core_cbs_get_audio_devices_list_updated(LinphoneCoreCbs *cbs);

/**
 * Sets the audio devices list updated callback.
 * @param cbs LinphoneCoreCbs object @notnil
 * @param cb The callback to use
 **/
LINPHONE_PUBLIC void linphone_core_cbs_set_audio_devices_list_updated(LinphoneCoreCbs *cbs,
                                                                      LinphoneCoreCbsAudioDevicesListUpdatedCb cb);

/**
 * @brief Sets a callback to call each time the echo-canceler calibration is completed.
 * @param cbs LinphoneCoreCbs object @notnil
 * @param cb The callback to use
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_ec_calibration_result(LinphoneCoreCbs *cbs,
                                                                 LinphoneCoreCbsEcCalibrationResultCb cb);

/**
 * @brief Sets a callback to call when the echo-canceler calibrator has completed its audio graph.
 * @param cbs LinphoneCoreCbs object @notnil
 * @param cb The callback to use
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_ec_calibration_audio_init(LinphoneCoreCbs *cbs,
                                                                     LinphoneCoreCbsEcCalibrationAudioInitCb cb);

/**
 * @brief Sets a callback to call when the echo-canceler calibrator destroys its audio graph.
 * @param cbs LinphoneCoreCbs object @notnil
 * @param cb The callback to use
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_ec_calibration_audio_uninit(LinphoneCoreCbs *cbs,
                                                                       LinphoneCoreCbsEcCalibrationAudioUninitCb cb);

/**
 * @brief Sets a callback to call when a chat room is exhumed. Internal use only!
 * @param cbs LinphoneCoreCbs object @notnil
 * @return The callback to use
 * @donotwrap
 */
LINPHONE_PUBLIC LinphoneCoreCbsChatRoomExhumedCb linphone_core_cbs_get_chat_room_exhumed(LinphoneCoreCbs *cbs);

/**
 * @brief Sets a callback to call when a chat room is exhumed. Internal use only!
 * @param cbs LinphoneCoreCbs object @notnil
 * @param cb The callback to use
 * @donotwrap
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_chat_room_exhumed(LinphoneCoreCbs *cbs, LinphoneCoreCbsChatRoomExhumedCb cb);

/**
 * Sets the account registration state changed callback.
 * @param cbs #LinphoneCoreCbs object. @notnil
 * @param cb The account registration state changed callback to be used.
 */
LINPHONE_PUBLIC void
linphone_core_cbs_set_account_registration_state_changed(LinphoneCoreCbs *cbs,
                                                         LinphoneCoreCbsAccountRegistrationStateChangedCb cb);
/**
 * Sets the default account changed callback.
 * @param cbs #LinphoneCoreCbs object. @notnil
 * @param cb The default account changed callback to be used.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_default_account_changed(LinphoneCoreCbs *cbs,
                                                                   LinphoneCoreCbsDefaultAccountChangedCb cb);

/**
 * Gets the default account changed callback.
 * @param cbs #LinphoneCoreCbs object. @notnil
 * @return The default account changed callback that will be triggered.
 */
LINPHONE_PUBLIC LinphoneCoreCbsDefaultAccountChangedCb
linphone_core_cbs_get_default_account_changed(LinphoneCoreCbs *cbs);

/**
 * Sets the account added callback.
 * @param cbs #LinphoneCoreCbs object. @notnil
 * @param cb The account added callback to be used.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_account_added(LinphoneCoreCbs *cbs, LinphoneCoreCbsAccountAddedCb cb);

/**
 * Gets the account added callback.
 * @param cbs #LinphoneCoreCbs object. @notnil
 * @return The account added callback that will be triggered.
 */
LINPHONE_PUBLIC LinphoneCoreCbsAccountAddedCb linphone_core_cbs_get_account_added(LinphoneCoreCbs *cbs);

/**
 * Sets the account removed callback.
 * @param cbs #LinphoneCoreCbs object. @notnil
 * @param cb The new account removed callback to be used.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_account_removed(LinphoneCoreCbs *cbs, LinphoneCoreCbsAccountRemovedCb cb);

/**
 * Gets the new account removed callback.
 * @param cbs #LinphoneCoreCbs object. @notnil
 * @return The new account removed callback that will be triggered.
 */
LINPHONE_PUBLIC LinphoneCoreCbsAccountRemovedCb linphone_core_cbs_get_account_removed(LinphoneCoreCbs *cbs);

/**
 * Gets the account registration state changed callback.
 * @param cbs #LinphoneCoreCbs object. @notnil
 * @return The current account registration state changed callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsAccountRegistrationStateChangedCb
linphone_core_cbs_get_account_registration_state_changed(LinphoneCoreCbs *cbs);

/**
 * Sets the message waiting indication changed callback.
 * @param cbs #LinphoneCoreCbs object. @notnil
 * @param cb The message waiting indication changed callback to be used.
 */
LINPHONE_PUBLIC void
linphone_core_cbs_set_message_waiting_indication_changed(LinphoneCoreCbs *cbs,
                                                         LinphoneCoreCbsMessageWaitingIndicationChangedCb cb);

/**
 * Gets the message waiting indication changed callback.
 * @param cbs #LinphoneCoreCbs object. @notnil
 * @return The current message waiting indication changed callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsMessageWaitingIndicationChangedCb
linphone_core_cbs_get_message_waiting_indication_changed(LinphoneCoreCbs *cbs);

/**
 * Gets the snapshot taken callback.
 * @param cbs The #LinphoneCoreCbs object. @notnil
 * @return The current snapshot taken callback.
 */
LINPHONE_PUBLIC LinphoneCoreCbsSnapshotTakenCb linphone_core_cbs_get_snapshot_taken(LinphoneCoreCbs *cbs);

/**
 * Sets the snapshot taken callback.
 * @param cbs The #LinphoneCoreCbs object. @notnil
 * @param cb The snapshot taken callback to be used.
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_snapshot_taken(LinphoneCoreCbs *cbs, LinphoneCoreCbsSnapshotTakenCb cb);

/**
 * @}
 **/

/**
 * @addtogroup initializing
 * @{
 **/

/**
 * Tells whether the linphone core log collection is enabled.
 * @return The #LinphoneLogCollectionState of the #LinphoneCore log collection.
 */
LINPHONE_PUBLIC LinphoneLogCollectionState linphone_core_log_collection_enabled(void);

/**
 * Enables the linphone core log collection to upload logs on a server.
 * @param state #LinphoneLogCollectionState value telling whether to enable log collection or not.
 */
LINPHONE_PUBLIC void linphone_core_enable_log_collection(LinphoneLogCollectionState state);

/**
 * Gets the path where the log files will be written for log collection.
 * @return The path where the log files will be written. @notnil
 */
LINPHONE_PUBLIC const char *linphone_core_get_log_collection_path(void);

/**
 * Sets the path of a directory where the log files will be written for log collection.
 * When log collection is enabled, the function will close the file with the current prefix in the old path and it will
 * open the new file with current prefix in the new path. If you need to change the path and the file at the same time,
 * then you should deactivate log collection with linphone_core_enable_log_collection() before doing modifications.
 * @param path The path where the log files will be written. @notnil
 */
LINPHONE_PUBLIC void linphone_core_set_log_collection_path(const char *path);

/**
 * Gets the prefix of the filenames that will be used for log collection.
 * @return The prefix of the filenames used for log collection. @notnil
 */
LINPHONE_PUBLIC const char *linphone_core_get_log_collection_prefix(void);

/**
 * Sets the prefix of the filenames that will be used for log collection.
 * When log collection is enabled, the function will close the old file and it will open the new one in the current
 * path. If you need to change the path and the file at the same time, then you should deactivate log collection with
 * linphone_core_enable_log_collection() before doing modifications.
 * @param prefix The prefix to use for the filenames for log collection. @notnil
 */
LINPHONE_PUBLIC void linphone_core_set_log_collection_prefix(const char *prefix);

/**
 * Gets the max file size in bytes of the files used for log collection.
 * @return The max file size in bytes of the files used for log collection. @notnil
 */
LINPHONE_PUBLIC size_t linphone_core_get_log_collection_max_file_size(void);

/**
 * Sets the max file size in bytes of the files used for log collection.
 * Warning: this function should only not be used to change size
 * dynamically but instead only before calling
 * linphone_core_enable_log_collection(). If you increase max size
 * on runtime, logs chronological order COULD be broken.
 * @param size The max file size in bytes of the files used for log collection.
 */
LINPHONE_PUBLIC void linphone_core_set_log_collection_max_file_size(size_t size);

/**
 * Sets the url of the server where to upload the collected log files.
 * @param core #LinphoneCore object @notnil
 * @param server_url The url of the server where to upload the collected log files. @maybenil
 */
LINPHONE_PUBLIC void linphone_core_set_log_collection_upload_server_url(LinphoneCore *core, const char *server_url);

/**
 * Gets the url of the server where to upload the collected log files.
 * @param core #LinphoneCore object @notnil
 * @return The url of the server where to upload the collected log files. @maybenil
 */
LINPHONE_PUBLIC const char *linphone_core_get_log_collection_upload_server_url(LinphoneCore *core);

/**
 * Uploads the log collection to the configured server url.
 * @param core #LinphoneCore object @notnil
 */
LINPHONE_PUBLIC void linphone_core_upload_log_collection(LinphoneCore *core);

/**
 * Compresses the log collection in a single file.
 * @return The path of the compressed log collection file. @notnil @tobefreed
 */
LINPHONE_PUBLIC char *linphone_core_compress_log_collection(void);

/**
 * Resets the log collection by removing the log files.
 */
LINPHONE_PUBLIC void linphone_core_reset_log_collection(void);

/**
 * Enables logs serialization (output logs from either the thread that creates the linphone core or the thread that
 * calls linphone_core_iterate()). Must be called before creating the #LinphoneCore.
 */
LINPHONE_PUBLIC void linphone_core_serialize_logs(void);

/**
 * Returns liblinphone's version as a string.
 * @return the current version of the #LinphoneCore @notnil
 **/
LINPHONE_PUBLIC const char *linphone_core_get_version(void);

/**
 * Gets the user-agent as a string.
 * @return liblinphone's user agent as a string. @notnil
 **/
LINPHONE_PUBLIC const char *linphone_core_get_user_agent(LinphoneCore *core);

/**
 * Gets whether the Core is considering itself in background or not.
 * The Core foreground/background state depends on the last call made to linphone_core_enter_background() or
 *linphone_core_enter_foreground().
 * @param core the #LinphoneCore
 * @return TRUE if the Core is in background, FALSE otherwise.
 **/
LINPHONE_PUBLIC bool_t linphone_core_is_in_background(const LinphoneCore *core);

/**
 * Set a label - for logging/troubleshooting purpose - to the core object.
 * This label is used by the logger to give context. When running an application with several #LinphoneCore objects,
 * (such as a test), it is useful to enhance the log's readability'.
 * @param core the #LinphoneCore
 * @param label a developper-friendly label.
 **/
LINPHONE_PUBLIC void linphone_core_set_label(LinphoneCore *core, const char *label);

/**
 * Get the label assigned to the LinphoneCore. The default value is NULL (no label).
 * @param core the #LinphoneCore
 * @return the assigned label.
 **/
LINPHONE_PUBLIC const char *linphone_core_get_label(const LinphoneCore *core);

/**
 * Sets the maximum number of call logs to retrieve when using linphone_core_get_call_logs() or
 *linphone_account_get_call_logs().
 * @param core the #LinphoneCore. @notnil
 * @param max the maximum number of call logs, use -1 to get them all.
 **/
LINPHONE_PUBLIC void linphone_core_set_max_call_logs(LinphoneCore *core, int max);

/**
 * Gets the maximum number of call logs retrieved when using linphone_core_get_call_logs() or
 *linphone_account_get_call_logs().
 * @param core the #LinphoneCore. @notnil
 * @return the maximum number of call logs that will be returned. -1 will return them all.
 **/
LINPHONE_PUBLIC int linphone_core_get_max_call_logs(const LinphoneCore *core);

/**
 * @}
 **/

/**
 * Starts a #LinphoneCore object after it has been instantiated and not automatically started.
 * Also re-initialize a #LinphoneCore object that has been stopped using linphone_core_stop().
 * Must be called only if #LinphoneGlobalState is either Ready of Off. State will changed to Startup, Configuring and
 * then On.
 * @ingroup initializing
 * @param core The #LinphoneCore object to be started @notnil
 * @return 0: success, -1: global failure, -2: could not connect database
 */
LINPHONE_PUBLIC LinphoneStatus linphone_core_start(LinphoneCore *core);

/**
 * Stops a #LinphoneCore object after it has been instantiated and started.
 * If stopped, it can be started again using linphone_core_start().
 * Must be called only if #LinphoneGlobalState is either On. State will changed to Shutdown and then Off.
 * This function may block to perform SIP server unregistration.
 * Using linphone_core_stop_async() is preferred.
 * @ingroup initializing
 * @param core The #LinphoneCore object to be stopped @notnil
 */
LINPHONE_PUBLIC void linphone_core_stop(LinphoneCore *core);

/**
 * Asynchronously stops a #LinphoneCore object after it has been instantiated and started.
 * State changes to Shutdown then linphone_core_iterate() must be called to allow the Core to end asynchronous tasks
 * (terminate call, etc.). When all tasks are finished, State will change to Off. Must be called only if
 * #LinphoneGlobalState is On. When #LinphoneGlobalState is Off #LinphoneCore can be started again using
 * linphone_core_start().
 * @ingroup initializing
 * @param core The #LinphoneCore object to be stopped @notnil
 */
LINPHONE_PUBLIC void linphone_core_stop_async(LinphoneCore *core);

/**
 * Increment the reference counter of a #LinphoneCore object.
 * @param core The #LinphoneCore which the ref counter is to be incremented. @notnil
 * @return A pointer on the #LinphoneCore passed as parameter. @notnil
 * @ingroup initializing
 */
LINPHONE_PUBLIC LinphoneCore *linphone_core_ref(LinphoneCore *core);

/**
 * Decrement the ref counter of a #LinphoneCore object and destroy it
 * if the counter reach 0.
 * @param core The #LinphoneCore which the reference counter is to be decreased. @notnil
 * @ingroup initializing
 */
LINPHONE_PUBLIC void linphone_core_unref(LinphoneCore *core);

/**
 * Main loop integration.
 * Unless auto-iterate mode is provided ( see linphone_core_enable_auto_iterate() ),
 * it is crucial that your application calls linphone_core_iterate() repeatedly.
 *
 * linphone_core_iterate() performs various backgrounds tasks:
 * - receiving of SIP messages
 * - handles timers and timeout
 * - performs registration to proxies
 * - authentication retries
 * The application MUST call this function periodically, from its main loop.
 * Be careful that this function must be called from the same thread as
 * other liblinphone methods. If it is not the case make sure all liblinphone calls are
 * serialized with a mutex.
 * A recommanded timer value is 20 ms for calling this function.
 * @param core #LinphoneCore object @notnil
 * @ingroup initializing
 **/
LINPHONE_PUBLIC void linphone_core_iterate(LinphoneCore *core);

/**
 * @ingroup initializing
 * Add a listener in order to be notified of #LinphoneCore events. Once an event is received, registred #LinphoneCoreCbs
 * are invoked sequencially.
 * @param core The #LinphoneCore object to monitor. @notnil
 * @param cbs A #LinphoneCoreCbs object holding the callbacks you need. A reference is taken by #LinphoneCore until you
 * invoke linphone_core_remove_callbacks(). @notnil
 */
LINPHONE_PUBLIC void linphone_core_add_callbacks(LinphoneCore *core, LinphoneCoreCbs *cbs);

/**
 * @ingroup initializing
 * Remove a listener from a #LinphoneCore
 * @param core The #LinphoneCore @notnil
 * @param cbs The pointer on the #LinphoneCoreCbs to remove. @notnil
 */
LINPHONE_PUBLIC void linphone_core_remove_callbacks(LinphoneCore *core, const LinphoneCoreCbs *cbs);

/**
 * @brief Set the user agent string used in SIP messages.
 *
 * Set the user agent string used in SIP messages as "[ua_name]/[version]". No slash character will be printed if NULL
 *is given to "version". If NULL is given to "ua_name" and "version" both, the User-agent header will be empty.
 *
 * This function should be called just after linphone_factory_create_core() ideally.
 * @param core The core. @notnil
 * @param name Name of the user agent. @maybenil
 * @param version Version of the user agent. @maybenil
 * @ingroup misc
 **/
LINPHONE_PUBLIC void linphone_core_set_user_agent(LinphoneCore *core, const char *name, const char *version);

/**
 * Constructs a #LinphoneAddress from the given string if possible.
 *
 * In case of just a username, characters will be unescaped.
 * If a phone number is detected, it will be flattened.
 * sip: or sips: prefix will be added if not present.
 * Finally, \@domain will be added if not present using default proxy config.
 * @see linphone_proxy_config_normalize_sip_uri() for documentation.
 * @param core The core @notnil
 * @param url the url to parse @notnil
 * @return the #LinphoneAddress matching the url or NULL in case of failure. @maybenil @tobefreed
 * @ingroup misc
 * @deprecated on 18/07/2022, use linphone_core_interpret_url_2() instead.
 */
LINPHONE_PUBLIC LinphoneAddress *linphone_core_interpret_url(LinphoneCore *core, const char *url);

/**
 * Constructs a #LinphoneAddress from the given string if possible.
 *
 * In case of just a username, characters will be unescaped.
 * If a phone number is detected, it will be flattened.
 * sip: or sips: prefix will be added if not present.
 * Finally, \@domain will be added if not present using the default #Account.
 * @see linphone_account_normalize_sip_uri() for a similar function.
 * @param core The core @notnil
 * @param url the url to parse @notnil
 * @param apply_international_prefix whether or not to try to format url as phone number using default account prefix if
 * it set (and if url is a number).
 * @return the #LinphoneAddress matching the url or NULL in case of failure. @maybenil @tobefreed
 * @ingroup misc
 */
LINPHONE_PUBLIC LinphoneAddress *
linphone_core_interpret_url_2(LinphoneCore *core, const char *url, bool_t apply_international_prefix);

/**
 * @brief Initiates an outgoing call.
 *
 * The application doesn't own a reference to the returned LinphoneCall object.
 * Use linphone_call_ref() to safely keep the LinphoneCall pointer valid within your application.
 *
 * @param core LinphoneCore object @notnil
 * @param url The destination of the call (sip address, or phone number). @notnil
 * @return A #LinphoneCall object or NULL in case of failure. @maybenil
 * @ingroup call_control
 **/
LINPHONE_PUBLIC LinphoneCall *linphone_core_invite(LinphoneCore *core, const char *url);

/**
 * Initiates an outgoing call given a destination #LinphoneAddress
 * The #LinphoneAddress can be constructed directly using linphone_factory_create_address(), or
 * created by linphone_core_interpret_url_2().
 * In C, the application doesn't own a reference to the returned #LinphoneCall object.
 * Use linphone_call_ref() to safely keep the #LinphoneCall pointer valid within your application.
 * @param core #LinphoneCore object @notnil
 * @param addr The destination of the call (sip address). @notnil
 * @return A #LinphoneCall object or NULL in case of failure. @maybenil
 * @ingroup call_control
 **/
LINPHONE_PUBLIC LinphoneCall *linphone_core_invite_address(LinphoneCore *core, const LinphoneAddress *addr);

/**
 * Initiates an outgoing call according to supplied call parameters
 * In C, the application doesn't own a reference to the returned #LinphoneCall object.
 * Use linphone_call_ref() to safely keep the #LinphoneCall pointer valid within your application.
 * @param core #LinphoneCore object @notnil
 * @param url The destination of the call (sip address, or phone number). @notnil
 * @param params the #LinphoneCallParams call parameters @notnil
 * @return A #LinphoneCall object or NULL in case of failure. @maybenil
 * @ingroup call_control
 **/
LINPHONE_PUBLIC LinphoneCall *
linphone_core_invite_with_params(LinphoneCore *core, const char *url, const LinphoneCallParams *params);

/**
 * Initiates an outgoing call given a destination #LinphoneAddress
 * The #LinphoneAddress can be constructed directly using linphone_factory_create_address(), or
 * created by linphone_core_interpret_url_2().
 * In C, the application doesn't own a reference to the returned #LinphoneCall object.
 * Use linphone_call_ref() to safely keep the #LinphoneCall pointer valid within your application.
 * If the #LinphoneAccount is not specified in parameters, the caller's account will be automatically selected by
 *finding what is the best to reach the destination of the call.
 * @param core #LinphoneCore object @notnil
 * @param addr The destination of the call (sip address). @notnil
 * @param params Call parameters @notnil
 * @return A #LinphoneCall object or NULL in case of failure. @maybenil
 * @ingroup call_control
 **/
LINPHONE_PUBLIC LinphoneCall *linphone_core_invite_address_with_params(LinphoneCore *core,
                                                                       const LinphoneAddress *addr,
                                                                       const LinphoneCallParams *params);

/**
 * Initiates an outgoing call given a destination #LinphoneAddress
 * The #LinphoneAddress can be constructed directly using linphone_factory_create_address(), or
 * created by linphone_core_interpret_url_2().
 * In C, the application doesn't own a reference to the returned #LinphoneCall object.
 * Use linphone_call_ref() to safely keep the #LinphoneCall pointer valid within your application.
 * If the #LinphoneAccount is not specified in parameters, the caller's account will be automatically selected by
 *finding what is the best to reach the destination of the call.
 * @param core #LinphoneCore object @notnil
 * @param addr The destination of the call (sip address). @notnil
 * @param params Call parameters @notnil
 * @param subject Subject of the call, UTF-8 encoded @maybenil
 * @param content Body of the SIP INVITE @maybenil
 * @return A #LinphoneCall object or NULL in case of failure. @maybenil
 * @ingroup call_control
 **/
LINPHONE_PUBLIC LinphoneCall *linphone_core_invite_address_with_params_2(LinphoneCore *core,
                                                                         const LinphoneAddress *addr,
                                                                         const LinphoneCallParams *params,
                                                                         const char *subject,
                                                                         const LinphoneContent *content);

/**
 * @brief Starts a new call as a consequence of a transfer request received from a call.
 *
 * This function is for advanced usage: the execution of transfers is automatically managed by the LinphoneCore. However
 *if an application wants to have control over the call parameters for the new call, it should call this function
 *immediately during the #LinphoneCallStateRefered notification.
 * @see #LinphoneCoreCbs
 * @param core #LinphoneCore object @notnil
 * @param call A #LinphoneCall that has just been notified about #LinphoneCallStateRefered state event. @notnil
 * @param params The call parameters to be applied to the new call. @maybenil
 * @return A #LinphoneCall corresponding to the new call that is attempted to the transfer destination. @maybenil
 **/
LINPHONE_PUBLIC LinphoneCall *
linphone_core_start_refered_call(LinphoneCore *core, LinphoneCall *call, const LinphoneCallParams *params);

/**
 * @brief Tells whether there is an incoming invite pending.
 *
 * @ingroup call_control
 * @param core #LinphoneCore object @notnil
 * @return A boolean telling whether an incoming invite is pending or not.
 */
LINPHONE_PUBLIC bool_t linphone_core_is_incoming_invite_pending(LinphoneCore *core);

/**
 * Tells whether there is a call running.
 * @param core #LinphoneCore object @notnil
 * @return A boolean value telling whether a call is currently running or not
 * @ingroup call_control
 **/
LINPHONE_PUBLIC bool_t linphone_core_in_call(const LinphoneCore *core);

/**
 * Gets the current call.
 * The current call is defined as follows:
 * If multiple concurrent calls exist, it is the only one that currelty uses the microphone or speaker,
 * otherwise it is the unique call that exists unless this call is in #LinphoneCallStatePaused state.
 * @param core #LinphoneCore object @notnil
 * @return The current call or NULL if no call is running. @maybenil
 * @ingroup call_control
 **/
LINPHONE_PUBLIC LinphoneCall *linphone_core_get_current_call(const LinphoneCore *core);

/**
 * Terminates all the calls.
 * @param core #LinphoneCore object @notnil
 * @return 0
 * @ingroup call_control
 **/
LINPHONE_PUBLIC LinphoneStatus linphone_core_terminate_all_calls(LinphoneCore *core);

/**
 * Pauses all currently running calls.
 * @param core #LinphoneCore object @notnil
 * @return 0
 * @ingroup call_control
 **/
LINPHONE_PUBLIC LinphoneStatus linphone_core_pause_all_calls(LinphoneCore *core);

/**
 * Creates a #LinphoneCallParams suitable for linphone_core_invite_with_params(),
 * linphone_call_accept_with_params(), linphone_call_accept_early_media_with_params(), linphone_call_update() or
 * linphone_call_accept_update_with_params(). The parameters are initialized according to the current #LinphoneCore
 * configuration and the current state of the call if any.
 * In order to create a #LinphoneCallParams for an already created call, the call argument must not be null.
 * @param core #LinphoneCore object
 * @param call #LinphoneCall for which the parameters are to be built, or NULL in the case where the parameters are to
 * be used for a new outgoing call. @maybenil
 * @return A new #LinphoneCallParams object. @maybenil
 * @ingroup call_control
 */
LINPHONE_PUBLIC LinphoneCallParams *linphone_core_create_call_params(LinphoneCore *core, LinphoneCall *call);

/**
 * Get the call with the specified #LinphoneAddress.
 * @param core the #LinphoneCore @notnil
 * @param remote_address the #LinphoneAddress for which the call remote address must match @notnil
 * @return the #LinphoneCall of the call if found. @maybenil
 * @ingroup call_control
 */
LINPHONE_PUBLIC LinphoneCall *linphone_core_get_call_by_remote_address2(const LinphoneCore *core,
                                                                        const LinphoneAddress *remote_address);

/**
 * Sets the local "from" SIP identity used for calls made out of any configured #LinphoneAccount.
 * Not using a SIP account is not recommended.
 *
 * @ingroup proxies
 * This data is used in absence of any proxy configuration or when no
 * account configuration is set. See #LinphoneAccount
 * @param core the Core @notnil
 * @param contact the contact to set @notnil
 * @return 0 if successful, -1 otherwise
 **/
LINPHONE_PUBLIC LinphoneStatus linphone_core_set_primary_contact(LinphoneCore *core, const char *contact);

/**
 * Returns the default identity when no account is used.
 * This SIP address usually contains a private ip address, and may not be routable globally.
 *
 * @ingroup proxies
 * @param core the Core @notnil
 * @return the primary contact identity @notnil
 **/
LINPHONE_PUBLIC const char *linphone_core_get_primary_contact(LinphoneCore *core);

/**
 * Gets the default identity SIP address.
 * This is an helper function.
 * If no default proxy is set, this will return the primary contact (
 * see linphone_core_get_primary_contact() ). If a default proxy is set
 * it returns the registered identity on the proxy.
 * @param core #LinphoneCore object @notnil
 * @return The default identity SIP address. @notnil
 * @ingroup proxies
 **/
LINPHONE_PUBLIC const char *linphone_core_get_identity(LinphoneCore *core);

/**
 * Tells #LinphoneCore to guess local hostname automatically in primary contact.
 * @ingroup proxies
 * @param core the #LinphoneCore @notnil
 * @param enable whether to enable the guess hostname feature or not
 **/
LINPHONE_PUBLIC void linphone_core_set_guess_hostname(LinphoneCore *core, bool_t enable);

/**
 * Returns TRUE if hostname part of primary contact is guessed automatically.
 * @ingroup proxies
 * @param core the #LinphoneCore @notnil
 * @return TRUE if guess hostname enabled, FALSE otherwise.
 **/
LINPHONE_PUBLIC bool_t linphone_core_get_guess_hostname(LinphoneCore *core);

/**
 * Tells to LinphoneCore to use LIME X3DH
 * @param core LinphoneCore object @notnil
 * @param enable A boolean value telling whether to enable or disable LIME X3DH
 * @ingroup chatroom
 */
LINPHONE_PUBLIC void linphone_core_enable_lime_x3dh(LinphoneCore *core, bool_t enable);

/**
 * Tells wether LIME X3DH is enabled or not
 * @param core LinphoneCore object @notnil
 * @return The current lime state
 * @ingroup chatroom
 **/
LINPHONE_PUBLIC bool_t linphone_core_lime_x3dh_enabled(const LinphoneCore *core);

/**
 * Set the x3dh server url.
 * If empty, this function will disable LIME X3DH from core.
 * Otherwise, or if different from the existing value, this will (re-)initialize the LIME X3DH engine.
 * @param core LinphoneCore object @notnil
 * @param url The x3dh server url. @maybenil
 * @ingroup misc
 * @deprecated 26/08/2022 Use linphone_account_params_set_lime_server_url() instead.
 **/
LINPHONE_DEPRECATED LINPHONE_PUBLIC void linphone_core_set_lime_x3dh_server_url(LinphoneCore *core, const char *url);

/**
 * Get the x3dh server url.
 * @param core LinphoneCore object @notnil
 * @return The x3dh server url. @maybenil
 * @ingroup misc
 * @deprecated 26/08/2022 Use linphone_account_params_get_lime_server_url() instead.
 **/
LINPHONE_DEPRECATED LINPHONE_PUBLIC const char *linphone_core_get_lime_x3dh_server_url(LinphoneCore *core);

/**
 * Tells if LIME X3DH is available
 * @param core LinphoneCore object @notnil
 * @ingroup chatroom
 **/
LINPHONE_PUBLIC bool_t linphone_core_lime_x3dh_available(const LinphoneCore *core);

/**
 * Tells whether IPv6 is enabled or not.
 * @param core #LinphoneCore object @notnil
 * @return A boolean value telling whether IPv6 is enabled or not
 * @ingroup network_parameters
 **/
LINPHONE_PUBLIC bool_t linphone_core_ipv6_enabled(LinphoneCore *core);

/**
 * Turns IPv6 support on or off.
 * @param core #LinphoneCore object @notnil
 * @param enable A boolean value telling whether to enable IPv6 support
 * @ingroup network_parameters
 **/
LINPHONE_PUBLIC void linphone_core_enable_ipv6(LinphoneCore *core, bool_t enable);

/**
 * Gets whether the use RTCP NACK for reliability of video transmission is enabled or not.
 * Using RTCP NACK feedback is one of the available techniques to help mitigate the loss of video
 * RTP packets. It gives good results when round trip time is low.
 * It is disabled by default.
 * @param core #LinphoneCore object @notnil
 * @return A boolean value telling whether NACK usage is enabled or not
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC bool_t linphone_core_retransmission_on_nack_enabled(LinphoneCore *core);

/**
 * Sets whether the use RTCP NACK for reliability of video transmission is enabled or not.
 * Using RTCP NACK feedback is one of the available techniques to help mitigate the loss of video
 * RTP packets. It gives good results when round trip time is low.
 * It is disabled by default.
 * @param core #LinphoneCore object @notnil
 * @param enable A boolean value telling whether to enable NACK context
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_enable_retransmission_on_nack(LinphoneCore *core, bool_t enable);

/**
 * Tells whether Wifi only mode is enabled or not.
 * @warning Only works for Android platform.
 * @param core #LinphoneCore object @notnil
 * @return A boolean value telling whether Wifi only mode is enabled or not
 * @ingroup network_parameters
 **/
LINPHONE_PUBLIC bool_t linphone_core_wifi_only_enabled(LinphoneCore *core);

/**
 * Turns Wifi only mode on or off. If enabled, app won't register when active network isn't WiFi or Ethernet.
 * @warning Only works for Android platform.
 * @param core #LinphoneCore object @notnil
 * @param enable A boolean value telling whether to enable IPv6 support
 * @ingroup network_parameters
 **/
LINPHONE_PUBLIC void linphone_core_enable_wifi_only(LinphoneCore *core, bool_t enable);

/**
 * Same as linphone_core_get_primary_contact() but the result is a #LinphoneAddress object
 * instead of const char *.
 * @param core the #LinphoneCore @notnil
 * @return a #LinphoneAddress object. @maybenil
 * @ingroup proxies
 * @deprecated prefer using linphone_core_get_primary_contact_address()
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneAddress *linphone_core_create_primary_contact_parsed(LinphoneCore *core);

/**
 * Sets maximum available download bandwidth
 * This is IP bandwidth, in kbit/s.
 * This information is signaled to other parties during
 * calls (within SDP messages) so that the remote end can have
 * sufficient knowledge to properly configure its audio & video
 * codec output bitrate to not overflow available bandwidth.
 * By default, the download and upload bandwidth are unknowns (set to zero),
 * in which case adaptive algorithms are run during calls in order to
 * detect available bandwidth and adapt audio and video bitrate usage.
 * @see linphone_core_enable_adaptive_rate_control().
 *
 * @ingroup media_parameters
 *
 * @param core the #LinphoneCore object @notnil
 * @param bandwidth the bandwidth in kbits/s, 0 if unknown.
 */
LINPHONE_PUBLIC void linphone_core_set_download_bandwidth(LinphoneCore *core, int bandwidth);

/**
 * Sets maximum available upload bandwidth
 * This is IP bandwidth, in kbit/s.
 * This information is used by liblinphone together with remote
 * side available bandwidth signaled in SDP messages to properly
 * configure audio & video codec's output bitrate.
 * By default, the download and upload bandwidth are unknowns (set to zero),
 * in which case adaptive algorithms are run during calls in order to
 * detect available bandwidth and adapt audio and video bitrate usage.
 * @see linphone_core_enable_adaptive_rate_control().
 *
 * @param core the #LinphoneCore object @notnil
 * @param bandwidth the bandwidth in kbits/s, 0 for unknown.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_upload_bandwidth(LinphoneCore *core, int bandwidth);

/**
 * Do not use, this function does nothing.
 *
 * @param core the #LinphoneCore object @notnil
 * @param bandwidth the bandwidth in kbits/s, 0 for infinite
 * @ingroup media_parameters
 * @deprecated 04/09/2024 this function does nothing.
 */
LINPHONE_PUBLIC void linphone_core_set_expected_bandwidth(LinphoneCore *core, int bandwidth);

/**
 * Retrieve the maximum available download bandwidth.
 * This value was set by linphone_core_set_download_bandwidth().
 * @param core the #LinphoneCore object @notnil
 * @return the download bandiwdth in kbits/s, 0 for unknown.
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC int linphone_core_get_download_bandwidth(const LinphoneCore *core);

/**
 * Retrieve the maximum available upload bandwidth.
 * This value was set by linphone_core_set_upload_bandwidth().
 * @param core the #LinphoneCore object @notnil
 * @return the upload bandiwdth in kbits/s, 0 for unknown.
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC int linphone_core_get_upload_bandwidth(const LinphoneCore *core);

/**
 * Enable adaptive rate control.
 *
 * Adaptive rate control consists in using RTCP feedback provided information to dynamically
 * control the output bitrate of the audio and video encoders, so that we can adapt to the network conditions and
 * available bandwidth. Control of the audio encoder is done in case of audio-only call, and control of the video
 *encoder is done for audio & video calls. Adaptive rate control feature is enabled by default.
 * @ingroup media_parameters
 * @param core the #LinphoneCore @notnil
 * @param enabled TRUE to enable adaptive rate control, FALSE otherwise
 **/
LINPHONE_PUBLIC void linphone_core_enable_adaptive_rate_control(LinphoneCore *core, bool_t enabled);

/**
 * Returns whether adaptive rate control is enabled.
 * @see linphone_core_enable_adaptive_rate_control()
 * @ingroup media_parameters
 * @param core the #LinphoneCore @notnil
 * @return TRUE if adaptive rate control is enabled, FALSE otherwise
 **/
LINPHONE_PUBLIC bool_t linphone_core_adaptive_rate_control_enabled(const LinphoneCore *core);

/**
 * Sets adaptive rate algorithm. It will be used for each new calls starting from
 * now. Calls already started will not be updated.
 * @param core the core @notnil
 * @param algorithm the adaptive rate control algorithm. Currently two values are supported: 'advanced', which is the
 *default value, or 'basic'. @notnil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_adaptive_rate_algorithm(LinphoneCore *core, const char *algorithm);

/**
 * Returns which adaptive rate algorithm is currently configured for future calls.
 * @see linphone_core_set_adaptive_rate_algorithm()
 * @param core the #LinphoneCore @notnil
 * @return the adaptive rate algorithm. Currently two values are supported: 'advanced', which is the default value, or
 *'basic'. @notnil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC const char *linphone_core_get_adaptive_rate_algorithm(const LinphoneCore *core);

/**
 * Set audio packetization time linphone expects to receive from peer.
 * A value of zero means that ptime is not specified.
 * @param core the #LinphoneCore @notnil
 * @param ptime the download packetization time to set
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_download_ptime(LinphoneCore *core, int ptime);

/**
 * Get audio packetization time linphone expects to receive from peer.
 * A value of zero means that ptime is not specified.
 * @param core the #LinphoneCore @notnil
 * @return the download packetization time set
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC int linphone_core_get_download_ptime(LinphoneCore *core);

/**
 * Sets audio packetization time linphone will send (in absence of requirement from peer)
 * A value of 0 stands for the current codec default packetization time.
 * @param core the #LinphoneCore @notnil
 * @param ptime the upload packetization time to set
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_upload_ptime(LinphoneCore *core, int ptime);

/**
 * Gets audio packetization time linphone will send (in absence of requirement from peer)
 * A value of 0 stands for the current codec default packetization time.
 * @param core the #LinphoneCore @notnil
 * @return the upload packetization time set
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC int linphone_core_get_upload_ptime(LinphoneCore *core);

/**
 * Sets the SIP transport timeout, which represents the maximum time permitted to establish a
 * connection to a SIP server.
 * @param core #LinphoneCore object. @notnil
 * @param timeout_ms The SIP transport timeout in milliseconds.
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_sip_transport_timeout(LinphoneCore *core, int timeout_ms);

/**
 * Gets the SIP transport timeout, which represents the maximum time permitted to establish a
 * connection to a SIP server.
 * @param core #LinphoneCore object. @notnil
 * @return The SIP transport timeout in milliseconds.
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC int linphone_core_get_sip_transport_timeout(LinphoneCore *core);

/**
 * Enable or disable DNS SRV resolution.
 * @param core #LinphoneCore object. @notnil
 * @param enable TRUE to enable DNS SRV resolution, FALSE to disable it.
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC void linphone_core_enable_dns_srv(LinphoneCore *core, bool_t enable);

/**
 * Tells whether DNS SRV resolution is enabled.
 * @param core #LinphoneCore object. @notnil
 * @return TRUE if DNS SRV resolution is enabled, FALSE if disabled.
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC bool_t linphone_core_dns_srv_enabled(const LinphoneCore *core);

/**
 * Enable or disable DNS search (use of local domain if the fully qualified name did return results).
 * @param core #LinphoneCore object. @notnil
 * @param enable TRUE to enable DNS search, FALSE to disable it.
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC void linphone_core_enable_dns_search(LinphoneCore *core, bool_t enable);

/**
 * Tells whether DNS search (use of local domain if the fully qualified name did return results) is enabled.
 * @param core #LinphoneCore object. @notnil
 * @return TRUE if DNS search is enabled, FALSE if disabled.
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC bool_t linphone_core_dns_search_enabled(const LinphoneCore *core);

/**
 * Tells if the DNS was set by an application
 * @param core #LinphoneCore object. @notnil
 * @return TRUE if DNS was set by app, FALSE otherwise.
 *@ingroup network_parameters
 */
LINPHONE_PUBLIC bool_t linphone_core_get_dns_set_by_app(LinphoneCore *core);

/**
 * Forces liblinphone to use the supplied list of dns servers, instead of system's ones
 * and set dns_set_by_app at true or false according to value of servers list.
 * @see linphone_core_get_dns_set_by_app()
 * @param core #LinphoneCore object. @notnil
 * @param servers A list of strings containing the IP addresses of DNS servers to be used. \bctbx_list{const char *}
 * Setting to NULL restores default behaviour, which is to use the DNS server list provided by the system.
 * The list is copied internally. @maybenil
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_dns_servers_app(LinphoneCore *core, const bctbx_list_t *servers);

/**
 * Forces liblinphone to use the supplied list of dns servers, instead of system's ones.
 * @param core #LinphoneCore object. @notnil
 * @param servers A list of strings containing the IP addresses of DNS servers to be used. \bctbx_list{const char *}
 * Setting to NULL restores default behaviour, which is to use the DNS server list provided by the system.
 * The list is copied internally. @maybenil
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_dns_servers(LinphoneCore *core, const bctbx_list_t *servers);

/**
 * Returns the list of the available audio payload types.
 * @param core The core. @notnil
 * @return A freshly allocated list of the available payload types.
 * @bctbx_list{LinphonePayloadType} @maybenil @tobefreed
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC bctbx_list_t *linphone_core_get_audio_payload_types(LinphoneCore *core);

/**
 * Redefines the list of the available payload types (codecs).
 * @param core The core. @notnil
 * @param payload_types The new list of payload types. \bctbx_list{LinphonePayloadType} @maybenil
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_audio_payload_types(LinphoneCore *core, const bctbx_list_t *payload_types);

/**
 * Returns the list of the available video payload types (codecs).
 * @param core The core. @notnil
 * @return A freshly allocated list of the available payload types.  @bctbx_list{LinphonePayloadType} @maybenil
 * @tobefreed
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC bctbx_list_t *linphone_core_get_video_payload_types(LinphoneCore *core);

/**
 * Redefines the list of the available video payload types (codecs).
 * Calling this function if the video codec priority policy is LinphoneCodecPriorityPolicyAuto
 * turns video codec priority policy to basic scheme, since application is not supposed to control
 * the order of video codecs when LinphoneCodecPriorityPolicyAuto is selected, by definition.
 * @see linphone_core_set_video_codec_priority_policy()
 * @param core The core. @notnil
 * @param payload_types The new list of codecs. The core does not take
 * ownership on it. \bctbx_list{LinphonePayloadType} @maybenil
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_video_payload_types(LinphoneCore *core, const bctbx_list_t *payload_types);

/**
 * Sets the priority policy for video codecs (payload types).
 * Since version 5.3, the default value is #LinphoneCodecPriorityPolicyAuto unless the core's
 * configuration file describes a list of video payload type to use.
 * This is to preserve backward compatibility for upgrading applications.
 * See #LinphoneCodecPriorityPolicy for more details.
 * @param core the core @notnil
 * @param policy the #LinphoneCodecPriorityPolicy to apply
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_video_codec_priority_policy(LinphoneCore *core,
                                                                   LinphoneCodecPriorityPolicy policy);

/**
 * Gets the current priority policy for video codecs (payload types).
 *
 * See #LinphoneCodecPriorityPolicy for more details.
 * @param core the core @notnil
 * @return the current #LinphoneCodecPriorityPolicy
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC LinphoneCodecPriorityPolicy linphone_core_get_video_codec_priority_policy(const LinphoneCore *core);

/**
 * Returns the list of the available text payload types.
 * @param core The core. @notnil
 * @return A freshly allocated list of the available payload types.  @bctbx_list{LinphonePayloadType} @maybenil
 * @tobefreed
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC bctbx_list_t *linphone_core_get_text_payload_types(LinphoneCore *core);

/**
 * Redefines the list of the available payload types.
 * @param core The core. @notnil
 * @param payload_types The new list of payload types. \bctbx_list{LinphonePayloadType} @maybenil
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_text_payload_types(LinphoneCore *core, const bctbx_list_t *payload_types);

/**
 * Enable RFC3389 generic comfort noise algorithm (CN payload type).
 * @ingroup media_parameters
 * It is disabled by default, because this algorithm is only relevant for legacy codecs (PCMU, PCMA, G722).
 * Enablement requires a SDK built with full G729 support: -DENABLE_G729=ON -DENABLE_G729B_CNG=ON .
 * @warning: the G729 support is not included in Liblinphone default licence - the purchase of a license extension is
 *required.
 * @param core #LinphoneCore object @notnil
 * @param enabled TRUE if enabled, FALSE otherwise.
 **/
LINPHONE_PUBLIC void linphone_core_enable_generic_comfort_noise(LinphoneCore *core, bool_t enabled);

/**
 * Returns enablement of RFC3389 generic comfort noise algorithm.
 * @ingroup media_parameters
 * @param core #LinphoneCore object @notnil
 * @return TRUE if generic comfort noise is enabled, FALSE otherwise.
 **/
LINPHONE_PUBLIC bool_t linphone_core_generic_comfort_noise_enabled(const LinphoneCore *core);

/**
 * Get payload type from mime type and clock rate.
 * @ingroup media_parameters
 * This function searches in audio and video codecs for the given payload type name and clockrate.
 * @param core #LinphoneCore object @notnil
 * @param type payload mime type (I.E "speex", "pcmu", "h264") @notnil
 * @param rate can be #LINPHONE_FIND_PAYLOAD_IGNORE_RATE (-1)
 * @param channels  number of channels, can be #LINPHONE_FIND_PAYLOAD_IGNORE_CHANNELS (-1)
 * @return Returns NULL if not found. If a #LinphonePayloadType is returned, it must be released with
 * linphone_payload_type_unref() after using it. @maybenil @tobefreed
 */
LINPHONE_PUBLIC LinphonePayloadType *
linphone_core_get_payload_type(LinphoneCore *core, const char *type, int rate, int channels);

/**
 * @addtogroup proxies
 * @{
 */

/**
 * Create a proxy config with default values from Linphone core.
 * @param core #LinphoneCore object @notnil
 * @return #LinphoneProxyConfig with default values set @notnil
 * @deprecated 04/09/2024 Use linphone_core_create_account()
 */
LINPHONE_PUBLIC LinphoneProxyConfig *linphone_core_create_proxy_config(LinphoneCore *core);

/**
 * Add a proxy configuration.
 * This will start registration on the proxy, if registration is enabled.
 * @param core #LinphoneCore object @notnil
 * @param config the #LinphoneProxyConfig to add @notnil
 * @return 0 if successful, -1 otherwise
 * @deprecated 04/09/2024 Use linphone_core_add_account()
 **/
LINPHONE_PUBLIC LinphoneStatus linphone_core_add_proxy_config(LinphoneCore *core, LinphoneProxyConfig *config);

/**
 * Erase all proxies from config.
 * @param core #LinphoneCore object @notnil
 * @deprecated 04/09/2024 Use linphone_core_clear_accounts()
 **/
LINPHONE_PUBLIC void linphone_core_clear_proxy_config(LinphoneCore *core);

/**
 * Removes a proxy configuration.
 *
 * #LinphoneCore will then automatically unregister and place the proxy configuration
 * on a deleted list. For that reason, a removed proxy does NOT need to be freed.
 * @param core #LinphoneCore object @notnil
 * @param config the #LinphoneProxyConfig to remove @notnil
 * @deprecated 04/09/2024 Use linphone_core_remove_account()
 **/
LINPHONE_PUBLIC void linphone_core_remove_proxy_config(LinphoneCore *core, LinphoneProxyConfig *config);

/**
 * Returns an unmodifiable list of entered proxy configurations.
 * @param core The #LinphoneCore object @notnil
 * @return A list of #LinphoneProxyConfig. @bctbx_list{LinphoneProxyConfig} @maybenil
 * @deprecated 04/09/2024 Use linphone_core_get_account_list()
 **/
LINPHONE_PUBLIC const bctbx_list_t *linphone_core_get_proxy_config_list(const LinphoneCore *core);

/**
 * Searches for a #LinphoneProxyConfig by it's idkey.
 * @param core the #LinphoneCore object @notnil
 * @param idkey An arbitrary idkey string associated to a proxy configuration
 * @return the #LinphoneProxyConfig object for the given idkey value, or NULL if none found @maybenil
 * @deprecated 04/09/2024 Use linphone_core_get_account_by_idkey()
 **/
LINPHONE_PUBLIC LinphoneProxyConfig *linphone_core_get_proxy_config_by_idkey(LinphoneCore *core, const char *idkey);

/**
 * Returns the default proxy configuration, that is the one used to determine the current identity.
 * @param core #LinphoneCore object @notnil
 * @return The default proxy configuration. @maybenil
 * @deprecated 04/09/2024 Use linphone_core_get_default_account()
 **/
LINPHONE_PUBLIC LinphoneProxyConfig *linphone_core_get_default_proxy_config(const LinphoneCore *core);

/**
 * Sets the default proxy.
 *
 * This default proxy must be part of the list of already entered LinphoneProxyConfig.
 * Toggling it as default will make #LinphoneCore use the identity associated with
 * the proxy configuration in all incoming and outgoing calls.
 * @param core #LinphoneCore object @notnil
 * @param config The proxy configuration to use as the default one. @maybenil
 * @deprecated 04/09/2024 Use linphone_core_set_default_account()
 **/
LINPHONE_PUBLIC void linphone_core_set_default_proxy_config(LinphoneCore *core, LinphoneProxyConfig *config);

/**
 * @}
 */

/**
 * @addtogroup account
 * @{
 */

/**
 * Create an account params using default values from Linphone core.
 * @param core #LinphoneCore object @notnil
 * @return #LinphoneAccountParams with default values set @notnil
 */
LINPHONE_PUBLIC LinphoneAccountParams *linphone_core_create_account_params(LinphoneCore *core);

/**
 * Creates an account using given parameters, see linphone_core_create_account_params().
 * Once created, the account must be added to the #LinphoneCore in order to be used for registration, calls,
 * messages etc. Use linphone_core_add_account() to add it to the #LinphoneCore.
 * @param core #LinphoneCore object @notnil
 * @param params #LinphoneAccountParams object @notnil
 * @return #LinphoneAccount with default values set @notnil
 */
LINPHONE_PUBLIC LinphoneAccount *linphone_core_create_account(LinphoneCore *core, LinphoneAccountParams *params);

/**
 * Adds an account.
 * This will start registration on the proxy, if registration is enabled.
 * @param core #LinphoneCore object @notnil
 * @param account the #LinphoneAccount to add @notnil
 * @return 0 if successful, -1 otherwise
 **/
LINPHONE_PUBLIC LinphoneStatus linphone_core_add_account(LinphoneCore *core, LinphoneAccount *account);

/**
 * Erases all accounts from config.
 * @param core #LinphoneCore object @notnil
 **/
LINPHONE_PUBLIC void linphone_core_clear_accounts(LinphoneCore *core);

/**
 * Removes an account.
 *
 * #LinphoneCore will then automatically unregister and place the account
 * on a deleted list. For that reason, a removed account does NOT need to be freed.
 * @param core #LinphoneCore object @notnil
 * @param account the #LinphoneAccount to remove @notnil
 **/
LINPHONE_PUBLIC void linphone_core_remove_account(LinphoneCore *core, LinphoneAccount *account);

/**
 * Returns an unmodifiable list of entered accounts.
 * @param core The #LinphoneCore object @notnil
 * @return \bctbx_list{LinphoneAccount} @maybenil
 **/
LINPHONE_PUBLIC const bctbx_list_t *linphone_core_get_account_list(const LinphoneCore *core);

/**
 * Search for a #LinphoneAccount by it's idkey.
 * @param core the #LinphoneCore object @notnil
 * @param idkey An arbitrary idkey string associated to an account. @maybenil
 * @return the #LinphoneAccount object for the given idkey value, or NULL if none found @maybenil
 **/
LINPHONE_PUBLIC LinphoneAccount *linphone_core_get_account_by_idkey(LinphoneCore *core, const char *idkey);

/**
 * Returns the default account, that is the one used to determine the current identity.
 * @param core #LinphoneCore object @notnil
 * @return The default account. @maybenil
 **/
LINPHONE_PUBLIC LinphoneAccount *linphone_core_get_default_account(const LinphoneCore *core);

/**
 * Sets the default account.
 *
 * This default account must be part of the list of already entered LinphoneAccount.
 * Toggling it as default will make #LinphoneCore default to the identity associated with
 * the account in all incoming and outgoing calls if the destination SIP uri does not explicitely match
 * any other accounts.
 * @param core #LinphoneCore object @notnil
 * @param account The account to use as the default one. @maybenil
 **/
LINPHONE_PUBLIC void linphone_core_set_default_account(LinphoneCore *core, LinphoneAccount *account);

/**
 * @}
 */

/**
 * Adds authentication information to the #LinphoneCore.
 * These nformation will be used during all SIP or HTTP transactions that require authentication.
 * @param core The #LinphoneCore. @notnil
 * @param info The #LinphoneAuthInfo to add. @notnil
 * @ingroup authentication
 */
LINPHONE_PUBLIC void linphone_core_add_auth_info(LinphoneCore *core, const LinphoneAuthInfo *info);

/**
 * Removes an authentication information object.
 * @param core The #LinphoneCore from which the #LinphoneAuthInfo will be removed. @notnil
 * @param info The #LinphoneAuthInfo to remove. @notnil
 * @ingroup authentication
 */
LINPHONE_PUBLIC void linphone_core_remove_auth_info(LinphoneCore *core, const LinphoneAuthInfo *info);

/**
 * Returns an unmodifiable list of currently entered #LinphoneAuthInfo.
 * @param core The #LinphoneCore object. @notnil
 * @return A list of #LinphoneAuthInfo. \bctbx_list{LinphoneAuthInfo} @maybenil
 * @ingroup authentication
 */
LINPHONE_PUBLIC const bctbx_list_t *linphone_core_get_auth_info_list(const LinphoneCore *core);

/**
 * Finds authentication info matching realm, username, domain criteria.
 * First of all, (realm,username) pair are searched. If multiple results (which should not happen because realm are
 *supposed to be unique), then domain is added to the search.
 * @param core the #LinphoneCore @notnil
 * @param realm the authentication 'realm' (optional) @maybenil
 * @param username the SIP username to be authenticated (mandatory) @notnil
 * @param sip_domain the SIP domain name (optional) @maybenil
 * @return a #LinphoneAuthInfo if found. @maybenil
 * @ingroup authentication
 **/
LINPHONE_PUBLIC const LinphoneAuthInfo *
linphone_core_find_auth_info(LinphoneCore *core, const char *realm, const char *username, const char *sip_domain);

/**
 * This method is used to abort a user authentication request initiated by #LinphoneCore
 * from the authentication_requested callback of #LinphoneCoreCbs
 * @note this function currently does not take into account the 'info' parameter.
 * All pending authentication requests are aborted.
 * @param core the #LinphoneCore @notnil
 * @param info the #LinphoneAuthInfo for which to abort authentication @maybenil
 * @ingroup authentication
 **/
LINPHONE_PUBLIC void linphone_core_abort_authentication(LinphoneCore *core, const LinphoneAuthInfo *info);

/**
 * Clears all authentication information.
 * @ingroup authentication
 * @param core the #LinphoneCore @notnil
 **/
LINPHONE_PUBLIC void linphone_core_clear_all_auth_info(LinphoneCore *core);

/**
 * Setup a new digest authentication policy applicable for SIP and HTTP.
 *
 * @param core the #LinphoneCore @notnil
 * @param policy a #LinphoneDigestAuthenticationPolicy @notnil
 * @see linphone_factory_create_digest_authentication_policy()
 * @ingroup authentication
 **/
LINPHONE_PUBLIC void linphone_core_set_digest_authentication_policy(LinphoneCore *core,
                                                                    LinphoneDigestAuthenticationPolicy *policy);

/**
 * Get the current digest authentication policy applicable for SIP and HTTP.
 *
 * @param core the #LinphoneCore @notnil
 * @return The current digest authentication policy. @notnil
 * @ingroup authentication
 **/
LINPHONE_PUBLIC const LinphoneDigestAuthenticationPolicy *
linphone_core_get_digest_authentication_policy(const LinphoneCore *core);

/**
 * Sets the #LinphoneAccountCreator url on the #LinphoneCore
 * @param core The #LinphoneCore @notnil
 * @param url The URL to reach @maybenil
 * @ingroup account_creator
 **/
LINPHONE_PUBLIC void linphone_core_set_account_creator_url(LinphoneCore *core, const char *url);

/**
 * Gets the #LinphoneAccountCreator url on the #LinphoneCore
 * @param core The #LinphoneCore @notnil
 * @return url The URL to reach. @maybenil
 * @ingroup account_creator
 **/
LINPHONE_PUBLIC const char *linphone_core_get_account_creator_url(LinphoneCore *core);

/**
 * Sets the #LinphoneAccountCreator backend on the #LinphoneCore
 * @param core The #LinphoneCore  @notnil
 * @param backend The #LinphoneAccountCreatorBackend
 * @ingroup account_creator
 **/
LINPHONE_PUBLIC void linphone_core_set_account_creator_backend(LinphoneCore *core,
                                                               LinphoneAccountCreatorBackend backend);

/**
 * Gets the #LinphoneAccountCreator backend set for the #LinphoneCore
 * @param core The #LinphoneCore @notnil
 * @return The #LinphoneAccountCreatorBackend
 * @ingroup account_creator
 **/
LINPHONE_PUBLIC LinphoneAccountCreatorBackend linphone_core_get_account_creator_backend(const LinphoneCore *core);

/**
 * Sets an default account creator service in the core
 * @param core #LinphoneCore object @notnil
 * @param service #LinphoneAccountCreatorService object @maybenil
 **/
LINPHONE_PUBLIC void linphone_core_set_account_creator_service(LinphoneCore *core,
                                                               LinphoneAccountCreatorService *service);

/**
 * Gets default account creator service from the core
 * @param core #LinphoneCore object @notnil
 * @return #LinphoneAccountCreatorService object @maybenil
 **/
LINPHONE_PUBLIC LinphoneAccountCreatorService *linphone_core_get_account_creator_service(LinphoneCore *core);

/**
 * Enables or disables the audio adaptive jitter compensation.
 * It is enabled by default and should be disabled only to verify how communication over IP
 * is a disaster without jitter compensation.
 * @param core #LinphoneCore object @notnil
 * @param enable TRUE to enable the audio adaptive jitter compensation, FALSE to disable it.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_enable_audio_adaptive_jittcomp(LinphoneCore *core, bool_t enable);

/**
 * Tells whether the audio adaptive jitter compensation is enabled.
 * @param core #LinphoneCore object @notnil
 * @return TRUE if the audio adaptive jitter compensation is enabled, FALSE otherwise.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC bool_t linphone_core_audio_adaptive_jittcomp_enabled(LinphoneCore *core);

/**
 * Returns the nominal audio jitter buffer size in milliseconds.
 * @param core #LinphoneCore object @notnil
 * @return The nominal audio jitter buffer size in milliseconds
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC int linphone_core_get_audio_jittcomp(LinphoneCore *core);

/**
 * Sets the nominal audio jitter buffer size in milliseconds.
 * The value takes effect immediately for all running and pending calls, if any.
 * A value of 0 disables the jitter buffer.
 * @ingroup media_parameters
 * @param core the #LinphoneCore object @notnil
 * @param milliseconds the audio jitter buffer size to set in milliseconds
 **/
LINPHONE_PUBLIC void linphone_core_set_audio_jittcomp(LinphoneCore *core, int milliseconds);

/**
 * Enables or disables the video adaptive jitter compensation.
 * @param core #LinphoneCore object @notnil
 * @param enable TRUE to enable the video adaptive jitter compensation, FALSE to disable it.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_enable_video_adaptive_jittcomp(LinphoneCore *core, bool_t enable);

/**
 * Tells whether the video adaptive jitter compensation is enabled.
 * @param core #LinphoneCore object @notnil
 * @return TRUE if the video adaptive jitter compensation is enabled, FALSE otherwise.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC bool_t linphone_core_video_adaptive_jittcomp_enabled(LinphoneCore *core);

/**
 * Returns the nominal video jitter buffer size in milliseconds.
 * @param core #LinphoneCore object @notnil
 * @return The nominal video jitter buffer size in milliseconds
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC int linphone_core_get_video_jittcomp(LinphoneCore *core);

/**
 * Sets the nominal video jitter buffer size in milliseconds.
 * The value takes effect immediately for all running and pending calls, if any.
 * A value of 0 disables the jitter buffer.
 * @ingroup media_parameters
 * @param core the #LinphoneCore @notnil
 * @param milliseconds the jitter buffer size in milliseconds
 **/
LINPHONE_PUBLIC void linphone_core_set_video_jittcomp(LinphoneCore *core, int milliseconds);

/**
 * Gets the UDP port used for audio streaming.
 * @param core #LinphoneCore object @notnil
 * @return The UDP port used for audio streaming
 * @ingroup network_parameters
 **/
LINPHONE_PUBLIC int linphone_core_get_audio_port(const LinphoneCore *core);

/**
 * Get the audio port range from which is randomly chosen the UDP port used for audio streaming.
 * @param core #LinphoneCore object
 * @param[out] min_port The lower bound of the audio port range being used
 * @param[out] max_port The upper bound of the audio port range being used
 * @ingroup network_parameters
 * @donotwrap
 */
LINPHONE_PUBLIC void linphone_core_get_audio_port_range(const LinphoneCore *core, int *min_port, int *max_port);

/**
 * Get the audio port range from which is randomly chosen the UDP port used for audio streaming.
 * @param core #LinphoneCore object @notnil
 * @return a #LinphoneRange object @notnil @tobefreed
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC LinphoneRange *linphone_core_get_audio_ports_range(const LinphoneCore *core);

/**
 * Gets the UDP port used for video streaming.
 * @param core #LinphoneCore object @notnil
 * @return The UDP port used for video streaming
 * @ingroup network_parameters
 **/
LINPHONE_PUBLIC int linphone_core_get_video_port(const LinphoneCore *core);

/**
 * Get the video port range from which is randomly chosen the UDP port used for video streaming.
 * @param core #LinphoneCore object
 * @param[out] min_port The lower bound of the video port range being used
 * @param[out] max_port The upper bound of the video port range being used
 * @ingroup network_parameters
 * @donotwrap
 */
LINPHONE_PUBLIC void linphone_core_get_video_port_range(const LinphoneCore *core, int *min_port, int *max_port);

/**
 * Get the video port range from which is randomly chosen the UDP port used for video streaming.
 * @param core #LinphoneCore object @notnil
 * @return a #LinphoneRange object @notnil @tobefreed
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC LinphoneRange *linphone_core_get_video_ports_range(const LinphoneCore *core);

/**
 * Gets the UDP port used for text streaming.
 * @param core #LinphoneCore object @notnil
 * @return The UDP port used for text streaming
 * @ingroup network_parameters
 **/
LINPHONE_PUBLIC int linphone_core_get_text_port(const LinphoneCore *core);

/**
 * Get the video port range from which is randomly chosen the UDP port used for text streaming.
 * @param core #LinphoneCore object
 * @param[out] min_port The lower bound of the text port range being used
 * @param[out] max_port The upper bound of the text port range being used
 * @ingroup network_parameters
 * @donotwrap
 */
LINPHONE_PUBLIC void linphone_core_get_text_port_range(const LinphoneCore *core, int *min_port, int *max_port);

/**
 * Get the text port range from which is randomly chosen the UDP port used for text streaming.
 * @param core #LinphoneCore object @notnil
 * @return a #LinphoneRange object @notnil @tobefreed
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC LinphoneRange *linphone_core_get_text_ports_range(const LinphoneCore *core);

/**
 * Gets the value of the no-rtp timeout.
 *
 * When no RTP or RTCP packets have been received for a while
 * #LinphoneCore will consider the call is broken (remote end crashed or
 * disconnected from the network), and thus will terminate the call.
 * The no-rtp timeout is the duration above which the call is considered broken.
 * @param core #LinphoneCore object @notnil
 * @return The value of the no-rtp timeout in seconds
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC int linphone_core_get_nortp_timeout(const LinphoneCore *core);

/**
 * Gets the value of the no-rtp timeout when the call is on hold.
 *
 * When no RTP or RTCP packets have been received for a while when the call is on hold
 * #LinphoneCore will consider the call is broken (remote end crashed or
 * disconnected from the network), and thus will terminate the call.
 * The no-rtp timeout is the duration above which the call is considered broken.
 * @param core #LinphoneCore object @notnil
 * @return The value of the no-rtp timeout in seconds when the call is on hold
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC int linphone_core_get_nortp_onhold_timeout(const LinphoneCore *core);

/**
 * Sets the UDP port used for audio streaming.
 * A value of -1 will request the system to allocate the local port randomly.
 * This is recommended in order to avoid firewall warnings.
 * @param core #LinphoneCore object @notnil
 * @param port The UDP port to use for audio streaming
 * @ingroup network_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_audio_port(LinphoneCore *core, int port);

/**
 * Sets the UDP port range from which to randomly select the port used for audio streaming.
 * @param core #LinphoneCore object @notnil
 * @param min_port The lower bound of the audio port range to use
 * @param max_port The upper bound of the audio port range to use
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_audio_port_range(LinphoneCore *core, int min_port, int max_port);

/**
 * Sets the UDP port used for video streaming.
 * A value of -1 will request the system to allocate the local port randomly.
 * This is recommended in order to avoid firewall warnings.
 * @param core #LinphoneCore object @notnil
 * @param port The UDP port to use for video streaming
 * @ingroup network_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_video_port(LinphoneCore *core, int port);

/**
 * Sets the UDP port range from which to randomly select the port used for video streaming.
 * @param core #LinphoneCore object @notnil
 * @param min_port The lower bound of the video port range to use
 * @param max_port The upper bound of the video port range to use
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_video_port_range(LinphoneCore *core, int min_port, int max_port);

/**
 * Sets the UDP port used for text streaming.
 * A value if -1 will request the system to allocate the local port randomly.
 * This is recommended in order to avoid firewall warnings.
 * @param core #LinphoneCore object @notnil
 * @param port The UDP port to use for text streaming
 * @ingroup network_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_text_port(LinphoneCore *core, int port);

/**
 * Sets the UDP port range from which to randomly select the port used for text streaming.
 * @param core #LinphoneCore object @notnil
 * @param min_port The lower bound of the text port range to use
 * @param max_port The upper bound of the text port range to use
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_text_port_range(LinphoneCore *core, int min_port, int max_port);

/**
 * Sets the no-rtp timeout value in seconds.
 * @param core #LinphoneCore object @notnil
 * @param seconds The no-rtp timeout value to use in seconds
 * @ingroup media_parameters
 * @see linphone_core_get_nortp_timeout() for details.
 **/
LINPHONE_PUBLIC void linphone_core_set_nortp_timeout(LinphoneCore *core, int seconds);

/**
 * Sets the no-rtp timeout value in seconds when the call is on hold.
 * @param core #LinphoneCore object @notnil
 * @param seconds The no-rtp timeout value to use in seconds when the call is on hold
 * @ingroup media_parameters
 * @see linphone_core_get_nortp_on_hold_timeout() for details.
 **/
LINPHONE_PUBLIC void linphone_core_set_nortp_onhold_timeout(LinphoneCore *core, int seconds);

/**
 * Sets whether SIP INFO method can be used to send digits. This non-standard but common practice.
 * @param core #LinphoneCore object @notnil
 * @param use_info A boolean value telling whether to use SIP INFO to send digits
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_use_info_for_dtmf(LinphoneCore *core, bool_t use_info);

/**
 * Indicates whether SIP INFO can be used to send digits.
 * @param core #LinphoneCore object @notnil
 * @return A boolean value telling whether SIP INFO is used to send digits
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC bool_t linphone_core_get_use_info_for_dtmf(LinphoneCore *core);

/**
 * Sets whether RFC2833 or RFC4633 can be to be used to send digits.
 * This is preferred method to reliabily transmit DTMFs codes.
 * There are two settings relevant to dtmf sending: linphone_core_set_use_rfc2833_for_dtmf()
 * and linphone_core_set_use_info_for_dtmf(); Resulting in 4 cases:
 * - If neither are enabled, don't send anything.
 * - If one is enabled but not the other, then send the DTMF using the one the that is enabled.
 * - If both are enabled, use RFC2833, then SIP INFO as fallback only if the media does not
 * support telephone-events. In that last sub-case, note that the DTMF will also be sent modulated
 * into the audio signal.
 *
 * @param core #LinphoneCore object @notnil
 * @param use_rfc2833 A boolean value telling whether to use RFC2833 to send digits
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_use_rfc2833_for_dtmf(LinphoneCore *core, bool_t use_rfc2833);

/**
 * Indicates whether RFC2833/RFC4633 can be used to send digits.
 * @param core #LinphoneCore object @notnil
 * @return A boolean value telling whether RFC2833 is used to send digits
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC bool_t linphone_core_get_use_rfc2833_for_dtmf(LinphoneCore *core);

/**
 * Sets the ports to be used for each of transport (UDP or TCP)
 * A zero value port for a given transport means the transport
 * is not used.
 * A value of LC_SIP_TRANSPORT_RANDOM (-1) means the port is to be choosen randomly by the system.
 * A value of LC_SIP_TRANSPORT_DONTBIND (-2) means that the socket will not be bound explicitely, in other
 * words liblinphone won't listen for incoming connections at all. This mode is suitable for a pure client
 * application (ex: a mobile application).
 * @param core #LinphoneCore object @notnil
 * @param transports A #LinphoneSipTransports structure giving the ports to use @notnil
 * @return 0
 * @ingroup network_parameters
 **/
LINPHONE_PUBLIC LinphoneStatus linphone_core_set_transports(LinphoneCore *core, const LinphoneTransports *transports);

/**
 * Retrieves the port configuration used for each transport (udp, tcp, tls).
 * A zero value port for a given transport means the transport
 * is not used.
 * A value of LC_SIP_TRANSPORT_RANDOM (-1) means the port is to be chosen randomly by the system.
 * A value of LC_SIP_TRANSPORT_DONTBIND (-2) means that the socket will not be bound explicitely, in other
 * words liblinphone won't listen for incoming connections at all. This mode is suitable for a pure client
 * application (ex: a mobile application).
 * @param core #LinphoneCore object @notnil
 * @return A #LinphoneTransports structure with the configured ports @notnil @tobefreed
 * @ingroup network_parameters
 **/
LINPHONE_PUBLIC LinphoneTransports *linphone_core_get_transports(LinphoneCore *core);

/**
 * Retrieves the real port number assigned for each sip transport (udp, tcp, tls).
 * A zero value means that the transport is not activated.
 * If LC_SIP_TRANSPORT_RANDOM was passed to linphone_core_set_sip_transports(), the random port choosed by the system is
 *returned.
 * @param core #LinphoneCore object @notnil
 * @return A #LinphoneTransports structure with the ports being used @notnil @tobefreed
 * @ingroup network_parameters
 **/
LINPHONE_PUBLIC LinphoneTransports *linphone_core_get_transports_used(LinphoneCore *core);

/**
 * Increment refcount.
 * @param transports #LinphoneTransports object @notnil
 * @return the same #LinphoneTransports object @notnil
 * @ingroup network_parameters
 **/
LINPHONE_PUBLIC LinphoneTransports *linphone_transports_ref(LinphoneTransports *transports);

/**
 * Decrement refcount and possibly free the object.
 * @param transports #LinphoneTransports object @notnil
 * @ingroup network_parameters
 **/
LINPHONE_PUBLIC void linphone_transports_unref(LinphoneTransports *transports);

/**
 * Gets the user data in the #LinphoneTransports object
 * @param transports the #LinphoneTransports @notnil
 * @return the user data. @maybenil
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC void *linphone_transports_get_user_data(const LinphoneTransports *transports);

/**
 * Sets the user data in the #LinphoneTransports object
 * @param transports the #LinphoneTransports object @notnil
 * @param user_data the user data @maybenil
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC void linphone_transports_set_user_data(LinphoneTransports *transports, void *user_data);

/**
 * Gets the UDP port in the #LinphoneTransports object
 * @param transports the #LinphoneTransports object @notnil
 * @return the UDP port
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC int linphone_transports_get_udp_port(const LinphoneTransports *transports);

/**
 * Gets the TCP port in the #LinphoneTransports object
 * @param transports the #LinphoneTransports object @notnil
 * @return the TCP port
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC int linphone_transports_get_tcp_port(const LinphoneTransports *transports);

/**
 * Gets the TLS port in the #LinphoneTransports object
 * @param transports the #LinphoneTransports object @notnil
 * @return the TLS port
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC int linphone_transports_get_tls_port(const LinphoneTransports *transports);

/**
 * Gets the DTLS port in the #LinphoneTransports object
 * @param transports the #LinphoneTransports object @notnil
 * @return the DTLS port
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC int linphone_transports_get_dtls_port(const LinphoneTransports *transports);

/**
 * Sets the UDP port in the #LinphoneTransports object
 * @param transports the #LinphoneTransports object @notnil
 * @param port the UDP port
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC void linphone_transports_set_udp_port(LinphoneTransports *transports, int port);

/**
 * Sets the TCP port in the #LinphoneTransports object
 * @param transports the #LinphoneTransports object @notnil
 * @param port the TCP port
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC void linphone_transports_set_tcp_port(LinphoneTransports *transports, int port);

/**
 * Sets the TLS port in the #LinphoneTransports object
 * @param transports the #LinphoneTransports object @notnil
 * @param port the TLS port
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC void linphone_transports_set_tls_port(LinphoneTransports *transports, int port);

/**
 * Sets the DTLS port in the #LinphoneTransports object
 * @param transports the #LinphoneTransports object @notnil
 * @param port the DTLS port
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC void linphone_transports_set_dtls_port(LinphoneTransports *transports, int port);

/**
 * Tells whether the given transport type is supported by the library.
 * @param core #LinphoneCore object @notnil
 * @param transport_type #LinphoneTranportType to check for support
 * @return A boolean value telling whether the given transport type is supported by the library
 **/
LINPHONE_PUBLIC bool_t linphone_core_sip_transport_supported(const LinphoneCore *core,
                                                             LinphoneTransportType transport_type);

LINPHONE_PUBLIC bool_t linphone_core_content_encoding_supported(const LinphoneCore *core, const char *content_encoding);

/**
 * Sets the incoming call timeout in seconds.
 * If an incoming call isn't answered for this timeout period, it is
 * automatically declined.
 * @param core #LinphoneCore object @notnil
 * @param seconds The new timeout in seconds
 * @ingroup call_control
 **/
LINPHONE_PUBLIC void linphone_core_set_inc_timeout(LinphoneCore *core, int seconds);

/**
 * Returns the incoming call timeout
 * See linphone_core_set_inc_timeout() for details.
 * @param core #LinphoneCore object @notnil
 * @return The current incoming call timeout in seconds
 * @ingroup call_control
 **/
LINPHONE_PUBLIC int linphone_core_get_inc_timeout(LinphoneCore *core);

/**
 * Configures the minimum interval between a push notification and the corresponding incoming INVITE.
 * If exceeded, Linphone Call is transitioned to CallError and further incoming invite associated to this push is
 *declined if any.
 * @param core #LinphoneCore object @notnil
 * @param seconds The new timeout in seconds
 * @ingroup call_control
 **/
LINPHONE_PUBLIC void linphone_core_set_push_incoming_call_timeout(LinphoneCore *core, int seconds);

/**
 * Returns the push incoming call timeout
 * See linphone_core_set_push_incoming_call_timeout() for details.
 * @param core #LinphoneCore object @notnil
 * @return The current push incoming call timeout in seconds
 * @ingroup call_control
 **/
LINPHONE_PUBLIC int linphone_core_get_push_incoming_call_timeout(const LinphoneCore *core);

/**
 * Sets the maximum duration of a call in seconds.
 * After this timeout period, the call is automatically hangup.
 * A value of 0 disables this feature.
 * @param core #LinphoneCore object @notnil
 * @param seconds The new timeout in seconds
 * @ingroup call_control
 **/
LINPHONE_PUBLIC void linphone_core_set_in_call_timeout(LinphoneCore *core, int seconds);

/**
 * Gets the maximum duration of a call.
 * See linphone_core_set_in_call_timeout() for details.
 * @param core #LinphoneCore object @notnil
 * @return The current in call timeout in seconds
 * @ingroup call_control
 **/
LINPHONE_PUBLIC int linphone_core_get_in_call_timeout(LinphoneCore *core);

/**
 * Set the in delayed timeout in seconds.
 * After this timeout period, a delayed call (internal call initialisation or resolution) is resumed.
 * @param core #LinphoneCore object @notnil
 * @param seconds The new delayed timeout
 * @ingroup call_control
 * @deprecated 04/09/2024 Obscure.
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED void linphone_core_set_delayed_timeout(LinphoneCore *core, int seconds);

/**
 * Gets the delayed timeout
 * See linphone_core_set_delayed_timeout() for details.
 * @param core #LinphoneCore object @notnil
 * @return The current delayed timeout in seconds
 * @ingroup call_control
 * @deprecated 04/09/2024 Obscure.
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED int linphone_core_get_delayed_timeout(LinphoneCore *core);

/**
 * Set the STUN server address to use when the firewall policy is set to STUN.
 * @param core #LinphoneCore object @notnil
 * @param server The STUN server address to use. @maybenil
 * @deprecated 04/09/2024 use linphone_core_set_nat_policy() or linphone_account_params_set_nat_policy().
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED void linphone_core_set_stun_server(LinphoneCore *core, const char *server);

/**
 * Get the STUN server address being used.
 * @param core #LinphoneCore object @notnil
 * @return The STUN server address being used. @maybenil
 * @deprecated 04/09/2024 use linphone_core_get_nat_policy() or linphone_account_params_get_nat_policy().
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED const char *linphone_core_get_stun_server(const LinphoneCore *core);

/**
 * Return the availability of uPnP.
 * @return true if uPnP is available otherwise return false.
 * @warning UPNP support has been removed, and proved to be useless.
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC bool_t linphone_core_upnp_available(void);

/**
 * Return the internal state of uPnP.
 * @param core #LinphoneCore @notnil
 * @return an LinphoneUpnpState.
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC LinphoneUpnpState linphone_core_get_upnp_state(const LinphoneCore *core);

/**
 * Return the external ip address of router.
 * In some cases the uPnP can have an external ip address but not a usable uPnP
 * (state different of Ok).
 *
 * @param core #LinphoneCore @notnil
 * @return a null terminated string containing the external ip address. If the
 * the external ip address is not available return null. @maybenil
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC const char *linphone_core_get_upnp_external_ipaddress(const LinphoneCore *core);

/**
 * Deprecated. This function was used to force a given IP address to appear in SDP.
 * Unfortunately, this cannot work as explained by https://www.rfc-editor.org/rfc/rfc5389#section-2 .
 * @param core #LinphoneCore object. @notnil
 * @param addr The public IP address of NAT to use. @maybenil
 * @deprecated 12/10/2022 Use linphone_core_set_nat_policy()
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED void linphone_core_set_nat_address(LinphoneCore *core, const char *addr);

/**
 * Deprecated. Get the public IP address of NAT being used.
 * @param core #LinphoneCore object. @notnil
 * @return The public IP address of NAT being used. @maybenil
 * @deprecated 12/10/2022 Use linphone_core_get_nat_policy()
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED const char *linphone_core_get_nat_address(const LinphoneCore *core);

/**
 * Set the policy to use to pass through NATs/firewalls.
 * It may be overridden by a NAT policy for a specific proxy config.
 * @param core #LinphoneCore object @notnil
 * @param policy #LinphoneNatPolicy object @maybenil
 * @ingroup network_parameters
 * @see linphone_account_params_set_nat_policy()
 */
LINPHONE_PUBLIC void linphone_core_set_nat_policy(LinphoneCore *core, LinphoneNatPolicy *policy);

/**
 * Artificially cause the relay path to be selected when ICE is used.
 * This is mainly a function for test, for example to validate that the relay service (ever TURN or media-aware SIP
 * proxy) is working as expected. Indeed, in many cases a path through host or server reflexive candidate will be found
 * by ICE, which makes difficult to make sure that the relay service is working as expected.
 * @param[in] core #LinphoneCore object
 * @param[in] enable boolean value
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC void linphone_core_enable_forced_ice_relay(LinphoneCore *core, bool_t enable);

/**
 * Indicates whether the ICE relay path is forcibly selected.
 * @param[in] core #LinphoneCore object
 * @return a boolean value indicating whether forced relay is enabled.
 * @ingroup network_parameters
 * @see linphone_core_enable_forced_ice_relay().
 */
LINPHONE_PUBLIC bool_t linphone_core_forced_ice_relay_enabled(const LinphoneCore *core);

/**
 * Get The policy that is used to pass through NATs/firewalls.
 * It may be overridden by a NAT policy for a specific proxy config.
 * @param core #LinphoneCore object
 * @return #LinphoneNatPolicy object in use. @maybenil
 * @ingroup network_parameters
 * @see linphone_account_params_get_nat_policy()
 */
LINPHONE_PUBLIC LinphoneNatPolicy *linphone_core_get_nat_policy(const LinphoneCore *core);

/**
 * Gets the list of the available sound devices.
 * @param core #LinphoneCore object @notnil
 * @return An unmodifiable array of strings contanining the names of the available
 * sound devices that is NULL terminated. \bctbx_list{char *} @maybenil @tobefreed
 * @deprecated 10/04/2021 Use linphone_core_get_audio_devices() instead.
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC bctbx_list_t *linphone_core_get_sound_devices_list(const LinphoneCore *core);

/**
 * Update detection of sound devices.
 *
 * Use this function when the application is notified of USB plug events, so that
 * list of available hardwares for sound playback and capture is updated.
 * @param core #LinphoneCore object. @notnil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_reload_sound_devices(LinphoneCore *core);

/**
 * Allow to control microphone level: gain in db.
 * @param core #LinphoneCore object @notnil
 * @param level The new microphone level
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_mic_gain_db(LinphoneCore *core, float level);

/**
 * Get microphone gain in db.
 * @param core #LinphoneCore object @notnil
 * @return The current microphone gain
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC float linphone_core_get_mic_gain_db(LinphoneCore *core);

/**
 * Calling this method with disable=true will cause the microhone to be completely deactivated when muted,
 * when given possible by the implementation on the platform on which liblinphone is running.
 * Otherwise, sound recording remains active but silence is sent instead of recorded audio.
 * Playback sound will be briefly interrupted while the audio is reconfigured.
 * Currently only implemented for IOS, it will also disable Apple's microphone recording indicator when microphone is
 * muted.
 *
 * @param core #LinphoneCore object @notnil
 * @param disable True if you wish to entirely stop the audio recording when muting the microphone.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_disable_record_on_mute(LinphoneCore *core, bool_t disable);

/**
 * Get whether the microphone will be completely deactivated when muted. Please refer to
 * linphone_core_set_disable_record_on_mute().
 *
 * @param core #LinphoneCore object @notnil
 * @return  True if you wish to entirely stop the audio recording when muting the microphone.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC bool_t linphone_core_get_disable_record_on_mute(LinphoneCore *core);

/**
 * Allow to control play level before entering sound card:  gain in db
 * @param core #LinphoneCore object @notnil
 * @param level The new play level
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_playback_gain_db(LinphoneCore *core, float level);

/**
 * Gets playback gain in db (before entering sound card).
 * @param core #LinphoneCore object @notnil
 * @return The current playback gain
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC float linphone_core_get_playback_gain_db(LinphoneCore *core);

/**
 * Gets the name of the currently assigned sound device for ringing.
 * @param core #LinphoneCore object @notnil
 * @return The name of the currently assigned sound device for ringing. @maybenil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC const char *linphone_core_get_ringer_device(LinphoneCore *core);

/**
 * Gets the name of the currently assigned sound device for playback.
 * @param core #LinphoneCore object @notnil
 * @return The name of the currently assigned sound device for playback. @maybenil
 * @deprecated 11/09/2024 use linphone_core_get_output_audio_device() or
 * linphone_core_get_default_output_audio_device() instead.
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC const char *linphone_core_get_playback_device(LinphoneCore *core);

/**
 * Gets the name of the currently assigned sound device for capture.
 * @param core #LinphoneCore object @notnil
 * @return The name of the currently assigned sound device for capture. @maybenil
 * @deprecated 11/09/2024 use linphone_core_get_input_audio_device() or
 * linphone_core_get_default_input_audio_device() instead.
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC const char *linphone_core_get_capture_device(LinphoneCore *core);

/**
 * Gets the name of the currently assigned sound device for media.
 * @param core #LinphoneCore object @notnil
 * @return The name of the currently assigned sound device for capture. @maybenil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC const char *linphone_core_get_media_device(LinphoneCore *core);

/**
 * Sets the sound device used for ringing.
 * @param core #LinphoneCore object @notnil
 * @param devid The device name as returned by linphone_core_get_sound_devices() @maybenil
 * @return 0
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC LinphoneStatus linphone_core_set_ringer_device(LinphoneCore *core, const char *devid);

/**
 * Sets the sound device used for playback.
 * @param core #LinphoneCore object @notnil
 * @param devid The device name as returned by linphone_core_get_sound_devices() @maybenil
 * @return 0
 * @deprecated 11/09/2024 use linphone_core_set_output_audio_device()
 * or linphone_core_set_default_output_audio_device() instead.
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC LinphoneStatus linphone_core_set_playback_device(LinphoneCore *core, const char *devid);

/**
 * Sets the sound device used for capture.
 * @param core #LinphoneCore object @notnil
 * @param devid The device name as returned by linphone_core_get_sound_devices() @maybenil
 * @return 0
 * @deprecated 11/09/2024 use linphone_core_set_input_audio_device() or
 * linphone_core_set_default_input_audio_device() instead.
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC LinphoneStatus linphone_core_set_capture_device(LinphoneCore *core, const char *devid);

/**
 * Sets the sound device used for media.
 * @param core #LinphoneCore object @notnil
 * @param devid The device name as returned by linphone_core_get_sound_devices() @maybenil
 * @return 0
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC LinphoneStatus linphone_core_set_media_device(LinphoneCore *core, const char *devid);

/**
 * Whenever the liblinphone is playing a ring to advertise an incoming call or ringback of an outgoing call, this
 *function stops the ringing. Typical use is to stop ringing when the user requests to ignore the call.
 * @param core #LinphoneCore object @notnil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_stop_ringing(LinphoneCore *core);

/**
 * Sets the path to a wav file used for ringing. The file must be a wav 16bit linear.
 * If null, ringing is disable unless #linphone_core_get_use_native_ringing() is enabled, in which case we use the
 *device ringtone.
 * @param core #LinphoneCore object @notnil
 * @param path The path to a wav file to be used for ringing, null to disable or use device ringing depending on
 *#linphone_core_get_use_native_ringing(). @maybenil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_ring(LinphoneCore *core, const char *path);

/**
 * Returns the path to the wav file used for ringing.
 * @param core #LinphoneCore object @notnil
 * @return The path to the wav file used for ringing. @maybenil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC const char *linphone_core_get_ring(const LinphoneCore *core);

/**
 * Sets whether to use the native ringing (Android only).
 * @param core #LinphoneCore object @notnil
 * @param enable True to enable native ringing, false otherwise
 * @ingroup media_parameters
 * @deprecated 16/12/2021 Use linphone_core_enable_native_ringing() instead.
 **/
LINPHONE_PUBLIC void linphone_core_set_native_ringing_enabled(LinphoneCore *core, bool_t enable);

/**
 * Sets whether to use the native ringing (Android only).
 * @param core #LinphoneCore object @notnil
 * @param enable True to enable native ringing, false otherwise
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_enable_native_ringing(LinphoneCore *core, bool_t enable);

/**
 * Returns whether the native ringing is enabled or not.
 * @param core #LinphoneCore object @notnil
 * @return True if we use the native ringing, false otherwise
 * @ingroup media_parameters
 * @deprecated 16/12/2021 Use linphone_core_native_ringing_enabled() instead.
 **/
LINPHONE_PUBLIC bool_t linphone_core_is_native_ringing_enabled(const LinphoneCore *core);

/**
 * Returns whether the native ringing is enabled or not.
 * @param core #LinphoneCore object @notnil
 * @return True if we use the native ringing, false otherwise
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC bool_t linphone_core_native_ringing_enabled(const LinphoneCore *core);

/**
 * Specify whether the tls server certificate must be verified when connecting to a SIP/TLS server.
 * @param core #LinphoneCore object @notnil
 * @param yesno A boolean value telling whether the tls server certificate must be verified
 * @ingroup initializing
 **/
LINPHONE_PUBLIC void linphone_core_verify_server_certificates(LinphoneCore *core, bool_t yesno);

/**
 * Get whether the tls server certificate must be verified when connecting to a SIP/TLS server.
 * @param core #LinphoneCore object @notnil
 * @return True if the tls server certificate must be verified
 * @ingroup initializing
 **/
LINPHONE_PUBLIC bool_t linphone_core_is_verify_server_certificates(LinphoneCore *core);

/**
 * Specify whether the tls server certificate common name must be verified when connecting to a SIP/TLS server.
 * @param core #LinphoneCore object @notnil
 * @param yesno A boolean value telling whether the tls server certificate common name must be verified
 * @ingroup initializing
 **/
LINPHONE_PUBLIC void linphone_core_verify_server_cn(LinphoneCore *core, bool_t yesno);

/**
 * Get whether the tls server certificate common name must be verified when connecting to a SIP/TLS server.
 * @param core #LinphoneCore object @notnil
 * @return True if the tls server certificate common name must be verified
 * @ingroup initializing
 **/
LINPHONE_PUBLIC bool_t linphone_core_is_verify_server_cn(LinphoneCore *core);

/**
 * Gets the path to a file or folder containing the trusted root CAs (PEM format)
 * @param core #LinphoneCore object @notnil
 * @return The path to a file or folder containing the trusted root CAs. @maybenil
 * @ingroup initializing
 **/
LINPHONE_PUBLIC const char *linphone_core_get_root_ca(LinphoneCore *core);

/**
 * Sets the path to a file or folder containing trusted root CAs (PEM format)
 * @param core #LinphoneCore object @notnil
 * @param path The path to a file or folder containing trusted root CAs. @maybenil
 * @ingroup initializing
 **/
LINPHONE_PUBLIC void linphone_core_set_root_ca(LinphoneCore *core, const char *path);

/**
 * Sets the trusted root CAs (PEM format)
 * @param core #LinphoneCore object @notnil
 * @param data The trusted root CAs as a string @maybenil
 * @ingroup initializing
 **/
LINPHONE_PUBLIC void linphone_core_set_root_ca_data(LinphoneCore *core, const char *data);

/**
 * @internal
 * Set the pointer to an externally provided ssl configuration for the crypto library
 * @param core #LinphoneCore object @notnil
 * @param ssl_config A pointer to an opaque structure which will be provided directly to the crypto library used in
 * bctoolbox. Use with extra care. This ssl_config structure is responsibility of the caller and will not be freed at
 * the connection's end. @notnil
 * @ingroup initializing
 * @endinternal
 */
LINPHONE_PUBLIC void linphone_core_set_ssl_config(LinphoneCore *core, void *ssl_config);

/**
 * Sets the path to a wav file used for ringing back.
 * Ringback means the ring that is heard when it's ringing at the remote party.
 * The file must be a wav 16bit linear.
 * @param core #LinphoneCore object @notnil
 * @param path The path to a wav file to be used for ringing back. @maybenil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_ringback(LinphoneCore *core, const char *path);

/**
 * Returns the path to the wav file used for ringing back.
 * @param core #LinphoneCore object @notnil
 * @return The path to the wav file used for ringing back. @maybenil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC const char *linphone_core_get_ringback(const LinphoneCore *core);

/**
 * Specify a ring back tone to be played to far end during incoming calls.
 * @param core #LinphoneCore object @notnil
 * @param ring The path to the remote ring back tone to be played. @maybenil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_remote_ringback_tone(LinphoneCore *core, const char *ring);

/**
 * Get the ring back tone played to far end during incoming calls.
 * @param core #LinphoneCore object @notnil
 * @return the path to the remote ring back tone to be played. @maybenil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC const char *linphone_core_get_remote_ringback_tone(const LinphoneCore *core);

/**
 * Enable or disable the ring play during an incoming early media call.
 * @param core #LinphoneCore object @notnil
 * @param enable A boolean value telling whether to enable ringing during an incoming early media call.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_ring_during_incoming_early_media(LinphoneCore *core, bool_t enable);

/**
 * Tells whether the ring play is enabled during an incoming early media call.
 * @param core #LinphoneCore object @notnil
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC bool_t linphone_core_get_ring_during_incoming_early_media(const LinphoneCore *core);

LINPHONE_PUBLIC LinphoneStatus linphone_core_preview_ring(LinphoneCore *core,
                                                          const char *ring,
                                                          LinphoneCoreCbFunc func,
                                                          void *userdata);

/**
 * Returns the MSFactory (mediastreamer2 factory) used by the #LinphoneCore to control mediastreamer2 library.
 **/
LINPHONE_PUBLIC MSFactory *linphone_core_get_ms_factory(LinphoneCore *core);

/**
 * Plays an audio file to the local user.
 * This function works at any time, during calls, or when no calls are running.
 * It doesn't request the underlying audio system to support multiple playback streams.
 * @param core #LinphoneCore object @notnil
 * @param audiofile The path to an audio file in wav PCM 16 bit format @notnil
 * @return 0 on success, -1 on error
 * @ingroup misc
 **/
LINPHONE_PUBLIC LinphoneStatus linphone_core_play_local(LinphoneCore *core, const char *audiofile);

/**
 * Enables or disables echo cancellation. Value is saved and used for subsequent calls.
 * This actually controls software echo cancellation only.
 * When 'enable' is set to FALSE, software echo cancellation is disabled, but hardware one, if available, remains
 * activated. When set to TRUE, software echo cancellation is activated in either of these two conditions:
 * - hardware echo cancellation is not available
 * - an echo calibration procedure was run successfully, which implicitly cause the software echo canceller to be
 *preferred over hardware one.
 * @param core #LinphoneCore object @notnil
 * @param enable A boolean value telling whether echo cancellation is to be enabled or disabled.
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_enable_echo_cancellation(LinphoneCore *core, bool_t enable);

/**
 * Clears all state resulting from a previous echo canceller calibration procedure, which restores default policy and
 * settings for echo cancellation.
 * @see linphone_core_enable_echo_cancellation() and linphone_core_start_echo_canceller_calibration()
 * @param core #LinphoneCore object @notnil
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_reset_echo_cancellation_calibration(LinphoneCore *core);

/**
 * Gets the currently stored calibration delay for the software echo cancellation.
 * @param core #LinphoneCore object @notnil
 * @return the current calibration value, -1 if it failed, 0 if not done or not needed, a positive value if a software
 * echo canceller is required after running linphone_core_start_echo_canceller_calibration().
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC int linphone_core_get_echo_cancellation_calibration(const LinphoneCore *core);

/**
 * Returns TRUE if echo cancellation is enabled.
 * @param core #LinphoneCore object @notnil
 * @return A boolean value telling whether echo cancellation is enabled or disabled
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC bool_t linphone_core_echo_cancellation_enabled(const LinphoneCore *core);

/**
 * Enables or disable echo limiter.
 * "Echo limiter" refers to an algorithm that creates half-duplex conversation in order to
 * suppress echo. It is experimental and shall be used only in rare cases where
 * echo cancellation cannot perform because of non-linear speaker/mic coupling.
 * You shall not expected good audio quality with the echo limiter.
 * @param core #LinphoneCore object. @notnil
 * @param enable TRUE to enable echo limiter, FALSE to disable it.
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_enable_echo_limiter(LinphoneCore *core, bool_t enable);

/**
 * Tells whether echo limiter is enabled.
 * Enables or disable echo limiter.
 * "Echo limiter" refers to an algorithm that creates half-duplex conversation in order to
 * suppress echo. It is experimental and shall be used only in rare cases where
 * echo cancellation cannot perform because of non-linear speaker/mic coupling.
 * You shall not expected good audio quality with the echo limiter.
 * @param core #LinphoneCore object. @notnil
 * @return TRUE if the echo limiter is enabled, FALSE otherwise.
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC bool_t linphone_core_echo_limiter_enabled(const LinphoneCore *core);

/**
 * Enables the experimental software Automatic Gain Control.
 * This algorithm is very experimental, not usable in its current state.
 * Automatic Gain Control is usally provided by sound devices, typically on iOS and Android.
 * @param core #LinphoneCore object. @notnil
 * @param val a boolean value
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_enable_agc(LinphoneCore *core, bool_t val);

/**
 * Tells whether the experimental software Automatic Gain Control is activated.
 * This algorithm is very experimental, not usable in its current state.
 * @param core #LinphoneCore object. @notnil
 * @return TRUE if the AGC is enabled, FALSE otherwise.
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC bool_t linphone_core_agc_enabled(const LinphoneCore *core);

/**
 * Enables or disables the microphone.
 * This effectively enable or disable microphone (mute) for currently the running call or conference if any,
 * as well as it applies to future currently running calls or conferences.
 * @param core #LinphoneCore object @notnil
 * @param enable TRUE to enable the microphone, FALSE to disable it.
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_enable_mic(LinphoneCore *core, bool_t enable);

/**
 * Tells whether the microphone is enabled.
 * @param core #LinphoneCore object @notnil
 * @return TRUE if the microphone is enabled, FALSE if disabled.
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC bool_t linphone_core_mic_enabled(LinphoneCore *core);

/**
 * Returns the RTP transmission status for an active stream.
 * If audio is muted and config parameter rtp_no_xmit_on_audio_mute
 * has been set on then the RTP transmission is also muted.
 * @param core The #LinphoneCore. @notnil
 * @return TRUE if the RTP transmisison is muted.
 */
LINPHONE_PUBLIC bool_t linphone_core_is_rtp_muted(LinphoneCore *core);

LINPHONE_PUBLIC bool_t linphone_core_get_rtp_no_xmit_on_audio_mute(const LinphoneCore *core);

LINPHONE_PUBLIC void linphone_core_set_rtp_no_xmit_on_audio_mute(LinphoneCore *core, bool_t val);

/*******************************************************************************
 * Call log related functions                                                  *
 ******************************************************************************/

/**
 * @addtogroup call_logs
 * @{
 **/

/**
 * Get the list of call logs (past calls).
 * @param core #LinphoneCore object @notnil
 * @return A list of #LinphoneCallLog. \bctbx_list{LinphoneCallLog} @maybenil
 **/
LINPHONE_PUBLIC const bctbx_list_t *linphone_core_get_call_logs(LinphoneCore *core);

/**
 * Gets the list of call logs (past calls).
 * At the contrary of linphone_core_get_call_logs, it is your responsibility to unref the logs and free this list once
 * you are done using it. Requires ENABLE_DB_STORAGE to work.
 * @param core #LinphoneCore object. @notnil
 * @param peer_address The remote #LinphoneAddress object. @notnil
 * @param local_address The local #LinphoneAddress object @notnil
 * @return A list of #LinphoneCallLog. \bctbx_list{LinphoneCallLog} @tobefreed @maybenil
 **/
LINPHONE_PUBLIC bctbx_list_t *linphone_core_get_call_history_2(LinphoneCore *core,
                                                               const LinphoneAddress *peer_address,
                                                               const LinphoneAddress *local_address);

/**
 * Gets the latest outgoing call log.
 * Conference calls are not returned by this function!
 * Requires ENABLE_DB_STORAGE to work.
 * @param core #LinphoneCore object @notnil
 * @return The last outgoing call log if any. @maybenil
 **/
LINPHONE_PUBLIC LinphoneCallLog *linphone_core_get_last_outgoing_call_log(LinphoneCore *core);

/**
 * Gets the call log matching the call id, or NULL if can't be found.
 * @param core #LinphoneCore object @notnil
 * @param call_id Call id of the call log to find @notnil
 * @return A call log matching the call id if any. @maybenil @tobefreed
 **/
LINPHONE_PUBLIC LinphoneCallLog *linphone_core_find_call_log_from_call_id(LinphoneCore *core, const char *call_id);

/**
 * Gets the call log matching the call id, or NULL if can't be found.
 * @param core #LinphoneCore object @notnil
 * @param call_id Call id of the call log to find @notnil
 * @param limit Search limit of the most recent call logs to find @notnil
 * @return A call log matching the call id if any. @maybenil @tobefreed
 **/
LINPHONE_PUBLIC LinphoneCallLog *linphone_core_find_call_log(LinphoneCore *core, const char *call_id, int limit);

/**
 * Erases the call log list.
 * @param core #LinphoneCore object @notnil
 **/
LINPHONE_PUBLIC void linphone_core_clear_call_logs(LinphoneCore *core);

/**
 * Gets the number of missed calls.
 * Once checked, this counter can be reset with linphone_core_reset_missed_calls_count().
 * @param core #LinphoneCore object. @notnil
 * @return The number of missed calls.
 **/
LINPHONE_PUBLIC int linphone_core_get_missed_calls_count(LinphoneCore *core);

/**
 * Resets the counter of missed calls.
 * @param core #LinphoneCore object. @notnil
 **/
LINPHONE_PUBLIC void linphone_core_reset_missed_calls_count(LinphoneCore *core);

/**
 * Removes a specific call log from call history list.
 * This function destroys the call log object. It must not be accessed anymore by the application after calling this
 *function.
 * @param core #LinphoneCore object @notnil
 * @param call_log #LinphoneCallLog object to remove. @notnil
 **/
LINPHONE_PUBLIC void linphone_core_remove_call_log(LinphoneCore *core, LinphoneCallLog *call_log);

/**
 * Migrates the call logs from the linphonerc to the database if not done yet
 * @ingroup initializing
 * @param core the #LinphoneCore. @notnil
 **/
LINPHONE_PUBLIC void linphone_core_migrate_logs_from_rc_to_db(LinphoneCore *core);

/**
 * @}
 **/

/**
 * Tells whether VCARD support is builtin.
 * @return TRUE if VCARD is supported, FALSE otherwise.
 * @ingroup misc
 */
LINPHONE_PUBLIC bool_t linphone_core_vcard_supported(void);

/**
 * Test if video is supported
 * @param core the #LinphoneCore @notnil
 * @return TRUE if the library was built with video support, FALSE otherwise
 * @ingroup misc
 **/
LINPHONE_PUBLIC bool_t linphone_core_video_supported(LinphoneCore *core);

/**
 * Returns TRUE if either capture or display is enabled, FALSE otherwise.
 * same as  ( #linphone_core_video_capture_enabled() | #linphone_core_video_display_enabled() )
 * @ingroup media_parameters
 * @param core the #LinphoneCore @notnil
 * @return TRUE if either capture or display is enabled, FALSE otherwise.
 **/
LINPHONE_PUBLIC bool_t linphone_core_video_enabled(LinphoneCore *core);

/**
 * Enable or disable video capture.
 *
 * This function does not have any effect during calls. It just indicates the #LinphoneCore to
 * initiate future calls with video capture or not.
 * @param core #LinphoneCore object. @notnil
 * @param enable TRUE to enable video capture, FALSE to disable it.
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_enable_video_capture(LinphoneCore *core, bool_t enable);

/**
 * Enable or disable video display.
 *
 * This function does not have any effect during calls. It just indicates the #LinphoneCore to
 * initiate future calls with video display or not.
 * @param core #LinphoneCore object. @notnil
 * @param enable TRUE to enable video display, FALSE to disable it.
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_enable_video_display(LinphoneCore *core, bool_t enable);

/**
 * Enable or disable video source reuse when switching from preview to actual video call.
 *
 * This source reuse is useful when you always display the preview, even before calls are initiated.
 * By keeping the video source for the transition to a real video call, you will smooth out the
 * source close/reopen cycle.
 *
 * This function does not have any effect during calls. It just indicates the #LinphoneCore to
 * initiate future calls with video source reuse or not.
 * Also, at the end of a video call, the source will be closed whatsoever for now.
 * @param core #LinphoneCore object @notnil
 * @param enable TRUE to enable video source reuse. FALSE to disable it for subsequent calls.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_enable_video_source_reuse(LinphoneCore *core, bool_t enable);

/**
 * Tells whether video capture is enabled.
 * @param core #LinphoneCore object. @notnil
 * @return TRUE if video capture is enabled, FALSE if disabled.
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC bool_t linphone_core_video_capture_enabled(LinphoneCore *core);

/**
 * Tells whether video display is enabled.
 * @param core #LinphoneCore object. @notnil
 * @return TRUE if video display is enabled, FALSE if disabled.
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC bool_t linphone_core_video_display_enabled(LinphoneCore *core);

/**
 * Increment refcount.
 * @param policy #LinphoneVideoActivationPolicy object @notnil
 * @return the same #LinphoneVideoActivationPolicy object @notnil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC LinphoneVideoActivationPolicy *
linphone_video_activation_policy_ref(LinphoneVideoActivationPolicy *policy);

/**
 * Decrement refcount and possibly free the object.
 * @param policy #LinphoneVideoActivationPolicy object @notnil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_video_activation_policy_unref(LinphoneVideoActivationPolicy *policy);

/**
 * Gets the user data in the #LinphoneVideoActivationPolicy object
 * @param policy the #LinphoneVideoActivationPolicy @notnil
 * @return the user data @maybenil
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void *linphone_video_activation_policy_get_user_data(const LinphoneVideoActivationPolicy *policy);

/**
 * Sets the user data in the #LinphoneVideoActivationPolicy object
 * @param policy the #LinphoneVideoActivationPolicy object @notnil
 * @param user_data the user data @maybenil
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_video_activation_policy_set_user_data(LinphoneVideoActivationPolicy *policy,
                                                                    void *user_data);

/**
 * Instantiates a new #LinphoneVideoActivationPolicy object with same values as the source
 * @param policy the #LinphoneVideoActivationPolicy object to clone @notnil
 * @returns the newly created #LinphoneVideoActivationPolicy object @notnil
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC LinphoneVideoActivationPolicy *
linphone_video_activation_policy_clone(const LinphoneVideoActivationPolicy *policy);

/**
 * Gets the value for the automatically accept video policy
 * @param policy the #LinphoneVideoActivationPolicy object @notnil
 * @return whether or not to automatically accept video requests is enabled
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC bool_t
linphone_video_activation_policy_get_automatically_accept(const LinphoneVideoActivationPolicy *policy);

/**
 * Gets the value for the automatically accept video direction
 * @param policy the #LinphoneVideoActivationPolicy object @notnil
 * @return the #LinphoneMediaDirection that will be set for video stream if automatically accepted.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC LinphoneMediaDirection
linphone_video_activation_policy_get_automatically_accept_direction(const LinphoneVideoActivationPolicy *policy);

/**
 * Gets the value for the automatically initiate video policy
 * @param policy the #LinphoneVideoActivationPolicy object @notnil
 * @return whether or not to automatically initiate video calls is enabled
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC bool_t
linphone_video_activation_policy_get_automatically_initiate(const LinphoneVideoActivationPolicy *policy);

/**
 * Sets the value for the automatically accept video policy
 * @param policy the #LinphoneVideoActivationPolicy object @notnil
 * @param enable whether or not to enable automatically accept video requests
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_video_activation_policy_set_automatically_accept(LinphoneVideoActivationPolicy *policy,
                                                                               bool_t enable);

/**
 * Sets the value for the automatically accept direction
 * @param policy the #LinphoneVideoActivationPolicy object @notnil
 * @param direction the #LinphoneMediaDirection desired for video stream when automatically accepted.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void
linphone_video_activation_policy_set_automatically_accept_direction(LinphoneVideoActivationPolicy *policy,
                                                                    LinphoneMediaDirection direction);

/**
 * Sets the value for the automatically initiate video policy
 * @param policy the #LinphoneVideoActivationPolicy object @notnil
 * @param enable whether or not to enable automatically initiate video calls
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_video_activation_policy_set_automatically_initiate(LinphoneVideoActivationPolicy *policy,
                                                                                 bool_t enable);

/**
 * Sets the default policy for video.
 * This policy defines whether:
 * - video shall be initiated by default for outgoing calls
 * - video shall be accepted by default for incoming calls
 * - if video shall be accepted by default, what direction do we want (send/receive, receive only, etc...)
 *
 * @param core the #LinphoneCore object @notnil
 * @param policy The #LinphoneVideoActivationPolicy to use @notnil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_video_activation_policy(LinphoneCore *core,
                                                               LinphoneVideoActivationPolicy *policy);

/**
 * Get the default policy for video.
 * See linphone_core_set_video_activation_policy() for more details.
 * @param core #LinphoneCore object @notnil
 * @return The currently used video policy @notnil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC const LinphoneVideoActivationPolicy *
linphone_core_get_video_activation_policy(const LinphoneCore *core);

/**
 * Set the preferred video definition for the stream that is captured and sent to the remote party.
 * All standard video definitions are accepted on the receive path.
 * @param core #LinphoneCore object @notnil
 * @param video_definition #LinphoneVideoDefinition object @notnil
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_preferred_video_definition(LinphoneCore *core,
                                                                  LinphoneVideoDefinition *video_definition);

/**
 * Sets the preferred video definition by its name.
 * Call #linphone_factory_get_supported_video_definitions() to have a list of supported video definitions.
 *
 * @ingroup media_parameters
 * @param core The #LinphoneCore object @notnil
 * @param name The name of the definition to set @notnil
 **/
LINPHONE_PUBLIC void linphone_core_set_preferred_video_definition_by_name(LinphoneCore *core, const char *name);

/**
 * Set the video definition for the captured (preview) video.
 * This method is for advanced usage where a video capture must be set independently of the definition of the stream
 * actually sent through the call. This allows for example to have the preview window in High Definition  even if due to
 * bandwidth constraint the sent video definition is small. Using this feature increases the CPU consumption, since a
 * rescaling will be done internally.
 * @param core #LinphoneCore object @notnil
 * @param video_definition #LinphoneVideoDefinition object @maybenil
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_preview_video_definition(LinphoneCore *core,
                                                                LinphoneVideoDefinition *video_definition);

/**
 * * Set the video definition for the captured (preview) video by its name.
 * Call #linphone_factory_get_supported_video_definitions() to have a list of supported video definitions.
 * Video resolution names are: qcif, svga, cif, vga, 4cif, svga ...
 * @ingroup media_parameters
 * @param core The #LinphoneCore object @notnil
 * @param name The name of the definition to set @notnil
 **/
LINPHONE_PUBLIC void linphone_core_set_preview_video_definition_by_name(LinphoneCore *core, const char *name);

/**
 * Get the definition of the captured video.
 * @param core #LinphoneCore object @notnil
 * @return The captured #LinphoneVideoDefinition if it was previously set by
 * linphone_core_set_preview_video_definition(), otherwise a 0x0 LinphoneVideoDefinition. @maybenil
 * @see linphone_core_set_preview_video_definition()
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC const LinphoneVideoDefinition *linphone_core_get_preview_video_definition(const LinphoneCore *core);

/**
 * Get the effective video definition provided by the camera for the captured video.
 * When preview is disabled or not yet started this function returns a 0x0 video definition.
 * @param core #LinphoneCore object @notnil
 * @return The captured #LinphoneVideoDefinition. @notnil
 * @ingroup media_parameters
 * @see linphone_core_set_preview_video_definition()
 */
LINPHONE_PUBLIC LinphoneVideoDefinition *linphone_core_get_current_preview_video_definition(LinphoneCore *core);

/**
 * Get the preferred video definition for the stream that is captured and sent to the remote party.
 * @param core #LinphoneCore object @notnil
 * @return The preferred #LinphoneVideoDefinition @notnil
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC const LinphoneVideoDefinition *linphone_core_get_preferred_video_definition(const LinphoneCore *core);

/**
 * Set the preferred frame rate for video.
 * Based on the available bandwidth constraints and network conditions, the video encoder
 * remains free to lower the framerate. There is no warranty that the preferred frame rate be the actual framerate.
 * used during a call. Default value is 0, which means "use encoder's default fps value".
 * @param core the #LinphoneCore @notnil
 * @param fps the target frame rate in number of frames per seconds.
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_preferred_framerate(LinphoneCore *core, float fps);

/**
 * Returns the preferred video framerate, previously set by linphone_core_set_preferred_framerate().
 * @param core the #LinphoneCore. @notnil
 * @return frame rate in number of frames per seconds.
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC float linphone_core_get_preferred_framerate(LinphoneCore *core);

/**
 * Call generic OpenGL render for a given core.
 * @param core The core. @notnil
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_preview_ogl_render(const LinphoneCore *core);

/**
 * Controls video preview enablement.
 * @param core #LinphoneCore object @notnil
 * @param enable A boolean value telling whether the video preview is to be shown
 * Video preview refers to the action of displaying the local webcam image
 * to the user while not in call.
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_enable_video_preview(LinphoneCore *core, bool_t enable);

/**
 * Tells whether video preview is enabled.
 * @param core #LinphoneCore object @notnil
 * @return A boolean value telling whether video preview is enabled
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC bool_t linphone_core_video_preview_enabled(const LinphoneCore *core);

/**
 * Controls QRCode scanning enablement. When enabled, several QRCodes can be decoded.
 * The recognized QR code are notified through the LinphoneCoreCbs interface (see linphone_core_cbs_set_qrcode_found())
 * @param core LinphoneCore object @notnil
 * @param enable A boolean value telling whether to enable QRCode in the preview.
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_enable_qrcode_video_preview(LinphoneCore *core, bool_t enable);

/**
 * Set the rectangle where the decoder will search a QRCode
 * @param core LinphoneCore* object @notnil
 * @param x axis
 * @param y axis
 * @param w width
 * @param h height
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void
linphone_core_set_qrcode_decode_rect(LinphoneCore *core, const int x, const int y, const int w, const int h);

/**
 * Tells whether QRCode is enabled in the preview.
 * @param core LinphoneCore object @notnil
 * @return A boolean value telling whether QRCode is enabled in the preview.
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC bool_t linphone_core_qrcode_video_preview_enabled(const LinphoneCore *core);

/**
 * Takes a photo of currently from capture device and write it into a jpeg file.
 * Note that the snapshot is asynchronous, an application shall not assume that the file is created when the function
 *returns.
 * @ingroup misc
 * @param core the #LinphoneCore. @notnil
 * @param file a path where to write the jpeg content. @notnil
 * @return 0 if successful, -1 otherwise (typically if jpeg format is not supported).
 **/
LINPHONE_PUBLIC LinphoneStatus linphone_core_take_preview_snapshot(LinphoneCore *core, const char *file);

/**
 * Enables or disable self view during calls.
 * @param core #LinphoneCore object @notnil
 * @param enable A boolean value telling whether to enable self view
 * Self-view refers to having local webcam image inserted in corner
 * of the video window during calls.
 * This function works at any time, including during calls.
 * @ingroup media_parameters
 * @deprecated 04/09/2024 Prefer using linphone_core_set_native_preview_window_id() to assign a view to render the local
 *image.
 **/
LINPHONE_PUBLIC void linphone_core_enable_self_view(LinphoneCore *core, bool_t enable);

/**
 * Tells whether video self view during call is enabled or not.
 * @param core #LinphoneCore object @notnil
 * @return A boolean value telling whether self view is enabled
 * @see linphone_core_enable_self_view() for details.
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC bool_t linphone_core_self_view_enabled(const LinphoneCore *core);

/**
 * Update detection of camera devices.
 *
 * Use this function when the application is notified of USB plug events, so that
 * list of available hardwares for video capture is updated.
 * @param core #LinphoneCore object. @notnil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_reload_video_devices(LinphoneCore *core);

/**
 * Gets the list of the available video capture devices.
 * @param core #LinphoneCore object @notnil
 * @return An unmodifiable array of strings contanining the names of the available video capture devices that is NULL
 *terminated. \bctbx_list{char *} @maybenil @tobefreed
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC bctbx_list_t *linphone_core_get_video_devices_list(const LinphoneCore *core);

/**
 * Sets the active video device.
 * @param core #LinphoneCore object @notnil
 * @param id The name of the video device to use as returned by linphone_core_get_video_devices() @maybenil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC LinphoneStatus linphone_core_set_video_device(LinphoneCore *core, const char *id);

/**
 * Returns the name of the currently active video device.
 * @param core #LinphoneCore object @notnil
 * @return The name of the currently active video device. @maybenil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC const char *linphone_core_get_video_device(const LinphoneCore *core);

/**
 * Set the path to the image file to stream when "Static picture" is set as the video device.
 * @param core #LinphoneCore object. @notnil
 * @param path The path to the image file to use. @maybenil
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC LinphoneStatus linphone_core_set_static_picture(LinphoneCore *core, const char *path);

/**
 * Get the path to the image file streamed when "Static picture" is set as the video device.
 * @param core #LinphoneCore object. @notnil
 * @return The path to the image file streamed when "Static picture" is set as the video device. @maybenil
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC const char *linphone_core_get_static_picture(LinphoneCore *core);

/**
 * Set the frame rate for static picture.
 * @param core #LinphoneCore object. @notnil
 * @param fps The new frame rate to use for static picture.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC LinphoneStatus linphone_core_set_static_picture_fps(LinphoneCore *core, float fps);

/**
 * Get the frame rate for static picture
 * @param core #LinphoneCore object. @notnil
 * @return The frame rate used for static picture.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC float linphone_core_get_static_picture_fps(LinphoneCore *core);

/**
 * Create a Window ID from the current call. Available for MSQOGL and MSOGL.
 * see linphone_core_set_native_video_window_id() for details about `window_id`
 *
 * When MSQOgl can be used for the creation:
 ** linphone_core_create_native_video_window_id_2() returns a #QQuickFramebufferObject::Renderer. This object must be
 *returned by your QQuickFramebufferObject::createRenderer() overload for Qt.
 ** linphone_core_set_native_video_window_id() must be called with this object after the creation.
 ** Note : Qt blocks GUI thread when calling createRenderer(), so it is safe to call linphone functions there if needed.
 *
 * A context can be used to prevent Linphone from allocating the container (#MSOglContextInfo for MSOGL). NULL if not used.
 *
 * @param core #LinphoneCore object @notnil
 * @param context preallocated Window ID (Used only for MSOGL) @maybenil
 * @return The created Window ID @maybenil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void *linphone_core_create_native_video_window_id_2(const LinphoneCore *core, void *context);


/**
 * Create a native window handle for the video window from the current call.
 * see linphone_core_create_native_video_window_id_2() for details
 *
 * @param core #LinphoneCore object @notnil
 * @return The native window handle of the video window. @maybenil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void *linphone_core_create_native_video_window_id(const LinphoneCore *core);

/**
 * Get the native window handle of the video window.
 * see #linphone_core_set_native_video_window_id for details about `window_id`
 *
 * There is a special case for Qt :
 ** linphone_core_get_native_video_window_id() returns a #QQuickFramebufferObject::Renderer.
 ** Note : Qt blocks GUI thread when calling createRenderer(), so it is safe to call linphone functions there if needed.
 *
 * @param core #LinphoneCore object @notnil
 * @return The native window handle of the video window. @maybenil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void *linphone_core_get_native_video_window_id(const LinphoneCore *core);

/**
 * @ingroup media_parameters
 * For MacOS, Linux, Windows: core will create its own window
 */
#define LINPHONE_VIDEO_DISPLAY_AUTO (void *)(0)

/**
 * @ingroup media_parameters
 * For MacOS, Linux, Windows: do nothing
 */
#define LINPHONE_VIDEO_DISPLAY_NONE (void *)(-1)

/**
 * @ingroup media_parameters
 * Set the native video window id where the video is to be displayed.
 *
 * On Desktop platforms(MacOS, Linux, Windows), the display filter is "MSOGL" by default. That means :
 ** If `window_id` is not set or set to #LINPHONE_VIDEO_DISPLAY_NONE, then the core will not create its own window,
 *unless the special id #LINPHONE_VIDEO_DISPLAY_AUTO is given.
 ** This is currently only supported for Linux X11 (Window type), Windows UWP (SwapChainPanel type) and Windows Win32
 *(HWND type).
 **
 ** The C# Wrapper on Windows for UWP takes directly a #SwapChainPanel without Marshalling.
 ** On other platforms, `window_id` is a #MSOglContextInfo defined in msogl.h of mediastreamer2
 ** There is a special case for Qt :
 *** The "MSQOGL" filter must be selected by using linphone_core_set_video_display_filter().
 *** Setting window id is only used to stop rendering by passing #LINPHONE_VIDEO_DISPLAY_NONE.
 *** linphone_core_get_native_video_window_id() returns a #QQuickFramebufferObject::Renderer and
 *linphone_core_create_native_video_window_id_2() creates one.
 *** After a creation, linphone_core_set_native_video_window_id() must be called with the new object.
 *
 * On mobile operating systems, #LINPHONE_VIDEO_DISPLAY_AUTO is not supported and `window_id` depends of the platform :
 ** iOS : It is a UIView.
 ** Android : It is a TextureView.
 *
 * @param core #LinphoneCore object @notnil
 * @param window_id The native window id where the remote video is to be displayed. @maybenil
 **/
LINPHONE_PUBLIC void linphone_core_set_native_video_window_id(LinphoneCore *core, void *window_id);

/**
 * Create a Window ID for the video preview window. Available for MSQOGL and MSOGL.
 * see linphone_core_set_native_video_window_id() for details about `window_id`
 *
 * MSQOgl can be used for the creation.
 ** linphone_core_create_native_preview_window_id_2() returns a #QQuickFramebufferObject::Renderer. This object must be
 *returned by your QQuickFramebufferObject::createRenderer() overload for Qt.
 ** linphone_core_set_native_preview_window_id_2() must be called with this object after the creation.
 ** Note : Qt blocks GUI thread when calling createRenderer(), so it is safe to call linphone functions there if needed.
 *
 * A context can be used to prevent Linphone from allocating the container (#MSOglContextInfo for MSOGL). NULL if not used.
 *
 * @param core #LinphoneCore object @notnil
 * @param context preallocated Window ID (Used only for MSOGL) @maybenil
 * @return The created Window ID. @maybenil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void *linphone_core_create_native_preview_window_id_2(LinphoneCore *core, void *context);

/**
 * Create a native window handle for the video preview window.
 * see linphone_core_create_native_preview_window_id_2() for details
 *
 * @param core #LinphoneCore object @notnil
 * @return The native window handle of the video preview window. @maybenil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void *linphone_core_create_native_preview_window_id(LinphoneCore *core);

/**
 * Get the native window handle of the video preview window.
 * see linphone_core_set_native_video_window_id() for details about `window_id`
 *
 * There is a special case for Qt :
 ** linphone_core_get_native_preview_window_id() returns a #QQuickFramebufferObject::Renderer.
 ** Note : Qt blocks GUI thread when calling createRenderer(), so it is safe to call linphone functions there if needed.
 *
 * @param core #LinphoneCore object @notnil
 * @return The native window handle of the video preview window. @maybenil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void *linphone_core_get_native_preview_window_id(LinphoneCore *core);

/**
 * Set the native window id where the preview video (local camera) is to be displayed.
 * This has to be used in conjunction with linphone_core_use_preview_window().
 * see linphone_core_set_native_video_window_id() for general details about `window_id`
 *
 * On Android : #org.linphone.mediastream.video.capture.CaptureTextureView is used for
 *linphone_core_set_native_preview_window_id(). It is inherited from #TextureView and takes care of rotating the
 *captured image from the camera and scale it to keep it's ratio.
 *
 * @param core #LinphoneCore object @notnil
 * @param window_id The native window id where the preview video is to be displayed. @maybenil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_native_preview_window_id(LinphoneCore *core, void *window_id);

/**
 * Tells the core to use a separate window for local camera preview video, instead of
 * inserting local view within the remote video window.
 * @param core #LinphoneCore object. @notnil
 * @param yesno TRUE to use a separate window, FALSE to insert the preview in the remote video window.
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_use_preview_window(LinphoneCore *core, bool_t yesno);

/**
 * Gets the current device orientation.
 * @param core #LinphoneCore object @notnil
 * @return The current device orientation
 * @ingroup media_parameters
 * @see linphone_core_set_device_rotation()
 */
LINPHONE_PUBLIC int linphone_core_get_device_rotation(LinphoneCore *core);

/**
 * Tells the core the device current orientation. This can be used by capture filters
 * on mobile devices to select between portrait/landscape mode and to produce properly
 * oriented images. The exact meaning of the value in rotation if left to each device
 * specific implementations.
 * IOS supported values are 0 for UIInterfaceOrientationPortrait and 270 for UIInterfaceOrientationLandscapeRight.
 * @param core #LinphoneCore object @notnil
 * @param rotation The orientation to use
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_device_rotation(LinphoneCore *core, int rotation);

/**
 * Sets the whitebalance of the camera (currently only supported by Android).
 * @param core #LinphoneCore object. @notnil
 * @param whitebalance The whitebalance of the camera.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC LinphoneStatus linphone_core_set_camera_whitebalance(LinphoneCore *core, int whitebalance);

/**
 * Gets the whitebalance of the camera (currently only supported by Android).
 * @param core #LinphoneCore object. @notnil
 * @return The whitebalance of the camera, default is -1 (disabled).
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC int linphone_core_get_camera_whitebalance(LinphoneCore *core);

/**
 * Get the camera sensor rotation.
 *
 * This is needed on some mobile platforms to get the number of degrees the camera sensor
 * is rotated relative to the screen.
 * @param core The linphone core related to the operation @notnil
 * @ingroup media_parameters
 * @return The camera sensor rotation in degrees (0 to 360) or -1 if it could not be retrieved
 */
LINPHONE_PUBLIC int linphone_core_get_camera_sensor_rotation(LinphoneCore *core);

/**
 * Start or stop streaming video in case of embedded window.
 * Can be used to disable video showing to free XV port
 * @param core #LinphoneCore object @notnil
 * @param show TRUE to start video streaming, FALSE to stop it
 **/
void linphone_core_show_video(LinphoneCore *core, bool_t show);

/**
 * Ask the core to stream audio from and to files, instead of using the soundcard.
 * @param core #LinphoneCore object @notnil
 * @param yesno A boolean value asking to stream audio from and to files or not.
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_use_files(LinphoneCore *core, bool_t yesno);

/**
 * Gets whether linphone is currently streaming audio from and to files, rather
 * than using the soundcard.
 * @param core #LinphoneCore object @notnil
 * @return A boolean value representing whether linphone is streaming audio from and to files or not.
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC bool_t linphone_core_get_use_files(LinphoneCore *core);

/**
 * Get the wav file that is played when files are used instead of soundcards (see linphone_core_set_use_files()).
 *
 * The file is a 16 bit linear wav file.
 * @param core #LinphoneCore object @notnil
 * @return The path to the file that is played when files are used instead of soundcards. @maybenil
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC const char *linphone_core_get_play_file(const LinphoneCore *core);

/**
 * Sets a wav file to be played when files are used instead of soundcards (see linphone_core_set_use_files()).
 *
 * The file must be a 16 bit linear wav file.
 * @param core #LinphoneCore object @notnil
 * @param file The path to the file to be played when files are used instead of soundcards. @maybenil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_play_file(LinphoneCore *core, const char *file);

/**
 * Get the wav file where incoming stream is recorded,
 * when files are used instead of soundcards (see linphone_core_set_use_files()).
 *
 * This feature is different from call recording (linphone_call_params_set_record_file())
 * The file is a 16 bit linear wav file.
 * @param core #LinphoneCore object @notnil
 * @return The path to the file where incoming stream is recorded. @maybenil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC const char *linphone_core_get_record_file(const LinphoneCore *core);

/**
 * Sets a wav file where incoming stream is to be recorded,
 * when files are used instead of soundcards (see linphone_core_set_use_files()).
 *
 * This feature is different from call recording (linphone_call_params_set_record_file())
 * The file will be a 16 bit linear wav file.
 * @param core #LinphoneCore object @notnil
 * @param file The path to the file where incoming stream is to be recorded. @maybenil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_record_file(LinphoneCore *core, const char *file);

/**
 * Get the wav file played when putting a call on hold with linphone_call_pause()
 *
 * The file is a 16 bit linear file.
 * @param core #LinphoneCore object @notnil
 * @return The path to the file that is played when putting a call on hold. @maybenil
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC const char *linphone_core_get_call_on_hold_music_file(const LinphoneCore *core);

/**
 * Sets a wav file to be played when putting somebody on hold (see linphone_core_set_use_files()).
 *
 * The file must be a 16 bit linear wav file.
 * @param core #LinphoneCore object @notnil
 * @param file The path to the file to be played when putting somebody on hold. @maybenil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_call_on_hold_music_file(LinphoneCore *core, const char *file);

/**
 * Plays a dtmf sound to the local user.
 * @param core #LinphoneCore object @notnil
 * @param dtmf DTMF to play ['0'..'16'] | '#' | '#'
 * @param duration_ms Duration in ms, -1 means play until next further call to #linphone_core_stop_dtmf()
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_play_dtmf(LinphoneCore *core, char dtmf, int duration_ms);

/**
 * Stops playing a dtmf started by linphone_core_play_dtmf().
 * @param core #LinphoneCore object @notnil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_stop_dtmf(LinphoneCore *core);

LINPHONE_PUBLIC int linphone_core_get_current_call_duration(const LinphoneCore *core);

/**
 * Returns the maximum transmission unit size in bytes.
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC int linphone_core_get_mtu(const LinphoneCore *core);

/**
 * Sets the maximum transmission unit size in bytes.
 * This information is useful for sending RTP packets.
 * Default value is 1500.
 * @param core #LinphoneCore object @notnil
 * @param mtu The MTU in bytes
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_mtu(LinphoneCore *core, int mtu);

/**
 * Enable or disable the UPDATE method support
 * @param core #LinphoneCore object @notnil
 * @param value Enable or disable it
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_enable_sip_update(const LinphoneCore *core, int value);

/**
 * Enable the Session Timers support
 * @param core #LinphoneCore object @notnil
 * @param enabled Enable or disable it
 * @ingroup media_parameters
 * @deprecated 16/12/2021 Use linphone_core_enable_session_expires() instead.
 **/
LINPHONE_PUBLIC void linphone_core_set_session_expires_enabled(const LinphoneCore *core, bool_t enabled);

/**
 * Enable the Session Timers support
 * @param core #LinphoneCore object @notnil
 * @param enabled Enable or disable it
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_enable_session_expires(const LinphoneCore *core, bool_t enabled);

/**
 * Check if the Session Timers feature is enabled
 * @param core #LinphoneCore object @notnil
 * @ingroup media_parameters
 * @deprecated 16/12/2021 Use linphone_core_session_expires_enabled() instead.
 **/
LINPHONE_PUBLIC bool_t linphone_core_get_session_expires_enabled(const LinphoneCore *core);

/**
 * Check if the Session Timers feature is enabled
 * @param core #LinphoneCore object @notnil
 * @return TRUE if session timers are enabled, FALSE otherwise
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC bool_t linphone_core_session_expires_enabled(const LinphoneCore *core);

/**
 * Sets the session expires value, 0 by default
 * @param core #LinphoneCore object @notnil
 * @param expires The session expires value
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_session_expires_value(const LinphoneCore *core, int expires);

/**
 * Returns the session expires value
 * @param core #LinphoneCore object @notnil
 * @return The session timer expire value
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC int linphone_core_get_session_expires_value(const LinphoneCore *core);

/**
 * Sets the session expires refresher value
 * @param core #LinphoneCore object @notnil
 * @param refresher The #LinphoneSessionExpiresRefresher configuration value
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_session_expires_refresher_value(const LinphoneCore *core,
                                                                       LinphoneSessionExpiresRefresher refresher);

/**
 * Returns the session expires refresher value
 * @param core #LinphoneCore object @notnil
 * @return The #LinphoneSessionExpiresRefresher configuration refresher value
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC LinphoneSessionExpiresRefresher
linphone_core_get_session_expires_refresher_value(const LinphoneCore *core);

/**
 * Sets the session expires minSE value, forced to a minimum of 90 by default
 * @param core #LinphoneCore object @notnil
 * @param min The minSE value
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_session_expires_min_value(const LinphoneCore *core, int min);

/**
 * Returns the session expires min value, 90 by default
 * @param core #LinphoneCore object @notnil
 * @return The minSE value
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC int linphone_core_get_session_expires_min_value(const LinphoneCore *core);

/**
 * This method is called by the application to notify the linphone core library when network is reachable.
 * Calling this method with true trigger linphone to initiate a registration process for all proxies.
 * Calling this method disables the automatic network detection mode. It means you must call this method after each
 * network state changes.
 * @ingroup network_parameters
 * @param core the #LinphoneCore object @notnil
 * @param reachable TRUE if network is reachable, FALSE otherwise
 */
LINPHONE_PUBLIC void linphone_core_set_network_reachable(LinphoneCore *core, bool_t reachable);

/**
 * return network state either as positioned by the application or by linphone itself.
 * @ingroup network_parameters
 * @param core the #LinphoneCore object @notnil
 * @return TRUE if network is reachable, FALSE otherwise
 */
LINPHONE_PUBLIC bool_t linphone_core_is_network_reachable(LinphoneCore *core);

/**
 * This method is called by the application to notify the linphone core library when the SIP network is reachable.
 * This is for advanced usage, when SIP and RTP layers are required to use different interfaces.
 * Most applications just need linphone_core_set_network_reachable().
 * @ingroup network_parameters
 * @param core the #LinphoneCore object @notnil
 * @param reachable TRUE if network is reachable, FALSE otherwise
 */
LINPHONE_PUBLIC void linphone_core_set_sip_network_reachable(LinphoneCore *core, bool_t reachable);

/**
 * This method is called by the application to notify the linphone core library when the media (RTP) network is
 * reachable. This is for advanced usage, when SIP and RTP layers are required to use different interfaces. Most
 * applications just need linphone_core_set_network_reachable().
 * @ingroup network_parameters
 * @param core the #LinphoneCore object @notnil
 * @param reachable TRUE if network is reachable, FALSE otherwise
 */
LINPHONE_PUBLIC void linphone_core_set_media_network_reachable(LinphoneCore *core, bool_t reachable);

/**
 * Enables signaling keep alive, small udp packet sent periodically to keep udp NAT association.
 * @param core #LinphoneCore object @notnil
 * @param enable A boolean value telling whether signaling keep alive is to be enabled
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC void linphone_core_enable_keep_alive(LinphoneCore *core, bool_t enable);

/**
 * Is signaling keep alive enabled.
 * @param core #LinphoneCore object @notnil
 * @return A boolean value telling whether signaling keep alive is enabled
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC bool_t linphone_core_keep_alive_enabled(LinphoneCore *core);

/**
 * Retrieves the user pointer that was given to linphone_core_new()
 * @param core #LinphoneCore object @notnil
 * @return The user data associated with the #LinphoneCore object. @maybenil
 * @ingroup initializing
 **/
LINPHONE_PUBLIC void *linphone_core_get_user_data(const LinphoneCore *core);

/**
 * Associate a user pointer to the linphone core.
 * @param core #LinphoneCore object @notnil
 * @param user_data The user data to associate with the #LinphoneCore object. @maybenil
 * @ingroup initializing
 **/
LINPHONE_PUBLIC void linphone_core_set_user_data(LinphoneCore *core, void *user_data);

/**
 * This method is called by the application to notify the linphone core library when it enters background mode.
 * @ingroup misc
 * @param core the #LinphoneCore @notnil
 */
LINPHONE_PUBLIC void linphone_core_enter_background(LinphoneCore *core);

/**
 * This method is called by the application to notify the linphone core library when it enters foreground mode.
 * @ingroup misc
 * @param core the #LinphoneCore @notnil
 */
LINPHONE_PUBLIC void linphone_core_enter_foreground(LinphoneCore *core);

/**
 * Returns the config object used to manage the storage (config) file.
 * @param core #LinphoneCore object @notnil
 * The application can use the #LinphoneConfig object to insert its own private
 * sections and pairs of key=value in the configuration file.
 * @return a #LinphoneConfig object. @notnil
 * @ingroup misc
 **/
LINPHONE_PUBLIC LinphoneConfig *linphone_core_get_config(const LinphoneCore *core);

/**
 * Create a #LinphoneConfig object from a user config file.
 * @param core #LinphoneCore object @notnil
 * @param filename The filename of the config file to read to fill the instantiated #LinphoneConfig @maybenil
 * @return a #LinphoneConfig object. @notnil
 * @ingroup misc
 */
LINPHONE_PUBLIC LinphoneConfig *linphone_core_create_config(LinphoneCore *core, const char *filename);

/**
 * Returns the list of registered SipSetup (linphonecore plugins)
 */
LINPHONE_PUBLIC const bctbx_list_t *linphone_core_get_sip_setups(LinphoneCore *core);

/*for advanced users:*/
typedef RtpTransport *(*LinphoneCoreRtpTransportFactoryFunc)(void *data, int port);
struct _LinphoneRtpTransportFactories {
	LinphoneCoreRtpTransportFactoryFunc audio_rtp_func;
	void *audio_rtp_func_data;
	LinphoneCoreRtpTransportFactoryFunc audio_rtcp_func;
	void *audio_rtcp_func_data;
	LinphoneCoreRtpTransportFactoryFunc video_rtp_func;
	void *video_rtp_func_data;
	LinphoneCoreRtpTransportFactoryFunc video_rtcp_func;
	void *video_rtcp_func_data;
};
typedef struct _LinphoneRtpTransportFactories LinphoneRtpTransportFactories;

void linphone_core_set_rtp_transport_factories(LinphoneCore *core, LinphoneRtpTransportFactories *factories);

/**
 * Gets the number of Call
 * @param core #LinphoneCore object @notnil
 * @return The current number of calls
 * @ingroup call_control
 **/
LINPHONE_PUBLIC int linphone_core_get_calls_nb(const LinphoneCore *core);

/**
 * Gets the current list of calls.
 * Note that this list is read-only and might be changed by the core after a function call to linphone_core_iterate().
 * Similarly the #LinphoneCall objects inside it might be destroyed without prior notice.
 * To hold references to #LinphoneCall object into your program, you must use linphone_call_ref().
 * @param core The #LinphoneCore object @notnil
 * @return A list of #LinphoneCall \bctbx_list{LinphoneCall} @maybenil
 * @ingroup call_control
 **/
LINPHONE_PUBLIC const bctbx_list_t *linphone_core_get_calls(LinphoneCore *core);

/**
 * Gets the call by its SIP callid.
 * @param core The #LinphoneCore object @notnil
 * @param call_id of call @notnil
 * @return call #LinphoneCall, return null if there is no call find. @maybenil
 * @ingroup call_control
 **/
LINPHONE_PUBLIC LinphoneCall *linphone_core_get_call_by_callid(const LinphoneCore *core, const char *call_id);

/**
 * Returns the global state of core.
 * @param core #LinphoneCore object @notnil
 * @return a #LinphoneGlobalState enum. @notnil
 * @ingroup misc
 **/
LINPHONE_PUBLIC LinphoneGlobalState linphone_core_get_global_state(const LinphoneCore *core);

/**
 * Forces registration refresh to be initiated upon next iterate
 * @ingroup proxies
 * @param core The #LinphoneCore object @notnil
 */
LINPHONE_PUBLIC void linphone_core_refresh_registers(LinphoneCore *core);

/**
 * Sets the path to the file storing the zrtp secrets cache.
 * @param core #LinphoneCore object @notnil
 * @param file The path to the file to use to store the zrtp secrets cache. @maybenil
 * @ingroup initializing
 */
LINPHONE_PUBLIC void linphone_core_set_zrtp_secrets_file(LinphoneCore *core, const char *file);

/**
 * Gets the path to the file storing the zrtp secrets cache.
 * @param core #LinphoneCore object. @notnil
 * @return The path to the file storing the zrtp secrets cache. @maybenil
 * @ingroup initializing
 */
LINPHONE_PUBLIC const char *linphone_core_get_zrtp_secrets_file(LinphoneCore *core);

struct _zrtpCacheAccess {
	void *db; /**< points to the zrtp cache sqlite database, is cast into a void * to support cacheless build */
	bctbx_mutex_t *dbMutex; /**< the mutex used to prevent conflicting access to the database */
};
typedef struct _zrtpCacheAccess zrtpCacheAccess;

/**
 * Gets a pointer to a structure holding pointers to access zrtp/lime cache.
 * The structure will hold a sqlite db pointer and a bctoolbox mutex pointer
 *
 * @param core #LinphoneCore object. @notnil
 * @return a structure holding db pointer(NULL is cache is not available by built or runtime error) and the mutex
 * associated to it
 */
LINPHONE_PUBLIC zrtpCacheAccess linphone_core_get_zrtp_cache_access(LinphoneCore *core);

/**
 * Get the zrtp sas validation status for a peer uri.
 * Once the SAS has been validated or rejected, the status will never return to Unknown (unless you delete your cache)
 * @param core #LinphoneCore object. @notnil
 * @param addr the peer uri @notnil
 * @ingroup media_parameters
 * @return  - LinphoneZrtpPeerStatusUnknown: this uri is not present in cache OR during calls with the active device,
 * SAS never was validated or rejected
 *  		- LinphoneZrtpPeerStatusValid: the active device status is set to valid
 *  		- LinphoneZrtpPeerStatusInvalid: the active peer device status is set to invalid
 */
LINPHONE_PUBLIC LinphoneZrtpPeerStatus linphone_core_get_zrtp_status(LinphoneCore *core, const char *addr);

/**
 * Set the path to the directory storing the user's x509 certificates (used by dtls)
 * @param core #LinphoneCore object @notnil
 * @param path The path to the directory to use to store the user's certificates. @maybenil
 * @ingroup initializing
 */
LINPHONE_PUBLIC void linphone_core_set_user_certificates_path(LinphoneCore *core, const char *path);

/**
 * Get the path to the directory storing the user's certificates.
 * @param core #LinphoneCore object. @notnil
 * @return The path to the directory storing the user's certificates. @maybenil
 * @ingroup initializing
 */
LINPHONE_PUBLIC const char *linphone_core_get_user_certificates_path(LinphoneCore *core);

/**
 * Reload mediastreamer2 plugins from specified directory.
 * @param core #LinphoneCore object. @notnil
 * @param path the path from where plugins are to be loaded, pass NULL to use default (compile-time determined) plugin
 * directory. @maybenil
 * @ingroup initializing
 */
LINPHONE_PUBLIC void linphone_core_reload_ms_plugins(LinphoneCore *core, const char *path);

/**
 * @addtogroup conference
 * @{
 */

/**
 * Create a conference. Local or client conference is determinated from the 'conference_type' variable in the 'misc'
 * section of the configuration, or by the factory address parameter. See
 * linphone_conference_params_set_conference_factory_address() for more details.
 * @param core The #LinphoneCore instance where the conference will be created inside. @notnil
 * @param params Parameters of the conference. See #LinphoneConferenceParams. @notnil
 * @return A pointer on the freshly created conference #LinphoneConference. That object will be automatically
 * freed by the core after calling linphone_core_terminate_conference(). @maybenil
 * @warning To guarantee the backward comatibility, this method will assign the created conference to the conference
 * context held by the core. Nonetheless starting from release 5.4, the conference context will be override at every
 * conference created by calling this method
 */
LINPHONE_PUBLIC LinphoneConference *linphone_core_create_conference_with_params(LinphoneCore *core,
                                                                                const LinphoneConferenceParams *params);

/**
 * Create a conference scheduler that can be used to schedule conferences on a client conference service and then send
 * conference information invitation as an ICS object through chat.
 * @param core The #LinphoneCore. @notnil
 * @return A pointer on the freshly created #LinphoneConferenceScheduler. @notnil
 * @deprecated 23/07/2024 Use linphone_core_create_conference_scheduler_2() or
 * linphone_core_create_conference_scheduler_with_type() instead.
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneConferenceScheduler *
linphone_core_create_conference_scheduler(LinphoneCore *core);

/**
 * Create a conference scheduler that can be used to create client conferences for now or later and then send conference
 * info as an ICS through chat.
 * A SipConferenceScheduler is created if the #LinphoneAccount has not defined the URL of the CCMP server, other it will
 * create a CCMPConferenceServer
 * @param core The #LinphoneCore. @notnil
 * @param account The #LinphoneAccount to use in the #LinphoneConferenceScheduler. @maybenil
 * @return A pointer on the freshly created #LinphoneConferenceScheduler. @notnil
 */
LINPHONE_PUBLIC LinphoneConferenceScheduler *linphone_core_create_conference_scheduler_2(LinphoneCore *core,
                                                                                         LinphoneAccount *account);

/**
 * Create a conference scheduler that can be used to create client conferences for now or later and then send conference
 * info as an ICS through chat by specifying its type.
 * @param core The #LinphoneCore. @notnil
 * @param account The #LinphoneAccount to use in the #LinphoneConferenceScheduler. @maybenil
 * @param scheduling_type The type of the #LinphoneConferenceScheduler.
 * @return A pointer on the freshly created #LinphoneConferenceScheduler. @notnil
 */
LINPHONE_PUBLIC LinphoneConferenceScheduler *linphone_core_create_conference_scheduler_with_type(
    LinphoneCore *core, LinphoneAccount *account, LinphoneConferenceSchedulerType scheduling_type);

/**
 * Create a SIP conference scheduler that can be used to create client conferences for now or later and then send
 * conference info as an ICS through chat. A SipConferenceScheduler creates a conference on a server by using a SIP
 * dialog
 * @param core The #LinphoneCore. @notnil
 * @param account The #LinphoneAccount to use in the #LinphoneConferenceScheduler. @maybenil
 * @return A pointer on the freshly created #LinphoneConferenceScheduler. @notnil
 */
LINPHONE_PUBLIC LinphoneConferenceScheduler *linphone_core_create_sip_conference_scheduler(LinphoneCore *core,
                                                                                           LinphoneAccount *account);

/**
 * Create a database conference scheduler that can be used to create client conferences for now or later and then send
 * conference info as an ICS through chat. The DBConferenceScheduler only creates a conference info to be stored in the
 * database of the linphone core
 * @param core The #LinphoneCore. @notnil
 * @param account The #LinphoneAccount to use in the #LinphoneConferenceScheduler. @maybenil
 * @return A pointer on the freshly created #LinphoneConferenceScheduler. @notnil
 */
LINPHONE_PUBLIC LinphoneConferenceScheduler *linphone_core_create_db_conference_scheduler(LinphoneCore *core,
                                                                                          LinphoneAccount *account);

/**
 * Create a SIP conference scheduler that can be used to create client conferences for now or later and then send
 * conference info as an ICS through chat. A CCMPConferenceScheduler creates a conference on a server by using the CCMP
 * protocol
 * @param core The #LinphoneCore. @notnil
 * @param account The #LinphoneAccount to use in the #LinphoneConferenceScheduler. @maybenil
 * @return A pointer on the freshly created #LinphoneConferenceScheduler. @notnil
 */
LINPHONE_PUBLIC LinphoneConferenceScheduler *linphone_core_create_ccmp_conference_scheduler(LinphoneCore *core,
                                                                                            LinphoneAccount *account);

/**
 * Find a conference.
 *
 * @param core A #LinphoneCore object @notnil
 * @param params The conference parameters to match #LinphoneConferenceParams or NULL @maybenil
 * @param localAddr #LinphoneAddress representing the local proxy configuration or NULL @maybenil
 * @param remoteAddr #LinphoneAddress to search for or NULL @maybenil
 * @param participants The participants that must be present in the chat room to find \bctbx_list{LinphoneAddress}
 * @maybenil
 * @return A pointer on #LinphoneConference satisfying the non-NULL function arguments or NULL if none matches @maybenil
 */
LINPHONE_PUBLIC LinphoneConference *linphone_core_search_conference(const LinphoneCore *core,
                                                                    const LinphoneConferenceParams *params,
                                                                    const LinphoneAddress *localAddr,
                                                                    const LinphoneAddress *remoteAddr,
                                                                    const bctbx_list_t *participants);

/**
 * Finds a conference.
 *
 * @param core A #LinphoneCore object @notnil
 * @param conferenceAddr #LinphoneAddress representing the conference address @notnil
 * @return A pointer on #LinphoneConference whose conference address is the one provided as argument or NULL if none
 * matches @maybenil
 */
LINPHONE_PUBLIC LinphoneConference *linphone_core_search_conference_2(const LinphoneCore *core,
                                                                      const LinphoneAddress *conferenceAddr);

/**
 * Find a conference by its identifier.
 *
 * @param core A #LinphoneCore object @notnil
 * @param identifier The conference identifier @notnil
 * @return A matching conference or NULL if none matches. @maybenil
 */
LINPHONE_PUBLIC LinphoneConference *linphone_core_search_conference_by_identifier(const LinphoneCore *core,
                                                                                  const char *identifier);

/**
 * Adds a participant to the conference. If no conference is going on
 * a new internal conference context is created and the participant is
 * added to it.
 * @param core #LinphoneCore @notnil
 * @param call The current call with the participant to add @notnil
 * @return 0 if succeeded. Negative number if failed
 * @deprecated 23/01/2025 Use linphone_conference_add_participant() instead.
 */
LINPHONE_DEPRECATED LINPHONE_PUBLIC LinphoneStatus linphone_core_add_to_conference(LinphoneCore *core,
                                                                                   LinphoneCall *call);

/**
 * Adds all calls into the conference. If no conference is running
 * a new internal conference context is created and all current calls
 * are added to it.
 * @param core #LinphoneCore @notnil
 * @return 0 if succeeded. Negative number if failed
 * @warning This function guarantees that the local endpoint is added to the conference.
 * @deprecated 23/01/2025 Use linphone_conference_invite_participants() instead.
 */
LINPHONE_DEPRECATED LINPHONE_PUBLIC LinphoneStatus linphone_core_add_all_to_conference(LinphoneCore *core);

/**
 * Removes a call from the conference.
 * @param core the #LinphoneCore. @notnil
 * @param call a call that has been previously merged into the conference. @notnil
 *
 * After removing the remote participant belonging to the supplied call, the call becomes a normal call in paused state.
 * If one single remote participant is left alone together with the local user in the conference after the removal, then
 *the conference is automatically transformed into a simple call in StreamsRunning state. The conference's resources are
 *then automatically destroyed.
 *
 * In other words, unless linphone_core_leave_conference() is explicitly called, the last remote participant of a
 *conference is automatically put in a simple call in running state.
 *
 * @return 0 if successful, -1 otherwise.
 * @deprecated 23/01/2025 Use linphone_conference_remove_participant_2() instead.
 **/
LINPHONE_DEPRECATED LINPHONE_PUBLIC LinphoneStatus linphone_core_remove_from_conference(LinphoneCore *core,
                                                                                        LinphoneCall *call);

/**
 * Indicates whether the local participant is part of a conference.
 * @warning That function automatically fails in the case of conferences using a
 * conferencet server (focus). If you use such a conference, you should use
 * linphone_conference_remove_participant() instead.
 * @param core the #LinphoneCore. @notnil
 * @return TRUE if the local participant is in a conference, FALSE otherwise.
 * @deprecated 09/03/2021 Use linphone_conference_is_in() instead.
 */
LINPHONE_DEPRECATED LINPHONE_PUBLIC bool_t linphone_core_is_in_conference(const LinphoneCore *core);

/**
 * Joins the local participant to the running conference
 * @param core #LinphoneCore @notnil
 * @return 0 if succeeded. Negative number if failed
 * @deprecated 09/03/2021 Use linphone_conference_enter() instead.
 */
LINPHONE_DEPRECATED LINPHONE_PUBLIC LinphoneStatus linphone_core_enter_conference(LinphoneCore *core);

/**
 * Makes the local participant leave the running conference
 * @param core #LinphoneCore @notnil
 * @return 0 if succeeded. Negative number if failed
 * @deprecated 09/03/2021 Use linphone_conference_leave() instead.
 */
LINPHONE_DEPRECATED LINPHONE_PUBLIC LinphoneStatus linphone_core_leave_conference(LinphoneCore *core);

/**
 * Returns the input volume of the local participant
 * @param core #LinphoneCore
 * @return A value inside [0.0 ; 1.0]
 * @deprecated 23/01/2025 Use linphone_conference_get_input_volume() instead.
 */
LINPHONE_DEPRECATED LINPHONE_PUBLIC float linphone_core_get_conference_local_input_volume(LinphoneCore *core);

/**
 * Terminates the running conference. If it is a local conference, all calls
 * inside it will become back separate calls and will be put in #LinphoneCallPaused state.
 * If it is a conference involving a focus server, all calls inside the conference
 * will be terminated.
 * @param core #LinphoneCore @notnil
 * @return 0 if succeeded. Negative number if failed
 * @deprecated 23/01/2025 Use linphone_conference_terminate() instead.
 */
LINPHONE_DEPRECATED LINPHONE_PUBLIC LinphoneStatus linphone_core_terminate_conference(LinphoneCore *core);

/**
 * Creates some default conference parameters for instanciating a conference with
 *linphone_core_create_conference_with_params().
 * @param core the #LinphoneCore object @notnil
 * @param conference #LinphoneConference for which the parameters are to be build, or NULL in the case where the
 *parameters are to be used for a new conference. @maybenil
 * @return a #LinphoneConferenceParams object. @notnil
 **/
LINPHONE_PUBLIC LinphoneConferenceParams *linphone_core_create_conference_params_2(LinphoneCore *core,
                                                                                   LinphoneConference *conference);

/**
 * Enables the conference server mode. This has the effect to listen of the conference factory uri
 * to create new conferences when receiving INVITE messages there.
 * Enabling this mode is required when using liblinphone in a conference server application, but shall not
 * be be employed in any client application.
 * @param core A #LinphoneCore object @notnil
 * @param enable A boolean value telling whether to enable or disable the conference server feature
 */
LINPHONE_PUBLIC void linphone_core_enable_conference_server(LinphoneCore *core, bool_t enable);

/**
 * Tells whether the conference server feature is enabled.
 * @param core A #LinphoneCore object @notnil
 * @return A boolean value telling whether the conference server feature is enabled or not
 */
LINPHONE_PUBLIC bool_t linphone_core_conference_server_enabled(const LinphoneCore *core);

/**
 * Enables the flexible FEC feature (RFC8627) for video calls to recover RTP packet loss.
 * @param core A #LinphoneCore object @notnil
 * @param enable A boolean value telling whether to enable or disable the flexible FEC feature
 */
LINPHONE_PUBLIC void linphone_core_enable_fec(LinphoneCore *core, bool_t enable);

/**
 * Tells whether the flexible FEC feature (RFC8627) is enabled for this #LinphoneCore object.
 * @param core A #LinphoneCore object @notnil
 * @return A boolean value telling whether the flexible FEC feature is enabled or not for this #LinphoneCore object
 */
LINPHONE_PUBLIC bool_t linphone_core_fec_enabled(const LinphoneCore *core);

/**
 * Selects whether the default conference participant list is open or closed
 * @param core A #LinphoneCore object @notnil
 * @param type A #LinphoneConferenceParticipantListType participant list type
 */
LINPHONE_PUBLIC void linphone_core_set_conference_participant_list_type(LinphoneCore *core,
                                                                        LinphoneConferenceParticipantListType type);

/**
 * Tells whether the default conference participant list is open or closed
 * @param core A #LinphoneCore object @notnil
 * @return A #LinphoneConferenceParticipantListType participant list type
 */
LINPHONE_PUBLIC LinphoneConferenceParticipantListType
linphone_core_get_conference_participant_list_type(const LinphoneCore *core);

/**
 * @}
 */

/**
 * @addtogroup alert
 * @{
 */
/**
 * Sets the on alert callback.
 * @param cbs #LinphoneCoreCbs object. @notnil
 * @param alert_cb The #LinphoneCoreCbsOnAlertCb callback to call. @notnil
 */
LINPHONE_PUBLIC void linphone_core_cbs_set_new_alert_triggered(LinphoneCoreCbs *cbs,
                                                               LinphoneCoreCbsNewAlertTriggeredCb alert_cb);
/**
 * Gets the on alert callback.
 * @param cbs #LinphoneCoreCbs object. @notnil
 * @return The #LinphoneCoreCbsOnAlertCb callback called.
 */
LINPHONE_PUBLIC LinphoneCoreCbsNewAlertTriggeredCb linphone_core_cbs_get_new_alert_triggered(LinphoneCoreCbs *cbs);

/**
 * Enables alerts. See #LinphoneAlert for more details.
 * @param core the #LinphoneCore .
 * @param enable whether alert reporting is enabled or not.
 */
LINPHONE_PUBLIC void linphone_core_enable_alerts(LinphoneCore *core, bool_t enable);

/**
 * Returns whether alert reporting is enabled. See #LinphoneAlert for more details.
 * @param core the #LinphoneCore .
 * @return whether alert reporting is enabled.
 */
LINPHONE_PUBLIC bool_t linphone_core_alerts_enabled(const LinphoneCore *core);

/**
 * @}
 */

/**
 * @addtogroup call_control
 * @{
 */

/**
 * Empties sound resources to allow a new call to be accepted.
 * This function is autyomatically called by the core if the media resource mode is set to unique.
 * @param core A #LinphoneCore object @notnil
 * @return An integer returning the exit value. If it is 0, sound resources have been emptied. Otherwise, sound
 * resources are busy and cannot be freed immediately.
 */
LINPHONE_PUBLIC int linphone_core_preempt_sound_resources(LinphoneCore *core);

/**
 * Sets the media resources mode. Value values are: unique and shared.
 * When the mode is set to unique, then only one call in the state StreamsRunning is allowed. While accepting a call,
 * the core will try to free media resource used by the current call. If it is unsuccessful, then the call is not
 * accepted. If mode is set to shared, then the media resources of the current call (if any) are not emptied when taking
 * a new call. If the user whishes to free them, he/she is responsible to call linphone_core_preempt_sound_resources
 * himself/herself
 * @param core A #LinphoneCore object @notnil
 * @param mode the chosen mode
 */
LINPHONE_PUBLIC void linphone_core_set_media_resource_mode(LinphoneCore *core, LinphoneMediaResourceMode mode);

/**
 * This function returns the media resource mode for this core
 * @param core A #LinphoneCore object @notnil
 * @return The media resource mode
 */
LINPHONE_PUBLIC LinphoneMediaResourceMode linphone_core_get_media_resource_mode(const LinphoneCore *core);

/**
 * @}
 */

/**
 * Gets the maximum number of simultaneous calls Linphone core can manage at a time. All new call above this limit are
 * declined with a busy answer
 * @param core core @notnil
 * @return maximum number of simultaneous calls
 * @ingroup initializing
 */
LINPHONE_PUBLIC int linphone_core_get_max_calls(LinphoneCore *core);

/**
 * Sets the maximum number of simultaneous calls Linphone core can manage at a time. All new call above this limit are
 * declined with a busy answer
 * @param core core @notnil
 * @param max number of simultaneous calls
 * @ingroup initializing
 */
LINPHONE_PUBLIC void linphone_core_set_max_calls(LinphoneCore *core, int max);

/**
 * Checks if a call will need the sound resources in near future (typically an outgoing call that is awaiting
 * response).
 * In liblinphone, it is not possible to have two independant calls using sound device or camera at the same time.
 * In order to prevent this situation, an application can use linphone_core_sound_resources_locked() to know whether
 * it is possible at a given time to start a new outgoing call.
 * When the function returns TRUE, an application should not allow the user to start an outgoing call.
 * @param core #LinphoneCore object @notnil
 * @return A boolean value telling whether a call will need the sound resources in near future
 * @ingroup call_control
 **/
LINPHONE_PUBLIC bool_t linphone_core_sound_resources_locked(LinphoneCore *core);

/**
 * Sets the crypto suites available to the core
 * @param core core @notnil
 * @param suites comma separated list of supported suites @notnil
 * @ingroup initializing
 **/
LINPHONE_PUBLIC void linphone_core_set_srtp_crypto_suites(LinphoneCore *core, const char *suites);

/**
 * Gets the crypto suites available to the core
 * @param core core @notnil
 * @return a comma separated list of supported suites @notnil
 * @ingroup initializing
 **/
LINPHONE_PUBLIC const char *linphone_core_get_srtp_crypto_suites(LinphoneCore *core);

/**
 * Checks if the ZRTP go clear is enabled or not.
 * @param core #LinphoneCore object. @notnil
 * @return TRUE if ZTRP go clear is enabled; FALSE otherwise.
 * @ingroup initializing
 */
LINPHONE_PUBLIC bool_t linphone_core_zrtp_go_clear_enabled(const LinphoneCore *core);

/**
 * Defines whether ZRTP go clear is enabled
 * @param core #LinphoneCore object. @notnil
 * @param enabled TRUE to enable ZRTP go clear; FALSE otherwise.
 * @ingroup initializing
 */
LINPHONE_PUBLIC void linphone_core_enable_zrtp_go_clear(LinphoneCore *core, bool_t enabled);

/**
 * Checks if a media encryption type is supported
 * @param core core @notnil
 * @param menc #LinphoneMediaEncryption
 * @return whether a media encryption scheme is supported by the #LinphoneCore engine
 * @ingroup initializing
 **/
LINPHONE_PUBLIC bool_t linphone_core_media_encryption_supported(LinphoneCore *core, LinphoneMediaEncryption menc);

/**
 * Chooses the media encryption type to be used for RTP packets.
 * @param core #LinphoneCore object. @notnil
 * @param menc The media encryption policy to be used.
 * @return 0 if successful, any other value otherwise.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC LinphoneStatus linphone_core_set_media_encryption(LinphoneCore *core, LinphoneMediaEncryption menc);

/**
 * Gets the media encryption type being used for RTP packets.
 * @param core #LinphoneCore object. @notnil
 * @return The #LinphoneMediaEncryption policy being used.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC LinphoneMediaEncryption linphone_core_get_media_encryption(const LinphoneCore *core);

/**
 * Checks if the configured media encryption is mandatory or not.
 * @param core #LinphoneCore object. @notnil
 * @return TRUE if media encryption is mandatory; FALSE otherwise.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC bool_t linphone_core_is_media_encryption_mandatory(LinphoneCore *core);

/**
 * Sets the supported media encryptions
 * @param core #LinphoneCore object. @notnil
 * @param encs The supported media encryptions. Elements of the list are pointers to the values of enum
 * #LinphoneMediaEncryption. \bctbx_list{LinphoneCoreMediaEncryption} @notnil
 * @ingroup media_parameters
 * @donotwrap
 */
LINPHONE_PUBLIC void linphone_core_set_supported_media_encryptions(LinphoneCore *core, const bctbx_list_t *encs);

/**
 * Gets the list of supported media descriptions.
 * @param core #LinphoneCore object. @notnil
 * @return list of supported media encryptions as intptr_t. \bctbx_list{LinphoneCoreMediaEncryption} @notnil
 * @ingroup media_parameters
 * @donotwrap
 */
LINPHONE_PUBLIC const bctbx_list_t *linphone_core_get_supported_media_encryptions(LinphoneCore *core);

/**
 * Checks if given media encryption is supported
 * @param core #LinphoneCore object. @notnil
 * @param menc The media encryption policy to be used.
 * @return TRUE if the media encryption is supported, FALSE otherwise
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC bool_t linphone_core_is_media_encryption_supported(LinphoneCore *core, LinphoneMediaEncryption menc);

/**
 * Defines whether the configured media encryption is mandatory, if it is and the negotation cannot result
 * in the desired media encryption then the call will fail. If not an INVITE will be resent with encryption
 * disabled.
 * @param core #LinphoneCore object. @notnil
 * @param mandatory TRUE to set it mandatory; FALSE otherwise.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_media_encryption_mandatory(LinphoneCore *core, bool_t mandatory);

/**
 * Checks if cfg lines are going to the merged if the capability negotiation (RFC5939) is supported or not.
 * @param core #LinphoneCore object. @notnil
 * @return TRUE if acfg and pcfg lines with consecutive indexes are going to be merged; FALSE otherwise.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC bool_t linphone_core_cfg_lines_merging_enabled(const LinphoneCore *core);

/**
 * Defines whether cfg lines are going to be merged if capability negotiation (RFC5939) is supported
 * @param core #LinphoneCore object. @notnil
 * @param merge TRUE to merge acfg and pcfg lines with consecutive indexes; FALSE otherwise.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_enable_cfg_lines_merging(LinphoneCore *core, bool_t merge);

/**
 * Checks if tcap lines are going to the merged if the capability negotiation (RFC5939) is supported or not.
 * @param core #LinphoneCore object. @notnil
 * @return TRUE if tcap lines with consecutive indexes are going to be merged; FALSE otherwise.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC bool_t linphone_core_tcap_lines_merging_enabled(const LinphoneCore *core);

/**
 * Defines whether tcap lines are going to be merged if capability negotiation (RFC5939) is supported
 * @param core #LinphoneCore object. @notnil
 * @param merge TRUE to merge tcap lines with consecutive indexes; FALSE otherwise.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_enable_tcap_line_merging(LinphoneCore *core, bool_t merge);

/**
 * Checks if the capability negotiation (RFC5939) reINVITE is enabled or not.
 * @param core #LinphoneCore object. @notnil
 * @return TRUE if capability negotiation reINVITE is enabled; FALSE otherwise.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC bool_t linphone_core_capability_negotiation_reinvite_enabled(const LinphoneCore *core);

/**
 * Defines whether RTP port is set to 0 when a stream is inactive
 * @param core #LinphoneCore object. @notnil
 * @param enable TRUE to set the RTP port to 0 if the stream direction is inactive; FALSE otherwise.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_enable_zero_rtp_port_for_stream_inactive(LinphoneCore *core, bool_t enable);

/**
 * Checks if RTP port is set to 0 when a stream is inactive
 * @param core #LinphoneCore object. @notnil
 * @return TRUE if the RTP port is set to 0 if the stream direction is inactive; FALSE otherwise.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC bool_t linphone_core_zero_rtp_port_for_stream_inactive_enabled(const LinphoneCore *core);

/**
 * Defines whether capability negotiation (RFC5939) reINVITE is enabled
 * @param core #LinphoneCore object. @notnil
 * @param enable TRUE to enable capability negotiation reINVITE; FALSE otherwise.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_enable_capability_negotiation_reinvite(LinphoneCore *core, bool_t enable);

/**
 * Checks if the capability negotiation (RFC5939) is supported or not.
 * @param core #LinphoneCore object. @notnil
 * @return TRUE if capability negotiation is supported; FALSE otherwise.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC bool_t linphone_core_capability_negociation_enabled(const LinphoneCore *core);

/**
 * Defines whether capability negotiation (RFC5939) is supported
 * @param core #LinphoneCore object. @notnil
 * @param enable TRUE to support RFC5939; FALSE otherwise.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_enable_capability_negociation(LinphoneCore *core, bool_t enable);

/**
 * True if tunnel extension was compiled.
 * @return TRUE if library was built with tunnel, FALSE otherwise
 * @ingroup tunnel
 */
LINPHONE_PUBLIC bool_t linphone_core_tunnel_available(void);

/**
 * Gets tunnel instance if available
 * @ingroup tunnel
 * @param core core object @notnil
 * @return #LinphoneTunnel or NULL if not available. @maybenil
 */
LINPHONE_PUBLIC LinphoneTunnel *linphone_core_get_tunnel(const LinphoneCore *core);

/**
 * Sets the DSCP field for SIP signaling channel.
 * The DSCP defines the quality of service in IP packets.
 * @note It is usually useless or a bad idea to try to play with DSCP bits unless having full control on the network.
 * @warning Setting the DSCP bits is more or less well supported by operating systems and sometimes requires to disable
 *IPv6.
 * @param core #LinphoneCore object @notnil
 * @param dscp The DSCP value to set
 * @ingroup network_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_sip_dscp(LinphoneCore *core, int dscp);

/**
 * Gets the DSCP field for SIP signaling channel.
 * The DSCP defines the quality of service in IP packets.
 * @param core #LinphoneCore object @notnil
 * @return The current DSCP value
 * @ingroup network_parameters
 **/
LINPHONE_PUBLIC int linphone_core_get_sip_dscp(const LinphoneCore *core);

/**
 * Sets the DSCP field for outgoing audio streams.
 * The DSCP defines the quality of service in IP packets.
 * @note It is usually useless or a bad idea to try to play with DSCP bits unless having full control on the network.
 * @warning Setting the DSCP bits is more or less well supported by operating systems and sometimes requires to disable
 *IPv6.
 * @param core #LinphoneCore object @notnil
 * @param dscp The DSCP value to set
 * @ingroup network_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_audio_dscp(LinphoneCore *core, int dscp);

/**
 * Gets the DSCP field for outgoing audio streams.
 * The DSCP defines the quality of service in IP packets.
 * @param core #LinphoneCore object @notnil
 * @return The current DSCP value
 * @ingroup network_parameters
 **/
LINPHONE_PUBLIC int linphone_core_get_audio_dscp(const LinphoneCore *core);

/**
 * Sets the DSCP field for outgoing video streams.
 * The DSCP defines the quality of service in IP packets.
 * When RTP bundling is negociated during the call (see linphone_core_enable_rtp_bundle()), the video
 * packets are sent through the audio RTP/UDP connection, which leaves the video dscp setting wihtout effect.
 * @note It is usually useless or a bad idea to try to play with DSCP bits unless having full control on the network.
 * @warning Setting the DSCP bits is more or less well supported by operating systems and sometimes requires to disable
 *IPv6.
 * @param core #LinphoneCore object @notnil
 * @param dscp The DSCP value to set
 * @ingroup network_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_video_dscp(LinphoneCore *core, int dscp);

/**
 * Gets the DSCP field for outgoing video streams.
 * The DSCP defines the quality of service in IP packets.
 * @param core #LinphoneCore object @notnil
 * @return The current DSCP value
 * @ingroup network_parameters
 **/
LINPHONE_PUBLIC int linphone_core_get_video_dscp(const LinphoneCore *core);

/**
 * Gets the name of the mediastreamer2 engine (filter) used for rendering video.
 * @param core #LinphoneCore object @notnil
 * @return The currently selected video display filter. @maybenil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC const char *linphone_core_get_video_display_filter(LinphoneCore *core);

/**
 * Sets the name of the mediastreamer2 engine (filter) to be used for rendering video.
 * @param core the #LinphoneCore @notnil
 * @param filter_name the filter name to use or NULL to use default. @maybenil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_video_display_filter(LinphoneCore *core, const char *filter_name);

/**
 * Gets the name of the default mediastreamer2 filter used for rendering video on the current platform.
 * This is for advanced users of the library, mainly to expose mediastreamer video filter name and status.
 * @param core #LinphoneCore object @notnil
 * @return The default video display filter. @notnil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC const char *linphone_core_get_default_video_display_filter(LinphoneCore *core);

/**
 * Checks if the given media filter is loaded and usable.
 * This is for advanced users of the library, mainly to expose mediastreamer video filter status.
 * @param core #LinphoneCore object @notnil
 * @param filtername the filter name @notnil
 * @return TRUE	if the filter is loaded and usable, FALSE otherwise
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC bool_t linphone_core_is_media_filter_supported(LinphoneCore *core, const char *filtername);

/**
 * Gets the name of the mediastreamer2 filter used for echo cancelling.
 * @param core #LinphoneCore object @notnil
 * @return The name of the mediastreamer2 filter used for echo cancellation. @maybenil
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC const char *linphone_core_get_echo_canceller_filter_name(const LinphoneCore *core);

/**
 * Sets the name of the mediastreamer2 filter to be used for echo cancelling.
 * @param core #LinphoneCore object @notnil
 * @param filtername The name of the mediastreamer2 filter to be used for echo cancellation. @maybenil
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_echo_canceller_filter_name(LinphoneCore *core, const char *filtername);

/**
 * Contact Providers
 */

typedef void (*ContactSearchCallback)(LinphoneContactSearch *id,
                                      bctbx_list_t *searchResults,
                                      void *data,
                                      bool_t haveMoreResults);

/**
 * Sets the URI where to download xml configuration file at startup.
 * http://, https:// and file:// uris are supported.
 * This can also be set from configuration file or factory config file, from [misc] section, item "config-uri".
 * Calling this function does not load the configuration. It will write the value into configuration so that
 * configuration from URI will take place during next linphone_core_start() invocation.
 * The format the xml file is briefly documented here:
 * https://wiki.linphone.org/xwiki/wiki/public/view/Lib/Features/Remote%20Provisioning/
 * @param core the #LinphoneCore object @notnil
 * @param uri the uri to use in order to obtain the configuration. Passing NULL will disable remote
 *provisioning. @maybenil
 * @return -1 if uri could not be parsed, 0 otherwise. Note that this does not check validity of URI endpoint nor scheme
 *and download may still fail.
 * @ingroup initializing
 **/
LINPHONE_PUBLIC LinphoneStatus linphone_core_set_provisioning_uri(LinphoneCore *core, const char *uri);

/**
 * Get provisioning URI.
 * @param core the #LinphoneCore object @notnil
 * @return the provisioning URI. @maybenil
 * @ingroup initializing
 **/
LINPHONE_PUBLIC const char *linphone_core_get_provisioning_uri(const LinphoneCore *core);

/**
 * Add an extra header for retrieving the remote provisioning (check linphone_core_set_provisioning_uri()).
 * This can also be set from configuration file or factory config file, from[misc] section, item "config-uri-headers_X"
 * where X is the index of the header starting by 0.
 * @param core the #LinphoneCore object @notnil
 * @param header_name the header to use when downloading the configuration. @notnil
 * @param value the value to use when downloading the configuration. @notnil
 * @ingroup initializing **/
LINPHONE_PUBLIC void
linphone_core_add_provisioning_header(LinphoneCore *core, const char *header_name, const char *value);

/**
 * Clear all headers that were added with linphone_core_add_provisioning_header().
 * @param core the #LinphoneCore object @notnil
 * @ingroup initializing
 **/
LINPHONE_PUBLIC void linphone_core_clear_provisioning_headers(LinphoneCore *core);

/**
 * Gets if the provisioning URI should be removed after it's been applied successfully.
 * @param core the #LinphoneCore object @notnil
 * @return TRUE if the provisioning URI should be removed, FALSE otherwise
 */
LINPHONE_PUBLIC bool_t linphone_core_is_provisioning_transient(LinphoneCore *core);

/**
 * Control when media offer is sent in SIP INVITE.
 * Enabling this type of SIP call flow is not recommended. This setting is mainly for internal testing.
 * @param core the #LinphoneCore object @notnil
 * @param enable true if INVITE has to be sent whitout SDP.
 * @ingroup network_parameters
 **/
LINPHONE_PUBLIC void linphone_core_enable_sdp_200_ack(LinphoneCore *core, bool_t enable);

/**
 * Media offer control param for SIP INVITE.
 * @param core the #LinphoneCore object @notnil
 * @return true if INVITE has to be sent whitout SDP.
 * @ingroup network_parameters
 **/
LINPHONE_PUBLIC bool_t linphone_core_sdp_200_ack_enabled(const LinphoneCore *core);

/**
 * @brief
 * Assign an audio file to be played as a specific tone id.
 * This function typically allows to customize telephony tones per country.
 *
 * If you want to disable a tone, set a path to a non-existent file.
 * To disable all tones, use linphone_core_enable_call_tone_indications()
 * or set the tone_indications to 0 in the [misc] section of your linphonerc.
 * @param core the core @notnil
 * @param tone_id the #LinphoneToneId
 * @param audiofile a wav file to be played or NULL to use the default (generated) one. @maybenil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_tone(LinphoneCore *core, LinphoneToneID tone_id, const char *audiofile);

/**
 * Globaly sets an http file transfer server to be used for content type application/vnd.gsma.rcs-ft-http+xml.
 * Url may be like: "https://file.linphone.org/upload.php".
 * This value can also be set for a dedicated account using linphone_account_params_set_file_transfer_server().
 * @param core #LinphoneCore to be modified @notnil
 * @param server_url URL of the file server. @maybenil
 * @ingroup misc
 * */
LINPHONE_PUBLIC void linphone_core_set_file_transfer_server(LinphoneCore *core, const char *server_url);

/**
 * Gets the globaly set http file transfer server to be used for content type application/vnd.gsma.rcs-ft-http+xml.
 * Url may be like: "https://file.linphone.org/upload.php".
 * @param core #LinphoneCore from which to get the server_url @notnil
 * @return URL of the file server. @maybenil
 * @ingroup misc
 * */
LINPHONE_PUBLIC const char *linphone_core_get_file_transfer_server(LinphoneCore *core);

/**
 * Returns a list of strings containing the file format types supported for call recording.
 * @param core the core @notnil
 * @return The supported formats, typically 'wav', 'mka', 'mkv', 'smff'. \bctbx_list{char *} @notnil @tobefreed
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC bctbx_list_t *linphone_core_get_supported_file_formats_list(const LinphoneCore *core);

/**
 * Returns whether a specific file format is supported.
 * @see linphone_core_get_supported_file_formats()
 * @param core A #LinphoneCore object @notnil
 * @param fmt The format extension (wav, mkv). @notnil
 * @return TRUE if the file format is supported, FALSE otherwise
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC bool_t linphone_core_file_format_supported(LinphoneCore *core, const char *fmt);

/**
 * Set the SIP supported tags
 * @param core #LinphoneCore object @notnil
 * @param tags The SIP feature tags to set @maybenil
 * @ingroup initializing
 **/
LINPHONE_PUBLIC void linphone_core_set_supported_tag(LinphoneCore *core, const char *tags);

/**
 * This function controls signaling features supported by the core.
 * They are typically included in a SIP Supported header.
 * @param core #LinphoneCore object @notnil
 * @param tag The feature tag name @notnil
 * @ingroup initializing
 **/
LINPHONE_PUBLIC void linphone_core_add_supported_tag(LinphoneCore *core, const char *tag);

/**
 * Remove a supported tag.
 * @param core #LinphoneCore object @notnil
 * @param tag The tag to remove @notnil
 * @ingroup initializing
 * @see linphone_core_add_supported_tag()
 **/
LINPHONE_PUBLIC void linphone_core_remove_supported_tag(LinphoneCore *core, const char *tag);

/**
 * Enables RTCP feedback (also known as RTP/AVPF profile).
 * Setting #LinphoneAVPFDefault is equivalent to LinphoneAVPFDisabled.
 * This setting can be overriden per #LinphoneAccount with linphone_account_params_set_avpf_mode().
 * The value set here is used for calls placed or received out of any account configured, or if the account is
 *configured with LinphoneAVPFDefault.
 * @param core #LinphoneCore object @notnil
 * @param mode The AVPF mode to use.
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_avpf_mode(LinphoneCore *core, LinphoneAVPFMode mode);

/**
 * Returns AVPF enablement. See linphone_core_set_avpf_mode() .
 * @param core #LinphoneCore object @notnil
 * @return The current #LinphoneAVPFMode mode
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC LinphoneAVPFMode linphone_core_get_avpf_mode(const LinphoneCore *core);

/**
 * Sets the avpf report interval in seconds.
 * This value can be overriden at Account level using linphone_account_params__set_avpf_rr_interval().
 * @param core #LinphoneCore object @notnil
 * @param interval The report interval in seconds
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_set_avpf_rr_interval(LinphoneCore *core, int interval);

/**
 * Returns the avpf report interval in seconds.
 * @param core #LinphoneCore object @notnil
 * @return The current AVPF report interval in seconds
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC int linphone_core_get_avpf_rr_interval(const LinphoneCore *core);

/**
 * Use to set multicast address to be used for audio stream.
 * @param core #LinphoneCore @notnil
 * @param ip an ipv4/6 multicast address. @maybenil
 * @return 0 in case of success
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC LinphoneStatus linphone_core_set_audio_multicast_addr(LinphoneCore *core, const char *ip);

/**
 * Use to set multicast address to be used for video stream.
 * @param core #LinphoneCore @notnil
 * @param ip an ipv4/6 multicast address. @maybenil
 * @return 0 in case of success
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC LinphoneStatus linphone_core_set_video_multicast_addr(LinphoneCore *core, const char *ip);

/**
 * Use to get multicast address to be used for audio stream.
 * @param core #LinphoneCore @notnil
 * @return an ipv4/6 multicast address or default value. @maybenil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC const char *linphone_core_get_audio_multicast_addr(const LinphoneCore *core);

/**
 * Use to get multicast address to be used for video stream.
 * @param core #LinphoneCore @notnil
 * @return an ipv4/6 multicast address, or default value. @maybenil
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC const char *linphone_core_get_video_multicast_addr(const LinphoneCore *core);

/**
 * Use to set multicast ttl to be used for audio stream.
 * @param core #LinphoneCore @notnil
 * @param ttl value or -1 if not used. [0..255] default value is 1
 * @return 0 in case of success
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC LinphoneStatus linphone_core_set_audio_multicast_ttl(LinphoneCore *core, int ttl);

/**
 * Use to set multicast ttl to be used for video stream.
 * @param core #LinphoneCore @notnil
 * @param  ttl value or -1 if not used. [0..255] default value is 1
 * @return 0 in case of success
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC LinphoneStatus linphone_core_set_video_multicast_ttl(LinphoneCore *core, int ttl);

/**
 * Use to get multicast ttl to be used for audio stream.
 * @param core #LinphoneCore @notnil
 * @return a time to leave value
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC int linphone_core_get_audio_multicast_ttl(const LinphoneCore *core);

/**
 * Use to get multicast ttl to be used for video stream.
 * @param core #LinphoneCore @notnil
 * @return a time to leave value
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC int linphone_core_get_video_multicast_ttl(const LinphoneCore *core);

/**
 * Use to enable multicast rtp for audio stream.
 * If enabled, outgoing calls put a multicast address from #linphone_core_get_video_multicast_addr() into audio cline.
 *In case of outgoing call audio stream is sent to this multicast address. For incoming calls behavior is unchanged.
 * @param core #LinphoneCore @notnil
 * @param yesno if yes, subsequent calls will propose multicast ip set by #linphone_core_set_audio_multicast_addr()
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_enable_audio_multicast(LinphoneCore *core, bool_t yesno);

/**
 * Use to get multicast state of audio stream.
 * @param core #LinphoneCore @notnil
 * @return TRUE if subsequent calls will propose multicast ip set by #linphone_core_set_audio_multicast_addr()
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC bool_t linphone_core_audio_multicast_enabled(const LinphoneCore *core);

/**
 * Use to enable multicast rtp for video stream.
 * If enabled, outgoing calls put a multicast address from #linphone_core_get_video_multicast_addr() into video cline.
 *In case of outgoing call video stream is sent to this  multicast address. For incoming calls behavior is unchanged.
 * @param core #LinphoneCore @notnil
 * @param yesno if yes, subsequent outgoing calls will propose multicast ip set by
 *#linphone_core_set_video_multicast_addr()
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC void linphone_core_enable_video_multicast(LinphoneCore *core, bool_t yesno);

/**
 * Use to get multicast state of video stream.
 * @param core #LinphoneCore @notnil
 * @return TRUE if subsequent calls will propose multicast ip set by #linphone_core_set_video_multicast_addr()
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC bool_t linphone_core_video_multicast_enabled(const LinphoneCore *core);

/**
 * Enables or disables call ringing.
 * This value is taken into account from next time call parameters are created.
 * This feature can also be enabled per-call using #LinphoneCallParams.
 * @param core the #LinphoneCore @notnil
 * @param yesno a boolean to indicate whether the feature is to be disabled.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_disable_call_ringing(const LinphoneCore *core, bool_t yesno);

/**
 * Check whether ringing of calls is disabled
 * @param core #LinphoneCore @notnil
 * @return TRUE if call ringing is disabled
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC bool_t linphone_core_call_ringing_disabled(const LinphoneCore *core);

/**
 * Enables or disables call tone indications.
 * This value is taken into account from next time call parameters are created.
 * This feature can also be enabled per-call using #LinphoneCallParams.
 * @param core the #LinphoneCore @notnil
 * @param yesno a boolean to indicate whether the feature is to be enabled.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_enable_call_tone_indications(const LinphoneCore *core, bool_t yesno);

/**
 * Check whether tone indications of calls are enabled
 * @param core #LinphoneCore @notnil
 * @return TRUE if call tone indications are enabled
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC bool_t linphone_core_call_tone_indications_enabled(const LinphoneCore *core);

/**
 * Returns whether RTP bundle mode (also known as Media Multiplexing) is enabled.
 * See https://datatracker.ietf.org/doc/html/rfc8843 for more information.
 * @param core the #LinphoneCore @notnil
 * @return a boolean indicating the enablement of rtp bundle mode.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC bool_t linphone_core_rtp_bundle_enabled(const LinphoneCore *core);

/**
 * Enables or disables RTP bundle mode (Media Multiplexing).
 * See https://datatracker.ietf.org/doc/html/rfc8843 for more information about the feature.
 * When enabled, liblinphone will try to negociate the use of a single port for all streams when doing an outgoing call.
 * It automatically enables rtcp-mux.
 * This feature can also be enabled per-call using #LinphoneCallParams.
 * RTP Bundle mode is required for video conferencing.
 * @param core the #LinphoneCore @notnil
 * @param value a boolean to indicate whether the feature is to be enabled.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_enable_rtp_bundle(LinphoneCore *core, bool_t value);

/**
 * @brief Set the network simulator parameters.
 *
 * Liblinphone has the capabability of simulating the effects of a network (latency, lost packets, jitter, max
 *bandwidth). Please refer to the oRTP documentation for the meaning of the parameters of the OrtpNetworkSimulatorParams
 *structure. This function has effect for future calls, but not for currently running calls, though this behavior may be
 *changed in future versions.
 * @warning Due to design of network simulation in oRTP, simulation is applied independently for audio and video stream.
 *This means for example that a bandwidth limit of 250kbit/s will have no effect on an audio stream running at 40kbit/s
 *while a videostream targetting 400kbit/s will be highly affected.
 * @param core the #LinphoneCore
 * @param params the parameters used for the network simulation.
 * @return 0 if successful, -1 otherwise.
 * @ingroup media_parameters
 * @donotwrap
 **/
LINPHONE_PUBLIC LinphoneStatus linphone_core_set_network_simulator_params(LinphoneCore *core,
                                                                          const OrtpNetworkSimulatorParams *params);

/**
 * @brief Get the previously set network simulation parameters.
 * @see linphone_core_set_network_simulator_params()
 * @return a #OrtpNetworkSimulatorParams structure.
 * @ingroup media_parameters
 * @donotwrap
 **/
LINPHONE_PUBLIC const OrtpNetworkSimulatorParams *linphone_core_get_network_simulator_params(const LinphoneCore *core);

/**
 * Set the video preset to be used for video calls.
 * @param core #LinphoneCore object @notnil
 * @param preset The name of the video preset to be used (can be NULL to use the default video preset). @maybenil
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_video_preset(LinphoneCore *core, const char *preset);

/**
 * Get the video preset used for video calls.
 * @param core #LinphoneCore object @notnil
 * @return The name of the video preset used for video calls (can be NULL if the default video preset is used).
 * @maybenil
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC const char *linphone_core_get_video_preset(const LinphoneCore *core);

/**
 * Gets if realtime text is enabled or not (RFC4103).
 * @param core #LinphoneCore object @notnil
 * @return TRUE if realtime text is enabled, FALSE otherwise
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC bool_t linphone_core_realtime_text_enabled(LinphoneCore *core);

/**
 * Enables realtime text support in calls (RFC4103).
 * See also linphone_call_params_enable_realtime_text().
 * @param core #LinphoneCore object @notnil
 * @param value a boolean.
 **/
LINPHONE_PUBLIC void linphone_core_enable_realtime_text(LinphoneCore *core, bool_t value);

/**
 * Gets keep alive interval of real time text (RFC4103).
 * @param core #LinphoneCore object @notnil
 * @return keep alive interval of real time text.
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC unsigned int linphone_core_get_realtime_text_keepalive_interval(const LinphoneCore *core);

/**
 * Set keep alive interval for real time text (RFC4103).
 * @param core #LinphoneCore object @notnil
 * @param interval The keep alive interval of real time text, 25000 by default.
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_realtime_text_keepalive_interval(LinphoneCore *core, unsigned int interval);

/**
 * Sets http proxy address to be used for signaling during next channel connection. Use
 * #linphone_core_set_network_reachable() FASLE/TRUE to force channel restart.
 * @param core #LinphoneCore object @notnil
 * @param host Hostname of IP adress of the http proxy (can be NULL to disable). @maybenil
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_http_proxy_host(LinphoneCore *core, const char *host);

/**
 * Sets http proxy port to be used for signaling.
 * @param core #LinphoneCore object @notnil
 * @param port of the http proxy.
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_http_proxy_port(LinphoneCore *core, int port);

/**
 * Returns whether automatic http proxy is enabled.
 * @param core #LinphoneCore object @notnil
 * @return TRUE if automatic http proxy is enabled or FALSE.
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC bool_t linphone_core_automatic_http_proxy_detection_enabled(const LinphoneCore *core);

/**
 * Enables or disables automatic http proxy detection.
 * @param core #LinphoneCore object @notnil
 * @param enable TRUE if automatic http proxy is enabled or FALSE.
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC void linphone_core_enable_automatic_http_proxy_detection(LinphoneCore *core, bool_t enable);

/**
 * Gets http proxy address to be used for signaling.
 * @param core #LinphoneCore object @notnil
 * @return hostname of IP adress of the http proxy (can be NULL to disable). @maybenil
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC const char *linphone_core_get_http_proxy_host(const LinphoneCore *core);

/**
 * Gets http proxy port to be used for signaling.
 * @param core #LinphoneCore object @notnil
 * @return port of the http proxy.
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC int linphone_core_get_http_proxy_port(const LinphoneCore *core);

LINPHONE_PUBLIC LinphoneRingtonePlayer *linphone_core_get_ringtoneplayer(LinphoneCore *core);

/**
 * Sets a TLS certificate used for TLS authentication
 * The certificate won't be stored, you have to set it after each #LinphoneCore startup
 * @param core #LinphoneCore object @notnil
 * @param tls_cert the TLS certificate. @maybenil
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_tls_cert(LinphoneCore *core, const char *tls_cert);

/**
 * Sets a TLS key used for TLS authentication
 * The key won't be stored, you have to set it after each #LinphoneCore startup
 * @param core #LinphoneCore object @notnil
 * @param tls_key the TLS key. @maybenil
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_tls_key(LinphoneCore *core, const char *tls_key);

/**
 * Sets a TLS certificate path used for TLS authentication
 * The path will be stored in the rc file and automatically restored on startup
 * @param core #LinphoneCore object @notnil
 * @param tls_cert_path path to the TLS certificate. @maybenil
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_tls_cert_path(LinphoneCore *core, const char *tls_cert_path);

/**
 * Sets a TLS key path used for TLS authentication
 * The path will be stored in the rc file and automatically restored on startup
 * @param core #LinphoneCore object @notnil
 * @param tls_key_path path to the TLS key. @maybenil
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_tls_key_path(LinphoneCore *core, const char *tls_key_path);

/**
 * Gets the TLS certificate
 * @param core #LinphoneCore object @notnil
 * @return the TLS certificate or NULL if not set yet. @maybenil
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC const char *linphone_core_get_tls_cert(const LinphoneCore *core);

/**
 * Gets the TLS key
 * @param core #LinphoneCore object @notnil
 * @return the TLS key or NULL if not set yet. @maybenil
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC const char *linphone_core_get_tls_key(const LinphoneCore *core);

/**
 * Gets the path to the TLS certificate file
 * @param core #LinphoneCore object @notnil
 * @return the TLS certificate path or NULL if not set yet. @maybenil
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC const char *linphone_core_get_tls_cert_path(const LinphoneCore *core);

/**
 * Gets the path to the TLS key file
 * @param core #LinphoneCore object @notnil
 * @return the TLS key path or NULL if not set yet. @maybenil
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC const char *linphone_core_get_tls_key_path(const LinphoneCore *core);

/**
 * Sets an IM Encryption Engine in the core
 * @param core #LinphoneCore object
 * @param imee #LinphoneImEncryptionEngine object
 * @ingroup chatroom
 * @donotwrap
 */
LINPHONE_PUBLIC void linphone_core_set_im_encryption_engine(LinphoneCore *core, LinphoneImEncryptionEngine *imee);

/**
 * Gets the IM Encryption Engine in the core if possible
 * @param core #LinphoneCore object
 * @return the IM Encryption Engine in the core or NULL
 * @ingroup chatroom
 * @donotwrap
 */
LINPHONE_PUBLIC LinphoneImEncryptionEngine *linphone_core_get_im_encryption_engine(const LinphoneCore *core);

/**
 * Tells whether a content type is supported.
 * @param core #LinphoneCore object @notnil
 * @param content_type The content type to check @notnil
 * @return A boolean value telling whether the specified content type is supported or not.
 * @ingroup chatroom
 */
LINPHONE_PUBLIC bool_t linphone_core_is_content_type_supported(const LinphoneCore *core, const char *content_type);

/**
 * Add support for the specified content type.
 * It is the application responsibility to handle it correctly afterwards.
 * @param core #LinphoneCore object @notnil
 * @param content_type The content type to add support for @notnil
 * @ingroup chatroom
 */
LINPHONE_PUBLIC void linphone_core_add_content_type_support(LinphoneCore *core, const char *content_type);

/**
 * Remove support for the specified content type.
 * It is the application responsibility to handle it correctly afterwards.
 * @param core LinphoneCore object @notnil
 * @param content_type The content type to remove support for @notnil
 * @ingroup chatroom
 */
LINPHONE_PUBLIC void linphone_core_remove_content_type_support(LinphoneCore *core, const char *content_type);

/**
 * Returns the global unread chat message count.
 * @param core #LinphoneCore object. @notnil
 * @return The global unread chat message count.
 * @ingroup chatroom
 */
LINPHONE_PUBLIC int linphone_core_get_unread_chat_message_count(const LinphoneCore *core);

/**
 * Returns the unread chat message count for a given local address.
 * @param core #LinphoneCore object. @notnil
 * @param address #LinphoneAddress object. @notnil
 * @return The unread chat message count.
 */
LINPHONE_PUBLIC int linphone_core_get_unread_chat_message_count_from_local(const LinphoneCore *core,
                                                                           const LinphoneAddress *address);

/**
 * Returns the unread chat message count for all active local address. (Primary contact + proxy configs.)
 * @param core #LinphoneCore object. @notnil
 * @return The unread chat message count.
 * @ingroup chatroom
 */
LINPHONE_PUBLIC int linphone_core_get_unread_chat_message_count_from_active_locals(const LinphoneCore *core);

/**
 * Sets the linphone specs list value telling what functionalities the linphone client supports.
 * The said "specs" (specifications) are pair of keyword/version that helps advertising the level
 * feature supported by liblinphone.
 * An application usually does not need to set this information, that is automatically assigned at #LinphoneCore start.
 * Setting this is mainly for internal tests of backward compatibility.
 * @param core #LinphoneCore object @notnil
 * @param specs The list of string specs to set. \bctbx_list{char *} @maybenil
 * @ingroup initializing
 */
LINPHONE_PUBLIC void linphone_core_set_linphone_specs_list(LinphoneCore *core, const bctbx_list_t *specs);

/**
 * Add the given linphone specs to the list of functionalities the linphone client supports.
 * @see linphone_core_set_linphone_specs_list()
 * @param core #LinphoneCore object @notnil
 * @param spec The spec to add @notnil
 * @ingroup initializing
 */
LINPHONE_PUBLIC void linphone_core_add_linphone_spec(LinphoneCore *core, const char *spec);

/**
 * Remove the given linphone specs from the list of functionalities the linphone client supports.
 * @see linphone_core_set_linphone_specs_list()
 * @param core #LinphoneCore object @notnil
 * @param spec The spec to remove @notnil
 * @ingroup initializing
 */
LINPHONE_PUBLIC void linphone_core_remove_linphone_spec(LinphoneCore *core, const char *spec);

/**
 * Get the list of linphone specs string values representing what functionalities the linphone client supports
 * @see linphone_core_set_linphone_specs_list()
 * @param core #LinphoneCore object @notnil
 * @return A list of supported specs. The list must be freed with bctbx_list_free() after usage. \bctbx_list{char *}
 * @maybenil @tobefreed
 * @ingroup initializing
 */
LINPHONE_PUBLIC bctbx_list_t *linphone_core_get_linphone_specs_list(LinphoneCore *core);

/**
 * @addtogroup chatroom
 * @{
 */
/**
 * Create a chat room.
 *
 * @param core A #LinphoneCore object @notnil
 * @param params The chat room creation parameters #LinphoneConferenceParams @notnil
 * @param participants The initial list of participants of the chat room. \bctbx_list{LinphoneAddress} @notnil
 * @return The newly created chat room (can be an existing one if backend is Basic) or NULL. @maybenil
 * @ingroup chatroom
 */
LINPHONE_PUBLIC LinphoneChatRoom *linphone_core_create_chat_room_7(LinphoneCore *core,
                                                                   const LinphoneConferenceParams *params,
                                                                   const bctbx_list_t *participants);

/**
 * Find a chat room.
 *
 * @param core A #LinphoneCore object @notnil
 * @param params The chat room parameters to match #LinphoneChatRoomParams or NULL @maybenil
 * @param localAddr #LinphoneAddress of a local #LinphoneAccount identity or NULL @maybenil
 * @param remoteAddr #LinphoneAddress to search for or NULL @maybenil
 * @param participants The participants that must be present in the chat room to find. \bctbx_list{LinphoneAddress}
 * @maybenil
 * @return A matching chat room or NULL if none matches. @maybenil
 * @deprecated 22/10/2024, use linphone_core_search_chat_room_2() instead
 * @ingroup chatroom
 */
LINPHONE_PUBLIC LinphoneChatRoom *linphone_core_search_chat_room(const LinphoneCore *core,
                                                                 const LinphoneChatRoomParams *params,
                                                                 const LinphoneAddress *localAddr,
                                                                 const LinphoneAddress *remoteAddr,
                                                                 const bctbx_list_t *participants);

/**
 * Find a chat room.
 *
 * @param core A #LinphoneCore object @notnil
 * @param params The chat room parameters to match #LinphoneChatRoomParams or NULL @maybenil
 * @param localAddr #LinphoneAddress of a local #LinphoneAccount identity or NULL @maybenil
 * @param remoteAddr #LinphoneAddress to search for or NULL @maybenil
 * @param participants The participants that must be present in the chat room to find. \bctbx_list{LinphoneAddress}
 * @maybenil
 * @return A matching chat room or NULL if none matches. @maybenil
 */
LINPHONE_PUBLIC LinphoneChatRoom *linphone_core_search_chat_room_2(const LinphoneCore *core,
                                                                   const LinphoneConferenceParams *params,
                                                                   const LinphoneAddress *localAddr,
                                                                   const LinphoneAddress *remoteAddr,
                                                                   const bctbx_list_t *participants);

/**
 * Find a chat room by its identifier.
 *
 * @param core A #LinphoneCore object @notnil
 * @param identifier The chat room identifier @notnil
 * @return A matching chat room or NULL if none matches. @maybenil
 */
LINPHONE_PUBLIC LinphoneChatRoom *linphone_core_search_chat_room_by_identifier(const LinphoneCore *core,
                                                                               const char *identifier);

/**
 * Removes a chatroom including all message history from the LinphoneCore.
 * @param core A #LinphoneCore object @notnil
 * @param chat_room A #LinphoneChatRoom object @notnil
 **/
LINPHONE_PUBLIC void linphone_core_delete_chat_room(LinphoneCore *core, LinphoneChatRoom *chat_room);

/**
 * Inconditionnaly disables incoming chat messages.
 * @param core A #LinphoneCore object @notnil
 * @param deny_reason the deny reason (#LinphoneReasonNone has no effect).
 **/
LINPHONE_PUBLIC void linphone_core_disable_chat(LinphoneCore *core, LinphoneReason deny_reason);

/**
 * Enables reception of incoming chat messages.
 * By default it is enabled but it can be disabled with linphone_core_disable_chat().
 * @param core A #LinphoneCore object @notnil
 **/
LINPHONE_PUBLIC void linphone_core_enable_chat(LinphoneCore *core);

/**
 * Returns whether chat is enabled.
 * @param core A #LinphoneCore object @notnil
 * @return TRUE if chat is enabled, FALSE otherwise
 **/
LINPHONE_PUBLIC bool_t linphone_core_chat_enabled(const LinphoneCore *core);

/**
 * Gets the #LinphoneImNotifPolicy object controlling the instant messaging delivery and displayed service
 * notifications.
 * @param core #LinphoneCore object @notnil
 * @return A #LinphoneImNotifPolicy object. @maybenil
 */
LINPHONE_PUBLIC LinphoneImNotifPolicy *linphone_core_get_im_notif_policy(const LinphoneCore *core);

/**
 * @}
 */

/**
 * Creates a content with default values from Linphone core.
 * @param core #LinphoneCore object @notnil
 * @return #LinphoneContent object with default values set @notnil
 * @ingroup misc
 */
LINPHONE_PUBLIC LinphoneContent *linphone_core_create_content(LinphoneCore *core);

/**
 * @addtogroup event_api
 * @{
 **/

/**
 * Creates and send an outgoing subscription, specifying the destination resource, the event name, and an optional
 *content body. If accepted, the subscription runs for a finite period, but is automatically renewed if not terminated
 *before.
 * @param core the #LinphoneCore @notnil
 * @param resource the destination resource @notnil
 * @param event the event name @notnil
 * @param expires the whished duration of the subscription
 * @param body an optional body, may be NULL. @maybenil
 * @return a #LinphoneEvent holding the context of the created subcription. @notnil
 **/
LINPHONE_PUBLIC LinphoneEvent *linphone_core_subscribe(
    LinphoneCore *core, LinphoneAddress *resource, const char *event, int expires, LinphoneContent *body);

/**
 * Creates an outgoing subscription, specifying the destination resource, the event name, and an optional content body.
 * If accepted, the subscription runs for a finite period, but is automatically renewed if not terminated before.
 * Unlike linphone_core_subscribe() the subscription isn't sent immediately. It will be send when calling
 *linphone_event_send_subscribe().
 * @param core the #LinphoneCore @notnil
 * @param resource the destination resource @notnil
 * @param event the event name @notnil
 * @param expires the whished duration of the subscription
 * @return a #LinphoneEvent holding the context of the created subcription. @notnil
 **/
LINPHONE_PUBLIC LinphoneEvent *
linphone_core_create_subscribe(LinphoneCore *core, LinphoneAddress *resource, const char *event, int expires);

/**
 * Creates an outgoing subscription, specifying the destination resource, the event name, and an optional content body.
 * If accepted, the subscription runs for a finite period, but is automatically renewed if not terminated before.
 * Unlike linphone_core_subscribe() the subscription isn't sent immediately. It will be send when calling
 *linphone_event_send_subscribe().
 * @param core the #LinphoneCore @notnil
 * @param resource the destination resource @notnil
 * @param proxy the proxy configuration to use @notnil
 * @param event the event name @notnil
 * @param expires the whished duration of the subscription
 * @return a #LinphoneEvent holding the context of the created subcription. @notnil
 **/
LINPHONE_PUBLIC LinphoneEvent *linphone_core_create_subscribe_2(
    LinphoneCore *core, LinphoneAddress *resource, LinphoneProxyConfig *proxy, const char *event, int expires);

/**
 * Creates an out-of-dialog notification, specifying the destination resource, the event name.
 * The notification can be send with linphone_event_notify().
 * @param core the #LinphoneCore @notnil
 * @param resource the destination resource @notnil
 * @param event the event name @notnil
 * @return a #LinphoneEvent holding the context of the notification. @notnil
 **/
LINPHONE_PUBLIC LinphoneEvent *
linphone_core_create_notify(LinphoneCore *core, LinphoneAddress *resource, const char *event);

/**
 * Publishes an event state.
 * This first create a #LinphoneEvent with linphone_core_create_publish() and calls linphone_event_send_publish() to
 *actually send it. After expiry, the publication is refreshed unless it is terminated before.
 * @param core the #LinphoneCore @notnil
 * @param resource the resource uri for the event @notnil
 * @param event the event name @notnil
 * @param expires the lifetime of event being published, -1 if no associated duration, in which case it will not be
 *refreshed.
 * @param body the actual published data @notnil
 * @return the #LinphoneEvent holding the context of the publish. @maybenil
 **/
LINPHONE_PUBLIC LinphoneEvent *linphone_core_publish(
    LinphoneCore *core, LinphoneAddress *resource, const char *event, int expires, LinphoneContent *body);

/**
 * Creates a publish context for an event state.
 * After being created, the publish must be sent using linphone_event_send_publish().
 * After expiry, the publication is refreshed unless it is terminated before.
 * @param core the #LinphoneCore @notnil
 * @param resource the resource uri for the event @notnil
 * @param event the event name @notnil
 * @param expires the lifetime of event being published, -1 if no associated duration, in which case it will not be
 *refreshed.
 * @return the #LinphoneEvent holding the context of the publish. @notnil
 **/
LINPHONE_PUBLIC LinphoneEvent *
linphone_core_create_publish(LinphoneCore *core, LinphoneAddress *resource, const char *event, int expires);

/**
 * Creates a publish context for a one-shot publish.
 * After being created, the publish must be sent using linphone_event_send_publish().
 * The #LinphoneEvent is automatically terminated when the publish transaction is finished, either with success or
 *failure. The application must not call linphone_event_terminate() for such one-shot publish.
 * @param core the #LinphoneCore @notnil
 * @param resource the resource uri for the event @notnil
 * @param event the event name @notnil
 * @return the #LinphoneEvent holding the context of the publish. @notnil
 **/
LINPHONE_PUBLIC LinphoneEvent *
linphone_core_create_one_shot_publish(LinphoneCore *core, LinphoneAddress *resource, const char *event);

/**
 * @}
 */

/**
 * @addtogroup buddy_list
 * @{
 */

/**
 * Creates an empty LinphoneFriend.
 * @param core #LinphoneCore object @notnil
 * @return The created #LinphoneFriend object @notnil
 */
LINPHONE_PUBLIC LinphoneFriend *linphone_core_create_friend(LinphoneCore *core);

/**
 * Creates a #LinphoneFriend from the given address.
 * @param core #LinphoneCore object @notnil
 * @param address A string containing the address to create the #LinphoneFriend from @notnil
 * @return The created #LinphoneFriend object. @maybenil
 */
LINPHONE_PUBLIC LinphoneFriend *linphone_core_create_friend_with_address(LinphoneCore *core, const char *address);

/**
 * Creates a #LinphoneFriend from ai #LinphoneVcard.
 * @param core #LinphoneCore object @notnil
 * @param vcard a #LinphoneVcard object @notnil
 * @return A new #LinphoneFriend object which has its vCard attribute initialized from the given vCard, accessible using
 * linphone_friend_get_vcard(). @maybenil
 */
LINPHONE_PUBLIC LinphoneFriend *linphone_core_create_friend_from_vcard(LinphoneCore *core, LinphoneVcard *vcard);

/**
 * Sets my presence model
 * @param core #LinphoneCore object @notnil
 * @param presence #LinphonePresenceModel @maybenil
 */
LINPHONE_PUBLIC void linphone_core_set_presence_model(LinphoneCore *core, LinphonePresenceModel *presence);

/**
 * Gets my presence model
 * @param core #LinphoneCore object @notnil
 * @return A #LinphonePresenceModel object, or NULL if no presence model has been set. @maybenil
 */
LINPHONE_PUBLIC LinphonePresenceModel *linphone_core_get_presence_model(const LinphoneCore *core);

/**
 * Gets my consolidated presence
 * @param core #LinphoneCore object @notnil
 * @return My #LinphoneConsolidatedPresence presence
 */
LINPHONE_PUBLIC LinphoneConsolidatedPresence linphone_core_get_consolidated_presence(const LinphoneCore *core);

/**
 * Sets my consolidated presence
 * @param core #LinphoneCore object @notnil
 * @param presence #LinphoneConsolidatedPresence value
 */
LINPHONE_PUBLIC void linphone_core_set_consolidated_presence(LinphoneCore *core, LinphoneConsolidatedPresence presence);

/**
 * Black list a friend. same as linphone_friend_set_inc_subscribe_policy() with #LinphoneSPDeny policy;
 * @param core #LinphoneCore object @notnil
 * @param linphone_friend #LinphoneFriend to reject @notnil
 */
LINPHONE_PUBLIC void linphone_core_reject_subscriber(LinphoneCore *core, LinphoneFriend *linphone_friend);

/**
 * Notifies all friends that have subscribed
 * @param core #LinphoneCore object @notnil
 * @param presence #LinphonePresenceModel to notify @notnil
 */
LINPHONE_PUBLIC void linphone_core_notify_all_friends(LinphoneCore *core, LinphonePresenceModel *presence);

/**
 * Searches a #LinphoneFriend by its address.
 * @param core #LinphoneCore object. @notnil
 * @param address The #LinphoneAddress to use to search the friend. @notnil
 * @return The #LinphoneFriend object corresponding to the given address or NULL if not found. @maybenil
 */
LINPHONE_PUBLIC LinphoneFriend *linphone_core_find_friend(const LinphoneCore *core, const LinphoneAddress *address);

/**
 * Searches a #LinphoneFriend by its phone number.
 * @param core #LinphoneCore object. @notnil
 * @param phone_number The phone number to use to search the friend. @notnil
 * @return The #LinphoneFriend object corresponding to the given phone number or NULL if not found. @maybenil
 */
LINPHONE_PUBLIC LinphoneFriend *linphone_core_find_friend_by_phone_number(const LinphoneCore *core,
                                                                          const char *phone_number);

/**
 * Searches all #LinphoneFriend matching an address.
 * @param core #LinphoneCore object. @notnil
 * @param address The address to use to search the friends. @notnil
 * @return A list of #LinphoneFriend corresponding to the given address. \bctbx_list{LinphoneFriend} @maybenil
 * @tobefreed
 */
LINPHONE_PUBLIC bctbx_list_t *linphone_core_find_friends(const LinphoneCore *core, const LinphoneAddress *address);

/**
 * Searches a #LinphoneFriend by its reference key.
 * @param core #LinphoneCore object. @notnil
 * @param key The reference key to use to search the friend. @notnil
 * @return The #LinphoneFriend object corresponding to the given reference key. @maybenil
 */
LINPHONE_PUBLIC LinphoneFriend *linphone_core_get_friend_by_ref_key(const LinphoneCore *core, const char *key);

/**
 * Sets the database filename where friends will be stored.
 * If the file does not exist, it will be created.
 * @ingroup initializing
 * @param core the #LinphoneCore. @notnil
 * @param path filesystem path. @maybenil
 * @deprecated 27/10/2023 Friends are now stored in the main db
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED void linphone_core_set_friends_database_path(LinphoneCore *core, const char *path);

/**
 * Creates a new empty #LinphoneFriendList object.
 * @param core #LinphoneCore object. @notnil
 * @return A new #LinphoneFriendList object. @notnil
 **/
LINPHONE_PUBLIC LinphoneFriendList *linphone_core_create_friend_list(LinphoneCore *core);

/**
 * Add a friend list.
 * @param core #LinphoneCore object @notnil
 * @param list #LinphoneFriendList object @notnil
 */
LINPHONE_PUBLIC void linphone_core_add_friend_list(LinphoneCore *core, LinphoneFriendList *list);

/**
 * Removes a friend list.
 * @param core #LinphoneCore object @notnil
 * @param list #LinphoneFriendList object @notnil
 */
LINPHONE_PUBLIC void linphone_core_remove_friend_list(LinphoneCore *core, LinphoneFriendList *list);

/**
 * Retrieves the list of #LinphoneFriendList from the core.
 * @param core #LinphoneCore object @notnil
 * @return A list of #LinphoneFriendList. \bctbx_list{LinphoneFriendList} @maybenil
 */
LINPHONE_PUBLIC const bctbx_list_t *linphone_core_get_friends_lists(const LinphoneCore *core);

/**
 * Retrieves the first list of #LinphoneFriend from the core.
 * @param core #LinphoneCore object @notnil
 * @return the first #LinphoneFriendList object or NULL. @maybenil
 */
LINPHONE_PUBLIC LinphoneFriendList *linphone_core_get_default_friend_list(const LinphoneCore *core);

/**
 * Retrieves the list of #LinphoneFriend from the core that has the given display name.
 * @param core #LinphoneCore object @notnil
 * @param name the name of the list @notnil
 * @return the first #LinphoneFriendList object or NULL. @maybenil
 */
LINPHONE_PUBLIC LinphoneFriendList *linphone_core_get_friend_list_by_name(const LinphoneCore *core, const char *name);

/**
 * Sets whether or not to start friend lists subscription when in foreground
 * @param core The #LinphoneCore @notnil
 * @param enable whether or not to enable the feature
 **/
LINPHONE_PUBLIC void linphone_core_enable_friend_list_subscription(LinphoneCore *core, bool_t enable);

/**
 * Returns whether or not friend lists subscription are enabled
 * @param core The #LinphoneCore @notnil
 * @return whether or not the feature is enabled
 * @deprecated 16/12/2021 Use linphone_core_friend_list_subscription_enabled() instead.
 **/
LINPHONE_PUBLIC bool_t linphone_core_is_friend_list_subscription_enabled(LinphoneCore *core);

/**
 * Returns whether or not friend lists subscription are enabled
 * @param core The #LinphoneCore @notnil
 * @return whether or not the feature is enabled
 **/
LINPHONE_PUBLIC bool_t linphone_core_friend_list_subscription_enabled(LinphoneCore *core);

/**
 * Retrieves a list of #LinphoneAddress sort and filter
 * @param core #LinphoneCore object @notnil
 * @param filter Chars used for the filter* @notnil
 * @param sip_only Only sip address or not
 * @return A list of filtered #LinphoneAddress + the #LinphoneAddress created with the filter.
 *\bctbx_list{LinphoneAddress} @maybenil
 **/
LINPHONE_PUBLIC const bctbx_list_t *
linphone_core_find_contacts_by_char(LinphoneCore *core, const char *filter, bool_t sip_only);

/**
 * Creates a #LinphonePresenceActivity with the given type and description.
 * @param core #LinphoneCore object. @notnil
 * @param acttype The #LinphonePresenceActivityType to set for the activity.
 * @param description An additional description of the activity to set for the activity. Can be NULL if no additional
 * description is to be added. @maybenil
 * @return The created #LinphonePresenceActivity object. @notnil
 */
LINPHONE_PUBLIC LinphonePresenceActivity *linphone_core_create_presence_activity(LinphoneCore *core,
                                                                                 LinphonePresenceActivityType acttype,
                                                                                 const char *description);

/**
 * Creates a default LinphonePresenceModel.
 * @param core #LinphoneCore object. @notnil
 * @return The created #LinphonePresenceModel object. @notnil
 */
LINPHONE_PUBLIC LinphonePresenceModel *linphone_core_create_presence_model(LinphoneCore *core);

/**
 * Creates a #LinphonePresenceModel with the given activity type and activity description.
 * @param core #LinphoneCore object. @notnil
 * @param acttype The #LinphonePresenceActivityType to set for the activity of the created model.
 * @param description An additional description of the activity to set for the activity. Can be NULL if no additional
 * description is to be added. @maybenil
 * @return The created #LinphonePresenceModel object. @notnil
 */
LINPHONE_PUBLIC LinphonePresenceModel *linphone_core_create_presence_model_with_activity(
    LinphoneCore *core, LinphonePresenceActivityType acttype, const char *description);

/**
 * Creates a #LinphonePresenceModel with the given activity type, activity description, note content and note language.
 * @param core #LinphoneCore object. @notnil
 * @param acttype The #LinphonePresenceActivityType to set for the activity of the created model.
 * @param description An additional description of the activity to set for the activity. Can be NULL if no additional
 * description is to be added. @maybenil
 * @param note The content of the note to be added to the created model. @notnil
 * @param lang The language of the note to be added to the created model. @maybenil
 * @return The created #LinphonePresenceModel object. @notnil
 */
LINPHONE_PUBLIC LinphonePresenceModel *
linphone_core_create_presence_model_with_activity_and_note(LinphoneCore *core,
                                                           LinphonePresenceActivityType acttype,
                                                           const char *description,
                                                           const char *note,
                                                           const char *lang);

/**
 * Creates a #LinphonePresenceNote with the given content and language.
 * @param core #LinphoneCore object. @notnil
 * @param content The content of the note to be created. @notnil
 * @param lang The language of the note to be created. @maybenil
 * @return The created #LinphonePresenceNote object. @notnil
 */
LINPHONE_PUBLIC LinphonePresenceNote *
linphone_core_create_presence_note(LinphoneCore *core, const char *content, const char *lang);

/**
 * Creates a #LinphonePresencePerson with the given id.
 * @param core #LinphoneCore object @notnil
 * @param id The id of the person to be created. @notnil
 * @return The created #LinphonePresencePerson object. @notnil
 */
LINPHONE_PUBLIC LinphonePresencePerson *linphone_core_create_presence_person(LinphoneCore *core, const char *id);

/**
 * Creates a #LinphonePresenceService with the given id, basic status and contact.
 * @param core #LinphoneCore object. @notnil
 * @param id The id of the service to be created. @notnil
 * @param basic_status The basic status of the service to be created.
 * @param contact A string containing a contact information corresponding to the service to be created. @notnil
 * @return The created #LinphonePresenceService object. @notnil
 */
LINPHONE_PUBLIC LinphonePresenceService *linphone_core_create_presence_service(LinphoneCore *core,
                                                                               const char *id,
                                                                               LinphonePresenceBasicStatus basic_status,
                                                                               const char *contact);

/**
 * Notifies the upper layer that a presence status has been received by calling the appropriate
 * callback if one has been set.
 * This method is for advanced usage, where customization of the liblinphone's internal behavior is required.
 * @param core the #LinphoneCore object. @notnil
 * @param linphone_friend the #LinphoneFriend whose presence information has been received. @notnil
 */
LINPHONE_PUBLIC void linphone_core_notify_notify_presence_received(LinphoneCore *core, LinphoneFriend *linphone_friend);

/**
 * Notifies the upper layer that a presence model change has been received for the uri or
 * telephone number given as a parameter, by calling the appropriate callback if one has been set.
 * This method is for advanced usage, where customization of the liblinphone's internal behavior is required.
 * @param core the #LinphoneCore object. @notnil
 * @param linphone_friend the #LinphoneFriend whose presence information has been received. @notnil
 * @param uri_or_tel telephone number or sip uri @notnil
 * @param presence_model the #LinphonePresenceModel that has been modified @notnil
 */
LINPHONE_PUBLIC void
linphone_core_notify_notify_presence_received_for_uri_or_tel(LinphoneCore *core,
                                                             LinphoneFriend *linphone_friend,
                                                             const char *uri_or_tel,
                                                             const LinphonePresenceModel *presence_model);

/**
 * Sets the size under which incoming files in chat messages will be downloaded automatically.
 * @param core #LinphoneCore object @notnil
 * @param size The size in bytes, -1 to disable the autodownload feature, 0 to download them all no matter the size
 * @ingroup chat
 **/
LINPHONE_PUBLIC void linphone_core_set_max_size_for_auto_download_incoming_files(LinphoneCore *core, int size);

/**
 * Gets the size under which incoming files in chat messages will be downloaded automatically.
 * @param core #LinphoneCore object @notnil
 * @return The size in bytes, -1 if autodownload feature is disabled, 0 to download them all no matter the size
 * @ingroup chat
 **/
LINPHONE_PUBLIC int linphone_core_get_max_size_for_auto_download_incoming_files(LinphoneCore *core);

/**
 * Enables automatic download of files attached to a chat message if it's content type matches the one we use for voice
 *recordings.
 * @param core #LinphoneCore object @notnil
 * @param auto_download_voice_recordings TRUE to automatically download incoming voice recordings, FALSE to disable it.
 * @ingroup chat
 * @deprecated 16/12/2021 Use linphone_core_enable_auto_download_voice_recordings() instead.
 **/
LINPHONE_PUBLIC void linphone_core_set_auto_download_voice_recordings_enabled(LinphoneCore *core,
                                                                              bool_t auto_download_voice_recordings);

/**
 * Enables automatic download of files attached to a chat message if it's content type matches the one we use for voice
 *recordings.
 * @param core #LinphoneCore object @notnil
 * @param auto_download_voice_recordings TRUE to automatically download incoming voice recordings, FALSE to disable it.
 * @ingroup chat
 **/
LINPHONE_PUBLIC void linphone_core_enable_auto_download_voice_recordings(LinphoneCore *core,
                                                                         bool_t auto_download_voice_recordings);

/**
 * Gets if the auto download for incoming voice recordings is enabled or not.
 * @param core #LinphoneCore object @notnil
 * @return TRUE if voice recordings will be automatically downloaded, FALSE otherwise.
 * @ingroup chat
 * @deprecated 16/12/2021 Use linphone_core_auto_download_voice_recordings_enabled() instead.
 **/
LINPHONE_PUBLIC bool_t linphone_core_is_auto_download_voice_recordings_enabled(LinphoneCore *core);

/**
 * Gets if the auto download for incoming voice recordings is enabled or not.
 * @param core #LinphoneCore object @notnil
 * @return TRUE if voice recordings will be automatically downloaded, FALSE otherwise.
 * @ingroup chat
 **/
LINPHONE_PUBLIC bool_t linphone_core_auto_download_voice_recordings_enabled(LinphoneCore *core);

/**
 * Automatically downloads files attached to a chat message if it's content type matches the one we use for icalendars.
 * @param core #LinphoneCore object @notnil
 * @param auto_download_icalendars TRUE to automatically download incoming icalendars, FALSE to disable it.
 * @ingroup chat
 * @deprecated 16/12/2021 Use linphone_core_enable_auto_download_icalendars() instead.
 **/
LINPHONE_PUBLIC void linphone_core_set_auto_download_icalendars_enabled(LinphoneCore *core,
                                                                        bool_t auto_download_icalendars);

/**
 * Automatically downloads files attached to a chat message if it's content type matches the one we use for icalendars.
 * @param core #LinphoneCore object @notnil
 * @param auto_download_icalendars TRUE to automatically download incoming icalendars, FALSE to disable it.
 * @ingroup chat
 **/
LINPHONE_PUBLIC void linphone_core_enable_auto_download_icalendars(LinphoneCore *core, bool_t auto_download_icalendars);

/**
 * Gets if the automatic download of incoming icalendars is enabled or not.
 * @param core #LinphoneCore object @notnil
 * @return TRUE if icalendars will be automatically downloaded, FALSE otherwise.
 * @ingroup chat
 * @deprecated 16/12/2021 Use linphone_core_auto_download_icalendars_enabled() instead.
 **/
LINPHONE_PUBLIC bool_t linphone_core_is_auto_download_icalendars_enabled(LinphoneCore *core);

/**
 * Gets if the automatic download of incoming icalendars is enabled or not.
 * @param core #LinphoneCore object @notnil
 * @return TRUE if icalendars will be automatically downloaded, FALSE otherwise.
 * @ingroup chat
 **/
LINPHONE_PUBLIC bool_t linphone_core_auto_download_icalendars_enabled(LinphoneCore *core);

/**
 * Returns whether or not sender name is hidden in a forwarded message.
 * @param core The #LinphoneCore @notnil
 * @return whether or not the feature
 **/
LINPHONE_PUBLIC bool_t linphone_core_is_sender_name_hidden_in_forward_message(LinphoneCore *core);

/**
 * Enables whether or not to hide sender name in forwarded message
 * @param core The #LinphoneCore @notnil
 * @param enable whether or not to enable the feature
 **/
LINPHONE_PUBLIC void linphone_core_enable_sender_name_hidden_in_forward_message(LinphoneCore *core, bool_t enable);

/**
 * Enables the record-aware feature that will warn other users when doing recording during a call.
 * @param core #LinphoneCore object @notnil
 * @param enable TRUE to activate the record aware feature, FALSE to disable it.
 * @deprecated 16/12/2021 Use linphone_core_enable_record_aware() instead.
 **/
LINPHONE_PUBLIC void linphone_core_set_record_aware_enabled(LinphoneCore *core, bool_t enable);

/**
 * Enables the record-aware feature that will warn other users when doing recording during a call.
 * See @LinphoneCallCbs for being notified when a call is being recorded.
 * @param core #LinphoneCore object @notnil
 * @param enable TRUE to activate the record aware feature, FALSE to disable it.
 **/
LINPHONE_PUBLIC void linphone_core_enable_record_aware(LinphoneCore *core, bool_t enable);

/**
 * Gets if the record aware feature is enabled or not.
 * @param core #LinphoneCore object @notnil
 * @return TRUE if the record aware feature is enabled, FALSE otherwise.
 * @deprecated 16/12/2021 Use linphone_core_record_aware_enabled() instead.
 **/
LINPHONE_PUBLIC bool_t linphone_core_is_record_aware_enabled(LinphoneCore *core);

/**
 * Gets if the record aware feature is enabled or not.
 * @param core #LinphoneCore object @notnil
 * @return TRUE if the record aware feature is enabled, FALSE otherwise.
 **/
LINPHONE_PUBLIC bool_t linphone_core_record_aware_enabled(LinphoneCore *core);

/**
 * Enables the automatic sending of 180 Ringing when receiving a call.
 * @param core #LinphoneCore object @notnil
 * @param enable TRUE to activate the automatic 180 Ringing, FALSE to disable it.
 **/
LINPHONE_PUBLIC void linphone_core_enable_auto_send_ringing(LinphoneCore *core, bool_t enable);

/**
 * Gets if the automatic sending of 180 Ringing is enabled or not.
 * @param core #LinphoneCore object @notnil
 * @return TRUE if the automatic sending of 180 Ringing is enabled, FALSE otherwise.
 **/
LINPHONE_PUBLIC bool_t linphone_core_auto_send_ringing_enabled(LinphoneCore *core);

/**
 * @}
 */

/**
 * Create a new #LinphoneNatPolicy object with every policies being disabled.
 * @param core #LinphoneCore object @notnil
 * @return A new #LinphoneNatPolicy object. @notnil
 * @ingroup network_parameters
 */
LINPHONE_PUBLIC LinphoneNatPolicy *linphone_core_create_nat_policy(LinphoneCore *core);

/**
 * Create a #LinphoneAccountCreator and set Linphone Request callbacks.
 * @param core The #LinphoneCore used for the XML-RPC communication @notnil
 * @param xmlrpc_url The URL to the XML-RPC server. @maybenil
 * @return The new #LinphoneAccountCreator object. @notnil
 * @ingroup account_creator
 * @deprecated 04/09/2024 : The #LinphoneAccountCreator interface is replaced by the #LinphoneAccountManagerServices
 *interface.
 **/
LINPHONE_PUBLIC LinphoneAccountCreator *linphone_core_create_account_creator(LinphoneCore *core,
                                                                             const char *xmlrpc_url);

/**
 * Creates a #LinphoneAccountManagerServices.
 * @param core The #LinphoneCore to use @notnil
 * @return The new #LinphoneAccountManagerServices object. @notnil
 * @ingroup account_creator
 **/
LINPHONE_PUBLIC LinphoneAccountManagerServices *linphone_core_create_account_manager_services(LinphoneCore *core);

/**
 * Create a #LinphoneXmlRpcSession for a given url.
 * @param core The #LinphoneCore used for the XML-RPC communication @notnil
 * @param url The URL to the XML-RPC server. Must be NON NULL. @notnil
 * @return The new #LinphoneXmlRpcSession object. @notnil
 * @ingroup misc
 **/
LINPHONE_PUBLIC LinphoneXmlRpcSession *linphone_core_create_xml_rpc_session(LinphoneCore *core, const char *url);

/**
 * Update current config with the content of a xml config file
 * @param core The #LinphoneCore to update @notnil
 * @param xml_uri the path to the xml file @notnil
 * @ingroup misc
 **/
LINPHONE_PUBLIC void linphone_core_load_config_from_xml(LinphoneCore *core, const char *xml_uri);

/**
 * Call this method when you receive a push notification (if you handle push notifications manually).
 * It will ensure the proxy configs are correctly registered to the proxy server,
 * so the call or the message will be correctly delivered.
 * @param core The #LinphoneCore @notnil
 * @ingroup misc
 * @deprecated 09/03/2022 See #linphone_core_process_push_notification() instead.
 **/
LINPHONE_PUBLIC void linphone_core_ensure_registered(LinphoneCore *core);

/**
 * Call this method when you receive a push notification (if you handle push notifications manually).
 * It will ensure the proxy configs are correctly registered to the proxy server,
 * so the call or the message will be correctly delivered.
 * @param core The #LinphoneCore @notnil
 * @param call_id the Call-ID of the MESSAGE or INVITE for which the push was received and to wait for. @maybenil
 * @ingroup misc
 **/
LINPHONE_PUBLIC void linphone_core_process_push_notification(LinphoneCore *core, const char *call_id);

/**
 * This method is called by Android & iOS platform helpers to notify the Core of a received push notification.
 * It will simply call Core->pushNotificationReceived() like linphone_core_process_push_notification().
 * @param core The #LinphoneCore @notnil
 * @param payload the payload of the push notification if any. @maybenil
 * @param call_id the Call-ID of the MESSAGE or INVITE for which the push was received and to wait for. @maybenil
 * @ingroup misc
 * @donotwrap
 **/
LINPHONE_PUBLIC void
linphone_core_push_notification_received(LinphoneCore *core, const char *payload, const char *call_id);

/**
 * This method is called by Android & iOS platform helpers to notify the Core of a received push notification.
 * It will simply call Core->pushNotificationReceived() like linphone_core_process_push_notification().
 * @param core The #LinphoneCore @notnil
 * @param payload the payload of the push notification if any. @maybenil
 * @param call_id the Call-ID of the MESSAGE or INVITE for which the push was received and to wait for. @maybenil
 * @param is_core_starting if TRUE the Core will skill network tasks we usually do when a push is received to ensure the
 *sockets are alive.
 * @ingroup misc
 * @donotwrap
 **/
LINPHONE_PUBLIC void linphone_core_push_notification_received_2(LinphoneCore *core,
                                                                const char *payload,
                                                                const char *call_id,
                                                                bool_t is_core_starting);

/**
 * Gets the chat message with the call_id included in the push notification body
 * This will start the core given in parameter, iterate until the message is received and return it.
 * By default, after 25 seconds the function returns because iOS kills the app extension after 30 seconds.
 * @warning Specific to iOS platform to satisfy contraints of notification service extension.
 * @param core The #LinphoneCore @notnil
 * @param call_id The callId of the Message SIP transaction @notnil
 * @return The #LinphoneChatMessage object. @maybenil
 * @ingroup chatroom
 **/
LINPHONE_PUBLIC LinphonePushNotificationMessage *linphone_core_get_new_message_from_callid(LinphoneCore *core,
                                                                                           const char *call_id);

/**
 * Gets the chat room we have been added into using the chat_room_addr included in the push notification body
 * This will start the core given in parameter, iterate until the new chat room is received and return it.
 * By default, after 25 seconds the function returns because iOS kills the app extension after 30 seconds.
 * @param core The #LinphoneCore @notnil
 * @param chat_room_addr The sip address of the chat room @notnil
 * @return The #LinphoneChatRoom object. @maybenil
 * @ingroup chatroom
 **/
LINPHONE_PUBLIC LinphoneChatRoom *linphone_core_get_new_chat_room_from_conf_addr(LinphoneCore *core,
                                                                                 const char *chat_room_addr);

/**
 * Enables or disables push notifications on Android & iOS.
 * If enabled, it will try to get the push token add configure each account with push_notification_allowed
 * set to true with push parameters. IOS: will also instanciate a PushRegistry, so make sure that your app does not
 * instanciate one too or there will be a conflict.
 * @param core The #LinphoneCore @notnil
 * @param enable TRUE to enable push notifications, FALSE to disable
 * @ingroup initializing
 * @deprecated 16/12/2021 Use linphone_core_enable_push_notification() instead.
 */
LINPHONE_PUBLIC void linphone_core_set_push_notification_enabled(LinphoneCore *core, bool_t enable);

/**
 * Enables or disables push notifications on Android & iOS.
 * If enabled, it will try to get the push token add configure each account with push_notification_allowed
 * set to true with push parameters. IOS: will also instanciate a PushRegistry, so make sure that your app does not
 * instanciate one too or there will be a conflict.
 * @param core The #LinphoneCore @notnil
 * @param enable TRUE to enable push notifications, FALSE to disable
 * @ingroup initializing
 */
LINPHONE_PUBLIC void linphone_core_enable_push_notification(LinphoneCore *core, bool_t enable);

/**
 * Gets whether push notifications are enabled or not (Android & iOS only). If not, the app will have to handle all the
 * push-related settings for each accounts
 * @param core The #LinphoneCore @notnil
 * @return TRUE if push notifications are enabled, FALSE otherwise
 * @ingroup initializing
 * @deprecated 16/12/2021 Use linphone_core_push_notification_enabled() instead.
 */
LINPHONE_PUBLIC bool_t linphone_core_is_push_notification_enabled(LinphoneCore *core);

/**
 * Gets whether push notifications are enabled or not (Android & iOS only). If not, the app will have to handle all the
 * push-related settings for each accounts
 * @param core The #LinphoneCore @notnil
 * @return TRUE if push notifications are enabled, FALSE otherwise
 * @ingroup initializing
 */
LINPHONE_PUBLIC bool_t linphone_core_push_notification_enabled(LinphoneCore *core);

/**
 * Gets whether push notifications are available or not (Android & iOS only).
 * @param core The #LinphoneCore @notnil
 * @return TRUE if push notifications are available, FALSE otherwise
 * @ingroup initializing
 */
LINPHONE_PUBLIC bool_t linphone_core_is_push_notification_available(LinphoneCore *core);

/**
 * Sets device_token when application didRegisterForRemoteNotificationsWithDeviceToken (IOS only).
 * @param core The #LinphoneCore @notnil
 * @param device_token format (NSData *). @maybenil
 * @ingroup initializing
 */
LINPHONE_PUBLIC void linphone_core_did_register_for_remote_push(LinphoneCore *core, void *device_token);

/**
 * Sets device_token when application didRegisterForRemoteNotificationsWithDeviceToken (IOS only).
 * @param core The #LinphoneCore @notnil
 * @param device_token_str string extracted from the Data objectf received in
 * didRegisterForRemoteNotificationsWithDeviceToken ios function. Append ":remote" after data formating.. @maybenil
 * @ingroup initializing
 */
LINPHONE_PUBLIC void linphone_core_did_register_for_remote_push_with_stringified_token(LinphoneCore *core,
                                                                                       const char *device_token_str);
/**
 * Sets the dispatch queue that will be used to receive VOIP push notifications, and process
 * enterbackground/enternockground notification from the application delegate. Main queue by default. Must be use before
 * core is started (iOS only).
 * @param core The #LinphoneCore @notnil
 * @param dispatch_queue dispatch_queue. @notnil
 * @ingroup initializing
 * @donotwrap
 * @deprecated 03/06/2025 the "Push and AppDelegate" dispatch queue is now called the "Core queue". It is also used for
 * auto-iterate. The proper way to set the Core Queue is too pass it during the core creation using the SystemContext
 * (void*) parameter.
 */
LINPHONE_PUBLIC void linphone_core_set_push_and_app_delegate_dispatch_queue(LinphoneCore *core, void *dispatch_queue);

/**
 * Enable or disable the automatic schedule of #linphone_core_iterate() method on Android & iOS.
 * If enabled, #linphone_core_iterate() will be called on the main thread every 20ms automatically.
 * If disabled, it is the application that must do this job.
 * @param core The #LinphoneCore @notnil
 * @param enable TRUE to enable auto iterate, FALSE to disable
 * @ingroup initializing
 * @deprecated 16/12/2021 Use linphone_core_enable_auto_iterate() instead.
 */
LINPHONE_PUBLIC void linphone_core_set_auto_iterate_enabled(LinphoneCore *core, bool_t enable);

/**
 * Enable or disable the automatic schedule of #linphone_core_iterate() method on Android & iOS.
 * If enabled, #linphone_core_iterate() will be called on the main thread every 20ms automatically.
 * If disabled, it is the application that must do this job.
 * @param core The #LinphoneCore @notnil
 * @param enable TRUE to enable auto iterate, FALSE to disable
 * @ingroup initializing
 */
LINPHONE_PUBLIC void linphone_core_enable_auto_iterate(LinphoneCore *core, bool_t enable);

/**
 * Gets whether auto iterate is enabled or not (Android & iOS only).
 * @param core The #LinphoneCore @notnil
 * @return TRUE if #linphone_core_iterate() is scheduled automatically, FALSE otherwise
 * @ingroup initializing
 * @deprecated 16/12/2021 Use linphone_core_auto_iterate_enabled() instead.
 */
LINPHONE_PUBLIC bool_t linphone_core_is_auto_iterate_enabled(const LinphoneCore *core);

/**
 * Gets whether auto iterate is enabled or not (Android & iOS only).
 * @param core The #LinphoneCore @notnil
 * @return TRUE if #linphone_core_iterate() is scheduled automatically, FALSE otherwise
 * @ingroup initializing
 */
LINPHONE_PUBLIC bool_t linphone_core_auto_iterate_enabled(const LinphoneCore *core);

/**
 * Gets the timer used to schedule the call to core.iterate() method when in foreground (Android only).
 * This is only used when #linphone_core_auto_iterate_enabled() returns TRUE.
 * @param core The #LinphoneCore @notnil
 * @return The timing in milliseconds used to schedule the call while in foreground (default is 20ms).
 * @ingroup misc
 */
LINPHONE_PUBLIC int linphone_core_get_auto_iterate_foreground_schedule(const LinphoneCore *core);

/**
 * Sets the timer used to schedule the call to core.iterate() method when in foreground (Android only).
 * @param core The #LinphoneCore @notnil
 * @param schedule The timing in milliseconds used to schedule the call while in foreground.
 * @ingroup misc
 */
LINPHONE_PUBLIC void linphone_core_set_auto_iterate_foreground_schedule(LinphoneCore *core, int schedule);

/**
 * Gets the timer used to schedule the call to core.iterate() method when in background (Android only).
 * This is only used when #linphone_core_auto_iterate_enabled() returns TRUE.
 * @param core The #LinphoneCore @notnil
 * @return The timing in milliseconds used to schedule the call while in background (default is 500ms).
 * @ingroup misc
 */
LINPHONE_PUBLIC int linphone_core_get_auto_iterate_background_schedule(const LinphoneCore *core);

/**
 * Sets the timer used to schedule the call to core.iterate() method when in background (Android only).
 * @param core The #LinphoneCore @notnil
 * @param schedule The timing in milliseconds used to schedule the call while in background.
 * @ingroup misc
 */
LINPHONE_PUBLIC void linphone_core_set_auto_iterate_background_schedule(LinphoneCore *core, int schedule);

/**
 * Enable vibration will incoming call is ringing (Android only).
 * @param core The #LinphoneCore @notnil
 * @param enable TRUE to enable the vibration on incoming call, FALSE otherwise
 * @ingroup misc
 * @deprecated 16/12/2021 Use linphone_core_enable_vibration_on_incoming_call() instead.
 */
LINPHONE_PUBLIC void linphone_core_set_vibration_on_incoming_call_enabled(LinphoneCore *core, bool_t enable);

/**
 * Enable vibration will incoming call is ringing (Android only).
 * @param core The #LinphoneCore @notnil
 * @param enable TRUE to enable the vibration on incoming call, FALSE otherwise
 * @ingroup misc
 */
LINPHONE_PUBLIC void linphone_core_enable_vibration_on_incoming_call(LinphoneCore *core, bool_t enable);

/**
 * Gets whether the device will vibrate while an incoming call is ringing (Android only).
 * @param core The #LinphoneCore @notnil
 * @return TRUE if the device will vibrate (if possible), FALSE otherwise
 * @ingroup misc
 * @deprecated 16/12/2021 Use linphone_core_vibration_on_incoming_call_enabled() instead.
 */
LINPHONE_PUBLIC bool_t linphone_core_is_vibration_on_incoming_call_enabled(LinphoneCore *core);

/**
 * Gets whether the device will vibrate while an incoming call is ringing (Android only).
 * @param core The #LinphoneCore @notnil
 * @return TRUE if the device will vibrate (if possible), FALSE otherwise
 * @ingroup misc
 */
LINPHONE_PUBLIC bool_t linphone_core_vibration_on_incoming_call_enabled(LinphoneCore *core);

/**
 * Returns a list of audio devices, with only the first device for each type
 * To have the list of all audio devices, use #linphone_core_get_extended_audio_devices()
 * @param core The #LinphoneCore @notnil
 * @returns \bctbx_list{LinphoneAudioDevice} A list with the first #LinphoneAudioDevice of each type @maybenil
 * @tobefreed
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC bctbx_list_t *linphone_core_get_audio_devices(const LinphoneCore *core);

/**
 * Returns the list of all audio devices
 * @param core The #LinphoneCore @notnil
 * @returns \bctbx_list{LinphoneAudioDevice} A list of all #LinphoneAudioDevice @maybenil @tobefreed
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC bctbx_list_t *linphone_core_get_extended_audio_devices(const LinphoneCore *core);

/**
 * Sets the given #LinphoneAudioDevice as input for all active calls.
 * @param core The #LinphoneCore @notnil
 * @param audio_device The #LinphoneAudioDevice. NULL does nothing. @maybenil
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_input_audio_device(LinphoneCore *core, LinphoneAudioDevice *audio_device);

/**
 * Sets the given #LinphoneAudioDevice as output for all active calls.
 * @param core The #LinphoneCore @notnil
 * @param audio_device The #LinphoneAudioDevice. NULL does nothing. @maybenil
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_output_audio_device(LinphoneCore *core, LinphoneAudioDevice *audio_device);

/**
 * Gets the input audio device for the current call
 * @param core The #LinphoneCore @notnil
 * @returns The input audio device for the current or first call, NULL if there is no call. @maybenil
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC const LinphoneAudioDevice *linphone_core_get_input_audio_device(const LinphoneCore *core);

/**
 * Gets the output audio device for the current call
 * @param core The #LinphoneCore @notnil
 * @returns The output audio device for the current or first call, NULL if there is no call. @maybenil
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC const LinphoneAudioDevice *linphone_core_get_output_audio_device(const LinphoneCore *core);

/**
 * Sets the given #LinphoneAudioDevice as default input for next calls.
 * @param core The #LinphoneCore @notnil
 * @param audio_device The #LinphoneAudioDevice @maybenil
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_default_input_audio_device(LinphoneCore *core,
                                                                  LinphoneAudioDevice *audio_device);

/**
 * Sets the given #LinphoneAudioDevice as default output for next calls.
 * @param core The #LinphoneCore @notnil
 * @param audio_device The #LinphoneAudioDevice @maybenil
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_default_output_audio_device(LinphoneCore *core,
                                                                   LinphoneAudioDevice *audio_device);

/**
 * Gets the default input audio device
 * @param core The #LinphoneCore @notnil
 * @returns The default input audio device @maybenil
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC const LinphoneAudioDevice *linphone_core_get_default_input_audio_device(const LinphoneCore *core);

/**
 * Gets the default output audio device
 * @param core The #LinphoneCore @notnil
 * @returns The default output audio device @maybenil
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC const LinphoneAudioDevice *linphone_core_get_default_output_audio_device(const LinphoneCore *core);

/**
 * Sets the default ephemeral message mode
 * @param core the #LinphoneCore. @notnil
 * @param mode default ephemeral message mode #LinphoneChatRoomEphemeralMode
 * @ingroup chatroom
 */
LINPHONE_PUBLIC void linphone_core_chat_room_set_default_ephemeral_mode(LinphoneCore *core,
                                                                        LinphoneChatRoomEphemeralMode mode);

/**
 * Gets the default ephemeral message mode
 * @param core the #LinphoneCore. @notnil
 * @return the default ephemeral message mode #LinphoneChatRoomEphemeralMode
 * @ingroup chatroom
 **/
LINPHONE_PUBLIC LinphoneChatRoomEphemeralMode
linphone_core_chat_room_get_default_ephemeral_mode(const LinphoneCore *core);

/**
 * Set the default ephemeral lifetime in seconds
 * @param core the #LinphoneCore. @notnil
 * @param value lifetime of ephemeral messages in seconds
 * @ingroup chatroom
 */
LINPHONE_PUBLIC void linphone_core_set_default_ephemeral_lifetime(LinphoneCore *core, long value);

/**
 * Gets the default lifetime of ephemeral messages in seconds
 * @param core the #LinphoneCore. @notnil
 * @return lifetime of ephemeral messages in seconds
 * @ingroup chatroom
 **/
LINPHONE_PUBLIC long linphone_core_get_default_ephemeral_lifetime(const LinphoneCore *core);

/**
 * Enable sending conference invitations in the chat message body instead of as a file attachment.
 * @param core the #LinphoneCore
 * @param enable TRUE to send ICS as message body, FALSE to send it as file transfer
 * @ingroup conference
 */
LINPHONE_PUBLIC void linphone_core_enable_conference_ics_in_message_body(LinphoneCore *core, bool_t enable);

/**
 * Gets wether conference invitations will be sent in the chat message body or as a file attachment.
 * @param core the #LinphoneCore
 * @return TRUE if ICS will be sent in the message body (by default), FALSE if it will be sent as a file attachment.
 * @ingroup conference
 */
LINPHONE_PUBLIC bool_t linphone_core_conference_ics_in_message_body_enabled(const LinphoneCore *core);

/**
 * Sets the maximum number of thumbnails requested in the SDP during a conference call
 * @param core the #LinphoneCore. @notnil
 * @param max the maximum number of thumbnails requested in the SDP during a conference call
 * @ingroup conference
 **/
LINPHONE_PUBLIC void linphone_core_set_conference_max_thumbnails(LinphoneCore *core, int max);

/**
 * Gets the maximum number of thumbnails requested in the SDP during a conference call
 *linphone_account_get_call_logs().
 * @param core the #LinphoneCore. @notnil
 * @return the maximum number of thumbnails requested in the SDP during a conference call
 * @ingroup conference
 **/
LINPHONE_PUBLIC int linphone_core_get_conference_max_thumbnails(const LinphoneCore *core);

/**
 * Set the default conference layout
 * @param core the #LinphoneCore. @notnil
 * @param value layout
 * @ingroup conference
 */
LINPHONE_PUBLIC void linphone_core_set_default_conference_layout(LinphoneCore *core, LinphoneConferenceLayout value);

/**
 * Gets the default conference layout
 * @param core the #LinphoneCore. @notnil
 * @return conference layout
 * @ingroup conference
 **/
LINPHONE_PUBLIC LinphoneConferenceLayout linphone_core_get_default_conference_layout(const LinphoneCore *core);

/**
 * Set the conference cleanup timer period. This timer helps managing the automatic deletion of ended conferences. In
 * fact, under normal circumstances a conference is deleted only if it transition from an active to an inactive state
 * after its end time. Nonetheless a side effect is that there may be a conference that never became active or it was
 * terminate before its due date and time. This timer, if setup, therefore periodically looks for expired conference and
 * cleans then up
 * @param core the #LinphoneCore. @notnil
 * @param seconds period of the timer. A 0 or negative value stops the timer
 * @ingroup conference
 */
LINPHONE_PUBLIC void linphone_core_set_conference_cleanup_period(LinphoneCore *core, long seconds);

/**
 * Gets the conference cleanup timer period
 * @param core the #LinphoneCore. @notnil
 * @return the period of the conference cleanup timer
 * @ingroup conference
 **/
LINPHONE_PUBLIC long linphone_core_get_conference_cleanup_period(const LinphoneCore *core);

/**
 * Set the conference availability before start. It is the number of seconds clients can join the conference before its
 * actual start time.
 * @param core the #LinphoneCore. @notnil
 * @param seconds number of seconds the conference can be joined early. A negative value means always
 * @ingroup conference
 * @warning This setting is only applicable to conference servers
 */
LINPHONE_PUBLIC void linphone_core_set_conference_availability_before_start(LinphoneCore *core, long seconds);

/**
 * Gets the conference availability before start
 * @param core the #LinphoneCore. @notnil
 * @return the number of seconds the conference can be joined early
 * @ingroup conference
 * @warning This setting is only applicable to conference servers
 **/
LINPHONE_PUBLIC long linphone_core_get_conference_availability_before_start(const LinphoneCore *core);

/**
 * Set the IMDN resend period. It is the number of seconds after the first attempt to send, an IMDN message is sent
 * again on startup if failed earlier on
 * @param core the #LinphoneCore. @notnil
 * @param seconds number of seconds after the first attempt to send an IMDN, it is retried at startup. A negative
 * value means all IMDNs are resent at startup.
 * @ingroup chatroom
 */
LINPHONE_PUBLIC void linphone_core_set_imdn_resend_period(LinphoneCore *core, long seconds);

/**
 * Gets the IMDN resend period
 * @param core the #LinphoneCore. @notnil
 * @return the number of second to resend an failed IMDN message
 * @ingroup chatroom
 **/
LINPHONE_PUBLIC long linphone_core_get_imdn_resend_period(const LinphoneCore *core);

/**
 * Set the conference expire period. It is the number of seconds after the end time or the last participant joined -
 * whichever is later - the conference cannot be joined anymore
 * @param core the #LinphoneCore. @notnil
 * @param seconds number of seconds before the conference expires. A 0 or negative value means immediately after the end
 * @ingroup conference
 * @warning This setting is only applicable to conference servers
 */
LINPHONE_PUBLIC void linphone_core_set_conference_expire_period(LinphoneCore *core, long seconds);

/**
 * Gets the conference expire period
 * @param core the #LinphoneCore. @notnil
 * @return the number of second after which the conference cannot be joined
 * @ingroup conference
 * @warning This setting is only applicable to conference servers
 **/
LINPHONE_PUBLIC long linphone_core_get_conference_expire_period(const LinphoneCore *core);

/**
 * Retrieve the conference information linked to the provided URI if any.
 * @param core #LinphoneCore object. @notnil
 * @param uri Uri of the conference as provided by the CCMP server backend. @notnil
 * @return The #LinphoneConferenceInfo found if any, NULL otherwise. @tobefreed @maybenil
 * @ingroup conference
 */
LINPHONE_PUBLIC LinphoneConferenceInfo *linphone_core_find_conference_information_from_ccmp_uri(LinphoneCore *core,
                                                                                                const char *uri);

/**
 * Retrieve the conference information linked to the provided URI if any.
 * @param core #LinphoneCore object. @notnil
 * @param uri #LinphoneAddress of the uri. @notnil
 * @return The #LinphoneConferenceInfo found if any, NULL otherwise. @tobefreed @maybenil
 * @ingroup conference
 */
LINPHONE_PUBLIC LinphoneConferenceInfo *linphone_core_find_conference_information_from_uri(LinphoneCore *core,
                                                                                           LinphoneAddress *uri);

/**
 * Retrieve the list of conference information on DB.
 * @param core #LinphoneCore object. @notnil
 * @return The list of conference infos \bctbx_list{LinphoneConferenceInfo}. @tobefreed @maybenil
 * @ingroup conference
 */
LINPHONE_PUBLIC bctbx_list_t *linphone_core_get_conference_information_list(LinphoneCore *core);

#if 0
/**
 * Retrieve the list of conference information on DB.
 * @param core #LinphoneCore object. @notnil
 * @param capabilities the list of conference capabilities that the conference information must has set
 * \bctbx_list{LinphoneStreamType}. @tobefreed @maybenil
 * @return The list of conference infos \bctbx_list{LinphoneConferenceInfo}. @maybenil
 * @ingroup conference
 */
LINPHONE_PUBLIC bctbx_list_t *linphone_core_get_conference_information_list_2(LinphoneCore *core,
                                                                              bctbx_list_t *capabilities);
#endif

/**
 * Retrieve the list of conference information on DB where the address passed as argument is either the organizer or a
 * participant.
 * @param core #LinphoneCore object. @notnil
 * @param uri #LinphoneAddress of the participant. @notnil
 * @return The list of conference infos \bctbx_list{LinphoneConferenceInfo} where the address passed as argument is
 * either the organizer or a participant. @tobefreed @maybenil
 * @ingroup conference
 */
LINPHONE_PUBLIC bctbx_list_t *linphone_core_get_conference_informations_with_participant(LinphoneCore *core,
                                                                                         LinphoneAddress *uri);

/**
 * Retrieve the list of future conference information on DB.
 * @param core #LinphoneCore object. @notnil
 * @return The list of future conference infos \bctbx_list{LinphoneConferenceInfo}. @tobefreed @maybenil
 * @ingroup conference
 */
LINPHONE_PUBLIC bctbx_list_t *linphone_core_get_future_conference_information_list(LinphoneCore *core);

#if 0
/**
 * Retrieve the list of future conference information on DB.
 * @param core #LinphoneCore object. @notnil
 * @param capabilities the list of conference capabilities that the conference information must has set
 * \bctbx_list{LinphoneStreamType}. @tobefreed @maybenil
 * @return The list of future conference infos \bctbx_list{LinphoneConferenceInfo}. @maybenil
 * @ingroup conference
 */
LINPHONE_PUBLIC bctbx_list_t *linphone_core_get_future_conference_information_list_2(LinphoneCore *core,
                                                                                     bctbx_list_t *capabilities);
#endif

/**
 * Retrieve the list of conference information on DB after a certain time.
 * @param core #LinphoneCore object. @notnil
 * @param time Time to retrieve conference info.
 * @return The list of conference infos \bctbx_list{LinphoneConferenceInfo}. @tobefreed @maybenil
 * @ingroup conference
 */
LINPHONE_PUBLIC bctbx_list_t *linphone_core_get_conference_information_list_after_time(LinphoneCore *core, time_t time);

#if 0
/**
 * Retrieve the list of conference information on DB after a certain time.
 * @param core #LinphoneCore object. @notnil
 * @param time Time to retrieve conference info.
 * @param capabilities the list of conference capabilities that the conference information must has set
 * \bctbx_list{LinphoneStreamType}. @tobefreed @maybenil
 * @return The list of conference infos \bctbx_list{LinphoneConferenceInfo}. @maybenil
 * @ingroup conference
 */
LINPHONE_PUBLIC bctbx_list_t *
linphone_core_get_conference_information_list_after_time_2(LinphoneCore *core, time_t time, bctbx_list_t *capabilities);
#endif

/**
 * Deletes a conference information from DB.
 * @param core #LinphoneCore object. @notnil
 * @param conference_info the #LinphoneConferenceInfo to delete. @notnil
 * @ingroup conference
 */
LINPHONE_PUBLIC void linphone_core_delete_conference_information(LinphoneCore *core,
                                                                 LinphoneConferenceInfo *conference_info);

/**
 * @addtogroup contacts
 * @{
 */

/**
 * Creates a new CardDAV server params object that may be used later by #LinphoneMagicSearch to query contacts.
 * @param core #LinphoneCore object. @notnil
 * @param params the #LinphoneCardDavParams to configure this remote contact directory. @notnil
 * @return a new #LinphoneRemoteContactDirectory object to configure, and then add using
 * linphone_core_add_remote_contact_directory(). @notnil
 */
LINPHONE_PUBLIC LinphoneRemoteContactDirectory *
linphone_core_create_card_dav_remote_contact_directory(LinphoneCore *core, LinphoneCardDavParams *params);

/**
 * Creates a new CardDAV server params object that may be used later by #LinphoneMagicSearch to query contacts.
 * @param core #LinphoneCore object. @notnil
 * @param params the #LinphoneLdapParams to configure this remote contact directory. @notnil
 * @return a new #LinphoneRemoteContactDirectory object to configure, and then add using
 * linphone_core_add_remote_contact_directory(). @notnil
 */
LINPHONE_PUBLIC LinphoneRemoteContactDirectory *
linphone_core_create_ldap_remote_contact_directory(LinphoneCore *core, LinphoneLdapParams *params);

/**
 * Adds a #LinphoneRemoteContactDirectory object previously created to the Core, to be used later by
 * #LinphoneMagicSearch to query contacts using either LDAP or CardDAV.
 * @param core #LinphoneCore object. @notnil
 * @param remoteContactDirectory the newly created #LinphoneRemoteContactDirectory to add. @notnil
 */
LINPHONE_PUBLIC void linphone_core_add_remote_contact_directory(LinphoneCore *core,
                                                                LinphoneRemoteContactDirectory *remoteContactDirectory);

/**
 * Removes a #LinphoneRemoteContactDirectory object previously added to the Core.
 * @param core #LinphoneCore object. @notnil
 * @param remoteContactDirectory the #LinphoneRemoteContactDirectory to remove. @notnil
 */
LINPHONE_PUBLIC void
linphone_core_remove_remote_contact_directory(LinphoneCore *core,
                                              LinphoneRemoteContactDirectory *remoteContactDirectory);

/**
 * Gets the list of currently configured LDAP or CardDAV remote servers used by #LinphoneMagicSearch.
 * @param core #LinphoneCore object. @notnil
 * @return the list of #LinphoneRemoteContactDirectory currently configured, if any.
 * \bctbx_list{LinphoneRemoteContactDirectory} @maybenil @tobefreed
 */
LINPHONE_PUBLIC bctbx_list_t *linphone_core_get_remote_contact_directories(LinphoneCore *core);

/**
 * Creates a new CardDAV server params object that may be used later by #LinphoneMagicSearch to query contacts.
 * @param core #LinphoneCore object. @notnil
 * @return a new #LinphoneCardDavParams object to configure, and then use it to create a #LinphoneRemoteContactDirectory
 * using linphone_core_create_card_dav_remote_contact_directory(). @notnil
 */
LINPHONE_PUBLIC LinphoneCardDavParams *linphone_core_create_card_dav_params(LinphoneCore *core);

/**
 * Creates a vCard from a text, if possible
 * @param core #LinphoneCore object. @notnil
 * @param input the vCard raw text to parse. @notnil
 * @return a new #LinphoneVcard object if the parsing of the input succeeded, NULL otherwise. @maybenil
 */
LINPHONE_PUBLIC LinphoneVcard *linphone_core_create_vcard_from_text(const LinphoneCore *core, const char *input);

/**
 * @}
 * End of group contacts
 */

/**
 * Tells if LDAP is available
 * @param core LinphoneCore object @notnil
 * @ingroup misc
 * @return TRUE if LDAP is available, FALSE otherwise
 **/
LINPHONE_PUBLIC bool_t linphone_core_ldap_available(LinphoneCore *core);

/**
 * @addtogroup ldap
 * @{
 */

/**
 * Create a LDAP params using default values from Linphone core.
 * Check #linphone_ldap_params to update values.
 * In order to add a new LDAP configuration to #LinphoneMagicSearch, these parameters must be passed to
 * linphone_core_create_ldap_with_params. Or, use linphone_ldap_set_params().
 *
 * @param core #LinphoneCore object @notnil
 * @return #LinphoneLdapParams with default values set. @notnil @tobefreed
 */
LINPHONE_PUBLIC LinphoneLdapParams *linphone_core_create_ldap_params(LinphoneCore *core);

/**
 * Creates an empty LDAP search.
 *
 * linphone_ldap_set_params() must be call to save the parameters in the configuration file.
 *
 * @param core #LinphoneCore object @notnil
 * @return #LinphoneLdap with default values set @notnil @tobefreed
 * @deprecated 18/11/2024 use linphone_core_create_ldap_remote_contact_directory() instead.
 */
LINPHONE_PUBLIC LinphoneLdap *linphone_core_create_ldap(LinphoneCore *core);

/**
 * Creates a LDAP search using given parameters and store them in the configuration file.
 *
 * @param core #LinphoneCore object @notnil
 * @param params #LinphoneLdapParams object @notnil
 * @return #LinphoneLdap with default values set @notnil @tobefreed
 * @deprecated 18/11/2024 use linphone_core_create_ldap_remote_contact_directory() instead.
 */
LINPHONE_PUBLIC LinphoneLdap *linphone_core_create_ldap_with_params(LinphoneCore *core, LinphoneLdapParams *params);

/**
 * Erases all LDAP from the configuration.
 * @param core #LinphoneCore object @notnil
 * @deprecated 18/11/2024 use linphone_core_remove_remote_contact_directory() instead.
 *
 **/
LINPHONE_PUBLIC void linphone_core_clear_ldaps(LinphoneCore *core);

/**
 * Add or update a LDAP server and save it to the configuration.
 * @param core #LinphoneCore object @notnil
 * @param ldap The LDAP to add/update. @notnil
 * @deprecated 18/11/2024 use linphone_core_add_remote_contact_directory() instead.
 **/
LINPHONE_PUBLIC void linphone_core_add_ldap(LinphoneCore *core, LinphoneLdap *ldap);

/**
 * Remove a LDAP from the configuration.
 * @param core #LinphoneCore object @notnil
 * @param ldap The LDAP to remove. @notnil
 * @deprecated 18/11/2024 use linphone_core_remove_remote_contact_directory() instead.
 **/
LINPHONE_PUBLIC void linphone_core_remove_ldap(LinphoneCore *core, LinphoneLdap *ldap);

/**
 * Returns a list of entered LDAPs. Items must be freed with linphone_ldap_unref()
 * @param core The #LinphoneCore object @notnil
 * @return \bctbx_list{LinphoneLdap} @maybenil @tobefreed
 * @deprecated 18/11/2024 use linphone_core_get_remote_contact_directories() instead.
 **/
LINPHONE_PUBLIC bctbx_list_t *linphone_core_get_ldap_list(LinphoneCore *core);

/**
 * @}
 * End of group ldap
 */

/**
 * Returns whether chat messages grouping is enabled or not.
 * @param core The #LinphoneCore object @notnil
 * @return TRUE if received chat messages will be notified as a bundle, FALSE otherwise.
 * @ingroup chatroom
 **/
LINPHONE_PUBLIC bool_t linphone_core_get_chat_messages_aggregation_enabled(LinphoneCore *core);

/**
 * Sets whether chat messages grouping is enabled or not.
 * This optimisation is turned on by default. It allows to receive bulks of incoming message faster,
 * and notify them to the application in a row.
 * Set [sip] chat_messages_aggregation in your configuration file for the timer, default is 500ms.
 * @param core The #LinphoneCore object @notnil
 * @param enabled TRUE to wait for chat messages and notify them as at once, FALSE to keep legacy behavior.
 * @ingroup chatroom
 **/
LINPHONE_PUBLIC void linphone_core_set_chat_messages_aggregation_enabled(LinphoneCore *core, bool_t enabled);

/**
 * Forces a flush of the config to disk.
 * @param core The #LinphoneCore object @notnil
 * @return 0 if successful, -1 otherwise
 * @ingroup misc
 **/
LINPHONE_PUBLIC LinphoneStatus linphone_core_config_sync(LinphoneCore *core);

/**
 * Gets the push notification configuration object if it exists.
 * @param core The #LinphoneCore object @notnil
 * @return the #LinphonePushNotificationConfig if it exists, NULL otherwise. @maybenil
 * @ingroup initializing
 **/
LINPHONE_PUBLIC const LinphonePushNotificationConfig *
linphone_core_get_push_notification_config(const LinphoneCore *core);

/**
 * Return the list of loaded plugins
 * @param core A #LinphoneCore object @notnil
 * @return the list of loaded plugins \bctbx_list{char *} @maybenil
 * @ingroup misc
 */
LINPHONE_PUBLIC const bctbx_list_t *linphone_core_get_loaded_plugins(LinphoneCore *core);

/**
 * Tells whether a plugin is loaded or not.
 * @param core A #LinphoneCore object @notnil
 * @param name name of the plugin @notnil
 * @return A boolean value telling whether the plugin has been loaded
 * @ingroup misc
 */
LINPHONE_PUBLIC bool_t linphone_core_is_plugin_loaded(const LinphoneCore *core, const char *name);

/**
 * Set the refresh window. During this window, belle-sip schedules the refreshes of the sip messages
 * @param core A #LinphoneCore object @notnil
 * @param min_value lower bound of the refresh window
 * @param max_value upper bound of the refresh window
 * @warning The refresh window must be set before starting the core
 * @ingroup account
 */
LINPHONE_PUBLIC void linphone_core_set_refresh_window(LinphoneCore *core, const int min_value, const int max_value);

/**
 * Get the number of participants including me, if it in, in the running conference. The local
 * participant is included in the count only if it is in the conference.
 * @param core #LinphoneCore @notnil
 * @return The number of participants including me, if it in.
 * @deprecated 16/04/2021 Use linphone_conference_get_participant_count() instead.
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED int linphone_core_get_conference_size(LinphoneCore *core);

/**
 * Get a pointer on the internal conference object.
 * @param core #LinphoneCore @notnil
 * @return A pointer on #LinphoneConference or NULL if no conference are going on. @maybenil
 * @deprecated 10/08/2023 Use linphone_core_search_conference() instead.
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneConference *linphone_core_get_conference(LinphoneCore *core);

/**
 * Are PostQuantum algoritms available
 * @return  TRUE if Post Quantum algorithms are available FALSE otherwise
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC bool_t linphone_core_get_post_quantum_available(void);

/**
 * Return the list of the available ZRTP key agreement algorithns.
 * @param core The core. @notnil
 * @return A freshly allocated list of the available algorithms. The list
 * must be destroyed with bctbx_list_free() after usage. The elements of the list
 * haven't to be unref. @bctbx_list{LinphoneZrtpKeyAgreement} @maybenil @tobefreed
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC bctbx_list_t *linphone_core_get_zrtp_available_key_agreement_list(LinphoneCore *core);

/**
 * Return the ordonated list of the ZRTP key agreement algorithns currently configured.
 * @param core The core. @notnil
 * @return A freshly allocated list of the available algorithms. The list
 * must be destroyed with bctbx_list_free() after usage. The elements of the list
 * haven't to be unref. @bctbx_list{LinphoneZrtpKeyAgreement} @maybenil @tobefreed
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC bctbx_list_t *linphone_core_get_zrtp_key_agreement_list(LinphoneCore *core);

/**
 * Redefine the list of prefered ZRTP key agreement algorithms.
 * @param core The core. @notnil
 * @param key_agreements The new list of key agreements algorithms, in order of preference. The core does not take
 * ownership on it. The setting accepts a maximum of 7 algorithms, if the list is longer, only the first 7 available
 * algorithms are selected \bctbx_list{LinphoneZrtpKeyAgreement} @maybenil
 * @ingroup media_parameters
 */
LINPHONE_PUBLIC void linphone_core_set_zrtp_key_agreement_suites(LinphoneCore *core, bctbx_list_t *key_agreements);

/**
 * Enable the deletion of empty chatrooms (i.e. chatrooms with no participants)
 * @param core A #LinphoneCore object @notnil
 * @param enable A boolean value telling whether to enable or disable the deletion of chat rooms with no participants in
 * (Applicable to servers only) it
 * @ingroup chatroom
 */
LINPHONE_PUBLIC void linphone_core_enable_empty_chatrooms_deletion(LinphoneCore *core, bool_t enable);

/**
 * Tells whether empty chat rooms are deleted or not.
 * @param core A #LinphoneCore object @notnil
 * @return A boolean value telling whether the deletion of empty chatrooms is enabled or not (Applicable to servers
 * only)
 * @ingroup chatroom
 */
LINPHONE_PUBLIC bool_t linphone_core_empty_chatrooms_deletion_enabled(const LinphoneCore *core);

/**
 * Returns the threshold for sending IMDN to all participants to a #LinphoneChatRoom
 * @param core A #LinphoneCore object @notnil
 * @return An integer value telling the threshold for sending IMDN to all participants to a #LinphoneChatRoom
 * @ingroup chatroom
 */
LINPHONE_PUBLIC int linphone_core_get_imdn_to_everybody_threshold(const LinphoneCore *core);

/**
 * Sets the threshold for sending IMDN to all participants to a #LinphoneChatRoom
 * @param core A #LinphoneCore object @notnil
 * @param threshold the threshold for sending IMDN to all participants to a #LinphoneChatRoom
 * @ingroup chatroom
 */
LINPHONE_PUBLIC void linphone_core_set_imdn_to_everybody_threshold(LinphoneCore *core, int threshold);

/**
 * Sets if accounts will wait for network to be connected before trying to REGISTER.
 * @param core the #LinphoneCore
 * @param register_only_when_network_is_up TRUE to wait for an internet connection before trying to REGISTER, FALSE to
 * do it no matter the network state.
 * @ingroup initializing
 */
LINPHONE_PUBLIC void linphone_core_set_register_only_when_network_is_up(LinphoneCore *core,
                                                                        bool_t register_only_when_network_is_up);

/**
 * Gets if accounts will wait for network to be connected before trying to REGISTER or not.
 * @param core the #LinphoneCore
 * @return TRUE if accounts will wait for internet connection before trying to REGISTER, FALSE otherwise.
 * @ingroup initializing
 */
LINPHONE_PUBLIC bool_t linphone_core_get_register_only_when_network_is_up(const LinphoneCore *core);

/************ */
/* DEPRECATED */
/* ********** */

/**
 * Search from the list of current calls if a remote address match uri
 * @ingroup call_control
 * @param core the #LinphoneCore object. @notnil
 * @param uri which should match call remote uri @notnil
 * @return #LinphoneCall or NULL if no match is found. @maybenil
 * @deprecated 27/10/2020. Use linphone_core_get_call_by_remote_address2() instead.
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneCall *linphone_core_find_call_from_uri(const LinphoneCore *core,
                                                                                   const char *uri);

/**
 * Create some default conference parameters for instanciating a conference with
 *linphone_core_create_conference_with_params().
 * @param core the #LinphoneCore object @notnil
 * @return a #LinphoneConferenceParams object. @notnil
 * @deprecated 23/07/2024. Use linphone_core_create_conference_params_2() instead.
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneConferenceParams *
linphone_core_create_conference_params(LinphoneCore *core);

/**
 * @brief Define a log handler.
 * @param logfunc The function pointer of the log handler.
 * @deprecated 10/10/2017 Use #linphone_logging_service_cbs_set_log_message_written() instead.
 * @donotwrap
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED void linphone_core_set_log_handler(OrtpLogFunc logfunc);

/**
 * @brief Define a log file.
 *
 * If the file pointer passed as an argument is NULL, stdout is used instead.
 * @param file A pointer to the FILE structure of the file to write to.
 * @deprecated 10/10/2017 Use #linphone_log_service_set_file() instead.
 * @donotwrap
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED void linphone_core_set_log_file(FILE *file);

/**
 * @brief Define the minimum level for logging.
 * @param loglevel Minimum level for logging messages.
 * @deprecated 10/10/2017 Use #linphone_logging_service_set_log_level() instead.
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED void linphone_core_set_log_level(OrtpLogLevel loglevel);

/**
 * Define the log level using mask.
 *
 * The loglevel parameter is a bitmask parameter. Therefore to enable only warning and error
 * messages, use ORTP_WARNING | ORTP_ERROR. To disable logs, simply set loglevel to 0.
 *
 * @param mask A bitmask of the log levels to set.
 * @deprecated 10/10/2017 Use #linphone_logging_service_set_log_level() instead.
 * @donotwrap
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED void linphone_core_set_log_level_mask(unsigned int mask);

/**
 * Get defined log level mask.
 *
 * @return The loglevel parameter is a bitmask parameter. Therefore to enable only warning and error
 * messages, use ORTP_WARNING | ORTP_ERROR. To disable logs, simply set loglevel to 0.
 * @deprecated 10/10/2017 Use #linphone_logging_service_get_log_level_mask() instead.
 * @donotwrap
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED unsigned int linphone_core_get_log_level_mask(void);

/**
 * Enable logs in supplied FILE*.
 * @param file a C FILE* where to fprintf logs. If null stdout is used.
 * @deprecated 12/01/2017 Use #linphone_core_set_log_file and #linphone_core_set_log_level() instead.
 * @donotwrap
 **/
LINPHONE_DEPRECATED LINPHONE_PUBLIC void linphone_core_enable_logs(FILE *file);

/**
 * Enable logs through the user's supplied log callback.
 * @param logfunc The address of a OrtpLogFunc callback whose protoype is
 *            	  typedef void (*OrtpLogFunc)(OrtpLogLevel lev, const char *fmt, va_list args);
 * @deprecated 12/01/2017 Use #linphone_core_set_log_handler() and #linphone_core_set_log_level() instead.
 * @donotwrap
 **/
LINPHONE_DEPRECATED LINPHONE_PUBLIC void linphone_core_enable_logs_with_cb(OrtpLogFunc logfunc);

/**
 * Entirely disable logging.
 * @deprecated 12/01/2017 Use #linphone_core_set_log_level() instead.
 * @donotwrap
 **/
LINPHONE_DEPRECATED LINPHONE_PUBLIC void linphone_core_disable_logs(void);

/**
 * @deprecated 19/11/2015 Use #linphone_core_get_user_agent() instead.
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED const char *linphone_core_get_user_agent_name(void);

/**
 * @deprecated 19/11/2015 Use #linphone_core_get_user_agent() instead.
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED const char *linphone_core_get_user_agent_version(void);

/**
 * Gets the database filename where friends will be stored.
 * @ingroup initializing
 * @param core the #LinphoneCore. @notnil
 * @return filesystem path. @maybenil
 * @deprecated 27/10/2023 Friends are now stored in the main db
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED const char *linphone_core_get_friends_database_path(LinphoneCore *core);

/**
 * Instanciates a #LinphoneCore object.
 * @ingroup initializing
 *
 * The #LinphoneCore object is the primary handle for doing all phone actions.
 * It should be unique within your application.
 * @param vtable a #LinphoneCoreVTable structure holding your application callbacks
 * @param config_path a path to a config file. If it does not exists it will be created.
 *        The config file is used to store all settings, call logs, friends, proxies... so that all these settings
 *        become persistent over the life of the LinphoneCore object.
 *        It is allowed to set a NULL config file. In that case LinphoneCore will not store any settings.
 * @param factory_config_path a path to a read-only config file that can be used to
 *        to store hard-coded preference such as proxy settings or internal preferences.
 *        The settings in this factory file always override the one in the normal config file.
 *        It is OPTIONAL, use NULL if unneeded.
 * @param userdata an opaque user pointer that can be retrieved at any time (for example in
 *        callbacks) using linphone_core_get_user_data().
 * @see linphone_core_new_with_config()
 * @deprecated 12/01/2017 Use #linphone_factory_create_core() instead.
 * @donotwrap
 **/
LINPHONE_DEPRECATED LINPHONE_PUBLIC LinphoneCore *linphone_core_new(const LinphoneCoreVTable *vtable,
                                                                    const char *config_path,
                                                                    const char *factory_config_path,
                                                                    void *userdata);

/**
 * Instantiates a #LinphoneCore object with a given LpConfig.
 * @ingroup initializing
 *
 * The #LinphoneCore object is the primary handle for doing all phone actions.
 * It should be unique within your application.
 * @param vtable a #LinphoneCoreVTable structure holding your application callbacks
 * @param config a pointer to an LpConfig object holding the configuration of the #LinphoneCore to be instantiated.
 * @param userdata an opaque user pointer that can be retrieved at any time (for example in
 *        callbacks) using linphone_core_get_user_data().
 * @see linphone_core_new()
 * @deprecated 12/01/2017 Use #linphone_factory_create_core_with_config() instead.
 * @donotwrap
 **/
LINPHONE_DEPRECATED LINPHONE_PUBLIC LinphoneCore *
linphone_core_new_with_config(const LinphoneCoreVTable *vtable, LpConfig *config, void *userdata);

/**
 * @ingroup initializing
 * add a listener to be notified of linphone core events. Once events are received, registered vtable are invoked in
 * order.
 * @param vtable a #LinphoneCoreVTable structure holding your application callbacks. Object is owned by linphone core
 * until linphone_core_remove_listener.
 * @param core object
 * @deprecated 12/01/2017 Use linphone_core_add_callbacks() instead.
 * @donotwrap
 */
LINPHONE_DEPRECATED LINPHONE_PUBLIC void linphone_core_add_listener(LinphoneCore *core, LinphoneCoreVTable *vtable);

/**
 * @ingroup initializing
 * remove a listener registred by linphone_core_add_listener.
 * @param core object
 * @param vtable a #LinphoneCoreVTable structure holding your application callbacks.
 * @deprecated 12/01/2017 Use linphone_core_remove_callbacks() instead.
 * @donotwrap
 */
LINPHONE_DEPRECATED LINPHONE_PUBLIC void linphone_core_remove_listener(LinphoneCore *core,
                                                                       const LinphoneCoreVTable *vtable);

/**
 * @brief Performs a simple call transfer to the specified destination.
 *
 * The remote endpoint is expected to issue a new call to the specified destination.
 * The current call remains active and thus can be later paused or terminated.
 * It is possible to follow the progress of the transfer provided that transferee sends notification about it.
 * In this case, the transfer_state_changed callback of the #LinphoneCoreVTable is invoked to notify of the state of the
 * new call at the other party. The notified states are #LinphoneCallStateOutgoingInit ,
 *#LinphoneCallStateOutgoingProgress, #LinphoneCallStateOutgoingRinging and #LinphoneCallStateConnected.
 * @param core #LinphoneCore object
 * @param call The call to be transferred
 * @param refer_to The destination the call is to be referred to
 * @return 0 on success, -1 on failure
 * @ingroup call_control
 * @deprecated 12/01/2017 Use #linphone_call_transfer() instead.
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneStatus linphone_core_transfer_call(LinphoneCore *core,
                                                                               LinphoneCall *call,
                                                                               const char *refer_to);

/**
 * @brief Transfers a call to destination of another running call. This is used for "attended transfer" scenarios.
 *
 * The transferred call is supposed to be in paused state, so that it is able to accept the transfer immediately.
 * The destination call is a call previously established to introduce the transferred person.
 * This method will send a transfer request to the transferred person. The phone of the transferred is then
 * expected to automatically call to the destination of the transfer. The receiver of the transfer will then
 * automatically close the call with us (the 'dest' call). It is possible to follow the progress of the transfer
 *provided that transferee sends notification about it. In this case, the transfer_state_changed callback of the
 * #LinphoneCoreVTable is invoked to notify of the state of the new call at the other party. The notified states are
 * #LinphoneCallStateOutgoingInit , #LinphoneCallStateOutgoingProgress, #LinphoneCallStateOutgoingRinging and
 *#LinphoneCallStateConnected.
 * @param core #LinphoneCore object
 * @param call A running call you want to transfer
 * @param dest A running call whose remote person will receive the transfer
 * @return 0 on success, -1 on failure
 * @ingroup call_control
 * @deprecated 12/01/2017 Use #linphone_call_transfer_to_another() instead.
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneStatus linphone_core_transfer_call_to_another(LinphoneCore *core,
                                                                                          LinphoneCall *call,
                                                                                          LinphoneCall *dest);

/**
 * @brief Accept an incoming call.
 *
 * Basically the application is notified of incoming calls within the
 * call_state_changed callback of the #LinphoneCoreVTable structure, where it will receive
 * a #LinphoneCallStateIncoming event with the associated #LinphoneCall object.
 * The application can later accept the call using this method.
 * @param core #LinphoneCore object
 * @param call The #LinphoneCall object representing the call to be answered
 * @return 0 on success, -1 on failure
 * @ingroup call_control
 * @deprecated 13/02/2017 Use #linphone_call_accept() instead.
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneStatus linphone_core_accept_call(LinphoneCore *core, LinphoneCall *call);

/**
 * @brief Accept an incoming call, with parameters.
 *
 * Basically the application is notified of incoming calls within the
 * call_state_changed callback of the #LinphoneCoreVTable structure, where it will receive
 * a #LinphoneCallStateIncoming event with the associated #LinphoneCall object.
 * The application can later accept the call using
 * this method.
 * @param core #LinphoneCore object
 * @param call The #LinphoneCall object representing the call to be answered
 * @param params The specific parameters for this call, for example whether video is accepted or not. Use NULL to use
 * default parameters
 * @return 0 on success, -1 on failure
 * @ingroup call_control
 * @deprecated 13/02/2017 Use #linphone_call_accept_with_params() instead.
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneStatus
linphone_core_accept_call_with_params(LinphoneCore *core, LinphoneCall *call, const LinphoneCallParams *params);

/**
 * @brief When receiving an incoming, accept to start a media session as early-media.
 *
 * This means the call is not accepted but audio & video streams can be established if the remote party supports early
 * media. However, unlike after call acceptance, mic and camera input are not sent during early-media, though received
 * audio & video are played normally. The call can then later be fully accepted using linphone_core_accept_call() or
 * linphone_core_accept_call_with_params().
 * @param core #LinphoneCore object
 * @param call The call to accept
 * @param params The call parameters to use (can be NULL)
 * @return 0 if successful, -1 otherwise
 * @ingroup call_control
 * @deprecated 10/10/2017 Use linphone_call_accept_early_media_with_params() instead.
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneStatus
linphone_core_accept_early_media_with_params(LinphoneCore *core, LinphoneCall *call, const LinphoneCallParams *params);

/**
 * @brief Accept an early media session for an incoming call.
 *
 * This is identical as calling linphone_core_accept_early_media_with_params() with NULL call parameters.
 * @param core #LinphoneCore object
 * @param call The incoming call to accept
 * @return 0 if successful, -1 otherwise
 * @ingroup call_control
 * @see linphone_core_accept_early_media_with_params()
 * @deprecated 13/02/2017 Use #linphone_call_accept_early_media() instead.
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneStatus linphone_core_accept_early_media(LinphoneCore *core,
                                                                                    LinphoneCall *call);

/**
 * @brief Terminates a call.
 *
 * @param core LinphoneCore object
 * @param call The LinphoneCall object representing the call to be terminated
 * @return 0 on success, -1 on failure
 * @ingroup call_control
 * @deprecated 13/02/2017 Use #linphone_call_terminate() instead.
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneStatus linphone_core_terminate_call(LinphoneCore *core, LinphoneCall *call);

/**
 * Redirect the specified call to the given redirect URI.
 * @param core #LinphoneCore object
 * @param call The #LinphoneCall to redirect
 * @param redirect_uri The URI to redirect the call to
 * @return 0 if successful, -1 on error.
 * @ingroup call_control
 * @deprecated 13/02/2017 Use #linphone_call_redirect() instead.
 * @donotwrap
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneStatus linphone_core_redirect_call(LinphoneCore *core,
                                                                               LinphoneCall *call,
                                                                               const char *redirect_uri);

/**
 * @brief Decline a pending incoming call, with a reason.
 * @param core #LinphoneCore object
 * @param call The #LinphoneCall to decline, must be in the IncomingReceived state
 * @param reason The reason for rejecting the call: #LinphoneReasonDeclined or #LinphoneReasonBusy
 * @return 0 on success, -1 on failure
 * @ingroup call_control
 * @deprecated 13/02/2017 Use #linphone_call_decline() instead.
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneStatus linphone_core_decline_call(LinphoneCore *core,
                                                                              LinphoneCall *call,
                                                                              LinphoneReason reason);

/**
 * @brief Pauses the call. If a music file has been setup using linphone_core_set_play_file(),
 * this file will be played to the remote user.
 *
 * The only way to resume a paused call is to call linphone_core_resume_call().
 * @param core #LinphoneCore object
 * @param call The call to pause
 * @return 0 on success, -1 on failure
 * @ingroup call_control
 * @see linphone_core_resume_call()
 * @deprecated 13/02/2017 Use #linphone_call_pause() instead.
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneStatus linphone_core_pause_call(LinphoneCore *core, LinphoneCall *call);

/**
 * @brief Resumes a call.
 *
 * The call needs to have been paused previously with linphone_core_pause_call().
 * @param core #LinphoneCore object
 * @param call The call to resume
 * @return 0 on success, -1 on failure
 * @ingroup call_control
 * @see linphone_core_pause_call()
 * @deprecated 13/02/2017 Use #linphone_call_resume() instead.
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneStatus linphone_core_resume_call(LinphoneCore *core, LinphoneCall *call);

/**
 * @brief Updates a running call according to supplied call parameters or parameters changed in the LinphoneCore.
 *
 * In this version this is limited to the following use cases:
 * - setting up/down the video stream according to the video parameter of the #LinphoneCallParams (see
 * linphone_call_params_enable_video() ).
 * - changing the size of the transmitted video after calling linphone_core_set_preferred_video_size()
 * In case no changes are requested through the #LinphoneCallParams argument, then this argument can be omitted and set
 * to NULL. WARNING: Updating a call in the #LinphoneCallStatePaused state will still result in a paused call even if
 *the media directions set in the params are sendrecv. To resume a paused call, you need to call
 * linphone_core_resume_call().
 *
 * @param core #LinphoneCore object
 * @param call The call to be updated
 * @param params The new call parameters to use (may be NULL)
 * @return 0 if successful, -1 otherwise.
 * @ingroup call_control
 * @deprecated 13/02/2017 Use #linphone_call_update() instead.
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneStatus linphone_core_update_call(LinphoneCore *core,
                                                                             LinphoneCall *call,
                                                                             const LinphoneCallParams *params);

/**
 * When receiving a #LinphoneCallStateUpdatedByRemote state notification, prevent #LinphoneCore from performing an
 *automatic answer.
 *
 * When receiving a #LinphoneCallStateUpdatedByRemote state notification (ie an incoming reINVITE), the default
 *behaviour of #LinphoneCore is defined by the "defer_update_default" option of the "sip" section of the config. If this
 *option is 0 (the default) then the #LinphoneCore automatically answers the reINIVTE with call parameters unchanged.
 *However when for example when the remote party updated the call to propose a video stream, it can be useful to prompt
 *the user before answering. This can be achieved by calling linphone_core_defer_call_update() during the call state
 * notification, to deactivate the automatic answer that would just confirm the audio but reject the video. Then, when
 * the user responds to dialog prompt, it becomes possible to call linphone_core_accept_call_update() to answer the
 * reINVITE, with eventually video enabled in the #LinphoneCallParams argument.
 *
 * The #LinphoneCallStateUpdatedByRemote notification can also arrive when receiving an INVITE without SDP. In such
 *case, an unchanged offer is made in the 200Ok, and when the ACK containing the SDP answer is received,
 * #LinphoneCallStateUpdatedByRemote is triggered to notify the application of possible changes in the media session.
 *However in such case defering the update has no meaning since we just generating an offer.
 *
 * @param core #LinphoneCore object
 * @param call The call for which to defer the update
 * @return 0 if successful, -1 if the linphone_core_defer_call_update() was done outside a valid
 * #LinphoneCallStateUpdatedByRemote notification
 * @ingroup call_control
 * @deprecated 13/02/2017 Use linphone_call_defer_update() instead
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneStatus linphone_core_defer_call_update(LinphoneCore *core,
                                                                                   LinphoneCall *call);

/**
 * @brief Accept call modifications initiated by other end.
 *
 * This call may be performed in response to a #LinphoneCallStateUpdatedByRemote state notification.
 * When such notification arrives, the application can decide to call linphone_core_defer_update_call() so that it can
 * have the time to prompt the user. linphone_call_get_remote_params() can be used to get information about the call
 * parameters requested by the other party, such as whether a video stream is requested.
 *
 * When the user accepts or refuse the change, linphone_core_accept_call_update() can be done to answer to the other
 * party. If params is NULL, then the same call parameters established before the update request will continue to be
 *used (no change). If params is not NULL, then the update will be accepted according to the parameters passed. Typical
 * example is when a user accepts to start video, then params should indicate that video stream should be used (see
 * linphone_call_params_enable_video()).
 * @param core #LinphoneCore object
 * @param call The call for which to accept an update
 * @param params A #LinphoneCallParams object describing the call parameters to accept
 * @return 0 if successful, -1 otherwise (actually when this function call is performed outside ot
 *#LinphoneCallStateUpdatedByRemote state)
 * @ingroup call_control
 * @deprecated 13/02/2017 Use #linphone_call_accept_update() instead.
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneStatus linphone_core_accept_call_update(LinphoneCore *core,
                                                                                    LinphoneCall *call,
                                                                                    const LinphoneCallParams *params);

/**
 * Get the call with the remote_address specified
 * @param core #LinphoneCore object @notnil
 * @param remote_address The remote address of the call that we want to get @notnil
 * @return The call if it has been found, NULL otherwise. @maybenil
 * @ingroup call_control
 * @deprecated 08/07/2020 use linphone_core_get_call_by_remote_address2() instead
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneCall *linphone_core_get_call_by_remote_address(const LinphoneCore *core,
                                                                                           const char *remote_address);

/**
 * @brief Send the specified dtmf.
 *
 * This function only works during calls. The dtmf is automatically played to the user.
 * @param core The #LinphoneCore object
 * @param dtmf The dtmf name specified as a char, such as '0', '#' etc...
 * @deprecated 23/11/2015 Use #linphone_call_send_dtmf() instead.
 * @ingroup media_parameters
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED void linphone_core_send_dtmf(LinphoneCore *core, char dtmf);

/**
 * Same as linphone_core_get_primary_contact() but the result is a #LinphoneAddress object
 * instead of const char *.
 * @param core the #LinphoneCore @notnil
 * @return a #LinphoneAddress object. @maybenil @tobefreed
 * @ingroup proxies
 * @deprecated 22/10/2018 Use linphone_core_create_primary_contact_parsed() instead.
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneAddress *linphone_core_get_primary_contact_parsed(LinphoneCore *core);

/**
 * Same as linphone_core_get_primary_contact() but the result is a #LinphoneAddress object
 * instead of a string.
 * @param core the #LinphoneCore @notnil
 * @return a #LinphoneAddress object. @maybenil @tobefreed
 * @ingroup proxies
 **/
LINPHONE_PUBLIC LinphoneAddress *linphone_core_get_primary_contact_address(LinphoneCore *core);

/**
 * Returns the list of available audio codecs.
 * @param core The #LinphoneCore object
 * @return A list of #OrtpPayloadType. @bctbx_list{OrtpPayloadType}
 *
 * This list is unmodifiable. The ->data field of the bctbx_list_t points a PayloadType
 * structure holding the codec information.
 * It is possible to make copy of the list with bctbx_list_copy() in order to modify it
 * (such as the order of codecs).
 * @ingroup media_parameters
 * @deprecated 31/03/2017 Use linphone_core_get_audio_payload_types() instead.
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED const bctbx_list_t *linphone_core_get_audio_codecs(const LinphoneCore *core);

/**
 * Sets the list of audio codecs.
 * @param core The #LinphoneCore object
 * @param codecs The new list of codecs. The list is taken by the #LinphoneCore,
 * thus the application should not free it. \bctbx_list{OrtpPayloadType}
 * @return 0
 * @deprecated 31/03/2017 Use linphone_core_set_audio_payload_types() instead.
 * @ingroup media_parameters
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneStatus linphone_core_set_audio_codecs(LinphoneCore *core,
                                                                                  bctbx_list_t *codecs);

/**
 * Returns the list of available video codecs.
 * @param core The #LinphoneCore object
 * @return A list of #OrtpPayloadType. \bctbx_list{OrtpPayloadType}
 *
 * This list is unmodifiable. The ->data field of the bctbx_list_t points a PayloadType
 * structure holding the codec information.
 * It is possible to make copy of the list with bctbx_list_copy() in order to modify it
 * (such as the order of codecs).
 * @ingroup media_parameters
 * @deprecated 31/03/2017 Use linphone_core_get_video_payload_types() instead.
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED const bctbx_list_t *linphone_core_get_video_codecs(const LinphoneCore *core);

/**
 * Sets the list of video codecs.
 * @param core The #LinphoneCore object
 * @param codecs The new list of codecs. The list is taken by the #LinphoneCore,
 * thus the application should not free it. \bctbx_list{OrtpPayloadType}
 * @return 0
 * @deprecated 31/03/2017 Use linphone_core_set_video_payload_types() instead.
 * @ingroup media_parameters
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneStatus linphone_core_set_video_codecs(LinphoneCore *core,
                                                                                  bctbx_list_t *codecs);

/**
 * Returns the list of available text codecs.
 * @param core The #LinphoneCore object
 * @return A list of OrtpPayloadType. \bctbx_list{OrtpPayloadType}
 *
 * This list is unmodifiable. The ->data field of the bctbx_list_t points a PayloadType
 * structure holding the codec information.
 * It is possible to make copy of the list with bctbx_list_copy() in order to modify it
 * (such as the order of codecs).
 * @ingroup media_parameters
 * @deprecated 31/03/2017 Use linphone_core_get_text_payload_types() instead.
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED const bctbx_list_t *linphone_core_get_text_codecs(const LinphoneCore *core);

/**
 * Sets the list of text codecs.
 * @param core The #LinphoneCore object
 * @param codecs The new list of codecs. The list is taken by the #LinphoneCore,
 * thus the application should not free it. \bctbx_list{LinphonePayloadType}
 * @return 0
 * @deprecated 31/03/2017 Use linphone_core_set_text_payload_types() instead.
 * @ingroup media_parameters
 * @donotwrap
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneStatus linphone_core_set_text_codecs(LinphoneCore *core,
                                                                                 bctbx_list_t *codecs);

/**
 * Enable RFC3389 generic comfort noise algorithm (CN payload type).
 * It is disabled by default, because this algorithm is only relevant for legacy codecs (PCMU, PCMA, G722).
 * @param core #LinphoneCore object
 * @param enabled TRUE if enabled, FALSE otherwise.
 * @deprecated 20/12/2016 Use linphone_core_enable_generic_comfort_noise() instead
 * @donotwrap
 */
#define linphone_core_enable_generic_confort_noise(core, enabled)                                                      \
	linphone_core_enable_generic_comfort_noise(core, enabled)

/**
 * Returns enablement of RFC3389 generic comfort noise algorithm.
 * @param core #LinphoneCore object
 * @return TRUE or FALSE.
 * @deprecated 20/12/2016 Use linphone_core_generic_comfort_noise_enabled() instead
 * @donotwrap
 */
#define linphone_core_generic_confort_noise_enabled(core) linphone_core_generic_comfort_noise_enabled(core)

/**
 * Enable text sending via Baudot tones in the audio stream.
 * @ingroup media_parameters
 * It is disabled by default.
 * Enablement requires a SDK built with full Baudot support: -DENABLE_BAUDOT=ON .
 * @param core #LinphoneCore object @notnil
 * @param enabled TRUE if enabled, FALSE otherwise.
 **/
LINPHONE_PUBLIC void linphone_core_enable_baudot(LinphoneCore *core, bool_t enabled);

/**
 * Returns enablement of text sending via Baudot tones in the audio stream.
 * @ingroup media_parameters
 * @param core #LinphoneCore object @notnil
 * @return TRUE if text sending via Baudot tones in the audio stream is enabled, FALSE otherwise.
 **/
LINPHONE_PUBLIC bool_t linphone_core_baudot_enabled(const LinphoneCore *core);

/**
 * Tells whether the specified payload type is enabled.
 * @param core #LinphoneCore object.
 * @param pt The payload type to check.
 * @return TRUE if the payload type is enabled, FALSE if disabled.
 * @ingroup media_parameters
 * @deprecated 31/03/2017 Use linphone_payload_type_enabled() instead.
 * @donotwrap
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED bool_t linphone_core_payload_type_enabled(const LinphoneCore *core,
                                                                              const OrtpPayloadType *pt);

/**
 * Tells whether the specified payload type represents a variable bitrate codec.
 * @param core #LinphoneCore object.
 * @param pt The payload type to check.
 * @return TRUE if the payload type represents a VBR codec, FALSE if disabled.
 * @ingroup media_parameters
 * @deprecated 31/03/2017 Use linphone_payload_type_is_vbr() instead.
 * @donotwrap
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED bool_t linphone_core_payload_type_is_vbr(const LinphoneCore *core,
                                                                             const OrtpPayloadType *pt);

/**
 * Set an explicit bitrate (IP bitrate, not codec bitrate) for a given codec, in kbit/s.
 * @param core the #LinphoneCore object
 * @param pt the payload type to modify.
 * @param bitrate the IP bitrate in kbit/s.
 * @ingroup media_parameters
 * @deprecated 31/03/2017 Use linphone_payload_type_set_normal_bitrate() instead.
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED void
linphone_core_set_payload_type_bitrate(LinphoneCore *core, OrtpPayloadType *pt, int bitrate);

/**
 * Get the bitrate explicitely set with linphone_core_set_payload_type_bitrate().
 * @param core the #LinphoneCore object
 * @param pt the payload type to modify.
 * @return bitrate the IP bitrate in kbit/s, or -1 if an error occured.
 * @ingroup media_parameters
 * @deprecated 31/03/2017 Use linphone_payload_type_get_bitrate().
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED int linphone_core_get_payload_type_bitrate(LinphoneCore *core,
                                                                               const OrtpPayloadType *pt);

/**
 * Enable or disable the use of the specified payload type.
 * @param core #LinphoneCore object.
 * @param pt The payload type to enable or disable. It can be retrieved using #linphone_core_find_payload_type()
 * @param enable TRUE to enable the payload type, FALSE to disable it.
 * @return 0 if successful, any other value otherwise.
 * @ingroup media_parameters
 * @deprecated 31/03/2017 Use linphone_payload_type_enable().
 * @donotwrap
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneStatus linphone_core_enable_payload_type(LinphoneCore *core,
                                                                                     OrtpPayloadType *pt,
                                                                                     bool_t enable);

/**
 * Wildcard value used by #linphone_core_find_payload_type() to ignore rate in search algorithm
 * @ingroup media_parameters
 */
#define LINPHONE_FIND_PAYLOAD_IGNORE_RATE -1

/**
 * Wildcard value used by #linphone_core_find_payload_type() to ignore channel in search algorithm
 * @ingroup media_parameters
 */
#define LINPHONE_FIND_PAYLOAD_IGNORE_CHANNELS -1

/**
 * Get payload type from mime type and clock rate.
 * @ingroup media_parameters
 * This function searches in audio and video codecs for the given payload type name and clockrate.
 * @param core #LinphoneCore object
 * @param type payload mime type (I.E SPEEX, PCMU, VP8)
 * @param rate can be #LINPHONE_FIND_PAYLOAD_IGNORE_RATE
 * @param channels  number of channels, can be #LINPHONE_FIND_PAYLOAD_IGNORE_CHANNELS
 * @return Returns NULL if not found.
 * @deprecated 31/03/2017 Use linphone_core_get_payload_type() instead.
 * @donotwrap
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED OrtpPayloadType *
linphone_core_find_payload_type(LinphoneCore *core, const char *type, int rate, int channels);

/**
 * Returns the payload type number assigned for this codec.
 * @ingroup media_parameters
 * @deprecated 13/02/2017 Use linphone_payload_type_get_number() instead
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED int linphone_core_get_payload_type_number(LinphoneCore *core,
                                                                              const OrtpPayloadType *pt);

/**
 * Force a number for a payload type. The #LinphoneCore does payload type number assignment automatically. THis function
 *is to be used mainly for tests, in order to override the automatic assignment mechanism.
 * @ingroup media_parameters
 * @deprecated 13/02/2017 Use linphone_payload_type_set_number() instead
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED void
linphone_core_set_payload_type_number(LinphoneCore *core, OrtpPayloadType *pt, int number);

/**
 * Get a description of the encoder used to supply a payload type.
 * @param core The core.
 * @param pt The payload type.
 * @return The description of the encoder. Can be NULL if the format is not supported by Mediastreamer2.
 * @deprecated 31/03/2017 Use linphone_payload_type_get_encoder_description() instead.
 * @donotwrap
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED const char *linphone_core_get_payload_type_description(LinphoneCore *core,
                                                                                           const OrtpPayloadType *pt);

/**
 * Return TRUE if codec can be used with bandwidth, FALSE else
 * @ingroup media_parameters
 * @deprecated 31/03/2017 Use linphone_payload_type_is_usable() instead.
 * @donotwrap
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED bool_t linphone_core_check_payload_type_usability(LinphoneCore *core,
                                                                                      const OrtpPayloadType *pt);

/**
 * @return the default proxy configuration, that is the one used to determine the current identity.
 * @deprecated 29/07/2015 Use linphone_core_get_default_proxy_config() instead.
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED int linphone_core_get_default_proxy(LinphoneCore *core,
                                                                        LinphoneProxyConfig **config);

/**
 * Create an authentication information with default values from Linphone core.
 * @param core #LinphoneCore object
 * @param username String containing the username part of the authentication credentials
 * @param userid String containing the username to use to calculate the authentication digest (optional)
 * @param passwd String containing the password of the authentication credentials (optional, either passwd or ha1 must
 * be set)
 * @param ha1 String containing a ha1 hash of the password (optional, either passwd or ha1 must be set)
 * @param realm String used to discriminate different SIP authentication domains (optional)
 * @param domain String containing the SIP domain for which this authentication information is valid, if it has to be
 * restricted for a single SIP domain.
 * @return #LinphoneAuthInfo with default values set
 * @ingroup authentication
 * @deprecated 13/02/2019 use linphone_factory_create_auth_info() instead.
 * @donotwrap
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneAuthInfo *linphone_core_create_auth_info(LinphoneCore *core,
                                                                                     const char *username,
                                                                                     const char *userid,
                                                                                     const char *passwd,
                                                                                     const char *ha1,
                                                                                     const char *realm,
                                                                                     const char *domain);

/**
 * Sets the UDP port to be used by SIP.
 * @param core #LinphoneCore object
 * @param port The UDP port to be used by SIP
 * @ingroup network_parameters
 * @deprecated 20/12/2016 use linphone_core_set_sip_transports() instead.
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED void linphone_core_set_sip_port(LinphoneCore *core, int port);

/**
 * Gets the UDP port used by SIP.
 * @param core #LinphoneCore object
 * @return The UDP port used by SIP
 * @ingroup network_parameters
 * @deprecated 20/12/2016 use linphone_core_get_sip_transports() instead.
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED int linphone_core_get_sip_port(LinphoneCore *core);

/**
 * Sets the ports to be used for each of transport (UDP or TCP)
 * A zero value port for a given transport means the transport
 * is not used. A value of LC_SIP_TRANSPORT_RANDOM (-1) means the port is to be choosen randomly by the system.
 * @param core #LinphoneCore object
 * @param transports A #LinphoneSipTransports structure giving the ports to use
 * @return 0
 * @ingroup network_parameters
 * @deprecated 18/04/2017 Use linphone_core_set_transports instead
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneStatus
linphone_core_set_sip_transports(LinphoneCore *core, const LinphoneSipTransports *transports);

/**
 * Retrieves the port configuration used for each transport (udp, tcp, tls).
 * A zero value port for a given transport means the transport
 * is not used. A value of LC_SIP_TRANSPORT_RANDOM (-1) means the port is to be chosen randomly by the system.
 * @param core #LinphoneCore object
 * @param[out] transports A #LinphoneSipTransports structure that will receive the configured ports
 * @return 0
 * @ingroup network_parameters
 * @deprecated 18/04/2017
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneStatus linphone_core_get_sip_transports(LinphoneCore *core,
                                                                                    LinphoneSipTransports *transports);

/**
 * Retrieves the real port number assigned for each sip transport (udp, tcp, tls).
 * A zero value means that the transport is not activated.
 * If LC_SIP_TRANSPORT_RANDOM was passed to linphone_core_set_sip_transports(), the random port choosed by the system is
 *returned.
 * @param core #LinphoneCore object
 * @param[out] tr A #LinphoneSipTransports structure that will receive the ports being used
 * @ingroup network_parameters
 * @deprecated 18/04/2017 Use linphone_core_get_transports_used instead
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED void linphone_core_get_sip_transports_used(LinphoneCore *core,
                                                                               LinphoneSipTransports *tr);

/**
 * Give access to the UDP sip socket. Can be useful to configure this socket as persistent I.E
 * kCFStreamNetworkServiceType set to kCFStreamNetworkServiceTypeVoIP)
 * @param core #LinphoneCore
 * @return socket file descriptor
 * @deprecated 11/05/2018
 * @donotwrap
 */
LINPHONE_DEPRECATED ortp_socket_t linphone_core_get_sip_socket(LinphoneCore *core);

/**
 * Set the policy to use to pass through firewalls.
 * @param core #LinphoneCore object.
 * @param pol The #LinphoneFirewallPolicy to use.
 * @ingroup network_parameters
 * @deprecated 30/03/2016 Use linphone_core_set_nat_policy() instead.
 * @donotwrap
 */
LINPHONE_DEPRECATED LINPHONE_PUBLIC void linphone_core_set_firewall_policy(LinphoneCore *core,
                                                                           LinphoneFirewallPolicy pol);

/**
 * Get the policy that is used to pass through firewalls.
 * @param core #LinphoneCore object.
 * @return The #LinphoneFirewallPolicy that is being used.
 * @ingroup network_parameters
 * @deprecated 27/03/2016 Use linphone_core_get_nat_policy() instead
 * @donotwrap
 */
LINPHONE_DEPRECATED LINPHONE_PUBLIC LinphoneFirewallPolicy linphone_core_get_firewall_policy(const LinphoneCore *core);

/**
 * Gets the list of the available sound devices.
 * @param core #LinphoneCore object
 * @return An unmodifiable array of strings contanining the names of the available sound devices that is NULL terminated
 * @ingroup media_parameters
 * @donotwrap
 * @deprecated 12/10/2017 use linphone_core_get_sound_devices_list instead
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED const char **linphone_core_get_sound_devices(LinphoneCore *core);

/**
 * Use this function when you want to set the default sound devices
 * @deprecated 08/07/2020
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED void linphone_core_set_default_sound_devices(LinphoneCore *core);

/**
 * Tells whether a specified sound device can capture sound.
 * @param core #LinphoneCore object @notnil
 * @param device the device name as returned by linphone_core_get_sound_devices() @notnil
 * @return A boolean value telling whether the specified sound device can capture sound
 * @deprecated 08/07/2020 use #LinphoneAudioDevice API instead()
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED bool_t linphone_core_sound_device_can_capture(LinphoneCore *core,
                                                                                  const char *device);

/**
 * Tells whether a specified sound device can play sound.
 * @param core #LinphoneCore object @notnil
 * @param device the device name as returned by linphone_core_get_sound_devices() @notnil
 * @return A boolean value telling whether the specified sound device can play sound
 * @deprecated 08/07/2020 use #LinphoneAudioDevice API instead()
 * @ingroup media_parameters
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED bool_t linphone_core_sound_device_can_playback(LinphoneCore *core,
                                                                                   const char *device);

/**
 * Get ring sound level in 0-100 scale.
 * @ingroup media_parameters
 * @deprecated 20/12/2016
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED int linphone_core_get_ring_level(LinphoneCore *core);

/**
 * Get playback sound level in 0-100 scale.
 * @ingroup media_parameters
 * @deprecated 20/12/2016
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED int linphone_core_get_play_level(LinphoneCore *core);

/**
 * Get sound capture level in 0-100 scale.
 * @ingroup media_parameters
 * @deprecated 20/12/2016
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED int linphone_core_get_rec_level(LinphoneCore *core);

/**
 * Get sound media level in 0-100 scale.
 * @ingroup media_parameters
 * @deprecated 20/12/2018
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED int linphone_core_get_media_level(LinphoneCore *core);

/**
 * Set sound ring level in 0-100 scale.
 * @ingroup media_parameters
 * @deprecated 20/12/2016
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED void linphone_core_set_ring_level(LinphoneCore *core, int level);

/**
 * Set sound playback level in 0-100 scale.
 * @deprecated 20/12/2016
 * @ingroup media_parameters
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED void linphone_core_set_play_level(LinphoneCore *core, int level);

/**
 * Set sound capture level in 0-100 scale.
 * @deprecated 20/12/2016
 * @ingroup media_parameters
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED void linphone_core_set_rec_level(LinphoneCore *core, int level);

/**
 * Set sound media level in 0-100 scale.
 * @deprecated 12/12/2018
 * @ingroup media_parameters
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED void linphone_core_set_media_level(LinphoneCore *core, int level);

LINPHONE_DEPRECATED char linphone_core_get_sound_source(LinphoneCore *core);

LINPHONE_DEPRECATED void linphone_core_set_sound_source(LinphoneCore *core, char source);

/*
 * Get the list of call logs (past calls) that matches the given #LinphoneAddress.
 * At the contrary of linphone_core_get_call_logs, it is your responsibility to unref the logs and free this list once
 *you are done using it. Requires ENABLE_DB_STORAGE to work.
 * @param core #LinphoneCore object @notnil
 * @param address #LinphoneAddress object @notnil
 * @return A list of #LinphoneCallLog. \bctbx_list{LinphoneCallLog} @tobefreed @maybenil
 * @deprecated 29/10/2018 Use #linphone_core_get_call_history_2() instead.
 * @ingroup call_logs
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED bctbx_list_t *
linphone_core_get_call_history_for_address(LinphoneCore *core, const LinphoneAddress *address);

/**
 * Enables video globally.
 *
 * This function does not have any effect during calls. It just indicates #LinphoneCore to
 * initiate future calls with video or not. The two boolean parameters indicate in which
 * direction video is enabled. Setting both to false disables video entirely.
 *
 * @param core The #LinphoneCore object
 * @param vcap_enabled indicates whether video capture is enabled
 * @param display_enabled indicates whether video display should be shown
 * @ingroup media_parameters
 * @deprecated 09/10/2013 Use #linphone_core_enable_video_capture() and #linphone_core_enable_video_display() instead.
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED void
linphone_core_enable_video(LinphoneCore *core, bool_t vcap_enabled, bool_t display_enabled);

/**
 * @brief Returns the zero terminated table of supported video resolutions.
 * @ingroup media_parameters
 * @deprecated 28/03/2017 Use #linphone_factory_get_supported_video_definitions() instead.
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED const MSVideoSizeDef *linphone_core_get_supported_video_sizes(LinphoneCore *core);

/**
 * @brief Sets the preferred video size.
 *
 * This applies only to the stream that is captured and sent to the remote party,
 * since we accept all standard video size on the receive path.
 * @ingroup media_parameters
 * @deprecated 28/03/2017 Use linphone_core_set_preferred_video_definition() instead.
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED void linphone_core_set_preferred_video_size(LinphoneCore *core, MSVideoSize vsize);

/**
 * @brief Sets the video size for the captured (preview) video.
 *
 * This method is for advanced usage where a video capture must be set independently of the size of the stream actually
 *sent through the call. This allows for example to have the preview window with HD resolution even if due to bandwidth
 *constraint the sent video size is small. Using this feature increases the CPU consumption, since a rescaling will be
 *done internally.
 * @ingroup media_parameters
 * @param core the #LinphoneCore. @notnil
 * @param vsize the video resolution choosed for capuring and previewing. It can be (0,0) to not request any specific
 *preview size and let the core optimize the processing.
 * @deprecated 28/03/2017 Use #linphone_core_set_preview_video_definition() instead.
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED void linphone_core_set_preview_video_size(LinphoneCore *core, MSVideoSize vsize);

/**
 * Sets the preview video size by its name. See linphone_core_set_preview_video_size() for more information about this
 *feature.
 *
 * Video resolution names are: qcif, svga, cif, vga, 4cif, svga ...
 * @ingroup media_parameters
 * @deprecated 28/03/2017 Use linphone_core_set_preview_video_definition_by_name() instead
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED void linphone_core_set_preview_video_size_by_name(LinphoneCore *core,
                                                                                      const char *name);

/**
 * @brief Returns video size for the captured video if it was previously set by #linphone_core_set_preview_video_size(),
 *otherwise returns a 0,0 size.
 * @see #linphone_core_set_preview_video_size()
 * @ingroup media_parameters
 * @param core the core
 * @return a #MSVideoSize
 * @deprecated 28/03/2017 Use #linphone_core_get_preview_video_definition() instead.
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED MSVideoSize linphone_core_get_preview_video_size(const LinphoneCore *core);

/**
 * @brief Returns the effective video size for the captured video as provided by the camera.
 *
 * When preview is disabled or not yet started, this function returns a zeroed video size.
 * @see #linphone_core_set_preview_video_size()
 * @ingroup media_parameters
 * @param core the core
 * @return a #MSVideoSize
 * @deprecated 28/03/2017 Use #linphone_core_get_current_preview_video_definition() instead.
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED MSVideoSize linphone_core_get_current_preview_video_size(const LinphoneCore *core);

/**
 * @brief Returns the current preferred video size for sending.
 * @ingroup media_parameters
 * @deprecated 28/03/2017 Use linphone_core_get_preferred_video_definition() instead.
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED MSVideoSize linphone_core_get_preferred_video_size(const LinphoneCore *core);

/**
 * Get the name of the current preferred video size for sending.
 * @param core #LinphoneCore object.
 * @return A string containing the name of the current preferred video size (to be freed with ms_free()).  @tobefreed
 * @deprecated 28/03/2017 Use linphone_core_get_preferred_video_defintion() and linphone_video_definition_get_name()
 * instead
 * @donotwrap
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED char *linphone_core_get_preferred_video_size_name(const LinphoneCore *core);

/**
 * Sets the preferred video size by its name.
 *
 * This is identical to linphone_core_set_preferred_video_size() except
 * that it takes the name of the video resolution as input.
 * Video resolution names are: qcif, svga, cif, vga, 4cif, svga ...
 * @ingroup media_parameters
 * @deprecated 28/03/2017 Use linphone_core_set_preferred_video_definition_by_name() instead
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED void linphone_core_set_preferred_video_size_by_name(LinphoneCore *core,
                                                                                        const char *name);

/**
 * Gets the list of the available video capture devices.
 * @param core #LinphoneCore object
 * @return An unmodifiable array of strings contanining the names of the available video capture devices that is NULL
 *terminated
 * @ingroup media_parameters
 * @deprecated 12/10/2017 use linphone_core_get_video_devices_list instead
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED const char **linphone_core_get_video_devices(const LinphoneCore *core);

/**
 * Create a LpConfig object from a user config file.
 * @param core #LinphoneCore object
 * @param filename The filename of the config file to read to fill the instantiated LpConfig
 * @ingroup misc
 * @deprecated 12/01/2017 Use linphone_core_create_config() instead.
 * @donotwrap
 */
LINPHONE_DEPRECATED LINPHONE_PUBLIC LinphoneConfig *linphone_core_create_lp_config(LinphoneCore *core,
                                                                                   const char *filename);

/**
 * Destroys a #LinphoneCore
 * @param core #LinphoneCore object
 * @ingroup initializing
 * @deprecated 12/01/2017 Use linphone_core_unref() instead.
 * @donotwrap
 **/
LINPHONE_DEPRECATED LINPHONE_PUBLIC void linphone_core_destroy(LinphoneCore *core);

/**
 * Get a pointer to the sqlite db holding zrtp/lime cache.
 * @param core #LinphoneCore object. @notnil
 * @return An sqlite3 pointer cast to a void one or NULL if cache is not available(not enabled at compile or access
 * failed) @maybenil
 * @ingroup initializing
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED void *linphone_core_get_zrtp_cache_db(LinphoneCore *core);

/**
 * Returns a null terminated table of strings containing the file format extension supported for call recording.
 * @param core the core
 * @return the supported formats, typically 'wav' and 'mkv'
 * @ingroup media_parameters
 * @deprecated 12/10/2017 use linphone_core_get_supported_file_formats_list instead
 * @donotwrap
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED const char **linphone_core_get_supported_file_formats(LinphoneCore *core);

/**
 * Get the linphone specs value telling what functionalities the linphone client supports.
 * @param core #LinphoneCore object @notnil
 * @return The linphone specs telling what functionalities the linphone client supports @maybenil
 * @ingroup initializing
 * @deprecated 07/02/2019 Use linphone_core_get_linphone_specs_list instead.
 * @donotwrap
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED const char *linphone_core_get_linphone_specs(const LinphoneCore *core);

/**
 * Set the linphone specs value telling what functionalities the linphone client supports.
 * @param core #LinphoneCore object @notnil
 * @param specs The linphone specs to set @maybenil
 * @ingroup initializing
 * @deprecated 07/02/2019 Use linphone_core_set_linphone_specs_list or linphone_core_add_linphone_spec instead.
 * @donotwrap
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED void linphone_core_set_linphone_specs(LinphoneCore *core, const char *specs);

/**
 * Set the chat database path.
 * @param core the #LinphoneCore. @notnil
 * @param path the database path
 * @deprecated 10/01/2018: Use only for migration purposes
 * @donotwrap
 */
LINPHONE_DEPRECATED LINPHONE_PUBLIC void linphone_core_set_chat_database_path(LinphoneCore *core, const char *path);

/**
 * Get path to the database file used for storing chat messages.
 * @param core the #LinphoneCore. @notnil
 * @return file path or NULL if not exist
 * @deprecated 10/01/2018
 * @donotwrap
 **/
LINPHONE_DEPRECATED LINPHONE_PUBLIC const char *linphone_core_get_chat_database_path(const LinphoneCore *core);

/**
 * Returns whether the database is enabled.
 * @param core the #LinphoneCore @notnil
 * @return a boolean indicating the enablement of the database.
 * @ingroup initializing
 */
LINPHONE_PUBLIC bool_t linphone_core_database_enabled(const LinphoneCore *core);

/**
 * Enables or disables database usage. This function can only be called before starting the core up
 * @param core the #LinphoneCore @notnil
 * @param value a boolean to indicate whether the database is to be enabled.
 * @ingroup initializing
 */
LINPHONE_PUBLIC void linphone_core_enable_database(LinphoneCore *core, bool_t value);

/**
 * Create a client-side group chat room. When calling this function the chat room is only created
 * at the client-side and is empty. You need to call linphone_chat_room_add_participants() to
 * create at the server side and add participants to it.
 * Also, the created chat room will not be a one-to-one chat room even if linphone_chat_room_add_participants() is
 * called with only one participant.
 *
 * @param core A #LinphoneCore object @notnil
 * @param subject The subject of the group chat room @notnil
 * @param fallback Boolean value telling whether we should plan on being able to fallback to a basic chat room if the
 * client-side group chat room creation fails
 * @return The newly created client-side group chat room. @maybenil
 * @deprecated 02/07/2020, use linphone_core_create_chat_room_7() instead
 * @ingroup chatroom
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneChatRoom *
linphone_core_create_client_group_chat_room(LinphoneCore *core, const char *subject, bool_t fallback);

/**
 * Create a client-side group chat room. When calling this function the chat room is only created
 * at the client-side and is empty. You need to call linphone_chat_room_add_participants() to
 * create at the server side and add participants to it.
 * Also, the created chat room will not be a one-to-one chat room even if linphone_chat_room_add_participants() is
 * called with only one participant.
 *
 * @param core A #LinphoneCore object @notnil
 * @param subject The subject of the group chat room @notnil
 * @param fallback Boolean value telling whether we should plan on being able to fallback to a basic chat room if the
 * client-side group chat room creation fails
 * @param encrypted Boolean value telling whether we should apply encryption or not on chat messages sent and received
 * on this room.
 * @return The newly created client-side group chat room. @maybenil
 * @deprecated 02/07/2020, use linphone_core_create_chat_room_7() instead
 * @ingroup chatroom
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneChatRoom *linphone_core_create_client_group_chat_room_2(LinphoneCore *core,
                                                                                                    const char *subject,
                                                                                                    bool_t fallback,
                                                                                                    bool_t encrypted);

/**
 * Create a chat room.
 *
 * @param core A #LinphoneCore object @notnil
 * @param params The chat room creation parameters #LinphoneChatRoomParams @notnil
 * @param localAddr #LinphoneAddress representing the local proxy configuration to use for the chat room creation
 * @notnil
 * @param subject The subject of the group chat room @notnil
 * @param participants The initial list of participants of the chat room \bctbx_list{LinphoneAddress} @notnil
 * @return The newly created chat room. @maybenil
 * @deprecated 02/07/2020, use linphone_core_create_chat_room_7() instead
 * @ingroup chatroom
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneChatRoom *
linphone_core_create_chat_room(LinphoneCore *core,
                               const LinphoneChatRoomParams *params,
                               const LinphoneAddress *localAddr,
                               const char *subject,
                               const bctbx_list_t *participants);

/**
 * Create a chat room.
 *
 * @param core A #LinphoneCore object @notnil
 * @param params The chat room creation parameters #LinphoneChatRoomParams @notnil
 * @param subject The subject of the group chat room @notnil
 * @param participants The initial list of participants of the chat room. \bctbx_list{LinphoneAddress} @notnil
 * @return The newly created chat room. @maybenil
 * @deprecated 02/07/2020, use linphone_core_create_chat_room_7() instead
 * @ingroup chatroom
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneChatRoom *linphone_core_create_chat_room_2(
    LinphoneCore *core, const LinphoneChatRoomParams *params, const char *subject, const bctbx_list_t *participants);

/**
 *
 * @param core A #LinphoneCore object @notnil
 * @param subject The subject of the group chat room @notnil
 * @param participants The initial list of participants of the chat room. \bctbx_list{LinphoneAddress} @notnil
 * @return The newly created chat room. @maybenil
 * @deprecated 02/07/2020, use linphone_core_create_chat_room_7() instead
 * @ingroup chatroom
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneChatRoom *
linphone_core_create_chat_room_3(LinphoneCore *core, const char *subject, const bctbx_list_t *participants);

/**
 *
 * @param core A #LinphoneCore object @notnil
 * @param params The chat room creation parameters #LinphoneChatRoomParams @notnil
 * @param localAddr #LinphoneAddress representing the local proxy configuration to use for the chat room creation
 * @notnil
 * @param participant #LinphoneAddress representing the initial participant to add to the chat room @notnil
 * @return The newly created chat room. @maybenil
 * @deprecated 02/07/2020, use linphone_core_create_chat_room_7() instead
 * @ingroup chatroom
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneChatRoom *
linphone_core_create_chat_room_4(LinphoneCore *core,
                                 const LinphoneChatRoomParams *params,
                                 const LinphoneAddress *localAddr,
                                 const LinphoneAddress *participant);

/**
 *
 * @param core A #LinphoneCore object @notnil
 * @param participant #LinphoneAddress representing the initial participant to add to the chat room @notnil
 * @return The newly created chat room. @maybenil
 * @deprecated 02/07/2020, use linphone_core_create_chat_room_7() instead
 * @ingroup chatroom
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneChatRoom *
linphone_core_create_chat_room_5(LinphoneCore *core, const LinphoneAddress *participant);

/**
 * Create a chat room.
 *
 * @param core A #LinphoneCore object @notnil
 * @param params The chat room creation parameters #LinphoneChatRoomParams @notnil
 * @param localAddr #LinphoneAddress of a local #LinphoneAccount identity or NULL @maybenil
 * @param participants The initial list of participants of the chat room. \bctbx_list{LinphoneAddress} @notnil
 * @return The newly created chat room (can be an existing one if backend is Basic) or NULL. @maybenil
 * @deprecated 22/10/2024, use linphone_core_create_chat_room_7() instead
 * @ingroup chatroom
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneChatRoom *
linphone_core_create_chat_room_6(LinphoneCore *core,
                                 const LinphoneChatRoomParams *params,
                                 const LinphoneAddress *localAddr,
                                 const bctbx_list_t *participants);

/**
 * Get a chat room whose peer is the supplied address. If it does not exist yet, it will be created as a basic chat
 *room. No reference is transferred to the application. The #LinphoneCore keeps a reference on the chat room.
 * @warning This method is prone to errors, use linphone_core_search_chat_room() instead
 * @param core the #LinphoneCore. @notnil
 * @param addr a linphone address. @notnil
 * @return #LinphoneChatRoom where messaging can take place. @maybenil
 * @deprecated 02/07/2020, use linphone_core_search_chat_room() instead
 * @ingroup chatroom
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneChatRoom *linphone_core_get_chat_room(LinphoneCore *core,
                                                                                  const LinphoneAddress *addr);

/**
 * Get a chat room. If it does not exist yet, it will be created as a basic chat room.
 * No reference is transferred to the application. The #LinphoneCore keeps a reference on the chat room.
 * @warning This method is prone to errors, use linphone_core_search_chat_room() instead
 * @param core the #LinphoneCore. @notnil
 * @param peer_addr a linphone address. @notnil
 * @param local_addr a linphone address. @notnil
 * @return #LinphoneChatRoom where messaging can take place. @maybenil
 * @deprecated 02/07/2020, use linphone_core_search_chat_room() instead
 * @ingroup chatroom
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneChatRoom *
linphone_core_get_chat_room_2(LinphoneCore *core, const LinphoneAddress *peer_addr, const LinphoneAddress *local_addr);

/**
 * Get a chat room for messaging from a sip uri like sip:joe@sip.linphone.org. If it does not exist yet, it will be
 *created as a basic chat room. No reference is transferred to the application. The #LinphoneCore keeps a reference on
 *the chat room.
 * @warning This method is prone to errors, use linphone_core_search_chat_room() instead
 * @param core A #LinphoneCore object @notnil
 * @param to The destination address for messages. @notnil
 * @return #LinphoneChatRoom where messaging can take place. @maybenil
 * @deprecated 02/07/2020, use linphone_core_search_chat_room() instead
 * @ingroup chatroom
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneChatRoom *linphone_core_get_chat_room_from_uri(LinphoneCore *core,
                                                                                           const char *to);

/**
 * Find a chat room.
 * No reference is transferred to the application. The #LinphoneCore keeps a reference on the chat room.
 * @param core the #LinphoneCore. @notnil
 * @param peer_addr a linphone address. @notnil
 * @param local_addr a linphone address. @notnil
 * @return #LinphoneChatRoom where messaging can take place. @maybenil
 * @deprecated 02/07/2020, use linphone_core_search_chat_room() instead
 * @ingroup chatroom
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneChatRoom *linphone_core_find_chat_room(const LinphoneCore *core,
                                                                                   const LinphoneAddress *peer_addr,
                                                                                   const LinphoneAddress *local_addr);

/**
 * Find a one to one chat room.
 * No reference is transferred to the application. The #LinphoneCore keeps a reference on the chat room.
 * @param core the #LinphoneCore. @notnil
 * @param local_addr a linphone address. @notnil
 * @param participant_addr a linphone address. @notnil
 * @return #LinphoneChatRoom where messaging can take place. @maybenil
 * @deprecated 12/12/2018, use linphone_core_find_one_to_one_chat_room_2 instead
 * @donotwrap
 * @ingroup chatroom
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneChatRoom *linphone_core_find_one_to_one_chat_room(
    const LinphoneCore *core, const LinphoneAddress *local_addr, const LinphoneAddress *participant_addr);

/**
 * Find a one to one chat room.
 * No reference is transferred to the application. The #LinphoneCore keeps a reference on the chat room.
 * @param core the #LinphoneCore. @notnil
 * @param local_addr a linphone address. @notnil
 * @param participant_addr a linphone address. @notnil
 * @param encrypted whether to look for an encrypted chat room or not
 * @return #LinphoneChatRoom where messaging can take place. @maybenil
 * @deprecated 02/07/2020, use linphone_core_search_chat_room() instead
 * @ingroup chatroom
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneChatRoom *
linphone_core_find_one_to_one_chat_room_2(const LinphoneCore *core,
                                          const LinphoneAddress *local_addr,
                                          const LinphoneAddress *participant_addr,
                                          bool_t encrypted);

/**
 * Returns how many attachments are yet to be downloaded
 * @param core the #LinphoneCore. @notnil
 * @return how many attachments are yet to be downloaded.
 * @ingroup chatroom
 **/
LINPHONE_PUBLIC unsigned int linphone_core_get_remaining_download_file_count(LinphoneCore *core);

/**
 * Returns how many attachments are yet to be uploaded
 * @param core the #LinphoneCore. @notnil
 * @return how many attachments are yet to be uploaded.
 * @ingroup chatroom
 **/
LINPHONE_PUBLIC unsigned int linphone_core_get_remaining_upload_file_count(LinphoneCore *core);

/**
 * @deprecated 03/02/2017 Use linphone_core_interpret_url() instead
 * @donotwrap
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED void
linphone_core_interpret_friend_uri(LinphoneCore *core, const char *uri, char **result);

/**
 * Add a friend to the current buddy list.
 * A SIP SUBSCRIBE message is sent if subscription has been enabled by linphone_friend_enable_subscribes().
 * @param core #LinphoneCore object
 * @param fr #LinphoneFriend to add
 * @deprecated 03/02/2017 use linphone_friend_list_add_friend() instead.
 * @donotwrap
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED void linphone_core_add_friend(LinphoneCore *core, LinphoneFriend *fr);

/**
 * Removes a friend from the buddy list
 * @param core #LinphoneCore object
 * @param fr #LinphoneFriend to remove
 * @deprecated 03/02/2017 use linphone_friend_list_remove_friend() instead.
 * @donotwrap
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED void linphone_core_remove_friend(LinphoneCore *core, LinphoneFriend *fr);

/**
 * Set my presence status
 * @param core #LinphoneCore object
 * @param minutes_away how long in away
 * @param alternative_contact sip uri used to redirect call in state #LinphoneStatusMoved
 * @param os #LinphoneOnlineStatus
 * @deprecated 03/02/2017 Use linphone_core_set_presence_model() instead
 * @donotwrap
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED void linphone_core_set_presence_info(LinphoneCore *core,
                                                                         int minutes_away,
                                                                         const char *alternative_contact,
                                                                         LinphoneOnlineStatus os);

/**
 * Get Buddy list of #LinphoneFriend
 * @param core #LinphoneCore object
 * @return A list of #LinphoneFriend. \bctbx_list{LinphoneFriend}
 * @deprecated 03/02/2017 use linphone_core_get_friends_lists() or linphone_friend_list_get_friends() instead.
 * @donotwrap
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED const bctbx_list_t *linphone_core_get_friend_list(const LinphoneCore *core);

/**
 * Search a #LinphoneFriend by its address.
 * @param core #LinphoneCore object.
 * @param addr The address to use to search the friend.
 * @return The #LinphoneFriend object corresponding to the given address.
 * @deprecated 03/02/2017 use linphone_core_find_friend() instead.
 * @donotwrap
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneFriend *linphone_core_get_friend_by_address(const LinphoneCore *core,
                                                                                        const char *addr);

/**
 * Get my presence status
 * @param core #LinphoneCore object
 * @return #LinphoneOnlineStatus
 * @deprecated 03/02/2017 Use linphone_core_get_presence_model() instead
 * @donotwrap
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED LinphoneOnlineStatus linphone_core_get_presence_info(const LinphoneCore *core);

/**
 * Sets the database filename where call logs will be stored.
 * If the file does not exist, it will be created.
 * @ingroup initializing
 * @param core the #LinphoneCore. @notnil
 * @param path filesystem path @maybenil
 * @deprecated 07/12/2021: Use only for migration purposes
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED void linphone_core_set_call_logs_database_path(LinphoneCore *core,
                                                                                   const char *path);

/**
 * Gets the database filename where call logs will be stored.
 * @ingroup initializing
 * @param core the #LinphoneCore. @notnil
 * @return filesystem path. @maybenil
 * @deprecated 07/12/2021: Use only for migration purposes
 **/
LINPHONE_PUBLIC LINPHONE_DEPRECATED const char *linphone_core_get_call_logs_database_path(LinphoneCore *core);

/**
 * Gets a #LinphoneEktInfo from an XML body.
 * @param core the #LinphoneCore
 * @param xml_body the string containing the XML body @notnil
 * @return The #LinphoneEktInfo @maybenil
 * @ingroup ekt_api
 */
LINPHONE_PUBLIC const LinphoneEktInfo *linphone_core_create_ekt_info_from_xml(const LinphoneCore *core,
                                                                              const char *xml_body);

/**
 * Gets an XML body
 * @param core the #LinphoneCore
 * @param ekt_info the #LinphoneEktInfo @notnil
 * @return The XML body @maybenil @tobefreed
 * @deprecated 06/02/2025 use linphone_core_create_xml_from_ekt_info_2().
 * @ingroup ekt_api
 */
LINPHONE_PUBLIC LINPHONE_DEPRECATED char *linphone_core_create_xml_from_ekt_info(const LinphoneCore *core,
                                                                                 const LinphoneEktInfo *ekt_info);

/**
 * Gets an XML body using a specific account
 * @param core the #LinphoneCore
 * @param ekt_info the #LinphoneEktInfo @notnil
 * @param account the #LinphoneAccount associated with the conference @maybenil
 * @return The XML body @maybenil @tobefreed
 * @ingroup ekt_api
 */
LINPHONE_PUBLIC char *linphone_core_create_xml_from_ekt_info_2(const LinphoneCore *core,
                                                               const LinphoneEktInfo *ekt_info,
                                                               const LinphoneAccount *account);

/**
 * Gets if the EKT plugin is currently loaded in the Linphone core instance.
 * @param core the #LinphoneCore
 * @return true if the EKT plugin is loaded
 * @ingroup ekt_api
 */
LINPHONE_PUBLIC bool_t linphone_core_is_ekt_plugin_loaded(const LinphoneCore *core);

/**
 * sets the state of the EKT plugin in the Linphone core instance.
 * @param core the #LinphoneCore
 * @param ekt_plugin_loaded whether the EKT plugin is loaded or not
 * @ingroup ekt_api
 */
LINPHONE_PUBLIC void linphone_core_set_ekt_plugin_loaded(LinphoneCore *core, bool_t ekt_plugin_loaded);

#ifdef __cplusplus
}
#endif

#endif

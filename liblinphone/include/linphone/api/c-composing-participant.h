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

#ifndef LINPHONE_COMPOSING_PARTICIPANT_H
#define LINPHONE_COMPOSING_PARTICIPANT_H

#include "linphone/api/c-types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup chatroom
 * @{
 */

/**
 * Clones a composing participant.
 * @param composing_participant The #LinphoneComposingParticipant object to be cloned. @notnil
 * @return The newly created #LinphoneComposingParticipant object. @notnil
 */
LINPHONE_PUBLIC LinphoneComposingParticipant *
linphone_composing_participant_clone(const LinphoneComposingParticipant *composing_participant);

/**
 * Takes a reference on a #LinphoneComposingParticipant.
 * @param composing_participant The #LinphoneComposingParticipant object. @notnil
 * @return the same #LinphoneComposingParticipant object. @notnil
 */
LINPHONE_PUBLIC LinphoneComposingParticipant *
linphone_composing_participant_ref(LinphoneComposingParticipant *composing_participant);

/**
 * Releases a #LinphoneComposingParticipant.
 * @param composing_participant The #LinphoneComposingParticipant object. @notnil
 */
LINPHONE_PUBLIC void linphone_composing_participant_unref(LinphoneComposingParticipant *composing_participant);

// =============================================================================

/**
 * Gets the participant's address.
 * @param composing_participant The #LinphoneComposingParticipant object. @notnil
 * @return the #LinphoneAddress of the participant. @notnil
 */
LINPHONE_PUBLIC const LinphoneAddress *
linphone_composing_participant_get_address(const LinphoneComposingParticipant *composing_participant);

/**
 * Gets the content-type of what the participant is being composing.
 * @param composing_participant The #LinphoneComposingParticipant object. @notnil
 * @return the content-type set if any, NULL otherwise. @maybenil
 */
LINPHONE_PUBLIC const char *
linphone_composing_participant_get_content_type(const LinphoneComposingParticipant *composing_participant);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* LINPHONE_COMPOSING_PARTICIPANT_H */

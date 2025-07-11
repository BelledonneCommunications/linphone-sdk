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

#include <ctype.h>

#include "c-wrapper/c-wrapper.h"
#include "chat/chat-room/composing-participant.h"
#include "linphone/api/c-composing-participant.h"
#include "linphone/wrapper_utils.h"

using namespace LinphonePrivate;

// =============================================================================

LinphoneComposingParticipant *
linphone_composing_participant_clone(const LinphoneComposingParticipant *composing_participant) {
	return ComposingParticipant::toCpp(composing_participant)->clone()->toC();
}

LinphoneComposingParticipant *linphone_composing_participant_ref(LinphoneComposingParticipant *composing_participant) {
	ComposingParticipant::toCpp(composing_participant)->ref();
	return composing_participant;
}

void linphone_composing_participant_unref(LinphoneComposingParticipant *composing_participant) {
	ComposingParticipant::toCpp(composing_participant)->unref();
}

// =============================================================================

const LinphoneAddress *
linphone_composing_participant_get_address(const LinphoneComposingParticipant *composing_participant) {
	return ComposingParticipant::toCpp(composing_participant)->getAddress()->toC();
}

const char *linphone_composing_participant_get_content_type(const LinphoneComposingParticipant *composing_participant) {
	return L_STRING_TO_C(ComposingParticipant::toCpp(composing_participant)->getContentType());
}

// =============================================================================
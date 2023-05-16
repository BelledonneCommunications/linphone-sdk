/*
 * Copyright (c) 2010-2023 Belledonne Communications SARL.
 *
 * This file is part of the Liblinphone EKT server plugin
 * (see https://gitlab.linphone.org/BC/private/gc).
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

#include <linphone/core.h>

using namespace std;

LINPHONE_BEGIN_NAMESPACE

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER)
#define PLUGIN_EXPORT __declspec(dllexport)
#else
#define PLUGIN_EXPORT
#endif
PLUGIN_EXPORT void libektserver_init(LinphoneCore *core) {
	ms_message("EKT server plugin for core %s has been succesfully loaded", linphone_core_get_identity(core));
}

#ifdef __cplusplus
}
#endif

LINPHONE_END_NAMESPACE

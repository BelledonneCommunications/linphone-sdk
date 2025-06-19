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

#include "ekt-server-main.h"

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
PLUGIN_EXPORT void liblinphone_ektserver_init(LinphoneCore *core) {
	ms_message("EKT server plugin for core %s has been succesfully loaded", linphone_core_get_identity(core));
	/* Build a C++ Core object on top of the LinphoneCore C object */
	auto cppCore = linphone::Object::cPtrToSharedPtr<linphone::Core>(core);
	/* The C++ linphone::Core object is passed and hold by the EktServerMain.
	 * Indeed, in the scope of liblinphone-tester execution, there is no linphone::Core that is hold
	 * anywhere else.
	 * Unfortunately the linphone::CoreListener cannot work in this situation where the LinphoneCore
	 * is destroyed while no linphone::Core object holds it.
	 * To workaround this, the linphone::Core object is hold as a shared_ptr by the EktServerMain.
	 * This shared_ptr<> is manually reset when the Core is stopped.
	 * As a result of this: a liblinphone plugin written on top of the linphone++ wrapper
	 * requires that core.stop() is called at some point in order to clear the circular shared_ptr
	 * dependency.
	 * Anyone having a better solution that does not mandate the core.stop() is welcome to change this.
	 */
	cppCore->addListener(make_shared<EktServerPlugin::EktServerMain>(cppCore));
	cppCore->setEktPluginLoaded(true);
}

#ifdef __cplusplus
}
#endif

LINPHONE_END_NAMESPACE

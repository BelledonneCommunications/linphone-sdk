/*
 * core.cpp
 * Copyright (C) 2010-2017 Belledonne Communications SARL
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "core-p.h"
#include "logger/logger.h"
#include "paths/paths.h"

// TODO: Remove me later.
#include "c-wrapper/c-wrapper.h"

// =============================================================================

#define LINPHONE_DB "linphone.db"

using namespace std;

LINPHONE_BEGIN_NAMESPACE

Core::Core () : Object(*new CorePrivate) {}

shared_ptr<Core> Core::create (LinphoneCore *cCore) {
	// Do not use `make_shared` => Private constructor.
	shared_ptr<Core> core = shared_ptr<Core>(new Core);

	CorePrivate * const d = core->getPrivate();

	d->cCore = cCore;
	d->mainDb.reset(new MainDb(core->getSharedFromThis()));

	AbstractDb::Backend backend;
	string uri = L_C_TO_STRING(lp_config_get_string(linphone_core_get_config(d->cCore), "storage", "backend", nullptr));
	if (!uri.empty())
		backend = strcmp(lp_config_get_string(linphone_core_get_config(d->cCore), "storage", "uri", nullptr), "mysql") == 0
			? MainDb::Mysql
			: MainDb::Sqlite3;
	else {
		backend = AbstractDb::Sqlite3;
		uri = core->getDataPath() + LINPHONE_DB;
	}

	lInfo() << "Opening " LINPHONE_DB " at: " << uri;
	if (!d->mainDb->connect(backend, uri))
		lFatal() << "Unable to open linphone database.";

	for (auto &chatRoom : d->mainDb->getChatRooms())
		d->insertChatRoom(chatRoom);

	return core;
}

LinphoneCore *Core::getCCore () const {
	L_D();
	return d->cCore;
}

string Core::getDataPath() const {
	L_D();
	return Paths::getPath(Paths::Data, static_cast<PlatformHelpers *>(d->cCore->platform_helper));
}

string Core::getConfigPath() const {
	L_D();
	return Paths::getPath(Paths::Config, static_cast<PlatformHelpers *>(d->cCore->platform_helper));
}

LINPHONE_END_NAMESPACE

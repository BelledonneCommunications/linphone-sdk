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

#include "main-db-event-key.h"
#include "core/core-p.h"
#include "event-log/event-log-p.h"
#include "main-db-key-p.h"
#include "main-db-p.h"

// =============================================================================

using namespace std;

LINPHONE_BEGIN_NAMESPACE

// -----------------------------------------------------------------------------

MainDbEventKey::MainDbEventKey() : MainDbKey() {};

MainDbEventKey::MainDbEventKey(const shared_ptr<Core> &core, long long storageId) : MainDbKey(core, storageId) {
}

MainDbEventKey::~MainDbEventKey() {
	resetStorageId();
}

void MainDbEventKey::resetStorageId() {
	L_D();

	if (isValid()) {
		auto core = d->core.lock();
		if (core) {
			if (auto db = core->getDatabase()) {
				auto &storageIdToEvent = db.value().get().getPrivate()->storageIdToEvent;
				auto it = storageIdToEvent.find(d->storageId);
				if (it != storageIdToEvent.end()) {
					// Another event log may be cached with this storage ID, for instance one reloaded from the database
					// after a core restart while the one carrying this key was still referenced: leave it. The entry of
					// the event log carrying this key has already expired if it is being destroyed.
					auto cachedEventLog = it->second.lock();
					if (!cachedEventLog || (&cachedEventLog->getPrivate()->dbKey == this)) storageIdToEvent.erase(it);
				}
			}
		}
	}

	d->storageId = -1;
}

LINPHONE_END_NAMESPACE

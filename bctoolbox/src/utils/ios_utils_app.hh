/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of Liblinphone.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "bctoolbox/ios_utils.hh"

namespace bctoolbox {

/*
 * Implementation of IOSUtilsInterface that has full app functionnality, ie
 * encapsulate system library calls that are only available to apps.
 * In order to avoid a build-time linkage that is not allowed in app extension,
 * the implementation is embedded inside the bctoolbox-ios framework,
 * that is loaded dynamically from bctoolbox.
 * Since bctoolbox-ios cannot depend on bctoolbox archive (if it does so, symbols are duplicated),
 * a pointer to the main log function is passed.
 */
class IOSUtilsApp : public IOSUtilsInterface {
public:
	unsigned long beginBackgroundTask(const char *name, std::function<void()> cb) override;
	void endBackgroundTask(unsigned long id) override;
	bool isApplicationStateActive() override;
	void setLoggingFunction(BctbxLogFunc logFunction) override;
	void bctbxLog(BctbxLogLevel level, const char *fmt, ...);

private:
	BctbxLogFunc mLogFunc;
};

} // namespace bctoolbox

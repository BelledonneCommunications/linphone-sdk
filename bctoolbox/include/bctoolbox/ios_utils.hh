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

#include "bctoolbox/logging.h"
#include <functional>

namespace bctoolbox {

/*
 * The IOSUtilsInterface is an abstract class used to provide functionnality that may not be available at runtime.
 * For example, getting application state or using background tasks is not allowed in iOS app extension.
 */
class IOSUtilsInterface {
public:
	virtual void setLoggingFunction(BctbxLogFunc logFunction) = 0;
	virtual unsigned long beginBackgroundTask(const char *name, std::function<void()> cb) = 0;
	virtual void endBackgroundTask(unsigned long id) = 0;
	virtual bool isApplicationStateActive() = 0;

	virtual ~IOSUtilsInterface() = default;
};

/*
 * Main class to manipulate some iOS functionnality that may not be available
 * depending on the execution context.
 * In case it is not available (not in an app), a stub implementation is used.
 */
class IOSUtils {
public:
	unsigned long beginBackgroundTask(const char *name, std::function<void()> cb);
	void endBackgroundTask(unsigned long id);
	bool isApplicationStateActive();
	bool isApp();
	int getOSMajorVersion() const;
	static IOSUtils &getUtils();

	IOSUtils(const IOSUtils &) = delete;
	IOSUtils &operator=(const IOSUtils &) = delete;
	~IOSUtils();

private:
	void *mHandle;
	IOSUtilsInterface *mUtils;
	bool mIsApp;
	static std::unique_ptr<IOSUtils> sInstance;
	IOSUtils();

	void openDynamicLib();
	void *loadSymbol(const char *symbol);
};

} // namespace bctoolbox

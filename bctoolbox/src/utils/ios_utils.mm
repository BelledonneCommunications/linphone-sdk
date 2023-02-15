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

#ifdef __APPLE__

#include <TargetConditionals.h>

#import <Foundation/Foundation.h>
#include <dlfcn.h>

#include "bctoolbox/defs.h"
#include "bctoolbox/ios_utils.hh"
#include "bctoolbox/exception.hh"
#include "bctoolbox/logging.h"
#include "ios_utils_stub.hh"

#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#endif


namespace bctoolbox {

std::unique_ptr<IOSUtils> IOSUtils::sInstance = nullptr;

IOSUtils& IOSUtils::getUtils() {
    if (!sInstance) {
        sInstance = std::unique_ptr<IOSUtils>(new IOSUtils);
    }
    
    return *sInstance;
}

IOSUtils::IOSUtils() {
	mIsApp = [[[NSBundle mainBundle] bundlePath] hasSuffix:@".app"];
    if (mIsApp) {
        openDynamicLib();
        using create_t = IOSUtilsInterface *(*)();
        auto createUtils = reinterpret_cast<create_t>(loadSymbol("bctbx_create_ios_utils_app"));
        mUtils = createUtils();
    } else {
        mUtils = new IOSUtilsStub();
    }
}

IOSUtils::~IOSUtils() {
    if (mIsApp) {
        using destroy_t = void (*)(IOSUtilsInterface *);
        auto destroyUtils = reinterpret_cast<destroy_t>(loadSymbol("bctbx_destroy_ios_utils_app"));
        destroyUtils(mUtils);
        dlclose(mHandle);
    } else {
        delete mUtils;        
    }
}

bool IOSUtils::isApp() {
    return mIsApp;
}

void IOSUtils::openDynamicLib() {
    NSString *frameworkPath = [[[NSBundle mainBundle] resourcePath] stringByAppendingString: @"/Frameworks/bctoolbox-ios.framework/bctoolbox-ios"];
    const char *frameworkChar = [frameworkPath cStringUsingEncoding:[NSString defaultCStringEncoding]];

    mHandle = dlopen(frameworkChar, RTLD_LAZY);
    if (!mHandle) {
        throw BCTBX_EXCEPTION << "bctoolbox error dlopen : " << dlerror();
    }

    // reset errors
    dlerror();
}

void *IOSUtils::loadSymbol(const char *symbol) {
    void *loadedSymbol = dlsym(mHandle, symbol);
    const char *dlsym_error = dlerror();
    
    if (dlsym_error) {
        std::string error = dlsym_error;
        dlclose(mHandle);
        throw BCTBX_EXCEPTION << "bctoolbox error dlsym : " << error;
    }
    
    return loadedSymbol;
}

unsigned long IOSUtils::beginBackgroundTask(const char *name, std::function<void()> cb) {
    return mUtils->beginBackgroundTask(name, cb);
}

void IOSUtils::endBackgroundTask(unsigned long id) {
    return mUtils->endBackgroundTask(id);
}

bool IOSUtils::isApplicationStateActive() {
    return mUtils->isApplicationStateActive();
}

int IOSUtils::getOSMajorVersion() const{
#if TARGET_OS_IPHONE
    NSArray *versionCompatibility = [[UIDevice currentDevice].systemVersion componentsSeparatedByString:@"."];
	return [[versionCompatibility objectAtIndex:0] intValue];
#else
	bctbx_error("IOSUtils::getOSMajorVersion() not running on iOS");
	return 0;
#endif
}


unsigned long IOSUtilsStub::beginBackgroundTask(BCTBX_UNUSED(const char *name), BCTBX_UNUSED(std::function<void()> cb)) {
    return 0;
}

void IOSUtilsStub::endBackgroundTask(BCTBX_UNUSED(unsigned long id)) {}

bool IOSUtilsStub::isApplicationStateActive() {
    return false;
}

} //namespace bctoolbox

#endif

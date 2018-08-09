/*
	bctoolbox
	Copyright (C) 2016  Belledonne Communications SARL

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_EXECINFO
#include <execinfo.h>
#endif

#include <unistd.h>
#include <dlfcn.h>
#include <cxxabi.h>
#include <exception>
#include <iomanip>
#include <libgen.h>

#include "bctoolbox/exception.hh"
#include "bctoolbox/logging.h"

using namespace std;

#ifdef HAVE_EXECINFO
static void uncaught_handler() {
	std::exception_ptr p = current_exception();
	try {
		rethrow_exception(p);
	} catch (BctbxException &e) {
		BCTBX_SLOGE << e;
	} catch (std::exception &ee) {
		BCTBX_SLOGE << "Unexpected exception [" << ee.what() << " ] use BctbxException for better debug";
	}
	abort();
}
#endif

BctbxException::BctbxException(const char *message) : mOffset(1), mSize(0) {
#ifdef HAVE_EXECINFO
	mSize = backtrace(mArray, sizeof(mArray) / sizeof(void *));
#else
	mSize = 0;
#endif
	if (message)
		mOs << message;
#ifdef HAVE_EXECINFO
#if __clang
	if (get_terminate() != uncaught_handler)
#endif
		set_terminate(uncaught_handler); // invoke in case of uncautch exception for this thread
#endif
}

BctbxException::BctbxException(const BctbxException &other) : mOffset(other.mOffset), mSize(other.mSize) {
	memcpy(mArray, other.mArray, sizeof(mArray));
	mOs << other.str();
}

#if __cplusplus > 199711L
BctbxException::BctbxException(const string &msg) : BctbxException(msg.c_str()) {
	mOffset++;
}
#else
BctbxException::BctbxException(const string &message) : mOffset(2) {
#ifdef HAVE_EXECINFO
	mSize = backtrace(mArray, sizeof(mArray) / sizeof(void *));
#else
	mSize = 0;
#endif
	*this << message;
	set_terminate(uncaught_handler); // invoke in case of uncautch exception for this thread
}
#endif

BctbxException::~BctbxException() throw() {
	// nop
}

#if __cplusplus > 199711L
BctbxException::BctbxException() : BctbxException("") {
	mOffset++;
}
#else
BctbxException::BctbxException() : mOffset(2) {
#ifdef HAVE_EXECINFO
	mSize = backtrace(mArray, sizeof(mArray) / sizeof(void *));
#else
	mSize = 0;
#endif
	*this << "";
#ifdef HAVE_EXECINFO
	set_terminate(uncaught_handler); // invoke in case of uncautch exception for this thread
#endif
}
#endif

void BctbxException::printStackTrace() const {
#ifdef HAVE_EXECINFO
	backtrace_symbols_fd(mArray + mOffset, mSize - mOffset, STDERR_FILENO);
#else
	std::cout << "stack trace not available on this platform";
#endif
}

void BctbxException::printStackTrace(std::ostream &os) const {
#ifdef HAVE_EXECINFO
	char **bt = backtrace_symbols(mArray, mSize);
	int position=0;
	for (unsigned int i = mOffset; i < mSize; ++i) {
		Dl_info info;
		char *demangled = NULL;
		int status = -1;
		if (dladdr(mArray[i], &info) && info.dli_sname) {
			demangled = abi::__cxa_demangle(info.dli_sname, NULL, 0, &status);
			os << position++ << setw(20) << basename((char*)info.dli_fname) << setw(16) << info.dli_saddr ;
			os << " ";
			if (demangled) {
				os << demangled;
				free(demangled);
			}
			else{
				os << info.dli_sname;
			}
		} else {
			os << bt[i];
		}
		os << std::endl;
	}
	free(bt);
#else
	os << "stack trace not available on this platform";
#endif
}

const std::string &BctbxException::str() const {
	mMessage = mOs.str(); // avoid returning a reference to temporary
	return mMessage;
}
const char *BctbxException::what() const throw() {
	return str().c_str();
}

// Class BctbxException
std::ostream &operator<<(std::ostream &__os, const BctbxException &e) {
	__os << e.str() << std::endl;
	e.printStackTrace(__os);
	return __os;
}

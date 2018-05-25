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

#include <algorithm>
#include <unordered_map>

#include <NSString>
#include <NSStringEncoding>

#include "bctoolbox/logging.h"
#include "bctoolbox/port.h"
#include "bctoolbox/charconv.h"

extern "C" char *bctbx_locale_to_utf8 (const char *str) {
	NSString *string = [NSString initWithCString:str encoding:defaultCStringEncoding];
	return bctbx_strdup([string cStringUsingEncoding:defaultCStringEncoding]);
}

extern "C" char *bctbx_utf8_to_locale (const char *str) {
	NSString *string = [NSString initWithUTF8String:str];
	return bctbx_strdup([string UTF8String]);
}

extern "C" char *bctbx_convert_any_to_utf8 (const char *str, const char *encoding) {
	if (!encoding)
		return NULL;

	NSString *encodingString = [NSString initWithCString:encoding encoding:defaultCStringEncoding];
	if (encodingString == kCFStringEncodingInvalidId) {
		bctbx_error("Error while converting a string from '%s' to 'UTF-8': unknown charset", encoding);
		return NULL;
	}

	NSStringEncoding encodingValue = CFStringConvertEncodingToNSStringEncoding(CFStringConvertIANACharSetNameToEncoding((CFStringRef) encodingString));

	NSString *string = [NSString initWithCString:str encoding:encodingValue];
	return bctbx_strdup([string UTF8String]);
}

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
#import "config.h"
#endif

#import <Foundation/Foundation.h>
#import <Foundation/NSString.h>

#import "bctoolbox/logging.h"
#import "bctoolbox/port.h"
#import "bctoolbox/charconv.h"

static NSStringEncoding getDefaultStringEncoding (const char *encoding) {
	const char *defaultEncoding = encoding;

	if (defaultEncoding == NULL)
		defaultEncoding = bctbx_get_default_encoding();

	if (!strcmp(defaultEncoding, "UTF-8"))
		return NSUTF8StringEncoding;

	if (!strcmp(defaultEncoding, "locale")) {
		return [NSString defaultCStringEncoding];
	} else {
		NSString *encodingString = [[NSString alloc] initWithCString:defaultEncoding encoding:[NSString defaultCStringEncoding]];

		CFStringEncoding stringEncoding = CFStringConvertIANACharSetNameToEncoding((CFStringRef) encodingString);
		if (stringEncoding == kCFStringEncodingInvalidId) {
			bctbx_error("Error invalid ID '%s': unknown charset", defaultEncoding);
			return [NSString defaultCStringEncoding];
		}

		return CFStringConvertEncodingToNSStringEncoding(stringEncoding);
	}
}

char *bctbx_locale_to_utf8 (const char *str) {
	NSStringEncoding encoding = getDefaultStringEncoding(NULL);

	if (encoding == NSUTF8StringEncoding)
		return bctbx_strdup(str);

	NSString *string = [[NSString alloc] initWithCString:str encoding:encoding];

	return bctbx_strdup([string UTF8String]);
}

char *bctbx_utf8_to_locale (const char *str) {
	NSStringEncoding encoding = getDefaultStringEncoding(NULL);

	if (encoding == NSUTF8StringEncoding)
		return bctbx_strdup(str);

	NSString *string = [[NSString alloc] initWithUTF8String:str];

	return bctbx_strdup([string cStringUsingEncoding:encoding]);
}

char *bctbx_convert_any_to_utf8 (const char *str, const char *encoding) {
	if (!encoding)
		return NULL;

	if (!strcmp(encoding, "UTF-8"))
		return bctbx_strdup(str);

	NSString *encodingString = [[NSString alloc] initWithCString:encoding encoding:[NSString defaultCStringEncoding]];

	CFStringEncoding stringEncoding = CFStringConvertIANACharSetNameToEncoding((CFStringRef) encodingString);
	if (stringEncoding == kCFStringEncodingInvalidId) {
		bctbx_error("Error while converting a string from '%s' to 'UTF-8': unknown charset", encoding);
		return NULL;
	}

	NSStringEncoding encodingValue = CFStringConvertEncodingToNSStringEncoding(stringEncoding);

	NSString *string = [[NSString alloc] initWithCString:str encoding:encodingValue];
	return bctbx_strdup([string UTF8String]);
}

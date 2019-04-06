/*
 *
 * Copyright (C) 2012  Belledonne Comunications, Grenoble, France
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#import <Foundation/Foundation.h>
#include "linphone/linphonecore.h"

#define LOGV(level, ...) [Log log:level file:__FILE__ line:__LINE__ format:__VA_ARGS__]
#define LOGD(...) LOGV(ORTP_DEBUG, __VA_ARGS__)
#define LOGI(...) LOGV(ORTP_MESSAGE, __VA_ARGS__)
#define LOGW(...) LOGV(ORTP_WARNING, __VA_ARGS__)
#define LOGE(...) LOGV(ORTP_ERROR, __VA_ARGS__)
#define LOGF(...) LOGV(ORTP_FATAL, __VA_ARGS__)

@interface Log : NSObject {
}

+ (void)log:(OrtpLogLevel)severity file:(const char *)file line:(int)line format:(NSString *)format, ...;
+ (void)enableLogs:(OrtpLogLevel)level;

void linphone_iphone_log_handler(const char *domain, OrtpLogLevel lev, const char *fmt, va_list args);
@end

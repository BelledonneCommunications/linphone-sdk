
#ifndef apple_utils_h
#define apple_utils_h

#import <CoreFoundation/CoreFoundation.h>

#define MS2_FRAMEWORK "org.linphone.mediastreamer2"

char *getPluginsDir(void);

char *toSafeCStr(CFStringRef str, CFStringEncoding encoding);

char *getBundleResourceDirPath(const char *framework, const char *resource);

#endif


#include "apple_utils.h"

//Plugins are installed in the 'Libraries' sub directory of the mediastreamer2 Framework
char *getPluginsDir(void) {
        return getBundleResourceDirPath(MS2_FRAMEWORK, "Libraries/");
}

//Safely get an encoded string from the given CFStringRef
char *toSafeCStr(CFStringRef str, CFStringEncoding encoding) {
    char *ret;

    if (str == NULL) {
        return ret;
    }
    CFIndex length = CFStringGetLength(str);
    CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, encoding) + 1;
    char *buffer = (char *) malloc((size_t) maxSize);
    if (buffer) {
        if (CFStringGetCString(str, buffer, maxSize, encoding)) {
            ret = buffer;
        }
    }
    return ret;
}


//Returns the absolute path of the given resource relative to the mediastreamer2 Framework location
char *getBundleResourceDirPath (const char *framework, const char *resource) {
	CFStringEncoding encodingMethod = CFStringGetSystemEncoding();
	CFStringRef cfFramework = CFStringCreateWithCString(NULL, framework, encodingMethod);
	CFStringRef cfResource = CFStringCreateWithCString(NULL, resource, encodingMethod);
	CFBundleRef bundle = CFBundleGetBundleWithIdentifier(cfFramework);
	char *path = NULL;

	if (bundle) {
	    CFRetain(bundle);
	    CFURLRef bundleUrl = CFBundleCopyBundleURL(bundle);

	    if (bundleUrl) {
	        CFURLRef resourceUrl = CFURLCreateCopyAppendingPathComponent(NULL, bundleUrl, cfResource, true);
			CFStringRef cfSystemPath = CFURLCopyFileSystemPath(resourceUrl, kCFURLPOSIXPathStyle);
	        path = toSafeCStr(cfSystemPath, encodingMethod);
			CFRelease(cfSystemPath);
			CFRelease(bundleUrl);
			CFRelease(resourceUrl);
	    }
	    CFRelease(bundle);
	}
	CFRelease(cfResource);
	CFRelease(cfFramework);
	return path;
}

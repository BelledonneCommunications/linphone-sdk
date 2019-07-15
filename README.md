[![pipeline status](https://gitlab.linphone.org/BC/public/linphone-sdk/badges/master/pipeline.svg)](https://gitlab.linphone.org/BC/public/linphone-sdk/commits/master)

# linphone-sdk

Meta repository holding all the dependencies to build a full Linphone SDK.

The currently supported platforms are Android, iOS, Desktop (Linux, Windows, Mac OS X) and UWP (Universal Windows Platform).

## Build dependencies

### Common to all target platforms

The following tools must be installed on the build machine:
 - cmake >= 3.6
 - python = 2.7 (python 3.7 if C# wrapper generation is disabled)
 - pip
 - yasm
 - nasm
 - doxygen
 - Pystache (use `pip install pystache`)
 - six (use `pip install six`)

### Windows

SDK compilation is supported on Visual Studio 15 2017.
Setting the build environment on Windows is a bit tricky.
In addition to the common components listed above, these components must be installed:
 - MinGW (select all installer options except Ada and Fortran)
 - Yasm:
	- download yasm-1.3.0-win32.exe
	- copy it to a `bin` directory of your user directory,
	- rename yasm-1.3.0-win32.exe as yasm.exe

Visual Studio must also be properly configured with addons. Under "Tools"->"Obtain tools and features", make sure that the following components are installed:
 - Tasks: Select Windows Universal Platform development, Desktop C++ Development, .NET Development
 - Individual component: Windows 8.1 SDK
	
Finally add your user `bin` directory and `C:\Mingw\bin` to the PATH environement variable from windows advanced settings. 

## Building and customizing the SDK

The steps to build the SDK are:

 1. Create and go inside a directory where the SDK will be built:
 `mkdir build && cd build
 2. Execute CMake to configure the project:
 `cmake ..`
 3. Build the SDK:
 `cmake --build . ` 
 or 
 `cmake --build . --parallel <number of jobs>` (which is faster).

You can pass some options to CMake at the second step to configure the SDK as you want.
For instance, to build an iOS SDK (the default being Desktop):
 `cmake .. -DLINPHONESDK_PLATFORM=IOS`

### Windows
 `cmake --build .` works on Windows as for all platforms.
 However it may be convenient to build from Visual Studio, which you can do:
 - open `linphone-sdk.sln` with Visual Studio
 - make sure that RelWithDebInfo mode is selected unless you specified -DCMAKE_BUILD_TYPE=Debug to cmake (see customization options below).
 - use `Build solution` to build.

## Upgrading your SDK

Simply re-invoking `cmake --build .` in your build directory should update your SDK. If compilation fails, you may need to rebuilding everything by erasing your build directory and restarting your build as explained above.

## Customizing features

The SDK can be customized by passing `-D` options to CMake when configuring the project. If you know the options you want to use, just pass them to CMake.

Otherwise, you can use the `cmake-gui` or `ccmake` commands to configure all the available options interactively.

### Important customization options

Some customization are particularly important:

 1. `CMAKE_BUILD_TYPE`: By default it is set to `RelWithDebInfo` to build in release mode keeping the debug information. You might want to set it to `Debug` to ease the debugging. On Android, use `ASAN` to make a build linking with the Android Adress Sanitizer (https://github.com/google/sanitizers/wiki/AddressSanitizerOnAndroid).
 2. `LINPHONESDK_PLATFORM`: The platform for which you want to build the Linphone SDK. It must be one of: `Android`, `IOS`.
 3. `LINPHONESDK_ANDROID_ARCHS`: A comma-separated list of the architectures for which you want to build the Android Linphone SDK for.
 4. `LINPHONESDK_IOS_ARCHS`: Same as `LINPHONESDK_ANDROID_ARCHS` but for an iOS build.
 5. `LINPHONESDK_IOS_BASE_URL`: The base of the URL that will be used to download the zip file of the SDK.
 6. `LINPHONESDK_UWP_ARCHS`: Same as `LINPHONESDK_ANDROID_ARCHS` but for an UWP build.

## Licensing: GPL third parties versus non GPL third parties

This SDK can be generated in 2 flavors:

* GPL third parties enabled means that the Linphone SDK includes GPL third parties like FFmpeg. If you choose this flavor, your final application **must comply with GPL in any case**. This is the default mode.

* NO GPL third parties means that the Linphone SDK will only use non GPL code except for the code from Belledonne Communications. If you choose this flavor, your final application is **still subject to the GPL except if you have a [commercial license from Belledonne Communications](http://www.belledonne-communications.com/products.html)**.

To generate the a SDK without GPL third parties, use the `-DENABLE_GPL_THIRD_PARTIES=NO` option when configuring the project.

## Note regarding third party components subject to license

The Linphone SDK is compiled with third parties code that are subject to patent license, especially: AMR, SILK, G729 and H264 codecs.
To build a SDK with any of these features you need to enable the `ENABLE_NON_FREE_CODECS` option.
Before embedding these features in your final application, **make sure to have the right to do so**.

### Windows UWP
You can use linphone-sdk Win32 in your Windows UWP app.
To do this follow instructions from microsoft.
https://docs.microsoft.com/en-us/windows/uwp/porting/desktop-to-uwp-root

A nuget package (.nupkg) can be generated by running the bat script cmake/Windows/nuget/make-nuget.bat from the top-level source directory of this project. (TODO: incorporate this script as build step of CMakeLists.txt).


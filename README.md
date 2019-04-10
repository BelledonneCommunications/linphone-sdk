# linphone-sdk

Meta repository holding all the dependencies to build a full Linphone SDK.

The currently supported platforms are Android, iOS, Desktop (Linux, Windows, Mac OS X) and UWP (Universal Windows Platform).

## Dependencies

### Windows
SDK compilation is supported on Visual Studio 15 2017.
Setting the build environment on Windows is very tricky.
Please read carefully the instructions below.
The following software components must be installed to perform the compilation:
 - MinGW (select all installer options except Ada and Fortran)
 - Python 3.7 (important: toggle the option 'add Python to environment variable')
 - Doxygen
 - Pystache and six (use `pip install pystache six`)
 - Yasm:
	- download yasm-1.3.0-win32.exe
	- copy it to a `bin` directory of your user directory,
	- rename yasm-1.3.0-win32.exe as yasm.exe
 - Install Windows 8.1 SDK from Visual Studio, from "Tools" -> "Obtain tools and features".
	
  Finally add your user `bin` directory and `C:\Mingw\bin` to the PATH environement variable from windows advanced settings. 

## Building and customizing the SDK

The build system is based on CMake, so you need to install it first if you don't have it on your machine.

The steps to build the SDK are:

 1. Create and go inside a directory where the SDK will be built:
 `mkdir build && cd build
 2. Execute CMake to configure the project:
 `cmake ..`
 3. Build the SDK:
 `cmake --build . ` 
 or 
 `cmake --build . --parallel <number of jobs>` (which is faster).

### Windows
 `cmake --build .` works on Windows as for all platforms.
 However it may be convenient to build from Visual Studio, which you can do:
 - open `linphone-sdk.sln` with Visual Studio
 - make sure that RelWithDebInfo mode is selected unless you specified -DCMAKE_BUILD_TYPE=Debug to cmake (see customization options below).
 - use `Build solution` to build.


You can pass some options to CMake at the second step to configure the SDK as you want.
For instance, to build an iOS SDK (the default being Desktop):
 `cmake .. -DLINPHONESDK_PLATFORM=IOS`

## Upgrading your SDK

Simply re-invoking `cmake --build .` in your build directory should update your SDK. If compilation fails, you may need to rebuilding everything by erasing your build directory and restarting your build as explained above.

## Customizing features

The SDK can be customized by passing `-D` options to CMake when configuring the project. If you know the options you want to use, just pass them to CMake.

Otherwise, you can use the `cmake-gui` or `ccmake` commands to configure all the available options interactively.

### Important customization options

Some customization are particularly important:

 1. `CMAKE_BUILD_TYPE`: By default it is set to `RelWithDebInfo` to build in release mode keeping the debug information. You might want to set it to `Debug` to ease the debugging.
 2. `LINPHONESDK_PLATFORM`: The platform for which you want to build the Linphone SDK. It must be one of: `Android`, `IOS`.
 3. `LINPHONESDK_ANDROID_ARCHS`: A comma-separated list of the architectures for which you want to build the Android Linphone SDK for.
 4. `LINPHONESDK_IOS_ARCHS`: Same as `LINPHONESDK_ANDROID_ARCHS` but for an iOS build.
 5. `LINPHONESDK_IOS_BASE_URL`: The base of the URL that will be used to download the zip file of the SDK.
 6. `LINPHONESDK_UWP_ARCHS`: Same as `LINPHONESDK_ANDROID_ARCHS` but for an UWP build.

## Licensing: GPL third parties versus non GPL third parties

This SDK can be generated in 2 flavors:

* GPL third parties enabled means that the Linphone SDK includes GPL third parties like FFmpeg. If you choose this flavor, your final application **must comply with GPL in any case**. This is the default mode.

* NO GPL third parties means that the Linphone SDK will only use non GPL code except for the code from Belledonne Communications. If you choose this flavor, your final application is **still subject to GPL except if you have a [commercial license from Belledonne Communications](http://www.belledonne-communications.com/products.html)**.

To generate the a SDK without GPL third parties, use the `-DENABLE_GPL_THIRD_PARTIES=NO` option when configuring the project.

## Note regarding third party components subject to license

The Linphone SDK is compiled with third parties code that are subject to patent license, especially: AMR, SILK, G729 and H264 codecs.
To build a SDK with any of these features you need to enable the `ENABLE_NON_FREE_CODECS` option.
Before embedding these features in your final application, **make sure to have the right to do so**.

### Windows UWP
You can use linphone-sdk Win32 in your Windows UWP app.
To do this follow instructions from microsoft.
https://docs.microsoft.com/en-us/windows/uwp/porting/desktop-to-uwp-root

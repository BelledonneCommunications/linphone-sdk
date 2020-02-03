[![pipeline status](https://gitlab.linphone.org/BC/public/linphone-sdk/badges/master/pipeline.svg)](https://gitlab.linphone.org/BC/public/linphone-sdk/commits/master)

# linphone-sdk

Linphone-sdk is a project that bundles liblinphone and its dependencies as git submodules, in the purpose of simplifying
the compilation and packaging of the whole liblinphone suite, comprising mediastreamer2, belle-sip, ortp and many others.
Its compilation produces a SDK suitable to create applications running on top of these components.
The submodules that are not developped or maintained by the Linphone team are grouped in the external/ directory.
The currently supported platforms are Android, iOS, Desktop (Linux, Windows, Mac OS X) and UWP (Universal Windows Platform).

## License

Copyright © Belledonne Communications

The software products developed in the context of the Linphone project are dual licensed, and are available either :

 - under a [GNU/GPLv3 license](https://www.gnu.org/licenses/gpl-3.0.en.html), for free (open source). Please make sure that you understand and agree with the terms of this license before using it (see LICENSE.txt file for details).

 - under a proprietary license, for a fee, to be used in closed source applications. Contact [Belledonne Communications](https://www.linphone.org/contact) for any question about costs and services.

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
 - Perl: You can use binaries from http://strawberryperl.com/
 
Visual Studio must also be properly configured with addons. Under "Tools"->"Obtain tools and features", make sure that the following components are installed:
 - Tasks: Select Windows Universal Platform development, Desktop C++ Development, .NET Development
 - Individual component: Windows 8.1 SDK
	
Finally add your user `bin` directory, `C:\Mingw\bin` and `<perl_path>\bin` to the PATH environement variable from windows advanced settings. 

## Build

The generic steps to build the SDK are:

 1. Create and go inside a directory where the SDK will be built:
 `mkdir build && cd build`
 2. Execute CMake to configure the project:
 `cmake <SOME OPTIONS> ..`
 3. Build the SDK:
 `cmake --build . ` 
 or 
 `cmake --build . --parallel <number of jobs>` (which is faster).

The options below define the target of the compilation, and hence are required most of the time:
- `LINPHONESDK_PLATFORM`: The platform for which you want to build the Linphone SDK. It must be one of: `Android`, `IOS`, Raspberry or Desktop (default value).
- `CMAKE_BUILD_TYPE`: By default it is set to `RelWithDebInfo` to build in release mode keeping the debug information. You might want to set it to `Debug` to ease the debugging. On Android, use `ASAN` to make a build linking with the Android Adress Sanitizer (https://github.com/google/sanitizers/wiki/AddressSanitizerOnAndroid).

These generic steps work for all platforms, but a few specifics behaviors are good to know and are described
in the next subsections.

### iOS

Requirement: Xcode 10 or earlier (Xcode 11 is not supported yet).

Cmake has limited swift support: only Ninja and Xcode generators can handle swift.
Until cmake has full swift support, you need to specify configuration step by specifying one of the two backends:

`cmake .. -G Xcode -DLINPHONESDK_PLATFORM=IOS` or `cmake .. -G Ninja -DLINPHONESDK_PLATFORM=IOS`

If using the Xcode generator, the build type must be specified for compilation step with `--config`:
`cmake --build . --config <cfg>`, where `<cfg>` is one of `Debug`, `Release`, `RelWithDebInfo` or `MinSizeRel`.
If nothing is specified, the SDK will be built in Debug mode.

Please note that the Xcode backend is very slow: about one hour of build time, compared to approximately 15 mn for Ninja.

### Windows
 `cmake --build .` works on Windows as for all platforms.
 However it may be convenient to build from Visual Studio, which you can do:
 - open `linphone-sdk.sln` with Visual Studio
 - make sure that RelWithDebInfo mode is selected unless you specified -DCMAKE_BUILD_TYPE=Debug to cmake (see customization options below).
 - use `Build solution` to build.

## Upgrading your SDK

Simply re-invoking `cmake --build .` in your build directory should update your SDK. If compilation fails, you may need to rebuilding everything by erasing your build directory and restarting your build as explained above.

## Customizing the build

The SDK compilation can be customized by passing `-D` options to CMake when configuring the project. If you know the options you want to use, just pass them to CMake.

Otherwise, you can use the `cmake-gui` or `ccmake` commands to configure all the available options interactively.

The following options control the cpu architectures built for a target platform:
- `LINPHONESDK_ANDROID_ARCHS`: A comma-separated list of the architectures for which you want to build the Android Linphone SDK for.
- `LINPHONESDK_IOS_ARCHS`: Same as `LINPHONESDK_ANDROID_ARCHS` but for an iOS build.
- `LINPHONESDK_IOS_BASE_URL`: The base of the URL that will be used to download the zip file of the SDK.
- `LINPHONESDK_UWP_ARCHS`: Same as `LINPHONESDK_ANDROID_ARCHS` but for an UWP build.

These ON/OFF options control the enablement of important features of the SDK, which have an effect on the size of produced size object code:
- `ENABLE_VIDEO`: enablement of video call features.
- `ENABLE_ADVANCED_IM`: enablement of group chat and secure IM features
- `ENABLE_ZRTP`: enablement of ZRTP ciphering
- `ENABLE_DB_STORAGE`: enablement of database storage for IM.
- `ENABLE_VCARD`: enablement of Vcard features.
- `ENABLE_MKV`: enablement of Matroska video file reader/writer.

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


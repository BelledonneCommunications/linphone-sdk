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
 - cmake >= 3.11
 - python >= 3.6
 - pip (or pip3 if the build machine has both python2 and python3)
 - yasm
 - nasm
 - doxygen
 - Pystache (use `pip install pystache` or `pip3 install pystache`)
 - six (use `pip install six` or `pip3 install six`)

### Retrieve the dependencies

Linphone-sdk's git repository comprises git submodules. It must be cloned with the `--recursive` option. After updating or switching branches, never forget to checkout and update the submodules with:

    git submodule update --init --recursive

### Windows

SDK compilation is supported on `Visual Studio 15 2017` and `MSYS2` https://www.msys2.org/.

#### MSYS2

Follow MSYS2 instructions on their "Getting Started" page.

For MinGW32, install the needed tools in `MSYS2 MSYS` console:
 - `pacman -Sy --needed base-devel mingw-w64-i686-toolchain`
If you are using python from MSYS:
 - `pacman -S python3-pip` in `MSYS2 MSYS` console
 - `python3 -m pip install pystache six` in `cmd`

When building the SDK, it will install automatically from MSYS2 : `perl`, `yasm`, `gawk`, `bzip2`, `nasm, `sed`, `patch`, `pkg-config`, `gettext`, `glib2` and `intltool` (if needed)

Visual Studio must also be properly configured with addons. Under "Tools"->"Obtain tools and features", make sure that the following components are installed:
 - Tasks: Select Windows Universal Platform development, Desktop C++ Development, .NET Development
 - Under "Installation details". Go to "Desktop C++ Development" and add "SDK Windows 8.1 and SDK UCRT"
 - Individual component: Windows 8.1 SDK

In this order, add `C:\msys64\`, `C:\msys64\usr\bin` and `C:\msys64\mingw32\bin` in your PATH (the last one is needed by cmake to know where gcc is) to the PATH environement variable from windows advanced settings.

## Build

A build with the Ninja generator (`-G "Ninja"` ) is prefered for speed-up build times.

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

Requirement: 
 - Xcode >= 10
 - cmake >= 3.18.2

Cmake has limited swift support: only Ninja and Xcode generators can handle swift.
Until cmake has full swift support, you need to specify configuration step by specifying one of the two backends:

`cmake .. -G Xcode -DLINPHONESDK_PLATFORM=IOS` or `cmake .. -G Ninja -DLINPHONESDK_PLATFORM=IOS`

If using the Xcode generator, the build type must be specified for compilation step with `--config`:
`cmake --build . --config <cfg>`, where `<cfg>` is one of `Debug`, `Release`, `RelWithDebInfo` or `MinSizeRel`.
If nothing is specified, the SDK will be built in Debug mode.

Please note that the Xcode backend is very slow: about one hour of build time, compared to approximately 15 mn for Ninja.

### Android (using Docker)

Download the Docker image of the Android build environment:

```bash
docker login gitlab.linphone.org:4567
docker pull gitlab.linphone.org:4567/bc/public/linphone-sdk/bc-dev-android-r20:20210217_python3
```

Load the build environment:

```bash
cd <linphone-sdk-source>
docker run -it --volume=$PWD:/home/bc/linphone-sdk gitlab.linphone.org:4567/bc/public/linphone-sdk/bc-dev-android:r20 /bin/bash -i
```

Next command lines must be typed in the docker shell:

```bash
# Make build directory
mkdir /home/bc/linphone-sdk/build && cd /home/bc/linphone-sdk/build

# Configure the build
cmake .. -DLINPHONESDK_PLATFORM=Android -DLINPHONESDK_ANDROID_ARCHS=arm64 <extra-variable-definitions>

# Build
make -j <njobs>

# Quit build environment
exit
```

The freshly built SDK is located in `<linphone-sdk>/build`.


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

## Windows UWP
You can use linphone-sdk in your Windows UWP app with 2 kinds of library : using the Windows Bridge or the UWP mode.
The Windows Bridge mode is built by using `-DCMAKE_TOOLCHAIN_FILE=../cmake-builder/toolchains/toolchain-windows-store.cmake`. It is only for x86 build. You will find all libraries in linphone-sdk/desktop. Add `-DENABLE_CSHARP_WRAPPER=ON` to generate the C# wrapper.
The UWP mode is built by setting the option `-DLINPHONESDK_PLATFORM=UWP`. It is for x64 builds. You will find all libraries in linphone-sdk/uwp-x64

Then, you can inject directly all your libraries that you need or package the SDK in a Nuget package.

Build SDK:

    cmake --build . --target ALL_BUILD --config=RelWithDebInfo

### Nuget packaging
You can package 3 kinds of binaries : win32, uwp and win32 with Windows Store Compatibility.

- win32: this is the win32 version of linphone-sdk without any restrictions. The framework is 'win'.
- uwp : this is a uwp x64 version of linphone--sdk. You will not be able to use OpenH264 and Lime X3DH. The framework is 'uap10.0'.
- win32 Windows Store : this is the win32 version of linphone-sdk with the Windows Store Compatibility enabled for Windows Bridge. The framework is 'netcore'.

In an another build folder (like buildNuget), set these options :
- (Needed) -DLINPHONESDK_PACKAGER=Nuget
- (Optional) -DLINPHONESDK_DESKTOP_ZIP_PATH=<path of the zip file containing the Desktop binaries> (eg. C:/projects/desktop-uwp/linphone-sdk/buildx86/linphone-sdk) 
- (Optional) -DLINPHONESDK_UWP_ZIP_PATH=<path of the zip file containing the UWP binaries> (eg. C:/projects/desktop-uwp/linphone-sdk/builduwp/linphone-sdk)
- (Optional) -DLINPHONESDK_WINDOWSSTORE_ZIP_PATH=<path of the zip file containing the Desktop binaries with Store compatibility enabled> (eg. C:/projects/desktop-uwp/linphone-sdk/buildx86_store/linphone-sdk)

Build the Package:

    cmake .. 
	cmake --build . --target ALL_BUILD --config=RelWithDebInfo
	
The nuget package will be in linphone-sdk/packages
The generated package can keep the same file name between each generations on the same git version. Visual studio keep a cache of the Nuget and you need to delete its internal folder to takke account any newer version for the same name.
The folder can be found in your system path at <User>/.nuget/packages/linphonesdk

### Demo app

There is a very limited version of an applciation that can use this nuget at `https://gitlab.linphone.org/BC/public/linphone-windows10/tree/feature/uwp`

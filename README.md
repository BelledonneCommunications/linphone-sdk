[![pipeline status](https://gitlab.linphone.org/BC/public/linphone-sdk/badges/master/pipeline.svg)](https://gitlab.linphone.org/BC/public/linphone-sdk/commits/master)

# Linphone-SDK

Linphone-SDK is a project that bundles Liblinphone and its dependencies as git submodules, in the purpose of simplifying
the compilation and packaging of the whole Liblinphone suite, comprising Mediastreamer2, Belle-sip, oRTP and many others.
Its compilation produces a SDK suitable to create applications running on top of these components.
The submodules that are not developped or maintained by the Linphone team are grouped in the external/ directory.
The currently supported platforms are Android, iOS, Desktop (Linux, Windows, Mac OS X) and UWP (Universal Windows Platform).

## License

Copyright © Belledonne Communications

The software products developed in the context of the Linphone project are dual licensed, and are available either :

 - under a [GNU/AGPLv3 license](https://www.gnu.org/licenses/agpl-3.0.html), for free (open source). Please make sure that you understand and agree with the terms of this license before using it (see LICENSE.txt file for details). AGPLv3 is choosen over GPLv3 because linphone-sdk can be used to create server-side applications, not just client-side ones. Any product incorporating linphone-sdk to provide a remote service then has to be licensed under AGPLv3.
 For a client-side product, the "remote interaction" clause of AGPLv3 being irrelevant, the usual terms GPLv3 terms do apply (the two licences differ by this only clause).

 - under a proprietary license, for a fee, to be used in closed source applications. Contact [Belledonne Communications](https://www.linphone.org/contact) for any question about costs and services.

 **For more information about de third-party licences, please see the [Licensing: GPL third parties versus non GPL third parties](#licensing-gpl-third-parties-versus-non-gpl-third-parties) section

## Build dependencies

### Common to all target platforms

The following tools must be installed on the build machine:
 - Cmake >= 3.11
 - python >= 3.6
 - pip (or pip3 if the build machine has both python2 and python3)
 - yasm
 - nasm
 - doxygen
 - Pystache (use `pip install pystache` or `pip3 install pystache`)
 - six (use `pip install six` or `pip3 install six`)

### Retrieve the dependencies

Linphone-SDK's git repository comprises git submodules. It must be cloned with the `--recursive` option. After updating or switching branches, never forget to checkout and update the submodules with:

    git submodule update --init --recursive

### Windows

SDK compilation is supported on `Visual Studio 15 2017`/`Visual Studio 16 2019` and `MSYS2` https://www.msys2.org/.

Only [CMake](https://cmake.org/), [7Zip](https://www.7-zip.org/download.html) and [MSYS2](https://www.msys2.org/) are needed before the build.
Add the 7Zip executable in your PATH as it is not automatically done.


#### MSYS2

Follow MSYS2 instructions on their ["Getting Started" page](https://www.msys2.org/).

Both MinGW32 and MinGW64 are supported.

When building the SDK and if you set `-DLINPHONE_BUILDER_WINDOWS_TOOLS_CHECK=ON`, it will install automatically from MSYS2 : `toolchain`, `python`, `doxygen`, `perl`, `yasm`, `gawk`, `bzip2`, `nasm`, `sed`, `patch`, `pkg-config`, `gettext`, `glib2`, `intltool` and `graphviz` (if needed)

In this order, add `C:\msys64\mingw<N>\bin`, `C:\msys64\` and `C:\msys64\usr\bin` in your PATH environement variable from Windows advanced settings. Binaries from the msys folder (not from mingw32/64) doesn't fully support Windows Path and thus, they are to be avoided.
*<N> is the version of MinGW32/64*

#### Visual Studio

Visual Studio must also be properly configured with addons. Under "Tools"->"Obtain tools and features", make sure that the following components are installed:
 - Tasks: Select Windows Universal Platform development, Desktop C++ Development, .NET Development
 
For Visual Studio 2017 :
 - Under "Installation details". Go to "Desktop C++ Development" and add "SDK Windows 8.1 and SDK UCRT"
 - Individual component: Windows 8.1 SDK

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
 - Cmake >= 3.18.2

Cmake has limited swift support: only Ninja and Xcode generators can handle swift.
Until Cmake has full swift support, you need to specify configuration step by specifying one of the two backends:

`cmake .. -G Xcode -DLINPHONESDK_PLATFORM=IOS` or `cmake .. -G Ninja -DLINPHONESDK_PLATFORM=IOS`

If using the Xcode generator, the build type must be specified for compilation step with `--config`:
`cmake --build . --config <cfg>`, where `<cfg>` is one of `Debug`, `Release`, `RelWithDebInfo` or `MinSizeRel`.
If nothing is specified, the SDK will be built in Debug mode.

Please note that the Xcode backend is very slow: about one hour of build time, compared to approximately 15 mn for Ninja.

⚙ Note to developers: If a new Apple `.framework` folder needs to be added to the iOS build, remember to update the [NuGet iOS project] to include it.

[NuGet iOS project]: cmake/NuGet/Xamarin/LinphoneSDK.Xamarin/LinphoneSDK.Xamarin.iOS/LinphoneSDK.Xamarin.iOS.csproj

### Android (using Docker)

Download the Docker image of the Android build environment:


---
**public access, with token**

Use this token to access the Docker registry :

user : gitlab+deploy-token-17

pass : fFVgA_5Mf-qn2WbvsKRL

---


**private access**

A simple login with your Gitlab account should work.

---

```bash
docker login gitlab.linphone.org:4567/bc/public/linphone-sdk
docker pull gitlab.linphone.org:4567/bc/public/linphone-sdk/bc-dev-android-r20:20210914_update_java11
```

Load the build environment:

```bash
cd <linphone-sdk-source>
docker run -it --volume=$PWD:/home/bc/linphone-sdk gitlab.linphone.org:4567/bc/public/linphone-sdk/bc-dev-android-r20:20210914_update_java11 /bin/bash -i
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

### MacOS

Requirement:
 - Xcode >= 12

### Windows

 `cmake --build .` works on Windows as for all platforms.
 However it may be convenient to build from Visual Studio, which you can do:
 - open `linphone-sdk.sln` with Visual Studio
 - make sure that RelWithDebInfo mode is selected unless you specified -DCMAKE_BUILD_TYPE=Debug to Cmake (see customization options below).
 - use `Build solution` to build.

### Windows UWP and Stores

You can use linphone-sdk in your Windows UWP app with the UWP mode.
Win32 application can use the Windows Store mode in order to be publishable in Windows Stores.
The Windows Bridge mode is built by using `-DCMAKE_TOOLCHAIN_FILE=../cmake-builder/toolchains/toolchain-windows-store.cmake`. It is only for x86 build. You will find all libraries in linphone-sdk/desktop. Add `-DENABLE_CSHARP_WRAPPER=ON` to generate the C# wrapper.
The UWP mode is built by setting the option `-DLINPHONESDK_PLATFORM=UWP`. If `-DLINPHONESDK_UWP_ARCHS` is not used, x86 and x64 will be build. 
All libraries will be in linphone-sdk/uwp-x86 or linphone-sdk/uwp-x64

Then, you can inject directly all your libraries that you need or package the SDK in a Nuget package.

### NuGet packaging

The Linphone SDK is available as a `.nuget` package for .NET applications (Windows & Xamarin).

See the [`cmake/NuGet`](cmake/NuGet/README.md) folder for build instructions.


## Upgrading your SDK

Simply re-invoking `cmake --build .` in your build directory should update your SDK. If compilation fails, you may need to rebuilding everything by erasing your build directory and restarting your build as explained above.

## Customizing the build

The SDK compilation can be customized by passing `-D` options to CMake when configuring the project. If you know the options you want to use, just pass them to CMake.

Otherwise, you can use the `cmake-gui` or `ccmake` commands to configure all the available options interactively.

The following options control the cpu architectures built for a target platform:
- `LINPHONESDK_ANDROID_ARCHS`: A comma-separated list of the architectures for which you want to build the Android Linphone SDK for.
- `LINPHONESDK_IOS_ARCHS`: Same as `LINPHONESDK_ANDROID_ARCHS` but for an iOS build.
- `LINPHONESDK_IOS_BASE_URL`: The base of the URL that will be used to download the zip file of the SDK.
- `LINPHONESDK_MACOS_ARCHS`: Same as `LINPHONESDK_ANDROID_ARCHS` but for a MacOS build. Currently supported : x86_64, arm64
- `LINPHONESDK_UWP_ARCHS`: Same as `LINPHONESDK_ANDROID_ARCHS` but for an UWP build and Nuget.

These ON/OFF options control the enablement of important features of the SDK, which have an effect on the size of produced size object code:
- `ENABLE_VIDEO`: enablement of video call features.
- `ENABLE_ADVANCED_IM`: enablement of group chat and secure IM features
- `ENABLE_ZRTP`: enablement of ZRTP ciphering
- `ENABLE_DB_STORAGE`: enablement of database storage for IM.
- `ENABLE_VCARD`: enablement of Vcard features.
- `ENABLE_MKV`: enablement of Matroska video file reader/writer.
- `ENABLE_LDAP`: enablement of OpenLDAP.

<a name="licensing-gpl-third-parties-versus-non-gpl-third-parties"></a>
## Licensing: GPL third parties versus non GPL third parties

This SDK can be generated in 2 flavors:

* GPL third parties enabled means that the Linphone SDK includes GPL third parties like FFmpeg. If you choose this flavor, your final application **must comply with GPL in any case**.

* NO GPL third parties means that the Linphone SDK will only use non GPL code except for the code from Belledonne Communications. If you choose this flavor, your final application is **still subject to the GPL except if you have a [commercial license from Belledonne Communications](http://www.belledonne-communications.com/products.html)**. **This is the default mode.**

To generate the a SDK with GPL third parties, use the `-DENABLE_GPL_THIRD_PARTIES=YES` option when configuring the project (you can set "NO" to disable explicitly).

## Note regarding third party components subject to license

The Linphone SDK can be compiled with third parties code that are subject to patent license, especially: AMR, SILK and H264 codecs.
To build a SDK with any of these features you need to enable the `ENABLE_NON_FREE_CODECS` option (**disabled by default**).
Before embedding these features in your final application, **make sure to have the right to do so**.

For more information, please visit [our dedicated wiki page](https://wiki.linphone.org/xwiki/wiki/public/view/Linphone/Third%20party%20components%20/)

### Nuget packaging
You can package 3 kinds of binaries : win32, uwp and win32 with Windows Store Compatibility.

- win32: this is the win32 version of Linphone-SDK without any restrictions. The framework is 'win'.
- uwp : this is a uwp x64/x86 version of Linphone-SDK. The framework is 'uap10.0'.
- win32 Windows Store : this is the win32 version of Linphone-SDK with the Windows Store Compatibility enabled for Windows Bridge. The framework is 'netcore'.

In an another build folder (like buildNuget), set these options. At least one path is needed :
- (Needed) -DLINPHONESDK_PACKAGER=Nuget
- (Optional) -DLINPHONESDK_DESKTOP_ZIP_PATH=<path of the zip file containing the Desktop binaries> (eg. C:/projects/desktop-uwp/linphone-sdk/buildx86/linphone-sdk)
- (Optional) -DLINPHONESDK_UWP_ZIP_PATH=<path of the zip file containing the UWP binaries> (eg. C:/projects/desktop-uwp/linphone-sdk/builduwp/linphone-sdk)
- (Optional) -DLINPHONESDK_WINDOWSSTORE_ZIP_PATH=<path of the zip file containing the Desktop binaries with Store compatibility enabled> (eg. C:/projects/desktop-uwp/linphone-sdk/buildx86_store/linphone-sdk)
- (Optional) -DLINPHONESDK_UWP_ARCHS=<list of archs to package> (eg. "x86, x64" or "x64")

Build the Package:

	cmake .. -DLINPHONESDK_PACKAGER=Nuget -DLINPHONESDK_UWP_ZIP_PATH=C:/projects/desktop-uwp/linphone-sdk/builduwp/linphone-sdk
	cmake --build . --target ALL_BUILD --config=RelWithDebInfo

The nuget package will be in linphone-sdk/packages
The generated package can keep the same file name between each generations on the same git version. Visual studio keep a cache of the Nuget and you need to delete its internal folder to take account any newer version for the same name.
The folder can be found in your system path at <User>/.nuget/packages/linphonesdk

### Demo app

There is a very limited version of an application that can use this nuget at `https://gitlab.linphone.org/BC/public/linphone-windows10`


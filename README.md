[![pipeline status](https://gitlab.linphone.org/BC/public/linphone-sdk/badges/master/pipeline.svg)](https://gitlab.linphone.org/BC/public/linphone-sdk/commits/master)

# Linphone-SDK

Linphone-SDK is a project that bundles Liblinphone and its dependencies as git submodules, in the purpose of simplifying
the compilation and packaging of the whole Liblinphone suite, comprising Mediastreamer2, Belle-sip, oRTP and many others.
Its compilation produces a SDK suitable to create applications running on top of these components.
The submodules that are not developed or maintained by the Linphone team are grouped in the external/ directory.
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
 - Cmake >= 3.22
 - python >= 3.6
 - pip (or pip3 if the build machine has both python2 and python3)
 - yasm
 - nasm
 - doxygen
 - Pystache (use `pip install pystache` or `pip3 install pystache`)
 - six (use `pip install six` or `pip3 install six`)

If you are building the AV1 codec, which is enabled by default (`ENABLE_AV1=Off` to disable), you will also need:
 - Meson
 - Ninja
 - Perl

### Retrieve the dependencies

Linphone-SDK's git repository comprises git submodules. It must be cloned with the `--recursive` option. After updating or switching branches, never forget to checkout and update the submodules with:

    git submodule update --init --recursive

### Windows

SDK compilation requires `Visual Studio 17 2022` and `MSYS2` https://www.msys2.org/.

Only [CMake](https://cmake.org/), [7Zip](https://www.7-zip.org/download.html) and [MSYS2](https://www.msys2.org/) are needed before the build.
Add the 7Zip executable in your PATH as it is not automatically done.


#### MSYS2

Follow MSYS2 instructions on their ["Getting Started" page](https://www.msys2.org/).

Both MinGW32 and MinGW64 are supported.

When building the SDK and if you set `-DENABLE_WINDOWS_TOOLS_CHECK=ON`, it will install automatically from MSYS2 : `toolchain`, `python`, `doxygen`, `perl`, `yasm`, `gawk`, `bzip2`, `nasm`, `sed`, `intltool`, `graphviz`, `meson` and `ninja` (if needed)

In this order, add `C:\msys64\mingw<N>\bin`, `C:\msys64\` and `C:\msys64\usr\bin` in your PATH environement variable from Windows advanced settings. Binaries from the msys folder (not from mingw32/64) doesn't fully support Windows Path and thus, they are to be avoided.
*<N> is the version of MinGW32/64*

#### Visual Studio

Visual Studio must also be properly configured with addons. Under "Tools"->"Obtain tools and features", make sure that the following components are installed:
 - Tasks: Select Windows Universal Platform development, Desktop C++ Development, .NET Development
 
For Visual Studio 2022 :
 - Under "Installation details". Go to "Desktop C++ Development" and add "SDK Windows 8.1 and SDK UCRT"
 - Individual component: Windows 8.1 SDK

## Build

A build with the Ninja generator (`-G "Ninja"`) or the Ninja Multi-Config generator (`-G "Ninja Multi-Config"`) is preferred for speed-up build times.

There are two slightly different ways to build, depending on whether you use a multi-config CMake generator (`Xcode`, `Ninja Multi-Config`, Visual Studio) or not (`Unix Makefiles`, `Ninja`). In both cases, the steps are:
 1. Configure the project
 2. Build the project

The steps described here are the base for building, but a few specifics behaviors for each platform are good to know and are described in the next subsections.

#### Multi-Config generator

In the case of a multi-config generator, you will:
 1. Execute CMake to configure the project by giving the preset you want to build (you can get the list of presets available with the `cmake --list-presets` command), the build directory where you want the build to take place, the generator you want to use and eventually some additional options:
 `cmake --preset=<PRESET> -B <BUILD DIRECTORY> -G <MULTI-CONFIG GENERATOR> <SOME OPTIONS>`
 2. Build the SDK:
 `cmake --build <BUILD DIRECTORY> --config <CONFIG>`
 or
 `cmake --build <BUILD DIRECTORY> --config <CONFIG> --parallel <number of jobs>` (which is faster).

The `<CONFIG>` is one of these:
 - `RelWithDebInfo` to build in release mode keeping the debug information (this is the recommended configuration to use).
 - `Release` to build in release mode without the debug information.
 - `Debug` to ease the debugging.
 - On Android, use `ASAN` to make a build linking with the Android Adress Sanitizer (https://github.com/google/sanitizers/wiki/AddressSanitizerOnAndroid).

### Unique config generator

In this case, you will need to choose the build configuration in the first step, the configuration one. For that, you need to use the `CMAKE_BUILD_TYPE=<CONFIG>` option, and then you do not need to pass the `--config <CONFIG>` option in the build step. This will give:
 1. Configure the project:
 `cmake --preset=<PRESET> -B <BUILD DIRECTORY> -G <GENERATOR> -DCMAKE_BUILD_TYPE=<CONFIG> <SOME OPTIONS>`
 2. Build the SDK:
 `cmake --build <BUILD DIRECTORY>`


### iOS

Requirement:
 - Xcode >= 15
 
Sample configuration for arm64 targeting iPhone only in Debug mode:

`cmake --preset=ios-sdk -G Xcode -B build-ios -DLINPHONESDK_IOS_PLATFORM=Iphone -DLINPHONESDK_IOS_ARCHS="arm64"`
`cmake --build build-ios --config Debug`

You can also build using the 'Ninja' or 'Unix makefiles' generators:

`cmake --preset=ios-sdk -G Ninja -B build-ios`
`cmake --build build-ios`

If the generator is not specified, Xcode will be used by default.




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
To know what docker image to pull, first check [.gitlab-ci-files/android/builds.yml](https://gitlab.linphone.org/BC/public/linphone-sdk/-/blob/master/.gitlab-ci-files/android/builds.yml)
Currently we are using `bc-dev-android-r27` image name.

You'll find the associated tag in [.gitlab-ci-files/.docker-images.yml](https://gitlab.linphone.org/BC/public/linphone-sdk/-/blob/master/.gitlab-ci-files/.docker-images.yml) (for Android R27 image it is currently `20240717_update_ndk`).

Replace `<name>` and `<tag>` in the commands below by the value you found.

---

```bash
docker login gitlab.linphone.org:4567/bc/public/linphone-sdk
docker pull gitlab.linphone.org:4567/bc/public/linphone-sdk/<name>:<tag>
```

Load the build environment:

```bash
cd <linphone-sdk-source>
docker run -it --volume=$PWD:/home/bc/linphone-sdk gitlab.linphone.org:4567/bc/public/linphone-sdk/<name>:<tag> /bin/bash -i
```

Next command lines must be typed in the docker shell:

```bash
# Configure the build
cmake --preset=android-sdk -B build-android -DLINPHONESDK_ANDROID_ARCHS=arm64 -DCMAKE_BUILD_TYPE=RelWithDebInfo <extra-variable-definitions>

# Build
cmake --build build-android --parallel <number of jobs>

# Quit build environment
exit
```

The freshly built SDK is located in the `build-android/` directory.

### MacOS

Requirement:
 - Xcode >= 15

Configure the project with:

`cmake --preset=mac-sdk -B build-mac -G Xcode`

And build it with:

`cmake --build build-mac --config RelWithDebInfo`


### Windows

Requirement:
 - Microsoft Visual Studio  >= 2022

Configure the project with:

`cmake --preset=windows-sdk -B build-windows`

As for all other platforms, you can then build with:

`cmake --build build-windows --config RelWithDebInfo`

However it may be convenient to build from Visual Studio, which you can do:
 - open `linphone-sdk.sln` with Visual Studio
 - select the configuration you want to build
 - use `Build solution` to build.

### Windows UWP and Stores

Requirement:
 - Microsoft Visual Studio  >= 2022

You can use linphone-sdk in your Windows UWP app with the UWP mode.
Win32 application can use the Windows Store mode in order to be publishable in Windows Stores.
The Windows Bridge mode is built by using the `windows-store-sdk` preset instead of the `windows-sdk` one.
The UWP mode is built by using the `uwp-sdk` preset instead of the `windows-sdk` one. If `-DLINPHONESDK_UWP_ARCHS` is not used, x86 and x64 will be build.

Then, you can inject directly all your libraries that you need or package the SDK in a Nuget package.

### NuGet packaging

The Linphone SDK is available as a `.nuget` package:
- LinphoneSDK nuget for .NET applications MaUI for Android and iOS.
- LinphoneSDK.windows for Windows OS comprising binaries for Win32, Win32Store and UWP targets

See the [`cmake/NuGet`](cmake/NuGet/README.md) folder for build instructions.

### Python wrapper & wheel packaging

To build the python wrapper, you first need to install `cython` tool using pip. If you want to build the documentation, also install `pdoc` tool. 
Finally install `wheel` tool to be able to build a .whl package.
Minimal python version to build wrapper is `3.10`

Then build the SDK with `-DENABLE_PYTHON_WRAPPER=ON` and optionally `-DENABLE_DOC=ON`.
To generate the wheel package, use `wheel` target after `install` target (required for RPATH to be properly set for shared libs inside the wheel).

For example:

	cmake --preset=default -B build-python -DENABLE_PYTHON_WRAPPER=ON -DENABLE_DOC=ON
	cmake --build build-python --target install
	cmake --build build-python --target wheel

## Upgrading your SDK

Simply re-invoking `cmake --build <BUILD DIRECTORY>` in your build directory should update your SDK. If compilation fails, you may need to rebuilding everything by erasing your build directory and restarting your build as explained above.

## Customizing the build

The SDK compilation can be customized by passing `-D` options to CMake when configuring the project. If you know the options you want to use, just pass them to CMake.

Otherwise, you can use the `cmake-gui` or `ccmake` commands to configure all the available options interactively.

The following options control the cpu architectures built for a target platform:
- `LINPHONESDK_ANDROID_ARCHS`: A comma-separated list of the architectures for which you want to build the Android Linphone SDK for.
- `LINPHONESDK_IOS_ARCHS`: Same as `LINPHONESDK_ANDROID_ARCHS` but for an iOS build.
- `LINPHONESDK_IOS_BASE_URL`: The base of the URL that will be used to download the zip file of the SDK when building for iOS.
- `LINPHONESDK_MACOS_ARCHS`: Same as `LINPHONESDK_ANDROID_ARCHS` but for a MacOS build. Currently supported : x86_64, arm64
- `LINPHONESDK_MACOS_BASE_URL`: The base of the URL that will be used to download the zip file of the SDK when building for macos.
- `LINPHONESDK_WINDOWS_ARCHS`: Same as `LINPHONESDK_ANDROID_ARCHS` but for a windows build.
- `LINPHONESDK_WINDOWS_BASE_URL`: The base of the URL that will be used to download the zip file of the SDK when building for windows.
- `LINPHONESDK_UWP_ARCHS`: Same as `LINPHONESDK_ANDROID_ARCHS` but for an UWP build and Nuget.

These ON/OFF options control the enablement of important features of the SDK, which have an effect on the size of produced size object code:
- `ENABLE_VIDEO`: enablement of video call features.
- `ENABLE_ADVANCED_IM`: enablement of group chat and secure IM features
- `ENABLE_ZRTP`: enablement of ZRTP ciphering
- `ENABLE_DB_STORAGE`: enablement of database storage for IM.
- `ENABLE_VCARD`: enablement of Vcard features.
- `ENABLE_MKV`: enablement of Matroska video file reader/writer.
- `ENABLE_LDAP`: enablement of OpenLDAP.

### Android permissions

The SDK declares a bunch of permission it may or may not need depending on your usage.

If you need to remove one or more of them, you can do it in your own app's AndroidManifest.xml file like this:
```
<uses-permission android:name="android.permission.FOREGROUND_SERVICE_DATA_SYNC" tools:node="remove" />
```

<a name="licensing-gpl-third-parties-versus-non-gpl-third-parties"></a>
## Licensing: GPL third parties versus non GPL third parties

This SDK can be generated in 2 flavors:

* GPL third parties enabled means that the Linphone SDK includes GPL third parties like FFmpeg. If you choose this flavor, your final application **must comply with GPL in any case**.

* NO GPL third parties means that the Linphone SDK will only use non GPL code except for the code from Belledonne Communications. If you choose this flavor, your final application is **still subject to the GPL except if you have a [commercial license from Belledonne Communications](http://www.belledonne-communications.com/products.html)**. **This is the default mode.**

To generate the a SDK with GPL third parties, use the `-DENABLE_GPL_THIRD_PARTIES=YES` option when configuring the project (you can set "NO" to disable explicitly).

## Note regarding third party components subject to license

The Linphone SDK can be compiled with third parties code that are subject to patent license, especially: AMR, SILK and H264 codecs.
To build a SDK with any of these features you need to enable the `ENABLE_NON_FREE_FEATURES` option (**disabled by default**).
Before embedding these features in your final application, **make sure to have the right to do so**.

For more information, please visit [our dedicated wiki page](https://wiki.linphone.org/xwiki/wiki/public/view/Linphone/Third%20party%20components%20/)



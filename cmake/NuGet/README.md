# LinphoneSDK as a NuGet package

## Build instructions

### LinphoneSDK.Dotnet

> ⚠ The NuGet package with dotnet can be built from any operating system which have dotnet binary installed.

To build a NuGet package for any platform, you will first need to build (or download) the SDK for these platforms, and make sure you have the C# wrapper (which can be generated by any of these builds).

```sh
mkdir build-nuget
cd build-nuget
cmake .. -DLINPHONESDK_CSHARP_WRAPPER_PATH="..." -DLINPHONESDK_WIN32_DESKTOP_PATH="..." -DLINPHONESDK_WIN64_DESKTOP_PATH="..." -DLINPHONESDK_OSX_FRAMEWORK_PATH="..." -DLINPHONESDK_LINUX_SO_PATH="..."  -DLINPHONESDK_SHARE_PATH="..." -DLINPHONESDK_ANDROID_AAR_PATH="..." -DLINPHONESDK_IOS_XCFRAMEWORK_PATH="..." -DLINPHONESDK_VERSION="..."
cmake --build .
```

If you run cmake from the root of the `linphone-sdk` repository, then you will need to pass `-DLINPHONESDK_PACKAGER="Nuget"` as well.

The generation does not build anything, so it can in theory be consumed by any recent .NET framework.

Only LINPHONESDK_CSHARP_WRAPPER_PATH and LINPHONESDK_VERSION are mandatory, then you can choose what libraries you want in the nuget.

LINPHONESDK_SHARE_PATH is mandatory for any desktop nuget (Eg. Windows, Linux, MacOSX), it is the same share folder generated on any desktop build.

Dotnet keeps many caches that you need to clear to force a new packaging, the command to do it is :
```sh
dotnet nuget locals all --clear
```

### LinphoneSDK.Windows

> ⚠ The NuGet package for Windows development with .NET can only be built from a Microsoft Windows operating system.

You can package 3 kinds of binaries : win32, uwp and win32 with Windows Store Compatibility.

- win32: this is the win32 version of linphone-sdk without any restrictions. The framework is 'win'.
- uwp : this is a uwp x64 version of linphone-sdk. You will not be able to use OpenH264 and Lime X3DH. The framework is 'uap10.0'.
- win32 Windows Store : this is the win32 version of linphone-sdk with the Windows Store Compatibility enabled for Windows Bridge. The framework is 'netcore'.

In an another build folder (like build-nuget), run ```cmake .. ``` with these options :
- (Needed) `--DLINPHONESDK_BUILD_TYPE=Packager -DLINPHONESDK_PACKAGER=Nuget`
- (Optional) `-DLINPHONESDK_DESKTOP_ZIP_PATH=<path of the zip file containing the Desktop binaries>` (eg. C:/projects/desktop-uwp/linphone-sdk/buildx86/linphone-sdk) 
- (Optional) `-DLINPHONESDK_UWP_ZIP_PATH=<path of the zip file containing the UWP binaries>` (eg. C:/projects/desktop-uwp/linphone-sdk/builduwp/linphone-sdk)
- (Optional) `-DLINPHONESDK_WINDOWSSTORE_ZIP_PATH=<path of the zip file containing the Desktop binaries with Store compatibility enabled>` (eg. C:/projects/desktop-uwp/linphone-sdk/buildx86_store/linphone-sdk)

Build the Package:

```sh
cmake ..
cmake --build . --target ALL_BUILD --config=RelWithDebInfo
```

The nuget package will be in `linphone-sdk/packages`
The generated package can keep the same file name between generations on the same git version.
Visual studio keeps several caches and you need to delete it to force a new packaging, there is an option to do it.
The first cache folder can be found in your system path at `<User>/.nuget/packages/linphonesdk`

## Pitfalls knowledge base

Intended for maintainers working on packaging the LinphoneSDK for .NET

### System.DllNotFoundException

```
System.DllNotFoundException
  Message=linphone assembly:<unknown assembly> type:<unknown type> member:(null)
```

You are probably missing the native library for your architecture.

E.g. if you are building for an Android emulator, make sure the `.aar` includes the `x86_64` architecture. (You can use `ANDROID_ARCHS=arm64,armv7,x86_64,x86` in the CI pipeline)

### I keep running into the same error even after changing something and rebuilding the package

**Beware [the `nuget` cache].**

If you are manually tinkering with a local build, you probably re-exported the package **with the same version**. In that case, nuget will reuse the cached version of the package. Make sure you either clear the cache or use a different version name when exporting the package. (You can use the `-Suffix` option of the `nuget` cli for that.)

[the `nuget` cache]: https://docs.microsoft.com/en-us/nuget/Consume-Packages/managing-the-global-packages-and-cache-folders

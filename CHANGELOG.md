# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

# Preamble

This changelog file keeps track of changes made to the linphone-sdk project, which is a only an unbrella project
that bundles liblinphone and its dependencies as git submodules.
Please refer to CHANGELOG.md files of submodules (mainly: *liblinphone*, *mediastreamer2*, *ortp*) for the actual
changes made to these components.


## [5.4.0] 2025-01-14

### Added
- Swift Package for MacOS and iOS.

### Changed
- Mac/iOS:
  * Ready for Xcode 16
  * Update macOS deployment target version to 10.15 and iOS deployment target version to iOS13
  * Removed deprecated bitcode compilation for iOS and Mac targets.
- Windows: requires Visual Studio 2022.
- Android: requires NDK 27.
- MbedTLS upgraded to 3.6.

### Removed
- Cocoapods (no longer supported)


## [5.3.0] 2023-12-18

### Added
- dav1d and libaom dependencies for AV-1 codec

### Changed
- Improved CMake build scripts, so that they are faster and integrate smoothly with IDEs.
- Update ZLib to 1.2.13
- Enum relocations dictionnary is now automatically computed, causing an API change in C++, Swift & Java wrappers!
- Update macOS deployment target version to 10.14 and iOS deployment target version to iOS12
- Android 14 ready
- Update to mbdetls 3.3
- TLS Client certificate request authentication callback removed (due to mbedtls update).
  Application using TLS client certificate must provide it before any TLS connexion needing it.

# Removed
- Jazzy dependency (used to generate Swift documentation), replaced by docc (xcodebuild docbuild)


## [5.2.0] - 2022-11-14

### Added
- Upgrade of zxing-cpp library in order to support QR-code generation.
- OpenH264 compilation for UWP.
- Support for ARM64 GNU/Linux, XCode 14, Android NDK r25.
- New dependencies: Libyuv (image scaling/conversion) and liboqs (quantum-safe cryptographic algorithms).

### Changed
- Updated firebase messaging dependency for Android, now requires at least 23.0.6 (BoM 30.2.0)


## [5.1.0] 2022-02-14

### Added
- Support for Android NDK r23b
- Android device rotation is now handled by linphone's PlatformHelper, apps no longer need to do it.

### Changed
- Link Android build to OpenGLESv3 instead of OpenGLESv2


## [5.0.0] - 2021-07-08

### Added
- Native Windows 10 UWP platform support.
- OpenLDAP dependency - desktop platforms only.

## Changed
- Windows build now relying on MSYS2 (for parts not built with MSVC)


## [4.5.0] - 2021-03-29

### Added
- Windows Store compatibility

### Changed
- Android minimum compatibility has been increased to Android 6 (Android SDK 23).
- Most of changes are documented in liblinphone and mediastreamer2's CHANGELOG.md files.
- Python >= 3.6 is now required for build (was python 2.7 previously)

### Changed
- Update libvpx to 1.9.0

## [4.4.0] - 2020-06-16

### Added
- Windows Store compatibility

### Changed
- liblinphone is now placed into the *liblinphone* directory, naturally.
- Android min SDK version updated from 21 to 23.


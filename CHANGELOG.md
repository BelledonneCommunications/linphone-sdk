# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

# Preamble

This changelog file keeps track of changes made to the linphone-sdk project, which is a only an unbrella project
that bundles liblinphone and its dependencies as git submodules.
Please refer to CHANGELOG.md files of submodules (mainly: *liblinphone*, *mediastreamer2*, *ortp*) for the actual
changes made to these components.


## [Unreleased]
### Added
- Add stateful PRACK sending [RFC3262](https://www.rfc-editor.org/info/rfc3262)
- MainDb: An API to check tables and schemas in order to know if an upgrade is needed. Forbid to start the core if upgrade is not done.
- Swift package flavor built with the G729 codec, published to a separate repository for licensees.
- [sip]/call_accept_handles_replaces property so that is possible to disable the automatic termination of a replaces-pointed call upon accepting the INVITE.


### Changed
- Refactor ICE and STUN/TURN from C to C++
- Handle multiple transports for TURN at the same time during ICE processing
- Upgrade of Opus source code to version 1.5.2 to activate OSCE (deep PLC and LACE/noLACE).
- Updated Sqlite3 dependency to 3.54.0
- Updated decaf dependency to 1.0.5
- Updated SRTP library dependency to 2.8.1

### Fixed
- During attended transfer, allow using the remote AOR uri rather than contact URI to forge the Refer-to header, when [sip]/refer_to_uses_aor is set to 1
- Unexpected video direction change from recvonly to sendrecv, resulting in camera activation, when an application accepts a video stream as recvonly and subsequently receives a ICE re-INVITE.

## [5.5.39] - 2026-10-26

### Fixed
- Remove provisioning configuration keys parsing if generation of documentation has not been requested.
- Fix potential crash when accessing messages and eventlogs after a core restart

### Security
- Fix vulnerabilities in RTCP PSFB RPSI, RTCP XR and RTP extension header remapping.
- Bounds validation on RTCP RTPFB packets and RTP extension headers.


## [5.5.35] 2026-10-19

### Changed
- Update to MBedTls v3.6.7

### Fixed
- Fix potential crash when accessing messages and eventlogs after a core restart

## [5.5.34] 2026-10-15

### Fixed
- Use the information in the NOTIFY message XML body to set the subject of a conference instead of looking at the INVITE message's Subject header

## [5.5.33] 2026-10-13

### Fixed
- Reinitialize encryption contexts when the encryption keys or the origin address put down in the SDP changes

## [5.5.32] 2026-10-08

### Fixed
- Fix bug on iOS that could lead the SDK to wrongfully identify a cellular network as a Wifi network
- CSharp Wrapper: Separate getter/setter deprecation and set minimal version on C# for property obsolete features (8.0).

## [5.5.30] 2026-10-05

### Fixed
- MKV: Add checks on buffer to avoid Out-of-bounds reads.
- VP8: Fix a cast to avoid losing data.

## [5.5.29] 2026-09-30

### Fixed
- iOS : fix possible audio route bug when switching audio device during call

## [5.5.0] 2026-05-25

### Added
- [RNNoise](https://github.com/xiph/rnnoise) dependency for noise suppression feature.
- GCC 15 support
- New Swift wrapper test suite.
- [HIDAPI](https://github.com/libusb/hidapi) dependency for Jabra headsets support.

### Changes
- MAJOR CHANGE: most git submodules have been removed and incororated within the linphone-sdk git project.
  Only external dependencies remain as git submodule within the linphone-sdk project.
  The switch to release/5.5 branch requires manual removal of former submodules, because git is not able
  to this task automatically. For that reason, doing a fresh git clone of the linphone-sdk is the recommended solution.
- Upgrade of WebRTC source code (used for Acoustic Echo Cancellation and Voice Activity Dection)

### Removed
- Dependency to WebRTC's repository, replaced by a copy of need source code files for AEC3.
- WebRTC AECM for (Acoustic Echo Canceller for Mobile) is removed, superseed by AEC3
- ISAC and iLBC audio codecs.

### Fixed
- Decoding errors with msopenh264 plugin.

## [5.4.92] 2026-02-20

### Changed
- Ephemeral API: one-time activation/deactivation from new functions:
 * linphone_chat_params_activate_ephemeral()/linphone_chat_params_deactivate_ephemeral()
 * linphone_chat_room_activate_ephemeral()/linphone_chat_room_deactivate_ephemeral()


## [5.4.0] 2025-03-11

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
- Swap values between LINPHONE_VIDEO_DISPLAY_NONE and LINPHONE_VIDEO_DISPLAY_AUTO to match with NULL value.
- LINPHONE_VIDEO_DISPLAY_AUTO is defined as default for native ids.

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


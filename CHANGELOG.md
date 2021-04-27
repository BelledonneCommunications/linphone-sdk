# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

# Preamble

This changelog file keeps track of changes made to the linphone-sdk project, which is a only an unbrella project
that bundles liblinphone and its dependencies as git submodules.
Please refer to CHANGELOG.md files of submodules (mainly: *liblinphone*, *mediastreamer2*, *ortp*) for the actual
changes made to these components.__

## [Unreleased]

### Added
- MSYS2 support
- OpenLDAP on Desktop

## [4.5.0] 2021-03-29

### Added
- Windows Store compatibility

### Changed
- Android minimum compatibility has been increased to Android 6 (Android SDK 23).
- Most of changes are documented in liblinphone and mediastreamer2's CHANGELOG.md files.
- Python >= 3.6 is now required for build (was python 2.7 previously)

### Changed
- Update libvpx to 1.9.0

## [4.4.0] 2020-06-16

### Added
- Windows Store compatibility

### Changed
- liblinphone is now placed into the *liblinphone* directory, naturally.
- Android min SDK version updated from 21 to 23.


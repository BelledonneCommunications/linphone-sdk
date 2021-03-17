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
- Windows Store compatibility
- Audio routes API for Android & iOS
- Core now automatically handles certain tasks for Android & iOS (see liblinphone changelog)

### Changed
- Update libvpx to 1.9.0

## [4.4.0] 2020-06-16

### Added
- Windows Store compatibility

### Changed
- liblinphone is now placed into the *liblinphone* directory, naturally.
- Android min SDK version updated from 21 to 23.


# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## Unreleased

### Added
- Acoustic Echo Canceller using AEC3 from a recent version of WebRTC, applied also for mobile

### Changed
- Upgrade of the source code from WebRTC, commit ca932cbe20e11eca8f29b48ba4e4967a9c4d556a on main branch: [Update WebRTC code version (2024-10-18T04:06:16)](https://webrtc.googlesource.com/src/+/ca932cbe20e11eca8f29b48ba4e4967a9c4d556a)
- Modification of the filters MatchedFilterCore_AVX2 and MatchedFilterCore_AccumulatedError_AVX2 for MSVC build
- Updated Voice Activity Detection to the new version of WebRTC with minor changes
- The delay of the echo path cannot be set to the AEC, because it is continuously estimated by AEC3. 

### Removed
- Initial AEC for echo cancellation is removed and replaced by AEC3
- AECM for mobile echo cancellation is removed and replaced by AEC3
- Removed ISAC and iLBC audio codecs

## [1.1.1] 2017-07-20

### Fixed
- Bug fixes

## [1.0.0] 2016-08-17

### Added
- Adding iLBC build support for autotools.
- iLBC codec.

### Changed
- MSFactory - changing init to use MSFactory.
# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).
## [5.3.0] - XXXX-XX-XX
### Changed
- Lime manager keeps an open connexion to the db
- Update timer is managed internally: keep track of last successful update for each local user
- Db schema updated from version 0.0.1 to 0.1.0


## [5.2.0] - 2022-11-08
### Added
- API to compute peer status for a list of peer device

## [4.5.0] - 2021-03-29
### Added
- Ability to force an active session to stale
### Changed
- use bctoolbox C++ API for RNG to improve error detections
### Fixed
- The Changelog!

## [4.4.0] - 2020-05-11
### Fixed
- minor bug fix in build scripts
- flexibility on server error code

## [4.3.0] - 2019-11-14
### Added
- Java API
- user republishing on X3DH server
- support multithreading
- improved peer device management

### Fixed
- Key's Id generation

### Removed
- PHP test server

### Changed
- X3DH register user in one command


## [0.1.0] - 2018-12-06
### Added
- rpm generation
- C API

### Fixed
- X3DH message parsing bug on user not found

## [0.0.1] - 2018-08-07
### Added
- Initial release.






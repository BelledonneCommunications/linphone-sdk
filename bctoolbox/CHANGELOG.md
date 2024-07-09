# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]
### Removed
- Crypto: remove polarssl support


## [5.3.68] - 2024-07-10

## Added
- Use locale encoding from setlocale() on Windows and fallback to CP_APC if not found.

## [5.2.0] - 2022-11-14

## Added
- Crypto: add support for post-quantum algorithms.
- Crash handler and backtrace logger for Windows.

## Changed
- minor changes.


## [5.1.0] - 2022-02-14

### Added
- new vfs read2/write2 functions without offset parameter, to easy mapping with standard libc functions.

### Changed
- optimized C++ logging macros.


## [5.0.0] - 2021-07-08

### Added
- Tester API: add API to set maximum number of failed tests.

### Fixed
- Few bugfixes (see git log)


## [4.5.0] - 2021-03-29

### Added
- Encrypted VFS API.

### Changed
- Callback for client certificate query is now passed with the full list of subjects.
- miscellaneous small things, see git commits for details.


## [4.4.0] - 2020-6-09

### Changed
- Version number now follows linphone-sdk's versionning.

### Removed
- Polarssl 1.2 support.


## [0.6.0] - 2017-07-20
### Added
- Add API to escape/unescape strings (SIP, VCARD).


## [0.5.1] - 2017-02-22
### Added
- "const char * to void *" map feature

### Fixed
- security bugfix: TLS session could be successfully established whereas the common
  name did not match the server name.

## [0.2.0] - 2016-08-08
### Added
- Creating a Virtual File System bctbx_vfs allowing direct file access and I/Os.
- integrate OS abstraction layer, list API, logging API from oRTP
- integrate getaddrinfo() abstraction api, in order to provide consistent getaddrinfo() support on all platforms.


## [0.0.1] - 2016-01-01
### Added
- Initial release.

### Changed

### Removed




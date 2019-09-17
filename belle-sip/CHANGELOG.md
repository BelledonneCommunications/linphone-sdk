# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.7.0] - 2019-09-06

### Added
- RFC3262 (100 rel) support.
- Use maddr for http uris.

### Changed
- License is now GPLv3.

### Removed
- Some useless files.

## [1.6.0] - 2017-02-23


## [1.5.0] - 2016-08-08

### Added
- support for zlib in body handling.

### Changed
- move general purpose and encryption related functions to bctoolbox, which becomes a mandatory dependency.
- mbedTLS support through bctoolbox.
- SUBSCRIBE/NOTIFY dialog improvements: can be created upon reception of NOTIFY (as requested by RFC),
  automatic deletion when dialog expires.

### Fixed
- retransmit 200Ok of a reINVITE (was formely only done for initial INVITE)



## [1.4.0] - 2015-03-11

### Added
- Accept, Content-Disposition and Supported headers.
- Support of display names of type (token LWS)* instead of just token.
- Support for absolute URIs.

### Fixed
- various things.

## [1.3.0] - 2014-09-18

### Added
- First mature version of belle-sip.



# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed
 - SIP & HTTP parser no longer uses antlr, but belr.

## [5.2.0] - 2022-11-07

## Added
- Support for authentication headers with multiple challenges.

## Fixed
- various weaknesses within multipart decoding process.
- endless UDP transaction in a specific scenario.
- crash on iOS 16 because of kCFStreamNetworkServiceTypeVoIP that no longer works.
- crashes.


## [5.1.0] - 2022-02-14

### Fixed
- Multipart boundaries are random and used-once.
- Avoid using V4MAPPED formatted IP addresses in CONNECT http requests, because
  http proxies may not be always IPv6-capable.

## [5.0.0] - 2021-07-08

### Added
- Added SDP API for Capability Negociation headers (RFC5939).

### Changed
- SDP parser no longer uses antlr, but belr.

### Fixed
- Erroneous closing of file description 0, causing unexpected behaviors.
- Crash when receiving invalid from header.


## [4.5.0] - 2021-03-29

### Changed
- Use DNSService framework on iOS, to workaround the local network permission request triggered on iOS >= 14

### Fixed
- Fix routing according to RFC3263: uri with a port number shall not be resolved with SRV.
- See git commits for full list of fixes.


## [4.4.0] - 2020-06-09

### Added
- New C++ facilities:
  * HybridObject implements toString() using typeid() operator.
  * Mainloop's do_later() and create_timer() with lambdas.
- Session-Expires header

### Changed
- version number is aligned for all components of linphone-sdk, for simplicity.
- Via header parsing is more tolerant, following recommandation of https://tools.ietf.org/html/rfc5118#section-4.5

### Fixed
- bad channel selection in some specific cases.
- an undefined behavior and a memory leak when timer is added with an interval greater than MAX_INT.
- accumulation of data in main loop's control file descriptor (for use with threads).

## [1.7.0] - 2019-09-06

### Added
- RFC3262 (100 rel) support.
- Use maddr for http uris.

### Changed
- License is now GPLv3.

### Removed
- Some useless files.

## [1.6.0] - 2017-02-23


## [1.5.0] - 2016-08-16

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



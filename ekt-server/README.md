# ekt-server

## Description

A liblinphone plugin to manage EKT key dispatching accross a server-managed conference.
The EKT (Encrypted Key Transport) is defined in RFC8870 - Encrypted Key Transport for DTLS and Secure RTP.
The EKT key secures the transport of SRTP inner keys and thus takes the role of the conference's master key,
allowing each participant to encode and decode sRTP streams.

In linphone-sdk implementation, the EKT key is not setup through DTLS, but dispatched with end-to-end encryption
accross all participants thanks to a SIP PUBLISH/SUBSCRIBE/NOTIFY event package system.

## Usage

This plugin is automatically by liblinphone and connects with Conference objects, in order to provide EKT events
to conference's participants.
It is used by Flexisip conference server.

## Compilation

This module cannot be compiled standalone. It is as a part of the whole linphone-sdk project.
It is compiled when -DENABLE_EKT_SERVER_PLUGIN=ON cmake option is provided.
It is built over liblinphone's C++ wrapper.

## RFCs

[RFC8870 - Encrypted Key Transport for DTLS and Secure RTP](https://datatracker.ietf.org/doc/html/rfc8870)
[RFC8723 - Double Encryption Procedures for the Secure Real-Time Transport Protocol (SRTP)](https://datatracker.ietf.org/doc/rfc8723/)

## Licence

GNU Affeiro GPLv3.

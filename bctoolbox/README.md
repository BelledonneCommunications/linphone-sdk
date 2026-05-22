[![pipeline status](https://gitlab.linphone.org/BC/public/bctoolbox/badges/master/pipeline.svg)](https://gitlab.linphone.org/BC/public/bctoolbox/commits/master)

BcToolbox
=========

Utilities library used by Belledonne Communications softwares like belle-sip, mediastreamer2 and liblinphone.


Depends
-------

- **mbedtls[1]**: implementation of TLS interface of BcToolbox. For backward
  compatibility, support of mbedtlsv2 is also provided.
- **bcunit[2]** for unitary test tools. (optional)
- **openssl[3]** alternate TLS and crypto implementation. (optional)


To compile
----------

	cmake . -DCMAKE_INSTALL_PREFIX=<install prefix> -DCMAKE_PREFIX_PATH=<search prefix>
	
	make
	make install


To make an rpm package
----------------------

	cmake . -DCMAKE_INSTALL_PREFIX=/usr -DCPACK_GENERATOR="RPM"
	
	make package 


Options
-------

- `CMAKE_INSTALL_PREFIX=<string>`: install prefix.
- `CMAKE_PREFIX_PATH=<string>`: search path prefix for dependencies e.g. mbedtls.
- `ENABLE_MBEDTLS=NO`: do not look for mbedtls.
- `ENABLE_OPENSSL=NO`: do not look for openssl.
- `ENABLE_STRICT=NO`: do not build with strict compilator flags e.g. `-Wall -Werror`.
- `ENABLE_UNIT_TESTS=NO`: do not build testing binaries.
- `ENABLE_TESTS_COMPONENT=NO`: do not build libbctoolbox-tester.


Notes
-----

For backward compatibility with distributions not having the required 2.8.12 cmake version, an automake/autoconf build system is also available.
It is maintained as a best effort and then should be used only in last resort.


TLS CryptoProvider smoke validation
-----------------------------------

The current `crypto_provider` setting is propagated through the SIP/TLS configuration path:

`LinphoneCore -> SAL -> belle_tls_crypto_config -> bctbx_ssl_config`

Supported values are:

- `crypto_provider=mbedtls`
- `crypto_provider=openssl`

Important limitation:

- Runtime provider selection is limited to providers compiled into the current build.
- In the current bctoolbox tree, OpenSSL and MbedTLS are not built together in one TLS build.
- An explicit invalid or unavailable provider must fail clearly; it must not silently fall back.
- If `crypto_provider` is omitted or empty, the current compiled backend remains in use.

Expected logs:

- `[CryptoProvider] requested: ...`
- `[CryptoProvider] selected: MbedTlsCryptoProvider` or `OpenSslCryptoProvider`
- `[CryptoProvider] unavailable: ... is not compiled in this build`
- `[CryptoProvider] unknown provider: ...`
- `[CryptoProvider] no provider configured, using compiled backend: ...`
- `[TLS] handshake started`
- `[TLS] handshake completed`
- `[TLS] handshake duration: ... ms`
- `[TLS] certificate verification result: success/failure`

Manual smoke cases:

Case A: `crypto_provider=mbedtls`

- Expected: selected provider log shows `MbedTlsCryptoProvider`.
- Expected: TLS handshake starts.
- Expected: TLS handshake completes or fails with the same behavior as before the abstraction.
- Expected: certificate verification behavior is unchanged.

Case B: `crypto_provider=openssl`

- Expected: selected provider log shows `OpenSslCryptoProvider` when OpenSSL is the compiled backend.
- Expected: if OpenSSL is not compiled, the error clearly says the provider is unavailable.
- Expected: no silent fallback occurs.

Case C: `crypto_provider=invalid-provider`

- Expected: clear unknown provider error.
- Expected: no crash.
- Expected: TLS does not silently fall back unless an explicit fallback mechanism is added in the future.

Case D: `crypto_provider` omitted or empty

- Expected: current build-selected backend behavior remains unchanged.
- Expected: the compiled backend is logged when possible.
- Expected: no certificate verification behavior change.


Note for packagers
------------------

Our CMake scripts may automatically add some paths into research paths of generated binaries.
To ensure that the installed binaries are striped of any rpath, use `-DCMAKE_SKIP_INSTALL_RPATH=ON`
while you invoke cmake.

--------------------

- [1] <https://github.com/ARMmbed/mbedtls.git>
- [2] git://git.linphone.org/bctoolbox.git or <http://www.linphone.org/releases/sources/bctoolbox/>
- [3] <https://github.com/openssl/openssl.git>

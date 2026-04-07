[![pipeline status](https://gitlab.linphone.org/BC/private/postquantumcryptoengine/badges/master/pipeline.svg)](https://gitlab.linphone.org/BC/private/postquantumcryptoengine/commits/master)

PostQuantumCryptoEngine
=======================

Extension to the bctoolbox lib providing Post Quantum Cryptography.
Provides:
- ML-KEM 512, 768 and 1024
- Kyber 512, 768 and 1024 (deprecated)
- HQC 128, 192 and 256 (NIST round 3 version)
- X25519 and X448 in KEM version
and a way to combine two or more of theses.

Licensing
---------

Copyright © Belledonne Communications

PostQuantumCryptoEngine is dual licensed, and is available either :

 - under a [GNU/GPLv3 license](https://www.gnu.org/licenses/gpl-3.0.en.html), for free (open source). Please make sure that you understand and agree with the terms of this license before using it (see LICENSE.txt file for details).

 - under a proprietary license, for a fee, to be used in closed source applications. Contact [Belledonne Communications](https://www.linphone.org/contact) for any question about costs and services.


Depends
-------

- **liboqs[1]**: implementation of a collection of Post Quantum algorithms
- **bctoolbox[2]**

Tests
-----
When build with the enable unit tests option, this module produces a pqcrypto-tester executable.

It tests against published KAT patterns for ML-KEM, Kyber and HQC and self-generated KAT patterns for combined KEM.

--------------------

- [1] <https://gitlab.linphone.org/BC/public/external/liboqs>
- [2] <https://gitlab.linphone.org/BC/public/linphone-sdk/-/tree/master/bctoolbox>


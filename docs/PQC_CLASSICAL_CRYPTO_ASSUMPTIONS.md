# PQC Classical Crypto Assumptions (Current State)

## 1. Purpose
This document records current classical cryptography assumptions in Linphone SDK code paths that are relevant for future PQC/hybrid integration planning.

It is documentation only. It does **not** indicate that real PQC is implemented, and it does **not** claim current RSA/DH behavior is broken.

## 2. What Is Currently Classical-Only
Current assumptions include:
- Self-signed certificate generation uses fixed RSA-3072 key generation.
- DHM context creation supports only classical MODP groups currently exposed by this API surface.
- Key agreement discovery and context types are still primarily classical/ECC oriented in these specific paths.

These are acceptable for current classical TLS behavior and interoperability.

## 3. RSA-3072 Self-Signed Certificate Assumptions
### Current behavior
- mbedTLS backend self-signed generation hardcodes RSA key generation with size 3072.
- OpenSSL backend self-signed generation hardcodes `EVP_RSA_gen(3072)`.

### Code locations
- `bctoolbox/src/crypto/mbedtls.c` - `bctbx_x509_certificate_generate_selfsigned()`
- `bctoolbox/src/crypto/openssl.c` - `bctbx_x509_certificate_generate_selfsigned()`

### Why acceptable now
- Produces a strong classical key size and stable behavior for existing deployments.
- Keeps certificate generation deterministic and backend-simple for current usage.

### Future limitation
- Not algorithm-agile for future PQC/hybrid certificate generation strategies.
- No direct way in this path to select non-RSA or hybrid identity key material once needed.

## 4. DH/Key-Agreement Assumptions
### Current behavior
- DHM context structure and setup are tied to classical DH group sizing assumptions.
- DHM creation code currently supports classical `BCTBX_DHM_2048` / `BCTBX_DHM_3072` style group handling in these paths.
- Key-agreement listing in the referenced API path reflects currently exposed classical/ECC baseline behavior.

### Code locations
- `bctoolbox/include/bctoolbox/crypto.h` - `bctbx_DHMContext_t`
- `bctoolbox/src/crypto/mbedtls.c` - `bctbx_CreateDHMContext()`
- `bctoolbox/src/crypto/openssl.c` - `bctbx_CreateDHMContext()`
- `bctoolbox/src/crypto/crypto.c` - `bctbx_key_agreement_algo_list()`

### Why acceptable now
- Matches current classical interoperability needs.
- Keeps existing DH/ECDH key exchange behavior stable.

### Future limitation
- DHM API shape is not yet a generic container for future PQC/hybrid KEM artifact handling.
- Future hybrid migration may require algorithm-agile negotiation/exchange abstractions beyond current DHM fields and assumptions.

## 5. What Is Intentionally Not Changed Now
This documentation effort intentionally does **not**:
- Refactor RSA self-signed generation behavior.
- Redesign DH/key agreement APIs.
- Change provider selection logic.
- Introduce real PQC or hybrid key exchange implementation.
- Change runtime behavior.

## 6. Revisit Points When Real PQC Provider/Library Arrives
When the production PQC provider/library is integrated, revisit:
- Self-signed certificate generation API to support algorithm selection (classical, PQC, hybrid).
- Key agreement API/data model to support non-classical key exchange artifacts and metadata.
- Capability reporting so upper layers can clearly detect and enforce available group/KEM options.
- TLS policy/config wiring to avoid implicit fallback to classical-only paths when hybrid is required.

## 7. Recommended Future Cleanup Direction
1. Add algorithm-agile parameters/options for self-signed identity generation (without breaking legacy defaults).
2. Introduce explicit capability descriptors for key exchange modes (classical, hybrid, PQC).
3. Extend or complement DHM-oriented APIs with a generic key-exchange abstraction suitable for KEM/hybrid payloads.
4. Add focused tests asserting configured algorithm/group/KEM intent is actually applied (no silent fallback).

## 8. Scope Reminder
Current RSA/DH assumptions are valid for today’s classical TLS operation. They are documented here as **known future technical debt** for PQC/hybrid transition planning.

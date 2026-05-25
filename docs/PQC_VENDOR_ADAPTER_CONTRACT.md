# PQC Vendor Adapter Contract

## Purpose
`FuturePqcCryptoProvider` is a forward-looking integration point for a future external PQC backend.

It provides a stable selection/configuration surface now (`crypto_provider=future-pqc`) while keeping current TLS behavior safe and explicit.

## Current Implementation
- Provider name is registered: `future-pqc`
- Provider class name is registered: `FuturePqcCryptoProvider`
- The provider is intentionally marked unavailable because no external PQC backend is integrated yet
- Selection of `future-pqc` is handled explicitly with controlled logs and controlled fallback/failure behavior

## Intentionally Not Implemented Yet
- No real post-quantum algorithm implementation
- No external PQC backend loading/linking
- No KEM/signature runtime dispatch to a PQC library
- No new external dependency

## Expected External Backend API
A future backend integration is expected to expose an adapter surface equivalent to:

- `init(config, out_ctx)`
- `free(ctx)`
- `get_capabilities(ctx, out_caps)`
- `generate_keypair(ctx, algorithm_id, out_public_key, out_private_key)`
- `key_exchange(...)` or KEM-style:
  - `kem_encapsulate(ctx, algorithm_id, peer_public_key, out_ciphertext, out_shared_secret)`
  - `kem_decapsulate(ctx, algorithm_id, private_key, ciphertext, out_shared_secret)`
- `sign(ctx, algorithm_id, private_key, message, out_signature)`
- `verify(ctx, algorithm_id, public_key, message, signature)`

## Memory Ownership Rules
- Backend-owned context (`ctx`) is created by `init` and released by `free`
- Caller-owned input buffers must never be retained beyond call scope unless explicitly copied
- Any heap output allocated by backend must have a matching backend free routine or use caller-provided buffers

## Buffer Ownership Rules
- Input buffers are immutable from the backend perspective
- Output buffers must clearly define one of:
  - caller-allocated size + out-length, or
  - backend-allocated pointer + length + backend-free function
- API must return required-size information when caller buffer is too small

## Error Code Expectations
- Deterministic, documented error codes
- Distinct codes for:
  - invalid arguments
  - unsupported algorithm/capability
  - backend not initialized
  - buffer too small
  - cryptographic operation failure
  - internal backend failure
- No silent success on partial failure

## Thread-Safety Expectations
- Thread-safety model must be explicit:
  - either fully thread-safe context operations, or
  - context-per-thread requirement
- No hidden global mutable state without synchronization

## Version and Capability Checks
- Backend must expose a version identifier
- Capability query must include supported algorithms, key sizes, signature sizes, and ciphertext sizes
- Integration layer must validate required capabilities at startup before enabling provider selection

## Handling Large Artifacts
The adapter must support large PQC artifacts, including:
- public keys
- private keys
- signatures
- ciphertexts
- certificate-like blobs

Requirements:
- no fixed small stack buffers
- size-aware allocation and bounds checks
- explicit maximum size constraints and validation

## Fallback Behavior
When `crypto_provider=future-pqc` is selected and backend is unavailable:
- log request of `FuturePqcCryptoProvider`
- log external backend unavailability
- if `fallback_provider` is configured (for example `mbedtls` or `openssl`) and available, use it and log selected fallback provider
- if `fallback_provider` is `none` (or disabled), fail cleanly with explicit TLS failure log

## Plug-in Steps for a Future Real Backend
1. Implement the adapter API listed above
2. Add capability and version checks during provider initialization
3. Switch `FuturePqcCryptoProvider` availability from placeholder to runtime-backed availability
4. Keep controlled fallback/failure behavior unchanged for missing or incompatible backends
5. Extend tests with backend-present and backend-mismatch cases

#!/usr/bin/env bash
export OQS_ROOT="$HOME/oqs"
export OPENSSL_MODULES="$OQS_ROOT/oqs-provider/lib/ossl-modules"
export LD_LIBRARY_PATH="$OQS_ROOT/liboqs/lib:$LD_LIBRARY_PATH"

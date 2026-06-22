/*
 * Copyright (c) 2010-2025 Belledonne Communications SARL.
 *
 * This file is part of Liblinphone
 * (see https://gitlab.linphone.org/BC/public/liblinphone).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

package org.linphone.core.tools.security;

import android.content.Context;
import android.security.KeyChain;

import java.security.KeyStore;
import java.security.PrivateKey;
import java.security.Signature;
import java.util.concurrent.ConcurrentHashMap;

import org.linphone.core.AuthInfo;
import org.linphone.core.tools.Log;

public final class KeystoreSigner {
    private static final String ANDROID_KEY_STORE = "AndroidKeyStore";

    private static final int LINPHONE_KEYSIGN_UNDEFINED = 0;
    private static final int LINPHONE_KEYSIGN_RSA_PSS = 1;
    private static final int LINPHONE_KEYSIGN_RSA_PKCS1_V15 = 2;
    private static final int LINPHONE_KEYSIGN_ECDSA = 3;

    private static final int LINPHONE_HASH_UNDEFINED = 0;
    private static final int LINPHONE_HASH_SHA256 = 1;
    private static final int LINPHONE_HASH_SHA384 = 2;
    private static final int LINPHONE_HASH_SHA512 = 3;

    private static final byte[] DIGEST_INFO_SHA256 = {
        0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, (byte) 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01,
        0x05, 0x00, 0x04, 0x20
    };
    private static final byte[] DIGEST_INFO_SHA384 = {
        0x30, 0x41, 0x30, 0x0d, 0x06, 0x09, 0x60, (byte) 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02,
        0x05, 0x00, 0x04, 0x30
    };
    private static final byte[] DIGEST_INFO_SHA512 = {
        0x30, 0x51, 0x30, 0x0d, 0x06, 0x09, 0x60, (byte) 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03,
        0x05, 0x00, 0x04, 0x40
    };

    private static Context sContext;

    private static final ConcurrentHashMap<String, PrivateKey> sPrivateKeys =
        new ConcurrentHashMap<String, PrivateKey>();

    private KeystoreSigner() {}

    public static void setContext(Context context) {
        Context applicationContext = context != null ? context.getApplicationContext() : null;
        if (applicationContext != sContext) {
            sPrivateKeys.clear();
        }
        sContext = applicationContext;
    }

    public static void clearCachedKeys() {
        sPrivateKeys.clear();
    }

    public static void setKeyRef(AuthInfo authInfo, String alias) {
        if (authInfo == null) {
            Log.e("[Keystore Signer] Missing auth info, can't set key reference [" + alias + "]");
            return;
        }
        nativeSetAuthInfoKeyRef(authInfo.getNativePointer(), alias);
    }

    private static native void nativeSetAuthInfoKeyRef(long authInfoPtr, String alias);

    public static byte[] sign(String alias, int signAlgo, int hashAlgo, byte[] hash) {
        if (alias == null || hash == null) {
            Log.e("[Keystore Signer] Missing alias or hash");
            return null;
        }

        if (signAlgo == LINPHONE_KEYSIGN_RSA_PSS) {
            Log.e(
                "[Keystore Signer] RSA-PSS is not supported by the Android key store on a pre-computed hash, " +
                    "alias [" + alias + "] (use an ECDSA key for TLS 1.3)"
            );
            return null;
        }

        if (signAlgo != LINPHONE_KEYSIGN_ECDSA && signAlgo != LINPHONE_KEYSIGN_RSA_PKCS1_V15) {
            Log.e("[Keystore Signer] Unsupported signature algorithm [" + signAlgo + "] for alias [" + alias + "]");
            return null;
        }

        PrivateKey cachedKey = sPrivateKeys.get(alias);
        if (cachedKey != null) {
            byte[] signature = doSign(cachedKey, alias, signAlgo, hashAlgo, hash);
            if (signature != null) {
                return signature;
            }
            Log.w("[Keystore Signer] Cached private key for alias [" + alias + "] failed, resolving it again");
            sPrivateKeys.remove(alias, cachedKey);
        }

        PrivateKey privateKey = resolvePrivateKey(alias);
        if (privateKey == null) {
            Log.e("[Keystore Signer] No private key found for alias [" + alias + "]");
            return null;
        }

        byte[] signature = doSign(privateKey, alias, signAlgo, hashAlgo, hash);
        if (signature != null) {
            sPrivateKeys.put(alias, privateKey);
        }
        return signature;
    }

    private static byte[] doSign(PrivateKey privateKey, String alias, int signAlgo, int hashAlgo, byte[] hash) {
        try {
            if (signAlgo == LINPHONE_KEYSIGN_ECDSA) {
                Signature signature = Signature.getInstance("NONEwithECDSA");
                signature.initSign(privateKey);
                signature.update(hash);
                return signature.sign();
            }

            byte[] digestInfo = getDigestInfoPrefix(hashAlgo);
            if (digestInfo == null) {
                Log.e("[Keystore Signer] Unsupported hash [" + hashAlgo + "] for RSA key [" + alias + "]");
                return null;
            }
            byte[] toSign = new byte[digestInfo.length + hash.length];
            System.arraycopy(digestInfo, 0, toSign, 0, digestInfo.length);
            System.arraycopy(hash, 0, toSign, digestInfo.length, hash.length);

            Signature signature = Signature.getInstance("NONEwithRSA");
            signature.initSign(privateKey);
            signature.update(toSign);
            return signature.sign();
        } catch (Throwable e) {
            Log.e("[Keystore Signer] Signature failed for alias [" + alias + "]: " + e);
            return null;
        }
    }

    private static PrivateKey resolvePrivateKey(String alias) {
        if (sContext != null) {
            try {
                PrivateKey key = KeyChain.getPrivateKey(sContext, alias);
                if (key != null) {
                    return key;
                }
                Log.w("[Keystore Signer] No KeyChain private key for alias [" + alias + "], trying AndroidKeyStore");
            } catch (Throwable e) {
                Log.w("[Keystore Signer] KeyChain lookup failed for alias [" + alias + "]: " + e);
            }
        } else {
            Log.w("[Keystore Signer] No context set, KeyChain unavailable, trying AndroidKeyStore");
        }

        try {
            KeyStore keyStore = KeyStore.getInstance(ANDROID_KEY_STORE);
            keyStore.load(null);
            return (PrivateKey) keyStore.getKey(alias, null);
        } catch (Throwable e) {
            Log.e("[Keystore Signer] AndroidKeyStore lookup failed for alias [" + alias + "]: " + e);
            return null;
        }
    }

    private static byte[] getDigestInfoPrefix(int hashAlgo) {
        switch (hashAlgo) {
            case LINPHONE_HASH_SHA256:
                return DIGEST_INFO_SHA256;
            case LINPHONE_HASH_SHA384:
                return DIGEST_INFO_SHA384;
            case LINPHONE_HASH_SHA512:
                return DIGEST_INFO_SHA512;
            case LINPHONE_HASH_UNDEFINED:
            default:
                return null;
        }
    }
}

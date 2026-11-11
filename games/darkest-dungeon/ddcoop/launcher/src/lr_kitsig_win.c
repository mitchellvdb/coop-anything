/* Windows: ECDSA P-256 / SHA-256 check of a kit manifest with CNG (launcher/tests/test_lr_kitsig_win.c). */
#include "lr_kitsig_win.h"

#include <windows.h>
#include <bcrypt.h>
#include <string.h>

#include "lr_hashes.h"
#include "lr_mods.h"

#ifndef BCRYPT_ECDSA_PUBLIC_P256_MAGIC
#define BCRYPT_ECDSA_PUBLIC_P256_MAGIC 0x31534345
#endif

static int unhex64(const char *s, uint8_t out[64]) {
    if (!s || strlen(s) != 128) return 0;
    for (int i = 0; i < 64; i++) {
        int v = 0;
        for (int k = 0; k < 2; k++) {
            char c = s[2 * i + k];
            int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
            if (d < 0) return 0;
            v = v << 4 | d;
        }
        out[i] = (uint8_t)v;
    }
    return 1;
}

int lr_kit_verify_p256(const char *public_hex, const uint8_t *msg, size_t n, const uint8_t signature[64]) {
    struct {
        BCRYPT_ECCKEY_BLOB head;
        uint8_t xy[64];
    } blob;
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_KEY_HANDLE key = NULL;
    uint8_t hash[32];
    lr_sha256_ctx c;
    int ok = 0;
    if (!msg || !signature || !unhex64(public_hex, blob.xy)) return 0;
    blob.head.dwMagic = BCRYPT_ECDSA_PUBLIC_P256_MAGIC;
    blob.head.cbKey = 32;
    lr_sha256_init(&c);
    lr_sha256_update(&c, msg, n);
    lr_sha256_final(&c, hash);
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_ECDSA_P256_ALGORITHM, NULL, 0) == 0 &&
        BCryptImportKeyPair(alg, NULL, BCRYPT_ECCPUBLIC_BLOB, &key, (PUCHAR)&blob, sizeof blob, 0) == 0)
        ok = BCryptVerifySignature(key, NULL, hash, sizeof hash, (PUCHAR)signature, 64, 0) == 0;
    if (key) BCryptDestroyKey(key);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}

int lr_kit_verify_owner(const uint8_t *msg, size_t n, const uint8_t signature[64]) {
    return lr_kit_verify_p256(LR_KIT_PUBKEY, msg, n, signature);
}

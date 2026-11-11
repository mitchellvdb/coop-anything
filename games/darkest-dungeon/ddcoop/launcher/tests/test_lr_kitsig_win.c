/* Windows CNG check of kit signatures against a manifest signed by tools/sign_kit.ps1 with a throwaway key. */
#include "lr_kit.h"
#include "lr_kitsig_win.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #x);                                                         \
            failures++;                                                                                                \
        }                                                                                                              \
    } while (0)

static const char TEST_KEY[] = "2f8f5217b2d5d4599cfe8e5c41ffc7919b3daf77141846942952d7d471ec40461317b4bd32661b86dc38ac65040f35198c0858305384b1707d001f9380ae90c8";
static const char VECTOR[] =
    "DDCoop kit 1\n"
    "version=0.5.0-test77\n"
    "serial=77\n"
    "kit=7fc50367\n"
    "file=DDCoop.exe 14 9197dbc13fca2f5d6ad6516ef9822295749bd4bd2a1fe41d0d90adf4bda0792b\n"
    "file=SDL2.dll 15 1d624b6ae4e9ce0ce174f872a5d05f6491b0cb388d1ba053f87f902708320a23\n"
    "file=ddcoop-runtime.dll 13 7fc5036745ec73b6169495830cf3ef3b58127addf27fcc0d0b8fe87b4de5808c\n"
    "signature=fac195a323e4e6698aa955ae16f1f996c723ce7357e3df145e061a38a91d0f5ed372dfb134aef0cf61ce025a8b715ce5455f47f14961d1df75c3be6eb9f9f044\n";

int main(void) {
    lr_kit_manifest m;
    CHECK(lr_kit_parse((const uint8_t *)VECTOR, strlen(VECTOR), &m));
    CHECK(lr_kit_verify_p256(TEST_KEY, (const uint8_t *)VECTOR, m.signed_len, m.signature));
    /* every changed byte of the signed text, the signature or the key fails */
    for (size_t i = 0; i < m.signed_len; i++) {
        char copy[sizeof VECTOR];
        memcpy(copy, VECTOR, sizeof VECTOR);
        copy[i] ^= 1;
        CHECK(!lr_kit_verify_p256(TEST_KEY, (const uint8_t *)copy, m.signed_len, m.signature));
    }
    for (int i = 0; i < 64; i++) {
        uint8_t sig[64];
        memcpy(sig, m.signature, 64);
        sig[i] ^= 0x10;
        CHECK(!lr_kit_verify_p256(TEST_KEY, (const uint8_t *)VECTOR, m.signed_len, sig));
    }
    CHECK(!lr_kit_verify_p256(TEST_KEY, (const uint8_t *)VECTOR, m.signed_len - 1, m.signature));
    char other[sizeof TEST_KEY];
    memcpy(other, TEST_KEY, sizeof TEST_KEY);
    other[5] = other[5] == '0' ? '1' : '0';
    CHECK(!lr_kit_verify_p256(other, (const uint8_t *)VECTOR, m.signed_len, m.signature));
    CHECK(!lr_kit_verify_p256("short", (const uint8_t *)VECTOR, m.signed_len, m.signature));
    /* the launcher's own key is not the test key: a test-signed kit is never accepted */
    CHECK(!lr_kit_verify_owner((const uint8_t *)VECTOR, m.signed_len, m.signature));
    puts("Kit signature (CNG ECDSA P-256): signer's vector valid; every signed byte, signature byte, length, other and malformed keys and the owner key refuse");
    printf(failures ? "test_lr_kitsig_win: %d FAILURES\n" : "test_lr_kitsig_win: all passed\n", failures);
    return failures != 0;
}

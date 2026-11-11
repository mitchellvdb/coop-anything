/* Kit manifests (lr_kit.c): the format tools/sign_kit.ps1 writes, every malformed variant refused, serials,
 * file checks, bundle checks with a fake signature callback, every truncation and 100,000 mutations.
 * The real ECDSA check is tested on Windows by test_lr_kitsig_win.c. */
#include "lr_kit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #x);                                                         \
            failures++;                                                                                                \
        }                                                                                                              \
    } while (0)

/* Written by tools/sign_kit.ps1 with a throwaway test key for the files "launcher bytes", "bootstrap bytes"
 * and "runtime bytes". */
static const char VECTOR[] =
    "DDCoop kit 1\n"
    "version=0.5.0-test77\n"
    "serial=77\n"
    "kit=7fc50367\n"
    "file=DDCoop.exe 14 9197dbc13fca2f5d6ad6516ef9822295749bd4bd2a1fe41d0d90adf4bda0792b\n"
    "file=SDL2.dll 15 1d624b6ae4e9ce0ce174f872a5d05f6491b0cb388d1ba053f87f902708320a23\n"
    "file=ddcoop-runtime.dll 13 7fc5036745ec73b6169495830cf3ef3b58127addf27fcc0d0b8fe87b4de5808c\n"
    "signature=fac195a323e4e6698aa955ae16f1f996c723ce7357e3df145e061a38a91d0f5ed372dfb134aef0cf61ce025a8b715ce5455f47f14961d1df75c3be6eb9f9f044\n";
static const char *const FILES[3] = {"launcher bytes", "bootstrap bytes", "runtime bytes"};

static size_t signed_seen;
static int fake_ok, fake_calls;
static int fake_verify(const uint8_t *msg, size_t n, const uint8_t sig[64]) {
    (void)msg;
    fake_calls++;
    signed_seen = n;
    return fake_ok && sig[0] == 0xfa;
}

static void bundle(lr_kit_bundle *b, const char *manifest) {
    memset(b, 0, sizeof *b);
    b->manifest_len = strlen(manifest);
    memcpy(b->manifest, manifest, b->manifest_len);
    CHECK(lr_kit_parse(b->manifest, b->manifest_len, &b->m));
    for (int f = 0; f < 3; f++) {
        b->data[f] = malloc(strlen(FILES[f]));
        memcpy(b->data[f], FILES[f], strlen(FILES[f]));
        b->got[f] = (uint32_t)strlen(FILES[f]);
    }
}

static int parses(const char *text) {
    lr_kit_manifest m;
    return lr_kit_parse((const uint8_t *)text, strlen(text), &m);
}

static void replace_and_check(const char *from, const char *to) {
    char buf[2048];
    const char *at = strstr(VECTOR, from);
    CHECK(at != NULL);
    if (!at) return;
    snprintf(buf, sizeof buf, "%.*s%s%s", (int)(at - VECTOR), VECTOR, to, at + strlen(from));
    if (parses(buf)) printf("accepted: %s -> %s\n", from, to);
    CHECK(!parses(buf));
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    lr_kit_manifest m;
    CHECK(lr_kit_parse((const uint8_t *)VECTOR, strlen(VECTOR), &m));
    CHECK(!strcmp(m.version, "0.5.0-test77") && m.serial == 77 && m.kit == 0x7fc50367);
    CHECK(m.size[0] == 14 && m.size[1] == 15 && m.size[2] == 13 && m.signature[0] == 0xfa && m.signature[63] == 0x44);
    CHECK(m.signed_len == (size_t)(strstr(VECTOR, "signature=") - VECTOR));
    for (int f = 0; f < 3; f++) CHECK(lr_kit_file_ok(&m, f, (const uint8_t *)FILES[f], strlen(FILES[f])));
    CHECK(!lr_kit_file_ok(&m, 0, (const uint8_t *)"launcher bytez", 14));
    CHECK(!lr_kit_file_ok(&m, 0, (const uint8_t *)"launcher byte", 13));
    CHECK(!lr_kit_file_ok(&m, 3, (const uint8_t *)FILES[0], 14));
    char out[2048];
    size_t n = lr_kit_format(&m, 1, out, sizeof out);
    CHECK(n == strlen(VECTOR) && !memcmp(out, VECTOR, n));
    CHECK(lr_kit_format(&m, 0, out, sizeof out) == m.signed_len && !memcmp(out, VECTOR, m.signed_len));
    CHECK(!lr_kit_format(&m, 1, out, 100));

    CHECK(lr_kit_serial("0.5.0-test77") == 77 && lr_kit_serial("0.5.0-test1") == 1);
    CHECK(!lr_kit_serial("0.5.0-duo59") && !lr_kit_serial("dev") && !lr_kit_serial("0.5.0-test") &&
          !lr_kit_serial("0.5.0-test07") && !lr_kit_serial("0.5.0-test77a") && !lr_kit_serial("0.5.0-test1-test2"));

    /* every line, field and separator is exact */
    replace_and_check("DDCoop kit 1", "DDCoop kit 2");
    replace_and_check("version=0.5.0-test77", "version=0.5.0-duo59");
    replace_and_check("version=0.5.0-test77", "version=0.5.0-Test77");
    replace_and_check("serial=77", "serial=78");
    replace_and_check("serial=77", "serial=077");
    replace_and_check("kit=7fc50367", "kit=7FC50367");
    replace_and_check("kit=7fc50367", "kit=00000000");
    replace_and_check("kit=7fc50367", "kit=7fc5036");
    replace_and_check("file=DDCoop.exe 14", "file=DDCoop.exe 0");
    replace_and_check("file=DDCoop.exe 14", "file=DDCoop.exe 16777217");
    replace_and_check("file=DDCoop.exe 14", "file=DDCoop.exe  14");
    replace_and_check("file=DDCoop.exe", "file=ddcoop.exe");
    replace_and_check("file=SDL2.dll", "file=ddcoop-runtime.dll");
    replace_and_check("9197dbc1", "9197DBC1");
    replace_and_check("signature=fac1", "signature=fac");
    replace_and_check("\nsignature", "\r\nsignature");
    replace_and_check("044\n", "044");
    replace_and_check("044\n", "044\n\n");
    CHECK(!parses(""));
    for (size_t cut = 0; cut < strlen(VECTOR); cut++) CHECK(!lr_kit_parse((const uint8_t *)VECTOR, cut, &m));

    /* bundles: signature callback over exactly the signed part, every file, the kit id */
    lr_kit_bundle b;
    bundle(&b, VECTOR);
    fake_ok = 1;
    CHECK(lr_kit_bundle_ok(&b, fake_verify, 0) && signed_seen == b.m.signed_len);
    CHECK(lr_kit_bundle_ok(&b, fake_verify, 0x7fc50367) && !lr_kit_bundle_ok(&b, fake_verify, 0x719b6e74));
    fake_ok = 0;
    CHECK(!lr_kit_bundle_ok(&b, fake_verify, 0));
    fake_ok = 1;
    CHECK(!lr_kit_bundle_ok(&b, NULL, 0));
    b.data[2][0] ^= 1;
    CHECK(!lr_kit_bundle_ok(&b, fake_verify, 0));
    b.data[2][0] ^= 1;
    free(b.data[1]);
    b.data[1] = NULL;
    CHECK(!lr_kit_bundle_ok(&b, fake_verify, 0));
    lr_kit_bundle_free(&b);
    bundle(&b, VECTOR);
    b.m.serial = 78; /* the parsed copy must be the manifest's own */
    CHECK(!lr_kit_bundle_ok(&b, fake_verify, 0));
    lr_kit_bundle_free(&b);

    /* mutations: never a crash; anything accepted formats back to the same bytes */
    srand(9);
    long accepted = 0;
    for (long i = 0; i < 100000; i++) {
        char buf[2048];
        size_t len = strlen(VECTOR);
        memcpy(buf, VECTOR, len);
        for (int e = 0; e < 1 + rand() % 3; e++) {
            int kind = rand() % 3;
            if (kind == 0 && len) buf[rand() % len] = (char)(rand() % 128);
            else if (kind == 1 && len) len = (size_t)rand() % len;
            else if (len < sizeof buf) buf[len++] = (char)(rand() % 128);
        }
        if (lr_kit_parse((const uint8_t *)buf, len, &m)) {
            accepted++;
            CHECK(lr_kit_format(&m, 1, out, sizeof out) == len && !memcmp(out, buf, len));
        }
    }
    printf("Kit manifest: signer's format, exact fields, serials, file and bundle checks, every truncation, 100000 mutations (%ld accepted, all canonical)\n", accepted);
    printf(failures ? "test_lr_kit: %d FAILURES\n" : "test_lr_kit: all passed\n", failures);
    return failures != 0;
}

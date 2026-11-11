#include "lr_logpack.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int failures, calls;
#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            printf("FAIL %d: %s\n", __LINE__, #x);                                                                     \
            ++failures;                                                                                                \
        }                                                                                                              \
    } while (0)
static int visit(void *ctx, unsigned file, unsigned flags, uint64_t original, const uint8_t *p, size_t n) {
    (void)ctx;
    (void)file;
    (void)flags;
    (void)original;
    (void)p;
    (void)n;
    ++calls;
    return 1;
}
int main(int argc, char **argv) {
    lr_logpack p = {0};
    const char *text = "safe\npassword=synthetic-secret\nmore\n";
    CHECK(lr_logpack_add(&p, 1, 1, 100, (const uint8_t *)text, strlen(text)));
    CHECK(lr_logpack_add(&p, 8, 2, 0, NULL, 0));
    CHECK(lr_logpack_visit(p.data, p.len, visit, NULL) && calls == 2);
    CHECK(!memcmp(p.data + 24, "safe\n[removed]\nmore\n", 20));
    CHECK(!strstr((char *)text, "[removed]")); /* caller data was not changed */
    CHECK(!lr_logpack_add(&p, 1, 0, 0, NULL, 0));
    CHECK(!lr_logpack_add(&p, 13, 0, 0, NULL, 0));
    CHECK(!lr_logpack_add(&p, 7, 2, 1, NULL, 0));
    CHECK(!lr_logpack_add(&p, 7, 0, 1, (const uint8_t *)text, 2));
    for (size_t n = 0; n < p.len; ++n) {
        calls = 0;
        CHECK(!lr_logpack_visit(p.data, n, visit, NULL) && !calls);
    }
    uint8_t *bad = malloc(p.len + 1);
    memcpy(bad, p.data, p.len);
    bad[p.len] = 0;
    calls = 0;
    CHECK(!lr_logpack_visit(bad, p.len + 1, visit, NULL) && !calls);
    memcpy(bad, p.data, p.len);
    bad[8] = 255;
    CHECK(!lr_logpack_visit(bad, p.len, NULL, NULL));
    memcpy(bad, p.data, p.len);
    bad[7] = 1;
    CHECK(!lr_logpack_visit(bad, p.len, NULL, NULL));
    free(bad);
    lr_logzip z = {0};
    CHECK(!lr_logzip_add(&z, "../escape.log", (const uint8_t *)text, strlen(text)));
    CHECK(!lr_logzip_add(&z, "C:/escape.log", NULL, 0));
    CHECK(!lr_logzip_add(&z, "/absolute.log", NULL, 0));
    const char *clean = "safe\n[removed]\nmore\n";
    CHECK(lr_logzip_add(&z, "host/REPORT.txt", (const uint8_t *)clean, strlen(clean)));
    CHECK(!lr_logzip_add(&z, "host/REPORT.txt", NULL, 0));
    CHECK(lr_logzip_add(&z, "player-2/empty.log", NULL, 0));
    CHECK(lr_logzip_finish(&z));
    CHECK(!lr_logzip_finish(&z) && !lr_logzip_add(&z, "after.txt", NULL, 0));
    if (argc > 1) {
        FILE *f = fopen(argv[1], "wb");
        CHECK(f != NULL);
        if (f) {
            CHECK(fwrite(z.data, 1, z.len, f) == z.len);
            fclose(f);
        }
    }
    lr_logpack_free(&p);
    lr_logzip_free(&z);
    printf("Log packet/ZIP: fixed ids/duplicates/bounds/malformed packets before sink, secret redaction, bounded safe "
           "ZIP names/CRC: %s (%d failures)\n",
           failures ? "FAILED" : "PASS", failures);
    return failures != 0;
}

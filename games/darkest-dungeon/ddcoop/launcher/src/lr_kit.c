#include "lr_kit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lr_mods.h" /* lr_sha256_* */

const char *const lr_kit_file_names[LR_KIT_FILES] = {"DDCoop.exe", "SDL2.dll", "ddcoop-runtime.dll"};

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1; /* lowercase only: one spelling per manifest */
}

static int unhex(const char *s, size_t n, uint8_t *out) {
    for (size_t i = 0; i < n; i++) {
        int a = hexval(s[2 * i]), b = hexval(s[2 * i + 1]);
        if (a < 0 || b < 0) return 0;
        out[i] = (uint8_t)(a << 4 | b);
    }
    return 1;
}

static void tohex(const uint8_t *p, size_t n, char *out) {
    for (size_t i = 0; i < n; i++) snprintf(out + 2 * i, 3, "%02x", p[i]);
}

/* A decimal without sign or leading zero, below limit. */
static int number(const char *s, size_t n, uint32_t limit, uint32_t *out) {
    uint64_t v = 0;
    if (!n || n > 10 || (n > 1 && s[0] == '0')) return 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') return 0;
        v = v * 10 + (uint64_t)(s[i] - '0');
    }
    if (v > limit) return 0;
    *out = (uint32_t)v;
    return 1;
}

uint32_t lr_kit_serial(const char *version) {
    const char *t = strstr(version, "-test");
    uint32_t v = 0;
    if (!t || strstr(t + 5, "-test")) return 0;
    t += 5;
    return number(t, strlen(t), 1000000, &v) ? v : 0;
}

static int version_ok(const char *s, size_t n) {
    if (!n || n > LR_KIT_VERSION_MAX) return 0;
    for (size_t i = 0; i < n; i++)
        if (!((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= '0' && s[i] <= '9') || s[i] == '.' || s[i] == '-')) return 0;
    return 1;
}

int lr_kit_parse(const uint8_t *buf, size_t len, lr_kit_manifest *out) {
    const char *p = (const char *)buf, *end = p + len;
    const char *line[8];
    size_t n[8];
    memset(out, 0, sizeof *out);
    if (len == 0 || len > LR_KIT_MANIFEST_MAX || memchr(buf, 0, len)) return 0;
    for (int i = 0; i < 8; i++) {
        const char *nl = memchr(p, '\n', (size_t)(end - p));
        if (!nl) return 0;
        line[i] = p;
        n[i] = (size_t)(nl - p);
        if (memchr(p, '\r', n[i])) return 0;
        p = nl + 1;
    }
    if (p != end) return 0;
    if (n[0] != 12 || memcmp(line[0], "DDCoop kit 1", 12)) return 0;
    if (n[1] < 9 || memcmp(line[1], "version=", 8) || !version_ok(line[1] + 8, n[1] - 8)) return 0;
    memcpy(out->version, line[1] + 8, n[1] - 8);
    if (n[2] < 8 || memcmp(line[2], "serial=", 7) || !number(line[2] + 7, n[2] - 7, 1000000, &out->serial) ||
        !out->serial || out->serial != lr_kit_serial(out->version))
        return 0;
    {
        uint8_t k[4];
        if (n[3] != 12 || memcmp(line[3], "kit=", 4) || !unhex(line[3] + 4, 4, k)) return 0;
        out->kit = (uint32_t)k[0] << 24 | (uint32_t)k[1] << 16 | (uint32_t)k[2] << 8 | k[3];
        if (!out->kit) return 0;
    }
    for (int f = 0; f < LR_KIT_FILES; f++) {
        const char *s = line[4 + f], *e = s + n[4 + f];
        size_t name = strlen(lr_kit_file_names[f]);
        if ((size_t)(e - s) < 5 + name + 1 || memcmp(s, "file=", 5) || memcmp(s + 5, lr_kit_file_names[f], name) ||
            s[5 + name] != ' ')
            return 0;
        s += 5 + name + 1;
        const char *sp = memchr(s, ' ', (size_t)(e - s));
        if (!sp || !number(s, (size_t)(sp - s), LR_KIT_MAX_FILE, &out->size[f]) || !out->size[f]) return 0;
        if (e - (sp + 1) != 64 || !unhex(sp + 1, 32, out->sha256[f])) return 0;
    }
    if (n[7] != 10 + 128 || memcmp(line[7], "signature=", 10) || !unhex(line[7] + 10, 64, out->signature)) return 0;
    out->signed_len = (size_t)(line[7] - (const char *)buf);
    return 1;
}

size_t lr_kit_format(const lr_kit_manifest *m, int with_signature, char *out, size_t cap) {
    char sha[65], sig[129];
    size_t k = 0;
    int w = snprintf(out, cap, "DDCoop kit 1\nversion=%s\nserial=%u\nkit=%08x\n", m->version, m->serial, m->kit);
    if (w < 0 || (size_t)w >= cap) return 0;
    k = (size_t)w;
    for (int f = 0; f < LR_KIT_FILES; f++) {
        tohex(m->sha256[f], 32, sha);
        w = snprintf(out + k, cap - k, "file=%s %u %s\n", lr_kit_file_names[f], m->size[f], sha);
        if (w < 0 || (size_t)w >= cap - k) return 0;
        k += (size_t)w;
    }
    if (with_signature) {
        tohex(m->signature, 64, sig);
        w = snprintf(out + k, cap - k, "signature=%s\n", sig);
        if (w < 0 || (size_t)w >= cap - k) return 0;
        k += (size_t)w;
    }
    return k;
}

int lr_kit_file_ok(const lr_kit_manifest *m, int index, const uint8_t *data, size_t len) {
    lr_sha256_ctx c;
    uint8_t h[32];
    if (index < 0 || index >= LR_KIT_FILES || !data || len != m->size[index]) return 0;
    lr_sha256_init(&c);
    lr_sha256_update(&c, data, len);
    lr_sha256_final(&c, h);
    return memcmp(h, m->sha256[index], 32) == 0;
}

void lr_kit_bundle_free(lr_kit_bundle *b) {
    for (int f = 0; f < LR_KIT_FILES; f++) free(b->data[f]);
    memset(b, 0, sizeof *b);
}

int lr_kit_bundle_ok(const lr_kit_bundle *b, lr_kit_verify_fn verify, uint32_t want_kit) {
    lr_kit_manifest m;
    if (!verify || !lr_kit_parse(b->manifest, b->manifest_len, &m) || memcmp(&m, &b->m, sizeof m) != 0) return 0;
    if (want_kit && m.kit != want_kit) return 0;
    if (!verify(b->manifest, m.signed_len, m.signature)) return 0;
    for (int f = 0; f < LR_KIT_FILES; f++)
        if (!lr_kit_file_ok(&m, f, b->data[f], m.size[f])) return 0;
    return 1;
}

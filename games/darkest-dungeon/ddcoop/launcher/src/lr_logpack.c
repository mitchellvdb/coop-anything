#include "lr_logpack.h"
#include "lr_logs.h"
#include "lr_wire.h"
#include <stdlib.h>
#include <string.h>
const char *const lr_log_names[LR_LOG_FILES] = {"DDCoop-launcher.log",  "ddcoop-fw-host.log",   "ddcoop-fw-guest.log",
                                                "ddcoop-fw-guest2.log", "ddcoop-fw-guest3.log", "ddcoop-heroes.ini",
                                                "ddcoop-prepare.log",   "ddcoop-restore.log",   "app.log",
                                                "app.log.bak",          "steam_appid.txt",      "checks.txt",
                                                "report-info.txt"};
static void w16(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}
static void w32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; ++i)
        p[i] = (uint8_t)(v >> (i * 8));
}
static uint32_t r32(const uint8_t *p) {
    uint32_t v = 0;
    for (unsigned i = 0; i < 4; ++i)
        v |= (uint32_t)p[i] << (i * 8);
    return v;
}
static void w64(uint8_t *p, uint64_t v) {
    for (unsigned i = 0; i < 8; ++i)
        p[i] = (uint8_t)(v >> (i * 8));
}
static uint64_t r64(const uint8_t *p) {
    uint64_t v = 0;
    for (unsigned i = 0; i < 8; ++i)
        v |= (uint64_t)p[i] << (i * 8);
    return v;
}
static size_t limit(unsigned file) { return file >= 1 && file <= 4 ? LR_LOG_RUNTIME_MAX : LR_LOG_OTHER_MAX; }
void lr_logpack_free(lr_logpack *p) {
    free(p->data);
    memset(p, 0, sizeof *p);
}
int lr_logpack_add(lr_logpack *p, unsigned file, unsigned flags, uint64_t original, const uint8_t *data, size_t size) {
    if (file >= LR_LOG_FILES || flags > 3 || (flags & LR_LOG_MISSING && (size || original)) || (size && !data) ||
        size > limit(file) || original < size || p->count >= LR_LOG_FILES || (p->seen & (1u << file)))
        return 0;
    size_t start = p->len ? p->len : 8;
    if (start > LR_LOG_PACKET_MAX - 16 || size > LR_LOG_PACKET_MAX - start - 16)
        return 0;
    uint8_t *b = realloc(p->data, start + 16 + size);
    if (!b)
        return 0;
    p->data = b;
    if (!p->len) {
        memcpy(b, "DDLG\1\0\0\0", 8);
        p->len = 8;
    }
    uint8_t *h = b + start;
    memset(h, 0, 16);
    h[0] = (uint8_t)file;
    h[1] = (uint8_t)flags;
    w64(h + 8, original);
    if (size)
        memcpy(h + 16, data, size);
    size_t safe = lr_logs_redact(h + 16, size, NULL);
    w32(h + 4, (uint32_t)safe);
    p->len = start + 16 + safe;
    p->data[6] = (uint8_t)++p->count;
    p->seen |= 1u << file;
    return 1;
}
int lr_logpack_visit(const uint8_t *p, size_t n, lr_log_visit visit, void *ctx) {
    if (!p || n < 8 || n > LR_LOG_PACKET_MAX || memcmp(p, "DDLG\1\0", 6) || !p[6] || p[6] > LR_LOG_FILES || p[7])
        return 0;
    for (unsigned pass = 0; pass < (visit ? 2u : 1u); ++pass) {
        size_t pos = 8;
        unsigned seen = 0;
        for (unsigned i = 0; i < p[6]; ++i) {
            if (n - pos < 16)
                return 0;
            const uint8_t *h = p + pos;
            unsigned file = h[0], flags = h[1];
            size_t size = r32(h + 4);
            uint64_t original = r64(h + 8);
            pos += 16;
            if (file >= LR_LOG_FILES || flags > 3 || h[2] || h[3] || seen & (1u << file) || size > limit(file) ||
                size > n - pos || original < size || (flags & LR_LOG_MISSING && (size || original)))
                return 0;
            seen |= 1u << file;
            if (pass && !visit(ctx, file, flags, original, p + pos, size))
                return 0;
            pos += size;
        }
        if (pos != n)
            return 0;
    }
    return 1;
}
#define ZIP_MAX (4u * LR_LOG_PACKET_MAX + 65536u)
void lr_logzip_free(lr_logzip *z) {
    free(z->data);
    memset(z, 0, sizeof *z);
}
static int reserve(lr_logzip *z, size_t n) {
    if (z->len > ZIP_MAX || n > ZIP_MAX - z->len)
        return 0;
    size_t need = z->len + n;
    if (need <= z->capacity)
        return 1;
    size_t cap = z->capacity ? z->capacity : 4096;
    while (cap < need) {
        if (cap > ZIP_MAX / 2) {
            cap = ZIP_MAX;
            break;
        }
        cap *= 2;
    }
    uint8_t *b = realloc(z->data, cap);
    if (!b)
        return 0;
    z->data = b;
    z->capacity = cap;
    return 1;
}
static int name_ok(const char *s) {
    if (!s || !*s || strlen(s) >= 80 || s[0] == '/' || strstr(s, ".."))
        return 0;
    for (; *s; ++s)
        if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || strchr("._-/", *s)))
            return 0;
    return 1;
}
int lr_logzip_add(lr_logzip *z, const char *name, const uint8_t *data, size_t n) {
    if (z->finished || !name_ok(name) || (n && !data) || n > LR_LOG_PACKET_MAX || z->count >= 64)
        return 0;
    for (unsigned i = 0; i < z->count; ++i)
        if (!strcmp(z->entries[i].name, name))
            return 0;
    size_t m = strlen(name);
    if (!reserve(z, 30 + m + n))
        return 0;
    lr_zip_entry *e = &z->entries[z->count++];
    strcpy(e->name, name);
    e->offset = (uint32_t)z->len;
    e->size = (uint32_t)n;
    e->crc = lr_crc32(0, data, n);
    uint8_t *p = z->data + z->len;
    memset(p, 0, 30);
    w32(p, 0x04034b50);
    w16(p + 4, 20);
    w16(p + 12, 0x21);
    w32(p + 14, e->crc);
    w32(p + 18, e->size);
    w32(p + 22, e->size);
    w16(p + 26, (uint32_t)m);
    memcpy(p + 30, name, m);
    if (n)
        memcpy(p + 30 + m, data, n);
    z->len += 30 + m + n;
    return 1;
}
int lr_logzip_finish(lr_logzip *z) {
    if (z->finished || !z->count || z->count > 64)
        return 0;
    size_t start = z->len;
    for (unsigned i = 0; i < z->count; ++i) {
        lr_zip_entry *e = &z->entries[i];
        size_t m = strlen(e->name);
        if (!reserve(z, 46 + m))
            return 0;
        uint8_t *p = z->data + z->len;
        memset(p, 0, 46);
        w32(p, 0x02014b50);
        w16(p + 4, 20);
        w16(p + 6, 20);
        w16(p + 14, 0x21);
        w32(p + 16, e->crc);
        w32(p + 20, e->size);
        w32(p + 24, e->size);
        w16(p + 28, (uint32_t)m);
        w32(p + 42, e->offset);
        memcpy(p + 46, e->name, m);
        z->len += 46 + m;
    }
    size_t central = z->len - start;
    if (!reserve(z, 22))
        return 0;
    uint8_t *p = z->data + z->len;
    memset(p, 0, 22);
    w32(p, 0x06054b50);
    w16(p + 8, z->count);
    w16(p + 10, z->count);
    w32(p + 12, (uint32_t)central);
    w32(p + 16, (uint32_t)start);
    z->len += 22;
    z->finished = 1;
    return 1;
}

#include "lr_mods.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lr_files.h"

/* ---- SHA-256 (FIPS 180-4) ---- */

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha_block(lr_sha256_ctx *s, const uint8_t *b) {
    uint32_t w[64], a, bb, c, d, e, f, g, h;
    for (int i = 0; i < 16; i++)
        w[i] = (uint32_t)b[4 * i] << 24 | (uint32_t)b[4 * i + 1] << 16 | (uint32_t)b[4 * i + 2] << 8 | b[4 * i + 3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = s->h[0], bb = s->h[1], c = s->h[2], d = s->h[3], e = s->h[4], f = s->h[5], g = s->h[6], h = s->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
        uint32_t t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & bb) ^ (a & c) ^ (bb & c));
        h = g, g = f, f = e, e = d + t1, d = c, c = bb, bb = a, a = t1 + t2;
    }
    s->h[0] += a, s->h[1] += bb, s->h[2] += c, s->h[3] += d, s->h[4] += e, s->h[5] += f, s->h[6] += g, s->h[7] += h;
}

void lr_sha256_init(lr_sha256_ctx *s) {
    static const uint32_t iv[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memcpy(s->h, iv, sizeof iv);
    s->bytes = 0;
    s->used = 0;
}

void lr_sha256_update(lr_sha256_ctx *s, const uint8_t *p, size_t n) {
    s->bytes += n;
    while (n) {
        size_t k = 64 - s->used < n ? 64 - s->used : n;
        memcpy(s->block + s->used, p, k);
        s->used += k, p += k, n -= k;
        if (s->used == 64) {
            sha_block(s, s->block);
            s->used = 0;
        }
    }
}

void lr_sha256_final(lr_sha256_ctx *s, uint8_t out[32]) {
    uint64_t bits = s->bytes * 8;
    uint8_t pad = 0x80, zero = 0, len[8];
    lr_sha256_update(s, &pad, 1);
    while (s->used != 56) lr_sha256_update(s, &zero, 1);
    for (int i = 0; i < 8; i++) len[i] = (uint8_t)(bits >> (56 - 8 * i));
    lr_sha256_update(s, len, 8);
    for (int i = 0; i < 8; i++)
        out[4 * i] = (uint8_t)(s->h[i] >> 24), out[4 * i + 1] = (uint8_t)(s->h[i] >> 16),
        out[4 * i + 2] = (uint8_t)(s->h[i] >> 8), out[4 * i + 3] = (uint8_t)s->h[i];
}

/* ---- where a mod is ---- */

static int all_digits(const char *s) {
    if (!*s) return 0;
    for (; *s; s++)
        if (*s < '0' || *s > '9') return 0;
    return 1;
}

static int has_word(const char *source, const char *word) {
    char low[LR_UGC_SOURCE_MAX + 1];
    size_t i = 0;
    for (; source[i] && i < LR_UGC_SOURCE_MAX; i++) low[i] = (char)(source[i] >= 'A' && source[i] <= 'Z' ? source[i] + 32 : source[i]);
    low[i] = 0;
    return strstr(low, word) != NULL;
}

/* lr_join is a display/helper API that truncates. A compatibility fingerprint
 * must instead reject a path that cannot be represented completely. */
static int mod_join(char *out, size_t cap, const char *a, const char *b) {
    if (!cap || strlen(a) + strlen(b) + 2 > cap) {if (cap) out[0] = 0;return 0;}
    lr_join(out, cap, a, b);return 1;
}
static int workshop_root(const char *game_dir, char *out, size_t cap) {
    char a[LR_PATH_MAX], b[LR_PATH_MAX];
    return mod_join(a, sizeof a, game_dir, "..") && mod_join(b, sizeof b, a, "..") &&
           mod_join(a, sizeof a, b, "workshop") && mod_join(b, sizeof b, a, "content") &&
           mod_join(out, cap, b, "262060");
}

/* Reads <Tag>...</Tag> from a small XML file (project.xml); empty when absent. */
static void xml_value(const char *dir, const char *tag, char *out, size_t cap) {
    char path[LR_PATH_MAX], open[40], close[40];
    uint8_t *data;
    size_t len;
    out[0] = 0;
    if (!mod_join(path, sizeof path, dir, "project.xml")) return;
    if (!lr_read_file(path, 256 * 1024, &data, &len)) return;
    snprintf(open, sizeof open, "<%s>", tag);
    snprintf(close, sizeof close, "</%s>", tag);
    char *text = malloc(len + 1);
    if (text) {
        memcpy(text, data, len);
        text[len] = 0;
        char *a = strstr(text, open), *b = a ? strstr(a, close) : NULL;
        if (a && b) {
            a += strlen(open);
            while (a < b && (*a == ' ' || *a == '\t' || *a == '\r' || *a == '\n')) a++;
            while (b > a && (b[-1] == ' ' || b[-1] == '\t' || b[-1] == '\r' || b[-1] == '\n')) b--;
            /* The five XML entities become their characters; control characters become spaces. */
            static const char *const ent[][2] = {{"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&apos;", "'"}};
            size_t k = 0;
            while (a < b && k + 1 < cap) {
                size_t e = 0, step = 1;
                const char *put = NULL;
                for (; e < 5; e++)
                    if ((size_t)(b - a) >= strlen(ent[e][0]) && !strncmp(a, ent[e][0], strlen(ent[e][0]))) break;
                if (e < 5) put = ent[e][1], step = strlen(ent[e][0]);
                if (!put && ((unsigned char)*a & 0xc0) == 0xc0) { /* a whole UTF-8 character, or stop */
                    size_t need = ((unsigned char)*a & 0xe0) == 0xc0 ? 2 : ((unsigned char)*a & 0xf0) == 0xe0 ? 3 : 4;
                    if (need > (size_t)(b - a) || k + need >= cap) break;
                    memcpy(out + k, a, need);
                    k += need, a += need;
                    continue;
                }
                out[k++] = put ? *put : ((unsigned char)*a < 0x20 || *a == 0x7f ? ' ' : *a);
                a += step;
            }
            out[k] = 0;
        }
        free(text);
    }
    free(data);
}

typedef struct {
    const char *root;
    const char *id;
    char *dir;
    size_t cap;
    int found;
} find_ctx;

static int find_published(void *p, const char *name, int is_dir) {
    find_ctx *f = p;
    char dir[LR_PATH_MAX], value[32];
    if (!is_dir) return 1;
    if (!mod_join(dir, sizeof dir, f->root, name)) return 1;
    xml_value(dir, "PublishedFileId", value, sizeof value);
    if (strcmp(value, f->id) != 0 || strlen(dir) + 1 > f->cap) return 1;
    snprintf(f->dir, f->cap, "%s", dir);
    f->found = 1;
    return 0;
}

int lr_mods_locate(const char *game_dir, const lr_ugc *u, char *dir, size_t cap) {
    char roots[2][LR_PATH_MAX], path[LR_PATH_MAX];
    if (!lr_ugc_name_ok(u->name)) return 0;
    if (!workshop_root(game_dir, roots[0], sizeof roots[0]) ||
        !mod_join(roots[1], sizeof roots[1], game_dir, "mods")) return 0;
    /* Workshop items have numeric ids; a local mod has its folder name. The source, when the save gives
     * one, says which to try first. */
    int first = all_digits(u->name) ? 0 : 1;
    if (has_word(u->source, "steam") || has_word(u->source, "workshop")) first = 0;
    if (has_word(u->source, "local") || has_word(u->source, "mods")) first = 1;
    for (int k = 0; k < 2; k++) {
        int r = k ? 1 - first : first;
        if (r == 1 && !strcmp(u->name, "DDCoop")) continue; /* this launcher's own folder, not a game mod */
        if (!mod_join(path, sizeof path, roots[r], u->name)) return 0;
        if (lr_dir_exists(path)) {
            if (strlen(path) + 1 > cap) return 0;
            snprintf(dir, cap, "%s", path);
            return 1;
        }
    }
    /* A Workshop mod copied into mods\ under another folder name keeps its id in project.xml. */
    if (all_digits(u->name)) {
        find_ctx f = {roots[1], u->name, dir, cap, 0};
        if (lr_list_dir(roots[1], find_published, &f) < 0) return 0;
        if (f.found) return 1;
    }
    return 0;
}

/* ---- fingerprint ---- */

typedef struct {
    char **paths; /* relative, '/' separated */
    int count, cap;
    int ok;
} walk;

static int walk_dir(walk *w, const char *base, const char *rel, int depth);

typedef struct {
    walk *w;
    const char *base, *rel;
    int depth;
} walk_ctx;

static int walk_entry(void *p, const char *name, int is_dir) {
    walk_ctx *c = p;
    char rel[LR_PATH_MAX];
    int n = c->rel[0] ? snprintf(rel, sizeof rel, "%s/%s", c->rel, name) : snprintf(rel, sizeof rel, "%s", name);
    if (n <= 0 || (size_t)n >= sizeof rel - 1) return c->w->ok = 0;
    if (is_dir) return walk_dir(c->w, c->base, rel, c->depth + 1);
    if (c->w->count >= LR_MOD_MAX_FILES) return c->w->ok = 0;
    if (c->w->count == c->w->cap) {
        int cap = c->w->cap ? c->w->cap * 2 : 64;
        char **p2 = realloc(c->w->paths, (size_t)cap * sizeof *p2);
        if (!p2) return c->w->ok = 0;
        c->w->paths = p2;
        c->w->cap = cap;
    }
    if (!(c->w->paths[c->w->count] = malloc((size_t)n + 1))) return c->w->ok = 0;
    memcpy(c->w->paths[c->w->count++], rel, (size_t)n + 1);
    return 1;
}

static int native_path(char *out, size_t cap, const char *base, const char *rel) {
    if (!mod_join(out, cap, base, rel)) return 0;
#ifdef _WIN32
    for (char *s = out; *s; s++)
        if (*s == '/') *s = '\\';
#endif
    return 1;
}

static int walk_dir(walk *w, const char *base, const char *rel, int depth) {
    char dir[LR_PATH_MAX];
    if (depth > LR_MOD_MAX_DEPTH) return w->ok = 0;
    if (rel[0]) {if (!native_path(dir, sizeof dir, base, rel)) return w->ok = 0;}
    else {if (strlen(base) + 1 > sizeof dir) return w->ok = 0;snprintf(dir, sizeof dir, "%s", base);}
    walk_ctx c = {w, base, rel, depth};
    if (lr_list_dir(dir, walk_entry, &c) < 0) w->ok = 0;
    return w->ok;
}

static int by_path(const void *a, const void *b) { return strcmp(*(char *const *)a, *(char *const *)b); }

static int hash_piece(void *p, const uint8_t *data, size_t n) {
    lr_sha256_update(p, data, n);
    return 1;
}

int lr_mods_fingerprint(const char *dir, lr_mod_print *p) {
    walk w = {NULL, 0, 0, 1};
    p->found = 0;
    p->files = 0;
    p->bytes = 0;
    memset(p->sha256, 0, sizeof p->sha256);
    p->title[0] = 0;
    if (!lr_dir_exists(dir)) return 0;
    walk_dir(&w, dir, "", 0);
    if (w.ok) qsort(w.paths, (size_t)w.count, sizeof *w.paths, by_path);
    lr_sha256_ctx all;
    lr_sha256_init(&all);
    for (int i = 0; w.ok && i < w.count; i++) {
        char path[LR_PATH_MAX];
        uint8_t one[32], size[8];
        uint64_t n;
        lr_sha256_ctx file;
        lr_sha256_init(&file);
        if (!native_path(path, sizeof path, dir, w.paths[i])) {w.ok = 0;break;}
        if (!lr_read_stream(path, hash_piece, &file, &n)) {
            w.ok = 0;
            break;
        }
        lr_sha256_final(&file, one);
        for (int k = 0; k < 8; k++) size[k] = (uint8_t)(n >> (8 * k));
        lr_sha256_update(&all, (const uint8_t *)w.paths[i], strlen(w.paths[i]) + 1);
        lr_sha256_update(&all, size, 8);
        lr_sha256_update(&all, one, 32);
        p->files++;
        p->bytes += n;
    }
    for (int i = 0; i < w.count; i++) free(w.paths[i]);
    free(w.paths);
    if (!w.ok) {
        p->files = 0;
        p->bytes = 0;
        return 0;
    }
    lr_sha256_final(&all, p->sha256);
    xml_value(dir, "Title", p->title, sizeof p->title);
    p->found = 1;
    return 1;
}

void lr_mods_print(const char *game_dir, const lr_ugc *u, lr_mod_print *p) {
    char dir[LR_PATH_MAX];
    memset(p, 0, sizeof *p);
    p->id = *u;
    if (lr_mods_locate(game_dir, u, dir, sizeof dir)) lr_mods_fingerprint(dir, p);
}

/* ---- the LR_MODS body ---- */
/* Per entry: name length (1 byte), name, source length, source, title length, title, found (1 byte),
 * files (4 bytes), bytes (8 bytes), SHA-256 (32 bytes). Little endian. */

static int title_ok(const char *s, size_t n) {
    for (size_t i = 0; i < n; i++)
        if ((unsigned char)s[i] < 0x20 || s[i] == 0x7f) return 0;
    return 1;
}

static int source_ok(const char *s) {
    for (; *s; s++)
        if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || (*s >= '0' && *s <= '9') || *s == '_')) return 0;
    return 1;
}

int lr_mods_encode(const lr_mod_print *m, int count, uint8_t *out, size_t cap, size_t *len) {
    size_t k = 0;
    *len = 0;
    if (count < 0 || count > LR_MAX_UGC) return 0;
    for (int i = 0; i < count; i++) {
        const char *text[3] = {m[i].id.name, m[i].id.source, m[i].title};
        size_t max[3] = {LR_UGC_NAME_MAX, LR_UGC_SOURCE_MAX, LR_MOD_TITLE_MAX};
        if (!lr_ugc_name_ok(m[i].id.name) || !source_ok(m[i].id.source) || !title_ok(m[i].title, strlen(m[i].title)) ||
            (m[i].found != 0 && m[i].found != 1))
            return 0;
        for (int t = 0; t < 3; t++) {
            size_t n = strlen(text[t]);
            if (n > max[t] || k + 1 + n > cap) return 0;
            out[k++] = (uint8_t)n;
            memcpy(out + k, text[t], n);
            k += n;
        }
        if (k + 45 > cap) return 0;
        out[k++] = (uint8_t)m[i].found;
        for (int b = 0; b < 4; b++) out[k++] = (uint8_t)(m[i].files >> (8 * b));
        for (int b = 0; b < 8; b++) out[k++] = (uint8_t)(m[i].bytes >> (8 * b));
        memcpy(out + k, m[i].sha256, 32);
        k += 32;
    }
    *len = k;
    return 1;
}

int lr_mods_decode(const uint8_t *buf, size_t len, uint32_t count, lr_mod_print *out, int cap) {
    size_t k = 0;
    static const uint8_t zero[32] = {0};
    if (count > LR_MAX_UGC || (int)count > cap) return -1;
    for (uint32_t i = 0; i < count; i++) {
        lr_mod_print *p = &out[i];
        char *text[3] = {p->id.name, p->id.source, p->title};
        size_t max[3] = {LR_UGC_NAME_MAX, LR_UGC_SOURCE_MAX, LR_MOD_TITLE_MAX};
        memset(p, 0, sizeof *p);
        for (int t = 0; t < 3; t++) {
            if (k >= len) return -1;
            size_t n = buf[k++];
            if (n > max[t] || n > len - k || memchr(buf + k, 0, n)) return -1;
            memcpy(text[t], buf + k, n);
            text[t][n] = 0;
            k += n;
        }
        if (len - k < 45) return -1;
        if (buf[k] > 1) return -1;
        p->found = buf[k++];
        for (int b = 0; b < 4; b++) p->files |= (uint32_t)buf[k++] << (8 * b);
        for (int b = 0; b < 8; b++) p->bytes |= (uint64_t)buf[k++] << (8 * b);
        memcpy(p->sha256, buf + k, 32);
        k += 32;
        if (!lr_ugc_name_ok(p->id.name) || !source_ok(p->id.source) || !title_ok(p->title, strlen(p->title))) return -1;
        if (!p->found && (p->files || p->bytes || memcmp(p->sha256, zero, 32))) return -1;
        if (p->files > LR_MOD_MAX_FILES) return -1;
        for (uint32_t j = 0; j < i; j++)
            if (!strcmp(out[j].id.name, p->id.name) && !strcmp(out[j].id.source, p->id.source)) return -1;
    }
    return k == len ? (int)count : -1;
}

/* ---- comparing ---- */

void lr_mods_describe(const lr_mod_print *p, char *out, size_t cap) {
    char where[LR_UGC_NAME_MAX + 24];
    if (all_digits(p->id.name)) snprintf(where, sizeof where, "Workshop item %s", p->id.name);
    else snprintf(where, sizeof where, "mods\\%s", p->id.name);
    if (p->title[0]) snprintf(out, cap, "%s (%s)", p->title, where);
    else snprintf(out, cap, "%s", where);
}

static void add_name(char *list, size_t cap, const lr_mod_print *p) {
    char d[LR_MOD_TITLE_MAX + LR_UGC_NAME_MAX + 32];
    size_t k = strlen(list);
    lr_mods_describe(p, d, sizeof d);
    if (k + strlen(d) + 3 < cap) snprintf(list + k, cap - k, "%s%s", k ? ", " : "", d);
    else if (k + 6 < cap && !strstr(list, "...")) snprintf(list + k, cap - k, ", ...");
}

int lr_mods_match(const lr_game_info *game, const lr_mod_print *host, int host_count, const lr_mod_print *mine,
                  char *msg, size_t cap) {
    char missing[112] = "", different[112] = "", unknown[112] = ""; /* each fits the window's line after the instruction */
    for (int i = 0; i < game->ugc_count; i++) {
        const lr_mod_print *h = NULL;
        for (int j = 0; j < host_count; j++)
            if (!strcmp(host[j].id.name, game->ugc[i].name) && !strcmp(host[j].id.source, game->ugc[i].source)) h = &host[j];
        if (!h || !h->found) {
            lr_mod_print named = h ? *h : mine[i];
            add_name(unknown, sizeof unknown, &named);
            continue;
        }
        lr_mod_print named = mine[i];
        if (!named.title[0]) snprintf(named.title, sizeof named.title, "%s", h->title);
        if (!mine[i].found) add_name(missing, sizeof missing, &named);
        else if (mine[i].files != h->files || mine[i].bytes != h->bytes || memcmp(mine[i].sha256, h->sha256, 32))
            add_name(different, sizeof different, &named);
    }
    if (!missing[0] && !different[0] && !unknown[0]) {
        if (cap) msg[0] = 0;
        return 1;
    }
    /* The window shows about 250 characters: the instruction first, the names after it. */
    size_t k = (size_t)snprintf(msg, cap, "Your mods differ from the host's; your campaign was not replaced. Get the same versions (Steam Workshop), then join again.");
    if (missing[0] && k < cap) k += (size_t)snprintf(msg + k, cap - k, " Missing: %s.", missing);
    if (different[0] && k < cap) k += (size_t)snprintf(msg + k, cap - k, " Other version: %s.", different);
    if (unknown[0] && k < cap) snprintf(msg + k, cap - k, " Not found by the host: %s.", unknown);
    return 0;
}

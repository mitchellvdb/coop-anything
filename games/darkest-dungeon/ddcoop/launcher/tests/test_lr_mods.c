/* Campaign mods (lr_mods.c, lr_game_parse's persistent_ugcs): SHA-256 against published vectors, finding
 * Workshop and local mods in a fake Steam library on disk, fingerprints that change with any byte, name or
 * file, the LR_MODS body (round trips, every truncation, 200,000 mutations) and the guest's comparison.
 * Run under ASan/UBSan by launcher/build-linux.sh. */
#include "lr_files.h"
#include "lr_mods.h"
#include "lr_roster.h"

#include "dson_writer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <winioctl.h>
#else
#include <unistd.h>
#endif

static int failures;
#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #x);                                                         \
            failures++;                                                                                                \
        }                                                                                                              \
    } while (0)

static void hex(const uint8_t *h, char out[65]) {
    for (int i = 0; i < 32; i++) snprintf(out + 2 * i, 3, "%02x", h[i]);
}

static void sha_text(const char *s, size_t n, char out[65]) {
    lr_sha256_ctx c;
    uint8_t h[32];
    lr_sha256_init(&c);
    lr_sha256_update(&c, (const uint8_t *)s, n);
    lr_sha256_final(&c, h);
    hex(h, out);
}

static void test_sha256(void) {
    char out[65];
    sha_text("", 0, out);
    CHECK(!strcmp(out, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    sha_text("abc", 3, out);
    CHECK(!strcmp(out, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    const char *two = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    sha_text(two, strlen(two), out);
    CHECK(!strcmp(out, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
    /* one million 'a', fed in uneven pieces */
    static uint8_t a[1000];
    memset(a, 'a', sizeof a);
    lr_sha256_ctx c;
    uint8_t h[32];
    lr_sha256_init(&c);
    size_t done = 0;
    for (size_t step = 1; done < 1000000; step = step % 997 + 1) {
        size_t n = 1000000 - done < step ? 1000000 - done : step;
        lr_sha256_update(&c, a, n);
        done += n;
    }
    lr_sha256_final(&c, h);
    hex(h, out);
    CHECK(!strcmp(out, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
}

/* ---- a fake Steam library ---- */

static char root[512];

static void put(const char *dir, const char *rel, const char *text) {
    char path[LR_PATH_MAX], folder[LR_PATH_MAX];
    lr_join(path, sizeof path, dir, rel);
    snprintf(folder, sizeof folder, "%s", path);
    char *slash = strrchr(folder, '/'), *back = strrchr(folder, '\\');
    if (back > slash) slash = back;
    if (slash) {
        *slash = 0;
        CHECK(lr_make_dirs(folder));
    }
    CHECK(lr_write_file(path, (const uint8_t *)text, strlen(text)));
}

typedef struct {
    char game[LR_PATH_MAX], mods[LR_PATH_MAX], workshop[LR_PATH_MAX];
} library;

static void make_library(library *l, const char *name) {
    char base[LR_PATH_MAX], a[LR_PATH_MAX];
    lr_join(base, sizeof base, root, name);
    lr_join(a, sizeof a, base, "steamapps");
    lr_join(l->workshop, sizeof l->workshop, a, "workshop");
    lr_join(base, sizeof base, l->workshop, "content");
    lr_join(l->workshop, sizeof l->workshop, base, "262060");
    lr_join(base, sizeof base, a, "common");
    lr_join(l->game, sizeof l->game, base, "DarkestDungeon");
    lr_join(l->mods, sizeof l->mods, l->game, "mods");
    CHECK(lr_make_dirs(l->workshop) && lr_make_dirs(l->mods));
}

/* One Workshop-style mod; variant changes one thing. Files are written in a different order each time. */
static void make_mod(const char *dir, int variant, int reverse) {
    static const char *const rel[] = {"project.xml", "heroes/hero.info.darkest", "heroes/sub/skills.json", "localization/x.loc"};
    for (int k = 0; k < 4; k++) {
        int i = reverse ? 3 - k : k;
        char text[200];
        if (i == 0) snprintf(text, sizeof text, "<project>\n<Title> Better Hero %s</Title>\n<PublishedFileId>12345</PublishedFileId>\n</project>", variant == 4 ? "\xc3\xa9" : "");
        else if (i == 1) snprintf(text, sizeof text, "hero: dmg %d", variant == 1 ? 6 : 5);
        else snprintf(text, sizeof text, "file %d", i);
        put(dir, i == 2 && variant == 2 ? "heroes/sub/skillz.json" : rel[i], text);
    }
    if (variant == 3) put(dir, "extra.txt", "");
}

static void test_locate_and_fingerprint(void) {
    library l, other;
    make_library(&l, "lib1");
    make_library(&other, "lib2");
    char ws[LR_PATH_MAX], local[LR_PATH_MAX], dir[LR_PATH_MAX];
    lr_join(ws, sizeof ws, l.workshop, "12345");
    make_mod(ws, 0, 0);
    lr_join(local, sizeof local, l.mods, "My Local Mod");
    put(local, "effects/a.txt", "local");
    put(l.mods, "DDCoop/ddcoop.ini", "role=off"); /* the launcher's own folder */

    lr_ugc steam = {"12345", "Steam"}, plain = {"12345", ""}, loc = {"My Local Mod", ""}, loc_steam = {"My Local Mod", "Steam"},
           ddcoop = {"DDCoop", ""}, absent = {"777", "Steam"}, bad1 = {"../x", ""}, bad2 = {"a/b", ""}, bad3 = {"", ""};
    CHECK(lr_mods_locate(l.game, &steam, dir, sizeof dir) && lr_dir_exists(dir) && strstr(dir, "workshop"));
    CHECK(lr_mods_locate(l.game, &plain, dir, sizeof dir) && strstr(dir, "workshop"));
    CHECK(lr_mods_locate(l.game, &loc, dir, sizeof dir) && strstr(dir, "My Local Mod"));
    CHECK(lr_mods_locate(l.game, &loc_steam, dir, sizeof dir) && strstr(dir, "My Local Mod")); /* falls back to mods\ */
    CHECK(!lr_mods_locate(l.game, &ddcoop, dir, sizeof dir));
    CHECK(!lr_mods_locate(l.game, &absent, dir, sizeof dir));
    CHECK(!lr_mods_locate(l.game, &bad1, dir, sizeof dir) && !lr_mods_locate(l.game, &bad2, dir, sizeof dir) &&
          !lr_mods_locate(l.game, &bad3, dir, sizeof dir));
    /* A Workshop mod copied into mods\ under another folder name is found by the id in its project.xml. */
    char copied[LR_PATH_MAX];
    lr_join(copied, sizeof copied, other.mods, "copied by hand");
    make_mod(copied, 0, 1);
    CHECK(lr_mods_locate(other.game, &steam, dir, sizeof dir) && strstr(dir, "copied by hand"));

    lr_mod_print p, q;
    lr_mods_print(l.game, &steam, &p);
    CHECK(p.found && p.files == 4 && p.bytes > 0 && !strcmp(p.title, "Better Hero") && !strcmp(p.id.name, "12345"));
    lr_mods_print(other.game, &steam, &q); /* same files, written in reverse order, other folder name */
    CHECK(q.found && q.files == 4 && q.bytes == p.bytes && !memcmp(p.sha256, q.sha256, 32));
    lr_mods_print(l.game, &absent, &q);
    CHECK(!q.found && !q.files && !strcmp(q.id.name, "777"));
    for (int v = 1; v <= 4; v++) {
        char name[32], d[LR_PATH_MAX];
        snprintf(name, sizeof name, "variant%d", v);
        lr_join(d, sizeof d, other.workshop, name);
        make_mod(d, v, v & 1);
        CHECK(lr_mods_fingerprint(d, &q) && q.found);
        CHECK(memcmp(p.sha256, q.sha256, 32) != 0); /* one byte, a renamed file, an added empty file, the title */
        if (v == 3) CHECK(q.files == 5);
        if (v == 4) CHECK(!strcmp(q.title, "Better Hero \xc3\xa9"));
    }
    /* titles: XML entities decoded, control characters blanked, long titles cut between whole UTF-8 characters */
    char tdir[LR_PATH_MAX];
    lr_join(tdir, sizeof tdir, other.workshop, "titles");
    put(tdir, "project.xml", "<Title>Jeanne&amp;Jeanne &lt;Alter&gt;\t&quot;2&quot;</Title>");
    CHECK(lr_mods_fingerprint(tdir, &q) && !strcmp(q.title, "Jeanne&Jeanne <Alter> \"2\""));
    char long_title[300] = "<Title>";
    for (int i = 0; i < 40; i++) strcat(long_title, "\xe5\xae\x88"); /* 3-byte characters, 120 bytes */
    strcat(long_title, "</Title>");
    put(tdir, "project.xml", long_title);
    CHECK(lr_mods_fingerprint(tdir, &q) && strlen(q.title) == 63 && !memcmp(q.title, "\xe5\xae\x88", 3));
    put(tdir, "project.xml", "<Title>no end");
    CHECK(lr_mods_fingerprint(tdir, &q) && !q.title[0]);
    CHECK(!lr_mods_fingerprint(l.workshop, &q) || q.found); /* a folder of folders is fine */
    char none[LR_PATH_MAX];
    lr_join(none, sizeof none, root, "does-not-exist");
    CHECK(!lr_mods_fingerprint(none, &q) && !q.found);
    printf("Mods on disk: Workshop/local/source-hint/project.xml-id lookup, own folder and unsafe names refused; fingerprints equal for identical files in any order and differ for a byte, a name, an added file or the title\n");
}

/* A directory link must not disappear from a supposedly complete fingerprint.
 * A Windows junction needs no symbolic-link privilege. Keep targets/test links
 * within this test's disposable build folder and detach links after the check. */
static int directory_link(const char *link, const char *target) {
#ifdef _WIN32
    wchar_t wl[LR_PATH_MAX], wt[LR_PATH_MAX], full[LR_PATH_MAX];
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, link, -1, wl, LR_PATH_MAX) ||
        !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, target, -1, wt, LR_PATH_MAX)) return 0;
    DWORD n = GetFullPathNameW(wt, LR_PATH_MAX, full, NULL);
    if (!n || n >= LR_PATH_MAX || !CreateDirectoryW(wl, NULL)) return 0;
    struct { DWORD tag; WORD len, reserved, sub_off, sub_len, print_off, print_len;
             wchar_t path[2 * LR_PATH_MAX + 8]; } r;
    memset(&r, 0, sizeof r);r.tag = IO_REPARSE_TAG_MOUNT_POINT;
    wcscpy(r.path, L"\\??\\");wcscat(r.path, full);
    size_t sub = wcslen(r.path), print = wcslen(full);
    r.sub_len = (WORD)(sub * sizeof(wchar_t));r.print_off = (WORD)((sub + 1) * sizeof(wchar_t));
    r.print_len = (WORD)(print * sizeof(wchar_t));wcscpy(r.path + sub + 1, full);
    r.len = (WORD)(8 + (sub + 1 + print + 1) * sizeof(wchar_t));
    HANDLE h = CreateFileW(wl, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                           FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
    DWORD bytes;int ok = h != INVALID_HANDLE_VALUE &&
        DeviceIoControl(h, FSCTL_SET_REPARSE_POINT, &r, (DWORD)r.len + 8, NULL, 0, &bytes, NULL);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    if (!ok) RemoveDirectoryW(wl);
    return ok;
#else
    char absolute[LR_PATH_MAX];
    return realpath(target, absolute) && symlink(absolute, link) == 0;
#endif
}
static void detach_directory_link(const char *link) {
#ifdef _WIN32
    wchar_t w[LR_PATH_MAX];
    CHECK(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, link, -1, w, LR_PATH_MAX) && RemoveDirectoryW(w));
#else
    CHECK(unlink(link) == 0);
#endif
}
static int count_entry(void *ctx, const char *name, int is_dir) {
    (void)name;(void)is_dir;(*(int *)ctx)++;return 1;
}
static void test_incomplete_tree_refused(void) {
    char a[LR_PATH_MAX], b[LR_PATH_MAX], target_a[LR_PATH_MAX], target_b[LR_PATH_MAX], link_a[LR_PATH_MAX], link_b[LR_PATH_MAX];
    lr_join(a, sizeof a, root, "link-mod-a");lr_join(b, sizeof b, root, "link-mod-b");
    lr_join(target_a, sizeof target_a, root, "link-target-a");lr_join(target_b, sizeof target_b, root, "link-target-b");
    put(a, "project.xml", "<Title>same mod</Title>");put(b, "project.xml", "<Title>same mod</Title>");
    put(target_a, "hero.info.darkest", "damage 1");put(target_b, "hero.info.darkest", "damage 99");
    lr_join(link_a, sizeof link_a, a, "heroes");lr_join(link_b, sizeof link_b, b, "heroes");
    int linked_a = directory_link(link_a, target_a), linked_b = directory_link(link_b, target_b);
    CHECK(linked_a && linked_b);
    if (linked_a && linked_b) {
        lr_mod_print p, q;int entries = 0;
        CHECK(lr_list_dir(a, count_entry, &entries) < 0);
        CHECK(!lr_mods_fingerprint(a, &p) && !p.found && !p.files && !p.bytes);
        CHECK(!lr_mods_fingerprint(b, &q) && !q.found && !q.files && !q.bytes);
        CHECK(!lr_mods_fingerprint(link_a, &p) && !p.found); /* root itself is a link */
    }
    if (linked_a) detach_directory_link(link_a);
    if (linked_b) detach_directory_link(link_b);
    lr_mod_print p;int entries = 0;
    CHECK(lr_mods_fingerprint(a, &p) && p.found && p.files == 1);
    CHECK(lr_list_dir(target_a, count_entry, &entries) == 1 && entries == 1);
#ifdef _WIN32
    /* An existing but unreadable enumeration must not be certified as empty. */
    HANDLE dir_lock = CreateFileA(target_a, FILE_LIST_DIRECTORY, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    CHECK(dir_lock != INVALID_HANDLE_VALUE);
    if (dir_lock != INVALID_HANDLE_VALUE) {
        CHECK(lr_list_dir(target_a, count_entry, &entries) < 0);
        CHECK(!lr_mods_fingerprint(target_a, &p) && !p.found && !p.files && !p.bytes);
        CloseHandle(dir_lock);
    }
    char locked_file[LR_PATH_MAX];lr_join(locked_file, sizeof locked_file, target_a, "hero.info.darkest");
    HANDLE file_lock = CreateFileA(locked_file, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(file_lock != INVALID_HANDLE_VALUE);
    if (file_lock != INVALID_HANDLE_VALUE) {
        CHECK(!lr_mods_fingerprint(target_a, &p) && !p.found && !p.files && !p.bytes);
        CloseHandle(file_lock);
    }
#endif
    char over[LR_PATH_MAX + 20];memset(over, 'x', sizeof over - 1);over[sizeof over - 1] = 0;
    CHECK(lr_list_dir(over, count_entry, &entries) < 0 && !lr_mods_fingerprint(over, &p));
    puts("Mod tree completeness: unequal linked targets and linked roots refuse instead of hashing a partial tree; normal files and overlong paths checked");
}

/* ---- the LR_MODS body ---- */

static void sample(lr_mod_print m[3]) {
    memset(m, 0, 3 * sizeof *m);
    strcpy(m[0].id.name, "12345");
    strcpy(m[0].id.source, "Steam");
    m[0].found = 1;
    m[0].files = 4;
    m[0].bytes = 5000000000ull;
    for (int i = 0; i < 32; i++) m[0].sha256[i] = (uint8_t)(i * 7 + 1);
    strcpy(m[0].title, "Better Hero");
    strcpy(m[1].id.name, "My Local Mod");
    m[1].found = 1;
    m[1].files = 1;
    m[1].bytes = 5;
    m[1].sha256[0] = 9;
    strcpy(m[2].id.name, "777");
    strcpy(m[2].id.source, "Steam");
}

static void test_wire_body(void) {
    lr_mod_print m[3], back[LR_MAX_UGC];
    static uint8_t buf[LR_MAX_UGC * 200], copy[LR_MAX_UGC * 200];
    size_t len, len2;
    sample(m);
    CHECK(lr_mods_encode(m, 0, buf, sizeof buf, &len) && len == 0);
    CHECK(lr_mods_decode(buf, 0, 0, back, LR_MAX_UGC) == 0);
    CHECK(lr_mods_encode(m, 3, buf, sizeof buf, &len) && len > 0);
    CHECK(lr_mods_decode(buf, len, 3, back, LR_MAX_UGC) == 3);
    CHECK(!memcmp(&back[0], &m[0], sizeof m[0]) && !memcmp(&back[1], &m[1], sizeof m[1]) && !memcmp(&back[2], &m[2], sizeof m[2]));
    CHECK(lr_mods_decode(buf, len, 2, back, LR_MAX_UGC) < 0 && lr_mods_decode(buf, len, 4, back, LR_MAX_UGC) < 0);
    CHECK(lr_mods_decode(buf, len, 3, back, 2) < 0);
    for (size_t cut = 0; cut < len; cut++) CHECK(lr_mods_decode(buf, cut, 3, back, LR_MAX_UGC) < 0);
    memcpy(copy, buf, len);
    copy[len] = 0;
    CHECK(lr_mods_decode(copy, len + 1, 3, back, LR_MAX_UGC) < 0);
    CHECK(!lr_mods_encode(m, 3, buf, len - 1, &len2));
    /* refused on encode: unsafe name, bad source, control character in a title, not-found with a fingerprint */
    lr_mod_print bad[3];
    sample(bad);
    strcpy(bad[0].id.name, "a\\b");
    CHECK(!lr_mods_encode(bad, 3, buf, sizeof buf, &len2));
    sample(bad);
    strcpy(bad[0].id.source, "St eam");
    CHECK(!lr_mods_encode(bad, 3, buf, sizeof buf, &len2));
    sample(bad);
    strcpy(bad[0].title, "a\nb");
    CHECK(!lr_mods_encode(bad, 3, buf, sizeof buf, &len2));
    sample(bad);
    bad[2].sha256[5] = 1; /* encodes, but a decoder refuses a fingerprint for a mod that was not found */
    CHECK(lr_mods_encode(bad, 3, buf, sizeof buf, &len2) && lr_mods_decode(buf, len2, 3, back, LR_MAX_UGC) < 0);
    sample(bad);
    bad[1] = bad[0]; /* duplicate */
    CHECK(lr_mods_encode(bad, 2, buf, sizeof buf, &len2) && lr_mods_decode(buf, len2, 2, back, LR_MAX_UGC) < 0);
    /* the largest list fits one message */
    static lr_mod_print many[LR_MAX_UGC];
    for (int i = 0; i < LR_MAX_UGC; i++) {
        memset(&many[i], 'x', sizeof many[i]);
        memset(many[i].id.name, '1', LR_UGC_NAME_MAX);
        many[i].id.name[LR_UGC_NAME_MAX] = 0;
        many[i].id.name[0] = (char)('A' + i % 26);
        many[i].id.name[1] = (char)('a' + i / 26);
        memset(many[i].id.source, 'S', LR_UGC_SOURCE_MAX);
        many[i].id.source[LR_UGC_SOURCE_MAX] = 0;
        memset(many[i].title, 'T', LR_MOD_TITLE_MAX);
        many[i].title[LR_MOD_TITLE_MAX] = 0;
        many[i].found = 1;
        many[i].files = 7;
    }
    CHECK(lr_mods_encode(many, LR_MAX_UGC, buf, 16000, &len2) && len2 <= 16000);
    CHECK(lr_mods_decode(buf, len2, LR_MAX_UGC, back, LR_MAX_UGC) == LR_MAX_UGC);
    CHECK(lr_mods_decode(buf, len2, LR_MAX_UGC + 1, back, LR_MAX_UGC + 1 > LR_MAX_UGC ? LR_MAX_UGC : 0) < 0);
    /* mutations: never a crash; whatever is accepted encodes back to the same bytes */
    srand(77);
    lr_mods_encode(m, 3, buf, sizeof buf, &len);
    long accepted = 0;
    for (long i = 0; i < 200000; i++) {
        size_t n = len;
        memcpy(copy, buf, len);
        int edits = 1 + rand() % 4;
        for (int e = 0; e < edits; e++) {
            int kind = rand() % 3;
            if (kind == 0 && n) copy[rand() % n] = (uint8_t)rand();
            else if (kind == 1 && n) n = (size_t)rand() % n;
            else if (n < sizeof copy) copy[n++] = (uint8_t)rand();
        }
        uint32_t count = (uint32_t)(rand() % 5);
        int r = lr_mods_decode(copy, n, count, back, LR_MAX_UGC);
        if (r >= 0) {
            accepted++;
            CHECK(lr_mods_encode(back, r, buf + len, sizeof buf - len, &len2) && len2 == n && !memcmp(buf + len, copy, n));
        }
    }
    printf("LR_MODS body: round trip, every truncation, trailing byte, wrong count, unsafe fields, duplicates, 32 maximal entries in one message, 200000 mutations (%ld accepted, all re-encode identically)\n", accepted);
}

static void test_match(void) {
    lr_mod_print host[3], mine[3];
    lr_game_info g;
    char msg[256];
    memset(&g, 0, sizeof g);
    sample(host);
    host[2].found = 1;
    host[2].files = 2;
    host[2].sha256[0] = 3;
    g.ugc_count = 3;
    for (int i = 0; i < 3; i++) g.ugc[i] = host[i].id;
    memcpy(mine, host, sizeof mine);
    mine[0].title[0] = 0;
    CHECK(lr_mods_match(&g, host, 3, mine, msg, sizeof msg) && !msg[0]);
    mine[0].found = 0;
    mine[0].files = 0;
    memset(mine[0].sha256, 0, 32);
    CHECK(!lr_mods_match(&g, host, 3, mine, msg, sizeof msg) && strstr(msg, "Missing: Better Hero (Workshop item 12345)") &&
          strstr(msg, "not replaced"));
    memcpy(mine, host, sizeof mine);
    mine[1].sha256[31] ^= 1;
    CHECK(!lr_mods_match(&g, host, 3, mine, msg, sizeof msg) && strstr(msg, "Other version: mods\\My Local Mod") && !strstr(msg, "Missing"));
    memcpy(mine, host, sizeof mine);
    mine[1].bytes++;
    CHECK(!lr_mods_match(&g, host, 3, mine, msg, sizeof msg) && strstr(msg, "Other version"));
    memcpy(mine, host, sizeof mine);
    CHECK(!lr_mods_match(&g, host, 2, mine, msg, sizeof msg) && strstr(msg, "Not found by the host: Workshop item 777"));
    host[2].found = 0;
    host[2].files = 0;
    memset(host[2].sha256, 0, 32);
    CHECK(!lr_mods_match(&g, host, 3, mine, msg, sizeof msg) && strstr(msg, "Not found by the host"));
    /* a message with many names stays within the window's line */
    lr_mod_print h2[LR_MAX_UGC], m2[LR_MAX_UGC];
    memset(&g, 0, sizeof g);
    for (int i = 0; i < LR_MAX_UGC; i++) {
        memset(&h2[i], 0, sizeof h2[i]);
        snprintf(h2[i].id.name, sizeof h2[i].id.name, "%d", 1000000 + i);
        h2[i].found = 1;
        h2[i].sha256[0] = 1;
        m2[i] = h2[i];
        m2[i].found = 0;
        m2[i].sha256[0] = 0;
        g.ugc[g.ugc_count++] = h2[i].id;
    }
    CHECK(!lr_mods_match(&g, h2, LR_MAX_UGC, m2, msg, sizeof msg) && strstr(msg, "...") && strlen(msg) < sizeof msg);
    printf("Mod comparison: identical lists accepted; missing, other version (bytes or fingerprint) and host-unconfirmed mods named with titles; long lists shortened\n");
}

static void test_game_parse(void) {
    static uint8_t bytes[65536];
    lr_game_info g;
    size_t n;
    static const char *const names[] = {"12345", "My Local Mod", "98765"};
    static const char *const sources[] = {"Steam", "local", NULL};
    n = dw_game_ugc(bytes, names, sources, 3, 0);
    CHECK(lr_game_parse(bytes, n, &g) && !g.ugc_bad && g.ugc_count == 3);
    CHECK(!strcmp(g.ugc[0].name, "12345") && !strcmp(g.ugc[0].source, "Steam") && !strcmp(g.ugc[1].name, "My Local Mod") &&
          !strcmp(g.ugc[2].name, "98765") && !g.ugc[2].source[0]);
    n = dw_game_ugc(bytes, names, sources, 3, 1); /* one group level */
    CHECK(lr_game_parse(bytes, n, &g) && !g.ugc_bad && g.ugc_count == 3 && !strcmp(g.ugc[1].source, "local"));
    n = dw_game_ugc(bytes, names, sources, 0, 0);
    CHECK(lr_game_parse(bytes, n, &g) && !g.ugc_bad && !g.ugc_count);
    n = dw_game(bytes, 0, "", NULL, 0); /* no list at all */
    CHECK(lr_game_parse(bytes, n, &g) && !g.ugc_bad && !g.ugc_count);
    static const char *const dup[] = {"12345", "12345"};
    static const char *const dup_src[] = {"Steam", "Steam"};
    n = dw_game_ugc(bytes, dup, dup_src, 2, 0);
    CHECK(lr_game_parse(bytes, n, &g) && !g.ugc_bad && g.ugc_count == 1);
    static const char *const unsafe[][1] = {{"../evil"}, {"a\\b"}, {"c:"}, {" lead"}, {"trail."}, {"x?"}};
    for (int i = 0; i < 6; i++) {
        n = dw_game_ugc(bytes, unsafe[i], NULL, 1, 0);
        CHECK(lr_game_parse(bytes, n, &g) && g.ugc_bad && !g.ugc_count); /* the campaign still reads; mods refuse */
    }
    static const char *const bad_source[] = {"St eam"};
    n = dw_game_ugc(bytes, names, bad_source, 1, 0);
    CHECK(lr_game_parse(bytes, n, &g) && g.ugc_bad);
    static char many_names[LR_MAX_UGC + 1][16];
    static const char *many[LR_MAX_UGC + 1];
    for (int i = 0; i <= LR_MAX_UGC; i++) {
        snprintf(many_names[i], sizeof many_names[i], "%d", 5000 + i);
        many[i] = many_names[i];
    }
    n = dw_game_ugc(bytes, many, NULL, LR_MAX_UGC, 0);
    CHECK(lr_game_parse(bytes, n, &g) && !g.ugc_bad && g.ugc_count == LR_MAX_UGC);
    n = dw_game_ugc(bytes, many, NULL, LR_MAX_UGC + 1, 0);
    CHECK(lr_game_parse(bytes, n, &g) && g.ugc_bad && !g.ugc_count);
    /* deeper than one group level, a name that is not text, a list that is not an object */
    for (int kind = 0; kind < 3; kind++) {
        dson_writer w;
        memset(&w, 0, sizeof w);
        dw_begin(&w, "base_root");
        dw_bool(&w, "inraid", 0);
        dw_str(&w, "raiddungeon", "none");
        if (kind == 2) dw_int(&w, "persistent_ugcs", 1);
        else {
            dw_begin(&w, "persistent_ugcs");
            dw_begin(&w, "a");
            if (kind == 0) {
                dw_begin(&w, "b");
                dw_begin(&w, "0");
                dw_str(&w, "name", "1");
                dw_end(&w);
                dw_end(&w);
            } else {
                dw_begin(&w, "0");
                dw_int(&w, "name", 1);
                dw_end(&w);
            }
            dw_end(&w);
            dw_end(&w);
        }
        dw_end(&w);
        n = dw_finish(&w, bytes);
        CHECK(lr_game_parse(bytes, n, &g) && g.ugc_bad);
    }
    /* Nonempty unknown layouts must not silently become an empty mod list. */
    for (int kind = 0; kind < 8; kind++) {
        dson_writer w = {0};dw_begin(&w, "base_root");dw_bool(&w, "inraid", 0);dw_str(&w, "raiddungeon", "none");
        dw_begin(&w, "persistent_ugcs");
        if (kind == 0) dw_str(&w, "0", "12345");
        if (kind == 1) {dw_begin(&w, "ugcs");dw_str(&w, "0", "12345");dw_end(&w);}
        if (kind == 2) {dw_begin(&w, "0");dw_str(&w, "title", "unknown entry");dw_end(&w);}
        if (kind == 3) {dw_begin(&w, "0");dw_str(&w, "name", "12345");dw_str(&w, "name", "67890");dw_end(&w);}
        if (kind == 4) {dw_begin(&w, "0");dw_str(&w, "name", "12345");dw_str(&w, "source", "Steam");dw_str(&w, "source", "local");dw_end(&w);}
        if (kind == 6) {dw_begin(&w, "0");dw_end(&w);} /* unknown empty object, nonempty list */
        if (kind == 7) {dw_begin(&w, "0");dw_str(&w, "name", "12345");dw_end(&w);dw_begin(&w, "1");dw_end(&w);}
        dw_end(&w);
        if (kind == 5) {dw_begin(&w, "persistent_ugcs");dw_begin(&w, "0");dw_str(&w, "name", "12345");dw_end(&w);dw_end(&w);}
        dw_end(&w);n = dw_finish(&w, bytes);
        CHECK(lr_game_parse(bytes, n, &g) && g.ugc_bad && !g.ugc_count);
    }
    /* mutated copies never crash and never produce an unsafe name */
    n = dw_game_ugc(bytes, names, sources, 3, 1);
    static uint8_t copy[65536];
    srand(5);
    for (long i = 0; i < 200000; i++) {
        memcpy(copy, bytes, n);
        for (int e = 0; e < 1 + rand() % 3; e++) copy[rand() % n] = (uint8_t)rand();
        size_t m = rand() % 8 ? n : (size_t)rand() % n;
        if (lr_game_parse(copy, m, &g) && !g.ugc_bad)
            for (int k = 0; k < g.ugc_count; k++) CHECK(lr_ugc_name_ok(g.ugc[k].name));
    }
    printf("Campaign mod list: direct and grouped entries, optional source, duplicates collapsed, empty/absent lists, unsafe names/sources, 33 mods, unknown nesting and non-text names flagged; 200000 mutated saves\n");
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
#ifdef _WIN32
    snprintf(root, sizeof root, "build/lrmods-%lu-%llu", GetCurrentProcessId(), (unsigned long long)GetTickCount64());
#else
    snprintf(root, sizeof root, "build/lrmods-%ld-%ld", (long)getpid(), (long)time(NULL));
#endif
    CHECK(lr_make_dirs(root));
    (void)dw_roster; /* shared test helper, not needed here */
    test_sha256();
    test_locate_and_fingerprint();
    test_incomplete_tree_refused();
    test_wire_body();
    test_match();
    test_game_parse();
    printf(failures ? "test_lr_mods: %d FAILURES\n" : "test_lr_mods: all passed\n", failures);
    return failures != 0;
}

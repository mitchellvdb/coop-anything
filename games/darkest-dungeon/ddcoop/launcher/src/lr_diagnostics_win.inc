/* UI/file adapter. Transport, validation, bounds and ZIP are independently
 * tested in lr_diagnostics.c/lr_logpack.c; no campaign or game writes here. */
static const char *diagnostics_player(unsigned player) {
    static const char *const names[] = {"host", "player-2", "player-3", "player-4"};
    return player < 4 ? names[player] : "unknown";
}
static int diagnostics_tail(const char *path, size_t cap, uint8_t **out, size_t *len, uint64_t *original) {
    *out = NULL;
    *len = 0;
    *original = 0;
    HANDLE h = CreateFileW(W(path), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    LARGE_INTEGER size, at;
    if (!GetFileSizeEx(h, &size) || size.QuadPart < 0) {
        CloseHandle(h);
        return 0;
    }
    *original = (uint64_t)size.QuadPart;
    at.QuadPart = size.QuadPart > (LONGLONG)cap ? size.QuadPart - (LONGLONG)cap : 0;
    size_t n = (size_t)(size.QuadPart - at.QuadPart);
    uint8_t *p = malloc(n ? n : 1);
    DWORD got = 0;
    int ok = p && SetFilePointerEx(h, at, NULL, FILE_BEGIN) && ReadFile(h, p, (DWORD)n, &got, NULL);
    CloseHandle(h);
    if (!ok) {
        free(p);
        return 0;
    }
    size_t skip = 0;
    if (at.QuadPart) {
        while (skip < got && p[skip] != '\n')
            ++skip;
        if (skip < got)
            ++skip;
    }
    if (skip)
        memmove(p, p + skip, got - skip);
    *out = p;
    *len = got - skip;
    return 1;
}
static void diagnostics_directory(uint32_t report) {
    if (G.diag_folder_id == report && G.diag_dir[0])
        return;
    G.diag_folder_id = report;
    G.diag_dir[0] = 0;
    memset(G.diag_files, 0, sizeof G.diag_files);
    wchar_t desktop[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desktop) != S_OK)
        return;
    char base[LR_PATH_MAX], stamp[120];
    to_utf8(desktop, base, sizeof base);
    time_t now = time(NULL);
    struct tm when;
    localtime_s(&when, &now);
    char date[64];
    strftime(date, sizeof date, "%Y-%m-%d_%H%M%S", &when);
    snprintf(stamp, sizeof stamp, "DDCoop-report-%s-%08x", date, report);
    lr_join(G.diag_dir, sizeof G.diag_dir, base, stamp);
    if (!lr_make_dirs(G.diag_dir))
        G.diag_dir[0] = 0;
}
typedef struct {
    char dir[LR_PATH_MAX], info[4096];
    size_t used;
    unsigned present;
} diagnostics_sink;
static int diagnostics_file(void *ctx, unsigned file, unsigned flags, uint64_t original, const uint8_t *data,
                            size_t size) {
    diagnostics_sink *s = ctx;
    int n = snprintf(s->info + s->used, sizeof s->info - s->used, "%s: %s; source=%llu exported=%zu%s\r\n",
                     lr_log_names[file], flags & LR_LOG_MISSING ? "missing/unreadable" : "included",
                     (unsigned long long)original, size, flags & LR_LOG_TRUNCATED ? " (tail truncated)" : "");
    if (n < 0 || (size_t)n >= sizeof s->info - s->used)
        return 0;
    s->used += (size_t)n;
    if (flags & LR_LOG_MISSING)
        return 1;
    uint8_t *copy = malloc(size ? size : 1);
    if (!copy)
        return 0;
    if (size)
        memcpy(copy, data, size);
    size_t safe = lr_logs_redact(copy, size, NULL);
    char path[LR_PATH_MAX];
    lr_join(path, sizeof path, s->dir, lr_log_names[file]);
    int ok = lr_write_file(path, copy, safe);
    free(copy);
    if (ok)
        s->present |= 1u << file;
    return ok;
}
static int diagnostics_save(void *ctx, uint32_t report, unsigned player, const uint8_t *data, size_t size) {
    (void)ctx;
    if (player > 3 || !lr_logpack_visit(data, size, NULL, NULL))
        return 0;
    diagnostics_directory(report);
    if (!G.diag_dir[0])
        return 0;
    diagnostics_sink s = {0};
    lr_join(s.dir, sizeof s.dir, G.diag_dir, diagnostics_player(player));
    if (!lr_make_dirs(s.dir) || !lr_logpack_visit(data, size, diagnostics_file, &s))
        return 0;
    char path[LR_PATH_MAX];
    lr_join(path, sizeof path, s.dir, "FILES.txt");
    int ok = lr_write_file(path, (const uint8_t *)s.info, s.used);
    if (ok)
        G.diag_files[player] = s.present;
    return ok;
}
static int diagnostics_collect(void *ctx, uint8_t **out, size_t *size) {
    (void)ctx;
    lr_logpack pack = {0};
    *out = NULL;
    *size = 0;
    if (G.log)
        fflush(G.log);
    unsigned player = G.core.role == LR_ROLE_GUEST ? (unsigned)G.core.player : 0;
    unsigned runtime = player ? player + 1 : 1;
    int solo = G.core.role == LR_ROLE_HOST && strstr(DDC_VERSION, "duo") != NULL;
    for (unsigned file = 0; file < 11; ++file) {
        PHASE("diagnostics: bounded log snapshot");
        if (file >= 1 && file <= 4 && !solo && file != runtime)
            continue;
        char path[LR_PATH_MAX];
        lr_join(path, sizeof path, file >= 8 ? G.bin : G.moddir, lr_log_names[file]);
        uint8_t *data = NULL;
        size_t len = 0;
        uint64_t original = 0;
        int present = diagnostics_tail(path, file >= 1 && file <= 4 ? LR_LOG_RUNTIME_MAX : LR_LOG_OTHER_MAX, &data,
                                       &len, &original);
        unsigned flags = !present ? LR_LOG_MISSING : original > len ? LR_LOG_TRUNCATED : 0;
        int ok = lr_logpack_add(&pack, file, flags, present ? original : 0, data, len);
        free(data);
        if (!ok) {
            lr_logpack_free(&pack);
            return 0;
        }
    }
    char text[4096];
    int n = snprintf(text, sizeof text, "DDCoop %s\r\nstate: %s\r\nstatus: %s\r\nerror: %s\r\n", DDC_VERSION,
                     lr_state_name(G.core.state), G.core.status, G.core.error);
    for (unsigned i = 0; i < CHK_COUNT && n > 0 && n < (int)sizeof text - 300; ++i)
        n += snprintf(text + n, sizeof text - (size_t)n, "[%s] %s\r\n", G.ok[i] ? "OK" : "--", G.check_text[i]);
    if (n < 0 || n >= (int)sizeof text || !lr_logpack_add(&pack, 11, 0, (uint64_t)n, (const uint8_t *)text, (size_t)n))
        goto failed;
    n = snprintf(text, sizeof text,
                 "DDCoop %s\r\nReport %08x\r\nPlayer: %s\r\nCampaign slot: %d\r\nRuntime tail limit:2MiB; other "
                 "logs:512KiB.\r\nOnly diagnostic allowlist; secret lines removed. See FILES.txt for missing/truncated "
                 "files.\r\n",
                 DDC_VERSION, G.diagnostics.report, diagnostics_player(player), G.core.cfg.coop_slot);
    if (n < 0 || n >= (int)sizeof text || !lr_logpack_add(&pack, 12, 0, (uint64_t)n, (const uint8_t *)text, (size_t)n))
        goto failed;
    /* A guest retains a bounded local copy even if the link fails mid-upload. */
    if (G.diagnostics.role == 2)
        (void)diagnostics_save(NULL, G.diagnostics.report, player, pack.data, pack.len);
    *out = pack.data;
    *size = pack.len;
    return 1;
failed:
    lr_logpack_free(&pack);
    return 0;
}
static void diagnostics_changed(void *ctx, int finished) {
    (void)ctx;
    lr_diagnostics *d = &G.diagnostics;
    if (d->report)
        diagnostics_directory(d->report);
    if (finished)
        G.diag_done_at = now_ms();
    if (d->role == 2) {
        if (finished)
            logline("automatic diagnostics: %s; local copy: %s",
                    d->phase == 6 ? "received by host" : "delivery failed; original logs remain on this PC",
                    G.diag_dir);
        return;
    }
    char report[2048];
    int n = snprintf(report, sizeof report,
                     "DDCoop %s diagnostic report %08x\r\n%s\r\nReason: %d (1 manual,2 desync,3 resync,4 "
                     "session/recovery event)\r\n\r\n",
                     DDC_VERSION, d->report,
                     finished ? "Collection finished (check every player's status below)."
                              : "Collecting - keep the launchers open.",
                     d->reason);
    static const char *const status[] = {"not participating",
                                         "waiting",
                                         "receiving",
                                         "received and verified",
                                         "MISSING - disconnected or timed out",
                                         "FAILED - collection, integrity or disk error"};
    unsigned expected = 0, received = 0;
    for (unsigned i = 0; i < 4; ++i)
        if (!i || d->slots[i].status != LR_D_NONE) {
            ++expected;
            if (d->slots[i].status == LR_D_OK)
                ++received;
            n += snprintf(report + n, sizeof report - (size_t)n, "%s: %s\r\n", diagnostics_player(i),
                          status[d->slots[i].status]);
        }
    if (G.diag_dir[0]) {
        char path[LR_PATH_MAX];
        lr_join(path, sizeof path, G.diag_dir, "REPORT.txt");
        lr_write_file(path, (const uint8_t *)report, (size_t)n);
    }
    if (!finished) {
        wchar_t label[64];
        swprintf(label, 64, L"Logs %u/%u", received, expected);
        SetWindowTextW(G.logs, label);
        return;
    }
    SetWindowTextW(G.logs, L"Copy logs");
    int ok = G.diag_dir[0] != 0;
    lr_logzip zip = {0};
    if (ok)
        ok = lr_logzip_add(&zip, "REPORT.txt", (const uint8_t *)report, (size_t)n);
    for (unsigned player = 0; ok && player < 4; ++player)
        if (!player || d->slots[player].status != LR_D_NONE) {
            char dir[LR_PATH_MAX];
            lr_join(dir, sizeof dir, G.diag_dir, diagnostics_player(player));
            for (unsigned file = 0; ok && file <= LR_LOG_FILES; ++file) {
                const char *name = file == LR_LOG_FILES ? "FILES.txt" : lr_log_names[file];
                char path[LR_PATH_MAX], entry[80];
                lr_join(path, sizeof path, dir, name);
                uint8_t *data = NULL;
                size_t len = 0;
                PHASE("diagnostics: report ZIP");
                if (lr_read_file(path, LR_LOG_RUNTIME_MAX, &data, &len)) {
                    snprintf(entry, sizeof entry, "%s/%s", diagnostics_player(player), name);
                    ok = lr_logzip_add(&zip, entry, data, len);
                    free(data);
                } else if (d->slots[player].status == LR_D_OK &&
                           (file == LR_LOG_FILES || (G.diag_files[player] & (1u << file))))
                    ok = 0;
            }
        }
    char path[LR_PATH_MAX];
    snprintf(path, sizeof path, "%s.zip", G.diag_dir);
    ok = ok && lr_logzip_finish(&zip) && lr_write_file(path, zip.data, zip.len);
    lr_logzip_free(&zip);
    logline("automatic diagnostics: %u/%u players verified; ZIP %s: %s", received, expected, ok ? "ready" : "FAILED",
            path);
    if (!ok && G.diag_dir[0]) {
        char note[LR_PATH_MAX];
        lr_join(note, sizeof note, G.diag_dir, "ZIP-FAILED.txt");
        const char text[] = "The combined ZIP could not be completed. Keep this report folder; check disk space and "
                            "permissions. No successful ZIP is claimed.\r\n";
        lr_write_file(note, (const uint8_t *)text, sizeof text - 1);
    }
    if (G.diag_open && G.diag_dir[0])
        ShellExecuteW(NULL, L"explore", W(G.diag_dir), NULL, NULL, SW_SHOWNORMAL);
    G.diag_open = 0;
}
static void diagnostics_sync(void) {
    if (!G.core.cfg.kit || !G.core.cfg.send || G.core.state == LR_ST_IDLE)
        return;
    int role = (int)G.core.role;
    uint64_t host = role == LR_ROLE_GUEST ? G.core.peer : 0;
    if ((role != LR_ROLE_HOST && role != LR_ROLE_GUEST) || (role == LR_ROLE_GUEST && !host))
        return;
    if (!G.diagnostics.kit) {
        lr_diag_config cfg = {NULL, G.core.cfg.send, diagnostics_collect, diagnostics_save, diagnostics_changed};
        uint32_t seed = (uint32_t)now_ms();
        if (G.core.cfg.random)
            G.core.cfg.random(G.core.cfg.ctx, (uint8_t *)&seed, sizeof seed);
        lr_diag_init(&G.diagnostics, &cfg, role, host, G.core.cfg.kit, seed);
    }
    if (role == LR_ROLE_HOST) {
        if (strstr(DDC_VERSION, "duo"))
            lr_diag_players(&G.diagnostics, 1);
        else if (G.core.cfg.max_guests >= 1 && G.core.cfg.max_guests <= 3)
            lr_diag_players(&G.diagnostics, (unsigned)G.core.cfg.max_guests + 1);
        uint64_t peers[3];
        for (unsigned i = 0; i < 3; ++i)
            peers[i] = G.core.guests[i].peer;
        lr_diag_peers(&G.diagnostics, peers);
    }
}
static int diagnostics_request(unsigned reason, int open_folder) {
    if (!lr_steam_lobby())
        return 0;
    diagnostics_sync();
    if (!G.diagnostics.kit)
        return 0;
    if (G.diagnostics.role == 2 && !lr_steam_is_member(G.diagnostics.host))
        return 0;
    if (open_folder && G.diagnostics.role == 1 && G.diagnostics.report && !G.diagnostics.active &&
        now_ms() - G.diagnostics.last_request < 3000 && G.diag_dir[0]) {
        ShellExecuteW(NULL, L"explore", W(G.diag_dir), NULL, NULL, SW_SHOWNORMAL);
        return 1;
    }
    if (open_folder)
        G.diag_open = 1;
    return lr_diag_request(&G.diagnostics, reason, now_ms());
}
static void diagnostics_reset(void) {
    if (G.diagnostics.active || G.diagnostics.notice_pending) {
        if (G.diagnostics.role == 1)
            for (unsigned i = 0; i < 3; ++i)
                lr_diag_left(&G.diagnostics, G.diagnostics.slots[i + 1].peer);
        else
            lr_diag_left(&G.diagnostics, G.diagnostics.host);
    }
    lr_diag_free(&G.diagnostics);
    memset(&G.diagnostics, 0, sizeof G.diagnostics);
    G.diag_open = 0;
    G.diag_keep_until = G.diag_done_at = 0;
    G.diag_folder_id = 0;
    G.diag_dir[0] = 0;
}
static void diagnostics_mark(void) {
    G.diag_scan_ready = G.diag_seen_stop = 0;
    G.diag_scan_offset = 0;
    G.diag_scan_at = 0;
    unsigned player = G.core.role == LR_ROLE_GUEST ? (unsigned)G.core.player : 0;
    char path[LR_PATH_MAX];
    lr_join(path, sizeof path, G.moddir, lr_log_names[player ? player + 1 : 1]);
    uint8_t *data = NULL;
    size_t n = 0;
    uint64_t original = 0;
    if (diagnostics_tail(path, 1, &data, &n, &original))
        G.diag_scan_offset = original;
    free(data);
    G.diag_scan_ready = 1;
}
static void diagnostics_tick(uint64_t now) {
    diagnostics_sync();
    lr_diag_tick(&G.diagnostics, now);
    int drained = !G.diagnostics.active && !G.diagnostics.notice_pending &&
                  (!G.diag_done_at || now >= G.diag_done_at + 1500); /* allow queued receipt ACK to flush */
    if (G.diag_keep_until && (drained || now > G.diag_keep_until)) {
        if (G.diagnostics.active || G.diagnostics.notice_pending) {
            if (G.diagnostics.role == 1)
                for (unsigned i = 0; i < 3; ++i)
                    lr_diag_left(&G.diagnostics, G.diagnostics.slots[i + 1].peer);
            else
                lr_diag_left(&G.diagnostics, G.diagnostics.host);
        }
        for (unsigned i = 0; i < 3; ++i)
            if (G.diagnostics.peers[i])
                lr_steam_close_session(G.diagnostics.peers[i]);
        if (G.diagnostics.host)
            lr_steam_close_session(G.diagnostics.host);
        lr_steam_leave_lobby();
        G.diag_keep_until = 0;
    }
    if (!G.diag_scan_ready || !G.game || now - G.diag_scan_at < 1000)
        return;
    G.diag_scan_at = now;
    unsigned player = G.core.role == LR_ROLE_GUEST ? (unsigned)G.core.player : 0;
    char path[LR_PATH_MAX];
    lr_join(path, sizeof path, G.moddir, lr_log_names[player ? player + 1 : 1]);
    uint8_t *data = NULL;
    size_t n = 0;
    uint64_t original = 0;
    if (!diagnostics_tail(path, LR_LOG_RUNTIME_MAX, &data, &n, &original))
        return;
    uint64_t start = original > n ? original - n : 0;
    size_t offset =
        G.diag_scan_offset > start && G.diag_scan_offset <= original ? (size_t)(G.diag_scan_offset - start) : 0;
    if (original != G.diag_scan_offset && n >= offset) {
        char *tail = malloc(n - offset + 1);
        if (tail) {
            memcpy(tail, data + offset, n - offset);
            tail[n - offset] = 0;
            if (!G.diag_seen_stop && lr_diag_stop_marker(tail)) {
                G.diag_seen_stop = 1;
                (void)diagnostics_request(LR_DIAG_DESYNC, 0);
            }
            free(tail);
        }
    }
    G.diag_scan_offset = original;
    free(data);
}

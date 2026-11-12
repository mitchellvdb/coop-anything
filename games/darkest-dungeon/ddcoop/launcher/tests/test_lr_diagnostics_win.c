/* Exercise the actual launcher adapter with disposable files, fake membership
 * and transport. No Steam initialization, game execution or real Desktop writes. */
#define wWinMain diagnostics_unused_entry
#ifdef TEST_DIAG_DUO
#define DDC_VERSION "0.5.0-duo61"
#else
#define DDC_VERSION "0.5.0-test82"
#endif
#define lr_steam_lobby diagnostics_fake_lobby
#define lr_steam_close_session diagnostics_fake_close
#define lr_steam_leave_lobby diagnostics_fake_leave
#include "../src/main.c"
#undef wWinMain
static unsigned failures, closed, left, send_count;
#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            printf("FAIL %d: %s\n", __LINE__, #x);                                                                     \
            ++failures;                                                                                                \
        }                                                                                                              \
    } while (0)
uint64_t diagnostics_fake_lobby(void) { return 1; }
void diagnostics_fake_close(uint64_t peer) {
    (void)peer;
    ++closed;
}
void diagnostics_fake_leave(void) { ++left; }
static int fake_send_(void *ctx, uint64_t peer, int channel, const uint8_t *data, size_t n) {
    (void)ctx;
    (void)peer;
    (void)channel;
    lr_msg m;
    CHECK(lr_decode(data, n, channel, &m));
    ++send_count;
    return 1;
}
static void path_(char *out, const char *base, const char *name) { lr_join(out, LR_PATH_MAX, base, name); }
static int contains_(const char *file, const char *needle) {
    uint8_t *data = NULL;
    size_t n = 0;
    if (!lr_read_file(file, 16u * 1024 * 1024, &data, &n))
        return 0;
    int found = 0;
    size_t m = strlen(needle);
    for (size_t i = 0; i + m <= n; ++i)
        if (!memcmp(data + i, needle, m)) {
            found = 1;
            break;
        }
    free(data);
    return found;
}
static void setup_(const char *root, const char *report) {
    memset(&G, 0, sizeof G);
    closed = left = send_count = 0;
    path_(G.moddir, root, "mod");
    path_(G.bin, root, "bin");
    CHECK(lr_make_dirs(G.moddir) && lr_make_dirs(G.bin));
    path_(G.diag_dir, root, report);
    CHECK(lr_make_dirs(G.diag_dir));
    G.diag_folder_id = 99;
    G.core.role = LR_ROLE_HOST;
    G.core.state = LR_ST_HOST_PLAYING;
    G.core.cfg.kit = 0x82828282;
    G.core.cfg.send = fake_send_;
    G.core.cfg.coop_slot = 3;
    lr_diag_config cfg = {NULL, fake_send_, diagnostics_collect, diagnostics_save, diagnostics_changed};
    lr_diag_init(&G.diagnostics, &cfg, LR_ROLE_HOST, 0, G.core.cfg.kit, 98);
}
static char test_folder_root[LR_PATH_MAX];
static int test_folder_id;
static HRESULT WINAPI test_known_folder(HWND w,int id,HANDLE h,DWORD flags,LPWSTR out) {
    (void)w;(void)h;(void)flags;
    CHECK(id==test_folder_id);
    return MultiByteToWideChar(CP_UTF8,0,test_folder_root,-1,out,MAX_PATH)>0?S_OK:E_FAIL;
}
static void report_location_test(const char *root) {
    HRESULT (WINAPI *original)(HWND,int,HANDLE,DWORD,LPWSTR)=g_diagnostics_folder;
    g_diagnostics_folder=test_known_folder;
    for(int guest=0;guest<2;guest++) {
        setup_(root,"location-fixture");G.diagnostics.role=guest?LR_ROLE_GUEST:LR_ROLE_HOST;
        path_(test_folder_root,root,guest?"guest-appdata":"host-desktop");
        test_folder_id=guest?CSIDL_LOCAL_APPDATA:CSIDL_DESKTOPDIRECTORY;
        diagnostics_directory(321u+(unsigned)guest);
        char expected[LR_PATH_MAX];
        if(guest)lr_join(expected,sizeof expected,test_folder_root,"DDCoop/Reports");
        else snprintf(expected,sizeof expected,"%s",test_folder_root);
        CHECK(G.diag_dir[0] && !strncmp(G.diag_dir,expected,strlen(expected)));
        CHECK(!guest || !strstr(G.diag_dir,"host-desktop"));
    }
    g_diagnostics_folder=original;
}
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    char root[LR_PATH_MAX];
    DWORD got = GetFullPathNameA(argv[1], sizeof root, root, NULL);
    if (!got || got >= sizeof root)
        return 2;
    CHECK(lr_make_dirs(root));
    report_location_test(root);
    setup_(root,"runtime-event-report");
    G.game=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,GetCurrentProcessId());CHECK(G.game!=NULL);
    overlay_command(DD_OVERLAY_SYNC_STOP,(LPARAM)(GetCurrentProcessId()+1));CHECK(!G.diag_seen_stop && !G.diagnostics.report);
    overlay_command(DD_OVERLAY_SYNC_STOP,(LPARAM)GetCurrentProcessId());
    CHECK(G.diag_seen_stop && G.diagnostics.report && G.diagnostics.reason==LR_DIAG_DESYNC);
    uint32_t event_report=G.diagnostics.report;
    overlay_command(DD_OVERLAY_SYNC_STOP,(LPARAM)GetCurrentProcessId());CHECK(G.diagnostics.report==event_report);
    CloseHandle(G.game);G.game=NULL;diagnostics_reset();
    setup_(root, "packet-report");
    char path[LR_PATH_MAX];
    path_(path, G.moddir, "DDCoop-launcher.log");
    const char log[] = "ordinary diagnostic\npassword=synthetic-secret\nlast line\n";
    CHECK(lr_write_file(path, (const uint8_t *)log, strlen(log)));
    path_(path, G.moddir, "ddcoop.ini");
    CHECK(lr_write_file(path, (const uint8_t *)"never-include-this-file", 23));
    path_(path, G.moddir, "ddcoop-fw-host.log");
    size_t big = LR_LOG_RUNTIME_MAX + 100;
    uint8_t *data = malloc(big);
    memset(data, 'x', big);
    const char tail[] = "markerless-secret-fragment\nSync stop: reason=3\npassword=synthetic-tail\nsafe tail\n";
    memcpy(data + big - sizeof tail + 1, tail, sizeof tail - 1);
    CHECK(lr_write_file(path, data, big));
    free(data);
    for (unsigned player = 1; player < 4; ++player) {
        path_(path, G.moddir, lr_log_names[player + 1]);
        CHECK(lr_write_file(path, (const uint8_t *)"solo guest diagnostic", 21));
    }
    uint8_t *pack = NULL;
    size_t n = 0;
    G.diagnostics.report = 99;
    CHECK(diagnostics_collect(NULL, &pack, &n) && n < LR_DIAG_MAX && lr_logpack_visit(pack, n, NULL, NULL));
    CHECK(diagnostics_save(NULL, 99, 0, pack, n));
    path_(path, G.diag_dir, "host/DDCoop-launcher.log");
    CHECK(contains_(path, "ordinary diagnostic") && !contains_(path, "synthetic-secret"));
    path_(path, G.diag_dir, "host/ddcoop-fw-host.log");
    CHECK(contains_(path, "Sync stop:") && !contains_(path, "markerless-secret-fragment") &&
          !contains_(path, "synthetic-tail"));
    path_(path, G.diag_dir, "host/ddcoop-fw-guest3.log");
    CHECK(lr_file_exists(path) == (strstr(DDC_VERSION, "duo") != NULL));
    path_(path, G.diag_dir, "host/FILES.txt");
    CHECK(contains_(path, "tail truncated") && contains_(path, "missing/unreadable"));
    /* A peer that bypasses sender filtering is filtered again by the host. */
    lr_logpack malicious = {0};
    const char peerlog[] = "zzzzzzzz=synthetic-peer\nsafe peer\n";
    CHECK(lr_logpack_add(&malicious, 2, 0, strlen(peerlog), (const uint8_t *)peerlog, strlen(peerlog)));
    memcpy(malicious.data + 24, "password", 8);
    CHECK(diagnostics_save(NULL, 99, 1, malicious.data, malicious.len));
    path_(path, G.diag_dir, "player-2/ddcoop-fw-guest.log");
    CHECK(contains_(path, "safe peer") && !contains_(path, "synthetic-peer"));
    malicious.data[8] = 255;
    CHECK(!diagnostics_save(NULL, 99, 2, malicious.data, malicious.len));
    lr_logpack_free(&malicious);
    G.diagnostics.slots[0].status = LR_D_OK;
    G.diagnostics.slots[1].status = LR_D_OK;
    G.diagnostics.slots[2].status = LR_D_MISSING;
    diagnostics_changed(NULL, 1);
    snprintf(path, sizeof path, "%s.zip", G.diag_dir);
    CHECK(lr_file_exists(path));
    CHECK(!contains_(path, "never-include-this-file") && !contains_(path, "synthetic-secret") &&
          !contains_(path, "synthetic-peer"));
    free(pack);
    lr_diag_free(&G.diagnostics);
    /* Previous-session Sync stop is ignored; a newly appended one starts the
     * report once, through the real bounded detector. */
    setup_(root, "detected-report");
    G.game = (HANDLE)(uintptr_t)1;
    diagnostics_mark();
    diagnostics_tick(now_ms() + 2000);
    CHECK(!G.diagnostics.report);
    path_(path, G.moddir, "ddcoop-fw-host.log");
    FILE *f = fopen(path, "ab");
    CHECK(f != NULL);
    if (f) {
        fputs("\nSync stop: reason=2 role=host peer=0\n", f);
        fclose(f);
    }
    diagnostics_tick(now_ms() + 4000);
    CHECK(G.diagnostics.report == 99 && G.diagnostics.slots[0].status == LR_D_OK);
    diagnostics_tick(now_ms() + 6000);
    CHECK(G.diagnostics.report == 99);
    G.game = NULL;
    lr_diag_free(&G.diagnostics);
    /* End gameplay/recovery immediately, retain only diagnostic transport.
     * Its deadline produces an explicit missing-player report, then leaves. */
    setup_(root, "closing-report");
    G.core.guests[0].peer = 102;
    diagnostics_sync();
    CHECK(lr_diag_request(&G.diagnostics, LR_DIAG_FAILURE, now_ms()));
    CHECK(G.diagnostics.active);
    stop_session("fixture game exited");
    CHECK(G.core.state == LR_ST_IDLE && G.diag_keep_until && !closed && !left);
    diagnostics_tick(G.diag_keep_until + 1);
    CHECK(!G.diag_keep_until && !G.diagnostics.active && closed == 1 && left == 1);
    path_(path, G.diag_dir, "REPORT.txt");
    CHECK(contains_(path, "MISSING"));
    diagnostics_reset();
    setup_(root, "pending-guest-report");
    G.core.role = LR_ROLE_GUEST;
    G.core.peer = 101;
    lr_diag_free(&G.diagnostics);
    lr_diag_config guestcfg = {NULL, fake_send_, diagnostics_collect, diagnostics_save, diagnostics_changed};
    lr_diag_init(&G.diagnostics, &guestcfg, LR_ROLE_GUEST, 101, G.core.cfg.kit, 98);
    CHECK(lr_diag_request(&G.diagnostics, LR_DIAG_FAILURE, now_ms()) && G.diagnostics.notice_pending);
    stop_session("fixture guest exited before request");
    CHECK(G.diag_keep_until && !left && !closed);
    diagnostics_tick(G.diag_keep_until + 1);
    CHECK(!G.diagnostics.notice_pending && !G.diag_keep_until && left == 1 && closed == 1);
    diagnostics_reset();
    printf("Windows diagnostics adapter: actual allowlist/tail/partial-line and double redaction, safe grouped ZIP, "
           "new-only desync trigger, non-blocking gameplay stop and bounded transport cleanup: %s (%u failures)\n",
           failures ? "FAILED" : "PASS", failures);
    return failures != 0;
}

/* DDCoop.exe: the co-op window for Darkest Dungeon (Windows). It runs next to the game, never inside it.
 *
 * It checks the install, lets you host or join a Steam friend, hands the session to lr_core (password,
 * campaign, ddcoop.ini), relays the co-op runtime's connection over Steam (lr_steam) and starts the game.
 * The hero list on the right is where each player ticks the heroes they command in the dungeon (lr_owners).
 * F9 brings the window up over the game. Everything runs on the window thread from one 10 ms timer. */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <bcrypt.h>
#include <commctrl.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <shlobj.h>
#include <tlhelp32.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lr_core.h"
#include "lr_restart.h"
#include "lr_hashes.h"
#include "lr_install.h"
#include "lr_snapshot.h"
#include "lr_prepare_win.h"
#include "lr_steam.h"
#include "lr_mascot.h"
#include "lr_logs.h"
#include "lr_diagnostics.h"
#include "lr_logpack.h"
#include "lr_dlc.h"
#include "lr_kit.h"
#include "lr_kitsig_win.h"

/* How long a session waits for Steam to serve a save the game just wrote (LR_PREPARE_UNREAD), trying every 3 s. */
#define LR_STEAM_WAIT_MS 90000u

#ifndef DDC_VERSION
#define DDC_VERSION "dev"
#endif

/* Optional research build only. Normal kits save host progress; guests always use -nosave. */
#ifndef DDC_TEST_NOSAVE
#define DDC_TEST_NOSAVE 0
#endif

enum {
    ID_HOST = 100, ID_JOIN, ID_INVITE, ID_JOIN_CODE, ID_COPY_CODE, ID_STOP, ID_LOGS, ID_FRIENDS, ID_CODE, ID_TIMER,
    ID_HOTKEY, ID_HEROES, ID_SLOT, ID_SAVE, ID_UNDO, ID_PLAYERS, ID_ASSIGN_PLAYER, ID_ASSIGN_HERO, ID_RESYNC,
    ID_NAME,
};

enum { CHK_GAME, CHK_VERSION, CHK_MOD, CHK_STEAM, CHK_SLOT, CHK_LOBBY, CHK_FRIEND, CHK_RUNTIME, CHK_COUNT };

static struct {
    HWND wnd, status, checks, friends, code, host, join, invite, join_code, copy_code, stop, logs, hint, slot;
    HWND save_box, undo, players_box, assign_player, assign_button, resync;
    HWND name_box, players_line; /* launcher/NAMES.md */
    int name_filling;
    char players_shown[400];     /* labels on screen (players line and assign list); "" = redraw */
    int resync_closing;
    uint64_t resync_close_at;
    uint32_t assign_hero;
    int players; /* the host's choice: 2..4 players in a session */
    int saving;                 /* host: the game saves its progress in the chosen slot (guests never save) */
    int prepare_undo;           /* the running helper puts the chosen slot back as it was before a session */
    int chosen_slot, slot_busy; /* The player's own choice; a guest's session replaces cfg.coop_slot for its duration. */
    int new_campaign_slot;      /* The game is running alone, to make or finish the campaign in this slot. */
    HWND heroes, heroes_hint;
    HFONT font, big, party_font;
    char root[LR_PATH_MAX];      /* the game folder */
    char bin[LR_PATH_MAX];       /* _windows\win64 */
    char moddir[LR_PATH_MAX];    /* mods\DDCoop */
    char log_path[LR_PATH_MAX];
    FILE *log;
    int ok[CHK_COUNT];
    char check_text[CHK_COUNT][200];
    int install_ok, steam_ok;
    lr_core core;
    lr_restart restart;
    lr_friend friends_list[256];
    int friend_count;
    HANDLE game;
    HANDLE prepare;
    int prepare_restore, prepare_nosave, restore_failed, want_close;
    uint64_t prepare_since;
    char prepare_report[LR_PATH_MAX];
    char prepare_bundle[LR_PATH_MAX]; /* the session snapshot, for another try while Steam still takes in a save */
    uint64_t prepare_retry_at, prepare_wait_since;
    char auto_logdir[LR_PATH_MAX];
    lr_diagnostics diagnostics;
    char diag_dir[LR_PATH_MAX];
    uint32_t diag_folder_id;
    unsigned diag_files[4];
    int diag_open,diag_scan_ready,diag_seen_stop;
    uint64_t diag_keep_until,diag_done_at,diag_scan_offset,diag_scan_at;
    int hosting_pending;         /* host: waiting for the lobby before starting the game */
    uint64_t hosting_since;
    char last_status[512];
    size_t host_log_start;
    char runtime_line[200];
    uint64_t last_friends, last_runtime, last_ui;
    int hotkey_ok;
    int in_tick;                 /* a message box inside the tick would re-enter Steam's callback loop */
    char deferred[512];
    UINT deferred_icon;
    /* hero list */
    uint32_t heroes_shown;       /* table revision on screen; 0xFFFFFFFF = redraw */
    int heroes_role, heroes_filling, party_count;
    int live_party_count;
    uint32_t live_party_ids[4];
    uint8_t party_flags[LR_MAX_HEROES];
    char heroes_hint_text[300];
    uint64_t heroes_hint_until, heroes_recheck_at;
    uint32_t roster_crc;         /* host: last persist.roster.json read from disk */
    uint64_t last_roster;
    /* kit updates (UPDATE.md): our own installed, verified kit; the host's offer is asked about once */
    lr_kit_bundle own_kit;
    int update_asked, update_prompt;
} G;

/* ---- helpers ---- */

static uint64_t now_ms(void) { return GetTickCount64(); }

static int g_dpi = 96;
static int D(int px) { return MulDiv(px, g_dpi, 96); } /* layout is in 96-dpi pixels */

static wchar_t *W(const char *s) {
    static wchar_t buf[4][2048];
    static int next;
    wchar_t *w = buf[next++ & 3];
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, 2048);
    return w;
}

static void to_utf8(const wchar_t *w, char *out, size_t cap) {
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out, (int)cap, NULL, NULL);
}

static void logline(const char *fmt, ...) {
    if (!G.log) return;
    char line[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(G.log, "%02d:%02d:%02d.%03d %s\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, line);
    fflush(G.log);
}

/* Watchdog. The window thread marks every step with PHASE(); if no step is marked for 4 s the watchdog
 * thread suspends it for a moment, copies its registers and stack, and logs the phase plus the code
 * addresses (module + offset) found there, so a hang on someone else's PC shows exactly where it is. */
static volatile LONG64 g_beat;
static const char *volatile g_phase = "start";
static HANDLE g_gui_thread;
#define PHASE(name)                                                                                                    \
    do {                                                                                                               \
        g_phase = (name);                                                                                              \
        g_beat = (LONG64)GetTickCount64();                                                                             \
    } while (0)

static void log_address(const char *what, DWORD64 addr) {
    HMODULE mod = NULL;
    wchar_t wname[MAX_PATH] = L"?";
    char name[MAX_PATH];
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCWSTR)(uintptr_t)addr, &mod)) {
        GetModuleFileNameW(mod, wname, MAX_PATH);
        wchar_t *base = wcsrchr(wname, L'\\');
        to_utf8(base ? base + 1 : wname, name, sizeof name);
        logline("    %s %s+0x%llx", what, name, (unsigned long long)(addr - (DWORD64)(uintptr_t)mod));
    }
}

static DWORD WINAPI watchdog(void *arg) {
    (void)arg;
    int reported = 0;
    for (;;) {
        Sleep(1000);
        uint64_t age = GetTickCount64() - (uint64_t)g_beat;
        if (age < 2000) {
            if (reported) logline("watchdog: window responds again (phase %s)", g_phase);
            reported = 0;
            continue;
        }
        if (age < 4000 || reported) continue;
        reported = 1;
        CONTEXT ctx;
        memset(&ctx, 0, sizeof ctx);
        ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
        static DWORD64 stack[2048];
        size_t words = 0;
        if (SuspendThread(g_gui_thread) != (DWORD)-1) {
            /* only copy while it is suspended: logging could need a lock the window thread holds */
            if (GetThreadContext(g_gui_thread, &ctx)) {
                MEMORY_BASIC_INFORMATION mbi;
                if (VirtualQuery((void *)(uintptr_t)ctx.Rsp, &mbi, sizeof mbi)) {
                    uintptr_t end = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
                    words = (end - (uintptr_t)ctx.Rsp) / 8;
                    if (words > 2048) words = 2048;
                    memcpy(stack, (void *)(uintptr_t)ctx.Rsp, words * 8);
                }
            }
            ResumeThread(g_gui_thread);
        }
        logline("watchdog: WINDOW FROZEN for %llu s in phase '%s'", (unsigned long long)(age / 1000), g_phase);
        log_address("rip", ctx.Rip);
        int shown = 0;
        for (size_t i = 0; i < words && shown < 40; i++) {
            HMODULE mod;
            if (stack[i] > 0x10000 &&
                GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   (LPCWSTR)(uintptr_t)stack[i], &mod)) {
                char what[32];
                snprintf(what, sizeof what, "stack+%03zx", i * 8);
                log_address(what, stack[i]);
                shown++;
            }
        }
    }
    return 0;
}

static void log_cb(void *ctx, const char *line) {
    (void)ctx;
    logline("%s", line);
}

static void message(const char *text, UINT icon) {
    if (G.in_tick) { /* shown when the tick is done; ticks keep running while it is open */
        snprintf(G.deferred, sizeof G.deferred, "%s", text);
        G.deferred_icon = icon;
        return;
    }
    MessageBoxW(G.wnd, W(text), L"DDCoop", MB_OK | icon);
}

static int sha256_file(const char *path, char hex[65]) { return lr_sha256_file(path, hex); }

static void random_cb(void *ctx, uint8_t *buf, size_t len) {
    (void)ctx;
    BCryptGenRandom(NULL, buf, (ULONG)len, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
}

static int darkest_running(void) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe = {.dwSize = sizeof pe};
    int found = 0;
    for (BOOL more = Process32FirstW(snap, &pe); more; more = Process32NextW(snap, &pe))
        if (_wcsicmp(pe.szExeFile, L"Darkest.exe") == 0) found = 1;
    CloseHandle(snap);
    return found;
}

static int game_running_cb(void *ctx) {
    (void)ctx;
    return darkest_running();
}

/* Data readiness only: a package directory alone does not prove its feature files exist. */
static int has_dlc_cb(void *ctx, const char *name) {
    (void)ctx;
    return lr_dlc_present(G.root, name);
}

/* Campaign mods: Workshop items next to the game's steamapps folder, local mods in <game>\mods. */
static void mod_print_cb(void *ctx, const lr_ugc *mod, lr_mod_print *out) {
    (void)ctx;
    lr_mods_print(G.root, mod, out);
}

static void set_check(int i, int ok, const char *fmt, ...) {
    G.ok[i] = ok;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(G.check_text[i], sizeof G.check_text[i], fmt, ap);
    va_end(ap);
}

/* ---- install checks ---- */

static void parent_dir(char *path) {
    char *s = strrchr(path, '\\');
    if (s) *s = 0;
}

static void find_paths(void) {
    wchar_t exe[LR_PATH_MAX];
    GetModuleFileNameW(NULL, exe, LR_PATH_MAX);
    char p[LR_PATH_MAX];
    to_utf8(exe, p, sizeof p);
    parent_dir(p); /* ...\mods\DDCoop\launcher */
    parent_dir(p); /* ...\mods\DDCoop */
    snprintf(G.moddir, sizeof G.moddir, "%s", p);
    parent_dir(p); /* ...\mods */
    parent_dir(p); /* game root */
    snprintf(G.root, sizeof G.root, "%s", p);
    lr_join(G.bin, sizeof G.bin, G.root, "_windows\\win64");
}

static void check_install(void) {
    char exe[LR_PATH_MAX], hex[65];
    lr_join(exe, sizeof exe, G.bin, "Darkest.exe");
    if (!lr_file_exists(exe)) {
        set_check(CHK_GAME, 0, "Game not found. DDCoop belongs in <Darkest Dungeon>\\mods\\DDCoop\\launcher. Run "
                               "Install DDCoop.bat again.");
        set_check(CHK_VERSION, 0, "Version: unable to verify");
        set_check(CHK_MOD, 0, "Mod files: unable to verify");
        return;
    }
    set_check(CHK_GAME, 1, "Game found: %s", G.root);
    if (sha256_file(exe, hex) && strcmp(hex, LR_HASH_GAME) == 0)
        set_check(CHK_VERSION, 1, "Game version verified");
    else
        set_check(CHK_VERSION, 0, "This Darkest Dungeon version is not supported. Was there a Steam update? "
                                  "An updated DDCoop build is required.");
    char boot[LR_PATH_MAX], orig[LR_PATH_MAX], runtime[LR_PATH_MAX], h2[65];
    lr_join(boot, sizeof boot, G.bin, "SDL2.dll");
    lr_join(orig, sizeof orig, G.bin, "SDL2_orig.dll");
    lr_join(runtime, sizeof runtime, G.moddir, "ddcoop-runtime.dll");
    int repaired = lr_repair_bootstrap(G.root, G.moddir);
    if (repaired) logline("automatic bootstrap repair: %s", repaired > 0 ? "verified" : "could not complete");
    int boot_ok = sha256_file(boot, hex) && strcmp(hex, LR_HASH_BOOTSTRAP) == 0;
    int rt_ok = sha256_file(runtime, h2) && strcmp(h2, LR_HASH_RUNTIME) == 0;
    int orig_ok = sha256_file(orig, h2) && strcmp(h2, LR_HASH_SDL2_ORIGINAL) == 0;
    if (boot_ok && rt_ok && orig_ok)
        set_check(CHK_MOD, 1, "Mod files complete");
    else
        set_check(CHK_MOD, 0, "Mod files %s. Run Repair DDCoop.bat (for example after a Steam update or "
                              "'Verify integrity').",
                  !boot_ok ? "missing (SDL2.dll was restored)" : !rt_ok ? "corrupted (runtime)" : "missen (SDL2_orig.dll)");
    G.install_ok = G.ok[CHK_GAME] && G.ok[CHK_VERSION] && G.ok[CHK_MOD];
}

static int steam_userdata_remote(char *out, size_t cap) {
    wchar_t path[LR_PATH_MAX];
    DWORD size = sizeof path;
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", RRF_RT_REG_SZ, NULL, path, &size) != 0)
        return 0;
    char steam[LR_PATH_MAX];
    to_utf8(path, steam, sizeof steam);
    for (char *c = steam; *c; c++)
        if (*c == '/') *c = '\\';
    char user[16];
    snprintf(user, sizeof user, "%lu", (unsigned long)(lr_steam_me() & 0xFFFFFFFFu));
    static const char middle[] = "\\userdata\\";
    static const char tail[] = "\\262060\\remote";
    size_t steam_len = strlen(steam), user_len = strlen(user);
    if (steam_len + sizeof middle - 1 + user_len + sizeof tail > cap) return 0;
    memcpy(out, steam, steam_len);
    memcpy(out + steam_len, middle, sizeof middle - 1);
    memcpy(out + steam_len + sizeof middle - 1, user, user_len);
    memcpy(out + steam_len + sizeof middle - 1 + user_len, tail, sizeof tail);
    return 1;
}

static void log_save_folders(const char *remote) {
    char pattern[LR_PATH_MAX];
    lr_join(pattern, sizeof pattern, remote, "*");
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(W(pattern), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        logline("save folder root does not exist (no Steam saves for Darkest Dungeon on this account?)");
        return;
    }
    do {
        char name[LR_PATH_MAX];
        to_utf8(fd.cFileName, name, sizeof name);
        if (name[0] != '.') logline("  in remote: %s%s", name, (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? "\\" : "");
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

/* ---- which campaign to host ---- */

static void check_slot(void);
static int session_active(void);
static int slot_cannot_host(int slot, char *why, size_t cap);
static void stop_session(const char *why);
static int diagnostics_request(unsigned reason,int open_folder);
static void diagnostics_reset(void);
static void diagnostics_sync(void);
static void diagnostics_mark(void);
static void diagnostics_tick(uint64_t now);
static int spawn_game_process(const char *exe, const char *args, HANDLE *process);

static void slot_path(char *path, size_t cap) { lr_join(path, cap, G.moddir, "ddcoop-slot.txt"); }
static int slot_remembered(void) {
    char path[LR_PATH_MAX];
    uint8_t *data = NULL;
    size_t len = 0;
    int slot = 0;
    slot_path(path, sizeof path);
    if (lr_read_file(path, 16, &data, &len)) {
        if (len >= 1 && data[0] >= '1' && data[0] <= '9') slot = data[0] - '0';
        free(data);
    }
    return slot;
}
static void slot_remember(int slot) {
    char path[LR_PATH_MAX];
    uint8_t digit = (uint8_t)('0' + slot);
    slot_path(path, sizeof path);
    lr_write_file(path, &digit, 1);
}
/* Host saving: the player's choice, remembered next to the slot. A missing file means the kit's default. */
/* The number of players the host plays with (2..4), remembered like the slot. */
static void players_path(char *path, size_t cap) { lr_join(path, cap, G.moddir, "ddcoop-players.txt"); }
static int players_remembered(void) {
    char path[LR_PATH_MAX];
    uint8_t *data;
    size_t len = 0;
    int n = 2;
    players_path(path, sizeof path);
    if (lr_read_file(path, 64, &data, &len)) {
        if (len >= 1 && data[0] >= '2' && data[0] <= '4') n = data[0] - '0';
        free(data);
    }
    return n;
}
static void players_remember(int n) {
    char path[LR_PATH_MAX], text[4] = {(char)('0' + n), '\r', '\n', 0};
    players_path(path, sizeof path);
    lr_write_file(path, (const uint8_t *)text, 3);
}
/* The player's own name (launcher/NAMES.md), UTF-8, remembered like the slot. */
static void name_path(char *path, size_t cap) { lr_join(path, cap, G.moddir, "ddcoop-name.txt"); }
static void name_remembered(char out[LR_PLAYER_NAME_MAX + 1]) {
    char path[LR_PATH_MAX], raw[128] = "";
    uint8_t *data = NULL;
    size_t len = 0;
    name_path(path, sizeof path);
    if (lr_read_file(path, 256, &data, &len)) {
        if (len >= 3 && data[0] == 0xef && data[1] == 0xbb && data[2] == 0xbf) memmove(data, data + 3, len -= 3);
        while (len && (data[len - 1] == '\n' || data[len - 1] == '\r')) len--;
        if (len >= sizeof raw) len = sizeof raw - 1;
        memcpy(raw, data, len);
        raw[len] = 0;
        free(data);
    }
    lr_player_name_clean(raw, out);
}
static void name_remember(const char *name) {
    char path[LR_PATH_MAX];
    name_path(path, sizeof path);
    lr_write_file(path, (const uint8_t *)name, strlen(name));
}
/* What the others are called on this screen: their name, else "host" / "player N". */
static const char *player_label(int player) {
    static char out[4][LR_PLAYER_NAME_MAX + 16];
    static int next;
    char *o = out[next++ & 3];
    const char *name = lr_core_player_name(&G.core, player);
    if (name[0]) snprintf(o, sizeof out[0], "%s", name);
    else if (player == 0) snprintf(o, sizeof out[0], "host");
    else snprintf(o, sizeof out[0], "player %d", player + 1);
    return o;
}
static void saving_path(char *path, size_t cap) { lr_join(path, cap, G.moddir, "ddcoop-save.txt"); }
static int saving_remembered(void) {
    char path[LR_PATH_MAX];
    uint8_t *data = NULL;
    size_t len = 0;
    int on = !DDC_TEST_NOSAVE;
    saving_path(path, sizeof path);
    if (lr_read_file(path, 16, &data, &len)) {
        if (len >= 1 && (data[0] == '0' || data[0] == '1')) on = data[0] == '1';
        free(data);
    }
    return on;
}
static void saving_remember(int on) {
    char path[LR_PATH_MAX];
    uint8_t digit = (uint8_t)(on ? '1' : '0');
    saving_path(path, sizeof path);
    lr_write_file(path, &digit, 1);
}
/* The dropdown: what is in the save folder now, with the name the player gave it. */
static void slot_fill(void) {
    lr_campaign_entry list[9];
    if (!G.slot) return;
    lr_list_campaigns(G.core.cfg.remote_dir, list);
    SendMessageW(G.slot, CB_RESETCONTENT, 0, 0);
    for (int i = 0; i < 9; i++) {
        wchar_t name[64] = L"", line[128];
        MultiByteToWideChar(CP_UTF8, 0, list[i].estate, -1, name, 64);
        if (list[i].empty) swprintf(line, 128, L"Slot %d: leeg", list[i].slot);
        else if (list[i].unreadable) swprintf(line, 128, L"Slot %d: unreadable", list[i].slot);
        else swprintf(line, 128, L"Slot %d: %ls%ls", list[i].slot, name[0] ? name : L"(naamloos)", list[i].tutorial ? L"  (tutorial not completed)" : L"");
        LRESULT at = SendMessageW(G.slot, CB_ADDSTRING, 0, (LPARAM)line);
        SendMessageW(G.slot, CB_SETITEMDATA, (WPARAM)at, list[i].slot);
    }
    /* Nothing chosen yet: slot 3 was the old fixed place, otherwise the first campaign there is. A choice stays, also of an empty slot. */
    if (!G.chosen_slot) {
        G.chosen_slot = 3;
        if (list[2].empty || list[2].unreadable)
            for (int i = 8; i >= 0; i--)
                if (!list[i].empty && !list[i].unreadable) G.chosen_slot = list[i].slot;
    }
    SendMessageW(G.slot, CB_SETCURSEL, (WPARAM)(G.chosen_slot - 1), 0);
    if (!session_active()) G.core.cfg.coop_slot = G.chosen_slot;
}
/* An empty or unreadable slot has nothing to send to a friend. */
static int slot_cannot_host(int slot, char *why, size_t cap) {
    lr_campaign_entry list[9];
    lr_list_campaigns(G.core.cfg.remote_dir, list);
    if (slot < 1 || slot > 9 || (!list[slot - 1].empty && !list[slot - 1].unreadable)) return 0;
    if (why) snprintf(why, cap, "The save in slot %d is unreadable. Choose another slot, or open this slot through Steam first.", slot);
    return 1;
}
/* A campaign is made by the game itself, alone, and saved as always: its tutorial comes before there is anything to share. Only this process gets to know the slot. */
static void new_campaign_start(int slot) {
    char exe[LR_PATH_MAX];
    wchar_t value[2] = {(wchar_t)(L'0' + slot), 0};
    int ok;
    stop_session("new campaign");
    G.core.cfg.coop_slot = slot;
    lr_write_ini(G.core.cfg.ini_path, "off", LR_DEFAULT_GAME_PORT, "unused-until-ddcoop-starts", slot);
    lr_join(exe, sizeof exe, G.bin, "Darkest.exe");
    SetEnvironmentVariableW(L"DDCOOP_NEW_SLOT", value);
    ok = spawn_game_process(exe, "", &G.game);
    SetEnvironmentVariableW(L"DDCOOP_NEW_SLOT", NULL);
    if (!ok) return;
    G.new_campaign_slot = slot;
    logline("game started alone for a campaign in slot %d (pid %lu)", slot, GetProcessId(G.game));
    snprintf(G.core.status, sizeof G.core.status, "Create a campaign in slot %d and complete the tutorial to reach the Hamlet. Close the game, then click Host.", slot);
}
/* Host on a slot without a playable campaign yet. 1 = dealt with here. */
static int new_campaign_offer(int slot) {
    lr_campaign_entry list[9];
    wchar_t text[400];
    lr_list_campaigns(G.core.cfg.remote_dir, list);
    if (slot < 1 || slot > 9 || (!list[slot - 1].empty && !list[slot - 1].tutorial)) return 0;
    if (list[slot - 1].empty)
        swprintf(text, 400, L"Slot %d is empty. Create a new campaign?\n\nThe game starts without your friend. Choose a name and finish the tutorial to reach the Hamlet; progress is saved in slot %d. Close the game afterwards and click Host.", slot, slot);
    else
        swprintf(text, 400, L"The campaign in slot %d is still in the tutorial. Continue playing?\n\nThe game starts without your friend. Complete the tutorial to reach the Hamlet; progress is saved. Close the game afterwards and click Host.", slot);
    if (MessageBoxW(G.wnd, text, L"DDCoop", MB_YESNO | MB_ICONQUESTION) == IDYES) new_campaign_start(slot);
    return 1;
}
static void slot_chosen(void) {
    LRESULT at = SendMessageW(G.slot, CB_GETCURSEL, 0, 0);
    if (at == CB_ERR) return;
    G.chosen_slot = (int)SendMessageW(G.slot, CB_GETITEMDATA, (WPARAM)at, 0);
    G.core.cfg.coop_slot = G.chosen_slot;
    slot_remember(G.chosen_slot);
    logline("chosen campaign: slot %d", G.chosen_slot);
    check_slot();
}

static void check_slot(void) {
    char name[LR_NAME_MAX + 1];
    lr_game_info game;
    if (G.core.cfg.remote_dir[0] && lr_find_save_folder(G.core.cfg.remote_dir, G.core.cfg.coop_slot, name, sizeof name) && !slot_cannot_host(G.core.cfg.coop_slot, NULL, 0)) {
        if (lr_read_game_info(G.core.cfg.remote_dir, name, &game) && (game.text || game.tutorial))
            set_check(CHK_SLOT, 1, "Campaign in slot %d is still in the tutorial. To host it, complete the tutorial first: click Host.",
                      G.core.cfg.coop_slot);
        else
            set_check(CHK_SLOT, 1, "Co-op campaign found in slot %d (%s)", G.core.cfg.coop_slot, name);
    } else
        set_check(CHK_SLOT, 1, "Slot %d is empty. To host, create a campaign there first: click Host.", G.core.cfg.coop_slot);
}

/* ---- the game ---- */

static int restore_pending(void) { return G.core.cfg.restore_path[0] && lr_file_exists(G.core.cfg.restore_path); }

/* Both the preparation child and the game must use the real game's Steam identity.
 * The lobby launcher remains in its own AppID 480 process. */
static int spawn_game_process(const char *exe, const char *args, HANDLE *process) {
    /* Our environment says SteamAppId=480 (Spacewar); the game must not inherit that. */
    wchar_t *env = GetEnvironmentStringsW(), *out = calloc(65536, sizeof(wchar_t));
    if (!env || !out) {
        if (env) FreeEnvironmentStringsW(env);
        free(out);
        return 0;
    }
    size_t n = 0;
    for (wchar_t *p = env; *p; p += wcslen(p) + 1) {
        if (_wcsnicmp(p, L"SteamAppId=", 11) == 0 || _wcsnicmp(p, L"SteamGameId=", 12) == 0 ||
            _wcsnicmp(p, L"SteamOverlayGameId=", 19) == 0)
            continue;
        size_t len = wcslen(p) + 1;
        if (n + len + 1 >= 65536) break;
        memcpy(out + n, p, len * sizeof(wchar_t));
        n += len;
    }
    const wchar_t ids[] = L"SteamAppId=262060\0SteamGameId=262060\0";
    if (n + sizeof ids / sizeof ids[0] >= 65536) {
        free(out);
        FreeEnvironmentStringsW(env);
        return 0;
    }
    memcpy(out + n, ids, sizeof ids);
    FreeEnvironmentStringsW(env);
    char cmd[LR_PATH_MAX * 5];
    if (snprintf(cmd, sizeof cmd, "\"%s\" %s", exe, args) >= (int)sizeof cmd) {
        free(out);
        return 0;
    }
    STARTUPINFOW si = {.cb = sizeof si};
    PROCESS_INFORMATION pi;
    wchar_t wcmd[LR_PATH_MAX * 5], wexe[LR_PATH_MAX], wbin[LR_PATH_MAX];
    if (!MultiByteToWideChar(CP_UTF8, 0, cmd, -1, wcmd, LR_PATH_MAX * 5)) {
        free(out);
        return 0;
    }
    wcscpy(wexe, W(exe));
    wcscpy(wbin, W(G.bin));
    BOOL ok = CreateProcessW(wexe, wcmd, NULL, NULL, FALSE, CREATE_UNICODE_ENVIRONMENT, out, wbin, &si, &pi);
    free(out);
    if (!ok) {
        logline("CreateProcess failed: %lu", GetLastError());
        message("Could not start Darkest Dungeon. See DDCoop-launcher.log.", MB_ICONERROR);
        return 0;
    }
    CloseHandle(pi.hThread);
    *process = pi.hProcess;
    return 1;
}

/* restore: 1 = a guest's own campaign back after a session; 2 = the player asked to put a slot back from a backup. */
static int begin_prepare(const char *bundle, int restore, int nosave) {
    if (G.prepare || darkest_running()) return 0;
    char exe[LR_PATH_MAX], backup[LR_PATH_MAX], name[96], args[LR_PATH_MAX * 4];
    wchar_t wexe[LR_PATH_MAX];
    GetModuleFileNameW(NULL, wexe, LR_PATH_MAX);
    to_utf8(wexe, exe, sizeof exe);
    if (!lr_make_dirs(G.core.cfg.backup_root)) return 0;
    SYSTEMTIME t;
    GetLocalTime(&t);
    snprintf(name, sizeof name, "Steam-%s-%04u%02u%02u-%02u%02u%02u-%llu.bin",
             restore == 2 ? "undo" : restore ? "restore" : "prepare", t.wYear,
             t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, (unsigned long long)now_ms());
    lr_join(backup, sizeof backup, G.core.cfg.backup_root, name);
    lr_join(G.prepare_report, sizeof G.prepare_report, G.moddir, restore ? "ddcoop-restore.log" : "ddcoop-prepare.log");
    if (snprintf(args, sizeof args, "--prepare-save \"%s\" \"%s\" \"%s\" \"%s\"", G.bin, bundle, backup,
                 G.prepare_report) >= (int)sizeof args)
        return 0;
    /* A failed spawn must not leave a previous successful report behind. */
    lr_delete_file(G.prepare_report);
    if (!spawn_game_process(exe, args, &G.prepare)) return 0;
    G.prepare_restore = restore == 1;
    G.prepare_undo = restore == 2;
    G.prepare_nosave = nosave;
    G.prepare_since = now_ms();
    snprintf(G.core.status, sizeof G.core.status,
             restore == 2 ? "Restoring the backup through Steam. Please wait..."
             : restore    ? "Restoring your own campaign automatically. Please wait..."
                          : "Preparing and verifying the campaign through Steam. Please wait...");
    logline("Steam %s helper started; game stays closed", restore == 2 ? "undo" : restore ? "restore" : "prepare");
    return 1;
}

static int launch_game(int nosave) {
    check_install();
    if (!G.install_ok) return 0;
    G.auto_logdir[0]=0;
    diagnostics_mark();
    if (G.prepare || G.game || darkest_running()) {
        message("Darkest Dungeon is running, or DDCoop is preparing/restoring a campaign. Please wait.",
                MB_ICONWARNING);
        return 0;
    }
    lr_snapshot *s = calloc(1, sizeof *s);
    if (!s) return 0;
    s->user = lr_steam_me();
    snprintf(s->folder, sizeof s->folder, "%s", G.core.save_folder);
    int ok = G.core.file_count > 0;
    for (uint32_t i = 0; ok && i < G.core.file_count; i++) {
        lr_file *f = &G.core.files[i];
        char backup[LR_SNAPSHOT_NAME_MAX + 1];
        snprintf(backup, sizeof backup, "backup/%s", f->name);
        ok = lr_snapshot_add(s, f->name, f->data, f->size) && lr_snapshot_add(s, backup, f->data, f->size);
    }
    char path[LR_PATH_MAX];
    lr_join(path, sizeof path, G.moddir, "ddcoop-session.bin");
    ok = ok && lr_snapshot_write(path, s);
    lr_snapshot_free(s);
    free(s);
    snprintf(G.prepare_bundle, sizeof G.prepare_bundle, "%s", path);
    G.prepare_retry_at = G.prepare_wait_since = 0;
    return ok && begin_prepare(path, 0, nosave);
}

/* ---- session control ---- */

static int session_active(void) { return G.core.state != LR_ST_IDLE && G.core.state != LR_ST_FAILED; }

static void stop_session(const char *why) {
    if(lr_restart_active(&G.restart)) {
        lr_restart_abort(&G.restart);
        (void)lr_restart_tick(&G.restart,now_ms(),0); /* Send ABORT before dropping transport. */
    }
    memset(&G.restart,0,sizeof G.restart);
    if (session_active() || lr_steam_lobby()) logline("stop: %s", why);
    int keep_logs=(G.diagnostics.active || G.diagnostics.notice_pending) && strcmp(why,"new session")!=0;
    if(keep_logs)G.diag_keep_until=now_ms()+60000;
    else {
        if (G.core.role == LR_ROLE_HOST) {
            for (int i = 0; i < LR_MAX_GUESTS; i++)
                if (G.core.guests[i].peer) lr_steam_close_session(G.core.guests[i].peer);
        } else if (G.core.peer) lr_steam_close_session(G.core.peer);
    }
    lr_core_stop(&G.core);
    if(!keep_logs)lr_steam_leave_lobby();
    G.hosting_pending = 0;
    G.prepare_retry_at = G.prepare_wait_since = 0;
    snprintf(G.core.status, sizeof G.core.status, "%s", why);
}

/* The newest backup DDCoop made of the chosen slot before writing a session's campaign into it (Steam-prepare-...). */
static int newest_backup(int slot, char *out, size_t cap, char *when, size_t when_cap) {
    char pattern[LR_PATH_MAX];
    WIN32_FIND_DATAW fd;
    FILETIME best = {0, 0};
    int found = 0;
    lr_join(pattern, sizeof pattern, G.core.cfg.backup_root, "Steam-prepare-*.bin");
    HANDLE h = FindFirstFileW(W(pattern), &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    lr_snapshot *snap = calloc(1, sizeof *snap);
    uint64_t me = lr_steam_me();
    do {
        char name[LR_PATH_MAX], path[LR_PATH_MAX];
        int at = 0;
        if (!snap || (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || CompareFileTime(&fd.ftLastWriteTime, &best) <= 0)
            continue;
        to_utf8(fd.cFileName, name, sizeof name);
        lr_join(path, sizeof path, G.core.cfg.backup_root, name);
        memset(snap, 0, sizeof *snap);
        if (lr_snapshot_read(path, snap) && snap->user == me && lr_slot_folder_ok(snap->folder, &at) && at == slot) {
            best = fd.ftLastWriteTime;
            snprintf(out, cap, "%s", path);
            found = 1;
        }
        lr_snapshot_free(snap);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    free(snap);
    if (found) {
        SYSTEMTIME u, t;
        FileTimeToSystemTime(&best, &u);
        SystemTimeToTzSpecificLocalTime(NULL, &u, &t);
        snprintf(when, when_cap, "%u-%u-%04u at %02u:%02u", t.wDay, t.wMonth, t.wYear, t.wHour, t.wMinute);
    }
    return found;
}

static void do_undo(void) {
    char path[LR_PATH_MAX], when[64];
    wchar_t text[600];
    if (G.prepare || restore_pending() || session_active() || G.game) {
        message("This is only available with no active session and the game closed.", MB_ICONINFORMATION);
        return;
    }
    if (!G.install_ok || !G.steam_ok) {
        message("All checks above must be green (OK) first.", MB_ICONWARNING);
        return;
    }
    if (darkest_running()) {
        message("Close Darkest Dungeon first.", MB_ICONWARNING);
        return;
    }
    if (!newest_backup(G.chosen_slot, path, sizeof path, when, sizeof when)) {
        message("DDCoop has no backup of this slot yet.", MB_ICONINFORMATION);
        return;
    }
    swprintf(text, 600, L"Restore slot %d to its state before the last DDCoop session in this slot (%hs)?\n\n"
             L"Progress saved since then will be replaced. DDCoop first keeps it as a "
             L"new backup in your DDCoop-backups folder.", G.chosen_slot, when);
    if (MessageBoxW(G.wnd, text, L"DDCoop", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return;
    logline("putting slot %d back from %s", G.chosen_slot, path);
    if (!begin_prepare(path, 2, 1)) message("Restore could not start. Nothing was changed.", MB_ICONWARNING);
}

static void host_log_mark(void) {
    char path[LR_PATH_MAX];uint8_t *data=NULL;size_t len=0;
    lr_join(path,sizeof path,G.moddir,"ddcoop-fw-host.log");
    G.host_log_start=lr_read_file(path,32u*1024*1024,&data,&len)?len:0;
    free(data);G.last_runtime=0;G.runtime_line[0]=0;G.ok[CHK_RUNTIME]=0;
}
static void do_host(void) {
    if(lr_restart_active(&G.restart)){message("Resync is in progress. Please wait, or click Stop to cancel.",MB_ICONINFORMATION);return;}
    if (G.prepare || restore_pending()) {
        message("DDCoop must restore your own campaign first. Close any guest game and wait for recovery.",
                MB_ICONINFORMATION);
        return;
    }
    if (!G.install_ok || !G.steam_ok) {
        message("All checks above must be green (OK) first.", MB_ICONWARNING);
        return;
    }
    if (darkest_running()) {
        message("Close Darkest Dungeon first. DDCoop starts the game with co-op enabled.", MB_ICONWARNING);
        return;
    }
    if (new_campaign_offer(G.chosen_slot)) return;
    {
        char why[300];
        if (slot_cannot_host(G.chosen_slot, why, sizeof why)) {
            message(why, MB_ICONINFORMATION);
            return;
        }
    }
    diagnostics_reset();
    stop_session("new session");
    G.core.cfg.coop_slot = G.chosen_slot;
    G.core.cfg.max_guests = G.players >= 2 && G.players <= 4 ? G.players - 1 : 1;
    logline("host: %d players (waits for %d friend(s) before the campaign loads)", G.core.cfg.max_guests + 1, G.core.cfg.max_guests);
    host_log_mark();
    if (!lr_core_host_start(&G.core, now_ms())) {
        message(G.core.error, MB_ICONWARNING);
        return;
    }
    if (!lr_steam_host_lobby()) {
        stop_session("Steam could not create a lobby");
        message("Steam could not create a lobby. Try again shortly.", MB_ICONWARNING);
        return;
    }
    G.hosting_pending = 1;
    G.hosting_since = now_ms();
    snprintf(G.core.status, sizeof G.core.status, "Creating lobby...");
}

static void join_lobby(uint64_t lobby) {
    if(lr_restart_active(&G.restart)){message("Resync is in progress. Please wait, or click Stop to cancel.",MB_ICONINFORMATION);return;}
    if (G.prepare || restore_pending()) {
        message("DDCoop must restore your own campaign first. Close any guest game and wait for recovery.",
                MB_ICONINFORMATION);
        return;
    }
    if (!G.install_ok || !G.steam_ok) {
        message("All checks above must be green (OK) first.", MB_ICONWARNING);
        return;
    }
    if (darkest_running()) {
        message("Close Darkest Dungeon first. DDCoop prepares the host campaign, then starts the game.",
                MB_ICONWARNING);
        return;
    }
    diagnostics_reset();
    stop_session("new session");
    if (!lr_steam_join_lobby(lobby)) {
        message("Could not open the lobby.", MB_ICONWARNING);
        return;
    }
    snprintf(G.core.status, sizeof G.core.status, "Entering host lobby...");
    logline("joining lobby %llu", (unsigned long long)lobby);
}

static lr_friend *selected_friend(void) {
    LRESULT i = SendMessageW(G.friends, LB_GETCURSEL, 0, 0);
    if (i == LB_ERR) return NULL;
    LRESULT idx = SendMessageW(G.friends, LB_GETITEMDATA, (WPARAM)i, 0);
    return (idx >= 0 && idx < G.friend_count) ? &G.friends_list[idx] : NULL;
}

static void do_join_selected(void) {
    lr_friend *f = selected_friend();
    if (!f) {
        message("Select a friend in the list first.", MB_ICONINFORMATION);
        return;
    }
    if (!f->hosting) {
        message("This friend is not hosting DDCoop yet. Ask them to click Host or invite you.",
                MB_ICONINFORMATION);
        return;
    }
    join_lobby(f->lobby);
}

static void do_invite_selected(void) {
    lr_friend *f = selected_friend();
    if (!f) {
        message("Select a friend in the list first.", MB_ICONINFORMATION);
        return;
    }
    if (G.core.role != LR_ROLE_HOST || !lr_steam_lobby()) {
        message("Click 'Host a game' first; then you can invite friends.", MB_ICONINFORMATION);
        return;
    }
    if (lr_steam_invite(f->id)) {
        char t[256];
        snprintf(t, sizeof t, "Invitation sent to %.64s. Your friend must have DDCoop open and accept the Steam invitation "
                              "(or select you and click Join).", f->name);
        message(t, MB_ICONINFORMATION);
    } else {
        message("Steam could not send the invitation.", MB_ICONWARNING);
    }
}

static void do_join_code(void) {
    wchar_t w[64];
    GetWindowTextW(G.code, w, 64);
    char s[64];
    to_utf8(w, s, sizeof s);
    char *p = s;
    while (*p == ' ') p++;
    uint64_t lobby = _strtoui64(p, NULL, 10);
    if (!lobby) {
        message("Paste the host code into the box first (digits only).", MB_ICONINFORMATION);
        return;
    }
    join_lobby(lobby);
}

static void do_copy_code(void) {
    uint64_t lobby = lr_steam_lobby();
    if (G.core.role != LR_ROLE_HOST || !lobby) {
        message("A code is available after hosting. Click 'Host a game' first.", MB_ICONINFORMATION);
        return;
    }
    char code[32];
    snprintf(code, sizeof code, "%llu", (unsigned long long)lobby);
    if (OpenClipboard(G.wnd)) {
        EmptyClipboard();
        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, 64 * sizeof(wchar_t));
        wcscpy(GlobalLock(h), W(code));
        GlobalUnlock(h);
        SetClipboardData(CF_UNICODETEXT, h);
        CloseClipboard();
    }
    char t[160];
    snprintf(t, sizeof t, "Code %s copied to clipboard. Send it to your friend to paste into the code box.", code);
    message(t, MB_ICONINFORMATION);
}

static void collect_logs(int open_folder) {
    unsigned why=open_folder?LR_DIAG_MANUAL:lr_restart_active(&G.restart)?LR_DIAG_RESYNC:LR_DIAG_FAILURE;
    if(diagnostics_request(why,open_folder))return;
    wchar_t desktop[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desktop) != S_OK) return;
    char d[LR_PATH_MAX], dir[LR_PATH_MAX], stamp[64];
    to_utf8(desktop, d, sizeof d);
    time_t t = time(NULL);
    strftime(stamp, sizeof stamp, "DDCoop-logs-%Y-%m-%d_%H%M%S", localtime(&t));
    if(!open_folder && G.auto_logdir[0])snprintf(dir,sizeof dir,"%s",G.auto_logdir);
    else {
        lr_join(dir, sizeof dir, d, stamp);
        if(!open_folder)snprintf(G.auto_logdir,sizeof G.auto_logdir,"%s",dir);
    }
    lr_make_dirs(dir);
    const char *names[] = {"DDCoop-launcher.log",
                           "ddcoop-fw-host.log",
                           "ddcoop-fw-guest.log",
                           "ddcoop-fw-guest2.log",
                           "ddcoop-fw-guest3.log",
                           "ddcoop-heroes.ini",
                           "ddcoop-prepare.log",
                           "ddcoop-restore.log",
                           "app.log",
                           "app.log.bak",
                           "steam_appid.txt"};
    if (G.log) fflush(G.log);
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        char from[LR_PATH_MAX], to[LR_PATH_MAX];
        uint8_t *data;
        size_t len;
        lr_join(from, sizeof from, i >= 8 ? G.bin : G.moddir, names[i]);
        lr_join(to, sizeof to, dir, names[i]);
        if (lr_read_file(from, 64u * 1024 * 1024, &data, &len)) {
            size_t safe_len = lr_logs_redact(data, len, NULL);
            lr_write_file(to, data, safe_len);
            free(data);
        }
    }
    char summary[4096];
    int n = snprintf(summary, sizeof summary, "DDCoop %s\r\nstate: %s\r\nstatus: %s\r\nerror: %s\r\n", DDC_VERSION,
                     lr_state_name(G.core.state), G.core.status, G.core.error);
    for (int i = 0; i < CHK_COUNT && n < (int)sizeof summary - 300; i++)
        n += snprintf(summary + n, sizeof summary - (size_t)n, "[%s] %s\r\n", G.ok[i] ? "OK" : "--", G.check_text[i]);
    char path[LR_PATH_MAX];
    lr_join(path, sizeof path, dir, "checks.txt");
    size_t safe_len = lr_logs_redact((uint8_t *)summary, strlen(summary), NULL);
    lr_write_file(path, (const uint8_t *)summary, safe_len);
    if (open_folder) ShellExecuteW(NULL, L"explore", W(dir), NULL, NULL, SW_SHOWNORMAL);
}
static void do_logs(void) { collect_logs(1); }
#include "lr_diagnostics_win.inc"

static BOOL CALLBACK close_game_window(HWND wnd, LPARAM pid) {
    DWORD owner = 0;
    GetWindowThreadProcessId(wnd, &owner);
    if (owner == (DWORD)pid && IsWindowVisible(wnd)) PostMessageW(wnd, WM_CLOSE, 0, 0);
    return TRUE;
}
static void request_game_close(void) {
    if (G.game) EnumWindows(close_game_window, (LPARAM)GetProcessId(G.game));
}

static void preparation_tick(void) {
    if (G.prepare && WaitForSingleObject(G.prepare, 0) == WAIT_OBJECT_0) {
        DWORD code = 1;
        GetExitCodeProcess(G.prepare, &code);
        CloseHandle(G.prepare);
        G.prepare = NULL;
        uint8_t *data = NULL;
        size_t len = 0;
        if (lr_read_file(G.prepare_report, 65536, &data, &len)) {
            char *text = malloc(len + 1);
            if (text) {
                memcpy(text, data, len);
                text[len] = 0;
                logline("preparation report:\n%s", text);
                free(text);
            }
            free(data);
        }
        if (G.prepare_undo) {
            G.prepare_undo = 0;
            logline("backup put back: %s (exit %lu)", code == 0 ? "verified" : "FAILED", code);
            snprintf(G.core.status, sizeof G.core.status, code == 0
                     ? "Backup restored to slot %d. The slot's previous contents were also kept."
                     : "Restore failed; slot %d was left unchanged or rolled back. The backup was retained.",
                     G.chosen_slot);
            if (code != 0) collect_logs(0);
        } else if (G.prepare_restore) {
            if (code == 0 && lr_delete_file(G.core.cfg.restore_path)) {
                G.restore_failed = 0;
                logline("guest original campaign restored through Steam and verified; recovery journal cleared");
                snprintf(G.core.status, sizeof G.core.status,
                         "Your own campaign has been restored. Ready for a new session.");
                collect_logs(0);
            } else {
                G.restore_failed = 1;
                G.want_close = 0;
                logline("guest restore failed (exit %lu); recovery journal retained", code);
                snprintf(G.core.status, sizeof G.core.status,
                         "Recovery is not complete. Your backup is safe; restart DDCoop to finish recovery.");
                message("Your own campaign could not yet be fully restored. The backup is safe. Close the "
                        "game and restart DDCoop. Send the automatically collected logs if this persists.",
                        MB_ICONWARNING);
                collect_logs(0);
            }
        } else if (code == LR_PREPARE_UNREAD && session_active() && !G.want_close &&
                   (!G.prepare_wait_since || now_ms() - G.prepare_wait_since < LR_STEAM_WAIT_MS)) {
            /* Steam is still taking in the save the game just wrote, most often right after the game closed. Nothing was
             * written; try again in a moment, and say so. */
            if (!G.prepare_wait_since) G.prepare_wait_since = now_ms();
            G.prepare_retry_at = now_ms() + 3000;
            logline("Steam does not serve slot %d yet; trying again in 3 seconds", G.core.cfg.coop_slot);
            snprintf(G.core.status, sizeof G.core.status,
                     "Steam is still saving slot %d (this may take a little while after closing the game). "
                     "DDCoop waits and retries automatically; the game starts when Steam is ready.", G.core.cfg.coop_slot);
        } else if (code != 0) {
            logline("preparation refused launch (exit %lu)", code);
            stop_session("Campaign preparation failed; the game was not started.");
            message("The campaign could not be prepared reliably. DDCoop will not start the game. Any "
                    "guest backup is restored automatically. Diagnostic logs are collected on your desktop.",
                    MB_ICONWARNING);
            collect_logs(0);
        } else if (session_active() && !G.want_close) {
            char exe[LR_PATH_MAX];
            lr_join(exe, sizeof exe, G.bin, "Darkest.exe");
            if (spawn_game_process(exe, G.prepare_nosave ? "-nosave" : "", &G.game)) {
                UnregisterHotKey(G.wnd,ID_HOTKEY); /* F9 is now handled inside SDL, without leaving the game. */
                G.runtime_line[0] = 0;
                logline("game started%s after Steam save verification (pid %lu)",
                        G.prepare_nosave ? " with -nosave" : " (host saving enabled)", GetProcessId(G.game));
                snprintf(G.core.status, sizeof G.core.status, "Campaign verified; the game starts and opens slot %d automatically.", G.core.cfg.coop_slot);
            } else
                stop_session("The game could not start; recovering the guest campaign.");
        }
    }
    if (G.prepare_retry_at && !G.prepare && now_ms() >= G.prepare_retry_at) {
        G.prepare_retry_at = 0;
        if (session_active() && !G.want_close && !G.game) {
            if (darkest_running())
                stop_session("Darkest Dungeon is running; close it, then click Host or Join again.");
            else if (!begin_prepare(G.prepare_bundle, 0, G.prepare_nosave))
                stop_session("Campaign preparation could not restart; the game was not started.");
            else
                snprintf(G.core.status, sizeof G.core.status,
                         "Steam is still saving slot %d. DDCoop will retry (%llu s)...",
                         G.core.cfg.coop_slot, (unsigned long long)((now_ms() - G.prepare_wait_since) / 1000));
        }
    }
    if (G.prepare && G.prepare_since && now_ms() - G.prepare_since > 30000) {
        G.prepare_since = 0;
        logline("Steam storage helper still running after 30 seconds; not terminating a possible save write");
        snprintf(G.core.status, sizeof G.core.status,
                 "Steam is still saving the campaign. Please wait; the game remains closed.");
    }
    if (!G.prepare && !G.game && !session_active() && restore_pending() && !G.restore_failed && G.steam_ok) {
        if (!darkest_running()) {
            if(G.core.state==LR_ST_FAILED)stop_session("Recovering your campaign after an interrupted session...");
            if (!begin_prepare(G.core.cfg.restore_path, 1, 1)) {
                G.restore_failed = 1;
                message("DDCoop could not start recovery. Your original campaign is safe. Close the game "
                        "and restart DDCoop.",
                        MB_ICONWARNING);
            }
        } else
            snprintf(G.core.status, sizeof G.core.status,
                     "Close Darkest Dungeon first; DDCoop will then restore your own campaign.");
    }
    if (G.want_close && !G.prepare && !restore_pending() && !G.game) DestroyWindow(G.wnd);
    static uint64_t close_at;
    if (G.want_close && G.game && now_ms() - close_at > 1000) {
        close_at = now_ms();
        request_game_close();
    }
}

/* ---- Steam callbacks ---- */

static int accept_session_cb(void *ctx, uint64_t peer) {
    (void)ctx;
    if (G.core.role == LR_ROLE_HOST) return lr_steam_is_member(peer);
    return peer != 0 && peer == lr_steam_lobby_owner();
}

static int allowed_cb(void *ctx, uint64_t peer) {
    (void)ctx;
    return lr_steam_is_member(peer);
}

static void on_message(void *ctx, uint64_t peer, int channel, const uint8_t *buf, size_t len) {
    (void)ctx;
    lr_msg m;
    int decoded=lr_decode(buf,len,channel,&m);
    if(decoded && m.type>=LR_DIAG_REQUEST && m.type<=LR_DIAG_NOTICE) {
        if(!lr_steam_is_member(peer))return;
        diagnostics_sync();(void)lr_diag_message(&G.diagnostics,peer,&m,now_ms());return;
    }
    if(decoded && m.type==LR_RESTART) {
        if(!lr_steam_is_member(peer))return;
        if(!G.restart.kit) {
            if(m.a!=LR_R_BEGIN || G.core.role!=LR_ROLE_GUEST || peer!=G.core.peer || peer!=lr_steam_lobby_owner())return;
            lr_restart_init(&G.restart,LR_ROLE_GUEST,peer,G.core.cfg.kit,G.core.cfg.send,G.core.cfg.ctx);
        }
        (void)lr_restart_message(&G.restart,peer,&m,now_ms());return;
    }
    lr_core_on_message(&G.core, peer, channel, buf, len, now_ms());
}

static void on_event(void *ctx, const lr_steam_event *ev) {
    (void)ctx;
    switch (ev->type) {
    case LR_EV_LOBBY_CREATED:
        if (G.hosting_pending) {
            G.hosting_pending = 0;
            if (!launch_game(!G.saving)) {
                stop_session("The game could not start.");
                return;
            }
            /* launch_game queues verified preparation; its status stays visible until the child finishes. */
        }
        break;
    case LR_EV_LOBBY_FAILED:
        logline("lobby failed: %s", ev->text);
        stop_session(ev->text);
        message(ev->text, MB_ICONWARNING);
        break;
    case LR_EV_LOBBY_ENTERED:
        if(lr_restart_active(&G.restart))break; /* GO alone starts the fresh join. */
        if (G.core.role == LR_ROLE_GUEST && G.core.peer == ev->user && session_active()) break;
        if (!lr_core_guest_join(&G.core, ev->user, now_ms())) {
            lr_steam_leave_lobby();
            message(G.core.error, MB_ICONWARNING);
        }
        break;
    case LR_EV_MEMBER_JOINED:
        logline("lobby: %llu joined", (unsigned long long)ev->user);
        break;
    case LR_EV_MEMBER_LEFT:
        lr_diag_left(&G.diagnostics,ev->user);
        if(lr_restart_active(&G.restart)) {
            int participant=G.restart.role==LR_ROLE_GUEST?G.restart.host==ev->user:0;
            for(unsigned i=0;i<3;i++)if(G.restart.peers[i] && G.restart.peers[i]==ev->user)participant=1;
            if(participant)lr_restart_abort(&G.restart);
        }
        logline("lobby: %llu left", (unsigned long long)ev->user);
        lr_core_peer_left(&G.core, ev->user);
        lr_steam_close_session(ev->user); /* half-open sessions broke a quick rejoin (1.1.0) */
        break;
    case LR_EV_JOIN_REQUESTED:
        logline("steam: join requested for lobby %llu", (unsigned long long)ev->lobby);
        if (session_active()) {
            message("A session is already active. Click Stop before joining this friend.", MB_ICONINFORMATION);
            return;
        }
        ShowWindow(G.wnd, SW_SHOWNORMAL);
        SetForegroundWindow(G.wnd);
        join_lobby(ev->lobby);
        break;
    }
}

/* ---- window contents ---- */

static void update_checks(void) {
    lr_state s = G.core.state;
    int host = G.core.role == LR_ROLE_HOST, guest = G.core.role == LR_ROLE_GUEST;
    if (lr_steam_lobby())
        set_check(CHK_LOBBY, 1, host ? "Lobby open (friends can join)" : "In the host lobby");
    else
        set_check(CHK_LOBBY, 0, "Lobby: not open (click Host or join a friend)");
    if ((host && (s == LR_ST_HOST_SENDING || s == LR_ST_HOST_PLAYING)) || (guest && s >= LR_ST_GUEST_RECEIVING && s != LR_ST_FAILED))
        set_check(CHK_FRIEND, 1, host ? "Friend connected through Steam" : "Connected to host through Steam");
    else
        set_check(CHK_FRIEND, 0, "Friend: not connected");
    if (strstr(G.runtime_line, "framework OFF"))
        set_check(CHK_RUNTIME, 0, "In-game co-op is OFF: the runtime does not fully recognize this game and makes no changes "
                                  "(the game runs normally). An updated ddcoop-runtime.dll is required.");
    else if (G.core.host_spent)
        set_check(CHK_RUNTIME, 0, "This game session has ended. Close the game and click Host again.");
    else if (strstr(G.runtime_line, "host rejected or protocol"))
        set_check(CHK_RUNTIME, 0, "The host game is not responding. Has the host loaded slot %d? If so, the host must close "
                                  "the game and click Host again.", G.core.cfg.coop_slot);
    else if (G.core.relay_up && G.core.relay.game_connected && strstr(G.runtime_line, "ready"))
        set_check(CHK_RUNTIME, 1, "In-game co-op active. Both players should load slot %d.", G.core.cfg.coop_slot);
    else if (G.core.relay_up && G.core.relay.game_connected)
        set_check(CHK_RUNTIME, 1, "Games connected through DDCoop (%s)", G.runtime_line[0] ? G.runtime_line : "waiting for runtime");
    else
        set_check(CHK_RUNTIME, 0, "Games: not connected%s%s", G.runtime_line[0] ? " - last message: " : "",
                  G.runtime_line);

    char text[4096];
    int n = 0;
    for (int i = 0; i < CHK_COUNT; i++)
        n += snprintf(text + n, sizeof text - (size_t)n, "%s  %s\r\n", G.ok[i] ? "[OK]" : "[ - ]", G.check_text[i]);
    wchar_t old[4096];
    GetWindowTextW(G.checks, old, 4096);
    if (wcscmp(old, W(text)) != 0) {
        int first=(int)SendMessageW(G.checks,EM_GETFIRSTVISIBLELINE,0,0);
        DWORD from=0,to=0;
        SendMessageW(G.checks,EM_GETSEL,(WPARAM)&from,(LPARAM)&to);
        SetWindowTextW(G.checks,W(text));
        int length=GetWindowTextLengthW(G.checks);
        SendMessageW(G.checks,EM_SETSEL,from>(DWORD)length?(WPARAM)length:from,to>(DWORD)length?(LPARAM)length:to);
        int now=(int)SendMessageW(G.checks,EM_GETFIRSTVISIBLELINE,0,0);
        SendMessageW(G.checks,EM_LINESCROLL,0,first-now);
    }

    char status[512];
    if (s == LR_ST_FAILED)
        snprintf(status, sizeof status, "Problem: %s", G.core.error);
    else
        snprintf(status, sizeof status, "%s", G.core.status);
    if (strcmp(status, G.last_status) != 0) {
        snprintf(G.last_status, sizeof G.last_status, "%s", status);
        SetWindowTextW(G.status, W(status));
    }
}

static void update_friends(void) {
    lr_friend list[256];
    int n = lr_steam_friends(list, 256);
    uint64_t selected = 0;
    lr_friend *f = selected_friend();
    if (f) selected = f->id;
    memcpy(G.friends_list, list, sizeof(lr_friend) * (size_t)n);
    G.friend_count = n;
    SendMessageW(G.friends, WM_SETREDRAW, FALSE, 0);
    SendMessageW(G.friends, LB_RESETCONTENT, 0, 0);
    for (int i = 0; i < n; i++) {
        char line[256];
        snprintf(line, sizeof line, "%s   %s", list[i].name,
                 list[i].hosting ? "<< HOST DDCOOP - click Join" : list[i].in_game ? "(in a game)" : "(online)");
        LRESULT idx = SendMessageW(G.friends, LB_ADDSTRING, 0, (LPARAM)W(line));
        SendMessageW(G.friends, LB_SETITEMDATA, (WPARAM)idx, i);
        if (list[i].id == selected) SendMessageW(G.friends, LB_SETCURSEL, (WPARAM)idx, 0);
    }
    if (n == 0) SendMessageW(G.friends, LB_ADDSTRING, 0, (LPARAM)L"(no friends online)");
    SendMessageW(G.friends, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(G.friends, NULL, TRUE);
}

static void update_runtime_line(void) {
    const char *name = G.core.role == LR_ROLE_HOST ? "ddcoop-fw-host.log" : G.core.role == LR_ROLE_GUEST ?
        (G.core.player==3?"ddcoop-fw-guest3.log":G.core.player==2?"ddcoop-fw-guest2.log":"ddcoop-fw-guest.log") : NULL;
    if (!name) {
        G.runtime_line[0] = 0;
        return;
    }
    char path[LR_PATH_MAX];
    lr_join(path, sizeof path, G.moddir, name);
    uint8_t *data;
    size_t len;
    if (!lr_read_file(path, 32u * 1024 * 1024, &data, &len)) return;
    size_t end = len;
    while (end > 0 && (data[end - 1] == '\n' || data[end - 1] == '\r')) end--;
    size_t start = end;
    while (start > 0 && data[start - 1] != '\n') start--;
    size_t n = end - start < sizeof G.runtime_line - 1 ? end - start : sizeof G.runtime_line - 1;
    memcpy(G.runtime_line, data + start, n);
    G.runtime_line[n] = 0;
    /* Only what this game wrote: the log keeps earlier sessions too. */
    if (G.core.role == LR_ROLE_HOST && len > G.host_log_start && len - G.host_log_start < 64u * 1024 * 1024) {
        size_t k = len - G.host_log_start;
        char *tail = malloc(k + 1);
        if (tail) {
            memcpy(tail, data + G.host_log_start, k);
            tail[k] = 0;
            if (strstr(tail, "guest disconnected") || strstr(tail, "shared actions stopped")) lr_core_host_spent(&G.core);
            if (strstr(tail, "Start screen: loading slot") || strstr(tail, "town hooks installed")) lr_core_host_loaded(&G.core);
            free(tail);
        }
    }
    free(data);
}

/* ---- hero list ---- */

static void set_heroes_hint(const char *text) {
    if (strcmp(text, G.heroes_hint_text) == 0) return;
    snprintf(G.heroes_hint_text, sizeof G.heroes_hint_text, "%s", text);
    SetWindowTextW(G.heroes_hint, W(text));
}

/* The players line ("2/4 players connected: ...") and the host's assign list, with everyone's name. */
static void update_players(void) {
    char text[400], key[400];
    int total = 0, here = lr_core_players_connected(&G.core, &total);
    size_t k = 0;
    text[0] = 0;
    if (here) {
        k = (size_t)snprintf(text, sizeof text, "%d/%d players connected:", here, total);
        for (int i = 0, first = 1; i < 4 && k < sizeof text; i++) {
            if (!lr_core_player_present(&G.core, i)) continue;
            k += (size_t)snprintf(text + k, sizeof text - k, "%s %s%s", first ? "" : ",", player_label(i),
                                  i == lr_core_player(&G.core) ? " (you)" : "");
            first = 0;
        }
    } else if (session_active()) snprintf(text, sizeof text, "Players: waiting for the host...");
    snprintf(key, sizeof key, "%s|%s|%s|%s|%s", text, player_label(0), player_label(1), player_label(2), player_label(3));
    if (!strcmp(key, G.players_shown)) return;
    snprintf(G.players_shown, sizeof G.players_shown, "%s", key);
    SetWindowTextW(G.players_line, W(text));
    LRESULT chosen = SendMessageW(G.assign_player, CB_GETCURSEL, 0, 0);
    SendMessageW(G.assign_player, CB_RESETCONTENT, 0, 0);
    for (int i = 0; i < 4; i++) {
        char item[LR_PLAYER_NAME_MAX + 24];
        const char *name = lr_core_player_name(&G.core, i);
        if (i == 0) snprintf(item, sizeof item, "%s (host)", name[0] ? name : "Player 1");
        else if (name[0]) snprintf(item, sizeof item, "%s", name);
        else snprintf(item, sizeof item, "Player %d", i + 1);
        SendMessageW(G.assign_player, CB_ADDSTRING, 0, (LPARAM)W(item));
    }
    SendMessageW(G.assign_player, CB_SETCURSEL, chosen >= 0 && chosen <= 3 ? (WPARAM)chosen : 0, 0);
    G.heroes_shown = 0xFFFFFFFFu; /* the Player column shows the names too */
}

/* Redraws the list when the table changed (new revision, other role, session ended). */
#include "lr_live_party.inc"
static void update_heroes(void) {
    live_party_refresh();
    const lr_owners *o = &G.core.owners;
    int role = session_active() ? (int)G.core.role : 0;
    int me = lr_core_player(&G.core);
    uint64_t t = now_ms();
    if (G.heroes_recheck_at && t >= G.heroes_recheck_at) { /* a tick the host refused leaves the revision as it was */
        G.heroes_recheck_at = 0;
        G.heroes_shown = 0xFFFFFFFFu;
    }
    int can_assign = role == LR_ROLE_HOST && lr_owners_find(o, G.assign_hero) >= 0;
    EnableWindow(G.assign_player, can_assign);
    EnableWindow(G.assign_button, can_assign);
    if (o->revision == G.heroes_shown && role == G.heroes_role) return;
    G.heroes_shown = o->revision;
    G.heroes_role = role;
    G.heroes_filling = 1; /* our own check changes are not clicks */
    SendMessageW(G.heroes, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(G.heroes);
    int order[LR_MAX_HEROES];
    G.party_count=lr_owners_live_order(o,G.live_party_ids,G.live_party_count,order,G.party_flags);
    for (int position = 0; role && position < o->count; position++) {
        int i=order[position];
        const lr_hero *h = &o->heroes[i];
        int party=G.party_flags[i];
        char cls[LR_HERO_CLASS_MAX + 1], level[8];
        snprintf(cls, sizeof cls, "%s", h->cls);
        for (char *c = cls; *c; c++)
            if (*c == '_') *c = ' ';
        snprintf(level, sizeof level, "%d", lr_hero_level(h->xp));
        const char *by = o->owner[i] == me ? "you" : o->owner[i] < 4 ? player_label(o->owner[i]) : "friend";
        LVITEMW it = {0};
        it.mask = LVIF_TEXT | LVIF_PARAM;
        it.iItem = position;
        wchar_t name[LR_HERO_NAME_MAX+8];
        swprintf(name,sizeof name/sizeof name[0],L"%ls%ls",party?L"\x25CF ":L"",W(h->name[0]?h->name:"?"));
        it.pszText = name;
        it.lParam = (LPARAM)h->id;
        int row = (int)SendMessageW(G.heroes, LVM_INSERTITEMW, 0, (LPARAM)&it);
        const char *cols[] = {cls, level, by};
        for (int k = 0; k < 3; k++) {
            LVITEMW sub = {0};
            sub.iSubItem = k + 1;
            sub.pszText = W(cols[k]);
            SendMessageW(G.heroes, LVM_SETITEMTEXTW, (WPARAM)row, (LPARAM)&sub);
        }
        ListView_SetCheckState(G.heroes, row, o->owner[i] == me);
        if (h->id == G.assign_hero)
            ListView_SetItemState(G.heroes, row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }
    SendMessageW(G.heroes, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(G.heroes, NULL, TRUE);
    G.heroes_filling = 0;
    if (t < G.heroes_hint_until) return; /* a message about the last click stays a moment */
    if (!role)
        set_heroes_hint("Host a game or join a friend to see the co-op campaign heroes here.");
    else if (o->count == 0)
        set_heroes_hint(role == LR_ROLE_HOST ? "No heroes found in the campaign (see DDCoop-launcher.log)."
                                             : "Waiting for the host hero list...");
    else
        set_heroes_hint("\xe2\x97\x8f = current in-game party, shown first. Tick the heroes you control; "
                        "changes apply at the next supported choice boundary. The host can also select a row and assign a player.");
}

/* A tick box was clicked: ask lr_core (the host decides). The list then follows the table again: the host's
 * own change is a new revision at once; a guest's comes back from the host, so check again after 2 s. */
static void on_hero_checked(const NMLISTVIEW *nm) {
    if (G.heroes_filling || !(nm->uChanged & LVIF_STATE)) return;
    if (nm->uNewState & LVIS_SELECTED) G.assign_hero = (uint32_t)nm->lParam;
    UINT old_state = (nm->uOldState & LVIS_STATEIMAGEMASK) >> 12, new_state = (nm->uNewState & LVIS_STATEIMAGEMASK) >> 12;
    if (old_state == 0 || new_state == 0 || old_state == new_state) return; /* not a tick box change */
    uint32_t id = (uint32_t)nm->lParam;
    int claim = new_state == 2;
    logline("hero %u %s by this player", (unsigned)id, claim ? "ticked" : "unticked");
    if (!lr_core_claim(&G.core, id, claim)) {
        set_heroes_hint("That did not work; the list shows the current owners again.");
        G.heroes_hint_until = now_ms() + 4000;
        G.heroes_shown = 0xFFFFFFFFu;
    } else if (G.core.role == LR_ROLE_GUEST) {
        G.heroes_recheck_at = now_ms() + 2000;
    }
}

/* Host: the game rewrites persist.roster.json after recruiting, dismissing and returning to town. */
static void refresh_roster(void) {
    if (G.core.role != LR_ROLE_HOST || !session_active() || !G.core.save_folder[0]) return;
    char dir[LR_PATH_MAX], path[LR_PATH_MAX];
    lr_join(dir, sizeof dir, G.core.cfg.remote_dir, G.core.save_folder);
    lr_join(path, sizeof path, dir, "persist.roster.json");
    uint8_t *data;
    size_t len;
    if (!lr_read_file(path, LR_MAX_FILE_SIZE, &data, &len)) return;
    uint32_t crc = lr_crc32(0, data, len);
    if (crc != G.roster_crc) {
        G.roster_crc = crc;
        lr_core_host_roster(&G.core, data, len);
    }
    free(data);
}

static void automatic_restart_tick(uint64_t t) {
    lr_restart *r=&G.restart;
    if(G.restore_failed && lr_restart_active(r))lr_restart_abort(r);
    int ready=0;
    if((r->phase==LR_R_CLOSING || r->phase==LR_R_WAITING) && !G.game && !G.prepare && !restore_pending())ready=!darkest_running();
    int action=lr_restart_tick(r,t,ready);
    if(action==LR_R_CLOSE) {
        collect_logs(0);G.resync_closing=1;G.resync_close_at=0;
        G.ok[CHK_RUNTIME]=0;G.runtime_line[0]=0;
        lr_core_pause_restart(&G.core);request_game_close();
        logline("automatic resync: normal close requested; lobby retained");
    } else if(action==LR_R_HOST_START) {
        G.resync_closing=0;host_log_mark();
        int ok=lr_core_host_start(&G.core,t) && lr_core_reserve_restart(&G.core,r->peers) && lr_relay_set_epoch(&G.core.relay,r->generation);
        if(ok)ok=launch_game(!G.saving);
        lr_restart_started(r,ok);
        logline("automatic resync: fresh host snapshot/preparation %s",ok?"started":"refused");
    } else if(action==LR_R_GUEST_JOIN) {
        G.resync_closing=0;G.last_runtime=0;G.runtime_line[0]=0;G.ok[CHK_RUNTIME]=0;
        int ok=!G.game && !G.prepare && !restore_pending() && lr_core_guest_join(&G.core,r->host,t) && lr_relay_set_epoch(&G.core.relay,r->generation);
        lr_restart_started(r,ok);
        logline("automatic resync: fresh guest join %s",ok?"started":"refused");
    } else if(action==LR_R_FAIL) {
        lr_core_pause_restart(&G.core);G.resync_closing=1;request_game_close();collect_logs(0);
        snprintf(G.core.status,sizeof G.core.status,"Automatic resync stopped. Let recovery finish, then start a new Host/Join session. Backups are retained.");
        message("Automatic resync could not complete (a player left, recovery/start failed, or the wait expired). "
                "Games close normally; let guest recovery finish. Then start a new session. Send the collected logs.",MB_ICONWARNING);
    }
    if(r->phase==LR_R_RESTARTING && G.game && !G.prepare && G.ok[CHK_RUNTIME] &&
       (r->role!=LR_ROLE_HOST || G.core.host_loaded))lr_restart_complete(r);
    if((r->phase==LR_R_CLOSING || r->phase==LR_R_WAITING) && !G.prepare && !G.restore_failed)
        snprintf(G.core.status,sizeof G.core.status,"Resync: waiting for all games to close and guest recovery. Reconnecting and loading will follow automatically...");
}

static void tick_body(void);
static void overlay_publish(void);
static void offer_update(void);

static void tick(void) {
    if (G.in_tick) return;
    G.in_tick = 1;
    tick_body();
    PHASE("idle");
    G.in_tick = 0;
    if (G.deferred[0]) {
        char text[512];
        snprintf(text, sizeof text, "%s", G.deferred);
        G.deferred[0] = 0;
        message(text, G.deferred_icon);
    }
    if (G.update_prompt) { /* asked outside the tick, like other dialogs */
        G.update_prompt = 0;
        offer_update();
    }
}

static void tick_body(void) {
    uint64_t t = now_ms();
    static int ticks;
    if (ticks < 3) logline("tick %d", ++ticks);
    lr_state before = G.core.state;
    PHASE("tick: steam callbacks");
    lr_steam_pump();
    PHASE("tick: session");
    /* Steam callbacks may update last_rx with a newer timestamp than t captured above. */
    t = now_ms();
    lr_core_tick(&G.core, t);
    diagnostics_tick(t);
    /* A guest session that fails on its own (missing DLC, lost host) gets a pop-up, not only the red status line.
     * Failures from a button (Host, Join) already show one there. */
    if(lr_restart_active(&G.restart) && G.core.state==LR_ST_FAILED)lr_restart_abort(&G.restart);
    automatic_restart_tick(t);
    if (!G.update_asked && lr_core_update(&G.core)) {
        G.update_asked = 1;
        G.update_prompt = 1;
    }
    if (G.core.role == LR_ROLE_GUEST && before != LR_ST_FAILED && G.core.state == LR_ST_FAILED && !G.core.resync_requested && !lr_restart_active(&G.restart) && G.restart.phase!=LR_R_FAILED &&
        !lr_core_update(&G.core)) {
        message(G.core.error, MB_ICONWARNING);
        collect_logs(0);
    }
    if (G.core.resync_requested == 2 && !G.core.resend_len && !G.resync_closing) {
        G.resync_closing = 1;
        collect_logs(0);
        request_game_close(); /* WM_CLOSE only: never terminate a save write. */
        stop_session("Resync: closing normally. Wait for guest recovery, then Host and Join again from the host saved checkpoint.");
    }
    if (G.resync_closing && G.game && t-G.resync_close_at >= 1000) {
        G.resync_close_at=t;request_game_close();
    }
    if (G.resync_closing && !G.game && !G.prepare && !restore_pending()) G.resync_closing=0;
    EnableWindow(G.resync, G.core.role == LR_ROLE_HOST && session_active() && !G.prepare && !G.resync_closing && !G.core.resync_requested && !lr_restart_active(&G.restart));
    if (lr_core_take_launch(&G.core)) {
        if (!launch_game(1)) stop_session("The game could not start.");
    }
    if (G.hosting_pending && t - G.hosting_since > 20000) {
        logline("lobby: no answer from Steam in 20 s");
        stop_session("Steam did not create a lobby within 20 seconds.");
        message(lr_steam_logged_on()
                    ? "Steam did not create a lobby within 20 seconds. Try again; if this persists, click 'Copy logs' and send the folder."
                    : "Steam is offline, so a lobby cannot be created. Set Steam online (Steam menu > Go Online) and try again.",
                MB_ICONWARNING);
    }
    if (G.game && WaitForSingleObject(G.game, 0) == WAIT_OBJECT_0) {
        int guest = restore_pending();
        DWORD exit_code = 0;
        GetExitCodeProcess(G.game, &exit_code);
        logline("game exited with code %lu; guest recovery pending=%d", exit_code, guest);
        CloseHandle(G.game);
        G.game = NULL;
        G.hotkey_ok=RegisterHotKey(G.wnd,ID_HOTKEY,MOD_NOREPEAT,VK_F9);
        if(G.restart.phase!=LR_R_CLOSING && G.restart.phase!=LR_R_WAITING)
            stop_session("Darkest Dungeon closed; session ended. Click Host or Join for a new session.");
        else logline("automatic resync: game exited normally; waiting for recovery");
        if (G.new_campaign_slot) {
            int slot = G.new_campaign_slot;
            lr_campaign_entry list[9];
            G.new_campaign_slot = 0;
            lr_list_campaigns(G.core.cfg.remote_dir, list);
            snprintf(G.core.status, sizeof G.core.status,
                     list[slot - 1].empty ? "There is no campaign in slot %d yet. Click Host to try again." :
                     list[slot - 1].tutorial ? "Slot %d is still in the tutorial. Click Host to continue playing." :
                     "The campaign in slot %d is ready. Click Host to play together. DDCoop waits automatically if Steam is still saving.", slot);
        }
        if (guest) collect_logs(0);
    }
    preparation_tick();
    if (t - G.last_friends > 3000) {
        G.last_friends = t;
        PHASE("tick: friends list");
        uint64_t f0 = now_ms();
        if (G.steam_ok) update_friends();
        if (now_ms() - f0 > 500) logline("friends list took %llu ms (%d online)", (unsigned long long)(now_ms() - f0), G.friend_count);
    }
    if (t - G.last_runtime > 1000) {
        G.last_runtime = t;
        PHASE("tick: runtime log");
        update_runtime_line();
        PHASE("tick: save folder");
        {
            /* The choice is for the next session: not while one is on, or its game or a restore is running. */
            int busy = session_active() || G.prepare || G.game || lr_restart_active(&G.restart);
            if (busy != G.slot_busy) {
                G.slot_busy = busy;
                EnableWindow(G.slot, !busy);
                EnableWindow(G.save_box, !busy);
                EnableWindow(G.players_box, !busy);
                EnableWindow(G.undo, !busy);
                if (!busy) slot_fill();
            }
        }
        if (!session_active()) check_slot();
    }
    if (t - G.last_roster > 5000) {
        G.last_roster = t;
        PHASE("tick: roster");
        refresh_roster();
    }
    if (t - G.last_ui > 250) {
        G.last_ui = t;
        PHASE("tick: window text");
        update_checks();
        PHASE("tick: hero list");
        update_players();
        update_heroes();
        overlay_publish();
    }
}

/* ---- window ---- */

static HWND control(const wchar_t *cls, const wchar_t *text, DWORD style, int x, int y, int w, int h, int id, HFONT font) {
    DWORD ex = (wcscmp(cls, WC_LISTBOXW) == 0 || wcscmp(cls, L"EDIT") == 0) ? WS_EX_CLIENTEDGE : 0;
    HWND c = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, D(x), D(y), D(w), D(h), G.wnd,
                             (HMENU)(INT_PTR)id, NULL, NULL);
    SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE);
    return c;
}

static void build_window(void) {
    int y = 12;
    G.status = control(L"STATIC", L"", SS_LEFT, 14, y, 640, 44, 0, G.big);
    y += 50;
    G.checks = control(L"EDIT", L"", WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL |
                       ES_READONLY | ES_NOHIDESEL, 14, y, 640, 150, 0, G.font);
    y += 156;
    control(L"STATIC", L"Campaign to host:", SS_LEFT, 14, y + 5, 170, 20, 0, G.font);
    G.slot = control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, 190, y, 464, 240, ID_SLOT, G.font);
    y += 36;
    G.host = control(L"BUTTON", L"Host a game", BS_PUSHBUTTON, 14, y, 200, 34, ID_HOST, G.font);
    G.stop = control(L"BUTTON", L"Stop session", BS_PUSHBUTTON, 224, y, 140, 34, ID_STOP, G.font);
    G.logs = control(L"BUTTON", L"Copy logs", BS_PUSHBUTTON, 374, y, 130, 34, ID_LOGS, G.font);
    G.resync = control(L"BUTTON", L"Resync...", BS_PUSHBUTTON | WS_TABSTOP, 514, y, 140, 34, ID_RESYNC, G.font);
    y += 42;
    G.save_box = control(L"BUTTON", L"Save progress when hosting", BS_AUTOCHECKBOX | WS_TABSTOP, 14, y, 260, 26, ID_SAVE, G.font);
    control(L"STATIC", L"Players:", SS_LEFT, 285, y + 5, 60, 20, 0, G.font);
    G.players_box = control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 345, y, 60, 200, ID_PLAYERS, G.font);
    G.undo = control(L"BUTTON", L"Restore this slot's backup...", BS_PUSHBUTTON, 424, y - 2, 230, 30, ID_UNDO, G.font);
    y += 36;
    control(L"STATIC", L"Your name:", SS_LEFT, 14, y + 5, 90, 20, 0, G.font);
    G.name_box = control(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP | WS_BORDER, 104, y, 200, 26, ID_NAME, G.font);
    SendMessageW(G.name_box, EM_LIMITTEXT, 24, 0);
    SendMessageW(G.name_box, EM_SETCUEBANNER, TRUE, (LPARAM)L"shown to the other players");
    G.players_line = control(L"STATIC", L"", SS_LEFT | SS_ENDELLIPSIS | SS_NOPREFIX, 316, y + 5, 338, 20, 0, G.font);
    y += 34;
    control(L"STATIC", L"Steam friends (hosts shown first):", SS_LEFT, 14, y, 400, 20, 0, G.font);
    y += 22;
    G.friends = control(WC_LISTBOXW, L"", LBS_NOTIFY | WS_VSCROLL | LBS_NOINTEGRALHEIGHT, 14, y, 460, 170, ID_FRIENDS, G.font);
    G.join = control(L"BUTTON", L"Join friend", BS_PUSHBUTTON, 484, y, 170, 34, ID_JOIN, G.font);
    G.invite = control(L"BUTTON", L"Invite", BS_PUSHBUTTON, 484, y + 42, 170, 34, ID_INVITE, G.font);
    y += 180;
    control(L"STATIC", L"Or use a code:", SS_LEFT, 14, y + 6, 120, 20, 0, G.font);
    G.code = control(L"EDIT", L"", ES_AUTOHSCROLL, 140, y, 200, 28, ID_CODE, G.font);
    G.join_code = control(L"BUTTON", L"Join code", BS_PUSHBUTTON, 350, y, 124, 28, ID_JOIN_CODE, G.font);
    G.copy_code = control(L"BUTTON", L"Copy my code", BS_PUSHBUTTON, 140, y + 40, 200, 28, ID_COPY_CODE, G.font);
    lr_mascot_create(GetModuleHandleW(NULL),G.wnd,D(494),D(472),D(150),D(150));
    y += 80;
    G.hint = control(L"STATIC", L"", SS_LEFT, 14, y, 640, 70, 0, G.font);

    /* right column: the heroes of the co-op campaign */
    control(L"STATIC", L"Who controls each hero?", SS_LEFT, 674, 12, 370, 22, 0, G.big);
    G.heroes = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_NOSORTHEADER |
                                   LVS_SHOWSELALWAYS,
                               D(674), D(40), D(370), D(532), G.wnd, (HMENU)(INT_PTR)ID_HEROES, NULL, NULL);
    SendMessageW(G.heroes, WM_SETFONT, (WPARAM)G.font, TRUE);
    LOGFONTW party_style;
    if(GetObjectW(G.font,sizeof party_style,&party_style)) {
        party_style.lfWeight=FW_BOLD;G.party_font=CreateFontIndirectW(&party_style);
    }
    ListView_SetExtendedListViewStyle(G.heroes, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    static const struct { const wchar_t *name; int width; } cols[] = {
        {L"Hero", 100}, {L"Class", 110}, {L"Lvl.", 36}, {L"Player", 90}};
    for (int i = 0; i < 4; i++) {
        LVCOLUMNW c = {0};
        c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        c.pszText = (wchar_t *)cols[i].name;
        c.cx = D(cols[i].width);
        c.iSubItem = i;
        SendMessageW(G.heroes, LVM_INSERTCOLUMNW, (WPARAM)i, (LPARAM)&c);
    }
    control(L"STATIC", L"Assign selected hero (host):", SS_LEFT, 674, 580, 370, 20, 0, G.font);
    G.assign_player = control(L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP, 674, 604, 246, 160, ID_ASSIGN_PLAYER, G.font);
    static const wchar_t *const choices[] = {L"Player 1 (host)", L"Player 2", L"Player 3", L"Player 4"};
    for (int i = 0; i < 4; i++) SendMessageW(G.assign_player, CB_ADDSTRING, 0, (LPARAM)choices[i]);
    SendMessageW(G.assign_player, CB_SETCURSEL, 0, 0);
    G.assign_button = control(L"BUTTON", L"Assign", BS_PUSHBUTTON | WS_TABSTOP, 930, 604, 114, 28, ID_ASSIGN_HERO, G.font);
    EnableWindow(G.assign_player, FALSE); EnableWindow(G.assign_button, FALSE);
    G.heroes_hint = control(L"STATIC", L"", SS_LEFT, 674, 646, 370, 68, 0, G.font);
    G.heroes_shown = 0xFFFFFFFFu;
    wchar_t hint[400];
    swprintf(hint, 400, L"%ls  Works best with Darkest Dungeon in windowed or borderless mode. DDCoop %hs",
             G.hotkey_ok ? L"F9 = show or hide this window, including during gameplay." : L"F9 is used by another program; use the DDCoop taskbar icon.",
             DDC_VERSION);
    SetWindowTextW(G.hint, hint);
}

#include "lr_overlay.inc"

static void toggle_from_hotkey(void) {
    if (IsWindowVisible(G.wnd) && !IsIconic(G.wnd) && GetForegroundWindow() == G.wnd) {
        ShowWindow(G.wnd, SW_MINIMIZE);
        return;
    }
    ShowWindow(G.wnd, IsIconic(G.wnd) ? SW_RESTORE : SW_SHOWNORMAL);
    SetWindowPos(G.wnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    SetForegroundWindow(G.wnd);
}

static LRESULT CALLBACK wndproc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if(g_overlay_message && msg==g_overlay_message){overlay_command(wp,lp);return 0;}
    switch (msg) {
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_HOST: do_host(); break;
        case ID_JOIN: do_join_selected(); break;
        case ID_INVITE: do_invite_selected(); break;
        case ID_JOIN_CODE: do_join_code(); break;
        case ID_COPY_CODE: do_copy_code(); break;
        case ID_LOGS: do_logs(); break;
        case ID_UNDO: do_undo(); break;
        case ID_SAVE:
            if (HIWORD(wp) == BN_CLICKED) {
                G.saving = SendMessageW(G.save_box, BM_GETCHECK, 0, 0) == BST_CHECKED;
                saving_remember(G.saving);
                logline("host saving %s", G.saving ? "on" : "off");
            }
            break;
        case ID_STOP:
            stop_session("Stopped. Click Host or Join for a new session.");
            if (G.game) request_game_close();
            break;
        case ID_FRIENDS:
            if (HIWORD(wp) == LBN_DBLCLK) do_join_selected();
            break;
        case ID_RESYNC:
            if (HIWORD(wp) == BN_CLICKED && G.core.role == LR_ROLE_HOST && session_active() && !G.prepare && !lr_restart_active(&G.restart)) {
                int count=lr_core_guest_count(&G.core);
                if(!count && strstr(DDC_VERSION,"duo")) {
                    message("Solo .bat guest windows have no launcher. Close all guest windows manually, then stop and Host again.",MB_ICONINFORMATION);break;
                }
                if(count<1 || (G.core.cfg.max_guests>0 && count!=G.core.cfg.max_guests)) {
                    message("Automatic resync needs every selected player present. Close all games and start a new session with the correct player count if someone has left.",MB_ICONINFORMATION);break;
                }
                int answer=MessageBoxW(G.wnd,
                    L"Automatically restart everyone from the host checkpoint?\n\n"
                    L"All games close normally and guest saves recover. DDCoop then reconnects, transfers the fresh host save and starts every game automatically. "
                    L"Keep all launchers open. Unsaved progress may be lost; no live repair is attempted.",
                    L"DDCoop resync", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
                if(answer==IDYES) {
                    uint64_t peers[3];for(unsigned i=0;i<3;i++)peers[i]=G.core.guests[i].peer;
                    uint32_t generation=0;G.core.cfg.random(G.core.cfg.ctx,(uint8_t *)&generation,sizeof generation);
                    if(!generation)generation=1;
                    if(generation==G.restart.generation)generation^=0x9e3779b9u;
                    if(!generation)generation=1;
                    lr_restart_init(&G.restart,LR_ROLE_HOST,0,G.core.cfg.kit,G.core.cfg.send,G.core.cfg.ctx);
                    if(lr_restart_begin(&G.restart,peers,generation,now_ms()))collect_logs(0);
                }
            }
            break;
        case ID_ASSIGN_HERO:
            if (HIWORD(wp) == BN_CLICKED) {
                int player = (int)SendMessageW(G.assign_player, CB_GETCURSEL, 0, 0);
                if (!lr_core_assign(&G.core, G.assign_hero, player)) {
                    set_heroes_hint("Select a hero and a player included in this session. An unchanged owner needs no update.");
                    G.heroes_hint_until = now_ms() + 4000;
                }
                G.heroes_shown = 0xFFFFFFFFu;
                update_heroes();
            }
            break;
        case ID_NAME:
            if (HIWORD(wp) == EN_CHANGE && !G.name_filling) {
                wchar_t wide[64];
                char utf8[256];
                GetWindowTextW(G.name_box, wide, 64);
                to_utf8(wide, utf8, sizeof utf8);
                lr_core_set_name(&G.core, utf8);
                name_remember(G.core.my_name);
                G.players_shown[0] = 0;
            }
            break;
        case ID_PLAYERS:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                LRESULT at = SendMessageW(G.players_box, CB_GETCURSEL, 0, 0);
                if (at >= 0 && at <= 2) {
                    G.players = (int)at + 2;
                    players_remember(G.players);
                    logline("chosen: %d players", G.players);
                }
            }
            break;
        case ID_SLOT:
            if (HIWORD(wp) == CBN_DROPDOWN) slot_fill();
            if (HIWORD(wp) == CBN_SELCHANGE) slot_chosen();
            break;
        }
        return 0;
    case WM_NOTIFY: {
        const NMHDR *nh = (const NMHDR *)lp;
        if (nh->idFrom == ID_HEROES && nh->code == LVN_ITEMCHANGED) on_hero_checked((const NMLISTVIEW *)lp);
        if(nh->idFrom==ID_HEROES && nh->code==NM_CUSTOMDRAW) {
            NMLVCUSTOMDRAW *draw=(NMLVCUSTOMDRAW *)lp;
            if(draw->nmcd.dwDrawStage==CDDS_PREPAINT)return CDRF_NOTIFYITEMDRAW;
            if(draw->nmcd.dwDrawStage==CDDS_ITEMPREPAINT) {
                int i=lr_owners_find(&G.core.owners,(uint32_t)draw->nmcd.lItemlParam);
                if(i>=0 && G.party_flags[i]) {
                    draw->clrTextBk=RGB(255,242,204);draw->clrText=RGB(90,52,12);
                    if(G.party_font)SelectObject(draw->nmcd.hdc,G.party_font);
                    return CDRF_NEWFONT;
                }
            }
        }
        return 0;
    }
    case WM_TIMER:
        if (wp == ID_TIMER) tick();
        return 0;
    case WM_HOTKEY:
        if (wp == ID_HOTKEY) toggle_from_hotkey();
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE) SetWindowPos(h, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        return 0;
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp;
        if ((HWND)lp == G.status) SetTextColor(dc, G.core.state == LR_ST_FAILED ? RGB(170, 20, 20) : RGB(20, 90, 20));
        SetBkMode(dc, TRANSPARENT);
        return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
    }
    case WM_CLOSE:
        if ((G.prepare || G.game || restore_pending()) && !G.restore_failed) {
            G.want_close = 1;
            stop_session("Closing game and restoring your own campaign before DDCoop exits...");
            request_game_close();
            return 0;
        }
        if (session_active() && G.game &&
            MessageBoxW(h, L"A co-op session is active. Closing DDCoop disconnects your friend. Close anyway?",
                        L"DDCoop", MB_YESNO | MB_ICONQUESTION) != IDYES)
            return 0;
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        diagnostics_reset();
        live_party_close();
        overlay_close();
        if(G.party_font){DeleteObject(G.party_font);G.party_font=NULL;}
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

/* ---- kit updates (UPDATE.md) ---- */

/* Our own installed kit: offered to a friend whose launcher is older. Only when ddcoop-kit.sig next to this
 * DDCoop.exe is signed by the owner, names this build and this runtime, and the installed files match it. */
static void load_own_kit(lr_config *cfg) {
    lr_kit_bundle *k = &G.own_kit;
    wchar_t w[LR_PATH_MAX];
    char self[LR_PATH_MAX], dir[LR_PATH_MAX], path[LR_PATH_MAX], boot[LR_PATH_MAX], runtime[LR_PATH_MAX];
    uint8_t *data = NULL;
    size_t len = 0;
    memset(k, 0, sizeof *k);
    cfg->kit_verify = lr_kit_verify_owner;
    cfg->kit_serial = lr_kit_serial(DDC_VERSION);
    if (!cfg->kit_serial) {
        logline("kit: updates are off in this build (%s)", DDC_VERSION);
        return;
    }
    GetModuleFileNameW(NULL, w, LR_PATH_MAX);
    to_utf8(w, self, sizeof self);
    snprintf(dir, sizeof dir, "%s", self);
    char *slash = strrchr(dir, '\\');
    if (slash) *slash = 0;
    lr_join(path, sizeof path, dir, "ddcoop-kit.sig");
    if (!lr_read_file(path, LR_KIT_MANIFEST_MAX, &data, &len)) {
        logline("kit: no signed manifest next to DDCoop.exe; nothing to offer to friends");
        return;
    }
    memcpy(k->manifest, data, len);
    k->manifest_len = len;
    free(data);
    lr_join(boot, sizeof boot, G.moddir, "SDL2-bootstrap.dll");
    lr_join(runtime, sizeof runtime, G.moddir, "ddcoop-runtime.dll");
    const char *paths[LR_KIT_FILES] = {self, boot, runtime};
    int ok = lr_kit_parse(k->manifest, k->manifest_len, &k->m) && !strcmp(k->m.version, DDC_VERSION) && k->m.kit == cfg->kit;
    for (int f = 0; ok && f < LR_KIT_FILES; f++) {
        ok = lr_read_file(paths[f], LR_KIT_MAX_FILE, &k->data[f], &len) && len == k->m.size[f];
        if (ok) k->got[f] = (uint32_t)len;
    }
    if (!ok || !lr_kit_bundle_ok(k, lr_kit_verify_owner, cfg->kit)) {
        logline("kit: the signed manifest does not match this launcher and its installed files; nothing offered");
        lr_kit_bundle_free(k);
        return;
    }
    cfg->kit_offer = k;
    logline("kit: signed DDCoop %s (serial %u) ready to offer to a friend with an older launcher", k->m.version, k->m.serial);
}

/* Writes the host's checked kit to mods\DDCoop\update\<version>, reads every file back, starts its DDCoop.exe
 * with --update (which waits for us to close and installs through the normal install path) and closes. */
static void apply_update(const lr_kit_bundle *k) {
    char sub[LR_PATH_MAX], dir[LR_PATH_MAX], path[LR_PATH_MAX];
    if (session_active() || G.prepare || G.game || restore_pending() || darkest_running()) {
        message("Close Darkest Dungeon and let the current session finish first, then join the host again to update.", MB_ICONWARNING);
        return;
    }
    lr_join(sub, sizeof sub, G.moddir, "update");
    lr_join(dir, sizeof dir, sub, k->m.version);
    int ok = lr_make_dirs(dir);
    for (int f = 0; ok && f < LR_KIT_FILES; f++) {
        uint8_t *back = NULL;
        size_t len = 0;
        lr_join(path, sizeof path, dir, lr_kit_file_names[f]);
        ok = lr_write_file(path, k->data[f], k->m.size[f]) && lr_read_file(path, LR_KIT_MAX_FILE, &back, &len) &&
             lr_kit_file_ok(&k->m, f, back, len);
        free(back);
    }
    lr_join(path, sizeof path, dir, "ddcoop-kit.sig");
    if (ok) ok = lr_write_file(path, k->manifest, k->manifest_len);
    lr_join(path, sizeof path, dir, "DDCoop.exe");
    if (!ok || (INT_PTR)ShellExecuteW(NULL, L"open", W(path), L"--update", W(dir), SW_SHOWNORMAL) <= 32) {
        logline("update: could not prepare or start %s", path);
        message("The update could not be prepared (see DDCoop-launcher.log). Nothing was changed.", MB_ICONWARNING);
        return;
    }
    logline("update: DDCoop %s started from %s; closing for the install", k->m.version, dir);
    PostMessageW(G.wnd, WM_CLOSE, 0, 0);
}

static void offer_update(void) {
    const lr_kit_bundle *k = lr_core_update(&G.core);
    char text[700];
    if (!k) return;
    snprintf(text, sizeof text,
             "The host runs a newer DDCoop: %s (you have %s). It is signed by the DDCoop owner, and every file was "
             "checked.\n\nUpdate now? DDCoop closes, installs the new version and offers to open it again. Your game "
             "and your saves are not changed.\n\nYour virus scanner may still question the new files; see the kit's README.",
             k->m.version, DDC_VERSION);
    if (MessageBoxW(G.wnd, W(text), L"DDCoop update", MB_YESNO | MB_ICONQUESTION) != IDYES) {
        logline("update: declined by the player");
        return;
    }
    apply_update(k);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmdline, int show) {
    (void)prev;
    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(cmdline && *cmdline ? GetCommandLineW() : L"DDCoop.exe", &argc);
    if (argv && argc >= 2 && wcscmp(argv[1], L"--prepare-save") == 0) return lr_prepare_win_main(argc, argv);
    if (argv && argc >= 2 && wcsncmp(argv[1], L"--", 2) == 0) return lr_install_main(argv[1]);
    HANDLE mutex = CreateMutexW(NULL, TRUE, L"Local\\DDCoopLauncher");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND other = FindWindowW(L"DDCoopWindow", NULL);
        if (other) {
            ShowWindow(other, SW_RESTORE);
            SetForegroundWindow(other);
        }
        return 0;
    }
    find_paths();
    lr_join(G.log_path, sizeof G.log_path, G.moddir, "DDCoop-launcher.log");
    G.log = _wfopen(W(G.log_path), L"w");
    PHASE("startup");
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_gui_thread, 0, FALSE,
                    DUPLICATE_SAME_ACCESS);
    CloseHandle(CreateThread(NULL, 0, watchdog, NULL, 0, NULL));
    logline("DDCoop %s starting; game folder %s", DDC_VERSION, G.root);
    lr_sock_startup();

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.lpszClassName = L"DDCoopWindow";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    RegisterClassW(&wc);
    {
        HDC screen = GetDC(NULL);
        g_dpi = GetDeviceCaps(screen, LOGPIXELSY);
        ReleaseDC(NULL, screen);
        if (g_dpi < 96) g_dpi = 96;
    }
    INITCOMMONCONTROLSEX icc = {sizeof icc, ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&icc);
    RECT r = {0, 0, D(1058), D(726)};
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&r, style, FALSE);
    G.wnd = CreateWindowExW(0, L"DDCoopWindow", L"DDCoop - Darkest Dungeon co-op", style, CW_USEDEFAULT, CW_USEDEFAULT,
                            r.right - r.left, r.bottom - r.top, NULL, NULL, inst, NULL);
    G.font = CreateFontW(-D(15), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    G.big = CreateFontW(-D(18), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    G.hotkey_ok = RegisterHotKey(G.wnd, ID_HOTKEY, MOD_NOREPEAT, VK_F9);
    g_overlay_message=RegisterWindowMessageW(DD_OVERLAY_MESSAGE);
    build_window();

    lr_config cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.send = lr_steam_send;
    cfg.allowed = allowed_cb;
    cfg.game_running = game_running_cb;
    cfg.has_dlc = has_dlc_cb;
    cfg.mod_print = mod_print_cb;
    cfg.random = random_cb;
    cfg.log = log_cb;
    cfg.coop_slot = 3;
    {
        char kit[9];
        memcpy(kit, LR_HASH_RUNTIME, 8);
        kit[8] = 0;
        cfg.kit = (uint32_t)strtoul(kit, NULL, 16);
    }
    lr_join(cfg.ini_path, sizeof cfg.ini_path, G.moddir, "ddcoop.ini");
    lr_join(cfg.heroes_path, sizeof cfg.heroes_path, G.moddir, "ddcoop-heroes.ini");
    lr_join(cfg.restore_path, sizeof cfg.restore_path, G.moddir, "ddcoop-guest-original.bin");
    {
        wchar_t profile[MAX_PATH];
        char p[LR_PATH_MAX];
        if (SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, profile) == S_OK) {
            to_utf8(profile, p, sizeof p);
            lr_join(cfg.backup_root, sizeof cfg.backup_root, p, "DDCoop-backups");
        }
    }
    check_install();
    load_own_kit(&cfg);
    {
        /* both launchers say which build they run; the host refuses a different one */
        char exe_hex[65], exe[LR_PATH_MAX];
        lr_join(exe, sizeof exe, G.bin, "Darkest.exe");
        if (sha256_file(exe, exe_hex)) {
            exe_hex[8] = 0;
            cfg.build = (uint32_t)strtoul(exe_hex, NULL, 16);
        }
    }

    lr_steam_callbacks cb = {0};
    cb.on_message = on_message;
    cb.on_event = on_event;
    cb.accept_session = accept_session_cb;
    cb.log = log_cb;
    char err[256] = "";
    wchar_t dll[LR_PATH_MAX];
    {
        wchar_t exe[LR_PATH_MAX];
        GetModuleFileNameW(NULL, exe, LR_PATH_MAX);
        wchar_t *s = wcsrchr(exe, L'\\');
        if (s) *s = 0;
        swprintf(dll, LR_PATH_MAX, L"%ls\\steam_api64.dll", exe);
        SetCurrentDirectoryW(exe); /* steam_appid.txt (480) is read from here */
    }
    G.steam_ok = lr_steam_start(dll, &cb, err, sizeof err);
    if (G.steam_ok) {
        cfg.steam_user = lr_steam_me();
        if (lr_steam_logged_on())
            set_check(CHK_STEAM, 1, "Steam: signed in as %s", lr_steam_my_name());
        else
            set_check(CHK_STEAM, 0, "Steam staat in offline-modus (%s). Zet Steam online: menu Steam > Ga online.",
                      lr_steam_my_name());
        steam_userdata_remote(cfg.remote_dir, sizeof cfg.remote_dir);
        logline("save folder root: %s", cfg.remote_dir);
        log_save_folders(cfg.remote_dir);
    } else {
        set_check(CHK_STEAM, 0, "%s", err);
        logline("steam: %s", err);
    }
    PHASE("startup: session init");
    lr_core_init(&G.core, &cfg);
    G.chosen_slot = slot_remembered();
    G.saving = saving_remembered();
    SendMessageW(G.save_box, BM_SETCHECK, G.saving ? BST_CHECKED : BST_UNCHECKED, 0);
    G.players = players_remembered();
    {
        char name[LR_PLAYER_NAME_MAX + 1];
        name_remembered(name);
        lr_core_set_name(&G.core, name);
        G.name_filling = 1;
        SetWindowTextW(G.name_box, W(G.core.my_name));
        G.name_filling = 0;
        logline("startup: player name %s", G.core.my_name[0] ? "set" : "not set");
    }
    SendMessageW(G.players_box, CB_ADDSTRING, 0, (LPARAM)L"2");
    SendMessageW(G.players_box, CB_ADDSTRING, 0, (LPARAM)L"3");
    SendMessageW(G.players_box, CB_ADDSTRING, 0, (LPARAM)L"4");
    SendMessageW(G.players_box, CB_SETCURSEL, (WPARAM)(G.players - 2), 0);
    slot_fill();
    /* A crash or power cut can leave a session ini behind; a normal Steam start must stay single player. */
    PHASE("startup: ini reset");
    lr_write_ini(cfg.ini_path, "off", LR_DEFAULT_GAME_PORT, "unused-until-ddcoop-starts", cfg.coop_slot);
    logline("startup: ini reset to role=off");
    snprintf(G.core.status, sizeof G.core.status,
             G.install_ok && G.steam_ok ? "Ready. Host a game, or select a hosting friend and click Join."
                                        : "Not ready: resolve the checks marked [ - ] below first.");
    PHASE("startup: checks");
    check_slot();
    update_checks();
    PHASE("startup: show window");
    ShowWindow(G.wnd, show);
    UpdateWindow(G.wnd);
    SetTimer(G.wnd, ID_TIMER, 10, NULL);
    logline("startup: window shown, entering the message loop");
    PHASE("message loop");
    timeBeginPeriod(1);

    MSG m;
    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        if (IsDialogMessageW(G.wnd, &m)) continue;
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    timeEndPeriod(1);
    stop_session("afgesloten");
    lr_steam_stop();
    if (G.game) CloseHandle(G.game);
    logline("DDCoop closed");
    if (G.log) fclose(G.log);
    ReleaseMutex(mutex);
    return 0;
}

/* Straight into the chosen campaign: no opening and no menu, in a session only (ddcoop.ini role and coop_slots).
 *
 * This goes through the game's own screen, the way a player does (FrontEndDisplay, object at 118e030, its state at +1f18):
 * - 2a6ebe0 says how the screen starts when it is created (read at 415311). 2 is what the game sets itself after its first time (3de1c7), for coming back to the
 *   menu: no logos, no movies, straight to the title. This only sets it before the first time. Nothing in the steps skipped writes a global.
 * - State 5 is the title. Its button (functor 424150, the screen at +8) runs the game's own check of Steam's saves; a question from it stays on screen, for the player.
 * - State 6 goes on by itself (415a03). State 7 is the row of saves. 41c390 is what a slot's load button runs (functor 425090, slots 0..8), which leads to the game's normal
 *   loading of a campaign. Slot 9's button is the Butcher's Circus (41be60): never used here. So is everything guarded by 2a6d5e0, 2a6d5e8, 2a6d4b9 and 2a6d4bb.
 * Each step happens once. Whatever goes differently, the player is left at the game's own screen, which works as always.
 *
 * Together (protocol 22): in a session the connection starts here already, and each copy waits at the row of saves until the
 * other one stands there too (DD_MSG_LOAD, the slot). Then both load the same save at the same moment. In the owner's run of
 * 8 Nov 16:01 the host loaded a saved fight 44 s before the guest, played the monster's turn alone with other numbers, and the
 * fight parted (a Vestal afflicted on one copy, virtuous on the other). The player can still click a slot: that wins.
 *
 * A new campaign: the launcher starts the game alone, without a session, with DDCOOP_NEW_SLOT=<slot> in this process's environment only. The same way to the row of saves, then
 * 41b330, what an empty slot's new_game button runs (functor 423c80): the game asks for a name and the player plays the tutorial, saved as always. If the slot has a save by then
 * (the tutorial was left halfway), it loads it, to go on with it. */
#define DD_FE_OBJECT 0x118e030u
#define DD_FE_START_MODE 0x2a6ebe0u
#define DD_FE_START_AT_TITLE 2
#define DD_FE_STATE 0x1f18u
#define DD_FE_STATE_TITLE 5
#define DD_FE_STATE_SAVES 7
#define DD_FE_SLOTS 0x1768u
#define DD_FE_SLOT_SIZE 0xc10u
#define DD_FE_SLOT_HAS_SAVE 0x145u
#define DD_FE_SLOT_LOADABLE 0x146u
#define DD_FE_SETTLE_MS 1000u
#define DD_FE_GIVE_UP_MS 180000u
typedef void(__fastcall *fe_start_fn)(void *);
typedef void(__fastcall *fe_load_fn)(void *, int32_t);
static fe_start_fn g_fe_start;
static fe_load_fn g_fe_load, g_fe_new;
static int g_fe_new_slot;
static uintptr_t (*g_fe_image)(void) = image_base;
static int g_fe_done, g_fe_mode_done, g_fe_clicked, g_fe_waiting;
static uint64_t g_fe_load_sent_at;
static void (*g_frontend_transport)(void) = runtime_transport_start;
static int (*g_frontend_departure_ready)(void) = departure_start_ready;
static const char g_fe_wait_text[] = "DDCoop is waiting for the other game to reach the campaign list. Both games then load the "
                                     "campaign together so they start in sync. (Clicking the slot manually is possible, but it "
                                     "may not stay in sync.)";
static int32_t g_fe_state_seen = -1;
static uint64_t g_fe_state_at, g_fe_give_up_at;
static void frontend_reset(void) {
    g_departure_load_armed = 0;
    g_fe_done = g_fe_mode_done = g_fe_clicked = g_fe_waiting = 0;
    g_fe_load_sent_at = 0;
    g_fe_state_seen = -1;
    g_fe_state_at = g_fe_give_up_at = 0;
}
static void frontend_read_new_slot(void) {
    char value[4] = "";
    DWORD n = GetEnvironmentVariableA("DDCOOP_NEW_SLOT", value, sizeof value);
    g_fe_new_slot = n == 1 && value[0] >= '1' && value[0] <= '9' ? value[0] - '0' : 0;
}
/* The one slot of this session, or the launcher's new campaign without a session; or 0. */
static int frontend_slot(const dd_config *cfg) {
    int bad = 0, slot = 0;
    uint32_t mask;
    if (cfg->role == DD_ROLE_OFF) return g_fe_new_slot;
    mask = dd_coop_slots_parse(cfg->coop_slots, &bad);
    if (bad || !mask || (mask & (mask - 1))) return 0;
    while (!((mask >> slot) & 1)) slot++;
    return slot >= 1 && slot <= 9 ? slot : 0;
}
/* The game is on its way to the Butcher's Circus (an invitation, or its menu), or this cannot be read. */
static int frontend_circus(uintptr_t base) {
    uint64_t lobby = 0, invite = 0;
    uint8_t mode = 1, roster = 1;
    return !safe_read(base + 0x2a6d5e0, &lobby, 8) || !safe_read(base + 0x2a6d5e8, &invite, 8) || !safe_read(base + 0x2a6d4b9, &mode, 1) ||
           !safe_read(base + 0x2a6d4bb, &roster, 1) || lobby || invite || mode || roster;
}
static void frontend_stop(const char *why) {
    g_fe_done = 1;
    if (g_fe_waiting) shop_notice_clear(g_fe_wait_text);
    dd_log("Start screen: %s", why);
}
static void frontend_send_load(int slot, uint64_t now) {
    dd_msg m;
    memset(&m, 0, sizeof m);
    m.type = DD_MSG_LOAD;
    m.u.load.slot = (uint32_t)slot;
    if (net_send(g_cfg.role == DD_ROLE_HOST ? DD_TOWN_ALL : 0, &m)) g_fe_load_sent_at = now;
}
/* How many guests the host's launcher has in this session (ddcoop-guests.txt next to this DLL, one number), or -1 if it
 * does not say: then the host loads once no guest has arrived for DD_FE_QUIET_MS. */
#define DD_FE_QUIET_MS 8000u
static int frontend_expected_file(void) {
    char path[MAX_PATH], text[8] = "";
    HMODULE self = NULL;
    char *slash;
    FILE *f;
    int n = -1;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)frontend_expected_file, &self) ||
        !GetModuleFileNameA(self, path, sizeof path) || !(slash = strrchr(path, '\\')) || (size_t)(slash + 1 - path) + 18 >= sizeof path)
        return -1;
    strcpy(slash + 1, "ddcoop-guests.txt");
    if (!(f = fopen(path, "rb"))) return -1;
    if (fgets(text, sizeof text, f) && text[0] >= '1' && text[0] <= '3' && (!text[1] || text[1] == '\r' || text[1] == '\n')) n = text[0] - '0';
    fclose(f);
    return n;
}
static int (*g_fe_expected)(void) = frontend_expected_file;
/* In a session: 1 once both copies stand at the row of saves for the same slot. Until then this copy says it is there, every second. */
static int frontend_together(int slot, uint64_t now) {
    LONG peer;
    g_fe_give_up_at = now + DD_FE_GIVE_UP_MS; /* Waiting for the other player is not the screen failing. */
    if (!g_fe_waiting) {
        g_fe_waiting = 1;
        dd_log("Start screen: waiting for the other copy to stand at its save screen too");
        shop_notice(g_fe_wait_text);
    }
    if (!InterlockedCompareExchange(&g_session_ready, 0, 0)) return 0;
    if (g_cfg.role != DD_ROLE_HOST) {
        /* A guest says it is ready, every second, and loads when the host says it loads now. */
        if (!g_fe_load_sent_at || now - g_fe_load_sent_at >= 1000) frontend_send_load(slot, now);
        peer = InterlockedCompareExchange(&g_peer_load_slot, 0, 0);
        if (!peer) return 0;
        if (peer != slot) {
            dd_log("Start screen: the host loads slot %ld, this copy waits for slot %d", (long)peer, slot);
            frontend_stop("not the same slot; left to the player");
            return 0;
        }
        shop_notice_clear(g_fe_wait_text);
        dd_log("Start screen: the host loads now; loading together");
        return 1;
    }
    /* The host: once every connected guest stands at its save screen for this slot, and either the launcher's number of guests
     * is there or nobody new came for a while, it says "now" to all and loads. */
    {
        int count = 0, at_saves = 0, expected = g_fe_expected();
        uint64_t changed = (uint64_t)InterlockedCompareExchange64(&g_guest_change_at, 0, 0);
        for (int i = 0; i < DD_MAX_GUESTS; i++) {
            if (!InterlockedCompareExchange(&g_guests[i].ready, 0, 0)) continue;
            count++;
            peer = InterlockedCompareExchange(&g_guests[i].load_slot, 0, 0);
            if (peer == slot) at_saves++;
            else if (peer) {
                dd_log("Start screen: guest player %d waits for slot %ld, the host for slot %d", i + 1, (long)peer, slot);
                frontend_stop("not the same slot; left to the player");
                return 0;
            }
        }
        if (!count || at_saves < count) return 0;
        if (expected > 0 ? count < expected : now < changed + DD_FE_QUIET_MS) return 0;
        frontend_send_load(slot, now);
        shop_notice_clear(g_fe_wait_text);
        dd_log("Start screen: %d guest(s) at the save screen; loading together", count);
        return 1;
    }
}
/* Before the screen exists: it starts at the title. */
static void frontend_try_mode(uintptr_t base, int slot) {
    int32_t mode = 1;
    g_fe_mode_done = 1;
    if (safe_read(base + DD_FE_START_MODE, &mode, 4) && !mode) {
        int32_t title = DD_FE_START_AT_TITLE;
        memcpy((void *)(base + DD_FE_START_MODE), &title, 4);
        dd_log("Start screen: straight to the title, then slot %d", slot);
    }
}
/* The game thread, every frame until it is done. */
static void frontend_poll(void) {
    uintptr_t base;
    uint64_t screen = 0, now;
    int32_t state = 0;
    int slot;
    if (g_fe_done) {
        /* A save loaded into town never consumes the one-shot RaidStart arm.
         * Once its configured campaign is active it is no longer needed. */
        if (InterlockedCompareExchange(&g_campaign_active, 0, 0)) g_departure_load_armed = 0;
        return;
    }
    slot = frontend_slot(&g_cfg);
    if (!slot || InterlockedCompareExchange(&g_campaign_active, 0, 0)) {
        g_fe_done = 1;
        return;
    }
    base = g_fe_image();
    if (frontend_circus(base)) {
        frontend_stop("not this time; left to the player");
        return;
    }
    if (g_cfg.role != DD_ROLE_OFF) g_frontend_transport();
    if (!safe_read(base + DD_FE_OBJECT, &screen, 8)) {
        frontend_stop("unreadable; left to the player");
        return;
    }
    if (!screen) {
        if (!g_fe_mode_done) frontend_try_mode(base, slot);
        return;
    }
    g_fe_mode_done = 1; /* Too late for this time: the opening runs as usual, and the rest still works. */
    now = GetTickCount64();
    if (!g_fe_give_up_at) g_fe_give_up_at = now + DD_FE_GIVE_UP_MS;
    if (now > g_fe_give_up_at) {
        frontend_stop("no save screen in time; left to the player");
        return;
    }
    if (!safe_read((uintptr_t)screen + DD_FE_STATE, &state, 4)) {
        frontend_stop("state unreadable; left to the player");
        return;
    }
    if (state != g_fe_state_seen) {
        g_fe_state_seen = state;
        g_fe_state_at = now;
        g_fe_clicked = 0;
    }
    if (state > DD_FE_STATE_SAVES) {
        if (g_fe_waiting) dd_log("Start screen: the player chose a slot while waiting for the other copy");
        frontend_stop("a slot was chosen already");
        return;
    }
    if (now - g_fe_state_at < DD_FE_SETTLE_MS) return; /* Its fade in. */
    if (state == DD_FE_STATE_TITLE && !g_fe_clicked && g_fe_start) {
        uint64_t functor[2] = {0, screen};
        g_fe_clicked = 1;
        dd_log("Start screen: past the title");
        g_fe_start(functor);
        return;
    }
    if (state == DD_FE_STATE_SAVES) {
        uint64_t slots = 0;
        uint8_t has = 0, loadable = 0;
        int alone = g_cfg.role == DD_ROLE_OFF;
        if (!g_fe_load || !g_fe_new || !safe_read((uintptr_t)screen + DD_FE_SLOTS, &slots, 8) || !slots ||
            !safe_read((uintptr_t)slots + (uint64_t)(slot - 1) * DD_FE_SLOT_SIZE + DD_FE_SLOT_HAS_SAVE, &has, 1) ||
            !safe_read((uintptr_t)slots + (uint64_t)(slot - 1) * DD_FE_SLOT_SIZE + DD_FE_SLOT_LOADABLE, &loadable, 1) || (!has && !alone) || !loadable) {
            dd_log("Start screen: slot %d has no loadable save here (%u, %u)", slot, has, loadable);
            frontend_stop("left to the player");
            return;
        }
        /* A warm load can enter RaidStart before the slot-watch thread enables
         * town hooks. Arm its single Rules hook before advertising readiness,
         * while the exact build, normal screen and chosen slot are validated. */
        if (!alone && InterlockedCompareExchange(&g_session_ready, 0, 0) &&
            (!g_frontend_departure_ready || !g_frontend_departure_ready())) {
            frontend_stop("RaidStart/Rules could not be prepared; campaign not loaded");
            shop_notice("DDCoop could not prepare synchronized loading. The campaign was not loaded. "
                        "Close both games and send the logs.");
            return;
        }
        if (!alone && !frontend_together(slot, now)) return;
        g_fe_done = 1;
        if (has) {
            if (!alone) g_departure_load_armed = 1;
            dd_log("Start screen: loading slot %d", slot);
            g_fe_load((void *)(uintptr_t)screen, slot - 1);
        } else {
            dd_log("Start screen: new campaign in slot %d", slot);
            g_fe_new((void *)(uintptr_t)screen, slot - 1);
        }
    }
}
static void frontend_bind(void) {
    g_fe_start = (fe_start_fn)hamlet_address("fe_start_button");
    g_fe_load = (fe_load_fn)hamlet_address("fe_load_slot");
    g_fe_new = (fe_load_fn)hamlet_address("fe_new_slot");
    frontend_read_new_slot();
}
/* From the startup thread, as soon as the settings and the game's build are known: the frame poll comes a second later, and once that was too late (6 Nov 02:18, second window). */
void dd_runtime_frontend_early(const dd_config *cfg) {
    uintptr_t base = g_fe_image();
    uint64_t screen = 1;
    int slot;
    frontend_read_new_slot();
    slot = frontend_slot(cfg);
    if (!slot || frontend_circus(base) || !safe_read(base + DD_FE_OBJECT, &screen, 8) || screen) return;
    frontend_try_mode(base, slot);
}

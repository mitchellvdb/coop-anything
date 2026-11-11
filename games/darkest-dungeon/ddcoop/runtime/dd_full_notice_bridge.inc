/* Only the loot inventory-full informational dialog, not arbitrary confirmations.
 * Add captures the exact native entry; ShowEntry is the safe native removal
 * boundary. Keys are ordered loot sequence + area, never a translated message,
 * an address from another process, or a later popup with the same text.
 * tests/test_full_notice_layout.py proves the empty callback and native ABI. */
typedef void (__fastcall *confirm_enqueue_fn)(void *, const char *, void *);
typedef void (__fastcall *confirm_show_fn)(void *, void *);
typedef void (__fastcall *confirm_ack_fn)(void *);
static confirm_enqueue_fn g_confirm_enqueue;
static confirm_show_fn g_confirm_show_entry;
static confirm_ack_fn g_confirm_ack;
#define DD_FULL_HISTORY 32
typedef struct { uint32_t seq, area; int closed; } full_token;
typedef struct { uintptr_t manager, entry; uint32_t seq, area; int asked; } full_entry;
static full_token g_full_tokens[DD_FULL_HISTORY];
static full_entry g_full_entries[DD_FULL_HISTORY];
static unsigned g_full_next;
static uint32_t g_full_loot_seq, g_full_loot_area;
static full_entry *g_full_render;
static const char g_full_name[] = "not_enough_room_confirm";
static int full_notice_caller(uintptr_t rva) { return rva == 0x7ebcd8 || rva == 0x717427; }
static int (*g_full_notice_caller)(uintptr_t) = full_notice_caller;
static void full_notice_reset(void) {
    memset(g_full_tokens, 0, sizeof g_full_tokens);
    memset(g_full_entries, 0, sizeof g_full_entries);
    g_full_next = 0; g_full_loot_seq = g_full_loot_area = 0; g_full_render = NULL;
}
static full_token *full_notice_token(uint32_t seq, uint32_t area) {
    for (unsigned i = 0; i < DD_FULL_HISTORY; ++i)
        if (g_full_tokens[i].seq == seq && g_full_tokens[i].area == area && seq) return &g_full_tokens[i];
    return NULL;
}
static void full_notice_loot(uint32_t seq, uint32_t area) {
    g_full_loot_seq = seq; g_full_loot_area = area;
    if (!seq || full_notice_token(seq, area)) return;
    full_token *token = &g_full_tokens[g_full_next++ % DD_FULL_HISTORY];
    token->seq = seq; token->area = area; token->closed = 0;
}
static int full_notice_name(uintptr_t address) {
    char name[sizeof g_full_name];
    return safe_read(address, name, sizeof name) && !memcmp(name, g_full_name, sizeof name);
}
static uintptr_t full_notice_tail(uintptr_t manager) {
    uint64_t slots = 0, tail = 0, mask = 0, count = 0, entry = 0;
    if (!safe_read(manager + 8, &slots, 8) || !slots || !safe_read(manager + 0x10, &tail, 8) ||
        !safe_read(manager + 0x18, &mask, 8) || mask > 4095 || (mask & (mask + 1)) ||
        !safe_read(manager + 0x20, &count, 8) || !count || count > mask + 1 ||
        !safe_read(slots + ((tail - 1) & mask) * 8, &entry, 8)) return 0;
    return (uintptr_t)entry;
}
static void __fastcall hook_confirm_enqueue(void *manager, const char *name, void *params) {
    uintptr_t callback = 1;
    walk_view v = {0};
    int track = walk_shared() && g_full_loot_seq &&
        g_full_notice_caller((uintptr_t)_ReturnAddress() - image_base()) &&
        full_notice_name((uintptr_t)name) && params && safe_read((uintptr_t)params + 0x38, &callback, 8) && !callback &&
        g_walk_view(&v) && !v.battle && v.area == g_full_loot_area;
    if (!g_confirm_enqueue) return;
    g_confirm_enqueue(manager, name, params);
    if (!track) return;
    uintptr_t entry = full_notice_tail((uintptr_t)manager);
    if (!entry || !full_notice_name(entry)) {
        dd_log("Inventory-full: native entry could not be identified"); runtime_sync_stop(3); return;
    }
    full_entry *free_entry = NULL;
    for (unsigned i = 0; i < DD_FULL_HISTORY; ++i) {
        if (g_full_entries[i].entry == entry && g_full_entries[i].manager == (uintptr_t)manager) {
            /* Add can coalesce a duplicate into an existing native notice.
             * Preserve the older cause instead of retagging a live popup. */
            return;
        }
        if (!g_full_entries[i].entry) free_entry = &g_full_entries[i];
    }
    if (!free_entry) { dd_log("Inventory-full: too many pending notices"); runtime_sync_stop(3); return; }
    free_entry->entry = entry; free_entry->manager = (uintptr_t)manager;
    free_entry->seq = g_full_loot_seq; free_entry->area = v.area; free_entry->asked = 0;
    dd_log("Inventory-full: notice for loot sequence=%u area=%08x", free_entry->seq, free_entry->area);
}
static int full_notice_apply(const dd_action *a) {
    full_token *token = full_notice_token(a->u.item.id, a->u.item.area);
    if (!token) { dd_log("Inventory-full: unknown/stale loot sequence=%u; refused", a->u.item.id); return 0; }
    /* No dialog here yet is fine: native processing of the loot may be later.
     * Only an entry captured from this very order can consume the marker. */
    token->closed = 1;
    dd_log("Inventory-full: shared dismissal for loot sequence=%u", token->seq);
    return 1;
}
static void __fastcall hook_confirm_ack(void *capture) {
    uintptr_t manager = 0, callback = 1;
    full_entry *entry = g_full_render;
    if (!g_confirm_ack) return;
    if (!entry || !walk_shared() || g_town.needs_resync ||
        !safe_read((uintptr_t)capture + 8, &manager, 8) || manager != entry->manager ||
        !safe_read((uintptr_t)capture + 0x48, &callback, 8) || callback) {
        g_confirm_ack(capture); return;
    }
    if (entry->asked || runtime_shared_blocked()) return;
    dd_action a = {0};
    a.kind = DD_ACT_ITEM; a.u.item.op = DD_ITEM_OP_FULL_NOTICE; a.u.item.hero = 255;
    a.u.item.id = entry->seq; a.u.item.area = entry->area;
    entry->asked = 1;
    if (g_cfg.role == DD_ROLE_GUEST) dd_town_guest_local(&g_town, &a);
    else if (!dd_town_host_local(&g_town, &a)) entry->asked = 0;
}
static void __fastcall hook_confirm_show_entry(void *manager, void *native_entry) {
    full_entry *saved = g_full_render, *entry = NULL;
    uint32_t before = 0, after = 0;
    walk_view v = {0};
    if (walk_shared() && !g_town.needs_resync && g_walk_view(&v)) {
        for (unsigned i = 0; i < DD_FULL_HISTORY; ++i) {
            full_entry *candidate = &g_full_entries[i];
            if (candidate->entry == (uintptr_t)native_entry && candidate->manager == (uintptr_t)manager &&
                candidate->area == v.area && full_notice_name((uintptr_t)native_entry)) { entry = candidate; break; }
        }
    }
    g_full_render = entry;
    int readable = safe_read((uintptr_t)manager + 0x30, &before, 4);
    if (g_confirm_show_entry) g_confirm_show_entry(manager, native_entry);
    /* Reuse native deferred removal while this exact entry is being shown.
     * Never increment twice, never call an entry's gameplay callback. */
    if (entry && readable && safe_read((uintptr_t)manager + 0x30, &after, 4) && after == before &&
        !g_town.needs_resync && g_confirm_ack) {
        full_token *token = full_notice_token(entry->seq, entry->area);
        if (token && token->closed) {
            uint64_t empty_capture[10] = {0}; empty_capture[1] = (uintptr_t)manager;
            g_confirm_ack(empty_capture);
            memset(entry, 0, sizeof *entry);
        }
    }
    g_full_render = saved;
}

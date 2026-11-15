/* Normal campaign wave-room Continue button. Its callback780240 captures only
 * RaidDisplay, then76fda0 starts the native teleport/fade. WaveLogic moves the
 * dungeon and starts battle later, bypassing the ordinary770080 door hook.
 * Never share those automatic updates a second time. */
typedef uint8_t(__fastcall *wave_continue_fn)(void *);
static wave_continue_fn g_wave_continue;
static uint32_t g_wave_request;
static void wave_reset(void) { g_wave_request = 0; }
static void wave_request_done(uint32_t request) { if (request && request == g_wave_request) g_wave_request = 0; }
typedef struct { uint64_t logic; uint32_t phase, action, group, dungeon; uint8_t waiting, fading; } wave_view;
static uint32_t wave_type_key(void) {
    uint32_t type = 0;
    safe_read(image_base() + 0x2628598, &type, 4);
    return type;
}
static uint32_t (*g_wave_type_key)(void) = wave_type_key;
/* The native Start/FinishTeleportWait checks quest definition+40 against the
 * initialized wave type2628598. The WaveLogic parent must be this same raid. */
static int wave_read(const walk_view *v, wave_view *w) {
    uint64_t quest = 0, definition = 0, owner = 0;
    uint32_t type = 0, wave_type = 0;
    memset(w, 0, sizeof *w);
    wave_type = g_wave_type_key();
    return wave_type &&
        safe_read((uintptr_t)v->raid + 0x52c0, &quest, 8) && quest &&
        safe_read((uintptr_t)quest + 0x108, &definition, 8) && definition &&
        safe_read((uintptr_t)definition + 0x40, &type, 4) && type == wave_type &&
        safe_read((uintptr_t)v->raid + 0x5f38, &w->logic, 8) && w->logic &&
        safe_read((uintptr_t)w->logic, &owner, 8) && owner == v->raid &&
        safe_read((uintptr_t)w->logic + 0x264, &w->phase, 4) && w->phase < 16 &&
        safe_read((uintptr_t)w->logic + 0x268, &w->action, 4) && w->action < 16 &&
        safe_read((uintptr_t)w->logic + 0x26c, &w->group, 4) && w->group < 1000000 &&
        safe_read((uintptr_t)w->logic + 0x38, &w->dungeon, 4) && w->dungeon < 4096 &&
        safe_read((uintptr_t)w->logic + 0x3d0, &w->waiting, 1) && w->waiting <= 1 &&
        safe_read((uintptr_t)v->display + 0x46ac, &w->fading, 1) && w->fading <= 1;
}
static int wave_ready(const walk_view *v, const wave_view *w) {
    uint32_t mode = 0;
    return item_use_phase(v) && !w->waiting && !w->fading &&
        walk_overlay_state(v, &mode) && !mode && !loot_open(v);
}
static int wave_gate_ready(const walk_view *v, const dd_action *a) {
    wave_view w;
    /* A competing click is already stale when the first one starts fading.
     * Reject it now; do not keep it ahead of battle orders for30 seconds. */
    if (a->u.item.id != g_prop_revision) return 1;
    return wave_read(v, &w) && wave_ready(v, &w);
}
static uint8_t __fastcall hook_wave_continue(void *args) {
    walk_view v; wave_view w; dd_action a;
    uint64_t table = 0, display = 0;
    if (!g_wave_continue) return 0;
    if (g_game_apply_depth || !walk_coop_active()) return g_wave_continue(args);
    if (!walk_shared() || runtime_shared_blocked()) return 0;
    if (!g_walk_view(&v) || !safe_read((uintptr_t)args, &table, 8) ||
        table != image_base() + 0xed3ab8 || !safe_read((uintptr_t)args + 8, &display, 8) || display != v.display) return 0;
    /* This same native callback also supports non-wave area changes. Preserve
     * that existing native path, but never fall through a malformed wave. */
    uint64_t quest = 0, definition = 0; uint32_t type = 0, wave_type = 0;
    if (!safe_read((uintptr_t)v.raid + 0x52c0, &quest, 8) || !quest ||
        !safe_read((uintptr_t)quest + 0x108, &definition, 8) || !definition ||
        !safe_read((uintptr_t)definition + 0x40, &type, 4) || !(wave_type = g_wave_type_key())) return 0;
    if (type != wave_type) return g_wave_continue(args);
    if (g_wave_request || !wave_read(&v, &w) || !wave_ready(&v, &w) ||
        !item_order(DD_ITEM_OP_WAVE_CONTINUE, &a, &v)) return 0;
    a.u.item.id = g_prop_revision;
    a.u.item.type = w.phase; a.u.item.amount = w.action;
    a.u.item.round = w.group; a.u.item.turn = w.dungeon;
    a.u.item.pos = 0;
    dd_log("Wave: Continue requested area=%08x phase=%u action=%u group=%u dungeon=%u revision=%u role=%s",
        v.area,w.phase,w.action,w.group,w.dungeon,g_prop_revision,dd_role_name(g_cfg.role));
    if (g_cfg.role == DD_ROLE_GUEST) g_wave_request = dd_town_guest_local(&g_town, &a);
    else (void)dd_town_host_local(&g_town, &a);
    return 0; /* Same return as the native button callback. */
}
static int wave_apply(const dd_action *a) {
    walk_view v; wave_view w;
    if (!dd_action_valid(a) || !g_wave_continue || !g_walk_view(&v) ||
        !wave_read(&v,&w) || !wave_ready(&v,&w) || v.area != a->u.item.area ||
        a->u.item.id != g_prop_revision || a->u.item.type != w.phase || a->u.item.amount != w.action ||
        a->u.item.round != w.group || a->u.item.turn != w.dungeon || a->u.item.rules != battle_rules()) {
        dd_log("Wave: Continue does not match this ready room; refused before effects (revision=%u)",g_prop_revision);
        return 0;
    }
    if (!prop_choice_scene()) return 0;
    uint64_t args[2] = { image_base() + 0xed3ab8, v.display };
    g_wave_continue(args);
    uint8_t waiting = 0, fading = 0;
    if (!safe_read((uintptr_t)w.logic+0x3d0,&waiting,1) || waiting!=1 ||
        !safe_read((uintptr_t)v.display+0x46ac,&fading,1) || fading!=1) {
        dd_log("Wave: native Continue did not start its teleport and fade; stopping before ACK");
        runtime_sync_stop(3); return 0;
    }
    dd_log("Wave: Continue applied area=%08x group=%u dungeon=%u revision=%u role=%s",
        v.area,w.group,w.dungeon,a->u.item.id,dd_role_name(g_cfg.role));
    return 1;
}

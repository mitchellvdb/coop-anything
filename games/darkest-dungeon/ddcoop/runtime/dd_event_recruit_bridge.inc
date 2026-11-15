/* Town event recruit (DD_ACT_EVENT_RECRUIT, protocol 38).
 *
 * Owner, 15 Nov: a hero a town event offered ("Hear Ye") was recruited on the host and stayed in the event on the guest.
 * The event's heroes are not Stagecoach candidates: they live in TownEvent::System (campaign+1808), its Hero entries at
 * +38 (0x15b0 each, +137c ID, +15a8 available). hook_roster_drop only knew the Stagecoach list, so it let the game add
 * the event hero locally as if it were an ordinary roster move. Now the drop is one ordered action. Every copy does what
 * the game does for that drop: Roster::AddEntry (589ab0) with the event's entry, then the event screen's callback
 * (6d0690, lambda_2 of TownEventDisplay::OnEnterStart; captures +0 display, +8 System), which marks the hero taken, sends
 * HeroRecruited, takes the event's cost and saves persist.town_event. That callback needs the event screen's slot that
 * holds the hero, so every copy keeps the TownEventDisplay its OnEnterStart (6d0130) last ran on.
 * tests/test_event_recruit_layout.py proves the layout. */
typedef void(__fastcall *event_display_enter_fn)(void *display);
typedef void(__fastcall *event_recruit_done_fn)(void *captures, void *hero);
static event_display_enter_fn g_event_display_enter;
static event_recruit_done_fn g_event_recruit_done;
static uintptr_t g_event_display;
#define DD_EVENT_DISPLAY_VTABLE 0xec2020u
#define DD_EVENT_SYSTEM 0x1808u
static void __fastcall hook_event_display_enter(void *display) {
    if (g_event_display_enter) g_event_display_enter(display);
    g_event_display = (uintptr_t)display;
}
/* The event screen on this copy shows this hero in an active slot (what 6d0690 looks for). */
static int event_display_holds(uint64_t entry) {
    uint64_t vtable = 0, begin = 0, end = 0;
    if (!g_event_display || !safe_read(g_event_display, &vtable, 8) || vtable != image_base() + DD_EVENT_DISPLAY_VTABLE ||
        !safe_read(g_event_display + 0x120, &begin, 8) || !safe_read(g_event_display + 0x128, &end, 8) || end < begin ||
        (end - begin) % 8 || (end - begin) / 8 > 16)
        return 0;
    for (uint64_t at = begin; at < end; at += 8) {
        uint64_t slot = 0, item = 0, hero = 0;
        int32_t active = 0;
        if (safe_read((uintptr_t)at, &slot, 8) && slot && safe_read((uintptr_t)slot + 0x58, &active, 4) && active &&
            safe_read((uintptr_t)slot + 0x158, &item, 8) && item && safe_read((uintptr_t)item + 0x28, &hero, 8) && hero == entry)
            return 1;
    }
    return 0;
}
static int event_recruit_apply(const dd_action *a) {
    dd_recruit s;
    uint64_t entry = 0, print = 0;
    uint8_t available = 0, after_open = 1;
    unsigned before = 0, after = 0;
    uint32_t id = a->u.recruit.hero_id, limit = 0;
    if (!g_event_recruit_done || !g_roster_add || !g_roster_count || !g_roster_capacity || !g_recruit_open_model(&s) ||
        !dd_recruit_event_candidate(&s, id, &entry, &available) || !available) {
        dd_log("Town event recruit refused: hero %u is not an available event hero here", id);
        return 0;
    }
    if (!dd_recruit_event_fingerprint(&s, id, &print) || print != a->u.recruit.fingerprint) {
        dd_log("Town event recruit refused: hero %u differs here (local=%016llx requested=%016llx)", id,
               (unsigned long long)print, (unsigned long long)a->u.recruit.fingerprint);
        return 0;
    }
    unsigned used = g_roster_count(), capacity = g_roster_capacity();
    if (dd_recruit_roster_has(&s, id, &before) != 0 || !safe_read((uintptr_t)entry + 0x144c, &limit, 4) || limit ||
        !capacity || capacity > 64 || used >= capacity) {
        dd_log("Town event recruit refused: hero %u present, special roster limit %u or roster full (%u/%u)", id, limit, used, capacity);
        return 0;
    }
    if (!event_display_holds(entry)) {
        dd_log("Town event recruit refused: the event screen here does not show hero %u", id);
        return 0;
    }
    uint64_t captures[2] = {g_event_display, s.campaign + DD_EVENT_SYSTEM};
    if (!g_roster_add((void *)(uintptr_t)s.campaign, (void *)(uintptr_t)entry)) return 0;
    g_event_recruit_done(captures, (void *)(uintptr_t)entry);
    int present = dd_recruit_roster_has(&s, id, &after);
    int readable = dd_recruit_event_candidate(&s, id, &entry, &after_open);
    dd_log("Town event recruit hero=%u roster=%u->%u present=%d taken=%d (role=%s)", id, before, after, present,
           readable ? !after_open : -1, dd_role_name(g_cfg.role));
    if (present == 1 && after == before + 1 && readable && !after_open) return 1;
    g_town.needs_resync = 1;
    dd_log("Town event recruit partial result; shared actions stop");
    return 0;
}
/* From hook_roster_drop: 1 = this drop was an event hero and is handled (shared or refused), 0 = not an event hero. */
static int event_recruit_drop(uint64_t entry, uint32_t id) {
    dd_recruit s;
    uint64_t at = 0;
    uint8_t available = 0;
    uint32_t limit = 0;
    if (!entry || !id || !g_recruit_open_model(&s) || !dd_recruit_event_candidate(&s, id, &at, &available) || at != entry ||
        !available)
        return 0;
    if (!safe_read((uintptr_t)entry + 0x144c, &limit, 4) || limit) {
        shop_notice("This hero uses a special roster limit that is not supported in this kit yet.");
        return 1;
    }
    if (!InterlockedCompareExchange(&g_session_ready, 0, 0) || g_town.needs_resync) {
        shop_notice("Wait until both games are connected before recruiting. After a synchronization error, restart both games.");
        return 1;
    }
    if (!g_event_recruit_done || !event_display_holds(entry)) {
        dd_log("Town event recruit: dropped hero %u is not in this copy's event screen", id);
        shop_notice("This event hero could not be shared. Nothing changed; send the logs.");
        return 1;
    }
    dd_action a = {0};
    a.kind = DD_ACT_EVENT_RECRUIT;
    a.u.recruit.hero_id = id;
    if (!dd_recruit_event_fingerprint(&s, id, &a.u.recruit.fingerprint)) {
        dd_log("Town event recruit: hero %u unreadable", id);
        shop_notice("This event hero could not be shared. Nothing changed; send the logs.");
        return 1;
    }
    dd_log("Town event recruit dragged here: hero=%u (role=%s)", id, dd_role_name(g_cfg.role));
    if (g_cfg.role == DD_ROLE_HOST) {
        if (!dd_town_host_local(&g_town, &a) && !g_town.needs_resync)
            shop_notice("This hero could not be recruited. Check the roster space and try again.");
    } else
        (void)dd_town_guest_local(&g_town, &a);
    return 1;
}

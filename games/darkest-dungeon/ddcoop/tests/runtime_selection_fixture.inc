/* Post-combat SELECT is an ordered choice, not proof that every native battle
 * presentation has already finished. Real gates/FIFO/codec; native select only
 * is modeled. Keep every participant's old selected hero until it is ready. */
static int sel_native_no_effect;
static void __fastcall sel_native(void *party, int32_t index) {
    if (sel_native_no_effect) {
        CHECK(party == (void *)(wk_raid_lists + 8));
        wk_selects++; wk_select_index = index;
        return;
    }
    wk_select(party, index);
    if (!sel_native_no_effect && index >= 0 && index < 4)
        putptr(wk_raid_lists + 0x438, bt_party[index]);
}
static void sel_reset(int role, unsigned player) {
    it_reset(role);
    g_cfg.player = role == DD_ROLE_GUEST ? player : 0;
    g_town.next_request = role == DD_ROLE_GUEST ? (player << 24) | 1u : 1;
    g_party_select = sel_native;
    sel_native_no_effect = 0;
    putptr(wk_raid_lists + 0x438, bt_party[0]);
    putint(wk_display + 0x3f88, 3);
    g_walk_selected = 0;
}
static dd_msg sel_wire(dd_msg m) {
    uint8_t bytes[DD_PROTO_HEADER + DD_PROTO_MAX_BODY];
    dd_msg decoded = {0}; size_t used = 0;
    size_t n = dd_encode(&m, bytes, sizeof bytes);
    CHECK(n && dd_decode(bytes, n, &decoded, &used) == DD_DECODE_OK && used == n);
    return decoded;
}
static uint32_t sel_selected_id(void) {
    uint64_t hero = 0; uint32_t id = 0;
    memcpy(&hero, wk_raid_lists + 0x438, 8);
    CHECK(hero && safe_read(hero + 0x137c, &id, 4));
    return id;
}
static dd_msg sel_choice(unsigned initiator) {
    sel_reset(initiator ? DD_ROLE_GUEST : DD_ROLE_HOST, initiator);
    putptr(wk_raid_lists + 0x438, bt_party[2]);
    walk_selection_poll(&wk_view);
    CHECK(sent == 1 && !wk_selects && last_message.u.town.action.kind == DD_ACT_ITEM);
    CHECK(last_message.u.town.action.u.item.op == DD_ITEM_OP_SELECT &&
          last_message.u.town.action.u.item.hero == 2 &&
          last_message.u.town.action.u.item.id == 13 &&
          last_message.u.town.action.u.item.area == 100);
    return sel_wire(last_message);
}
static void selection_transition_checks(void) {
    dd_msg orders[4];
    /* Guest requests are allowed while the host is still finishing its battle.
     * The host waits instead of rejecting a valid peer's slightly earlier UI. */
    for (unsigned initiator = 0; initiator < 4; initiator++) {
        dd_msg choice = sel_choice(initiator);
        if (initiator) {
            CHECK(choice.type == DD_MSG_TOWN_REQUEST &&
                  (choice.u.town.request_id >> 24) == initiator);
            sel_reset(DD_ROLE_HOST, 0); wk_view.battle = 34;
            CHECK(queue_push(&g_incoming, &choice));
            dd_runtime_poll(); dd_runtime_poll();
            CHECK(g_incoming.count == 1 && !wk_selects && !g_town.accepted &&
                  !g_town.rejected && !sent && !g_town.needs_resync);
            wk_view.battle = 0; dd_runtime_poll();
            CHECK(!g_incoming.count && wk_selects == 1 && wk_select_index == 2 &&
                  sel_selected_id() == 13 && g_town.applied_seq == 1 &&
                  !g_town.needs_resync && sent == 1);
            choice = sel_wire(last_message);
            CHECK(choice.type == DD_MSG_TOWN_APPLY &&
                  choice.u.town.request_id == ((initiator << 24) | 1u));
        } else CHECK(choice.type == DD_MSG_TOWN_APPLY);
        orders[initiator] = choice;
        for (unsigned receiver = 1; receiver < 4; receiver++) {
            sel_reset(DD_ROLE_GUEST, receiver); wk_view.battle = 34;
            dd_msg later = {0}; later.type = DD_MSG_TOWN_APPLY; later.seq = 2;
            later.u.town.action.kind = DD_ACT_ITEM;
            later.u.town.action.u.item.op = DD_ITEM_OP_RULES;
            later.u.town.action.u.item.hero = 255;
            CHECK(queue_push(&g_incoming, &choice) && queue_push(&g_incoming, &later));
            dd_runtime_poll(); dd_runtime_poll();
            CHECK(g_incoming.count == 2 && !wk_selects && !g_town.applied_seq &&
                  !g_town.accepted && !g_town.needs_resync && !g_sync_stop_sent);
            /* A transient missing display is another readiness delay. */
            wk_view_fail = 1; dd_runtime_poll();
            CHECK(g_incoming.count == 2 && !wk_selects && !g_town.applied_seq &&
                  !g_town.needs_resync);
            wk_view_fail = 0; wk_view.battle = 0; dd_runtime_poll();
            int32_t display_field = -1; memcpy(&display_field, wk_display + 0x3f88, 4);
            CHECK(!g_incoming.count && wk_selects == 1 && wk_select_index == 2 &&
                  sel_selected_id() == 13 && display_field == 1 &&
                  g_town.applied_seq == 2 && g_town.accepted == 2 && !g_town.needs_resync);
            dd_runtime_poll();
            CHECK(wk_selects == 1 && g_town.applied_seq == 2); /* no echo/repeat */
        }
    }
    /* Ordinary selection needs no battle/position wait. Native loot/UI camera
     * offsets do not change which persistent hero a valid choice identifies. */
    for (unsigned receiver = 1; receiver < 4; receiver++) {
        sel_reset(DD_ROLE_GUEST, receiver); wk_view.pos += 1800.0f;
        CHECK(queue_push(&g_incoming, &orders[0])); dd_runtime_poll();
        CHECK(wk_selects == 1 && g_town.applied_seq == 1 && !g_town.needs_resync &&
              sel_selected_id() == 13);
    }
    /* A different area or battle that never ends times out with no ACK/effect.
     * Reusing the message after that stop may not revive this copy. */
    for (unsigned missing = 0; missing < 3; missing++) {
        sel_reset(DD_ROLE_GUEST, 2);
        if (!missing) wk_view.area++;
        else if (missing == 1) wk_view.battle = 34;
        else wk_view_fail = 1;
        CHECK(queue_push(&g_incoming, &orders[0])); dd_runtime_poll();
        CHECK(g_incoming.count == 1 && !wk_selects && !g_town.applied_seq && !g_town.needs_resync);
        g_item_wait_until = 1; dd_runtime_poll();
        CHECK(!wk_selects && !g_town.applied_seq && g_town.needs_resync && g_sync_stop_sent);
        wk_view_fail = 0; wk_view.battle = 0; wk_view.area = 100; dd_runtime_poll();
        CHECK(!wk_selects && !g_town.applied_seq && g_town.needs_resync);
    }
    /* Same area/phase alone is not permission to select another hero, operate
     * on an invalid vector, or ACK a missing native effect. */
    for (unsigned broken = 0; broken < 10; broken++) {
        sel_reset(DD_ROLE_GUEST, 1); dd_msg bad = orders[0];
        if (broken == 0) bad.u.town.action.u.item.id = 99;
        if (broken == 1) bt_party[2] = 0;
        if (broken == 2) putptr(wk_raid_lists + 0x10, (uintptr_t)bt_party + 33);
        if (broken == 3) putptr(wk_raid_lists + 0x10, (uintptr_t)bt_party + 40);
        if (broken == 4) putptr(wk_raid_lists + 0x10, (uintptr_t)bt_party - 8);
        if (broken == 5) putptr(wk_raid_lists + 0x10, (uintptr_t)bt_party + 16);
        if (broken == 6) g_party_select = NULL;
        if (broken == 7) sel_native_no_effect = 1;
        if (broken == 8) { uint32_t stale_id = 77; memcpy(bt_heroes[2] + 0x137c, &stale_id, 4); }
        if (broken == 9) bad.u.town.action.u.item.id = 0;
        CHECK(queue_push(&g_incoming, &bad)); dd_runtime_poll();
        CHECK(!g_town.applied_seq && g_town.needs_resync && g_sync_stop_sent &&
              wk_selects == (unsigned)(broken == 7));
    }
    /* A valid user choice still refuses before native execution after a stop;
     * invalid sequence numbers cannot bypass the normal ordering checks. */
    sel_reset(DD_ROLE_GUEST, 3); g_town.needs_resync = 1;
    CHECK(queue_push(&g_incoming, &orders[0])); dd_runtime_poll();
    CHECK(!wk_selects && !g_town.applied_seq);
    sel_reset(DD_ROLE_GUEST, 3); dd_msg future = orders[0]; future.seq = 2;
    CHECK(queue_push(&g_incoming, &future)); dd_runtime_poll();
    CHECK(!wk_selects && !g_town.applied_seq && g_town.needs_resync && g_sync_stop_sent);
    sel_reset(DD_ROLE_GUEST, 1); bt_unwire();
    puts("Post-combat selection: all4 initiators/3 guests wait without ACK or overtaking during native battle/loading; persistent identity, bounded party, exact native effect, missing view/area timeout and stopped session verified");
}

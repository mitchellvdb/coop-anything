/* Report: returned to Hamlet, two remembered party members entered buildings.
 * Replacing only one of them left the other in 'after', rejecting every edit. */
static void party_return_checks(void) {
    hm_reset(DD_ROLE_HOST);
    hm_party[0] = 1; hm_party[1] = 2;
    putint(hm_entries[0] + 0x15b0, 2);
    putint(hm_entries[1] + 0x15b0, 3);
    hook_hamlet_party_slot(hm_slots[2], hm_entries[2] + 8);
    CHECK(sent == 1 && last_message.type == DD_MSG_TOWN_APPLY);
    CHECK(hm_party[0] == UINT32_MAX && hm_party[1] == UINT32_MAX && hm_party[2] == 3);
    CHECK(hm_entries[0][0x15b0] == 2 && hm_entries[1][0x15b0] == 3 && hm_entries[2][0x15b0] == 1);
    CHECK(!g_town.needs_resync);
    for (unsigned player = 1; player <= 3; player++) {
        hm_reset(DD_ROLE_GUEST); g_cfg.player = player;
        hm_party[0] = 1; hm_party[1] = 2;
        putint(hm_entries[0] + 0x15b0, 2); putint(hm_entries[1] + 0x15b0, 3);
        hook_hamlet_party_slot(hm_slots[2], hm_entries[2] + 8);
        CHECK(sent == 1 && last_message.type == DD_MSG_TOWN_REQUEST && !hm_native);
        CHECK(hm_party[0] == 1 && hm_party[1] == 2 && hm_entries[2][0x15b0] == 0);
        dd_msg request = last_message;
        request.u.town.request_id = (player << 24) | 1;
        CHECK(request.u.town.action.u.party.before[0] == 1 && request.u.town.action.u.party.before[1] == 2);
        CHECK(!request.u.town.action.u.party.after[0] && !request.u.town.action.u.party.after[1] && request.u.town.action.u.party.after[2] == 3);
        /* The host need not open expedition UI and owns all combat heroes. */
        hm_reset(DD_ROLE_HOST); g_hero_owner_mask = 0;
        hm_party[0] = 1; hm_party[1] = 2;
        putint(hm_entries[0] + 0x15b0, 2); putint(hm_entries[1] + 0x15b0, 3);
        uint8_t treated[2][sizeof hm_entries[0]];
        memcpy(treated, hm_entries, sizeof treated);
        putptr(hm_embark + 0x90, (uintptr_t)hm_rows);
        CHECK(queue_push(&g_incoming, &request)); dd_runtime_poll();
        CHECK(g_town.accepted == 1 && !g_town.rejected && !hm_slot_calls && !g_town.needs_resync);
        CHECK(hm_party[0] == UINT32_MAX && hm_party[1] == UINT32_MAX && hm_party[2] == 3 && !g_hero_owner_mask);
        CHECK(!memcmp(treated, hm_entries, sizeof treated));
        dd_msg order = last_message;
        CHECK(order.type == DD_MSG_TOWN_APPLY);
        for (unsigned receiver = 1; receiver <= 3; receiver++) {
            hm_reset(DD_ROLE_GUEST); g_cfg.player = receiver;
            hm_party[0] = 1; hm_party[1] = 2;
            putint(hm_entries[0] + 0x15b0, 2); putint(hm_entries[1] + 0x15b0, 3);
            memcpy(treated, hm_entries, sizeof treated);
            if (receiver & 1) putptr(hm_embark + 0x90, (uintptr_t)hm_rows);
            CHECK(queue_push(&g_incoming, &order)); dd_runtime_poll();
            CHECK(g_town.applied_seq == 1 && !g_town.needs_resync && !g_incoming.count);
            CHECK(hm_party[0] == UINT32_MAX && hm_party[1] == UINT32_MAX && hm_party[2] == 3 && hm_entries[2][0x15b0] == 1);
            CHECK(!memcmp(treated, hm_entries, sizeof treated));
            putptr(hm_embark + 0x90, (uintptr_t)(hm_rows + 4)); g_party_pending_at = 0; dd_runtime_poll();
            uint64_t shown = 0; memcpy(&shown, hm_slots[2] + 0x28, 8);
            CHECK(shown == (uintptr_t)(hm_entries[2] + 8) && !g_party_pending_mask);
            CHECK(!memcmp(treated, hm_entries, sizeof treated));
        }
    }
    /* Restoring an available member at the same remembered rank used to emit
     * show-only for the entire invalid party. It must prune the other busy ID. */
    hm_reset(DD_ROLE_HOST); hm_party[1] = 2; putint(hm_entries[1] + 0x15b0, 2);
    memset(hm_slots[0] + 0x28, 0, 8);
    hook_hamlet_party_slot(hm_slots[0], hm_entries[0] + 8);
    CHECK(sent == 1 && !last_message.u.town.action.u.party.show && hm_party[0] == 1 && hm_party[1] == UINT32_MAX);
    CHECK(hm_entries[1][0x15b0] == 2);
    /* Selecting a busy new hero is still refused without changing its treatment. */
    hm_reset(DD_ROLE_HOST); putint(hm_entries[1] + 0x15b0, 2);
    hook_hamlet_party_slot(hm_slots[1], hm_entries[1] + 8);
    CHECK(!sent && !hm_native && hm_entries[1][0x15b0] == 2 && hm_party[0] == 1 && hm_party[1] == UINT32_MAX);
    /* Add/move/remove from every participant, independent of combat ownership.
     * These still travel through the host FIFO before a guest changes locally. */
    for (unsigned initiator = 0; initiator <= 3; initiator++) for (unsigned operation = 0; operation < 3; operation++) {
        int role = initiator ? DD_ROLE_GUEST : DD_ROLE_HOST;
        hm_reset(role); g_cfg.player = initiator; g_hero_owner_mask = 0;
        if (operation) {hm_party[1] = 2; putint(hm_entries[1] + 0x15b0, 1);}
        hook_hamlet_party_slot(hm_slots[operation == 1 ? 2 : 1], operation == 2 ? NULL : hm_entries[1] + 8);
        CHECK(sent == 1 && !g_town.needs_resync);
        dd_msg order = last_message;
        if (initiator) {
            CHECK(!hm_native && order.type == DD_MSG_TOWN_REQUEST);
            order.u.town.request_id = (initiator << 24) | 1;
            hm_reset(DD_ROLE_HOST); g_hero_owner_mask = 0;
            if (operation) {hm_party[1] = 2; putint(hm_entries[1] + 0x15b0, 1);}
            CHECK(queue_push(&g_incoming, &order)); dd_runtime_poll();
            CHECK(g_town.accepted == 1 && !g_town.rejected && !g_town.needs_resync);
            order = last_message;
        }
        CHECK(order.type == DD_MSG_TOWN_APPLY && !g_hero_owner_mask);
        CHECK(hm_party[1] == (operation ? UINT32_MAX : 2) && hm_party[2] == (operation == 1 ? 2 : UINT32_MAX));
        hm_reset(DD_ROLE_GUEST);
        if (operation) {hm_party[1] = 2; putint(hm_entries[1] + 0x15b0, 1);}
        CHECK(queue_push(&g_incoming, &order)); dd_runtime_poll();
        CHECK(g_town.applied_seq == 1 && !g_town.needs_resync);
        CHECK(hm_party[1] == (operation ? UINT32_MAX : 2) && hm_party[2] == (operation == 1 ? 2 : UINT32_MAX));
    }
    hm_reset(DD_ROLE_HOST);
    puts("Party return: multiple busy heroes pruned, treatments byte-identical, all four players add/move/remove regardless of combat owner, closed host/guest model and deferred native UI, same-rank cleanup and busy-new-hero refusal verified");
}

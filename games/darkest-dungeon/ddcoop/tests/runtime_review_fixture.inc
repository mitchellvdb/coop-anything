/* Independent audit regressions: a logged refusal is not a successful apply. */
static void runtime_review_checks(void) {
    dd_action a = {0};
    uint8_t rules[16] = {0}; uint32_t actual = 2;
    bt_reset(DD_ROLE_GUEST); memcpy(rules + 8, &actual, 4); g_rules_object = rules;
    a.kind = DD_ACT_BATTLE; a.u.battle.op = DD_BATTLE_OP_SKILL;
    a.u.battle.round = 2; a.u.battle.turn = 1; a.u.battle.state = DD_BATTLE_CHOOSE_TARGET_STATE;
    a.u.battle.hero = 2; a.u.battle.actor_id = 13; a.u.battle.value = 1; a.u.battle.rules = 1;
    wk_push(1, &a); dd_runtime_poll();
    CHECK(!bt_picks && !g_town.applied_seq && g_town.needs_resync && g_sync_stop_sent);
    g_rules_object = NULL;
    it_reset(DD_ROLE_GUEST); bt_at(2, 1, DD_BATTLE_CHOOSE_TARGET_STATE);
    memset(&a, 0, sizeof a); a.kind = DD_ACT_ITEM; a.u.item.op = DD_ITEM_OP_USE;
    a.u.item.round = 2; a.u.item.turn = 1; a.u.item.state = DD_BATTLE_CHOOSE_TARGET_STATE;
    a.u.item.hero = 2; a.u.item.slot = 3; a.u.item.flag = 1; a.u.item.rules = 1;
    memcpy(rules + 8, &actual, 4); g_rules_object = rules;
    wk_push(1, &a); dd_runtime_poll();
    CHECK(!it_uses && !g_town.applied_seq && g_town.needs_resync && g_sync_stop_sent);
    g_rules_object = NULL;
    it_camp_reset(DD_ROLE_GUEST, 0); bt_at(2, 1, DD_BATTLE_CHOOSE_TARGET_STATE);
    a.u.item.op = DD_ITEM_OP_RETREAT;
    memcpy(rules + 8, &actual, 4); g_rules_object = rules;
    wk_push(1, &a); dd_runtime_poll();
    CHECK(!it_retreats && !g_town.applied_seq && g_town.needs_resync && g_sync_stop_sent);
    g_rules_object = NULL;

    int (*saved_view)(walk_view *) = g_walk_view;
    hm_raid_reset(DD_ROLE_GUEST); g_walk_view = hm_raid_view; hm_loot_window(2);
    dd_hamlet model; uint64_t inv = 0; unsigned count = 0; uint32_t quantity = 0;
    memset(&a, 0, sizeof a); a.kind = DD_ACT_TRINKET_MOVE;
    a.u.move.src_hero = DD_TRINKET_LOOT; a.u.move.src_slot = 2;
    a.u.move.dst_hero = 1; a.u.move.dst_slot = 1;
    CHECK(g_hamlet_open_model(&model) && dd_hamlet_inventory(&model, DD_TRINKET_LOOT, &inv, &count));
    CHECK(dd_hamlet_item(&model, inv, 2, &a.u.move.src_type, &a.u.move.src_id, &quantity) && quantity == 1);
    memset(hm_raid_loot, 0, sizeof hm_raid_loot);
    wk_push(1, &a); dd_runtime_poll();
    CHECK(!g_town.applied_seq && g_town.needs_resync && g_sync_stop_sent && !hm_item_alive);
    g_walk_view = saved_view;

    it_reset(DD_ROLE_GUEST); it_bag_set();
    memset(&a, 0, sizeof a); a.kind = DD_ACT_ITEM; a.u.item.op = DD_ITEM_OP_BAG_SLOT;
    a.u.item.hero = 255; a.u.item.slot = 0; a.u.item.amount = 1;
    a.u.item.type = 11; a.u.item.id = 21;
    wk_push(1, &a); dd_runtime_poll();
    CHECK(!g_town.applied_seq && g_town.needs_resync && g_sync_stop_sent);
    /* All three guests keep independent votes; a zero from an idle window
     * cannot erase a held key, nor may packet order decide an opposition. */
    wk_reset(DD_ROLE_HOST);
    dd_msg wish = {0}; wish.type = DD_MSG_WALK_INTENT;
    wish.u.intent.direction = 1; walk_intent_receive(&wish, 1);
    wish.u.intent.direction = 0; walk_intent_receive(&wish, 2); walk_intent_receive(&wish, 3);
    CHECK(walk_direction(0, GetTickCount64()) == 1);
    CHECK(walk_direction(-1, GetTickCount64()) == 0);
    wish.u.intent.direction = -1; walk_intent_receive(&wish, 2);
    CHECK(walk_direction(0, GetTickCount64()) == 0);
    wish.u.intent.direction = 0; walk_intent_receive(&wish, 2);
    CHECK(walk_direction(1, GetTickCount64()) == 1);
    InterlockedExchange64(&g_walk_intents[0], (LONG64)((GetTickCount64() - 1000) << 2) | 2);
    CHECK(!walk_direction(0, GetTickCount64()));
    wish.u.intent.direction = 1; walk_intent_receive(&wish, 0); walk_intent_receive(&wish, 4);
    CHECK(!walk_direction(0, GetTickCount64()));
    const unsigned orders[6][3] = {{1,2,3},{1,3,2},{2,1,3},{2,3,1},{3,1,2},{3,2,1}};
    for (unsigned i = 0; i < 6; i++) {
        walk_reset();
        for (unsigned j = 0; j < 3; j++) {
            unsigned player = orders[i][j]; wish.u.intent.direction = player == 1 ? 1 : player == 2 ? -1 : 0;
            walk_intent_receive(&wish, (int)player);
        }
        CHECK(!walk_direction(0, GetTickCount64()));
    }
    dd_msg hello; hello_message(&hello, "Guest1"); CHECK(hello_compatible(&hello));
    hello.u.hello.mod_version[0] ^= 1; CHECK(!hello_compatible(&hello));
    hello_message(&hello, "Guest1"); hello.u.hello.build[0] ^= 1; CHECK(!hello_compatible(&hello));
    hello_message(&hello, "Guest1"); hello.u.hello.protocol++; CHECK(!hello_compatible(&hello));
    bt_unwire(); wk_reset(DD_ROLE_HOST);
    puts("Runtime audit: no ACK/native effect after RNG/item mismatch; independent three-guest movement/zero/conflict/stale/order checks and exact runtime hello version verified");
}

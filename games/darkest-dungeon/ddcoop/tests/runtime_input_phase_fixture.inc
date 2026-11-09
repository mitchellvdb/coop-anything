/* The Collector report stopped Player3 in skill-selection (29) while a
 * SELECT issued from target-selection (30) waited ahead of its own effect. */
static void battle_input_phase_checks(void) {
    dd_action choice;
    uint64_t functor[2] = {0};
    bt_reset(DD_ROLE_HOST);
    hook_battle_skill_button(functor);
    choice = last_message.u.town.action;
    CHECK(choice.u.battle.state == DD_BATTLE_CHOOSE_TARGET_STATE);
    for (unsigned player = 1; player <= 3; player++) {
        bt_reset(DD_ROLE_GUEST); g_cfg.player = player;
        bt_at(2, 1, player == 2 ? DD_BATTLE_CHOOSE_SKILL_STATE : DD_BATTLE_CHOOSE_TARGET_STATE);
        wk_push(1, &choice); dd_runtime_poll();
        CHECK(bt_picks == 1 && g_town.applied_seq == 1 && !g_incoming.count && !g_town.needs_resync);
    }
    /* SELECT can also clear/change a skill from29 while the receiver is30.
     * It does not write the state machine; native frame updates still own it. */
    choice.u.battle.state = DD_BATTLE_CHOOSE_SKILL_STATE;
    choice.u.battle.value = -1;
    bt_reset(DD_ROLE_GUEST); wk_push(1, &choice); dd_runtime_poll();
    CHECK(bt_picks == 1 && bt_pick_slot == -1 && wk_view.battle == DD_BATTLE_CHOOSE_TARGET_STATE && !g_town.needs_resync);
    /* Guest-first SELECT reaches the host in29, with the same owner check. */
    bt_reset(DD_ROLE_HOST); g_hero_owner_mask = 2u << 4;
    bt_at(2, 1, DD_BATTLE_CHOOSE_SKILL_STATE);
    choice.u.battle.state = DD_BATTLE_CHOOSE_TARGET_STATE;
    {dd_msg m = {0}; m.type = DD_MSG_TOWN_REQUEST; m.u.town.request_id = (2u << 24) | 1; m.u.town.action = choice;
     CHECK(queue_push(&g_incoming, &m)); dd_runtime_poll();
     CHECK(bt_picks == 1 && g_town.accepted == 1 && !g_incoming.count && wk_view.battle == DD_BATTLE_CHOOSE_SKILL_STATE);
     g_hero_owner_mask = 0; m.u.town.request_id++;
     CHECK(queue_push(&g_incoming, &m)); dd_runtime_poll(); CHECK(bt_picks == 1 && g_town.rejected == 1);}
    /* A TARGET still waits for native30. No target or move may run early. */
    bt_reset(DD_ROLE_GUEST); bt_at(2, 1, DD_BATTLE_CHOOSE_SKILL_STATE);
    choice.u.battle.op = DD_BATTLE_OP_ENEMY; choice.u.battle.value = 0; choice.u.battle.side = 1; choice.u.battle.place = 0;
    wk_push(1, &choice); dd_runtime_poll();
    CHECK(!bt_enemies && !g_town.applied_seq && g_incoming.count == 1);
    bt_at(2, 1, DD_BATTLE_CHOOSE_TARGET_STATE); dd_runtime_poll();
    CHECK(bt_enemies == 1 && g_town.applied_seq == 1);
    bt_unwire();
    puts("Battle input phase: all three guests apply a skill choice before waiting for its resulting target state");
}

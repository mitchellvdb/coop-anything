/* Actual Wave bridge/FIFO; only native game effect and initialized type key are
 * faked. Owned byte buffers exercise the real rooted readers. */
static uint8_t wave_logic_bytes[0x400], wave_quest_bytes[0x120], wave_definition_bytes[0x60];
static unsigned wave_native_calls;
static int wave_partial;
static uint32_t wave_test_key(void) { return 0x11223344; }
static uint8_t __fastcall wave_native(void *args) {
    uint64_t table = 0, display = 0;
    memcpy(&table,args,8);memcpy(&display,(uint8_t *)args+8,8);
    CHECK(table == image_base()+0xed3ab8 && display == (uintptr_t)wk_display);
    wave_native_calls++;
    wave_logic_bytes[0x3d0] = 1;
    if (!wave_partial) wk_display[0x46ac] = 1;
    return 0;
}
static void wave_fixture_reset(int role) {
    wk_reset(role);
    memset(wave_logic_bytes,0,sizeof wave_logic_bytes);
    memset(wave_quest_bytes,0,sizeof wave_quest_bytes);
    memset(wave_definition_bytes,0,sizeof wave_definition_bytes);
    wk_display[0x46ac] = 0;
    putint(wk_display+DD_WALK_OVERLAY+0xd88,0);
    putint(wk_display+DD_WALK_LOOT_OVERLAY+8,0);
    putptr(wk_raid_lists+0x52c0,(uintptr_t)wave_quest_bytes);
    putptr(wk_raid_lists+0x5f38,(uintptr_t)wave_logic_bytes);
    putptr(wave_quest_bytes+0x108,(uintptr_t)wave_definition_bytes);
    putint(wave_definition_bytes+0x40,wave_test_key());
    putptr(wave_logic_bytes,(uintptr_t)wk_raid_lists);
    putint(wave_logic_bytes+0x264,2);
    putint(wave_logic_bytes+0x268,1);
    putint(wave_logic_bytes+0x26c,3);
    putint(wave_logic_bytes+0x38,1);
    g_wave_continue=wave_native;g_wave_type_key=wave_test_key;
    wave_native_calls=0;wave_partial=0;g_wave_request=0;
    g_town.apply=runtime_apply;g_walk_view=wk_read;
}
static void wave_transactions(void) {
    wave_continue_fn saved_native=g_wave_continue;
    uint32_t (*saved_key)(void)=g_wave_type_key;
    int (*saved_view)(walk_view *)=g_walk_view;
    dd_action ordered={0};
    uint64_t args[2]={image_base()+0xed3ab8,(uintptr_t)wk_display};
    for(unsigned initiator=0;initiator<4;initiator++) {
        wave_fixture_reset(initiator?DD_ROLE_GUEST:DD_ROLE_HOST);
        if(initiator)g_cfg.player=initiator;
        CHECK(!hook_wave_continue(args));
        CHECK(sent==1 && last_message.u.town.action.u.item.op==DD_ITEM_OP_WAVE_CONTINUE);
        ordered=last_message.u.town.action;
        CHECK(dd_action_valid(&ordered));
        if(initiator) {
            CHECK(!wave_native_calls && !wave_logic_bytes[0x3d0] && !wk_display[0x46ac]);
            hook_wave_continue(args);CHECK(sent==1); /* no repeated request while waiting */
            wave_fixture_reset(DD_ROLE_HOST);
            dd_msg m={0};m.type=DD_MSG_TOWN_REQUEST;m.u.town.request_id=(initiator<<24)|1;m.u.town.action=ordered;
            CHECK(queue_push(&g_incoming,&m));dd_runtime_poll();
        }
        CHECK(wave_native_calls==1 && wave_logic_bytes[0x3d0] && wk_display[0x46ac]);
        dd_msg crossed={0};crossed.type=DD_MSG_TOWN_REQUEST;crossed.u.town.request_id=0x02000002;crossed.u.town.action=ordered;
        CHECK(queue_push(&g_incoming,&crossed));dd_runtime_poll();
        CHECK(!g_incoming.count && wave_native_calls==1 && g_town.rejected==1 && !g_town.needs_resync);
        for(unsigned receiver=1;receiver<4;receiver++) {
            wave_fixture_reset(DD_ROLE_GUEST);g_cfg.player=receiver;
            wk_push(1,&ordered);dd_runtime_poll();
            CHECK(g_town.applied_seq==1 && wave_native_calls==1 && !g_town.needs_resync);
            /* Duplicate sequence and crossed old choice never repeat teleport. */
            wk_push(1,&ordered);dd_runtime_poll();CHECK(wave_native_calls==1);
        }
    }
    /* A peer still fading/loading waits at the FIFO head, then acts once. */
    wave_fixture_reset(DD_ROLE_GUEST);wk_view.state=7;wk_push(1,&ordered);dd_runtime_poll();
    CHECK(!wave_native_calls && g_incoming.count==1 && !g_town.applied_seq);
    wk_view.state=2;dd_runtime_poll();CHECK(wave_native_calls==1 && g_town.applied_seq==1);
    for(unsigned scenario=0;scenario<9;scenario++) {
        wave_fixture_reset(DD_ROLE_GUEST);
        dd_action bad=ordered;
        if(scenario==0)bad.u.item.area++;
        if(scenario==1)bad.u.item.id++;
        if(scenario==2)bad.u.item.round++;
        if(scenario==3)bad.u.item.turn++;
        if(scenario==4)bad.u.item.type++;
        if(scenario==5)bad.u.item.amount++;
        if(scenario==6)bad.u.item.rules^=1;
        if(scenario==7)putptr(wave_logic_bytes,(uintptr_t)wk_areas);
        if(scenario==8)wave_logic_bytes[0x3d0]=1;
        CHECK(!wave_apply(&bad) && !wave_native_calls);
    }
    wave_fixture_reset(DD_ROLE_GUEST);wave_partial=1;wk_push(1,&ordered);dd_runtime_poll();
    CHECK(wave_native_calls==1 && !g_town.applied_seq && g_town.needs_resync && g_sync_stop_sent);
    wave_fixture_reset(DD_ROLE_HOST);uint64_t alien[2]={image_base()+0xed3ac0,(uintptr_t)wk_display};
    hook_wave_continue(alien);CHECK(!sent && !wave_native_calls);
    args[1]++;hook_wave_continue(args);CHECK(!sent && !wave_native_calls);args[1]--;
    putint(wave_definition_bytes+0x40,123);hook_wave_continue(args);
    CHECK(wave_native_calls==1 && !sent); /* unchanged ordinary native callback */
    wave_fixture_reset(DD_ROLE_GUEST);wk_view.state=7;wk_push(1,&ordered);dd_runtime_poll();
    g_item_wait_until=1;dd_runtime_poll();
    CHECK(!wave_native_calls && !g_town.applied_seq && g_town.needs_resync);
    g_wave_continue=saved_native;g_wave_type_key=saved_key;g_walk_view=saved_view;wave_reset();
    puts("Wave Continue: all4 initiators/3 peers, no optimistic guest effect, ready FIFO, replay/stale/foreign/model guards and native partial failure checked");
}

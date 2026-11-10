/* Regressions: duplicate Iron Maiden OPEN, no-shovel obstacle confirmation,
 * and an overlay that left the guest's item/walking input behind the host. */
static unsigned pcx_opens,pcx_confirms,pcx_asserts;
static unsigned pcx_cancels;
static void __fastcall pcx_cancel(void *functor) {
    uint64_t panel = 0; uint32_t mode = 0;
    memcpy(&panel, (uint8_t *)functor + 8, 8);
    CHECK(panel == (uintptr_t)wk_display + DD_WALK_OVERLAY);
    memcpy(&mode, (void *)(uintptr_t)(panel + 0xd88), 4); CHECK(mode == 2);
    pcx_cancels++; mode = 3;
    memcpy((void *)(uintptr_t)(panel + 0xd88), &mode, 4); /* Native fade, no result/RNG/inventory mutation. */
}
static void pcx_panel(uint32_t mode,int curio) {
    uint64_t prop=(uintptr_t)wk_props[1],panel=(uintptr_t)wk_display+DD_WALK_OVERLAY;
    memcpy((void *)(uintptr_t)(panel+0xd88),&mode,4);
    memcpy((void *)(uintptr_t)(panel+(curio?0x6a0:0xd70)),&prop,8);
}
static uint8_t __fastcall pcx_open(void *display,void *prop,int32_t option,uint8_t flag) {
    uint32_t mode=0;(void)option;(void)flag;
    CHECK(display==wk_display && prop==wk_props[1]);
    memcpy(&mode,wk_display+DD_WALK_OVERLAY+0xd88,4);
    if(mode) {pcx_asserts++;return 0;} /* Native InteractWithProp assert. */
    pcx_opens++;pcx_panel(2,1);return 1;
}
static void __fastcall pcx_confirm(void *functor) {
    uint64_t panel=0;uint32_t mode=0;
    memcpy(&panel,(uint8_t *)functor+8,8);CHECK(panel==(uintptr_t)wk_display+DD_WALK_OVERLAY);
    memcpy(&mode,(void *)(uintptr_t)(panel+0xd88),4);CHECK(mode==2);
    pcx_confirms++;pcx_panel(3,0); /* Native fade closes then resets event to0. */
}
static void pcx_reset(int role) {
    it_reset(role);wk_view.battle=0;
    {uint64_t selected=bt_party[2];memcpy(wk_raid_lists+0x438,&selected,8);}
    memset(wk_display+DD_WALK_OVERLAY,0,0xd90);
    g_raid_use=pcx_open;g_overlay_use=pcx_confirm;
    g_prop_cancel=g_curio_cancel=pcx_cancel;pcx_cancels=0;
    pcx_opens=pcx_confirms=pcx_asserts=0;
}
static void prop_cancel_checks(void) {
    dd_action cancel; dd_msg request;
    uint64_t panel = (uintptr_t)wk_display + DD_WALK_OVERLAY, functor[2] = {0, panel};
    /* Every player may cancel; guests wait for the ordered native close. */
    for (unsigned initiator = 0; initiator < 4; initiator++) {
        pcx_reset(initiator ? DD_ROLE_GUEST : DD_ROLE_HOST); g_cfg.player = initiator;
        pcx_panel(2, 1); hook_curio_cancel(functor); cancel = last_message.u.town.action; request = last_message;
        CHECK(cancel.kind == DD_ACT_USE && cancel.u.use.door == 6 && !pcx_confirms && !pcx_opens && !pcx_asserts);
        CHECK(pcx_cancels == (initiator ? 0u : 1u));
        if (initiator) {
            pcx_reset(DD_ROLE_HOST); pcx_panel(2, 1);
            CHECK(queue_push(&g_incoming, &request)); dd_runtime_poll();
            cancel = last_message.u.town.action;
            CHECK(pcx_cancels == 1 && g_town.accepted == 1);
            /* A concurrent cancellation after native close has reset the event
             * is rejected without a second native call or prop result. */
            pcx_panel(0, 1); request.u.town.request_id++;
            CHECK(queue_push(&g_incoming, &request)); dd_runtime_poll();
            CHECK(pcx_cancels == 1 && g_town.accepted == 1 && g_town.rejected == 1 && !pcx_confirms);
        }
        for (unsigned player = 1; player < 4; player++) {
            pcx_reset(DD_ROLE_GUEST); g_cfg.player = player; pcx_panel(2, 1);
            wk_push(1, &cancel); dd_runtime_poll();
            CHECK(pcx_cancels == 1 && g_town.applied_seq == 1 && !g_town.needs_resync && !pcx_confirms && !pcx_opens);
            /* A fabricated second committed APPLY with an old revision is invalid. */
            pcx_panel(0, 1); wk_push(2, &cancel); dd_runtime_poll();
            CHECK(pcx_cancels == 1 && g_town.applied_seq == 1 && g_town.needs_resync);
        }
    }
    pcx_reset(DD_ROLE_GUEST); pcx_panel(1, 1);
    wk_push(1, &cancel); dd_runtime_poll();
    CHECK(!pcx_cancels && g_incoming.count == 1);
    pcx_panel(2, 1); dd_runtime_poll();
    CHECK(pcx_cancels == 1 && g_town.applied_seq == 1);
    /* Reopening clears the completion record. A different panel and an unseen
     * closed prop cannot consume this order silently; timeout stops the FIFO. */
    pcx_panel(0, 1);
    {dd_action open = cancel; open.u.use.door = 0;open.u.use.revision=g_prop_revision; CHECK(walk_use_apply(&open)); CHECK(!g_prop_cancelled);}
    pcx_reset(DD_ROLE_GUEST); pcx_panel(2, 1);
    {uint64_t wrong = (uintptr_t)wk_props[0]; memcpy((void *)(uintptr_t)(panel + 0x6a0), &wrong, 8);}
    wk_push(1, &cancel); dd_runtime_poll(); CHECK(!pcx_cancels && g_incoming.count == 1);
    g_walk_use_wait_until = GetTickCount64() - 16000; dd_runtime_poll();
    CHECK(g_town.needs_resync && !pcx_cancels && !g_town.applied_seq);
    pcx_reset(DD_ROLE_GUEST); CHECK(!walk_use_apply(&cancel) && !pcx_cancels);
    /* Obstacle X uses its own native callback and +d70 field, even when a
     * previous curio remains in +6a0. It must never confirm the rubble. */
    pcx_reset(DD_ROLE_HOST); pcx_panel(2, 0);
    {uint64_t stale = (uintptr_t)wk_props[0]; memcpy((void *)(uintptr_t)(panel + 0x6a0), &stale, 8);}
    hook_prop_cancel(functor); cancel = last_message.u.town.action;
    CHECK(pcx_cancels == 1 && cancel.u.use.option == 1 && !pcx_confirms);
    pcx_reset(DD_ROLE_GUEST); pcx_panel(2, 0); wk_push(1, &cancel); dd_runtime_poll();
    CHECK(pcx_cancels == 1 && g_town.applied_seq == 1 && !pcx_confirms);
    g_prop_cancel = g_curio_cancel = NULL; wk_reset(DD_ROLE_HOST);
    puts("Shared curio cancel: any of four players, three guests, delayed readiness, crossed/closed duplicates, reopen, wrong prop/timeout; native close once without interaction verified");
}
static void prop_lifecycle_checks(void) {
    uint64_t panel=(uintptr_t)wk_display+DD_WALK_OVERLAY,functor[2]={0,panel};
    dd_action opened,confirmed,food;
    pcx_reset(DD_ROLE_HOST);
    CHECK(hook_raid_use(wk_display,wk_props[1],0,0));
    opened=last_message.u.town.action;
    CHECK(pcx_opens==1 && !opened.u.use.door && !pcx_asserts);
    CHECK(!hook_raid_use(wk_display,wk_props[1],0,0));
    CHECK(pcx_opens==1 && !pcx_asserts && sent==1);
    /* Concurrent trap/door requests cannot bypass the event guard either. */
    {dd_action crossed=opened;crossed.u.use.door=2;
     CHECK(!walk_use_apply(&crossed) && !pcx_asserts);
     crossed.u.use.door=1;CHECK(!walk_use_apply(&crossed) && !wk_traps);}
    /* Guest already auto-opened that prop on arrival. OPEN must not confirm it. */
    pcx_reset(DD_ROLE_GUEST);pcx_panel(2,0);
    wk_push(1,&opened);dd_runtime_poll();
    CHECK(!pcx_opens && !pcx_confirms && !pcx_asserts && g_town.applied_seq==1);
    wk_push(2,&opened);dd_runtime_poll();
    CHECK(!pcx_confirms && !pcx_asserts && g_town.applied_seq==2);
    /* The same curio closed on arrival: native OPEN exactly once. */
    pcx_reset(DD_ROLE_GUEST);
    wk_push(1,&opened);dd_runtime_poll();wk_push(2,&opened);dd_runtime_poll();
    CHECK(pcx_opens==1 && !pcx_confirms && !pcx_asserts && g_town.applied_seq==2);
    /* Corpse pile, no tool: GUEST may choose the native no-shovel button. */
    pcx_reset(DD_ROLE_GUEST);pcx_panel(2,0);
    {uint64_t hero=bt_party[2];memcpy((void *)(uintptr_t)(panel+0xd78),&hero,8);}
    hook_overlay_use(functor);dd_msg request=last_message;
    CHECK(request.type==DD_MSG_TOWN_REQUEST && request.u.town.action.u.use.door==5 && !pcx_confirms);
    pcx_reset(DD_ROLE_HOST);pcx_panel(2,0);
    CHECK(queue_push(&g_incoming,&request));dd_runtime_poll();
    confirmed=last_message.u.town.action;
    CHECK(pcx_confirms==1 && !pcx_opens && !pcx_asserts && g_town.accepted==1);
    /* A second/crossed confirm while fading is rejected, no second effect. */
    request.u.town.request_id++;
    CHECK(queue_push(&g_incoming,&request));dd_runtime_poll();
    CHECK(pcx_confirms==1 && g_town.rejected==1 && !pcx_asserts);
    pcx_reset(DD_ROLE_GUEST);pcx_panel(1,0);
    wk_push(1,&confirmed);dd_runtime_poll();
    CHECK(!pcx_confirms && g_incoming.count==1 && !g_town.applied_seq);
    pcx_panel(2,0);dd_runtime_poll();
    CHECK(pcx_confirms==1 && !pcx_opens && !pcx_asserts && g_town.applied_seq==1);
    /* Once the native close animation has reset its event, guest food and
     * walking are usable; no fake local completion or queue skip is needed. */
    pcx_panel(0,0);
    hook_item_use((void *)(uintptr_t)0x1234,bt_heroes[2],wk_raid_lists+DD_WALK_RAID_INVENTORY,3,1);
    food=last_message.u.town.action;
    CHECK(food.kind==DD_ACT_ITEM && food.u.item.op==DD_ITEM_OP_USE && !it_uses);
    wk_push(2,&food);dd_runtime_poll();
    CHECK(it_uses==1 && g_town.applied_seq==2 && !g_town.needs_resync);
    wk_target(wk_view.area,wk_view.pos+200);
    {float direction[3]={0};hook_raid_move(wk_display,direction);CHECK(wk_moves==1);}
    /* HOST button also produces a distinct confirm; either side is allowed. */
    pcx_reset(DD_ROLE_HOST);pcx_panel(2,0);
    {uint64_t hero=bt_party[1];memcpy((void *)(uintptr_t)(panel+0xd78),&hero,8);}
    hook_overlay_use(functor);confirmed=last_message.u.town.action;
    CHECK(pcx_confirms==1 && confirmed.u.use.door==5 && !pcx_opens);
    pcx_reset(DD_ROLE_GUEST);pcx_panel(2,0);wk_push(1,&confirmed);dd_runtime_poll();
    CHECK(pcx_confirms==1 && !pcx_opens && !pcx_asserts && g_town.applied_seq==1);
    /* A missing/wrong overlay cannot silently turn into OPEN or allow a later
     * food/fight order through. On bounded timeout the peer receives SYNC_STOP. */
    pcx_reset(DD_ROLE_GUEST);
    wk_push(1,&confirmed);wk_push(2,&food);dd_runtime_poll();
    CHECK(g_incoming.count==2 && !it_uses && !pcx_opens);
    g_walk_use_wait_until=GetTickCount64()-16000;dd_runtime_poll();
    CHECK(g_town.needs_resync && !it_uses && !pcx_opens && !g_town.applied_seq);
    CHECK(g_transport_sync_stop != 0);
    wk_reset(DD_ROLE_HOST);items_reset();
    puts("Prop lifecycle: distinct OPEN/confirm, duplicate Iron Maiden opens, either player's no-shovel choice once, delayed panel, food/walking after native close, timeout peer stop and no native event assertion verified");
}

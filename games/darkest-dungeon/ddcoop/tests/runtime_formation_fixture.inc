static unsigned fm_calls;
static int fm_partial;
static uint8_t __fastcall fm_default(void *raid,uint64_t *out) {
    CHECK(raid==wk_raid_lists);
    for(unsigned i=0;i<4;i++)out[i]=(uintptr_t)bt_heroes[i];
    return !memcmp(out,bt_party,32);
}
/* Like 76f060: asks 614cb0 (hooked, so hook_formation_default) which hero goes to which rank, stops when the party
 * already stands so, else moves every hero there, rank masks and party vector. */
static void __fastcall fm_native(void *display) {
    uint64_t order[4]={0};
    CHECK(display==wk_display);fm_calls++;
    if(fm_partial)return;
    if(hook_formation_default(wk_raid_lists,order))return;
    for(unsigned i=0;i<4;i++)if(order[i]) {
        bt_party[i]=order[i];
        uint32_t mask=1u<<i;memcpy((uint8_t *)(uintptr_t)order[i]+0x1088,&mask,4);
    }
    wk_display[0x4119]=1; /* Real native code keeps this set until presentation finishes. */
}
static void fm_reset(int role) {
    it_reset(role);
    wk_display[0x4118]=wk_display[0x4119]=0;
    g_formation_default=g_formation_default_orig=fm_default;g_formation_native=fm_native;fm_calls=fm_partial=0;
    uint64_t p=bt_party[0];bt_party[0]=bt_party[1];bt_party[1]=p;
}
static void fm_swap(unsigned a, unsigned b) { uint64_t p=bt_party[a];bt_party[a]=bt_party[b];bt_party[b]=p; }
static int fm_ids_are(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    uint32_t ids[4];
    return formation_ids(wk_view.raid,ids) && ids[0]==a && ids[1]==b && ids[2]==c && ids[3]==d;
}
/* FORMATION_SET (protocol37): a drag outside a fight changes one copy only; it is noticed and shared as a whole order. */
static void hand_order_checks(void) {
    dd_msg order,request,reject;
    /* Host drags: the change is ordered for everyone, the host itself needs no native call. */
    fm_reset(DD_ROLE_HOST);
    CHECK(fm_ids_are(12,11,13,14));
    formation_watch(&wk_view);CHECK(!sent && g_formation_known_valid); /* first sight: taken as it stands */
    fm_swap(2,3);formation_watch(&wk_view);
    CHECK(sent==1 && last_message.type==DD_MSG_TOWN_APPLY && last_message.u.town.action.kind==DD_ACT_FORMATION_SET && !fm_calls && g_town.applied_seq==1);
    order=last_message;
    CHECK(order.u.town.action.u.formation.before[2]==13 && order.u.town.action.u.formation.after[2]==14 && order.u.town.action.u.formation.after[3]==13);
    formation_watch(&wk_view);CHECK(sent==1); /* known now: not again */
    /* A guest with the old order makes the same order through the native rerank, nothing else. */
    fm_reset(DD_ROLE_GUEST);
    CHECK(queue_push(&g_incoming,&order));dd_runtime_poll();
    CHECK(fm_calls==1 && g_town.applied_seq==1 && fm_ids_are(12,11,14,13) && !g_town.needs_resync);
    CHECK(!g_formation_override); /* the default is the game's own again */
    { uint64_t out[4];CHECK(hook_formation_default(wk_raid_lists,out) == 0 && out[0]==(uintptr_t)bt_heroes[0]); }

    /* Guest drags: one request, waits (busy) until the host orders it, no second request for the same order. */
    fm_reset(DD_ROLE_GUEST);
    formation_watch(&wk_view);fm_swap(2,3);formation_watch(&wk_view);request=last_message;
    formation_watch(&wk_view);
    CHECK(sent==1 && request.type==DD_MSG_TOWN_REQUEST && request.u.town.action.kind==DD_ACT_FORMATION_SET && g_formation_request && formation_busy());
    float direction[3]={1,0,0};unsigned before=wk_natives;
    hook_raid_move(wk_display,direction);CHECK(!direction[0] && wk_natives==before); /* no walking with an unshared order */
    fm_reset(DD_ROLE_HOST);
    CHECK(queue_push(&g_incoming,&request));dd_runtime_poll();order=last_message;
    CHECK(fm_calls==1 && order.type==DD_MSG_TOWN_APPLY && order.u.town.request_id==request.u.town.request_id && fm_ids_are(12,11,14,13));
    fm_reset(DD_ROLE_GUEST);formation_watch(&wk_view);fm_swap(2,3);formation_watch(&wk_view);
    g_formation_request=order.u.town.request_id;sent=0;
    CHECK(queue_push(&g_incoming,&order));dd_runtime_poll();
    CHECK(!fm_calls && !g_formation_request && g_town.applied_seq==1 && fm_ids_are(12,11,14,13) && !sent);
    formation_watch(&wk_view);CHECK(!sent);

    /* Two players drag at once: each order is applied in turn, everyone ends on the last one. */
    fm_reset(DD_ROLE_HOST);formation_watch(&wk_view);
    dd_msg crossed=request;crossed.u.town.request_id=request.u.town.request_id+7;
    crossed.u.town.action.u.formation.after[0]=11;crossed.u.town.action.u.formation.after[1]=12;
    crossed.u.town.action.u.formation.after[2]=13;crossed.u.town.action.u.formation.after[3]=14;
    CHECK(dd_action_valid(&crossed.u.town.action));
    fm_swap(0,1);formation_watch(&wk_view);CHECK(sent==1 && fm_ids_are(11,12,13,14)); /* the host's own drag */
    CHECK(queue_push(&g_incoming,&request));dd_runtime_poll();
    CHECK(fm_calls==1 && fm_ids_are(12,11,14,13) && g_town.applied_seq==2);
    wk_display[0x4119]=0;formation_watch(&wk_view);CHECK(sent==2); /* applied, so known: nothing new to share */

    /* The host refuses (other members there): the guest's own drag is undone on the guest, once its animation is over. */
    fm_reset(DD_ROLE_GUEST);
    formation_watch(&wk_view);fm_swap(2,3);formation_watch(&wk_view);request=last_message;
    memset(&reject,0,sizeof reject);reject.type=DD_MSG_TOWN_REJECT;reject.u.reject.request_id=request.u.town.request_id;
    wk_display[0x4119]=1;
    CHECK(queue_push(&g_incoming,&reject));dd_runtime_poll();
    CHECK(!g_formation_request && g_formation_revert_due && !fm_calls && fm_ids_are(12,11,14,13));
    wk_display[0x4119]=0;formation_watch(&wk_view);
    CHECK(fm_calls==1 && !g_formation_revert_due && fm_ids_are(12,11,13,14) && !g_town.needs_resync);
    formation_watch(&wk_view);CHECK(fm_calls==1 && sent==1);

    /* In a fight and while loading every order is the game's own: taken over, never shared. Other members likewise. */
    fm_reset(DD_ROLE_HOST);formation_watch(&wk_view);
    wk_view.battle=12;fm_swap(0,3);formation_watch(&wk_view);CHECK(!sent);
    wk_view.battle=0;formation_watch(&wk_view);CHECK(!sent); /* settling after the fight */
    formation_watch(&wk_view);CHECK(!sent);
    fm_swap(1,2);wk_view.state=6;formation_watch(&wk_view);CHECK(!sent);
    wk_view.state=2;formation_watch(&wk_view);CHECK(!sent);
    { uint32_t other=77;memcpy(bt_heroes[1]+0x137c,&other,4); }
    formation_watch(&wk_view);
    CHECK(!sent && (g_formation_known[0]==77 || g_formation_known[1]==77 || g_formation_known[2]==77 || g_formation_known[3]==77));
    { uint32_t back=12;memcpy(bt_heroes[1]+0x137c,&back,4); }

    /* Hook: a wanted order that is not the live party moves nothing; no wanted order is the game's own default. */
    fm_reset(DD_ROLE_HOST);
    { uint32_t want[4]={11,12,13,99};uint64_t out[4]={1,1,1,1};
      g_formation_override=want;CHECK(hook_formation_default(wk_raid_lists,out)==1 && !out[0] && !out[3]);g_formation_override=NULL; }
    /* Applying a hand order that the native rerank does not produce stops everyone. */
    fm_reset(DD_ROLE_GUEST);fm_partial=1;
    CHECK(queue_push(&g_incoming,&order));dd_runtime_poll();
    CHECK(fm_calls==1 && g_town.needs_resync && g_transport_sync_stop != 0);
    puts("Hand-made party order: host/guest drags noticed and shared once, guest waits and blocks walking, kept or natively rerun on every copy, crossed drags converge, refused order undone locally after animation, fights/loading/other members adopted, foreign wanted IDs move nothing, partial native result stops");
}
/* A manual drag immediately after combat must not become a silent local baseline.
 * Native animation/blocked states defer the change until ready, retaining the last shared order. */
static void formation_after_combat_checks(void) {
    for (unsigned player=0;player<4;player++) {
        for (unsigned delayed=0;delayed<3;delayed++) {
            fm_reset(player?DD_ROLE_GUEST:DD_ROLE_HOST);
            g_cfg.player=(int)player;
            if(player)g_town.next_request=(player<<24)|1u;
            formation_watch(&wk_view);
            wk_view.battle=12;fm_swap(0,1);formation_watch(&wk_view);
            CHECK(!sent); /* Engine rank changes in combat remain native. */
            wk_view.battle=0;
            if(delayed==1)wk_display[0x4119]=1;
            if(delayed==2)wk_display[0x4118]=1;
            fm_swap(2,3);formation_watch(&wk_view);
            if(delayed) {
                CHECK(!sent && g_formation_known[2]==13 && g_formation_known[3]==14);
                wk_display[0x4118]=wk_display[0x4119]=0;
                formation_watch(&wk_view);
            }
            CHECK(sent==1 && last_message.u.town.action.kind==DD_ACT_FORMATION_SET);
            CHECK(last_message.u.town.action.u.formation.before[2]==13 && last_message.u.town.action.u.formation.after[2]==14);
            CHECK(player ? (g_formation_request!=0 && formation_busy()) : (g_town.applied_seq==1));
            formation_watch(&wk_view);CHECK(sent==1); /* No duplicate. */
            if(player) {
                dd_msg order=last_message;
                order.type=DD_MSG_TOWN_APPLY;order.seq=1;
                CHECK(queue_push(&g_incoming,&order));dd_runtime_poll();
                CHECK(!g_formation_request && g_town.applied_seq==1 && !g_town.needs_resync);
            }
        }
    }
    puts("Post-combat formation: all4 initiators, immediate drag, animation/blocked deferral, persistent before/after, once-only order and guest acknowledgement verified");
}
static void formation_checks(void) {
    formation_after_combat_checks();
    dd_msg order,request,stop;uint32_t ids[4];
    /* Host button/shortcut: same native entry, one mutation then ordered apply. */
    fm_reset(DD_ROLE_HOST);
    hook_formation_reset(wk_display);
    CHECK(fm_calls==1 && sent==1 && g_town.applied_seq==1 && last_message.u.town.action.kind==DD_ACT_FORMATION_RESET);
    order=last_message;
    CHECK(formation_ids(wk_view.raid,ids) && ids[0]==11 && ids[1]==12 && wk_display[0x4119]);
    hook_formation_reset(wk_display);CHECK(fm_calls==1 && sent==1);
    float direction[3]={1,0,0};unsigned before=wk_natives;
    hook_raid_move(wk_display,direction);
    CHECK(!direction[0] && wk_natives==before); /* Walking waits for the reorder animation. */
    wk_display[0x4119]=0;
    hook_formation_reset(wk_display);CHECK(fm_calls==1 && sent==1); /* Already default. */
    fm_reset(DD_ROLE_GUEST);
    CHECK(queue_push(&g_incoming,&order));dd_runtime_poll();
    CHECK(fm_calls==1 && g_town.applied_seq==1 && formation_ids(wk_view.raid,ids) && ids[0]==11 && ids[1]==12);

    /* Guest initiated: no local change before approval, no duplicate request. */
    fm_reset(DD_ROLE_GUEST);
    hook_formation_reset(wk_display);request=last_message;
    hook_formation_reset(wk_display);
    CHECK(!fm_calls && sent==1 && g_formation_request && request.type==DD_MSG_TOWN_REQUEST && formation_ids(wk_view.raid,ids) && ids[0]==12);
    fm_reset(DD_ROLE_HOST);
    CHECK(queue_push(&g_incoming,&request));dd_runtime_poll();order=last_message;
    CHECK(fm_calls==1 && order.type==DD_MSG_TOWN_APPLY && order.u.town.request_id==request.u.town.request_id);
    fm_reset(DD_ROLE_GUEST);
    g_formation_request=request.u.town.request_id;
    CHECK(queue_push(&g_incoming,&order));dd_runtime_poll();
    CHECK(fm_calls==1 && !g_formation_request && g_town.applied_seq==1);

    /* Both click: first order wins. A stale second request cannot reorder twice. */
    fm_reset(DD_ROLE_HOST);hook_formation_reset(wk_display);
    CHECK(queue_push(&g_incoming,&request));dd_runtime_poll();
    CHECK(g_incoming.count==1 && fm_calls==1);
    wk_display[0x4119]=0;dd_runtime_poll();
    CHECK(!g_incoming.count && fm_calls==1 && last_message.type==DD_MSG_TOWN_REJECT && !g_town.needs_resync);

    /* Delayed client presentation and combat end: wait, then apply once. */
    fm_reset(DD_ROLE_GUEST);wk_display[0x4119]=1;
    CHECK(queue_push(&g_incoming,&order));dd_runtime_poll();CHECK(!fm_calls && g_incoming.count==1);
    wk_display[0x4119]=0;wk_view.battle=34;dd_runtime_poll();CHECK(!fm_calls && g_incoming.count==1);
    wk_view.battle=0;dd_runtime_poll();CHECK(fm_calls==1 && g_town.applied_seq==1);

    /* No mutation for missing member/default, stale before-order or wrong area. */
    fm_reset(DD_ROLE_HOST);
    uint32_t wrong=99;memcpy(bt_heroes[0]+0x137c,&wrong,4);
    CHECK(!runtime_apply(NULL,&order.u.town.action) && !fm_calls);
    fm_reset(DD_ROLE_GUEST);wk_view.area++;
    CHECK(queue_push(&g_incoming,&order));dd_runtime_poll();CHECK(g_incoming.count==1 && !fm_calls);
    g_formation_wait=1;dd_runtime_poll();
    CHECK(g_town.needs_resync && !fm_calls && !g_town.applied_seq && g_transport_sync_stop != 0);

    /* Partial native result is never broadcast/ACKed; neither peer may continue. */
    fm_reset(DD_ROLE_HOST);fm_partial=1;
    hook_formation_reset(wk_display);
    CHECK(fm_calls==1 && !sent && !g_town.applied_seq && g_town.needs_resync && g_transport_sync_stop != 0);
    fm_reset(DD_ROLE_GUEST);fm_partial=1;
    CHECK(queue_push(&g_incoming,&order));dd_runtime_poll();
    CHECK(fm_calls==1 && !g_town.applied_seq && g_town.needs_resync);

    /* Equal rights, but only outside combat/loading/doors and in the chosen slot. */
    for(int role=DD_ROLE_HOST;role<=DD_ROLE_GUEST;role++) {
        fm_reset(role);wk_view.battle=29;hook_formation_reset(wk_display);CHECK(!fm_calls && !sent);
        wk_view.battle=0;g_walk_door_until=GetTickCount64()+1000;hook_formation_reset(wk_display);CHECK(!fm_calls && !sent);
        g_walk_door_until=0;wk_view.state=6;hook_formation_reset(wk_display);CHECK(!fm_calls && !sent);
    }
    fm_reset(DD_ROLE_OFF);hook_formation_reset(wk_display);CHECK(fm_calls==1 && !sent);
    fm_reset(DD_ROLE_HOST);g_campaign_active=0;hook_formation_reset(wk_display);CHECK(fm_calls==1 && !sent);
    fm_reset(DD_ROLE_HOST);g_session_ready=0;hook_formation_reset(wk_display);CHECK(!fm_calls && !sent);
    fm_reset(DD_ROLE_HOST);g_formation_default=NULL;hook_formation_reset(wk_display);CHECK(!fm_calls && !sent);
    hand_order_checks();
    g_formation_native=NULL;g_formation_default_orig=NULL;wk_display[0x4119]=0;
    puts("Formation reset: HOST/GUEST intent before mutation, exact persistent IDs/native default, one ordered native call, crossed/duplicate clicks, delayed animation/combat, area/slot/session guards, native partial failure and pre-travel divergence stop verified");
}

/* Real hooks, validation, ordered apply and FIFO; only native UI/animation is
 * modeled. This never opens a game or accesses a real campaign. */
static uint8_t fn_manager[0x60], fn_entries[2][0x340], fn_params[0x40];
static uintptr_t fn_slots[4];
static unsigned fn_enqueued, fn_shown, fn_acks;
static int fn_click;
static void __fastcall fn_enqueue(void *manager, const char *name, void *params) {
    (void)params;
    uint64_t slots = (uintptr_t)fn_slots, tail = ++fn_enqueued, mask = 3, count = tail;
    CHECK(tail <= 2);
    strcpy((char *)fn_entries[tail-1], name);
    fn_slots[(tail-1)&mask] = (uintptr_t)fn_entries[tail-1];
    memcpy((uint8_t *)manager+8,&slots,8); memcpy((uint8_t *)manager+0x10,&tail,8);
    memcpy((uint8_t *)manager+0x18,&mask,8); memcpy((uint8_t *)manager+0x20,&count,8);
}
static void __fastcall fn_ack(void *capture) {
    uintptr_t manager=0; uint32_t removals=0;
    memcpy(&manager,(uint8_t *)capture+8,8);
    memcpy(&removals,(void *)(manager+0x30),4); ++removals;
    memcpy((void *)(manager+0x30),&removals,4); ++fn_acks;
}
static void __fastcall fn_show(void *manager, void *entry) {
    (void)entry; ++fn_shown;
    if(fn_click) { uint64_t capture[10]={0}; capture[1]=(uintptr_t)manager; hook_confirm_ack(capture); }
}
static void fn_reset(int role) {
    it_reset(role); it_loot(2);
    memset(fn_manager,0,sizeof fn_manager); memset(fn_entries,0,sizeof fn_entries);
    memset(fn_slots,0,sizeof fn_slots); memset(fn_params,0,sizeof fn_params);
    fn_enqueued=fn_shown=fn_acks=0; fn_click=0;
    g_confirm_enqueue=fn_enqueue;g_confirm_ack=fn_ack;g_confirm_show_entry=fn_show;
    g_full_notice_caller=it_caller_yes;
    g_town.applied_seq=7; g_town.next_apply_seq=8; full_notice_loot(7,wk_view.area);
}
static void fn_open(void) { hook_confirm_enqueue(fn_manager,g_full_name,fn_params); }
static void fn_render(unsigned index) {
    /* The real manager removes entries using +30 outside ShowEntry. Model the
     * beginning of each new frame so a double native increment is observable. */
    memset(fn_manager+0x30,0,4);
    hook_confirm_show_entry(fn_manager,fn_entries[index]);
}
static void full_notice_checks(void) {
    dd_action close={0};close.kind=DD_ACT_ITEM;close.u.item.op=DD_ITEM_OP_FULL_NOTICE;
    close.u.item.hero=255;close.u.item.id=7;close.u.item.area=100;
    CHECK(dd_action_valid(&close));
    CHECK(full_notice_caller(0x7ebcd8) && full_notice_caller(0x717427) && !full_notice_caller(0x717426));
    for(unsigned initiator=0;initiator<4;++initiator) {
        fn_reset(initiator?DD_ROLE_GUEST:DD_ROLE_HOST);g_cfg.player=(int)initiator;
        fn_open();fn_click=1;fn_render(0);
        CHECK(sent==1 && last_message.u.town.action.u.item.id==7);
        if(initiator) {
            CHECK(!fn_acks && last_message.type==DD_MSG_TOWN_REQUEST);
            fn_render(0);CHECK(sent==1 && !fn_acks); /* no duplicate request */
            dd_msg request=last_message;
            fn_reset(DD_ROLE_HOST);dd_town_host_request(&g_town,(int)initiator,&request);
            CHECK(sent==1 && last_message.type==DD_MSG_TOWN_APPLY && g_town.applied_seq==8 && !fn_acks);
        } else CHECK(fn_acks==1 && last_message.type==DD_MSG_TOWN_APPLY);
        dd_msg ordered=last_message;
        for(unsigned receiver=0;receiver<4;++receiver) {
            if(receiver==initiator)continue;
            fn_reset(receiver?DD_ROLE_GUEST:DD_ROLE_HOST);g_cfg.player=(int)receiver;
            fn_open();
            if(receiver) { queue_push(&g_incoming,&ordered);dd_runtime_poll(); }
            else CHECK(full_notice_apply(&ordered.u.town.action));
            CHECK(!fn_acks);fn_render(0);CHECK(fn_acks==1);
            fn_render(0);CHECK(fn_acks==1 && !g_town.needs_resync);
        }
    }
    /* The other copy may not have rendered/enqueued its information yet. */
    fn_reset(DD_ROLE_GUEST);wk_push(8,&close);dd_runtime_poll();
    CHECK(g_town.applied_seq==8 && !fn_acks && !g_town.needs_resync);
    fn_open();fn_render(0);CHECK(fn_acks==1);
    /* A new loot attempt in the same room has a different identity. */
    full_notice_loot(9,100);fn_open();fn_render(1);CHECK(fn_acks==1);
    /* Another named confirmation, or any supplied gameplay callback, remains
     * local and intact. Even an identically worded later popup is not enough. */
    fn_reset(DD_ROLE_HOST);hook_confirm_enqueue(fn_manager,"unrelated_confirmation",fn_params);
    fn_click=1;fn_render(0);CHECK(fn_acks==1 && !sent);
    fn_reset(DD_ROLE_HOST);uintptr_t callback=1;memcpy(fn_params+0x38,&callback,8);
    fn_open();fn_click=1;fn_render(0);CHECK(fn_acks==1 && !sent);
    fn_reset(DD_ROLE_HOST);g_full_notice_caller=it_caller_no;fn_open();fn_click=1;fn_render(0);
    CHECK(fn_acks==1 && !sent);
    fn_reset(DD_ROLE_HOST);fn_open();wk_view.area=101;fn_click=1;fn_render(0);CHECK(fn_acks==1 && !sent);
    fn_reset(DD_ROLE_HOST);fn_open();g_town.needs_resync=1;fn_click=1;fn_render(0);
    CHECK(fn_acks==1 && !sent); /* informational local dismissal remains possible after STOP */
    fn_reset(DD_ROLE_HOST);dd_action bad=close;bad.u.item.id=6;
    CHECK(!full_notice_apply(&bad) && !fn_acks);
    bad=close;bad.u.item.flag=1;CHECK(!dd_action_valid(&bad));
    bad=close;bad.u.item.id=0;CHECK(!dd_action_valid(&bad));
    full_notice_reset();CHECK(!full_notice_token(7,100));
    g_confirm_enqueue=NULL;g_confirm_ack=NULL;g_confirm_show_entry=NULL;
    g_full_notice_caller=full_notice_caller;
    puts("Inventory-full dismissal: all4 initiators/3 receivers, ordered/deferred native close once, absent/delayed notice, duplicate, new loot, unrelated/callback/caller/area/STOP/reset guards verified");
}
static void __fastcall fn_takeall(void *capture) {
    (void)capture; ++it_alls;
    wk_display[DD_WALK_LOOT_OVERLAY+0x218]=1;
}
static void loot_pending_checks(void) {
    for(unsigned player=0;player<4;++player) {
        it_reset(player?DD_ROLE_GUEST:DD_ROLE_HOST);it_loot(2);g_cfg.player=(int)player;
        wk_display[DD_WALK_LOOT_OVERLAY+0x218]=1;
        hook_loot_take_all(NULL);hook_loot_close(NULL);
        CHECK(!sent && !it_alls && !it_closes);
    }
    it_reset(DD_ROLE_HOST);it_loot(2);g_loot_take_all=fn_takeall;
    hook_loot_take_all(NULL);hook_loot_take_all(NULL);hook_loot_close(NULL);
    CHECK(sent==1 && it_alls==1 && !it_closes && g_full_loot_seq==1);
    dd_action all=last_message.u.town.action;
    it_reset(DD_ROLE_GUEST);it_loot(2);wk_push(1,&all);dd_runtime_poll();
    CHECK(g_full_loot_seq==1 && wk_display[DD_WALK_LOOT_OVERLAY+0x218]);
    dd_action close=all;close.u.item.op=DD_ITEM_OP_LOOT_CLOSE;wk_push(2,&close);dd_runtime_poll();
    CHECK(g_incoming.count==1 && g_town.applied_seq==1 && !wk_display[DD_WALK_LOOT_OVERLAY+0x219]);
    wk_display[DD_WALK_LOOT_OVERLAY+0x218]=0;dd_runtime_poll();
    CHECK(!g_incoming.count && g_town.applied_seq==2 && wk_display[DD_WALK_LOOT_OVERLAY+0x219]);
    puts("Pending loot buttons: all4 roles drop a second native-frame click; FIFO retains next command until prior native flag consumed");
}
static void battle_animation_checks(void) {
    dd_action choice={0};
    bt_reset(DD_ROLE_HOST);bt_at(2,1,29);hook_battle_skill_button(NULL);choice=last_message.u.town.action;
    CHECK(choice.kind==DD_ACT_BATTLE);
    for(unsigned role=DD_ROLE_HOST;role<=DD_ROLE_GUEST;++role) {
        for(unsigned phase=25;phase<=35;++phase) {
            if(phase==29 || phase==30)continue;
            bt_reset(role);g_hero_owner_mask=0x55;
            bt_at(2,1,phase);
            dd_msg m={0};m.type=role==DD_ROLE_HOST?DD_MSG_TOWN_REQUEST:DD_MSG_TOWN_APPLY;
            m.seq=1;m.u.town.request_id=0x01000001;m.u.town.action=choice;
            queue_push(&g_incoming,&m);dd_runtime_poll();
            CHECK(g_incoming.count==1 && !bt_picks && !g_town.applied_seq && !g_town.needs_resync);
            bt_at(2,1,29);dd_runtime_poll();
            CHECK(!g_incoming.count && bt_picks==1 && g_town.applied_seq==1 && !g_town.needs_resync);
        }
    }
    bt_reset(DD_ROLE_GUEST);bt_at(2,1,33);wk_push(1,&choice);dd_runtime_poll();
    g_battle_wait_until=1;dd_runtime_poll();CHECK(!bt_picks && !g_town.applied_seq && g_town.needs_resync);
    bt_reset(DD_ROLE_GUEST);bt_at(3,1,33);wk_push(1,&choice);dd_runtime_poll();
    CHECK(!bt_picks && !g_town.applied_seq && g_town.needs_resync); /* truly later round still stale */
    bt_unwire();
    puts("Battle animation timing: host and guest wait for same-turn input through phases25..35, resume once, bounded timeout and later-round refusal verified");
}

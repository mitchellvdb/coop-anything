/* Actual hooks/authority/codec/FIFO; only per-copy game roots/native effects
 * are modeled. The host never receives a local user click for guest intents.
 * Native hide/setup transitions are modeled explicitly, not extra user input. */
static dd_msg af_wire(const dd_msg *m) {
    uint8_t bytes[DD_PROTO_HEADER+DD_PROTO_MAX_BODY]; size_t used=0;
    dd_msg decoded={0};size_t n=dd_encode(m,bytes,sizeof bytes);
    CHECK(n && dd_decode(bytes,n,&decoded,&used)==DD_DECODE_OK && used==n);
    return decoded;
}
static void af_player(unsigned player) {
    if (player) {g_cfg.player=(int)player;g_town.next_request=(player<<24)|1u;}
}
static void af_prepare_all(void) {
    uint64_t capture[2]={0,(uintptr_t)pp_town};
    for(unsigned initiator=0;initiator<4;initiator++) {
        pp_reset(initiator?DD_ROLE_GUEST:DD_ROLE_HOST,1);af_player(initiator);
        hook_prepare_open(capture);
        CHECK(sent==1 && pp_opens==(initiator?0u:1u));
        dd_msg m=af_wire(&last_message);
        if(initiator) {
            CHECK(m.type==DD_MSG_TOWN_REQUEST && (m.u.town.request_id>>24)==initiator);
            pp_reset(DD_ROLE_HOST,0); /* host idle, different mission selected */
            CHECK(queue_push(&g_incoming,&m));dd_runtime_poll();
            CHECK(pp_opens==1 && pp_fills==1 && pp_stock_fills==1 && g_town.accepted==1 && !alerts);
            m=af_wire(&last_message);
        }
        CHECK(m.type==DD_MSG_TOWN_APPLY && m.u.town.action.u.prepare.index==1 && m.u.town.action.u.prepare.key==8);
        for(unsigned receiver=1;receiver<=3;receiver++) {
            pp_reset(DD_ROLE_GUEST,0);af_player(receiver);
            CHECK(queue_push(&g_incoming,&m));dd_runtime_poll();
            CHECK(pp_opens==1 && pp_fills==1 && pp_stock_fills==1 && g_town.applied_seq==1 && !alerts && !g_town.needs_resync);
            CHECK(g_supply_quest_known && g_supply_quest_index==1 && g_supply_quest_key==8);
        }
    }
}
static void af_supply_reset(int role,unsigned player,int with_food) {
    sw_reset(role);af_player(player);
    if(with_food) {
        dd_action buy=sw_order(DD_SUPPLY_BUY,0,75);
        CHECK(runtime_apply(NULL,&buy) && sw_buys==1 && *sw_gold()==925);
        sw_buys=0;
    }
}
static void af_supplies_all(void) {
    for(unsigned op=DD_SUPPLY_BUY;op<=DD_SUPPLY_CANCEL;op++)for(unsigned initiator=0;initiator<4;initiator++) {
        af_supply_reset(initiator?DD_ROLE_GUEST:DD_ROLE_HOST,initiator,op!=DD_SUPPLY_BUY);
        if(op==DD_SUPPLY_BUY)sw_click_buy(0);
        else if(op==DD_SUPPLY_RETURN)sw_click_return(1); /* food after free wood */
        else {hook_supply_back(NULL);sw_show(0);provisions_poll();}
        CHECK(sent==1);
        dd_msg m=af_wire(&last_message);
        if(initiator) {
            CHECK(m.type==DD_MSG_TOWN_REQUEST && (m.u.town.request_id>>24)==initiator);
            af_supply_reset(DD_ROLE_HOST,0,op!=DD_SUPPLY_BUY);
            if(op!=DD_SUPPLY_CANCEL)sw_show(0); /* closed host shop; no host click */
            CHECK(queue_push(&g_incoming,&m));dd_runtime_poll();
            if(op==DD_SUPPLY_CANCEL) {CHECK(sw_backs==1 && g_incoming.count==1);sw_show(0);dd_runtime_poll();}
            CHECK(g_town.accepted==1 && !g_town.needs_resync && !alerts);
            m=af_wire(&last_message);
        }
        CHECK(m.type==DD_MSG_TOWN_APPLY && m.u.town.action.u.supplies.op==op);
        uint8_t expected[32];CHECK(dd_supplies_digest(&sw,expected));
        for(unsigned receiver=1;receiver<=3;receiver++) {
            af_supply_reset(DD_ROLE_GUEST,receiver,op!=DD_SUPPLY_BUY);
            if(op!=DD_SUPPLY_CANCEL)sw_show(0);
            CHECK(queue_push(&g_incoming,&m));dd_runtime_poll();
            if(op==DD_SUPPLY_CANCEL) {CHECK(sw_backs==1 && g_incoming.count==1);sw_show(0);dd_runtime_poll();}
            uint8_t actual[32];CHECK(dd_supplies_digest(&sw,actual) && !memcmp(actual,expected,32));
            CHECK(g_town.applied_seq==1 && !g_town.needs_resync && !alerts);
        }
    }
}
static void af_embark_all(void) {
    for(unsigned initiator=0;initiator<4;initiator++) {
        em_reset(initiator?DD_ROLE_GUEST:DD_ROLE_HOST);af_player(initiator);
        hook_town_depart();
        CHECK(sent==1 && dp_departs==(initiator?0u:1u));
        dd_msg ack=af_wire(&last_message);
        if(initiator) {
            CHECK(ack.type==DD_MSG_TOWN_REQUEST && (ack.u.town.request_id>>24)==initiator);
            em_reset(DD_ROLE_HOST);CHECK(g_supply_bg && !dp_departs);
            CHECK(queue_push(&g_incoming,&ack));dd_runtime_poll();
            CHECK(dp_departs==1 && dp_party_calls==1 && !g_town.needs_resync);
            ack=af_wire(&last_message);
        }
        CHECK(ack.type==DD_MSG_TOWN_APPLY && ack.seq==1 && ack.u.town.action.kind==DD_ACT_EMBARK);
        hook_raid_start(dp_app);dd_msg leave=af_wire(&last_message);
        CHECK(leave.type==DD_MSG_TOWN_APPLY && leave.seq==2 && leave.u.town.action.kind==DD_ACT_DEPARTURE && dp_maps==1);
        uint32_t map=dp_map_seed[0],rules=dp_rules_seed;
        for(unsigned receiver=1;receiver<=3;receiver++) {
            em_reset(DD_ROLE_GUEST);af_player(receiver);
            CHECK(queue_push(&g_incoming,&ack) && queue_push(&g_incoming,&leave));dd_runtime_poll();
            CHECK(dp_departs==1 && dp_party_calls==1 && !dp_maps && g_town.applied_seq==2 && !g_town.needs_resync);
            hook_raid_start(dp_app);
            CHECK(dp_maps==1 && dp_map_seed[0]==map && dp_rules_seed==rules && !dp_native_calls && !alerts);
        }
    }
}
static void af_delayed_departure(void) {
    em_reset(DD_ROLE_HOST);hook_town_depart();hook_raid_start(dp_app);
    dd_msg leave=af_wire(&last_message);leave.seq=1;
    for(unsigned state=0;state<2;state++) {
        dp_reset(DD_ROLE_GUEST);
        if(state==0)dp_view_fail=1;else dp_in_raid=1;
        CHECK(queue_push(&g_incoming,&leave));dd_runtime_poll();
        CHECK(g_incoming.count==1 && !g_town.applied_seq && !g_town.needs_resync && !alerts && !g_departure_seed && !dp_departs);
        dp_view_fail=dp_in_raid=0;dd_runtime_poll();
        CHECK(!g_incoming.count && g_town.applied_seq==1 && dp_departs==1 && !g_town.needs_resync);
    }
    for(unsigned state=0;state<2;state++) {
        dp_reset(DD_ROLE_GUEST);
        if(state==0)dp_view_fail=1;else dp_in_raid=1;
        CHECK(queue_push(&g_incoming,&leave));dd_runtime_poll();
        g_departure_follow_until=1;dd_runtime_poll();
        CHECK(!g_town.applied_seq && g_town.needs_resync && g_sync_stop_sent && !dp_departs && !g_departure_seed && !g_departure_have_host);
    }
    /* Invalid sequence must be rejected immediately, never held as readiness. */
    dp_reset(DD_ROLE_GUEST);dp_view_fail=1;leave.seq=2;
    CHECK(queue_push(&g_incoming,&leave));dd_runtime_poll();
    CHECK(!g_incoming.count && g_town.needs_resync && !g_departure_follow_until);
}
static void guest_afk_transactions(void) {
    af_prepare_all();af_supplies_all();af_embark_all();af_delayed_departure();
    departure_reset();provisions_reset();g_prepare_read=prepare_read;g_depart_view=depart_read;
    g_depart_party=depart_party;g_town_depart=NULL;g_rules_reseed=NULL;g_rules_object=NULL;
    g_rand_s_native=NULL;g_raid_start=NULL;g_raid_map=NULL;g_prepare_open=NULL;
    g_supply_open=supply_open;g_supply_fill[0]=g_supply_fill[1]=NULL;
    puts("Guest autonomy: all four initiators open/provision/buy/return/refund/embark through actual FIFO/codec; idle host needs no click, three receivers agree on mission/items/funds/map/Rules; late town follows automatically");
}

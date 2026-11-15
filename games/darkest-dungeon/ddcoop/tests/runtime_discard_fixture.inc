static uint8_t discard_items[2][2][DD_ITEM_SIZE];
static unsigned discard_calls;static int discard_partial;
static uint32_t discard_test_quest(void) {return 1234567;}
static uint64_t __fastcall discard_native(void *capture,void *b,void *c,void *d) {
    (void)b;(void)c;(void)d;
    uint64_t inv=0,p=0,take=0;uint32_t slot=0,amount=0,count=0;
    memcpy(&inv,capture,8);memcpy(&slot,(uint8_t *)capture+8,4);memcpy(&take,(uint8_t *)capture+0x10,8);
    memcpy(&p,(uint8_t *)(uintptr_t)inv+8,8);memcpy(&amount,(void *)(uintptr_t)take,4);
    CHECK(slot<2 && inv==bag_base(wk_view.raid,0));
    uint8_t *item=(uint8_t *)(uintptr_t)p+slot*DD_ITEM_SIZE;
    memcpy(&count,item,4);CHECK(count>=amount);discard_calls++;
    if(!discard_partial){count-=amount;memcpy(item,&count,4);}
    return 0;
}
static void discard_fixture_reset(int role) {
    wk_reset(role);discard_reset();g_walk_view=wk_read;g_bag_throw_b=discard_native;
    g_discard_quest_type=discard_test_quest;discard_calls=0;discard_partial=0;
    memset(discard_items,0,sizeof discard_items);
    for(unsigned inv=0;inv<2;inv++) {
        putptr((uint8_t *)(uintptr_t)bag_base(wk_view.raid,(uint8_t)inv)+8,(uintptr_t)discard_items[inv]);
        putptr((uint8_t *)(uintptr_t)bag_base(wk_view.raid,(uint8_t)inv)+0x10,(uintptr_t)(discard_items[inv]+2));
    }
    putint(discard_items[0][0],2);putint(discard_items[0][0]+0x44,0x6a930e79);putint(discard_items[0][0]+0x88,0x3d7485cc);
    putint(discard_items[0][1],2);putint(discard_items[0][1]+0x44,0x6a930e79);putint(discard_items[0][1]+0x88,0x3d7485cc);
}
static void discard_transactions(void) {
    bag_call_fn saved=g_bag_throw_b;uint32_t (*saved_quest)(void)=g_discard_quest_type;
    int (*saved_view)(walk_view *)=g_walk_view;
    dd_action order={0};uint32_t take=2;
    uint64_t capture[3]={bag_base((uintptr_t)wk_raid_lists,0),0,(uintptr_t)&take};
    for(unsigned player=0;player<4;player++) {
        discard_fixture_reset(player?DD_ROLE_GUEST:DD_ROLE_HOST);g_cfg.player=player;
        hook_bag_throw_b(capture,NULL,NULL,NULL);
        CHECK(sent==1 && last_message.u.town.action.u.item.op==DD_ITEM_OP_DISCARD);
        order=last_message.u.town.action;CHECK(dd_action_valid(&order));
        if(player) {
            CHECK(!discard_calls && discard_items[0][0][0]==2);
            hook_bag_throw_b(capture,NULL,NULL,NULL);CHECK(sent==1);
            discard_fixture_reset(DD_ROLE_HOST);
            dd_msg m={0};m.type=DD_MSG_TOWN_REQUEST;m.u.town.request_id=(player<<24)|1;m.u.town.action=order;
            CHECK(queue_push(&g_incoming,&m));dd_runtime_poll();
        }
        CHECK(discard_calls==1 && !discard_items[0][0][0] && discard_items[0][1][0]==2);
        dd_msg crossed={0};crossed.type=DD_MSG_TOWN_REQUEST;crossed.u.town.request_id=0x02000002;crossed.u.town.action=order;
        CHECK(queue_push(&g_incoming,&crossed));dd_runtime_poll();CHECK(discard_calls==1 && g_town.rejected==1 && !g_town.needs_resync);
        for(unsigned peer=1;peer<4;peer++) {
            discard_fixture_reset(DD_ROLE_GUEST);g_cfg.player=peer;
            wk_push(1,&order);dd_runtime_poll();
            CHECK(g_town.applied_seq==1 && discard_calls==1 && !discard_items[0][0][0] && discard_items[0][1][0]==2);
        }
    }
    discard_fixture_reset(DD_ROLE_GUEST);order.u.item.amount=1;
    wk_push(1,&order);dd_runtime_poll();CHECK(discard_calls==1 && discard_items[0][0][0]==1);
    for(unsigned scenario=0;scenario<5;scenario++) {
        discard_fixture_reset(DD_ROLE_GUEST);dd_action bad=order;
        if(scenario==0)bad.u.item.id++;
        if(scenario==1)bad.u.item.round++;
        if(scenario==2)bad.u.item.area++;
        if(scenario==3)bad.u.item.type=discard_test_quest();
        if(scenario==4)bad.u.item.slot=2;
        CHECK(!discard_apply(&bad) && !discard_calls && discard_items[0][0][0]==2);
    }
    discard_fixture_reset(DD_ROLE_GUEST);discard_partial=1;wk_push(1,&order);dd_runtime_poll();
    CHECK(discard_calls==1 && g_town.needs_resync && !g_town.applied_seq && g_sync_stop_sent);
    discard_fixture_reset(DD_ROLE_GUEST);wk_view.state=7;wk_push(1,&order);dd_runtime_poll();
    CHECK(!discard_calls && g_incoming.count==1);wk_view.state=2;dd_runtime_poll();CHECK(discard_calls==1 && g_town.applied_seq==1);
    g_bag_throw_b=saved;g_discard_quest_type=saved_quest;g_walk_view=saved_view;discard_reset();
    puts("Confirmed Shift-discard: all4 initiators/3 receivers, guest waits, duplicate cookie stacks conserved, crossed/stale/quest/type/count/slot guards, partial-native STOP and loading FIFO verified");
}

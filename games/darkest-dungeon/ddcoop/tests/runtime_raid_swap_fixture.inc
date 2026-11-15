static uint8_t rs_source[0x100],rs_target[0x100],rs_drag[0x610],rs_bar[0x30],rs_ui_inv[0x30],rs_ui_item[2][DD_ITEM_SIZE];
static uint64_t rs_widgets[2];
static void rs_reset(int role,unsigned direction,unsigned variant) {
    hm_raid_reset(role);bag_state_reset();g_walk_view=hm_raid_view;hm_loot_window(2);g_discard_quest_type=discard_test_quest;
    memset(rs_source,0,sizeof rs_source);memset(rs_target,0,sizeof rs_target);memset(rs_drag,0,sizeof rs_drag);
    memset(rs_bar,0,sizeof rs_bar);memset(rs_ui_inv,0,sizeof rs_ui_inv);memset(rs_ui_item,0,sizeof rs_ui_item);
    for(unsigned i=0;i<4;i++) {
        putint(hm_raid_bag[i],8);putint(hm_raid_bag[i]+0x44,0x6a930e79);putint(hm_raid_bag[i]+0x88,300+i);
    }
    uint8_t *bag=hm_raid_bag[1],*loot=hm_raid_loot[2];
    if(variant==0){putint(loot,1);putint(loot+0x44,3950276909u);putint(loot+0x88,101);}
    if(variant==1){putint(bag,1);putint(bag+0x44,3950276909u);putint(bag+0x88,105);putint(loot,525);putint(loot+0x44,0xeed472);putint(loot+0x88,0);}
    if(variant==2){putint(bag,1);putint(bag+0x44,3950276909u);putint(bag+0x88,105);}
    memcpy(rs_ui_item[0],loot,DD_ITEM_SIZE);
    uint8_t *bag_ui=direction?rs_target:rs_source,*loot_ui=direction?rs_source:rs_target;
    putptr(bag_ui+0x28,(uintptr_t)(hm_raid+DD_WALK_RAID_INVENTORY));putptr(bag_ui+0x30,(uintptr_t)bag);putint(bag_ui+0xb8,1);
    putptr(loot_ui+0x28,(uintptr_t)rs_ui_inv);putptr(loot_ui+0x30,(uintptr_t)rs_ui_item[0]);putint(loot_ui+0xb8,0);
    rs_widgets[0]=(uintptr_t)loot_ui;rs_widgets[1]=0;
    putptr(rs_bar+8,(uintptr_t)rs_ui_inv);putptr(rs_bar+0x10,(uintptr_t)rs_widgets);putptr(rs_bar+0x18,(uintptr_t)(rs_widgets+2));
    putptr(rs_ui_inv+8,(uintptr_t)rs_ui_item);putptr(rs_ui_inv+0x10,(uintptr_t)(rs_ui_item+2));
    putptr(hm_display+DD_WALK_LOOT_OVERLAY+8,(uintptr_t)rs_bar);
    hm_display[DD_WALK_LOOT_OVERLAY+0x218]=hm_display[DD_WALK_LOOT_OVERLAY+0x219]=0;
    putptr(hm_display+0x40e0,0);putptr(hm_display+0x40e8,0);hm_display[0x4118]=0;
}
static void raid_swap_transactions(void) {
    int (*view)(walk_view *)=g_walk_view;uint32_t (*quest)(void)=g_discard_quest_type;
    dd_action order={0};
    for(unsigned player=0;player<4;player++)for(unsigned direction=0;direction<2;direction++)for(unsigned variant=0;variant<3;variant++) {
        rs_reset(player?DD_ROLE_GUEST:DD_ROLE_HOST,direction,variant);g_cfg.player=player;
        bag_slot old[2];CHECK(bag_read_slot((uintptr_t)(direction?hm_raid_loot[2]:hm_raid_bag[1]),&old[0]));
        CHECK(bag_read_slot((uintptr_t)(direction?hm_raid_bag[1]:hm_raid_loot[2]),&old[1]));
        hook_trinket_pickup_inventory(rs_source,NULL,NULL,rs_drag);
        putint(rs_drag+8,(uint32_t)old[0].count);
        CHECK(g_raid_drag.valid);
        CHECK(!hook_trinket_drop_inventory(rs_target+0x10,NULL,rs_drag));
        CHECK(sent==1 && last_message.u.town.action.kind==DD_ACT_RAID_SWAP);
        order=last_message.u.town.action;
        CHECK(dd_action_valid(&order) && order.u.raid_swap.slot[direction?0:1]==2);
        if(player) {
            CHECK(!hm_native);hook_trinket_cancel_inventory(rs_source,rs_drag);CHECK(!hm_native && !rs_source[0xc0]);
            dd_msg request=last_message;
            rs_reset(DD_ROLE_HOST,direction,variant);CHECK(queue_push(&g_incoming,&request));dd_runtime_poll();
            CHECK(g_town.accepted==1 && !g_town.needs_resync);
        }
        CHECK(hm_native==3 && !hm_item_alive);
        bag_slot now[2];CHECK(bag_read_slot((uintptr_t)(direction?hm_raid_loot[2]:hm_raid_bag[1]),&now[0]));
        CHECK(bag_read_slot((uintptr_t)(direction?hm_raid_bag[1]:hm_raid_loot[2]),&now[1]));
        CHECK(!memcmp(&now[0],&old[1],sizeof now[0]) && !memcmp(&now[1],&old[0],sizeof now[1]));
        for(unsigned peer=1;peer<4;peer++) {
            rs_reset(DD_ROLE_GUEST,direction,variant);g_cfg.player=peer;
            wk_push(1,&order);dd_runtime_poll();CHECK(g_town.applied_seq==1 && hm_native==3 && !hm_item_alive && !g_town.needs_resync);
        }
    }
    /* Ordinary supply/gem drag through the bag's copied InventorySlot, not the
     * canonical Item pointer: regression for torches exchanged with onyx. */
    for(unsigned player=0;player<4;player++)for(unsigned direction=0;direction<2;direction++)for(unsigned copy=0;copy<2;copy++) {
        rs_reset(player?DD_ROLE_GUEST:DD_ROLE_HOST,direction,2);g_cfg.player=player;
        uint8_t bag_copy[DD_ITEM_SIZE]={0};
        putint(hm_raid_bag[1],8);putint(hm_raid_bag[1]+0x44,0x6a930e79);putint(hm_raid_bag[1]+0x88,0x378f6e20);
        memcpy(bag_copy,hm_raid_bag[1],DD_ITEM_SIZE);
        putint(hm_raid_loot[2],1);putint(hm_raid_loot[2]+0x44,0x47f85);putint(hm_raid_loot[2]+0x88,0x100f8a6);
        memcpy(rs_ui_item[0],hm_raid_loot[2],DD_ITEM_SIZE);
        uint8_t *bag_ui=direction?rs_target:rs_source;
        putptr(bag_ui+0x28,(uintptr_t)(hm_raid+DD_WALK_RAID_INVENTORY-0x100));putptr(bag_ui+0x30,(uintptr_t)(copy?bag_copy:hm_raid_bag[1]));
        hook_trinket_pickup_inventory(rs_source,NULL,NULL,rs_drag);
        CHECK(g_raid_drag.valid);
        putint(rs_drag+8,(uint32_t)g_raid_drag.item.count);
        CHECK(!hook_trinket_drop_inventory(rs_target+0x10,NULL,rs_drag));
        CHECK(sent==1 && last_message.u.town.action.kind==DD_ACT_RAID_SWAP);
        if(player)CHECK(!hm_native);
        else CHECK(hm_native==3);
        raid_swap_reset();sent=0;hm_native=0;
        /* A stale UI copy/index must not resolve a different matching stack. */
        putint(bag_copy,999);putptr(bag_ui+0x30,(uintptr_t)bag_copy);
        uint8_t inv=0;uint16_t slot=0;bag_slot item;
        walk_view scene;CHECK(hm_raid_view(&scene));
        CHECK(!raid_widget_endpoint(&scene,(uintptr_t)bag_ui,&inv,&slot,&item));
        hook_trinket_pickup_inventory(bag_ui,NULL,NULL,rs_drag);
        CHECK(g_raid_drag.valid==2 && g_raid_drag.submitted);
        CHECK(!hook_trinket_drop_inventory((direction?rs_source:rs_target)+0x10,NULL,rs_drag) && !sent && !hm_native);
    }
    rs_reset(DD_ROLE_GUEST,0,0);order.u.raid_swap.inv[0]=0;order.u.raid_swap.slot[0]=1;
    order.u.raid_swap.inv[1]=1;order.u.raid_swap.slot[1]=2;
    order.u.raid_swap.count[0]=8;order.u.raid_swap.type[0]=0x6a930e79;order.u.raid_swap.id[0]=301;
    order.u.raid_swap.count[1]=1;order.u.raid_swap.type[1]=3950276909u;order.u.raid_swap.id[1]=101;
    for(unsigned bad=0;bad<6;bad++) {
        rs_reset(DD_ROLE_GUEST,0,0);dd_action a=order;
        if(bad==0)a.u.raid_swap.count[0]++;
        if(bad==1)a.u.raid_swap.id[1]++;
        if(bad==2)a.u.raid_swap.area++;
        if(bad==3)a.u.raid_swap.slot[0]=63;
        if(bad==4){putint(hm_raid_bag[1]+0x44,discard_test_quest());a.u.raid_swap.type[0]=discard_test_quest();}
        if(bad==5)hm_loot_window(3);
        CHECK(!raid_swap_apply(&a) && !hm_native && !hm_item_alive);
    }
    rs_reset(DD_ROLE_GUEST,0,0);hm_loot_window(0);wk_push(1,&order);dd_runtime_poll();
    CHECK(!hm_native && g_incoming.count==1);hm_loot_window(2);dd_runtime_poll();CHECK(hm_native==3 && g_town.applied_seq==1);
    rs_reset(DD_ROLE_GUEST,0,0);hm_broken_swap=1;wk_push(1,&order);dd_runtime_poll();
    CHECK(hm_native==3 && !hm_item_alive && !g_town.applied_seq && g_town.needs_resync && g_sync_stop_sent);
    rs_reset(DD_ROLE_HOST,0,0);CHECK(raid_swap_apply(&order));
    CHECK(!raid_swap_apply(&order) && hm_native==3); /* crossed old endpoints do not swap back */
    rs_reset(DD_ROLE_HOST,0,0);g_full_loot_seq=1;dd_msg stale={0};stale.type=DD_MSG_TOWN_REQUEST;stale.u.town.request_id=0x01000001;stale.u.town.action=order;
    CHECK(queue_push(&g_incoming,&stale));dd_runtime_poll();CHECK(g_town.rejected==1 && !hm_native && !g_incoming.count && !g_town.needs_resync);
    rs_reset(DD_ROLE_GUEST,0,0);hook_trinket_pickup_inventory(rs_source,NULL,NULL,rs_drag);putint(rs_drag+8,1);
    CHECK(!hook_trinket_drop_inventory(rs_target+0x10,NULL,rs_drag) && !sent && !hm_native); /* no full swap of a split drag */
    /* Equipped trinket dropped onto the loot window's copied/compacted slot:
     * the existing hero-equip transaction must resolve that destination too. */
    rs_reset(DD_ROLE_GUEST,0,2);uint8_t hero_ui[0x100]={0};
    memcpy(hm_gear[0][0],hm_estate[1],DD_ITEM_SIZE);
    putptr(hero_ui+0x28,(uintptr_t)(hm_entries[0]+8+0x1280));putptr(hero_ui+0x30,(uintptr_t)hm_gear[0][0]);
    hook_trinket_pickup_hero(hero_ui,NULL,NULL,rs_drag);CHECK(g_trinket_drag.valid==1 && !g_raid_drag.valid);
    CHECK(!hook_trinket_drop_inventory(rs_target+0x10,NULL,rs_drag));
    CHECK(sent==1 && last_message.u.town.action.kind==DD_ACT_TRINKET_MOVE && last_message.u.town.action.u.move.dst_slot==2);
    dd_msg gear_request=last_message;rs_reset(DD_ROLE_HOST,0,2);memcpy(hm_gear[0][0],hm_estate[1],DD_ITEM_SIZE);
    CHECK(queue_push(&g_incoming,&gear_request));dd_runtime_poll();
    CHECK(g_town.accepted==1 && hm_native==3 && !hm_item_alive && hm_gear[0][0][0x88]==101 && hm_raid_loot[2][0x88]==102);
    g_walk_view=view;g_discard_quest_type=quest;raid_swap_reset();
    puts("Raid bag/loot exchange: whole supply/gold/trinket stacks both ways, full bag, rooted compacted loot UI, all4 initiators/3 peers, native lifetime, source cleanup, stale/quest/loading guards verified");
}

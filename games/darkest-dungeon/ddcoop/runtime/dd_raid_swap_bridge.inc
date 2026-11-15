/* A whole stack exchanged between the normal raid bag and open loot. Separate
 * from equipping a hero: either endpoint can be supplies, gold or a trinket. */
static struct {uint64_t ui,data;uint8_t inv;uint16_t slot;bag_slot item;int valid,submitted;} g_raid_drag;
static uint32_t g_raid_swap_request;
static int raid_swap_owns_drag(void) {return g_raid_drag.submitted;}
static void raid_swap_reset(void) {memset(&g_raid_drag,0,sizeof g_raid_drag);g_raid_swap_request=0;}
static void raid_swap_request_done(uint32_t request) {if(request && request==g_raid_swap_request)g_raid_swap_request=0;}
static int raid_widget_endpoint(const walk_view *v,uint64_t widget,uint8_t *inv,uint16_t *slot,bag_slot *item) {
    uint64_t inventory=0,selected=0,begin=0;uint32_t index=0;unsigned count=0;
    if(!safe_read(widget+0x28,&inventory,8) || !safe_read(widget+0x30,&selected,8) || !selected ||
       !safe_read(widget+0xb8,&index,4))return 0;
    for(uint8_t i=0;i<2;i++) {
        if(!bag_span(v->raid,i,&begin,&count) || index>=count ||
           selected!=begin+(uint64_t)index*DD_ITEM_SIZE)continue;
        /* A canonical Item roots itself even when the UI stores its containing
         * wrapper rather than the native inventory subobject at+100. */
        if(!bag_read_slot(selected,item))return 0;
        *inv=i;*slot=(uint16_t)index;return 1;
    }
    /* The bag's native InventorySlot may hold a presentation Item too. Its
     * wrapper+100 is the rooted raid inventory; b8 is still the physical slot.
     * Require its complete identity/quantity at that slot, never search for a
     * similar stack (duplicates and stale split drags must not select another). */
    if(inventory && inventory<=UINT64_MAX-0x100 && inventory+0x100==bag_base(v->raid,0) &&
       bag_span(v->raid,0,&begin,&count) && index<count) {
        bag_slot shown;
        if(!bag_read_slot(selected,&shown) || !bag_read_slot(begin+(uint64_t)index*DD_ITEM_SIZE,item) ||
           memcmp(&shown,item,sizeof shown))return 0;
        *inv=0;*slot=(uint16_t)index;return 1;
    }
    dd_hamlet s;uint64_t canonical=0;uint32_t owner=0,at=0;
    if(!g_hamlet_open_model(&s) || !loot_widget_source(&s,widget,selected,&canonical,&owner,&at) ||
       owner!=DD_TRINKET_LOOT || !bag_span(v->raid,1,&begin,&count) || at>=count ||
       !bag_read_slot(begin+(uint64_t)at*DD_ITEM_SIZE,item))return 0;
    *inv=1;*slot=(uint16_t)at;return 1;
}
static void raid_swap_pickup(void *ui,void *data) {
    walk_view v;memset(&g_raid_drag,0,sizeof g_raid_drag);
    if(g_game_apply_depth || !walk_shared() || !g_walk_view(&v) || !loot_ready(&v) || loot_pending(&v))return;
    uint64_t wrapper=0,bar=0,loot_wrapper=0;
    int rooted=safe_read((uintptr_t)ui+0x28,&wrapper,8) && wrapper &&
        (wrapper==bag_base(v.raid,0) || (wrapper<=UINT64_MAX-0x100 && wrapper+0x100==bag_base(v.raid,0)) ||
         wrapper==bag_base(v.raid,1) || (safe_read(v.display+DD_WALK_LOOT_OVERLAY+8,&bar,8) && bar &&
         safe_read(bar+8,&loot_wrapper,8) && wrapper==loot_wrapper));
    if(!raid_widget_endpoint(&v,(uintptr_t)ui,&g_raid_drag.inv,&g_raid_drag.slot,&g_raid_drag.item) ||
       g_raid_drag.item.count<=0) {
        if(rooted) { /* A stale known raid widget cannot fall through to a local-only native exchange. */
            g_raid_drag.ui=(uintptr_t)ui;g_raid_drag.data=(uintptr_t)data;
            g_raid_drag.valid=2;g_raid_drag.submitted=1;
            dd_log("Raid swap: rooted source could not be verified; drag refused before effects");
        }
        return;
    }
    g_raid_drag.ui=(uintptr_t)ui;g_raid_drag.data=(uintptr_t)data;g_raid_drag.valid=1;
    dd_log("Raid swap: rooted source inventory=%u slot=%u quantity=%d",g_raid_drag.inv,g_raid_drag.slot,g_raid_drag.item.count);
}
static int raid_swap_finish(void *ui,void *data) {
    if(!g_raid_drag.submitted || g_raid_drag.ui!=(uintptr_t)ui || g_raid_drag.data!=(uintptr_t)data)return 0;
    /* Drop returned Cancel to the drag manager. Its stale UI copy must never
     * overwrite the authoritative source. Reset presentation flags only. */
    uint8_t shown=1,dragging=0;walk_view v;uint64_t begin=0;unsigned n=0;bag_slot s;
    if(g_walk_view(&v) && bag_span(v.raid,g_raid_drag.inv,&begin,&n) && g_raid_drag.slot<n &&
       bag_read_slot(begin+(uint64_t)g_raid_drag.slot*DD_ITEM_SIZE,&s))shown=(uint8_t)(s.count>0);
    WriteProcessMemory(GetCurrentProcess(),(uint8_t *)ui+0xc,&shown,1,NULL);
    WriteProcessMemory(GetCurrentProcess(),(uint8_t *)ui+0xc0,&dragging,1,NULL);
    return 1;
}
static int raid_swap_drop(unsigned kind,void *ui,void *data) {
    walk_view v;bag_slot target;uint8_t inv=0;uint16_t slot=0;int32_t dragged=0;
    if(g_game_apply_depth || !g_raid_drag.valid || kind>=2 || !walk_shared() || !g_walk_view(&v))return -1;
    if(g_raid_drag.valid==2)return 0;
    /* Existing drop hooks receive the secondary slot interface at primary+10. */
    if((uintptr_t)ui<0x10 || !raid_widget_endpoint(&v,(uintptr_t)ui-0x10,&inv,&slot,&target))return -1;
    if(inv==g_raid_drag.inv)return -1; /* existing within-bag drag/stack path */
    if(g_raid_drag.submitted || g_raid_swap_request || runtime_shared_blocked() || !loot_ready(&v) || loot_pending(&v) ||
       g_raid_drag.data!=(uintptr_t)data || !safe_read((uintptr_t)data+8,&dragged,4))return 0;
    if(dragged!=g_raid_drag.item.count) {
        dd_log("Raid swap: partial-stack drag refused before effects");return 0;
    }
    if(target.count>0 && target.type==g_raid_drag.item.type && target.id==g_raid_drag.item.id)return -1; /* native merge */
    uint32_t quest=g_discard_quest_type();
    if(!quest || (g_raid_drag.inv==0 && g_raid_drag.item.type==quest) || (inv==0 && target.count>0 && target.type==quest))return 0;
    dd_action a={0};a.kind=DD_ACT_RAID_SWAP;a.u.raid_swap.area=v.area;a.u.raid_swap.loot_seq=g_full_loot_seq;
    a.u.raid_swap.inv[0]=g_raid_drag.inv;a.u.raid_swap.slot[0]=g_raid_drag.slot;
    a.u.raid_swap.count[0]=(uint32_t)g_raid_drag.item.count;a.u.raid_swap.type[0]=g_raid_drag.item.type;a.u.raid_swap.id[0]=g_raid_drag.item.id;
    a.u.raid_swap.inv[1]=inv;a.u.raid_swap.slot[1]=slot;
    a.u.raid_swap.count[1]=(uint32_t)target.count;a.u.raid_swap.type[1]=target.type;a.u.raid_swap.id[1]=target.id;
    if(!dd_action_valid(&a))return 0;
    g_raid_drag.submitted=1;
    dd_log("Raid swap: request %u:%u %08x/%08x/%u with %u:%u %08x/%08x/%u role=%s",
        a.u.raid_swap.inv[0],a.u.raid_swap.slot[0],a.u.raid_swap.type[0],a.u.raid_swap.id[0],a.u.raid_swap.count[0],
        inv,slot,target.type,target.id,(unsigned)target.count,dd_role_name(g_cfg.role));
    if(g_cfg.role==DD_ROLE_GUEST)g_raid_swap_request=dd_town_guest_local(&g_town,&a);
    else (void)dd_town_host_local(&g_town,&a);
    return 0;
}
static int raid_swap_apply(const dd_action *a) {
    walk_view v;uint64_t inv[2],item[2],begin=0;unsigned n=0;bag_slot before[2],after;
    uint32_t quest=g_discard_quest_type();
    if(!dd_action_valid(a) || !quest || !g_walk_view(&v) || v.area!=a->u.raid_swap.area ||
       a->u.raid_swap.loot_seq!=g_full_loot_seq || !loot_ready(&v) || loot_pending(&v) ||
       !g_hamlet_copy || !g_hamlet_clear || !g_hamlet_swap || !g_hamlet_destroy)return 0;
    for(unsigned i=0;i<2;i++) {
        inv[i]=bag_base(v.raid,a->u.raid_swap.inv[i]);
        if(!bag_span(v.raid,a->u.raid_swap.inv[i],&begin,&n) || a->u.raid_swap.slot[i]>=n)return 0;
        item[i]=begin+(uint64_t)a->u.raid_swap.slot[i]*DD_ITEM_SIZE;
        if(!bag_read_slot(item[i],&before[i]) || before[i].count!=(int32_t)a->u.raid_swap.count[i] ||
           before[i].type!=a->u.raid_swap.type[i] || before[i].id!=a->u.raid_swap.id[i] ||
           (a->u.raid_swap.inv[i]==0 && before[i].count>0 && before[i].type==quest)) {
            dd_log("Raid swap: stale/different endpoint %u refused before effects",i);return 0;
        }
    }
    __declspec(align(16)) uint8_t temporary[DD_ITEM_SIZE];
    g_hamlet_copy(temporary,(const void *)(uintptr_t)item[0]);g_hamlet_clear(temporary);
    g_hamlet_swap((void *)(uintptr_t)inv[0],a->u.raid_swap.slot[0],temporary);
    g_hamlet_swap((void *)(uintptr_t)inv[1],a->u.raid_swap.slot[1],temporary);
    g_hamlet_swap((void *)(uintptr_t)inv[0],a->u.raid_swap.slot[0],temporary);
    g_hamlet_destroy(temporary);
    for(unsigned i=0;i<2;i++)if(!bag_read_slot(item[i],&after) || memcmp(&after,&before[1-i],sizeof after)) {
        dd_log("Raid swap: native result incomplete; stop before ACK");runtime_sync_stop(3);return 0;
    }
    bag_recount(v.raid,0);bag_recount(v.raid,1);
    dd_log("Raid swap: native whole-stack exchange applied role=%s",dd_role_name(g_cfg.role));
    return 1;
}

/* Curio item drop BEFORE70bf70 copies/decrements the owning dragged Item.
 * Canonical shared use owns the debit. Native source Restore must not put the
 * old drag count back after a synchronous host apply. */
static int (__fastcall *g_prop_item_drop)(void *,void *,void *);
typedef uint8_t(__fastcall *item_curio_route_fn)(void *,uint32_t,uint32_t);
static item_curio_route_fn g_item_curio_route;
typedef int(__fastcall *prop_item_match_fn)(const void *,const char *,uint32_t,uint32_t);
static prop_item_match_fn g_prop_item_match;
static struct {
    uintptr_t ui,data;
    uint64_t raid,item;
    uint32_t slot,type,id;
    int32_t count;
    int tracked,cleanup;
} g_prop_item_drag;

static void prop_item_reset(void) {memset(&g_prop_item_drag,0,sizeof g_prop_item_drag);}
static int prop_item_coop(void) {
    return g_cfg.role!=DD_ROLE_OFF && InterlockedCompareExchange(&g_campaign_active,0,0);
}
/* Existing inventory pickup hook calls this before native pickup constructs the
 * drag object. Source Item belongs to the raid bag; pickup itself only copies. */
static void prop_item_pickup(unsigned kind,void *ui,void *data) {
    walk_view v;uint64_t item=0,begin=0;unsigned count=0;
    uint32_t type=0,id=0;int32_t quantity=0;
    prop_item_reset();
    if(kind!=1 || !ui || !data || g_game_apply_depth || !prop_item_coop() ||
       !g_walk_view(&v) || v.battle || !bag_span(v.raid,0,&begin,&count) ||
       !safe_read((uintptr_t)ui+0x30,&item,8) || item<begin ||
       item-begin>=(uint64_t)count*DD_ITEM_SIZE || (item-begin)%DD_ITEM_SIZE ||
       !safe_read((uintptr_t)item,&quantity,4) || quantity<=0 ||
       !safe_read((uintptr_t)item+0x44,&type,4) || !type ||
       !safe_read((uintptr_t)item+0x88,&id,4))return;
    g_prop_item_drag.ui=(uintptr_t)ui;g_prop_item_drag.data=(uintptr_t)data;
    g_prop_item_drag.raid=v.raid;g_prop_item_drag.item=item;
    g_prop_item_drag.slot=(uint32_t)((item-begin)/DD_ITEM_SIZE);
    g_prop_item_drag.type=type;g_prop_item_drag.id=id;g_prop_item_drag.count=quantity;
    g_prop_item_drag.tracked=1;
}
static int prop_item_source_done(void *ui,void *data) {
    if(!g_prop_item_drag.cleanup || g_prop_item_drag.ui!=(uintptr_t)ui ||
       g_prop_item_drag.data!=(uintptr_t)data)return 0;
    uint64_t begin=0;unsigned count=0;int32_t quantity=0;
    uint8_t visible=0,dragging=0;
    if(bag_span(g_prop_item_drag.raid,0,&begin,&count) && g_prop_item_drag.slot<count &&
       safe_read((uintptr_t)(begin+(uint64_t)g_prop_item_drag.slot*DD_ITEM_SIZE),&quantity,4))
        visible=(uint8_t)(quantity>0);
    int ok=WriteProcessMemory(GetCurrentProcess(),(uint8_t *)ui+0xc,&visible,1,NULL) &&
           WriteProcessMemory(GetCurrentProcess(),(uint8_t *)ui+0xc0,&dragging,1,NULL);
    prop_item_reset();
    if(!ok)runtime_sync_stop(3);
    return 1; /* Matched stale native finish/cancel is suppressed, even on STOP. */
}
static int __fastcall hook_prop_item_drop(void *slot,void *position,void *data) {
    if(g_game_apply_depth || !prop_item_coop())
        return g_prop_item_drop?g_prop_item_drop(slot,position,data):0;
    /* A rejected shared drop also cancels its drag. If another shared action
     * changed the bag since pickup, even that native cancel has stale counts. */
    if(g_prop_item_drag.tracked && g_prop_item_drag.data==(uintptr_t)data)
        g_prop_item_drag.cleanup=1;
    if(!walk_shared() || runtime_shared_blocked())return 0;
    walk_view v;uint64_t table=0,drag_table=0,prop=0,hero=0,begin=0;
    uint32_t mode=0,type=0,id=0,drag_type=0,drag_id=0;unsigned count=0;
    int32_t quantity=0,drag_quantity=0;
    if(!g_prop_item_drag.tracked || g_prop_item_drag.data!=(uintptr_t)data ||
       !g_walk_view(&v) || v.battle || v.raid!=g_prop_item_drag.raid ||
       (uintptr_t)slot!=v.display+DD_WALK_OVERLAY+0x780 ||
       !safe_read((uintptr_t)slot,&table,8) || table!=image_base()+0xec85b0 ||
       !safe_read((uintptr_t)data,&drag_table,8) ||
       (drag_table!=image_base()+0xeddd10 && drag_table!=image_base()+0xeddd48) ||
       !safe_read((uintptr_t)data+8,&drag_quantity,4) || drag_quantity!=g_prop_item_drag.count ||
       !safe_read((uintptr_t)data+0x4c,&drag_type,4) || drag_type!=g_prop_item_drag.type ||
       !safe_read((uintptr_t)data+0x90,&drag_id,4) || drag_id!=g_prop_item_drag.id ||
       !bag_span(v.raid,0,&begin,&count) || g_prop_item_drag.slot>=count ||
       begin+(uint64_t)g_prop_item_drag.slot*DD_ITEM_SIZE!=g_prop_item_drag.item ||
       !safe_read((uintptr_t)g_prop_item_drag.item,&quantity,4) || quantity!=g_prop_item_drag.count || quantity<=0 ||
       !safe_read((uintptr_t)g_prop_item_drag.item+0x44,&type,4) || type!=drag_type ||
       !safe_read((uintptr_t)g_prop_item_drag.item+0x88,&id,4) || id!=drag_id ||
       !walk_overlay_state(&v,&mode) || mode!=2 ||
       !safe_read((uintptr_t)v.display+DD_WALK_OVERLAY+0x6a0,&prop,8) || !prop ||
       !safe_read((uintptr_t)v.display+DD_WALK_OVERLAY+0xd78,&hero,8)) {
        dd_log("Prop choice: curio item drop refused before native mutation");
        return 0;
    }
    dd_action a;
    if(!walk_use_order(&v,prop,hero,0,0,7,&a))return 0;
    a.u.use.item_type=type;a.u.use.item_id=id;
    dd_log("Prop choice: curio item drag submitted before native consumption");
    walk_choice_dispatch(&a);
    return 0; /* Cancel source gesture; ordered native curio use owns all effects. */
}
/* Match429750's exact routing predicate. Hero presence does NOT distinguish
 * food on a hero from an item on a curio. Only the pinned pure predicate does. */
static int prop_item_route(const walk_view *v,void *controller,void *inventory,int32_t slot,dd_action *a) {
    uint64_t prop=0,hero=0,begin=0;uint32_t mode=0,type=0,id=0;unsigned count=0;
    if(v->battle)return 0;
    uint64_t panel=v->display+DD_WALK_OVERLAY;
    if(!safe_read((uintptr_t)panel+0x6a0,&prop,8) || !safe_read((uintptr_t)panel+0xd88,&mode,4))return -1;
    if(!prop || (mode!=1 && mode!=2))return 0;
    uint8_t inv=(uintptr_t)inventory==v->raid+DD_WALK_RAID_INVENTORY?0:
                (uintptr_t)inventory==v->raid+DD_WALK_RAID_LOOT?1:2;
    if(inv>1 || slot<0 || !g_item_curio_route || !bag_span(v->raid,inv,&begin,&count) ||
       (unsigned)slot>=count || !safe_read((uintptr_t)(begin+(uint64_t)slot*DD_ITEM_SIZE+0x44),&type,4) ||
       !safe_read((uintptr_t)(begin+(uint64_t)slot*DD_ITEM_SIZE+0x88),&id,4))return -1;
    if(!g_item_curio_route(controller,type,id))return 0; /* ordinary hero use */
    if(inv || mode!=2 || !safe_read((uintptr_t)panel+0xd78,&hero,8) ||
       !walk_use_order(v,prop,hero,0,0,7,a))return -1;
    a->u.use.item_type=type;a->u.use.item_id=id;return 1;
}
static int prop_item_ready(const walk_view *v,const dd_action *a) {
    uint64_t prop=0,option=0,begin=0,end=0;int terminated=0;
    if(!g_curio_item || !g_prop_item_match || !a->u.use.item_type ||
       !(prop=walk_prop_at(v->area_ptr,a->u.use.prop)) ||
       !item_in_bag(v->raid,a->u.use.item_type,a->u.use.item_id) ||
       !safe_read((uintptr_t)prop+0xd0,&option,8) || !option ||
       !safe_read((uintptr_t)option+0x208,&begin,8) ||
       !safe_read((uintptr_t)option+0x210,&end,8) || end<begin ||
       (end-begin)%8 || (end-begin)/8>4096 || (!begin && end))return 0;
    /* Match native712e40 before revision, selection, RNG, or bag effects. Its
     * signed result is an interaction index; -1 means an ordinary invalid item,
     * not a partially executed shared change requiring a session stop. */
    for(unsigned i=0;i<64;i++) {
        uint8_t c=0;
        if(!safe_read((uintptr_t)prop+0x125+i,&c,1))return 0;
        if(!c){terminated=1;break;}
    }
    if(!terminated)return 0;
    for(uint64_t p=begin;p<end;p+=8) {
        uint64_t entry=0,table=0,method=0;uint32_t type=0;
        if(!safe_read((uintptr_t)p,&entry,8) || !entry || entry>UINT64_MAX-0x94 ||
           !safe_read((uintptr_t)entry+0x90,&type,4))return 0;
        if(type==a->u.use.item_type) {
            MEMORY_BASIC_INFORMATION region={0};
            if(!safe_read((uintptr_t)entry,&table,8) || !table || table>UINT64_MAX-16 ||
               !safe_read((uintptr_t)table+8,&method,8) || !method ||
               VirtualQuery((const void *)(uintptr_t)method,&region,sizeof region)!=sizeof region ||
               region.State!=MEM_COMMIT || (region.Protect&(PAGE_GUARD|PAGE_NOACCESS)) ||
               !(region.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return 0;
        }
    }
    return g_prop_item_match((const void *)(uintptr_t)option,
        (const char *)(uintptr_t)(prop+0x125),a->u.use.item_type,a->u.use.item_id)>=0;
}
static int prop_item_apply(const walk_view *v,const dd_action *a) {
    uint64_t item=item_in_bag(v->raid,a->u.use.item_type,a->u.use.item_id);
    int32_t before=0,after=-1;
    if(!g_curio_item || !item || !safe_read((uintptr_t)item,&before,4) || before<=0)return 0;
    if(!g_curio_item((void *)(uintptr_t)(v->display+DD_WALK_OVERLAY),(void *)(uintptr_t)item))return 0;
    item_take_one(v,item);
    return safe_read((uintptr_t)item,&after,4) && after==before-1;
}

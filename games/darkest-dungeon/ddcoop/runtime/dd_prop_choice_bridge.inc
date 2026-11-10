/* All terminal curio/obstacle choices use the same ordered native path.
 * Preflight is read-only: a rejected competitor must not change selection/RNG. */
static int walk_choice_dispatch(const dd_action *a) {
    if(!walk_shared() || runtime_shared_blocked() || !dd_action_valid(a))return 0;
    if(walk_use_repeat(a))return 0;
    dd_log("Prop choice: request op=%u area=%08x prop=%u revision=%u actor=%u null=%u role=%s",
        a->u.use.door,a->u.use.area,a->u.use.prop,a->u.use.revision,a->u.use.actor_id,a->u.use.flag,dd_role_name(g_cfg.role));
    if(g_cfg.role==DD_ROLE_GUEST)return dd_town_guest_local(&g_town,a)!=0;
    return dd_town_host_local(&g_town,a);
}
static int walk_choice_apply(const dd_action *a) {
    walk_view v={0};uint64_t prop=0,hero=0,begin=0,end=0,item=0;
    uint32_t mode=0,kind=0;uint8_t op=a->u.use.door;
    if(!dd_action_valid(a) || op<3 || op>7 || a->u.use.revision!=g_prop_revision ||
       g_prop_revision==UINT32_MAX || !g_walk_view(&v) || v.battle ||
       v.area!=a->u.use.area || !(prop=walk_prop_at(v.area_ptr,a->u.use.prop)) ||
       !safe_read((uintptr_t)prop+0xc,&kind,4) || kind!=a->u.use.prop_kind ||
       !walk_overlay_state(&v,&mode) || mode!=2 ||
       !walk_overlay_matches(&v,prop,(op==7?3:op==6?(a->u.use.option?5:3):op)) ||
       (g_prop_resolved.valid && g_prop_resolved.area==a->u.use.area &&
        g_prop_resolved.prop==a->u.use.prop && g_prop_resolved.kind==kind)) {
        dd_log("Prop choice: refused before effects op=%u revision=%u current=%u area=%08x prop=%u mode=%u",
            op,a->u.use.revision,g_prop_revision,a->u.use.area,a->u.use.prop,mode);
        return 0;
    }
    if(op==6) {
        if(!(a->u.use.option?g_prop_cancel:g_curio_cancel))return 0;
    } else {
        if(!g_party_select || !safe_read((uintptr_t)v.raid+8,&begin,8) || !begin ||
           !safe_read((uintptr_t)v.raid+0x10,&end,8) || end<begin || (end-begin)%8 ||
           (end-begin)/8>4 || a->u.use.hero>=(end-begin)/8 ||
           !safe_read((uintptr_t)(begin+(uint64_t)a->u.use.hero*8),&hero,8) || !hero ||
           !safe_read((uintptr_t)hero+0x137c,&kind,4) || kind!=a->u.use.actor_id)return 0;
        if(op==3 && !g_curio_button)return 0;
        if(op==4 && (!g_curio_tool || !(item=item_in_bag(v.raid,a->u.use.item_type,a->u.use.item_id))))return 0;
        if(op==5 && !g_overlay_use)return 0;
        if(op==7 && !prop_item_ready(&v,a))return 0;
    }
    /* Advance before native callbacks. Every committed copy does this once;
     * stale hand/tool/X requests cannot act after a cancel/reopen or scene. */
    ++g_prop_revision;
    g_prop_resolved.area=a->u.use.area;g_prop_resolved.prop=a->u.use.prop;
    g_prop_resolved.kind=a->u.use.prop_kind;g_prop_resolved.valid=1;
    if(op!=6)walk_rules_send(&v);
    uint64_t panel=v.display+DD_WALK_OVERLAY,functor[2]={0,panel};
    if(op==6) {
        overlay_use_fn native=a->u.use.option?g_prop_cancel:g_curio_cancel;
        native(functor);
        g_prop_cancelled=1;g_prop_cancel_area=a->u.use.area;
        g_prop_cancel_index=a->u.use.prop;g_prop_cancel_type=a->u.use.option;
    } else {
        walk_select(&v,a->u.use.hero);
        /* NULL is a normal native obstacle actor, not a missing-hero error.
         * Ordering metadata uses selected HeroID; preserve the native argument. */
        uint64_t native_hero=a->u.use.flag?0:hero;
        memcpy((void *)(uintptr_t)(panel+0xd78),&native_hero,8);
        if(op==3) {
            memcpy((void *)(uintptr_t)(panel+0x76c),&a->u.use.item_type,4);
            memcpy((void *)(uintptr_t)(panel+0x770),&a->u.use.item_id,4);
            g_curio_button(functor);
        } else if(op==4) {
            __declspec(align(16)) uint8_t capture[8+0x5a8];
            memcpy(capture,&panel,8);memcpy(capture+8,(void *)(uintptr_t)item,0x5a8);
            g_curio_tool(capture);
        } else if(op==5)g_overlay_use(functor);
        else if(!prop_item_apply(&v,a)){runtime_sync_stop(3);return 0;}
    }
    dd_log("Prop choice: applied op=%u area=%08x prop=%u revision=%u next=%u role=%s",
        op,a->u.use.area,a->u.use.prop,a->u.use.revision,g_prop_revision,dd_role_name(g_cfg.role));
    return !g_town.needs_resync;
}

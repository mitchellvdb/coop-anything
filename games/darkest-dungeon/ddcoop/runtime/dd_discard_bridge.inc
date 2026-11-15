/* Shift-click736cb0 -> optional Yes functor738510 ->737070. Order the
 * confirmed removal before mutation, rather than a later per-frame drag mark. */
static uint32_t g_discard_request;
static void discard_reset(void) { g_discard_request=0; }
static void discard_request_done(uint32_t request) { if(request && request==g_discard_request)g_discard_request=0; }
static uint32_t discard_quest_type(void) {uint32_t t=0;safe_read(image_base()+0x2a89494,&t,4);return t;}
static uint32_t (*g_discard_quest_type)(void)=discard_quest_type;
static int discard_ready(const walk_view *v, uint8_t inv) {
    return v->state>1 && v->state!=6 && v->state!=7 && (!inv || loot_ready(v));
}
static uint64_t __fastcall hook_bag_throw_b(void *capture,void *b,void *c,void *d) {
    walk_view v;dd_action a={0};uint64_t inventory=0,item=0,begin=0;unsigned n=0;
    uint32_t slot=0,take=0;bag_slot before;
    if(!g_bag_throw_b)return 0;
    if(g_game_apply_depth || !walk_coop_active())return g_bag_throw_b(capture,b,c,d);
    if(!walk_shared() || runtime_shared_blocked())return 0;
    if(!g_walk_view(&v))return g_bag_throw_b(capture,b,c,d); /* existing town path */
    if(g_discard_request || !safe_read((uintptr_t)capture,&inventory,8) ||
       !safe_read((uintptr_t)capture+8,&slot,4) || !safe_read((uintptr_t)capture+0x10,&item,8) || !item ||
       !safe_read((uintptr_t)item,&take,4) || !take || take>INT32_MAX)return 0;
    uint8_t inv=inventory==bag_base(v.raid,0)?0:inventory==bag_base(v.raid,1)?1:2;
    if(inv>1 || !discard_ready(&v,inv) || !bag_span(v.raid,inv,&begin,&n) || slot>=n ||
       !bag_read_slot(begin+(uint64_t)slot*DD_ITEM_SIZE,&before) || before.count<=0 || take>(uint32_t)before.count) {
        dd_log("Discard: confirmed source not a ready canonical raid inventory (inv=%u slot=%u take=%u)",inv,slot,take);
        return 0;
    }
    uint32_t quest_type=g_discard_quest_type();
    if(!quest_type || before.type==quest_type)return 0;
    a.kind=DD_ACT_ITEM;a.u.item.op=DD_ITEM_OP_DISCARD;a.u.item.hero=255;
    a.u.item.area=v.area;a.u.item.inv=inv;a.u.item.slot=(int32_t)slot;
    a.u.item.type=before.type;a.u.item.id=before.id;
    a.u.item.round=(uint32_t)before.count;a.u.item.amount=take;
    dd_log("Discard: confirmed request inventory=%u slot=%u type=%08x item=%08x count=%u take=%u role=%s",
        inv,slot,before.type,before.id,a.u.item.round,take,dd_role_name(g_cfg.role));
    if(g_cfg.role==DD_ROLE_GUEST)g_discard_request=dd_town_guest_local(&g_town,&a);
    else (void)dd_town_host_local(&g_town,&a);
    return 0;
}
static int discard_apply(const dd_action *a) {
    walk_view v;uint64_t begin=0;unsigned n=0;bag_slot before,after;
    if(!dd_action_valid(a) || !g_bag_throw_b || !g_walk_view(&v) || v.area!=a->u.item.area ||
       !discard_ready(&v,a->u.item.inv) || !bag_span(v.raid,a->u.item.inv,&begin,&n) || (unsigned)a->u.item.slot>=n)return 0;
    uint64_t item=begin+(uint64_t)a->u.item.slot*DD_ITEM_SIZE;
    uint32_t quest_type=g_discard_quest_type();
    if(!quest_type || !bag_read_slot(item,&before) || before.type==quest_type ||
       before.count!=(int32_t)a->u.item.round || before.type!=a->u.item.type || before.id!=a->u.item.id) {
        dd_log("Discard: stale or different source; no native effect (inventory=%u slot=%d)",a->u.item.inv,a->u.item.slot);return 0;
    }
    /*737070 reads only inventory+0,index+8 and count-pointer+10, then invokes
     * native Item clear and recount. No owning C++ Item is fabricated/copied. */
    uint64_t args[3]={bag_base(v.raid,a->u.item.inv),(uint32_t)a->u.item.slot,(uintptr_t)&a->u.item.amount};
    g_bag_throw_b(args,NULL,NULL,NULL);
    int32_t remaining=before.count-(int32_t)a->u.item.amount;
    if(!bag_read_slot(item,&after) || after.count!=remaining ||
       (remaining && (after.type!=before.type || after.id!=before.id))) {
        dd_log("Discard: native quantity/identity result failed; stop before ACK");runtime_sync_stop(3);return 0;
    }
    dd_log("Discard: applied inventory=%u slot=%d item=%08x count=%d->%d role=%s",
        a->u.item.inv,a->u.item.slot,before.id,before.count,remaining,dd_role_name(g_cfg.role));
    return 1;
}

/* Normal dungeon default-order button AND its keyboard shortcut converge on
 * RaidDisplay::RestoreDefaultPartyOrder at76f060. Native code sets rank masks,
 * updates the party vector immediately and schedules the presentation itself.
 * No foreign pointers, direct rank/vector writes or battle AI hooks are used. */
typedef void(__fastcall *formation_native_fn)(void *);
typedef uint8_t(__fastcall *formation_default_fn)(void *, uint64_t *);
static formation_native_fn g_formation_native;
static formation_default_fn g_formation_default;
static uint32_t g_formation_request;
static uint64_t g_formation_wait;
/* Hand-made party order (FORMATION_SET, protocol37). A drag of a hero to another rank outside a fight changes only the
 * copy it happens on (owner's run of 12 Nov 16:19 and 16:21). Its native input path is not mapped; instead every frame
 * compares the party with the last shared order, and a new order of the same members is shared as a whole. Each copy
 * then produces it through the same native rerank the default-order button uses (76f060): Raid::GetDefaultPartyOrder
 * (614cb0) is hooked, and only while that call runs returns the wanted order instead of the default. */
static formation_default_fn g_formation_default_orig; /* trampoline of the hooked 614cb0 */
static const uint32_t *g_formation_override;          /* the IDs the native rerank is to produce, during that call only */
static uint32_t g_formation_known[4], g_formation_known_area, g_formation_sent[4], g_formation_revert[4];
static int g_formation_known_valid, g_formation_revert_due;
static uint64_t g_formation_quiet_until;              /* after a fight: the game settles ranks itself, adopted */
static dd_action g_formation_pending;                 /* guest: the order asked for in g_formation_request */
static void formation_reset(void) {
    g_formation_request = 0; g_formation_wait = 0;
    g_formation_known_valid = 0; g_formation_revert_due = 0; g_formation_quiet_until = 0;
    memset(g_formation_sent, 0, sizeof g_formation_sent);
    memset(&g_formation_pending, 0, sizeof g_formation_pending);
}
static int formation_ids(uint64_t raid, uint32_t ids[4]) {
    uint64_t begin=0,end=0;
    memset(ids,0,16);
    if(!safe_read((uintptr_t)raid+8,&begin,8) || !safe_read((uintptr_t)raid+16,&end,8) ||
       end<begin || (end-begin)%8 || (end-begin)/8>4 || end==begin)return 0;
    for(unsigned i=0;i<(end-begin)/8;i++) {
        uint64_t hero=0;
        if(!safe_read((uintptr_t)begin+i*8,&hero,8))return 0;
        if(hero && (!safe_read((uintptr_t)hero+0x137c,&ids[i],4) || !ids[i] || ids[i]==UINT32_MAX))return 0;
        for(unsigned j=0;j<i;j++)if(ids[i] && ids[i]==ids[j])return 0;
    }
    return ids[0] || ids[1] || ids[2] || ids[3];
}
static int formation_animating(const walk_view *v) {
    uint8_t busy=1;
    return !safe_read((uintptr_t)v->display+0x4119,&busy,1) || busy;
}
static int formation_busy(void) {
    walk_view v;
    return g_formation_request || (g_walk_view(&v) && formation_animating(&v));
}
static int formation_ready(const walk_view *v) {
    uint64_t now=GetTickCount64(); uint8_t blocked=1;
    return !v->battle && v->state>1 && v->state!=6 && v->state!=7 && !formation_animating(v) &&
        safe_read((uintptr_t)v->display+0x4118,&blocked,1) && !blocked &&
        (!g_walk_door_pending || now>=g_walk_door_pending) && now>=g_walk_door_until;
}
static int formation_default_ids(const walk_view *v, uint32_t ids[4]) {
    uint64_t heroes[4]={0};
    if(!g_formation_default)return 0;
    (void)g_formation_default((void *)(uintptr_t)v->raid,heroes);
    memset(ids,0,16);
    for(unsigned i=0;i<4;i++)if(heroes[i] && !safe_read((uintptr_t)heroes[i]+0x137c,&ids[i],4))return 0;
    return 1; /* Caller validates uniqueness and exact membership against current party. */
}
static int formation_same_members(const uint32_t a[4], const uint32_t b[4]) {
    unsigned na=0,nb=0;
    for(unsigned i=0;i<4;i++) {
        na+=a[i]!=0;nb+=b[i]!=0;
        if(a[i]) { unsigned j=0; for(;j<4;j++)if(b[j]==a[i])break; if(j==4)return 0; }
    }
    return na==nb && na>0;
}
static void formation_adopt(const walk_view *v, const uint32_t ids[4]) {
    memcpy(g_formation_known,ids,16);g_formation_known_area=v->area;g_formation_known_valid=1;
}
/* 614cb0 (raid, out: four Hero pointers by rank) -> 1 when the party already stands so. Outside our own rerank it is the
 * game's own default; during formation_native_order it names the heroes of the wanted order, found by persistent ID in
 * the live party vector (never a pointer from a message). */
static uint8_t __fastcall hook_formation_default(void *raid, uint64_t *out) {
    const uint32_t *want=g_formation_override;
    if(want && out) {
        uint64_t begin=0,end=0,heroes[4]={0};int same=1;
        if(safe_read((uintptr_t)raid+8,&begin,8) && safe_read((uintptr_t)raid+16,&end,8) && end>begin &&
           (end-begin)%8==0 && (end-begin)/8<=4) {
            unsigned count=(unsigned)((end-begin)/8),found=0,wanted=0;
            for(unsigned i=0;i<4;i++) {
                if(!want[i])continue;
                wanted++;
                for(unsigned j=0;j<count;j++) {
                    uint64_t hero=0;uint32_t id=0;
                    if(safe_read((uintptr_t)begin+j*8,&hero,8) && hero && safe_read((uintptr_t)hero+0x137c,&id,4) && id==want[i]) {
                        heroes[i]=hero;found++;if(j!=i)same=0;break;
                    }
                }
            }
            if(found==wanted && found==count) {
                memcpy(out,heroes,sizeof heroes);
                return (uint8_t)same;
            }
        }
        dd_log("Formation: wanted order not found in the live party; nothing is moved");
        memset(out,0,32);
        return 1; /* "already in order": the native rerank changes nothing, and the caller's check stops. */
    }
    return g_formation_default_orig ? g_formation_default_orig(raid,out) : 1;
}
/* The native rerank with the wanted order; 1 when the party vector afterwards is exactly that order. */
static int formation_native_order(const walk_view *v, const uint32_t want[4]) {
    uint32_t after[4];
    if(!g_formation_native)return 0;
    g_formation_override=want;
    g_formation_native((void *)(uintptr_t)v->display);
    g_formation_override=NULL;
    return formation_ids(v->raid,after) && !memcmp(after,want,16);
}
static int formation_set_apply(const dd_action *a) {
    walk_view v;uint32_t ids[4];const uint32_t *after=a->u.formation.after;
    if(!g_walk_view(&v) || v.battle || v.area!=a->u.formation.area || !formation_ids(v.raid,ids))return 0;
    if(!memcmp(ids,after,16)) { /* the copy it was made on, or one that made the same order */
        formation_adopt(&v,ids);memset(g_formation_sent,0,16);
        dd_log("Formation: hand-made order kept area=%08x ids=%u,%u,%u,%u (role=%s)",v.area,ids[0],ids[1],ids[2],ids[3],dd_role_name(g_cfg.role));
        return 1;
    }
    if(!formation_ready(&v) || !formation_same_members(ids,after)) {
        dd_log("Formation: hand-made order refused here: not ready or other members (role=%s)",dd_role_name(g_cfg.role));
        return 0;
    }
    if(!formation_native_order(&v,after)) {
        dd_log("Formation: native rerank did not produce the hand-made order; shared actions stop");
        runtime_sync_stop(1);return 0;
    }
    formation_adopt(&v,after);memset(g_formation_sent,0,16);
    dd_log("Formation: hand-made order applied area=%08x ids=%u,%u,%u,%u (role=%s)",v.area,after[0],after[1],after[2],after[3],dd_role_name(g_cfg.role));
    return 1;
}
/* Every frame in a raid. A different order of the same heroes outside a fight that no shared action made is a player's
 * own drag: the host orders it for everyone, a guest asks the host (and the host waits with walking meanwhile). */
static void formation_watch(const walk_view *v) {
    uint32_t ids[4];uint64_t now=GetTickCount64();
    if(!formation_ids(v->raid,ids))return;
    if(g_formation_revert_due && !g_formation_request && formation_ready(v)) {
        g_formation_revert_due=0;
        if(memcmp(ids,g_formation_revert,16) && formation_same_members(ids,g_formation_revert)) {
            if(formation_native_order(v,g_formation_revert)) {
                memcpy(ids,g_formation_revert,16);
                dd_log("Formation: the host refused this order; the party stands as before again");
                shop_notice("The host could not take over this party order. Your heroes stand as before; try again.");
            } else {
                dd_log("Formation: refused order could not be undone here; shared actions stop");
                runtime_sync_stop(1);return;
            }
        }
        formation_adopt(v,ids);return;
    }
    if(!g_formation_known_valid || v->area!=g_formation_known_area || v->battle || !walk_shared() || g_game_apply_depth ||
       now<g_formation_quiet_until || v->state<=1 || v->state==6 || v->state==7) {
        if(v->battle)g_formation_quiet_until=now+1000;
        formation_adopt(v,ids);return;
    }
    if(!memcmp(ids,g_formation_known,16) || !memcmp(ids,g_formation_sent,16))return;
    if(!formation_same_members(ids,g_formation_known)) {
        dd_log("Formation: party members changed outside a fight; the new party is taken as it stands");
        formation_adopt(v,ids);return;
    }
    dd_action a={0};
    a.kind=DD_ACT_FORMATION_SET;a.u.formation.area=v->area;
    memcpy(a.u.formation.before,g_formation_known,16);memcpy(a.u.formation.after,ids,16);
    if(!dd_action_valid(&a)) { formation_adopt(v,ids);return; }
    dd_log("Formation: order changed here by hand area=%08x before=%u,%u,%u,%u after=%u,%u,%u,%u (role=%s)",v->area,
        a.u.formation.before[0],a.u.formation.before[1],a.u.formation.before[2],a.u.formation.before[3],
        ids[0],ids[1],ids[2],ids[3],dd_role_name(g_cfg.role));
    memcpy(g_formation_sent,ids,16);
    if(g_cfg.role==DD_ROLE_GUEST) {
        g_formation_pending=a;
        g_formation_request=dd_town_guest_local(&g_town,&a);
        if(!g_formation_request) { dd_log("Formation: the order could not be sent; shared actions stop");runtime_sync_stop(1); }
    } else if(!dd_town_host_local(&g_town,&a)) {
        dd_log("Formation: the host could not order its own party change; shared actions stop");runtime_sync_stop(1);
    }
}
static int formation_apply(const dd_action *a) {
    walk_view v; uint32_t ids[4],expected[4],after[4];
    if(!g_formation_native || !g_walk_view(&v) || !formation_ready(&v) ||
       v.area!=a->u.formation.area || !formation_ids(v.raid,ids))return 0;
    if(!memcmp(ids,a->u.formation.after,16))return g_cfg.role==DD_ROLE_GUEST;
    if(memcmp(ids,a->u.formation.before,16) || !formation_default_ids(&v,expected) ||
       memcmp(expected,a->u.formation.after,16)) {
        dd_log("Formation: stale party or different native default; refused (role=%s)",dd_role_name(g_cfg.role));
        return 0;
    }
    g_formation_native((void *)(uintptr_t)v.display);
    if(!formation_ids(v.raid,after) || memcmp(after,a->u.formation.after,16)) {
        dd_log("Formation: native reset did not produce expected IDs; shared actions stop");
        runtime_sync_stop(1);return 0;
    }
    formation_adopt(&v,after);
    dd_log("Formation: default order applied area=%08x ids=%u,%u,%u,%u (role=%s)",v.area,after[0],after[1],after[2],after[3],dd_role_name(g_cfg.role));
    return 1;
}
static void __fastcall hook_formation_reset(void *display) {
    walk_view v;dd_action a={0};
    if(!g_formation_native)return;
    if(g_game_apply_depth || g_cfg.role==DD_ROLE_OFF || !InterlockedCompareExchange(&g_campaign_active,0,0)) {
        g_formation_native(display);return;
    }
    if(runtime_shared_blocked() || runtime_waiting_for_peer() || !walk_shared() ||
       !g_walk_view(&v) || (uintptr_t)display!=v.display || !formation_ready(&v))return;
    a.kind=DD_ACT_FORMATION_RESET;a.u.formation.area=v.area;
    if(!formation_ids(v.raid,a.u.formation.before) || !formation_default_ids(&v,a.u.formation.after))return;
    if(!memcmp(a.u.formation.before,a.u.formation.after,16))return;
    if(!dd_action_valid(&a)) {
        dd_log("Formation: default order has different or unknown members; not shared");
        shop_notice("The default party order could not be verified. Nothing moved; send the logs.");
        return;
    }
    dd_log("Formation: reset requested area=%08x before=%u,%u,%u,%u after=%u,%u,%u,%u (role=%s)",v.area,
        a.u.formation.before[0],a.u.formation.before[1],a.u.formation.before[2],a.u.formation.before[3],
        a.u.formation.after[0],a.u.formation.after[1],a.u.formation.after[2],a.u.formation.after[3],dd_role_name(g_cfg.role));
    if(g_cfg.role==DD_ROLE_GUEST)g_formation_request=dd_town_guest_local(&g_town,&a);
    else (void)dd_town_host_local(&g_town,&a);
}
static void formation_request_done(uint32_t request) {
    if(request && request==g_formation_request) {
        g_formation_request=0;
        memset(&g_formation_pending,0,sizeof g_formation_pending);
    }
}
/* Guest: the host refused our hand-made order. It stands on this copy only, so it is undone here (formation_watch). */
static void formation_request_rejected(uint32_t request) {
    if(request && request==g_formation_request && g_formation_pending.kind==DD_ACT_FORMATION_SET) {
        memcpy(g_formation_revert,g_formation_pending.u.formation.before,16);
        g_formation_revert_due=1;memset(g_formation_sent,0,16);
    }
    formation_request_done(request);
}
static int formation_gate(const dd_msg *m) {
    if((m->type!=DD_MSG_TOWN_REQUEST && m->type!=DD_MSG_TOWN_APPLY) ||
       (m->u.town.action.kind!=DD_ACT_FORMATION_RESET && m->u.town.action.kind!=DD_ACT_FORMATION_SET) ||
       !dd_action_valid(&m->u.town.action))return 0;
    if(g_cfg.role==DD_ROLE_GUEST ? (m->type!=DD_MSG_TOWN_APPLY || m->seq!=g_town.applied_seq+1) : m->type!=DD_MSG_TOWN_REQUEST)return 0;
    walk_view v;uint64_t now=GetTickCount64();
    if(!g_formation_wait)g_formation_wait=now+15000;
    uint32_t ids[4];
    int kept=m->u.town.action.kind==DD_ACT_FORMATION_SET && g_walk_view(&v) && !v.battle &&
        v.area==m->u.town.action.u.formation.area && formation_ids(v.raid,ids) && !memcmp(ids,m->u.town.action.u.formation.after,16);
    if(!kept && (!g_walk_view(&v) || !formation_ready(&v) || v.area!=m->u.town.action.u.formation.area) && now<g_formation_wait)return 1;
    g_formation_wait=0;return 0;
}
static int formation_bind(void) {
    g_formation_default=(formation_default_fn)hamlet_address("formation_default");
    return g_formation_default!=NULL;
}

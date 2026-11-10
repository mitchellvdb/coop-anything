/* Ordered drafts and confirmed stress-building slots. UI is a projection, not
 * authority. Never let native OnDrop dereference a stale/NULL displayed hero. */
#include "dd_activity_reservations.h"
#include "dd_activity_native.inc"
typedef int (__fastcall *activity_drop_fn)(void *,void *,void *);
typedef uintptr_t (__fastcall *activity_destroy_fn)(void *,uint32_t);
static dd_activity_reservations g_activity_res;
static uintptr_t g_activity_widgets[6][3];
static uint32_t g_activity_epoch=1;
/* Exact native weekly caller; UI accept has a different pinned return site. */
static int activity_native_assign_caller(uintptr_t rva) {return rva==0x53cbef;}
static int g_activity_seeded;
static struct {uintptr_t widget;uint32_t hero,revision,epoch;} g_activity_drag;

static void *activity_model_native(const dd_hamlet *s,unsigned kind) {
    return activity_in_town(s->campaign+0x1138,(int)kind);
}
static void *(*g_activity_model)(const dd_hamlet *,unsigned)=activity_model_native;
static int activity_shared(void) {
    return g_cfg.role!=DD_ROLE_OFF && InterlockedCompareExchange(&g_campaign_active,0,0) &&
        InterlockedCompareExchange(&g_session_ready,0,0);
}
static void activity_reset(void) {
    memset(&g_activity_res,0,sizeof g_activity_res);
    memset(g_activity_widgets,0,sizeof g_activity_widgets);
    g_activity_seeded=0;g_activity_epoch=1;memset(&g_activity_drag,0,sizeof g_activity_drag);
}
static int activity_seed(void) {
    if(g_activity_seeded)return 1;
    dd_hamlet s;uint32_t committed[DD_ACTIVITY_COUNT_][3]={{0}};
    if(!g_hamlet_open_model(&s))return 0;
    for(unsigned a=0;a<6;a++) {
        uint64_t activity=(uintptr_t)g_activity_model(&s,a);
        if(!activity)continue; /* A building may not yet exist in this campaign. */
        if(!activity_native_stress(&s,activity,a))return 0;
        for(unsigned i=0;i<3;i++) {
            uint64_t p=0;
            if(!activity_native_slot(&s,activity,i,&p))continue;
            if(!activity_native_read(&s,p,&committed[a][i],4))return 0;
        }
    }
    if(!dd_activity_reservations_seed(&g_activity_res,committed))return 0;
    g_activity_seeded=1;return 1;
}
static int activity_widget(void *widget,unsigned *kind,unsigned *slot) {
    uint64_t vt=0,secondary=0,activity=0;uint32_t rank=3;
    if(!safe_read((uintptr_t)widget,&vt,8) || vt!=image_base()+0xee0500 ||
       !safe_read((uintptr_t)widget+0x10,&secondary,8) || secondary!=image_base()+0xee04c0 ||
       !safe_read((uintptr_t)widget+0x230,&rank,4) || rank>=3 ||
       !safe_read((uintptr_t)widget+0x280,&activity,8))return 0;
    int a=g_activity_kind((void *)(uintptr_t)activity);
    dd_hamlet s;
    if(a<0 || a>=6 || !g_hamlet_open_model(&s) ||
       (uintptr_t)g_activity_model(&s,(unsigned)a)!=activity)return 0;
    *kind=(unsigned)a;*slot=rank;return 1;
}
static int activity_project_one(uintptr_t widget,unsigned kind,unsigned slot) {
    unsigned a=0,i=0;dd_hamlet s;dd_hamlet_hero hero;uint64_t pointer=0;
    uint8_t locked=1,visible=0,enabled=0;
    if(!widget || !activity_widget((void *)widget,&a,&i) || a!=kind || i!=slot)return 0;
    const dd_activity_res_slot *value=&g_activity_res.slots[kind][slot];
    if(value->hero) {
        if(!g_hamlet_open_model(&s) || dd_hamlet_find(&s,value->hero,&hero)!=1)return 0;
        pointer=hero.hero;
    }
    if(!safe_read(widget+0x235,&locked,1))return 0;
    visible=(uint8_t)(pointer && !locked);enabled=(uint8_t)!locked;
    /* The verified setter uses exactly these UI fields. Calling its NULL
     * branch to refresh a picture would remove a real paid treatment. */
    return WriteProcessMemory(GetCurrentProcess(),(void *)(widget+0x28),&pointer,8,NULL) &&
        WriteProcessMemory(GetCurrentProcess(),(void *)(widget+0xc),&visible,1,NULL) &&
        WriteProcessMemory(GetCurrentProcess(),(void *)(widget+0x20),&enabled,1,NULL);
}
static void activity_refresh(void) {
    if(!g_activity_seeded)return;
    for(unsigned a=0;a<6;a++)for(unsigned i=0;i<3;i++) {
        uintptr_t w=g_activity_widgets[a][i];unsigned kind=0,slot=0;
        if(!w)continue;
        if(!activity_widget((void *)w,&kind,&slot) || kind!=a || slot!=i) {
            g_activity_widgets[a][i]=0;continue;
        }
        if(!activity_project_one(w,a,i))runtime_sync_stop(3);
    }
}
static void activity_register(void *widget) {
    unsigned a=0,i=0;
    if(!activity_widget(widget,&a,&i))return;
    g_activity_widgets[a][i]=(uintptr_t)widget;
    if(activity_shared() && activity_seed())activity_project_one((uintptr_t)widget,a,i);
}
static void activity_forget(void *widget) {
    if(g_activity_drag.widget==(uintptr_t)widget)memset(&g_activity_drag,0,sizeof g_activity_drag);
    for(unsigned a=0;a<6;a++)for(unsigned i=0;i<3;i++)
        if(g_activity_widgets[a][i]==(uintptr_t)widget)g_activity_widgets[a][i]=0;
}
static void activity_new_epoch(uint32_t epoch) {
    if(epoch<=g_activity_epoch)return;
    g_activity_epoch=epoch;g_activity_seeded=0;memset(&g_activity_drag,0,sizeof g_activity_drag);
    memset(&g_activity_res,0,sizeof g_activity_res);
    if(activity_seed())activity_refresh();
}
static int activity_has_draft(uint32_t id) {
    if(!g_activity_seeded || !id)return 0;
    for(unsigned a=0;a<6;a++)for(unsigned i=0;i<3;i++)
        if(g_activity_res.slots[a][i].hero==id && g_activity_res.slots[a][i].phase==DD_ACTIVITY_RES_DRAFT)return 1;
    return 0;
}
static int activity_release_drafts(void) {
    if(!g_activity_seeded)return 1;
    if(!dd_activity_reservations_release_drafts(&g_activity_res))return 0;
    memset(&g_activity_drag,0,sizeof g_activity_drag);
    activity_refresh();return !g_town.needs_resync;
}
static int activity_commit(void *unused,unsigned kind,unsigned slot,uint32_t id) {
    (void)unused;dd_hamlet s;dd_hamlet_hero hero,after;
    uint64_t act=0,address=0,association=0;uint32_t old=0,actual=0,state=0;
    uint64_t span[2],after_span[2];size_t n=0,after_n=0;
    uint8_t before[ACTIVITY_NATIVE_WALLET_BYTES],wallet[ACTIVITY_NATIVE_WALLET_BYTES],result[DD_RESULT_BYTES]={0};
    if(!g_original_assign || !g_hamlet_open_model(&s) || dd_hamlet_find(&s,id,&hero)!=1 || hero.location>1 ||
       !(act=(uintptr_t)g_activity_model(&s,kind)) || !activity_native_stress(&s,act,kind) ||
       !activity_native_slot(&s,act,slot,&address) || !activity_native_read(&s,address,&old,4) || old ||
       !activity_native_wallet(&s,span,before,&n))return 0;
    int rank=(int)slot;
    if(!WriteProcessMemory(GetCurrentProcess(),(void *)(uintptr_t)(act+DD_ACTIVITY_SLOT_OFFSET),&rank,4,NULL))return 0;
    g_original_assign((void *)(uintptr_t)act,result,id);
    if(!result[0])return 0;
    if(!activity_native_read(&s,address,&actual,4) || actual!=id ||
       !activity_native_read(&s,address+4,&state,4) || state!=1 ||
       dd_hamlet_find(&s,id,&after)!=1 || after.hero!=hero.hero || after.location!=2 ||
       !activity_native_read(&s,after.entry+0x15c0,&association,8) || association!=act ||
       !activity_native_wallet(&s,after_span,wallet,&after_n) || after_n!=n || memcmp(span,after_span,sizeof span)) {
        runtime_sync_stop(3);return 0;
    }
    for(size_t i=0;i<n;i+=0x48) {
        uint32_t type=0;int32_t old_amount=0,new_amount=0;
        memcpy(&type,before+i+0x44,4);memcpy(&old_amount,before+i,4);memcpy(&new_amount,wallet+i,4);
        if(memcmp(before+i+4,wallet+i+4,0x44) || new_amount<0 ||
           (type==DD_SUPPLIES_GOLD ? new_amount>old_amount : new_amount!=old_amount)) {
            runtime_sync_stop(3);return 0;
        }
    }
    return 1;
}
static int activity_release_confirmed(void *unused,unsigned kind,unsigned slot,uint32_t id) {
    (void)unused;dd_hamlet s;int mutated=0;
    if(!g_hamlet_open_model(&s))return 0;
    uint64_t act=(uintptr_t)g_activity_model(&s,kind);
    int ok=activity_native_release(&s,act,kind,slot,id,g_activity_set,&mutated);
    if(!ok && mutated)runtime_sync_stop(3);
    return ok;
}
static int activity_apply(const dd_action *a) {
    if(a->u.activity.activity>=6 || a->u.activity.epoch!=g_activity_epoch || !activity_seed())return 0;
    dd_activity_res_command c={a->u.activity.op,a->u.activity.activity,a->u.activity.slot,0,
        a->u.activity.hero_id,a->u.activity.revision};
    dd_activity_res_result check=dd_activity_reservations_check(&g_activity_res,&c);
    if(check!=DD_ACTIVITY_RES_OK && check!=DD_ACTIVITY_RES_UNCHANGED)return 0;
    if(c.op==DD_ACTIVITY_RESERVE) {
        dd_hamlet s;dd_hamlet_hero hero;uint64_t act=0,p=0;uint32_t committed=0;
        if(!g_hamlet_open_model(&s) || dd_hamlet_find(&s,c.hero,&hero)!=1 || hero.location>1 ||
           !(act=(uintptr_t)g_activity_model(&s,c.activity)) || !activity_native_stress(&s,act,c.activity) ||
           !activity_native_slot(&s,act,c.slot,&p) || !activity_native_read(&s,p,&committed,4) || committed ||
           !g_activity_blocked || !g_activity_slot_blocked || g_activity_blocked((void *)(uintptr_t)act) ||
           g_activity_slot_blocked((void *)(uintptr_t)act,c.slot))return 0;
    }
    dd_activity_res_result result=dd_activity_reservations_apply(&g_activity_res,&c,
        c.op==DD_ACTIVITY_RELEASE?activity_release_confirmed:activity_commit,NULL);
    if(result!=DD_ACTIVITY_RES_OK && result!=DD_ACTIVITY_RES_UNCHANGED)return 0;
    dd_log("Activity shared: op=%u activity=%u slot=%u hero=%u revision=%u epoch=%u",c.op,c.activity,c.slot,c.hero,
        g_activity_res.slots[c.activity][c.slot].revision,g_activity_epoch);
    activity_refresh();return !g_town.needs_resync;
}
static int activity_submit(unsigned op,unsigned kind,unsigned slot,uint32_t id) {
    if(!activity_shared() || runtime_shared_blocked() || !activity_seed() || kind>=6 || slot>=3)return 0;
    dd_action a={0};a.kind=DD_ACT_ACTIVITY;a.u.activity.op=(uint8_t)op;
    a.u.activity.activity=(uint8_t)kind;a.u.activity.slot=(uint8_t)slot;
    a.u.activity.hero_id=id;a.u.activity.revision=g_activity_res.slots[kind][slot].revision;
    a.u.activity.epoch=g_activity_epoch;
    dd_activity_res_command c={(uint8_t)op,(uint8_t)kind,(uint8_t)slot,0,id,a.u.activity.revision};
    dd_activity_res_result valid=dd_activity_reservations_check(&g_activity_res,&c);
    if(valid!=DD_ACTIVITY_RES_OK && valid!=DD_ACTIVITY_RES_UNCHANGED) {
        shop_notice("This activity slot or hero is already reserved. Use the shared X to clear it before choosing another hero.");
        activity_refresh();return 0;
    }
    if(g_cfg.role==DD_ROLE_GUEST)return dd_town_guest_local(&g_town,&a)!=0;
    return dd_town_host_local(&g_town,&a);
}
static uintptr_t activity_accept_input(void *activity,void *result,uint32_t id) {
    int kind=g_activity_kind(activity),slot=-1;uint8_t answer[DD_RESULT_BYTES]={0};
    int accepted=kind>=0 && kind<6 && safe_read((uintptr_t)activity+DD_ACTIVITY_SLOT_OFFSET,&slot,4) &&
        slot>=0 && slot<3 && activity_submit(DD_ACTIVITY_ACCEPT,(unsigned)kind,(unsigned)slot,id);
    answer[0]=(uint8_t)(accepted && g_cfg.role==DD_ROLE_HOST);
    if(result)WriteProcessMemory(GetCurrentProcess(),result,answer,sizeof answer,NULL);
    return (uintptr_t)result;
}
static int activity_user_clear(uintptr_t caller) {return caller==image_base()+0x68c373;}
static int (*g_activity_user_clear)(uintptr_t)=activity_user_clear;
static int activity_coop(void) {return g_cfg.role!=DD_ROLE_OFF && InterlockedCompareExchange(&g_campaign_active,0,0);}
static int activity_other_kind(void *widget) {
    uint64_t activity=0;
    if(!safe_read((uintptr_t)widget+0x280,&activity,8))return 0;
    int kind=g_activity_kind((void *)(uintptr_t)activity);
    return kind>=6 && kind<DD_ACTIVITY_COUNT_;
}
static void __fastcall hook_activity_set(void *widget,void *hero) {
    if(!g_activity_set)return;
    unsigned kind=0,slot=0;
    if(g_activity_constructing || !activity_coop()) {g_activity_set(widget,hero);return;}
    if(!activity_widget(widget,&kind,&slot)) {
        if(activity_other_kind(widget) && !runtime_shared_blocked())g_activity_set(widget,hero);
        return; /* A stale stress widget must never enter the original setter. */
    }
    if(!activity_shared() || runtime_shared_blocked()) {activity_refresh();return;}
    if(g_game_apply_depth) {if(activity_seed())activity_project_one((uintptr_t)widget,kind,slot);return;}
    if(!activity_seed())return;
    if(!hero) {
#ifdef _MSC_VER
        uintptr_t caller=(uintptr_t)_ReturnAddress();
#else
        uintptr_t caller=(uintptr_t)__builtin_return_address(0);
#endif
        /* Native panel cleanup is not a user cancellation. The normal red-X
         * callback calls SetHero before its roster refresh, without model writes. */
        if(g_activity_user_clear(caller)) {
            const dd_activity_res_slot *value=&g_activity_res.slots[kind][slot];
            if(value->hero)activity_submit(value->phase==DD_ACTIVITY_RES_COMMITTED?DD_ACTIVITY_RELEASE:DD_ACTIVITY_CLEAR,
                kind,slot,value->hero);
        }
    }
    if(hero && g_activity_drag.widget==(uintptr_t)widget)memset(&g_activity_drag,0,sizeof g_activity_drag);
    activity_project_one((uintptr_t)widget,kind,slot);
}
static int __fastcall hook_activity_drop(void *secondary,void *position,void *drag) {
    if(!g_activity_drop)return 0;
    void *widget=(void *)((uintptr_t)secondary-0x10);unsigned kind=0,slot=0;
    if(!activity_coop())return g_activity_drop(secondary,position,drag);
    if(!activity_widget(widget,&kind,&slot)) {
        if(activity_other_kind(widget) && !runtime_shared_blocked() && !runtime_waiting_for_peer())
            return g_activity_drop(secondary,position,drag);
        return 0;
    }
    if(!activity_shared() || runtime_shared_blocked() || runtime_waiting_for_peer() || !activity_seed())return 0;
    uint64_t type=0,hero=0;dd_hamlet s;dd_hamlet_hero entry;
    if(safe_read((uintptr_t)drag,&type,8) && type==image_base()+0xeddca0 &&
       safe_read((uintptr_t)drag+8,&hero,8) && g_hamlet_open_model(&s) &&
       dd_hamlet_hero_pointer(&s,hero,&entry))activity_submit(DD_ACTIVITY_RESERVE,kind,slot,entry.id);
    activity_project_one((uintptr_t)widget,kind,slot);
    return 0; /* No optimistic native mutation; native source drag is cancelled. */
}
static uintptr_t __fastcall hook_activity_destroy(void *widget,uint32_t flags) {
    activity_forget(widget);
    return g_activity_destroy?g_activity_destroy(widget,flags):0;
}

/* Stamp the state at drag start. A delayed finish may never clear a newer
 * reservation merely because it occupies the same widget now. */
static void __fastcall hook_activity_pickup(void *widget,void *a,void *b,void *data) {
    unsigned kind=0,slot=0;memset(&g_activity_drag,0,sizeof g_activity_drag);
    if(!activity_coop()) {if(g_activity_pickup)g_activity_pickup(widget,a,b,data);return;}
    if(!activity_widget(widget,&kind,&slot)) {
        if(activity_other_kind(widget) && !runtime_shared_blocked() && g_activity_pickup)g_activity_pickup(widget,a,b,data);
        return;
    }
    if(!activity_shared() || runtime_shared_blocked() || !activity_seed())return;
    const dd_activity_res_slot *value=&g_activity_res.slots[kind][slot];
    if(!value->hero || !activity_project_one((uintptr_t)widget,kind,slot))return;
    g_activity_drag.widget=(uintptr_t)widget;g_activity_drag.hero=value->hero;
    g_activity_drag.revision=value->revision;g_activity_drag.epoch=g_activity_epoch;
    if(g_activity_pickup)g_activity_pickup(widget,a,b,data);
}
static void activity_source_done(void *widget,void *drag,int finished) {
    unsigned kind=0,slot=0;
    if(!activity_coop() || (activity_other_kind(widget) && !runtime_shared_blocked())) {
        if(finished) {if(g_activity_source_finish)g_activity_source_finish(widget,drag);}
        else if(g_activity_source_remove)g_activity_source_remove(widget,drag);
        return;
    }
    if(!activity_widget(widget,&kind,&slot))return;
    if(!activity_shared())return;
    if(activity_seed() && !g_game_apply_depth && !runtime_shared_blocked()) {
        const dd_activity_res_slot *value=&g_activity_res.slots[kind][slot];
        if(value->hero && g_activity_drag.widget==(uintptr_t)widget &&
           g_activity_drag.hero==value->hero && g_activity_drag.revision==value->revision &&
           g_activity_drag.epoch==g_activity_epoch) {
            memset(&g_activity_drag,0,sizeof g_activity_drag);
            activity_submit(value->phase==DD_ACTIVITY_RES_COMMITTED?DD_ACTIVITY_RELEASE:DD_ACTIVITY_CLEAR,
                kind,slot,value->hero);
        }
    }
    if(g_activity_drag.widget==(uintptr_t)widget)memset(&g_activity_drag,0,sizeof g_activity_drag);
    uint8_t done=0;
    WriteProcessMemory(GetCurrentProcess(),(uint8_t *)widget+0x28d,&done,1,NULL);
    activity_project_one((uintptr_t)widget,kind,slot);
}
static void __fastcall hook_activity_source_finish(void *widget,void *drag) {activity_source_done(widget,drag,1);}
static void __fastcall hook_activity_source_remove(void *widget,void *drag) {activity_source_done(widget,drag,0);}

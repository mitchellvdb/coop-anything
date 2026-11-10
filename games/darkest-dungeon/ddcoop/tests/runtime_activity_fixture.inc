/* Actual activity bridge/FIFO/codec with modeled native effects. No game,
 * Steam or owner saves; fake world uses the existing bounded Hamlet reader. */
static uint8_t ar_activity[6][0x100], ar_records[6][3][24], ar_widgets[6][3][0x2d0];
static unsigned ar_commits,ar_clears,ar_unsafe,ar_destroyed;
static int ar_refuse,ar_partial,ar_locked;
static uint8_t __fastcall ar_blocked(void *a){(void)a;return (uint8_t)ar_locked;}
static uint8_t __fastcall ar_slot_blocked(void *a,uint32_t slot){(void)a;(void)slot;return 0;}
static void *ar_model(const dd_hamlet *s,unsigned kind) {(void)s;return kind<6?ar_activity[kind]:NULL;}
static int ar_kind(void *p) {for(int i=0;i<6;i++)if(p==ar_activity[i])return i;return -1;}
static uintptr_t __fastcall ar_assign(void *p,void *result,uint32_t id) {
    int kind=ar_kind(p);uint32_t slot=3;
    memcpy(&slot,(uint8_t *)p+0x90,4);CHECK(kind>=0 && slot<3 && id>=1 && id<=3);
    if(kind<0 || slot>=3 || !id || id>3)return 0;
    memset(result,0,DD_RESULT_BYTES);
    if(ar_refuse)return (uintptr_t)result;
    ar_commits++;
    putint(ar_records[kind][slot],ar_partial?id+1:id);putint(ar_records[kind][slot]+4,1);
    ar_records[kind][slot][0x14]=1;
    putint(hm_entries[id-1]+0x15b0,2);putptr(hm_entries[id-1]+0x15c0,(uintptr_t)p);
    int32_t gold=0;memcpy(&gold,hm_wallet,4);gold-=5;memcpy(hm_wallet,&gold,4);
    ((uint8_t *)result)[0]=1;return (uintptr_t)result;
}
static void __fastcall ar_set(void *capture,void *hero) {
    uint64_t act=0,selected=0;uint32_t slot=3,id=0;
    memcpy(&act,(uint8_t *)capture+0x280,8);memcpy(&selected,(uint8_t *)capture+0x28,8);
    memcpy(&slot,(uint8_t *)capture+0x230,4);
    CHECK(!hero && !*(uint64_t *)capture && selected && slot<3);
    if(hero || !selected || slot>=3)return;
    memcpy(&id,(void *)(uintptr_t)(selected+0x137c),4);
    int kind=ar_kind((void *)(uintptr_t)act);CHECK(kind>=0 && id>=1 && id<=3);
    if(kind<0 || !id || id>3)return;
    ar_clears++;memset(ar_records[kind][slot],0,8);ar_records[kind][slot][0x14]=0;
    putint(hm_entries[id-1]+0x15b0,0);putptr(hm_entries[id-1]+0x15c0,0);
    putptr((uint8_t *)capture+0x28,0);((uint8_t *)capture)[0xc]=0;((uint8_t *)capture)[0x20]=1;
}
static int __fastcall ar_drop_bad(void *a,void *b,void *c) {(void)a;(void)b;(void)c;ar_unsafe++;return 1;}
static void __fastcall ar_pickup(void *w,void *a,void *b,void *d){(void)w;(void)a;(void)b;(void)d;}
static void __fastcall ar_source_bad(void *a,void *b) {(void)a;(void)b;ar_unsafe++;}
static uintptr_t __fastcall ar_destroy(void *w,uint32_t flags) {(void)flags;ar_destroyed++;return (uintptr_t)w;}
static int ar_clear_user(uintptr_t caller) {(void)caller;return 1;}
static int ar_clear_auto(uintptr_t caller) {(void)caller;return 0;}
static void ar_widget(unsigned a,unsigned slot) {
    uint8_t *w=ar_widgets[a][slot];memset(w,0,0x2d0);
    putptr(w,image_base()+0xee0500);putptr(w+0x10,image_base()+0xee04c0);
    putptr(w+0x280,(uintptr_t)ar_activity[a]);putint(w+0x230,slot);
    activity_register(w);
}
static void ar_reset(int role,int open) {
    hm_reset(role);activity_reset();g_activity_kind=ar_kind;g_activity_model=ar_model;
    memset(ar_activity,0,sizeof ar_activity);memset(ar_records,0,sizeof ar_records);memset(ar_widgets,0,sizeof ar_widgets);
    ar_commits=ar_clears=ar_unsafe=ar_destroyed=0;ar_refuse=ar_partial=ar_locked=0;
    g_activity_blocked=ar_blocked;g_activity_slot_blocked=ar_slot_blocked;
    g_activity_set=ar_set;g_activity_drop=ar_drop_bad;g_activity_destroy=ar_destroy;
    g_activity_source_finish=g_activity_source_remove=ar_source_bad;g_activity_pickup=ar_pickup;
    g_activity_user_clear=ar_clear_user;g_original_assign=ar_assign;
    static const uint32_t vt[6]={0xe9d728,0xe9d618,0xe9d6a0,0xe9aea8,0xe9ad28,0xe9adb0};
    uint64_t base=(uintptr_t)sale_image;
    sale_image[0x2340a0]=0xc2;sale_image[0x2340a1]=sale_image[0x2340a2]=0;
    putint(hm_wallet+0x44,DD_SUPPLIES_GOLD);
    for(unsigned a=0;a<6;a++) {
        putptr(ar_activity[a],base+vt[a]);putptr(sale_image+vt[a]+0x38,base+0x2340a0);
        putptr(ar_activity[a]+0x78,(uintptr_t)ar_records[a]);
        putptr(ar_activity[a]+0x80,(uintptr_t)(ar_records[a]+3));
        if(open)for(unsigned i=0;i<3;i++)ar_widget(a,i);
    }
}
static dd_msg ar_wire(dd_msg m) {
    uint8_t b[DD_PROTO_HEADER+DD_PROTO_MAX_BODY];dd_msg out={0};size_t used=0,n=dd_encode(&m,b,sizeof b);
    CHECK(n && dd_decode(b,n,&out,&used)==DD_DECODE_OK && used==n);return out;
}
static void ar_receive(dd_msg m) {m=ar_wire(m);CHECK(queue_push(&g_incoming,&m));dd_runtime_poll();}
static dd_action ar_action(unsigned op,unsigned a,unsigned slot,uint32_t hero,uint32_t revision) {
    dd_action command={0};command.kind=DD_ACT_ACTIVITY;command.u.activity.op=(uint8_t)op;
    command.u.activity.activity=(uint8_t)a;command.u.activity.slot=(uint8_t)slot;
    command.u.activity.hero_id=hero;command.u.activity.revision=revision;command.u.activity.epoch=1;return command;
}
static uint64_t ar_shown(unsigned a,unsigned slot) {uint64_t p=0;memcpy(&p,ar_widgets[a][slot]+0x28,8);return p;}
static void activity_bridge_checks(void) {
    int (*old_kind)(void *)=g_activity_kind;
    void *(*old_model)(const dd_hamlet *,unsigned)=g_activity_model;
    assign_hero_fn old_assign=g_original_assign;
    for(unsigned initiator=0;initiator<4;initiator++) {
        dd_msg reserve,accept,release;
        if(initiator) {
            ar_reset(DD_ROLE_GUEST,1);g_cfg.player=(int)initiator;
            g_town.next_request=((uint32_t)initiator<<24)|1;
            uint64_t drag[3]={image_base()+0xeddca0,(uintptr_t)(hm_entries[0]+8),(uintptr_t)hm_entries[0]};
            CHECK(!hook_activity_drop(ar_widgets[0][0]+0x10,NULL,drag));
            CHECK(!ar_shown(0,0) && !ar_commits && !ar_clears && sent==1 && last_message.type==DD_MSG_TOWN_REQUEST);
            reserve=last_message;
        } else {memset(&reserve,0,sizeof reserve);reserve.type=DD_MSG_TOWN_REQUEST;reserve.u.town.request_id=1;reserve.u.town.action=ar_action(1,0,0,1,1);}
        /* Host never opened the tavern; no widget or human click is needed. */
        ar_reset(DD_ROLE_HOST,0);ar_receive(reserve);
        CHECK(g_town.applied_seq==1 && !ar_commits && !ar_clears && !ar_unsafe && g_activity_res.slots[0][0].hero==1);
        reserve=last_message;CHECK(reserve.type==DD_MSG_TOWN_APPLY);
        dd_msg collision={0};collision.type=DD_MSG_TOWN_REQUEST;collision.u.town.request_id=0x02000002u;
        collision.u.town.action=ar_action(1,0,0,2,1);ar_receive(collision);
        CHECK(last_message.type==DD_MSG_TOWN_REJECT && g_town.applied_seq==1 && g_activity_res.slots[0][0].hero==1);
        ar_widget(0,0);CHECK(ar_shown(0,0)==(uintptr_t)(hm_entries[0]+8));
        dd_msg request={0};request.type=DD_MSG_TOWN_REQUEST;request.u.town.request_id=(initiator<<24)|3;
        request.u.town.action=ar_action(3,0,0,1,2);ar_receive(request);accept=last_message;
        CHECK(accept.type==DD_MSG_TOWN_APPLY && accept.seq==2 && ar_commits==1 && !ar_clears && !ar_unsafe);
        /* Native model occupied + stale NULL picture reproduced the owner's
         *7e20ac crash. New drop guard never invokes the unsafe native path. */
        putptr(ar_widgets[0][0]+0x28,0);
        uint64_t drag[3]={image_base()+0xeddca0,(uintptr_t)(hm_entries[1]+8),(uintptr_t)hm_entries[1]};
        CHECK(!hook_activity_drop(ar_widgets[0][0]+0x10,NULL,drag));
        CHECK(!ar_unsafe && ar_shown(0,0)==(uintptr_t)(hm_entries[0]+8));
        ar_receive(request);CHECK(ar_commits==1 && last_message.type==DD_MSG_TOWN_REJECT);
        request.u.town.request_id=0x03000004u;request.u.town.action=ar_action(4,0,0,1,3);
        ar_receive(request);release=last_message;
        CHECK(release.type==DD_MSG_TOWN_APPLY && release.seq==3 && ar_clears==1 && !ar_shown(0,0));
        CHECK(!ar_unsafe && !g_town.needs_resync);
        for(unsigned receiver=1;receiver<=3;receiver++) {
            ar_reset(DD_ROLE_GUEST,receiver!=2);g_cfg.player=(int)receiver;
            ar_receive(reserve);CHECK(!g_town.needs_resync && !ar_commits && !ar_clears);
            if(receiver==2)ar_widget(0,0); /* first open after another player's drop */
            CHECK(ar_shown(0,0)==(uintptr_t)(hm_entries[0]+8));
            unsigned before=sent;CHECK(!hook_activity_drop(ar_widgets[0][0]+0x10,NULL,drag));
            CHECK((unsigned)sent==before && !ar_unsafe && ar_shown(0,0)==(uintptr_t)(hm_entries[0]+8));
            ar_receive(accept);CHECK(ar_commits==1 && !g_town.needs_resync);
            ar_receive(release);CHECK(ar_clears==1 && !ar_shown(0,0) && !g_town.needs_resync);
            CHECK(g_activity_res.slots[0][0].revision==4 && !ar_unsafe);
        }
    }
    /* Explicit X clears a draft for everyone; internal panel cleanup doesn't. */
    ar_reset(DD_ROLE_HOST,1);CHECK(activity_submit(1,1,0,2));
    g_activity_user_clear=ar_clear_auto;hook_activity_set(ar_widgets[1][0],NULL);
    CHECK(g_activity_res.slots[1][0].hero==2 && ar_shown(1,0));
    g_activity_user_clear=ar_clear_user;hook_activity_set(ar_widgets[1][0],NULL);
    CHECK(!g_activity_res.slots[1][0].hero && !ar_shown(1,0));
    /* Source-finish and discarded drag both go through the ordered operation. */
    for(unsigned finish=0;finish<2;finish++) {
        ar_reset(DD_ROLE_HOST,1);CHECK(activity_submit(1,2,1,3));
        hook_activity_pickup(ar_widgets[2][1],NULL,NULL,NULL);
        if(finish)hook_activity_source_finish(ar_widgets[2][1],NULL);
        else hook_activity_source_remove(ar_widgets[2][1],NULL);
        CHECK(!g_activity_res.slots[2][1].hero && !ar_shown(2,1) && !ar_unsafe);
    }
    /* A stale drag end must not clear a different/newer hero in this slot. */
    ar_reset(DD_ROLE_HOST,1);CHECK(activity_submit(1,0,0,1));
    hook_activity_pickup(ar_widgets[0][0],NULL,NULL,NULL);
    CHECK(activity_submit(2,0,0,1) && activity_submit(1,0,0,2));
    hook_activity_source_remove(ar_widgets[0][0],NULL);
    CHECK(g_activity_res.slots[0][0].hero==2 && ar_shown(0,0)==(uintptr_t)(hm_entries[1]+8) && !ar_unsafe);
    ar_reset(DD_ROLE_HOST,1);CHECK(activity_submit(1,0,0,1));
    uint32_t old_revision=g_activity_res.slots[0][0].revision;
    CHECK(activity_release_drafts() && !ar_shown(0,0) && g_activity_res.slots[0][0].revision>old_revision);
    CHECK(!activity_apply(&(dd_action){.kind=DD_ACT_ACTIVITY,.u.activity={3,0,0,1,old_revision,1}}));
    /* Destroy unregisters and a new widget projects the still-shared draft. */
    CHECK(activity_submit(1,0,0,1));hook_activity_destroy(ar_widgets[0][0],0);
    CHECK(ar_destroyed==1 && !g_activity_widgets[0][0]);ar_widget(0,0);CHECK(ar_shown(0,0));
    activity_new_epoch(100);
    CHECK(!activity_apply(&(dd_action){.kind=DD_ACT_ACTIVITY,.u.activity={1,0,0,2,1,1}}));
    /* No local effects after STOP or before connection, even from source callbacks. */
    ar_reset(DD_ROLE_HOST,1);g_town.needs_resync=1;
    uint64_t drag[3]={image_base()+0xeddca0,(uintptr_t)(hm_entries[0]+8),(uintptr_t)hm_entries[0]};
    CHECK(!hook_activity_drop(ar_widgets[0][0]+0x10,NULL,drag));hook_activity_source_remove(ar_widgets[0][0],drag);
    CHECK(!ar_unsafe && !ar_commits && !ar_clears);
    ar_reset(DD_ROLE_HOST,0);ar_locked=1;CHECK(!activity_submit(1,0,0,1) && !ar_commits && !g_activity_res.slots[0][0].hero);
    /* A partial native success cannot be ACKed or repeated. */
    ar_reset(DD_ROLE_HOST,1);CHECK(activity_submit(1,0,0,1));ar_partial=1;
    CHECK(!activity_submit(3,0,0,1) && g_town.needs_resync && ar_commits==1);
    activity_reset();g_activity_kind=old_kind;g_activity_model=old_model;g_original_assign=old_assign;
    g_activity_user_clear=activity_user_clear;
    puts("Activity bridge: all4 initiators/3 guests, closed host panel, live/reopened widgets, first-wins slot/hero identity, commit once, confirmed clear, source cleanup and observed NULL-widget drop guard; epochs/navigation/STOP/partial effects verified");
}

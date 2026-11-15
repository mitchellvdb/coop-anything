/* Normal confirmed Unequip All -> Roster58ce70(campaign, estate). The dialog is local;
 * only its model effect is ordered. Other players need no RealmInventory panel open.
 * A logical pre/post digest protects concurrent swaps, duplicates, dead heroes and item conservation. */
typedef void(__fastcall *trinket_bulk_model_fn)(void *,void *);
static trinket_bulk_model_fn g_trinket_bulk_model;
static int hamlet_bulk_in_raid(void);
static int trinket_bulk_caller(uintptr_t rva) {return rva==0x6ae19f;}
static int (*g_trinket_bulk_caller)(uintptr_t)=trinket_bulk_caller;
static void trinket_bulk_word(uint64_t *h,uint64_t v) {
    for(unsigned i=0;i<8;i++){*h^=(uint8_t)(v>>(8*i));*h*=UINT64_C(1099511628211);}
}
static int trinket_key_compare(const void *left,const void *right) {
    uint64_t a=*(const uint64_t *)left,b=*(const uint64_t *)right;return a>b?1:a<b?-1:0;
}
static int trinket_bulk_state(const dd_hamlet *s,uint64_t *before,uint64_t *after,unsigned *moved) {
    uint64_t rb=0,re=0,estate=0;unsigned estate_slots=0,owned=0;
    uint32_t ids[DD_HAMLET_HEROES]={0},normal=0;
    uint64_t pre=UINT64_C(14695981039346656037),post=pre;
    *before=*after=0;*moved=0;
    if(!hamlet_read(s,s->campaign+0x20,&rb,8) || !hamlet_read(s,s->campaign+0x28,&re,8) ||
       re<rb || (re-rb)%8 || (re-rb)/8>DD_HAMLET_HEROES ||
       !hamlet_read(s,s->image+0x2a6f868,&normal,4) || !normal ||
       !dd_hamlet_inventory(s,0,&estate,&estate_slots))return 0;
    unsigned heroes=(unsigned)((re-rb)/8);
    for(unsigned i=0;i<heroes;i++) {
        uint64_t entry=0;if(!hamlet_read(s,rb+i*8,&entry,8) || !entry || !hamlet_read(s,entry,&ids[i],4) ||
            !ids[i] || !DD_TRINKET_IS_HERO(ids[i]))return 0;
        for(unsigned j=i;j && ids[j]<ids[j-1];j--){uint32_t id=ids[j];ids[j]=ids[j-1];ids[j-1]=id;}
    }
    uint64_t *keys=calloc(9999u+2u*DD_HAMLET_HEROES,sizeof *keys);
    if(!keys)return 0;
    int ok=0;
    for(unsigned i=0;i<estate_slots;i++) {
        uint32_t t=0,id=0,q=0;if(!dd_hamlet_item(s,estate,i,&t,&id,&q))goto done;
        if(!q)continue;
        if(q!=1 || t!=normal)goto done;
        keys[owned++]=((uint64_t)t<<32)|id;
    }
    qsort(keys,owned,sizeof *keys,trinket_key_compare);
    trinket_bulk_word(&pre,1);trinket_bulk_word(&post,1);
    trinket_bulk_word(&pre,heroes);trinket_bulk_word(&post,heroes);
    unsigned total=owned;
    for(unsigned i=0;i<heroes;i++) {
        dd_hamlet_hero h;uint64_t inv=0;unsigned count=0;
        if((i && ids[i]==ids[i-1]) || dd_hamlet_find(s,ids[i],&h)!=1 ||
           !dd_hamlet_inventory(s,ids[i],&inv,&count))goto done;
        trinket_bulk_word(&pre,h.id);trinket_bulk_word(&post,h.id);
        trinket_bulk_word(&pre,h.location);trinket_bulk_word(&post,h.location);
        trinket_bulk_word(&pre,count);trinket_bulk_word(&post,count);
        for(unsigned j=0;j<count;j++) {
            uint32_t t=0,id=0,q=0;if(!dd_hamlet_item(s,inv,j,&t,&id,&q) || (q && (q!=1 || t!=normal)))goto done;
            trinket_bulk_word(&pre,q);trinket_bulk_word(&pre,t);trinket_bulk_word(&pre,id);
            int keep=h.location==3; /* Native helper skips graveyard entries, all other locations are included. */
            trinket_bulk_word(&post,keep?q:0);trinket_bulk_word(&post,keep?t:0);trinket_bulk_word(&post,keep?id:0);
            if(q && !keep){keys[total++]=((uint64_t)t<<32)|id;(*moved)++;}
        }
    }
    trinket_bulk_word(&pre,owned);
    for(unsigned i=0;i<owned;i++)trinket_bulk_word(&pre,keys[i]);
    if(total>9999)goto done;
    qsort(keys,total,sizeof *keys,trinket_key_compare);
    trinket_bulk_word(&post,total);
    for(unsigned i=0;i<total;i++)trinket_bulk_word(&post,keys[i]);
    *before=pre?pre:UINT64_MAX;*after=post?post:UINT64_MAX;ok=1;
done:
    free(keys);return ok;
}
static int trinket_bulk_apply(const dd_hamlet *s,const dd_action *a) {
    uint64_t before=0,expected=0,actual=0,unused=0,panel=0;unsigned count=0,left=0;
    if(!g_trinket_bulk_model || hamlet_bulk_in_raid() ||
       !trinket_bulk_state(s,&before,&expected,&count))return hamlet_fail("Unequip All context unreadable",0);
    if(!count && before==a->u.unequip.after)return 1; /* Crossed confirmations: native work already completed. */
    if(before!=a->u.unequip.before || expected!=a->u.unequip.after || count!=a->u.unequip.count)
        return hamlet_fail("Unequip All state changed; confirm again",0);
    g_trinket_bulk_model((void *)(uintptr_t)s->campaign,(void *)(uintptr_t)(s->campaign+0xe88));
    if(!trinket_bulk_state(s,&actual,&unused,&left) || left || actual!=a->u.unequip.after)
        return hamlet_fail("Unequip All native result incomplete",1);
    /* RealmInventory's normal confirm callback sets +538; resolve the current embedded panel afresh. */
    if(dd_hamlet_panel(s,0xebef28,&panel)==1){uint32_t dirty=1;WriteProcessMemory(GetCurrentProcess(),(void *)(uintptr_t)(panel+0x538),&dirty,4,NULL);}
    if(g_recruit_save[0])g_recruit_save[0]((void *)(uintptr_t)s->campaign,0);
    if(g_hamlet_notify)g_hamlet_notify((void *)(uintptr_t)(s->campaign+0xe50));
    dd_log("Trinket: Unequip All verified items=%u role=%s",count,dd_role_name(g_cfg.role));return 1;
}
static void __fastcall hook_trinket_bulk_model(void *campaign,void *estate) {
    if(!g_trinket_bulk_model)return;
    if(g_game_apply_depth || !hamlet_local_active()) {g_trinket_bulk_model(campaign,estate);return;}
#ifdef _MSC_VER
    uintptr_t caller=(uintptr_t)_ReturnAddress();
#else
    uintptr_t caller=(uintptr_t)__builtin_return_address(0);
#endif
    dd_hamlet s;dd_action a={0};unsigned count=0;
    if(!g_trinket_bulk_caller(caller-image_base()) || !hamlet_input_ready() || hamlet_bulk_in_raid() ||
       !g_hamlet_open_model(&s) || s.campaign!=(uintptr_t)campaign || (uintptr_t)estate!=s.campaign+0xe88 ||
       !trinket_bulk_state(&s,&a.u.unequip.before,&a.u.unequip.after,&count)) {
        shop_notice("Unequip All could not be verified. Nothing changed; send the logs.");return;
    }
    if(!count)return;
    if(g_cfg.role==DD_ROLE_GUEST && g_hamlet_requests[DD_ACT_UNEQUIP_ALL])return;
    a.kind=DD_ACT_UNEQUIP_ALL;a.u.unequip.count=(uint16_t)count;
    (void)hamlet_submit(&a);
}

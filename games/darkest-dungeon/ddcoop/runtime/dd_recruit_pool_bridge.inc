/* Content comparison before recruitment. Control packets never mutate the
 * engine and are consumed only after their ordered-action anchor is applied. */
static dd_msg_queue g_pool_controls;
static uint32_t g_pool_epoch,g_pool_anchor,g_pool_count,g_pool_floor;
static int g_pool_floor_valid;
static uint64_t g_pool_hash,g_pool_deadline,g_pool_initial_at,g_pool_apply_wait,g_pool_retry_at;
static unsigned g_pool_expected,g_pool_acks;
static int g_pool_pending,g_pool_verified,g_pool_raid;
static dd_msg g_pool_probe;
static const char g_pool_wait[]="Checking the Stagecoach candidates on every game. Please wait a moment before recruiting.";
static int pool_read(uint64_t *digest,unsigned *count) {
    dd_recruit s;
    return g_recruit_open_model(&s) && dd_recruit_pool_digest(&s,digest,count);
}
static int (*g_pool_read)(uint64_t *,unsigned *)=pool_read;
static int pool_town(void) {uint32_t d[3];return town_check_here(d);}
static int (*g_pool_in_town)(void)=pool_town;
static unsigned pool_peers(void) {
    unsigned mask=0;
    for(unsigned i=0;i<DD_MAX_GUESTS;i++)
        if(InterlockedCompareExchange(&g_guests[i].ready,0,0))mask|=1u<<(i+1);
    return mask;
}
static void pool_fail(void) {
    dd_log("Stagecoach pool: stop epoch=%u role=%s applied=%u anchor=%u floor=%u/%d pending=%d raid=%d received=%x expected=%x",g_pool_epoch,dd_role_name(g_cfg.role),g_town.applied_seq,g_pool_probe.type?g_pool_probe.u.pool.anchor:g_pool_anchor,g_pool_floor,g_pool_floor_valid,g_pool_pending,g_pool_raid,g_pool_acks,g_pool_expected);
    g_pool_verified=0;g_pool_pending=0;
    runtime_sync_stop(3);
    shop_notice("The Stagecoach candidates differ or could not be compared. Shared actions have stopped. Use Resync in the host launcher; keep every launcher open.");
}
static void pool_reset(void) {
    g_pool_epoch=g_pool_anchor=g_pool_count=g_pool_floor=0;g_pool_floor_valid=0;
    g_pool_hash=g_pool_deadline=g_pool_initial_at=g_pool_apply_wait=g_pool_retry_at=0;
    g_pool_expected=g_pool_acks=0;g_pool_pending=g_pool_verified=g_pool_raid=0;
    memset(&g_pool_probe,0,sizeof g_pool_probe);queue_clear(&g_pool_controls);
}
static void pool_enter_raid(void) {
    if(g_pool_raid)return;
    if(!g_pool_raid){g_pool_floor=g_town.applied_seq;g_pool_floor_valid=1;}
    g_pool_raid=1;g_pool_pending=g_pool_verified=0;g_pool_initial_at=0;
    memset(&g_pool_probe,0,sizeof g_pool_probe);
}
static void pool_start(void) {
    uint64_t hash=0;unsigned count=0,peers=pool_peers();
    if(g_cfg.role!=DD_ROLE_HOST || !peers || g_pool_pending || g_town.needs_resync)return;
    if(g_pool_epoch==UINT32_MAX || !g_pool_read(&hash,&count)){pool_fail();return;}
    g_pool_epoch++;g_pool_hash=hash;g_pool_count=count;g_pool_anchor=g_town.applied_seq;
    g_pool_expected=peers;g_pool_acks=0;g_pool_pending=1;g_pool_verified=g_pool_raid=0;
    g_pool_deadline=GetTickCount64()+60000;g_pool_retry_at=GetTickCount64()+1000;
    dd_msg m={0};m.type=DD_MSG_POOL;m.u.pool.op=1;m.u.pool.epoch=g_pool_epoch;
    m.u.pool.anchor=g_pool_anchor;m.u.pool.digest=hash;m.u.pool.count=count;
    g_town.send(g_town.net,DD_TOWN_ALL,&m);
    if(InterlockedCompareExchange(&g_queue_failed,0,0)){pool_fail();return;}
    recruit_log_pool("host baseline");
    dd_log("Stagecoach pool: compare epoch=%u anchor=%u count=%u hash=%016llx peers=%x",g_pool_epoch,g_pool_anchor,count,(unsigned long long)hash,peers);
}
/* Called by authenticated connection workers, never reads the game. */
static int pool_receive(dd_msg m,int host,int player) {
    if(m.type!=DD_MSG_POOL)return 0;
    if(host) {
        if(m.u.pool.op!=2 || player<1 || player>3 || m.u.pool.player!=(unsigned)player)return 0;
        m.u.pool.player=(uint8_t)player;
    } else if(m.u.pool.op==2 || m.u.pool.player)return 0;
    return queue_push(&g_pool_controls,&m);
}
static int pool_recruit_ready(void) {
    if(g_pool_verified && !g_town.needs_resync)return 1;
    if(!g_town.needs_resync)shop_notice(g_pool_wait);
    return 0;
}
static void pool_poll(void) {
    if(g_town.needs_resync)return;
    if(g_pool_raid && g_pool_in_town())g_pool_raid=0;
    dd_msg m;
    while(queue_pop(&g_pool_controls,&m)) {
        if(m.u.pool.epoch<g_pool_epoch || (g_pool_floor_valid && m.u.pool.anchor<=g_pool_floor))continue;
        if(g_cfg.role==DD_ROLE_HOST) {
            if(m.u.pool.epoch!=g_pool_epoch || !g_pool_pending)continue;
            unsigned bit=1u<<m.u.pool.player;
            if(!(g_pool_expected&bit) || m.u.pool.anchor!=g_pool_anchor ||
               m.u.pool.digest!=g_pool_hash || m.u.pool.count!=g_pool_count) {
                dd_log("Stagecoach pool: guest comparison failed epoch=%u player=%u",g_pool_epoch,m.u.pool.player);
                pool_fail();return;
            }
            if(!(g_pool_acks&bit))dd_log("Stagecoach pool: report epoch=%u player=%u anchor=%u count=%u hash=%016llx",g_pool_epoch,m.u.pool.player,m.u.pool.anchor,m.u.pool.count,(unsigned long long)m.u.pool.digest);
            g_pool_acks|=bit;
        } else if(m.u.pool.op==1) {
            if(m.u.pool.epoch==g_pool_epoch || (g_pool_floor_valid && m.u.pool.anchor<=g_pool_floor))continue;
            g_pool_epoch=m.u.pool.epoch;g_pool_probe=m;g_pool_pending=1;g_pool_verified=0;g_pool_acks=0;
            g_pool_deadline=GetTickCount64()+60000;
        } else {
            if(g_pool_verified && m.u.pool.epoch==g_pool_epoch)continue;
            /* A release follows our report on this same authenticated stream. */
            if(!g_pool_pending || !g_pool_probe.type || m.u.pool.epoch!=g_pool_epoch ||
               m.u.pool.anchor!=g_pool_probe.u.pool.anchor || m.u.pool.digest!=g_pool_probe.u.pool.digest ||
               m.u.pool.count!=g_pool_probe.u.pool.count || !g_pool_acks) {pool_fail();return;}
            uint64_t actual;unsigned count;
            int readable=g_pool_read(&actual,&count);
            if(!readable || actual!=m.u.pool.digest || count!=m.u.pool.count){dd_log("Stagecoach pool: release comparison readable=%d count=%u/%u hash=%016llx/%016llx",readable,readable?count:0,m.u.pool.count,(unsigned long long)(readable?actual:0),(unsigned long long)m.u.pool.digest);recruit_log_pool("release mismatch");pool_fail();return;}
            g_pool_verified=1;g_pool_pending=0;g_pool_probe.type=0;
            shop_notice_clear(g_pool_wait);
            dd_log("Stagecoach pool: all players verified epoch=%u",g_pool_epoch);
        }
    }
    if(g_pool_pending && g_cfg.role==DD_ROLE_HOST && pool_peers()!=g_pool_expected){pool_fail();return;}
    if(g_pool_pending && GetTickCount64()>g_pool_deadline) {
        dd_log("Stagecoach pool: timeout epoch=%u anchor=%u expected=%x received=%x role=%s",g_pool_epoch,g_pool_anchor,g_pool_expected,g_pool_acks,dd_role_name(g_cfg.role));
        pool_fail();return;
    }
    /* The TCP handshake can finish before a guest has loaded the selected campaign. Its inactive poll clears
     * controls. Re-send the SAME comparison only to guests still missing a report, once per second and within
     * the original deadline. No new epoch, extended timeout, native effect or release without all exact reports. */
    if(g_cfg.role==DD_ROLE_HOST && g_pool_pending && g_pool_acks!=g_pool_expected && GetTickCount64()>=g_pool_retry_at) {
        g_pool_retry_at=GetTickCount64()+1000;
        dd_msg probe={0};probe.type=DD_MSG_POOL;probe.u.pool.op=1;probe.u.pool.epoch=g_pool_epoch;
        probe.u.pool.anchor=g_pool_anchor;probe.u.pool.digest=g_pool_hash;probe.u.pool.count=g_pool_count;
        for(unsigned player=1;player<=3;player++)if((g_pool_expected & ~g_pool_acks) & (1u<<player))
            g_town.send(g_town.net,(int)player,&probe);
        if(InterlockedCompareExchange(&g_queue_failed,0,0)){pool_fail();return;}
    }
    if(g_cfg.role==DD_ROLE_HOST && g_pool_pending && g_pool_acks==g_pool_expected) {
        uint64_t hash;unsigned count;
        if(pool_peers()!=g_pool_expected || !g_pool_read(&hash,&count) || hash!=g_pool_hash || count!=g_pool_count){pool_fail();return;}
        dd_msg release={0};release.type=DD_MSG_POOL;release.u.pool.op=3;
        release.u.pool.epoch=g_pool_epoch;release.u.pool.anchor=g_pool_anchor;
        release.u.pool.digest=hash;release.u.pool.count=count;
        g_town.send(g_town.net,DD_TOWN_ALL,&release);
        if(InterlockedCompareExchange(&g_queue_failed,0,0)){pool_fail();return;}
        g_pool_verified=1;g_pool_pending=0;shop_notice_clear(g_pool_wait);
        dd_log("Stagecoach pool: all players verified epoch=%u",g_pool_epoch);
    }
    if(g_cfg.role==DD_ROLE_GUEST && g_pool_pending && g_pool_probe.type && !g_pool_acks &&
       g_town.applied_seq>=g_pool_probe.u.pool.anchor && g_pool_in_town()) {
        uint64_t hash;unsigned count;
        if(!g_pool_read(&hash,&count)){dd_log("Stagecoach pool: guest baseline unreadable at anchor=%u",g_pool_probe.u.pool.anchor);recruit_log_pool("unreadable baseline");pool_fail();return;}
        recruit_log_pool("guest baseline");
        dd_msg reply=g_pool_probe;reply.u.pool.op=2;reply.u.pool.player=(uint8_t)g_cfg.player;
        reply.u.pool.digest=hash;reply.u.pool.count=count;
        g_town.send(g_town.net,0,&reply);
        if(InterlockedCompareExchange(&g_queue_failed,0,0)){pool_fail();return;}
        g_pool_acks=1;
        if(hash!=g_pool_probe.u.pool.digest || count!=g_pool_probe.u.pool.count){dd_log("Stagecoach pool: guest baseline differs count=%u/%u hash=%016llx/%016llx",count,g_pool_probe.u.pool.count,(unsigned long long)hash,(unsigned long long)g_pool_probe.u.pool.digest);pool_fail();return;}
    }
    if(!g_pool_pending && !g_pool_verified && g_cfg.role==DD_ROLE_HOST && g_pool_in_town()) {
        uint64_t now=GetTickCount64();
        if(!g_pool_initial_at)g_pool_initial_at=now+2000;
        if(now>=g_pool_initial_at)pool_start();
    }
}

static int pool_apply_gate(const dd_msg *m) {
    if((m->type!=DD_MSG_TOWN_APPLY && m->type!=DD_MSG_TOWN_REQUEST) ||
       (m->u.town.action.kind!=DD_ACT_RECRUIT && m->u.town.action.kind!=DD_ACT_PREPARE && m->u.town.action.kind!=DD_ACT_EMBARK))return 0;
    /* RELEASE and APPLY use separate main-thread queues. The worker may enqueue
     * both just after this frame's control poll; preserve the ordered FIFO head. */
    if(!g_pool_verified)pool_poll();
    if(g_pool_verified){g_pool_apply_wait=0;return 0;}
    if(!g_pool_apply_wait)g_pool_apply_wait=GetTickCount64()+60000;
    if(GetTickCount64()>g_pool_apply_wait)pool_fail();
    return 1;
}

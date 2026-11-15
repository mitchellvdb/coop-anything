/* Presence never enters the action FIFO or calls a native function on a worker. */
#define DD_PRESENCE_FRESH_MS 5000u
static SRWLOCK g_presence_lock=SRWLOCK_INIT;
static struct {uint8_t code[4],mask;uint64_t at[4],snapshot_at;} g_presence;
static uint64_t g_presence_sample_at;
static void presence_clear(int player) {
    AcquireSRWLockExclusive(&g_presence_lock);
    if(player<0)memset(&g_presence,0,sizeof g_presence);
    else if(player<4){g_presence.code[player]=0;g_presence.at[player]=0;g_presence.mask&=(uint8_t)~(1u<<player);}
    ReleaseSRWLockExclusive(&g_presence_lock);
}
static int presence_receive(const dd_msg *m,int host,int player,uint64_t now) {
    if(m->type!=DD_MSG_PRESENCE || m->seq ||
       !dd_presence_valid(m->u.presence.kind,m->u.presence.mask,m->u.presence.place))return 0;
    if(host ? (player<1 || player>3 || m->u.presence.kind || m->u.presence.mask!=(1u<<player)) :
               m->u.presence.kind!=1)return 0;
    AcquireSRWLockExclusive(&g_presence_lock);
    if(host) {g_presence.code[player]=m->u.presence.place[player];g_presence.at[player]=now;}
    else {
        g_presence.mask=m->u.presence.mask;g_presence.snapshot_at=now;
        for(int i=0;i<4;i++)if(i!=g_cfg.player) {
            g_presence.code[i]=m->u.presence.place[i];g_presence.at[i]=now;
        }
    }
    ReleaseSRWLockExclusive(&g_presence_lock);
    return 1;
}
static uint8_t presence_mask(int host) {
    uint8_t mask=1;
    if(host) {
        for(int i=0;i<3;i++)if(InterlockedCompareExchange(&g_guests[i].ready,0,0))mask|=(uint8_t)(2u<<i);
    } else mask=g_presence.mask;
    return mask;
}
/* Called with the presence lock held. The local row expires too when the game
 * thread no longer pumps; a responsive network thread must not preserve Guild. */
static uint8_t presence_code(int i,uint64_t now) {
    return g_presence.at[i] && now>=g_presence.at[i] && now-g_presence.at[i]<=DD_PRESENCE_FRESH_MS ? g_presence.code[i]:0;
}
static void presence_message(int host,dd_msg *m,uint64_t now) {
    memset(m,0,sizeof *m);m->type=DD_MSG_PRESENCE;m->u.presence.kind=(uint8_t)host;
    AcquireSRWLockShared(&g_presence_lock);
    m->u.presence.mask=host?presence_mask(1):(uint8_t)(1u<<g_cfg.player);
    for(int i=0;i<4;i++)if(m->u.presence.mask&(1u<<i))m->u.presence.place[i]=presence_code(i,now);
    ReleaseSRWLockShared(&g_presence_lock);
}
static unsigned presence_native(void) {
    walk_view v;dd_hamlet s;
    if(!InterlockedCompareExchange(&g_campaign_active,0,0))return DD_PLACE_LOADING;
    if(g_walk_view(&v))return DD_PLACE_DUNGEON;
    return g_hamlet_open_model(&s)?dd_hamlet_presence(&s):DD_PLACE_UNKNOWN;
}
static unsigned (*g_presence_native)(void)=presence_native;
static void presence_poll(void) {
    uint64_t now=GetTickCount64();uint8_t codes[4]={0},mask;
    if(g_cfg.role==DD_ROLE_OFF || g_cfg.player<0 || g_cfg.player>3) {
        dd_overlay_presence(0,0,0,codes);return;
    }
    if(g_presence_sample_at && now>=g_presence_sample_at && now-g_presence_sample_at<250)return;
    g_presence_sample_at=now;
    unsigned local=g_presence_native();
    if(local>=DD_PLACE_COUNT)local=DD_PLACE_UNKNOWN;
    AcquireSRWLockExclusive(&g_presence_lock);
    g_presence.code[g_cfg.player]=(uint8_t)local;g_presence.at[g_cfg.player]=now;
    mask=(uint8_t)(presence_mask(g_cfg.role==DD_ROLE_HOST)|(1u<<g_cfg.player));
    if(!InterlockedCompareExchange(&g_session_ready,0,0) || InterlockedCompareExchange(&g_session_lost,0,0))
        mask=(uint8_t)(1u<<g_cfg.player);
    for(int i=0;i<4;i++)codes[i]=presence_code(i,now);
    if(g_cfg.role==DD_ROLE_GUEST && (!g_presence.snapshot_at || now<g_presence.snapshot_at || now-g_presence.snapshot_at>DD_PRESENCE_FRESH_MS))
        for(int i=0;i<4;i++)if(i!=g_cfg.player)codes[i]=0;
    ReleaseSRWLockExclusive(&g_presence_lock);
    /* Dungeon mode belongs solely to the validated raid view; a missing town
     * view during loading must not leave yesterday's player blocks behind. */
    dd_overlay_presence(local!=DD_PLACE_DUNGEON && local!=DD_PLACE_LOADING &&
                        InterlockedCompareExchange(&g_campaign_active,0,0),mask,g_cfg.player,codes);
}

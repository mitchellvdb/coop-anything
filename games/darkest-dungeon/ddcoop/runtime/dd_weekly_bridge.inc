/* The normal engine calls Darkest::RaidFinish before TownVisitStart and before
 * the results display's Next buttons. Synchronize Rules at that exact native
 * boundary, also when a party wipe reaches it without a UI exit command.
 * The authenticated network worker only latches data. It never calls the game.
 */
#define DD_WEEKLY_WAIT_MS 30000u
#define DD_WEEKLY_RULES_MAX 2147483646u
/* Campaign+58 is nextGuid (persist.roster nextGuid; read-only disassembly of the pinned exe): new heroes take
 * guid = nextGuid++ (58637c, 584ff2, ...), and so do listener handles (589e40/589ef0), which the roster panel
 * 6b48d0 and others register whenever a player opens them. Each copy therefore drifts with its own UI use, and
 * the Stagecoach heroes generated inside RaidFinish got ids one or two apart (owner test74, 11 Nov). The host
 * picks a value above its own counter and every copy jumps to it right before RaidFinish. A gap is harmless;
 * going below a copy's own counter could reuse a live handle, so a guest refuses that instead. */
#define DD_WEEKLY_GUID_OFFSET 0x58u
#define DD_WEEKLY_GUID_MARGIN 1024u

typedef void(__fastcall *weekly_finish_fn)(void *);
static weekly_finish_fn g_weekly_finish;
static SRWLOCK g_weekly_lock = SRWLOCK_INIT;
static uint32_t g_weekly_received, g_weekly_consumed, g_weekly_rules, g_weekly_guid;
static int g_weekly_pending, g_weekly_fault;
static volatile LONG g_weekly_in_finish;

static uint64_t weekly_now_native(void) { return GetTickCount64(); }
static void weekly_pause_native(unsigned ms) { Sleep(ms); }
static int weekly_send_native(const dd_msg *m) { return net_send(DD_TOWN_ALL,m); }
static int weekly_finish_caller(uintptr_t rva) { return rva==0x3e24c6; }
static int weekly_rules_set(uint32_t rules) {
    uint32_t before=0,after=0;
    if(!g_rules_object || !safe_read((uintptr_t)g_rules_object+8,&before,4))return 0;
    memcpy((uint8_t *)g_rules_object+8,&rules,4);
    return safe_read((uintptr_t)g_rules_object+8,&after,4) && after==rules;
}
static int weekly_guid_read(uint32_t *value) {
    dd_hamlet s;
    return g_hamlet_open_model(&s) && hamlet_read(&s,s.campaign+DD_WEEKLY_GUID_OFFSET,value,4);
}
static int weekly_guid_write(uint32_t value) {
    dd_hamlet s;uint32_t after=0;
    if(!g_hamlet_open_model(&s) || !s.campaign)return 0;
    memcpy((void *)(uintptr_t)(s.campaign+DD_WEEKLY_GUID_OFFSET),&value,4);
    return hamlet_read(&s,s.campaign+DD_WEEKLY_GUID_OFFSET,&after,4) && after==value;
}
static uint64_t (*g_weekly_now)(void)=weekly_now_native;
static void (*g_weekly_pause)(unsigned)=weekly_pause_native;
static int (*g_weekly_send)(const dd_msg *)=weekly_send_native;
static int (*g_weekly_finish_caller)(uintptr_t)=weekly_finish_caller;
static int (*g_weekly_rules_set)(uint32_t)=weekly_rules_set;
static int (*g_weekly_guid_get)(uint32_t *)=weekly_guid_read;
static int (*g_weekly_guid_set)(uint32_t)=weekly_guid_write;

static int weekly_busy(void) { return InterlockedCompareExchange(&g_weekly_in_finish,0,0)!=0; }
static void weekly_reset(void) {
    AcquireSRWLockExclusive(&g_weekly_lock);
    g_weekly_received=g_weekly_consumed=g_weekly_rules=g_weekly_guid=0;
    g_weekly_pending=g_weekly_fault=0;
    ReleaseSRWLockExclusive(&g_weekly_lock);
}
static int weekly_faulted(void) {
    int value;
    AcquireSRWLockShared(&g_weekly_lock);value=g_weekly_fault;ReleaseSRWLockShared(&g_weekly_lock);
    return value;
}
static int weekly_receive(const dd_msg *m,int host,int player) {
    int ok=0;
    (void)player; /* The caller is the already-authenticated connection worker. */
    AcquireSRWLockExclusive(&g_weekly_lock);
    if(!m || host || g_cfg.role!=DD_ROLE_GUEST || m->type!=DD_MSG_WEEKLY || m->seq ||
       !m->u.weekly.epoch || !m->u.weekly.rules || m->u.weekly.rules>DD_WEEKLY_RULES_MAX ||
       !m->u.weekly.guid || m->u.weekly.guid>DD_PROTO_GUID_MAX || g_weekly_fault) {
        g_weekly_fault=1;
    } else if(m->u.weekly.epoch==g_weekly_received && m->u.weekly.rules==g_weekly_rules && m->u.weekly.guid==g_weekly_guid) {
        ok=1; /* Identical last duplicate is harmless before or after consumption. */
    } else if(g_weekly_pending || g_weekly_received==UINT32_MAX ||
              g_weekly_received!=g_weekly_consumed || m->u.weekly.epoch!=g_weekly_received+1) {
        g_weekly_fault=1;
    } else {
        g_weekly_received=m->u.weekly.epoch;g_weekly_rules=m->u.weekly.rules;g_weekly_guid=m->u.weekly.guid;
        g_weekly_pending=1;ok=1;
    }
    ReleaseSRWLockExclusive(&g_weekly_lock);
    if(!ok)InterlockedCompareExchange(&g_peer_sync_stop,3,0);
    return ok;
}
static int weekly_interrupted(void) {
    return weekly_faulted() || g_town.needs_resync ||
        InterlockedCompareExchange(&g_peer_sync_stop,0,0) ||
        InterlockedCompareExchange(&g_transport_sync_stop,0,0) ||
        InterlockedCompareExchange(&g_session_lost,0,0) ||
        !InterlockedCompareExchange(&g_session_ready,0,0) ||
        !InterlockedCompareExchange(&g_campaign_active,0,0);
}
static int weekly_host_prepare(uint32_t *epoch,uint32_t *rules,uint32_t *guid) {
    dd_msg m={0};uint32_t here=0;
    if(!g_rules_object || !safe_read((uintptr_t)g_rules_object+8,rules,4) ||
       !*rules || *rules>DD_WEEKLY_RULES_MAX)return 0;
    if(!g_weekly_guid_get(&here) || !here || here>DD_PROTO_GUID_MAX-DD_WEEKLY_GUID_MARGIN)return 0;
    *guid=here+DD_WEEKLY_GUID_MARGIN;
    AcquireSRWLockExclusive(&g_weekly_lock);
    int ok=!g_weekly_fault && !g_weekly_pending && g_weekly_received==g_weekly_consumed &&
        g_weekly_received!=UINT32_MAX;
    if(ok) {
        *epoch=++g_weekly_received;g_weekly_consumed=*epoch;g_weekly_rules=*rules;g_weekly_guid=*guid;
    } else g_weekly_fault=1;
    ReleaseSRWLockExclusive(&g_weekly_lock);
    if(!ok)return 0;
    m.type=DD_MSG_WEEKLY;m.u.weekly.epoch=*epoch;m.u.weekly.rules=*rules;m.u.weekly.guid=*guid;
    return g_weekly_send(&m);
}
static int weekly_guest_wait(uint32_t *epoch,uint32_t *rules,uint32_t *guid) {
    uint64_t start=g_weekly_now();
    for(;;) {
        if(weekly_interrupted())return 0;
        AcquireSRWLockExclusive(&g_weekly_lock);
        int seed_ready=g_weekly_pending && g_weekly_received==g_weekly_consumed+1;
        if(seed_ready) {
            *epoch=g_weekly_received;*rules=g_weekly_rules;*guid=g_weekly_guid;
            g_weekly_consumed=*epoch;g_weekly_pending=0;
        }
        ReleaseSRWLockExclusive(&g_weekly_lock);
        if(seed_ready)return 1;
        uint64_t now=g_weekly_now();
        if(now<start || now-start>=DD_WEEKLY_WAIT_MS)return 0;
        /* Only the network worker can satisfy this wait. No main-thread FIFO,
         * native event pump, nested game update or UI input is processed here. */
        g_weekly_pause(5);
    }
}
static void weekly_stop(const char *why) {
    dd_log("Week: %s; shared actions stopped before further play",why);
    runtime_sync_stop(3);
    shop_notice("The new week could not be synchronized. Shared actions have stopped. Use Resync in the host launcher; send the logs.");
}
static void weekly_native_finish(void *context) {
    /* Native completion must not be replaced with a fake successful return.
     * It may itself call hooked model functions: those are engine work, not
     * fresh player requests, even when sharing has already been stopped. */
    g_game_apply_depth++;
    g_weekly_finish(context);
    g_game_apply_depth--;
}
static void __fastcall hook_weekly_finish(void *context) {
    if(!g_weekly_finish)return; /* Installation publishes the original first. */
    if(!walk_coop_active()) { g_weekly_finish(context);return; }
#ifdef _MSC_VER
    uintptr_t caller=(uintptr_t)_ReturnAddress();
#else
    uintptr_t caller=(uintptr_t)__builtin_return_address(0);
#endif
    if(InterlockedCompareExchange(&g_weekly_in_finish,1,0)) {
        weekly_stop("recursive native raid finish");weekly_native_finish(context);return;
    }
    uint32_t epoch=0,rules=0,guid=0,here=0;
    int ok=g_weekly_finish_caller(caller-image_base()) && !weekly_interrupted();
    if(ok)ok=g_cfg.role==DD_ROLE_HOST?weekly_host_prepare(&epoch,&rules,&guid):weekly_guest_wait(&epoch,&rules,&guid);
    if(ok)ok=!weekly_interrupted() && g_weekly_rules_set(rules);
    /* Never move a copy's counter down: a live handle could be handed out twice. */
    if(ok)ok=g_weekly_guid_get(&here) && here<=guid && g_weekly_guid_set(guid);
    if(ok)dd_log("Week: native raid finish uses host Rules epoch=%u seed=%08x guid=%u (here %u)",epoch,rules,guid,here);
    else weekly_stop("missing, invalid or interrupted native weekly seed or guid counter");
    weekly_native_finish(context);
    InterlockedExchange(&g_weekly_in_finish,0);
}

static const hamlet_hook_spec g_weekly_hooks[] = {
    {"weekly_finish",(uintptr_t)hook_weekly_finish,&g_weekly_finish,13},
};

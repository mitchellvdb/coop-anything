/* Real weekly hook and authenticated-worker latch, synthetic native effects.
 * No game, owner saves or real network. A separate worker tests the latch while
 * the main/native call waits without consuming its ordinary FIFO. */
static uint8_t wx_rng[16];
static unsigned wx_native_calls,wx_seed_writes,wx_send_calls,wx_pauses;
static uint32_t wx_world[12];
static uint64_t wx_clock;
static int wx_send_ok,wx_delay_mode,wx_expected_depth;
static DWORD wx_main_thread;
static dd_msg wx_packet;
static HANDLE wx_begin,wx_done;
static int wx_worker_result;
static int wx_context;
/* Campaign nextGuid stand-in: every copy starts with its own UI-driven value. */
static uint32_t wx_guid;static unsigned wx_guid_writes;static int wx_guid_readable;
static int wx_guid_get(uint32_t *v) { if(!wx_guid_readable)return 0;*v=wx_guid;return 1; }
static int wx_guid_set(uint32_t v) { ++wx_guid_writes;wx_guid=v;return 1; }
static uint32_t wx_state(void) { uint32_t n=0;memcpy(&n,wx_rng+8,4);return n; }
static uint32_t wx_draw(uint32_t *n) { *n=(uint32_t)(((uint64_t)*n*16807u)%2147483647u);return *n; }
static void __fastcall wx_native(void *p) {
    CHECK(p==&wx_context && GetCurrentThreadId()==wx_main_thread && (int)g_game_apply_depth==wx_expected_depth);
    ++wx_native_calls;
    uint32_t n=wx_state();
    for(unsigned i=0;i<12;i++)wx_world[i]=wx_draw(&n);
    memcpy(wx_rng+8,&n,4);
}
static uint64_t wx_now(void) { return wx_clock; }
static int wx_caller(uintptr_t rva) { (void)rva;return 1; }
static int wx_other_caller(uintptr_t rva) { (void)rva;return 0; }
static int wx_set(uint32_t rules) { ++wx_seed_writes;return weekly_rules_set(rules); }
static int wx_bad_set(uint32_t rules) { (void)rules;++wx_seed_writes;return 0; }
static int wx_send(const dd_msg *m) { ++wx_send_calls;wx_packet=*m;return wx_send_ok; }
static dd_msg wx_wire(dd_msg m) {
    uint8_t encoded[DD_PROTO_HEADER+DD_PROTO_MAX_BODY];dd_msg decoded={0};size_t used=0;
    size_t size=dd_encode(&m,encoded,sizeof encoded);
    CHECK(size && dd_decode(encoded,size,&decoded,&used)==DD_DECODE_OK && used==size);
    return decoded;
}
static DWORD WINAPI wx_worker(void *arg) {
    (void)arg;
    if(WaitForSingleObject(wx_begin,1000)!=WAIT_OBJECT_0) { wx_worker_result=-1;SetEvent(wx_done);return 0; }
    uint32_t before=wx_state();unsigned calls=wx_native_calls,writes=wx_seed_writes;
    dd_msg decoded=wx_wire(wx_packet);
    wx_worker_result=weekly_receive(&decoded,0,0);
    /* Receiving on the worker must not perform any native effect or seed write. */
    if(wx_state()!=before || wx_native_calls!=calls || wx_seed_writes!=writes)wx_worker_result=-2;
    SetEvent(wx_done);return 0;
}
static void wx_pause(unsigned ms) {
    ++wx_pauses;wx_clock+=ms;
    if(wx_delay_mode==1 && wx_pauses==3)CHECK(weekly_receive(&wx_packet,0,0));
    else if(wx_delay_mode==5 && wx_pauses==3)CHECK(!weekly_receive(&wx_packet,0,0));
    else if(wx_delay_mode==2)InterlockedExchange(&g_session_lost,1);
    else if(wx_delay_mode==3)wx_clock=10; /* Monotone-clock violation refuses. */
    else if(wx_delay_mode==4) {
        SetEvent(wx_begin);CHECK(WaitForSingleObject(wx_done,1000)==WAIT_OBJECT_0);
    }
}
static void wx_reset(unsigned player,uint32_t seed) {
    ar_reset(player?DD_ROLE_GUEST:DD_ROLE_HOST,0);g_cfg.player=(int)player;
    weekly_reset();InterlockedExchange(&g_weekly_in_finish,0);
    memset(wx_rng,0,sizeof wx_rng);memcpy(wx_rng+8,&seed,4);g_rules_object=wx_rng;
    memset(wx_world,0,sizeof wx_world);memset(&wx_packet,0,sizeof wx_packet);
    wx_native_calls=wx_seed_writes=wx_send_calls=wx_pauses=0;wx_clock=1000;
    wx_send_ok=1;wx_delay_mode=0;wx_expected_depth=1;wx_main_thread=GetCurrentThreadId();
    g_weekly_finish=wx_native;g_weekly_now=wx_now;g_weekly_pause=wx_pause;
    g_weekly_send=wx_send;g_weekly_finish_caller=wx_caller;g_weekly_rules_set=wx_set;
    wx_guid=100+3*player;wx_guid_writes=0;wx_guid_readable=1;
    g_weekly_guid_get=wx_guid_get;g_weekly_guid_set=wx_guid_set;
}
static dd_msg wx_seed(uint32_t epoch,uint32_t rules) {
    dd_msg m={0};m.type=DD_MSG_WEEKLY;m.u.weekly.epoch=epoch;m.u.weekly.rules=rules;m.u.weekly.guid=100+DD_WEEKLY_GUID_MARGIN;return m;
}
static void wx_success(void) {
    CHECK(wx_native_calls==1 && wx_seed_writes==1 && !g_town.needs_resync && !g_peer_sync_stop &&
          !g_transport_sync_stop && !g_game_apply_depth && !weekly_busy());
}
static void wx_failure(void) {
    CHECK(wx_native_calls==1 && !wx_seed_writes && g_town.needs_resync && !g_game_apply_depth && !weekly_busy());
}
static void weekly_bridge_checks(void) {
    weekly_finish_fn save_finish=g_weekly_finish;
    uint64_t (*save_now)(void)=g_weekly_now;
    void (*save_pause)(unsigned)=g_weekly_pause;
    int (*save_send)(const dd_msg *)=g_weekly_send;
    int (*save_caller)(uintptr_t)=g_weekly_finish_caller;
    int (*save_set)(uint32_t)=g_weekly_rules_set;
    int (*save_guid_get)(uint32_t *)=g_weekly_guid_get;
    int (*save_guid_set)(uint32_t)=g_weekly_guid_set;
    void *save_rules=g_rules_object;
    CHECK(save_caller(0x3e24c6) && !save_caller(0x3e24c5) && !save_caller(0x3e24c7) && !save_caller(0x3e2500));

    /* Host chooses its Rules at the actual native boundary, not the earlier
     * UI callback. All three receivers start with different local seeds. */
    wx_reset(0,1234567);hook_weekly_finish(&wx_context);wx_success();
    CHECK(wx_send_calls==1 && wx_packet.type==DD_MSG_WEEKLY && wx_packet.u.weekly.epoch==1 &&
          wx_packet.u.weekly.rules==1234567 && g_weekly_consumed==1 && g_weekly_received==1);
    /* The host jumps its own counter to the shared value it announced. */
    CHECK(wx_packet.u.weekly.guid==100+DD_WEEKLY_GUID_MARGIN && wx_guid==wx_packet.u.weekly.guid && wx_guid_writes==1);
    dd_msg canonical=wx_wire(wx_packet);uint32_t expected[12];memcpy(expected,wx_world,sizeof expected);
    for(unsigned player=1;player<4;player++)for(unsigned delayed=0;delayed<2;delayed++) {
        wx_reset(player,8765+player);wx_packet=canonical;
        if(delayed)wx_delay_mode=1;else {
            CHECK(weekly_receive(&canonical,0,0));CHECK(weekly_receive(&canonical,0,0));
        }
        dd_msg ordinary={0};ordinary.type=DD_MSG_TOWN_APPLY;ordinary.seq=1;
        ordinary.u.town.action.kind=DD_ACT_RECRUIT;ordinary.u.town.action.u.recruit.hero_id=3;
        CHECK(queue_push(&g_incoming,&ordinary));
        hook_weekly_finish(&wx_context);wx_success();
        CHECK(!memcmp(wx_world,expected,sizeof expected) && !wx_send_calls &&
              g_incoming.count==1 && !g_town.applied_seq && !g_weekly_pending);
        CHECK(wx_guid==canonical.u.weekly.guid && wx_guid_writes==1); /* every copy now draws the same next ids */
        CHECK(weekly_receive(&canonical,0,0)); /* Same last duplicate after use. */
        CHECK(!g_weekly_pending && g_weekly_consumed==1);
        CHECK(weekly_receive(&(dd_msg){.type=DD_MSG_WEEKLY,.u.weekly={2,7654321,2*DD_WEEKLY_GUID_MARGIN}},0,0));
        hook_weekly_finish(&wx_context);
        CHECK(wx_native_calls==2 && wx_seed_writes==2 && g_weekly_consumed==2 && !g_town.needs_resync);
        uint32_t after_second=wx_state();
        CHECK(!weekly_receive(&canonical,0,0) && weekly_faulted() && wx_state()==after_second && wx_native_calls==2);
    }
    /* A separate authenticated-worker simulation can satisfy a waiting native
     * call without any main-thread action processing, Rules write or native call. */
    wx_reset(2,9999);wx_packet=canonical;wx_delay_mode=4;wx_worker_result=0;
    wx_begin=CreateEventW(NULL,TRUE,FALSE,NULL);wx_done=CreateEventW(NULL,TRUE,FALSE,NULL);
    CHECK(wx_begin && wx_done);
    HANDLE worker=CreateThread(NULL,0,wx_worker,NULL,0,NULL);CHECK(worker);
    if(worker && wx_begin && wx_done) {
        hook_weekly_finish(&wx_context);CHECK(WaitForSingleObject(worker,1000)==WAIT_OBJECT_0);
        wx_success();CHECK(wx_worker_result==1 && !memcmp(wx_world,expected,sizeof expected));
    }
    if(worker)CloseHandle(worker);if(wx_begin)CloseHandle(wx_begin);if(wx_done)CloseHandle(wx_done);
    wx_begin=wx_done=NULL;

    /* Direction, sequence, semantic range and epoch mistakes cannot authorize
     * a reset. An identical last duplicate is idempotent; changed/reordered
     * announcements stop before any new shared/native seed effect. */
    for(unsigned which=0;which<11;which++) {
        wx_reset(1,123);dd_msg bad=canonical;int host=0;
        switch(which) {
        case 0:host=1;break;
        case 1:bad.seq=1;break;
        case 2:bad.u.weekly.epoch=0;break;
        case 3:bad.u.weekly.rules=0;break;
        case 4:bad.u.weekly.rules=2147483647u;break;
        case 5:bad.u.weekly.epoch=2;break;
        case 6:CHECK(weekly_receive(&canonical,0,0));bad.u.weekly.rules++;break;
        case 7:CHECK(weekly_receive(&canonical,0,0));bad.u.weekly.epoch=2;break;
        case 8:bad.type=DD_MSG_PING;break;
        case 9:bad.u.weekly.guid=0;break;
        default:bad.u.weekly.guid=DD_PROTO_GUID_MAX+1u;break;
        }
        CHECK(!weekly_receive(&bad,host,1) && weekly_faulted() && wx_state()==123);
        hook_weekly_finish(&wx_context);wx_failure();CHECK(!wx_send_calls);
    }
    for(unsigned player=0;player<4;player++)for(unsigned why=0;why<5;why++) {
        wx_reset(player,123);
        if(why==0)g_town.needs_resync=1;
        else if(why==1)InterlockedExchange(&g_peer_sync_stop,3);
        else if(why==2)InterlockedExchange(&g_transport_sync_stop,3);
        else if(why==3)InterlockedExchange(&g_session_lost,1);
        else InterlockedExchange(&g_session_ready,0);
        hook_weekly_finish(&wx_context);wx_failure();CHECK(!wx_send_calls);
    }
    for(unsigned player=1;player<4;player++)for(unsigned why=0;why<3;why++) {
        wx_reset(player,123);wx_delay_mode=why==1?5:(int)why;
        hook_weekly_finish(&wx_context);wx_failure();
        if(!why)CHECK(wx_clock==1000+DD_WEEKLY_WAIT_MS && wx_pauses==DD_WEEKLY_WAIT_MS/5);
        if(why==1)CHECK(wx_pauses==3); /* Blank packet injected at third wait fails. */
        if(why==2)CHECK(wx_pauses==1); /* Lost connection interrupts promptly. */
    }
    wx_reset(1,123);wx_delay_mode=3;hook_weekly_finish(&wx_context);wx_failure();CHECK(wx_pauses==1);
    wx_reset(0,123);wx_send_ok=0;hook_weekly_finish(&wx_context);wx_failure();CHECK(wx_send_calls==1);
    wx_reset(0,0);hook_weekly_finish(&wx_context);wx_failure();CHECK(!wx_send_calls);
    wx_reset(0,123);g_weekly_received=g_weekly_consumed=UINT32_MAX;
    hook_weekly_finish(&wx_context);wx_failure();CHECK(!wx_send_calls);
    for(unsigned player=0;player<4;player++) {
        wx_reset(player,123);g_weekly_finish_caller=wx_other_caller;
        hook_weekly_finish(&wx_context);wx_failure();CHECK(!wx_send_calls);
        wx_reset(player,123);g_weekly_rules_set=wx_bad_set;
        if(player)CHECK(weekly_receive(&canonical,0,0));
        hook_weekly_finish(&wx_context);
        CHECK(wx_native_calls==1 && wx_seed_writes==1 && g_town.needs_resync && !weekly_busy());
    }
    /* A guest whose own counter is already above the host's value refuses rather than move it down (a live
     * handle could be handed out twice); an unreadable counter or a host too close to the limit refuses too.
     * Native completion still runs once and the counter is left untouched. */
    for(unsigned why=0;why<4;why++) {
        unsigned player=why==2?0u:1u;wx_reset(player,123);
        if(why==0)wx_guid=canonical.u.weekly.guid+1;
        if(why==1)wx_guid_readable=0;
        if(why==2)wx_guid=DD_PROTO_GUID_MAX-DD_WEEKLY_GUID_MARGIN+1;
        if(why==3)wx_guid=canonical.u.weekly.guid; /* equal is fine */
        uint32_t before=wx_guid;
        if(player)CHECK(weekly_receive(&canonical,0,0));
        hook_weekly_finish(&wx_context);
        if(why==3)CHECK(wx_native_calls==1 && !g_town.needs_resync && wx_guid==canonical.u.weekly.guid);
        else CHECK(wx_native_calls==1 && g_town.needs_resync && !wx_guid_writes && wx_guid==before && !weekly_busy());
    }
    wx_reset(0,123);InterlockedExchange(&g_weekly_in_finish,1);
    hook_weekly_finish(&wx_context);
    CHECK(wx_native_calls==1 && !wx_seed_writes && !wx_send_calls && g_town.needs_resync && weekly_busy());
    InterlockedExchange(&g_weekly_in_finish,0);
    wx_reset(0,123);g_cfg.role=DD_ROLE_OFF;wx_expected_depth=0;
    hook_weekly_finish(&wx_context);CHECK(wx_native_calls==1 && !wx_seed_writes && !wx_send_calls && !weekly_busy());
    wx_reset(0,123);g_campaign_active=0;wx_expected_depth=0;
    hook_weekly_finish(&wx_context);CHECK(wx_native_calls==1 && !wx_seed_writes && !wx_send_calls && !weekly_busy());

    wx_reset(0,123);weekly_reset();g_weekly_finish=save_finish;g_weekly_now=save_now;g_weekly_pause=save_pause;
    g_weekly_send=save_send;g_weekly_finish_caller=save_caller;g_weekly_rules_set=save_set;g_rules_object=save_rules;
    g_weekly_guid_get=save_guid_get;g_weekly_guid_set=save_guid_set;
    puts("Weekly bridge: native boundary all4 roles, early/delayed/cross-thread host seed, no FIFO reentry, epoch/range/direction/duplicate/STOP/timeout/send-write failures, original native completion once; synthetic week effects match; shared guid counter jumps on every copy, never down");
}

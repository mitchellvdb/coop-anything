/* Actual runtime queues/sender with a modeled connection send, never Steam/game. */
static dd_msg ts_sent[128];
static unsigned ts_count;
static int ts_fail, ts_latch_after_first;
static int ts_send(dd_conn *c, const dd_msg *m) {
    (void)c;
    if (ts_count < sizeof ts_sent / sizeof *ts_sent) ts_sent[ts_count] = *m;
    ts_count++;
    if (ts_latch_after_first && ts_count == 1) runtime_queue_failure();
    return !ts_fail;
}
static void ts_reset(int role) {
    reset(role);
    g_guests[1].ready = g_guests[2].ready = role == DD_ROLE_HOST;
    for (int i = 0; i < DD_MAX_GUESTS; i++) queue_clear(guest_out(i));
    ts_count = 0; ts_fail = ts_latch_after_first = 0;
}
static void ts_fill(dd_msg_queue *q) {
    dd_msg m = {0}; m.type = DD_MSG_PING;
    for (unsigned i = 0; i < DD_QUEUE_CAP; i++) CHECK(queue_push(q, &m));
}
static void transport_stop_checks(void) {
    ts_reset(DD_ROLE_HOST);runtime_sync_stop(1);CHECK(overlay_stop_reports==1);
    runtime_sync_stop(1);CHECK(overlay_stop_reports==1);
    ts_reset(DD_ROLE_HOST);g_session_lost=1;CHECK(runtime_shared_blocked() && !overlay_stop_reports);
    dd_conn c = {0}; dd_msg m = {0}; m.type = DD_MSG_PING;
    /* One congested receiver must stop every healthy receiver too. */
    ts_reset(DD_ROLE_HOST); ts_fill(&g_outgoing);
    g_town.send = outgoing_send;
    dd_action a = {0}; a.kind = DD_ACT_DISMISS; a.u.hero.hero_id = 7;
    CHECK(dd_town_host_local(&g_town, &a));
    CHECK(g_queue_failed && g_transport_sync_stop == 2);
    CHECK(g_guests[1].out.count == 1 && g_guests[2].out.count == 1);
    dd_runtime_poll();
    CHECK(g_town.needs_resync && g_sync_stop_sent);
    for (int i = 0; i < DD_MAX_GUESTS; i++) {
        ts_count = 0;
        CHECK(!session_send_pending(&c, guest_out(i), ts_send));
        CHECK(ts_count == 1 && ts_sent[0].type == DD_MSG_SYNC_STOP && ts_sent[0].u.load.slot == 2);
        CHECK(!guest_out(i)->count);
    }
    /* Simultaneously full queues, both roles, failed send: STOP never needs capacity. */
    for (int role = DD_ROLE_HOST; role <= DD_ROLE_GUEST; role++) {
        ts_reset(role);
        int peers = role == DD_ROLE_HOST ? 3 : 1;
        for (int i = 0; i < peers; i++) ts_fill(guest_out(i));
        runtime_sync_stop(3);
        for (int i = 0; i < peers; i++) {
            ts_count = 0;
            ts_fail = i == peers - 1;
            CHECK(!session_send_pending(&c, guest_out(i), ts_send));
            CHECK(ts_count == 1 && ts_sent[0].type == DD_MSG_SYNC_STOP && ts_sent[0].u.load.slot == 3);
            CHECK(!guest_out(i)->count);
        }
        /* Draining a queue or transient campaign unload cannot clear the latch. */
        dd_runtime_campaign_active(0);
        CHECK(g_transport_sync_stop == 3);
        queue_clear(&g_outgoing); ts_count = 0; ts_fail = 0;
        CHECK(!session_send_pending(&c, &g_outgoing, ts_send));
        CHECK(ts_count == 1 && ts_sent[0].type == DD_MSG_SYNC_STOP);
    }
    /* Stops after an in-flight message are sent ahead of the remaining backlog. */
    ts_reset(DD_ROLE_HOST);
    for (unsigned i = 0; i < 3; i++) {m.u.ping.time_ms = i;CHECK(queue_push(&g_outgoing, &m));}
    ts_latch_after_first = 1;
    CHECK(!session_send_pending(&c, &g_outgoing, ts_send));
    CHECK(ts_count == 2 && ts_sent[0].type == DD_MSG_PING && ts_sent[1].type == DD_MSG_SYNC_STOP && !g_outgoing.count);
    /* With no stop, ordinary messages still drain exactly once, in FIFO order. */
    ts_reset(DD_ROLE_HOST);
    for (unsigned i = 0; i < 3; i++) {m.u.ping.time_ms = i;CHECK(queue_push(&g_outgoing, &m));}
    CHECK(session_send_pending(&c, &g_outgoing, ts_send) && ts_count == 3 && !g_outgoing.count);
    for (unsigned i = 0; i < 3; i++) CHECK(ts_sent[i].type == DD_MSG_PING && ts_sent[i].u.ping.time_ms == i);
    CHECK(session_send_pending(&c, &g_outgoing, ts_send) && ts_count == 3);
    /* Runtime-local calls to net_send are protected too, not just outgoing_send. */
    ts_reset(DD_ROLE_GUEST); ts_fill(&g_outgoing);
    CHECK(!net_send(0, &m) && g_queue_failed && g_transport_sync_stop == 2);
    ts_reset(DD_ROLE_HOST);
    puts("Transport stop: partial/full three-peer broadcasts, direct send failure, in-flight stop priority, campaign unload and normal FIFO order verified");
}

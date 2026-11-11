/* Real control/ordered recruit bridge, synthetic bounded native hero model.
 * Every role is rebuilt independently; only codec-checked packets cross copies. */
static uint64_t rp_digest;
static unsigned rp_count, rp_reads;
static int rp_readable, rp_town;
static int rp_read(uint64_t *digest, unsigned *count) {
    rp_reads++;if (!rp_readable) return 0;*digest = rp_digest;*count = rp_count;return 1;
}
static int rp_here(void) {return rp_town;}
static void rp_reset(int role, unsigned player) {
    world_reset(role);g_pool_verified = 0;g_cfg.player = (int)player;
    for (unsigned i = 0; i < DD_MAX_GUESTS; i++) g_guests[i].ready = role == DD_ROLE_HOST;
    g_guest_count = role == DD_ROLE_HOST ? 3 : 0;
    rp_digest = UINT64_C(0x1298ba677665abcd);rp_count = 1;rp_reads = 0;
    rp_readable = rp_town = 1;g_pool_read = rp_read;g_pool_in_town = rp_here;
}
static int rp_deliver(dd_msg m, int host, unsigned player) {
    uint8_t bytes[DD_PROTO_HEADER + DD_PROTO_MAX_BODY];dd_msg back;size_t used = 0;
    size_t n = dd_encode(&m, bytes, sizeof bytes);CHECK(n != 0);
    if (!n) return 0;
    CHECK(dd_decode(bytes, n, &back, &used) == DD_DECODE_OK && used == n);
    return pool_receive(back, host, (int)player);
}
static dd_msg rp_start(uint32_t anchor) {
    rp_reset(DD_ROLE_HOST, 0);g_town.applied_seq = anchor;pool_start();
    CHECK(sent == 1 && last_message.type == DD_MSG_POOL && last_message.u.pool.op == 1 &&
          !last_message.seq && last_message.u.pool.epoch == 1 && last_message.u.pool.anchor == anchor &&
          last_message.u.pool.digest == rp_digest && last_message.u.pool.count == 1 &&
          g_pool_pending && !g_pool_verified && g_pool_expected == 14 && !g_pool_acks && !add_calls);
    return last_message;
}
static dd_msg rp_report(unsigned player, dd_msg probe) {
    rp_reset(DD_ROLE_GUEST, player);g_town.applied_seq = probe.u.pool.anchor;
    CHECK(rp_deliver(probe, 0, 0));pool_poll();
    CHECK(sent == 1 && last_message.type == DD_MSG_POOL && last_message.u.pool.op == 2 &&
          last_message.u.pool.player == player && last_message.u.pool.epoch == probe.u.pool.epoch &&
          last_message.u.pool.anchor == probe.u.pool.anchor && last_message.u.pool.digest == rp_digest &&
          g_pool_pending && !g_pool_verified && g_pool_acks && !add_calls);
    return last_message;
}
static dd_msg rp_host_release(dd_msg probe, dd_msg reports[3]) {
    dd_msg restarted = rp_start(probe.u.pool.anchor);
    CHECK(restarted.u.pool.digest == probe.u.pool.digest && restarted.u.pool.count == probe.u.pool.count);
    for (unsigned player = 1; player <= 3; player++) {
        CHECK(rp_deliver(reports[player - 1], 1, player));pool_poll();
        CHECK(!g_town.needs_resync && !add_calls);
        if (player < 3) CHECK(!g_pool_verified && g_pool_pending && sent == 1);
    }
    CHECK(g_pool_verified && !g_pool_pending && g_pool_acks == 14 && sent == 2 &&
          last_message.type == DD_MSG_POOL && last_message.u.pool.op == 3);
    return last_message;
}
static void rp_guest_release(unsigned player, dd_msg probe, dd_msg release) {
    (void)rp_report(player, probe);CHECK(rp_deliver(release, 0, 0));pool_poll();
    CHECK(g_pool_verified && !g_pool_pending && !g_town.needs_resync && !add_calls);
}
static dd_action rp_candidate(void) {
    dd_recruit s;dd_action a = {0};a.kind = DD_ACT_RECRUIT;a.u.recruit.hero_id = 40;
    CHECK(world_open(&s) && dd_recruit_fingerprint(&s, 40, &a.u.recruit.fingerprint) && a.u.recruit.fingerprint);
    return a;
}
static void rp_no_native(void) {
    CHECK(!add_calls && !finish_calls && !drop_calls && world_count == 1 && world_entries[0x15a8]);
}
static void rp_stopped(void) {
    CHECK(g_town.needs_resync && !g_pool_verified && g_transport_sync_stop);
    CHECK(strstr(g_shop_notice, "Resync") != NULL);rp_no_native();
}
static void recruit_pool_checks(void) {
    int (*old_read)(uint64_t *, unsigned *) = g_pool_read;int (*old_here)(void) = g_pool_in_town;
    uint64_t drag[2] = {0, (uintptr_t)world_entries};
    dd_msg probe, reports[3], release;

    /* Nobody may recruit before the barrier, including the host's own native
     * drag callback. A report alone does not release an individual guest. */
    for (unsigned player = 0; player < 4; player++) {
        rp_reset(player ? DD_ROLE_GUEST : DD_ROLE_HOST, player);
        CHECK(!hook_roster_drop(NULL, NULL, drag) && !sent && !g_pool_verified);rp_no_native();
    }
    probe = rp_start(0);
    CHECK(!hook_roster_drop(NULL, NULL, drag) && sent == 1);rp_no_native();
    for (unsigned player = 1; player <= 3; player++) {
        reports[player - 1] = rp_report(player, probe);
        CHECK(!hook_roster_drop(NULL, NULL, drag) && sent == 1);rp_no_native();
    }

    /* All three actual reports are required; duplicate/stale/future reports
     * cannot count as a missing player, and only the authenticated role counts. */
    (void)rp_start(0);
    CHECK(rp_deliver(reports[0], 1, 1));pool_poll();
    CHECK(rp_deliver(reports[0], 1, 1));pool_poll();
    CHECK(g_pool_acks == 2 && !g_pool_verified && sent == 1);
    dd_msg bad = reports[1];bad.u.pool.epoch++;
    CHECK(rp_deliver(bad, 1, 2));pool_poll();CHECK(g_pool_acks == 2 && !g_pool_verified);
    CHECK(!pool_receive(reports[1], 1, 3) && !pool_receive(probe, 1, 1) &&
          !pool_receive(reports[0], 0, 0));
    CHECK(rp_deliver(reports[1], 1, 2));pool_poll();CHECK(g_pool_acks == 6 && !g_pool_verified);
    CHECK(rp_deliver(reports[2], 1, 3));pool_poll();
    CHECK(g_pool_verified && sent == 2);release = last_message;
    CHECK(rp_deliver(reports[0], 1, 1));pool_poll();CHECK(g_pool_verified && sent == 2 && !g_town.needs_resync);

    /* Each of the four initiators creates one content-bearing ordered recruit;
     * the host and all three receivers add exactly the same stable hero once. */
    for (unsigned initiator = 0; initiator < 4; initiator++) {
        release = rp_host_release(probe, reports);
        dd_msg request = {0}, apply;
        if (initiator) {
            rp_guest_release(initiator, probe, release);
            CHECK(!hook_roster_drop(NULL, NULL, drag) && last_message.type == DD_MSG_TOWN_REQUEST);
            request = last_message;CHECK(request.u.town.action.u.recruit.fingerprint);rp_no_native();
            (void)rp_host_release(probe, reports);CHECK(queue_push(&g_incoming, &request));dd_runtime_poll();
        } else CHECK(!hook_roster_drop(NULL, NULL, drag));
        CHECK(!g_town.needs_resync && g_town.accepted == 1 && g_town.applied_seq == 1 &&
              add_calls == 1 && finish_calls == 1 && world_count == 2 && !world_entries[0x15a8] &&
              last_message.type == DD_MSG_TOWN_APPLY && last_message.u.town.action.u.recruit.fingerprint);
        apply = last_message;
        for (unsigned player = 1; player <= 3; player++) {
            rp_guest_release(player, probe, release);
            CHECK(queue_push(&g_incoming, &apply));dd_runtime_poll();
            CHECK(!g_town.needs_resync && g_town.applied_seq == 1 && add_calls == 1 &&
                  finish_calls == 1 && world_count == 2 && !world_entries[0x15a8]);
        }
    }

    /* A control message may arrive ahead of the ordered checkpoint. It is
     * reported only after that anchor and a stable local town, without ACKing
     * an action early. Controls do not overtake or mutate the game themselves. */
    probe = rp_start(5);rp_reset(DD_ROLE_GUEST, 2);g_town.apply = fake_apply;g_town.applied_seq = 4;
    rp_town = 0;CHECK(rp_deliver(probe, 0, 0));pool_poll();
    CHECK(g_pool_pending && !g_pool_acks && !sent && !rp_reads);rp_no_native();
    dd_msg ordered = {0};ordered.type = DD_MSG_TOWN_APPLY;ordered.seq = 5;ordered.u.town.request_id = 1;
    ordered.u.town.action.kind = DD_ACT_BUY_UPGRADE;ordered.u.town.action.u.upgrade.tree = 1;
    ordered.u.town.action.u.upgrade.code = 'a';
    CHECK(queue_push(&g_incoming, &ordered));dd_runtime_poll();
    CHECK(g_town.applied_seq == 5 && applied_count == 1 && !sent && !g_pool_verified);
    rp_town = 1;pool_poll();CHECK(sent == 1 && g_pool_acks && !g_pool_verified);
    bad = probe;bad.u.pool.op = 3;CHECK(rp_deliver(bad, 0, 0));pool_poll();
    CHECK(g_pool_verified && !g_town.needs_resync);

    /* A host's future post-raid PROBE can arrive while this copy still
     * displays the raid. Repeated raid frames must not erase that pending
     * control; only the next distinct raid invalidates the newly verified pool. */
    rp_reset(DD_ROLE_GUEST, 2);rp_town = 0;g_town.applied_seq = 10;pool_enter_raid();
    dd_msg crossing = probe;crossing.u.pool.epoch = 2;crossing.u.pool.anchor = 11;
    CHECK(rp_deliver(crossing, 0, 0));pool_poll();
    for (unsigned frame = 0; frame < 5; frame++) {
        pool_enter_raid();pool_poll();
        CHECK(g_pool_raid && g_pool_pending && g_pool_probe.type == DD_MSG_POOL &&
              g_pool_epoch == 2 && g_pool_probe.u.pool.anchor == 11 && !g_pool_acks && !sent && !rp_reads);
    }
    g_town.applied_seq = 11;pool_poll();CHECK(!sent && !g_pool_acks);
    rp_town = 1;pool_poll();CHECK(sent == 1 && g_pool_acks && !g_pool_verified && !g_pool_raid);
    crossing.u.pool.op = 3;CHECK(rp_deliver(crossing, 0, 0));pool_poll();
    CHECK(g_pool_verified && !g_pool_pending && !g_town.needs_resync);
    rp_town = 0;g_town.applied_seq = 12;pool_enter_raid();
    CHECK(g_pool_raid && !g_pool_verified && !g_pool_pending && !g_pool_probe.type && g_pool_floor == 12);
    CHECK(rp_deliver(crossing, 0, 0));pool_poll();CHECK(!g_pool_verified && !g_town.needs_resync);

    /* New pool checks cannot become a way to bypass native panel readiness. */
    probe = rp_start(0);for (unsigned player = 1; player <= 3; player++) reports[player - 1] = rp_report(player, probe);
    release = rp_host_release(probe, reports);rp_guest_release(1, probe, release);
    CHECK(!hook_roster_drop(NULL, NULL, drag));dd_msg request = last_message;
    (void)rp_host_release(probe, reports);g_recruit_can_apply = recruit_context_settled;
    g_recruit_ready_at = GetTickCount64() + 60000;
    CHECK(queue_push(&g_incoming, &request));dd_runtime_poll();
    CHECK(g_incoming.count == 1 && !g_town.accepted);rp_no_native();
    g_recruit_ready_at = 0;dd_runtime_poll();CHECK(add_calls == 1 && finish_calls == 1 && !g_town.needs_resync);

    /* Whole-pool mismatch/read failure stops before a single native recruit. */
    for (unsigned player = 1; player <= 3; player++) for (unsigned variant = 0; variant < 3; variant++) {
        rp_reset(DD_ROLE_GUEST, player);if (variant == 0) rp_digest ^= 1;
        if (variant == 1) rp_count++;if (variant == 2) rp_readable = 0;
        CHECK(rp_deliver(probe, 0, 0));pool_poll();rp_stopped();
    }
    for (unsigned variant = 0; variant < 3; variant++) {
        (void)rp_start(0);bad = reports[1];
        if (variant == 0) bad.u.pool.digest ^= 1;if (variant == 1) bad.u.pool.count++;
        if (variant == 2) bad.u.pool.anchor++;
        CHECK(rp_deliver(bad, 1, 2));pool_poll();rp_stopped();
    }
    rp_reset(DD_ROLE_HOST, 0);rp_readable = 0;pool_start();rp_stopped();CHECK(!sent);
    rp_reset(DD_ROLE_HOST, 0);g_pool_epoch = UINT32_MAX;pool_start();rp_stopped();CHECK(!sent);

    /* Re-read at release on both sides: state may change after our report. */
    for (unsigned role = DD_ROLE_HOST; role <= DD_ROLE_GUEST; role++) for (unsigned variant = 0; variant < 3; variant++) {
        if (role == DD_ROLE_HOST) {
            (void)rp_start(0);for (unsigned player = 1; player <= 2; player++) {CHECK(rp_deliver(reports[player - 1], 1, player));pool_poll();}
        } else (void)rp_report(1, probe);
        if (variant == 0) rp_digest ^= 1;if (variant == 1) rp_count++;if (variant == 2) rp_readable = 0;
        CHECK(rp_deliver(role == DD_ROLE_HOST ? reports[2] : release, role == DD_ROLE_HOST, role == DD_ROLE_HOST ? 3 : 0));
        pool_poll();rp_stopped();
    }

    /* Matching numeric hero IDs are insufficient after a successful pool
     * barrier. A content-bearing action with a changed local name is refused
     * before any clone/add, screen callback, event, save or roster mutation. */
    for (int role = DD_ROLE_HOST; role <= DD_ROLE_GUEST; role++) {
        if (role == DD_ROLE_HOST) (void)rp_host_release(probe, reports);else rp_guest_release(1, probe, release);
        dd_action a = rp_candidate();strcpy((char *)world_entries + 8, "Different candidate");
        CHECK(!runtime_apply(NULL, &a));rp_stopped();CHECK(!world_events && !world_saves);
    }

    /* Recruitment, provisioning and embark all wait at the same FIFO gate,
     * whether a host is deciding a guest request or a guest receives APPLY.
     * Passing the gate never pops the action or calls any native handler. */
    static const uint8_t gated_kinds[] = {DD_ACT_RECRUIT, DD_ACT_PREPARE, DD_ACT_EMBARK};
    for (int role = DD_ROLE_HOST; role <= DD_ROLE_GUEST; role++) for (unsigned kind = 0; kind < 3; kind++) {
        if (role == DD_ROLE_HOST) (void)rp_start(0);else (void)rp_report(1, probe);
        dd_msg wait = {0};wait.type = role == DD_ROLE_HOST ? DD_MSG_TOWN_REQUEST : DD_MSG_TOWN_APPLY;
        wait.seq = role == DD_ROLE_HOST ? 0 : 1;wait.u.town.request_id = 1;wait.u.town.action.kind = gated_kinds[kind];
        CHECK(queue_push(&g_incoming, &wait) && pool_apply_gate(&wait) && !g_pool_verified &&
              g_incoming.count == 1 && !g_town.applied_seq);rp_no_native();
        if (role == DD_ROLE_HOST) {
            for (unsigned player = 1; player <= 3; player++) CHECK(rp_deliver(reports[player - 1], 1, player));
        } else CHECK(rp_deliver(release, 0, 0));
        CHECK(!pool_apply_gate(&wait) && g_pool_verified && !g_pool_apply_wait &&
              g_incoming.count == 1 && !g_town.applied_seq && !g_town.needs_resync);rp_no_native();
    }

    /* RELEASE controls and an ordinary APPLY can arrive in separate queues
     * after the frame's first pool poll. Gate the exact FIFO head; a following
     * action cannot overtake while release is missing. Re-poll before stopping. */
    (void)rp_report(1, probe);
    dd_msg gated = {0}, following = {0}, head;
    gated.type = following.type = DD_MSG_TOWN_APPLY;gated.seq = 1;following.seq = 2;
    gated.u.town.request_id = 1;gated.u.town.action = rp_candidate();
    following.u.town.request_id = 2;following.u.town.action.kind = DD_ACT_BUY_UPGRADE;
    following.u.town.action.u.upgrade.tree = 1;following.u.town.action.u.upgrade.code = 'a';
    CHECK(queue_push(&g_incoming, &gated) && queue_push(&g_incoming, &following));
    CHECK(pool_apply_gate(&gated) && g_pool_apply_wait && !g_pool_verified &&
          g_incoming.count == 2 && !g_town.applied_seq);rp_no_native();
    CHECK(rp_deliver(release, 0, 0)); /* Do not call pool_poll before the gate. */
    CHECK(!pool_apply_gate(&gated) && g_pool_verified && !g_pool_apply_wait && g_incoming.count == 2);
    CHECK(queue_pop(&g_incoming, &head) && dd_town_guest_apply(&g_town, &head));
    CHECK(add_calls == 1 && finish_calls == 1 && g_town.applied_seq == 1 &&
          g_incoming.count == 1 && queue_peek(&g_incoming, &head) && head.seq == 2);
    (void)rp_report(1, probe);g_pool_apply_wait = GetTickCount64() - 1;
    CHECK(pool_apply_gate(&gated));rp_stopped();

    /* Duplicate controls are harmless; neither stale reports nor a previous
     * week's release can verify the next pool or cause a false sync stop. */
    rp_guest_release(3, probe, release);unsigned sent_before = (unsigned)sent;
    CHECK(rp_deliver(probe, 0, 0) && rp_deliver(release, 0, 0));pool_poll();
    CHECK(g_pool_verified && !g_town.needs_resync && (unsigned)sent == sent_before);
    g_town.applied_seq = 10;pool_enter_raid();
    CHECK(rp_deliver(release, 0, 0) && rp_deliver(probe, 0, 0));pool_poll();
    CHECK(!g_pool_verified && !g_pool_pending && !g_town.needs_resync && (unsigned)sent == sent_before);
    dd_msg next = probe;next.u.pool.epoch++;next.u.pool.anchor = 11;g_town.applied_seq = 11;
    CHECK(rp_deliver(next, 0, 0));pool_poll();CHECK(g_pool_pending && g_pool_acks && !g_pool_verified);
    CHECK(rp_deliver(release, 0, 0));pool_poll();CHECK(!g_pool_verified && !g_town.needs_resync);
    next.u.pool.op = 3;CHECK(rp_deliver(next, 0, 0));pool_poll();CHECK(g_pool_verified && !g_town.needs_resync);

    /* A release with no matching probe/report, changed anchor or future epoch
     * cannot grant recruitment. Neither can an expired or disconnected pool. */
    for (unsigned variant = 0; variant < 3; variant++) {
        rp_reset(DD_ROLE_GUEST, 1);if (variant) (void)rp_report(1, probe);
        bad = release;if (variant == 1) bad.u.pool.anchor++;if (variant == 2) bad.u.pool.epoch++;
        CHECK(rp_deliver(bad, 0, 0));pool_poll();rp_stopped();
    }
    for (int role = DD_ROLE_HOST; role <= DD_ROLE_GUEST; role++) {
        if (role == DD_ROLE_HOST) (void)rp_start(0);else (void)rp_report(1, probe);
        g_pool_deadline = GetTickCount64() - 1;pool_poll();rp_stopped();
    }
    (void)rp_start(0);g_guests[2].ready = 0;pool_poll();rp_stopped();
    rp_reset(DD_ROLE_GUEST, 1);g_pool_verified = 1;g_town.needs_resync = 1;
    CHECK(rp_deliver(probe, 0, 0));pool_poll();CHECK(g_pool_controls.count == 1 && !pool_recruit_ready());
    CHECK(!hook_roster_drop(NULL, NULL, drag));rp_no_native();
    rp_reset(DD_ROLE_GUEST, 1);
    for (unsigned i = 0; i < DD_QUEUE_CAP; i++) CHECK(rp_deliver(probe, 0, 0));
    CHECK(!pool_receive(probe, 0, 0) && g_pool_controls.count == DD_QUEUE_CAP);rp_no_native();

    /* Use the actual bounded outbound queues, not a failing send mock. If one
     * receiver's queue is full, even the host must never mark the pool verified
     * or admit a new native recruit in the same frame. Probe failure also stops. */
    for (unsigned phase = 0; phase < 2; phase++) {
        if (phase) {
            (void)rp_start(0);
            for (unsigned player = 1; player <= 3; player++) CHECK(rp_deliver(reports[player - 1], 1, player));
        } else rp_reset(DD_ROLE_HOST, 0);
        queue_clear(&g_outgoing);queue_clear(&g_guests[1].out);queue_clear(&g_guests[2].out);
        dd_msg filler = {0};filler.type = DD_MSG_PING;
        for (unsigned i = 0; i < DD_QUEUE_CAP; i++) CHECK(queue_push(&g_guests[2].out, &filler));
        g_town.send = outgoing_send;
        if (phase) pool_poll();else pool_start();
        CHECK(g_queue_failed && !g_pool_verified);rp_stopped();
        CHECK(!hook_roster_drop(NULL, NULL, drag));rp_no_native();
    }
    queue_clear(&g_outgoing);queue_clear(&g_guests[1].out);queue_clear(&g_guests[2].out);

    rp_reset(DD_ROLE_GUEST,1);g_town.applied_seq=probe.u.pool.anchor;
    queue_clear(&g_outgoing);dd_msg full={0};full.type=DD_MSG_PING;
    for(unsigned i=0;i<DD_QUEUE_CAP;i++)CHECK(queue_push(&g_outgoing,&full));
    g_town.send=outgoing_send;CHECK(rp_deliver(probe,0,0));pool_poll();
    CHECK(g_queue_failed && !g_pool_verified && !g_pool_acks);rp_stopped();rp_no_native();
    queue_clear(&g_outgoing);
    rp_reset(DD_ROLE_HOST, 0);g_pool_read = old_read;g_pool_in_town = old_here;pool_reset();
    puts("Stagecoach pool bridge: host+3 guests/all4 initiators, anchored probe/report/release, exact content before native recruit, delayed town/panel/order, duplicate/stale controls, mismatch/read failure, timeout/disconnect/STOP and bounded controls verified");
}

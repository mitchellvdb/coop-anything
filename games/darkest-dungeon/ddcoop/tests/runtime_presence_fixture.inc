/* Display-only 4-player controls under independent clocks, no native game. */
static unsigned presence_fake_code;
static unsigned presence_fake_native(void){return presence_fake_code;}
static void presence_checks(void) {
    reset(DD_ROLE_HOST);presence_clear(-1);
    unsigned (*saved)(void)=g_presence_native;g_presence_native=presence_fake_native;
    for(int i=0;i<3;i++)g_guests[i].ready=1;
    dd_msg m={0},snapshot={0},decoded={0};uint8_t wire[64];size_t used=0;
    for(int player=1;player<4;player++) {
        memset(&m,0,sizeof m);m.type=DD_MSG_PRESENCE;m.u.presence.mask=(uint8_t)(1u<<player);
        m.u.presence.place[player]=(uint8_t)(DD_PLACE_GUILD+player);
        CHECK(!presence_receive(&m,1,player%3+1,10000)); /* authenticated slot cannot be forged */
        CHECK(presence_receive(&m,1,player,10000));
        CHECK(!presence_receive(&m,0,player,10000)); /* wrong direction */
    }
    g_presence.code[0]=DD_PLACE_HAMLET;g_presence.at[0]=10000;
    presence_message(1,&snapshot,10250);
    CHECK(snapshot.u.presence.mask==15 && snapshot.u.presence.kind==1 && !snapshot.seq);
    size_t size=dd_encode(&snapshot,wire,sizeof wire);
    CHECK(size==22 && dd_decode(wire,size,&decoded,&used)==DD_DECODE_OK);
    CHECK(!presence_receive(&snapshot,1,1,10250));
    int old_sent=sent,old_applied=applied_count,old_alerts=alerts;
    uint32_t old_next=g_town.applied_seq;
    for(int player=1;player<4;player++) {
        g_cfg.role=DD_ROLE_GUEST;g_cfg.player=player;
        CHECK(presence_receive(&decoded,0,player,10250));
        CHECK(g_presence.mask==15 && g_presence.code[0]==DD_PLACE_HAMLET);
        for(int other=1;other<4;other++)if(other!=player)
            CHECK(g_presence.code[other]==DD_PLACE_GUILD+other);
    }
    CHECK(sent==old_sent && applied_count==old_applied && alerts==old_alerts && g_town.applied_seq==old_next);
    /* Old native samples become unknown even while network heartbeats continue. */
    presence_message(1,&snapshot,16001);
    for(int i=0;i<4;i++)CHECK(snapshot.u.presence.place[i]==0);
    CHECK(snapshot.u.presence.mask==15);
    presence_clear(2);g_guests[1].ready=0;
    presence_message(1,&snapshot,16001);CHECK(snapshot.u.presence.mask==11 && !snapshot.u.presence.place[2]);
    /* A reconnect starts with unknown, never the earlier building. */
    g_guests[1].ready=1;presence_message(1,&snapshot,16001);
    CHECK(snapshot.u.presence.mask==15 && !snapshot.u.presence.place[2]);
    for(int player=0;player<4;player++) {
        reset(player?DD_ROLE_GUEST:DD_ROLE_HOST);g_cfg.player=player;
        presence_clear(-1);g_presence_sample_at=0;
        presence_fake_code=DD_PLACE_GUILD;presence_poll();
        CHECK(overlay_presence_town && (overlay_presence_mask&(1u<<player)) && overlay_presence_codes[player]==DD_PLACE_GUILD);
        presence_fake_code=DD_PLACE_DUNGEON;g_presence_sample_at=0;presence_poll();CHECK(!overlay_presence_town);
        presence_fake_code=DD_PLACE_LOADING;g_presence_sample_at=0;presence_poll();CHECK(!overlay_presence_town);
        presence_fake_code=DD_PLACE_HAMLET;g_presence_sample_at=0;g_session_ready=0;presence_poll();
        CHECK(overlay_presence_town && overlay_presence_mask==(1u<<player));
        g_session_ready=1;g_presence.mask=15;g_presence.snapshot_at=1;g_presence_sample_at=0;presence_poll();
        if(player)for(int i=0;i<4;i++)if(i!=player)CHECK(!overlay_presence_codes[i]);
    }
    g_presence_native=saved;presence_clear(-1);g_presence_sample_at=0;reset(DD_ROLE_HOST);
    puts("Hamlet presence: authenticated all4 roles, 3 peers, wire roundtrip, no action/ACK/effect, expiry/disconnect/reconnect, dungeon/loading mode: OK");
}

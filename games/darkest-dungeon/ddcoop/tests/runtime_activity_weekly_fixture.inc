/* Weekly town processing reuses AssignHero after clearing the native record.
 * It is not another click on the shared Accept button. Exercise the real hook,
 * reservation model and synthetic native wallet/hero effects on all four roles. */
static int aw_weekly_caller(uintptr_t rva) {(void)rva;return 1;}
static int aw_ui_caller(uintptr_t rva) {(void)rva;return 0;}
static uint32_t aw_read32(const void *p) {uint32_t v=0;memcpy(&v,p,4);return v;}
static uint64_t aw_read64(const void *p) {uint64_t v=0;memcpy(&v,p,8);return v;}

static void aw_weekly_prepare(unsigned player,unsigned kind,unsigned slot,uint32_t hero,int committed) {
    ar_reset(player?DD_ROLE_GUEST:DD_ROLE_HOST,0);g_cfg.player=(int)player;
    if(committed) {
        putint(ar_records[kind][slot],hero);putint(ar_records[kind][slot]+4,1);
        ar_records[kind][slot][0x14]=1;
        putint(hm_entries[hero-1]+0x15b0,2);
        putptr(hm_entries[hero-1]+0x15c0,(uintptr_t)ar_activity[kind]);
    }
    CHECK(activity_seed());
    CHECK(g_activity_res.slots[kind][slot].phase==
          (committed?DD_ACTIVITY_RES_COMMITTED:DD_ACTIVITY_RES_EMPTY));
    /* The game's week transition clears/reassigns native occupants before the
     * bridge starts its next epoch. Its cached slot is deliberately not DRAFT. */
    memset(ar_records[kind][slot],0,sizeof ar_records[kind][slot]);
    putint(hm_entries[hero-1]+0x15b0,0);putptr(hm_entries[hero-1]+0x15c0,0);
    putint(ar_activity[kind]+DD_ACTIVITY_SLOT_OFFSET,slot);
}

static void aw_native_effect(unsigned kind,unsigned slot,uint32_t hero) {
    CHECK(ar_commits==1 && !ar_clears && !ar_unsafe);
    CHECK(aw_read32(ar_records[kind][slot])==hero &&
          aw_read32(ar_records[kind][slot]+4)==1 && ar_records[kind][slot][0x14]==1);
    CHECK(aw_read32(hm_entries[hero-1]+0x15b0)==2 &&
          aw_read64(hm_entries[hero-1]+0x15c0)==(uintptr_t)ar_activity[kind]);
    CHECK(aw_read32(hm_wallet)==15); /* ar_assign charges the modeled native cost once. */
}

static void activity_weekly_checks(void) {
    int (*old_caller)(uintptr_t)=g_activity_native_assign_caller;
    int (*old_kind)(void *)=g_activity_kind;
    void *(*old_model)(const dd_hamlet *,unsigned)=g_activity_model;
    assign_hero_fn old_assign=g_original_assign;
    CHECK(old_caller(0x53cbef));
    CHECK(!old_caller(0x53cbee) && !old_caller(0x53cbf0) && !old_caller(0x68b0e7) && !old_caller(0));

    for(unsigned player=0;player<4;player++)for(unsigned kind=0;kind<6;kind++) {
        unsigned slot=kind%3;uint32_t hero=slot+1;
        for(int committed=0;committed<=1;committed++) {
            aw_weekly_prepare(player,kind,slot,hero,committed);
            dd_activity_reservations before_model=g_activity_res;
            uint8_t before_wallet[sizeof hm_wallet],answer[DD_RESULT_BYTES],expected[DD_RESULT_BYTES]={1};
            memcpy(before_wallet,hm_wallet,sizeof before_wallet);memset(answer,0xa5,sizeof answer);
            g_activity_native_assign_caller=aw_weekly_caller;
            CHECK(hook_assign_hero(ar_activity[kind],answer,hero)==(uintptr_t)answer);
            CHECK(!memcmp(answer,expected,sizeof answer));aw_native_effect(kind,slot,hero);
            CHECK(!memcmp(before_wallet+4,hm_wallet+4,sizeof before_wallet-4));
            CHECK(!memcmp(&before_model,&g_activity_res,sizeof before_model));
            CHECK(!sent && !alerts && !g_town.accepted && !g_town.rejected && !g_town.applied_seq &&
                  !g_town.needs_resync && !g_incoming.count && !g_outgoing.count && !g_game_apply_depth);

            /* The same native state from an ordinary UI caller still requires
             * a matching shared draft; no whitelist for the whole function. */
            aw_weekly_prepare(player,kind,slot,hero,committed);
            g_activity_native_assign_caller=aw_ui_caller;
            memset(answer,0xa5,sizeof answer);memset(expected,0,sizeof expected);
            CHECK(hook_assign_hero(ar_activity[kind],answer,hero)==(uintptr_t)answer);
            CHECK(!memcmp(answer,expected,sizeof answer) && !ar_commits && !sent && alerts==1);
            CHECK(!aw_read32(ar_records[kind][slot]) && aw_read32(hm_wallet)==20 && !g_town.needs_resync);
        }
    }

    /* A real UI Accept remains ordered, including when any guest initiates.
     * Draft setup is an already-ordered reservation, never a native placement. */
    dd_msg accepted={0};
    for(unsigned player=0;player<4;player++) {
        ar_reset(player?DD_ROLE_GUEST:DD_ROLE_HOST,1);g_cfg.player=(int)player;
        g_activity_native_assign_caller=aw_ui_caller;
        dd_action reserve=ar_action(DD_ACTIVITY_RESERVE,1,2,3,1);
        CHECK(activity_apply(&reserve) && g_activity_res.slots[1][2].phase==DD_ACTIVITY_RES_DRAFT);
        putint(ar_activity[1]+DD_ACTIVITY_SLOT_OFFSET,2);
        uint8_t answer[DD_RESULT_BYTES];memset(answer,0xa5,sizeof answer);
        CHECK(hook_assign_hero(ar_activity[1],answer,3)==(uintptr_t)answer);
        CHECK(sent==1 && !alerts && !g_town.needs_resync);
        CHECK(last_message.u.town.action.kind==DD_ACT_ACTIVITY &&
              last_message.u.town.action.u.activity.op==DD_ACTIVITY_ACCEPT &&
              last_message.u.town.action.u.activity.activity==1 &&
              last_message.u.town.action.u.activity.slot==2 &&
              last_message.u.town.action.u.activity.hero_id==3 &&
              last_message.u.town.action.u.activity.revision==2);
        if(!player) {
            CHECK(last_message.type==DD_MSG_TOWN_APPLY && last_message.seq==1 && answer[0]==1);
            CHECK(g_town.accepted==1 && g_town.applied_seq==1 &&
                  g_activity_res.slots[1][2].phase==DD_ACTIVITY_RES_COMMITTED);
            aw_native_effect(1,2,3);accepted=last_message;
        } else {
            CHECK(last_message.type==DD_MSG_TOWN_REQUEST && !answer[0] && !ar_commits &&
                  aw_read32(hm_wallet)==20 && !aw_read32(ar_records[1][2]) &&
                  g_activity_res.slots[1][2].phase==DD_ACTIVITY_RES_DRAFT);
            ar_receive(accepted);aw_native_effect(1,2,3);
            CHECK(g_town.applied_seq==1 && !g_town.needs_resync && !alerts &&
                  g_activity_res.slots[1][2].phase==DD_ACTIVITY_RES_COMMITTED);
        }
    }

    for(unsigned player=0;player<4;player++) {
        /* STOP blocks outside weekly work even from the recognized caller. */
        aw_weekly_prepare(player,0,0,1,1);g_activity_native_assign_caller=aw_weekly_caller;
        g_town.needs_resync=1;uint8_t answer[DD_RESULT_BYTES]={0};
        CHECK(!hook_assign_hero(ar_activity[0],answer,1));
        CHECK(!ar_commits && !sent && aw_read32(hm_wallet)==20 && !aw_read32(ar_records[0][0]));
        /* Inside an already-ordered apply, use the native trampoline without
         * creating another request/observed broadcast or changing depth. */
        for(int stopped=0;stopped<=1;stopped++) {
            aw_weekly_prepare(player,0,0,1,1);g_activity_native_assign_caller=aw_ui_caller;
            g_town.needs_resync=stopped;g_game_apply_depth=1;
            CHECK(hook_assign_hero(ar_activity[0],answer,1)==(uintptr_t)answer);
            aw_native_effect(0,0,1);
            CHECK(!sent && !alerts && !g_town.accepted && !g_town.applied_seq && g_game_apply_depth==1);
            g_game_apply_depth=0;
        }
    }
    ar_reset(DD_ROLE_HOST,0);activity_reset();
    g_activity_native_assign_caller=old_caller;g_activity_kind=old_kind;g_activity_model=old_model;
    g_original_assign=old_assign;g_activity_user_clear=activity_user_clear;
    puts("Weekly activity: exact native caller, all4 roles/all6 stress activities, committed/empty cache, native effect/cost once, no network/notices; UI draft/order and STOP/ordered-apply isolation verified");
}

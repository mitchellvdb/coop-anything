/* Reproduce a reordered bag containing two stacks of the SAME item. The first
 * finalized stack must never be stolen as the source for a later repair slot. */
static void bag_duplicate_repair_checks(void) {
    dd_msg repair[DD_BAG_SLOTS+2];uint32_t expected;
    it_reset(DD_ROLE_HOST);it_bag_set();
    it_bag_slot(it_bag,0,12,11,21);it_bag_slot(it_bag,1,12,11,21);
    it_bag_slot(it_bag,2,2,12,22);it_bag_slot(it_bag,3,4,13,23);
    expected=walk_bag_hash(wk_view.raid);bag_send_all(&wk_view);
    unsigned n=sent;CHECK(n==10);memcpy(repair,sent_messages,n*sizeof *repair);
    for(int copy=0;copy<3;copy++) {
        it_reset(DD_ROLE_GUEST);it_bag_set();
        if(!copy) {it_bag_slot(it_bag,0,2,12,22);it_bag_slot(it_bag,1,4,13,23);
            it_bag_slot(it_bag,2,7,11,21);it_bag_slot(it_bag,3,3,11,21);}
        else if(copy==1) {it_bag_slot(it_bag,0,2,12,22);it_bag_slot(it_bag,1,4,13,23);
            it_bag_slot(it_bag,2,7,11,21);} /* Only one local exemplar: safe native copy needed. */
        else {it_bag_slot(it_bag,0,12,11,21);it_bag_slot(it_bag,1,12,11,21);
            it_bag_slot(it_bag,2,2,12,22);it_bag_slot(it_bag,3,4,13,23);}
        for(unsigned i=0;i<n;i++)wk_push(i+1,&repair[i].u.town.action);
        dd_runtime_poll();
        CHECK(!g_town.needs_resync && !g_bag_repairing && walk_bag_hash(wk_view.raid)==expected);
        CHECK(it_count(it_bag,0)==12 && it_count(it_bag,1)==12);
        CHECK(it_bag[0x100]==0x40+11 && it_bag[DD_ITEM_SIZE+0x100]==0x40+11 && !hm_item_alive);
    }
    /* Right after a door (owner's runs 11 Nov 16:57, 12 Nov 19:15 and 19:32): the new area's loot is not readable
     * yet. The repair starts from the bag alone instead of stopping both copies. */
    {
        uint64_t bad_begin = 16, bad_end = 8;
        it_reset(DD_ROLE_GUEST);it_bag_set();
        it_bag_slot(it_bag,0,2,12,22);it_bag_slot(it_bag,1,4,13,23);it_bag_slot(it_bag,2,7,11,21);it_bag_slot(it_bag,3,3,11,21);
        memcpy(wk_raid_lists + DD_WALK_RAID_LOOT + 8, &bad_begin, 8);
        memcpy(wk_raid_lists + DD_WALK_RAID_LOOT + 0x10, &bad_end, 8);
        for(unsigned i=0;i<n;i++)wk_push(i+1,&repair[i].u.town.action);
        dd_runtime_poll();
        CHECK(!g_town.needs_resync && !g_bag_repairing && walk_bag_hash(wk_view.raid)==expected && !g_incoming.count);
        CHECK(it_count(it_bag,0)==12 && it_count(it_bag,1)==12 && !hm_item_alive);
    }
    /* The bag itself not readable for a moment (still loading): the repair waits, then runs. */
    {
        it_reset(DD_ROLE_GUEST);it_bag_set();
        it_bag_slot(it_bag,0,2,12,22);it_bag_slot(it_bag,1,4,13,23);it_bag_slot(it_bag,2,7,11,21);it_bag_slot(it_bag,3,3,11,21);
        it_bag_clear_lists();
        { uint64_t bad_begin = 16, bad_end = 8;
          memcpy(wk_raid_lists + DD_WALK_RAID_INVENTORY + 8, &bad_begin, 8);
          memcpy(wk_raid_lists + DD_WALK_RAID_INVENTORY + 0x10, &bad_end, 8); }
        for(unsigned i=0;i<n;i++)wk_push(i+1,&repair[i].u.town.action);
        dd_runtime_poll();
        CHECK(!g_town.needs_resync && g_incoming.count==n && !g_bag_repairing);
        uint8_t saved[sizeof it_bag];memcpy(saved,it_bag,sizeof saved);
        it_bag_set();memcpy(it_bag,saved,sizeof saved);
        dd_runtime_poll();
        CHECK(!g_town.needs_resync && !g_bag_repairing && !g_incoming.count && walk_bag_hash(wk_view.raid)==expected);
    }
    bt_unwire();
    puts("Duplicate-stack repair: reordered/fewer/local-matching stacks converge without stealing finalized slots; native Item metadata/lifetime and final hash checked; repairs right after a door (loot not yet readable, bag briefly unreadable) wait or run, never stop");
}

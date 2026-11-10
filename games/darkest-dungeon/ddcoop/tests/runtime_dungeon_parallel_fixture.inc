/* Independent four-copy future-core simulation. Native effects are modeled;
 * no live DNG1 adapter or separate hunger command is claimed. */
typedef struct {dd_dungeon nodes[4];world worlds[4];dd_dng_departure offer;unsigned commits;} four_dng;
static const unsigned four_ack_order[6][3] = {{1,2,3},{1,3,2},{2,1,3},{2,3,1},{3,1,2},{3,2,1}};
static void four_release(four_dng *f, const unsigned order[3]) {
    dd_dungeon *h = f->nodes;
    for (unsigned j = 0; j < 3; j++) {
        unsigned p = order[j];
        dd_dng_message ack = {0};ack.type = DD_DNG_ACK;ack.expedition = RUN_ID;ack.sequence = h->sequence;
        memcpy(ack.u.hash, f->nodes[p].checkpoint.hash, 32);ack = wire(ack);
        OK(dd_dungeon_ack(h,p,ack.expedition,ack.sequence,ack.u.hash));
        if (j < 2) CHECK(dd_dungeon_release(h,0,RUN_ID,h->sequence,h->checkpoint.hash) == DD_DNG_BUSY);
    }
    dd_dng_message release = {0};release.type = DD_DNG_RELEASE;release.expedition = RUN_ID;release.sequence = h->sequence;
    memcpy(release.u.hash,h->checkpoint.hash,32);release = wire(release);
    for (unsigned i = 0; i < 4; i++) OK(dd_dungeon_release(&f->nodes[i],0,release.expedition,release.sequence,release.u.hash));
}
static void four_start(four_dng *f, unsigned scene) {
    memset(f,0,sizeof *f);f->offer = departure();f->offer.players = 4;f->offer.connected = 15;
    for (unsigned i = 0; i < 4; i++) f->offer.owners[i] = (uint8_t)i;
    for (unsigned i = 0; i < 4; i++) {
        world *w = &f->worlds[i];
        for (unsigned j = 0; j < 4; j++) {w->hp[j] = 30;w->stress[j] = 8;}
        w->monster_hp[0] = w->monster_hp[1] = 100;w->gold = 500;w->provisions = 12;
        w->rules = f->offer.rules_seed;w->battle = f->offer.battle_seed;w->quest = f->offer.quest_seed;
    }
    dd_dng_checkpoint town = checkpoint(&f->worlds[0]);memcpy(f->offer.town_hash,town.hash,32);
    dd_dng_message msg = {0};msg.type = DD_DNG_OFFER;msg.expedition = RUN_ID;msg.u.offer = f->offer;msg = wire(msg);
    for (unsigned i = 0; i < 4; i++) {
        dd_dungeon_init(&f->nodes[i],i,native_validate,&f->worlds[i]);
        OK(dd_dungeon_offer(&f->nodes[i],0,msg.expedition,&msg.u.offer,now));
        OK(dd_dungeon_local_ready(&f->nodes[i],RUN_ID,town.hash,f->offer.content_hash));
        if (i) OK(dd_dungeon_ready(f->nodes,i,RUN_ID,town.hash,f->offer.content_hash));
    }
    for (unsigned i = 0; i < 4; i++) {
        OK(dd_dungeon_launch(&f->nodes[i],0,RUN_ID,now));f->worlds[i].scene = scene;f->worlds[i].location = 1;
        if (scene == DD_DNG_BATTLE) {f->worlds[i].encounter = 17;f->worlds[i].turn = 1;f->worlds[i].actor_kind = DD_DNG_MONSTER;f->worlds[i].actor = 9001;}
    }
    msg.type = DD_DNG_CHECKPOINT;msg.u.checkpoint = checkpoint(&f->worlds[0]);msg = wire(msg);
    OK(dd_dungeon_host_checkpoint(f->nodes,&msg.u.checkpoint,now));
    for (unsigned i = 1; i < 4; i++) {
        OK(dd_dungeon_guest_checkpoint(&f->nodes[i],0,RUN_ID,&msg.u.checkpoint,now));
        dd_dng_checkpoint actual = checkpoint(&f->worlds[i]);OK(dd_dungeon_guest_complete(&f->nodes[i],&actual,now));
    }
    four_release(f,four_ack_order[0]);
}
static dd_dng_result four_request(four_dng *f,unsigned from,const dd_dng_command *c) {
    dd_dng_message msg = {0};msg.type = DD_DNG_COMMAND;msg.expedition = RUN_ID;msg.sequence = f->nodes[0].sequence;msg.u.command = *c;msg = wire(msg);
    return dd_dungeon_request(f->nodes,from,msg.expedition,msg.sequence,&msg.u.command,now);
}
static void four_transact(four_dng *f,unsigned from,dd_dng_command c,const unsigned order[3]) {
    dd_dungeon *h = f->nodes;unsigned executions = f->worlds[0].executions;
    dd_dng_result accepted = four_request(f,from,&c);
    if (accepted != DD_DNG_OK) printf("Parallel request refused: player=%u kind=%u result=%u\n",from,c.kind,accepted);
    OK(accepted);CHECK(f->worlds[0].executions == executions);
    native_execute(&f->worlds[0],&h->pending.command);dd_dng_checkpoint after = checkpoint(&f->worlds[0]);
    dd_dng_message msg = {0};msg.type = DD_DNG_EVENT;msg.expedition = RUN_ID;
    OK(dd_dungeon_host_complete(h,&after,++now,&msg.u.event));msg.sequence = h->sequence;msg = wire(msg);
    CHECK(four_request(f,from,&c) == DD_DNG_BUSY);
    for (unsigned j = 0; j < 3; j++) {
        unsigned i = order[2-j];dd_dungeon *g = &f->nodes[i];
        OK(dd_dungeon_guest_event(g,0,RUN_ID,msg.sequence,&msg.u.event,now));
        native_execute(&f->worlds[i],&msg.u.event.command);dd_dng_checkpoint actual = checkpoint(&f->worlds[i]);
        OK(dd_dungeon_guest_complete(g,&actual,now));
        CHECK(dd_dungeon_guest_event(g,0,RUN_ID,msg.sequence,&msg.u.event,now) == DD_DNG_DUPLICATE);
    }
    CHECK(dd_dungeon_release(h,0,RUN_ID,h->sequence,h->checkpoint.hash) == DD_DNG_BUSY);
    four_release(f,order);
    for (unsigned i = 1; i < 4; i++) {dd_dng_checkpoint actual = checkpoint(&f->worlds[i]);CHECK(dd_dng_checkpoint_equal(&h->checkpoint,&actual));}
    f->commits++;
}
static void test_four_parallel(void) {
    unsigned total = 0;
    for (unsigned from = 0; from < 4; from++) for (unsigned order = 0; order < 6; order++) {
        four_dng f;four_start(&f,DD_DNG_EXPLORE);
        dd_dng_command c = command(f.nodes,DD_DNG_MOVE,0,1);
        CHECK(four_request(&f,4,&c) == DD_DNG_INVALID);
        four_transact(&f,from,c,four_ack_order[order]);
        c = command(f.nodes,DD_DNG_USE_ITEM,101+from,2);four_transact(&f,from,c,four_ack_order[order]);
        c = command(f.nodes,DD_DNG_CURIO,101+from,3);four_transact(&f,from,c,four_ack_order[order]);
        c = command(f.nodes,DD_DNG_FINISH_QUEST,0,4);four_transact(&f,from,c,four_ack_order[order]);
        CHECK(f.nodes[0].checkpoint.scene == DD_DNG_RETURNING);
        c = command(f.nodes,DD_DNG_RETURN_TOWN,0,5);four_transact(&f,from,c,four_ack_order[order]);
        for (unsigned i = 0; i < 4; i++) {OK(dd_dungeon_finish_return(&f.nodes[i]));CHECK(dd_dungeon_town_allowed(&f.nodes[i]));}
        total += f.commits;
    }
    four_dng f;four_start(&f,DD_DNG_BATTLE);
    dd_dng_command ai = command(f.nodes,DD_DNG_MONSTER_SKILL,9001,1);
    for (unsigned p = 1; p < 4; p++) CHECK(four_request(&f,p,&ai) == DD_DNG_OWNER);
    four_transact(&f,0,ai,four_ack_order[5]);
    dd_dng_command hero = command(f.nodes,DD_DNG_SKILL,101,2);
    CHECK(four_request(&f,1,&hero) == DD_DNG_OWNER);
    printf("PASS four-copy parallel core: %u actions, all4 initiators, six ACK orders, MOVE/owned food/curio/finish/return; monster AI host-only and hero ownership retained\n",total+f.commits);
}

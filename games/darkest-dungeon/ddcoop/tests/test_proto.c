/* Unit and fuzz tests for src/dd_proto.c. Build and run on Linux:
 *   gcc -std=c11 -O1 -g -Wall -Wextra -fsanitize=address,undefined -Isrc -o build/test_proto tests/test_proto.c src/dd_proto.c
 *   build/test_proto */
#include "dd_proto.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(cond, ...)                                \
    do {                                                \
        if (!(cond)) {                                  \
            failures++;                                 \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                        \
            printf("\n");                               \
        }                                               \
    } while (0)

/* Deterministic generator for the fuzz test (same input every run). */
static uint32_t rng_state = 12345;
static uint32_t rnd(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

static dd_msg town(uint16_t type, dd_action a) {
    dd_msg m;
    memset(&m, 0, sizeof m);
    m.type = type;
    m.seq = 77;
    m.u.town.request_id = 4242;
    m.u.town.action = a;
    return m;
}

static dd_action act(uint8_t kind) {
    dd_action a;
    memset(&a, 0, sizeof a);
    a.kind = kind;
    switch (kind) {
    case DD_ACT_ACTIVITY: a.u.activity.op=1;a.u.activity.activity=DD_ACTIVITY_BAR;a.u.activity.slot=0;a.u.activity.hero_id=7;a.u.activity.revision=1;a.u.activity.epoch=1;break;
    case DD_ACT_ASSIGN_HERO: a.u.assign.activity = DD_ACTIVITY_MEDITATION; a.u.assign.slot = 2; a.u.assign.hero_id = 7; break;
    case DD_ACT_BUY_UPGRADE: a.u.upgrade.tree = 0x3fb36379; a.u.upgrade.code = '0'; a.u.upgrade.hero_id = 1; break;
    case DD_ACT_RECRUIT: a.u.hero.hero_id = 30; break;
    case DD_ACT_DISMISS: a.u.hero.hero_id = 30; break;
    case DD_ACT_RENAME: a.u.rename.hero_id=1;strcpy(a.u.rename.before,"Hero");strcpy(a.u.rename.after,"Hero B");break;
    case DD_ACT_SKILL: a.u.skill.hero_id=1;a.u.skill.after=1;break;
    case DD_ACT_TRINKET_MOVE: a.u.move.src_type=1;a.u.move.src_id=1;a.u.move.dst_slot=1;break;
    case DD_ACT_FORMATION_RESET: a.u.formation.area=7;a.u.formation.before[0]=1;a.u.formation.before[1]=2;a.u.formation.after[0]=2;a.u.formation.after[1]=1;break;
    case DD_ACT_PARTY: a.u.party.before[0]=1;a.u.party.after[1]=1;break;
    case DD_ACT_SUPPLIES: a.u.supplies.op=DD_SUPPLY_BUY;a.u.supplies.type=0x72f14069;
        a.u.supplies.gold=75;memset(a.u.supplies.before,0x5a,32);break;
    case DD_ACT_DEPARTURE: a.u.departure.seed=12345;a.u.departure.rules=54321;a.u.departure.quest=7;a.u.departure.quest_index=2;a.u.departure.heroes[0]=8;break;
    case DD_ACT_TRAVEL: a.u.travel.dest[0]=77;a.u.travel.dest[1]=3;a.u.travel.dest[2]=1;a.u.travel.from_area=9;a.u.travel.rules=0x1234;a.u.travel.torch=0x42c80000;a.u.travel.bag=0x99;for(unsigned i=0;i<4;i++){a.u.travel.ids[i]=10+i;a.u.travel.hp[i]=0x41a00000u+i;a.u.travel.stress[i]=0x42000000u+i;a.u.travel.trinkets[i]=0xabc00000u+i;}a.u.travel.has_from=1;a.u.travel.flip=1;break;
    case DD_ACT_USE: a.u.use.area=77;a.u.use.prop=1;a.u.use.prop_kind=1;a.u.use.option=2;a.u.use.hero=3;a.u.use.door=1;break;
    case DD_ACT_EMBARK:
    case DD_ACT_PREPARE: a.u.prepare.index=1;a.u.prepare.key=8;break;
    case DD_ACT_OWNERS: a.u.owners.heroes[0]=1;a.u.owners.mask=1;a.u.owners.revision=5;break;
    case DD_ACT_ITEM: a.u.item.area=77;a.u.item.pos=0x43fa0000;a.u.item.round=2;a.u.item.turn=5;a.u.item.rules=9;a.u.item.amount=0xc0c00000;a.u.item.type=4;a.u.item.id=6;a.u.item.slot=3;a.u.item.op=DD_ITEM_OP_TORCH;a.u.item.state=0x1e;a.u.item.hero=255;a.u.item.inv=1;a.u.item.flag=1;break;
    case DD_ACT_BATTLE: a.u.battle.actor_id=13;a.u.battle.round=2;a.u.battle.turn=5;a.u.battle.rules=0x12345;a.u.battle.value=-1;a.u.battle.op=DD_BATTLE_OP_ENEMY;a.u.battle.state=0x1e;a.u.battle.hero=3;a.u.battle.flag=1;a.u.battle.side=1;a.u.battle.place=2;break;
    case DD_ACT_WAGON_BUY:
    case DD_ACT_WAGON_SELL:
        a.u.items.slot=3; a.u.items.count=1; a.u.items.type=1; a.u.items.id=4; a.u.items.cost=500;
        break;
    case DD_ACT_HEIRLOOM_EXCHANGE:
        a.u.exchange.src_type = 15176862; a.u.exchange.src_amount = 8;
        a.u.exchange.dst_type = 798419517; a.u.exchange.dst_amount = 2;
        break;
    default: a.u.items.slot = 3; a.u.items.count = 2;
    }
    return a;
}

static int same_msg(const dd_msg *a, const dd_msg *b) {
    uint8_t ea[DD_PROTO_HEADER + DD_PROTO_MAX_BODY], eb[DD_PROTO_HEADER + DD_PROTO_MAX_BODY];
    size_t na = dd_encode(a, ea, sizeof ea), nb = dd_encode(b, eb, sizeof eb);
    return na && na == nb && memcmp(ea, eb, na) == 0;
}

static void test_roundtrip(void) {
    uint8_t buf[512];
    size_t used;
    dd_msg out;

    for (uint8_t k = 1; k < DD_ACT_COUNT_; k++) {
        for (uint16_t type = DD_MSG_TOWN_REQUEST; type <= DD_MSG_TOWN_APPLY; type++) {
            dd_msg m = town(type, act(k));
            size_t n = dd_encode(&m, buf, sizeof buf);
            CHECK(n > DD_PROTO_HEADER, "encode action %u type %u", k, type);
            CHECK(dd_decode(buf, n, &out, &used) == DD_DECODE_OK && used == n, "decode action %u", k);
            CHECK(out.type == type && out.seq == 77 && out.u.town.request_id == 4242 && out.u.town.action.kind == k,
                  "fields of action %u", k);
            CHECK(same_msg(&m, &out), "action %u round trip", k);
        }
    }
    dd_msg upg = town(DD_MSG_TOWN_REQUEST, act(DD_ACT_BUY_UPGRADE));
    size_t n = dd_encode(&upg, buf, sizeof buf);
    dd_decode(buf, n, &out, &used);
    CHECK(out.u.town.action.u.upgrade.tree == 0x3fb36379 && out.u.town.action.u.upgrade.code == '0' &&
              out.u.town.action.u.upgrade.hero_id == 1, "upgrade payload");

    dd_msg h;
    memset(&h, 0, sizeof h);
    h.type = DD_MSG_HELLO;
    h.u.hello.protocol = DD_PROTO_VERSION;
    strcpy(h.u.hello.mod_version, "0.1.0");
    memcpy(h.u.hello.build, "\x4d\x78\xfb\xfa\xa6\x5b\x7d\x00", 8);
    strcpy(h.u.hello.name, "host");
    n = dd_encode(&h, buf, sizeof buf);
    CHECK(dd_decode(buf, n, &out, &used) == DD_DECODE_OK && strcmp(out.u.hello.name, "host") == 0 &&
              memcmp(out.u.hello.build, h.u.hello.build, 8) == 0, "hello round trip");

    dd_msg rj;
    memset(&rj, 0, sizeof rj);
    rj.type = DD_MSG_TOWN_REJECT;
    rj.u.reject.request_id = 9;
    rj.u.reject.reason = DD_REJECT_NOT_ALLOWED;
    n = dd_encode(&rj, buf, sizeof buf);
    CHECK(dd_decode(buf, n, &out, &used) == DD_DECODE_OK && out.u.reject.reason == DD_REJECT_NOT_ALLOWED, "reject");

    dd_msg p;
    memset(&p, 0, sizeof p);
    {
        dd_msg w, back;
        size_t walk_used = 0, walk_len;
        memset(&w, 0, sizeof w);
        w.type = DD_MSG_WALK;
        w.u.walk.area = 0x1234;
        w.u.walk.pos = 0x43b40000u; /* 360.0 */
        walk_len = dd_encode(&w, buf, sizeof buf);
        CHECK(walk_len && dd_decode(buf, walk_len, &back, &walk_used) == DD_DECODE_OK && back.u.walk.area == 0x1234 &&
              back.u.walk.pos == 0x43b40000u, "walk round trip");
        w.u.walk.pos = 0x7f800000u;
        walk_len = dd_encode(&w, buf, sizeof buf);
        CHECK(walk_len && dd_decode(buf, walk_len, &back, &walk_used) == DD_DECODE_BAD_VALUE, "walk refuses a position that is no number");
        memset(&w, 0, sizeof w);
        w.type = DD_MSG_WALK_INTENT;
        w.u.intent.direction = -1;
        walk_len = dd_encode(&w, buf, sizeof buf);
        CHECK(walk_len && dd_decode(buf, walk_len, &back, &walk_used) == DD_DECODE_OK && back.u.intent.direction == -1, "walk intent round trip");
        w.u.intent.direction = 2;
        walk_len = dd_encode(&w, buf, sizeof buf);
        CHECK(walk_len && dd_decode(buf, walk_len, &back, &walk_used) == DD_DECODE_BAD_VALUE, "walk intent is only a direction");
        /* At the save screen: a menu slot, 1 to 9, nothing else. */
        memset(&w, 0, sizeof w);
        w.type = DD_MSG_LOAD;
        w.u.load.slot = 9;
        walk_len = dd_encode(&w, buf, sizeof buf);
        CHECK(walk_len && dd_decode(buf, walk_len, &back, &walk_used) == DD_DECODE_OK && back.type == DD_MSG_LOAD &&
              back.u.load.slot == 9, "load round trip");
        for (uint32_t bad = 0; bad < 12; bad += 10) {
            w.u.load.slot = bad;
            walk_len = dd_encode(&w, buf, sizeof buf);
            CHECK(walk_len && dd_decode(buf, walk_len, &back, &walk_used) == DD_DECODE_BAD_VALUE, "load slot %u refused", bad);
        }
        /* A guest's busy state: 0 or 1. */
        memset(&w, 0, sizeof w);
        w.type = DD_MSG_BUSY;
        w.u.busy.busy = 1;
        walk_len = dd_encode(&w, buf, sizeof buf);
        CHECK(walk_len && dd_decode(buf, walk_len, &back, &walk_used) == DD_DECODE_OK && back.type == DD_MSG_BUSY && back.u.busy.busy == 1, "busy round trip");
        w.u.busy.busy = 2;
        walk_len = dd_encode(&w, buf, sizeof buf);
        CHECK(walk_len && dd_decode(buf, walk_len, &back, &walk_used) == DD_DECODE_BAD_VALUE, "busy is 0 or 1");
    }
    p.type = DD_MSG_PING;
    p.u.ping.time_ms = 0x0102030405060708ULL;
    n = dd_encode(&p, buf, sizeof buf);
    CHECK(dd_decode(buf, n, &out, &used) == DD_DECODE_OK && out.u.ping.time_ms == 0x0102030405060708ULL, "ping 64-bit");
}

static void test_invalid_encode(void) {
    uint8_t buf[512];
    dd_action bad = act(DD_ACT_ASSIGN_HERO);
    bad.u.assign.slot = 3;
    dd_msg m = town(DD_MSG_TOWN_REQUEST, bad);
    CHECK(dd_encode(&m, buf, sizeof buf) == 0, "slot 3 must not encode");
    bad = act(DD_ACT_RECRUIT);
    bad.u.hero.hero_id = 0;
    m = town(DD_MSG_TOWN_REQUEST, bad);
    CHECK(dd_encode(&m, buf, sizeof buf) == 0, "hero 0 must not encode");
    bad = act(DD_ACT_HEIRLOOM_EXCHANGE);
    bad.u.exchange.src_amount = -5;
    m = town(DD_MSG_TOWN_REQUEST, bad);
    CHECK(dd_encode(&m, buf, sizeof buf) == 0, "negative exchange must not encode");
    bad=act(DD_ACT_WAGON_BUY);bad.u.items.id=0;
    m=town(DD_MSG_TOWN_REQUEST,bad);
    CHECK(dd_encode(&m,buf,sizeof buf)==0,"wagon identity required");
    bad=act(DD_ACT_WAGON_BUY);bad.u.items.count=2;
    m=town(DD_MSG_TOWN_REQUEST,bad);
    CHECK(dd_encode(&m,buf,sizeof buf)==0,"wagon single item only");
    bad=act(DD_ACT_WAGON_BUY);bad.u.items.slot=128;
    m=town(DD_MSG_TOWN_REQUEST,bad);
    CHECK(dd_encode(&m,buf,sizeof buf)==0,"wagon bounded slot");
    bad=act(DD_ACT_WAGON_BUY);bad.u.items.cost=0x80000000u;
    m=town(DD_MSG_TOWN_REQUEST,bad);
    CHECK(dd_encode(&m,buf,sizeof buf)==0,"wagon bounded price");
    bad=act(DD_ACT_WAGON_SELL);bad.u.items.slot=9999;
    m=town(DD_MSG_TOWN_REQUEST,bad);CHECK(!dd_encode(&m,buf,sizeof buf),"estate slot bound");
    bad=act(DD_ACT_WAGON_SELL);bad.u.items.id=0;
    m=town(DD_MSG_TOWN_REQUEST,bad);CHECK(!dd_encode(&m,buf,sizeof buf),"sale needs item identity");
    bad=act(DD_ACT_WAGON_SELL);bad.u.items.cost=0;
    m=town(DD_MSG_TOWN_REQUEST,bad);CHECK(!dd_encode(&m,buf,sizeof buf),"sale proceeds must be positive");
    bad=act(DD_ACT_WAGON_SELL);bad.u.items.count=2;
    m=town(DD_MSG_TOWN_REQUEST,bad);CHECK(!dd_encode(&m,buf,sizeof buf),"whole-stack sale restricted to one trinket");
    bad=act(DD_ACT_RENAME);memset(bad.u.rename.after,'x',64);
    m=town(DD_MSG_TOWN_REQUEST,bad);CHECK(!dd_encode(&m,buf,sizeof buf),"unterminated name refused");
    bad=act(DD_ACT_RENAME);strcpy(bad.u.rename.after,"\xc0\xaf");
    m=town(DD_MSG_TOWN_REQUEST,bad);CHECK(!dd_encode(&m,buf,sizeof buf),"overlong UTF-8 refused");
    bad=act(DD_ACT_SKILL);bad.u.skill.index=32;
    m=town(DD_MSG_TOWN_REQUEST,bad);CHECK(!dd_encode(&m,buf,sizeof buf),"bounded skill index");
    bad=act(DD_ACT_TRINKET_MOVE);bad.u.move.src_hero=1;bad.u.move.src_slot=2;
    m=town(DD_MSG_TOWN_REQUEST,bad);CHECK(!dd_encode(&m,buf,sizeof buf),"hero trinket slot bound");
    bad=act(DD_ACT_PARTY);bad.u.party.after[0]=1;bad.u.party.after[1]=1;
    m=town(DD_MSG_TOWN_REQUEST,bad);CHECK(!dd_encode(&m,buf,sizeof buf),"duplicate party hero refused");
    bad.kind = 200;
    m = town(DD_MSG_TOWN_REQUEST, bad);
    CHECK(dd_encode(&m, buf, sizeof buf) == 0, "unknown action must not encode");
    m = town(DD_MSG_TOWN_REQUEST, act(DD_ACT_DISMISS));
    CHECK(dd_encode(&m, buf, 10) == 0, "too small a buffer");
}

static void test_decode_errors(void) {
    uint8_t buf[512];
    size_t used;
    dd_msg out;
    dd_msg m = town(DD_MSG_TOWN_REQUEST, act(DD_ACT_DISMISS));
    size_t n = dd_encode(&m, buf, sizeof buf);

    for (size_t cut = 0; cut < n; cut++)
        CHECK(dd_decode(buf, cut, &out, &used) == DD_DECODE_SHORT, "truncated at %zu must be SHORT", cut);

    uint8_t c[512];
    memcpy(c, buf, n); c[0] ^= 1;
    CHECK(dd_decode(c, n, &out, &used) == DD_DECODE_BAD_MAGIC, "magic");
    memcpy(c, buf, n); c[4] = 99;
    CHECK(dd_decode(c, n, &out, &used) == DD_DECODE_BAD_VERSION, "version");
    memcpy(c, buf, n); c[6] = 99;
    CHECK(dd_decode(c, n, &out, &used) == DD_DECODE_BAD_TYPE, "type");
    memcpy(c, buf, n); c[12] = (uint8_t)(c[12] + 1); c[n] = 0; /* body one byte longer than DISMISS needs */
    CHECK(dd_decode(c, n + 1, &out, &used) == DD_DECODE_BAD_LENGTH, "extra byte");
    memcpy(c, buf, n); c[12] = 0xff; c[13] = 0xff;
    CHECK(dd_decode(c, n, &out, &used) == DD_DECODE_BAD_LENGTH, "huge length");
    memcpy(c, buf, n); memset(c + DD_PROTO_HEADER + 5, 0, 4); /* hero id 0 */
    CHECK(dd_decode(c, n, &out, &used) == DD_DECODE_BAD_VALUE, "hero 0 must be rejected");

    /* Two messages in one stream buffer. */
    dd_msg m2 = town(DD_MSG_TOWN_APPLY, act(DD_ACT_BUY_UPGRADE));
    size_t n2 = dd_encode(&m2, buf + n, sizeof buf - n);
    CHECK(dd_decode(buf, n + n2, &out, &used) == DD_DECODE_OK && used == n && out.type == DD_MSG_TOWN_REQUEST, "first");
    CHECK(dd_decode(buf + used, n + n2 - used, &out, &used) == DD_DECODE_OK && out.type == DD_MSG_TOWN_APPLY, "second");
}

/* PerformCampingSkill arguments x/y index the party/selected skills respectively.
 * Reject oversized values before native execution; retain the target selector sentinel. */
static void test_camp_skill_rank_bounds(void) {
    uint8_t buf[DD_PROTO_HEADER + DD_PROTO_MAX_BODY];
    dd_msg m = {0}, out = {0};
    size_t used = 0;
    m.type = DD_MSG_TOWN_REQUEST;
    m.u.town.request_id = 1;
    dd_action *a = &m.u.town.action;
    a->kind = DD_ACT_ITEM;
    a->u.item.op = DD_ITEM_OP_CAMP_SKILL;
    a->u.item.hero = 255;
    a->u.item.round = 2;
    /* Selected skills use the existing bounded eight-entry model; target can be a sentinel. */
    a->u.item.type = 7;
    a->u.item.id = 0xffffffffu;
    for (uint32_t rank = 0; rank < 4; rank++) for (uint32_t selected = 0; selected < 8; selected++) {
        a->u.item.turn = rank;
        a->u.item.type = selected;
        size_t n = dd_encode(&m, buf, sizeof buf);
        CHECK(dd_action_valid(a) && n && dd_decode(buf, n, &out, &used) == DD_DECODE_OK &&
              out.u.town.action.u.item.turn == rank && out.u.town.action.u.item.type == a->u.item.type &&
              out.u.town.action.u.item.id == a->u.item.id, "camping actor rank %u selected position %u accepted", rank, selected);
    }
    const uint32_t invalid[] = {4, 255, 256, 0x7fffffffu, UINT32_MAX};
    for (unsigned i = 0; i < sizeof invalid / sizeof *invalid; i++) {
        a->u.item.turn = invalid[i];
        CHECK(!dd_action_valid(a) && !dd_encode(&m, buf, sizeof buf), "oversized camping actor %u refused by encoder", invalid[i]);
        a->u.item.turn = 0;
        size_t n = dd_encode(&m, buf, sizeof buf);
        /* header + request ID + kind + area + pos + stage, then actor rank. */
        size_t at = DD_PROTO_HEADER + 4 + 1 + 4 * 3;
        for (unsigned b = 0; b < 4; b++) buf[at + b] = (uint8_t)(invalid[i] >> (8 * b));
        CHECK(dd_decode(buf, n, &out, &used) == DD_DECODE_BAD_VALUE, "oversized camping actor %u refused by decoder", invalid[i]);
    }
    a->u.item.turn = 0;
    const uint32_t invalid_skill[] = {8, 32, 256, UINT32_MAX};
    for (unsigned i = 0; i < sizeof invalid_skill / sizeof *invalid_skill; i++) {
        a->u.item.type = invalid_skill[i];
        CHECK(!dd_action_valid(a) && !dd_encode(&m, buf, sizeof buf), "oversized camping skill %u refused by encoder", invalid_skill[i]);
        a->u.item.type = 0;
        size_t n = dd_encode(&m, buf, sizeof buf);
        size_t at = DD_PROTO_HEADER + 4 + 1 + 4 * 6;
        for (unsigned b = 0; b < 4; b++) buf[at + b] = (uint8_t)(invalid_skill[i] >> (8 * b));
        CHECK(dd_decode(buf, n, &out, &used) == DD_DECODE_BAD_VALUE, "oversized camping skill %u refused by decoder", invalid_skill[i]);
    }
    printf("Camping packet: four actor ranks/eight selected skills bounded; native target sentinel preserved\n");
}

static void test_activity_packets(void) {
    uint8_t bytes[DD_PROTO_HEADER+DD_PROTO_MAX_BODY];dd_msg out;size_t used=0;
    for(unsigned op=1;op<=4;op++)for(unsigned activity=0;activity<DD_ACTIVITY_COUNT_;activity++)
        for(unsigned slot=0;slot<3;slot++) {
            dd_action a=act(DD_ACT_ACTIVITY);a.u.activity.op=(uint8_t)op;
            a.u.activity.activity=(uint8_t)activity;a.u.activity.slot=(uint8_t)slot;
            a.u.activity.revision=UINT32_MAX;
            dd_msg m=town(DD_MSG_TOWN_REQUEST,a);size_t n=dd_encode(&m,bytes,sizeof bytes);
            CHECK(n && dd_decode(bytes,n,&out,&used)==DD_DECODE_OK && same_msg(&m,&out),"activity shape %u/%u/%u",op,activity,slot);
            /* A malformed payload may not reach the model/native adapter. */
            size_t at=DD_PROTO_HEADER+4+1;
            uint8_t saved=bytes[at];bytes[at]=5;
            CHECK(dd_decode(bytes,n,&out,&used)==DD_DECODE_BAD_VALUE,"activity opcode bound");bytes[at]=saved;
            saved=bytes[at+1];bytes[at+1]=DD_ACTIVITY_COUNT_;
            CHECK(dd_decode(bytes,n,&out,&used)==DD_DECODE_BAD_VALUE,"activity kind bound");bytes[at+1]=saved;
            saved=bytes[at+2];bytes[at+2]=3;
            CHECK(dd_decode(bytes,n,&out,&used)==DD_DECODE_BAD_VALUE,"activity slot bound");bytes[at+2]=saved;
            memset(bytes+at+3+4,0,4);
            CHECK(dd_decode(bytes,n,&out,&used)==DD_DECODE_BAD_VALUE,"activity revision zero");
        }
    dd_action a=act(DD_ACT_ACTIVITY);a.u.activity.hero_id=0;CHECK(!dd_action_valid(&a),"activity no empty hero");
    a.u.activity.hero_id=UINT32_MAX;CHECK(!dd_action_valid(&a),"activity sentinel hero refused");
    puts("Activity protocol32: reserve/clear/accept shapes, per-slot revision, malformed opcode/slot/hero/activity and full boundaries verified");
}

static void fuzz(void) {
    uint8_t buf[DD_PROTO_HEADER + DD_PROTO_MAX_BODY + 32];
    dd_msg out;
    size_t used;
    long accepted = 0;
    /* 1: random garbage, sometimes with a correct header so the body parsers are reached. */
    for (int i = 0; i < 500000; i++) {
        size_t len = rnd() % sizeof buf;
        for (size_t k = 0; k < len; k++) buf[k] = (uint8_t)rnd();
        if (len >= DD_PROTO_HEADER && (rnd() & 1)) {
            memcpy(buf, "DDCP", 4);
            buf[4] = DD_PROTO_VERSION; buf[5] = 0;
            buf[6] = (uint8_t)(1 + rnd() % 6); buf[7] = 0;
            uint32_t body = (uint32_t)(len - DD_PROTO_HEADER);
            memcpy(buf + 12, &body, 4);
        }
        if (dd_decode(buf, len, &out, &used) == DD_DECODE_OK) {
            accepted++;
            uint8_t again[sizeof buf];
            size_t n = dd_encode(&out, again, sizeof again);
            CHECK(n == used && memcmp(again, buf, n) == 0,
                  "accepted garbage must re-encode identically (type %u, used %zu, re-encoded %zu)", out.type, used, n);
        }
    }
    /* 2: bit flips in valid messages. */
    for (int i = 0; i < 500000; i++) {
        dd_msg m = town((uint16_t)(DD_MSG_TOWN_REQUEST + rnd() % 2), act((uint8_t)(1 + rnd() % (DD_ACT_COUNT_ - 1))));
        size_t n = dd_encode(&m, buf, sizeof buf);
        int flips = 1 + rnd() % 3;
        for (int f = 0; f < flips; f++) buf[rnd() % n] ^= (uint8_t)(1u << (rnd() % 8));
        if (dd_decode(buf, n, &out, &used) == DD_DECODE_OK) {
            accepted++;
            CHECK(dd_action_valid(&out.u.town.action) || out.type < DD_MSG_TOWN_REQUEST || out.type > DD_MSG_TOWN_APPLY,
                  "accepted town message must hold a valid action");
        }
    }
    printf("fuzz: 1000000 inputs, %ld accepted, all re-checked\n", accepted);
}

static void test_desync_wire(void) {
    uint8_t wire[512]; dd_msg out, m = {0}; size_t used = 0;
    m.type = DD_MSG_SYNC_STOP;
    for (unsigned reason = 1; reason <= 3; reason++) {
        m.u.load.slot = reason;
        size_t n = dd_encode(&m, wire, sizeof wire);
        CHECK(n && dd_decode(wire, n, &out, &used) == DD_DECODE_OK && out.u.load.slot == reason, "sync stop round trip");
        wire[DD_PROTO_HEADER] = 0;
        CHECK(dd_decode(wire, n, &out, &used) != DD_DECODE_OK, "invalid stop reason refused");
    }
    m.u.load.slot = 4;
    CHECK(!dd_encode(&m, wire, sizeof wire), "invalid stop encode refused");
    m = town(DD_MSG_TOWN_APPLY, act(DD_ACT_BATTLE));
    m.u.town.action.u.battle.side = 0;
    m.u.town.action.u.battle.target_id = 19;
    size_t n = dd_encode(&m, wire, sizeof wire);
    CHECK(n && dd_decode(wire, n, &out, &used) == DD_DECODE_OK && out.u.town.action.u.battle.actor_id == 13 && out.u.town.action.u.battle.target_id == 19, "stable hero IDs round trip");
    m.u.town.action.u.battle.target_id = 0;
    CHECK(!dd_encode(&m, wire, sizeof wire), "friendly target requires persistent ID");
    m.u.town.action.u.battle.target_id = 19;
    m.u.town.action.u.battle.actor_id = UINT32_MAX;
    CHECK(!dd_encode(&m, wire, sizeof wire), "invalid actor ID refused");
    m = town(DD_MSG_TOWN_APPLY,act(DD_ACT_FORMATION_RESET));
    n=dd_encode(&m,wire,sizeof wire);
    CHECK(n && dd_decode(wire,n,&out,&used)==DD_DECODE_OK && out.u.town.action.u.formation.after[0]==2,"formation wire round trip");
    m.u.town.action.u.formation.after[0]=3;
    CHECK(!dd_encode(&m,wire,sizeof wire),"formation cannot introduce another hero");
    m.u.town.action=act(DD_ACT_FORMATION_RESET);
    m.u.town.action.u.formation.after[1]=2;
    CHECK(!dd_encode(&m,wire,sizeof wire),"duplicate formation ID refused");
    m=town(DD_MSG_TOWN_APPLY,act(DD_ACT_USE));
    m.u.town.action.u.use.door=5;
    m.u.town.action.u.use.revision=1;m.u.town.action.u.use.actor_id=13;
    n=dd_encode(&m,wire,sizeof wire);
    CHECK(n && dd_decode(wire,n,&out,&used)==DD_DECODE_OK && out.u.town.action.u.use.door==5,"obstacle confirm round trip");
    m.u.town.action.u.use.door=6;
    m.u.town.action.u.use.option=m.u.town.action.u.use.hero=0;
    n=dd_encode(&m,wire,sizeof wire);
    CHECK(n && dd_decode(wire,n,&out,&used)==DD_DECODE_OK && out.u.town.action.u.use.door==6,"prop cancel round trip");
    m.u.town.action.u.use.option=1;
    CHECK(dd_encode(&m,wire,sizeof wire),"obstacle cancel allowed");
    m.u.town.action.u.use.option=2;
    CHECK(!dd_encode(&m,wire,sizeof wire),"cancel panel kind bounded");
    m.u.town.action.u.use.option=0;m.u.town.action.u.use.item_id=1;
    CHECK(!dd_encode(&m,wire,sizeof wire),"cancel cannot carry an interaction item");
    m.u.town.action.u.use.item_id=0;m.u.town.action.u.use.hero=1;
    CHECK(!dd_encode(&m,wire,sizeof wire),"cancel has no hero choice");
    m.u.town.action.u.use.hero=0;
    m.u.town.action.u.use.door=8;
    CHECK(!dd_encode(&m,wire,sizeof wire),"unknown prop operation refused");
}
static void test_selection_identity(void) {
    uint8_t wire[256]; dd_msg out = {0}; size_t used = 0;
    for (unsigned rank=0;rank<4;rank++) {
        dd_action a = {0}; a.kind=DD_ACT_ITEM; a.u.item.op=DD_ITEM_OP_SELECT;
        a.u.item.hero=(uint8_t)rank; a.u.item.area=100; a.u.item.id=11+rank;
        dd_msg m=town(DD_MSG_TOWN_APPLY,a);
        size_t n=dd_encode(&m,wire,sizeof wire);
        CHECK(n && dd_decode(wire,n,&out,&used)==DD_DECODE_OK && used==n &&
              out.u.town.action.u.item.id==11+rank,"selection persistent identity roundtrip");
        for (unsigned invalid=0;invalid<6;invalid++) {
            dd_msg bad=m;
            if(invalid==0)bad.u.town.action.u.item.id=0;
            if(invalid==1)bad.u.town.action.u.item.id=UINT32_MAX;
            if(invalid==2)bad.u.town.action.u.item.hero=255;
            if(invalid==3)bad.u.town.action.u.item.state=0x1d;
            if(invalid==4)bad.u.town.action.u.item.inv=1;
            if(invalid==5)bad.u.town.action.u.item.flag=1;
            CHECK(!dd_encode(&bad,wire,sizeof wire),"selection malformed identity/phase %u",invalid);
        }
    }
    puts("Selection packet: four ranks carry persistent identity; missing/sentinel hero, combat phase and unrelated flags refused");
}
static void test_prop_choice_packets(void) {
    uint8_t wire[256];dd_msg out={0};size_t used=0;
    for(unsigned op=3;op<=7;op++)for(unsigned rank=0;rank<(op==6?1u:4u);rank++)for(unsigned nullable=0;nullable<(op==6?1u:2u);nullable++) {
        dd_action a={0};a.kind=DD_ACT_USE;a.u.use.area=100;a.u.use.prop=1;a.u.use.prop_kind=2;
        a.u.use.door=(uint8_t)op;a.u.use.hero=(uint8_t)rank;a.u.use.flag=(uint8_t)nullable;
        a.u.use.revision=17;a.u.use.actor_id=op==6?0:11+rank;
        if(op==4 || op==7){a.u.use.item_type=41;a.u.use.item_id=42;}
        dd_msg m=town(DD_MSG_TOWN_APPLY,a);size_t n=dd_encode(&m,wire,sizeof wire);
        CHECK(n==DD_PROTO_HEADER+44 && dd_decode(wire,n,&out,&used)==DD_DECODE_OK && used==n &&
            out.u.town.action.u.use.revision==17 && out.u.town.action.u.use.actor_id==a.u.use.actor_id &&
            out.u.town.action.u.use.flag==nullable,"prop choice33 exact revision/identity/NULL actor wire");
        for(unsigned bad=0;bad<(op==6?3u:5u);bad++) {
            dd_msg invalid=m;
            if(bad==0)invalid.u.town.action.u.use.revision=0;
            if(bad==1)invalid.u.town.action.u.use.revision=UINT32_MAX;
            if(bad==2)invalid.u.town.action.u.use.door=8;
            if(bad==3)invalid.u.town.action.u.use.actor_id=0;
            if(bad==4)invalid.u.town.action.u.use.actor_id=UINT32_MAX;
            CHECK(!dd_encode(&invalid,wire,sizeof wire),"invalid prop revision/actor/op refused");
        }
    }
    dd_action old={0};old.kind=DD_ACT_ITEM;old.u.item.op=DD_ITEM_OP_CURIO;old.u.item.hero=255;
    CHECK(!dd_action_valid(&old),"optimistic curio item opcode is reserved/disabled in33");
    puts("Prop choices protocol33: all terminal operations, four actors/nullable arguments, exact revision/identity length and legacy bypass refusal verified");
}
int main(void) {
    test_desync_wire();
    test_roundtrip();
    test_invalid_encode();
    test_decode_errors();
    test_camp_skill_rank_bounds();
    test_activity_packets();
    test_selection_identity();
    test_prop_choice_packets();
    fuzz();
    printf("%s (%d failure%s)\n", failures ? "FAILED" : "OK", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}

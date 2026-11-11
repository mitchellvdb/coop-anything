#include "dd_recruit.h"
#include <stdio.h>
#include <string.h>
/* Isolated snapshot fixture: no game, saves or game writes. */
static unsigned char memory[0x3100000];
static const uint64_t image = 0x10000000, campaign = 0x13000000, head = 0x13010000, node = 0x13010100,
                      coach = 0x13011000, children = 0x13012000, store = 0x13013000, panel = 0x13020000,
                      rows = 0x13021000, row0 = 0x13022000, row1 = 0x13022800, controller0 = 0x13023000,
                      controller1 = 0x13023100, roster = 0x13030000, hero0 = 0x13040000, hero1 = 0x13042000,
                      regular = 0x13044000, entries = 0x13080000;
static int failures, reads, fail_at, change_at;
static uint64_t change_address,change_value;
#define CHECK(c)                                                                                             \
    do {                                                                                                     \
        if (!(c)) {                                                                                          \
            printf("FAIL line %d: %s\n", __LINE__, #c);                                                      \
            failures++;                                                                                      \
        }                                                                                                    \
    } while (0)
static int read_memory(void *unused, uint64_t at, void *out, size_t n) {
    (void)unused;
    reads++;
    if(fail_at && reads==fail_at)return 0;
    if(change_at && reads==change_at)memcpy(memory+(size_t)(change_address-image),&change_value,8);
    if (at < image || at - image > sizeof memory || n > sizeof memory - (size_t)(at - image)) return 0;
    memcpy(out, memory + (size_t)(at - image), n);
    return 1;
}
static void p64(uint64_t at, uint64_t value) { memcpy(memory + (size_t)(at - image), &value, 8); }
static void p32(uint64_t at, uint32_t value) { memcpy(memory + (size_t)(at - image), &value, 4); }
static void p8(uint64_t at, uint8_t value) { memory[(size_t)(at - image)] = value; }
static void vec(uint64_t at, uint64_t begin, unsigned bytes) {
    p64(at, begin);
    p64(at + 8, begin + bytes);
}
static int open(dd_recruit *s) { return dd_recruit_open(s, read_memory, NULL, image, campaign); }
static void setup(void) {
    memset(memory, 0, sizeof memory);
    reads = 0;fail_at=0;change_at=0;change_address=change_value=0;
    p64(campaign + 0x11f8, head);
    p8(head + 0x19, 1);
    p64(head + 8, node);
    p64(node, head);
    p64(node + 0x10, head);
    p64(node + 0x28, coach);
    p64(coach, image + 0xe9d088);
    vec(coach + 0xa8, children, 8);
    p64(children, store);
    p64(store, image + 0xe9cfd0);
    vec(store + 0x68, entries, 2 * 0x15b0);
    p32(entries + 0x137c, 40);
    p8(entries + 0x15a8, 1);
    p32(entries + 0x15b0 + 0x137c, 41);
    p8(entries + 0x15b0 + 0x15a8, 1);
    p64(panel, image + 0xec0cb0);
    p64(panel + 0x80, store);
    vec(panel + 0x350, rows, 32);
    p64(rows, row0);
    p64(rows + 16, row1);
    p64(row0, image + 0xebcc00);
    p64(row1, image + 0xebcc00);
    p64(row0 + 0x158, controller0);
    p64(row1 + 0x158, controller1);
    p32(row0 + 0x58, 1);
    p32(row1 + 0x58, 1);
    p64(controller0, image + 0xee0990);
    p64(controller1, image + 0xee0990);
    p64(controller0 + 0x28, entries);
    p64(controller1 + 0x28, entries + 0x15b0);
    p32(hero0 + 0x137c, 999);
    p32(hero1 + 0x137c, 998);
    p32(hero1 + 0x1384, 41);
    p32(regular + 0x1384, 1);
    vec(campaign + 0x20, roster, 8);
    p64(roster, regular);
}
static void normal(void) {
    setup();
    dd_recruit s;
    dd_recruit_choice c;
    unsigned count;
    CHECK(open(&s) && s.store == store);
    CHECK(dd_recruit_panel_ready(&s, panel));
    CHECK(dd_recruit_resolve(&s, panel, 41, &c) && c.id == 41 && c.hero == entries + 0x15b0 && c.index == 1 &&
          c.available && c.enabled);
    CHECK(dd_recruit_roster_has(&s, 41, &count) == 0 && count == 1);
    /* Ordinals can move; identity must select the matching genuine row. */
    p64(rows, row1);
    p64(rows + 16, row0);
    CHECK(dd_recruit_resolve(&s, panel, 41, &c) && c.index == 0 && c.hero == entries + 0x15b0);
    p32(row1 + 0x58, 0);
    CHECK(dd_recruit_resolve(&s, panel, 41, &c) && !c.enabled); /* full roster: caller must refuse */
    p8(entries + 0x15b0 + 0x15a8, 0);
    vec(campaign + 0x20, roster, 16);
    p64(roster + 8, hero1);
    CHECK(dd_recruit_candidate(&s, 41, &c) && !c.available);
    CHECK(!dd_recruit_resolve(&s, panel, 41, &c)); /* same candidate cannot be recruited twice */
    CHECK(dd_recruit_roster_has(&s, 41, &count) == 1 && count == 2);
    vec(panel + 0x350, rows, 0);
    CHECK(!dd_recruit_panel_ready(&s, panel));
}
static void invalid(void) {
    dd_recruit s;
    dd_recruit_choice c;
    unsigned count;
    setup();
    p8(image + 0x2a6d4bb, 1);
    CHECK(!open(&s));
    setup();
    p64(node, node);
    CHECK(!open(&s));
    CHECK(reads < 1500);
    setup();
    p64(store, image + 0xe8ebe0);
    CHECK(!open(&s)); /* shard store excluded */
    setup();
    p64(store + 0x70, entries + 65 * 0x15b0);
    CHECK(!open(&s));
    setup();
    CHECK(open(&s));
    p64(panel + 0x80, store + 8);
    CHECK(!dd_recruit_panel_ready(&s, panel));
    setup();
    CHECK(open(&s));
    p64(panel, image + 0xeb2968);
    CHECK(!dd_recruit_panel_ready(&s, panel));
    setup();
    CHECK(open(&s));
    p32(entries + 0x15b0 + 0x137c, 40);
    CHECK(!dd_recruit_candidate(&s, 40, &c));
    setup();
    CHECK(open(&s));
    p8(entries + 0x15a8, 2);
    CHECK(!dd_recruit_candidate(&s, 40, &c));
    setup();
    CHECK(open(&s));
    p64(controller0 + 0x28, UINT64_MAX - 10);
    CHECK(!dd_recruit_resolve(&s, panel, 40, &c));
    setup();
    CHECK(open(&s));
    p64(row0, image + 0xeae350);
    CHECK(!dd_recruit_resolve(&s, panel, 40, &c));
    setup();
    CHECK(open(&s));
    p32(hero0 + 0x137c, 40);
    p64(controller0 + 0x28, hero0);
    CHECK(!dd_recruit_resolve(&s, panel, 40, &c)); /* same ID is not the same source Entry */
    setup();
    CHECK(open(&s));
    p64(panel + 0x358, rows + 65 * 16);
    CHECK(!dd_recruit_panel_ready(&s, panel));
    setup();
    CHECK(open(&s));
    p64(campaign + 0x28, roster + 65 * 8);
    CHECK(dd_recruit_roster_has(&s, 40, &count) == -1);
}

static void pstr(uint64_t at,const char *text) {memcpy(memory+(size_t)(at-image),text,strlen(text)+1);}
static void content_setup(void) {
    setup();
    for(unsigned i=0;i<2;i++) {
        uint64_t e=entries+(uint64_t)i*0x15b0,k=0x130a0000+(uint64_t)i*0x200;
        uint64_t levels=0x130b0000+(uint64_t)i*0x100,selected=levels+0x40;
        uint64_t camping=levels+0x60,camp_selected=levels+0x80;
        uint64_t quirks=0x130c0000+(uint64_t)i*0x100;
        uint64_t definition=0x130d0000+(uint64_t)i*0x200;
        pstr(e+8,i?"Hero B":"Hero A");p64(e+0x1320,k);pstr(k+8,"crusader");p32(k+0xd4,1234);
        p32(e+0x1328,10);p32(e+0x132c,1);p32(e+0x1330,2);
        vec(e+0x13e0,levels,12);p32(levels,0);p32(levels+4,UINT32_MAX);p32(levels+8,1);
        vec(e+0x13c8,selected,8);p32(selected,2);p32(selected+4,0);
        vec(e+0x13b0,camping,8);p32(camping,0);p32(camping+4,0);
        vec(e+0x1398,camp_selected,4);p32(camp_selected,1);
        vec(e+0x1380,quirks,0x70);
        for(unsigned j=0;j<2;j++) {
            uint64_t q=quirks+j*0x38,d=definition+j*0x80;
            p64(q,d);pstr(d,j?"quirk_b":"quirk_a");p32(d+0x40,j+1);
            p32(q+0x20,3);p32(q+0x24,4);p8(q+0x28,1);p8(q+0x29,0);p8(q+0x2a,1);
            p32(q+0x2c,6);p32(q+0x30,7);
        }
    }
}
static int digest(uint64_t *hash,unsigned *count) {
    dd_recruit s;return open(&s) && dd_recruit_pool_digest(&s,hash,count);
}
static void fingerprints(void) {
    dd_recruit s;uint64_t initial=0,other=0,candidate=0;unsigned count=0;
    content_setup();CHECK(open(&s));CHECK(dd_recruit_pool_digest(&s,&initial,&count) && initial && count==2);
    CHECK(dd_recruit_fingerprint(&s,40,&candidate) && candidate);
    /* Logical equality survives distinct allocations and arbitrary unused bytes. */
    memcpy(memory+(0x130e0000-image),memory+(0x130a0000-image),0x200);
    p64(entries+0x1320,0x130e0000);
    memcpy(memory+(0x130e1000-image),memory+(0x130b0000-image),0x100);
    p64(entries+0x13e0,0x130e1000);p64(entries+0x13e8,0x130e100c);
    p64(entries+0x13c8,0x130e1040);p64(entries+0x13d0,0x130e1048);
    p64(entries+0x13b0,0x130e1060);p64(entries+0x13b8,0x130e1068);
    p64(entries+0x1398,0x130e1080);p64(entries+0x13a0,0x130e1084);
    p8(entries+30,0xff);p8(0x130e0000+40,0xff);p64(0x130c0000+8,UINT64_MAX);p8(0x130c0000+0x2b,0xff);
    CHECK(dd_recruit_fingerprint(&s,40,&other) && other==candidate);
    CHECK(dd_recruit_pool_digest(&s,&other,&count) && other==initial && count==2);
    unsigned char temporary[0x15b0];memcpy(temporary,memory+(size_t)(entries-image),sizeof temporary);
    memcpy(memory+(size_t)(entries-image),memory+(size_t)(entries+0x15b0-image),sizeof temporary);
    memcpy(memory+(size_t)(entries+0x15b0-image),temporary,sizeof temporary);
    CHECK(dd_recruit_pool_digest(&s,&other,&count) && other==initial);
    CHECK(dd_recruit_fingerprint(&s,40,&other) && other==candidate);
    /* Same ID with any changed persistent recruit content must not compare equal. */
    static const struct {uint64_t at;uint32_t value;} changes[]={
        {0x130a0000+0xd4,1235},{0x130a0000+8,0x78657263},{0x13080000+8,0x78657248},
        {0x13080000+0x1328,11},{0x13080000+0x132c,2},{0x13080000+0x1330,3},
        {0x13080000+0x15a8,0},{0x130b0000,1},{0x130b0000+4,0},{0x130b0000+0x40,0},
        {0x130b0000+0x60,1},{0x130b0000+0x80,0},{0x130d0000+0x40,5},{0x130d0000,0x78697271},
        {0x130c0000+0x20,9},{0x130c0000+0x24,9},{0x130c0000+0x28,0},
        {0x130c0000+0x29,1},{0x130c0000+0x2a,0},{0x130c0000+0x2c,9},{0x130c0000+0x30,9}
    };
    for(unsigned i=0;i<sizeof changes/sizeof *changes;i++) {
        content_setup();CHECK(open(&s));
        if(changes[i].at>=0x130c0000+0x28 && changes[i].at<=0x130c0000+0x2a)
            p8(changes[i].at,(uint8_t)changes[i].value);
        else if(changes[i].at==entries+0x15a8)p8(changes[i].at,(uint8_t)changes[i].value);
        else p32(changes[i].at,changes[i].value);
        /* A selected-skill duplicate is malformed and must be refused, not hashed. */
        int valid=dd_recruit_fingerprint(&s,40,&other);
        CHECK(!valid || other!=candidate);
        CHECK(!dd_recruit_pool_digest(&s,&other,&count) || other!=initial);
    }
    content_setup();CHECK(open(&s));reads=0;CHECK(dd_recruit_pool_digest(&s,&other,&count));int total=reads;
    for(int i=1;i<=total;i++) {
        content_setup();CHECK(open(&s));reads=0;fail_at=i;other=123;count=123;
        CHECK(!dd_recruit_pool_digest(&s,&other,&count) && !other && !count);
    }
    content_setup();CHECK(open(&s));reads=0;CHECK(dd_recruit_fingerprint(&s,40,&other));total=reads;
    for(int i=1;i<=total;i++) {
        content_setup();CHECK(open(&s));reads=0;fail_at=i;other=123;
        CHECK(!dd_recruit_fingerprint(&s,40,&other) && !other);
    }
    /* Change container bounds between the opening and final reads. */
    content_setup();CHECK(open(&s));reads=0;CHECK(dd_recruit_pool_digest(&s,&other,&count));total=reads;
    for(int i=3;i<total;i++) {
        content_setup();CHECK(open(&s));reads=0;change_at=i;change_address=store+0x70;
        change_value=entries+0x15b0;other=123;count=123;
        CHECK(!dd_recruit_pool_digest(&s,&other,&count) && !other && !count);
    }
    content_setup();CHECK(open(&s));p32(entries+0x15b0+0x137c,40);
    CHECK(!dd_recruit_pool_digest(&s,&other,&count) && !other && !count);
    content_setup();CHECK(open(&s));p64(entries+0x1388,0x130c0000+65*0x38);
    CHECK(!dd_recruit_fingerprint(&s,40,&other) && !other);
    content_setup();CHECK(open(&s));p32(0x130d0080+0x40,1);
    CHECK(!dd_recruit_fingerprint(&s,40,&other) && !other);
    content_setup();CHECK(open(&s));p8(0x130c0000+0x28,2);
    CHECK(!dd_recruit_fingerprint(&s,40,&other) && !other);
    content_setup();CHECK(open(&s));p64(entries+0x13e8,0x130b0000+33*4);
    CHECK(!dd_recruit_fingerprint(&s,40,&other) && !other);
    content_setup();CHECK(open(&s));p32(0x130b0000,5);
    CHECK(!dd_recruit_fingerprint(&s,40,&other) && !other);
    content_setup();CHECK(open(&s));p32(0x130b0000+0x40,1);
    CHECK(!dd_recruit_fingerprint(&s,40,&other) && !other); /* selected unlearned skill */
    content_setup();CHECK(open(&s));memset(memory+(size_t)(entries+8-image),'a',64);
    CHECK(!dd_recruit_fingerprint(&s,40,&other) && !other);
    content_setup();CHECK(open(&s));p64(entries+0x1320,UINT64_MAX-10);
    CHECK(!dd_recruit_fingerprint(&s,40,&other) && !other);
    content_setup();CHECK(open(&s));vec(store+0x68,0,0);
    CHECK(dd_recruit_pool_digest(&s,&other,&count) && other && !count);
    content_setup();CHECK(open(&s));other=123;count=123;
    CHECK(!dd_recruit_fingerprint(&s,999,&other) && !other);
    other=123;CHECK(!dd_recruit_fingerprint(NULL,40,&other) && !other);
    CHECK(!dd_recruit_fingerprint(&s,40,NULL));
    other=123;count=123;CHECK(!dd_recruit_pool_digest(NULL,&other,&count) && !other && !count);
    other=123;CHECK(!dd_recruit_pool_digest(&s,&other,NULL) && !other);
    count=123;CHECK(!dd_recruit_pool_digest(&s,NULL,&count) && !count);
    CHECK(!dd_recruit_pool_digest(&s,NULL,NULL));
    puts("Stagecoach fingerprints: stable logical content, pointer/padding/order independence, same-ID changes, bounded duplicate/invalid guards, every read failure and vector mutation: checked");
}
int main(void) {
    normal();
    invalid();
    fingerprints();
    printf("Stagecoach snapshot: candidate identity, normal model, genuine rows, "
           "capacity/duplicate/corruption guards: %s (%d failures)\n",
           failures ? "FAILED" : "OK", failures);
    return failures ? 1 : 0;
}

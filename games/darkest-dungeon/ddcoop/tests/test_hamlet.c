#include "dd_hamlet.h"
#include <stdio.h>
#include <string.h>
static unsigned char memory[0x3100000];
static const uint64_t image = 0x10000000, campaign = 0x13000000, roster = 0x13002000, entry = 0x13010000,
                      levels = 0x13020000, selected = 0x13021000, wallet = 0x13022000, party = 0x13023000,
                      registry = 0x13024000, screens = 0x13025000, screen = 0x13026000, panels = 0x13027000,
                      town = 0x13070000, panel = 0x13072f48, embark = 0x13073640, character = 0x130738c0,
                      rows = 0x1302a000, widgets = 0x13050000, controllers = 0x13060000, estate = 0x13030000,
                      gear = 0x13040000;
static int failures;
#define CHECK(c)                                                                                             \
    do {                                                                                                     \
        if (!(c)) {                                                                                          \
            printf("FAIL line %d: %s\n", __LINE__, #c);                                                      \
            failures++;                                                                                      \
        }                                                                                                    \
    } while (0)
static int read_mem(void *ctx, uint64_t p, void *out, size_t n) {
    (void)ctx;
    if (p < image || p - image > sizeof memory || n > sizeof memory - (size_t)(p - image)) return 0;
    memcpy(out, memory + (size_t)(p - image), n);
    return 1;
}
static void p64(uint64_t p, uint64_t x) { memcpy(memory + (size_t)(p - image), &x, 8); }
static void p32(uint64_t p, uint32_t x) { memcpy(memory + (size_t)(p - image), &x, 4); }
static void vec(uint64_t p, uint64_t b, uint64_t n) {
    p64(p, b);
    p64(p + 8, b + n);
}
static dd_hamlet setup(void) {
    memset(memory, 0, sizeof memory);
    vec(campaign + 0x20, roster, 8);
    p64(roster, entry);
    p32(entry, 1);
    p32(entry + 0x1384, 1);
    p32(entry + 0x15b0, 1);
    memcpy(memory + (size_t)(entry + 16 - image), "Hero", 5);
    vec(entry + 8 + 0x13e0, levels, 7 * 4);
    vec(entry + 8 + 0x13b0, levels, 7 * 4);
    p32(levels + 4, UINT32_MAX);
    vec(entry + 8 + 0x13c8, selected, 8);
    vec(entry + 8 + 0x1398, selected, 8);
    p32(selected, 0);
    p32(selected + 4, 2);
    vec(campaign + 0xe70, wallet, 2 * 0x48);
    p32(wallet, 20);
    p32(wallet + 0x44, 14924810);
    p32(wallet + 0x48, 3);
    p32(wallet + 0x48 + 0x44, 504499961);
    vec(campaign, party, 16);
    p32(party, 1);
    for (unsigned i = 1; i < 4; i++)
        p32(party + 4 * i, UINT32_MAX);
    p64(image + 0x118e140, registry);
    p64(image + 0x118e360, town);
    p64(town, image + 0xec37a8);
    p64(character, image + 0xee2ae8);
    vec(registry, screens, 8);
    p64(screens, screen);
    vec(screen, panels, 16);
    p64(panels, panel);
    p64(panel, image + 0xec00a0);
    p64(panels + 8, embark);
    p64(embark, image + 0xeba728);
    vec(embark + 0x88, rows, 32);
    for (unsigned i = 0; i < 4; i++) {
        p64(rows + 8 * i, widgets + 0x200 * i);
        p64(widgets + 0x200 * i, image + 0xebcc00);
        p64(widgets + 0x200 * i + 0x158, controllers + 0x300 * i);
        p64(controllers + 0x300 * i, image + 0xee0838);
        p32(controllers + 0x300 * i + 0x230, 3 - i);
    }
    vec(campaign + 0xe90, estate, 3 * 0x5a8);
    vec(entry + 8 + 0x1288, gear, 2 * 0x5a8);
    p32(estate, 1);
    p32(estate + 0x44, 3950276909u);
    p32(estate + 0x88, 101);
    dd_hamlet s;
    CHECK(dd_hamlet_open(&s, read_mem, NULL, image, campaign));
    return s;
}
static void presence_checks(void) {
    static const uint32_t off[]={0x3770,0x38c0,0x4058,0x45b8,0x4670,0x2a00,0x3478,0x17a8};
    static const uint32_t vt[]={0xebdde8,0xee2ae8,0xebef28,0xebbda0,0xec2020,0xeba368,0xebe530,0xeb7c68};
    static const unsigned code[]={DD_PLACE_PROVISIONS,DD_PLACE_HERO,DD_PLACE_TRINKETS,DD_PLACE_HEIRLOOMS,
        DD_PLACE_EVENT,DD_PLACE_DISTRICTS,DD_PLACE_EXPEDITIONS,DD_PLACE_ACTIVITY_LOG};
    static const uint32_t building[]={0xebb8d8,0xeb8c70,0xec1c50,0xeb6b78,0xec0720,0xeb9ae0,0xec0dc8,0xebd730,0xebb5a8,0xec1750};
    static const unsigned place[]={DD_PLACE_GUILD,DD_PLACE_BLACKSMITH,DD_PLACE_TAVERN,DD_PLACE_ABBEY,DD_PLACE_SANITARIUM,
        DD_PLACE_SURVIVALIST,DD_PLACE_STAGECOACH,DD_PLACE_WAGON,DD_PLACE_GRAVEYARD,DD_PLACE_ANCESTOR};
    dd_hamlet s=setup();p64(town+0x1730,registry);vec(town+0x5548,panels,8);p64(panels,screen);
    p64(screen+8,town);p64(screen,image+building[0]);
    for(unsigned i=0;i<8;i++){p64(town+off[i],image+vt[i]);p32(town+off[i]+0x58,0);}
    CHECK(dd_hamlet_presence(&s)==DD_PLACE_HAMLET);
    for(unsigned i=0;i<10;i++)for(unsigned state=0;state<8;state++) {
        p64(screen,image+building[i]);p32(screen+0x58,state);
        CHECK(dd_hamlet_presence(&s)==(state>=1 && state<=4?place[i]:DD_PLACE_HAMLET));
    }
    p64(screen,image+building[0]);p32(screen+0x58,4);
    for(unsigned i=0;i<8;i++) {
        p32(town+off[i]+0x58,4);CHECK(dd_hamlet_presence(&s)==code[i]);
        p32(town+off[i]+0x58,0);
    }
    p64(screen+8,0);CHECK(dd_hamlet_presence(&s)==DD_PLACE_UNKNOWN);p64(screen+8,town);
    vec(town+0x5548,panels,33*8);CHECK(dd_hamlet_presence(&s)==DD_PLACE_UNKNOWN);
    vec(town+0x5548,panels,16);p64(panels+8,screen);CHECK(dd_hamlet_presence(&s)==DD_PLACE_UNKNOWN);
    vec(town+0x5548,panels,8);p64(image+0x118e2c8,screen);CHECK(dd_hamlet_presence(&s)==DD_PLACE_UNKNOWN);
    p64(image+0x118e2c8,0);p64(town+off[0],0);CHECK(dd_hamlet_presence(&s)==DD_PLACE_UNKNOWN);
    puts("Presence native model: all10 buildings/all8 panel states, overlay precedence, malformed/ambiguous/raid snapshots refused");
}
int main(void) {
    presence_checks();
    dd_hamlet s = setup();
    /* A secret-room trinket is in raid loot, not estate storage. Never treat
     * its inventory as a hero or let an arbitrary pointer become an endpoint. */
    {
        const uint64_t raid = 0x13080000, loot = 0x13090000, bag = 0x130a0000;
        uint64_t inv = 0;
        unsigned n = 0;
        uint32_t owner = 0, t = 0, key = 0, q = 0;
        p64(image + 0x118e2c8, raid);
        vec(raid + 0x5248, loot, 3 * 0x5a8);
        vec(raid + 0x220, bag, 4 * 0x5a8);
        p32(loot + 2 * 0x5a8, 1);
        p32(loot + 2 * 0x5a8 + 0x44, 3950276909u);
        p32(loot + 2 * 0x5a8 + 0x88, 101);
        CHECK(dd_hamlet_inventory_owner(&s, raid + 0x5240, &owner) && owner == UINT32_MAX - 1);
        CHECK(dd_hamlet_inventory(&s, UINT32_MAX - 1, &inv, &n) && inv == raid + 0x5240 && n == 3);
        CHECK(dd_hamlet_item(&s, inv, 2, &t, &key, &q) && q == 1 && key == 101);
        CHECK(dd_hamlet_inventory_owner(&s, raid + 0x218, &owner) && owner == UINT32_MAX);
        CHECK(!dd_hamlet_inventory_owner(&s, raid + 0x318, &owner));
        p64(image + 0x118e2c8, 0);
        CHECK(!dd_hamlet_inventory(&s, UINT32_MAX - 1, &inv, &n));
    }
    dd_hamlet_hero hero;
    char name[64];
    int active = 0;
    int32_t amount = 0;
    CHECK(dd_hamlet_find(&s, 1, &hero) == 1 && hero.entry == entry && hero.hero == entry + 8 &&
          hero.location == 1);
    CHECK(dd_hamlet_hero_pointer(&s, entry + 8, &hero) && !dd_hamlet_hero_pointer(&s, entry, &hero));
    CHECK(dd_hamlet_find(&s, 1, &hero) == 1 && dd_hamlet_name(&s, &hero, name) && !strcmp(name, "Hero"));
    CHECK(dd_hamlet_skill(&s, &hero, 0, 2, &active) && active);
    CHECK(dd_hamlet_skill(&s, &hero, 1, 3, &active) && !active);
    CHECK(!dd_hamlet_skill(&s, &hero, 0, 1, &active) && !dd_hamlet_skill(&s, &hero, 0, 7, &active));
    CHECK(dd_hamlet_currency(&s, 14924810, &amount) && amount == 20 && !dd_hamlet_currency(&s, 123, &amount));
    CHECK(dd_hamlet_exchange_valid(14924810, 6, 504499961, 2));
    CHECK(!dd_hamlet_exchange_valid(14924810, 1, 504499961, 1));
    CHECK(!dd_hamlet_exchange_valid(14924810, 6, 504499961, 3));
    CHECK(!dd_hamlet_exchange_valid(14924810, 0, 504499961, 0));
    uint64_t found = 0, inventory = 0;
    unsigned count = 0;
    uint32_t ids[4], owner = 0, type = 0, id = 0, quantity = 0;
    CHECK(dd_hamlet_panel(&s, 0xec00a0, &found) == 1 && found == panel);
    /* Real TownDisplay owns the panels; they need not be registered as top-level save/UI screens. */
    vec(registry, screens, 0);
    CHECK(dd_hamlet_panel(&s, 0xec00a0, &found) == 1 && found == panel);
    CHECK(dd_hamlet_panel(&s, 0xee2ae8, &found) == 1 && found == character);
    uint64_t slots[4];
    CHECK(dd_hamlet_party_slots(&s, slots) && slots[0] == controllers + 3 * 0x300);
    vec(embark + 0x88, rows, 0);
    CHECK(dd_hamlet_party_slots_status(&s, slots) == 0 && !dd_hamlet_party_slots(&s, slots));
    vec(embark + 0x88, rows, 16);
    CHECK(dd_hamlet_party_slots_status(&s, slots) == 0);
    vec(embark + 0x88, rows, 32);
    p64(widgets + 0x158, 0);
    CHECK(dd_hamlet_party_slots_status(&s, slots) == 0);
    p64(widgets + 0x158, controllers);
    p64(controllers, image + 0x123456);
    CHECK(dd_hamlet_party_slots_status(&s, slots) == -1 && !dd_hamlet_party_slots(&s, slots));
    p64(controllers, image + 0xee0838);
    CHECK(dd_hamlet_party_slots_status(&s, slots) == 1);
    CHECK(dd_hamlet_party(&s, ids) && ids[0] == 1 && !ids[1]);
    /* Remembered last-party IDs survive a later building assignment. Reading that old identity
     * must succeed so the UI can clear it, but it must never be accepted as a new destination. */
    p32(entry + 0x15b0, 2);
    CHECK(dd_hamlet_party(&s, ids) && ids[0] == 1);
    CHECK(!dd_hamlet_party_valid(&s, ids));
    ids[0] = 0;
    CHECK(dd_hamlet_party_valid(&s, ids));
    p32(entry + 0x15b0, 1);
    CHECK(dd_hamlet_party(&s, ids));
    ids[1] = 1;
    CHECK(!dd_hamlet_party_valid(&s, ids));
    CHECK(dd_hamlet_inventory(&s, 1, &inventory, &count) && inventory == entry + 8 + 0x1280 && count == 2);
    CHECK(dd_hamlet_inventory_owner(&s, inventory, &owner) && owner == 1);
    CHECK(dd_hamlet_item(&s, campaign + 0xe88, 0, &type, &id, &quantity) && quantity == 1 && id == 101);
    CHECK(dd_hamlet_item(&s, campaign + 0xe88, 1, &type, &id, &quantity) && !quantity && !type && !id);
    CHECK(!dd_hamlet_item(&s, campaign + 0xe88, 3, &type, &id, &quantity));
    char text[64] = {0};
    strcpy(text, "H\xc3\xa9ro");
    CHECK(dd_hamlet_name_valid(text));
    memset(text, 'x', 64);
    CHECK(!dd_hamlet_name_valid(text));
    strcpy(text, "\xed\xa0\x80");
    CHECK(!dd_hamlet_name_valid(text));
    strcpy(text, "\xc0\xaf");
    CHECK(!dd_hamlet_name_valid(text));
    strcpy(text, "bad\nname");
    CHECK(!dd_hamlet_name_valid(text));
    p32(entry + 0x1384, 2);
    CHECK(dd_hamlet_find(&s, 1, &hero) == -1);
    s = setup();
    vec(screen, panels, 16);
    p64(panels + 8, panel);
    CHECK(dd_hamlet_panel(&s, 0xec00a0, &found) == 1 && found == panel);
    p64(town, 0);
    CHECK(dd_hamlet_panel(&s, 0xec00a0, &found) == -1);
    s = setup();
    p64(panel, 0);
    CHECK(dd_hamlet_panel(&s, 0xec00a0, &found) == -1 && !found);
    CHECK(dd_hamlet_panel(&s, 0xeba728, &found) == 1 && found == embark);
    CHECK(dd_hamlet_panel(&s, 0, &found) == 0 && !found);
    p64(image + 0x118e360, UINT64_MAX - 8);
    CHECK(dd_hamlet_panel(&s, 0xee2ae8, &found) == -1 && !found);
    s = setup();
    p32(controllers + 0x300 + 0x230, 3);
    CHECK(!dd_hamlet_party_slots(&s, slots));
    s = setup();
    vec(entry + 8 + 0x1288, gear, 3 * 0x5a8);
    CHECK(!dd_hamlet_inventory(&s, 1, &inventory, &count));
    s = setup();
    p32(selected + 4, 0);
    CHECK(dd_hamlet_find(&s, 1, &hero) == 1 && !dd_hamlet_skill(&s, &hero, 0, 2, &active));
    s = setup();
    memory[0x2a6d4bb] = 1;
    CHECK(!dd_hamlet_open(&s, read_mem, NULL, image, campaign));
    printf("Hamlet snapshots: normal roster identity, UTF-8 names, learned/active skills, rates, wallet, "
           "panels, party and inventory bounds: %s (%d failures)\n",
           failures ? "FAILED" : "OK", failures);
    return failures ? 1 : 0;
}

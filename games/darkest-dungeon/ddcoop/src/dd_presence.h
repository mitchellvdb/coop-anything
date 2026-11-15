/* Display-only finite locations; no native pointers or user text on the wire. */
#ifndef DD_PRESENCE_H
#define DD_PRESENCE_H
#include <stdint.h>
enum {
    DD_PLACE_UNKNOWN, DD_PLACE_HAMLET, DD_PLACE_GUILD, DD_PLACE_BLACKSMITH,
    DD_PLACE_TAVERN, DD_PLACE_ABBEY, DD_PLACE_SANITARIUM, DD_PLACE_SURVIVALIST,
    DD_PLACE_STAGECOACH, DD_PLACE_WAGON, DD_PLACE_HEIRLOOMS, DD_PLACE_DISTRICTS,
    DD_PLACE_EXPEDITIONS, DD_PLACE_PROVISIONS, DD_PLACE_EVENT, DD_PLACE_TRINKETS,
    DD_PLACE_HERO, DD_PLACE_GRAVEYARD, DD_PLACE_ANCESTOR, DD_PLACE_ACTIVITY_LOG,
    DD_PLACE_DUNGEON, DD_PLACE_LOADING, DD_PLACE_BUILDING, DD_PLACE_COUNT
};
static inline const char *dd_presence_label(unsigned code) {
    static const char *const labels[] = {
        "Status unavailable", "Hamlet", "Guild", "Blacksmith", "Tavern", "Abbey",
        "Sanitarium", "Survivalist", "Stagecoach", "Nomad Wagon", "Heirloom exchange",
        "Districts", "Expeditions", "Provisions", "Town event", "Trinket inventory",
        "Hero details", "Graveyard", "Ancestor's Memoirs", "Activity log", "Dungeon",
        "Loading / menu", "Other building"
    };
    return labels[code < DD_PLACE_COUNT ? code : 0];
}
static inline int dd_presence_valid(unsigned kind, unsigned mask, const uint8_t place[4]) {
    if (kind > 1 || !mask || mask > 15 || (kind == 1 ? !(mask & 1) : ((mask & 1) || (mask & (mask-1))))) return 0;
    for (unsigned i=0;i<4;i++)
        if (place[i]>=DD_PLACE_COUNT || (!(mask&(1u<<i)) && place[i])) return 0;
    return 1;
}
#endif

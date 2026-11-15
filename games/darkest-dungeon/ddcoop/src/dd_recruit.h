/* Bounded, read-only normal Stagecoach and genuine live display validation. */
#ifndef DD_RECRUIT_H
#define DD_RECRUIT_H
#include <stddef.h>
#include <stdint.h>
#define DD_RECRUIT_MAX_CANDIDATES 64
typedef int (*dd_recruit_read_fn)(void *ctx, uint64_t address, void *out, size_t length);
typedef struct {
    dd_recruit_read_fn read;
    void *ctx;
    uint64_t image, campaign, store;
} dd_recruit;
typedef struct {
    /* entry: persistent candidate; hero: the same Entry resolved through its genuine UI row. */
    uint64_t entry, hero;
    uint32_t id, index;
    int available, enabled;
} dd_recruit_choice;
int dd_recruit_open(dd_recruit *out, dd_recruit_read_fn read, void *ctx, uint64_t image, uint64_t campaign);
int dd_recruit_candidate(const dd_recruit *s, uint32_t id, dd_recruit_choice *out);
int dd_recruit_panel_ready(const dd_recruit *s, uint64_t panel);
int dd_recruit_resolve(const dd_recruit *s, uint64_t panel, uint32_t id, dd_recruit_choice *out);
/* The campaign's live Hero pointer vector. Returns -1 for an invalid/unreadable vector. */
int dd_recruit_roster_has(const dd_recruit *s, uint32_t id, unsigned *count);
/* FNV-1a logical consistency fingerprints, not cryptographic authentication.
 * Cover stable ID, availability, class/name, level/equipment, skills and persisted
 * quirks. Ignore addresses, padding, pool display order, quirk new/viewed UI flags and quirk +20 (the game leaves it uninitialised). Outputs clear on
 * every failure; malformed/duplicate/unstable snapshots are refused. */
int dd_recruit_fingerprint(const dd_recruit *, uint32_t id, uint64_t *out);
/* Prefix hashes: identity, equipment/XP, combat skills, camping skills, gameplay quirks. Read-only diagnostics. */
int dd_recruit_fingerprint_parts(const dd_recruit *, uint32_t id, uint64_t out[5]);
/* Read-only diagnostics: every quirk of a candidate as "id:+20/+24/flags/+2c/+30" (hex, flags +28..+2a), so two
 * copies' logs show which quirk field differs. No names. 1 = written. */
int dd_recruit_quirk_text(const dd_recruit *, uint32_t id, char *out, size_t cap);
/* The heroes a town event offers: TownEvent::System at campaign+1808, its Hero entries at +38 (0x15b0 each, +137c ID,
 * +15a8 available). 1 = found (entry address and availability out). The fingerprint is the Stagecoach one. */
int dd_recruit_event_candidate(const dd_recruit *, uint32_t id, uint64_t *entry, uint8_t *available);
int dd_recruit_event_fingerprint(const dd_recruit *, uint32_t id, uint64_t *out);
int dd_recruit_pool_digest(const dd_recruit *, uint64_t *out, unsigned *count);
#endif

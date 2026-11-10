/* Canonical provisional stress-building placements, independent of UI/native pointers.
 * Commands are already authenticated and totally ordered by dd_town; this is NOT
 * a wire format. All four players have the same reserve/clear/accept permissions.
 * A local drag must not mutate/predict ahead of the authoritative apply.
 */
#ifndef DD_ACTIVITY_RESERVATIONS_H
#define DD_ACTIVITY_RESERVATIONS_H
#include <stdint.h>
#include "dd_proto.h"
#define DD_ACTIVITY_RES_SLOTS 3u
#define DD_ACTIVITY_RES_PLAYERS 4u

typedef enum {
    DD_ACTIVITY_RES_EMPTY = 0, DD_ACTIVITY_RES_DRAFT,
    DD_ACTIVITY_RES_COMMITTING, DD_ACTIVITY_RES_COMMITTED
} dd_activity_res_phase;
typedef struct {
    uint32_t hero, revision;
    uint8_t phase;
} dd_activity_res_slot;
typedef struct {
    dd_activity_res_slot slots[DD_ACTIVITY_COUNT_][DD_ACTIVITY_RES_SLOTS];
} dd_activity_reservations;
typedef enum {
    DD_ACTIVITY_RESERVE = 1, DD_ACTIVITY_CLEAR, DD_ACTIVITY_ACCEPT, DD_ACTIVITY_RELEASE
} dd_activity_res_op;
typedef struct {
    uint8_t op, activity, slot, player;
    uint32_t hero, revision; /* exact expected per-slot pre-state, prevents ABA */
} dd_activity_res_command;
typedef enum {
    DD_ACTIVITY_RES_OK = 0, DD_ACTIVITY_RES_UNCHANGED,
    DD_ACTIVITY_RES_INVALID, DD_ACTIVITY_RES_STALE, DD_ACTIVITY_RES_OCCUPIED,
    DD_ACTIVITY_RES_HERO_BUSY, DD_ACTIVITY_RES_LOCKED, DD_ACTIVITY_RES_EXHAUSTED,
    DD_ACTIVITY_RES_NATIVE_REFUSED
} dd_activity_res_result;
/* Pass the native assignment callback for ACCEPT, native red-X removal callback
 * for RELEASE. Both return1 only after the exact native effect is verified.
 * Return0 only if native state is proved unchanged; partial mutation requires
 * a session-wide stop outside this model. Reserve/Clear never call it. */
typedef int (*dd_activity_res_commit_fn)(void *,unsigned activity,unsigned slot,uint32_t hero);
/* Seed once from verified committed native placements when a fresh session opens.
 * Failure zeroes output. Do not reseed away active draft reservations mid-session. */
int dd_activity_reservations_seed(dd_activity_reservations *,const uint32_t committed[DD_ACTIVITY_COUNT_][DD_ACTIVITY_RES_SLOTS]);
int dd_activity_reservations_valid(const dd_activity_reservations *);
/* Ordered navigation boundary only: release drafts and invalidate stale empty-
 * slot requests by advancing all uncommitted revisions. Committed placements
 * remain. Preflight prevents any mutation on an exhausted/in-flight slot. */
int dd_activity_reservations_release_drafts(dd_activity_reservations *);
dd_activity_res_result dd_activity_reservations_check(const dd_activity_reservations *,const dd_activity_res_command *);
dd_activity_res_result dd_activity_reservations_apply(dd_activity_reservations *,const dd_activity_res_command *,
                                                     dd_activity_res_commit_fn,void *);
#endif

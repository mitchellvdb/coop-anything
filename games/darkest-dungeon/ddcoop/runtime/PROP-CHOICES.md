# Shared curio and obstacle choices — test73 / duo56

## Owner report and verified cause

The duo55 four-window logs show combat and take-all complete on all copies.
At19:16:46 one guest clears an obstacle with its hand locally, with no shared
USE request. At19:17:00 the host uses a shovel; the other two guests apply it,
but the first guest no longer has that overlay. Sequence26 subsequently fails.

The hand callback714770 was correctly pinned. Native obstacle setup712b70
normally leaves overlay.actor+d78 NULL. Its hand callback permits that NULL;
the old hook required a party hero and fell back to the original local function
when it could not build a request. This was a supported native state, not a
corrupt hero. Existing tests populated the actor and missed the normal NULL case.

## Shared resolution, no local fallback

Hand, tool, curio button, X, direct item drop and routed inventory item use now
share terminal USE choices. Every player has the same permission. Both host
local input and guest requests use runtime_apply; guests have no predicted
native effect. The first accepted valid choice applies everywhere. A competing
request is rejected without moving selection, advancing Rules, consuming an item
or changing HP/stress. A bounded FIFO wait handles late native panel readiness.

Each choice carries exact area/prop/kind, monotone revision and persistent actor
ID. Revisions advance on accepted resolution and shared travel/departure. Closing
then reopening the same curio invalidates old choices and old OPEN requests.
Automatic native obstacle panels need no shared OPEN to obtain a revision.
The resolved-panel latch prevents a second choice during native deferred close;
observing a fade does not itself change the shared revision.

NULL native actors remain NULL through a command flag. Selected hero identity is
ordering metadata; the mod does not replace the engine's nullable actor semantics
or invent obstacle damage/stress values. All read-only preflight checks happen
before selection, Rules or native effects. An unreadable/transient active co-op
capture cannot fall back to a local-only action. Passive/off-campaign native calls
and internal calls within an already ordered apply retain their normal path.

## Items and native inventory ownership

Curio slot OnDrop70bf70 is intercepted before copying/decrementing a dragged
owning Item. Typed bag-source pickup and exact source cleanup are matched. The
ordered native curio call copies interaction metadata; the adapter consumes one
canonical bag item. Matched native source Restore/Finish only refreshes flags,
so it cannot restore stale counts after a synchronous host debit or another
player's inventory change. InventoryItemDragData and EquipmentDragData are
supported when they identify a verified canonical bag source. Unverified or
equipped-hero sources refuse without a local-only use.

Ordinary item clicks use the engine's pure428fe0 predicate to decide whether
429750 would route them into a curio. Food/hero use stays on its ordinary path
when that predicate says so. Apply repeats this routing check, preventing an
older ordinary item request from unexpectedly becoming a curio choice later.
The late optimistic ITEM_CURIO path is disabled in protocol33.

Pure4b12e0 checks the requested item and curio variation before accepting a use.
Unsupported items are ordinary rejections, not partial native failures. Its
option vector, bounded row pointers, matching vtable method and variation string
are checked before native lookup. A failed effect after preflight requires STOP;
it never silently ACKs a missing item effect.

## Validation and limits

The72 baseline suite passes. The first six-route new regression against old code
failed62 assertions without an ASan error. Final expanded native/MSVC ASan tests
cover seven routes, all4 initiators/all3 receivers, automatic NULL actor, delayed
panels, no fallback on STOP/not-ready/malformed contexts, shared HP/stress/items/
Rules, direct drop, last item and stale source Restore. A25-pair competing-choice
matrix runs100 winner/rival schedules, each with three receiving guests. Fourteen
invalid native item-compatibility cases reject before effects. Click-routing
true/false and delayed changed routing are tested independently. Effects are
modeled; the constants in tests are synthetic, not claimed game damage values.

Thirteen exact read-only native proofs pass.135 named pins cover131 native
entrypoints and88 distinct live/early hooks; the only new live hook is early
curio item drop. Three pure/native entry pins support drop/routing/acceptance.
Runtime protocol33 and exact hello73 require matching kits for all participants;
launcher wire/lobby3, owner-file and mapping formats are unchanged.

No real game, installer, save or Steam storage was modified. The next owner test
must confirm native hand/tool/drop behavior on actual curios and obstacles.
This changes common native interaction routes rather than special-casing a
Corpse Pile asset. It is not a claim that every DLC/mod-specific effect has been
live-tested. Direct loot-to-hero trinket equip, exact full transaction/combat-state
verification, earlier provisions crash and full live DLC remain open. The bar,
guest provisioning/departure and post-combat selection fixes are retained.
Guests always-nosave; host save preference remains. Recovery uses the host
checkpoint and can lose unsaved progress; solo batch guests restart manually.

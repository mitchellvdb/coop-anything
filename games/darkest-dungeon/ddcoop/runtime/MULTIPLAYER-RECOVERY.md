# Four-player supplies and checkpoint recovery (test65)

Based on Claude Code a48f117/test64, retaining protocol30, four-player routing,
the fixed-map departure change in test63 and the trinket identity fallback in64.

The four-player report stopped at the first food purchase, before embark. The
guest's initial supply digest did not match the host quote. The original host
pre-state was not logged, so the exact differing field/insertion order is not
proven. No quote, currency, inventory or native-result guard has been weakened.

PREPARE now calls the complete normal TownDisplay opener on every copy, including
those whose expedition screen was closed. Its existing ProvisionSelect state
machine calls the normal initializer: free items, stock, then view allocation.
Already-open panels are not refilled. Initial free Item slots are ordered by
type/ID/quantity using existing native Item copy/clear/Inventory swaps/destructor;
all Item metadata stays with its item. No raw C++ Item memcpy or native state
patch. Sorting is verified against the complete expected supply snapshot. It is
only performed during initialization, never while a purchase is being applied.
Initial native/ordered snapshots now log full digests, context and slot counts.

One lost guest now broadcasts SYNC_STOP to every surviving guest. A long but live
busy window keeps the host waiting; a missing busy heartbeat stops shared actions
instead of silently letting the host walk ahead. A guest first seen in a panel
also begins busy detection, without requiring an earlier walking callback.

The launcher provides host-only direct Player1..4 assignment by persistent hero
ID, bounded by the chosen session size. It uses the existing revision/file/table
route, including live ownership at supported choice boundaries. The Players
selector no longer overlaps Restore. Copy logs collects all three guest files.

Resync is a coordinated checkpoint restart, not a live-memory repair. The host
button explains unsaved-progress loss, then requests LR_BYE_RESYNC from all three
launcher peers. Each acknowledges before normal WM_CLOSE. Requests retry through
backpressure; a queued guest acknowledgement is flushed even in failed state.
Existing journaled guest recovery runs only after game exit/preparation ends.
The host closes after acknowledgements/departures. The host then clicks Host and
everyone joins again, obtaining a fresh checked snapshot. No forced termination,
save replacement while running, desync reset or automatic combat rejoin.
Solo guest windows started by .bat have no separate launcher connection; close
those manually for recovery/restart. A disconnected launcher cannot be remotely
closed: close its game normally and allow its own recovery to finish.

Tests: four free-item permutations produce the same first quote while preserving
metadata/lifetime; partial native swap refuses. Real ordered prepare/FIFO covers
host plus three guest player numbers, loading delays and duplicate opening.
Core fixtures cover Player4 assignment broadcast to three guests, invalid/guest
assignments, all three resync acknowledgements and backpressure. Native effects
are simulated. test_supply_init_layout.py verifies the complete native opening
chain read-only on the exact pinned executable. Live four-PC behavior remains
for the owner to validate. Existing unrelated loot fallback, arbitrary dungeon
formation drag support and full DLC validation remain outside this repair.

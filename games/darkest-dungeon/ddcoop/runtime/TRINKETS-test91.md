# Test91: rooted loot sources and shared Unequip All

Continue from Claude Code c7ad502/test90, including Guild refresh, raid bag drags,
week candidate schema3, event recruits and Sanitarium treatment/protocol38.

## Loot source

The owner's15 November test90 reports contain source-refused messages for direct
loot drags. A normal bag drag sometimes reached a separate non-trinket-endpoint
refusal. Keep both cases distinct; the latter now logs actual/expected endpoints.

The earlier fixture used the Raid+5140 wrapper directly. Actual OverlayLoot has
its own InventoryBar at overlay+8, with a presentation inventory at bar+8 and its
widget vector at bar+10..18. Widget+30 can point into that presentation vector,
so neither the canonical pointer test nor wrapper+100 test recognized it.

A new read-only route validates membership in the currently ready loot bar,
wrapper identity, bounded aligned Item/widget vectors and exact trinket content.
It resolves a canonical Raid+5240 slot by occurrence of that type/id, requiring
equal copy counts. Identical items remain separate occurrences; foreign widgets,
closed windows, changed counts and malformed spans refuse. Only the canonical
native Item is moved. No arbitrary-window or global-ID search is used.

The initial rooted-bar fixture failed against90, then passed with this route.
Additional negatives and duplicate-occurrence checks pass. Native proof checks
the actual draw/pickup layout. Live UI/engine testing is still required.

## Unequip All

Previously hamlet_bulk blocked both sorting and Unequip All for every role. Only
sorting remains blocked. Unequip All may open its ordinary local confirmation.
After Yes, the normal model call Roster::UnequipAll58ce70 is intercepted before
mutation. Exact normal caller6ae19f, campaign and estate identity are checked.

DD_ACT_UNEQUIP_ALL (28, protocol39) carries logical before/after digests and moved
count. Every player may request it; the existing host sequencer orders it.
Digests sort stable roster IDs and the estate item multiset, preserving duplicate
counts and skipping only status3 graveyard entries as the native helper does.
All other locations, including building heroes, follow native behavior. Estate
capacity<=9999 and equipped slots<=128 remain bounded. Native Item metadata and
lifetime are handled by the engine, never copied as C bytes by production code.

Each copy verifies before-state, runs the real model helper once, then verifies
empty eligible equipment and the conserved estate multiset. An already-completed
crossed confirmation is idempotent. Stale requests reject before mutation;
partial native results stop sharing before acknowledgement. Remote inventory
panels may be closed. The current normal RealmInventory panel, if present, is
resolved from TownDisplay+4058/vtableEBEF28 and marked dirty at+538. The normal
roster save and currency notification follow; guests retain-nosave.

Whole-operation fixtures model native effects for4 initiators/3 receivers,
reordered estate/roster, repeated item IDs, building/dead heroes, replayed and
crossed input, stale state, partial effect and raid/unready/caller refusal.
Native proof checks the confirmation/model ABI, transfer/clear/actor refresh
and current normal TownDisplay constructor offsets. This is not live engine QA.

Compatibility: exacthello91/runtime39 on every peer; launcher wire3 unchanged.
No new duo kit. Existing90 open curio/walking and inventory-full cases remain.
Shared press-any-key and full live DLC verification remain open.
Written by Codex,15 November2026; prior Claude Code contributions retained.

# Post-combat selection transition — test72 / duo55

## Confirmed owner progress and new failure

The owner confirmed Tavern provisional placement and provisioning now work in
the game with duo54. Those test71 changes are retained. The attached four-window
session subsequently stopped just after victory, before loot was taken.

All four logs show the same final combat choices and recorded Rules states.
The host records combat over and emits ITEM SELECT at18:57:14.890, sequence98,
rank2. All three guests fail that selection at14.904/.905/.917, without first
recording their own combat over. These timestamps support a native presentation
timing race, not evidence of different loot rolls. Logs contain no full native
combat-state snapshot, so identical Rules alone is not proof of every field.

The source confirms SELECT bypassed readiness (`go=1`) while its apply branch
refused `v.battle`. Thus a14–27ms native finish difference caused an immediate
shared stop. The screenshot's unattached-launcher notice is expected for a solo
guest window and does not explain the failed action98; its transport was active.

## Repair and invariant

SELECT now stays at the expected FIFO head for at most30s until a readable raid
is outside combat in the same area. No position/pixel match or manual host click
is required. Later orders cannot overtake. At timeout the unchanged apply checks
refuse instead of acknowledging a missing effect; a guest stops for recovery.
A host receiving a premature guest request waits as well; invalid host requests
retain normal rejection behavior if no effect occurred.

The existing item.id field now carries the persistent selected hero ID. The codec
requires a valid ID/rank and outside-combat SELECT shape. The sender reads the ID
before tracking/emitting a changed selection. Apply checks a nonempty bounded
party vector, aligned span, maximum4 heroes, rank, exact Hero* identity and native
function. After native execution it verifies raid.selected+438, persistent ID
and display reverse-rank field before reporting success. An unverified partial
effect stops instead of retrying or ACKing. Concurrent rank/ID changes refuse;
the action never silently retargets a different hero.

The pinned native Party selector606450 reads begin/end at+0/+8 and writes its
selected Hero* at+430, which is raid+438. This was independently disassembled and
the new read-only `test_selection_layout.py` proves those exact field accesses.
The isolated native fixture now models this existing effect rather than merely
incrementing a callback counter. Other callers of walk_select retain their path.

Runtime byte protocol32 and launcher wire3 are unchanged; **exact runtime hello
test72 is mandatory**, so71/72 cannot mix despite the unchanged packet length.
No new hook/address is added:132 named pins/128 entrypoints/87 hooks remain.

## Evidence and remaining work

The existing71 native/overlay baseline passes. The added actual-bridge regression
fails75 assertions with the old implementation. Final native and MSVC ASan tests
pass: all4 initiators/all3 receiving guests, host or guest late native victory,
several wait ticks, temporary missing view, no ACK/effect or FIFO overtaking while
waiting, then exactly one verified selection. Tests include ordinary far-pixel
selection, wrong area/endless battle/missing-view timeout, malformed identity,
stale actual hero, NULL slot, malformed/oversized/short party span, missing native
function, no-effect callback, STOP and sequence guards. Protocol tests roundtrip
all4 ranks and reject invalid IDs/phase/flags. All twelve read-only native proofs
pass, including the new native selection postcondition proof.

No real game/installer/save/Steam writes were made. Engine effects are modeled;
the repaired transition requires the next owner playtest. Full native/state
digest, exact cross-copy prices, remaining rank-based item uses, direct loot
equip, earlier provisions crash and full live DLC remain open. Test71's six
stress-activity scope and DLC groundwork are unchanged. Guests always-nosave;
host save checkbox remains. Recovery uses a host checkpoint and can lose unsaved
progress, and solo batch guests restart manually. No private logs/disassembly,
game binaries, artwork or saves are included in published source.

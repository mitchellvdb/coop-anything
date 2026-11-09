# Test59: prop opening and confirmation are different actions

The test58 two-PC report contains three OPEN requests for the same Iron Maiden.
Both copies assert `overlay_event.GetState() == es_noevent`. The guest then
misses curio/loot effects, waits30s on loot and never reaches the host's next
fight. Food orders later arrive, but input/walking are still in an old event.
The missing-shovel report is consistent with the same ambiguity: an OPEN could
execute the obstacle confirmation on a copy whose overlay was already open.

The pinned executable proves InteractWithProp7747c0 requires display+2e40==0.
That is overlay20b8+d88. Obstacle button714770 requires mode2, interacts through
the prop vtable, then sets mode3 to close normally. Never zero that event by hand.
Protocol28 gives that button its own USE operation5. Operation0 only opens;
3 remains curio confirm,4 tool and2 door. Native functions/addresses are unchanged.

- Local OPEN while an overlay is active is ignored. Ordered duplicate OPEN for
  the same prop is a no-op, including a copy that auto-opened it on arrival.
- An obstacle confirmation only uses the same ready prop panel and native
  button. Either player may initiate; no fallback to OPEN. Curio/tool confirms
  similarly wait for their matching ready panel, independent of walking pixels.
- Missing panels wait a bounded15s, then fail with peer SYNC_STOP. Declined
  approved OPEN/tool/curio/obstacle effects are not silently acknowledged.
- The native callback closes the panel on both copies. Queue processing, food
  and walking may proceed afterwards. No original file/save/system is edited.

The loot-trinket report contains an unsupported-source notice, without pointer
values. Native pickup7ee320 copies the Item from widget+30. A new bounded lookup
resolves that exact address in the canonical raid bag/loot vectors; a presentation
inventory or display index need not equal the canonical endpoint. Pointer copies,
unaligned pointers and matching IDs outside those vectors are refused. Native
Item copy/clear/swap/destruction is retained. The fixture models a presentation
wrapper/index mismatch; the live widget's precise mismatch remains unproven.
New source/slot diagnostics make the next live attempt traceable.

Tests exercise the actual hooks/FIFO with simulated native effects: duplicate
Iron Maiden opening, already-open guest, host/guest no-shovel confirmation once,
delayed ready panel, food and walking after native close, timeout stopping before
later item/fight orders, exact canonical loot item and Item lifetime. Static ABI
checks use the exact executable. Real two-PC gameplay still needs testing.

Retained limits: arbitrary manual dungeon formation drags, live rejoin and combat
resync are not added. A failed state check requires both games to restart. The
old unrelated loot-result fallback remains; do not claim general state recovery.

Written by Codex,9 Nov2026; prior Claude Code and Codex contributions retained.

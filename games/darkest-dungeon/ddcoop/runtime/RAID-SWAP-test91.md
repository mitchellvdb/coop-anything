# Whole-stack bag/loot exchange — test91, runtime40

The owner reports both ordinary items and trinkets cannot be exchanged with an
item still in the loot window. A full bag must not require an intermediate free
slot. The existing per-frame bag drag route and trinket-only equipment transaction
cannot describe a swap of different item types with different stack quantities.

DD_ACT_RAID_SWAP29 carries both canonical inventory/slot/type/id/count endpoints,
source area and last ordered loot action sequence. Only the normal raid bag and
current ready loot inventory are accepted. The selected UI Item can be a copy:
the rooted OverlayLoot InventoryBar/widget resolver supports positive whole-stack
quantities, zero-ID currencies and equal-count occurrence mapping. Foreign,
malformed, stale and ambiguous roots refuse. A secondary destination interface
is primary widget+10; no arbitrary cross-window ID search is used.

Any player can initiate. Guests wait before mutation. Different whole stacks
are exchanged through the existing native Item copy/clear/three Inventory swaps/
destroy path, then both complete logical endpoints and native lifetimes are
checked. C++ metadata is retained; no production memcpy of owning Items. A partial
effect stops before ACK. Exact old endpoints reject crossed requests instead of
swapping back. A pending/previous loot action cannot be overtaken; delayed peers
wait at the FIFO head. Stale drag-manager source finish/cancel is suppressed for
this transaction and resets presentation flags only; it must not emit another
bag-delta action. Native merge and within-bag operations keep their existing paths.

Only full different stacks are handled by this new transaction. A split-stack
drag is refused before effects; same-identity merging is a separate native route.
Quest items cannot be swapped out of the bag. Hero equipment keeps its existing
native class/duplicate restrictions; equipped trinkets dropped onto a copied loot
destination now resolve that rooted destination too.

Actual bridge/FIFO fixtures exercise both directions, full bag, supplies, gold
with ID0 and trinkets, all4 initiators/3 receivers, compacted UI copies, source
cleanup, independent native owning Items, stale/quest/type/count/slot/area guards,
delayed loot, split refusal and partial native failure. Existing codec matrix
covers the new exact-sized action. Read-only ABI proof distinguishes native swap
5df8f0 from merge5df6a0, confirms secondary slot offsets and displaced-Item copy.
These are modeled native effects; real UI refresh and gameplay need the next kit.
Written by Codex,15 November2026; prior Claude Code work retained.

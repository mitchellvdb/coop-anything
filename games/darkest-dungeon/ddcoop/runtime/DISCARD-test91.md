# Confirmed Shift-discard — test91

Owner reports Houndmaster Dog Treats discarded by Shift-click do not disappear
on peers. Previously737070 only ran locally then marked the bag for a later
frame's slot-delta publication. The exact cause in that live run is unproven;
the old reports do not record the confirmed source model.

The confirmed737070 effect now becomes ordered ITEM24, before any native
mutation. Preserve the native optional local Yes/No dialog: No sends nothing,
Yes follows the same route as the direct Shift action. Guests wait for the
accepted order. Source must be the current canonical raid bag or ready loot
inventory, with exact slot, type, ID, prior count and requested count. There is
no search that could discard another identical stack. The native quest-item
type guard is retained. A foreign/stale/unready source refuses before effects
and logs its inventory/slot/quantity; no unproved UI model is mutated.

Every copy calls the native confirmed helper737070 once, using only its proved
nonowning capture (inventory,index,count-pointer). Native Item clear and slot
recount preserve C++ lifetime; production does not copy an owning Item. Exact
remaining quantity/identity is verified before ACK. A partial effect stops.
Crossed stale host requests reject without taking the second duplicate stack.
Pending guest requests clear on acceptance, rejection and reset. Existing
non-raid behavior and drag-discard737c20 remain unchanged.

Actual bridge/FIFO fixtures model all4 initiators/3 receivers, full and partial
Dog Treat stacks, identical stacks, stale source/quantity/slot/area/type,
quest guard, native partial failure and delayed loading. Codec covers exact
quantity/identity roundtrip and malformed fields. Read-only native proof checks
the Shift entry, Yes functor tail-jump,9-byte existing hook, capture reads,
native decrement/clear/recount. These are modeled effects, not live gameplay.
The callback's real source rooting and UI refresh require the owner's next test.

Separate new feedback: exchanging a bag item with an item still in the loot
window remains under investigation. Do not claim this Shift-discard change fixes
that swap, or that the current trinket-specific loot resolver handles all items.
Written by Codex,15 November2026; prior Claude Code work retained.

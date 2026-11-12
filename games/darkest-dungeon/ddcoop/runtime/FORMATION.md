# Test57: restore default dungeon party order

The test56 report identified a missing player action. After an enemy shuffled the
party, one player restored the default order with the button beside the map.
The other copy did not. The next travel message already showed the front two
heroes reversed between copies. Later the same round/turn selected different
actors. Protocol26's peer stop worked and prevented further shared choices.

Protocol27 adds FORMATION_RESET: area identity, before IDs and after IDs, four
persistent hero IDs per array. Both arrays must be unique (zero is an empty rank),
contain the same nonempty membership and represent a change. Network host orders
the request, but either player may initiate it. Ownership assignments do not change.

Normal DDTabPanel::RenderPartyReorderButton's stateless callback at7812d0 and the
controller keyboard path at42bc65 both call RaidDisplay's native method76f060.
The runtime hooks that method with an instruction-aligned11-byte trampoline.
The pinned48-byte address table adds it and Raid::GetDefaultPartyOrder at614cb0.
The read-only ABI proof checks the button's actual RTTI/vtable, the keyboard call,
register arguments, rank masks, immediate vector changes and busy flag.

The guest sends intent before any mutation. The host validates its current party
and native default against both arrays, runs the native reset once, then verifies
the actual result before broadcasting. The guest performs the same validation and
native call. No sender pointers, raw party/rank writes, battle AI or PvP endpoints
are used. Native code also schedules its own animation and sets display+4119.
Walking and further shared inputs wait for that animation to finish.

A request waits up to15 seconds for matching area and safe phase on the receiving
copy. Loading, battle, a blocking overlay, door transition and an existing reorder
prevent mutation. Repeated guest clicks stay one request. Crossed clicks cannot
run the reset twice. Stale requests are rejected before mutation. A partial native
result or guest apply failure stops both players through existing SYNC_STOP.

At travel, formation identity is checked before HP/stress correction and native
entry to the next area. A different party order now stops both peers instead of
offering to continue into another fight with divergent initiative. Already-arrived
travel acknowledgements are also checked. This is a guard, not an automatic repair.

Scope: default-order button and shortcut. Combat move/effect paths retain their
existing handling. Arbitrary manual dungeon party drags are not newly enabled by
this change; any unshared difference must stop before the next travel. That separate
input path still needs native mapping and live validation.

Validation uses the real bridge/FIFO and fake native effects: host/guest initiation,
crossed/double clicks, delayed combat end/presentation, stale area/members, exact
post-state, native partial failure, campaign/session/loading/door guards and passive
game behavior. Static ABI proof uses only the exact owned executable. A real two-PC
playtest remains required. No game or save is launched/changed by these tests.

Written by Codex,9 Nov2026. Prior test56 fixes, live party display and DLC groundwork retained.

# Protocol37: hand-made party order (FORMATION_SET)

Owner's run of 12 Nov 2026: at 16:19 and 16:21 a guest dragged two heroes to other
ranks outside a fight. That drag is native and changed only that copy; the next
shared hero selection by rank was refused elsewhere and the session stopped.

The drag's own input path is still not mapped. Instead `formation_watch` compares
the live party vector (persistent IDs) with the last shared order every frame in a
raid. Outside a fight, loading or a session, any order of the same members that no
shared action produced is a player's drag. It becomes DD_ACT_FORMATION_SET: area,
before (last shared order) and after (the whole new order; heroes first, empty ranks
at the end). The host orders its own drag at once; a guest asks, counts as busy for
walking until the answer, and sends one request per new order.

Every copy produces `after` through the native rerank of the default button
(76f060). Raid::GetDefaultPartyOrder (614cb0) is hooked with an exact 11-byte
prologue; only during `formation_native_order` it returns the wanted heroes, found
by ID in the live vector, instead of the default. 76f060 then moves each hero whose
rank differs, sets rank masks, writes the vector and animates, as for the button.
A copy that already stands so changes nothing. Crossed drags are applied in host
order, so all copies end on the last one. A refused guest order is undone on that
guest once its animation is over. A partial native result stops everyone. Fights,
loading and changed membership are adopted, never shared. tests/
test_formation_layout.py proves the prologue, the array use and the vector writes.

Not yet live-tested. A drag of a guest and a host walk into the next area within the
network latency could still meet the travel guard; the guest's busy flag makes the
host wait otherwise.

Written by Claude Code, 12 Nov 2026 (runtime work allowed by the owner).

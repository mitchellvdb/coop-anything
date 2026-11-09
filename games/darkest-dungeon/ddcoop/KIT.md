# Test67 / duo50


Based on delivered66, retaining Claude Code60-64/a48f117. Runtime protocol31,
122 exact native signatures. Launcher wire/lobby3 and automatic checkpoint
restart from66 are retained. All players must use67 together.

The supplied four-window run first fails on Player3: at round3/turn2/actor15,
SELECT sequence90 was issued from target30, while that copy remained in skill29.
It waits30 seconds, then stops all peers. The host and the other guests proceed
through orders91..93 and have matching Rules draws up to the propagated stop.
This evidence identifies a circular input-state wait, not a proven Collector
summoning/RNG defect. Other Collector behaviors still require live validation.

SELECT now accepts either native input state29 or30 for the same round and turn.
Persistent actor identity and host-side owner checks still apply. Earlier turns
and animation states wait; stale turns/wrong actors still fail. TARGET and MOVE
retain their original exact phase gate. No battle state or RNG is overwritten.
Native5cdf70 sets hero+8e0; the native update owns transition to target selection.
The regression reproduces the old failure before the fix, covers three guests,
reverse input states and host handling; existing execution/identity tests remain.

Curio X is714270; obstacle X is714540. Both use functor+8, mode2, native fade3.
The obstacle callback additionally clears its completion flag. USE door6 carries
option0 curio (+6a0) or1 obstacle (+d70), with no hero/item side effects. Either
player initiates it; the host orders it and every copy calls its matching native
callback. No prop Interact, loot roll, inventory mutation or manual event-zero.
The guest waits for its matching ready panel. Simultaneous close requests are
idempotent while closing or after this shared cancellation completed; a new
OPEN clears that completion record. Wrong/missing panels cannot silently ACK;
the bounded wait still stops before later shared actions after failure.

Tests use the actual runtime hooks/FIFO and modeled native effects. They cover
all four initiators, three receivers, delayed readiness, crossed/closed clicks,
reopening, separate obstacle fields and wrong-prop timeout. Read-only exact-build
proof checks vtable bindings, complete overwritten instructions, native fade and
skill-pointer/state dispatch. Live game/installer/Steam storage not run here.
Prior66 duplicate-stack repair, fault diagnostics, automatic checkpoint restart,
English UI, optional embedded GIF and host save preference remain included.


RELEASES.json records exact normal/solo runtime, launcher and kit hashes. ZIP
payload/pins/labels/source parity and patches versus66/Claude64 were byte-verified.
Source only: no binaries, assets, saves, artwork, raw logs or dumps. Runtime,
launcher build/contract, runtime pin and release commits are separate.

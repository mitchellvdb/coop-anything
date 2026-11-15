# Normal campaign wave-room Continue — test91

The first Farmstead room-to-fight transition bypasses the existing normal door
hook770080. The private owner report has a guest already choosing its first
battle skill while the host remains outside battle until its own Continue.

Pinned normal RaidDisplay button4258 installs functor vtableED3AB8, whose invoke
780240 passes captured display+8 to76FDA0. That body checks quest definition+40
against initialized wave type2628598, sets WaveLogic3D0 waiting, and starts the
normal display46AC fade. Later native updates finish waiting, change WaveLogic's
dungeon and start the next encounter. We intercept the explicit button only;
automatic native updates remain untouched. No replay of native battle creation.

ITEM opcode23 carries source area, scene revision, native phase/action/group/
dungeon indices and Rules state. The rooted model must belong to this raid;
only a ready room without battle/fade/prop/loot can start. All four players use
the same ordered stream. A guest waits for the accepted action before any
native effect. Stale competing requests reject immediately during fade so they
cannot hold up battle orders. Receiving copies wait a bounded30 seconds for
readiness. Each accepted copy invokes the native callback once and verifies that
both teleport waiting and fade started; a partial effect stops before ACK.
Non-wave uses of the same native callback retain their original behavior.

Tests run the actual bridge/FIFO with modeled native effects for all4 initiators
and3 receivers: idle host, loading delay, duplicate sequences, crossed choices,
source/scene/phase/action/group/dungeon/Rules mismatch, wrong model/foreign
functor, bounded timeout and partial native failure. Codec rejects malformed
reserved fields and ranges. Read-only test_wave_layout.py proves the native
button binding, capture,8-byte hook boundary, native flag writes and automatic
WaveLogic movement on the exact supported EXE. These are not real engine tests.

Post-raid hero comparison now sorts id/stress/trinket tuples by persistent id,
instead of UI roster vector order. The new permutation/duplicate-ID regressions
failed on the previous code then pass. Actual stress/trinket/count/wallet
differences still stop. Per-hero numeric diagnostics are logged on the host
comparison and guest mismatch. The latest report only establishes equal wallet
and hero count with differing hero digest; it does not prove which field changed
or that the town event/blueprint award caused it. No state is overwritten.

Native shard shops/refunds, later wave-specific behavior and all-DLC gameplay
still require separate integration and live tests. This change does not claim
complete Color of Madness support. Runtime39/exacthello91; launcher wire3.
Written by Codex,15 November2026; prior Claude Code work retained.

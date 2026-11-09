# Test66: duplicate stacks and automatic checkpoint restart

Based on Codex65 / Claude Code a48f117/test64. Runtime protocol30 and120 native
addresses remain. Launcher wire/lobby version3 requires every PC on test66.

The last supplied four-player session stopped after travel at the bag repair
final check on guest index2 (Player3): expected9ccee08e, actual3406ba8b. Other guests
verified9ccee08e. A reproduced defect in the old repair swaps the first matching
identity into each destination. With two stacks of one kind, that can steal an
already-finalized stack; a later overwrite can also lose the only exemplar of
another kind. The logs lack the original per-slot identities, so the first cause
of the original pre-repair inventory difference remains unproven.

BEGIN now constructs bounded native Item templates for the identities present
in the local bag/loot. Each repair destination is filled from those immutable
templates. Duplicate stacks never consume a previous repaired slot or destroy
a future source. Native constructors/Inventory swap/destructors preserve Item
containers. Templates are destroyed at success, reset and every sync stop.
Missing identities/read errors still refuse; the final host hash remains mandatory.
The normal bag swap path also uses native swaps instead of a raw Item byte swap.

Host Resync now keeps the Steam lobby and coordinates these steps automatically:
normal tracked-game close, existing journaled guest recovery, READY from every
captured participant, fresh closed-host checkpoint snapshot, reserved original
player slots, GO, fresh guest joins and the existing verified transfer/backup/
Steam preparation/autoload. Repeated BEGIN/GO do not close or start twice. An
already-open game, unfinished recovery, failed start or timeout cannot authorize
save replacement. Host launch waits for every current READY and its own current
closed/recovered state. All original launchers must remain open and connected.

The coordinator binds phase messages to the exact kit, generation and participant
IDs; guest BEGIN comes only from its current host. Control sends rotate their
first recipient to avoid starvation under repeated backpressure. A blocked core
HELLO retries promptly rather than repeatedly aligning with keepalive traffic.
The old relay/snapshot are discarded before restarting; original hero owners and
player numbers survive even when guests rejoin in a different order. The host
log boundary is reset before relaunch so old loaded/disconnected lines cannot
invalidate the new session. Guest health reads the correct guest1/2/3 log.

This is checkpoint recovery, not an in-memory combat repair. No forced save,
forced process termination or desync-flag reset. Native host autosave/checkpoint
rules decide the resumed spot; exact unsaved progress may be lost. If a launcher
has left, use a fresh manual session. Solo .bat guests have no launcher, so their
restart stays manual. Closing launchers during automatic recovery aborts it.

An earlier supplied session ended with native access violation C0000005 on one
guest during provisions. No stack/crash location was recorded, so that crash is
NOT claimed fixed. Test66 adds bounded first-chance diagnostics: code, thread,
game/runtime relative instruction addresses and selected stack frames in the
existing runtime log. The observer always returns CONTINUE_SEARCH, never changes
exception/context handling, and writes no memory dump, save data or credentials.
An event may still have been handled by the game; a trace alone is not proof of
a fatal crash. The launcher exit code identifies actual process failure.

Validation: original duplicate-stack regression fails65, then passes66 for
permuted/fewer/matching stacks, preserved metadata/lifetime, missing identity and
strict final hash. Native/fuzz/FIFO, ASan and overlay pass. Four coordinator tests
model delayed close/recovery, duplicates, forged/early/stale control, retry,
timeout and start refusal. A separate four-core fake-Steam test transfers a new
host checkpoint byte-identically with reverse joins and retained Player1..4/
owners. Existing real isolated fake-Steam preparation/recovery tests pass. Fault
observer registration/cleanup and unchanged synthetic context are verified.
No real game, real Steam storage, live multi-PC repair or MinGW build was run.

Restart relay amendment: every channel0 OPEN/DATA/CLOSE carries its restart
generation. Both new relays set this epoch before starting new game connections;
stale frames are ignored even if a stream ID is reused. Epoch changes are refused
with active streams. Control and data channels can progress independently, so a
control READY alone is not treated as proof that old data packets have drained.
The isolated relay regression covers delayed old OPEN/DATA/CLOSE against a new
same-ID stream. Launcher wire3 includes the tag; runtime protocol30 unchanged.

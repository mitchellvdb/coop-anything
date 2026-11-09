# Test65 / duo48

Based on Claude Code a48f117/test64 and previously published Codex59. Protocol30,
four-player routing and120 native signatures retained. Changes and limits:
[multiplayer recovery](runtime/MULTIPLAYER-RECOVERY.md).

The reported four-player run stopped at its first food purchase. The exact host
pre-state was not logged, so the original differing field/order remains unproven.
Every copy now uses the full native provisions opener and initial free-item
slots are normalized via native Item lifetime/swaps. Strong inventory/currency
guards remain. New initial digests support the next live report.

Host direct Player1..4 assignment uses persistent hero IDs and the existing
revision/table/file route. Selection survives row refresh. Players no longer
overlaps Restore. The log collector includes all three guest runtime files.

Resync is an acknowledged launcher checkpoint restart: normal game close,
journaled guest recovery, then fresh Host/Join. Unsaved progress may be lost.
No live combat repair, desync reset, forced termination or save replacement while
running. Solo guest .bat windows close manually; they have no guest launcher.
One lost runtime link stops every surviving guest. Fresh busy reports continue
to block host walking after a minute; missing busy heartbeats stop shared play.

PASS baseline64 and final native/fuzz/FIFO, four supply permutations and native
metadata/lifetime/partial-swap refusal, prepare for all three guest numbers,
owner broadcast to three guests, resync ACK/retry/backpressure/authority, runtime
disconnect fanout and wait guards, MSVC ASan, OpenGL overlay, both launcher suites
including fake-Steam preparation/recovery, eight exact native ABI proofs,120
regenerated signatures, English/GIF native layout and read-only DLC fixture/data.

NOT RUN: live game/installer/four-PC repair, real Steam save writes or MinGW build.
Native effects are simulated. Full DLC waves/shards/live hero validation,
arbitrary manual dungeon drags and unrelated loot-result fallback remain.
Guest always-nosave; host save preference preserved. Source only: no binaries,
game assets, saves, artwork or private logs. RELEASES.json records exact normal
and solo binary/kit hashes; the solo runtime pin is a build variant.

Written by Codex; previous Claude Code contributions retained.

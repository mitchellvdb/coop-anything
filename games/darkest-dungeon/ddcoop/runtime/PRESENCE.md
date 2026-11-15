# Hamlet player presence (test92)

Owner request: reuse the four dungeon blocks in the Hamlet as player locations.
The dungeon retains hero ownership/rank/stress. Town blocks run left to right in
player-number order, one per connected player, with player number/name and screen.
Self is green, others orange; an unavailable location is grey. F9 hides the panel,
not these blocks. Names come only from existing launcher snapshot2 (NAMES.md),
with Player N fallback; neither names nor locations are recorded in logs.

Runtime wire41, exact hello92: PRESENCE14 is a display-only, sequence-zero control
message with 6 bytes: kind (0 guest update,1 host snapshot), mask,4 location bytes.
Unknown0 is valid; absent bytes must be zero. Guest updates contain exactly one
non-host bit, checked against the authenticated connection player. Host snapshots
include bit0. Host forwards only statuses from ready authenticated links. No action
FIFO, ACK, native write or gameplay authority depends on presence. Malformed role
or sender controls are ignored; the wire decoder rejects malformed values.

Main thread reads the pinned normal TownDisplay and its native registered building
panels, and checks child vtables/state58 (1..4 visible,0/5..7 hidden/closing).
Ambiguous/unreadable panels report Status unavailable. Dungeon reads reuse the
existing validated raid view. Samples change at most every250ms, heartbeat every
second; remote locations expire after5s, including a missing host snapshot. Links
clearing/reconnecting clear their old locations. All native reads are on the game
thread; network workers only update a bounded status table under its own lock.
Launcher wire3 and snapshot2 remain unchanged. No PvP or new gameplay hooks.

The owner also requests an explicit guest Send logs to host button. Existing
snapshot2 role/runtime text and local command2 carry the caption/receipt status;
launcher/DIAGNOSTICS.md specifies the interface before packaging. No new runtime
message or gameplay authority is introduced by that diagnostics UI change.

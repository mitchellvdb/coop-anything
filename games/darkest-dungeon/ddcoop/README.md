# DDCoop — Darkest Dungeon co-op

Current development snapshot **test71 / duo54**, runtime protocol32 and launcher
wire/lobby3. Source lives on `game/darkest-dungeon-ddcoop` under this subtree;
other projects/shared playbook remain unchanged. Earlier contributions retained.

Each player uses their own copy. All connected players can initiate supported
shared Hamlet/dungeon actions, ordered through the host. Combat ownership uses
persistent hero IDs. Host game/launcher remain running; no manual host click is
required for the newly tested guest preparation/provisioning/embark sequence.
Guest slot replacement is backed up/restored, guests always -nosave. Host saving
follows its checkbox. Selected slots1..9 only; profile_9 forbidden.

New shared Tavern/Abbey drafts synchronize before Accept: first accepted intent
reserves the hero/slot, revision/epoch reject stale choices, paid effects happen
once through verified native functions. Native drop/source callbacks are guarded
before unchecked UI lookup or local model mutation. Open/reopened widgets project
the shared state. Guest departure waits for a usable town instead of falsely
ACKing and asking for a manual second click. See [ACTIVITY.md](runtime/ACTIVITY.md)
and [native analysis](runtime/ACTIVITY-NATIVE.md) for exact scope and evidence.

Read [BUILD.md](BUILD.md), [KIT.md](KIT.md), [HANDOFF-CURRENT.md](HANDOFF-CURRENT.md)
and RELEASES.json for validation, versions and exact artifact hashes. All peers
need the same kit. Source-only GitHub delivery; player binaries remain outside git.

Tests model native effects, with actual bridge/FIFO/codec,5000 four-copy draft
schedules/640000 requests, all4 guest/host initiators, native fault injection,
MSVC ASan, eleven read-only EXE proofs, isolated overlay/preview and full normal/
duo launcher/fake-Steam suites. Real rendering/new native integration still need
owner game tests. No real game/installer/save/Steam write was performed here.

New draft scope is six stress activities. Sanitarium extra choices retain the
existing path; clear with X then place, no new atomic slot-to-slot drag move.
Exact cross-copy native price/gold verification, complete combat-state digest,
remaining rank-based item identity, direct loot equip and the prior provisions
crash remain open. Resync uses the host checkpoint and may lose unsaved progress;
solo batch guests restart manually. Linux/MinGW/optional Capstone were not run.

Nine campaign-feature DLC groundwork and shard-aware model APIs from70 remain;
native supplies are gold-only and full live DLC support is unvalidated. See
[DLC.md](runtime/DLC.md) and the retained [recheck](runtime/RECHECK-2026-11-10.md).
Never activate Circus/PlayFab integration or bypass TamperingManager. No game
assets, Steam DLL, binaries, saves, artwork or private logs are published.
Written/published by Codex; previous Claude Code/Codex contributions retained.

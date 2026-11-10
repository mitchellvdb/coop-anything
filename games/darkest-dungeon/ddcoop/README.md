# DDCoop — Darkest Dungeon co-op

Current development snapshot **test72 / duo55**, runtime byte protocol32 and
launcher wire/lobby3. Exact runtime hello72 is mandatory for all participants.
Source is isolated on game/darkest-dungeon-ddcoop under this subtree; unrelated
projects/shared playbook are preserved. Prior contributions retained.

Players each run their own game. Supported shared actions can start from any
player and are ordered through the host; hero ownership uses persistent IDs.
Guests back up/replace/restore their selected slot and always run-nosave. Host
saving follows its checkbox; selected slots1..9 only, profile_9 forbidden.
Host keeps game/launcher running; no manual host approval for guest departure.

The owner confirmed71 Tavern drafts and provisioning in a real four-window test.
A subsequent stop at victory was a SELECT readiness race: host had ended combat,
all3 guests were14-27ms behind, and immediate apply refused their battle state.
SELECT now waits at the expected FIFO head for a same-area non-battle view, then
checks persistent hero identity and the actual native effect before success.
No ACK/effect while waiting, no overtaking and bounded timeout into refusal/STOP.
See [transition analysis](runtime/SELECTION-TRANSITION.md), [BUILD.md](BUILD.md),
[KIT.md](KIT.md), [HANDOFF-CURRENT.md](HANDOFF-CURRENT.md) and RELEASES.json.

All4 initiators/all3 receivers are covered by actual bridge/FIFO/codec fixtures,
including delayed combat/loading, wrong area/identity, malformed party vectors,
missing/no-effect native function, timeout and stopped sessions. Existing71
baseline passed; new tests fail75 assertions on old code, final native/MSVC ASan
pass. Twelve exact read-only proofs include native selected pointer writes.
Full normal/duo launcher/fake-Steam/restore, retained reservation/DLC/transport/
recovery tests, isolated overlay and English/embedded GIF preview pass.
Native effects are modeled; live repair still needs owner playtesting. No real
game/installer/owner-save/Steam writes. Linux/MinGW/optional Capstone not run.

Test71's six Tavern/Abbey stress activities remain shared before Accept. Clear
with X then place; no new atomic slot-to-slot move or Sanitarium extra choices.
Full combat-state/transaction digest, exact cross-copy prices, remaining rank-
based item uses, direct loot equip and prior provisions crash remain open. Nine
DLC-feature groundwork/shard model APIs remain, native provisioning gold-only;
[DLC.md](runtime/DLC.md) lists unvalidated full live DLC work. Resync uses a host
checkpoint and may lose unsaved progress; solo batch guests restart manually.
Never activate Circus/PlayFab integration or bypass TamperingManager.
No binaries, game assets, Steam DLLs, saves, artwork or private logs published.
Written/published by Codex; earlier Claude Code/Codex contributions retained.

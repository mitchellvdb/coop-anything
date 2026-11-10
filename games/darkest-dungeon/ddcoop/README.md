# DDCoop — Darkest Dungeon co-op

Current development snapshot **test70 / duo53**, runtime protocol31 and launcher
wire/lobby3. Source lives on `game/darkest-dungeon-ddcoop` under this subtree;
other projects and the repository's shared playbook remain unchanged.

Players each run their own copy. Any connected player may initiate supported
shared choices, ordered through the host. Combat ownership uses persistent hero
IDs. Guest campaign replacement is backed up/restored; guests always -nosave.
Host saving follows its checkbox. Selected slots1..9 only; profile_9 forbidden.

This independent recheck fixes queue-full STOP loss, unbounded transport sends,
unsafe camping indices and false town-state ACKs. Metadata/DLC preflight now
rejects corrupted campaigns and empty/incomplete required feature data before
backup/replacement, while preserving valid basegame saves without a DLC object.

Read [runtime/RECHECK-2026-11-10.md](runtime/RECHECK-2026-11-10.md) for findings,
proof and remaining risks. [BUILD.md](BUILD.md), [KIT.md](KIT.md) and
[HANDOFF-CURRENT.md](HANDOFF-CURRENT.md) are the current starting documents.
RELEASES.json records both variants' exact hashes. All peers need the same kit.

Campaign-DLC groundwork covers nine features: Musketeer, Shieldbreaker,
CC/Flagellant/Districts, CoM and Fire's Edge/Runaway/Duelist. The read-only data
checker supports feature/common fingerprints and report comparison. Shard-aware
transaction APIs are modeled and tested; native provisioning remains gold-only.
See [runtime/DLC.md](runtime/DLC.md). Full live DLC support is NOT claimed.

Native engine effects in tests are modeled. Local TCP/crypto, four-controller
restart simulations and four-core fake-Steam transfer tests do not replace a
real multi-PC game/Cloud test. Direct loot equip, complete combat-state digest,
remaining rank-based item identity, prior provisions crash and live DLC remain
open. No game assets, Steam DLL, binary, save, artwork or private log is published.
Never activate Circus/PlayFab paths or bypass TamperingManager. Passive arena_mp
save flags remain readable without such integration. Prior contributions by
Claude Code/Codex retained; this source delivery written/published by Codex.

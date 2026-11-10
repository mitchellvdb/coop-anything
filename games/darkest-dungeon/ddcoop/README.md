# DDCoop — Darkest Dungeon co-op

Development snapshot **0.5.0-test69 / duo52**, runtime protocol 31 and launcher
wire/lobby 3. Work stays on `game/darkest-dungeon-ddcoop` under this subtree;
the repository's shared playbook and other projects are unchanged.

Players each run their own copy. Supported shared actions are ordered through
the host; any player may initiate them. Combat ownership is per persistent hero
ID, while Hamlet party composition is shared. Guest campaign replacement is
backed up/restored; guests always use -nosave. Host saving follows its checkbox.
Only selected slots 1..9 are used; profile_9/the game's restore slot is forbidden.

Test69 follows a broad source/test review. It corrects a native hook instruction
boundary, removes false shared-success/private-progress fallbacks, stops combat
RNG mismatch, separates three guest movement votes, checks exact runtime hello
version and redacts known secrets from exported logs. All peers need this kit.

Read [runtime/REVIEW-2026-11-10.md](runtime/REVIEW-2026-11-10.md) for concrete
findings, evidence and remaining priorities. [BUILD.md](BUILD.md) gives current
commands; [KIT.md](KIT.md) and [HANDOFF-CURRENT.md](HANDOFF-CURRENT.md) describe
the release. RELEASES.json pins both variants; ordinary runtime pin is canonical.

Hamlet/provisions, travel, combat, hunger, loot and return are development
features, not a claim that every live scenario is validated. Direct loot equip,
full post-combat state checks, live four-PC restart and complete DLC remain open.
Color of Madness, Shieldbreaker and Musketeer have compatibility/data groundwork.

Native bridge fixtures use modeled engine effects. Real game, installer, saves,
Steam Cloud and multi-PC recovery were not run in this review. No game assets,
Steam DLL, executable, saves, private raw logs or mascot artwork are published.
Never use Circus/PlayFab or bypass TamperingManager. Preserve pins and record
validation when contributing. Prior Claude Code/Codex contributions retained.

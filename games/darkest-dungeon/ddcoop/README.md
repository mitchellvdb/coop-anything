# DDCoop — Darkest Dungeon co-op

Development snapshot **0.5.0-test68**, runtime protocol31. The project owner
selected this repository for per-kit source commits. Work is isolated on
`game/darkest-dungeon-ddcoop` under this directory; the shared playbook/main branch is unchanged.

Players each run their own copy. Hamlet actions are ordered through the host;
either player may initiate supported actions. Hero ownership limits combat input
to each player's heroes. A guest campaign slot is backed up, replaced with the
host snapshot and restored afterwards; guests use -nosave. Only the selected
campaign slot1..9 is used. Slot10/the game's restore area is refused.

Implemented areas include Hamlet purchases/upgrades, hero actions, party and
trinkets; provisions/departure; walking/interactions; combat choices; hunger,
loot and return. These are development features with known synchronization risks.
Color of Madness, Shieldbreaker and Musketeer have data/compatibility groundwork;
full DLC waves, shard purchases and live support remain unvalidated.

Read [BUILD.md](BUILD.md) for commands and [KIT.md](KIT.md) for this kit's checks
and limitations. Tests use the actual bridge with fake native game effects, strict
wire/fuzz tests, isolated OpenGL and fake-Steam launcher fixtures. This is not a
claim of complete automated real-client QA. Gameplay still needs owner playtests.
No assets, Steam DLL, executable, save or private user logs are published.

Use this branch as the development starting point. Submit changes on separate
branches; preserve runtime pins and record which kit and checks you tested.
Never use the Butcher's Circus/PlayFab path or bypass TamperingManager.

Existing code by Claude Code and Codex is retained. Snapshot publication by Codex.


Test68 fixes next-party composition after several remembered members enter
buildings. Busy remembered IDs are pruned from a proposed destination, while
strict pre-state checks and treatment remain intact. Every connected player
may add/move/remove available heroes, regardless of combat owner or whether
the host expedition screen is open. See runtime/PARTY-RETURN.md.
Direct equip from loot remains a separate unresolved report: new item/endpoint
drag diagnostics help identify its error. No gameplay fix is claimed for it.
Prior67 curio X/input phases and66 repair/restart/fault diagnostics are retained.

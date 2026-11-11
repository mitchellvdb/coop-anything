# DDCoop — Darkest Dungeon co-op

Development candidate **test75 / duo58**, runtime34/exacthello75, launcher wire3.
It is built and the native/ASan/model/launcher unit suites pass. It is **not yet
released**: the isolated preparation/restore integration check requires every
real Darkest process to be closed. RELEASES.json still lists the released74 kits;
CANDIDATE.json records the new build hashes and pending check honestly.

This candidate compares Stagecoach pools before recruitment, provisioning and
departure. Every recruit also checks logical candidate content before its local
native roster addition. Normal native RaidFinish shares host Rules before week
generation, including automatic wipes; worker threads only latch seed packets.
No manual host approval is needed. Host game/launcher stay running. Guests use
-nosave with selected-slot backup/replace/restore; host saving follows its checkbox.

Claude Code's campaign mod preflight is retained, with fail-closed unknown list
and linked/unreadable/incomplete-tree checks. Matching files do not establish
full gameplay compatibility. See [STAGECOACH-WEEK.md](runtime/STAGECOACH-WEEK.md),
[MODS.md](launcher/MODS.md), [BUILD.md](BUILD.md), [START-HERE.md](START-HERE.md),
[KIT.md](KIT.md) and [HANDOFF-CURRENT.md](HANDOFF-CURRENT.md).

All four roles/three guests are modeled in actual-bridge tests, with delayed
controls/town/ordered actions, exact local recruit content, cross-thread seed
arrival, STOP/timeouts/disconnects and real full queues. Sixteen read-only native
proofs cover137 pins/133 addresses (132 function entries plus1 witness)/89 hooks.
Native effects modeled; no new live native/multi-PC/Cloud QA or real game/save/
Steam mutation. Linux/MinGW/Capstone not run. Direct loot-to-hero equipping, full
transaction/combat-state checks, older provisions crash and full native/live DLC
remain open. Checkpoint recovery may lose unsaved progress; solo batch guests
restart manually. Slots1..9 only; profile_9 forbidden. No Circus/PlayFab/tamper
integration. No binaries, assets, saves, artwork or private logs are published.
Written/published by Codex; Claude Code's work and prior contributions retained.

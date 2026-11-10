# DDCoop — Darkest Dungeon co-op

New to the project? Read [START-HERE.md](START-HERE.md) first: what it is, how the parts fit, the rules,
how to build and test, and the mistakes already made.

Current snapshot **test74 / duo57**, runtime bytewire33/exacthello74 and launcher
wire/lobby3. All peers need matching new kits. Source is on the dedicated
`game/darkest-dungeon-ddcoop` branch under this subtree. Shared playbook/other
projects and existing Claude/Codex contributions are preserved.

Each player runs their own copy. Supported shared actions may start from any
player and are ordered through the host. Combat ownership uses persistent hero
IDs. The host game/launcher stay running without manual host approval for guest
shared choices. Guests back up/replace/restore a selected campaign slot and always
use-nosave; host saving follows its checkbox. Slots1..9 only; profile_9 forbidden.

The latest owner four-window run completed149 shared applies on all3guests,
matching checked post-return Town state and3later recruits. A weekly activity
notice revealed a legitimate native AssignHero call being mistaken for a new UI
Accept. All copies omitted the native operation despite matching checked digests.
The exact pinned weekly caller now invokes native once without network re-emission;
UI Accept still requires shared drafts. See [ACTIVITY-WEEKLY.md](runtime/ACTIVITY-WEEKLY.md).

New fixture:322 before-fix assertion failures, then corrected native/MSVC ASan
passes across all4roles/all6stressactivities, empty/committed models, one modeled
native effect/cost, no messages/notices, normal UI ordering, STOP and nested apply.
Fourteen read-only native proofs pass:136 named pins/132 distinct addresses,
131function entries plus1caller witness,88distinct live/early hooks. Full normal/
duo launcher/fake-Steam/restore, native/fuzz/FIFO/transport/recovery/reservation/
DLC models, isolated overlay/hidden English/embedded GIF preview pass. Native
effects modeled; no new live fix verification or real game/save/Steam mutation.

Earlier provisional activity/provisioning/departure/selection/prop choices retained.
Owner latest logs confirm propitem3/X1; no hand/shovel route observed. Full live
DLC, direct loot-to-hero equip, full transaction/combat-state verification and an
older provisions crash remain open. Checkpoint recovery may lose unsaved progress;
solo batch guests restart manually. Linux/MinGW/optional Capstone not run.

See [BUILD.md](BUILD.md), [KIT.md](KIT.md), [HANDOFF-CURRENT.md](HANDOFF-CURRENT.md)
and RELEASES.json. Never use Circus/PlayFab or bypass TamperingManager. No game
assets, binaries, Steam DLLs, saves, artwork or private logs are published.
Written/published by Codex; earlier Claude Code contributions retained.

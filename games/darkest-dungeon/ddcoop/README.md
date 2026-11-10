# DDCoop — Darkest Dungeon co-op

Current snapshot **test73 / duo56**, runtime protocol33/exacthello73 and launcher
wire/lobby3. All peers need matching new kits. Source stays on the dedicated
`game/darkest-dungeon-ddcoop` branch under this subtree; other projects/shared
playbook and existing Claude/Codex contributions are preserved.

Each player runs their own game. Supported shared actions can start from any
player and are ordered through the host. Hero ownership uses persistent IDs.
Guests back up/replace/restore their selected slot and always use-nosave; host
saving follows its checkbox. Selected slots1..9 only, profile_9 forbidden.
Host game/launcher remain running; no manual host approval for guest choices.

This kit repairs a local-only obstacle hand path: a normal native NULL actor
made the old hook fall back to native with no request. Hand/tool/curio button/X,
item drop and routed inventory click now use one ordered resolution. First
accepted valid choice wins; exact revision/area/prop/kind/actor protects crossed
or stale choices, including X/reopen and old OPEN. NULL native actor semantics
are preserved. Pure preflight precedes selection/Rules/native effects.

Early typed drop and matched source cleanup prevent local inventory prediction
and stale Restore counts. Ordinary food/hero use remains distinct through the
native routing predicate, and unsupported curio items reject before effects.
See [PROP-CHOICES.md](runtime/PROP-CHOICES.md), [BUILD.md](BUILD.md), [KIT.md](KIT.md),
[HANDOFF-CURRENT.md](HANDOFF-CURRENT.md) and RELEASES.json for scope and hashes.

Native/MSVC ASan actual bridge/FIFO/codec tests pass: seven routes/all4 initiators/
all3 receivers, NULL actor, delayed panel, source cleanup/last item,25 competing
pairs x4 winners x3 receivers,14 native compatibility refusals, click-routing and
no local fallback. Old72 NULL control reproduces localhand1/sharedpackets0; the
initialsix-route before-fix test fails62 assertions. Thirteen exact read-only
native proofs cover135 named pins/131 entrypoints/88 hooks. Full normal/duo
launcher/fake-Steam/restore, retained transport/recovery/DLC/reservation models,
isolated overlay and English/embedded GIF preview pass. Native effects modeled;
new interactions still need owner playtests. No real game/installer/save/Steam
write. Linux/MinGW/optional Capstone not run. Existing warnings recorded.

Common native routes are covered, not a claim that every DLC/mod-specific effect
has been live-tested. Direct loot-to-hero trinket equip, full transaction/combat-
state verification, prior provisions crash and full native/live DLC remain open.
The bar, guest provisioning/departure and post-combat selection fixes are retained.
See [DLC.md](runtime/DLC.md); native supplies remain gold-only. Recovery uses the
host checkpoint and may lose unsaved progress; solo batch guests restart manually.
Never activate Circus/PlayFab integration or bypass TamperingManager. No binaries,
game assets, Steam DLLs, saves, artwork or private player logs are published.
Written/published by Codex; previous Claude Code/Codex contributions retained.

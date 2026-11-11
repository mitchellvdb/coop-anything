# DDCoop - Darkest Dungeon co-op

Current development kit: **test81 / solo duo60**, runtime protocol36/exacthello81,
launcher wire3. All Claude Code changes through local test80/57a494b are retained,
including signed host updates, shared weekly GUID generation and activity/skill
persistence. Either player can initiate supported shared actions; combat remains
limited to each hero's assigned player.

Test81 shares inventory-full informational dismissal by the causing loot order,
waits for same-turn combat animations and bounds duplicate native loot clicks.
Protective actor/RNG/content checks remain; some Stagecoach, curio and loot causes
are still under investigation. It is a development build requiring live playtests.
See [review](runtime/RECHECK-2026-11-11.md), [current handoff](HANDOFF-CURRENT.md),
[BUILD.md](BUILD.md), [KIT.md](KIT.md) and [launcher updates](launcher/UPDATE.md).

Windows/MSVC runtime/protocol/FIFO, final bridge ASan,19 exact native proofs and
both full launcher suites including isolated backup/restore passed. Native engine
effects are modeled in fixtures. No new real-client gameplay execution is claimed;
Linux/MinGW/Capstone were not run. Full live DLC remains unvalidated.

Sources only: no game files, binaries, saves, artwork, signing key or private logs.
Use the dedicated game/darkest-dungeon-ddcoop branch; the playbook/main is untouched.
Written/published by Codex; Claude Code and previous contributions retained.

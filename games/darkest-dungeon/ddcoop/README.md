# DDCoop - Darkest Dungeon co-op

Current development kit: **test82 / solo duo61**. Runtime gameplay is test81;
exact runtime hello82 changes the kit id, so older update-capable launchers get
the signed new kit before entering a session with diagnostic controls26..31.
Runtime wire36 and launcher wire3 remain. Claude Code's prior work is retained.

New: automatic private reports from all connected players to the host. Any
player can click Copy logs; new native desync markers, resync and session events
also collect. The host receives one grouped ZIP with explicit missing/failed
players and missing/truncated files. No outside service/account or public log
upload. See [diagnostics](launcher/DIAGNOSTICS.md), [current handoff](HANDOFF-CURRENT.md),
[BUILD.md](BUILD.md), [KIT.md](KIT.md) and [updates](launcher/UPDATE.md).

Windows runtime, both full launcher suites including isolated backup/restore,
portable diagnostics and actual Windows file-adapter ASan tests pass. Four
launchers, paced/backpressured transfer, integrity/auth/order/size/deadline
failures and delayed receipt overlap are modeled. Independent Python ZIP checks
real fixture output. Live Steam diagnostic delivery still needs the owner's
next test. Native effects elsewhere are modeled; no real gameplay run, installed
game/save mutation or Linux/MinGW/Capstone run claimed. Native hooks unchanged;
full live DLC and older Stagecoach/curio/loot causes remain unvalidated/open.

Sources only: no binaries, assets, saves, artwork, private keys or private logs.
Private reports may contain names/paths; never commit or publicly upload them.
Dedicated game/darkest-dungeon-ddcoop branch; main/playbook untouched.
Written/published by Codex; prior Claude Code and Codex contributions retained.

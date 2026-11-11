# Start here: DDCoop for a new developer

DDCoop is a co-op mod for **Darkest Dungeon 1** (Steam, Windows x64) for 2 to 4 players. Each player runs their
own legitimate copy of the game; together they play one campaign that belongs to the host.

- **Hamlet:** every player can act at the same time (blacksmith, guild, stagecoach, tavern, abbey, provisions...).
- **Dungeon:** every hero belongs to one player; that player chooses the hero's combat actions. Walking, curios
  and other shared choices can come from any player.
- Every action is ordered through the host. The host's game decides; then every copy runs the same game
  function in the same order.

This file explains how the parts fit, how to build and test, the rules the project keeps, and the mistakes
already made. The current state and open problems are in [HANDOFF-CURRENT.md](HANDOFF-CURRENT.md); exact build
commands in [BUILD.md](BUILD.md). The git history of this branch is the detailed changelog.

## 1. What you need

- **Darkest Dungeon on Steam, base game, your own copy.** The mod supports exactly one game build: Windows x64
  build 28385, `_windows\win64\Darkest.exe` SHA-256
  `f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12`. On any other build the runtime switches
  itself off instead of guessing (see section 7).
- **Windows 10/11, Visual Studio 2022** (C/C++ tools and Windows SDK) and **Python 3**. Build from an x64 Native
  Tools Command Prompt. Optional: MSYS2 UCRT64 (MinGW build of the launcher) and WSL/Linux (portable launcher
  tests under ASan/UBSan).
- **Two or more game copies to test with:** two PCs with two Steam accounts, or the **duo build**, which runs up
  to four game windows on one PC (section 5).

## 2. How it fits together

```
 DDCoop.exe (launcher, outside the game)          Darkest.exe (one per player)
 ┌──────────────────────────────────┐            ┌────────────────────────────────────────┐
 │ Host / Join, Steam lobby          │  starts    │ SDL2.dll  = DDCoop bootstrap (proxy)    │
 │ copies the host's campaign into   │ ─────────► │   forwards to SDL2_orig.dll (genuine)   │
 │   the guest's slot (with backup)  │            │   loads mods\DDCoop\ddcoop-runtime.dll  │
 │ writes ddcoop.ini, ddcoop-heroes  │            │ runtime: hooks game functions, orders   │
 │ carries the runtime's TCP stream  │ ◄────────► │   actions through the host, F9 panel    │
 │   over Steam networking           │  loopback  │                                         │
 └──────────────────────────────────┘            └────────────────────────────────────────┘
```

| Part | Where | What it does |
|---|---|---|
| Bootstrap | `runtime/sdl_proxy.c` | A proxy `SDL2.dll`. The game loads it instead of SDL2; it forwards to the genuine `SDL2_orig.dll`, loads the runtime from `mods\DDCoop` and calls it once per frame on the game's main thread. Pinned by SHA-256. |
| Runtime | `runtime/`, `src/` | Lives inside the game. Checks the exe hash, finds the game functions it needs (`src/addresses.inc`), hooks the game's own model functions (upgrade purchase, assign hero, recruit, provisions, combat choice, curio choice, departure, ...). A guest's action becomes a request; the host's game decides; the host sends ordered "apply" messages; every copy runs the native function itself. Encrypted TCP with a password handshake (`src/dd_session`, `src/dd_net`). Draws the F9 panel with OpenGL. |
| Launcher | `launcher/` | `DDCoop.exe`. Host/Join through a friends-only Steam lobby; the runtime's TCP stream travels over SteamNetworkingMessages, so nobody types an IP address. Puts the host's campaign into the guest's slot through a separate helper process that uses the game's own Steam storage API, after a verified backup, and restores the guest's own slot afterwards. Writes `mods\DDCoop\ddcoop.ini` (role, password, port) and `ddcoop-heroes.ini` (who owns which hero). Installs, repairs and uninstalls the mod. See [launcher/README.md](launcher/README.md). |
| Tools | `tools/` | Address table generator, kit packer (`make_dropkit.py`), RNG trace comparison. |
| Tests | `tests/`, `launcher/tests/`, `runtime/test-*.cmd` | Unit tests, fuzzers, a fake Hamlet/dungeon with host + 3 guests, and read-only "layout proofs" against the real exe. |
| Research probes | `testkit/` | The read-only probes (hardware breakpoints, logging) used to map the game in the beginning. |

### Design decisions worth knowing

- **The host's save is the truth.** At session start every game loads the same campaign together (a barrier:
  the host waits for all chosen friends; a friend who arrives after the campaign loaded is refused). Guests
  never save.
- **Host authority, identities not pointers.** Messages carry stable ids (hero roster id, item id, activity and
  slot), never live pointers. A request based on stale state is refused, not repaired.
- **Run the game's own code.** Every copy applies an action by calling the same native game function the UI
  would call, so the game's own checks (gold, requirements, room) run everywhere. Manual memory writes are a
  last resort.
- **Combat is deterministic** given the same save and the same inputs (proved early with unit snapshots). The
  game has named random streams (`Battle`, `Rules`); copies must consume them identically.
- **When copies disagree, stop.** A detected desync or a lost connection raises "Sync stop": shared actions stop
  for everyone and the players restart from the host's save. An automatic checkpoint restart exists for some
  cases ([runtime/AUTOMATIC-RESYNC.md](runtime/AUTOMATIC-RESYNC.md)).
- **Hero ownership:** each hero belongs to player 0 (host) or 1..3 (guests); players tick their heroes in the
  launcher; the host decides. Format and rules: [launcher/OWNERSHIP.md](launcher/OWNERSHIP.md).

## 3. Rules this project keeps

These are hard rules, not preferences. Keep them if you continue the work.

- **Never use the Butcher's Circus / PvP / PlayFab path** (matchmaking, ranked play, `ReportSuspiciousMatch`).
  Co-op uses its own connection only. In build 28385 the functions at `0x1406e3390`, `0x1406debf0`,
  `0x1406dfb40` and `0x1406dfbc0` belong to the Circus (they use `#Status_InCircus`, `matchmaking_%s`,
  `str_circus_offline_match_popup`); they look like town-to-expedition screens and a test build that called
  them crashed. The normal departure is `0x1406e7c70`.
- **Before calling or hooking any game function, read the strings and classes it and its callees use**, and
  confirm the normal game actually reaches it. A hook that stays silent in a real run is the wrong function.
- **Never bypass `TamperingManager`** and never use the developer option `disable_local_anticheat_checks`. They
  protect other people's online play.
- **Legitimate copies only, base game first.** DLC work is groundwork only and must be tested on DLC the player
  owns ([runtime/DLC.md](runtime/DLC.md)). Leave a `dlc/` folder you do not own alone.
- **Saves:** co-op uses one campaign slot the host chooses (menu slots 1..9 = `profile_0`..`profile_8`). Never
  profile 9 (`steam_profile_9`): it is the game's own restore area. Back up before replacing anything; guests
  always run `-nosave`, and because `-nosave` does not stop every write, the guest's slot is restored and
  verified after the session.
- **Nothing private in git:** no binaries (`.dll`, `.exe`, `.zip`), saves, raw logs or artwork. Game logs
  contain Steam names and SteamIDs; the launcher's "Copy logs" redacts tickets, tokens and passwords only.

## 4. Build and test

From an x64 Native Tools Command Prompt in the repository root (full list in [BUILD.md](BUILD.md)):

```
set "DD_GAME_EXE=<game dir>\_windows\win64\Darkest.exe"
runtime\test-windows-msvc.cmd          & rem runtime tests (fake game, never launches it)
runtime\build-windows-msvc.cmd         & rem build\win\ddcoop-runtime.dll
python -I tests\test_hook_layout.py "%DD_GAME_EXE%"   & rem read-only proof of every hook site
```

After rebuilding the runtime, put its SHA-256 in `LR_HASH_RUNTIME` in `launcher/src/lr_hashes.h` (the launcher
refuses any other runtime, and the first 8 hex digits are the kit id both players must share). Then:

```
set "DDC_VERSION=0.5.0-test75"
launcher\build-windows-msvc.cmd
launcher\test-windows-msvc.cmd         & rem close every Darkest Dungeon first
```

- The launcher tests use a fake Steam API and throwaway folders; `test_lr_prepare_win` refuses to run while
  `Darkest.exe` is running.
- The relay part of `test_lr_core` needs local port 27360: do not run two test suites (or a game) at once.
- Pack a kit with `python -I tools/make_dropkit.py <staging dir> <zip> <version>`; the staging dir holds
  `DDCoop.exe`, `SDL2.dll` (bootstrap, `runtime\build-bootstrap-msvc.cmd`) and `DDCoop\ddcoop-runtime.dll`.
- Tests with fake game functions prove the bookkeeping, not the game's behaviour. Only a real session proves
  that a feature works.

## 5. Playing a test session

- **Install:** unzip `DDCoop-install.zip`, run `Install DDCoop.bat` (installs into the Steam game folder,
  keeps the genuine SDL2 as `SDL2_orig.dll`). `Repair` and `Uninstall` scripts sit next to it.
- **Host:** open DDCoop, choose the campaign slot and the number of players, click Host, invite friends.
  **Guests:** click Join. Every player needs the same kit (same runtime hash).
- **In game:** F9 shows the co-op panel. Each player ticks their heroes in the launcher.
- **Duo build (several windows on one PC):** built with `DD_DUO` (see BUILD.md "Variants"). Start the host
  normally, then the `Start second/third/fourth window` scripts for guests 1..3. It is for testing only, never
  a player kit.
- **Logs:** "Copy logs" in the launcher collects `ddcoop-fw-host.log` / `ddcoop-fw-guest*.log` (runtime),
  `DDCoop-launcher.log`, the preparation log and the game's `app.log`. Useful lines: `host accepted guest town
  request` (ordered applies, with a sequence number), `local notice` (what the player saw), `Sync stop`,
  `Town: back from the raid` (state digests after a raid). The logs grow over several days: find the session by
  its `starting ===` line.

## 6. Where the knowledge is

| Document | About |
|---|---|
| [HANDOFF-CURRENT.md](HANDOFF-CURRENT.md) | Current state, latest kits, open problems |
| [BUILD.md](BUILD.md) | Supported build, all build/test commands, per-version notes |
| [launcher/README.md](launcher/README.md), [launcher/OWNERSHIP.md](launcher/OWNERSHIP.md) | Launcher, campaign transfer, hero ownership format |
| [launcher/MODS.md](launcher/MODS.md) | Campaign mods: the check that every PC has the same mod files |
| [launcher/UPDATE.md](launcher/UPDATE.md) | Signed kit updates from the host to friends with an older launcher |
| [runtime/PROVISIONS.md](runtime/PROVISIONS.md) | Shared supplies / provisions |
| [runtime/DUNGEON.md](runtime/DUNGEON.md), [runtime/PARALLEL-AUDIT.md](runtime/PARALLEL-AUDIT.md) | Dungeon actions, which inputs each player may give |
| [runtime/DESYNC.md](runtime/DESYNC.md), [runtime/FORMATION.md](runtime/FORMATION.md), [runtime/FRESH-RAIDS.md](runtime/FRESH-RAIDS.md), [runtime/PARTY-RETURN.md](runtime/PARTY-RETURN.md) | Desyncs found in real sessions and their fixes |
| [runtime/PROP-LIFECYCLE.md](runtime/PROP-LIFECYCLE.md), [runtime/PROP-CHOICES.md](runtime/PROP-CHOICES.md) | Curios and obstacles |
| [runtime/ACTIVITY.md](runtime/ACTIVITY.md), [runtime/ACTIVITY-NATIVE.md](runtime/ACTIVITY-NATIVE.md), [runtime/ACTIVITY-WEEKLY.md](runtime/ACTIVITY-WEEKLY.md) | Tavern and Abbey placement |
| [runtime/INPUT-PHASE.md](runtime/INPUT-PHASE.md), [runtime/SELECTION-TRANSITION.md](runtime/SELECTION-TRANSITION.md) | Combat input phases, post-combat selection |
| [runtime/MULTIPLAYER-RECOVERY.md](runtime/MULTIPLAYER-RECOVERY.md), [runtime/AUTOMATIC-RESYNC.md](runtime/AUTOMATIC-RESYNC.md) | Four players, checkpoints, restart |
| [runtime/DLC.md](runtime/DLC.md) | DLC groundwork |
| `runtime/REVIEW-2026-11-10.md`, `runtime/RECHECK-2026-11-10.md` | Two code reviews (written in Dutch) |

**Early research (30-31 Oct 2026)** is on the branch `game/darkest-dungeon` of this repository, file
`games/darkest-dungeon/NOTES.md`: how combat input, random streams, town actions, saves and slots were found,
with the probe results. Its addresses are for an **older exe** (SHA-256 `4d78fbfa...`, September 2026); the
class names, methods and conclusions still apply, the addresses do not. Use `src/addresses.inc` for build 28385.

How the game was mapped, in short: Ghidra on the exe, using the RTTI class names (thousands of `.?AV` names
are present) and the strings a function plays or logs (sounds such as `/ui/town/...` are good anchors); then
read-only runtime probes with hardware breakpoints and memory diffs while the owner played.

## 7. When Steam updates the game

The game hash is pinned in several places (`runtime/plugin.c`, `launcher/src/lr_hashes.h`, the layout proofs
in `tests/`; `git grep f9d914ca` lists them). `src/addresses.inc` holds the first 48 bytes of every function the
runtime uses; at startup the runtime compares them with the running game, and if a function moved it only
accepts a pattern that occurs exactly once, otherwise the mod stays off.

To support a new build: find each function's new address (Ghidra, strings and RTTI as above, then the strings
rule from section 3) and update the list in `tools/gen_address_table.py`; regenerate `src/addresses.inc` with
`python -I tools/gen_address_table.py <Darkest.exe> src/addresses.inc`; update the hash pins; re-run every
read-only layout proof and the runtime tests. Never relax a pin to make an unknown build start.

## 8. Mistakes already made (so you don't repeat them)

- **Wrong function, plausible name:** the Circus functions in section 3 looked like the expedition screens.
  Read the strings first.
- **The game calls your hooks too.** A hook on "assign hero to a building" also caught the game's own automatic
  weekly assignment and blocked it as if a player had clicked (fixed in test74). Tell native callers from UI
  callers by the exact return address.
- **One player acting while another has a window open** (a guest walking while the host is in a menu) split the
  copies. The host now waits for busy guests.
- **Players who join late** started from a different state. All games now load the campaign together, and late
  friends are refused.
- **Items by slot instead of identity:** the same trinket could sit in a different bag slot on a guest, so a
  move addressed by slot went wrong there. Items are addressed by identity now.
- **A fixed-map mission at departure** stopped shared actions until it was handled explicitly.
- **Equal digests are not equal saves.** The state digests in the logs cover what they check, nothing more.
- **Research tooling:** a stack-trace address is mid-function (decompile the containing function; creating a
  function there splits the real one in Ghidra). Execute breakpoints need the resume flag set before any early
  return, or the game hangs. Arm breakpoints on new threads immediately (loading runs on fresh threads).
  Comparing only random generator states proves nothing (one seed always gives one sequence): compare game state.
- **Slot check:** the campaign menu briefly mounts every campaign to list it; only act on a fully loaded one.
- **Windows shell:** PowerShell `>` writes UTF-16; read logs as UTF-8 only after converting.

## 9. Related project

"DD Co-op" (Nexus Mods, Darkest Dungeon mod 2282) is a separate co-op mod where the host runs the town and
guests play their heroes in fights. It has no source or license; this project uses only its public README and
changelog as a reference (lessons in NOTES.md) and copies no code.

Test75: read [STAGECOACH-WEEK.md](runtime/STAGECOACH-WEEK.md) for native week-seed timing and the candidate-pool comparison.

Current tested snapshot: test81 / duo60, protocol36/exacthello81. See BUILD.md and runtime/RECHECK-2026-11-11.md. Signed ordinary updates use launcher/UPDATE.md; solo duo remains manual.

Current release: test82 / solo duo61, exact runtime82, wire36/launcher3. Private automatic report transfer and bounds are documented in launcher/DIAGNOSTICS.md; all participants use the matching kit.

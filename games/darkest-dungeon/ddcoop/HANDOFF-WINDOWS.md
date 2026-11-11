# Handoff: Darkest Dungeon co-op (DDCoop) — continue on the owner's Windows PC

> **Voor project owner:** pak `DDCoop-handoff.zip` uit (bijv. naar `C:\dev\ddcoop-handoff`), open daar Claude Code en
> zeg: *"Lees HANDOFF-WINDOWS.md en ga verder."* De stappen in "First steps" doet Claude Code dan zelf.

State on 2026-10-31, written by the Claude Code session that built the launcher (Linux side, OPSBOX).

## 1. What the project is

The owner (project owner) plays **Darkest Dungeon 1** (Steam, Windows x64) in co-op with friends who each own the game.
Goal, agreed with the owner:

- one shared campaign (the host's), in **campaign slot 3** (`coop_slots = 3`);
- the Hamlet usable by everyone **in parallel** (blacksmith, guild, tavern at once), host-authoritative, changes synced;
- in the dungeon every player runs the expedition **in their own game in lockstep** and commands **their own heroes**;
- **our own mod**, not the existing "DD Co-op" on Nexus;
- installing must be foolproof: unzip, double-click a `.bat`, done.

It follows the process of the `cranebeaker/coop-anything` hub (`AGENTS.md`). Claim issue: #267; the hub write-up is
`games/darkest-dungeon/NOTES.md` (included in this zip; research addresses, town action map, save system,
determinism proof).

## 2. Current state (proven on two real PCs, 31 Oct 14:07)

| Part | State |
|---|---|
| DDCoop launcher 1.1.1 (`DDCoop.exe`, ours) | **Works end to end**: install via .bat, Steam lobby (AppID 480), friend join / invite / code, host's slot-3 campaign sent to the guest (backup first), per-session `ddcoop.ini`, guest game started with `-nosave`, the runtime's TCP connection relayed over Steam, version check, clean rejoin. |
| Runtime `ddcoop-runtime.dll` (Codex's, 0.4.0 rebuilt with our 48-byte table, sha256 `aff98e68…5374`) | READY 19/19 on both PCs; `direct TCP session ready`; **host → guest hero placement in a stress building syncs**. |
| Not yet (runtime scope) | guest actions (placement refused by design), upgrade / recruit / gold sync in both directions (guest spends gold locally → copies diverge; harmless because the guest runs `-nosave` and gets the host's save each session), embark / dungeon lockstep, hero ownership. |

The next runtime task is written down for Codex: **`codex-opdracht-stad.txt`** (step A: stop guest-local actions; step B:
host authority for all town actions using the tested `src/dd_proto` + `src/dd_town`; step C: embark + ownership,
design first).

## 3. Working agreements (keep these)

- **The in-game runtime layer is written by Codex, not by Claude.** In the earlier session a safety classifier stopped
  Claude twice while writing in-process hook code for the game, so the split became: Claude does research, tools, the
  launcher (outside the game process), protocol/town logic as tested platform-independent C, reviews and kits; Codex
  writes and builds `ddcoop-runtime.dll`. If you want this split changed, that is the owner's call — don't drift
  into it silently.
- The **DDCoop kit installs into the real Steam install** (owner's choice, 31 Oct). Research/probe kits used a separate
  test copy (`%USERPROFILE%\DD-coop-test`); never mix the two.
- Guests always run `-nosave`; the host's save is the truth. Profile 9 (`steam_profile_9`) is the game's restore area:
  never touch it. Other campaign slots are never touched.
- Never the pirated copy; base game behaviour only, leave DLC alone. **Never the PvP / PlayFab path** (Butcher's Circus).
- Ask the owner before: publishing anything, forks/issues/PRs, installs that change the system, accounts.
  Hub "DD Co-op" entry update: owner said "not yet".
- No real names or Steam usernames in anything published (logs contain persona names and SteamIDs).
- Communicate with the owner in **Dutch**, plainly; deliver files somewhere he can open them.

## 4. What's in the handoff zip

| Item | What |
|---|---|
| `darkest-dungeon-coop.bundle` | The whole repo with history (`git clone darkest-dungeon-coop.bundle darkest-dungeon-coop`), branch `worktree-rng-compare` = current (HEAD `248bac2`+this doc). There is no remote. |
| `runtime/SDL2.dll`, `runtime/DDCoop/ddcoop-runtime.dll` | Codex's bootstrap (sha `e7f1c4a7…a627`) and current runtime (`aff98e68…5374`); input for `tools/make_dropkit.py`. |
| `DDCoop-install.zip` | The current player kit (1.1.1). |
| `NOTES.md` | Hub research notes (addresses, town action map, save system, determinism). |
| `codex-opdracht-stad.txt`, `codex-feedback-2.txt` | The open Codex task and the address-table fix explanation. |

## 5. Repo map (what matters now)

| Path | What |
|---|---|
| `launcher/` | **DDCoop.exe** — see `launcher/README.md`. `src/lr_wire` (launcher↔launcher messages, strict decoder), `lr_relay` (TCP streams over peer messages), `lr_core` (session, password, campaign transfer, ini, keep-alive), `lr_steam` (game's own `steam_api64.dll`, flat API, AppID 480), `lr_install` (`--install/--repair/--uninstall/--start`), `main.c` (window, checklist, F9, watchdog), `lr_hashes.h` (pinned SHA-256s), `kit/` (.bat + LEESMIJ). |
| `launcher/tests/` | `test_lr_wire` (round trips + 1M fuzz), `test_lr_core` (two launchers end to end over an in-memory transport with backpressure, fake games on real loopback TCP, 14 scenarios). |
| `src/` | The framework / shared modules: `dd_proto` (co-op wire format), `dd_town` (host authority for town actions), `dd_session` + `dd_crypto_*` (password handshake, AES-GCM), `dd_net`, `dd_saves` (coop_slots), `dd_addr` + `addresses.inc` (address table, **48-byte unique patterns**). `main.c` here is the old read-only framework 0.2 proxy; the live runtime is Codex's. |
| `tools/` | `gen_address_table.py` (refuses non-unique patterns), `make_dropkit.py` (builds DDCoop-install.zip, checks hashes), `check_forwarders.py`, `compare_rng.py`. |
| `testkit/`, `fwkit/`, `netkit/` | Older research probe / framework / network self-test kits (test copy only). |
| `HANDOVER.md` | Earlier handover (research + framework). This file supersedes it for the launcher. |

## 6. First steps on Windows

1. `git clone darkest-dungeon-coop.bundle darkest-dungeon-coop` and `git checkout worktree-rng-compare`.
2. Toolchain: **MSYS2** (https://www.msys2.org) — ask the owner before installing. In the *UCRT64* shell:
   `pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-binutils python zip`.
   (Previously the exe was cross-compiled on Linux with `x86_64-w64-mingw32-gcc`; in MSYS2 the same tools are
   called `gcc`, `windres`, `strip`.)
3. Build DDCoop.exe (from the repo root, UCRT64 shell):

       windres -I launcher/src launcher/src/ddcoop.rc -O coff -o build/win/ddcoop.res
       gcc -std=c11 -O2 -Wall -Wextra -Werror -municode -mwindows -Ilauncher/src -D_WIN32_WINNT=0x0601 \
           -DDDC_VERSION='"1.1.2"' -o build/win/DDCoop.debug.exe \
           launcher/src/main.c launcher/src/lr_install.c launcher/src/lr_steam.c launcher/src/lr_core.c \
           launcher/src/lr_relay.c launcher/src/lr_wire.c launcher/src/lr_files.c launcher/src/lr_sock.c \
           build/win/ddcoop.res -lws2_32 -lbcrypt -luuid -lwinmm -lshell32 -lole32 -luser32 -lgdi32 -ladvapi32 \
           -static -static-libgcc -g
       strip -o build/win/DDCoop.exe build/win/DDCoop.debug.exe

   (`launcher/build-windows.sh` does the same with the cross-compiler names; `launcher/remote-build.sh` is the
   Linux-only route via the Proxmox container and does not apply here.)
4. Tests: `test_lr_wire` builds anywhere (`gcc -std=c11 -Ilauncher/src launcher/tests/test_lr_wire.c launcher/src/lr_wire.c`).
   `test_lr_core` and the ASan builds use POSIX (`mkdtemp`, `usleep`, `clock_gettime`, `setarch`): run
   `sh launcher/build-linux.sh` in **WSL** (or port those calls). Keep both tests green before every kit.
5. Kit: `python tools/make_dropkit.py <dir with SDL2.dll and DDCoop/ddcoop-runtime.dll> DDCoop-install.zip 1.1.2`.
   It refuses binaries whose hash differs from `launcher/src/lr_hashes.h`.
6. Advantage of being on Windows now: you can **run** DDCoop.exe and the game yourself. The Windows code was
   developed without ever running it locally; every bug so far was found from the owner's logs.

## 7. Where things live on the owner's PC

- Game: `D:\SteamLibrary\steamapps\common\DarkestDungeon` (Steam in `C:\Program Files (x86)\Steam`).
- Installed mod: `<game>\mods\DDCoop\` = `ddcoop-runtime.dll`, `ddcoop.ini` (rewritten per session, `role=off` after),
  logs `DDCoop-launcher.log`, `ddcoop-fw-host.log` / `ddcoop-fw-guest.log`; launcher in `mods\DDCoop\launcher\`
  (`DDCoop.exe`, copy of the game's `steam_api64.dll`, `steam_appid.txt` = 480).
  `<game>\_windows\win64\SDL2.dll` = Codex's bootstrap, original kept as `SDL2_orig.dll`.
- Saves: `Steam\userdata\<accountid>\262060\remote\profile_N` (slot 3 = `profile_2`; the game calls it
  `steam_profile_2`). Guest backups: `%USERPROFILE%\DDCoop-backups\<timestamp>\profile_2`.
- "Logs kopiëren" in DDCoop puts everything in `Desktop\DDCoop-logs-<timestamp>\` (+ `checks.txt`).

## 8. Lessons that cost a test round each (don't repeat)

- Steamworks flat API **bool returns are 1 byte (AL)**: declare them `_Bool`, never `int` (1.0.0 froze in an endless
  `GetNextCallback` loop).
- **`SteamAPI_ManualDispatch_Init` goes after `SteamAPI_Init`** (before it, not one callback ever arrived; 1.0.2).
- x64 `SteamNetworkingMessage_t`: data ptr at 0, size at 8, sender identity at 16.
- Steam can raise a session request before the lobby member list shows the peer: re-check instead of refusing (done).
- Close the Steam session of a peer that left, on both sides, or a quick rejoin hits a half-open session (1.1.1).
- A message box inside the timer tick re-enters Steam's callback loop: defer it (done in `main.c`).
- Address patterns must be **unique in .text**: 32 bytes of `provisions_buy` also matched `0x1404fa480`, which turned
  the whole runtime off. `gen_address_table.py` now enforces uniqueness at 48 bytes.
- The launcher's child game must not inherit `SteamAppId=480` (filtered in `launch_game`).
- Codex's bootstrap finds the runtime via `GetModuleFileNameA` (exe-relative, ANSI path).

## 9. Open items / next steps

1. **Codex: town sync** (`codex-opdracht-stad.txt` A+B), then pin the new runtime hash in `lr_hashes.h`
   (the kit id in the launcher handshake = first 8 hex digits of that hash), rebuild, new zip, two-PC test.
2. **Hero ownership (point 7)**: the launcher can show the host's roster from the transferred slot-3 save and let each
   player tick their heroes, then hand that to the runtime (ini key or small file) — agree the format with Codex first.
   The save files are `persist.*.json` in DD's binary format (see NOTES.md for the roster serializer).
3. **Embark / dungeon (point 6)**: design from NOTES.md Gate 2 (host sends seed + party at `Darkest::RaidStart`
   `0x1403D44B0`, lockstep inputs, per-turn state hash, resync from host). Research kit 9 (probe v9: RaidStart app
   write watch, battle struct diff, activity dump) was built but never run by the owner.
4. Launcher polish worth doing: the "Spellen" check keys on the runtime's last log line; a friends-list "hosting"
   marker while the host's game (262060) runs is unverified (invite/code work); only 1 guest (runtime limit).
5. Hub give-back (only with the owner's OK): the Spacewar playbook should state the ManualDispatch order and the
   bool-return rule; the DD write-up/directory entry is still to do.
6. Cleanup on the server side (owner decides): CT116 (Ghidra/MinGW container, excluded from backup) and the extra
   space on CT107.

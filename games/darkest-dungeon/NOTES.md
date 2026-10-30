# Darkest Dungeon: notes for the next agent

Darkest Dungeon (Red Hook Studios, 2016), Steam, Windows. Work started 2026-10-30.
Goal: host-authoritative co-op where each friend owns heroes in the party.

## Game identity

- Store: Steam (app 262060). Platform tested: Windows. Base game only, no DLC: co-op targets
  base-game combat first. DLC support comes later, tested on legitimately owned DLC.
- Build id: unknown (appmanifest not supplied). Exe link time 2026-09-21 (PE header).
- Version: unknown.
- SHA-256 `_windows/win64/Darkest.exe` (x64): `4d78fbfaa65b7d002d2208d02da809200f4a5723119521180e30100d6f1a75f2`
- SHA-256 `_windows/win32/Darkest.exe` (x86): `4ac994d6ea0550c425ce37e096416fb67e047fce717e5c7369c01c49549966fe`
- The install also ships `_windowsnosteam/` (a different exe hash). Work from `_windows/win64` only.
- Engine: in-house C++ (MSVC, VCRUNTIME140), SDL2 + OpenGL, FMOD Studio audio, Spine animation
  (`SpineDataFactory`), PlayFab SDK + libcurl + OpenSSL. RTTI is present: 7521 `.?AV` class names.
- The `dlc/` folder holds five DLC dirs left over from Steam Family Sharing that are no longer
  licensed. Base game only; leave `dlc/` alone. The Butcher's Circus (`1117860_arena_mp`) is free on Steam.

## Observations (checked, with source)

- No co-op mod found in the coop-anything directory, `games/`, issues or PRs (searched 2026-10-30).
- The only shipped multiplayer is the Butcher's Circus DLC versus mode, which is PvP, not co-op.
  [PlayCo-op](https://playco-opgame.com/darkest-dungeon/)
- The only "co-op" play reported is streamers passing one controller between friends.
  [GamesBeat](https://gamesbeat.com/twitch-broadcaster-turns-single-player-darkest-dungeon-into-a-co-op-game/)

- The exe contains multiplayer netcode from the Butcher's Circus PvP mode (RTTI names):
  `Network::PlayFabAndSteam::PlayFabAndSteamP2PManagement`, `Network::PlayFabAndRUDP::PlayFabAndRUDPManagement`,
  `Network::Steam::P2PSessionProcessing`, `Network::MultiplayerAPI::EMatchmakingState` (PlayFab matchmaking),
  `ReceivedExternalTurnMetaDataInSimpleMPEvent`, `ExternalSubsystemMultiplayerMessageReceivedEvent`,
  `Darkest::EnableMultiplayer`, `Battle::MultiplayerPartyRetreat`.
- `Battle::GetNetworkAISelectSkill` exists beside `Battle::GetMonsterAISelectSkill`.
- Combat code takes the RNG as an explicit parameter: `prng_minstd&` (also `prng_minstd_wrapped`),
  e.g. `Actor::ApplyHealthDamageInternal(prng_minstd&, ...)`, `Actor::QueueHeartAttack(prng_minstd&, bool)`.
- The monster AI is split into desires: `AI::MonsterSkillSelectionDesire`, `AI::MonsterTargetSelectionDesire`,
  `AI::MonsterBonusInitiativeDesire`.
- `scripts/*.darkest` and `*.json` are config data, not a script language.
- x64 exe imports include `WINMM.dll`, `dbghelp.dll`, `WINHTTP.dll` and `IPHLPAPI.DLL` (proxy DLL candidates); there is no `version.dll`.
- PvP reports matches to PlayFab: `PlayFabAndSteamP2PManagement::ReportSuspiciousMatch`.

## Combat turn input (Ghidra 12.2, x64 exe sha256 4d78fbfa...; addresses valid for that build only)

Found by decompiling the vftable xrefs of RTTI lambda classes (script approach: find `vftable`
symbols whose namespace contains the owner name, decompile the functions that write them).

- `0x1407598d0`: the combat state machine. In state `0x1d` the current actor chooses an action.
  - The actor is not a player hero (`actor->vfunc[+0x20]()` returns 0):
    - if raid type == `"arena_mp"` and a PvP session is up → `Battle::GetNetworkAISelectSkill(bool& wait)` = `0x1405a0af0`;
    - else → `Battle::GetMonsterAISelectSkill()` = `0x14059fe80`.
  - The actor is a player hero: reads the skill index the UI selected at `battle+0x9f7*8` (`-1` = nothing
    chosen yet, the turn waits), then validates skill + target (`0x1405a7d50`).
- Both AI functions call the selector `0x140592f80(..., char network, char* wait)`.
  - `network == 0`: monster desires roll a skill ("MonsterBrain" log lines).
  - `network == 1`: no roll. It reads a mailbox byte at `brain+0x118`: `0x80` = empty → `*wait = 1`;
    `-1` = pass; a valid index = use it, then reset the byte to `0x80`; an invalid index = "INVALID SKILL!" + report.
- `0x14035f4a0`: on an invalid skill/target it calls `ReportSuspiciousMatch` on the active PlayFab session
  (Steam P2P or RUDP flavour). It does this only if a PvP session pointer (`DAT_14117d038` / `DAT_14117d030`) is set.
- `Raid::GameControlMessageProcessor` listener for `ReceivedExternalTurnMetaDataInSimpleMPEvent`
  (`0x14060ab30`) only logs "Received message from external subsystem. Checking victory conditions."
  and checks victory. It does not carry the skill choice.

## Derived

- Combat is turn-based and one party moves together, so the turn-based impersonation pattern
  (playbook/engine-families.md, the Doors of Trithius row) fits. The host runs the real game,
  and a guest's command is executed on the turn of a hero that guest owns.

- The playbook tier moves from "closed binary, no MP" to "engine has MP": the combat loop already
  waits for an outside choice (PvP mailbox) and for the UI (`battle+0x9f7`).
- Preferred co-op hook: the player-hero path. For a hero a friend owns, the host DLL keeps the
  local UI from choosing, and writes the friend's skill index (and target) where the UI would.
  No PvP/PlayFab code runs, so `ReportSuspiciousMatch` can never fire. The PvP mailbox
  (`brain+0x118`) is a fallback, but it is only reached for non-hero actors in `arena_mp`.
- With a host-authoritative design + streamed view, the guests need no deterministic simulation.
- Explicit `prng_minstd` passing suggests deterministic, seed-driven combat (PvP needs both sides to agree).
- Fairness boundary (rule 10): never use the PlayFab matchmaking/ranked service or its
  cheat reporting. Reuse in-game turn code over a direct friend-to-friend transport only.

## Guesses (not verified)

- Steam ships a native Linux build of DD1. If so, bot QA could run in a Linux container.
- The PvP netcode may only be armed in arena mode (`EnableMultiplayer`) and may need the free DLC content.
- Community save editors exist for the save format. Find one and link it.

## Unknowns (Gate 1 questions)

- Where the target choice is stored for a player hero (UI field next to `battle+0x9f7`).
- How the UI writes `battle+0x9f7` (to block local input for friend-owned heroes).
- Where the prng_minstd seed comes from (only needed for a later mirrored-client design).
- Whether a winmm.dll proxy loads (needs a Windows test run).

## Log

- 2026-10-30: triage done, no prior work. Owner chose to build a real mod over streaming.
- 2026-10-30: claim issue https://github.com/cranebeaker/coop-anything/issues/267. Building
  against the Steam base game (no DLC) for now.
- 2026-10-30: Gate 1 static pass on the exe strings and PE headers: the PvP netcode and network-AI skill path found.
- 2026-10-30: Ghidra 12.2 headless on the x64 exe (11 min, 12G heap). Combat input path mapped; Gate 1 = go.

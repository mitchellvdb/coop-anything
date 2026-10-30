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

## Controller input (same build)

- The exe imports 61 SDL2 functions from the loose `SDL2.dll` (proxy candidate), including
  `SDL_GameControllerOpen/GetButton/GetAxis/Update`, `SDL_NumJoysticks`, `SDL_PollEvent`.
- Controller events are switched off (`SDL_GameControllerEventState(SDL_IGNORE)`, error string in exe).
  Controllers are polled instead.
- `0x14028a400`: the controller manager. It loops `SDL_NumJoysticks()`, opens every
  `SDL_IsGameController` device into the handle array `0x14117ceb0` (count `DAT_14117d018`), and each
  frame calls `SDL_GameControllerUpdate()` then reads buttons 0-4,6-14 and axes 0-5 of **every** handle,
  merged into one input. It also sets a flag "more than one joystick" (`+0x2405`).
- So several controllers already drive the game at once. Each has its own handle, which lets a proxy
  tell players apart.

## Randomness and determinism (same build)

- PvP is lockstep with a shared seed. Strings: "Written seedHalf: %d", "Received seedHalf: %d %d",
  "Not enough received data to init seed half for match init.", "hasOtherParty && hasRNGSeed", "RNGSeed", "OtherSeed".
- A per-PRNG debug trace exists: "PID: %d Name: %s Random int range A,B: %d , %d Value: %d Seed: %d"
  (also bool/float/normal/uniform, "Reseed seed %d", "Setting random seed: %d (0x%x)"). PRNG instances are named.
  The switch that enables it is unknown. It would make desync hunting easy.
- Gameplay RNG is `prng_minstd` (vftable in exe, default seed 0x17fa1), passed explicitly into combat code.
- `rand()` has one caller, `0x1404e4770`, which looks like a static initializer filling the global
  table names "entries"/"chances"/"chance_sum" (caller 0x1400a4260). Probably not gameplay; unverified.
- `rand_s` is used only for seeding: arg parsing (0x1403cb6d0), PvP match init (0x1403d1a70), roster
  load (0x14054c9d0), unknown (0x140562fb0), and the map generator.
- Map generation `0x1405e65b0`: local prng_minstd seeded with `rand_s() & 0x7fffffff` (0 → 0x17fa1),
  then the generator `0x1405e40f0`. For `arena_mp` the layout comes from the match instead (`0x1405e3ea0`).
  Co-op: the host's seed replaces that one rand_s result on every copy.
- Also imported: `std::_Random_device` (2 callers) and time sources (QPC, timeGetTime, SDL_GetTicks, _time64).
  Not yet checked whether any of them feeds gameplay.

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
- Superseded (owner wants parallel town work, all own the game): own copy per player, town actions
  replicated host-authoritatively, dungeon in lockstep with a host-chosen seed. Parsec + proxy SDL2.dll
  stays as the fallback if determinism fails.
- Earlier MVP idea (owner: town management open to everyone): a proxy `SDL2.dll` wraps
  `SDL_GameControllerGetButton/GetAxis` and returns "not pressed" for a controller that may not act
  now. In town everyone passes; on a hero's combat turn only the owner's controller passes. Friends
  join via Parsec, so each gets their own virtual controller (unverified). No guest app needed.
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
- 2026-10-30: owner wants town open to everyone. Controller manager mapped; design moves to controller-slot ownership through a proxy SDL2.dll.
- 2026-10-30: all three players own DD1 on Steam. Design: own copy each, parallel town work, lockstep dungeon. Randomness sources mapped; map seed hookable at one call.

# Darkest Dungeon: notes for the next agent

Darkest Dungeon (Red Hook Studios, 2016), Steam, Windows. Work started 2026-10-30.
Goal: host-authoritative co-op where each friend owns heroes in the party.

## Game identity

- Store: Steam (app 262060). Platform tested: Windows.
- Build id: unknown (not recorded yet).
- Version: unknown.
- SHA-256 of main exe: unknown.
- Engine: unknown. It is believed to be a custom C++ engine (not verified on the binary).

## Observations (checked, with source)

- No co-op mod found in the coop-anything directory, `games/`, issues or PRs (searched 2026-10-30).
- The only shipped multiplayer is the Butcher's Circus DLC versus mode, which is PvP, not co-op.
  [PlayCo-op](https://playco-opgame.com/darkest-dungeon/)
- The only "co-op" play reported is streamers passing one controller between friends.
  [GamesBeat](https://gamesbeat.com/twitch-broadcaster-turns-single-player-darkest-dungeon-into-a-co-op-game/)

## Derived

- Combat is turn-based and one party moves together, so the turn-based impersonation pattern
  (playbook/engine-families.md, the Doors of Trithius row) fits. The host runs the real game,
  and a guest's command is executed on the turn of a hero that guest owns.

## Guesses (not verified)

- Steam ships a native Linux build of DD1. If so, bot QA could run in a Linux container.
- Butcher's Circus PvP may contain netcode (a lobby, sending combat commands) that could be reused.
- Community save editors exist for the save format. Find one and link it.

## Unknowns (Gate 1 questions)

- Where the combat turn loop, skill select and target select functions are.
- The RNG: its source and seed, and whether combat is deterministic given the same seed.
- Whether a proxy DLL (version.dll / winmm.dll) loads.
- The bitness of the exe.

## Log

- 2026-10-30: triage done, no prior work. Owner chose to build a real mod over streaming.

DDCoop - development test kit {VERSION}

New in test75 / duo58
The new-week Rules state is shared before native raid completion, including
party wipes. Before recruiting, all games compare their Stagecoach candidates.
Each recruit also checks the selected hero's logical content before changing
its local roster. Provisioning and departure wait for this comparison.
Read TESTSTAPPEN-test75.txt and STAGECOACH-WEEK.md.

Campaign mod files are compared when joining a modded campaign. Unknown lists,
missing/different files and unreadable or linked mod folders are refused before
the guest's save is replaced. Matching files do not prove gameplay compatibility
with every mod. No mod is downloaded or enabled automatically. Read MODS.md.

Install
Close every game. Extract this ZIP and run Install DDCoop.bat. It installs into
your real Steam game folder, keeps the genuine SDL2 as SDL2_orig.dll and creates
mods\DDCoop. Repair and Uninstall scripts are included. Do not manually replace
saves or DLLs. Unsupported game versions are refused.

Play
Every PC needs this same kit. Open DDCoop, choose the host campaign slot and
player count, then Host or Join. Guests join before the host campaign loads.
F9 shows or hides the co-op panel; windowed or borderless mode works best.
Each player may use supported Hamlet/shared actions. Combat hero ownership is
assigned in the launcher. The host game and launcher stay running, but guests
do not require a manual host click to provision, embark or make shared choices.

Progress and recovery
The host follows Save progress when hosting. Guests always use -nosave. Only
the selected slot1..9 is borrowed: it is backed up, replaced with the host's
snapshot and restored and verified after the guest game closes. Profile_9 is
the game's restore area and is never borrowed. Keep every launcher open until
recovery finishes. Repair DDCoop.bat is not needed for normal save recovery.
Backups are in DDCoop-backups under your user folder.

Resync
Keep every launcher open. The host can use Resync for a coordinated checkpoint
restart of connected launcher games. This can lose unsaved progress; it is not
live in-memory combat repair. Disconnected players and solo batch guests still
need a fresh Host/Join or manual test-window restart. Do not rejoin an old running
session or copy a different campaign to work around a synchronization stop.

Testing and logs
Follow TESTSTAPPEN-test75.txt. Use Copy logs on every launcher if any check fails.
Copied logs redact known authentication secrets; review them before sharing.

Scope
Two to four players, supported Steam Windows x64 build28385. Shared Hamlet,
provisions/departure, dungeon actions and combat remain development features.
DLC groundwork is included; full native/live DLC validation, shard purchases,
direct loot-to-hero trinket equipping and complete game-state verification remain
open. The default-order button/shortcut is shared; arbitrary manual dungeon
formation dragging is not newly synchronized by this kit.

Files (SHA-256)
{HASHES}

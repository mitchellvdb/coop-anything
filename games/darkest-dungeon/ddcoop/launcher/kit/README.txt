DDCoop - development test kit {VERSION}

New in test80
Everything from test79, plus: each player closes their own tutorial popups
(a guest could get stuck on one the host did not have), and clicking an open
curio again no longer puts the guests' dice out of step (that ended a session
with a loot window that only the host had).

New in test79
Tavern and Abbey treatments, removals and skill choices are now saved by the
host's game right away, as the game itself does. Until test78 a restart could
lose a treatment while the gold stayed spent. Guest skill clicks that do not
reach the host now leave a line in the logs. Every PC needs test79 (a test77
or test78 friend is offered it by the host).

New in test78
After a raid the games compare their currencies per kind and write each amount
to the logs; the order of currencies inside the game no longer counts as a
difference. Every PC needs test78 (a test77 friend is offered it by the host).

New in test77
Updates from the host: when a friend with an older DDCoop joins, the host's
launcher sends its kit along and the friend's launcher offers to install it.
Only kits signed by the DDCoop owner are accepted. Install test77 by hand once;
later kits then arrive this way. Read UPDATE.md. Gameplay is unchanged from
test76 (same runtime), so test76 and test77 players can play together.

New in test76 / duo59
After a raid every game now gives the new Stagecoach heroes the same numbers:
the host's id counter is shared together with the new-week Rules state. A
shared camp Leave waits until the camp is not covered by another window on
that PC, instead of leaving only some games in camp.
From test75: Stagecoach candidates are compared before recruiting, and each
recruit checks the hero's content. Read TESTSTAPPEN-test76.txt and
STAGECOACH-WEEK.md.

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

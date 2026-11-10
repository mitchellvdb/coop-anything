DDCoop - development test kit {VERSION}

New in test73 / duo56
--------------------
Hand/tool/X/item choices on curios and obstacles now resolve as one shared
choice for every player. This includes items used from the bag by click or drag.
A competing choice cannot run another result or consume an extra item. The first
accepted valid choice wins. Everyone needs this new kit. Read PROP-CHOICES.md and TESTSTAPPEN-test73.txt.

New in test72 / duo55
--------------------
Post-combat hero selection waits for every receiving copy to finish its native
combat transition. Persistent hero identity and native selection are verified
before success. Tavern/Abbey and provisions from71 are retained and owner-tested.
All peers need this kit; do not mix with71 despite unchanged wire protocol32.
Read SELECTION-TRANSITION.md and TESTSTAPPEN-test72.txt.

New in test71 / duo54
--------------------
Tavern/Abbey provisional hero placement is shared BEFORE Accept. The first
choice reserves the slot; every player can accept or clear it. Other screens
hydrate the same hero when opened. Provisions/embark may start from any guest;
the host needs no manual click. Keep its game and launcher running.
All peers need this kit (runtime protocol32). Read ACTIVITY.md for scope/tests.


New in test70 / duo53
--------------------
Independent fault simulations found and repaired lost STOP messages under full
queues, unbounded network sends, unsafe camping indices and town mismatch ACKs.
Campaign metadata and nonempty required DLC data are checked before replacement.
A valid absent DLC-list is treated as an empty list; corrupted lists are refused.
Read RECHECK-2026-11-10.md and DLC.md for evidence and remaining live-test limits.

Campaign-DLC groundwork now covers nine features, including Fire's Edge and its
two heroes. It is not complete live DLC support. Native shard supplies still
refuse pending measured currency/price/refund binding; no prices are invented.

======================================

New in test69 / duo52
- Corrected a native instruction boundary in the tutorial-popup hook; every live
  and early hook now has a complete read-only instruction check.
- Missing shared item/loot/results effects and combat RNG mismatches stop for
  recovery instead of silently continuing with different state.
- Independent movement votes for three guests; idle windows do not erase held
  keys. Exact runtime version is checked. Remaining F9 labels are English.
- Copy logs removes authentication ticket/token/password lines from exports.
- Read the included review for remaining limits; this is not a claim that every
  real-game or DLC scenario is fully validated.

New in test68 / duo51
- Building treatment no longer blocks composing a new party when several old
  members are still remembered. Available replacements work from any player.
- Combat hero ownership does not restrict Hamlet party composition. Closed
  expedition screens receive the shared model and show it when opened.
- Direct equipping from loot is a separate report, not claimed fixed here.

New in test67 / duo50
- Curio X closes the matching panel for everyone, from any player. Native close
  animations are retained; cancelling does not trigger the curio result.
- A skill choice can reach a copy still selecting a skill, fixing the circular
  input wait seen during the Collector fight. Target/turn/actor guards remain.
- Collector gameplay needs a repeat test; see the included test steps.

New in test66 / duo49
- Inventory repair handles repeated stacks without moving an earlier repaired one.
- Host Resync automatically closes connected launcher games, restores guest saves,
  reconnects, transfers the fresh host checkpoint and opens the games again.
- Player numbers and hero ownership remain the same, even if rejoin order changes.
- Keep ALL launchers open. Native save rules decide the checkpoint; unsaved
  progress may be lost. A disconnected launcher and solo .bat guests need manual
  restart. This is not an in-memory combat repair.
- Limited fault traces help investigate the earlier provisions crash; that crash
  has not been established as fixed.

New in test65 / duo48
- Every player follows provisions through the same normal game opener. Initial
  free-item order is normalized, with inventory/currency checks retained.
- Select a hero row, choose Player 1..4 and Assign in the host launcher.
- Players and Restore controls no longer overlap; logs include all guest windows.
- Host Resync... closes connected launcher games normally for checkpoint recovery.
  Wait for guest recovery, then Host/Join again. Unsaved progress may be lost.

New in test64 / duo47
- A trinket that lands in a different bag slot for a friend no longer disappears
  when it is put on a hero.

New in test63 / duo46
- Missions with a fixed map no longer stop the session at embark.

New in test62 / duo45
- The host chooses the number of players (2, 3 or 4) before Host. The games load
  only when that many friends are there; a friend who comes later is told so.

New in test61 / duo44
- After a shared curio or loot the host waits to walk on until every friend has
  closed the window, so the corridor stays the same for everyone.

New in test60 / duo43
- Up to four players: one host and three friends. All games load together once
  everyone is at the save screen; each hero has one of the four players as owner.

New in test59 / duo42
- Prop opening and obstacle confirmation are separate shared actions. Duplicate
  opens cannot confirm a panel or start a second active event.
- Both players may clear an obstacle without a shovel. Confirmations wait for
  the matching panel, then native closing permits food and walking afterwards.
- Loot trinket drags also resolve the exact selected item in the raid inventory.
- English UI, animated mascot and fresh subsequent expedition fix remain.
- Automated tests pass; these repairs still need a live two-PC test.

Install on BOTH PCs
1. Close both games. Let DDCoop finish restoring the guest's own campaign, then
   close the launcher. Extract this entire ZIP.
2. Open DDCoop-install and double-click Install DDCoop.bat. It finds the supported
   Steam installation and installs into mods\DDCoop. No manual DLL copying.
3. Open DDCoop. Host: choose the campaign slot and click Host a game.
   Guest: select the hosting friend and click Join friend, or use the host code.
4. Let both games load the selected slot automatically. Wait for the connection
   status before shared actions. F9 opens DDCoop during gameplay.

Progress and recovery
The host follows Save progress when hosting. The guest always uses -nosave.
Only the selected campaign slot is borrowed; it is backed up, replaced with the
host snapshot and restored automatically after the guest game closes.
Close both games after testing and let recovery finish. Repair DDCoop.bat is not
needed for normal save recovery. Backups are in DDCoop-backups under your user folder.

If a player disconnects
This version cannot resume a running session. ALL players close the game. The
host clicks Host again to create a new session; the guest then joins that session.
Rejoining the old running host is intentionally refused to prevent loading an
outdated snapshot. Do not delete saves or manually copy a different campaign.

Test plan
See TESTSTAPPEN-test66.txt. Test two stacks of one kind, inventory rearrangement,
room travel and automatic host Resync with all four launchers still connected.
Everyone should reopen automatically from the same host checkpoint without a
manual Host/Join. Compare room, party, inventory and owners. Send every log bundle.

Scope and limits
Two to four players; supported Steam Windows x64 build28385. No live combat repair; Resync performs an automatic coordinated checkpoint restart.
Use the same kit on both PCs. Arbitrary manual dungeon formation drags are not
newly synchronized; the default-order button and shortcut are synchronized.
DLC groundwork remains, but full Color of Madness/shards/waves support and DLC
hero gameplay still need validation. Steam game assets are not included.

Files (SHA-256)
{HASHES}

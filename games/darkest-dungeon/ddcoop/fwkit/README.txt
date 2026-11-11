Darkest Dungeon co-op - framework test 2 (co-op campaign check)
===============================================================

What this is
  Framework 0.2 knows which campaigns are co-op campaigns: in ddcoop.ini you list them by their position
  in the game's campaign list (coop_slots=3). While you play, it reads which campaign is loaded and writes
  in ddcoop-fw.log whether co-op is allowed for it. It changes nothing in the game.
  start-coop.ps1 first zips a backup of ALL your Darkest Dungeon saves, then starts the test copy with
  -nosave (the game saves nothing in this test).

Steps
  1. Unzip this kit, open PowerShell in that folder and run:
         powershell -ExecutionPolicy Bypass -File .\install-framework.ps1
  2. With Steam running:
         powershell -ExecutionPolicy Bypass -File .\start-coop.ps1
     It prints where the save backup went (%USERPROFILE%\DD-coop-test\backups).
  3. Load the campaign in slot 3 (cooptest). Wait ~5 seconds in the Hamlet. Quit to the main menu.
  4. Optional: load your own campaign in slot 1, wait ~5 seconds, quit to the main menu. This only reads
     it: -nosave keeps the game from writing, and step 2 made a backup first. Skip it if you prefer.
  5. Quit the game. Put ddcoop-fw.log (in %USERPROFILE%\DD-coop-test\DarkestDungeon\_windows\win64)
     in <project dir> on the development machine.

Expected log lines
    co-op campaigns: menu slot(s) 3
    save slot: campaign in menu slot 3 ("steam_profile_2"): co-op allowed
    save slot: no campaign loaded ...
    save slot: campaign in menu slot 1 ("steam_profile_0"): single player      (only with step 4)

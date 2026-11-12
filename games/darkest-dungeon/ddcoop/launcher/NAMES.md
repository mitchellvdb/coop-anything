# Player names (launcher <-> launcher <-> runtime)

Each player can type a name in the launcher ("Your name:"). The others then see that name instead of
"player 2", in the launcher and in the game's party blocks. The launcher also shows how many players are in
the session ("2/4 players connected: ...").

## Rules for a name

- UTF-8, at most 31 bytes (`LR_PLAYER_NAME_MAX`); the edit box takes 24 characters.
- No control characters (C0, DEL, C1), no leading or trailing space. `lr_player_name_clean` makes any text
  valid (cuts on a character boundary); `lr_player_name_ok` is the strict check every decoder uses.
- Empty = no name: the others see "host" / "player N" (launcher) or "Host" / "Player N" (game).
- Stored only in `mods\DDCoop\ddcoop-name.txt` on the player's own PC. **Never written to a log** (logs get
  shared): the launcher logs only "set" / "not set" and counts.
- Display only. A name is never used in a path, a save, `ddcoop.ini` or `ddcoop-heroes.ini`.

## Launcher messages (control channel, wire version unchanged)

| Type | Direction | Body |
|---|---|---|
| `LR_NAME` = 32 | guest -> host | name length (1 byte, 0..31), name |
| `LR_PLAYERS` = 33 | host -> guest | revision (u32, not 0), then `lr_players`: total players (1 byte, 2..4) and per player 0..3: present (0/1), name length, name |

- The guest sends `LR_NAME` after the host's `LR_SESSION` and again after every change. The host takes it for
  that guest's player number only.
- The host is the authority for the list. It sends a new revision to every friend past `LR_SESSION` when
  someone joins or leaves or a name changes. Players at or above the chosen total are never present.
- Both types are sent only inside a session with the same kit, never during a kit offer, so older launchers
  never see them.

## Runtime overlay (local snapshot, `src/dd_overlay_wire.h` version 2)

- `players[4][32]`: the name of player 0 (host) .. 3, "" = none. `owner` per hero grew to 32 bytes (the F9
  Heroes tab shows the name).
- The runtime calls `dd_overlay_party_players(player[4])` (0..3, -1 unknown) before `dd_overlay_party`. Each
  party block shows that player's name from the snapshot, else "Host" / "Player N"; unknown owner = no text.
- Colours stay as before: green = mine, orange = another player's, grey = unknown. The blocks are larger
  (window width / 16, height 2/5 of that, stress bar the bottom quarter) to fit the name.
- Without a launcher (duo .bat windows) the snapshot is not fresh, so the blocks show "Host" / "Player N".

Tests: `test_lr_wire` (names, both messages, fuzz), `test_lr_core` 4a (exchange, cleaning, count), the UI
preview self-check (players line, assign list, overlay snapshot) and `tests/test_overlay.c` (actual GL text
in the blocks, state restored; it saves `<bmp>.party.bmp` to look at).

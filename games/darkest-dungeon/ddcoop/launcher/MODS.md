# Campaign mods: the same mods on every PC

Darkest Dungeon mods are chosen per campaign. The campaign's `persist.game.json` lists them under
`base_root.persistent_ugcs` (UGC = user-generated content). A guest receives the host's save, so its game
loads the same list. But when a mod is missing on one PC, or has another version (the Steam Workshop updates
mods by itself), that game simulates differently and the copies drift apart **without any error**. The
launcher therefore checks every mod before a guest's save is touched. Nothing here changes the game, the
runtime or any mod.

## What happens

1. **Host clicks Host.** The launcher reads the mod list from the frozen campaign snapshot
   (`lr_game_parse`). For each mod it finds the folder on this PC and makes a fingerprint (`lr_mods_print`).
   A mod the host does not have, or a list that cannot be read completely, stops hosting with a message.
2. **A friend joins.** After `SESSION` the host sends `LR_MODS` (wire type 22): the number of mods and per mod
   its name, source, title, file count, byte count and fingerprint. Always sent, also for zero mods.
3. **The friend's launcher**, once the campaign has arrived and the DLC check passed, fingerprints the same
   mods on its own PC (`lr_mods_match`). If all match, the campaign is stored as before. Otherwise it stops
   with a message that names each mod as missing, other version or not found by the host, sends
   `SAVE_RESULT` = `LR_BYE_MODS` (12) to the host, and its own save stays untouched.

A campaign without mods behaves exactly as before.

## Where mods are found

`game_dir` is `...\steamapps\common\DarkestDungeon`.

| Kind | Folder |
|---|---|
| Workshop item (numeric id) | `<game_dir>\..\..\workshop\content\262060\<id>` |
| Local mod | `<game_dir>\mods\<folder>` |
| A Workshop item copied into `mods\` by hand | the `mods\` subfolder whose `project.xml` has `<PublishedFileId>id</PublishedFileId>` |

A numeric name tries the Workshop first, another name `mods\` first; a `source` containing "steam"/"workshop"
or "local"/"mods" overrides that order. `mods\DDCoop` (this launcher's own folder) is never a mod. Names must
be plain folder names (printable ASCII, no path or Windows special characters, no `..`); anything else makes
the list unreadable and the session refuses.

## The fingerprint

SHA-256 over every file in the mod folder, sorted by relative path ('/' separated, byte order); for each file
its path, its size (8 bytes) and its own SHA-256. Links and junctions (including the mod root) refuse the check instead of being
omitted. Unreadable/incomplete enumeration and overlong paths also refuse; at most
50,000 files and 12 folder levels per mod. Any changed byte, renamed, added or removed file changes the
fingerprint, including `project.xml` and preview images: the same Workshop version gives the same files.
The title (`<Title>` of `project.xml`, XML entities decoded) is only for messages.

Reading speed on the owner's PC (read-only check, 11 Nov 2026): 15 subscribed Workshop items, 248 MB,
fingerprinted twice in 5.8 s the first time and in 1.5 s from the disk cache; repeated fingerprints were
identical. The window does not respond while the launcher reads, so a very large mod set delays Host/Join
by a few seconds.

## Not known yet (check with the first modded campaign)

- **The exact layout of `persistent_ugcs` entries.** All campaigns seen so far have an empty list. The parser
  accepts an object per mod with a `name` text and an optional `source` text, directly in the list or one
  group level deeper (the layout the save uses for `presented_dlc`). Scalar list entries, duplicate list/name/source keys, and anything deeper or different are refused
  rather than guessed. The first real modded campaign must be read (`tools/dson_dump.py`, read-only) to
  confirm, and the tests adjusted to it.
- **What the game does when a listed mod is missing.** The launcher refuses before that can happen.

## Not covered by this check

- A Workshop update during a session (the check runs when a friend joins).
- Mods that change what DDCoop's runtime assumes: extra Hamlet buildings or activity slots (the runtime has
  a fixed set), new currencies or supply types (supplies are gold-only), mods that ship their own
  `SDL2.dll` or loader (they clash with the DDCoop bootstrap). These need runtime work and testing; the
  check only guarantees that every PC has the same files.
- An older guest launcher ignores `LR_MODS` (the decoder refuses an unknown type) and checks nothing; an
  older host sends none, and a new guest then refuses a modded campaign. Players should run the same kit.

## Code and tests

`src/lr_mods.c` (finding, fingerprint, SHA-256, `LR_MODS` body, comparison), `lr_game_parse` in
`src/lr_roster.c` (the list), `src/lr_core.c` (`host_mods`, `SP_MODS`, `campaign_mods_ok`), `main.c`
(`mod_print_cb` with the game folder). Tests: `tests/test_lr_mods.c` (published SHA-256 vectors, a fake Steam
library on disk, every truncation and 200,000 mutations of the body and of saves) and the end-to-end cases in
`tests/test_lr_core.c` (identical, missing, other version, host without the mod, host that cannot confirm,
older host launcher, malformed list).

## Test75 review corrections (Codex, 11 Nov 2026)

The initial implementation could report an unknown nonempty scalar/list shape as
zero mods. It also omitted directory links and could treat a failed directory
listing as an empty folder. That allowed equal fingerprints for two different
linked hero definitions. The parser and tree reader now refuse those cases.
Ordinary empty/absent mod lists and the two supported object layouts still work.

The same new regression fixture fails fourteen assertions against the initial
implementation and passes after correction: direct/grouped unknown values,
unidentified entries (including empty objects alone or beside a known mod), duplicate list/name/source fields, unequal junction targets,
a linked root, a sharing-locked directory and an unreadable file. Windows tests
create junctions only in disposable test folders and detach them afterward;
production files/mods are never changed. Existing SHA-256 vectors, exact body
round trips, 200,000 body mutations and 200,000 save mutations are retained.

This is a compatibility preflight, not an atomic snapshot of a changing Workshop
tree. A real modded campaign still needs to establish its exact save layout and
native folder-resolution behavior before broad compatibility is claimed.

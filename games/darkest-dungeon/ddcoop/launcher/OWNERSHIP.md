# Test73 prop-choice contract

Runtime protocol33/exact hello73; launcher wire3 unchanged. USE now carries a
monotone resolution/scene revision and persistent actor ID. Hand/tool/X/direct
curio drop/click all use the ordered apply path for host and guests alike.
Native NULL actor is preserved by use.flag for terminal choices. Stale choices
and OPEN after a resolved panel refuse before native/selection/Rules effects.
Automatic obstacle panels need no explicit shared OPEN. No owner-file/mapping
change. See runtime/PROP-CHOICES.md. Existing session/recovery invariants remain.

# Test72 selection transition addendum

Runtime byte protocol32/launcher3 remain. Exact runtime hello72 is mandatory:
SELECT now uses existing item.id for the selected persistent hero ID, requires
outside-combat shape/rank0..3, and waits at the expected FIFO head up to30s for
a readable non-battle view in the same area. It checks bounded party membership
and actual native selection before success. No launcher mapping/ownership-file
change. All4 players may initiate; wrong identity/area never silently ACKs.
The owner confirmed the71 Tavern drafts and provisions in a four-window test.
See runtime/SELECTION-TRANSITION.md for the new post-combat timing repair.

# Current contract — test71 / runtime32

All participants use the new matching runtime. Shared activity drafts/accept/X
use persistent hero ID, exact slot revision and town epoch. Any connected player
may start them, provisions or embark; the host does not approve them manually.
A host game/launcher must remain running to order shared actions/save truth.
No file/mapping/lobby schema changes. PREPARE clears drafts atomically; confirmed
treatments remain. See runtime/ACTIVITY.md for the six-activity scope and tests.

# Current contract — test70

All participants use runtime test70 (normal or duo variant), protocol31 and
launcher wire3. A transport STOP is delivered outside the action FIFO and the
stream closes after the attempt; full queues and bounded-send failures require
session recovery. No shared action may continue after a known town mismatch.
Camping commands validate actual actor/selected skill before native execution.
Metadata/DLC checks precede guest backup or replacement. All supported shared
choices retain the same player permissions; combat owners still use hero IDs.
The DLC data report comparison remains a separate read-only tool, not a new
launcher/runtime wire contract. Shard model APIs do not enable a native shop.

# Current contract — test69 audit

Shared gameplay failures no longer authorize a private guest continuation or a
silent apply ACK. Runtime31 now also checks the exact runtime hello version.
Launcher wire3, file/overlay/party IPC formats, guest-nosave and host save
preference are retained. Exported logs redact known ticket/token/password lines;
original logs stay unchanged. All selected players need the same69 kit.

The amendments below are historical, newest first. A newer amendment supersedes
older two-player wording or behavior. runtime/REVIEW-2026-11-10.md lists current
findings, remaining limits and evidence. Windows default69; lr_logs.c is included
in every launcher build recipe and synthetic tests on Windows/Linux.

# Test68 party editing amendment (Codex)

Every connected player may compose the expedition party, including heroes whose
combat owner is another player. Host transport orders actions; this grants no
extra gameplay restriction to guests. Available heroes only. Old remembered IDs
now in buildings are pruned from a new destination without ending treatment.
Runtime31, launcher wire/lobby3 and mapping contracts remain unchanged. All PCs
need the same68 runtime kit. Windows build default68; host save preference and
guest-nosave unchanged. See runtime/PARTY-RETURN.md.

# Test67 runtime compatibility amendment (Codex)

Launcher wire/lobby3 and the live-party mapping contract remain unchanged.
Runtime protocol31 now carries USE door6 (option0 curio X, option1 obstacle X).
Every player must install67; the separately pinned runtime kit ID rejects older
builds. The launcher has no new gameplay authority or automatic restart change.
The Windows build default is test67. Guest always-nosave and host save preference
remain. See runtime/INPUT-PHASE.md for the skill-selection and cancel boundaries.

# Automatic checkpoint restart amendment (test66)

Explicit host Resync keeps the Steam lobby/control connection. LR_RESTART binds
BEGIN/READY/GO/JOINED/ABORT to a nonzero generation and exact runtime kit. Only the
current host and captured participant IDs are accepted. Every launcher pauses its
old relay, closes its tracked game normally and finishes existing guest recovery.
READY requires no game/helper/pending recovery. All READY plus host game exit is
required before capturing a fresh host snapshot, reserving each original player
slot and reopening through existing verified preparation. GO makes guests join
automatically; the usual checked transfer/backup/prepare/autoload route resumes
the host checkpoint. Duplicates/retries cannot close/relaunch twice. Timeouts or
recovery/start failures stop automatic work, retaining backups and normal close.
No live-memory reset or forced save/game termination. Exact unsaved progress is
not guaranteed. Solo guest .bat windows have no launcher; recovery stays manual.
Launcher wire version3; runtime protocol30 is unchanged.

# Coordinated checkpoint resync (test65)

Host Resync requests LR_BYE_RESYNC through the existing authenticated launcher
control channel. Each guest acknowledges, closes its game through WM_CLOSE and
uses existing journaled backup recovery after game exit. The host retries the
request until acknowledgements/peer departures, then closes normally too. No
process termination, live-memory rewrite, save while running or desync flag reset.
After recovery the host clicks Host and everyone joins the fresh checkpoint.
Unsaved progress may be lost; the button explains this before proceeding.

# Direct assignment amendment (test65, owner request 9 Nov 2026)

The host can select a hero row and assign Player 1 (host), 2, 3 or 4 directly.
The launcher validates the persistent hero ID, active host session and configured
player count. Guests retain their existing self-claim route; they cannot invoke
this host-only API. A changed owner increments the existing revision, persists
written_by=host and broadcasts the whole table to every connected guest through
LR_ROSTER. No wire/file layout change. Mid-dungeon changes still take effect at
the next supported choice boundary. Selection survives live-party row sorting.

# Live party display amendment, owner feedback 8 Nov 2026 (test55)

Party highlighting is a view of live membership, separate from who owns each hero.
Do not infer dungeon membership from persist.roster status or the ownership mask.
The runtime publishes only mode/count/persistent hero IDs in a56-byte local
`Local\DDCoopParty-<game PID>` mapping (src/dd_party_wire.h v1), with its own even
sequence and monotonic timestamp. No names, pointers, credentials or save data.
The launcher maps its own tracked game's report read-only; PID, size/version,
freshness<=3s, unique IDs/count<=4 and existing roster membership must all agree.
Stale/unknown/torn data removes markers. Never fall back to save-status guesses.
The actual party appears first in live order in both launcher and F9; every click
still uses the existing persistent-ID claim path, with no ownership/save changes.
The launcher-to-game F9 mapping and runtime network protocol25 are unchanged.

# Restored-raid ownership amendment, owner feedback 8 Nov 2026 (test49)

Loading an existing dungeon skips departure_send/apply. At the first normal input
boundary the host reads the live party's persistent IDs, sorts those IDs once,
and publishes the host file's table through the existing ordered DD_ACT_OWNERS.
Guests never read their own file for authority. The first table must match their
live party membership, independently of formation order. The slot and party gates
defer the initial table until loading finishes; no native action runs while inactive.
Until that table neither copy may choose a combat action; the party blocks are gray.
Missing/invalid host files use host defaults initially; later transient failures
keep the last agreed table. Normal embark and live F9 changes remain as before.
Wire format and runtime protocol22 are unchanged. The small blocks read current
formation each frame, front rank on the right: green = local owner, orange = other.
The large strip shows the active turn. Skill/enemy movement never changes ownership.

# Live ownership amendment, owner request 6 Nov 2026 (test34)

The owner now wants changes made in F9 during a dungeon to take effect during that dungeon.
This replaces the earlier frozen-per-expedition rule below. The file format and launcher are unchanged.
The host reads only the bounded, valid `written_by=host` table, at most twice per second.
At a normal input boundary (exploring or choosing a combat skill/target), it publishes DD_ACT_OWNERS
through the existing authenticated ordered action stream. An attack already running finishes first.
The message contains the original party's persistent IDs, owner mask and revision; guests apply only
that host message and never read their own file for authority. Missing/malformed/transient files keep
the last agreed live table; guest assignment requests sent to the runtime are refused.
No native hero stats, turn order or RNG is changed by changing ownership. Test34's native fixtures
cover a mid-fight update and forged/invalid inputs; actual F9-to-game behavior awaits the owner run.

The earlier contract is retained below as history; its statements freezing owners until embark
are superseded by this amendment. Mid-raid rejoining/host fallback is still not implemented.

# Hero ownership: launcher design and the file the runtime reads

Status 2026-11-01: **launcher side built and tested** (`lr_roster`, `lr_owners`, `lr_core`, hero list in the
window); not yet used by the runtime. **Codex agreed to the format and the reading rules below** (answers at the
end; Codex's full text is `runtime/HERO-OWNERSHIP-RESPONSE.md` on branch `codex`). No launcher change was needed.

## Goal (point 7 of the two-PC test)

Owner UI addition, 2026-11-03: after the existing Host/Join/save-prepare/start sequence, offer the co-op
controls as a compact F9 show/hide panel over the running game, following the supplied visual reference.
Keep the launcher as the background authority for connection, save preparation, backups and verified recovery.
The owner explicitly withdrew automatic game launch on opening DDCoop: guests must receive the host's save
before the game starts. Preserve stable-ID ownership semantics and host authority when moving the controls.
Overlay input must not also activate game controls. The screenshot's monster panel is not part of the request.

Test12 implementation: `src/dd_overlay_wire.h` defines a bounded, versioned local snapshot (no passwords,
Steam IDs or save data). The launcher publishes it in `Local\DDCoopOverlay-<game PID>` with a sequence counter.
The runtime maps it read-only, rejects stale/torn snapshots, and draws a compact OpenGL panel before SDL swaps.
F9 belongs to SDL while the game runs; the launcher unregisters its old global F9 hotkey until game exit.
Panel mouse/keyboard input is consumed before game input; outside clicks pass through. Hero clicks carry the
stable hero ID through a registered local window message to the existing `lr_core_claim` handler. Logs, copy
code and open-launcher buttons use the same path. Host/Join/invites and save preparation stay prelaunch in the
launcher; game exit/recovery still use the existing coordinator. Party highlighting follows the host's saved
roster; it is not an unsaved live-party memory feed. In-dungeon ownership enforcement is still future work.

Each player commands their own heroes in the dungeon. Ownership is chosen in the DDCoop launcher, outside
the game: it already has the host's slot-3 save, and the choice can change between expeditions without
touching the game.

## Rules

- Players: index 0 = host, 1 = guest (the runtime takes one guest now; the index leaves room for more).
- Every hero has exactly one owner. Default owner = host; that includes heroes recruited after the last
  write, which simply are not in the file yet.
- The **host's launcher is the authority**. The guest's launcher asks (tick / untick); the host's launcher
  applies it and sends the whole table back. Ticking a hero makes it yours; unticking a hero of yours gives it
  to the other player. Unticking someone else's hero is refused (the guest's list goes back to the table).
- Ownership is frozen per expedition: the runtime takes it at embark, later changes count from the next embark.
- The host's launcher builds the table from `persist.roster.json` in the campaign snapshot it takes when hosting
  starts, re-reads the file every 5 s while hosting (the game rewrites it after recruit, dismiss, return to town),
  and restores its choices from its own last `ddcoop-heroes.ini` (only if `written_by=host`, so a file this PC
  got as someone else's guest is never used).

## The file

`<game>\mods\DDCoop\ddcoop-heroes.ini`, written by the launcher on both PCs (temp file + rename, so a reader
never sees half a file). The guest's copy is the table the host sent. Kept after the session.

    ; DDCoop hero ownership. Written by DDCoop.exe, read by the runtime at embark.
    ; hero.<roster id>=<player>: 0 = host, 1 = guest. A hero that is not listed belongs to the host.
    version=1
    written_by=host
    revision=7
    ; Reynauld, crusader
    hero.1=0
    ; Dismas, highwayman
    hero.2=1

- `revision` increases on every change (lets the runtime log which table it used).
- `hero.<roster id>=<player index>`. The roster id is the key under `heroes` in `persist.roster.json`,
  the same id the game uses (`hero+0x130c`, `RaidStartParameters.heroes[i]->id`; Reynauld = 1).
- Lines starting with `;` are comments; unknown keys are ignored (room to add things later); a hero that is not
  listed belongs to the host. Line ends are CRLF.

Why a separate file and not `ddcoop.ini`: `ddcoop.ini` is written once at game start, and the host's game is
already running before the guest joins. The ownership file changes during the session.

## Who reads it, when (suggestion)

- The **host runtime** reads the file at embark (`Darkest::RaidStart` `0x1403D44B0`) and puts the owner of each
  party member into the departure message, next to the hero ids: `{quest, heroIds[4], owners[4], provisions,
  seed}` (NOTES.md, Gate 2).
- The **guest runtime** uses only the owners from the host's departure message, never its own copy. Then a late
  or missing file on the guest cannot cause a desync. (It may read its copy for display only.)
- A party member whose owner is not connected is commanded by the host.

## Launcher side (built)

Next-kit UI agreement (owner request, 2 Nov): use the existing host roster `status == 0` as current-party
membership, already shared in LR_ROSTER. Put these (at most four) above the rest with a gold background
and a visible dot/legend; retain stable ID order within each group. Keep each checkbox tied to its hero
ID, independent of display row. A malformed table with more than four party members is not guessed.
Refresh follows the host's saved roster, polled every 5 seconds. No change to ownership INI or wire format.
The left checklist becomes a read-only multiline edit with word wrap, keyboard/mouse vertical scrolling
and retained scroll position across status updates. These display changes never change hero ownership.

- `lr_roster`: reads id, name, class, resolve XP and status of every hero from the binary save.
- `lr_owners`: the table, the rules above, the ini text, restore from ini, and the `LR_ROSTER` body codec.
- Launcher messages: `LR_ROSTER` (host -> guest: revision + per hero id, owner, status, xp, name, class) and
  `LR_CLAIM` (guest -> host: hero id, tick or untick). `LR_VERSION` stays 1: an older launcher drops the two new
  types as malformed, which is harmless; the kit id in HELLO already makes both players run the same kit.
- `lr_core`: host sends every new revision (again after a refused tick); the guest queues ticks while Steam's
  send buffer is full; both write the file.
- Window: right-hand column "Wie bestuurt welke held?" with a tick box per hero, name, class, level and
  who commands it ("jij", "host", "vriend").
- Tests: `test_lr_owners`, `test_lr_roster`, `test_lr_wire`, `test_lr_core` (scenarios 4b-4f, 14).
  Checked on the owner's real slot-3 roster (8 heroes). The window was looked at with that roster loaded;
  clicking the tick boxes in the real window has not been tried yet, nor a session between two PCs.

## Questions for Codex, and Codex's answers (2026-11-01)

1. *Is `ddcoop-heroes.ini` in this format fine?* Yes: this place, this format, stable roster ids, unlisted = host,
   temp file + rename, CRLF. The runtime parses only the documented keys, ignores comments and unknown keys, and
   rejects malformed numbers or player indices outside 0..1.
2. *When does the host runtime read it?* Once per embark, just before it builds the departure data; that table
   holds for the whole expedition. Missing file = all heroes belong to the host. A file that exists but cannot be
   parsed: log it and do not start a co-op expedition (fail closed). The revision goes into the departure message
   for logs only.
3. *Does the guest use only the host's departure message?* Yes; the guest's own file is for the launcher window
   only. The guest checks the hero ids and owners in the departure and refuses the co-op embark when they are
   wrong. A hero whose owner is disconnected is commanded by the host.
4. *Anything else per hero?* No: party hero id + owner index (+ revision for logs). Name, class, XP and status
   are launcher display data. Checking dungeon input against the table is runtime work.

Agreed departure fields: the existing ones + per party hero its stable id and owner index + ownership revision.

Launcher notes for the runtime:

- The launcher already writes with temp file + rename (`lr_write_file`: `<path>.ddcoop-tmp`, then replace).
- `written_by`: the same PC can be host one evening and guest the next. When hosting starts, the launcher always
  rewrites the file with `written_by=host` (only if the save's roster cannot be read, it leaves the old file
  alone). Suggestion: the host runtime treats a file with `written_by` other than `host` as "not for me" (same
  as a missing file, or fail closed, Codex's choice), so a leftover guest copy is never used as the host table.

Restart relay amendment: every channel0 OPEN/DATA/CLOSE carries its restart
generation. Both new relays set this epoch before starting new game connections;
stale frames are ignored even if a stream ID is reused. Epoch changes are refused
with active streams. Control and data channels can progress independently, so a
control READY alone is not treated as proof that old data packets have drained.
The isolated relay regression covers delayed old OPEN/DATA/CLOSE against a new
same-ID stream. Launcher wire3 includes the tag; runtime protocol30 unchanged.

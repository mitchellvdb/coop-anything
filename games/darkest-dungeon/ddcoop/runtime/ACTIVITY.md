# Shared Tavern and Abbey drafts — test71 / duo54

All four players may place, accept and remove stress-building heroes, open
provisions and embark. Host authority orders actions; no manual host click or
open host building panel is required. Host must keep its game/launcher running.

## Owner report and verified native issue

The four-window session committed two activity assignments, while pending drops
were local. One guest then recorded first-chance C0000005 at game+7e20ac; the
other copies stopped when that connection closed. The exact supported EXE shows
an unchecked dereference of the widget's hero resolver there. Model occupancy
and UI Hero* were checked independently, so a filled native slot plus stale/NULL
picture could enter this unsafe path. The logs do not include register values.

The existing hook saw only Accept/payment. New ordered activity commands cover
RESERVE, CLEAR, ACCEPT and confirmed RELEASE, with exact hero/per-slot revision
and town epoch. A first accepted choice owns the slot; a competing/stale choice
cannot replace it. Every player has equal permissions. One hero cannot occupy
two reserved/committed slots. A draft costs nothing; Accept uses native payment
once. Confirmed removal uses the proved native setter and preserves its actual
wallet semantics (no invented refund).

Native slot OnDrop is guarded before its unsafe body. Active widgets project
the canonical hero and display flags. Late opening hydrates from shared state;
destruction unregisters, with no gameplay cancellation. Finish/removal callbacks
are intercepted too because they mutate before SetHero. Drag-start stamps the
original hero/revision/epoch so a late drag end cannot remove a newer choice.
Use X before choosing another hero; direct draft moves are not a new atomic
move operation. Automatic local panel cleanup is distinguished from explicit X.

PREPARE clears drafts as part of the same ordered transaction, preserving paid
treatments and invalidating old requests. Pending heroes cannot be dismissed or
added to the expedition party until cleared. A new shared town epoch prevents
old requests from applying after the next mission. No preview state is saved to
an owner file; native committed treatment follows the host save as before.

## Validation and limits

The portable reservation model runs5,000 four-copy schedules/640,000 conflicting
or stale requests under native/MSVC ASan checks. Actual bridge/FIFO/codec fixtures
cover all4 initiators, all3 receiving guests, a host that never opened Tavern,
first-wins conflicts, no local prediction, commit once, shared X/release,
reopened/stale widgets, source callbacks, old drags, navigation/epoch, stopped
sessions, locked slots and partial native failure. Engine effects are modeled.
The local commit checks preserve wallet layout/other currencies and prevent gold
increases; they do not prove an exact price/gold hash across real game copies.
That remains part of full transaction/state verification.
The isolated native release adapter covers18 valid6×3 combinations and14 fault
cases. Eleven exact read-only native proofs include132 named pins/128 native
entrypoints and87 distinct live/early hook targets. No real game was launched.

The new draft integration is limited to the six verified stress activities:
Bar, Gambling, Brothel, Meditation, Prayer and Flagellation. Sanitarium retains
its existing committed-placement path; its extra treatment choices are not newly
claimed synchronized. Unrelated DLC groundwork from70 remains; full live DLC,
direct loot equip, prior provisions crash and complete combat-state verification
remain open. Native rendering and the new hook integration require the next
owner game test. Raw player logs/disassembly are excluded from source publication.

## Guest-autonomous departure

All four initiators were tested through the actual preparation/supply/embark
FIFO with the host idle or its shop closed. All receivers agree on mission,
party/items/funds and map/Rules. A newly found old path ACKed a departure while
its town was unreadable and asked the guest to depart manually. It now waits
at the queue head up to30s and follows automatically; timeout refuses and stops.
The supplied latest session did not contain a provisions/embark attempt, so it
is not evidence of a new permissions failure there. No host-privileged action
was added. Native monster decisions and authoritative seed/snapshot generation
remain automatic engine/host responsibilities, not a player's manual approval.

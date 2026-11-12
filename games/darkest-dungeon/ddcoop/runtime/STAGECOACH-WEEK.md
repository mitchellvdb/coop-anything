# Stagecoach and native week transition — test75, 11 November 2026

## Evidence and cause

Claude Code reviewed an owner two-PC test74 session. The host generated its new
Stagecoach pool on raid completion. Later the guest requested candidate19, which
was absent on the host; candidate21 was accepted by ID alone. Only host logs
were available. Different content under matching IDs is therefore a risk exposed
by the code, not a proven cross-PC class comparison from those logs.

The previous post-raid checkpoint compared currencies, roster IDs/stress/trinkets
and count. It never compared the candidate pool. A separate control against the
unchanged74 runtime reproduces two different named heroes with ID40 being added
on host and guest. Equal IDs and existing Town digests did not prevent this.

Read-only executable proofs identify the normal sequence: RaidFinish3d8da0 from
caller3e24c1 (return3e24c6), followed by TownVisitStart556fb0 from3e24fb. The new
week and Stagecoach are generated before results Next. Sharing Rules at Next is
too late. Native Stagecoach draws use Rules; non-random inputs and ID allocation
still matter, so sharing that state alone is not a complete correctness proof.

## Prevention and comparison

A new exact native RaidFinish hook publishes the host Rules state with a monotone
week epoch. The authenticated connection worker only latches that seed. If a
guest reaches RaidFinish first, it waits up to30 seconds for the worker, without
pumping the engine or the main-thread action FIFO. All copies restore the same
Rules state immediately before the original native function. Automatic wipes
use this same boundary; they do not require a player exit-button callback.

Missing/invalid/conflicting/out-of-order seeds, send/write failure, timeout and
connection loss stop sharing. Native completion still runs once; the hook never
fakes a successful return by omitting engine work. The exact automatic weekly
activity assignment caller also finishes after STOP. Player UI stays blocked.
No existing saves are edited or repaired by these changes.

After initial town load and every raid return, the host starts a candidate-pool
comparison anchored after an ordered action sequence. Every connected guest
waits for that anchor and readable town data, then reports its content digest.
Only matching reports from all connected players release recruitment, provisions
and departure. The host and guests recheck their pool before release. Requests
and applies keep their FIFO head while a release is pending. No manual host
approval is required; the host game and launcher must stay running.

The comparison handles delayed town frames, different panel states, controls
arriving between control polling and ordinary action polling, stale messages
from the previous raid, duplicate controls, disconnects and bounded full queues.
All three send stages fail closed when an outbound queue is full.

Every recruit additionally carries the candidate's64-bit logical fingerprint.
Both host and guest validate it before their native roster addition. The host
uses the same early ordered route as a guest; its former optimistic local drag
is no longer permitted. A mismatching candidate cannot silently add a different
hero on that copy. Shared actions stop with a Resync notice instead.

## Fingerprint scope

The bounded reader compares ID, availability, class identity/name, hero name,
XP, weapon/armor levels, learned/selected combat and camping skills, and the
persisted logical quirk fields established by the native save/load routines.
Pointers, allocation addresses and padding are excluded. Pool entries are sorted
by stable ID. Duplicate IDs/invalid vectors/invalid UTF-8/unreadable or changing
snapshots refuse and clear outputs. Candidate and pool snapshots are rechecked.

FNV64 fingerprints are consistency checks, not cryptographic authentication or
complete Hero/save serialization. Other status/appearance fields may exist.
They do not replace full transaction or combat-state verification.

## Launcher and validation

Claude's campaign mod comparison is retained. New regressions show that unknown
nonempty mod lists and skipped directory links could previously certify unequal
content as equal. Unknown structures and linked/unreadable/incompletely enumerated
mod trees now refuse before guest save replacement. Matching files do not prove
that arbitrary mods or DLC are compatible with the runtime. See launcher/MODS.md.

Actual-bridge tests use modeled native effects: all four initiators, three guests,
exact content before clone, normal and delayed pool/weekly controls, cross-thread
seed delivery while the main thread waits, FIFO ordering, stale/duplicate packets,
timeouts, failures, STOP and real bounded queue overflow. Native/MSVC ASan suites
and strict protocol34 framing/fuzz tests cover these paths. Sixteen read-only
native proofs cover137 named pins/133 distinct addresses (132 function entries
plus one caller witness)/89 live or early hooks. No real game is launched by QA.

The new live check is a normal mission return, then comparing and recruiting the
new Stagecoach pool from different players, followed by guest-led provisioning
and another mission. A forced wipe is unnecessary. Collect every player's logs
on any refusal. Full live native/DLC, direct loot-to-hero equipping, an older
provisions crash and complete state verification remain open. Guests-nosave and
host save preference remain. Checkpoint recovery may lose unsaved progress;
solo batch guests still restart manually. Linux/MinGW/Capstone were not run.

## Test86: startup retries, cosmetic quirk flags and return diagnostics

A TCP-ready guest may still be loading the chosen campaign. Its inactive runtime
poll clears controls. The host now retries the same pool probe only to missing
reporters, at most once per second and within the original 60-second deadline.
Epoch, anchor, content and required membership remain unchanged. Duplicate probes
do not create reports, native mutations or an early release. The regression
actually clears the first probe through the inactive campaign poll, then recovers
all three guests through the retransmission and ordinary comparison.

Entry fingerprint schema2 omits only the quirk presentation flags at +28
(replaces_quirk_viewed) and +29 (is_new). They are still validated as booleans.
The native save routine names those fields separately from +2a (is_locked),
which remains compared along with all previous gameplay fields. Merely viewing
quirks may differ between independent screens. Four flag combinations now give
the same fingerprint; gameplay changes still differ. No candidate is copied or
repaired in memory, and no genuine content mismatch is waived.

Private return reports show matching currencies and roster digests immediately
before a failing new candidate-pool check. They do not establish which candidate
field differed. New read-only per-candidate cumulative hashes split identity,
gear/XP, combat, camping and quirks. Baseline/read/release failures include their
anchor/epoch and local/remote hashes. A repeat with test86 is needed to establish
whether cosmetic flags were the return cause or another native difference remains.
The earlier selected-skill and 150-gold differences also remain unexplained;
ordered wallet and learned/selected-skill traces now locate their first change.
Written by Codex, 12 November 2026.

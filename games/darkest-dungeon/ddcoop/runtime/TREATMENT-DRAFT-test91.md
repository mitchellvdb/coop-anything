# Sanitarium before confirmation — test91

Owner logs show kind6/slot0/group1/entry0 rejected while the session is healthy.
The old reader required a hero in the paid activity slot. The normal Sanitarium
builds treatment lists earlier, while that committed hero ID is still0.

Pinned QuirkTreatment virtual notification538830 receives activity, slot, HeroID.
For a different hero from the committed slot it builds that slot's data and
writes+238. That field is only the last hero across all slots, so reading it
directly would misidentify an older slot after another one was previewed.

A read-only observation hook tracks native record ownership per activity/slot
for the two normal Sanitarium modes. Native effects always run unchanged. An
empty UI initialization whose input matches committed0 does not rebuild, and
must not erase a pending ordered treatment. Cache resets with campaign/weekly
epoch. Paid slots retain the committed identity check.

Local treatment choice accepts the observed draft hero. A receiving copy without
a built list/preview may write the same existing native treatment record after
checking that the requested hero is alive/available in its normal roster. A
conflicting draft or another paid hero is refused before effects. This does not
assign a hero, waive payment, manufacture a quirk or force a different hero into
the local UI. Existing native selection/state and name postchecks remain; the
ordinary shared confirmation later performs allocation/payment. Guest requests
are pending until accept/reject; a rejection explains how to retry.

Tests cover both modes/all4 initiators/3 closed-list receivers, per-slot ownership
after the last-global-hero changes, empty initialization, paid slots, conflicting
drafts and unchanged no-list fallback. Native proof checks the12-byte hook,
QuirkTreatment vtable+38, +238 store, same-committed early exit and normal roster
list-builder jump. No real engine success is claimed. Draft placement itself is
not a new shared reservation for these two modes: competing different previews
of the same Sanitarium slot can be refused safely; full visual draft parity here
remains distinct from the six stress-building reservations.
Written by Codex,15 November2026; prior Claude Code work retained.

# Weekly activity correction — test74, 10 November 2026

## Latest owner session

Only the latest session in the appended logs was assessed: host startup at
21:18:21, followed by three guests. The owner reported a short successful run,
a notice after a party wipe, and all four copies returning to the Hamlet.

- Each guest applied sequences 1 through 149, without gaps or duplicates.
  The three complete applied-action lists match, in order.
- The post-return Town checkpoint matched on all four copies: currency digest
  `fa57cdac`, hero digest `f25c627d`, and nine heroes. Three subsequent shared
  stagecoach recruits brought each roster to twelve.
- No failed APPLY or gameplay STOP was recorded before the final connection
  loss at approximately 21:36:21. That loss stopped the remaining peers.
  The logs do not prove whether a window was deliberately closed or a process
  was lost; it must not be presented as a confirmed deliberate shutdown.
- Three item-on-prop choices and one prop-window X cancellation completed
  consistently. No hand-clearing or shovel choice occurred in this session.
  Those two routes therefore still require live confirmation.

The matching checkpoint is useful evidence for this run. It is not a complete
save comparison or a proof that every native weekly effect executed correctly.

## Confirmed cause of the activity notice

The game itself calls the same AssignHero entrypoint during its automatic
weekly processing. The bridge mistook that native call for another click on
the activity Accept button and required a shared DRAFT reservation. The cached
slot can already be COMMITTED, while the engine is clearing and reassigning
its native records. The bridge consequently refused a legitimate native call
and displayed the misleading already-reserved notice.

This was a real omitted native operation, not merely an unwanted popup. The
automatic caller continues with its slot-record flag update after AssignHero;
it does not use the UI caller's accepted-result check. Hiding the notice alone
would leave the incorrect control flow in place. Matching copies could also
omit the same operation, so equal digests alone do not exonerate this bug.

## Narrow correction

- The exact native weekly return RVA `0x53cbef` calls the original AssignHero
  directly, without generating another request or observed broadcast.
  Named signature pin 136 starts at `0x53cbd4` and protects that callsite.
- The distinct UI return RVA `0x68b0e7` retains shared reservations and ordered
  Accept. It gains no permission to replace an occupied or unreserved slot.
- A nested call inside an already-admitted ordered apply goes directly to the
  original native function. It cannot recursively enqueue or re-emit its effect.
- Outside such an apply, shared STOP and connection guards run before the
  weekly exception. Recognizing the native caller does not resume a stopped
  session or admit new player actions.

This correction adds a callsite pin, not another native hook. It neither edits
existing saves nor claims to repair any native state omitted in an earlier run.

## Validation and limits

The new weekly fixture reproduced 322 failed assertions with the old hook.
With the correction, the runtime fixture suite passes under AddressSanitizer.
It exercises all four roles and six stress activities with EMPTY/COMMITTED
cached slots, one native effect and charge, unchanged reservation state, and no
extra network messages or notices. It also checks ordinary UI draft rejection,
ordered acceptance/replay, outside-apply STOP, and nested native calls.

The native effects in these tests are modeled; no real game was launched.
The separate read-only ABI proof verifies both exact callers, stable hero ID,
slot selection, post-call flag behavior, and the unique pinned neighborhood
against the supported executable. These checks do not establish full weekly
save correctness or live hand/shovel behavior.

Next live check: finish a normal mission, return all copies to the Hamlet,
and verify the absence of the incorrect reserved-slot notice. Then test bar
and abbey drag, shared Accept, and shared X from different players, including
a player whose building panel is closed. A forced party wipe is unnecessary.

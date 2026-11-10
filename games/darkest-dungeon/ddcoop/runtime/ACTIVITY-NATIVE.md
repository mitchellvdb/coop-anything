# Build 28385 activity preview and release evidence

This is an isolated read-only investigation of the supported Windows executable
with SHA256 `f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12`.
No game process was launched, no native function executed, and no installed file
or save changed. All addresses below are image-relative offsets.

## Why the provisional selection diverges

`activity_assign_hero` (`0x5360c0`) runs on Accept and commits payment, roster
location and slot state. The earlier drag/drop selection is held in the UI:

| Object | Field | Meaning |
| --- | --- | --- |
| ActivityHeroSlotInterface | `+0x28` | Selected Hero pointer |
| | `+0x230` | Slot index, uint32 |
| | `+0x280` | Activity pointer |
| | `+0x0c`, `+0x20` | Visible and enabled bytes |
| | `+0x235` | Locked byte; preserve it when projecting UI |
| Activity | `+0x78`, `+0x80` | Begin/end of 24-byte slot records |
| Slot record | `+0`, `+4`, `+8`, `+0x14` | Hero ID, committed state, duration, flag |
| Roster entry | `+0`, `+0x15b0`, `+0x15c0` | Hero ID, location, associated activity |
| Roster entry | `+8` | Embedded Hero object |
| Hero | `+0x137c` | Hero ID, equivalent to entry `+0x1384` |

For bar, gambling, brothel, meditation, prayer and flagellation, activity virtual
method `+0x38` is the no-op `0x2340a0` (`c2 00 00`). Their vtable RVAs are
`e9d728`, `e9d618`, `e9d6a0`, `e9aea8`, `e9ad28`, `e9adb0`, in that order.
Thus that virtual does not retain a background preview. The mod must retain the
draft and hydrate actual widgets on construction. Sanatorium types have other
virtual implementations and require separate evidence.

## Exact observed crash

Drop `0x7e1fd0` receives `(widget + 0x10, position, dragData)` in `rcx/rdx/r8`.
At `0x7e209a` it checks canonical committed occupancy, then at `0x7e20a7` it
resolves the UI-selected hero using `0x7e32d0`. At `0x7e20ac` it dereferences the
returned pointer without a NULL check. A committed slot can therefore coexist
with a missing/stale UI selection after remote Accept, producing the recorded
access violation. Already-constructed widgets must be refreshed after commits.

HeroDragData has vtable `0xeddca0`, Hero pointer `+8`, entry pointer `+0x10`.
The native typed cast `0x43aa10` returns that same object or NULL. Validate the
Hero pointer against the current roster before reading its ID.

## Setter and release

`SetHero` (`0x7e1dc0`) has ABI `void __fastcall(void *widget, void *hero)`.
Setting a non-NULL Hero performs UI preview only for the six verified types.
Passing NULL also changes roster location, clears entry association, and clears
the canonical activity slot's first eight bytes and byte `+0x14`. Consequently
UI projection must never use a NULL setter merely to hide an already-paid hero.

`activity_native_adapter.inc` implements an explicit shared release with the
exact original setter. A zeroed, aligned 0x2d0-byte POD capture supplies only
the three proven fields `+0x28`, `+0x230`, `+0x280`; the setter does not access
this capture's vtable or construct/destruct any C++ member. Native roster
location writes are immediate (`0x58b587`), followed by roster notifications.

Before calling it, the helper verifies supported activity identity/no-op virtual,
canonical hero ID/state, roster location2 and exact association, then snapshots
the bounded campaign wallet. After calling, it verifies the precise slot clear,
location0, NULL association, unchanged wallet and unchanged slot remainder.
`mutated=0` denotes an invalid/stale request refused without effects;
`mutated=1` with failure requires shared recovery and must never be retried.
The caller must project all real widgets after the successful native release.

Deleting destructor `0x7e1c90` and its immediate base destructor do not call
SetHero. Remove cached UI handles on destruction; do not interpret destruction
as a player-requested shared cancellation.

The source callbacks also require interception: primary vtable `+0x18`
(`0x7e2740`) and `+0x20` (`0x7e2630`) contain inline model/location clears
before their final NULL setter. Both have ABI `void(widget, dragData)`.
The generic manager calls `+0x18` after target Drop returns1, `+0x10`
(`0x7e2600`, restores the hero) after return0, and neither after return2.
No-target discard and manager abort can call `+0x20`. Pickup `+8`
(`0x7e2530`) only fills drag data and temporary drag-state fields.

The activity-panel red-X callback `0x68c320` calls the NULL setter at `0x68c370`
before doing any model writes, then marks the roster entry dirty and refreshes
the roster. The setter hook covers that path. Another native caller at
`0x688b2f` automatically clears other uncommitted selections. Distinguish such
UI cleanup from an explicit shared cancellation so navigation cannot erase
other players' reservations.

## Verification and limits

`test_activity_layout.py <Darkest.exe>` checks the exact executable hash,
instruction boundaries, virtual pointers, drop/cast chain, observed NULL
dereference, setter writes and Accept/payment separation. `tools/gen_address_table.py` and `src/addresses.inc` contain the unique 48-byte
pins and copied-prologue lengths for the guarded native targets.

`runtime/test-windows-msvc.cmd` builds the isolated adapter and actual dd_hamlet
reader. A separate MSVC AddressSanitizer build also ran. Eighteen valid cases cover six activity kinds and
three slot indices; fourteen stale/invalid/posteffect fault cases verify refusal
or recovery behavior. All pass. The native setter's effects are modeled in this
fixture; executing it inside the game remains an integration-test requirement.

Raw disassembly text stays private: it contains embedded third-party build paths.

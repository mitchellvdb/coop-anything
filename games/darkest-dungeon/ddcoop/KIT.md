# Test68 / duo51


Based on delivered67, retaining Claude Code60-64/a48f117 and subsequent Codex
work. Runtime protocol31 /122 native signatures; launcher wire/lobby3 retained.
All participants must use68. No new native entry point, save format or dependency.

The supplied duo50 run returns to Hamlet and continues shared recruitment,
upgrades and purchases. Two remembered party members then enter stress buildings.
The campaign's last-party IDs remain7,15,0,0. Later party preflights match that
old vector, but reject the destination as unavailable because it retains one or
both busy heroes. Both host-local and guest-originated attempts fail. Guest
permissions were not the blocker: their requests already reach the host.

A proposed party destination now drops all remembered members whose current
roster location is outside free/party (0/1). The original before-vector remains
exact, so the authoritative stale/conflict check still applies. Removed members
in buildings keep their locations and treatment. Newly selected unavailable
heroes are refused. Re-showing an available hero in its remembered rank also
performs cleanup when needed instead of submitting an invalid show-only action.
Screen refreshes/passive campaigns retain their original native path.

There is no combat-owner restriction on Hamlet composition: Player1..4 may add,
move or remove any available hero. Guests send an intent before mutation; the
host applies and broadcasts it. A host/guest with no built expedition panel
updates the campaign model; native slots bind when the panel exists. No extra
screen requirement or manually edited ownership file. Conflicting stale whole-
party intents remain refused rather than overwriting another committed change.

The regression first fails on67 with two busy members and one available addition.
Final tests cover all four initiators, all three receivers, closed host/guest UI,
byte-identical treated entries, deferred UI, add/move/remove independent of combat
owner, same-rank cleanup and busy-new-hero refusal. Actual native functions are
modeled; live return-to-Hamlet validation still belongs to the owner.

Earlier loot-window direct-equip behavior has not been established as fixed by
this party change. Source/target/submit diagnostics now record bounded item/endpoint identifiers.
A fresh exact error message is still needed to identify that report.
Existing loot trinket tests use modeled native widgets; they do not prove the
reported real-game drag works. Earlier provisions C0000005 also remains unresolved.


RELEASES.json records exact normal/solo runtime, launcher and kit hashes. ZIP
payload/pins/labels/source parity and patches versus67/Claude64 are byte-verified.
All four participants/three receivers, closed host/guest panels, byte-identical
treated entries, add/move/remove, same-rank cleanup and unavailable-new-hero
refusal passed native and MSVC ASan regression tests. Direct loot equip still
requires an exact error report; logging is improved, gameplay fix not claimed.
Source only; no game assets, binaries, saves, artwork, raw logs or memory dumps.

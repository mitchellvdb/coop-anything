# test81 / duo60

Both builds use the same gameplay source. Duo adds DD_DUO and its separate runtime
pin; its launcher label is 0.5.0-duo60, while the runtime's exact hello remains81.
Do not mix normal and duo builds. See RELEASES.json for exact hashes. The ordinary
kit carries a verified owner-signed manifest, serial81; duo uses manual installs.

PASS full Windows runtime suite (including protocol/fuzz/FIFO); final actual
bridge ASan; all4 notice initiators/3 receivers; absent/late/duplicate/new loot,
unrelated dialog/callback/caller/area/STOP/reset; all4 pending loot button cases;
host/guest same-turn animation phases25..35, bounded timeout and stale round.
PASS19 read-only native proofs:140 pins/136 native addresses/92 hooks and exact
empty-callback confirmation ABI. PASS dungeon foundation under ASan, both full
Windows launcher suites including isolated snapshot/preparation/restore, signed
update transfer, CNG and embedded GIF tests. Actual ordinary signed bundle parsed
and all three hashes verified by launcher owner-key CNG code. /analyze: no runtime
warning, one existing launcher unreachable-code warning. Native effects modeled;
not a claim of live engine or multi-PC/Steam Cloud QA. No installed game/save or
system changes. Linux/MinGW/Capstone not run. Known Stagecoach/curio/loot causes
remain in the review; guards were not removed to hide mismatches.

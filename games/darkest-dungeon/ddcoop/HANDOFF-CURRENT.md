# Current: test70 / duo53 (Codex, 10 Nov 2026)

- Based on delivered/published test69 / duo52. Earlier Claude/Codex work retained.
  Claude local a48f117/test64 and Codex checkout/edits are preserved. Use this
  source-full.zip or the dedicated GitHub branch/subtree as development baseline.
- Runtime protocol31, launcher wire/lobby3 unchanged; exact hello version70 is
  required. 121 named pin entries /117 native engine entrypoints;81 distinct
  live/early hook targets. All participants must use the matching new kit.
- Independent recheck found transport STOP lost behind full queues; now atomically
  latched outside FIFO, sent first by every worker, followed by closing the link.
  Native/ASan regressions reproduce four original failures, final suite passes.
- A stalled/dribbling relay no longer blocks a send indefinitely: total5s budget,
  terminal partial-frame failure, checked final handshake send. Actual localhost
  regression1.5s budget versus old4.5s watchdog;8MiB and encrypted PING intact.
- Known town mismatch/unreadable timeout no longer ACKs or processes next order.
  Camping actor/selected skill bounded in codec AND validated against actual local
  vectors/learned skill before native execution;18 old parser failures reproduced.
- Launcher metadata is checked before backup/write; valid missing DLC list means
  empty, damaged present lists refuse. Known campaign DLC needs nonempty witness
  files, not an empty directory. Passive arena_mp flags remain readable, no PvP
  path activated. Small launcher-area fixes/build lists are separately committed.
-10,000 four-controller recovery interleavings;5,000 four-copy mixed-currency
  sequences;1,200 independent extreme-value currency cases. Full native/ASan,
  standalone dungeon model, overlay, both full launcher/fake-Steam suites and
  ten exact read-only native proofs pass. See RECHECK-2026-11-10.md for evidence.
- Nine campaign features: Musketeer, CC/Flagellant/Districts, Shieldbreaker, CoM,
  Fire's Edge/Runaway/Duelist. Read-only witness/fingerprint/JSON comparison tool;
  all five hero data sets fit codec.15 Python tests, one real symlink privilege
  test skipped; mocked link guards run. Matching data is not live support.
- Explicit shard-aware canonical model APIs conserve both currencies. The native
  runtime remains gold-only until actual currency binding/prices/refunds are
  validated. DLC.md lists remaining native shops/events/waves/dreams work.
- 0.5.0-test70: C:\dev\ddcoop-handoff\0.5.0-test70\DDCoop-install.zip
  Kit SHA256 23c8fa3c72a0d2b6a1e2217b6fdbbec497ed6c3cb40d2a2304c7f828a00a1da0
  Runtime 9d9502c123b55b6364a9509a644ddf9513d67bc846171a67da45d865d15a9918
  Launcher 60ee3944b7853021a729de05e3700749a6cc6d429b83388b0f3c82b448a19ab2
- 0.5.0-duo53: C:\dev\ddcoop-handoff\0.5.0-duo53\DDCoop-install.zip
  Kit SHA256 1b9dbdbf6a7379aa01a0e218e6c1530cd3ee12e67fe903f0f636258a1482dcb1
  Runtime 377d8708e2cb5c378c79e1c709093c6acfad49ef5ac26b03c817795f80427c67
  Launcher 2590b2d7a7ee29e2ae8ba3c10d381d3bfe50b4d1fc8b2ac1a81fde2b4031c18d
- Direct loot equip, full combat-state digest/remaining rank-action hero IDs,
  earlier provisions C0000005, live four-PC restart/Steam Cloud and full DLC
  behavior remain open. No real game/installer/Steam write executed in this review.
- Guests always-nosave; host save checkbox retained; selected slot1..9 only,
  profile_9 forbidden. Resync resumes native checkpoint and may lose unsaved
  progress. Solo .bat guests restart manually. Linux/MinGW/Capstone not run.
- Next owner test: same kit everywhere, curio/camp/fight/items/loot, shared results,
  next party/departure and same inventories/HP/owners. Copy logs immediately on
  any stop; no unsupported local continuation. DLC hero/mission tests separately.
- Source commits separate runtime safety, DLC model/readiness, minimal launcher,
  ordinary runtime pin and release/report. Full source plus byte-reproducing
  patches versus69 and Claude64. Written by Codex; Claude contributions retained.

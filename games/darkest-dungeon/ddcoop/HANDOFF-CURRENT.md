# Current: test71 / duo54 (Codex, 10 Nov 2026)

- Based on delivered/published test70 / duo53, all prior work retained. Claude
  checkout a48f117/test64 and existing Codex checkout/edits remain untouched.
  Use source-full.zip or the dedicated GitHub subtree as development baseline.
- Runtime protocol32; launcher wire/lobby3 unchanged. Exact runtime hello71 and
  matching kits required for every participant. 132 named pins /128 native
  entrypoints;87 distinct live/early hooks. Never relax unknown-build guards.
- New six stress-activity drafts: RESERVE/CLEAR/ACCEPT/confirmed RELEASE are
  ordered with exact hero, slot revision and town epoch. First accepted intent
  owns the slot. Pending placement projects into open/reopened Tavern and Abbey
  widgets without charging. Native Accept charges once; confirmed X preserves
  the actual native funds behavior. No invented refund or optimistic mutation.
- Guard native OnDrop before its observed NULL-resolver dereference path.
  Hook source finish/removal too; these mutate before SetHero. Drag-start stamps
  prevent late drags clearing new reservations. Widget destruction unregisters;
  automatic panel cleanup does not cancel shared choices. Explicit X does.
- PREPARE clears unpaid drafts in its ordered transaction; paid treatment stays.
  Pending heroes cannot be dismissed/added to expedition party until cleared.
  Native read-only predicates prevent reserving locked/blocked treatment slots.
- Any player can place/accept/remove, provision and embark with the host idle
  or its building/shop closed. Fixed an old departure ACK/manual-follow path:
  expected FIFO head waits for usable town up to30s, then follows or sync-stops.
  Host must keep its game/launcher running; no manual approval is required.
- Native and MSVC ASan actual-bridge regressions cover all4 initiators/3 guests,
  competing slots/hero identity, commit once, confirmed release, reopen/stale
  widgets, source/old drag, epochs/navigation/STOP and partial native failure.
  Engine functions are modeled. Portable model:5000 four-copy schedules,
  640000 requests. Adapter:18 valid slots/14 fault cases. Eleven read-only
  native proofs pass. Full normal/duo launcher/fake-Steam tests, overlay, hidden
  preview/English/GIF and retained transport/DLC/restart simulations pass.
- Real rendering/new hooks still need owner game testing. Six stress activities
  only; Sanitarium keeps its old committed path/extra choices. Clear with X
  before choosing another hero; no new atomic slot-to-slot drag move claimed.
  Full live DLC, direct loot equip, prior provisions crash, complete combat-state
  digest and remaining rank-based item identity remain open. DLC.md applies.
- Guests always-nosave, host save checkbox retained; selected slots1..9 only,
  profile_9 forbidden. Resync uses host checkpoint and may lose unsaved progress.
  Solo batch guests restart manually. No real game/installer/save/Steam writes.
  Linux/MinGW/optional Capstone not run; existing C4702 warnings noted.
- 0.5.0-test71: C:\dev\ddcoop-handoff\0.5.0-test71\DDCoop-install.zip
  Kit SHA256 f27a02ee6c80cc4692b8b9afe8d95db5e2d5c33a450c757b8c291936356785cf
  Runtime 388abd6a661fc5f5bbb692d33ddbc176b8726474eda639fdec93bf5b8e73f86d
- 0.5.0-duo54: C:\dev\ddcoop-handoff\0.5.0-duo54\DDCoop-install.zip
  Kit SHA256 5c6a5108d14a764861b05f466e0048be684092797466271e81d41715c8873603
  Runtime 6bafc3b394917fec2f8bc0e452a3dbc99c23839bc2df79076c7c2fb8e01c88b4
- Source commits separate runtime work, minimal launcher/tools, ordinary runtime
  pin and release/handoff. Byte-reproducing patches vs70 and Claude64 included.
  Written by Codex; earlier Claude Code contributions retained.

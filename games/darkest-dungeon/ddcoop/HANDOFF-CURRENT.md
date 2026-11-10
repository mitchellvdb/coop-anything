# Current: test74 / duo57 (Codex, 10 Nov 2026)

- Based on delivered/published test73 / duo56. Latest owner four-window session:
  all three guests apply seq1..149 without gaps/duplicates. All four post-return
  Town checkpoints agree (9 heroes); three later recruits take every roster to12.
  Prop item choices3 and X1 succeed; no hand/shovel action in this live session.
  No gameplay STOP. Final connection loss stops peers; intentional close versus
  lost process is not established by logs. Town digests are not full-save proof.
- The reserved-slot notice at the week update is a real bug: a native automatic
  AssignHero call is misread as UI Accept and blocked without a DRAFT reservation.
  The engine continues setting its slot flag despite the omitted native call.
- Exact native weekly return RVA53cbef now goes directly to the trampoline,
  without guest request/host observed broadcast. UI68b0e7 retains ordered shared
  drafts. Already-admitted nested effects finish once without re-emission;
  STOP/unready guards apply before outside native exception. Existing town
  epoch barrier re-seeds afterward. No old save repair is claimed.
- Runtime startup banner and exact hello now share DD_RUNTIME_VERSION. Bytewire33
  unchanged; exacthello74 requires matching new peers. Launcher wire/lobby3 and
  ownership/party mapping unchanged.136 named pins/132 native addresses include
  131 function entries plus1 exact caller witness;88 live/early hooks unchanged.
- New actual-bridge regression reproduces322 before-fix assertions then passes
  MSVC ASan: all4 roles/all6 stress activities/EMPTY+COMMITTED cache, one modeled
  native effect/charge, no packets/notices, ordinary UI ordering, STOP and nested
  applies. Fourteen exact read-only native proofs pass. Full native/fuzz/FIFO,
  both launcher/fake-Steam/restore suites, overlay/hidden English/GIF preview,
  analysis and retained recovery/transport/reservation/DLC models pass.
- See runtime/ACTIVITY-WEEKLY.md. Native effects in fixtures are modeled; no real
  game/installer/save/Steam write or live repair test. Next owner check: finish a
  normal mission, return to Hamlet, then shared bar/abbey draft/Accept/X from
  different players. A forced wipe is unnecessary. Hand/shovel still need live
  confirmation. Prior activity/provisioning/departure/selection/prop fixes retained.
- Direct loot-to-hero trinket equip, full native/live DLC, complete transaction/
  combat-state verification and older provisions crash remain open. Guests-nosave,
  host save checkbox, selected slots1..9/profile_9 forbidden remain unchanged.
  Checkpoint recovery may lose unsaved progress; solo batch guests restart
  manually. Linux/MinGW/optional Capstone not run; existing warnings recorded.
- 0.5.0-test74: C:\dev\ddcoop-handoff\0.5.0-test74\DDCoop-install.zip
  Kit SHA256 bd977b2edff7d0708842445dcbc45d7461aaedfb837937f56f4fbe82c9dcc37a
  Runtime 772945e677b76fe1398d98070ac8a923980ab466d7b2721397090e287d6f4fb5
- 0.5.0-duo57: C:\dev\ddcoop-handoff\0.5.0-duo57\DDCoop-install.zip
  Kit SHA256 2e82f749de3ff8201055c846ff2fd48dc6a2b2798c98df10a1c4b4ba61fb2cbf
  Runtime bf8281a3fbbfb1e8f2c4639fd9e45494d06c584824332ae35fee6f4337c214eb
- Source commits separate runtime work, minimal launcher/tools, ordinary runtime
  pin and release/handoff. Byte-reproducing patches vs73 and Claude64 included.
  Written by Codex; earlier Claude Code contributions retained.

# Current: test73 / duo56 (Codex, 10 Nov 2026)

- Based on delivered/published test72 / duo55. The latest four-window owner logs
  show combat/loot complete, then a guest hand-clearing an obstacle locally with
  no USE request. Host/other guests later shovel it; first guest lacks overlay.
- Correct hand callback714770 permitted a normal NULL actor; old request builder
  rejected it then ran native locally. Old72 negative control reproduces exactly
  one local hand effect/zero shared packets. Initial6-route fixtures fail62
  assertions before fix; final expanded tests are separate and pass.
- All terminal hand/tool/curio button/X/item-drop/routed-click choices now use
  one runtime_apply path for host and guests. First accepted valid choice wins.
  Exact area/prop/kind/revision/actor ID preflight precedes Rules/selection/native.
  Normal NULL native actor semantics preserved. Rejected rivals have no effect;
  old choices/OPEN after X/reopen and shared scene transitions cannot act again.
- Early typed curio drop70bf70 plus matched source cleanup avoids optimistic item
  use and stale native Restore counts. Canonical bag debit exactly once, including
  last item. Both native bag drag forms supported; other sources fail closed.
  Pure428fe0 distinguishes food/hero use from curio routing; pure4b12e0 validates
  item/variation before effects. Late optimistic ITEM_CURIO path disabled.
- Runtime protocol33/exacthello73; launcher wire/lobby3 unchanged. All peers need
  matching new kit.135 named pins/131 native entries/88 distinct live/early hooks.
  One new live early-drop hook plus pure route/match pins. Thirteen exact
  read-only proofs pass. Native Windows/ASan/FIFO/fuzz, full normal/duo launcher/
  fake-Steam/restore, overlay/hidden English/GIF preview and analysis pass.
- New tests: seven routes/all4 initiators/all3 receivers, NULL actors, late panel,
  no fallback on stopped/unready/malformed captures, shared synthetic HP/stress/
  items/Rules;25 valid competing pairs x4 winners x3 receivers;14 invalid native
  item-match cases; direct drop/last item/stale source cleanup; separate routed
  item clicks versus normal food and delayed changed routing. Effects modeled.
- Prior bar/guest provisioning/departure/post-combat selection, transport/recovery
  and DLC groundwork retained. No real game/installer/save/Steam writes. Next
  owner test: guest hand and shovel at separate obstacles, curio hand/item/X,
  normal food, loot/walk/next fight; collect all runtime logs on any stop.
- Shared native routes are covered, not every DLC/mod-specific effect live-tested.
  Direct loot-to-hero trinket equip, exact full transaction/combat-state digest,
  older provisions crash and full live DLC remain open. Guests always-nosave,
  host checkbox retained; selected slots1..9 only/profile_9 forbidden. Recovery
  uses host checkpoint and can lose unsaved progress; solo batch guests restart
  manually. Linux/MinGW/optional Capstone not run; existing warnings recorded.
- 0.5.0-test73: C:\dev\ddcoop-handoff\0.5.0-test73\DDCoop-install.zip
  Kit SHA256 40f6777f1499af7f1dc83982b600f11863fa042f296af6a5a5245bec40a8e56d
  Runtime 089c1266c175d2fc69f03a4ddbb29db035ee4a710308d2f23d3ea0f48d5ee8ad
- 0.5.0-duo56: C:\dev\ddcoop-handoff\0.5.0-duo56\DDCoop-install.zip
  Kit SHA256 f49ea0d93e3448267a0cf9d5fd610a82f60a242593a8a51d2055f252918a0de2
  Runtime 1397a12c304f8658114c0af56e0784819a0bf6914037cdefd305754d16829f23
- Source commits separate runtime work, minimal launcher/tools, ordinary runtime
  pin and release/handoff. Byte-reproducing patches vs72 and Claude64 included.
  Written by Codex; earlier Claude Code contributions retained.

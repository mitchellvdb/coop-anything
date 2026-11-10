# Current: test72 / duo55 (Codex, 10 Nov 2026)

- Based on delivered/published test71 / duo54. Owner confirmed Tavern draft
  placement and provisioning work in the actual four-window game test.
- New stop occurred after combat, not during placement/provisioning. Final
  combat choices/recorded Rules match. Host sends SELECTseq98 immediately after
  victory; all3 guests fail it14-27ms later before their own combat-over log.
  Source SELECT bypassed readiness but apply refused battle: transition race.
- SELECT now waits at the expected FIFO head up to30s for readable non-battle
  raid in the same area, for host receiving guest requests and all3 guests.
  No effect/ACK or later order while waiting. Wrong area/endless battle/loading
  refuses safely at timeout; native postcondition failures stop after effect.
- Existing item.id carries persistent selected HeroID. Codec rejects missing/
  sentinel identity, invalid rank/combat shape/flags. Apply bounds party to4,
  checks rank+ID/native availability and verifies selected Hero*+ID and display
  field before success. Concurrent shuffle mismatch refuses, never retargets.
- Runtime byte protocol32/launcher3 unchanged; exact hello72 and same kits on all
  peers required.132 named pins/128 native entries/87 hooks remain unchanged.
  Independent pinned606450 analysis proves party.selected+430 == raid+438.
- The71 native/overlay/build baseline passed before edits; added actual-bridge
  regressions fail75 assertions on old code. Final native/fuzz/FIFO and MSVC
  ASan pass: all4 initiators/3 guests, late combat/missing view, no overtaking,
  once-only native effect, identity/vector/native fault and timeout/STOP cases.
  Native effects modeled. Twelve exact read-only proofs and full normal/duo
  launcher/fake-Steam/restore tests pass; hidden preview/English/GIF retained.
- All71 shared activity/AFK work and70 transport/DLC/recovery groundwork remain.
  Six stress activities only, Sanitarium extra choices retain existing path;
  clear X then place, no new atomic slot-to-slot move. Full live DLC, exact
  cross-copy native prices, full combat-state digest, rank-based item identity,
  direct loot equip and earlier provisions crash remain open.
- No real game/installer/save/Steam writes; live repair needs the next owner
  fight-to-loot test. Host keeps game/launcher running, guests-nosave, host save
  checkbox unchanged. Only selected slots1..9, profile_9 forbidden. Recovery
  resumes host checkpoint and can lose unsaved progress; solo batch guests
  restart manually. Linux/MinGW/optional Capstone not run. Known warnings noted.
- 0.5.0-test72: C:\dev\ddcoop-handoff\0.5.0-test72\DDCoop-install.zip
  Kit SHA256 fc198c0700ec12e7603cbbc173b45dd3a52d59c903bb83b464815d3a79fe643c
  Runtime 5a093f34a61b1a147d057fca8341c188b4d444f7ebf76713db5210f3b8aa1fe2
- 0.5.0-duo55: C:\dev\ddcoop-handoff\0.5.0-duo55\DDCoop-install.zip
  Kit SHA256 4481f35f3e67973d64a00ea8079041287a5462a3825ce2ebf4e543e92421ad05
  Runtime 1c7b91fd6de2f0cc300e2adc8965938cd2ece34d2fc4e98918cdb63a4f64bf6c
- Source commits separate runtime work, minimal launcher/tools, ordinary runtime
  pin and release/handoff. Byte-reproducing patches vs71 and Claude64 included.
  Written by Codex; earlier Claude Code contributions retained.

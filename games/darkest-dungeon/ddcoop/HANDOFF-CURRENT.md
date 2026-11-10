# Current: test69 / duo52 (Codex, 10 Nov 2026)

- Based on delivered/published test68 / duo51; earlier Claude Code work retained.
  Local Claude a48f117/test64 and Codex checkout/edits are preserved. Current
  development source is this bundle or the dedicated GitHub branch/subtree.
- Runtime protocol 31, launcher wire/lobby 3 unchanged; exact runtime hello
  version is now required. Every participant must use 69. Native pins: 121 named
  entries / 117 distinct engine entrypoints; 81 distinct live/early hook targets.
- Thorough source review and baseline tests found a truncated native instruction
  in tutorial_show: 10 bytes cut push r13, corrected to 11. A new dumpbin proof
  reproduces the old failure and verifies all hook boundaries/relative operands.
  No proven connection to any previously reported crash.
- Failed shared item/trinket operations no longer receive false success ACKs.
  Removed guest private item/results fallbacks; bounded waits/peer-wide stop
  remain. Combat skill/item/retreat RNG mismatches stop before native mutation.
- Each of three guests has its own atomic movement vote; idle does not erase
  another guest, opposite directions stop, equal directions move once. Freshness,
  message order and disconnect cleanup tested. Host has the same movement vote.
- Copy logs redacts known auth ticket/token/password lines in exported copies
  only. Original logs/saves untouched; this is not full path/name anonymization.
  Launcher-area change is small and separately committed, including build lists.
- Hook array capacity follows the actual count, with a compile-time upper limit.
  Removed duplicate item_clear name, retained valid aliases; generator rejects
  duplicate names. Remaining F9 labels translated. BUILD/tool expectations fixed.
- Baseline 68 tests and before-fix failures preserved. Final native/fuzz/FIFO,
  MSVC ASan, overlay, full normal/duo launcher/fake-Steam suites, nine read-only
  native proofs, English/GIF/native layout and DLC readiness checks pass. Static
  analysis of 13 major units has no new analyzer diagnostics; existing C4702
  unreachable-code compiler warning remains noted separately.
- 0.5.0-test69: C:\dev\ddcoop-handoff\0.5.0-test69\DDCoop-install.zip
  Kit SHA256 e5f5bde623fd5f3ab5154f4423a609831c2b7f8cfc77c5bd3a6b440090cb89e8
  Runtime 2f81663c78d354792d77fbc5a76945e8f20231a3c17818dc667a2b179e581023
  Launcher f20ea86186a59e31a196a8f444ce7cfc4f18e1746f07be85639498e7125ea6ff
- 0.5.0-duo52: C:\dev\ddcoop-handoff\0.5.0-duo52\DDCoop-install.zip
  Kit SHA256 560077f2681be94095ac745bb6184b9a5a1c14759d39f785b1cee2dfe87f4402
  Runtime f7d4ba6bf40a0c9f3babe64a20aa4427c69680d7a4bfad6a863c6665ef6a1793
  Launcher 08b917fd5ccb29f794255743cef74bb3898372207c8d93f71f5054f4132127d2
- Read runtime/REVIEW-2026-11-10.md for findings, evidence, priorities and next
  player test. Direct loot-to-hero equip remains an unresolved live report; exact
  error pending, diagnostic hooks retained. Missing items now stop honestly.
- Next: same kit everywhere; curio, fight, inventory/HP/owners, shared results;
  replace two treated former party members, embark again. Try each guest walking
  while others idle and conflicting directions. Copy logs immediately on a stop.
- Open: full post-combat state digest/persistent IDs for remaining rank actions,
  real four-PC restart/Steam Cloud, live DLC waves/shards/dream transitions and
  unsupported sort/unequip-all/arbitrary dungeon drags. Earlier provisions
  C0000005 remains unresolved; tutorial-hook fix does not prove that crash fixed.
- NOT RUN: live game/installer, real Steam storage, multi-PC restart, Linux/MinGW
  or optional old Capstone model check. Fixtures use modeled native effects;
  nine new/current dumpbin layout proofs were run. Guest always -nosave, host
  save preference retained. Resync uses native host checkpoint and may lose
  unsaved progress. Solo .bat guests restart manually.
- Runtime, minimal launcher/log-export, ordinary runtime pin and release/report
  source are separate commits per owner authorization. Full source and patches
  versus test68 and Claude a48f117/test64 are byte-checked. No binaries, artwork,
  game assets, saves or private logs are published. Written by Codex; prior
  Claude Code contributions retained.

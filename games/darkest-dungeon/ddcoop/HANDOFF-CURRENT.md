# Development candidate: test75 / duo58 (Codex, 11 Nov 2026)

Latest delivered gameplay baseline remains74. Claude's current repo baseline
296222c adds campaign mod preflight and the Stagecoach task after an owner raid.

Implemented candidate75:
- Logical Stagecoach candidate/pool fingerprints (ID, availability, class/name,
  XP/equipment, learned/selected skills and persisted quirks). Same-ID different
  content refuses before local native clone. No raw pointer/padding hashing.
- Anchored all-peer pool probe/report/release after load/raid return. Recruitment,
  provisions and departure wait for every connected player's matching pool.
  No manual host approval. FIFO and late-control/late-town races are covered.
- Native RaidFinish shares host Rules before week generation, including automatic
  wipes. Network worker only latches data; bounded wait pumps no game/FIFO.
  Always complete native once on failure; exact automatic activity work finishes
  after STOP while player UI remains blocked. No old save repair claimed.
- Claude's campaign mod check retained, with unknown/empty/duplicate layouts and
  linked/unreadable/incompletely enumerated mod trees now refused before writes.
- Runtime34/exacthello75, launcher wire3;137 pins/133 addresses (132 native
  function entries plus1 witness),89 hooks. See runtime/STAGECOACH-WEEK.md.

Validation completed: native/protocol/fuzz/FIFO and MSVC ASan bridge (all4 roles,
3guests, cross-thread week seed, anchored pool, delayed native/UI frames,
STOP/disconnect/timeouts and full real queues),16 native proofs, both launcher
unit/core/fake-transfer suites, mod14-failure-before-fix/native+ASan regressions,
isolated overlay/English preview/GIF and static analysis. Old74 control accepts
sameID40/different named heroes on both copies. New tests prevent local clone.
Native effects modeled; no real game launched. Linux/MinGW/Capstone not run.

RELEASE PENDING: preparation/restore helper integration tests must run with all
real Darkest processes closed (BUILD.md). Owner is still playing. The baseline
helper refused correctly while the game was running; this is not a passing
restore test. No75 player kit has been delivered or installed. Existing kits and
Cdev checkouts/saves untouched. Pending artifacts and source are in the workspace
outputs/0.5.0-test75-pending and outputs/0.5.0-duo58-pending.

Next action: after the owner closes every game, run each built launcher's fake-
Steam preparation test; then package normal75/duo58, verify hashes/patches/payload,
publish final release metadata and deliver new Cdev version folders. Preserve
immutable older outputs. Do not rebuild runtime unless source changes; pins below
match the compiled source. Host checkbox/guest-nosave/selected slots1..9 retained.

Open: live normal mission return/recruit verification; full native/live DLC,
direct loot-to-hero equip, complete state checks and older provisions crash.
Checkpoint recovery may lose unsaved progress; solo batch guests restart manually.
Written by Codex; Claude Code's launcher work and prior contributions retained.

[
  {
    "version": "0.5.0-test75",
    "runtime_sha256": "d17d5b42b35ab76c07de143432a0f159ae8c0e1f0d6186ce44018100dbae384f",
    "launcher_sha256": "ce0b30a4cb1f6afd76dfde6073425abdbf43be94fd0885bf1e3ec3deb0709000"
  },
  {
    "version": "0.5.0-duo58",
    "runtime_sha256": "60708a9f365791e06cfc5249b3dcc8b804d09ccf6218ca59ae777dfef54741c6",
    "launcher_sha256": "61862b0a54c5bbf3a82803d8bfd013eebb088025bbec8c83b564aacb5899b692"
  }
]

# DDCoop build and validation — test75 / duo58

Start from the current source snapshot, not the older local test64 checkout.
Windows x64 requires Visual Studio 2022 C/C++ tools, Windows SDK and Python 3.
Use an x64 Native Tools Command Prompt. Player builds use /MT; no GCC runtime
DLLs or extra redaction library are required. Optional mascot artwork is external.
Historical instructions remain in earlier commits and source bundles.

## Supported game and invariants

Steam Windows x64 build 28385, SHA256:
f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12.
Never relax pins for an unknown update. 137 named signatures cover 133 distinct
native addresses:132 function entries plus one exact weekly caller witness
(some deliberate alias names);89 live/early hook targets are distinct.
Runtime protocol 34; exact runtime hello version75 also required. Launcher wire 3.
Guests always-nosave; the host keeps its save checkbox. Only selected slots 1..9;
profile_9/the game's restore slot is forbidden. No Circus/PlayFab/tamper path.

## Build and tests

    set "DD_GAME_EXE=<game dir>\_windows\win64\Darkest.exe"
    runtime\test-windows-msvc.cmd
    runtime\test-overlay-msvc.cmd
    runtime\build-windows-msvc.cmd

After rebuilding the runtime, compute its SHA256 and pin LR_HASH_RUNTIME in
launcher/src/lr_hashes.h in a separate commit. Then:

    set "DDC_TEST_NOSAVE=0"
    set "DDC_VERSION=0.5.0-test75"
    set "DDC_MASCOT_GIF=<optional local GIF>"
    launcher\build-windows-msvc.cmd
    launcher\test-windows-msvc.cmd

Close every real Darkest Dungeon process before launcher preparation tests.
Those tests use fake Steam/storage and disposable files; they refuse otherwise.
The launcher source lists include lr_restart.c and lr_logs.c on both toolchains.
The Windows/Linux suites include test_lr_logs (synthetic secrets only).

Read-only proofs (do not launch or modify the game):

    python -I tests/test_hook_layout.py "%DD_GAME_EXE%"
    python -I tests/test_provisions_layout.py "%DD_GAME_EXE%"
    python -I tests/test_hunger_layout.py "%DD_GAME_EXE%"
    python -I tests/test_quest_return_layout.py "%DD_GAME_EXE%"
    python -I tests/test_embark_layout.py "%DD_GAME_EXE%"
    python -I tests/test_formation_layout.py "%DD_GAME_EXE%"
    python -I tests/test_fresh_raid_layout.py "%DD_GAME_EXE%"
    python -I tests/test_prop_layout.py "%DD_GAME_EXE%"
    python -I tests/test_supply_init_layout.py "%DD_GAME_EXE%"
    python -I tests/test_camping_layout.py "%DD_GAME_EXE%"
    python -I tests/test_activity_layout.py "%DD_GAME_EXE%"
    python -I tests/test_selection_layout.py "%DD_GAME_EXE%"
    python -I tests/test_prop_choice_layout.py "%DD_GAME_EXE%"
    python -I tests/test_activity_weekly_layout.py "%DD_GAME_EXE%"
    python -I tests/test_recruit_layout.py "%DD_GAME_EXE%"
    python -I tests/test_week_transition_layout.py "%DD_GAME_EXE%"
    python -I tests/test_dlc_readiness.py

The full hook proof uses VS dumpbin from PATH or --dumpbin <tool path>. It checks
actual instruction boundaries/relative instructions for all live and early hooks.
The optional old test_game_build.py additionally uses Capstone 5 for model/RTTI
checks; it is not required by the dumpbin proof. Do not infer live success from
fake native functions. See runtime/RECHECK-2026-11-10.md for findings and open work.

MSVC ASan: compile the runtime-bridge test using the same source list as
runtime/test-windows-msvc.cmd with /fsanitize=address /Zi /Od /MT. Run that test
binary. Static /analyze is additional evidence, not native engine execution.

Additional isolated tests (never launch the game):

    set "DNG_ASAN=1"
    runtime\test-dungeon-msvc.cmd

The main runtime suite also runs real localhost bounded-send/crypto checks,
5,000 four-copy mixed-currency model sequences and 1,200 currency edge cases.
The full launcher suite includes 10,000 four-controller restart simulations.
DLC groundwork and data-comparison CLI are documented in runtime/DLC.md.
Native supply integration remains gold-only; the shard APIs are model groundwork.

## Variants and packaging

For duo58, build the same runtime with DD_DUO, pin that separate DLL and build the
launcher with DDC_VERSION=0.5.0-duo58. Gameplay source remains identical. Source
archives differ only in the duo runtime pin and test instructions.

The verified custom SDL2 bootstrap pin remains:
ad3c25374e9fb23ff5e388cde052a7359abad1cda9bf5afb6aad399e25e82589.
Keep the genuine SDL2_orig.dll; never overwrite an unknown original. A staging
directory contains DDCoop.exe, SDL2.dll and DDCoop/ddcoop-runtime.dll:

    python -I tools/make_dropkit.py <staging dir> <output zip> <version>

Do not publish binaries, game assets, saves, raw logs or mascot artwork in Git.
Copy logs now redacts known authentication ticket/token/password lines in the
exported copy only. It is not full anonymization of names or paths.

Linux portable tests: sh launcher/build-linux.sh (gcc, ASan/UBSan).
MinGW Windows build: launcher/build-windows.sh, with DDC_VERSION explicitly set.
Those toolchains were not run in this review; Windows/MSVC validation was run.

Test71 adds shared provisional Tavern/Abbey activity slots (six stress activities).
The actual bridge fixture checks all four initiators with a closed/idle host,
first-wins reservation, commit once, clear, native confirmed removal, UI hydration,
source-finish/removal and stale drag epochs. The native adapter proof is static;
actual engine effects in fixtures are modeled. See runtime/ACTIVITY.md and
runtime/ACTIVITY-NATIVE.md. Runtime32 requires the new matching kit on all peers.
Guest departure waits up to30s for its town then follows without a local click;
it refuses/STOPs at timeout rather than ACK a manual fallback.

Test72 repairs post-combat SELECT readiness and carries persistent hero identity
in existing item.id. The exact runtime hello72 is mandatory although bytewire32
is unchanged. Twelve read-only proofs include test_selection_layout.py for the
native selected pointer postcondition. Actual bridge/FIFO/codec regressions and
MSVC ASan cover all4 initiators/all3 receivers, delayed battle/loading, timeout,
area/identity/vector/native failure and no FIFO overtaking. See
runtime/SELECTION-TRANSITION.md. Test71 bar drafts/provisions are owner-confirmed.

Test73 shares all terminal curio/obstacle choice routes (hand/tool/X/item drop
and routed item click) through one first-accepted resolution. Nullable native
actors are preserved; revision and actor identity protect stale/crossed choices.
The early drop/source-cleanup pair prevents stale native count restoration.
Pure item route/acceptance preflight keeps hero food separate and rejects invalid
curio items without a shared effect. Thirteen read-only native proofs/135 named
pins/131 entries/88 hooks, final native/MSVC ASan and full normal/duo launcher
suites are validation requirements. See runtime/PROP-CHOICES.md. Wire33/hello73
requires all new peers. No live DLC or full-state-verification claim is added.

Test74 distinguishes the automatic weekly activity assignment caller from UI
Accept; it also centralizes startup/hello runtime version reporting. New bridge
fixtures reproduce322 assertion failures with the old hook, then pass with the
fix across all4 roles/all6 stress activities, stale EMPTY/COMMITTED caches,
single modeled native cost/effect, no extra packets/notices, UI ordering, STOP
and nested applies. Fourteen read-only proofs include the exact weekly/UI caller
proof. The new136th pin is a caller witness, not another hook/function entry.
See runtime/ACTIVITY-WEEKLY.md for the live session evidence and validation limits.

Test75 adds logical Stagecoach candidate/pool fingerprints, all-peer comparison
before recruitment/provisioning/departure, and a dedicated network-latched weekly
Rules seed at normal native RaidFinish (also automatic wipes). Sixteen exact
read-only proofs,137 named pins/133 distinct addresses/89 hooks are required.
Run the full native/MSVC ASan bridge and both launcher suites, including Claude's
campaign mods tests and strict unknown-list/linked-or-unreadable-tree regressions.
The new schema is runtime34/exacthello75; launcher wire3 remains. The old74 native
weekly assignment exception now completes exact engine week work even after a
synchronization STOP; ordinary UI remains blocked. See runtime/STAGECOACH-WEEK.md.

# DDCoop build and validation — test69 / duo52

Start from the current source snapshot, not the older local test64 checkout.
Windows x64 requires Visual Studio 2022 C/C++ tools, Windows SDK and Python 3.
Use an x64 Native Tools Command Prompt. Player builds use /MT; no GCC runtime
DLLs or extra redaction library are required. Optional mascot artwork is external.
Historical instructions remain in earlier commits and source bundles.

## Supported game and invariants

Steam Windows x64 build 28385, SHA256:
f9d914ca1d7d9ad3ff4f151a46e4853e532ce1f5f109f700b0c1381a3f7e0d12.
Never relax pins for an unknown update. 121 named signatures cover 117 native
entrypoints (some deliberate alias names);81 live/early hook targets are distinct.
Runtime protocol 31; exact runtime hello version also required. Launcher wire 3.
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
    set "DDC_VERSION=0.5.0-test69"
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
    python -I tests/test_dlc_readiness.py

The full hook proof uses VS dumpbin from PATH or --dumpbin <tool path>. It checks
actual instruction boundaries/relative instructions for all live and early hooks.
The optional old test_game_build.py additionally uses Capstone 5 for model/RTTI
checks; it is not required by the dumpbin proof. Do not infer live success from
fake native functions. See runtime/REVIEW-2026-11-10.md for findings and open work.

MSVC ASan: compile the runtime-bridge test using the same source list as
runtime/test-windows-msvc.cmd with /fsanitize=address /Zi /Od /MT. Run that test
binary. Static /analyze is additional evidence, not native engine execution.

## Variants and packaging

For duo52, build the same runtime with DD_DUO, pin that separate DLL and build the
launcher with DDC_VERSION=0.5.0-duo52. Gameplay source remains identical. Source
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

# Claude Code instructions

The shared working agreements for Codex and Claude Code are in AGENTS.md; follow them:

@AGENTS.md

## For Claude Code specifically

- Your working folder is `C:\dev\darkest-dungeon-coop`, your branch `hero-ownership` (or another `claude/...`
  branch). Codex works in `C:\dev\darkest-dungeon-coop-codex` on `codex`. See what Codex did with
  `git log hero-ownership..codex` and merge it with `git merge codex`.
- Don't write in-process runtime code (`runtime/`); that is Codex's part (AGENTS.md §3).
- Build on Windows in MSYS2 UCRT64 (`C:\msys64`): `CROSS= DDC_VERSION=<version> sh launcher/build-windows.sh`.
  From PowerShell, run it through `C:\msys64\usr\bin\bash.exe -lc '...'` with `MSYSTEM=UCRT64` and
  `CHERE_INVOKING=1`.
- Tests: `test_lr_wire` and `test_lr_roster` build natively; `test_lr_core` uses POSIX, so run
  `sh launcher/build-linux.sh` in WSL (Ubuntu, WSL1; it runs all four under ASan/UBSan and stops at the first
  failing one). The relay part of `test_lr_core` (around `launcher/tests/test_lr_core.c:500-530`) needs
  localhost port 27360 and up to itself: the core picks the first free port, and the fake host game listens on it
  only 1.5 s later. WSL1 shares Windows' ports, so a second test run at the same time (in WSL or on Windows), a
  launcher or a game makes it fail at lines 510, 522 or 529. Alone it passed 47 of 47 runs on 7 Nov; six runs at
  once failed 17 of 48. Run it alone.

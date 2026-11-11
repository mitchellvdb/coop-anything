#!/bin/sh
# Builds and runs the launcher's platform-independent tests on Linux (ASan/UBSan).
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
CC="gcc -std=c11 -D_GNU_SOURCE -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -Ilauncher/src"
CORE="launcher/src/lr_wire.c launcher/src/lr_sock.c launcher/src/lr_relay.c launcher/src/lr_files.c launcher/src/lr_core.c launcher/src/lr_dlc.c launcher/src/lr_mods.c launcher/src/lr_kit.c launcher/src/lr_restart.c launcher/src/lr_owners.c launcher/src/lr_roster.c launcher/src/lr_snapshot.c"
$CC -o build/test_lr_dlc launcher/tests/test_lr_dlc.c launcher/src/lr_dlc.c launcher/src/lr_files.c
$CC -o build/test_lr_logs launcher/tests/test_lr_logs.c launcher/src/lr_logs.c
$CC -o build/test_lr_restart launcher/tests/test_lr_restart.c launcher/src/lr_restart.c launcher/src/lr_wire.c
$CC -o build/test_lr_restart_sim launcher/tests/test_lr_restart_sim.c launcher/src/lr_restart.c launcher/src/lr_wire.c
$CC -o build/test_lr_wire launcher/tests/test_lr_wire.c launcher/src/lr_wire.c
$CC -o build/test_lr_core launcher/tests/test_lr_core.c $CORE
$CC -o build/test_lr_mods launcher/tests/test_lr_mods.c launcher/src/lr_mods.c launcher/src/lr_files.c launcher/src/lr_roster.c
$CC -o build/test_lr_kit launcher/tests/test_lr_kit.c launcher/src/lr_kit.c launcher/src/lr_mods.c launcher/src/lr_files.c launcher/src/lr_roster.c
$CC -o build/test_lr_roster launcher/tests/test_lr_roster.c launcher/src/lr_roster.c
$CC -o build/test_lr_owners launcher/tests/test_lr_owners.c launcher/src/lr_owners.c launcher/src/lr_roster.c
$CC -o build/test_lr_snapshot launcher/tests/test_lr_snapshot.c launcher/src/lr_snapshot.c launcher/src/lr_files.c launcher/src/lr_wire.c
setarch x86_64 -R build/test_lr_dlc
setarch x86_64 -R build/test_lr_logs
setarch x86_64 -R build/test_lr_restart
setarch x86_64 -R build/test_lr_restart_sim
setarch x86_64 -R build/test_lr_wire
setarch x86_64 -R build/test_lr_core
setarch x86_64 -R build/test_lr_roster
setarch x86_64 -R build/test_lr_kit
setarch x86_64 -R build/test_lr_mods
setarch x86_64 -R build/test_lr_owners
setarch x86_64 -R build/test_lr_snapshot

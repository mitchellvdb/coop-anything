# Updates from the host

Testers no longer download every new kit. When a friend whose DDCoop is older joins the host, the host's
launcher sends its own installed kit along; the friend's launcher checks it and asks the player:
"The host runs a newer DDCoop ... Update now?". Yes closes DDCoop, installs the new version through the
normal install path and offers to open it again. No server, no open port: it uses the existing Steam lobby
connection.

## Trust: signed by the owner

A launcher installs only a kit signed with the owner's key:

- `tools/sign_kit.ps1 -NewKey` created the key once (ECDSA P-256). The private key lives only in
  `%USERPROFILE%\DDCoop-signing\kit-signing.key`, encrypted with Windows DPAPI for the owner's Windows user.
  It is never in git or a kit. Anyone who can log in as that Windows user (including an agent working there)
  can sign kits.
- The public key is built into every launcher (`LR_KIT_PUBKEY` in `launcher/src/lr_hashes.h`).
- `tools/sign_kit.ps1 -Sign <kit folder> -Version 0.5.0-test77` writes `ddcoop-kit.sig` next to the kit's
  `DDCoop.exe`; `tools/make_dropkit.py` puts it in the zip and the installer copies it next to the installed
  `DDCoop.exe`.

`ddcoop-kit.sig` is a text manifest plus a signature:

    DDCoop kit 1
    version=0.5.0-test77
    serial=77
    kit=719b6e74
    file=DDCoop.exe <size> <sha256>
    file=SDL2.dll <size> <sha256>
    file=ddcoop-runtime.dll <size> <sha256>
    signature=<r||s, 128 hex>

The signature covers every byte before `signature=`. The parser (`src/lr_kit.c`) accepts exactly this layout.

## The flow

1. **At startup** a launcher loads its own kit (`load_own_kit` in `main.c`): the manifest next to
   `DDCoop.exe` must be signed by the owner, name this build (`DDC_VERSION`) and this runtime, and the running
   `DDCoop.exe`, `mods\DDCoop\SDL2-bootstrap.dll` and `mods\DDCoop\ddcoop-runtime.dll` must match it. Only
   then is there anything to offer. The log says which.
2. **A friend joins with another kit id** (the handshake compares the runtime hash prefix). Before the usual
   "versions differ" refusal, the host sends `LR_KIT_OFFER` (the manifest), `LR_KIT_CHUNK`s (the three files,
   16000 bytes each) and `LR_KIT_END`, then `LR_BYE` version, one friend at a time; the same friend is not
   offered again within 30 seconds. Launchers from before this change ignore the kit messages and see the
   usual refusal.
3. **The friend's launcher** (`guest_kit_message` in `lr_core.c`) takes the offer only if its own build
   accepts updates (serial > 0), the manifest parses, the serial is higher than its own, the signature is
   the owner's, and after `LR_KIT_END` every file matches the manifest. Anything else is logged and ignored;
   nothing is written.
4. **The window asks** (`offer_update`). On yes (`apply_update`): only with the game closed and no session or
   recovery pending, it writes the files to `mods\DDCoop\update\<version>\`, reads each back and checks it,
   starts that `DDCoop.exe --update` and closes.
5. **`DDCoop.exe --update`** waits up to 20 seconds for the old launcher to close and runs the normal install
   (`install(kit, repair)` in `lr_install.c`): the new launcher checks the new runtime and bootstrap against
   its own pins, installs them and its manifest, and offers to open DDCoop.

## Limits

- **The first update-capable kit (test77) must be installed by hand** on every PC. Updates start working from
  the next kit.
- The offer is made when the **runtime** differs (the kit id is the runtime hash prefix). A kit that changes
  only the launcher is not offered; players with the old and the new launcher can still play together.
- Only the host's kit is offered, and only if it is newer than the friend's. A friend with a newer kit sees
  the usual refusal; the host updates by hand (or joins that friend).
- A new **bootstrap** (`SDL2.dll`) would need the installer's known-predecessor handling first; the bootstrap
  has not changed since test12.
- **Virus scanners** still scan the new files. An update does not avoid a false "virus" alarm; see the kit
  README.
- Duo kits (several windows on one PC) do not take part (serial 0).

## Code and tests

`src/lr_kit.c` (manifest), `src/lr_kitsig_win.c` (CNG signature check), `src/lr_core.c` (`host_send_kit`,
`guest_kit_message`, `lr_core_update`), `src/main.c` (`load_own_kit`, `offer_update`, `apply_update`),
`src/lr_install.c` (`--update`, `install_manifest`), `tools/sign_kit.ps1`, `tools/make_dropkit.py`.
Tests: `tests/test_lr_kit.c` (signer's vector, every malformed field, truncations, 100,000 mutations),
`tests/test_lr_kitsig_win.c` (CNG: valid vector; every changed byte, other keys and the owner key refuse),
`tests/test_lr_wire.c` (the three messages), `tests/test_lr_core.c` (offer and verification end to end, with
backpressure; same/older serial, bad signature, corrupted chunk, host without kit, build without updates).
Not tested automatically: steps 4 and 5 against a real install (they change the installed game folder).

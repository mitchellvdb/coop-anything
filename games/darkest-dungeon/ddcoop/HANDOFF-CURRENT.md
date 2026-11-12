# Current: test85 (Claude Code, 12 Nov 2026): test84 plus the provisions Back fix

- Provisions Back (77b07f8): the other player's Back now really closes the shop (the Back button 6e84e0 instead of
  its after-hide callback 6e85e0, which never slid the panel away: queue stuck since 9 Nov), and the player's own
  Escape while that Back waits still leaves for town (it was swallowed: black screen, owner's run 12 Nov 17:06:43).
- Trinkets reported missing after a desync: not found in the logs. 16:48 stop was the Stagecoach pool; all guests'
  hero/trinket check matched. Shared town trinket moves do not trigger a save (the native drop does not call the
  writers directly either); open, needs a concrete case.
- 0.5.0-test85: C:\dev\ddcoop-handoff\0.5.0-test85\DDCoop-install.zip
  Kit SHA256 5648b0fb451970338202b59f47310bdd4556d104fab13b1cf59dc025114f15e4
  Runtime 721427250e527245adcac86535bdd75770f869d6635aa554ebeb06bb20ebbd35
  Launcher 89d535e283d649630a1ba23110b0d24a934c96e1d29a2ef2c61591cd45f3d8d7
  ddcoop-kit.sig (serial 85) verified with the launcher's code and the owner key; Defender: no threats.
- Validation: runtime tests exit 0, every suite 0 failures; test_provisions_layout.py and test_hook_layout.py pass
  on the pinned EXE; Windows launcher suites pass except test_lr_prepare_win (not run: the game was open); Linux
  ASan/UBSan suite passed. Not tested in a real session. No duo kit.
- Next: owner tests TEST-STEPS (Back/double Escape in the shop with 3-4 players, test84 checks). Codex: continue
  from hero-ownership.

# Current: test84 (Claude Code, 12 Nov 2026): player names, players connected, shared party order

- Player names (ce6d739 runtime, 1e128f3 launcher; launcher/NAMES.md): a name typed in the launcher is sent to the
  others (wire types 32/33) and shown in the hero table, the Assign list, a "x/4 players connected" line and the raid
  party blocks (overlay snapshot v2). Names never go into logs.
- Hand-made party order (8109085, protocol 37, runtime/FORMATION.md): the owner's stops of 12 Nov 16:19 and 16:21
  were a guest dragging heroes to other ranks outside a fight (one copy only). The runtime now notices a new order of
  the same members and shares it as FORMATION_SET; every copy produces it through the default-order button's native
  rerank with Raid::GetDefaultPartyOrder (614cb0) hooked during that call only.
- 0.5.0-test84: C:\dev\ddcoop-handoff\0.5.0-test84\DDCoop-install.zip
  Kit SHA256 12f664fbdf0b7e63c50427bdedf699c3af4cb11f94dd9efd22063f482dc799f6
  Runtime f95b0d08b8122bfcaa5d3b915bfb43a77c68027de2fbff442e81ddc34b376591
  Launcher 38a3b8b6749f83f1333db2d9347a0158082178cebaeb917e78b042107eeb5123
  ddcoop-kit.sig (serial 84) verified with the launcher's code and the owner key; Defender: no threats.
- Validation: runtime tests exit 0, every suite 0 failures; test_formation_layout.py and test_hook_layout.py (93
  hook targets) pass on the pinned EXE; overlay test 0 failures; Windows launcher suites pass except
  test_lr_prepare_win, which could not run (Darkest.exe was open); Linux ASan/UBSan suite passed. Not tested in a
  real session. No duo kit for test84.
- Next: owner tests TEST-STEPS (names, x/4 line, drag as host and guest). Codex: continue from hero-ownership
  (runtime changes in ce6d739 and 8109085 would be lost by importing an older source zip).

# Current: test83 (Claude Code, 12 Nov 2026): Codex's test81/test82 imported, three repairs

- Imported Codex's test81 (06c3709) and test82 (5d5138f) from his outputs/*/source-full.zip; both built on our
  test80. test81: battle choices wait for input, loot double clicks, shared inventory-full notice (protocol 36).
  test82: automatic diagnostics to the host (launcher/DIAGNOSTICS.md).
- Repairs (Claude Code): lr_wire.c builds with gcc -Werror again (2c41f07); confirm_* hooks are in
  tools/gen_address_table.py (5eb603a); no report after an ordinary session end, a disconnect before a
  "Sync stop" is not a desync (1931e8d, lr_diag_stop_marker).
- 0.5.0-test83: C:\dev\ddcoop-handoff\0.5.0-test83\DDCoop-install.zip
  Kit SHA256 6ad172edc15dab74925b24f1884a93e3c33c3dc05439fd9e52895167ae404a42
  Runtime 2be000695e28d7c0721ff1c0bc4d39422cf968943d023409403c3bc3ed1436d8 (test82 source, label test83)
  Launcher f418a983f6327bc88dfb9e90167a8b5c189d8fe8fdcd18b5b7be2181f8171b7c
  ddcoop-kit.sig (serial 83) verified with the launcher's code and the owner key; Defender: no threats.
- Validation: runtime tests 0 failures; test_full_notice_layout.py and test_town_save_layout.py pass; all Windows
  launcher suites passed (test_lr_prepare_win included, game closed); Linux ASan/UBSan suite passed. Not tested
  in a real session. No duo kit for test83 (Codex's duo61 stays the solo kit).

# Current: test82 / duo61 (Codex, 11 Nov 2026)

Automatic private diagnostics to the host, as requested by the owner. Native
Sync stop, resync/session events and Copy logs from any connected launcher
coalesce into one host-coordinated report. Desktop ZIP contains host/player
folders and REPORT/FILES status. Missing/failed/truncated data is explicit.
Fixed log allowlist, no saves/ddcoop.ini/signing key; known secrets removed at
sender and host. Names/paths may remain: reports are private, never published.
Runtime gameplay remains test81; only its exact hello label changes to82 so
old launchers receive the signed update before new diagnostic control types.
Runtime wire36/launcher wire3 retained; use matching new kits on every copy.

- 0.5.0-test82: C:\dev\ddcoop-handoff\0.5.0-test82\DDCoop-install.zip
  Kit SHA256 444690abc9d4a0f961f1552d8928c359501c3ddeae0a9c3b14b7ea6e5edc8b12
  Runtime fecf38a672c89b63f31e45202b09d7e0d523e8249626b32b391ceeb3c9ca326b
  Launcher ff9c0b19e544bd024e7c1d5c24be3a6bf81d9c7456a65392b07614009041bf6a
- 0.5.0-duo61: C:\dev\ddcoop-handoff\0.5.0-duo61\DDCoop-install.zip
  Kit SHA256 303309745be7e387a8365e527fbbe9de95f8a47a776152ab7b88fc9f143992bd
  Runtime 00eef369116d28af68d94a53ed5db1b848e0b303e5b69f6c78fda32f316e9575
  Launcher a2f16aa6421cb99233c349e1b583aff85b75751b51266fa9de36cf152f56203d

PASS full Windows runtime and both complete launcher suites, including snapshot/
preparation/restore and normal+solo actual Windows diagnostics file adapters.
PASS portable diagnostics/logpack MSVC ASan and real Windows adapter ASan;
independent Python ZIP reader validates CRCs, folders, missing participants,
allowlist and redaction. Four initiators/three receivers, simultaneous notices,
backpressure/CRC/offset/size/auth/stale/collection/disk failure, disconnect and
bounded deadlines covered. A lost receipt ACK overlapping the next report failed
before receipt history, then passed. /analyze has no new finding (existing main
unreachable-code warning); existing socket signedness build warnings remain.
Actual ordinary bundle signature+all3 hashes verified by launcher owner-key CNG.
Native hooks are unchanged, retaining test81's19 pinned read-only proofs.
No live Steam diagnostics transfer or installed game/save/system change was
performed. Linux/MinGW/Capstone not run. Owner next test: any guest Copy logs;
verify one combined ZIP on host and correct per-player files, then retain tests
for real desync/resync. Keep launchers open. Fully lost connection cannot send;
local logs remain. Post-stop diagnostic transport drains for <=60s without
blocking normal game close/recovery; active collection deadline120s. No public
log upload. Stagecoach/earlier curio/loot causes remain open as documented in81.
See launcher/DIAGNOSTICS.md and BUILD.md. Source-only GitHub step commits, no
.git ACL bypass. Shared checkouts unchanged; final archive/branch are authoritative.

## Previous state (retained history)

# Current: test81 / duo60 (Codex, 11 Nov 2026)

Built on Claude Code test80/local57a494b, retaining all intervening updates.
Inventory-full OK now follows every player using its causal loot order; exact
native empty callback/Entry verified. Combat same-turn animation waits for
input29/30. Native pending loot clicks are bounded and serialized. Added OPEN
and recruit-refusal diagnostics. Protocol36/exact runtime81; launcher wire3.
Read runtime/RECHECK-2026-11-11.md for evidence, scope and unresolved causes.

- 0.5.0-test81: C:\dev\ddcoop-handoff\0.5.0-test81\DDCoop-install.zip
  Kit SHA256 b82561d5dd2b45db4a77c7907bd578a4498a9337ce24f01b5a5ed6a5e2ce75e2
  Runtime 835d58030c99105ec1a7a63bde07f6c9fc9486c0a117c9ad52203431d1a549a3
  Launcher 578493360fd2c10d3effde988e76bbbde3892d423df836b0bbce9411f07cfcee
- 0.5.0-duo60: C:\dev\ddcoop-handoff\0.5.0-duo60\DDCoop-install.zip
  Kit SHA256 7b5811c30d0afa720d65a315619afe25c08497cac9dec202835c218cf2a78263
  Runtime 92ce93497998321a0c76dbeb425744a1e9e0ca20747a6fd82764934e7345a09f
  Launcher 487204ac352ff16d4fe7462545a559756cc6baac160a18777d3eed4d718709ac

PASS native/protocol/FIFO, final actual-bridge ASan,19 pinned native proofs,
dungeon foundation, both complete Windows launcher suites including isolated
backup/restore after real games closed. Signed ordinary manifest serial81 and
actual bundle hashes verified with launcher's owner-key CNG check. New live
playtest is pending. Native effects in fixtures are modeled. No real game/save/
Steam mutation. Stagecoach/earlier OPEN and some loot causes remain open.
Ordinary test81 is signed for existing update-capable launchers; duo60 is manual
and includes all three local guest startup batch files. Do not mix variants.
Source-only GitHub commits are on game/darkest-dungeon-ddcoop; no .git ACL bypass.

## Previous state (retained history)

# Current: test80 (Claude Code, 11 Nov 2026): test79 + local tutorial popups, curio double clicks

- **19:49:37 stop (owner, test78):** player 2 clicked an open curio three times. Each duplicate OPEN made the host
  send its Rules number (already past the curio's draws) before finding the overlay open; the guest took it over
  before its own curio drew, rolled differently, had no loot window, and the host's Take all failed there after
  30 s. Now a duplicate/refused OPEN returns before walk_rules_send (2135ec0).
- **20:04:12 stop:** player 2 had a tutorial popup the host did not; the shared close was refused four times while
  the host waited for that window, then Resync. Tutorial popups now close locally on each copy; an old TUTORIAL
  order closes an open popup or does nothing.
- **Skills:** player 2 sent skill toggles at 19:36:52-19:37:01 (hero 1078); one was applied, five refused by the
  host's precheck. The host now logs which value differed. Player 2's own logs were not sent.

- 0.5.0-test80: C:\dev\ddcoop-handoff\0.5.0-test80\DDCoop-install.zip
  Kit SHA256 63133acf3f00f4782ad672139a4c98a3ca74c26af94b31881e1b25cf6de05ea3
  Runtime a031e1a2a22e05029d5eeab5408d47827d828a56b02754b042b46109ca61fce1
  Launcher 6c91d5043ffb7f48c2f6ab6bfdf4a2d639f3ca4f2d160d0c21eb95ba2cf9d4b5
  ddcoop-kit.sig (serial 80) verified with the launcher's code and the owner key; Defender: no threats.
- Validation: runtime tests 0 failures; launcher suites passed except test_lr_prepare_win (refused: the owner's
  game was running). Not tested in a real session. test79 was built but superseded before the owner installed it.

## test79 (Claude Code, 11 Nov 2026): Tavern/Abbey and skill choices saved

- **Finding (owner, test77/78):** Tavern/Abbey treatments done before a restart were gone, the gold stayed spent.
  The host's save (profile_5, read only) had every Tavern/Abbey slot empty, also for the test78 session;
  persist.town.json was last written at 19:25 (Stagecoach recruit), estate later. The game's accept button
  (68b080) and red X (68c320) call the roster/town/raid writers after the native change; the shared path did
  not (the assign only notifies the wallet). The sheet's skill toggles (805208) also write the roster.
- **test79 (ab2862e, runtime 4b22351f):** every copy calls the same writers after a shared placement, removal
  or skill toggle (guests write nothing with -nosave). Proof: tests/test_town_save_layout.py. Guest skill
  clicks that stop before the host now log the reason or the unshared caller: no guest skill request appears
  in any log so far, so the owner's "only the host can choose skills" is not explained yet.
- **Update via the host works:** in the owner's 19:28 session both test77 friends were offered test78, left
  and rejoined 11 s later with the new kit.
- Open: Stagecoach "candidate content mismatch" at 19:23:16 and 19:24:43 (test77, right after resyncs) not
  analysed yet. The test78 wallet logging has not seen a raid yet.

- 0.5.0-test79: C:\dev\ddcoop-handoff\0.5.0-test79\DDCoop-install.zip
  Kit SHA256 d4a6ad5e2d228bbbc43ec3face717e0fd6e02e31390af1bf33b96db8859ca394
  Runtime 4b22351f1eaa444961a3263d8f32d0cbfbb098dcf3e60d12aac4d47d19e86b14
  Launcher e986121cfd357b214748152583761a2d1eb71a3e31454d9b5b8f675b88686bb7
  ddcoop-kit.sig (serial 79) verified with the launcher's code and the owner key; Defender: no threats.
- Validation: runtime tests 0 failures; launcher suites passed except test_lr_prepare_win (refused: the owner's
  game was running). Not tested in a real session. No duo kit for test79.

## test78 (Claude Code, 11 Nov 2026): after-raid currencies per kind; first update via the host

- **Finding (owner test77, 19:18):** after a raid the guest stopped with "Town: differs ... currencies
  df398e40/c2891da7, heroes equal". Both game logs show identical loot rolls (gold included), heirloom
  conversions, quest rewards and town entry; the week Rules and guid counter were shared correctly. The old
  digest hashed the native wallet vector in its order including zero currencies and logged no amounts.
- **test78 (4120b3e, runtime b6a63a34):** the check sorts currencies by kind, skips empty ones, refuses a
  duplicate kind, and logs "Town: wallet here <kind>=<amount> ..." (host every check, guest on a mismatch). If
  it recurs, those two lines name the currency. Not a proven fix: the cause is not identified yet.
- **First real update test:** only the host installs test78 by hand; friends on test77 are offered it.

- 0.5.0-test78: C:\dev\ddcoop-handoff\0.5.0-test78\DDCoop-install.zip
  Kit SHA256 1117fad94a2515c64c10acb0a6003873911983c9c4dabdbc8b31b8f7df42ac62
  Runtime b6a63a34ef5b9ab2a2b3e296a0605e5966a9c27555481b1d7c32855eb6765526
  Launcher 19333028efbca1796b9298858086f868e79e0ebe9bfa6a241e628b5fb9d183aa
  ddcoop-kit.sig (serial 78) verified with the launcher's code and the owner key.
- Validation: runtime tests 0 failures; launcher suites passed except test_lr_prepare_win, which refused because
  the owner's game was running (launcher code unchanged since test77, where it passed); Defender: no threats.
  No duo kit for test78.

## test77 (Claude Code, 11 Nov 2026): updates from the host; gameplay as test76

test77 adds signed kit updates to the launcher (launcher/UPDATE.md, commits 9b5483d, 4217a5b): a friend with an
older kit is offered the host's own installed kit and can install it with one click. Only kits signed with the
owner's key (tools/sign_kit.ps1; private key DPAPI-encrypted in %USERPROFILE%\DDCoop-signing, public key
LR_KIT_PUBKEY) are accepted. test77 must be installed by hand once; it uses the test76 runtime (719b6e74), so
test76 and test77 players play together. Offers start working with the next kit that changes the runtime.

- 0.5.0-test77: C:\dev\ddcoop-handoff\0.5.0-test77\DDCoop-install.zip
  Kit SHA256 22acda7f382d09272280165aa29c2a078bc7ecf04f4aa2813f8fcce2e46b41a9
  Runtime 719b6e74b13b25826ae764aff36f8e420e258c839ed8f1df6ad5289be7c89db6 (same as test76)
  Launcher 57f6a71e9fa677731dffdec9f8a9278318da40d85c240fb8b23525e3a2e371dd
  Signed manifest ddcoop-kit.sig (serial 77) checked with the launcher's own code and key: valid.
- Validation: launcher Windows suites (incl. test_lr_kit, test_lr_kitsig_win, core update scenario,
  test_lr_prepare_win) and Linux ASan/UBSan suite all passed; Defender finds no threat in the kit. Not tested:
  installing an update against a real install (first possible with the next kit). duo59 stays the duo kit.
- Every later test kit: build, then `tools/sign_kit.ps1 -Sign <kit folder> -Version 0.5.0-testNN` before
  tools/make_dropkit.py, so the zip carries ddcoop-kit.sig.

## test76 / duo59 (Claude Code, 11 Nov 2026, on top of Codex's test75)

The owner asked Claude Code to fix the two findings from the 11 Nov sessions while Codex was unavailable.
Runtime code is Codex's area (AGENTS.md section 3); these are exceptions at the owner's explicit request, each
in its own commit. Codex: please review 0117582 and 5dc63f3.

- **Camp Leave together (0117582).** 70b7c0 (Leave) returns at once while camp overlay+0x24 is 0; the overlay's
  frame update 70a1e0 holds +0x24 at 0 while another panel covers the camp. A shared Leave was acknowledged
  on a covered copy that never left camp (the guest then fought alone; the tutorial popup the host could not
  close was a symptom). The Leave order now waits at the FIFO head until +0x24 is set (up to 5 minutes, with
  a notice to close the open window); the apply refuses a Leave that could not start.
- **Same Stagecoach ids after a raid (5dc63f3).** Campaign+0x58 is nextGuid. New heroes take guid =
  nextGuid++, and so do listener handles that screens like the roster panel (6b48d0) register whenever a
  player opens them, so every copy's counter drifts with its own UI use (host 1-2 ahead in the logs; even the
  fight's monsters had other guids). The test75 RaidFinish boundary now also carries the counter: the host
  announces its own value + 1024 in DD_MSG_WEEKLY (body 12 bytes) and every copy sets campaign+0x58 to it
  right before the native RaidFinish. A copy whose counter is already higher refuses (never moved down: a
  live handle could be reused). Protocol 35, runtime version 0.5.0-test76.
- New read-only proof tests/test_camp_guid_layout.py pins both assumptions in the exe.

test75 (Codex) is unchanged underneath: Stagecoach pool comparison after load/raid, content fingerprint per
recruit, shared week Rules at RaidFinish, stricter campaign mod check (runtime/STAGECOACH-WEEK.md,
launcher/MODS.md).

Validation (Claude Code, every Darkest process closed): runtime\test-windows-msvc.cmd 0 failures (new camp
and guid regressions included); all 17 tests/test_*_layout.py proofs PASS against the installed exe; launcher
Windows suites passed for both the test76 and the duo59 build, including test_lr_prepare_win. Launcher code
is unchanged since test75 except the runtime pin (Linux suite last run on test75). Not tested: a real game
session.

- 0.5.0-test76: C:\dev\ddcoop-handoff\0.5.0-test76\DDCoop-install.zip
  Kit SHA256 33351744cf883e9284396ff7242b2300f45927128569db8bf6724b98f8f20bcc
  Runtime 719b6e74b13b25826ae764aff36f8e420e258c839ed8f1df6ad5289be7c89db6
  Launcher 9e0af7c1600708cfe093b1aa9ea93d7d81e9c407712654d8c7987247bcdd8949
- 0.5.0-duo59: C:\dev\ddcoop-handoff\0.5.0-duo59\DDCoop-install.zip
  Kit SHA256 21449f0fa488dbab36f75707492b14d9283b90c16def10fb631fcb5228997805
  Runtime 5fbf47a9983027a79346acbc31e9e9f7f5b744c7bae8d7e14a59a98f3a5daae1
  Launcher cea1e2d360099c1584c0f832781f10d461ff06e09804a7dec88f04db651bb58c
  (duo59 differs from git only in launcher/src/lr_hashes.h and TESTSTAPPEN-test76.txt; its source-full.zip
  holds both.)

Next owner check (TESTSTAPPEN-test76.txt): camp once with one player covering the camp while the other
leaves; after a mission compare the new Stagecoach on every copy and recruit from different players.

Open: other in-week UI-driven guid use (e.g. a hero created outside RaidFinish) is not shared; live DLC,
direct loot-to-hero equip, complete state checks, older provisions crash. Unchanged rules: guests -nosave, host
save checkbox, selected slots 1..9, profile_9 forbidden, no Circus/PlayFab path.

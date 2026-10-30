# Darkest Dungeon: notes for the next agent

Darkest Dungeon (Red Hook Studios, 2016), Steam, Windows. Work started 2026-10-30.
Goal: host-authoritative co-op where each friend owns heroes in the party.

## Game identity

- Store: Steam (app 262060). Platform tested: Windows. Base game only, no DLC: co-op targets
  base-game combat first. DLC support comes later, tested on legitimately owned DLC.
- Build id: unknown (appmanifest not supplied). Exe link time 2026-09-21 (PE header).
- Version: unknown.
- SHA-256 `_windows/win64/Darkest.exe` (x64): `4d78fbfaa65b7d002d2208d02da809200f4a5723119521180e30100d6f1a75f2`
- SHA-256 `_windows/win32/Darkest.exe` (x86): `4ac994d6ea0550c425ce37e096416fb67e047fce717e5c7369c01c49549966fe`
- The install also ships `_windowsnosteam/` (a different exe hash). Work from `_windows/win64` only.
- Engine: in-house C++ (MSVC, VCRUNTIME140), SDL2 + OpenGL, FMOD Studio audio, Spine animation
  (`SpineDataFactory`), PlayFab SDK + libcurl + OpenSSL. RTTI is present: 7521 `.?AV` class names.
- The `dlc/` folder holds five DLC dirs left over from Steam Family Sharing that are no longer
  licensed. Base game only; leave `dlc/` alone. The Butcher's Circus (`1117860_arena_mp`) is free on Steam.

## Observations (checked, with source)

- No co-op mod found in the coop-anything directory, `games/`, issues or PRs (searched 2026-10-30).
- The only shipped multiplayer is the Butcher's Circus DLC versus mode, which is PvP, not co-op.
  [PlayCo-op](https://playco-opgame.com/darkest-dungeon/)
- The only "co-op" play reported is streamers passing one controller between friends.
  [GamesBeat](https://gamesbeat.com/twitch-broadcaster-turns-single-player-darkest-dungeon-into-a-co-op-game/)

- The exe contains multiplayer netcode from the Butcher's Circus PvP mode (RTTI names):
  `Network::PlayFabAndSteam::PlayFabAndSteamP2PManagement`, `Network::PlayFabAndRUDP::PlayFabAndRUDPManagement`,
  `Network::Steam::P2PSessionProcessing`, `Network::MultiplayerAPI::EMatchmakingState` (PlayFab matchmaking),
  `ReceivedExternalTurnMetaDataInSimpleMPEvent`, `ExternalSubsystemMultiplayerMessageReceivedEvent`,
  `Darkest::EnableMultiplayer`, `Battle::MultiplayerPartyRetreat`.
- `Battle::GetNetworkAISelectSkill` exists beside `Battle::GetMonsterAISelectSkill`.
- Combat code takes the RNG as an explicit parameter: `prng_minstd&` (also `prng_minstd_wrapped`),
  e.g. `Actor::ApplyHealthDamageInternal(prng_minstd&, ...)`, `Actor::QueueHeartAttack(prng_minstd&, bool)`.
- The monster AI is split into desires: `AI::MonsterSkillSelectionDesire`, `AI::MonsterTargetSelectionDesire`,
  `AI::MonsterBonusInitiativeDesire`.
- `scripts/*.darkest` and `*.json` are config data, not a script language.
- x64 exe imports include `WINMM.dll`, `dbghelp.dll`, `WINHTTP.dll` and `IPHLPAPI.DLL` (proxy DLL candidates); there is no `version.dll`.
- PvP reports matches to PlayFab: `PlayFabAndSteamP2PManagement::ReportSuspiciousMatch`.

## Combat turn input (Ghidra 12.2, x64 exe sha256 4d78fbfa...; addresses valid for that build only)

Found by decompiling the vftable xrefs of RTTI lambda classes (script approach: find `vftable`
symbols whose namespace contains the owner name, decompile the functions that write them).

- `0x1407598d0`: the combat state machine. In state `0x1d` the current actor chooses an action.
  - The actor is not a player hero (`actor->vfunc[+0x20]()` returns 0):
    - if raid type == `"arena_mp"` and a PvP session is up → `Battle::GetNetworkAISelectSkill(bool& wait)` = `0x1405a0af0`;
    - else → `Battle::GetMonsterAISelectSkill()` = `0x14059fe80`.
  - The actor is a player hero: reads the skill index the UI selected at `battle+0x9f7*8` (`-1` = nothing
    chosen yet, the turn waits), then validates skill + target (`0x1405a7d50`).
- Both AI functions call the selector `0x140592f80(..., char network, char* wait)`.
  - `network == 0`: monster desires roll a skill ("MonsterBrain" log lines).
  - `network == 1`: no roll. It reads a mailbox byte at `brain+0x118`: `0x80` = empty → `*wait = 1`;
    `-1` = pass; a valid index = use it, then reset the byte to `0x80`; an invalid index = "INVALID SKILL!" + report.
- `0x14035f4a0`: on an invalid skill/target it calls `ReportSuspiciousMatch` on the active PlayFab session
  (Steam P2P or RUDP flavour). It does this only if a PvP session pointer (`DAT_14117d038` / `DAT_14117d030`) is set.
- `Raid::GameControlMessageProcessor` listener for `ReceivedExternalTurnMetaDataInSimpleMPEvent`
  (`0x14060ab30`) only logs "Received message from external subsystem. Checking victory conditions."
  and checks victory. It does not carry the skill choice.

## Controller input (same build)

- The exe imports 61 SDL2 functions from the loose `SDL2.dll` (proxy candidate), including
  `SDL_GameControllerOpen/GetButton/GetAxis/Update`, `SDL_NumJoysticks`, `SDL_PollEvent`.
- Controller events are switched off (`SDL_GameControllerEventState(SDL_IGNORE)`, error string in exe).
  Controllers are polled instead.
- `0x14028a400`: the controller manager. It loops `SDL_NumJoysticks()`, opens every
  `SDL_IsGameController` device into the handle array `0x14117ceb0` (count `DAT_14117d018`), and each
  frame calls `SDL_GameControllerUpdate()` then reads buttons 0-4,6-14 and axes 0-5 of **every** handle,
  merged into one input. It also sets a flag "more than one joystick" (`+0x2405`).
- So several controllers already drive the game at once. Each has its own handle, which lets a proxy
  tell players apart.

## Randomness and determinism (same build)

- PvP is lockstep with a shared seed. Strings: "Written seedHalf: %d", "Received seedHalf: %d %d",
  "Not enough received data to init seed half for match init.", "hasOtherParty && hasRNGSeed", "RNGSeed", "OtherSeed".
- A per-PRNG debug trace exists: "PID: %d Name: %s Random int range A,B: %d , %d Value: %d Seed: %d"
  (also bool/float/normal/uniform, "Reseed seed %d", "Setting random seed: %d (0x%x)"). PRNG instances are named.
  The switch that enables it is unknown. It would make desync hunting easy.
- Gameplay RNG is `prng_minstd` (vftable in exe, default seed 0x17fa1), passed explicitly into combat code.
- `rand()` has one caller, `0x1404e4770`, which looks like a static initializer filling the global
  table names "entries"/"chances"/"chance_sum" (caller 0x1400a4260). Probably not gameplay; unverified.
- `rand_s` is used only for seeding: arg parsing (0x1403cb6d0), PvP match init (0x1403d1a70), roster
  load (0x14054c9d0), unknown (0x140562fb0), and the map generator.
- Map generation `0x1405e65b0`: local prng_minstd seeded with `rand_s() & 0x7fffffff` (0 → 0x17fa1),
  then the generator `0x1405e40f0`. For `arena_mp` the layout comes from the match instead (`0x1405e3ea0`).
  Co-op: the host's seed replaces that one rand_s result on every copy.
- Also imported: `std::_Random_device` (2 callers) and time sources (QPC, timeGetTime, SDL_GetTicks, _time64).
  Not yet checked whether any of them feeds gameplay.

## Town actions (same build; found via the vftable writes of event classes)

The game has an event per town action. Classes: `HeroWeaponAndArmourUpgradedEvent`, `HeroSkillUpgradedEvent`,
`HeroRecruitedEvent`, `HeroDismissedEvent`, `Building::ResidentOccupiesActivityEvent`,
`ActivityOpenedEvent`, `ActivityFinishedEvent`, `Negative/PositiveQuirkTreatedEvent`, `QuirkLockedEvent`,
`BuildingUpgradedEvent`, `BuildingStateChangedEvent`, `EstateCurrencyChangeEvent`,
`Pre/PostOnStartTownVisitEvent`, `TownEventOnStartTownVisitEvent`.

| Action | Function | Level | Confidence |
|---|---|---|---|
| Blacksmith weapon/armour upgrade | `0x14065e720` (caller `0x14065edc0`) | UI handler | fires the event; body not read yet |
| Guild skill upgrade | `0x1406775e0`, `0x140666f10` (no direct callers = button callbacks) | UI handler | fires the event |
| Stagecoach recruit | `0x1406b7860`, `0x1406c42e0` (sound "/town/stage_coach_purchase") | UI handler | fires the event |
| Dismiss hero | `0x1406a8300` (caller `0x1407eb740`) | UI handler | fires the event |
| Building upgrade | `0x140589e00(this, int* building, ...)` (caller `0x1406c8e60`) | model | fires the event |
| Currency add / deduct | `0x14055e910(this, type, amount)` / `0x14055ea10(this, type, amount)`, 12 callers each; deduct logs "Deducted more currency ... then had in wallet" | model | strong |
| Sanitarium effects | `0x140588290` (positive), `0x140588520`/`0x140588850` (negative), `0x140588bd0` (lock), all from `0x14052f180` | model, resolved on the next town visit | fires the events |
| Activity resolution with RNG | `0x140587ae0` (logs PRNG draws), caller `0x14054d3e0` | model, start of town visit | medium |
| Activity slot object | ctor `0x14052b210` (`Building::Activity::Base`), listens to `ResidentOccupiesActivityEvent` + `HeroDeathEvent` (`0x14052b5c0`) | model | strong |
| Place hero in building | `0x14052c4b0` AssignHero(activity, result*, heroRosterId), slot index pre-set in `activity+0x90`; UI lock-in handler `0x14067ece0` | model | **strong (runtime tests 2+3)** |

- The UI handlers do the change inline (set level, deduct currency) and then call the recompute helpers:
  `0x1405c9da0` rebuilds the weapon/armour stats ("%s.weapon"/"%s.armour"), and `0x1405c9350`/`0x1405c98e0` rebuild skills.
- Per-subsystem save serializers: roster `0x14057be70` (`persist.roster.json`, "heroes", "dismissed_hero_count"),
  town `0x1405877e0` (`persist.town.json`, "buildings", "districts"), estate load `0x14055da20`.
- `0x14054c950` = `Campaign::Current()` (464 callers).

## Stress buildings, reload, tampering (same build)

- Activity subclasses: `Bar`, `Gambling`, `Brothel` (tavern); `Meditation`, `Prayer`, `Flagellation` (abbey);
  quirk/disease treatment (sanitarium). Side effects: `GoMissing`, `AddQuirk`, `AddTrinket`, `ChangeCurrency`,
  `ActivityLock`, `ApplyBuff`. Requirements: `IsDiseasedRequirement`, `NotHaveQuirks`.
- `Building::Activity::Base` vftable `0x140e8c9b8`: slots 7-15 are pure virtual; slot 2/3 save/load
  ("resident_occupied", "caretaker_occupied", "visitsRemaining"). `Bar` vftable `0x140e8ef70`; slot 14
  `0x140535110(this, slotIndex)` resolves a slot (rolls the stress heal on named PRNG `0x142acb850`).
- `ResidentOccupiesActivityEvent` is NOT player placement. It is fired only by `0x140587ae0` (start of town
  visit), which randomly picks slots that an NPC resident blocks this week (named PRNG `0x142acb850`).
- Stress relief UI: `TownUI::Panel::HeroStressReliefDisplay` (vftable `0x140eae588`); slot 14 `0x14067c0f0`
  builds the slot widgets (ctor `0x1406808d0`, 0x358 bytes).
- Save = 26 `persist.*.json` files. Town-relevant: roster, roster.network, estate, town, upgrades, town_event,
  party_builder, quest.
- `Darkest::LoadCampaign` `0x1403d8590` (steps: SetSaveRestoreDirectory, Restore, Campaign::Load,
  Upgrade::Load, NoveltyTracker::Create, LoadDisplay) → `0x14054c9d0` sub-loads (estate `0x14055da20`, roster
  `0x14057b260`/`0x14057b810`/`0x14057c620`, town `0x1405875c0`, ...). It runs only in app state
  `as_load_campaign` (`0x1403db460`), after `TamperingManager::StartCampaign` and `CreateCampaignSystems`.
- App states: `as_town_visit_start/update/finish`, `as_raid_load`, `as_raid_start/update/finish`,
  `as_raid_results_*`, `as_raid_result_to_town_transition`, `as_load_campaign`, `as_unload_campaign`,
  `as_restart`, `as_fe_flow_*`, ... The normal loop never reloads the campaign. unload → load (as from the
  menu) is the only full reload path.
- No code reloads a single subsystem mid-session. Whether the open UI survives that is unknown.
- `TamperingManager` detects edited save files ("Tampered file found : %s", foundLocalTamperedFile,
  foundGlobalTamperedFile, listtamperedfiles, was_endless_wave_highscore_tampered). Unknown whether a copied,
  unedited host save counts as tampered. Never bypass it (rule 10, it guards online leaderboards).

## Runtime test 1 (owner's PC, test copy, probe v1, 2026-10-30)

- A proxy `SDL2.dll` (845 forwarders to `SDL2_orig.dll`) loads and runs: two launches, ~20 min, 50 game
  threads with hardware breakpoints, no crash. Exe hash matched.
- 8 activities found by vftable scan, 3 slots each: 2x QuirkTreatment, Flagellation, Brothel, Prayer,
  Meditation, Bar, Gambling (`StressRelief` is only an intermediate class).
- Dragging a hero IN did not change slot+0 (int). Writes of 0 to slot+0 came only when the panel opened
  (from the ctor below) and on drag OUT (via the drag-and-drop code `0x140287e0e` / `0x14028899c`).
- `0x1407d4ee0(this, activity, slotIndex)`: ctor of `ActivityHeroSlotInterface` (bases `DragContext`,
  `DropContext`, `HeroSlotInterface`), the UI drop target per slot; it is built by the panel `0x14067c0f0`.
- `0x1407d5510(this, hero)`: the slot widget's "set hero" (drop/remove).
  - hero != 0: calls `activity->vfunc[+0x38](slotIndex, *(int*)(hero + 0x130c))`; for `Bar` that slot is an
    empty stub, so the model registration happens elsewhere.
  - hero == 0: looks the old hero up in the roster (`Campaign::Current()+0x20` vector, entry id at +0); if
    `entry+0x1540 != 1` → `0x1405817c0(roster, entry, 0)`; clears `entry+0x1550`, `entry+0x1554`; then sets
    slot qword +0 = 0 and slot byte +0x14 = 0.
- `Campaign::Current()` returns global `g_pCurrent` = `0x14117d9b8` (`+0x728` when `0x142a5c08b` is set).
- Tooling lesson: a stack-trace address is mid-function; decompile the containing function. Creating a
  function there splits the real one (fixed: DecompileAt now uses the containing function + body fixup).

## Runtime test 2 (probe v2, 2026-10-30): hero placement solved

- Dragging a hero into a slot is a UI preview only (drop handler `0x1407d5510` resets slot +0 / +0x10).
  The game commits on the explicit **lock-in**; closing the window without locking commits nothing
  (corrected by runtime test 3: placed, not locked, closed → no AssignHero call).
- Slot (0x18 bytes, vector at `activity+0x78`): `+0x0` int hero roster id, `+0x4` int occupied (1),
  `+0x8` qword from `vfunc[+0x68](slot)`, `+0x10` int (-1 when free in the UI), `+0x14` byte.
- Roster entry: `+0x0` id, `+0x1540` state (1 = idle in town, 2 = busy in a building), `+0x1550` qword =
  pointer to the activity it is in. `Roster: 0 heroes` when quitting = campaign unload.
- `0x14052c4b0(activity, result[0x28], heroRosterId)` = assign a hero (activity namespace range):
  1. `0x14052c240` checks the requirements into `result` (ok flag at +0, failure vector +8..+0x10, flags +0x20/+0x21);
  2. `vfunc[+0x30](this, &costs, heroId, slot)` builds the cost list (0x48-byte entries: amount at +0,
     currency type at +0x44); if `0x14052c850(this)` each one is deducted with `0x14055ea10(campaign+0xe50, type, amount)`
     (`campaign+0xe50` = estate wallet), then `0x14055dda0(wallet)`;
  3. hero: `0x1405817c0(roster, entry, 2)` (state → busy), `entry+0x1550 = activity`;
  4. slot[`activity+0x90`] = {heroId, 1, vfunc[+0x68]}; `vfunc[+0x50](this, slot, heroId)`; `activity+0x90 = -1`.
  Callers: UI lock-in `0x14067ece0` (sets `activity+0x90` from the widget, hero id from `hero+0x130c`) and `0x140532dc0` (not read).

## Town actions v2: model functions (static, to be confirmed by runtime test 3)

| Action | Function | Payload | Notes |
|---|---|---|---|
| Buy upgrade (blacksmith weapon/armour, guild skills, buildings, probably camping skills) | `0x140590da0(request*)` → 1 on success | 12 bytes `{u32 hash("class.item"), u8 level char, u32 heroId or 0}` (**confirmed, test 3**) | checks prerequisites (`campaign+0x1808`) and cost against the wallet list `campaign+0xe70`, pays, `0x14055dda0(campaign+0xe50)`, `0x140550730`, `0x1405907f0`, notifies upgrade listeners (`vfunc+0x10(listener, request)`); purchased-set map `DAT_142b54640` keyed by the request |
| Upgrade lookup | `0x140471f40(treeId, code)` | | tree table `DAT_142c29fa0` (0x70 per tree, id at +0x40, levels map at +0x60 keyed by byte) |
| Upgrade UI commit | `0x1406c8e60(ctx, code)` (relay `0x1406ca3c0`); request = {ctx+0x90, code, ctx+0x94} | | tree type by id: building `DAT_142a70a74` (→ `0x140589e00(campaign+0x1138, ...)` + narration), trainer `DAT_142a70728`/`DAT_142a70a78`, armour `DAT_142a70bd8`, weapon `DAT_142a70724`; sounds only |
| Blacksmith refresh | `0x14065e720` (relay `0x14065edc0`) | | `0x1405c9da0` recompute gear, fires `HeroWeaponAndArmourUpgradedEvent{hero+0x130c, hero+0x12bc weapon lvl, hero+0x12c0 armour lvl}` |
| Assign hero to building | `0x14052c4b0(activity, result, heroId)` | slot in `activity+0x90` | confirmed by runtime test 2 |
| Dismiss hero | `0x1406a8300(this, heroId)` | hero id | only fires `HeroDismissedEvent{heroId}`; the roster removes the hero in a listener |
| Recruit hero | `0x1406b7860(storePanel*, hero*, index)` | hero id (`hero+0x130c`) | fires `HeroRecruitedEvent{heroId}` via `0x1406b9120`; then clears the candidate flag (`+0x1538`, stagecoach records of 0x1540 at store+0x68), increments `campaign+0x1708`, saves roster/town. Recruiting is free |

- Event vftables: `HeroRecruitedEvent` `0x140e6bfa8`, `HeroDismissedEvent` `0x140e6c028`.
- Recruit and dismiss are event-driven: the event is the command. Co-op must also mirror the recruit side
  effects (candidate flag, counter); otherwise two players could recruit the same stagecoach hero.
- Not mapped yet: trinket equip/unequip, provisions at embark, party selection, sanitarium disease/quirk
  purchase (probably the activity path like the tavern), camping-skill purchase (probably the upgrade path).

## Runtime test 3 (probe v3, execute breakpoints, 2026-10-30): town actions confirmed

- Execute breakpoints on four functions ran through town, provisions, a full expedition and back with no
  problems (set the resume flag `EFlags |= 0x10000` in the handler).
- **Upgrade request decoded:** `{u32 tree, u8 code, u32 owner}`.
  - `tree` = `h = h*53 + byte` (uint32) over `"<class>.<item>"` (`0x1405c9350`, debug line "Debug Hash
    Output"). Verified: `crusader.weapon` = 0x3fb36379, `crusader.armour` = 0x218a59ad, `crusader.smite` = 0x090daa75,
    all three seen in the log for the owner's starting crusader.
  - `code` = level as a character: '0', '1' for hero gear/skills; 'a' for building upgrades.
  - `owner` = hero roster id (1 = the starting crusader) for hero trees; 0 for buildings.
  - Costs seen: weapon/armour level 0 → 750 gold each, smite level 1 → 250 gold; building upgrades cost heirlooms only.
  - Lookup per hero: `0x140591270(treeHash, heroId, 1)`.
- Recruit: `HeroRecruitedEvent{heroId}` from `0x1406b791f` (inside `0x1406b7860`) and also from `0x1406c4413`
  (inside `0x1406c42e0`, a second stagecoach path; fired 9 s after returning to the hamlet, owner thinks it
  was a town event). The roster grows right after. Sync hooks the event, so both paths are covered.
- Dismiss: `0x1406a8300(this, heroId)` from the roster panel (`0x1407eb958`); the roster shrinks right after.
- Assign hero is also called at the start of a town visit by `0x140532dc0` (from `0x140532fbf`): heroes that
  stay in a building get re-seated. Under the reload design that is host-only.
- Hero state at `entry+0x1540`: 0 right after recruit (and after leaving a building on return), 1 = idle in
  town, 2 = busy in a building.
- Wallet `campaign+0xe70`: 8 currencies; types are hashes (gold = 15651954 here; the heirlooms are others).
- Provisions purchase and heirloom exchange (seen in the log without buy_upgrade) are mapped statically below.

## Provisions and heirloom exchange (static, same build)

Found via the sound strings their commit handlers play (`str:` mode of DecompileAt).

| Action | Function | Payload | Model effect |
|---|---|---|---|
| Heirloom exchange (confirm) | `0x14067a550(ctx, ...)` (caller `0x14067a890`), sound "/ui/town/heirloom_exchange_confirm" | `{srcType, srcAmount, dstType, dstAmount}` | `0x14055ea10(wallet, srcType, srcAmount)` then `0x14055e910(wallet, dstType, dstAmount)` then `0x14055dda0(wallet)`; the rest is UI bookkeeping. Rates come from `heirloom_exchange.json` (ctx[4]: 16-byte entries {type, divisor, ...}) |
| Buy provisions | `0x14068a2b0(ctx, storeSlot)` (caller `0x14068b710`; ctx[1] = store inventory, ctx[2] = expedition inventory) | `{storeSlot}` (+ count from the stack) | room check `0x1405d4980` ("not_enough_room_confirm_provisions"); price `0x140571fd0(DAT_14117d978, item)` gold and `0x140572080` second currency (shards); `0x14055ea10(campaign+0xe50, gold DAT_142ba44f0, price*n)` and `(…, DAT_142ba4360, shards*n)`; move items `0x1405d4910(store, slot, n, expeditionInventory)` |
| Return / sell provisions | `0x1407df430(itemStack*, n)` (callers `0x140688710`, `0x14068a680`, `0x14068b510`, `0x1407e2e10`) | `{expeditionSlot, n}` | refund `0x14055e910(campaign+0xe50, DAT_142c02ae0, price*n)` (+ shards `DAT_142c02950`); put back into the provision store `0x1405d40c0(campaign+0x1618, item, 1)`; stack −n, removed when empty (`0x1405d2ed0`); "item_cant_sell_confirm" otherwise |

- The currency deduct `0x14055ea10` does NOT refuse a too-large amount; it only logs "Deducted more currency…
  then had in wallet". The exchange handler relies on the UI for limits, so in co-op the host must check the
  balance and the rate itself before executing a guest's exchange.
- The provision store stock lives in `campaign+0x1618`. It is generated per visit, so it arrives with the
  host's save at the start of the visit.
- Other stores: `0x14069b400` (sell, add currency; callers `0x1406a2090`, `0x1406a20f0`, `0x14069aeb0`) is probably
  the nomad wagon (trinkets); `0x140674160` only picks the buy sound ("/ui/town/buy", "buy_free", "buy_shard").
- Not runtime-confirmed yet (a probe v4 could add these to the execute breakpoints).

## Trinkets, skill selection, party (static leads, runtime test 4 pending)

- Classes: `EquipmentSlotInterface`, `EquipmentDragObject`, `TrinketDropAreaHeroInterface` (vftables
  `0x140eb4aa0`, `0x140eb51b8`), `TrinketDropAreaInventoryInterface` (`0x140eb0770`, `0x140eb0c40`),
  `RealmInventory` (estate trinket storage, `RealmInventoryChangedEvent`), `InventoryModifiedEvent`,
  `NomadWagon@Building`, `NomadWagonDisplay`, `BaseTrinketSupply`/`ShardTrinketSupply@Store@Building`,
  `PartySetupDisplay` (vftable `0x140eb4d60`), `PartyBuilderDisplay`, `EmbarkParty`, `EmbarkTownControllerState`.
- Error string mentions `Hero::SwapTrinket` ("target trinket id %u not found in library"); its function is not found yet.
- `0x1407ef030(widget, hero)`: refresh that plays "/ui/town/character_equip"/"_unequip" by comparing
  `0x1405c96e0(hero)` (count from `0x1405caf20` + 1, 0 for a corpse; probably trinket slots) and the size of
  the `u32` vector at `hero+0x1328..+0x1330` (probably the selected combat skills; embark checks
  "message_not_full_skills").
- Embark validation `0x140583360` ("message_not_full_party", "message_not_full_skills",
  "message_not_trinkets_equipped") → `0x140583a40` ("town_provision_embark_yes/no").
- Party composition sound/narration: `0x14068bc00` ("/ui/town/party_comp", "combo"), `0x14068cb40`.
- Nomad wagon sell candidate: `0x14069b400` (wallet add). Wallet-deduct callers not yet explained:
  `0x140668d50`, `0x140686cf0`, `0x140687780`, `0x140689d20`, `0x1406c6590`, `0x1407e2ae0`, `0x140532b50`.
- Probe v4 (ran, see runtime test 4): execute breakpoints on wallet deduct/add (logs caller), item transfer `0x1405d4910`,
  heirloom exchange `0x14067a550`; byte diffs of every roster record (0x1560 bytes) and the campaign
  object (0x2000 bytes), with per-byte noise suppression.

## Runtime test 4 (probe v4, wallet breakpoints + memory diffs, 2026-10-30)

Memory-diff method worked: per-byte noise suppression only had to silence `entry+0x1548/0x1549/0x154a`
(a per-hero timer). One diff line per real change.

| Action | Seen | Function (caller of wallet op) | Notes |
|---|---|---|---|
| Wallet load | add per currency on campaign load | `0x14055e34d` | 8 currencies |
| Guild skill level (1000 gold) | deduct | `0x140591088` = inside buy_upgrade | known path |
| **Nomad wagon buy** (7500 gold) | deduct | `0x140686e57` → **`0x140686cf0`** | |
| **Nomad wagon sell** (+750 = 10 %) | add | `0x14069b48e` → **`0x14069b400`** | |
| **Provisions buy** (2 food, -75 each) | deduct gold + second currency (0) | `0x140689f7d`/`0x140689f92` → **`0x140689d20`** | NOT `0x14068a2b0` (another route); `0x1405d4910` did not fire |
| Provisions return (+75) | add | `0x1407df58d` → `0x1407df430` | as mapped |
| **Leave provisions without embarking** | add = refund of everything bought | `0x1406d3e70`/`0x1406d3e8d` (also on entering, amount 0) | provisions only matter at departure |
| Heirloom exchange | not tested (not enough heirlooms) | | |

- **Trinket equip** (Reynauld = roster id 1): `entry+0x123c` 0→1 (equip) and 1→0 (unequip) = slot occupied flag;
  `campaign+0xeac` 3→2→3 = number of trinkets in the estate store. `entry+0x468` (vector realloc) and `entry+0xcfc`
  (floats 0.04 / -1.0) are derived stats. The trinket id itself did not change (the slot kept the old id). The game
  does not allow moving a trinket directly from hero to hero.
- **Combat skill selection**: `entry+0x1368` low byte e0→dc→e0 = end pointer of a `vector<u32>` at
  `entry+0x1360..+0x1368` (one skill removed, one added). Likely the same vector the UI reads at `hero+0x1328`,
  i.e. the Hero object starts at `entry+0x38` (unverified).
- **Party membership**: `entry+0x1540` = hero location: **0 = in the expedition party**, 1 = roster, 2 = in a
  building. Put-in/swap/remove gave exactly 16 in, 29 out, 1 in, 2 out, 17 in.
- **Party order** is not in the roster records nor in campaign[0..0x2000]. Embark: validation `0x140583360` → confirm
  dialog `0x140583a40` (only builds the yes/no dialog); the departure is in its "yes" callback (not found yet).
- Autosave writes a hero name and "persist.roster.json" into campaign buffers (`+0x170`, `+0x440`): not an action.
- After quitting to the menu the probe read an 18-hero roster at `campaign+0x728` (alt flag set) = the Butcher's
  Circus roster; not the campaign.

## Derived

- Combat is turn-based and one party moves together, so the turn-based impersonation pattern
  (playbook/engine-families.md, the Doors of Trithius row) fits. The host runs the real game,
  and a guest's command is executed on the turn of a hero that guest owns.

- The playbook tier moves from "closed binary, no MP" to "engine has MP": the combat loop already
  waits for an outside choice (PvP mailbox) and for the UI (`battle+0x9f7`).
- Preferred co-op hook: the player-hero path. For a hero a friend owns, the host DLL keeps the
  local UI from choosing, and writes the friend's skill index (and target) where the UI would.
  No PvP/PlayFab code runs, so `ReportSuspiciousMatch` can never fire. The PvP mailbox
  (`brain+0x118`) is a fallback, but it is only reached for non-hero actors in `arena_mp`.
- With a host-authoritative design + streamed view, the guests need no deterministic simulation.
- Placement sync: hook `0x14052c4b0` on guests → request {activity class+index, slot, heroId}; the host runs it
  (the game checks requirements and gold itself), then every copy runs it with `activity+0x90 = slot`.
  Same pattern expected for the other town actions: find the model function behind each UI commit.
- Town sync v2 (two layers):
  1. Boundaries (session start, every `as_town_visit_start` after a raid): the host's save is the truth.
     Guests do a full reload through the game's own unload → load path (a loading screen). All random
     start-of-visit results (stress relief, sanitarium, NPC residents, recruits) arrive inside that save.
  2. Within a town visit: action replication. Guest request → host validates and runs the handler → host
     broadcasts the action → every copy runs the same handler. Town actions are deterministic (upgrade,
     recruit from the synced pool, place in slot, dismiss, buy), so no RNG sync is needed here.
  Fallback for a desync inside a visit: a full reload from the host's save.
- Earlier town sync plan (snapshot after every action; superseded, no live subsystem reload exists): hook the UI handlers on guests → send a request. The host runs the game's own
  handler (gold/slots checked by the game), then sends a snapshot of the changed subsystem (roster /
  estate / town, in the game's own persist format), which guests load. The events say when. Guests
  never compute town effects (incl. random start-of-visit results), so the town needs no determinism.
  Open: whether a subsystem can be reloaded live in town.
- Superseded (owner wants parallel town work, all own the game): own copy per player, town actions
  replicated host-authoritatively, dungeon in lockstep with a host-chosen seed. Parsec + proxy SDL2.dll
  stays as the fallback if determinism fails.
- Earlier MVP idea (owner: town management open to everyone): a proxy `SDL2.dll` wraps
  `SDL_GameControllerGetButton/GetAxis` and returns "not pressed" for a controller that may not act
  now. In town everyone passes; on a hero's combat turn only the owner's controller passes. Friends
  join via Parsec, so each gets their own virtual controller (unverified). No guest app needed.
- Explicit `prng_minstd` passing suggests deterministic, seed-driven combat (PvP needs both sides to agree).
- Fairness boundary (rule 10): never use the PlayFab matchmaking/ranked service or its
  cheat reporting. Reuse in-game turn code over a direct friend-to-friend transport only.

## Guesses (not verified)

- Steam ships a native Linux build of DD1. If so, bot QA could run in a Linux container.
- The PvP netcode may only be armed in arena mode (`EnableMultiplayer`) and may need the free DLC content.
- Community save editors exist for the save format. Find one and link it.

## Unknowns (Gate 1 questions)

- Where the target choice is stored for a player hero (UI field next to `battle+0x9f7`).
- How the UI writes `battle+0x9f7` (to block local input for friend-owned heroes).
- Where the prng_minstd seed comes from (only needed for a later mirrored-client design).
- Whether a winmm.dll proxy loads (needs a Windows test run).

## Log

- 2026-10-30: triage done, no prior work. Owner chose to build a real mod over streaming.
- 2026-10-30: claim issue https://github.com/cranebeaker/coop-anything/issues/267. Building
  against the Steam base game (no DLC) for now.
- 2026-10-30: Gate 1 static pass on the exe strings and PE headers: the PvP netcode and network-AI skill path found.
- 2026-10-30: Ghidra 12.2 headless on the x64 exe (11 min, 12G heap). Combat input path mapped; Gate 1 = go.
- 2026-10-30: owner wants town open to everyone. Controller manager mapped; design moves to controller-slot ownership through a proxy SDL2.dll.
- 2026-10-30: all three players own DD1 on Steam. Design: own copy each, parallel town work, lockstep dungeon. Randomness sources mapped; map seed hookable at one call.
- 2026-10-30: town action map v1 (events → handlers). Missing: hero→building placement, trinkets, provisions, live subsystem reload.
- 2026-10-30: hero placement traced to the slot vector (writer: runtime test). Live reload: only the full unload→load path exists → town sync v2. TamperingManager found.
- 2026-10-30: test kit 1 built (read-only SDL2.dll proxy, 845 forwarders, hardware write breakpoints on activity slots). Waiting for the owner's Windows run.
- 2026-10-30: runtime test 1 passed loading; placement is not slot+0. Probe v2 (full slot bytes + roster hero fields) sent for run 2.
- 2026-10-30: runtime test 2: hero placement = `0x14052c4b0` (commit on window close). Model-level hook found.
- 2026-10-30: town action map v2 (model functions). Probe v3 (execute breakpoints on the four action functions + wallet log) sent as test kit 3.
- 2026-10-30: runtime test 3: upgrade request fully decoded (class.item hash, level char, hero id); recruit/dismiss confirmed; provisions and heirloom exchange found as extra actions.
- 2026-10-30: correction: hero placement commits on lock-in, not on window close.
- 2026-10-30: provisions (buy/return) and heirloom exchange mapped statically.
- 2026-10-30: trinkets/skills/party: static leads only; switched to runtime diffing (probe v4, test kit 4).
- 2026-10-30: runtime test 4: wagon buy/sell, provisions buy/refund, trinket flag, skill vector, party membership found; party order still open (at departure).

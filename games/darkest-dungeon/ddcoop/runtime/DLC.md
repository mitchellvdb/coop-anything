# Campaign DLC groundwork — test70 / duo53 (10 Nov 2026)

The owner authorized work toward all campaign DLC. Installed files/saves remain
untouched. Passive arena_mp save flags stay readable, but Circus/PlayFab native
paths are never activated or used by this co-op.

## Current coverage and limits

| Feature | Prepared | Still needs native integration / live validation |
|---|---|---|
| Musketeer | class data, skills/upgrades/ownership paths and shared base camping witness | real recruit/skills/equipment/trinkets/ownership in both directions |
| Shieldbreaker | class/skill codec and campaign data | camping nightmare, dream transitions and rewards |
| Flagellant | class, combat/camping definitions and codec bounds | special stress/death/skill restrictions and camping effects |
| Crimson Court | feature-specific data, curse/quest witnesses, campaign metadata | blood/curse/invitations, long Courtyard/persistent mission transitions |
| Districts | nonempty district data and save compatibility checks | native district purchases, costs and weekly effects |
| Color of Madness | campaign/provision data, mixed gold/shard transaction model | native shard binding/prices/refunds, separate shard shops/recruitment, waves/results |
| Fire's Edge | own district feature, package-common data | native district transactions and their effects |
| Runaway | class definitions, upgrades, camping and codec bounds | burn/stealth interactions and real ownership/combat/camping |
| Duelist | class definitions, upgrades, camping and codec bounds | stance/counter interactions and real ownership/combat/camping |

The legal local build contains all nine features. The five DLC heroes each have
seven distinct combat skills at levels 0..4; measured camping definitions fit the
existing codec. Data inspection is evidence for testing, not proof that those
scenarios currently synchronize. The runtime still uses the exact supported EXE.

## Read-only inspection and comparison

    python -I testkit/dlc-readiness/readiness.py <game root> > host-dlc.json
    python -I testkit/dlc-readiness/readiness.py --compare host-dlc.json guest-dlc.json

Optionally select required features using --enabled followed by feature names.
The tool never activates DLC, writes saves, copies content or changes Steam.
It rejects missing/empty witnessfiles, fingerprints feature and shared-package
text data, and includes Musketeer's shared base camping data. Comparison reports
known EXE/build parity, missing/changed/unknown features, duplicates and invalid
schemas. Matching reports are not entitlements or live co-op compatibility.
This comparison is not yet an automatic cross-launcher DLC-data handshake.

Synthetic regression tests cover all nine features, five hero codecs, missing
witnesses, shared data changes, subset comparison, no mutation, schema/build
mismatch and link guards. One real Windows symlink creation is skipped if the
OS denies it; mocked link checks remain mandatory. Arena data is excluded.

## Shard transaction foundation

src/dd_supplies.c now provides explicit *_with_shards APIs taking a proved shard
currency key. They check both balances and exact conservation of stock, bag,
gold, shards and every unrelated currency. Buy/return/refund, stale/crossed
commands, shortages and integer overflow are modeled through the actual codec:
5,000 four-copy sequences plus an independent 1,200-case extreme-value matrix.

The normal wrappers and native supply bridge deliberately remain gold-only.
Shard costs cannot be ignored. This is no native shard shop or wave integration.
Read-only analysis of the supported EXE confirms separate price_gold/price_shards
and refund totals. Purchase reads a shard key from RVA2bb5e80; cancel/refund reads
RVA2bc9a60. These are initialized globals, not proven constant values: validate
their meaning/equality and native payment/refund effects before enabling calls.
Existing native prices remain the source of truth; never invent DLC prices.

## Next work

1. Bind/prove shard currency identity and native prices/refunds in a disposable
   campaign; extend runtime post-checks with that exact key and measured effects.
2. Audit separate shard-trinket/recruit store ABI instead of assuming ordinary
   Nomad/Stagecoach hooks cover them.
3. Exercise each hero in an ordinary raid, then Shieldbreaker nightmares,
   Courtyard persistence/curse/blood, District purchases and Farmstead waves.
4. At explicit player choices, prove hooks/readiness and add ordered commands.
   Keep automatic engine events native; compare state and RNG at stable endpoints.
5. Validate both host and guest initiation, all ownership values and checkpoint
   recovery. Full live DLC support remains unconfirmed until these tests pass.

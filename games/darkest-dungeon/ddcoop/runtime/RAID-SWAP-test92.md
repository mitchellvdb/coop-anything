# Copied bag Items in loot exchanges (test92)

Owner reports a guest swapping torches for onyx changes only that copy. The91
whole-stack handler resolves canonical bag Item pointers and rooted loot UI
copies, but omitted the bag's own presentation copy. The older trinket-only
fallback accepts only quantity1/type trinket and cannot cover torches.

A canonical Item pointer roots its exact native slot even if the UI holds its
containing wrapper. For a presentation copy, require wrapper+100 to be the current native raid bag;
require its physical index and the complete quantity/type/id against that slot.
Never search for another similar stack. The existing ordered RAID_SWAP29 path
then owns both endpoints, native Item lifetime and conservation/postchecks.
A stale known raid widget refuses before effects and cannot silently fall back
to a native local exchange. Add numeric routing diagnostics without names.
No native hook/signature/action schema was added by this fix.

The new copied8-torch/1-onyx fixture covers both directions and all4 initiators,
with foreign/stale UI refusal. It fails against the original91 code. The full
changed runtime/FIFO and bridge MSVC ASan pass. Existing canonical/loot-copy,
zero-ID gold, trinket, full-bag,3-peer replay and partial-effect STOP tests remain.
29 native proof scripts pass, including existing native copy/drop/swap ABI.
The wrapper precondition is retained from the earlier trinket resolver.
Native effects are modeled; the real drag/UI still needs the owner's playtest.
The newest private reports also contain bag-repair missing-template and party
formation failures. This is a reproduced missing route, not a proven explanation
for every live desync. Split-stack exchange and native same-item merges are
unchanged. Prior Claude Code contributions retained; written by Codex.
